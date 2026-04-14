#ifndef TASK_QUEUE_H
#define TASK_QUEUE_H

#include <queue>
#include <mutex>
#include <thread>
#include <functional>
#include <atomic>
#include <chrono>

class TaskQueue
{
public:
    using Task = std::function<void()>; 

    void push(Task task)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        tasks_.push(std::move(task));
    }
    
    Task pop()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (tasks_.empty()) return Task();
        Task task = std::move(tasks_.front());
        tasks_.pop();
        return task;
    }
    
    bool empty() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return tasks_.empty();
    }
    
    size_t size() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return tasks_.size();
    }
    
    TaskQueue() = default;
    ~TaskQueue() = default;
    TaskQueue(const TaskQueue&) = delete;
    TaskQueue& operator=(const TaskQueue&) = delete;
    
private:
    std::queue<Task> tasks_;
    mutable std::mutex mutex_;
};

#endif