#ifndef MAIN_REACTOR_H
#define MAIN_REACTOR_H

#include <unistd.h>
#include <vector>
#include <memory>
#include <iostream>
#include "SubReactor.h"

class MainReactor {
public:
    MainReactor(int listen_fd)
        : listen_fd_(listen_fd),
          cpu_cnt_(sysconf(_SC_NPROCESSORS_ONLN)) {
        if (cpu_cnt_ <= 0) cpu_cnt_ = 4;
        
        for (int i = 0; i < cpu_cnt_; i++) {
            reactors_.emplace_back(new SubReactor(i));
        }
    }
    
    void bind_cpu(int cpu_id) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(cpu_id, &cpuset);
        pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    }
    
    MainReactor(const MainReactor&) = delete;
    MainReactor& operator=(const MainReactor&) = delete;
    
    ~MainReactor() {
        for (auto& r : reactors_) {
            r->join();
        }
    }
    
    void start() {
        bind_cpu(0);
        std::cout << "MainReactor 启动，监听 fd=" << listen_fd_ << std::endl;
        
        for (auto& r : reactors_) {
            r->start();
        }
        
        int idx = 0;
        while (true) {
            int cfd = accept(listen_fd_, nullptr, nullptr);
            if (cfd < 0) {
                perror("accept");
                continue;
            }
            std::cout << "接受新连接 fd=" << cfd << " 分配给 SubReactor " 
                      << (idx % cpu_cnt_) << std::endl;
            reactors_[idx % cpu_cnt_]->add_client(cfd);
            idx++;
        }
    }
    
private:
    int listen_fd_;
    int cpu_cnt_;
    std::vector<std::unique_ptr<SubReactor>> reactors_;
};

#endif