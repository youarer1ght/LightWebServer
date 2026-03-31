#ifndef __SENDER_HPP__
#define __SENDER_HPP__

#include "defs.hpp"
#include "Message.hpp"
#include <mutex>

/**
 * HTTP响应发送器类
 * 负责将HTTP响应序列化并通过网络套接字发送给客户端
 * 提供线程安全的响应发送功能，支持多线程并发发送
 */
class Sender {
private:
    int sockfd_;                            ///< 客户端套接字文件描述符，用于发送数据
    std::mutex mutex_;                      ///< 互斥锁，保证发送操作的线程安全
    std::vector<uint8_t> buffer_;           ///< 发送数据的缓冲区，用于暂存序列化后的响应数据

public:
    Sender() = delete;  ///< 禁用默认构造函数，必须提供有效的套接字
    
    /**
     * @brief 构造函数
     * @param sockfd 用于发送消息的客户端套接字文件描述符
     * @note 发送器将与指定的套接字绑定，所有发送操作都通过该套接字进行
     */
    Sender(int sockfd);
    
    /**
     * @brief 析构函数
     * 自动清理资源，但不会关闭套接字（由上层管理套接字生命周期）
     */
    ~Sender();

    /**
     * @brief 发送HTTP响应
     * 将HTTP响应对象序列化为字节流，并通过网络套接字发送给客户端
     * @param response 要发送的HTTP响应对象
     * @return 如果成功发送返回true，如果发送失败返回false
     * @note 此方法是线程安全的，多个线程可以同时调用不同Sender实例的此方法
     */
    bool send_response(Response &response);
};

#endif