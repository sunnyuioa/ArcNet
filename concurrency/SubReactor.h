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
#include "../mysql/DBWorker.h"
#include "TaskQueue.h"
#include "../net/socket_manager.h"
#include "../net/workthread.h"
#include "../common/common.h"        // 匹配任务类型定义
#include "../MatchManager/MatchManager.h"    // 全局匹配管理器

class SubReactor {
public:
    SubReactor(int cpu_id) : cpu_id_(cpu_id) {
        cpu_mysql_ = new DBWorker("10.254.153.228", "root", "Lianqi@123", "user");
        epfd_ = epoll_create1(0);
        if (epfd_ == -1) {
            perror("epoll_create1 failed");
        }
        // 注册到全局 MatchManager
        MatchManager::instance().registerReactor(this);
    }

    ~SubReactor() {
        delete cpu_mysql_;
        MatchManager::instance().unregisterReactor(this);
    }
  // 向指定 fd 发送原始字符串消息（线程安全，内部投递到本线程队列执行）
  void postRawMessage(int fd, const std::string& msg) {
    matchQueue_.push([this, fd, msg]() {
        Socket* s = getSocket(fd);
        if (s && s->IsConnected()) {
            send(fd, msg.c_str(), msg.size(), 0);
        }
    });
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
        if (!cpu_mysql_) {
            std::cerr << "DBWorker not initialized, refuse new client" << std::endl;
            close(client_fd);
            return;
        }
        Socket* s = new Socket(client_fd, 1024 * 16, 1024 * 16, *cpu_mysql_);
        socket_mgr_.add(client_fd, s);

        // ✅ 调用 _OnConnect 设置非阻塞、禁用 Nagle，并将 m_connected 置为 true
        // 注意：需要确保 Socket::_OnConnect 是 public 或在 Socket 中添加 public 的 OnAccept() 方法
        s->_OnConnect();  // 如果你已将 _OnConnect 改为 public，则直接使用

        // 注册 fd 到全局映射
        MatchManager::instance().registerFd(client_fd, this);

        epoll_event ev;
        ev.data.fd = client_fd;
        ev.events = EPOLLIN | EPOLLRDHUP;

        if (epoll_ctl(epfd_, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
            perror("epoll_ctl EPOLL_CTL_ADD failed");
            MatchManager::instance().unregisterFd(client_fd);
            delete s;
            socket_mgr_.remove(client_fd);
            close(client_fd);
            return;
        }
    }

    void join() {
        thread_.stop();
    }

    // ========== 匹配相关接口（保留） ==========
    // 向本 SubReactor 投递匹配任务（线程安全）
    void postMatchTask(const MatchTask& task) {
        matchQueue_.push([this, task]() {
            handleMatchTask(task);
        });
    }

    // 获取本线程管理的 Socket
    Socket* getSocket(int fd) {
        return socket_mgr_.get(fd);
    }

    int getCpuId() const { return cpu_id_; }

private:
    void run_loop() {
        epoll_event evs[128];
        while (true) {
            // 先处理匹配队列中的任务
            drainMatchTasks();

            int n = epoll_wait(epfd_, evs, 128, 100);  // 超时 100ms
            for (int i = 0; i < n; i++) {
                int fd = evs[i].data.fd;
                Socket* s = socket_mgr_.get(fd);
                if (!s) {
                    close(fd);
                    continue;
                }

                if (s->IsDeleted()) {
                    std::cout << "清理已删除的 socket fd=" << fd << std::endl;
                    epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr);
                    socket_mgr_.remove(fd);
                    MatchManager::instance().unregisterFd(fd);
                    delete s;
                    close(fd);
                    continue;
                }

                if (evs[i].events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
                    std::cout << "连接关闭或错误 fd=" << fd << std::endl;
                    epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr);
                    socket_mgr_.remove(fd);
                    MatchManager::instance().unregisterFd(fd);
                    delete s;
                    close(fd);
                    continue;
                }

                if (evs[i].events & EPOLLIN) {
                    s->onreads();
                }

                if (evs[i].events & EPOLLOUT) {
                    s->WriteCallback();
                }
            }
        }
    }

    void drainMatchTasks() {
        while (!matchQueue_.empty()) {
            auto task = matchQueue_.pop();
            if (task) task();
        }
    }

    void handleMatchTask(const MatchTask& task);

private:
    int epfd_;
    int cpu_id_;
    WorkerThread thread_;
    SocketManager socket_mgr_;
    DBWorker* cpu_mysql_;

    // 匹配消息队列（接收来自 MatchManager 的任务）
    TaskQueue matchQueue_;
};

#endif // SUB_REACTOR_H