#include "DBWorker.h"
#include <iostream>
#include <cstring>
#include <vector>

DBWorker::DBWorker(const std::string& host, const std::string& user,
                   const std::string& password, const std::string& database,
                   unsigned int port)
    : host_(host), user_(user), password_(password), database_(database), port_(port) {
    conn_ = mysql_init(nullptr);
    if (!conn_) {
        throw std::runtime_error("mysql_init failed");
    }

    bool reconnect = 1;
    mysql_options(conn_, MYSQL_OPT_RECONNECT, &reconnect);

    if (!mysql_real_connect(conn_, host_.c_str(), user_.c_str(), password_.c_str(),
                            database_.c_str(), port_, nullptr, 0)) {
        std::string errMsg = "mysql_real_connect failed: " + std::string(mysql_error(conn_));
        mysql_close(conn_);
        throw std::runtime_error(errMsg);
    }

    mysql_set_character_set(conn_, "utf8mb4");
}

DBWorker::~DBWorker() {
    if (conn_) {
        mysql_close(conn_);
        conn_ = nullptr;
    }
}

bool DBWorker::isConnected() {
    if (!conn_) return false;
    return mysql_ping(conn_) == 0;
}

// 通用预编译查询执行器（只有一个）
bool DBWorker::executePreparedQuery(const std::string& sql,
                                     std::vector<MYSQL_BIND>& params,
                                    std::function<bool(MYSQL_STMT*)> processResult) {
    if (!isConnected()) {
        std::cerr << "Database connection lost" << std::endl;
        return false;
    }

    MYSQL_STMT* stmt = mysql_stmt_init(conn_);
    if (!stmt) {
        std::cerr << "mysql_stmt_init failed" << std::endl;
        return false;
    }

    if (mysql_stmt_prepare(stmt, sql.c_str(), sql.length())) {
        std::cerr << "mysql_stmt_prepare failed: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return false;
    }

    if (!params.empty()) {
        if (mysql_stmt_bind_param(stmt, params.data())) {
            std::cerr << "mysql_stmt_bind_param failed: " << mysql_stmt_error(stmt) << std::endl;
            mysql_stmt_close(stmt);
            return false;
        }
    }

    if (mysql_stmt_execute(stmt)) {
        std::cerr << "mysql_stmt_execute failed: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 必须存储结果集，否则 SELECT 无法 fetch
    if (mysql_stmt_store_result(stmt) != 0) {
        std::cerr << "mysql_stmt_store_result failed: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return false;
    }

    bool result = true;
    if (processResult) {
        result = processResult(stmt);
    }

    mysql_stmt_close(stmt);
    return result;
}

// 登录验证
bool DBWorker::verifyLogin(const std::string& username, const std::string& password,
                           int* outUserId) {
    const std::string sql = "SELECT id, password_hash FROM users WHERE username = ?";

    MYSQL_BIND bind[1];
    memset(bind, 0, sizeof(bind));
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (void*)username.c_str();
    bind[0].buffer_length = username.length();

    std::vector<MYSQL_BIND> params(bind, bind + 1);

    int userId = -1;
    std::string storedHash;
    bool found = false;

    auto process = [&](MYSQL_STMT* stmt) -> bool {
        MYSQL_BIND resultBind[2];
        memset(resultBind, 0, sizeof(resultBind));

        unsigned long idLen;
        resultBind[0].buffer_type = MYSQL_TYPE_LONG;
        resultBind[0].buffer = &userId;
        resultBind[0].buffer_length = sizeof(userId);
        resultBind[0].length = &idLen;
        resultBind[0].is_null = nullptr;

        char hashBuffer[256];
        unsigned long hashLen;
        resultBind[1].buffer_type = MYSQL_TYPE_STRING;
        resultBind[1].buffer = hashBuffer;
        resultBind[1].buffer_length = sizeof(hashBuffer);
        resultBind[1].length = &hashLen;
        resultBind[1].is_null = nullptr;

        if (mysql_stmt_bind_result(stmt, resultBind)) {
            std::cerr << "mysql_stmt_bind_result failed: " << mysql_stmt_error(stmt) << std::endl;
            return false;
        }

        int ret = mysql_stmt_fetch(stmt);
        if (ret == 0) {
            found = true;
            storedHash.assign(hashBuffer, hashLen);
        } else if (ret != MYSQL_NO_DATA) {
            std::cerr << "mysql_stmt_fetch error: " << mysql_stmt_error(stmt) << std::endl;
        }
        return true;
    };

    if (!executePreparedQuery(sql, params, process)) {
        return false;
    }

    if (!found) {
        return false;
    }

    // 当前明文比较，后续替换为 bcrypt 等
    if (password == storedHash) {
        if (outUserId) *outUserId = userId;
        return true;
    }
    return false;
}

// 注册新用户
bool DBWorker::registerUser(const std::string& username, const std::string& passwordHash) {
    const std::string sql = "INSERT INTO users (username, password_hash) VALUES (?, ?)";
    MYSQL_BIND bind[2];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (void*)username.c_str();
    bind[0].buffer_length = username.length();

    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (void*)passwordHash.c_str();
    bind[1].buffer_length = passwordHash.length();

    std::vector<MYSQL_BIND> params(bind, bind + 2);

    auto processEmpty = [](MYSQL_STMT*) -> bool { return true; };
    return executePreparedQuery(sql, params, processEmpty);
}