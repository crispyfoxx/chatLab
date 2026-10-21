#include <iostream>

#include "client/client.h"

int main(int argc, char* argv[]) {  // argv[0]是程序名，argv[1]是第一个参数，argv[2]是第二个参数
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <server_ip>" << std::endl;
        return 1;
    }
    const char* server_ip = argv[1];
    const char* server_port = (argc >= 3) ? argv[2] : "9000";

    // int sockfd = ConnectToServer(server_ip, server_port);
    // if (sockfd < 0) {
    //     std::cerr << "Unable to connect to " << server_ip << ":" << server_port << '\n';
    //     return 1;
    // }
    // std::cout << "Connected to " << server_ip << ":" << server_port << std::endl;

    // const std::string message = "Hello, Server!";
    // if (send(sockfd, message.c_str(), message.size(), 0) < 0) {
    //     std::perror("client send");
    //     ::close(sockfd);
    //     return 1;
    // }
    // std::cout << "Sent message: " << message << std::endl;

    // while (true) {
    //     char buffer[1024];
    //     ssize_t bytes_received = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
    //     if (bytes_received < 0) {
    //         std::perror("recv");
    //         break;
    //     } else if (bytes_received == 0) {
    //         std::cout << "Server closed the connection." << std::endl;
    //         break;
    //     }
    //     buffer[bytes_received] = '\0';  // Null-terminate the received data
    //     std::cout << "Received from server: " << buffer << std::endl;
    // }

    // ::close(sockfd);
    return 0;
}