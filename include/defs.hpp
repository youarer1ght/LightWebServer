#ifndef __DEF_HPP__
#define __DEF_HPP__

#define MAX_BUFFER_SIZE 65536   // 最大缓冲区大小 64KB
#define MAX_CLIENT_NUM 255      // 最大并发客户端数量
#define MAX_EPOLL_EVENTS 64     // 最大 epoll 事件数
#define TIMEOUT 200             // epoll 等待时间

#define SERVER_ADDR INADDR_ANY  // 服务器监听地址：所有可用接口网络
#define SERVER_PORT 6025        // 服务器监听端口
#define USERNAME "3230106025"   // 学号
#define PASSWORD "6025"         // 学号后四位

typedef unsigned char uint8_t;

#define cast_sockaddr_in(addr) reinterpret_cast<sockaddr *>(&(addr))

#endif
