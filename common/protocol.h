#pragma once

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <zlib.h>  // crc32

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <stdexcept>
#include <vector>

// +-------------------+
// | magic       4 B   |  例如 "CHAT"
// | version     1 B   |
// | type        1 B   |
// | flags       2 B   |
// | payload_len 4 B   |  网络字节序
// | request_id  4 B   |
// | checksum    4 B   |  网络字节序
// +-------------------+
// | payload N bytes   |
// +-------------------+

#define FRAME_MAGIC 0x20040310
#define FRAME_VERSION 1
constexpr uint32_t MAX_PAYLOAD_SIZE = 1024 * 1024;  // 1 MiB
constexpr uint32_t HEADER_LEN = 20;                 // 4 + 1 + 1 + 2 + 4 + 4 + 4

struct FrameHeader {
    uint32_t magic;        // 魔数，标识协议
    uint8_t version;       // *协议版本号
    uint8_t type;          // *消息类型
    uint16_t flags;        // 标志位,当前为 0，保留以后扩展
    uint32_t payload_len;  // 负载长度，网络字节序
    uint32_t request_id;   // *请求流水号；服务器主动推送时为 0
    uint32_t checksum;     // *校验和，网络字节序
};
struct Frame {
    FrameHeader header;
    std::vector<uint8_t> payload;  // 负载数据
};

enum MessageType : uint8_t {
    LOGIN_REQ = 1,
    LOGIN_RESP = 2,

    USER_LIST = 10,

    BROADCAST_REQ = 20,
    BROADCAST_MSG = 21,

    PRIVATE_REQ = 30,
    PRIVATE_MSG = 31,

    ERROR_RESP = 40,

    PING = 50,
    PONG = 51,
    HEARTBEAT = 52
};

bool isValidLen(uint32_t len);
uint32_t calculateCRC32(const uint8_t* data, size_t len);

bool send_all(int fd, const void* data, size_t len);
bool recv_exact(int fd, void* buffer, size_t len);

std::vector<uint8_t> encode_frame(const Frame& frame);  // 把c++中的Frame转化为可以发送的网络字节流

bool send_frame(int fd, const Frame& frame);

enum class DecodeHeaderStatus {
    Ok = 0,
    InvalidMagic = 1,
    UnsupportedVersion = 2,
    PayloadTooLarge = 3,
    PayloadEmpty = 4,
    InvalidChecksum = 5,
    UnknownType = 6
};

DecodeHeaderStatus decode_header(const uint8_t* data, FrameHeader* header);

enum class ParseStatus {
    Ok = 0,
    PeerClosed = 1,
    SocketError = 2,
    InvalidMagic = 3,
    UnsupportedVersion = 4,
    FrameTooLarge = 5,
    PayloadEmpty = 6
};

ParseStatus recv_frame(int fd, Frame* frame);  // 从 TCP 字节流里面完整读出一整条 Frame
