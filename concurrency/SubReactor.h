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
    int flags = fcntl(client_fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL failed");
        close(client_fd);
        return;
    }
    if (fcntl(client_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL O_NONBLOCK failed");
        close(client_fd);
        return;
    }
    
    Socket* s = new Socket(client_fd, 1024 * 16, 1024 * 16);
    socket_mgr_.add(client_fd, s);
    
    epoll_event ev;
    ev.data.fd = client_fd;
    ev.events = // 把这行：
ev.events = EPOLLIN | EPOLLRDHUP;
    if (epoll_ctl(epfd_, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
        perror("epoll_ctl EPOLL_CTL_ADD failed");
        close(client_fd);
        return;
    }
    
    std::cout << "SubReactor " << cpu_id_ << " 添加客户端 fd=" << client_fd << std::endl;
}
    void join() { 
        thread_.stop(); 
    }
    
private:
    void run_loop() {
        epoll_event evs[128];
        std::cout << "SubReactor " << cpu_id_ << " 启动，epfd=" << epfd_ << std::endl;
        
        while (true) {
            int n = epoll_wait(epfd_, evs, 128, -1);
            std::cout << "epoll_wait returned " << n << " events" << std::endl;  // 添加这行
            if (n < 0) {
                perror("epoll_wait failed");
                continue;
            }
            
            for (int i = 0; i < n; i++) {
                int fd = evs[i].data.fd;
                Socket* s = socket_mgr_.get(fd);
                if (!s || s->IsDeleted()) {
                    close(fd);
                    socket_mgr_.remove(fd);
                    continue;
                }
                
                if (evs[i].events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
                    std::cout << "连接关闭或错误 fd=" << fd << std::endl;
                    s->Disconnect();
                    delete s;
                    close(fd);
                    socket_mgr_.remove(fd);
                    continue;
                }
                
                if (evs[i].events & EPOLLIN) {
                    s->OnRead(4096);
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