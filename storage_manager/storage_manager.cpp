#include "storage_manager.h"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <experimental/filesystem>

using json = nlohmann::json;

std::string StorageManager::getFilePath() const {
    return "user_" + std::to_string(user_id_) + ".json";
}

bool StorageManager::userExists() const {
    std::string filename = getFilePath();
    return std::experimental::filesystem::exists(filename);
}
// 从缓冲区构建发给 AI 的 JSON 数据（包含人设）

void StorageManager::saveHistory(const std::vector<Message>& history) {
    json j;
    j["user_id"] = user_id_;
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
        std::cout << "✅ 保存用户 " << user_id_ << " 历史，共 " << history.size() << " 条" << std::endl;
    } else {
        std::cerr << "❌ 无法保存用户 " << user_id_ << " 历史" << std::endl;
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
    
    std::cout << "📂 加载用户 " << user_id_ << " 历史，共 " << history.size() << " 条" << std::endl;
    return history;
}

void StorageManager::appendMessage(const std::string& role, const std::string& content) {
    // 如果用户不存在，先创建
    if (!userExists()) {
        createUserFile();
    }
    
    // 然后正常追加消息
    auto history = loadHistory();
    Message msg(role, content, time(nullptr));
    history.push_back(msg);
    
    // 只保留最近 500 条
    const int MAX_HISTORY = 500;
    if (history.size() > MAX_HISTORY) {
        history.erase(history.begin(), history.begin() + (history.size() - MAX_HISTORY));
    }
    saveHistory(history);
    
    // 更新统计
    updateStatsAfterAppend(role, content);
}

bool StorageManager::createUserFile() {
    std::string filename = getFilePath();
    
    if (std::experimental::filesystem::exists(filename)) {
        std::cout << "⚠️ 用户文件已存在: " << filename << std::endl;
        return false;
    }
    
    json j;
    j["user_id"] = user_id_;
    j["messages"] = json::array();
    j["total_messages"] = 0;
    j["last_update"] = time(nullptr);
    j["character"] = "";
    
    // 初始化统计字段
    j["stats"] = json::object();
    j["stats"]["user_messages"] = 0;
    j["stats"]["ai_messages"] = 0;
    j["stats"]["total_bytes"] = 0;
    j["stats"]["character_bytes"] = 0;
    
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "❌ 创建用户文件失败: " << filename << std::endl;
        return false;
    }
    
    file << j.dump(4);
    file.close();
    
    std::cout << "✨ 创建新用户文件成功: " << filename << std::endl;
    return true;
}

void StorageManager::deleteHistory() {
    std::string filename = getFilePath();
    if (std::experimental::filesystem::remove(filename)) {
        std::cout << "🗑️ 删除用户 " << user_id_ << " 的历史文件" << std::endl;
    }
}

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
    
    std::cout << "🔍 用户 " << user_id_ << " 搜索 '" << keyword << "'，找到 " << results.size() << " 条" << std::endl;
    return results;
}

int StorageManager::getMessageCount() {
    auto history = loadHistory();
    return history.size();
}

void StorageManager::saveCharacter(const std::string& character) {
    std::string filename = getFilePath();
    std::ifstream file(filename);
    
    json j;
    if (file.is_open()) {
        file >> j;
        file.close();
    } else {
        // 文件不存在，创建基础结构
        j["user_id"] = user_id_;
        j["messages"] = json::array();
        j["total_messages"] = 0;
        j["last_update"] = time(nullptr);
        
        // 初始化统计字段
        j["stats"] = json::object();
        j["stats"]["user_messages"] = 0;
        j["stats"]["ai_messages"] = 0;
        j["stats"]["total_bytes"] = 0;
        j["stats"]["character_bytes"] = 0;
    }
    
    // 更新人设
    j["character"] = character;
    j["last_update"] = time(nullptr);
    
    // 更新人设字数统计
    int char_bytes = character.length();
    j["stats"]["character_bytes"] = char_bytes;
    
    // 重新计算总字节数 = 消息总字节 + 人设字节
    int msg_bytes = j["stats"].value("total_bytes", 0);
    j["stats"]["total_bytes"] = msg_bytes + char_bytes;
    
    std::ofstream outFile(filename);
    if (outFile.is_open()) {
        outFile << j.dump(4);
        outFile.close();
        std::cout << "💾 保存用户 " << user_id_ << " 的人设: " << character << std::endl;
    }
}

std::string StorageManager::loadCharacter() {
    std::string filename = getFilePath();
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        return "";
    }
    
    json j;
    file >> j;
    file.close();
    
    return j.value("character", "");
}

void StorageManager::updateStatsAfterAppend(const std::string& role, const std::string& content) {
    std::string filename = getFilePath();
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        return;
    }
    
    json j;
    file >> j;
    file.close();
    
    // 确保 stats 对象存在
    if (!j.contains("stats")) {
        j["stats"] = json::object();
        j["stats"]["user_messages"] = 0;
        j["stats"]["ai_messages"] = 0;
        j["stats"]["total_bytes"] = 0;
        j["stats"]["character_bytes"] = 0;
    }
    
    // 更新消息计数
    if (role == "user") {
        j["stats"]["user_messages"] = j["stats"].value("user_messages", 0) + 1;
    } else if (role == "assistant") {
        j["stats"]["ai_messages"] = j["stats"].value("ai_messages", 0) + 1;
    }
    
    // 更新消息字节数
    int current_msg_bytes = j["stats"].value("total_bytes", 0);
    int new_msg_bytes = content.length();
    j["stats"]["total_bytes"] = current_msg_bytes + new_msg_bytes;
    
    // 加上人设的字节数
    int char_bytes = j["stats"].value("character_bytes", 0);
    j["stats"]["total_bytes"] = j["stats"]["total_bytes"].get<int>() + char_bytes;
    
    // 更新总消息数
    j["total_messages"] = j["stats"]["user_messages"].get<int>() + j["stats"]["ai_messages"].get<int>();
    j["last_update"] = time(nullptr);
    
    std::ofstream outFile(filename);
    if (outFile.is_open()) {
        outFile << j.dump(4);
        outFile.close();
    }
}

void StorageManager::updateStatsAfterSaveCharacter(const std::string& character) {
    // 在 saveCharacter 中已经处理，这里直接调用
    saveCharacter(character);
}

int StorageManager::getTotalUserMessages() const {
    std::string filename = getFilePath();
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        return 0;
    }
    
    json j;
    file >> j;
    file.close();
    
    return j["stats"].value("user_messages", 0);
}

int StorageManager::getTotalAIMessages() const {
    std::string filename = getFilePath();
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        return 0;
    }
    
    json j;
    file >> j;
    file.close();
    
    return j["stats"].value("ai_messages", 0);
}

int StorageManager::getTotalCharacters() const {
    std::string filename = getFilePath();
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        return 0;
    }
    
    json j;
    file >> j;
    file.close();
    
    return j["stats"].value("character_bytes", 0);
}

int StorageManager::getTotalBytes() const {
    std::string filename = getFilePath();
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        return 0;
    }
    
    json j;
    file >> j;
    file.close();
    
    return j["stats"].value("total_bytes", 0);
}