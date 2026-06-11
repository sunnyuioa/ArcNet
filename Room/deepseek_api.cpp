#include <string>
#include <cstdlib>
#include <iostream>
#include <nlohmann/json.hpp>
#include <curl/curl.h>

// ========== API Key 配置 ==========
// 请替换为你自己有效的 Key，或设置环境变量 DEEPSEEK_API_KEY
const std::string DEFAULT_API_KEY = "sk-f994d17ef9c04479ac605b0f995db53c"; // 已失效，仅作占位

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    size_t total = size * nmemb;
    output->append((char*)contents, total);
    return total;
}

/**
 * 调用 DeepSeek API
 * @param messages  已经构造好的消息数组（nlohmann::json 类型）
 * @return          AI 回复的文本内容，失败返回错误提示
 */
std::string callDeepSeek(const nlohmann::json& messages) {
    // 优先使用环境变量，否则使用默认值（仅用于测试，不安全）
    const char* envKey = std::getenv("DEEPSEEK_API_KEY");
    std::string apiKey = envKey ? std::string(envKey) : DEFAULT_API_KEY;
    if (apiKey.empty()) {
        std::cerr << "[DeepSeek] API Key 为空" << std::endl;
        return "AI 配置错误：API Key 未设置";
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "[DeepSeek] curl 初始化失败" << std::endl;
        return "AI 服务初始化失败";
    }

    // 构造请求体
    nlohmann::json requestBody;
    requestBody["model"] = "deepseek-v4-pro";
    requestBody["messages"] = messages;
    requestBody["stream"] = false;
    requestBody["thinking"] = {{"type", "enabled"}};
    requestBody["reasoning_effort"] = "high";
    std::string postData = requestBody.dump();

    curl_easy_setopt(curl, CURLOPT_URL, "https://api.deepseek.com/chat/completions");

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    std::string auth = "Authorization: Bearer " + apiKey;
    headers = curl_slist_append(headers, auth.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());

    std::string response;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std::cerr << "[DeepSeek] 请求失败: " << curl_easy_strerror(res) << std::endl;
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return "AI 请求失败";
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    // 解析响应
    try {
        auto jsonResp = nlohmann::json::parse(response);
        if (jsonResp.contains("choices") && !jsonResp["choices"].empty()) {
            std::string content = jsonResp["choices"][0]["message"]["content"];
            return content;
        } else {
            std::cerr << "[DeepSeek] 响应格式异常: " << response << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[DeepSeek] JSON 解析失败: " << e.what() << std::endl;
    }

    return "AI 回复为空";
}