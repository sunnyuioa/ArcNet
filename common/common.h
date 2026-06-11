#ifndef COMMON_H
#define COMMON_H

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <iostream>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <thread>
#include <functional>
#include <queue>
#include <map>
#include <vector>
#include <memory>
using std::string;
using std::cout;
using std::cerr;
using std::endl;
using std::min;
using std::max;
enum class MatchMsgType : uint8_t {
    JOIN_QUEUE,       // 用户加入匹配队列（存 fd）
    ASK_USER,         // 询问某用户是否同意匹配（存目标 fd + 对方用户名）
    USER_ACCEPT,      // 用户同意匹配（存 fd1, fd2）
    USER_REJECT,      // 用户拒绝 / 超时
    MATCH_SUCCESS,    // 匹配成功通知
    CANCEL,           // 用户取消匹配
    PEER_MSG,         // 真人聊天消息转发
    PEER_DISCONNECT   // 对方断开
};

struct MatchTask {
    MatchMsgType type;
    int fromFd;              // 发起方 fd
    int targetFd = -1;       // 目标 fd
    std::string userName;    // 用户名（询问时用）
    std::string content;     // 消息内容（聊天转发时用）
};
typedef int SOCKET;

#endif
