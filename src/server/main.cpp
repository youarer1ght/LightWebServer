#include "Server.hpp"
#include <iostream>
#include <future>

int main(int argc, char *argv[]) {
    // 参数初始化
    char hostname[128];
    gethostname(hostname, sizeof(hostname));
    std::string name(hostname);
    
    in_addr_t addr = SERVER_ADDR;
    int port = SERVER_PORT;

    // 命令行参数处理
    if (argc > 1) name = argv[1];
    if (argc > 2) addr = inet_addr(argv[2]);
    if (argc > 3) port = atoi(argv[3]);

    // 路由配置
    std::unordered_map<std::string, File> route = {
        {"/", {FileTypes::HTML, "assets/html/test.html"}},
        {"/index.html", {FileTypes::HTML, "assets/html/test.html"}},
        {"/index_noimg.html", {FileTypes::HTML, "assets/html/noimg.html"}},
        {"/info/server", {FileTypes::TXT, "assets/txt/test.txt"}},
        {"/assets/logo.jpg", {FileTypes::JPG, "assets/img/logo.jpg"}},
        {"/post", {FileTypes::HTML, "", true}}
    };

    std::cout << "[INFO] Server host name: " << name << std::endl;
    std::cout << "[INFO] Server address: " << inet_ntoa(*(in_addr *)&addr) << std::endl;
    std::cout << "[INFO] Server port: " << port << std::endl;

    // 创建服务器
    std::unique_ptr<Server> server;
    try {
        server = std::make_unique<Server>(name, addr, port, route);
    } catch (std::exception &e) {
        std::cout << "[ERR] " << e.what() << std::endl;
        return 1;
    }

    // 启动服务器线程
    std::thread runner(&Server::run, server.get());
    
    // 简化命令处理逻辑
    bool shouldExit = false;
    
    while (!shouldExit) {
        // 输出服务器消息
        server->output_message();
        
        // 非阻塞检查用户输入
        if (std::cin.rdbuf()->in_avail() > 0) {
            std::string command;
            if (std::getline(std::cin, command)) {
                if (command == "exit") {
                    shouldExit = true;
                } else {
                    std::cout << "[INFO] Please enter \"exit\" to close the server." << std::endl;
                }
            }
        }
        
        // 避免CPU忙等待
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 清理资源
    try {
        server->stop();
        runner.join();
    } catch (std::exception &e) {
        std::cerr << "[ERR] " << e.what() << std::endl;
        return 1;
    }

    return 0;
}