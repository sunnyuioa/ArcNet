#include"net/socket_ops.h"

namespace SocketOps
{
    SOCKET CreateTCPFileDescriptor()
    {
        return socket(AF_INET, SOCK_STREAM, 0);
    }
    
    bool Nonblocking(SOCKET fd)
    {
        uint32_t arg = 1;
        return (::ioctl(fd, FIONBIO, &arg) == 0);
    }
    
    bool Blocking(SOCKET fd)
    {
        uint32_t arg = 0;
        return (ioctl(fd, FIONBIO, &arg) == 0);
    }
    
    bool DisableBuffering(SOCKET fd)
    {
        uint32_t arg = 1;
        return (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (const char*)&arg, sizeof(arg)) == 0);
    }
    
    bool EnableBuffering(SOCKET fd)
    {
        uint32_t arg = 0;
        return (setsockopt(fd, 0x6, 0x1, (const char*)&arg, sizeof(arg)) == 0);
    }
    
    bool SetSendBufferSize(SOCKET fd, uint32_t size)
    {
        return (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (const char*)&size, sizeof(size)) == 0);
    }
    
    bool SetRecvBufferSize(SOCKET fd, uint32_t size)
    {
        return (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (const char*)&size, sizeof(size)) == 0);
    }
    
    bool SetTimeout(SOCKET fd, uint32_t timeout)
    {
        struct timeval to;
        to.tv_sec = timeout;
        to.tv_usec = 0;
        if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&to, (socklen_t)sizeof(to)) != 0)
            return false;
        return (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&to, (socklen_t)sizeof(to)) == 0);
    }
    
    void CloseSocket(SOCKET fd)
    {
        shutdown(fd, SHUT_RDWR);
        close(fd);
    }
    
    void ReuseAddr(SOCKET fd)
    {
        uint32_t option = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&option, 4);
    }
}