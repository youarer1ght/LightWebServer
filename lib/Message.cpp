#include "Message.hpp"
#include <stdexcept>
#include <sstream>
#include <algorithm>

// 使用静态常量提高性能
namespace {
    const std::unordered_map<StatusCodes, std::string> STATUS_STRINGS = {
        {StatusCodes::OK, "200 OK"},
        {StatusCodes::BAD_REQUEST, "400 Bad Request"},
        {StatusCodes::FORBIDDEN, "403 Forbidden"},
        {StatusCodes::NOT_FOUND, "404 Not Found"},
        {StatusCodes::INTERNAL_SERVER_ERROR, "500 Internal Server Error"}
    };

    const std::unordered_map<MethodTypes, std::string> METHOD_STRINGS = {
        {MethodTypes::GET, "GET"},
        {MethodTypes::POST, "POST"}
    };
}

std::string status_code_to_string(const StatusCodes& status_code) {
    auto it = STATUS_STRINGS.find(status_code);
    if (it != STATUS_STRINGS.end()) {
        return it->second;
    }
    return "Unknown Status Code";
}

std::string method_type_to_string(const MethodTypes& method_type) {
    auto it = METHOD_STRINGS.find(method_type);
    if (it != METHOD_STRINGS.end()) {
        return it->second;
    }
    return "UNKNOWN";
}

// Message 实现
Message::Message(
    const std::string& version,
    const std::string& body,
    const std::unordered_map<std::string, std::string>& headers
) : version_(version), body_(body), headers_(headers) {}

Message::Message(const Message& other) 
    : version_(other.version_), 
      body_(other.body_), 
      headers_(other.headers_) {}

Message::~Message() {}

std::string Message::get_version() const {
    return version_;
}

std::string Message::get_body() const {
    return body_;
}

std::unordered_map<std::string, std::string> Message::get_headers() const {
    return headers_;
}

std::string Message::get_header(const std::string& key) const {
    auto it = headers_.find(key);
    return it != headers_.end() ? it->second : "";
}

Message& Message::operator=(const Message& other) {
    if (this != &other) {
        version_ = other.version_;
        body_ = other.body_;
        headers_ = other.headers_;
    }
    return *this;
}

// Request 实现
Request::Request() 
    : Message("", "", {}), 
      method_type_(MethodTypes::UNKNOWN), 
      url_("") {}

Request::Request(
    const MethodTypes& method_type,
    const std::string& url,
    const std::string& version,
    const std::string& body,
    const std::unordered_map<std::string, std::string>& headers
) : Message(version, body, headers), 
    method_type_(method_type), 
    url_(url) {}

Request::Request(const Request& other) 
    : Message(other), 
      method_type_(other.method_type_), 
      url_(other.url_) {}

Request::~Request() {}

MethodTypes Request::get_method_type() const {
    return method_type_;
}

std::string Request::get_url() const {
    return url_;
}

std::string Request::to_string() const {
    std::stringstream ss;
    ss << method_type_to_string(method_type_) << " " 
       << url_ << " " 
       << get_version() << "\r\n";
    
    // 使用getter方法访问headers
    auto headers = get_headers();
    for (const auto& header : headers) {
        ss << header.first << ": " << header.second << "\r\n";
    }
    
    ss << "\r\n" << get_body();
    return ss.str();
}

Request& Request::operator=(const Request& other) {
    if (this != &other) {
        Message::operator=(other);
        method_type_ = other.method_type_;
        url_ = other.url_;
    }
    return *this;
}

// Response 实现
Response::Response() 
    : Message("", "", {}), 
      status_code_(StatusCodes::UNKNOWN) {}

Response::Response(
    const StatusCodes& status_code,
    const std::string& version,
    const std::unordered_map<std::string, std::string>& headers,
    const std::string& body
) : Message(version, body, headers), 
    status_code_(status_code) {}

Response::Response(const Response& other) 
    : Message(other), 
      status_code_(other.status_code_) {}

Response::~Response() {}

StatusCodes Response::get_status_code() const {
    return status_code_;
}

ssize_t Response::serialize(std::vector<uint8_t>& buffer) const {
    std::string str = to_string();
    buffer.resize(str.size());
    std::copy(str.begin(), str.end(), buffer.begin());
    return static_cast<ssize_t>(str.size());
}

std::string Response::to_string() const {
    std::stringstream ss;
    ss << get_version() << " " 
       << status_code_to_string(status_code_) << "\r\n";
    
    // 使用getter方法访问headers
    auto headers = get_headers();
    for (const auto& header : headers) {
        ss << header.first << ": " << header.second << "\r\n";
    }
    
    ss << "\r\n" << get_body();
    return ss.str();
}

Response& Response::operator=(const Response& other) {
    if (this != &other) {
        Message::operator=(other);
        status_code_ = other.status_code_;
    }
    return *this;
}