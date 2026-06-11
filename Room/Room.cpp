#include "Room.h"
#include <algorithm>
#include <chrono>

Room::Room(int roomId, int creatorFd, const std::string& creatorName,
           int maxPlayers, const std::string& story,
           const std::vector<std::string>& roleNames)
    : roomId_(roomId), creatorFd_(creatorFd), maxPlayers_(maxPlayers),
      story_(story), roleNames_(roleNames) {
    // 创建者自动加入，扮演第一个角色
    addMember(creatorFd, creatorName, 0);
    // 初始化 AI 触发计时器
    m_lastAiTriggerTime = std::chrono::steady_clock::now();

    // 设置存储为房间模式，并加载历史（如果文件存在）
    storageManager_.setRoomMode(roomId_);
    loadHistoryFromFile();
}

bool Room::addMember(int fd, const std::string& userName, int roleIndex) {
    if (members_.size() >= maxPlayers_) return false;
    if (isMember(fd)) return false;
    // 检查角色是否已被占用
    for (const auto& m : members_) {
        if (m.roleIndex == roleIndex) return false;
    }
    size_t idx = members_.size();
    members_.push_back({fd, userName, roleIndex});
    fdToIndex_[fd] = idx;
    return true;
}

void Room::removeMember(int fd) {
    auto it = fdToIndex_.find(fd);
    if (it == fdToIndex_.end()) return;
    size_t idx = it->second;
    members_.erase(members_.begin() + idx);
    // 重建 fd -> 索引映射
    fdToIndex_.clear();
    for (size_t i = 0; i < members_.size(); ++i) {
        fdToIndex_[members_[i].fd] = i;
    }
}

bool Room::isMember(int fd) const {
    return fdToIndex_.find(fd) != fdToIndex_.end();
}

const std::string& Room::getMemberName(int fd) const {
    auto it = fdToIndex_.find(fd);
    if (it != fdToIndex_.end()) {
        return members_[it->second].name;
    }
    static const std::string empty;
    return empty;
}

int Room::getMemberRoleIndex(int fd) const {
    auto it = fdToIndex_.find(fd);
    if (it != fdToIndex_.end()) {
        return members_[it->second].roleIndex;
    }
    return -1;
}

std::vector<int> Room::getMemberFds() const {
    std::vector<int> fds;
    for (const auto& m : members_) {
        fds.push_back(m.fd);
    }
    return fds;
}

std::vector<Room::RoleStatus> Room::getRoleStatuses() const {
    std::vector<RoleStatus> statuses;
    for (size_t i = 0; i < roleNames_.size(); ++i) {
        RoleStatus rs;
        rs.roleName = roleNames_[i];
        rs.taken = false;
        rs.takenByFd = -1;
        // 检查是否被真实玩家占用
        for (const auto& m : members_) {
            if (m.roleIndex == static_cast<int>(i)) {
                rs.taken = true;
                rs.takenByFd = m.fd;
                break;
            }
        }
        // 如果未被真实玩家占用，检查是否被 AI 角色占用
        if (!rs.taken) {
            for (const auto& aiName : m_aiRoleNames) {
                auto it = std::find(roleNames_.begin(), roleNames_.end(), aiName);
                if (it != roleNames_.end() && static_cast<size_t>(it - roleNames_.begin()) == i) {
                    rs.taken = true;
                    rs.takenByFd = -1;  // AI 没有 fd
                    break;
                }
            }
        }
        statuses.push_back(rs);
    }
    return statuses;
}

void Room::setAIRoles(const std::vector<std::string>& names,
                      const std::vector<std::string>& personas) {
    if (names.size() != personas.size()) return;
    m_aiRoleNames = names;
    m_aiPersonas  = personas;
}

// -------- 聊天历史持久化 ----------
void Room::addHistory(const std::string& role, const std::string& content) {
    m_chatHistory.emplace_back(role, content);
    // 只保留最近 50 条内存记录（避免无限增长）
    if (m_chatHistory.size() > 50)
        m_chatHistory.erase(m_chatHistory.begin(), m_chatHistory.begin() + (m_chatHistory.size() - 50));

    // 实时写入 JSON 文件（可配置为批量写入以减少 IO，但简单起见实时写入）
    storageManager_.appendMessage(role, content);
}

void Room::loadHistoryFromFile() {
    auto history = storageManager_.loadHistory();
    for (const auto& msg : history) {
        // 只加载到内存，不触发文件写入
        m_chatHistory.emplace_back(msg.role, msg.content);
    }
    // 如果文件中的历史超过 50 条，仅保留最近 50 条
    if (m_chatHistory.size() > 50) {
        m_chatHistory.erase(m_chatHistory.begin(), m_chatHistory.begin() + (m_chatHistory.size() - 50));
    }
}