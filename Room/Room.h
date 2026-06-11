#ifndef ROOM_H
#define ROOM_H

#include <vector>
#include <string>
#include <unordered_map>
#include <set>
#include <chrono>
#include "../storage_manager/storage_manager.h"  // 引入存储管理器

class Room {
public:
    Room(int roomId, int creatorFd, const std::string& creatorName,
         int maxPlayers, const std::string& story,
         const std::vector<std::string>& roleNames);

    // -------- 基本信息 --------
    int getRoomId() const { return roomId_; }
    int getMaxPlayers() const { return maxPlayers_; }
    int getCurrentPlayers() const { return members_.size(); }
    const std::string& getStory() const { return story_; }
    const std::vector<std::string>& getRoleNames() const { return roleNames_; }

    // -------- 成员管理 --------
    bool addMember(int fd, const std::string& userName, int roleIndex);
    void removeMember(int fd);
    bool isMember(int fd) const;
    const std::string& getMemberName(int fd) const;
    int getMemberRoleIndex(int fd) const;
    std::vector<int> getMemberFds() const;

    struct RoleStatus {
        std::string roleName;
        bool taken;
        int takenByFd;
    };
    std::vector<RoleStatus> getRoleStatuses() const;

    // -------- AI 角色管理 --------
    void setAIRoles(const std::vector<std::string>& names,
                    const std::vector<std::string>& personas);

    const std::vector<std::string>& getAIRoleNames() const { return m_aiRoleNames; }
    const std::vector<std::string>& getAIPersonas() const { return m_aiPersonas; }
    bool hasAIPlayer() const { return !m_aiRoleNames.empty(); }

    std::string getAIPersona(const std::string& roleName) const {
        for (size_t i = 0; i < m_aiRoleNames.size(); ++i) {
            if (m_aiRoleNames[i] == roleName)
                return m_aiPersonas[i];
        }
        return "";
    }

    // -------- 聊天历史（内存 + 文件持久化）--------
    void addHistory(const std::string& role, const std::string& content);
    const std::vector<std::pair<std::string, std::string>>& getHistory() const {
        return m_chatHistory;
    }

    // 从文件加载历史到内存
    void loadHistoryFromFile();

    // -------- 防刷屏 --------
    bool canSpeak(int fd) const {
        auto it = m_lastSpeakTime.find(fd);
        if (it == m_lastSpeakTime.end()) return true;
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                           std::chrono::steady_clock::now() - it->second).count();
        return elapsed >= 5;  // 5 秒间隔
    }

    void recordSpeakTime(int fd) {
        m_lastSpeakTime[fd] = std::chrono::steady_clock::now();
    }

    // -------- 发言轮次 --------
    void recordSpoken(int fd) { m_spokenThisRound.insert(fd); }
    bool allRealPlayersSpoke() const { return m_spokenThisRound.size() >= members_.size(); }
    void resetSpokenRound() { m_spokenThisRound.clear(); }

    // -------- AI 触发计时 --------
    bool aiCooldownExpired() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::seconds>(now - m_lastAiTriggerTime).count() >= 60;
    }
    void updateAiTriggerTime() { m_lastAiTriggerTime = std::chrono::steady_clock::now(); }

    // 检查文本是否提到了 AI 角色名
    bool mentionsAIRole(const std::string& text) const {
        for (const auto& name : m_aiRoleNames) {
            if (text.find(name) != std::string::npos) return true;
        }
        return false;
    }

private:
    int roomId_;
    int creatorFd_;
    int maxPlayers_;
    std::string story_;
    std::vector<std::string> roleNames_;

    // 真实玩家
    struct Member {
        int fd;
        std::string name;
        int roleIndex;
    };
    std::vector<Member> members_;
    std::unordered_map<int, size_t> fdToIndex_;

    // AI 配置
    std::vector<std::string> m_aiRoleNames;
    std::vector<std::string> m_aiPersonas;

    // 聊天历史 (内存缓冲区)
    std::vector<std::pair<std::string, std::string>> m_chatHistory;

    // 防刷屏
    std::unordered_map<int, std::chrono::steady_clock::time_point> m_lastSpeakTime;

    // 本轮已发言的真实玩家 fd 集合
    std::set<int> m_spokenThisRound;

    // 上次 AI 回复的时间
    std::chrono::steady_clock::time_point m_lastAiTriggerTime;

    // 持久化存储（房间模式）
    StorageManager storageManager_;
};

#endif // ROOM_H