#ifndef ROOM_MANAGER_H
#define ROOM_MANAGER_H

#include <unordered_map>
#include <memory>
#include <vector>
#include <string>
#include "../net/workthread.h"
#include "RoomDef.h"

class Room;
class SubReactor;

class RoomManager {
public:
    static RoomManager& instance();
    void start();
    void stop();
    // 从 I/O 线程投递房间任务
    void postRoomTask(const RoomTask& task);
    // 供外部查询（可在任意线程调用，但仅用于简单获取）
    std::vector<int> getRoomIds() const;

private:
    RoomManager() = default;
    ~RoomManager() = default;
    RoomManager(const RoomManager&) = delete;
    RoomManager& operator=(const RoomManager&) = delete;
    void handleRoomTask(const RoomTask& task);
    WorkerThread worker_;
    std::unordered_map<int, std::unique_ptr<Room>> rooms_;
    int nextRoomId_ = 100000;  // 起始房间号
    // 注意：所有成员只在 worker_ 线程中访问，因此无需额外加锁
};
#endif // ROOM_MANAGER_H