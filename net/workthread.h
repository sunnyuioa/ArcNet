#ifndef WORKTHREAD_H
#define WORKTHREAD_H

#include <queue>
#include <mutex>
#include <thread>
#include <functional>
#include <atomic>
#include <chrono>
#include"../concurrency/TaskQueue.h"
class WorkerThread {
private:
    std::thread t;
    TaskQueue taskQueue;  
    std::atomic<bool> running;
    
public:
    WorkerThread() : running(false) {}
    
    ~WorkerThread() {
        stop();
    }
    
    WorkerThread(const WorkerThread&) = delete;
    WorkerThread& operator=(const WorkerThread&) = delete;
    
    void post(const std::function<void()>& task) {
        taskQueue.push(task);
    }
    
    void run() {
        while (running) {
            std::function<void()> task = taskQueue.pop();
            if (!task) {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                continue;
            }
            task();
        }
    }
    
    void start() {
        running = true;
        t = std::thread(&WorkerThread::run, this);
    }
    
    void stop() {
        running = false;
        if (t.joinable()) {
            t.join();
        }
    }
};

#endif