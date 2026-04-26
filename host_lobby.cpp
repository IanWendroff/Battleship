#include "lobby_message.h"
#include <random>
#include <string>
#include <cstring>
#include <mutex>
#include <limits>
#include <atomic>

#define LOBBY_PORT 27015
using namespace std;

mutex playersMutex;
int numPlayers = 2;
atomic<bool> gameStarted = false;

int startGame(int numberOfNodes, vector<Player> players, int playerNodeID, int startingPlayerID);

int acceptPlayers(vector<Player>& players){
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0){
        perror("socket failed");
        return 1;
    }
    cout << "Socket created successfully" << endl;

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    //2 bind
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_family = AF_INET;
    address.sin_port = htons(LOBBY_PORT);

    int bind_value = bind(sock, (struct sockaddr * )&address,  sizeof(address));
    cout << "Bind happened" << endl;
    if(bind_value < 0){
        perror("Could not Bind");
        return 1;
    }

    //3 listen
    int listen_value = listen(sock, 3);
    cout << "Listen happened" << endl;
    if(listen_value < 0 ){
        perror("Could not listen");
        return 1;
    }

    //Set the timeout so the gameStarted checker works
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    //4 accept looping
    struct sockaddr_in remote_address;
    socklen_t remote_addrlen = sizeof(address);
    cout << "Begin looping" << endl;
    while(!gameStarted){
        memset(&remote_address, 0, sizeof(address));

        int client_socket = accept(sock, (struct sockaddr *)&remote_address, &remote_addrlen);
        if(client_socket < 0){
            continue; // timeout or error, loop back and check gameStarted
        } else {
            cout << "Accepted something" << endl;
            struct timeval no_timeout;
            no_timeout.tv_sec = 0;
            no_timeout.tv_usec = 0;
            setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, &no_timeout, sizeof(no_timeout));
        }

        string client_ip = inet_ntoa(remote_address.sin_addr);
        int remote_port = ntohs(remote_address.sin_port);

        //Once connect is made on join side, immediately sends name
        char name[17];
        int bytes_received = recv(client_socket, name, sizeof(name), MSG_WAITALL);
        if (bytes_received <= 0) {
            perror("recv failed, did not recieve name");
            return 0;
        } else {
            cout << "Player " << name << " has joined the lobby" << endl;
        }

        Player player;
        player.nodeID = players.size();
        strncpy(player.name, name, 16); player.name[16] = '\0';
        strncpy(player.ip, client_ip.c_str(), 15); player.ip[15] = '\0';
        player.socket = client_socket;

        lock_guard<std::mutex> lock(playersMutex);
        if((int)players.size() >= numPlayers){
            close(client_socket);
            continue; // lobby full, reject
        }
        players.push_back(move(player));
    }
    return 1;
}

int hostInput(vector<Player>& players){
    int loop = 1;
    string hostCommand;
    string name;
    cout << "Host commands:\n"
            "  s          - Start the game\n"
            "  c <number> - Change max player count (2-4)\n"
            "  k <name>   - Kick a player\n";
    while(loop == 1){
        getline(cin, hostCommand);
        if(hostCommand[0] == 'k'){ // Kick player, "k {name}"
            name = hostCommand.substr(2);
            bool erased = false;
            lock_guard<mutex> lock(playersMutex);
            for(int i = 1; i < players.size(); i++){
                if(erased){
                    players[i].nodeID = i; // Updates later players nodeIDs to match their location in the vector
                }
                else if(strcmp(players[i].name, name.c_str()) == 0){
                    players.erase(players.begin() + i);
                    erased = true;
                    i--;
                }
            }
            if(erased){
                cout << "Removed " << name << endl;
            } else {
                cout << "Player " << name << " is not in the lobby" << endl;
            }
        } else if (hostCommand[0] == 'c'){// Change max player count "c {numPlayers}" 2-4
            lock_guard<mutex> lock(playersMutex);
            if(players.size() > hostCommand[2] - '0'){ //already more players than they want to change lobby size too.
                cout << "Too many players in lobby, cannot change size" << endl;
                continue;
            }
            numPlayers = hostCommand[2] - '0';
            cout << "Changed max player count to " << numPlayers << endl;
        } else if (hostCommand[0] == 's'){
            lock_guard<mutex> lock(playersMutex);
            if(players.size() == numPlayers){//numPlayers has been reached and ready to start
                loop = 0;
                cout << "Starting Game" << endl;
                gameStarted = true;
                continue;
            } else {
                cout << "Player count has not reached total players, please change lobby size or add more players" << endl;
            }
        } else {
            cout << "Please re-enter command" << endl;
        }
    }
    return 1;
}

void resolveHostIP(char* hostIP){
    FILE* fp = popen("hostname -I", "r");
    if(fp == NULL){ hostIP[0] = '\0'; return; }
    char buf[256];
    fgets(buf, sizeof(buf), fp);
    pclose(fp);
    char* first = strtok(buf, " \t\n\r");
    if(first) strncpy(hostIP, first, INET_ADDRSTRLEN - 1);
    else hostIP[0] = '\0';
}

int main(int argc, char **argv){

    char hostIP[INET_ADDRSTRLEN];
    resolveHostIP(hostIP);
    if(hostIP[0] == '\0'){
        cout << "Could not detect IP. Please enter manually: " << endl;
        cin >> hostIP;
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }

    vector<Player> players{};

    cout << "Please enter name (16 characters max)" << endl;
    char hostName[17];
    bool loop = true;
    while(loop){
        cin >> hostName;
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        if(strlen(hostName) > 16){
            cout << "Please enter a name 16 characters max" << endl;
        } else {
            loop = false;
        }
    }
    Player host;
    host.nodeID = 0;
    strncpy(host.name, hostName, 16); host.name[16] = '\0';
    strncpy(host.ip, hostIP, 15); host.ip[15] = '\0';
    players.push_back(host);

    //Make sure to initialize the og player who started the match before this

    cout << "IP ADDRESS: " << host.ip << endl;

    //Start player looping thread and host commands thread to edit the vector:
    thread playerThread = thread(acceptPlayers, ref(players)); 
    thread hostThread = thread(hostInput, ref(players));

    //Before game starts needs to reconcile threads:
    playerThread.join();
    hostThread.join();

    //Implement the host distributing the players info here
    for(int i = 1; i < numPlayers; i++){
        //Send player count:
        int bytes_sent = send(players[i].socket, &numPlayers, sizeof(numPlayers), 0);
    }

    int startingPlayer = -1;
    random_device rd;
    static thread_local mt19937 rng(rd()); //Unique for each thread
    uniform_int_distribution<int> playerChooser(0, numPlayers - 1); // Chooses starting player

    startingPlayer = playerChooser(rng);

    for(int i = 1; i < numPlayers; i++){
        int bytes_sent = send(players[i].socket, &startingPlayer, sizeof(startingPlayer), 0);
    }
    
    //Send lobby info:
    for(int i = 1; i < numPlayers; i++){
        int bytes_sent = send(players[i].socket, players.data(), numPlayers * sizeof(Player), 0);
    }


    //Open up host game and send all player info
    startGame(numPlayers, players, 0, startingPlayer);
}































/* 
!Tried to use this to resolve hostIP address, but didn't work and only gave the fallback IP

    struct addrinfo* hostAddress;
    struct addrinfo hostHints;
    memset(&hostHints, 0, sizeof hostHints);
    char hostname[HOST_NAME_MAX];
    int status = gethostname(hostname, sizeof(hostname));
    if(status != 0){ //Check to make sure good hostname 
        cout << errno << endl;
    }

    //Set the hostname ip getting settings
    hostHints.ai_family = AF_INET;     // don't care IPv4 or IPv6
    hostHints.ai_socktype = SOCK_STREAM; // TCP stream sockets

    status = getaddrinfo(hostname, LOBBY_PORT, &hostHints, &hostAddress);
    //!Add error handling 
    
    char hostIP[INET_ADDRSTRLEN];
    const char* output = inet_ntop(AF_INET, &((sockaddr_in*)(hostAddress->ai_addr))->sin_addr, hostIP, INET_ADDRSTRLEN);
    if(output == NULL){
        cout << errno << endl;
    }

    cout << string(hostIP) << endl;

*/