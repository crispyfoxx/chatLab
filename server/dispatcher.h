#pragma once

#include <functional>
#include <unordered_map>

#include "common/protocol.h"
#include "session_manager.h"

class Dispatcher {
   public:
    using Handler = std::function<void(std::shared_ptr<Session>, const Frame&)>;
    // 如果 using 声明在类定义内部，则该别名属于该类的作用域。可以通过类名访问，或者在类的成员函数中直接使用。
    void register_handler(MessageType type, Handler handler);

    void dispatch(std::shared_ptr<Session> session, const Frame& frame);

   private:
    std::unordered_map<uint8_t, Handler> handlers_;
};