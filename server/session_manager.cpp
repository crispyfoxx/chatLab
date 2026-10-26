

#include "session_manager.h"

#include <mutex>
// 当使用双引号时，预处理器首先会在‌包含该指令的源文件所在的目录‌中搜索头文件
// 当使用尖括号时，预处理器‌跳过‌当前源文件所在目录，直接去‌标准系统目录‌和‌编译器配置的用户包含路径‌中搜索

void Session::send(const Frame& frame) {
    std::lock_guard<std::mutex> lock(send_mutex_);
    // 对外暴露send方法，保证所有向fd_发送的消息都经过上锁
    send_frame(fd_, frame);
}

bool SessionManager::add(const std::string& username, std::shared_ptr<Session> session) {
    std::lock_guard<std::mutex> lock(mutex_);
    // 轻量级、基于 ‌RAII（资源获取即初始化）‌ 机制的互斥锁管理工具
    // 当创建 lock_guard 对象时，它会自动调用底层互斥锁的 lock() 方法获取锁所有权。
    // 当 lock_guard对象离开作用域（例如函数返回、代码块结束或抛出异常）时，
    // 其析构函数会自动调用互斥锁的 unlock() 方法释放锁。

    // 在这个lock_guard管理的作用域，只有拿到mutex这把锁的线程才能执行这段代码
    if (sessions_.find(username) != sessions_.end()) {
        return false;
    }
    sessions_[username] = session;
    return true;
}

void SessionManager::remove(const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.erase(username);
}

std::shared_ptr<Session> SessionManager::find(const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(username);
    return (it != sessions_.end()) ? it->second : nullptr;
}

std::vector<std::string> SessionManager::listUsernames() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> usernames;
    for (const auto& pair : sessions_) {
        usernames.push_back(pair.first);
    }
    return usernames;
}

std::vector<std::shared_ptr<Session>> SessionManager::GetSessions() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::shared_ptr<Session>> sessions;
    for (const auto& pair : sessions_) {
        sessions.push_back(pair.second);
    }
    return sessions;
}