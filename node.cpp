#include "lobby_message.h"
#include "message.h"
#include <cstring>
#include <atomic>
#include <string>
#include <chrono>
#include <sstream>
#include <sys/select.h>

#define LOBBY_PORT 6010

using namespace std;


int sockets[4] = {-1, -1, -1, -1};
int numNodes = 0;
int nodeID = -1;

int nextPlayer = -1;
int playersLeft = -1;
bool shipsPlaced = false;
int shipsToPlace = 5;
bool placed[5] = {false};

Ship ships[5];

Cell boards[4][10][10];
int shipHealths[4][5] = {};
bool boardAlive[4] = {true, true, true, true};
int playersAlive = 0;
string names[4] = {};

atomic<bool> setupFinished = false;
atomic<bool> turnTaken = false;
atomic<bool> timedOut = false;

void initializeShips(){
    ships[0] = {6,{{0,0},{1,0},{2,0},{0,-1},{2,-1},{2,-2}},2,2};
    ships[1] = {5,{{0,0},{1,0},{2,0},{1,-1},{1,-2},{-1,-1}},2,2};
    ships[2] = {4,{{0,0},{1,0},{1,-1},{2,-1},{-1,-1},{-1,-1}},1,2};
    ships[3] = {3,{{0,0},{1,0},{1,-1},{-1,-1},{-1,-1},{-1,-1}},1,1};
    ships[4] = {2,{{0,0},{1,0},{-1,-1},{-1,-1},{-1,-1},{-1,-1}},0,1};
}

void displayShipsLeft() {
    vector<int> remaining;
    for (int i = 0; i < 5; i++) {
        if (!placed[i]) remaining.push_back(i);
    }
    if (remaining.empty()) {
        cout << "All ships placed!\n";
        return;
    }

    cout << "Ships left to place:\n";

    const int GRID = 3;
    const int COL_W = 6; // "Ship X" = 6 chars
    const int GAP = 3;
    int n = remaining.size();

    const char* ANCHOR_COLOR = "\033[33m";
    const char* RESET        = "\033[0m";

    // Build all grids and record each ship's anchor display position
    bool cells[5][GRID][GRID] = {};
    int anchorDr[5] = {}, anchorDc[5] = {};
    for (int k = 0; k < n; k++) {
        int s = remaining[k];
        int minR = 0, maxR = 0, minC = 0;
        for (int i = 0; i < ships[s].hp; i++) {
            int c = ships[s].offsets[i][0];
            int r = ships[s].offsets[i][1];
            if (c < minC) minC = c;
            if (r < minR) minR = r;
            if (r > maxR) maxR = r;
        }
        int h = maxR - minR + 1;
        int rowPad = GRID - h;
        // anchor offset is {0,0}, so its display position is:
        anchorDr[k] = 0 - minR + rowPad;
        anchorDc[k] = 0 - minC;
        for (int i = 0; i < ships[s].hp; i++) {
            int dc = ships[s].offsets[i][0] - minC;
            int dr = ships[s].offsets[i][1] - minR + rowPad;
            if (dr >= 0 && dr < GRID && dc >= 0 && dc < GRID)
                cells[k][dr][dc] = true;
        }
    }

    // Headers — all on one line
    for (int k = 0; k < n; k++) {
        string hdr = "Ship " + to_string(remaining[k] + 1);
        cout << hdr;
        for (int p = 0; p < COL_W - (int)hdr.size(); p++) cout << ' ';
        if (k < n - 1) for (int p = 0; p < GAP; p++) cout << ' ';
    }
    cout << '\n';

    // 3 grid rows — all ships on each line
    for (int row = 0; row < GRID; row++) {
        for (int k = 0; k < n; k++) {
            int printed = 0;
            for (int c = 0; c < GRID; c++) {
                if (c > 0) { cout << ' '; printed++; }
                bool isAnchor = (row == anchorDr[k] && c == anchorDc[k]);
                if (isAnchor) cout << ANCHOR_COLOR;
                cout << (cells[k][row][c] ? '#' : '.');
                if (isAnchor) cout << RESET;
                printed++;
            }
            // Pad to COL_W visible chars (GRID*2-1 = 5)
            for (int p = printed; p < COL_W; p++) cout << ' ';
            if (k < n - 1) for (int p = 0; p < GAP; p++) cout << ' ';
        }
        cout << '\n';
    }
    cout << '\n';
}

void printBoard(int ID) {
    const int BOARD_W = 22;
    string name = names[ID];
    int pad = (BOARD_W - (int)name.size()) / 2;
    cout << string(pad, ' ') << name << string(BOARD_W - pad - (int)name.size(), ' ') << '\n';

    cout << "  ";
    for (int i = 0; i < 10; i++) {
        cout << static_cast<char>('A' + i) << ' ';
    }
    cout << '\n';

    for (int i = 0; i < 10; i++) {
        cout << i << ' ';
        for (int j = 0; j < 10; j++) {
            const Cell& cell = boards[ID][i][j];
            char symbol = '~';

            if (cell.been_shot) {
                symbol = (cell.ship_connection != -1) ? 'X' : 'O';
            } else if (ID == nodeID && cell.ship_connection != -1) {
                symbol = '#';
            }

            cout << symbol << ' ';
        }
        cout << '\n';
    }
}

bool updateShipHealths(int boardID) {
    for (int s = 0; s < 5; s++) shipHealths[boardID][s] = 0;
    for (int r = 0; r < 10; r++) {
        for (int c = 0; c < 10; c++) {
            const Cell& cell = boards[boardID][r][c];
            if (cell.ship_connection != -1 && !cell.been_shot)
                shipHealths[boardID][cell.ship_connection]++;
        }
    }
    bool alive = false;
    for (int s = 0; s < 5; s++) {
        if (shipHealths[boardID][s] > 0) { alive = true; break; }
    }
    boardAlive[boardID] = alive;
    if(!alive){
        cout << "Player " << names[ boardID] << " has lost all their ships" << endl;
    }
    return alive;
}

void printOpponentBoards() {
    vector<int> alive;
    for (int i = 0; i < numNodes; i++) {
        if (i == nodeID) continue;
        if (boardAlive[i]) alive.push_back(i);
    }

    if (alive.empty()) return;

    const int BOARD_W = 22; // "  A B C D E F G H I J " = 22 chars
    const int GAP = 3;
    const string GAP_STR(GAP, ' ');

    // Player names centered above each board
    for (int k = 0; k < (int)alive.size(); k++) {
        string name = names[alive[k]];
        int pad = (BOARD_W - (int)name.size()) / 2;
        cout << string(pad, ' ') << name << string(BOARD_W - pad - (int)name.size(), ' ');
        if (k < (int)alive.size() - 1) cout << GAP_STR;
    }
    cout << '\n';

    // Column headers
    for (int k = 0; k < (int)alive.size(); k++) {
        cout << "  ";
        for (int i = 0; i < 10; i++) cout << static_cast<char>('A' + i) << ' ';
        if (k < (int)alive.size() - 1) cout << GAP_STR;
    }
    cout << '\n';

    // Board rows
    for (int row = 0; row < 10; row++) {
        for (int k = 0; k < (int)alive.size(); k++) {
            int id = alive[k];
            cout << row << ' ';
            for (int col = 0; col < 10; col++) {
                const Cell& cell = boards[id][row][col];
                char symbol = '~';
                if (cell.been_shot)
                    symbol = (cell.ship_connection != -1) ? 'X' : 'O';
                cout << symbol << ' ';
            }
            if (k < (int)alive.size() - 1) cout << GAP_STR;
        }
        cout << '\n';
    }
}

void acceptTo(vector<Player> players){

    int clientSock = socket(AF_INET, SOCK_STREAM, 0);
    if(clientSock < 0){
        perror("socket failed");
        return;
    }
    cout << "Socket created successfully" << endl;

    int opt = 1;
    setsockopt(clientSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    //2 bind
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_family = AF_INET;
    address.sin_port = htons(LOBBY_PORT);

    int bind_value = bind(clientSock, (struct sockaddr * )&address,  sizeof(address));
    cout << "Bind happened" << endl;
    if(bind_value < 0){
        perror("Could not Bind");
        return;
    }

    //3 listen
    int listen_value = listen(clientSock, 3);
    cout << "Listen happened" << endl;
    if(listen_value < 0 ){
        perror("Could not listen");
        return;
    }

    //4 accept looping
    struct sockaddr_in remote_address;
    socklen_t remote_addrlen = sizeof(address);
    cout << "Begin looping" << endl;
    memset(&remote_address, 0, sizeof(address));

    int client_socket = accept(clientSock, (struct sockaddr *)&remote_address, &remote_addrlen);
    //cout << "Accepted something" << endl;
    if(client_socket < 0 ){
        perror("Could not accpet");
        return;
    }

    string client_ip = inet_ntoa(remote_address.sin_addr);
    
    for(int i = 0; i < numNodes; i++){
        if(strcmp(players[i].ip, client_ip.c_str()) == 0){
            sockets[i] = client_socket;
        }
    }
}

void connectTo(char* ip, int nodeConnectionID){
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0){
        cout << "SOCKETPOOL CONNECTIONS FAILED" << endl;
        return;
    }
    cout << "socket made" << endl;

    //Fill in the structure of the socket connection
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_addr.s_addr = inet_addr(ip);
    address.sin_family = AF_INET;
    address.sin_port = htons(LOBBY_PORT);
    
    cout << "Socket settings" << endl;
    //Connect
    while(connect(sock, (sockaddr*)&address, sizeof(address)) != 0){
        sleep(1);
        cout << "In connect loop, 1 failed connect" << endl;
    }
    sockets[nodeConnectionID] = sock;
    cout << "Client connected" << endl;

    //set timeout
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    cout << "Socket Setup" << endl;
}

int setupMesh(int numNodes, vector<Player> players){
    int nodeConnectionID;
    //player 2 connect
    if(nodeID == 1){
        //bind and listen
        acceptTo(players);
        if(numNodes == 4){
            acceptTo(players);
        }
    } else {
        nodeConnectionID = 1;
        connectTo(players[1].ip, nodeConnectionID);
        // try to connect
    }

    if(nodeID == 2 && numNodes == 4){
        //bind and listen
        acceptTo(players);

    } else if (numNodes == 4){
        nodeConnectionID = 2;
        connectTo(players[2].ip, nodeConnectionID);
        // try to connect
    }
    return 0;
}

void runTimer(int length){
    chrono::time_point startTime = chrono::system_clock::now();
    while(!turnTaken && chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - startTime).count() < length){
        if(chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - startTime).count() % 15 == 0){//Cannot hardcode the update time as it will be diff for turns
            int timeRemaining = length - chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - startTime).count();
            cout << "\033[91m\n" << timeRemaining << " seconds remaining\033[0m" << endl;
        }
        this_thread::sleep_for(chrono::seconds(1));
    }
    if(!turnTaken){
        timedOut = true;
    }
    setupFinished = true;
}

void sendBoard(){
    for(int i = 0; i < numNodes; i++){
        if(nodeID == i){
            continue;
        } else {
            int res = send(sockets[i], &nodeID, sizeof(nodeID), 0);
            res = send(sockets[i], &boards[nodeID], sizeof(boards[nodeID]), 0);
        }
    }
}

void recvBoards(){
    for(int i = 0; i < numNodes; i++){
        if(nodeID == i){
            continue;
        } else {
            int ID = -1;
            int res = recv(sockets[i], &ID, sizeof(ID), MSG_WAITALL);
            if(res <= 0 || ID < 0 || ID >= numNodes) continue;
            res = recv(sockets[i], &boards[ID], sizeof(boards[ID]), MSG_WAITALL);
            bool temp = updateShipHealths(ID);//dont need this var, its jsut to update the board first time
        }
    }
}

void sendQuorum(int turnNode, int vote){
    for(int i = 0; i < numNodes; i++){
        if(i == turnNode){
            continue;
        } else if (i == nodeID){
            continue;
        } else if (!boardAlive[i]){
            continue;
        } else {
            int bytes_sent = send(sockets[i], &vote, sizeof(vote), 0);
        }
    }
}

void recvQuorum(int turnNode, int vote){
    int totalVotes = vote;
    for(int i = 0; i < numNodes; i++){
        if(i == turnNode){
            continue;
        } else if (i == nodeID){
            continue;
        } else if (!boardAlive[i]){
            continue;
        } else {
            int bytes_received = recv(sockets[i], &vote, sizeof(vote), MSG_WAITALL);
            totalVotes += vote;
        }
    }
    if(totalVotes > (playersAlive/2)){
        playersAlive--;
        boardAlive[turnNode] = false;
    }
    return;
}

void win(){
    for(int i = 0; i < numNodes; i++){
        if(boardAlive[i]){
            cout << names[i] << " has won the game!" << endl;
                exit(0);
        }
    }
}

void handleTurns(int startNode){
    int turnNode = startNode;
    while(true){
    Shot shot;
    bool healthCheck = true;
    turnTaken = false;
    timedOut = false;
    bool shotInTime = false;
    if(boardAlive[turnNode]){
        printBoard(nodeID);
        printOpponentBoards();
        if(turnNode == nodeID){//!Need to put healthCheck here after shot so its ready for post the inner 
            // After a shot lands, call updateShipHealths(targetBoardID).
            // If it returns false, that board was just destroyed — boardAlive[targetBoardID]
            // is already updated. Check boardAlive[] to find the newly eliminated player
            // and decrement playersLeft. If playersLeft == 1 and boardAlive[nodeID], we win.

            //Have to check if the shots already been taken, cell.shot...
            //Then broadcast shot
            thread timer = thread(runTimer, 60);
            string shotAttempt = "";
            while(!turnTaken && !timedOut){
                getline(cin, shotAttempt);
                istringstream iss(shotAttempt);
                string targetName;
                string cell;
                iss >> targetName >> cell;

                int targetBoard = -1;
                for (int i = 0; i < numNodes; i++) {
                    if (names[i] == targetName) {
                        targetBoard = i;
                        break;
                    }
                }

                if(targetBoard == -1){
                    cout << "Player " << targetName << " does not exist" << endl;
                    continue;
                } else if (targetBoard == nodeID){
                    cout << "You cannot shoot yourself" << endl;
                    continue;
                } else if (!boardAlive[targetBoard]){
                    cout << "Player " << targetName << " is already dead" << endl;
                    continue;
                }  else if (cell.length() != 2) {
                    cout << "Invalid shot location" << endl;
                    continue;
                }

                int col = cell[0] - 'A';
                int row = cell[1] - '0';
                
                if(!(col <= 9 && col >= 0)){
                    cout << "Invalid Column" << endl;
                    continue;
                } else if(!(row <= 9 && row >= 0)){
                    cout << "Invalid Row" << endl;
                    continue;
                } else if(boards[targetBoard][row][col].been_shot){
                    cout << "Location " << cell << " has already been shot" << endl;
                    continue;
                }
                
                //Now build shot
                shot.boardID = targetBoard;
                shot.row = row;
                shot.col = col;
                
                for(int i = 0; i < numNodes; i++){
                    if(boardAlive[i] && nodeID != i){
                        int bytes_sent = send(sockets[i], &shot, sizeof(shot), 0);
                    }
                }

                turnTaken = true;
            }
            timer.join();
            if(timedOut){
                cout << "You took too long on your turn so you lost" << endl;
                boardAlive[nodeID] = false;
                exit(0);
            }
            
            boards[shot.boardID][shot.row][shot.col].been_shot = true;
            if(boards[shot.boardID][shot.row][shot.col].ship_connection != -1){
                cout << names[turnNode] <<" shot player " << names[shot.boardID] << " at cell " << (char)('A'+ shot.col) << shot.row << " and hit their ship" << boards[shot.boardID][shot.row][shot.col].ship_connection << endl; 
                healthCheck = updateShipHealths(shot.boardID);
                if(shipHealths[shot.boardID][boards[shot.boardID][shot.row][shot.col].ship_connection] == 0){
                    cout << names[turnNode] <<" has sunk player " << names[shot.boardID] << "'s ship" << boards[shot.boardID][shot.row][shot.col].ship_connection << endl; 
                }
            } else {// Missed shot
                cout << names[turnNode] <<" shot player " << names[shot.boardID] << " at cell " << (char)('A'+ shot.col) << shot.row << " and missed" << endl; 
            }
        } else {
            //thread timer = thread(runTimer, 60);
            struct timeval timeout;
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;
            bool shotRecv = false;
            setsockopt(sockets[turnNode], SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
            for(int i = 0; i < 60; i++){
                int rec = recv(sockets[turnNode], &shot, sizeof(shot), MSG_WAITALL);
                if(rec > 0){
                    shotRecv = true;
                    break;
                }
            }
            if(shotRecv){
                shotInTime = true;
                boards[shot.boardID][shot.row][shot.col].been_shot = true;
                if(boards[shot.boardID][shot.row][shot.col].ship_connection != -1){
                    cout << names[turnNode] <<" shot player " << names[shot.boardID] << " at cell " << (char)('A'+ shot.col) << shot.row << " and hit their ship" << boards[shot.boardID][shot.row][shot.col].ship_connection << endl; 
                    healthCheck = updateShipHealths(shot.boardID);
                    if(shipHealths[shot.boardID][boards[shot.boardID][shot.row][shot.col].ship_connection] == 0){
                        cout << names[turnNode] <<" has sunk player " << names[shot.boardID] << "'s ship" << boards[shot.boardID][shot.row][shot.col].ship_connection << endl; 
                    }
                } else {// Missed shot
                    cout << names[turnNode] <<" shot player " << names[shot.boardID] << " at cell " << (char)('A'+ shot.col) << shot.row << " and missed" << endl; 
                }
            }

            if(shotInTime){
                thread sendQ = thread(sendQuorum, turnNode, 0);
                thread recvQ = thread(recvQuorum, turnNode, 0);
                sendQ.join();
                recvQ.join();
            } else {
                thread sendQ = thread(sendQuorum, turnNode, 1);
                thread recvQ = thread(recvQuorum, turnNode, 1);
                sendQ.join();
                recvQ.join();
            }
        }
        if(playersAlive <= 1){
            win();
            return;
        }
    }
    turnNode = (turnNode + 1) % numNodes;
    } // end while
}

void startGame(int numberOfNodes, vector<Player> players, int playerNodeID, int startingPlayerID){
    numNodes = numberOfNodes;
    nodeID = playerNodeID;
    for(int i = 0; i < numNodes; i++){
        sockets[i] = players[i].socket;
    }
    playersLeft = numberOfNodes;
    if(numNodes >= 3 && nodeID != 0){//More than 2 players
        setupMesh(numNodes, players);
    }//Otherwise the mesh is already setup

    playersAlive = numNodes;

    for(int i = 0; i < numNodes; i++){
        names[i] = players[i].name;
    }
    
    initializeShips();

    printBoard(nodeID);
    displayShipsLeft();
    cout << "Ships are placed using an anchor point (the gold cell, bottom-left of the shape).\n"
            "When you specify a location, the anchor will be placed there.\n";
    //Start the timer thread here
    thread timer = thread(runTimer, 120);

    //start the board creation 
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(STDIN_FILENO, &read_fds);

    while(!setupFinished && !shipsPlaced){
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        int shipName = -1;
        string location = "";
        char direction = '\0';
        int result = select(STDIN_FILENO + 1, &read_fds, NULL, NULL, &timeout);
        if(result == 0){
            continue;
        } else {
            string input = "";
            getline(cin, input);
            istringstream iss(input);
            iss >> shipName >> location >> direction;
            int shipIndex = shipName - 1;

            if(shipName < 1 || shipName > 5){
                cout << "Invalid ship number" << endl;
                continue;
            } else if (placed[shipIndex] == 1){
                cout << "Ship already placed" << endl;
                continue;
            } else if (location.length() != 2){
                cout << "Invalid location length" << endl;
                continue;
            } else if((location[0] < 'A' || location[0] > 'J') || (location[1] < '0' || location[1] > '9')){
                cout << "Invalid location" << endl;
                continue;
            } else if (direction != 'N' && direction != 'S' && direction != 'E' && direction != 'W'){
                cout << "Invalid direction " << endl;
                continue;
            }

            int col = tolower(location[0]) - 'a';
            int row = location[1] - '0';
            if(!(direction == 'N' && row - ships[shipIndex].maxRow >= 0 && col + ships[shipIndex].maxCol <= 9)
            && !(direction == 'E' && row + ships[shipIndex].maxCol <= 9 && col + ships[shipIndex].maxRow <= 9)
            && !(direction == 'S' && row + ships[shipIndex].maxRow <= 9 && col - ships[shipIndex].maxCol >= 0)
            && !(direction == 'W' && row - ships[shipIndex].maxCol >= 0 && col - ships[shipIndex].maxRow >= 0)){
                cout << "Ship doesn't fit" << endl;
                continue;
            }

            bool valid = true;
            for(int i = 0; i < ships[shipIndex].hp; i++){
                int col_offset = ships[shipIndex].offsets[i][0];
                int row_offset = ships[shipIndex].offsets[i][1];

                int targetRow = 0;
                int targetCol = 0;
                if (direction == 'N') {
                    targetRow = row + row_offset;
                    targetCol = col + col_offset;
                } else if (direction == 'E'){
                    targetRow = row + col_offset;
                    targetCol = col - row_offset;
                } else if (direction == 'S'){
                    targetRow = row - row_offset;
                    targetCol = col - col_offset;
                } else if (direction == 'W'){
                    targetRow = row - col_offset;
                    targetCol = col + row_offset;
                }
                if(boards[nodeID][targetRow][targetCol].ship_connection != -1){
                    cout << "Cell already taken by another ship" << endl;
                    valid = false;
                    break;
                }
            }
            if(!valid){
                continue;
            }
            
            for(int i = 0; i < ships[shipIndex].hp; i++){
                int col_offset = ships[shipIndex].offsets[i][0];
                int row_offset = ships[shipIndex].offsets[i][1];

                int targetRow = 0;
                int targetCol = 0;
                if (direction == 'N') {
                    targetRow = row + row_offset;
                    targetCol = col + col_offset;
                } else if (direction == 'E'){
                    targetRow = row + col_offset;
                    targetCol = col - row_offset;
                } else if (direction == 'S'){
                    targetRow = row - row_offset;
                    targetCol = col - col_offset;
                } else if (direction == 'W'){
                    targetRow = row - col_offset;
                    targetCol = col + row_offset;
                }
                boards[nodeID][targetRow][targetCol].ship_connection = shipIndex;
            }

            placed[shipIndex] = true;
            shipsToPlace--;
            if(shipsToPlace <= 0){
                shipsPlaced = true;
            }
            printBoard(nodeID);
            displayShipsLeft();
        }
    }

    timer.join();

    //Start the board sharing here:
    //Spin up 2 threads
    thread boardSend = thread(sendBoard);
    thread boardRecv = thread(recvBoards);

    boardSend.join();
    boardRecv.join();

    //Start turns now
    handleTurns(startingPlayerID);
}