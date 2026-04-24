#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include"net/tcp_socket.h"
#include<cstdint>
#include<cstring>
#include<iostream>
#include<sys/types.h>
#include<sys/socket.h>
#include <unordered_map>
using namespace std;
#include <sstream>
#include <nlohmann/json.hpp>
using json = nlohmann::json;
// 将 \u4f60 这种格式转换成中文
std::string unicodeDecode(const std::string& str) {
    std::string result;
    for (size_t i = 0; i < str.length(); i++) {
        if (str[i] == '\\' && i + 5 < str.length() && str[i+1] == 'u') {
            // 解析 \uXXXX
            std::string hex = str.substr(i + 2, 4);
            unsigned int codepoint;
            std::stringstream ss;
            ss << std::hex << hex;
            ss >> codepoint;
            // UTF-8 编码
            if (codepoint <= 0x7F) {
                result += (char)codepoint;
            } else if (codepoint <= 0x7FF) {
                result += (char)(0xC0 | ((codepoint >> 6) & 0x1F));
                result += (char)(0x80 | (codepoint & 0x3F));
            } else {
                result += (char)(0xE0 | ((codepoint >> 12) & 0x0F));
                result += (char)(0x80 | ((codepoint >> 6) & 0x3F));
                result += (char)(0x80 | (codepoint & 0x3F));
            }
            i += 5;
        } else {
            result += str[i];
        }
    }
    return result;
}

std::string g_http_test_buffer; 
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
std::string extractStringValue(const std::string& body, const std::string& key) {
    std::string search = "\"" + key + "\":\"";
    size_t pos = body.find(search);
    if (pos == std::string::npos) return "";
    pos += search.length();
    size_t end = body.find("\"", pos);
    if (end == std::string::npos) return "";
    return body.substr(pos, end - pos);
}
bool Socket::Send(CMessageOut &out)
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
}
std::string getHttpRequestBody(const char* requestBuffer) {
   
    const char* split = strstr(requestBuffer, "\r\n\r\n");
    if (!split) {
        return ""; 
    }
    return std::string(split + 4);
}
std::string parseMessage(const std::string& body) {
    size_t pos = body.find("\"message\":\"");
    if (pos == string::npos) return "";
    size_t start = pos + 10;
    size_t end = body.find("\"", start);
    return body.substr(start, end - start);
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
 
  return ::send(m_fd, (const char*)Bytes, Size,0);
  
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
    
    m_deleted.store(true);
    m_connected.store(false);
    
    // ❌ 不要在这里 close(m_fd)
    // 让 reactor 统一处理 close
}
std::string urlDecode(const std::string& str) {
    std::string result;
    result.reserve(str.length());
    
    for (size_t i = 0; i < str.length(); i++) {
        if (str[i] == '+') {
            result += ' ';
        }
        else if (str[i] == '%') {
            if (i + 2 >= str.length()) {
                result += str[i];
                continue;
            }
            
            // 将十六进制字符串转换为字节
            unsigned char byte = 0;
            for (int j = 1; j <= 2; j++) {
                char c = str[i + j];
                byte <<= 4;
                if (c >= '0' && c <= '9') {
                    byte |= (c - '0');
                } else if (c >= 'A' && c <= 'F') {
                    byte |= (c - 'A' + 10);
                } else if (c >= 'a' && c <= 'f') {
                    byte |= (c - 'a' + 10);
                } else {
                    // 无效字符
                    byte = 0;
                    break;
                }
            }
            
            result += static_cast<char>(byte);
            i += 2;
        }
        else {
            result += str[i];
        }
    }
    
    return result;
}
// 辅助函数：转义 JSON 字符串中的特殊字符
std::string jsonEscape(const std::string& s) {
    std::string result;
    for (char c : s) {
        if (c == '"') result += "\\\"";
        else if (c == '\\') result += "\\\\";
        else if (c == '\n') result += "\\n";
        else if (c == '\r') result += "\\r";
        else result += c;
    }
    return result;
}

// curl 写回调
/*size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    size_t total = size * nmemb;
    output->append((char*)contents, total);
    return total;
}
*/

std::string callAIService(const std::string& user_message) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return "网络连接失败 💕";
    }
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(5000);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return "AI 服务连接失败 💕";
    }
    
    std::string json_body = "{\"messages\":[{\"role\":\"user\",\"content\":\"" + 
                            jsonEscape(user_message) + 
                            "\"}],\"temperature\":0.95,\"max_tokens\":120}";
    
    std::string http_request = 
        "POST /chat HTTP/1.1\r\n"
        "Host: 127.0.0.1:5000\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: " + std::to_string(json_body.length()) + "\r\n"
        "Connection: close\r\n"
        "\r\n" +
        json_body;
    
    send(sock, http_request.c_str(), http_request.size(), 0);
    
    std::string response;
    char buffer[4096];
    ssize_t n;
    while ((n = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[n] = '\0';
        response += buffer;
    }
    
    close(sock);
    
    // 找 HTTP body
    size_t body_start = response.find("\r\n\r\n");
    if (body_start == std::string::npos) {
        return "解析响应失败 💕";
    }
    
    std::string json_body_str = response.substr(body_start + 4);
    
    // 手动解析：找 "content":"xxx"
    std::string ai_reply;
    size_t pos = json_body_str.find("\"content\":\"");
    if (pos != std::string::npos) {
        pos += 11;
        size_t end = json_body_str.find("\"", pos);
        if (end != std::string::npos) {
            ai_reply = json_body_str.substr(pos, end - pos);
        }
    }
    
    // 处理转义
    size_t esc;
    while ((esc = ai_reply.find("\\n")) != std::string::npos) {
        ai_reply.replace(esc, 2, "\n");
    }
    while ((esc = ai_reply.find("\\\"")) != std::string::npos) {
        ai_reply.replace(esc, 2, "\"");
    }
    
   return unicodeDecode(ai_reply);
}
void Socket::flushBufferToFile() {
    // 从环形缓冲区读取所有消息
    CircularBuffer& rbuf = GetReadBuffer();
    size_t buffer_size = rbuf.GetSize();
    
    if (buffer_size == 0) {
        std::cout << "📭 缓冲区为空，无需保存" << std::endl;
        return;
    }
    
    // 读取缓冲区内容
    uint8_t* data = new uint8_t[buffer_size + 1];
    rbuf.Read(data, buffer_size);
    data[buffer_size] = '\0';
    
    std::string buffer_content((char*)data);
    delete[] data;
    
    // 解析缓冲区中的消息（格式："user: xxx\nassistant: yyy\n"）
    std::vector<std::pair<std::string, std::string>> messages;
    std::istringstream iss(buffer_content);
    std::string line;
    
    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        
        size_t colon_pos = line.find(": ");
        if (colon_pos != std::string::npos) {
            std::string role = line.substr(0, colon_pos);
            std::string content = line.substr(colon_pos + 2);
            messages.push_back({role, content});
        }
    }
    
    // 保存到 StorageManager（持久化）
    for (const auto& msg : messages) {
        storageManager.appendMessage(msg.first, msg.second);
    }
    
    std::cout << "💾 已将 " << messages.size() << " 条消息从缓冲区保存到 JSON 文件" << std::endl;
    
    // 清空缓冲区
    rbuf.Remove(buffer_size);
}
std::string Socket::buildContextForAI() {
    json messages_array = json::array();
    
    // 1️⃣ 从文件加载人设（作为 system 消息）
    std::string character = storageManager.loadCharacter();
    if (!character.empty()) {
        messages_array.push_back({
            {"role", "system"},
            {"content", character}
        });
    } else {
        // 默认人设
        messages_array.push_back({
            {"role", "system"},
            {"content", "你是用户的AI男友，温柔又带点霸道，会叫我宝宝，说话带颜文字，超级宠溺。"}
        });
    }
    
    // 2️⃣ 从缓冲区读取所有消息
    CircularBuffer& rbuf = GetReadBuffer();
    size_t buffer_size = rbuf.GetSize();
    
    if (buffer_size > 0) {
        uint8_t* data = new uint8_t[buffer_size + 1];
        rbuf.Peek(data, buffer_size);
        data[buffer_size] = '\0';
        std::string buffer_content((char*)data);
        delete[] data;
        
        // 3️⃣ 解析缓冲区中的消息（格式："user: xxx\nassistant: yyy\n"）
        std::istringstream iss(buffer_content);
        std::string line;
        
        while (std::getline(iss, line)) {
            if (line.empty()) continue;
            
            size_t colon_pos = line.find(": ");
            if (colon_pos != std::string::npos) {
                std::string role = line.substr(0, colon_pos);
                std::string content = line.substr(colon_pos + 2);
                
                // 转换 role 格式：user -> user, assistant -> assistant
                std::string llm_role = (role == "user") ? "user" : "assistant";
                
                messages_array.push_back({
                    {"role", llm_role},
                    {"content", content}
                });
            }
        }
    }
    
    // 4️⃣ 构建完整请求
    json request_json;
    request_json["messages"] = messages_array;
    request_json["temperature"] = 0.95;
    request_json["max_tokens"] = 120;
    
    return request_json.dump();
}
// 在 OnRead_ 函数之前添加声明
// 新增：带完整上下文的 AI 调用
std::string callAIServiceWithContext(const std::string& json_context) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return "网络连接失败 💕";
    }
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(5000);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return "AI 服务连接失败 💕";
    }
    
    std::string http_request = 
        "POST /chat HTTP/1.1\r\n"
        "Host: 127.0.0.1:5000\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: " + std::to_string(json_context.length()) + "\r\n"
        "Connection: close\r\n"
        "\r\n" +
        json_context;
    
    send(sock, http_request.c_str(), http_request.size(), 0);
    
    std::string response;
    char buffer[4096];
    ssize_t n;
    while ((n = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[n] = '\0';
        response += buffer;
    }
    
    close(sock);
    
    size_t body_start = response.find("\r\n\r\n");
    if (body_start == std::string::npos) {
        return "解析响应失败 💕";
    }
    
    std::string json_body_str = response.substr(body_start + 4);
    
    std::string ai_reply;
    size_t pos = json_body_str.find("\"content\":\"");
    if (pos != std::string::npos) {
        pos += 11;
        size_t end = json_body_str.find("\"", pos);
        if (end != std::string::npos) {
            ai_reply = json_body_str.substr(pos, end - pos);
        }
    }
    
    size_t esc;
    while ((esc = ai_reply.find("\\n")) != std::string::npos) {
        ai_reply.replace(esc, 2, "\n");
    }
    while ((esc = ai_reply.find("\\\"")) != std::string::npos) {
        ai_reply.replace(esc, 2, "\"");
    }
    
    return unicodeDecode(ai_reply);
}
void Socket::OnRead_(uint32_t size)
{
    static std::unordered_map<int, std::string> buffer;
    
    char tmp_buf[1024];
    ssize_t n = recv(m_fd, tmp_buf, sizeof(tmp_buf) - 1, 0);
    
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return;
        }
        perror("recv error");
        buffer.erase(m_fd);
        Delete();
        return;
    }
    
    if (n == 0) {
        std::cout << "客户端断开连接 fd=" << m_fd << std::endl;
        
        // 断开时保存缓冲区到文件
        flushBufferToFile();
        
        buffer.erase(m_fd);
        Delete();
        return;
    }
    
    tmp_buf[n] = '\0';
    buffer[m_fd] += tmp_buf;
    
    size_t header_end = buffer[m_fd].find("\r\n\r\n");
    if (header_end == std::string::npos) {
        return;
    }
    
    std::string& full_request = buffer[m_fd];
    std::cout << "\n======== 收到请求 ========\n" << full_request << std::endl;
    
    std::string response;
    
    // 处理 OPTIONS 预检请求
    if (full_request.find("OPTIONS") == 0) {
        response = 
            "HTTP/1.1 204 No Content\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Access-Control-Allow-Methods: POST, GET, OPTIONS\r\n"
            "Access-Control-Allow-Headers: Content-Type\r\n"
            "Content-Length: 0\r\n"
            "Connection: close\r\n\r\n";
        
        send(m_fd, response.c_str(), response.size(), 0);
        buffer.erase(m_fd);
        Delete();
        return;
    }
    
    // ========== 处理 POST 请求 ==========
    if (full_request.find("POST") == 0) {
        std::string body = getHttpRequestBody(full_request.c_str());
        std::cout << "原始 body: " << body << std::endl;
        
        std::string user_message = extractStringValue(body, "message");
        std::string user_id_str = extractStringValue(body, "user_id");
        std::string character = extractStringValue(body, "character");
        
        if (user_message.empty()) {
            user_message = extractStringValue(body, "chat");
        }
        
        while (!user_message.empty() && (user_message.back() == '\r' || user_message.back() == '\n')) {
            user_message.pop_back();
        }
        
        std::cout << "\n👤 用户ID: " << user_id_str << std::endl;
        std::cout << "💬 用户说: " << user_message << std::endl;
        
        // 转换 user_id 为整数
        int user_id = 0;
        for (char c : user_id_str) {
            user_id = user_id * 31 + (unsigned char)c;
        }
        if (user_id == 0 && !user_id_str.empty()) {
            user_id = std::hash<std::string>{}(user_id_str);
        }
        
        storageManager.setUserId(user_id);
        
        // ========== 检查文件并加载历史到缓冲区 ==========
        bool is_new_user = !storageManager.userExists();
        
        if (is_new_user) {
            // 新用户：创建文件
            storageManager.createUserFile();
            std::cout << "✨ 新用户，已创建文件" << std::endl;
        } else {
            // 老用户：加载历史到缓冲区
            int total_bytes = storageManager.getTotalBytes();
            int buffer_capacity = GetReadBuffer().GetSpace();
            int buffer_size = GetReadBuffer().GetSize();
            int available_space = buffer_capacity + buffer_size;
            
            int bytes_to_load;
            if (total_bytes <= available_space * 2 / 3) {
                bytes_to_load = total_bytes;
                std::cout << "📚 文件较小 (" << total_bytes << " 字节)，全部加载到缓冲区" << std::endl;
            } else {
                bytes_to_load = available_space * 2 / 3;
                std::cout << "📚 文件较大，只加载 " << bytes_to_load << " 字节到缓冲区" << std::endl;
            }
            
            auto history = storageManager.loadHistory();
            
            int msgs_to_load = 0;
            int accumulated_bytes = 0;
            for (int i = history.size() - 1; i >= 0; i--) {
                int msg_bytes = history[i].content.length() + history[i].role.length() + 4;
                if (accumulated_bytes + msg_bytes > bytes_to_load) {
                    break;
                }
                accumulated_bytes += msg_bytes;
                msgs_to_load++;
            }
            
            int start_idx = history.size() - msgs_to_load;
            if (start_idx < 0) start_idx = 0;
            
            // 清空缓冲区
            GetReadBuffer().Remove(GetReadBuffer().GetSize());
            
            // 加载历史到缓冲区
            for (int i = start_idx; i < history.size(); i++) {
                std::string line = history[i].role + ": " + history[i].content + "\n";
                GetReadBuffer().Write((uint8_t*)line.c_str(), line.size());
            }
            
            std::cout << "📚 加载了 " << msgs_to_load << " 条历史消息到缓冲区，共 " 
                      << accumulated_bytes << " 字节" << std::endl;
        }
        
        // ========== 分支处理 ==========
        if (!character.empty()) {
            // 分支1：保存人设（直接写入文件，不经过缓冲区）
            std::cout << "💾 保存人设: " << character << std::endl;
            storageManager.saveCharacter(character);
            
            // 更新统计（人设）
            storageManager.updateStatsAfterSaveCharacter(character);
            
            std::string json_body = "{\"status\":\"ok\",\"message\":\"人设已保存\"}";
            response = 
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: application/json\r\n"
                "Content-Length: " + std::to_string(json_body.length()) + "\r\n"
                "Access-Control-Allow-Origin: *\r\n"
                "Connection: close\r\n\r\n" +
                json_body;
        } 
        else if (!user_message.empty()) {
            // 分支2：聊天消息
            
            // 1️⃣ 用户消息存入缓冲区
            std::string user_entry = "user: " + user_message + "\n";
            GetReadBuffer().Write((uint8_t*)user_entry.c_str(), user_entry.size());
            std::cout << "📝 用户消息已存入缓冲区" << std::endl;
            
            // 2️⃣ 更新统计（用户消息）
            storageManager.updateStatsAfterAppend("user", user_message);
            
            // 3️⃣ 从缓冲区构建 AI 的 JSON 数据（包含人设）
            std::string json_context = buildContextForAI();
            std::cout << "📤 发送给 AI 的上下文长度: " << json_context.length() << " 字节" << std::endl;
            
            // 4️⃣ 调用 AI 服务
            std::string ai_reply = callAIServiceWithContext(json_context);
            std::cout << "🤖 AI 回复: " << ai_reply << std::endl;
            
            // 5️⃣ AI 回复存入缓冲区
            std::string ai_entry = "assistant: " + ai_reply + "\n";
            GetReadBuffer().Write((uint8_t*)ai_entry.c_str(), ai_entry.size());
            std::cout << "📝 AI 回复已存入缓冲区" << std::endl;
            
            // 6️⃣ 更新统计（AI 消息）
            storageManager.updateStatsAfterAppend("assistant", ai_reply);
            
            // 7️⃣ 构造 JSON 响应返回给前端
            std::string escaped_reply = jsonEscape(ai_reply);
            std::string json_body = "{\"reply\":\"" + escaped_reply + "\"}";
            response = 
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: application/json\r\n"
                "Content-Length: " + std::to_string(json_body.length()) + "\r\n"
                "Access-Control-Allow-Origin: *\r\n"
                "Connection: close\r\n\r\n" +
                json_body;
        }
        else {
            response = 
                "HTTP/1.1 400 Bad Request\r\n"
                "Content-Length: 0\r\n"
                "Connection: close\r\n\r\n";
        }
    } 
    else {
        response = 
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html; charset=utf-8\r\n"
            "Content-Length: 50\r\n"
            "Connection: close\r\n\r\n"
            "<html><body>ArcNet Server Running</body></html>";
    }
    
    send(m_fd, response.c_str(), response.size(), 0);
    std::cout << "📤 已发送响应给前端" << std::endl;
    
    buffer.erase(m_fd);
    Delete();
}
void Socket::OnRead(uint32_t size)
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
void Socket::OnConnect()
{
    // 可被子类重写
}

void Socket::OnDisconnect()
{
    // 可被子类重写
}
// 从缓冲区构建发给 AI 的 JSON 数据（包含人设）

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
