#ifndef SOCKET_OPS_H
#define SOCKET_OPS_H

#include <sys/epoll.h>
#include <cstdint>
#include <netinet/tcp.h>
#include "../common/common.h"

namespace SocketOps
{
    SOCKET CreateTCPFileDescriptor();
    bool Nonblocking(SOCKET fd);
    bool Blocking(SOCKET fd);
    bool DisableBuffering(SOCKET fd);
    bool EnableBuffering(SOCKET fd);
    bool SetRecvBufferSize(SOCKET fd, ::std::uint32_t size);
    bool SetSendBufferSize(SOCKET fd, uint32_t size);
    bool SetTimeout(SOCKET fd, uint32_t timeout);
    void CloseSocket(SOCKET fd);
    void ReuseAddr(SOCKET fd);
}

enum SocketIOEvent : uint8_t
{
    SOCKET_IO_EVENT_READ_COMPLETE   = 0,
    SOCKET_IO_EVENT_WRITE_END       = 1,
    SOCKET_IO_THREAD_SHUTDOWN       = 2,
    NUM_SOCKET_IO_EVENTS            = 3,
};

#endif