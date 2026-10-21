#include "server.h"

#include <iostream>
#include <nlohmann/json.hpp>

#include "common/protocol.h"

bool Server::Init() {
    // 创建socket，返回文件描述符file descriptor
    sockfd_ = socket(AF_INET, SOCK_STREAM, 0);  // IPV4, TCP，自动选择协议
    if (sockfd_ < 0) {
        std::perror("create socket error");
        return false;
    }
    // 方便 server 重启后立即重新 bind 端口
    int reuse = 1;
    // 服务端重启以后更容易马上重新绑定原来的地址和端口
    if (::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        std::perror("setsockopt");
        return false;
    }
    // 绑定端口
    // 填充IP和端口号
    sockaddr_in addr;  // sockaddr_in6--IPV6 sockaddr_in--IPV4 sockaddr_un UDP
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    addr.sin_addr.s_addr = INADDR_ANY;                                                // 代表0.0.0.0 监听所有地址
    if (bind(sockfd_, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) < 0) {  // bind()的sockaddr是通用接口
        std::perror("bind error");
        return false;
    }

    if (::listen(sockfd_, 16) < 0) {  // 16是等待 accept 的连接队列容量参数
        std::perror("listen");
        return false;
    }

    return true;
}

void Server::Start() {
    std::cout << "Server listening on 0.0.0.0:" << port_ << std::endl;
    // TCP server socket()->bind()->listen()->accept()->recv()
    // TCP client socket()->connect()->()send()
    while (true) {
        sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        // 接受客户端连接
        int client_sockfd = accept(sockfd_, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
        if (client_sockfd < 0) {
            std::perror("accept error");
            return;
        }
        char ip[INET_ADDRSTRLEN]{};
        inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));  // netwotk byte order -> presentation format
        std::cout << "Accepted connection from " << ip << ":" << ntohs(client_addr.sin_port) << std::endl;

        while (true) {
            Frame frame;
            auto result = recv_frame(client_sockfd, &frame);
            if (result == ParseStatus::Ok) {
                std::string text(frame.payload.begin(), frame.payload.end());
                std::cout << "receive message from " << ip << ":" << ntohs(client_addr.sin_port) << std::endl;

                // std::cout << "magic = " << frame.header.magic << "\n";
                // std::cout << "version = " << static_cast<int>(frame.header.version) << "\n";
                // std::cout << "type = " << static_cast<int>(frame.header.type) << "\n";
                // std::cout << "flags = " << frame.header.flags << "\n";
                // std::cout << "payload_len = " << frame.header.payload_len << "\n";
                // std::cout << "request_id = " << frame.header.request_id << "\n";
                // std::cout << "checksum = " << frame.header.checksum << "\n";

                std::cout << "payload = " << text << "\n";

                if (frame.header.type == LOGIN_REQ) {
                    handleLogin(frame);
                }
            } else {
                continue;
            }
        }

        close(client_sockfd);
    }
}

void Server::handleLogin(const Frame& frame) {
    std::string text(frame.payload.begin(), frame.payload.end());

    nlohmann::json j;

    try {
        j = nlohmann::json::parse(text);
    } catch (...) {
        sendError(sockfd_, frame.header.request_id, "MALFORMED_PAYLOAD", "invalid JSON");
        return;
    }

    if (!j.contains("username") || !j["username"].is_string()) {
        sendError(sockfd_, frame.header.request_id, "INVALID_PAYLOAD", "username is required");

        return;
    }

    std::string username = j["username"].get<std::string>();

    if (username.empty()) {
        sendError(sockfd_, frame.header.request_id, "INVALID_USERNAME", "username cannot be empty");

        return;
    }

    std::cout << "Login request success for user: " << username << std::endl;
    online_list_.insert(username);
    // 暂时认为登录成功
    sendLoginSuccess(sockfd_, frame.header.request_id);
}

void Server::sendError(int fd, uint32_t request_id, std::string_view error_code, std::string_view error_message) {
    nlohmann::json j;

    j["code"] = error_code;
    j["message"] = error_message;

    std::string text = j.dump();

    Frame frame;

    frame.header.version = FRAME_VERSION;
    frame.header.type = static_cast<uint8_t>(MessageType::ERROR_RESP);
    frame.header.request_id = request_id;

    frame.payload.assign(text.begin(), text.end());
    frame.header.checksum = calculateCRC32(frame.payload.data(), frame.payload.size());

    send_frame(fd, frame);
}

void Server::sendLoginSuccess(int fd, uint32_t request_id) {
    nlohmann::json j;

    j["code"] = "SUCCESS";
    j["message"] = "Login successful";

    std::string text = j.dump();

    Frame frame;

    frame.header.version = FRAME_VERSION;
    frame.header.type = static_cast<uint8_t>(MessageType::LOGIN_RESP);
    frame.header.request_id = request_id;

    frame.payload.assign(text.begin(), text.end());
    frame.header.checksum = calculateCRC32(frame.payload.data(), frame.payload.size());

    send_frame(fd, frame);
}