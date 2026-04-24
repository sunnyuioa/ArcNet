#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include <string>
#include <vector>

struct Message {
    std::string role;     
    std::string content;
    long timestamp;
    
    Message() : timestamp(0) {}
    Message(const std::string& r, const std::string& c, long t = 0) 
        : role(r), content(c), timestamp(t) {}
};

class StorageManager {
public:
    // 构造函数，每个 Socket 独立创建
    StorageManager() = default;
    ~StorageManager() = default;
    
    // 获取统计信息
    int getTotalUserMessages() const;
    int getTotalAIMessages() const;
    int getTotalCharacters() const;  // 人设字数
    int getTotalBytes() const;       // 总字节数
    
    void updateStatsAfterAppend(const std::string& role, const std::string& content);
    void updateStatsAfterSaveCharacter(const std::string& character);
    
    // 设置用户ID
    void setUserId(int user_id) { user_id_ = user_id; }
    int getUserId() const { return user_id_; }
    
    // 用户文件管理
    bool userExists() const;
    bool createUserFile();
    void deleteHistory();
    
    // 历史消息管理
    void saveHistory(const std::vector<Message>& history);
    std::vector<Message> loadHistory();
    void appendMessage(const std::string& role, const std::string& content);
    
    // 查询功能
    std::vector<Message> getRecent(int count);
    std::vector<Message> searchHistory(const std::string& keyword);
    int getMessageCount();
    void saveCharacter(const std::string& character);
    std::string loadCharacter();
    
private:
    std::string getFilePath() const;
    int user_id_ = 0;
};

#endif