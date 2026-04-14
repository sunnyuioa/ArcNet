#include <iostream>
#include <signal.h>
#include <string>
#include "net/tcp_socket.h"

int main() {
    signal(SIGPIPE, SIG_IGN);
    
    std::cout << "正在连接服务器 127.0.0.1:8888 ..." << std::endl;
    
    Socket* s = ConnectTCPSocket<Socket>("127.0.0.1", 8888);
    if (!s || !s->IsConnected()) {
        std::cerr << "连接失败！请确保服务器已启动" << std::endl;
        return -1;
    }
    
    std::cout << "✅ 连接成功！" << std::endl;
    std::cout << "请输入消息（输入 quit 退出）：" << std::endl;
    
    std::string input;
    while (true) {
        std::cout << "> ";
        std::getline(std::cin, input);
        
        if (input == "quit" || input == "exit") {
            break;
        }
        
        if (input.empty()) {
            continue;
        }
        
        CMessageOut out;
        out.write8(100);  // 消息类型
        out.writeString(input);
        
        if (s->Send(out)) {
            std::cout << "✅ 已发送：" << input << std::endl;
        } else {
            std::cerr << "❌ 发送失败！" << std::endl;
        }
    }
    
    s->Disconnect();
    delete s;
    std::cout << "已断开连接" << std::endl;
    
    return 0;
}