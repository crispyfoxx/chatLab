#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

class Session {
    // 核心目的封装fd和username，对每个用户的会话做管理
   public:
    explicit Session(int fd) : fd_(fd) {}

    int get_fd() const { return fd_; }

    const std::string& get_username() const { return username_; }

    void set_username(std::string username) { username_ = std::move(username); }

    bool get_logged_in() const { return logged_in_; }

    void set_logged_in(bool value) { logged_in_ = value; }

   private:
    int fd_;
    std::string username_;
    bool logged_in_ = false;
    std::mutex send_mutex_;  // 防止不同线程同时往一个用户的socket写数据
};

class SessionManager {
   public:
    bool add(const std::string& username, std::shared_ptr<Session> session);

    void remove(const std::string& username);

    std::shared_ptr<Session> find(const std::string& username);

    std::vector<std::string> listUsernames();

   private:
    std::mutex mutex_;  // 全局在线用户表不能被多个线程同时增删查改导致数据竞争

    std::unordered_map<std::string, std::shared_ptr<Session>> sessions_;
};