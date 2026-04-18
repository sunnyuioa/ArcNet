#include"net/tcp_socket.h"
#include<cstdint>
#include<cstring>
#include<iostream>
#include<sys/types.h>
#include<sys/socket.h>
using namespace std;

Socket::Socket(SOCKET fd, uint32_t sendbuffersize, uint32_t recvbuffersize) : m_fd(fd), m_connected(false), m_deleted(false), m_writeLock(0)
{
    readBuffer.Allocate(recvbuffersize);
    writeBuffer.Allocate(sendbuffersize);

    m_BytesSent = 0;
    m_BytesRecieved = 0;
    m_eventCount = 0;
    isRealRemovedFromSet.store(false);
    if(m_fd == 0)
    {
        m_fd = SocketOps::CreateTCPFileDescriptor();
        SocketOps::ReuseAddr(m_fd);
    }

    m_heartJitter = time(NULL);
    m_readTimer = 0;
    m_readMsgCount = 0;
    m_readMsgBool.store(false);

    m_buffer_pos = 0;
    nMinExpectedSize = 6;
    masksOffset = 0;
    payloadSize = 0;

    remaining = 0;
    compress = 0;
    opcode = 0;
    mchecksum = 0;
}

Socket::~Socket()
{
}

bool Socket::Listen(uint32_t port, uint32_t backlog)
{
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(m_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        return false;
    }
    if (listen(m_fd, backlog) == -1) {
        return false;
    }
    return true;
}

void Socket::BurstPush()
{
    WriteCallback();
}

bool Socket::Connect(const char* Address, uint32_t Port)
{ 
    struct hostent* ci = gethostbyname(Address);
    if(ci == 0)
        return false;
    m_client.sin_family = ci->h_addrtype;
    m_client.sin_port = htons((u_short)Port);
    ::memcpy(&this->m_client.sin_addr.s_addr, ci->h_addr_list[0], ci->h_length);
    SocketOps::Blocking(m_fd);
    if(connect(m_fd, (const sockaddr*)&m_client, sizeof(m_client)) == -1)
        return false;
    _OnConnect();
    return true;
}

SOCKET Socket::Accept(sockaddr_in* address)
{
    socklen_t len = sizeof(sockaddr_in);
    SOCKET client_fd = ::accept(m_fd, (struct sockaddr*)address, &len);
    if (client_fd == -1)
    {
        return -1;
    }
    ::memcpy(&m_client, address, sizeof(*address));
    _OnConnect();
    return client_fd;
}

void Socket::_OnConnect()
{
    SocketOps::Nonblocking(m_fd);           
    SocketOps::DisableBuffering(m_fd);       
    m_connected.store(true);
    OnConnect();
}

/*bool Socket::Send(CMessageOut &out)
{
    cout<<"正在尝试发送消息";
    if (out.getLength() <= 0 || out.getLength() > 10240 - sizeof(MsgHeader))
        return false;

    if (!IsConnected() || IsDeleted())
        return false;

    bool rv = true;
    cout<<"要发消息了哦";
    try
    {
        BurstBegin();
        uint8_t SendbufferData[10240] = { 0 };

        int uSendSize = out.getLength();

        MsgHeader header = out.getHeader();
        header.bodyLen = uSendSize;

        ::memcpy(SendbufferData, &header, sizeof(MsgHeader));
        ::memcpy(SendbufferData + sizeof(MsgHeader),
            (const uint8_t*)out.getData().body,
            uSendSize);

        uint32_t totalLen = sizeof(MsgHeader) + uSendSize;

        if (totalLen < 10240)
        {
            rv = BurstSend(SendbufferData, totalLen);
        }

        if (rv)
            BurstPush();
    }
    catch (const std::exception& e)
    {
        rv = false;
    }

    BurstEnd();
    return rv;
}*/
bool Socket::Send(CMessageOut &out)
{
   string msg=out.getData().body;
   send(m_fd,(const char*)msg.c_str(), out.getLength(), 0);
   return true;
}
void Socket::WriteCallback()
{
    if (writeBuffer.GetSize() == 0) {
        return;
    }
    char temp[4096];
    uint32_t len = writeBuffer.Read((uint8_t*)temp, 4096);
    send(m_fd, temp, len, 0);
}

bool Socket::BurstSend(const uint8_t* Bytes, uint32_t Size)
{
   /* return writeBuffer.Write(Bytes, Size);*/
  /* ::socket_send(m_fd, (const char*)Bytes, Size);*/
   return true;
}

string Socket::GetRemoteIP()
{
    char* ip = (char*)inet_ntoa(m_client.sin_addr);
    if(ip != NULL)
        return string(ip);
    else
        return string("noip");
}

void Socket::Disconnect()
{
    if(!m_connected.load())
        return;
    OnDisconnect();
    if(!IsDeleted())
        Delete();
}

void Socket::Delete()
{
    if(m_deleted.load())
        return;
    if(IsConnected()) Disconnect();
    SocketOps::CloseSocket(m_fd);
}

/*void Socket::OnRead(uint32_t size)
{
    cout<<"vvrvvrrbrb";
    char tmp_buf[4096];
    ssize_t n = recv(m_fd, tmp_buf, sizeof(tmp_buf), 0);
    if (n <= 0) {
        if (n == 0) {
            cout << "客户端主动断开连接" << endl;
        } else {
            perror("recv error");
        }
        Disconnect();
        return;
    }

    GetReadBuffer().Write((uint8_t*)tmp_buf, n);

    m_heartJitter = time(NULL);
    CircularBuffer& rbuf = GetReadBuffer();

    while (true)
    {
        if (rbuf.GetSize() < sizeof(MsgHeader))
            break;
            
        MsgHeader header;
        rbuf.Peek((uint8_t*)&header, sizeof(MsgHeader));
        
        if (header.magic != PROTO_MAGIC)
        {
            Disconnect();
            return;
        }
        
        uint32_t totalLen = sizeof(MsgHeader) + header.bodyLen;
        if (rbuf.GetSize() < totalLen)
            break;

        uint8_t* msgData = new uint8_t[totalLen];
        rbuf.Read(msgData, totalLen);
        
        try
        {
            CMolMessageIn in((const char*)(msgData + sizeof(MsgHeader)), header.bodyLen);
            int8_t msgType = in.read8();
            
            if (msgType == 50)
            {
                delete[] msgData;
                continue;
            }
            
            std::string chat_content = in.readString();
            cout << "收到聊天消息：" << chat_content << endl;
            ++m_readMsgCount;
        }
        catch (const std::exception& e)
        {
            cout << "消息处理异常：" << e.what() << endl;
        }
        
        delete[] msgData;
    }
}
*/
/*void Socket::OnRead(uint32_t size)
{
    cout << "OnRead called" << endl;
    
    char tmp_buf[4096];
    ssize_t n = recv(m_fd, tmp_buf, sizeof(tmp_buf), 0);
    if (n <= 0) {
        cout << "recv returned " << n << endl;
        if (n == 0) {
            cout << "客户端主动断开连接" << endl;
        } else {
            perror("recv error");
        }
        Disconnect();
        return;
    }
    
    // 打印收到的原始数据（十六进制）
    cout << "收到 " << n << " 字节: ";
    for (int i = 0; i < n && i < 32; i++) {
        printf("%02X ", (unsigned char)tmp_buf[i]);
    }
    cout << endl;
    
    GetReadBuffer().Write((uint8_t*)tmp_buf, n);
    
    CircularBuffer& rbuf = GetReadBuffer();
    cout << "缓冲区大小: " << rbuf.GetSize() << endl;

    while (true)
    {
        if (rbuf.GetSize() < sizeof(MsgHeader)) {
            cout << "缓冲区不足，需要 " << sizeof(MsgHeader) << " 字节，当前 " << rbuf.GetSize() << endl;
            break;
        }
            
        MsgHeader header;
        rbuf.Peek((uint8_t*)&header, sizeof(MsgHeader));
        
        cout << "读取到的魔数: 0x" << hex << header.magic << dec << endl;
        cout << "期望魔数: 0x" << hex << PROTO_MAGIC << dec << endl;
        
        if (header.magic != PROTO_MAGIC)
        {
            cout << "魔数不匹配，断开连接" << endl;
            Disconnect();
            return;
        }
        
        uint32_t totalLen = sizeof(MsgHeader) + header.bodyLen;
        cout << "消息总长度: " << totalLen << endl;
        
        if (rbuf.GetSize() < totalLen) {
            cout << "数据不足，需要 " << totalLen << " 字节，当前 " << rbuf.GetSize() << endl;
            break;
        }

        uint8_t* msgData = new uint8_t[totalLen];
        rbuf.Read(msgData, totalLen);
        
        try
        {
            CMolMessageIn in((const char*)(msgData + sizeof(MsgHeader)), header.bodyLen);
            int8_t msgType = in.read8();
            cout << "消息类型: " << (int)msgType << endl;
            
            if (msgType == 50)
            {
                delete[] msgData;
                continue;
            }
            
            std::string chat_content = in.readString();
            cout << "收到聊天消息：" << chat_content << endl;
            ++m_readMsgCount;
        }
        catch (const std::exception& e)
        {
            cout << "消息处理异常：" << e.what() << endl;
        }
        
        delete[] msgData;
    }
}*/
/*void Socket::OnRead(uint32_t size)
{
    char buf[4096];
    int n = recv(m_fd, buf, sizeof(buf) - 1, 0);
    if (n > 0) {
        buf[n] = '\0';
        std::cout << "收到: " << buf << std::endl;
        // 直接 echo 回去
        send(m_fd, buf, n, 0);
    } else if (n == 0) {
        std::cout << "客户端断开" << std::endl;
        Disconnect();
    } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
        perror("recv error");
        Disconnect();
    }
}*/
void Socket::OnRead(uint32_t size)
{
    char buf[4096];
    int n = 0;
    cout<<"         afafafefeaef  ";
    
    // 循环读取直到读完所有数据（ET 模式需要）
    while (true) {
        n = recv(m_fd, buf, sizeof(buf) - 1, 0);
        if (n > 0) {
            buf[n] = '\0';
            std::cout << "收到: " << buf << std::endl;
            // echo 回去
            send(m_fd, buf, n, 0);
        } else if (n == 0) {
            std::cout << "客户端断开" << std::endl;
            Disconnect();
            return;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 数据读完了，退出循环
                break;
            } else {
                perror("recv error");
                Disconnect();
                return;
            }
        }
    }
}
void Socket::OnConnect()
{
    // 可被子类重写
}

void Socket::OnDisconnect()
{
    // 可被子类重写
}

void Socket::SetupReadEvent()
{
    // epoll事件设置
}

void Socket::ReadCallback(uint32_t len)
{
    OnRead(len);
}

void Socket::PostEvent(uint32_t events)
{
    // 事件分发
}

/*bool Socket::Send(const uint8_t* Bytes, uint32_t Size)
{
    return writeBuffer.Write(Bytes, Size);
}*/