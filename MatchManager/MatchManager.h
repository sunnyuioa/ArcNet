#ifndef MATCH_MANAGER_H
#define MATCH_MANAGER_H
#include <vector>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <string>
#include"../net/workthread.h"    // 你的 WorkerThread
#include "../common/common.h"
class SubReactor;
class MatchManager {
public:
    static MatchManager& instance();

    // ---------- 生命周期 ----------
    void start();   // 启动匹配线程
    void stop();    // 停止匹配线程

    // ---------- SubReactor 管理 ----------
    void registerReactor(SubReactor* reactor);
    void unregisterReactor(SubReactor* reactor);
    std::vector<SubReactor*> getReactors() const;

    // ---------- fd -> SubReactor 映射 ----------
    void registerFd(int fd, SubReactor* reactor);
    void unregisterFd(int fd);
    SubReactor* findReactorByFd(int fd);

    // ---------- 匹配任务投递 ----------
    void postMatchTask(const MatchTask& task);  // 从其他线程投递匹配任务

private:
    MatchManager() = default;
    ~MatchManager() = default;
    MatchManager(const MatchManager&) = delete;
    MatchManager& operator=(const MatchManager&) = delete;

    // 匹配线程的主循环（由 WorkerThread 调用）
    void handleMatchTask(const MatchTask& task);

    // ---------- 状态（仅匹配线程访问，无需锁） ----------
    std::unordered_map<int, MatchTask> pendingAsks_;   // 被询问者 fd → 询问任务
    std::queue<int> waitingQueue_;                    // 等待匹配的用户 fd

    // ---------- 线程与同步 ----------
    WorkerThread worker_;
    mutable std::mutex reactorMutex_;
    std::vector<SubReactor*> reactors_;
    std::unordered_map<int, SubReactor*> fdToReactor_;
};

#endif // MATCH_MANAGER_H
