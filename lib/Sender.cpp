#include "Sender.hpp"
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <algorithm>

Sender::Sender(int sockfd) : sockfd_(sockfd) {
    // 使用预定义的缓冲区大小，但实际发送时会根据响应大小调整
    buffer_.reserve(MAX_BUFFER_SIZE);
}

Sender::~Sender() {
    // 析构时不关闭套接字，由上层管理套接字生命周期
    // 但可以清理缓冲区资源
    std::vector<uint8_t>().swap(buffer_); // 彻底释放内存
}

bool Sender::send_response(Response &response) {
    std::unique_lock<std::mutex> lock(mutex_);
    
    // 检查套接字有效性
    if (sockfd_ < 0) {
        std::cerr << "[ERROR] Sender: Invalid socket descriptor" << std::endl;
        return false;
    }
    
    // 序列化响应到缓冲区
    ssize_t total_size = response.serialize(buffer_);
    if (total_size <= 0) {
        std::cerr << "[ERROR] Sender: Failed to serialize response" << std::endl;
        return false;
    }
    
    // 分段发送数据，避免一次性发送过大数据包
    const uint8_t* data_ptr = buffer_.data();
    size_t remaining_bytes = total_size;
    size_t total_sent = 0;
    
    while (remaining_bytes > 0) {
        // 计算本次发送的数据量（不超过缓冲区大小）
        size_t chunk_size = std::min(remaining_bytes, 
                                     static_cast<size_t>(MAX_BUFFER_SIZE));
        
        // 发送数据块
        ssize_t sent_bytes = send(sockfd_, data_ptr + total_sent, chunk_size, 0);
        
        if (sent_bytes == -1) {
            // 根据错误类型决定是否重试
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 非阻塞模式下缓冲区满，稍后重试
                continue;
            } else if (errno == EINTR) {
                // 被信号中断，继续发送
                continue;
            } else {
                // 其他错误，发送失败
                std::cerr << "[ERROR] Sender: send() failed: " 
                          << strerror(errno) << std::endl;
                return false;
            }
        }
        
        // 更新发送进度
        total_sent += sent_bytes;
        remaining_bytes -= sent_bytes;
        
        // 如果发送的字节数为0，表示连接已关闭
        if (sent_bytes == 0) {
            std::cerr << "[WARN] Sender: Connection closed by peer" << std::endl;
            return false;
        }
    }
    
    // 验证是否所有数据都已发送
    if (total_sent != static_cast<size_t>(total_size)) {
        std::cerr << "[WARN] Sender: Partial send (" << total_sent 
                  << "/" << total_size << " bytes)" << std::endl;
    }
    
    // 发送成功
    return true;
}