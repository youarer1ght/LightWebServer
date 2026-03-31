# Lab8 - 轻量化 Web 服务器

## 项目介绍

本项目实现了一个基于 Linux 的高性能 HTTP 服务器，采用现代 C++ 开发，支持 HTML、图片和文本等静态文件服务，能够解析并处理 GET 和 POST 类型的 HTTP 协议。整体采用 CMake 构建系统，模块化设计，支持高并发连接处理。

## 技术特性
- 使用 epoll 实现高效的事件驱动模型
- 非阻塞 I/O 操作，避免线程忙等待
- 异步任务处理，提高资源利用率

## 平台兼容性
开发与测试环境：wsl2 & ubuntu 22.04
不支持的平台：macOS 和 Windows （由于 epoll 是 Linux 独有机制）
- 如需移植，可以考虑使用 select、poll 代替 epoll

## 项目结构

``` text
WebServer/
├── assets/                         # 静态资源
│   ├── html/                       
│   │   ├── noimg.html              # 无图片测试界面
│   │   └── test.html               # 测试页面
│   ├── img/                        
│   │   └── logo.jpg                # 网站logo
│   └── txt/                        
│       └── test.txt                # 测试文本文件
├── include/                        # 公共库头文件
│   ├── def.hpp                     # 网络相关定义
│   ├── Map.hpp                     # Map工具类声明
│   ├── Message.hpp                 # 消息类声明
│   ├── Queue.hpp                   # 队列工具类声明
│   ├── Receiver.hpp                # 接收器类声明
│   └── Sender.hpp                  # 发送器类声明
├── lib/
│   ├── Makefile
│   ├── Message.cpp                 # 消息类实现
│   ├── Receiver.cpp                # 接收器类实现
│   └── Sender.cpp                  # 发送器类实现
├── Makefile
├── Readme.md
└── src
    ├── include
    │   └── Server.hpp              # 服务器类声明
    ├── Makefile
    └── server
        ├── main.cpp                # 程序入口
        ├── Makefile
        └── Server.cpp              # 服务器实现
```

## 构建与运行
### 环境要求
- Ubuntu 22.04+
- CMake 3.12+
- GCC & G++

### 编译步骤
```bash
make
```
在根目录下生成可执行文件server

### 运行服务器
```bash
./server [服务器名称] [主机地址] [端口号]
# 示例：监听名称 zzz 的本地 6025 端口
./server zzz 127.0.0.1 6025
```

## 核心实现
### Message
定义及实现 HTTP 消息、方法类，消息基类 Message，请求类 Request，响应类 Response及相关辅助函数。
### Receiver & Sender
Receiver 采用 epoll 边缘触发模式处理网络数据接收，Sender 将 HTTP 响应发送回客户端，支持分块发送大数据。
### Server
监听端口，接受客户端连接，为每个连接创建独立的 Receiver/Sender，根据请求 URL 提供静态文件，生成符合 HTTP 标准的响应，支持多客户端同时访问。
### Map & Queue
设计实现线程安全的容器模板类。Map接受外部的锁，支持多个操作在同一锁保护下原子执行，适用于需要复合操作的并发场景。Queue内部自动加锁，提供基础的入队、出队操作，适用于生产者-消费者模型。