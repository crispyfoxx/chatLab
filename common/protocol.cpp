#include "protocol.h"

#include <array>
#include <iomanip>
#include <iostream>
#include <stdexcept>

bool isValidLen(uint32_t len) { return len > 0 && len <= MAX_PAYLOAD_SIZE; }

uint32_t calculateCRC32(const uint8_t* data, size_t len) { return crc32(0L, data, len); }

void print_hex(const std::vector<uint8_t>& data) {
    for (uint8_t byte : data) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte) << ' ';
    }

    std::cout << std::dec << '\n';
}

bool send_all(int fd, const void* data, size_t len) {
    const auto* p = static_cast<const char*>(data);
    size_t sent = 0;
    while (sent < len) {  // ssize_t 是 C/C++ 编程中的有符号整数类型，专门用于表示可正可负的字节数量，常见于 read/write
                          // 等系统调用的返回值‌
        ssize_t n =
            send(fd, p + sent, len - sent,
                 MSG_NOSIGNAL);  // MSG_NOSIGNAL在对方已关闭连接时阻止内核发送 SIGPIPE 信号，从而避免进程被直接终止
        if (n > 0) {
            sent += static_cast<size_t>(n);
        } else if (n < 0 && errno == EINTR) {
            continue;
        } else {
            return false;
        }
    }
    return true;
}
bool recv_exact(int fd, void* buffer, size_t len) {
    auto* p = static_cast<char*>(buffer);
    size_t received = 0;
    while (received < len) {
        ssize_t n = recv(fd, p + received, len - received, 0);
        if (n > 0) {
            received += static_cast<size_t>(n);
        } else if (n < 0 && errno == EINTR) {  // 代表这次调用在成功发送任何数据之前被中断
            continue;
        } else {
            return false;
        }
    }
    return true;
}

std::vector<uint8_t> encode_frame(const Frame& frame) {  // 把c++中的Frame转化为可以发送的网络字节流
    uint32_t payload_size = static_cast<uint32_t>(frame.payload.size());
    std::cout << "Encoding frame, size = " << payload_size << "\n";
    if (payload_size > MAX_PAYLOAD_SIZE) {
        throw std::runtime_error("frame payload too large");
    }

    std::vector<uint8_t> buffer(HEADER_LEN + payload_size);
    size_t offset = 0;

    // magic number
    uint32_t magic_net = htonl(FRAME_MAGIC);
    std::memcpy(buffer.data() + offset, &magic_net, sizeof(magic_net));  // data()暴露底层连续存储的地址
    offset += 4;

    // version
    buffer[offset] = frame.header.version;
    offset += 1;

    // message type
    buffer[offset] = frame.header.type;
    offset += 1;

    // flags
    uint16_t flags_net = htons(0);  // 先填0，以后课扩展
    std::memcpy(buffer.data() + offset, &flags_net, sizeof(flags_net));
    offset += 2;

    // payload length
    uint32_t payload_len_net = htonl(payload_size);
    std::memcpy(buffer.data() + offset, &payload_len_net, sizeof(payload_len_net));
    offset += 4;

    // request id
    uint32_t request_id_net = htonl(frame.header.request_id);
    std::memcpy(buffer.data() + offset, &request_id_net, sizeof(request_id_net));
    offset += 4;

    // checksum
    uint32_t checksum_net = htonl(frame.header.checksum);
    std::memcpy(buffer.data() + offset, &checksum_net, sizeof(checksum_net));
    offset += 4;

    // payload
    std::cout << "payload = " << std::string(frame.payload.begin(), frame.payload.end()) << "\n";
    if (!frame.payload.empty()) {
        // payload是utf-8或json时本身是一串byte，不需要转换网络字节序，只有像uint32这样跨多个字节的整数需要转换
        std::memcpy(buffer.data() + offset, frame.payload.data(), frame.payload.size());
    }

    return buffer;
}

bool send_frame(int fd, const Frame& frame) {
    std::vector<uint8_t> data = encode_frame(frame);

    // std::cout << "sending frame hex:\n";
    // print_hex(data);

    return send_all(fd, data.data(), data.size());
}

DecodeHeaderStatus decode_header(const uint8_t* data, FrameHeader* header) {
    // 拿到从网络收到的 16 字节协议头，把它解释成有意义的数据，同时检查它是不是合法。
    // void* 表示“不解释这块内存”，uint8_t* 表示“我要按字节解释这块内存”，方便用如data[1] data[2]的形式逐个查看。

    size_t offset = 0;

    uint32_t magic_net;
    std::memcpy(&magic_net, data + offset, sizeof(magic_net));
    header->magic = ntohl(magic_net);
    offset += 4;

    header->version = data[offset];
    offset += 1;

    header->type = data[offset];
    offset += 1;

    uint16_t flags_net;
    std::memcpy(&flags_net, data + offset, sizeof(flags_net));
    header->flags = ntohs(flags_net);
    offset += 2;

    uint32_t payload_len_net;
    std::memcpy(&payload_len_net, data + offset, sizeof(payload_len_net));
    header->payload_len = ntohl(payload_len_net);
    offset += 4;

    uint32_t request_id_net;
    std::memcpy(&request_id_net, data + offset, sizeof(request_id_net));
    header->request_id = ntohl(request_id_net);
    offset += 4;

    uint32_t checksum_net;
    std::memcpy(&checksum_net, data + offset, sizeof(checksum_net));
    header->checksum = ntohl(checksum_net);
    offset += 4;

    // 校验
    if (header->magic != FRAME_MAGIC) {
        return DecodeHeaderStatus::InvalidMagic;
    }

    if (header->version != FRAME_VERSION) {
        return DecodeHeaderStatus::UnsupportedVersion;
    }

    if (header->payload_len > MAX_PAYLOAD_SIZE) {
        return DecodeHeaderStatus::PayloadTooLarge;
    }

    if (header->payload_len <= 0) {
        return DecodeHeaderStatus::PayloadEmpty;
    }

    return DecodeHeaderStatus::Ok;
}

ParseStatus recv_frame(int fd, Frame* frame) {  // 从 TCP 字节流里面完整读出一整条 Frame
    std::array<uint8_t, HEADER_LEN> header_buffer;
    if (recv_exact(fd, header_buffer.data(), HEADER_LEN) == false) {
        return ParseStatus::PeerClosed;
    }

    FrameHeader header;  //    InvalidMagic = 1,UnsupportedVersion = 2,PayloadTooLarge = 3, PayloadEmpty = 4,
                         //    InvalidChecksum = 5,UnknownType = 6
    DecodeHeaderStatus decode_status = decode_header(header_buffer.data(), &header);
    if (decode_status == DecodeHeaderStatus::InvalidMagic) {
        return ParseStatus::InvalidMagic;
    }
    if (decode_status == DecodeHeaderStatus::UnsupportedVersion) {
        return ParseStatus::UnsupportedVersion;
    }
    if (decode_status == DecodeHeaderStatus::PayloadTooLarge) {
        return ParseStatus::FrameTooLarge;
    }
    if (decode_status == DecodeHeaderStatus::PayloadEmpty) {
        return ParseStatus::PayloadEmpty;
    }
    frame->header = std::move(header);  // 转换成一个“允许被移动”的右值, 避免复制开销

    // Read the payload
    frame->payload.resize(frame->header.payload_len);
    if (recv_exact(fd, frame->payload.data(), frame->header.payload_len) == false) {
        return ParseStatus::SocketError;
    }

    return ParseStatus::Ok;
}

void PrintPayload(Frame frame) {
    std::string text(frame.payload.begin(), frame.payload.end());
    std::cout << text << std::endl;
}