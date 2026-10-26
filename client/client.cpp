#include "client/client.h"

#include <iostream>
#include <nlohmann/json.hpp>
#include <thread>

bool starts_with(const std::string& str, const std::string& prefix) {
    if (prefix.size() > str.size()) {
        return false;
    }
    // compare(pos, count, other_string)
    // 从 str 的位置 0 开始，比较 prefix.size() 个字符，是否与 prefix 相等
    return str.compare(0, prefix.size(), prefix) == 0;
}

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

void Client::SendLoginRequest() {
    nlohmann::json request_json;
    request_json["username"] = username_;

    std::string request_str = request_json.dump();

    Frame frame;

    frame.header.version = 1;
    frame.header.type = LOGIN_REQ;
    frame.header.request_id = max_request_id_++;

    frame.payload.assign(request_str.begin(), request_str.end());
    frame.header.checksum = calculateCRC32(frame.payload.data(), frame.payload.size());

    send_frame(sockfd_, frame);
}

void Client::Start(const std::string& host, const std::string& port) {
    sockfd_ = ConnectToServer(host, port);
    if (sockfd_ < 0) {
        std::cerr << "Unable to connect to " << host << ":" << port << '\n';
        return;
    }
    std::cout << "Connected to " << host << ":" << port << std::endl;
    host_ = host;
    port_ = port;

    // 启动接收线程
    is_running_ = true;
    std::thread receiver_thread(&Client::ReceiverLoop, this);

    // 发送登录信息
    SendLoginRequest();

    // 主线程接收命令行输入
    std::cout << "Enter command: " << std::endl;
    std::cout << "/list - List all users" << std::endl;
    std::cout << "/msg <user> <message> - Send a private message" << std::endl;
    std::cout << "/all <message> - Send a broadcast message" << std::endl;
    std::cout << "/quit - Quit the application" << std::endl;
    std::string line;
    while (is_running_ && std::getline(std::cin, line)) {
        ParseCommand(line);
    }

    is_running_ = false;

    // 让正在 recv() 阻塞的线程醒过来
    shutdown(sockfd_, SHUT_RDWR);
    receiver_thread.join();

    close(sockfd_);
    return;
}

void Client::ReceiverLoop() {
    while (is_running_) {
        Frame frame;
        auto result = recv_frame(sockfd_, &frame);
        if (result == ParseStatus::Ok) {
            std::string text(frame.payload.begin(), frame.payload.end());
            std::cout << "receive message from host " << host_ << ":" << port_ << std::endl;

            // std::cout << "magic = " << frame.header.magic << "\n";
            // std::cout << "version = " << static_cast<int>(frame.header.version) << "\n";
            // std::cout << "type = " << static_cast<int>(frame.header.type) << "\n";
            // std::cout << "flags = " << frame.header.flags << "\n";
            // std::cout << "payload_len = " << frame.header.payload_len << "\n";
            // std::cout << "request_id = " << frame.header.request_id << "\n";
            // std::cout << "checksum = " << frame.header.checksum << "\n";

            std::cout << "payload = " << text << "\n";

            HandleServerMessage(frame);
        } else if (result == ParseStatus::PeerClosed) {
            std::cout << "Lost connection to host " << std::endl;
            is_running_ = false;
            break;
        } else {
            continue;
        }
    }
}

void Client::ParseCommand(const std::string& line) {
    if (line == "/quit") {
        is_running_ = false;
        return;
    }

    if (starts_with(line, "/all ")) {
        std::string message = line.substr(5);
        // 从第5个位置截断，取后面的，可选参数len表示最多截取的字符数
        SendBroadcastRequest(message);
    }

    else if (starts_with(line, "/msg ")) {
        // 例如：
        // /msg bob hello
        std::string target_user;
        std::string message;
        bool command = ParsePrivateCommand(line, target_user, message);

        if (!command) {
            std::cout << "usage: /msg <user> <message>\n";
            return;
        }

        SendPrivateRequest(target_user, message);
    }

    else if (starts_with(line, "/list")) {
        SendListRequest();
    }

    else {
        std::cout << "unknown command\n";
    }

    return;
}

void Client::HandleServerMessage(const Frame& frame) {
    switch (frame.header.type) {
        case MessageType::LOGIN_RESP:
            std::cout << "Login successful!" << std::endl;
            break;
        case MessageType::USER_LIST_RESP:
            std::cout << "User list received!" << std::endl;
            PrintPayload(frame);
            break;
        case MessageType::BROADCAST_RESP:
            std::cout << "Broadcast message sent successfully!" << std::endl;
            break;
        case MessageType::PRIVATE_RESP:
            std::cout << "Private message sent successfully!" << std::endl;
            break;
        case MessageType::BROADCAST_MSG:
            std::cout << "Broadcast message received!" << std::endl;
            PrintPayload(frame);
            break;
        case MessageType::PRIVATE_MSG:
            std::cout << "Private message received!" << std::endl;
            PrintPayload(frame);
            break;
        default:
            std::cout << "Unknown message type!" << std::endl;
            break;
    }
}

void Client::SendListRequest() {
    nlohmann::json j;

    j["user"] = username_;

    std::string text = j.dump();

    Frame frame;

    frame.header.version = FRAME_VERSION;
    frame.header.type = static_cast<uint8_t>(MessageType::USER_LIST_REQ);
    frame.header.request_id = max_request_id_;

    frame.payload.assign(text.begin(), text.end());
    frame.header.checksum = calculateCRC32(frame.payload.data(), frame.payload.size());

    send_frame(sockfd_, frame);
}

void Client::SendPrivateRequest(const std::string& target_user, const std::string& message) {
    nlohmann::json j;

    j["to"] = target_user;
    j["message"] = message;

    std::string text = j.dump();

    Frame frame;

    frame.header.version = FRAME_VERSION;
    frame.header.type = static_cast<uint8_t>(MessageType::PRIVATE_REQ);
    frame.header.request_id = max_request_id_;

    frame.payload.assign(text.begin(), text.end());
    frame.header.checksum = calculateCRC32(frame.payload.data(), frame.payload.size());

    send_frame(sockfd_, frame);
}

bool Client::ParsePrivateCommand(const std::string& line, std::string& target_user, std::string& message) {
    std::istringstream iss(line);
    std::string command;

    // 1. 读取命令词 "/msg"
    if (!(iss >> command) || command != "/msg") {
        return false;
    }

    // 2. 读取用户名
    if (!(iss >> target_user)) {
        return false;
    }

    // 3. 读取剩余所有内容为消息
    // std::getline 会读取直到行尾的所有字符，包括空格
    if (!std::getline(iss, message)) {
        return false;
    }

    // 注意：getline 可能会读取到前导空格（如果用户名和消息间有多个空格）
    // 通常建议去除 message 的前导空格
    if (!message.empty() && message[0] == ' ') {
        message.erase(0, 1);  // 简单去除第一个空格，或使用更完善的 trim 函数
    }

    return !target_user.empty() && !message.empty();
}

void Client::SendBroadcastRequest(const std::string& message) {
    nlohmann::json j;

    j["message"] = message;

    std::string text = j.dump();

    Frame frame;

    frame.header.version = FRAME_VERSION;
    frame.header.type = static_cast<uint8_t>(MessageType::BROADCAST_REQ);
    frame.header.request_id = max_request_id_;

    frame.payload.assign(text.begin(), text.end());
    frame.header.checksum = calculateCRC32(frame.payload.data(), frame.payload.size());

    send_frame(sockfd_, frame);
}