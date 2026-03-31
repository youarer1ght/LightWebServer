#ifndef __RECEIVER_HPP__
#define __RECEIVER_HPP__

#include "defs.hpp"
#include "Message.hpp"
#include <mutex>
#include <sys/epoll.h>
#include <queue>
#include <atomic>

/**
 * HTTP请求接收器类
 * 负责从客户端套接字接收和解析HTTP请求，提供线程安全的请求接收功能
 * 使用epoll进行高效的事件驱动I/O处理
 */
class Receiver {
private:
    std::mutex mutex_;                          // 互斥锁，用于保证线程安全
    int sockfd_;                                // 客户端套接字文件描述符
    std::atomic<bool> running_;                 // 接收器运行状态标志（原子操作保证线程安全）
    std::vector<uint8_t> buffer_;               // 接收数据的缓冲区
    std::string remaining_;                     // 未处理完的剩余数据（用于处理不完整的请求）
    int epoll_fd_;                              // epoll 文件描述符

public:
    Receiver() = delete;  // 禁用默认构造函数，必须提供有效的套接字
    
    /**
     * @brief 构造函数
     * @param sockfd 要接收消息的客户端套接字文件描述符
     */
    Receiver(int sockfd);
    
    /**
     * @brief 析构函数
     * 自动清理资源，确保套接字正确关闭
     */
    ~Receiver();

    /**
     * @brief 关闭接收器
     * 停止接收数据并关闭套接字连接
     */
    void close();

    /**
     * @brief 接收HTTP请求
     * 从套接字读取数据，解析HTTP请求，支持Keep-Alive连接的多请求处理
     * @param request 输出参数，接收解析后的HTTP请求对象
     * @return 如果成功接收到完整请求返回true，否则返回false（如连接关闭或错误）
     */
    bool get_request(Request &request);
};

#endif