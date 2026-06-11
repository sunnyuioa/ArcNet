#ifndef SOCKET_H
#define SOCKET_H

#include "../storage_manager/storage_manager.h"
#include "socket_ops.h"
#include <atomic>
#include "../mysql/DBWorker.h"
#include <mutex>
#include <ctime>
#include <map>
#include <vector>
#include <string>
#include "../buffer/ring_buffer.h"
#include "../protocol/message_codec.h"

struct UserInfo;

class Socket
{
public:
    Socket(SOCKET fd, uint32_t sendbuffersize, uint32_t recvbuffersize, DBWorker& m_mysqll);
    virtual ~Socket();
    bool Connect(const char* Address, uint32_t Port);
    void Disconnect();
    bool login(DBWorker & d, std::map<int, UserInfo> &s);
    void set_need_close(bool value) { m_need_close = value; }
    bool need_close() const { return m_need_close; }
    time_t last_heartbeat() const { return m_heartJitter; }
    void OnRead_(uint32_t size, char * tmp_buf);
    void OnRead(uint32_t size, char * tmp_buf);
    SOCKET Accept(sockaddr_in* address);
    bool Send(const uint8_t* Bytes, uint32_t Size);
    std::string buildContextForAI();
    bool regiser(DBWorker & d, std::map<int, UserInfo> &s);
    void OnConnect();
    void OnDisconnect();
    bool Send(CMessageOut &out);

    bool Listen(uint32_t port, uint32_t backlog = 10);
    inline void BurstBegin() { m_writeMutex.lock(); }
    bool BurstSend(const uint8_t* Bytes, uint32_t Size);
    void BurstPush();
    inline void BurstEnd() { m_writeMutex.unlock(); }
    string GetRemoteIP();
    void flushBufferToFile();
    inline uint32_t GetRemotePort() { return ntohs(m_client.sin_port); }
    inline SOCKET GetFd() { return m_fd; }
    inline void SetFd(SOCKET fd)
    {
        m_fd = fd;
        if (m_fd == 0)
        {
            m_fd = SocketOps::CreateTCPFileDescriptor();
        }
    }

    inline CircularBuffer& GetReadBuffer() { return readBuffer; }
    inline CircularBuffer& GetWriteBuffer() { return writeBuffer; }
    void SetupReadEvent();
    void ReadCallback(uint32_t len);
    void WriteCallback();
    void onreads();
    inline void Clear(void)
    {
        m_connected.store(false);
        m_deleted.store(false);
        m_html5connected.store(false);
        m_writeLock.store(false);
        m_htmlMsgProcessed.store(false);
        m_eventCount.store(0);
        m_readTimer.store(0);
        m_readMsgCount.store(0);
        m_readMsgBool.store(true);
        isRealRemovedFromSet.store(false);
        m_htmlMsgProcessed.store(false);
        m_buffer_pos = 0;
        nMinExpectedSize = 6;
        masksOffset = 0;
        payloadSize = 0;
        readBuffer.Remove(readBuffer.GetSize());
        writeBuffer.Remove(writeBuffer.GetSize());
        m_BytesSent = 0;
        m_BytesRecieved = 0;
        m_heartJitter = time(NULL);
        remaining = 0;
        compress = 0;
        opcode = 0;
        mchecksum = 0;
    }

    inline void SetHeartCount(time_t count) { m_heartJitter = count; }
    inline time_t GetHeartCount(void) { return m_heartJitter; }
    inline bool IsDeleted() { return m_deleted.load(); }
    inline bool IsConnected() { return m_connected.load(); }
    inline sockaddr_in& GetRemoteStruct() { return m_client; }
    void Delete();
    inline in_addr GetRemoteAddress() { return m_client.sin_addr; }

    // ---------- 公开：用于 SubReactor 初始化连接状态 ----------
    void _OnConnect();   // 从 protected 移到 public

    CircularBuffer readBuffer;
    CircularBuffer writeBuffer;
    std::atomic<int> m_eventCount = {0};
    std::atomic<bool> isRealRemovedFromSet = {false};

public:
    // ---------- 匹配相关（新增） ----------
    int m_chatMode = 0;          // 0:无 1:AI 2:真人
    int m_peerFd = -1;           // 对方 fd
    std::string m_username;      // 登录后存储用户名

    // ---------- AI 聊天历史 ----------
    std::vector<std::pair<std::string, std::string>> m_chatHistory;

    // ---------- 锁操作 ----------
    inline void IncSendLock() { ++m_writeLock; }
    inline void DecSendLock() { --m_writeLock; }
    inline bool AcquireSendLock()
    {
        if (m_writeLock.exchange(1) != 0)
            return false;
        else
            return true;
    }
    inline bool HasSendLock()
    {
        return (m_writeLock.load() != 0);
    }

    void PostEvent(uint32_t events);
    StorageManager storageManager;

    void PollTraffic(unsigned long* sent, unsigned long* recieved)
    {
        m_writeMutex.lock();
        *sent = m_BytesSent;
        *recieved = m_BytesRecieved;
        m_BytesSent = 0;
        m_BytesRecieved = 0;
        m_writeMutex.unlock();
    }

protected:
    SOCKET m_fd;
    string s;
    std::atomic<bool> m_need_close = {false};
    std::mutex m_writeMutex;
    std::mutex m_readMutex;
    std::atomic<bool> m_connected;
    std::atomic<bool> m_deleted;
    std::atomic<bool> m_html5connected;
    char m_buffer[500];
    unsigned long m_buffer_pos;
    std::atomic<bool> m_htmlMsgProcessed;
    int nMinExpectedSize;
    int masksOffset;
    int64_t payloadSize;
    unsigned long m_BytesSent;
    unsigned long m_BytesRecieved;
    time_t m_heartJitter;
    std::atomic<unsigned long> m_readTimer;
    std::atomic<int> m_readMsgCount;
    std::atomic<bool> m_readMsgBool;
    uint32_t remaining;
    uint16_t opcode;
    uint16_t compress;
    uint32_t mchecksum;

private:
    std::atomic<int> m_writeLock;
    sockaddr_in m_client;
    DBWorker *m_mysql;
};

template<class T>
T* ConnectTCPSocket(const char* hostname, u_short port)
{
    sockaddr_in conn;
    hostent* host;
    host = gethostbyname(hostname);
    if (!host)
        return NULL;
    memcpy(&conn.sin_addr, host->h_addr_list[0], sizeof(in_addr));
    conn.sin_family = AF_INET;
    conn.sin_port = htons(port);
    T* s = new T(0, 8192, 8192);
    if (!s->Connect(hostname, port))
    {
        s->Delete();
        return NULL;
    }
    return s;
}

struct UserInfo {
    std::string account;
    std::string username;
};

#endif