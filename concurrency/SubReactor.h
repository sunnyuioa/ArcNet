#ifndef SUB_REACTOR_H
#define SUB_REACTOR_H

#include <sys/socket.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cstdio>
#include <pthread.h>
#include <vector>
#include <functional>
#include <memory>
#include <iostream>
#include "TaskQueue.h"
#include "../net/socket_manager.h"
#include "../net/workthread.h"

class SubReactor {
public:
    SubReactor(int cpu_id) : cpu_id_(cpu_id) {
        epfd_ = epoll_create1(0);
        if (epfd_ == -1) {
            perror("epoll_create1 failed");
        }
    }
    
    void bind_cpu(int cpu_id) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(cpu_id, &cpuset);
        pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    }
    
    void start() {
        thread_.start();
        thread_.post([this]() {
            bind_cpu(cpu_id_);
            run_loop();
        });
    }
    
    void foreach_socket(std::function<void(Socket*)> func) const {
        socket_mgr_.foreach_socket(func);
    }
    
  void add_client(int client_fd) {
    // ... 设置非阻塞等 ...
    
    Socket* s = new Socket(client_fd, 1024 * 16, 1024 * 16);
    socket_mgr_.add(client_fd, s);
    
    epoll_event ev;
    ev.data.fd = client_fd;
    ev.events = EPOLLIN | EPOLLRDHUP;  // ✅ 修复重复赋值
    
    if (epoll_ctl(epfd_, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
        perror("epoll_ctl EPOLL_CTL_ADD failed");
        delete s;  // ✅ 失败时删除对象
        socket_mgr_.remove(client_fd);
        close(client_fd);
        return;
    }
}
    void join() { 
        thread_.stop(); 
    }
    
private:
   void run_loop() {
    epoll_event evs[128];
    
    while (true) {
        int n = epoll_wait(epfd_, evs, 128, -1);
        
        for (int i = 0; i < n; i++) {
            int fd = evs[i].data.fd;
            Socket* s = socket_mgr_.get(fd);
            
            if (!s) {
                close(fd);
                continue;
            }
            
            // ✅ 检查是否标记删除
            if (s->IsDeleted()) {
                std::cout << "清理已删除的 socket fd=" << fd << std::endl;
                epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr);
                socket_mgr_.remove(fd);
                delete s;  // ✅ 在这里删除对象
                close(fd);
                continue;
            }
            
            if (evs[i].events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
                std::cout << "连接关闭或错误 fd=" << fd << std::endl;
                epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr);
                socket_mgr_.remove(fd);
                delete s;
                close(fd);
                continue;
            }
            
            if (evs[i].events & EPOLLIN) {
                s->OnRead_(4096);
                // OnRead_ 可能会调用 Delete()，下次循环会清理
            }
            
            if (evs[i].events & EPOLLOUT) {
                s->WriteCallback();
            }
        }
    }
}
    
private:
    int epfd_;
    int cpu_id_;
    WorkerThread thread_;
    SocketManager socket_mgr_;
};

#endif