#pragma once
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <cstdio>
#include <string>

#include "common/protocol.h"

class Client {
   public:
    Client(std::string username) : username_(username), max_request_id_(1), sockfd_(-1) {}

    ~Client() { ::close(sockfd_); }

    int ConnectToServer(const std::string& host, const std::string& port);

    void SendLoginRequest();

    void Start(const std::string& host, const std::string& port);
    void ReceiverLoop();

    void ParseCommand(const std::string& line);
    void HandleServerMessage(const Frame& frame);

    void SendBroadcastRequest(const std::string& message);  // TODO
    void SendListRequest();
    void SendPrivateRequest(const std::string& target_user, const std::string& message);
    bool ParsePrivateCommand(const std::string& line, std::string& target_user, std::string& message);

   private:
    std::string username_;

    int max_request_id_;

    std::string host_;
    std::string port_;

    int sockfd_;

    std::atomic<bool> is_running_;
    // 如果你只是需要一个‌线程安全的整数计数器或布尔开关‌，请用 std::atomic。
    // 如果你需要保护‌一段代码逻辑、多个变量或复杂对象‌，请用 std::mutex。
    // ‌不要混用‌：不要试图用 std::atomic 去替代 std::mutex
    // 来保护复杂逻辑，也不要给简单的原子变量加 std::mutex，前者不安全，后者性能差
};

// 每个 std::thread 对象必须在销毁前处于以下两种状态之一，否则程序会调用 std::terminate() 终止：
// ‌Joinable（可汇合）‌：线程正在运行或已结束但未回收资源。必须调用 join()。
// t.join()：阻塞当前线程，直到 t 执行完毕。适用于需要等待子线程结果的场景。
// ‌Detached（分离）‌：线程在后台独立运行，主线程不等待其结束。
// t.detach()：将线程与主线程分离。适用于无需关心线程何时结束的任务（如后台日志记录）。