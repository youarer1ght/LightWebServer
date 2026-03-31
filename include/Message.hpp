#ifndef __MESSAGE_HPP__
#define __MESSAGE_HPP__

#include "defs.hpp"
#include <vector>
#include <unordered_map>
#include <string>

/**
 * HTTP状态码枚举
 * 定义服务器可以返回的各种HTTP状态
 */
enum class StatusCodes {
    UNKNOWN=-1,                  ///< 未知状态码
    OK=200,                      ///< 请求成功 (200 OK)
    BAD_REQUEST=400,             ///< 客户端请求错误 (400 Bad Request)
    FORBIDDEN=403,               ///< 禁止访问 (403 Forbidden)
    NOT_FOUND=404,               ///< 资源未找到 (404 Not Found)
    INTERNAL_SERVER_ERROR=500    ///< 服务器内部错误 (500 Internal Server Error)
};

/**
 * HTTP方法类型枚举
 * 定义客户端可以使用的HTTP请求方法
 */
enum class MethodTypes {
    UNKNOWN=-1,  ///< 未知方法类型
    GET=0,       ///< HTTP GET 方法（获取资源）
    POST=1       ///< HTTP POST 方法（提交数据）
};

/**
 * 将状态码枚举值转换为字符串描述
 * @param status_code HTTP状态码枚举值
 * @return 状态码的字符串表示（如"200 OK"）
 */
std::string status_code_to_string(const StatusCodes& status_code);

/**
 * 将方法类型枚举值转换为字符串
 * @param method_type HTTP方法类型枚举值
 * @return 方法类型的字符串表示（如"GET"）
 */
std::string method_type_to_string(const MethodTypes& method_type);

/**
 * HTTP消息基类
 * 封装HTTP协议中消息的共同特性，包括版本、消息体和头部字段
 */
class Message {
private:
    std::string version_;                           ///< HTTP协议版本（如"HTTP/1.1"）
    std::string body_;                              ///< 消息体内容
    std::unordered_map<std::string, std::string> headers_; ///< HTTP头部字段映射表

public:
    Message() = delete;  ///< 禁用默认构造函数，必须提供版本、消息体和头部

    /**
     * @brief 构造函数
     * @param version HTTP协议版本
     * @param body HTTP消息体内容
     * @param headers HTTP头部字段映射
     */
    Message(
        const std::string& version,
        const std::string& body,
        const std::unordered_map<std::string, std::string>& headers
    );

    /**
     * @brief 拷贝构造函数
     * @param other 要拷贝的Message对象
     */
    Message(const Message& other);

    /**
     * @brief 虚析构函数
     */
    virtual ~Message();

    /**
     * @brief 获取HTTP协议版本
     * @return HTTP协议版本字符串
     */
    std::string get_version() const;

    /**
     * @brief 获取消息体内容
     * @return 消息体字符串
     */
    std::string get_body() const;

    /**
     * @brief 获取所有HTTP头部字段
     * @return HTTP头部字段的映射表
     */
    std::unordered_map<std::string, std::string> get_headers() const;

    /**
     * @brief 获取指定的HTTP头部字段值
     * @param key 要查找的头部字段键名
     * @return 头部字段值，如果不存在则返回空字符串
     */
    std::string get_header(const std::string& key) const;

    /**
     * @brief 拷贝赋值运算符
     * @param other 要赋值的Message对象
     * @return 当前对象的引用
     */
    Message& operator=(const Message& other);
};

/**
 * HTTP请求类
 * 继承自Message，表示客户端发送的HTTP请求
 */
class Request : public Message {
private:
    MethodTypes method_type_;   ///< HTTP请求方法类型
    std::string url_;           ///< 请求的URL路径

public:
    /**
     * @brief 默认构造函数
     * 创建一个空的、未知类型的请求
     */
    Request();

    /**
     * @brief 构造函数
     * @param method_type HTTP请求方法类型
     * @param url 请求的URL路径
     * @param version HTTP协议版本
     * @param body 请求体内容
     * @param headers 请求头部字段
     */
    Request(
        const MethodTypes& method_type,
        const std::string& url,
        const std::string& version,
        const std::string& body,
        const std::unordered_map<std::string, std::string>& headers
    );

    /**
     * @brief 拷贝构造函数
     * @param other 要拷贝的Request对象
     */
    Request(const Request& other);

    /**
     * @brief 析构函数
     */
    ~Request() override;

    /**
     * @brief 获取HTTP请求方法类型
     * @return HTTP方法类型枚举值
     */
    MethodTypes get_method_type() const;

    /**
     * @brief 获取请求的URL路径
     * @return URL路径字符串
     */
    std::string get_url() const;

    /**
     * @brief 将Request对象转换为字符串表示
     * @return 符合HTTP协议格式的请求字符串
     */
    std::string to_string() const;

    /**
     * @brief 拷贝赋值运算符
     * @param other 要赋值的Request对象
     * @return 当前对象的引用
     */
    Request& operator=(const Request& other);
};

/**
 * HTTP响应类
 * 继承自Message，表示服务器返回的HTTP响应
 */
class Response : public Message {
private:
    StatusCodes status_code_;   ///< HTTP响应状态码

public:
    /**
     * @brief 默认构造函数
     * 创建一个空的、未知状态的响应
     */
    Response();

    /**
     * @brief 构造函数
     * @param status_code HTTP响应状态码
     * @param version HTTP协议版本
     * @param headers 响应头部字段
     * @param body 响应体内容
     */
    Response(
        const StatusCodes& status_code,
        const std::string& version,
        const std::unordered_map<std::string, std::string>& headers,
        const std::string& body
    );

    /**
     * @brief 拷贝构造函数
     * @param other 要拷贝的Response对象
     */
    Response(const Response& other);

    /**
     * @brief 析构函数
     */
    ~Response() override;

    /**
     * @brief 获取HTTP响应状态码
     * @return 状态码枚举值
     */
    StatusCodes get_status_code() const;

    /**
     * @brief 序列化Response对象到字节缓冲区
     * 将HTTP响应转换为字节序列，便于网络传输
     * @param buffer 目标字节缓冲区
     * @return 序列化的字节数
     */
    ssize_t serialize(std::vector<uint8_t>& buffer) const;

    /**
     * @brief 将Response对象转换为字符串表示
     * @return 符合HTTP协议格式的响应字符串
     */
    std::string to_string() const;

    /**
     * @brief 拷贝赋值运算符
     * @param other 要赋值的Response对象
     * @return 当前对象的引用
     */
    Response& operator=(const Response& other);
};

#endif