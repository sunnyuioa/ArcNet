#ifndef ROOM_DEF_H
#define ROOM_DEF_H

#include <string>
#include <cstdint>

enum class RoomMsgType : uint8_t {
    CREATE,         // 创建房间
    JOIN,           // 加入房间
    LEAVE,          // 退出房间
    CHAT,           // 房间内消息
    LIST,           // 请求房间列表
    MEMBER_JOINED,  // 有人加入（广播通知）
    MEMBER_LEFT,    // 有人离开（广播通知）
    ROOM_CLOSED     // 房间关闭
};

struct RoomTask {
    RoomMsgType type;
    int fromFd = -1;       // 发起操作的 fd
    int roomId = 0;        // 房间号
    std::string userName;  // 用户名
    std::string content;   // 消息内容 / JSON 数据
    int playerCount = 0;   // 创建时指定的人数
    std::string story;     // 剧情
    // 可扩展角色名称列表（实际可用 JSON 传递）
};

#endif