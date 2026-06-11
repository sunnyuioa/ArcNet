#include <iostream>
#include <cstring>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "concurrency/MainReactor.h"
#include "MatchManager/MatchManager.h"
#include "Room/RoomManager.h"        // 新增：房间管理器

int main() {
    // 忽略 SIGPIPE，防止写断开连接时崩溃
    signal(SIGPIPE, SIG_IGN);
    
    // 启动匹配管理器
    MatchManager::instance().start();
    // ✅ 新增：启动房间管理器
    RoomManager::instance().start();
    
    // 1. 创建监听 socket
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        return -1;
    }
    // 2. 设置地址复用
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    // 3. 绑定端口
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8888);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(listen_fd);
        return -1;
    }

    // 4. 开始监听
    if (listen(listen_fd, 128) < 0) {
        perror("listen");
        close(listen_fd);
        return -1;
    }

    std::cout << "✅ ArcNet Server listening on 0.0.0.0:8888" << std::endl;
    std::cout << "按 Ctrl+C 退出" << std::endl;

    // 5. 启动主 Reactor
    MainReactor m(listen_fd);
    m.start();

    close(listen_fd);
    
    // 程序退出前停止房间和匹配管理器
    RoomManager::instance().stop();
    MatchManager::instance().stop();
    
    return 0;
}