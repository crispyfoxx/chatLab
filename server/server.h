#pragma once
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <set>

#include "common/protocol.h"

constexpr int OneMiB = 1024 * 1024;

class Server {
   public:
    Server(uint16_t port) : port_(port), sockfd_(-1) {}

    ~Server() {
        if (sockfd_ >= 0) {
            close(sockfd_);
        }
    }

    bool Init();

    void Start();

    void sendError(int fd, uint32_t request_id, std::string_view error_code,
                   std::string_view error_message);  // 出现错误时返给客户端的错误信息
    void handleLogin(const Frame& frame);
    void sendLoginSuccess(int fd, uint32_t request_id);

   private:
    uint16_t port_;
    int sockfd_;

    std::set<std::string> online_list_;
};