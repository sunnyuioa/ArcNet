#ifndef SOCKET_MANAGER_H
#define SOCKET_MANAGER_H

#include <map>
#include <functional>
#include "tcp_socket.h"

class SocketManager
{
public:
    SocketManager() = default;
    ~SocketManager() = default;

    SocketManager(const SocketManager&) = delete;
    SocketManager& operator=(const SocketManager&) = delete;
    SocketManager(SocketManager&&) = delete;
    SocketManager& operator=(SocketManager&&) = delete;

    void add(int fd, Socket* sock) {
        socket_map_[fd] = sock;
    }

    void foreach_socket(std::function<void(Socket*)> func) const {
        for (auto& pair : socket_map_) {
        if (pair.second) func(pair.second);
        }
    }
    
    Socket* get(int fd) {
        auto it = socket_map_.find(fd);
        if (it != socket_map_.end()) {
            return it->second;
        }
        return nullptr;
    }

    void remove(int fd) {
        socket_map_.erase(fd);
    }

    void clear() {
        socket_map_.clear();
    }

private:
    std::map<int, Socket*> socket_map_;
};

#endif