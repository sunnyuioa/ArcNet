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

    // 设置自动重连（可选）
    bool reconnect = 1;
    mysql_options(conn_, MYSQL_OPT_RECONNECT, &reconnect);

    if (!mysql_real_connect(conn_, host_.c_str(), user_.c_str(), password_.c_str(),
                            database_.c_str(), port_, nullptr, 0)) {
        std::string errMsg = "mysql_real_connect failed: " + std::string(mysql_error(conn_));
        mysql_close(conn_);
        throw std::runtime_error(errMsg);
    }

    // 设置字符集为 UTF-8
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

// 通用预编译查询执行器
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

    // 绑定参数
    if (!params.empty()) {
        if (mysql_stmt_bind_param(stmt, params.data())) {
            std::cerr << "mysql_stmt_bind_param failed: " << mysql_stmt_error(stmt) << std::endl;
            mysql_stmt_close(stmt);
            return false;
        }
    }

    // 执行
    if (mysql_stmt_execute(stmt)) {
        std::cerr << "mysql_stmt_execute failed: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // ========== 改动：增加这一整块 ==========
    // 存储结果集，对于 SELECT 语句必须调用，否则后续 fetch 会失败；
    // 对于 INSERT/UPDATE 等不产生结果集的语句，调用也安全（返回0且字段数为0）
    if (mysql_stmt_store_result(stmt) != 0) {
        std::cerr << "mysql_stmt_store_result failed: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return false;
    }
    // ========== 改动结束 ==========

    // 处理结果（如果需要）
    bool result = true;
    if (processResult) {
        result = processResult(stmt);
    }

    mysql_stmt_close(stmt);
    return result;
}
// 登录验证实现
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

    // 绑定参数
    if (!params.empty()) {
        if (mysql_stmt_bind_param(stmt, params.data())) {
            std::cerr << "mysql_stmt_bind_param failed: " << mysql_stmt_error(stmt) << std::endl;
            mysql_stmt_close(stmt);
            return false;
        }
    }

    // 执行
    if (mysql_stmt_execute(stmt)) {
        std::cerr << "mysql_stmt_execute failed: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // ========== 改动 1：必须存储结果集（修复查询无结果的问题）==========
    if (mysql_stmt_store_result(stmt) != 0) {
        std::cerr << "mysql_stmt_store_result failed: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return false;
    }

    // 处理结果（如果需要）
    bool result = true;
    if (processResult) {
        result = processResult(stmt);
    }

    mysql_stmt_close(stmt);
    return result;
}
// 注册新用户示例（使用预编译，密码应存哈希）
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

    // 执行插入，不需要处理结果集
    auto processEmpty = [](MYSQL_STMT*) -> bool { return true; };
    return executePreparedQuery(sql, params, processEmpty);
}