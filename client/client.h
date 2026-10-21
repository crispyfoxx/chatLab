#pragma once
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <string>

#include "common/protocol.h"

class Client {
   public:
    Client(std::string username) : 
        username_(username), max_request_id_(1), sockfd_(-1) {}

    ~Client() { ::close(sockfd_); }

    int ConnectToServer(const std::string& host, const std::string& port);
    void Disconnect();

    Frame makeLoginRequest();

   private:
    std::string username_;

    int max_request_id_;

    int sockfd_;
};
