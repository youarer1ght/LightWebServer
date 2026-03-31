#include "Server.hpp"
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <chrono>
#include <ctime>
#include <cstring>
#include <sstream>
#include <netinet/tcp.h>
#include <algorithm>

std::string get_file_type(FileTypes type) {
    static const std::unordered_map<FileTypes, std::string> type_map = {
        {FileTypes::HTML, "text/html"},
        {FileTypes::JPG,  "image/jpeg"},
        {FileTypes::TXT,  "text/plain"}
    };
    
    auto it = type_map.find(type);
    if (it != type_map.end()) {
        return it->second;
    }
    throw std::invalid_argument("Invalid file type");
}

ClientInfo::ClientInfo(
    sockaddr_in addr,
    int sockfd,
    uint8_t id,
    Sender *sender,
    Receiver *receiver
) : sockfd_(sockfd), addr_(addr), client_id_(id),
    sender_(sender), receiver_(receiver) {}

ClientInfo::~ClientInfo() {
    if (sockfd_ >= 0) {
        close(sockfd_);
    }
}

sockaddr_in ClientInfo::get_addr() {
    return addr_;
}

Sender *ClientInfo::get_sender() {
    return sender_.get();
}

Receiver *ClientInfo::get_receiver() {
    return receiver_.get();
}

Server::Server(
    std::string name,
    in_addr_t addr,
    int port,
    std::unordered_map<std::string, File> route
) : server_name_(std::move(name)), running_(true), route_(std::move(route)) {
    
    server_addr_ = {0};
    server_addr_.sin_family = AF_INET;
    server_addr_.sin_port = htons(port);
    server_addr_.sin_addr.s_addr = addr;
    
    sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd_ < 0) {
        throw std::runtime_error("Failed to create socket");
    }
    
    // 设置套接字选项
    int opt = 1;
    if (setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        close(sockfd_);
        throw std::runtime_error("Failed to set socket options");
    }
    
    if (bind(sockfd_, cast_sockaddr_in(server_addr_), sizeof(server_addr_)) < 0) {
        close(sockfd_);
        throw std::runtime_error("Failed to bind socket");
    }
    
    if (listen(sockfd_, MAX_CLIENT_NUM) < 0) {
        close(sockfd_);
        throw std::runtime_error("Failed to listen on socket");
    }
    
    // 初始化容器
    clientinfo_list_ = std::make_unique<Map<uint8_t, std::unique_ptr<ClientInfo>>>();
    client_recv_list_ = std::make_unique<Map<uint8_t, std::unique_ptr<std::thread>>>();
    output_queue_ = std::make_unique<Queue<std::string>>();

    output_queue_->push("[" + server_name_ + "] Server initialized on port " + std::to_string(port));
}

Server::~Server() {
    output_queue_->push("[INFO] Releasing server resources...");
    output_message();
    
    stop();
    join_threads();
    
    if (sockfd_ >= 0) {
        close(sockfd_);
    }
    
    output_queue_->push("[INFO] Server released successfully.");
    output_message();
}

void Server::wait_for_client() {
    sockaddr_in client_addr = {0};
    socklen_t addr_len = sizeof(client_addr);
    
    int client_sockfd = accept(sockfd_, cast_sockaddr_in(client_addr), &addr_len);
    
    if (!running_) {
        if (client_sockfd >= 0) ::close(client_sockfd);
        return;
    }
    
    if (client_sockfd < 0) {
        throw std::runtime_error("Failed to accept connection");
    }
    
    // 分配客户端ID
    uint8_t client_id = 1;
    std::unique_lock<std::mutex> info_lock(clientinfo_list_->get_mutex());
    while (clientinfo_list_->check_exist(client_id, info_lock)) {
        client_id++;
        if (client_id == 0) {
            ::close(client_sockfd);
            throw std::runtime_error("No available client IDs");
        }
    }
    
    // 尝试创建Receiver和Sender
    Receiver* receiver = nullptr;
    Sender* sender = nullptr;
    
    try {
        receiver = new Receiver(client_sockfd);
        sender = new Sender(client_sockfd);
    } catch (const std::exception& e) {
        // 记录错误并清理
        output_queue_->push("[ERR] Failed to create client handler for client " + 
                           std::to_string(client_id) + ": " + e.what());
        
        // 清理已分配的资源
        delete receiver;
        delete sender;
        ::close(client_sockfd);
        
        // 注意：这里不抛出异常，只是返回，让服务器继续运行
        // 可以选择重新抛出异常，取决于错误处理策略
        return;
    }
    
    // 创建客户端信息（转移所有权）
    auto client_info = std::make_unique<ClientInfo>(
        client_addr, client_sockfd, client_id, 
        sender, receiver  // 直接传递原始指针，ClientInfo会接管所有权
    );
    
    // 插入到客户端信息列表
    clientinfo_list_->insert_or_assign(client_id, std::move(client_info), info_lock);
    
    // 创建接收线程
    std::unique_lock<std::mutex> thread_lock(client_recv_list_->get_mutex());
    
    // 清理旧线程（如果存在）- 安全起见，虽然正常情况下不应该存在
    auto old_thread = client_recv_list_->find(client_id, thread_lock);
    if (old_thread != client_recv_list_->end(thread_lock)) {
        if (old_thread->second->joinable()) {
            old_thread->second->join();
        }
        client_recv_list_->erase(client_id, thread_lock);
    }
    
    // 创建新的接收线程
    try {
        auto recv_thread = std::make_unique<std::thread>(
            &Server::receive_from_client, this, client_id
        );
        
        client_recv_list_->insert_or_assign(client_id, std::move(recv_thread), thread_lock);
        
        // 记录连接成功
        output_queue_->push("[INFO] Client " + std::to_string(client_id) + 
                           " connected from " + 
                           inet_ntoa(client_addr.sin_addr) + ":" +
                           std::to_string(ntohs(client_addr.sin_port)));
                           
    } catch (const std::exception& e) {
        // 线程创建失败，清理资源
        output_queue_->push("[ERR] Failed to create thread for client " + 
                           std::to_string(client_id) + ": " + e.what());
        
        // 从客户端列表中移除
        clientinfo_list_->erase(client_id, info_lock);
        
        // 注意：Receiver和Sender会在ClientInfo析构时自动清理
        // 因为我们还没有成功创建ClientInfo，所以需要手动清理
        delete receiver;
        delete sender;
        ::close(client_sockfd);
    }
}

void Server::receive_from_client(uint8_t client_id) {
    // 获取客户端句柄
    std::unique_lock<std::mutex> lock(clientinfo_list_->get_mutex());
    if (!clientinfo_list_->check_exist(client_id, lock)) {
        return;
    }
    
    Sender* sender = clientinfo_list_->at(client_id, lock)->get_sender();
    Receiver* receiver = clientinfo_list_->at(client_id, lock)->get_receiver();
    lock.unlock();
    
    Request request;
    if (!receiver->get_request(request) || !running_) {
        // 清理客户端
        std::unique_lock<std::mutex> cleanup_lock(clientinfo_list_->get_mutex());
        clientinfo_list_->erase(client_id, cleanup_lock);
        return;
    }
    
    // 记录请求
    std::unique_lock<std::mutex> log_lock(clientinfo_list_->get_mutex());
    std::string method_str = (request.get_method_type() == MethodTypes::GET) ? "GET" : "POST";
    output_queue_->push(
        "[INFO] " + method_str + " " + request.get_url() + 
        " " + request.get_version() + " from " + 
        get_client_addr(client_id, log_lock)
    );
    log_lock.unlock();
    
    // 处理请求
    StatusCodes status_code;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
    
    if (request.get_method_type() == MethodTypes::GET) {
        std::string url = request.get_url();
        
        if (url == "/info/server") {
            std::ifstream file("assets/txt/test.txt", std::ios::binary);
            if (file) {
                status_code = StatusCodes::OK;
                std::stringstream buffer;
                buffer << file.rdbuf();
                body = buffer.str();
                headers["Content-Type"] = "text/plain";
            } else {
                status_code = StatusCodes::INTERNAL_SERVER_ERROR;
                body = "<html><body><h1>500 Internal Server Error</h1></body></html>";
                headers["Content-Type"] = "text/html";
            }
        } else if (route_.find(url) == route_.end()) {
            status_code = StatusCodes::NOT_FOUND;
            body = "<html><body><h1>404 Not Found</h1></body></html>";
            headers["Content-Type"] = "text/html";
        } else {
            File file = route_.at(url);
            std::ifstream file_stream(file.path, std::ios::binary);
            if (file_stream) {
                status_code = StatusCodes::OK;
                std::stringstream buffer;
                buffer << file_stream.rdbuf();
                body = buffer.str();
                headers["Content-Type"] = get_file_type(file.type);
            } else {
                status_code = StatusCodes::INTERNAL_SERVER_ERROR;
                body = "<html><body><h1>500 Internal Server Error</h1></body></html>";
                headers["Content-Type"] = "text/html";
            }
        }
    } else if (request.get_method_type() == MethodTypes::POST) {
        if (request.get_url() != "/dopost") {
            status_code = StatusCodes::NOT_FOUND;
            body = "<html><body><h1>404 Not Found</h1></body></html>";
            headers["Content-Type"] = "text/html";
        } else {
            // 解析表单数据
            std::unordered_map<std::string, std::string> params;
            std::string body_data = request.get_body();
            std::stringstream ss(body_data);
            std::string pair;
            
            while (std::getline(ss, pair, '&')) {
                size_t delim_pos = pair.find('=');
                if (delim_pos != std::string::npos) {
                    std::string key = pair.substr(0, delim_pos);
                    std::string value = pair.substr(delim_pos + 1);
                    params[key] = value;
                }
            }
            
            if (params.find("login") != params.end() && params.find("pass") != params.end()) {
                if (params["login"] == USERNAME && params["pass"] == PASSWORD) {
                    status_code = StatusCodes::OK;
                    body = "<html><body><h1>Login Success</h1></body></html>";
                } else {
                    status_code = StatusCodes::FORBIDDEN;
                    body = "<html><body><h1>Login Failed</h1></body></html>";
                }
            } else {
                status_code = StatusCodes::BAD_REQUEST;
                body = "<html><body><h1>400 Bad Request</h1></body></html>";
            }
            headers["Content-Type"] = "text/html";
        }
    } else {
        status_code = StatusCodes::BAD_REQUEST;
        body = "<html><body><h1>400 Bad Request</h1></body></html>";
        headers["Content-Type"] = "text/html";
    }
    
    headers["Content-Length"] = std::to_string(body.length());
    
    // 记录响应
    log_lock.lock();
    output_queue_->push(
        "[INFO] " + status_code_to_string(status_code) +
        " " + request.get_url() + " to " + 
        get_client_addr(client_id, log_lock)
    );
    log_lock.unlock();
    
    // 发送响应
    Response response(status_code, request.get_version(), headers, body);
    sender->send_response(response);
    
    // 清理客户端
    std::unique_lock<std::mutex> cleanup_lock(clientinfo_list_->get_mutex());
    clientinfo_list_->erase(client_id, cleanup_lock);
}

void Server::join_threads() {
    std::unique_lock<std::mutex> lock(client_recv_list_->get_mutex());
    
    for (auto it = client_recv_list_->begin(lock); 
         it != client_recv_list_->end(lock); ++it) {
        if (it->second->joinable()) {
            it->second->join();
        }
    }
    
    client_recv_list_->clear(lock);
}

void Server::run() {
    output_queue_->push("[INFO] Server started and listening for connections");
    
    while (running_) {
        try {
            wait_for_client();
        } catch (const std::exception& e) {
            if (running_) {
                output_queue_->push("[ERR] " + std::string(e.what()));
            }
        }
    }
    
    output_queue_->push("[INFO] Server main loop terminated");
}

void Server::stop() {
    if (!running_) return;
    
    output_queue_->push("[INFO] Stopping server...");
    running_ = false;
    
    // 关闭监听套接字
    if (sockfd_ >= 0) {
        shutdown(sockfd_, SHUT_RDWR);
    }
    
    // 关闭所有客户端连接
    std::unique_lock<std::mutex> lock(clientinfo_list_->get_mutex());
    for (auto it = clientinfo_list_->begin(lock); 
         it != clientinfo_list_->end(lock); ++it) {
        it->second->get_receiver()->close();
    }
}

bool Server::output_message() {
    if (output_queue_->empty()) {
        return false;
    }
    
    bool had_output = false;
    while (!output_queue_->empty()) {
        std::cout << output_queue_->pop() << std::endl;
        had_output = true;
    }
    
    return had_output;
}