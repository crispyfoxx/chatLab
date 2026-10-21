#include "client/client.h"

#include <iostream>
#include <nlohmann/json.hpp>

int Client::ConnectToServer(const std::string& host, const std::string& port) {
    // 1. 告诉 getaddrinfo：
    //    我要找 TCP 地址
    addrinfo hints{};

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // 2. getaddrinfo 会把结果链表放这里
    addrinfo* result = nullptr;

    int rc = ::getaddrinfo(host.c_str(), port.c_str(), &hints, &result);

    // 注意：
    // getaddrinfo 的错误不能用 errno / perror
    if (rc != 0) {
        std::cerr << "getaddrinfo: " << ::gai_strerror(rc) << '\n';

        return -1;
    }

    int sockfd = -1;

    // 3. result 可能包含多个候选地址
    for (addrinfo* p = result; p != nullptr; p = p->ai_next) {
        // 直接使用 getaddrinfo 给我们的参数
        sockfd = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);

        if (sockfd < 0) {
            continue;
        }

        // 4. 尝试连接这个地址
        if (::connect(sockfd, p->ai_addr, p->ai_addrlen) == 0) {
            // 成功
            break;
        }

        // 当前地址失败
        ::close(sockfd);
        sockfd = -1;
    }

    // 5. getaddrinfo 分配的结果必须释放
    ::freeaddrinfo(result);

    if (sockfd < 0) {
        std::cerr << "Unable to connect to " << host << ":" << port << '\n';
    } else {
        sockfd_ = sockfd;
        std::cout << "Connected to " << host << ":" << port << std::endl;
    }

    return sockfd;
}

Frame Client::makeLoginRequest() {
    nlohmann::json request_json;
    request_json["username"] = username_;

    std::string request_str = request_json.dump();

    Frame frame;

    frame.header.version = 1;
    frame.header.type = LOGIN_REQ;
    frame.header.request_id = max_request_id_++;

    frame.payload.assign(request_str.begin(), request_str.end());
    frame.header.checksum = calculateCRC32(frame.payload.data(), frame.payload.size());

    return frame;
}