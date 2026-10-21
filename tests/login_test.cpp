#include <iostream>

#include "client/client.h"

int main(int argc, char* argv[]) {  // argv[0]是程序名，argv[1]是第一个参数，argv[2]是第二个参数
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <server_ip>" << std::endl;
        return 1;
    }
    const char* server_ip = argv[1];
    const char* server_port = (argc >= 3) ? argv[2] : "9000";

    Client client("Alice");
    int sockfd = client.ConnectToServer(server_ip, server_port);
    if (sockfd < 0) {
        std::cerr << "Unable to connect to " << server_ip << ":" << server_port << '\n';
        return 1;
    }
    std::cout << "Connected to " << server_ip << ":" << server_port << std::endl;

    Frame frame = client.makeLoginRequest();
    send_frame(sockfd, frame);

    while (true) {
    }
    ::close(sockfd);
    return 0;
}