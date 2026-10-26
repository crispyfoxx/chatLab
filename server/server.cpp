#include "server.h"

#include <iostream>
#include <nlohmann/json.hpp>
#include <thread>

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

    // 注册handlers
    // 登录处理
    dispatcher_.register_handler(MessageType::LOGIN_REQ, [this](std::shared_ptr<Session> session, const Frame& frame) {
        handleLogin(session, frame);
    });
    // lambda 理解成一个“适配器”：把 Server 的成员函数包装成 Dispatcher 想要的普通 Handler 形式

    // 获取Userlist处理
    dispatcher_.register_handler(
        MessageType::USER_LIST_REQ,
        [this](std::shared_ptr<Session> session, const Frame& frame) { handleUserlist(session, frame); });

    // 私聊消息处理
    dispatcher_.register_handler(
        MessageType::PRIVATE_REQ,
        [this](std::shared_ptr<Session> session, const Frame& frame) { handlePrivate(session, frame); });

    // 广播消息处理
    dispatcher_.register_handler(
        MessageType::BROADCAST_REQ,
        [this](std::shared_ptr<Session> session, const Frame& frame) { handleBroadcast(session, frame); });

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
            continue;
        }

        auto session = std::make_shared<Session>(client_sockfd);
        std::thread(&Server::handleClient, this, session).detach();
        // this 总是指向调用这个函数的那个对象

        char ip[INET_ADDRSTRLEN]{};
        inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));  // netwotk byte order -> presentation format
        std::cout << "Accepted connection from " << ip << ":" << ntohs(client_addr.sin_port) << std::endl;
    }
}

void Server::handleClient(std::shared_ptr<Session> session) {
    while (true) {
        Frame frame;
        auto result = recv_frame(session->get_fd(), &frame);
        if (result == ParseStatus::Ok) {
            std::string text(frame.payload.begin(), frame.payload.end());
            std::cout << "receive message from " << session->get_username() << std::endl;

            // std::cout << "magic = " << frame.header.magic << "\n";
            // std::cout << "version = " << static_cast<int>(frame.header.version) << "\n";
            // std::cout << "type = " << static_cast<int>(frame.header.type) << "\n";
            // std::cout << "flags = " << frame.header.flags << "\n";
            // std::cout << "payload_len = " << frame.header.payload_len << "\n";
            // std::cout << "request_id = " << frame.header.request_id << "\n";
            // std::cout << "checksum = " << frame.header.checksum << "\n";

            std::cout << "payload = " << text << "\n";

            // dispatcher根据帧头的type调用相应的handler
            dispatcher_.dispatch(session, frame);
        } else if (result == ParseStatus::PeerClosed) {
            std::cout << "Client disconnected: " << session->get_username() << std::endl;
            session_manager_.remove(session->get_username());
            break;
        } else {
            continue;
        }
    }

    close(session->get_fd());
    return;
}

void Server::handleLogin(std::shared_ptr<Session> session, const Frame& frame) {
    std::string text(frame.payload.begin(), frame.payload.end());

    nlohmann::json j;

    try {
        j = nlohmann::json::parse(text);
    } catch (...) {
        sendError(session, frame.header.request_id, "MALFORMED_PAYLOAD", "invalid JSON");
        return;
    }

    if (!j.contains("username") || !j["username"].is_string()) {
        sendError(session, frame.header.request_id, "INVALID_PAYLOAD", "username is required");

        return;
    }

    std::string username = j["username"].get<std::string>();

    if (username.empty()) {
        sendError(session, frame.header.request_id, "INVALID_USERNAME", "username cannot be empty");

        return;
    }

    if (session_manager_.find(username) != nullptr) {
        std::cout << "User already exists: " << username << std::endl;
        sendError(session, frame.header.request_id, "USER_EXISTS", "username already exists");

        return;
    }

    std::cout << "Login request success for user: " << username << std::endl;
    session->set_username(username);
    session->set_logged_in(true);
    session_manager_.add(username, session);
    // 暂时认为登录成功
    sendLoginSuccess(session, frame.header.request_id);
}

void Server::sendError(std::shared_ptr<Session> session, uint32_t request_id, std::string_view error_code,
                       std::string_view error_message) {
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

    session->send(frame);
}

void Server::sendLoginSuccess(std::shared_ptr<Session> session, uint32_t request_id) {
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

    session->send(frame);
}

void Server::handleUserlist(std::shared_ptr<Session> session, const Frame& frame) {
    // 没有什么要验证的，直接发送
    std::vector<std::string> usernames = session_manager_.listUsernames();

    nlohmann::json j;
    j["code"] = "SUCCESS";
    j["usernamelist"] = usernames;
    std::string text = j.dump();

    Frame frame_temp;
    frame_temp.header.version = FRAME_VERSION;
    frame_temp.header.type = static_cast<uint8_t>(MessageType::USER_LIST_RESP);
    frame_temp.header.request_id = frame.header.request_id;
    frame_temp.payload.assign(text.begin(), text.end());
    frame_temp.header.checksum = calculateCRC32(frame_temp.payload.data(), frame_temp.payload.size());
    session->send(frame_temp);
}

void Server::handlePrivate(std::shared_ptr<Session> session, const Frame& frame) {
    std::string text(frame.payload.begin(), frame.payload.end());

    nlohmann::json j;

    try {
        j = nlohmann::json::parse(text);
    } catch (...) {
        sendError(session, frame.header.request_id, "MALFORMED_PAYLOAD", "invalid JSON");
        return;
    }

    if (!j.contains("to") || !j["to"].is_string()) {
        sendError(session, frame.header.request_id, "INVALID_PAYLOAD", "to is required");
        return;
    }
    if (!j.contains("message") || !j["message"].is_string()) {
        sendError(session, frame.header.request_id, "INVALID_PAYLOAD", "message is required");
        return;
    }

    std::string chat_target = j["to"].get<std::string>();
    std::string message = j["message"].get<std::string>();

    if (chat_target.empty()) {
        sendError(session, frame.header.request_id, "INVALID_CHAT_TARGET", "chat target cannot be empty");
        return;
    }

    auto target_session = session_manager_.find(chat_target);
    if (!target_session) {
        std::cout << "User not exists or not online: " << chat_target << std::endl;
        sendError(session, frame.header.request_id, "USER_NOT_FOUND", "user not found or not online");
        return;
    }

    std::cout << "Private message request for user: " << chat_target << std::endl;

    // 转发消息至目标用户
    sendPrivate(target_session, frame, session->get_username(), message);
    // 确认发送成功消息
    ConfirmSend(session, frame, MessageType::PRIVATE_RESP);
}

void Server::sendPrivate(std::shared_ptr<Session> target_session, const Frame& frame, const std::string& username,
                         const std::string& message) {
    nlohmann::json j;

    j["from"] = username;
    j["message"] = message;

    std::string text = j.dump();

    Frame frame_temp = frame;
    frame_temp.header.type = MessageType::PRIVATE_MSG;
    frame_temp.payload.assign(text.begin(), text.end());
    frame_temp.header.checksum = calculateCRC32(frame_temp.payload.data(), frame_temp.payload.size());
    target_session->send(frame_temp);
}

void Server::SendBroadcast(std::shared_ptr<Session> session, const Frame& frame, const std::string& username,
                           const std::string& message) {
    nlohmann::json j;

    j["from"] = username;
    j["message"] = message;

    std::string text = j.dump();

    Frame frame_temp = frame;
    frame_temp.header.type = MessageType::BROADCAST_MSG;
    frame_temp.payload.assign(text.begin(), text.end());
    frame_temp.header.checksum = calculateCRC32(frame_temp.payload.data(), frame_temp.payload.size());

    for (const auto& target_session : session_manager_.GetSessions()) {
        if (target_session != session) {
            target_session->send(frame_temp);
        }
    }
}

void Server::ConfirmSend(std::shared_ptr<Session> session, const Frame& frame, MessageType type) {
    nlohmann::json j;

    j["status"] = "success";

    std::string text = j.dump();

    Frame frame_temp = frame;
    frame_temp.header.type = type;
    frame_temp.payload.assign(text.begin(), text.end());
    frame_temp.header.checksum = calculateCRC32(frame_temp.payload.data(), frame_temp.payload.size());
    session->send(frame_temp);
}

void Server::handleBroadcast(std::shared_ptr<Session> session, const Frame& frame) {
    std::string text(frame.payload.begin(), frame.payload.end());

    nlohmann::json j;

    try {
        j = nlohmann::json::parse(text);
    } catch (...) {
        sendError(session, frame.header.request_id, "MALFORMED_PAYLOAD", "invalid JSON");
        return;
    }

    if (!j.contains("message") || !j["message"].is_string()) {
        sendError(session, frame.header.request_id, "INVALID_PAYLOAD", "message is required");
        return;
    }

    std::string message = j["message"].get<std::string>();

    // 群发
    SendBroadcast(session, frame, session->get_username(), message);
    // 确认发送成功消息
    ConfirmSend(session, frame, MessageType::BROADCAST_RESP);
}