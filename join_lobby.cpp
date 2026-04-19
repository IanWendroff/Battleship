#include "lobby_message.h"
#include <random>
#include <string>
#include <cstring>
#include <mutex>

#define LOBBY_PORT 6010
using namespace std;

int startGame(int numberOfNodes, vector<Player> players, int playerNodeID, int startingPlayerID);

int hostSocket;
int numPlayers;
char name[17];

int connectToHost(string hostIP){
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0){
        cout << "SOCKETPOOL CONNECTIONS FAILED" << endl; 
        close(sock);
        return -1;
    }

    //Fill in the structure of the socket connection
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_addr.s_addr = inet_addr(hostIP.c_str());
    address.sin_family = AF_INET;
    address.sin_port = htons(LOBBY_PORT);
    
    //Connect
    while(connect(sock, (sockaddr*)&address, sizeof(address)) != 0){
        sleep(1);
        return -1;
    }
    hostSocket = sock;
    cout << "Connected to Host " << endl;
    
    bool loop = true;
    cout << "Please enter display name (16 characters max):" << endl;
    while(loop){
        cin >> name;
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        if(strlen(name) > 16){
            cout << "Please enter a name 16 characters max" << endl;
        } else {
            loop = false;
        }
    }
    int bytes_sent = send(hostSocket, name, sizeof(name), 0);
    return 0;
}

int main(int argc, char **argv){
    char hostIP[INET_ADDRSTRLEN];
    bool loop = true;
    cout << "Please enter the host IP address" << endl;
    while(loop){
        cin >> hostIP;
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        if(connectToHost(string(hostIP)) == -1){
            cout << "Please enter a valid IP address" << endl;
        } else {
            loop = false;
        }
    }

    int bytes_received = recv(hostSocket, &numPlayers, sizeof(numPlayers), MSG_WAITALL);//Recv numPlayers to know how many to wait for
    if (bytes_received <= 0) {
        perror("recv failed, did not recieve numPlayers");
        return 0;
    } else {
        cout << "Total player count: " << numPlayers << endl;
    }

    Player players[numPlayers];
    bytes_received = recv(hostSocket, players, numPlayers * sizeof(Player), MSG_WAITALL);
    if(bytes_received < 0){
        cout << "Didn't recv all players info" << endl;
    }

    int startingPlayer = -1;
    bytes_received = recv(hostSocket, &startingPlayer, sizeof(startingPlayer), MSG_WAITALL);
    if(bytes_received < 0){
        cout << "Didn't recv all players info" << endl;
    }

    int playerNodeID = -1;
    //open up game and send all player info
    for(int i = 1; i < numPlayers; i++){
        if(strcmp(players[i].name, name){
            playerNodeID = i;
        }
    }

    startGame(numPlayers, players, playerNodeID, startingPlayer);
}