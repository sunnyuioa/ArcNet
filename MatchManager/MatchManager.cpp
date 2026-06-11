#include "MatchManager.h"
#include "../concurrency/SubReactor.h"
#include "../net/tcp_socket.h"
#include <iostream>

MatchManager& MatchManager::instance() {
    static MatchManager mgr;
    return mgr;
}

// --------------------- 生命周期 ---------------------
void MatchManager::start() {
    // WorkerThread 启动后，所有通过 postMatchTask 投递的任务
    // 都在 worker_ 的线程中串行执行，天然线程安全。
    worker_.start();
}

void MatchManager::stop() {
    worker_.stop();
}

// --------------------- SubReactor 管理 ---------------------
void MatchManager::registerReactor(SubReactor* reactor) {
    std::lock_guard<std::mutex> lock(reactorMutex_);
    reactors_.push_back(reactor);
}

void MatchManager::unregisterReactor(SubReactor* reactor) {
    std::lock_guard<std::mutex> lock(reactorMutex_);
    auto it = std::find(reactors_.begin(), reactors_.end(), reactor);
    if (it != reactors_.end()) reactors_.erase(it);
}

std::vector<SubReactor*> MatchManager::getReactors() const {
    std::lock_guard<std::mutex> lock(reactorMutex_);
    return reactors_;
}

void MatchManager::registerFd(int fd, SubReactor* reactor) {
    std::lock_guard<std::mutex> lock(reactorMutex_);
    fdToReactor_[fd] = reactor;
}

void MatchManager::unregisterFd(int fd) {
    std::lock_guard<std::mutex> lock(reactorMutex_);
    fdToReactor_.erase(fd);
}

SubReactor* MatchManager::findReactorByFd(int fd) {
    std::lock_guard<std::mutex> lock(reactorMutex_);
    auto it = fdToReactor_.find(fd);
    return (it != fdToReactor_.end()) ? it->second : nullptr;
}

// --------------------- 匹配任务投递 ---------------------
void MatchManager::postMatchTask(const MatchTask& task) {
    // 将任务投递到 worker_ 的队列中，由匹配线程执行
    worker_.post([this, task]() {
        handleMatchTask(task);
    });
}

// --------------------- 匹配逻辑（匹配线程内执行） ---------------------
void MatchManager::handleMatchTask(const MatchTask& task) {
    
    switch (task.type) {

    case MatchMsgType::JOIN_QUEUE: {
        std::cout << "[MatchManager] 处理 JOIN_QUEUE, fromFd=" << task.fromFd << " user=" << task.userName << std::endl;
        waitingQueue_.push(task.fromFd);

        // 遍历所有 SubReactor，向所有在线用户广播询问
        auto reactors = getReactors();
        std::cout << "[MatchManager] 当前注册的 reactor 数量: " << reactors.size() << std::endl;
        for (auto* reactor : reactors) {
            reactor->foreach_socket([&](Socket* s) {
                if (s->GetFd() != task.fromFd && s->IsConnected()) {
                    MatchTask askTask;
                    askTask.type     = MatchMsgType::ASK_USER;
                    askTask.fromFd   = task.fromFd;
                    askTask.targetFd = s->GetFd();
                    askTask.userName = task.userName;

                    // 暂存询问任务
                    pendingAsks_[s->GetFd()] = askTask;

                    // 投递给目标 SubReactor，由它发送弹窗命令
                    reactor->postMatchTask(askTask);
                    std::cout << "[MatchManager] 投递 ASK_USER 到 reactor cpu=" << reactor->getCpuId()
          << " targetFd=" << s->GetFd() << " user=" << task.userName << std::endl;
                }
            });
        }
        break;
    }

    case MatchMsgType::USER_ACCEPT: {
        int bFd = task.fromFd;
        auto it = pendingAsks_.find(bFd);
        if (it == pendingAsks_.end()) break;

        MatchTask askTask = it->second;
        pendingAsks_.erase(it);

        // 通知发起者 A
        MatchTask notifyA;
        notifyA.type     = MatchMsgType::MATCH_SUCCESS;
        notifyA.fromFd   = bFd;
        notifyA.targetFd = askTask.fromFd;
        notifyA.userName = task.userName;
        SubReactor* aReactor = findReactorByFd(askTask.fromFd);
        if (aReactor) aReactor->postMatchTask(notifyA);

        // 通知同意者 B
        MatchTask notifyB;
        notifyB.type     = MatchMsgType::MATCH_SUCCESS;
        notifyB.fromFd   = askTask.fromFd;
        notifyB.targetFd = bFd;
        notifyB.userName = askTask.userName;
        SubReactor* bReactor = findReactorByFd(bFd);
        if (bReactor) bReactor->postMatchTask(notifyB);

        break;
    }

    case MatchMsgType::CANCEL:
    case MatchMsgType::USER_REJECT:
        // 可扩展：从队列移除、超时处理等
    default:
        break;
    }
}