#include "lobby_message.h"
#include <cstring>
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
char board[100];

atomic<bool> setupFinished = false;

void acceptTo(vector<Player> players){
    int clientSock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0){
        perror("socket failed");
        return 1;
    }
    cout << "Socket created successfully" << endl;
    setsockopt(clientSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&address, 0, sizeof(address));
    address.sin_addr.s_addr = INADDR_ANY; /*inet_addr(nodeIPs[nodeID].c_str());*/
    address.sin_family = AF_INET;
    address.sin_port = htons(LOBBY_PORT);

    bind_value = bind(clientSock, (struct sockaddr * )&address,  sizeof(address));
    cout << "Bind happened" << endl;
    if(bind_value < 0){
        perror("Could not Bind");
        return 1;
    }

    //3 listen
    listen_value = listen(clientSock, 20);
    cout << "Listen happened" << endl;
    if(listen_value < 0 ){
        perror("Could not listen");
        return 1;
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
        return 1;
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
        close(sock);
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
}

int runTimer(int length){
    startTime = chrono::system_clock::now();
    while(chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - startTime).count() < length){
        this_thread::sleep_for(chrono::seconds(1))
    }
    setupFinished = true;
}




int startGame(int numberOfNodes, vector<Player> players, int playerNodeID, int startingPlayerID){
    numNodes = numberOfNodes;
    nodeID = playerNodeID;
    sockets[0] = players[0].socket;
    playersLeft = numberOfNodes;
    if(numNodes >= 3 && nodeID != 1){//More than 2 players
        setupMesh(numNodes, players);
    }//Otherwise the mesh is already setup

    //Start the timer thread here
    thread timer = thread(runTimer, 120);

    //start the board creation 
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    //initialize empty board
    for(int i = 0; i < 100; i++){
        board[i] = '~';
    }

    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(STDIN_FILENO, &read_fds);

    while(!setupFinished && !shipsPlaced){
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        string shipName = "";
        string location = "";
        char direction = '\0';
        int result = select(STDIN_FILENO + 1, &read_fds, NULL, NULL, timeout);
        if(result == 0){
            continue;
        } else {
            string input = getLine();
            istringstream iss(input);
            iss >> shipName >> location >> direction;

            //TODO validate all input -- DO THIS LATER
            
        }
    }



    timer.join();





















    
    //Start the game with whoever needs to with a call
    if(startingPlayerID != playerNodeID){
        retry:
            int bytes_recv = recv(sockets[startingPlayerID], &nextPlayer, sizeof(nextPlayer), MSG_WAITALL);
            if(bytes_recv <= 0){
                goto retry;
            }
    } else {
        for(int i = 0; i < numberOfNodes; i++){
            if(i == playerNodeID){
                continue;
            }
            nextPlayer = (startingPlayerID + 1) % numberOfNodes;
            int bytes_sent = send(sockets[i], &nextPlayer)
        }
    }




    //Should be ready to start nwo based off all info pased
}