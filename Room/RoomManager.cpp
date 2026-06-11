#include "RoomManager.h"
#include "Room.h"
#include "../concurrency/SubReactor.h"
#include "../MatchManager/MatchManager.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>
#include <memory>
#include <chrono>
#include <algorithm>   // for std::replace

// 前向声明 callDeepSeek（定义在 Room/deepseek_api.cpp 中）
std::string callDeepSeek(const nlohmann::json& messages);

RoomManager& RoomManager::instance() {
    static RoomManager mgr;
    return mgr;
}

void RoomManager::start() { worker_.start(); }
void RoomManager::stop() { worker_.stop(); }

void RoomManager::postRoomTask(const RoomTask& task) {
    worker_.post([this, task]() { handleRoomTask(task); });
}

std::vector<int> RoomManager::getRoomIds() const {
    std::vector<int> ids;
    for (auto& pair : rooms_) ids.push_back(pair.first);
    return ids;
}

static void sendMsgToClient(int fd, const std::string& msg) {
    SubReactor* reactor = MatchManager::instance().findReactorByFd(fd);
    if (reactor) reactor->postRawMessage(fd, msg);
}

void RoomManager::handleRoomTask(const RoomTask& task) {
    switch (task.type) {

    case RoomMsgType::CREATE: {
        int roomId = nextRoomId_++;
        std::vector<std::string> roleNames;
        std::vector<std::string> aiNames, aiPersonas;
        int playerCount = 2;          // 默认值
        std::string story;

        std::cout << "[RoomManager] CREATE raw content: " << task.content << std::endl;

        try {
            auto j = nlohmann::json::parse(task.content);
            std::cout << "[RoomManager] Parsed JSON:\n" << j.dump(4) << std::endl;

            // 兼容数组格式（旧版）
            if (j.is_array()) {
                for (auto& item : j) roleNames.push_back(item.get<std::string>());
            }
            // 主要处理对象格式
            if (j.is_object()) {
                // 提取人数
                if (j.contains("playerCount"))
                    playerCount = j["playerCount"].get<int>();
                // 提取故事
                if (j.contains("story"))
                    story = j["story"].get<std::string>();
                // 提取角色列表
                if (j.contains("roles") && j["roles"].is_array()) {
                    for (auto& item : j["roles"])
                        roleNames.push_back(item.get<std::string>());
                }
                // 提取 AI 配置
                if (j.contains("aiRoles")) {
                    std::cout << "[RoomManager] aiRoles field found" << std::endl;
                    if (j["aiRoles"].is_object()) {
                        for (auto it = j["aiRoles"].begin(); it != j["aiRoles"].end(); ++it) {
                            aiNames.push_back(it.key());
                            aiPersonas.push_back(it.value().get<std::string>());
                            std::cout << "  AI: " << it.key() << " -> " << it.value().get<std::string>() << std::endl;
                        }
                    } else {
                        std::cout << "[RoomManager] aiRoles is NOT an object!" << std::endl;
                    }
                } else {
                    std::cout << "[RoomManager] No aiRoles field in JSON" << std::endl;
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "[RoomManager] create JSON error: " << e.what() << std::endl;
            sendMsgToClient(task.fromFd, "RC_FAIL\n");
            return;
        }

        std::cout << "[RoomManager] roleNames: ";
        for (auto& n : roleNames) std::cout << n << " ";
        std::cout << "| playerCount=" << playerCount << " story=" << story << std::endl;

        if (roleNames.empty()) {
            sendMsgToClient(task.fromFd, "RC_FAIL\n");
            return;
        }

        auto room = std::unique_ptr<Room>(new Room(roomId, task.fromFd, task.userName,
                                                   playerCount, story, roleNames));

        // 设置 AI 角色
        if (!aiNames.empty()) {
            room->setAIRoles(aiNames, aiPersonas);
            std::cout << "[RoomManager] AI roles set successfully: " << aiNames.size() << " roles" << std::endl;
        } else {
            std::cout << "[RoomManager] No AI roles configured for this room" << std::endl;
        }

        rooms_[roomId] = std::move(room);
        sendMsgToClient(task.fromFd, "RC" + std::to_string(roomId) + "\n");
        std::cout << "[RoomManager] Room " << roomId << " created by " << task.userName << std::endl;
        break;
    }

    case RoomMsgType::JOIN: {
        auto it = rooms_.find(task.roomId);
        if (it == rooms_.end()) {
            sendMsgToClient(task.fromFd, "RJ_ERR\n");
            return;
        }
        Room* room = it->second.get();

        // 如果是已成员，直接返回房间信息（允许重复进入）
        if (room->isMember(task.fromFd)) {
            int myRole = room->getMemberRoleIndex(task.fromFd);
            nlohmann::json info;
            info["story"] = room->getStory();
            info["myRoleIndex"] = myRole;
            info["total"] = room->getMaxPlayers();
            info["current"] = room->getCurrentPlayers();
            if (myRole >= 0 && myRole < (int)room->getRoleNames().size())
                info["myRole"] = room->getRoleNames()[myRole];
            else
                info["myRole"] = "未分配";
            info["roles"] = nlohmann::json::array();
            for (auto& st : room->getRoleStatuses()) {
                nlohmann::json r;
                r["name"] = st.roleName;
                r["taken"] = st.taken;
                r["isMe"] = (st.takenByFd == task.fromFd);
                info["roles"].push_back(r);
            }
            std::string data = "RJ" + std::to_string(task.roomId) + info.dump() + "\n";
            sendMsgToClient(task.fromFd, data);
            std::cout << "[RoomManager] " << task.userName << " re-joined room " << task.roomId << std::endl;
            return;
        }

        // 新成员加入
        int roleIdx = -1;
        auto statuses = room->getRoleStatuses();
        for (size_t i = 0; i < statuses.size(); ++i)
            if (!statuses[i].taken) { roleIdx = i; break; }
        if (roleIdx == -1 || !room->addMember(task.fromFd, task.userName, roleIdx)) {
            sendMsgToClient(task.fromFd, "RJ_FULL\n");
            return;
        }

        std::string joinMsg = "R+" + task.userName + "\n";
        for (int fd : room->getMemberFds())
            sendMsgToClient(fd, joinMsg);

        nlohmann::json info;
        info["story"] = room->getStory();
        info["myRoleIndex"] = roleIdx;
        info["total"] = room->getMaxPlayers();
        info["current"] = room->getCurrentPlayers();
        if (roleIdx >= 0 && roleIdx < (int)room->getRoleNames().size())
            info["myRole"] = room->getRoleNames()[roleIdx];
        else
            info["myRole"] = "未分配";
        info["roles"] = nlohmann::json::array();
        for (auto& st : room->getRoleStatuses()) {
            nlohmann::json r;
            r["name"] = st.roleName;
            r["taken"] = st.taken;
            r["isMe"] = (st.takenByFd == task.fromFd);
            info["roles"].push_back(r);
        }
        std::string data = "RJ" + std::to_string(task.roomId) + info.dump() + "\n";
        sendMsgToClient(task.fromFd, data);
        std::cout << "[RoomManager] " << task.userName << " joined room " << task.roomId << std::endl;
        break;
    }

    case RoomMsgType::LEAVE:
        break;

    case RoomMsgType::CHAT: {
        Room* room = nullptr;
        for (auto& p : rooms_) {
            if (p.second->isMember(task.fromFd)) { room = p.second.get(); break; }
        }
        if (!room) return;

        if (!room->canSpeak(task.fromFd)) {
            sendMsgToClient(task.fromFd, "RS系统:发言太快，请5秒后再试\n");
            return;
        }
        room->recordSpeakTime(task.fromFd);

        room->addHistory(task.userName, task.content);
        std::string msg = "RS" + task.userName + ":" + task.content + "\n";
        for (int fd : room->getMemberFds()) sendMsgToClient(fd, msg);

        bool triggerAI = false;

        std::cout << "[RoomManager] CHAT trigger check: hasAI=" << room->hasAIPlayer()
                  << " mentions=" << room->mentionsAIRole(task.content)
                  << " allSpoke=" << room->allRealPlayersSpoke()
                  << " cooldown=" << room->aiCooldownExpired() << std::endl;

        if (room->mentionsAIRole(task.content)) {
            triggerAI = true;
        } else {
            room->recordSpoken(task.fromFd);
            if (room->allRealPlayersSpoke()) {
                triggerAI = true;
                room->resetSpokenRound();
            }
        }
        if (room->aiCooldownExpired()) {
            triggerAI = true;
        }

        if (triggerAI && room->hasAIPlayer()) {
            std::cout << "[RoomManager] Triggering AI reply..." << std::endl;
            room->updateAiTriggerTime();
            room->resetSpokenRound();

            nlohmann::json messages = nlohmann::json::array();
            for (size_t i = 0; i < room->getAIRoleNames().size(); ++i) {
                messages.push_back({
                    {"role", "system"},
                    {"content", room->getAIPersonas()[i]}
                });
            }
            auto& history = room->getHistory();
            int start = std::max(0, (int)history.size() - 30);
            for (int i = start; i < (int)history.size(); ++i) {
                messages.push_back({
                    {"role", "user"},
                    {"content", history[i].first + ": " + history[i].second}
                });
            }

            std::cout << "[RoomManager] Calling DeepSeek with " << messages.size() << " messages" << std::endl;
            std::string aiReply = callDeepSeek(messages);
            std::cout << "[RoomManager] AI reply received: " << aiReply << std::endl;

            // ✅ 关键：移除 AI 回复中的换行符，防止客户端分割消息
            std::replace(aiReply.begin(), aiReply.end(), '\n', ' ');
            std::replace(aiReply.begin(), aiReply.end(), '\r', ' ');

            std::string aiName = room->getAIRoleNames()[0];
            room->addHistory(aiName, aiReply);
            std::string aiMsg = "RS" + aiName + ":" + aiReply + "\n";
            for (int fd : room->getMemberFds()) sendMsgToClient(fd, aiMsg);
        } else {
            std::cout << "[RoomManager] AI not triggered (triggerAI=" << triggerAI 
                      << ", hasAI=" << room->hasAIPlayer() << ")" << std::endl;
        }
        break;
    }

    case RoomMsgType::LIST: {
        nlohmann::json arr = nlohmann::json::array();
        for (auto& p : rooms_) {
            nlohmann::json r;
            r["roomId"] = p.first;
            r["story"] = p.second->getStory();
            r["current"] = p.second->getCurrentPlayers();
            r["max"] = p.second->getMaxPlayers();
            arr.push_back(r);
        }
        std::string listStr = "RL" + arr.dump() + "\n";
        sendMsgToClient(task.fromFd, listStr);
        break;
    }

    default: break;
    }
}