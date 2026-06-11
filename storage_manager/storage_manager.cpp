#include "storage_manager.h"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <experimental/filesystem>

using json = nlohmann::json;

// ---------- 模式切换 ----------
void StorageManager::setUserId(int user_id) {
    setUserMode(user_id);
}

void StorageManager::setUserMode(int user_id) {
    id_ = user_id;
    filePrefix_ = "user_";
}

void StorageManager::setRoomMode(int room_id) {
    id_ = room_id;
    filePrefix_ = "room_";
}

// 通用文件路径生成
std::string StorageManager::getFilePath() const {
    return filePrefix_ + std::to_string(id_) + ".json";
}

// 兼容旧名：检查文件是否存在
bool StorageManager::userExists() const {
    return std::experimental::filesystem::exists(getFilePath());
}

bool StorageManager::fileExists() const {
    return std::experimental::filesystem::exists(getFilePath());
}

// ---------- 历史消息保存/加载 ----------
void StorageManager::saveHistory(const std::vector<Message>& history) {
    json j;
    j["type"] = filePrefix_ == "user_" ? "user" : "room";
    j["id"] = id_;
    j["last_update"] = time(nullptr);
    j["total_messages"] = history.size();
    
    for (const auto& msg : history) {
        j["messages"].push_back({
            {"role", msg.role},
            {"content", msg.content},
            {"time", msg.timestamp}
        });
    }
    
    std::string filename = getFilePath();
    std::ofstream file(filename);
    if (file.is_open()) {
        file << j.dump(4);
        file.close();
        std::cout << "✅ 保存 " << filePrefix_ << id_ << " 历史，共 " << history.size() << " 条" << std::endl;
    } else {
        std::cerr << "❌ 无法保存 " << filePrefix_ << id_ << " 历史" << std::endl;
    }
}

std::vector<Message> StorageManager::loadHistory() {
    std::string filename = getFilePath();
    std::ifstream file(filename);
    if (!file.is_open()) {
        return {};
    }
    
    json j;
    file >> j;
    file.close();
    
    std::vector<Message> history;
    if (j.contains("messages")) {
        for (const auto& msg : j["messages"]) {
            Message m;
            m.role = msg.value("role", "");
            m.content = msg.value("content", "");
            m.timestamp = msg.value("time", 0L);
            history.push_back(m);
        }
    }
    
    std::cout << "📂 加载 " << filePrefix_ << id_ << " 历史，共 " << history.size() << " 条" << std::endl;
    return history;
}

void StorageManager::appendMessage(const std::string& role, const std::string& content) {
    if (!fileExists()) {
        createUserFile();
    }
    
    auto history = loadHistory();
    Message msg(role, content, time(nullptr));
    history.push_back(msg);
    
    // 只保留最近 500 条
    const int MAX_HISTORY = 500;
    if (history.size() > MAX_HISTORY) {
        history.erase(history.begin(), history.begin() + (history.size() - MAX_HISTORY));
    }
    saveHistory(history);
    
    // 更新统计（仅用户模式需要统计，房间模式可跳过）
    if (filePrefix_ == "user_") {
        updateStatsAfterAppend(role, content);
    }
}

// ---------- 文件创建 ----------
bool StorageManager::createUserFile() {
    std::string filename = getFilePath();
    if (std::experimental::filesystem::exists(filename)) {
        std::cout << "⚠️ 文件已存在: " << filename << std::endl;
        return false;
    }
    
    json j;
    j["type"] = filePrefix_ == "user_" ? "user" : "room";
    j["id"] = id_;
    j["messages"] = json::array();
    j["total_messages"] = 0;
    j["last_update"] = time(nullptr);
    
    if (filePrefix_ == "user_") {
        j["character"] = "";
        j["stats"] = json::object();
        j["stats"]["user_messages"] = 0;
        j["stats"]["ai_messages"] = 0;
        j["stats"]["total_bytes"] = 0;
        j["stats"]["character_bytes"] = 0;
    }
    
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "❌ 创建文件失败: " << filename << std::endl;
        return false;
    }
    
    file << j.dump(4);
    file.close();
    std::cout << "✨ 创建新文件成功: " << filename << std::endl;
    return true;
}

void StorageManager::deleteHistory() {
    std::string filename = getFilePath();
    if (std::experimental::filesystem::remove(filename)) {
        std::cout << "🗑️ 删除文件 " << filename << std::endl;
    }
}

// ---------- 查询功能 ----------
std::vector<Message> StorageManager::getRecent(int count) {
    auto history = loadHistory();
    if (history.size() <= count) {
        return history;
    }
    return std::vector<Message>(history.end() - count, history.end());
}

std::vector<Message> StorageManager::searchHistory(const std::string& keyword) {
    auto history = loadHistory();
    std::vector<Message> results;
    for (const auto& msg : history) {
        if (msg.content.find(keyword) != std::string::npos) {
            results.push_back(msg);
        }
    }
    std::cout << "🔍 搜索 '" << keyword << "'，找到 " << results.size() << " 条" << std::endl;
    return results;
}

int StorageManager::getMessageCount() {
    return loadHistory().size();
}

// ---------- 人设（仅用户模式有效） ----------
void StorageManager::saveCharacter(const std::string& character) {
    std::string filename = getFilePath();
    std::ifstream file(filename);
    
    json j;
    if (file.is_open()) {
        file >> j;
        file.close();
    } else {
        j["type"] = filePrefix_ == "user_" ? "user" : "room";
        j["id"] = id_;
        j["messages"] = json::array();
        j["total_messages"] = 0;
        j["last_update"] = time(nullptr);
        if (filePrefix_ == "user_") {
            j["stats"] = json::object();
            j["stats"]["user_messages"] = 0;
            j["stats"]["ai_messages"] = 0;
            j["stats"]["total_bytes"] = 0;
            j["stats"]["character_bytes"] = 0;
        }
    }
    
    j["character"] = character;
    j["last_update"] = time(nullptr);
    
    // 更新人设字数统计
    int char_bytes = character.length();
    if (filePrefix_ == "user_") {
        j["stats"]["character_bytes"] = char_bytes;
        int msg_bytes = j["stats"].value("total_bytes", 0);
        j["stats"]["total_bytes"] = msg_bytes + char_bytes;
    }
    
    std::ofstream outFile(filename);
    if (outFile.is_open()) {
        outFile << j.dump(4);
        outFile.close();
        std::cout << "💾 保存 " << filePrefix_ << id_ << " 的人设: " << character << std::endl;
    }
}

std::string StorageManager::loadCharacter() {
    std::string filename = getFilePath();
    std::ifstream file(filename);
    if (!file.is_open()) return "";
    json j;
    file >> j;
    file.close();
    return j.value("character", "");
}

// ---------- 统计（仅用户模式） ----------
void StorageManager::updateStatsAfterAppend(const std::string& role, const std::string& content) {
    if (filePrefix_ != "user_") return;
    
    std::string filename = getFilePath();
    std::ifstream file(filename);
    if (!file.is_open()) return;
    
    json j;
    file >> j;
    file.close();
    
    if (!j.contains("stats")) {
        j["stats"] = json::object();
        j["stats"]["user_messages"] = 0;
        j["stats"]["ai_messages"] = 0;
        j["stats"]["total_bytes"] = 0;
        j["stats"]["character_bytes"] = 0;
    }
    
    if (role == "user") {
        j["stats"]["user_messages"] = j["stats"]["user_messages"].get<int>() + 1;
    } else if (role == "assistant") {
        j["stats"]["ai_messages"] = j["stats"]["ai_messages"].get<int>() + 1;
    }
    
    int current_msg_bytes = j["stats"]["total_bytes"].get<int>();
    j["stats"]["total_bytes"] = current_msg_bytes + (int)content.length();
    
    j["total_messages"] = j["stats"]["user_messages"].get<int>() + j["stats"]["ai_messages"].get<int>();
    j["last_update"] = time(nullptr);
    
    std::ofstream outFile(filename);
    if (outFile.is_open()) {
        outFile << j.dump(4);
        outFile.close();
    }
}

void StorageManager::updateStatsAfterSaveCharacter(const std::string& character) {
    saveCharacter(character);
}

int StorageManager::getTotalUserMessages() const {
    if (filePrefix_ != "user_") return 0;
    std::string filename = getFilePath();
    std::ifstream file(filename);
    if (!file.is_open()) return 0;
    json j;
    file >> j;
    file.close();
    return j["stats"].value("user_messages", 0);
}

int StorageManager::getTotalAIMessages() const {
    if (filePrefix_ != "user_") return 0;
    std::string filename = getFilePath();
    std::ifstream file(filename);
    if (!file.is_open()) return 0;
    json j;
    file >> j;
    file.close();
    return j["stats"].value("ai_messages", 0);
}

int StorageManager::getTotalCharacters() const {
    if (filePrefix_ != "user_") return 0;
    std::string filename = getFilePath();
    std::ifstream file(filename);
    if (!file.is_open()) return 0;
    json j;
    file >> j;
    file.close();
    return j["stats"].value("character_bytes", 0);
}

int StorageManager::getTotalBytes() const {
    if (filePrefix_ != "user_") return 0;
    std::string filename = getFilePath();
    std::ifstream file(filename);
    if (!file.is_open()) return 0;
    json j;
    file >> j;
    file.close();
    return j["stats"].value("total_bytes", 0);
}