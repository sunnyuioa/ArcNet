# ArcNet - 高性能C++网络框架

[![C++17](https://img.shields.io/badge/C++-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![Linux](https://img.shields.io/badge/Linux-支持-green.svg)](https://www.linux.org/)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

## 📖 简介

ArcNet 是一个基于 Reactor 模式的高性能 C++ 网络框架，采用 epoll + 非阻塞 IO + CPU 亲和性绑定架构，单机可支持 **10,000+ 并发连接**，QPS 可达 **5w+**。

> 💡 本项目由我独立设计架构、拆分模块，使用 AI 辅助编码实现，自己调优。核心模块（Reactor 模式、环形缓冲区、协议解析、负载均衡）的设计思路和关键逻辑均由我完成。

## ✨ 核心特性

| 特性 | 说明 |
|------|------|
| **Reactor 架构** | 主 Reactor 负责 accept，SubReactor 负责 IO 事件，职责分离 |
| **CPU 亲和性绑定** | 每个 SubReactor 绑定独立 CPU 核心，减少缓存 miss |
| **双协议支持** | HTTP + 自定义二进制协议，无缝切换 |
| **零拷贝环形缓冲区** | 解决 TCP 粘包问题，减少内存拷贝 |
| **心跳保活机制** | 30 秒检测，90 秒超时自动断链 |
| **会话持久化** | 用户消息历史 JSON 存储，支持人设保存 |
| **AI 服务集成** | 内置 AI 对话上下文管理和调用接口 |

## 🏗️ 架构设计
┌─────────────────────────────────────────────────────────────┐
│ MainReactor │
│ (监听 + 负载均衡) │
└───────────┬───────────┬───────────┬───────────┬─────────────┘
│ │ │ │
▼ ▼ ▼ ▼
┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐
│SubReactor│ │SubReactor│ │SubReactor│ │SubReactor│
│(CPU 0) │ │(CPU 1) │ │(CPU 2) │ │(CPU 3) │
│ epoll │ │ epoll │ │ epoll │ │ epoll │
│ Thread │ │ Thread │ │ Thread │ │ Thread │
└────┬────┘ └────┬────┘ └────┬────┘ └────┬────┘
│ │ │ │
└───────────┴───────────┴───────────┘
│
┌────┴────┐
│ Socket │
│+ RingBuf│
│+ 心跳检测 │
│+ Storage│
└─────────┘

text

## 📊 性能测试

**测试环境**：
- CPU: 4 核
- 内存: 8GB
- OS: Ubuntu 20.04
- 协议: 二进制模式

**压测结果**：

| 测试场景 | 连接数 | 总消息数 | QPS | 错误率 |
|----------|--------|----------|-----|--------|
| 基础压测 | 100 | 10,000 | 35,000 | 0% |
| 中等压测 | 500 | 100,000 | 52,000 | 0% |
| 高压测试 | 2,000 | 100,000 | 48,000 | 0.01% |

**长连接稳定性**：500 连接持续运行 30 秒，成功发送 24.6 万条心跳，**无崩溃，无内存泄漏**。

## 🚀 快速开始

### 环境要求

- Linux 操作系统（epoll 依赖）
- g++ 7.0+（支持 C++17）
- CMake 3.10+ / make
- nlohmann/json 库（可选，AI 功能需要）

### 编译

```bash
# 克隆项目
git clone https://github.com/sunnyuioa/ArcNet.git
cd ArcNet

# 使用 Makefile 编译
make clean
make

# 或使用 CMake
mkdir build && cd build
cmake .. && make -j$(nproc)
运行
bash
# 启动服务器（端口 8888）
./bin/arcnet_server

# 输出：
# ✅ ArcNet Server listening on 0.0.0.0:8888
# MainReactor 启动，监听 fd=x
测试客户端
bash
# 二进制协议客户端
./bin/arcnet_client

# 连接成功后输入消息即可
> 你好服务器
# ✅ 已发送：你好服务器
Web AI 聊天
bash
# 1. 修改 web/index.html 中的服务器地址
# 将 YOUR_SERVER_IP 改为实际 IP

# 2. 启动 HTTP 服务
python3 -m http.server 8080

# 3. 浏览器访问
http://localhost:8080
📡 协议说明
ArcNet 支持两种协议模式，通过 SubReactor 构造函数切换：

cpp
// HTTP 模式（默认，用于 Web 前端）
SubReactor* reactor = new SubReactor(cpu_id, false);

// 二进制模式（高性能客户端）
SubReactor* reactor = new SubReactor(cpu_id, true);
二进制协议格式
text
┌────────────┬────────────┬────────────┐
│   Magic    │  HeaderLen │  BodyLen   │
│  (4 bytes) │  (2 bytes) │  (2 bytes) │
├────────────┴────────────┴────────────┤
│            Body (变长)                │
└──────────────────────────────────────┘
字段	大小	值	说明
Magic	4 字节	0xABCD1234	固定魔数，校验用
HeaderLen	2 字节	8	头部长度
BodyLen	2 字节	变长	消息体长度
Body	变长	业务数据	第一个字节为消息类型
HTTP API（AI 服务）
端点	方法	请求字段	响应字段	说明
/chat	POST	message, user_id	reply	发送消息
/chat	POST	character	status	保存人设
🛠️ 技术栈
类别	技术
语言	C++17
网络	epoll、socket、非阻塞 IO、ET 模式
并发	std::thread、std::atomic、std::mutex、CPU 亲和性
内存	RAII、环形缓冲区、零拷贝
协议	HTTP/1.1、自定义二进制协议
存储	JSON 文件持久化
📁 目录结构
text
ArcNet/
├── buffer/                 # 环形缓冲区
│   ├── ring_buffer.h
│   └── ring_buffer.cpp
├── net/                    # 网络核心层
│   ├── socket_ops.h/cpp    # socket 操作封装
│   ├── tcp_socket.h/cpp    # TCP Socket 类（核心业务）
│   ├── socket_manager.h    # Socket 管理器
│   └── workthread.h        # 工作线程
├── protocol/               # 协议编解码
│   ├── message.h           # 协议定义（魔数、消息头）
│   ├── message_codec.h/cpp # 编解码实现
├── concurrency/            # 并发组件
│   ├── TaskQueue.h         # 任务队列
│   ├── WorkerThread.h      # 工作线程
│   ├── SubReactor.h        # 子 Reactor（IO 处理）
│   └── MainReactor.h       # 主 Reactor（监听+负载均衡）
├── storage_manager/        # 数据持久化
│   └── storage_manager.h   # JSON 存储用户历史
├── web/                    # 前端页面
│   └── index.html          # AI 聊天界面
├── common/                 # 公共头文件
│   └── common.h
├── bin/                    # 编译输出
├── main.cpp                # 服务器入口
├── client_main.cpp         # 客户端入口
├── benchmark.cpp           # 压测工具
├── Makefile
└── README.md
⚙️ 配置说明
修改端口
cpp
// main.cpp
if (!s.Listen(8888)) {  // 改成你想要的端口
修改缓冲区大小
cpp
// 参数：fd, 发送缓冲区大小, 接收缓冲区大小
Socket s(0, 65536, 65536);  // 64KB / 64KB
修改心跳超时阈值
cpp
// SubReactor.h
if (now - last_heartbeat_check >= 30) {  // 检测间隔
    if (now - s->GetHeartCount() > 90) {  // 超时阈值
        s->Delete();
    }
}
🔧 开发指南
添加自定义消息处理
cpp
class MySocket : public Socket {
public:
    void OnRead(uint32_t size) override {
        // 自定义二进制协议解析
        CircularBuffer& buf = GetReadBuffer();
        // 解析消息...
        UpdateHeartbeat();  // 更新心跳
    }
};
添加新 API 端点
cpp
// 在 Socket::HandleHttpRequest() 中添加
if (request.find("POST /myapi") == 0) {
    // 处理请求
    return "HTTP/1.1 200 OK\r\n...";
}
🐛 调试
启用详细日志
cpp
// common.h 或编译时
#define DEBUG_LOG

// 或在 Makefile 中添加
CXXFLAGS += -DDEBUG
查看连接状态
bash
# 查看连接数
ss -tan | grep :8888 | grep ESTABLISHED | wc -l

# 查看进程资源
top -p $(pidof arcnet_server)
📝 待办事项
添加 TLS/SSL 支持

实现内存池优化

支持 IPv6

配置文件热加载

增加更多协议支持（WebSocket）

🤝 贡献指南
欢迎提交 Issue 和 Pull Request！

📄 License
MIT License

Copyright (c) 2024 sunnyuioa

📧 联系方式
作者：sunnyuioa

Email：jxgz777@qq.com

GitHub：https://github.com/sunnyuioa/ArcNet

<p align="center"> Made with ❤️ by sunnyuioa </p> ```