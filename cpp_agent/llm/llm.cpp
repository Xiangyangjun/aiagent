#include "llm.h"
#include "../memory/short_term.h"
#include "../memory/long_term.h"
#include "../utils/logger.h"
#include "../utils/config.h"
#include <curl/curl.h>
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <regex>

namespace llm {

struct WriteData {
    std::string data;
};

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    WriteData* wd = static_cast<WriteData*>(userp);
    wd->data.append(static_cast<char*>(contents), realsize);
    return realsize;
}

std::string extractHabitKeywords(const std::string& user_id) {
    auto& short_mem = memory::ShortTermMemory::getInstance();
    std::string short_context = short_mem.getShortTermContext(user_id);
    
    std::ostringstream prompt;
    prompt << "\n请基于用户最近10轮对话上下文，提取其中明确提及的「习惯/爱好」类核心关键词，要求：\n"
           << "1. 仅返回中文关键词，用逗号分隔，无任何解释、说明或多余文字；\n"
           << "2. 关键词简洁（如：钓鱼、看电影、户外、跑步），不重复；\n"
           << "3. 只提取用户明确提及的内容，不猜测、不编造、不扩展；\n"
           << "4. 无相关习惯/爱好则返回\"无\"。\n\n"
           << "用户近10轮对话上下文：" << short_context << "\n";
    
    auto& config = utils::Config::getInstance();
    std::string api_key = config.getString("dashscope_api_key", "");
    if (api_key.empty()) {
        LOG_WARN("LLM", "dashscope_api_key未配置，无法提取关键词");
        return "无";
    }
    
    std::string api_url = "https://dashscope.aliyuncs.com/api/v1/services/aigc/text-generation/generation";
    
    // 转义JSON字符串（更严格的转义）
    std::string prompt_str = prompt.str();
    std::string json_escaped_prompt;
    json_escaped_prompt.reserve(prompt_str.length() * 2);
    
    for (size_t i = 0; i < prompt_str.length(); ++i) {
        unsigned char c = static_cast<unsigned char>(prompt_str[i]);
        
        if (c < 0x20) {
            switch (c) {
                case '\n': json_escaped_prompt += "\\n"; break;
                case '\r': json_escaped_prompt += "\\r"; break;
                case '\t': json_escaped_prompt += "\\t"; break;
                case '\b': json_escaped_prompt += "\\b"; break;
                case '\f': json_escaped_prompt += "\\f"; break;
                default:
                    // 其他控制字符使用Unicode转义
                    {
                        char hex[5];
                        snprintf(hex, sizeof(hex), "\\u%04x", c);
                        json_escaped_prompt += hex;
                    }
                    break;
            }
        } else {
            switch (c) {
                case '"': json_escaped_prompt += "\\\""; break;
                case '\\': json_escaped_prompt += "\\\\"; break;
                case '/': json_escaped_prompt += "\\/"; break;
                default:
                    json_escaped_prompt += c;
                    break;
            }
        }
    }
    
    std::ostringstream json_body;
    json_body << "{"
              << "\"model\":\"qwen-turbo\","
              << "\"input\":{"
              << "\"messages\":[{"
              << "\"role\":\"user\","
              << "\"content\":\"" << json_escaped_prompt << "\""
              << "}]"
              << "},"
              << "\"parameters\":{"
              << "\"temperature\":0.1,"
              << "\"result_format\":\"message\","
              << "\"max_tokens\":100"
              << "}"
              << "}";
    
    std::string request_body = json_body.str();
    LOG_DEBUG("LLM", "关键词提取请求体: " + request_body.substr(0, 300));
    
    CURL* curl = curl_easy_init();
    if (!curl) {
        return "无";
    }
    
    WriteData response_data;
    
    // 设置请求头（必须在设置POSTFIELDS之前）
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    std::string auth_header = "Authorization: Bearer " + api_key;
    headers = curl_slist_append(headers, auth_header.c_str());
    
    curl_easy_setopt(curl, CURLOPT_URL, api_url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(request_body.length()));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
    
    long response_code = 0;
    CURLcode res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        LOG_ERROR("LLM", "调用关键词提取API失败: " + std::string(curl_easy_strerror(res)));
        return "无";
    }
    
    if (response_code != 200) {
        LOG_WARN("LLM", "关键词提取API返回错误状态码: " + std::to_string(response_code));
        return "无";
    }
    
    // 改进的JSON解析：支持转义字符和嵌套结构
    // 查找 message.content 路径
    std::string keywords;
    size_t message_pos = response_data.data.find("\"message\"");
    if (message_pos != std::string::npos) {
        size_t content_pos = response_data.data.find("\"content\"", message_pos);
        if (content_pos != std::string::npos) {
            size_t colon_pos = response_data.data.find(':', content_pos);
            if (colon_pos != std::string::npos) {
                size_t value_start = colon_pos + 1;
                while (value_start < response_data.data.length() && 
                       (response_data.data[value_start] == ' ' || 
                        response_data.data[value_start] == '\t' ||
                        response_data.data[value_start] == '\n' ||
                        response_data.data[value_start] == '\r')) {
                    value_start++;
                }
                
                if (value_start < response_data.data.length() && response_data.data[value_start] == '"') {
                    value_start++;
                    size_t value_end = value_start;
                    bool escaped = false;
                    
                    while (value_end < response_data.data.length()) {
                        if (escaped) {
                            escaped = false;
                            value_end++;
                            continue;
                        }
                        if (response_data.data[value_end] == '\\') {
                            escaped = true;
                            value_end++;
                            continue;
                        }
                        if (response_data.data[value_end] == '"') {
                            break;
                        }
                        value_end++;
                    }
                    
                    if (value_end > value_start) {
                        keywords = response_data.data.substr(value_start, value_end - value_start);
                        
                        // 处理转义字符
                        std::string unescaped;
                        for (size_t i = 0; i < keywords.length(); ++i) {
                            if (keywords[i] == '\\' && i + 1 < keywords.length()) {
                                switch (keywords[i + 1]) {
                                    case 'n': unescaped += '\n'; i++; break;
                                    case 'r': unescaped += '\r'; i++; break;
                                    case 't': unescaped += '\t'; i++; break;
                                    case '"': unescaped += '"'; i++; break;
                                    case '\\': unescaped += '\\'; i++; break;
                                    default: unescaped += keywords[i]; break;
                                }
                            } else {
                                unescaped += keywords[i];
                            }
                        }
                        keywords = unescaped;
                    }
                }
            }
        }
    }
    
    // 如果上面的方法失败，尝试简单的正则表达式
    if (keywords.empty()) {
        std::regex content_regex(R"xxx("content"\s*:\s*"([^"]*)")xxx");
        std::smatch match;
        if (std::regex_search(response_data.data, match, content_regex)) {
            keywords = match[1].str();
        }
    }
    
    if (!keywords.empty()) {
        // 去除首尾空格
        keywords.erase(0, keywords.find_first_not_of(" \t\n\r"));
        keywords.erase(keywords.find_last_not_of(" \t\n\r") + 1);
        
        if (keywords.empty() || keywords == "无") {
            return "无";
        }
        return keywords;
    }
    
    LOG_DEBUG("LLM", "关键词提取API响应: " + response_data.data.substr(0, 300));
    return "无";
}

std::string callLLM(const std::string& /* session_id */,
                    const std::string& user_id,
                    const std::string& user_input) {
    auto& long_mem = memory::LongTermMemory::getInstance();
    auto& short_mem = memory::ShortTermMemory::getInstance();
    
    std::string long_keywords = long_mem.getLongTerm(user_id);
    std::string long_mem_str = "用户偏好关键词：" + long_keywords;
    if (long_keywords == "无") {
        long_mem_str = "用户暂无偏好信息";
    }
    
    std::string short_mem_str = short_mem.getShortTermContext(user_id);
    
    std::ostringstream prompt;
    prompt << "\n你是一个生活化、有同理心的AI助手，核心目标是基于用户的全量对话信息和长期偏好，生成有温度、个性化的回复。\n"
           << "【参考信息】\n"
           << "1. 历史会话上下文（最近10轮，按时间从旧到新排序）：" << short_mem_str << "\n"
           << "   - 规则：优先参考近3轮对话内容，确保回复承接上下文，不偏离用户对话逻辑\n"
           << "2. 用户的长期偏好/记忆（核心标签+偏好程度）：" << long_mem_str << "\n"
           << "   - 规则：仅作为个性化补充，不强行关联，避免偏离当前提问核心\n"
           << "3. 用户当前的提问/输入（含语气倾向）：" << user_input << "\n\n"
           << "【回复核心要求】\n"
           << "1. 语气风格：亲切自然，贴合用户当前输入的语气（用户轻松则活泼，用户提问则耐心，用户倾诉则共情）；\n"
           << "2. 内容要求：优先精准回应当前提问，再自然融入匹配的长期偏好（如用户喜欢钓鱼则可轻提相关）；\n"
           << "3. 表达规范：避免生硬机器感、套话和模板化回复，用词生活化；\n"
           << "4. 字数控制：整体回复控制在80-120字，逻辑清晰、语句通顺，无冗余信息；\n"
           << "5. 避坑点：不编造未提及的偏好，不忽视历史对话中的关键信息，不使用专业术语。\n";
    
    LOG_DEBUG("LLM", "Prompt: " + prompt.str());
    
    auto& config = utils::Config::getInstance();
    std::string api_key = config.getString("dashscope_api_key", "");
    if (api_key.empty()) {
        LOG_ERROR("LLM", "dashscope_api_key未配置");
        throw std::runtime_error("请先在config.json中配置dashscope_api_key");
    }
    
    std::string api_url = "https://dashscope.aliyuncs.com/api/v1/services/aigc/text-generation/generation";
    
    // 转义JSON字符串中的特殊字符（更严格的转义）
    std::string escaped_prompt = prompt.str();
    std::string json_escaped;
    json_escaped.reserve(escaped_prompt.length() * 2); // 预分配空间
    
    for (size_t i = 0; i < escaped_prompt.length(); ++i) {
        unsigned char c = static_cast<unsigned char>(escaped_prompt[i]);
        
        // 控制字符和特殊字符转义
        if (c < 0x20) {
            switch (c) {
                case '\n': json_escaped += "\\n"; break;
                case '\r': json_escaped += "\\r"; break;
                case '\t': json_escaped += "\\t"; break;
                case '\b': json_escaped += "\\b"; break;
                case '\f': json_escaped += "\\f"; break;
                default:
                    // 其他控制字符使用Unicode转义
                    {
                        char hex[5];
                        snprintf(hex, sizeof(hex), "\\u%04x", c);
                        json_escaped += hex;
                    }
                    break;
            }
        } else {
            switch (c) {
                case '"': json_escaped += "\\\""; break;
                case '\\': json_escaped += "\\\\"; break;
                case '/': json_escaped += "\\/"; break; // 可选，但更安全
                default:
                    // UTF-8字符直接添加（包括中文字符）
                    json_escaped += c;
                    break;
            }
        }
    }
    
    std::ostringstream json_body;
    json_body << "{"
              << "\"model\":\"qwen-turbo\","
              << "\"input\":{"
              << "\"messages\":[{"
              << "\"role\":\"user\","
              << "\"content\":\"" << json_escaped << "\""
              << "}]"
              << "},"
              << "\"parameters\":{"
              << "\"temperature\":0.5,"
              << "\"result_format\":\"message\""
              << "}"
              << "}";
    
    // 记录请求体（用于调试）
    std::string request_body = json_body.str();
    LOG_DEBUG("LLM", "请求体: " + request_body.substr(0, 500));
    
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("请求创建失败");
    }
    
    WriteData response_data;
    
    std::string request_body = json_body.str();
    LOG_DEBUG("LLM", "请求体: " + request_body.substr(0, 500));
    
    // 设置请求头（必须在设置POSTFIELDS之前）
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    std::string auth_header = "Authorization: Bearer " + api_key;
    headers = curl_slist_append(headers, auth_header.c_str());
    
    curl_easy_setopt(curl, CURLOPT_URL, api_url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(request_body.length()));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
    
    long response_code = 0;
    CURLcode res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        LOG_ERROR("LLM", "调用大模型API失败: " + std::string(curl_easy_strerror(res)));
        throw std::runtime_error("调用大模型失败");
    }
    
    if (response_code != 200) {
        LOG_ERROR("LLM", "API返回错误状态码: " + std::to_string(response_code));
        LOG_ERROR("LLM", "响应内容: " + response_data.data);
        throw std::runtime_error("API返回错误，状态码: " + std::to_string(response_code));
    }
    
    // 记录响应内容（用于调试）
    LOG_DEBUG("LLM", "API响应: " + response_data.data.substr(0, 500)); // 只记录前500字符
    
    // 解析JSON响应结构：output.choices[0].message.content
    // 响应格式：{"output":{"choices":[{"message":{"content":"..."}}]}}
    std::string reply;
    
    // 方法1：查找 "message" -> "content" 路径
    size_t message_pos = response_data.data.find("\"message\"");
    if (message_pos != std::string::npos) {
        size_t content_pos = response_data.data.find("\"content\"", message_pos);
        if (content_pos != std::string::npos) {
            // 找到content字段后的冒号
            size_t colon_pos = response_data.data.find(':', content_pos);
            if (colon_pos != std::string::npos) {
                // 跳过空白字符
                size_t value_start = colon_pos + 1;
                while (value_start < response_data.data.length() && 
                       (response_data.data[value_start] == ' ' || 
                        response_data.data[value_start] == '\t' ||
                        response_data.data[value_start] == '\n' ||
                        response_data.data[value_start] == '\r')) {
                    value_start++;
                }
                
                // 检查是否是字符串值（以引号开始）
                if (value_start < response_data.data.length() && response_data.data[value_start] == '"') {
                    value_start++; // 跳过开始引号
                    size_t value_end = value_start;
                    bool escaped = false;
                    
                    // 查找结束引号（考虑转义）
                    while (value_end < response_data.data.length()) {
                        if (escaped) {
                            escaped = false;
                            value_end++;
                            continue;
                        }
                        if (response_data.data[value_end] == '\\') {
                            escaped = true;
                            value_end++;
                            continue;
                        }
                        if (response_data.data[value_end] == '"') {
                            break; // 找到结束引号
                        }
                        value_end++;
                    }
                    
                    if (value_end > value_start) {
                        reply = response_data.data.substr(value_start, value_end - value_start);
                        
                        // 处理转义字符
                        std::string unescaped_reply;
                        for (size_t i = 0; i < reply.length(); ++i) {
                            if (reply[i] == '\\' && i + 1 < reply.length()) {
                                switch (reply[i + 1]) {
                                    case 'n': unescaped_reply += '\n'; i++; break;
                                    case 'r': unescaped_reply += '\r'; i++; break;
                                    case 't': unescaped_reply += '\t'; i++; break;
                                    case '"': unescaped_reply += '"'; i++; break;
                                    case '\\': unescaped_reply += '\\'; i++; break;
                                    case '/': unescaped_reply += '/'; i++; break;
                                    case 'u': 
                                        // Unicode转义（简化处理，跳过4个字符）
                                        if (i + 5 < reply.length()) {
                                            unescaped_reply += '?'; // 占位符
                                            i += 5;
                                        } else {
                                            unescaped_reply += reply[i];
                                        }
                                        break;
                                    default: unescaped_reply += reply[i]; break;
                                }
                            } else {
                                unescaped_reply += reply[i];
                            }
                        }
                        reply = unescaped_reply;
                    }
                }
            }
        }
    }
    
    // 如果上面的方法失败，尝试简单的正则表达式（向后兼容）
    if (reply.empty()) {
        // 尝试匹配 message.content 结构
        std::regex content_regex1(R"xxx("message"\s*:\s*\{[^}]*"content"\s*:\s*"([^"]*)")xxx");
        std::smatch match1;
        if (std::regex_search(response_data.data, match1, content_regex1)) {
            reply = match1[1].str();
        } else {
            // 尝试简单的content匹配
            std::regex content_regex2(R"xxx("content"\s*:\s*"([^"]*)")xxx");
            std::smatch match2;
            if (std::regex_search(response_data.data, match2, content_regex2)) {
                reply = match2[1].str();
            }
        }
    }
    
    if (!reply.empty()) {
        // 提取关键词并更新长期记忆
        std::string new_keywords = extractHabitKeywords(user_id);
        if (new_keywords != "无") {
            LOG_DEBUG("LLM", "提取到用户关键词: " + new_keywords + " (用户: " + user_id + ")");
        }
        long_mem.mergeAndSaveLongTerm(user_id, new_keywords);
        
        LOG_INFO("LLM", "成功生成回复 (用户: " + user_id + ", 长度: " + std::to_string(reply.length()) + ")");
        return reply;
    }
    
    LOG_ERROR("LLM", "响应格式错误，无法提取回复内容");
    LOG_ERROR("LLM", "完整响应: " + response_data.data);
    throw std::runtime_error("响应格式错误，无法提取回复内容。响应: " + response_data.data.substr(0, 500));
}

} // namespace llm
