#pragma once

#include <mysql/mysql.h>
#include <string>
#include <memory>
#include <functional>
#include <vector>
#include <functional>
class DBWorker {
public:
    // 构造函数：立即连接数据库
    DBWorker(const std::string& host, const std::string& user,
             const std::string& password, const std::string& database,
             unsigned int port = 3306);
    ~DBWorker();

    // 禁止拷贝和赋值
    DBWorker(const DBWorker&) = delete;
    DBWorker& operator=(const DBWorker&) = delete;

    // 检查连接是否有效，若断开则自动重连
    bool isConnected();

    // ---------- 业务接口 ----------
    // 登录验证：成功返回 true，并可通过 outUserId 返回用户 id（如果需要）
    bool verifyLogin(const std::string& username, const std::string& password,
                     int* outUserId = nullptr);
      bool executePreparedQuery(const std::string& sql,
                              std::vector<MYSQL_BIND>& params,
                              std::function<bool(MYSQL_STMT*)> processResult);
    // 可选：注册新用户（示例）
    bool registerUser(const std::string& username, const std::string& passwordHash);

    // 获取用户信息等...

private:
    MYSQL* conn_;
    std::string host_, user_, password_, database_;
    unsigned int port_;
};
