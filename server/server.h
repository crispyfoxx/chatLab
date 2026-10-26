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
#include "dispatcher.h"
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

    void sendError(std::shared_ptr<Session> session, uint32_t request_id, std::string_view error_code,
                   std::string_view error_message);  // 出现错误时返给客户端的错误信息

    void handleClient(std::shared_ptr<Session> session);  // 接入新用户时，创建新线程处理

    void handleLogin(std::shared_ptr<Session> session, const Frame& frame);
    void sendLoginSuccess(std::shared_ptr<Session> session, uint32_t request_id);

    void handleUserlist(std::shared_ptr<Session> session, const Frame& frame);

    void handlePrivate(std::shared_ptr<Session> session, const Frame& frame);
    void sendPrivate(std::shared_ptr<Session> target_session, const Frame& frame, const std::string& username,
                     const std::string& message);

    void ConfirmSend(std::shared_ptr<Session> session, const Frame& frame, MessageType type);

    void handleBroadcast(std::shared_ptr<Session> session, const Frame& frame);
    void SendBroadcast(std::shared_ptr<Session> session, const Frame& frame, const std::string& username,
                       const std::string& message);

   private:
    uint16_t port_;
    int sockfd_;

    Dispatcher dispatcher_;
    SessionManager session_manager_;
};