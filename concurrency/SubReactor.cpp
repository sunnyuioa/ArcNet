#include "SubReactor.h"
#include "../net/tcp_socket.h"
#include "../MatchManager/MatchManager.h"
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>

void SubReactor::handleMatchTask(const MatchTask& task) {
    switch (task.type) {

    case MatchMsgType::ASK_USER: {
        // 向客户端发送匹配询问命令
        Socket* target = getSocket(task.targetFd);
        if (target && target->IsConnected()) {
            std::string msg = "A" + task.userName;
            send(target->GetFd(), msg.c_str(), msg.size(), 0);
        }
        break;
    }

    case MatchMsgType::MATCH_SUCCESS: {
        // 设置匹配状态并通知客户端
        Socket* s = getSocket(task.targetFd);
        if (s && s->IsConnected()) {
            s->m_chatMode = 2;
            s->m_peerFd   = task.fromFd;
            std::string msg = "M" + task.userName;
            send(s->GetFd(), msg.c_str(), msg.size(), 0);
        }
        break;
    }

    case MatchMsgType::PEER_MSG: {
        // 转发真人聊天消息
        Socket* target = getSocket(task.targetFd);
        if (target && target->IsConnected()) {
            std::string fwd = "5" + task.content;   // 使用 '5' 表示真人聊天
            send(target->GetFd(), fwd.c_str(), fwd.size(), 0);
        }
        break;
    }

    case MatchMsgType::PEER_DISCONNECT: {
        // 通知对方断开
        Socket* target = getSocket(task.targetFd);
        if (target && target->IsConnected()) {
            send(target->GetFd(), "7对方已离开", 13, 0);
            target->m_chatMode = 0;
            target->m_peerFd   = -1;
        }
        break;
    }

    default:
        break;
    }
}