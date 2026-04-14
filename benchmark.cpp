#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <iomanip>
#include <signal.h>
#include "net/tcp_socket.h"

class Benchmark {
private:
    std::atomic<uint64_t> total_sent{0};
    std::atomic<uint64_t> total_recv{0};
    std::atomic<uint64_t> errors{0};
    std::vector<Socket*> connections;
    
public:
    ~Benchmark() {
        for (auto s : connections) delete s;
    }
    
    bool connect_all(int count, const char* host, int port) {
        std::cout << "建立 " << count << " 个连接..." << std::endl;
        for (int i = 0; i < count; i++) {
            Socket* s = ConnectTCPSocket<Socket>(host, port);
            if (s && s->IsConnected()) {
                connections.push_back(s);
            } else {
                errors++;
                delete s;
            }
            
            // 显示进度
            if ((i + 1) % 100 == 0) {
                std::cout << "\r连接进度: " << (i + 1) << "/" << count << std::flush;
            }
        }
        std::cout << "\n成功连接: " << connections.size() << "/" << count << std::endl;
        return !connections.empty();
    }
    
    void start_test(int msg_per_conn, bool show_progress = true) {
        if (connections.empty()) return;
        
        auto start = std::chrono::steady_clock::now();
        
        for (auto sock : connections) {
            for (int i = 0; i < msg_per_conn; i++) {
                CMessageOut out;
                out.write8(100);
                out.writeString("bench_msg_" + std::to_string(i));
                
                if (sock->Send(out)) {
                    total_sent++;
                } else {
                    errors++;
                }
                
                if (show_progress && total_sent % 10000 == 0) {
                    std::cout << "\r已发送: " << total_sent << " 条消息" << std::flush;
                }
            }
        }
        
        auto end = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "\n\n========== 压测结果 ==========" << std::endl;
        std::cout << "连接数: " << connections.size() << std::endl;
        std::cout << "每连接消息数: " << msg_per_conn << std::endl;
        std::cout << "总发送: " << total_sent << std::endl;
        std::cout << "错误数: " << errors << std::endl;
        std::cout << "总耗时: " << std::fixed << std::setprecision(2) 
                  << (ms.count() / 1000.0) << " 秒" << std::endl;
        std::cout << "QPS: " << std::setprecision(0) 
                  << (total_sent / (ms.count() / 1000.0)) << std::endl;
        std::cout << "平均每连接QPS: " << (total_sent / (ms.count() / 1000.0)) / connections.size() << std::endl;
    }
    
    void test_long_connection(int seconds) {
        std::cout << "\n长连接稳定性测试，运行 " << seconds << " 秒..." << std::endl;
        
        auto start = std::chrono::steady_clock::now();
        auto last_print = start;
        uint64_t heartbeat_count = 0;
        
        while (true) {
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - start).count() >= seconds) {
                break;
            }
            
            for (auto sock : connections) {
                CMessageOut out;
                out.write8(50);
                out.writeString("heartbeat");
                if (sock->Send(out)) {
                    heartbeat_count++;
                }
            }
            
            if (std::chrono::duration_cast<std::chrono::seconds>(now - last_print).count() >= 5) {
                std::cout << "运行中... 已发送 " << heartbeat_count << " 条心跳" << std::endl;
                last_print = now;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        std::cout << "✅ 长连接测试完成，无崩溃" << std::endl;
    }
};

int main(int argc, char* argv[]) {
    signal(SIGPIPE, SIG_IGN);
    
    int connections = 500;
    int msg_per_conn = 100;
    int test_seconds = 30;
    const char* server_ip = "127.0.0.1";
    int port = 8888;
    
    if (argc > 1) connections = atoi(argv[1]);
    if (argc > 2) msg_per_conn = atoi(argv[2]);
    if (argc > 3) test_seconds = atoi(argv[3]);
    
    std::cout << "========== ArcNet 压测工具 ==========" << std::endl;
    std::cout << "服务器: " << server_ip << ":" << port << std::endl;
    std::cout << "连接数: " << connections << std::endl;
    std::cout << "每连接消息数: " << msg_per_conn << std::endl;
    std::cout << "=====================================" << std::endl;
    
    Benchmark bench;
    
    if (!bench.connect_all(connections, server_ip, port)) {
        std::cerr << "连接失败！" << std::endl;
        return -1;
    }
    
    bench.start_test(msg_per_conn, true);
    bench.test_long_connection(test_seconds);
    
    return 0;
}