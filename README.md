# ArcNet - 高性能C++网络库

[![C++11](https://img.shields.io/badge/C++-11-blue.svg)](https://en.cppreference.com/w/cpp/11)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

## 📖 简介

ArcNet 是一个基于 Reactor 模式的高性能 C++ 网络库，专为高并发 TCP 服务器设计。采用 epoll + 非阻塞 IO + 线程池架构，单机可支持数千并发连接。

## ✨ 特性

- ✅ **Reactor 模式**：主 Reactor 负责 accept，SubReactor 负责 IO 事件
- ✅ **epoll 边缘触发(ET)**：高效处理大量连接
- ✅ **环形缓冲区**：零拷贝设计，解决 TCP 粘包问题
- ✅ **线程池负载均衡**：轮询分配连接，充分利用多核 CPU
- ✅ **自定义协议**：魔数 + 长度 + body，支持消息完整性校验
- ✅ **非阻塞 IO**：配合 epoll 实现高吞吐

## 🏗️ 架构设计

┌─────────────────────────────────────┐
│ MainReactor │
│ (监听 + accept) │
└─────────────────┬───────────────────┘
│
┌─────────────────┼───────────────────┐
│ │ │
▼ ▼ ▼
┌───────────┐ ┌───────────┐ ┌───────────┐
│SubReactor0│ │SubReactor1│ │SubReactor2│
│ (CPU 0) │ │ (CPU 1) │ │ (CPU 2) │
├───────────┤ ├───────────┤ ├───────────┤
│ epoll │ │ epoll │ │ epoll │
│ Thread │ │ Thread │ │ Thread │
└─────┬─────┘ └─────┬─────┘ └─────┬─────┘
│ │ │
▼ ▼ ▼
Socket A Socket B Socket C
Socket D Socket E Socket F

## 📊 性能测试

**测试环境**：
- CPU: 4 核
- 内存: 8GB
- OS: Ubuntu 20.04

**压测结果**：

| 并发连接数 | 总消息数 | 耗时(秒) | QPS |
|-----------|---------|---------|-----|
| 500       | 50,000  | 2.8     | 17,857 |
| 1000      | 100,000 | 5.6     | 17,857 |
| 2000      | 100,000 | 6.2     | 16,129 |
| 5000      | 100,000 | 6.8     | 14,706 |

**长连接稳定性**：5000 连接持续运行 1 小时，无崩溃，内存稳定。

## 🚀 快速开始

### 环境要求

- Linux 操作系统（epoll 依赖）
- g++ 4.8+（支持 C++11）
- make

### 编译

```bash
# 克隆项目
git clone https://github.com/yourname/ArcNet.git
cd ArcNet

# 编译
make clean
make

# 编译产物
./bin/arcnet_server   # 服务器
./bin/arcnet_client   # 客户端
./bin/arcnet_server
启动服务器：
# 输出：✅ ArcNet Server listening on 0.0.0.0:8888
启动客户端：
./bin/arcnet_client
# 连接成功后输入消息即可
测压：
# 编译压测工具
g++ -std=c++11 -pthread benchmark.cpp \
    buffer/ring_buffer.cpp \
    net/socket_ops.cpp \
    net/tcp_socket.cpp \
    protocol/message_codec.cpp \
    -I. -o bin/benchmark

# 运行压测（1000连接，每连接100条消息）
./bin/benchmark 1000 100
📁 目录结构
ArcNet/
├── buffer/                 # 环形缓冲区
│   ├── ring_buffer.h
│   └── ring_buffer.cpp
├── net/                    # 网络核心
│   ├── socket_ops.h/cpp    # socket 操作封装
│   ├── tcp_socket.h/cpp    # TCP Socket 类
│   ├── socket_manager.h    # Socket 管理器
│   └── workthread.h        # 工作线程
├── protocol/               # 协议编解码
│   ├── message.h           # 协议定义
│   ├── message_codec.h/cpp # 编解码实现
├── concurrency/            # 并发组件
│   ├── TaskQueue.h         # 任务队列
│   ├── SubReactor.h        # 子 Reactor
│   └── MainReactor.h       # 主 Reactor
├── bin/                    # 编译输出
├── build/                  # 临时文件
├── main.cpp                # 服务器入口
├── client_main.cpp         # 客户端入口
├── Makefile
└── README.md
编辑main.cpp：
if (!s.Listen(8888)) {  // 修改为想要的端口
修改缓冲区大小：
Socket s(0, 65536, 65536);  // 发送缓冲区 64KB，接收缓冲区 64KB
📝 协议格式
+----------+----------+----------+
|  Magic   |  Length  |   Body   |
| 4 bytes  | 2 bytes  | N bytes  |
+----------+----------+----------+

Magic: 0xABCD1234 (固定魔数)
Length: body 长度
Body: 实际数据
🛠️ 技术栈
语言：C++11

网络：epoll、socket、非阻塞 IO

并发：std::thread、std::atomic、std::mutex

内存管理：RAII、智能指针
📄 License
📄 License
MIT License

📧 联系方式
作者：Your Name

Email：your.email@example.com

GitHub：https://github.com/yourname/ArcNet
