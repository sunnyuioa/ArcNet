#include <iostream>
#include <signal.h>
#include "concurrency/MainReactor.h"
#include "net/tcp_socket.h"

int main() {
    // 忽略SIGPIPE，防止写断开连接时崩溃
    signal(SIGPIPE, SIG_IGN);
    
    Socket s(0, 65536, 65536);
    if (!s.Listen(8888)) {
        std::cerr << "Listen failed!" << std::endl;
        return -1;
    }
    
    std::cout << "✅ ArcNet Server listening on 0.0.0.0:8888" << std::endl;
    std::cout << "按 Ctrl+C 退出" << std::endl;
    
    MainReactor m(s.GetFd());
    m.start();
    
    return 0;
}
