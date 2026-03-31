#ifndef __SERVER_HPP__
#define __SERVER_HPP__

#include "defs.hpp"
#include "Message.hpp"
#include "Receiver.hpp"
#include "Sender.hpp"
#include "Map.hpp"
#include "Queue.hpp"
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include <thread>

#define get_client_addr(client_id, lock)\
    std::string(\
        inet_ntoa(clientinfo_list_->at((client_id), (lock))->get_addr().sin_addr)\
    ) +\
    ":" +\
    std::to_string(\
        ntohs(clientinfo_list_->at((client_id), (lock))->get_addr().sin_port)\
    )

/**
 * 文件类型枚举
 * 表示服务器支持的不同MIME类型
 */
enum class FileTypes {
    HTML,   ///< HTML网页文件
    JPG,    ///< JPEG图像文件
    TXT     ///< 纯文本文件
};

/**
 * 文件配置结构体
 * 定义路由到实际文件的映射关系
 */
struct File {
    FileTypes type;         ///< 文件类型
    std::string path;       ///< 文件系统路径
    bool is_post = false;   ///< 是否为POST端点
};

/**
 * 获取文件类型的MIME字符串
 * @param type 文件类型枚举值
 * @return 对应的MIME类型字符串
 */
std::string get_file_type(FileTypes type);

/**
 * 客户端连接信息类
 * 管理单个客户端连接的上下文信息
 */
class ClientInfo {
private:
    int sockfd_;                        ///< 客户端套接字描述符
    sockaddr_in addr_;                  ///< 客户端地址信息
    uint8_t client_id_;                 ///< 客户端唯一标识符
    std::unique_ptr<Sender> sender_;    ///< 数据发送器
    std::unique_ptr<Receiver> receiver_;///< 数据接收器

public:
    /**
     * 构造函数
     * @param addr 客户端网络地址
     * @param sockfd 客户端套接字描述符
     * @param id 客户端ID
     * @param sender 发送器指针
     * @param receiver 接收器指针
     */
    ClientInfo(
        sockaddr_in addr,
        int sockfd,
        uint8_t id,
        Sender *sender,
        Receiver *receiver
    );
    
    /**
     * 析构函数
     * 自动关闭客户端连接
     */
    ~ClientInfo();

    /**
     * 获取客户端地址信息
     * @return sockaddr_in结构体
     */
    sockaddr_in get_addr();
    
    /**
     * 获取发送器指针
     * @return 指向Sender对象的原始指针
     */
    Sender *get_sender();
    
    /**
     * 获取接收器指针
     * @return 指向Receiver对象的原始指针
     */
    Receiver *get_receiver();
};

/**
 * HTTP服务器类
 * 实现多线程HTTP服务器核心功能
 */
class Server {
private:
    int sockfd_;                                        // 服务器监听套接字
    std::string server_name_;                           // 服务器名称
    sockaddr_in server_addr_;                           // 服务器绑定地址
    std::atomic_bool running_;                          // 服务器运行状态标志
    const std::unordered_map<std::string, File> route_; // URL路由配置表
    
    /// 线程安全的客户端信息映射表
    std::unique_ptr<Map<uint8_t, std::unique_ptr<ClientInfo> > > clientinfo_list_;
    
    /// 线程安全的客户端接收线程映射表
    std::unique_ptr<Map<uint8_t, std::unique_ptr<std::thread> > > client_recv_list_;
    
    /// 线程安全的输出消息队列
    std::unique_ptr<Queue<std::string> > output_queue_;

    /**
     * 等待客户端连接
     * 阻塞等待并接受新的客户端连接请求
     * @throws std::runtime_error 接受连接失败时抛出异常
     */
    void wait_for_client();

    /**
     * 处理客户端通信
     * 接收并处理来自指定客户端的HTTP请求
     * @param client_id 客户端唯一标识符
     * @throws std::runtime_error 无效客户端ID或通信错误
     */
    void receive_from_client(uint8_t client_id);

    /**
     * 等待所有客户端线程结束
     * 确保所有客户端处理线程正确回收资源
     */
    void join_threads();

public:
    /**
     * 服务器构造函数
     * 初始化服务器配置并绑定到指定地址
     * @param name 服务器名称（用于日志）
     * @param addr 服务器绑定的IP地址
     * @param port 服务器监听端口
     * @param route URL路由配置映射
     * @throws std::runtime_error 套接字创建、绑定或监听失败
     */
    Server(
        std::string name,
        in_addr_t addr,
        int port,
        std::unordered_map<std::string, File> route
    );
    
    /**
     * 服务器析构函数
     * 清理所有资源并等待线程结束
     */
    ~Server();

    /**
     * 启动服务器主循环
     * 持续接受客户端连接并创建处理线程
     * 循环运行直至running_标志被设置为false
     */
    void run();

    /**
     * 停止服务器运行
     * 设置停止标志并关闭所有客户端连接
     * 线程安全的停止操作
     */
    void stop();

    /**
     * 输出消息队列内容
     * 清空并打印所有排队等待输出的消息
     * @return true 如果输出队列非空并成功打印
     * @return false 如果输出队列为空
     */
    bool output_message();
};

#endif