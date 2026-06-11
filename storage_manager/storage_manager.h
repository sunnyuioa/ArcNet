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
    StorageManager() = default;
    ~StorageManager() = default;

    // ---------- 模式切换 ----------
    void setUserId(int user_id);      // 兼容旧代码，自动切换为 user_ 前缀
    void setUserMode(int user_id);
    void setRoomMode(int room_id);

    // ---------- 文件管理 ----------
    bool userExists() const;          // 兼容旧名，实际检查文件是否存在
    bool fileExists() const;
    bool createUserFile();            // 兼容旧名，创建基础文件
    bool createFile();
    void deleteHistory();

    // ---------- 历史消息管理 ----------
    void saveHistory(const std::vector<Message>& history);
    std::vector<Message> loadHistory();
    void appendMessage(const std::string& role, const std::string& content);

    // ---------- 查询功能 ----------
    std::vector<Message> getRecent(int count);
    std::vector<Message> searchHistory(const std::string& keyword);
    int getMessageCount();
    void saveCharacter(const std::string& character);
    std::string loadCharacter();

    // 统计相关（房间模式一般不用，但保留）
    int getTotalUserMessages() const;
    int getTotalAIMessages() const;
    int getTotalCharacters() const;
    int getTotalBytes() const;
    void updateStatsAfterAppend(const std::string& role, const std::string& content);
    void updateStatsAfterSaveCharacter(const std::string& character);

    int getId() const { return id_; }

private:
    std::string getFilePath() const;
    int id_ = 0;
    std::string filePrefix_ = "user_";  // 默认用户模式
};

#endif