#include "Receiver.hpp"
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <sstream>
#include <cstring>  
#include <iostream> 

Receiver::Receiver(int sockfd) : sockfd_(sockfd), running_(true), epoll_fd_(-1) {
    // 使用预定义的缓冲区大小
    buffer_.resize(MAX_BUFFER_SIZE);

    // 创建 epoll 实例
    epoll_fd_ = epoll_create(MAX_EPOLL_EVENTS);
    if (epoll_fd_ == -1) {
        std::cerr << "[ERROR] Receiver: Failed to create epoll instance: " << strerror(errno) << std::endl;
        throw std::runtime_error("Failed to create epoll instance");
    }
    
    // 设置为非阻塞模式以提高性能
    int flags = fcntl(sockfd_, F_GETFL, 0);
    if (flags != -1) {
        fcntl(sockfd_, F_SETFL, flags | O_NONBLOCK);
    }

    // 添加套接字 epoll，使用边缘触发模式
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN | EPOLLET;  // 监听读事件 + 边缘触发
    ev.data.fd = sockfd_;           // 关联套接字

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, sockfd_, &ev) == -1) {
        std::cerr << "[ERROR] Receiver: Failed to add socket to epoll:" << strerror(errno) << std::endl;
        ::close(epoll_fd_);
        throw std::runtime_error("Failed to add socket to epoll");
    }

    // 调试信息
    std::cout << "[DEBUG] Receiver created with epoll_fd = " << epoll_fd_ << ", sockfd = " << sockfd_ << std::endl;
}

Receiver::~Receiver() {
    // 析构时确保关闭
    if (running_) {
        close();
    }

    if (epoll_fd_ != -1) {
        ::close(epoll_fd_);
        epoll_fd_ = -1;
    }
}

void Receiver::close() {
    std::unique_lock<std::mutex> lock(mutex_);
    running_ = false;

    if (epoll_fd_ != -1 && sockfd_ != -1) {
        struct epoll_event ev;
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, sockfd_, &ev);
    }

    if (sockfd_ != -1) {
        ::close(sockfd_);
        sockfd_ = -1;
    }
}

bool Receiver::get_request(Request &request) {
    std::unique_lock<std::mutex> lock(mutex_);
    
    if (!running_ || sockfd_ < 0 || epoll_fd_ < 0) {
        return false;
    }
    
    MethodTypes method_type = MethodTypes::UNKNOWN;
    std::string url, version, body;
    std::unordered_map<std::string, std::string> headers;
    int content_length = 0;
    bool request_complete = false;
    
    // 准备epoll事件数组
    struct epoll_event events[MAX_EPOLL_EVENTS];
    
    while (!request_complete && running_) {
        // 等待数据可读
        int num_events = epoll_wait(epoll_fd_, events, MAX_EPOLL_EVENTS, TIMEOUT);
        
        if (num_events == -1) {
            if (errno == EINTR) {
                continue; // 被信号中断，继续等待
            }
            std::cerr << "[ERROR] epoll_wait failed: " << strerror(errno) << std::endl;
            break;
        }
        
        if (num_events == 0) {
            // 超时，检查是否有完整请求
            continue;
        }
        
        // 接收数据
        ssize_t bytes_received = recv(sockfd_, buffer_.data(), buffer_.size(), 0);
        
        if (bytes_received == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue; // 非阻塞模式下无数据可读
            }
            std::cerr << "[ERROR] recv failed: " << strerror(errno) << std::endl;
            break;
        }
        
        if (bytes_received == 0) {
            // 客户端关闭连接
            // std::cout << "[DEBUG] Connection closed by peer, sockfd = " << sockfd_ << std::endl;
            break;
        }
        
        // 追加数据到剩余缓冲区
        remaining_.append(reinterpret_cast<char*>(buffer_.data()), bytes_received);
        
        // 解析HTTP请求
        while (!request_complete) {
            // 检查是否有完整的HTTP头部
            size_t header_end = remaining_.find("\r\n\r\n");
            if (header_end == std::string::npos) {
                break; // 头部不完整，继续接收
            }
            
            // 提取并解析头部
            std::string headers_str = remaining_.substr(0, header_end);
            remaining_ = remaining_.substr(header_end + 4); // 跳过\r\n\r\n
            
            // 解析请求行
            std::istringstream header_stream(headers_str);
            std::string request_line;
            if (!std::getline(header_stream, request_line)) {
                break;
            }
            
            // 移除可能的回车符
            if (!request_line.empty() && request_line.back() == '\r') {
                request_line.pop_back();
            }
            
            // 解析方法、URL和版本
            std::istringstream line_stream(request_line);
            std::string method_str;
            if (!(line_stream >> method_str >> url >> version)) {
                break;
            }
            
            // 确定HTTP方法类型
            if (method_str == "GET") {
                method_type = MethodTypes::GET;
            } else if (method_str == "POST") {
                method_type = MethodTypes::POST;
            } else {
                method_type = MethodTypes::UNKNOWN;
            }
            
            // 解析头部字段
            std::string header_line;
            while (std::getline(header_stream, header_line)) {
                if (header_line.empty() || header_line == "\r") {
                    break;
                }
                
                // 移除回车符
                if (!header_line.empty() && header_line.back() == '\r') {
                    header_line.pop_back();
                }
                
                // 解析键值对
                size_t colon_pos = header_line.find(':');
                if (colon_pos != std::string::npos) {
                    std::string key = header_line.substr(0, colon_pos);
                    std::string value = header_line.substr(colon_pos + 1);
                    
                    // 去除空格
                    key.erase(0, key.find_first_not_of(" \t"));
                    key.erase(key.find_last_not_of(" \t") + 1);
                    value.erase(0, value.find_first_not_of(" \t"));
                    value.erase(value.find_last_not_of(" \t") + 1);
                    
                    headers[key] = value;
                }
            }
            
            // 对于POST请求，获取Content-Length
            if (method_type == MethodTypes::POST) {
                auto it = headers.find("Content-Length");
                if (it != headers.end()) {
                    try {
                        content_length = std::stoi(it->second);
                    } catch (const std::exception& e) {
                        content_length = 0;
                    }
                }
            }
            
            // 检查请求是否完整
            if (method_type == MethodTypes::GET) {
                // GET请求只有头部，没有正文
                body = "";
                request_complete = true;
            } else if (method_type == MethodTypes::POST) {
                // POST请求需要检查正文是否完整
                if (remaining_.length() >= content_length) {
                    body = remaining_.substr(0, content_length);
                    remaining_ = remaining_.substr(content_length);
                    request_complete = true;
                } else {
                    // 正文不完整，继续接收
                    break;
                }
            } else {
                // 未知方法类型，当作GET处理
                body = "";
                request_complete = true;
            }
        }
    }
    
    if (!request_complete) {
        return false;
    }
    
    // 创建请求对象
    request = Request(method_type, url, version, body, headers);
    return true;
}