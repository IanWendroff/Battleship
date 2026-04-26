#ifndef LOBBY_MESSAGE_H
#define LOBBY_MESSAGE_H

#include <unistd.h>
#include <vector>
#include <limits.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <iostream>
#include <stdio.h>
#include <thread>

struct Player {
    int nodeID;
    char name[17];
    char ip[16];
    int socket;
};

#endif