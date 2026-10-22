#include <iostream>

#include "client/client.h"

int main(int argc, char* argv[]) {  // argv[0]是程序名，argv[1]是第一个参数，argv[2]是第二个参数
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <username> <server_ip> [server_port]" << std::endl;
        return 1;
    }
    const char* username = argv[1];
    const char* server_ip = argv[2];
    const char* server_port = (argc >= 4) ? argv[3] : "9000";

    Client client(username);
    client.Start(server_ip, server_port);
    return 0;
}