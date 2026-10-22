#pragma once
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <cstdlib>

#include "common/protocol.h"
#include "session_manager.h"

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
    void handleLogin(const Frame& frame, std::shared_ptr<Session> session);
    void sendLoginSuccess(int fd, uint32_t request_id);
    void handleClient(std::shared_ptr<Session> session); // 接入新用户时，创建新线程处理

   private:
    uint16_t port_;
    int sockfd_;

    SessionManager session_manager_;
};