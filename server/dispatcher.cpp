#include "dispatcher.h"

#include <cstdint>
#include <iostream>

void Dispatcher::register_handler(MessageType type, Handler handler) {
    handlers_[static_cast<uint8_t>(type)] = handler;
}

void Dispatcher::dispatch(std::shared_ptr<Session> session, const Frame& frame) {
    // Implementation for dispatching frames
    uint8_t type = static_cast<uint8_t>(frame.header.type);
    auto it = handlers_.find(type);
    if (it != handlers_.end()) {
        it->second(session, frame);
    } else {
        std::cerr << "Unhandled message type: " << static_cast<int>(type) << std::endl;
    }
}