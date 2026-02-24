#include "tts.h"
#include "../utils/config.h"
#include "../utils/logger.h"
#include "../utils/json_parser.h"
#include <curl/curl.h>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <regex>

namespace tts {

struct WriteData {
    std::string data;
};

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    WriteData* wd = static_cast<WriteData*>(userp);
    wd->data.append(static_cast<char*>(contents), realsize);
    return realsize;
}

std::string generateSpeech(const std::string& text) {
    if (text.empty()) {
        throw std::runtime_error("文本内容为空");
    }
    
    auto& config = utils::Config::getInstance();
    std::string api_key = config.getString("aliyun_tts_key", "sk-21c5679fdf204dc9928a322e2738a75f");
    if (api_key.empty()) {
        throw std::runtime_error("aliyun_tts_key未配置");
    }
    
    std::string api_url = "https://dashscope.aliyuncs.com/api/v1/services/aigc/multimodal-generation/generation";

    std::string json_escaped_text = utils::JsonParser::escapeJsonString(text);
    
    std::ostringstream json_body;
    json_body << "{"
              << "\"model\":\"qwen3-tts-flash\","
              << "\"input\":{"
              << "\"text\":\"" << json_escaped_text << "\","
              << "\"voice\":\"Cherry\","
              << "\"language_type\":\"Chinese\""
              << "},"
              << "\"output\":{"
              << "\"format\":\"wav\","
              << "\"type\":\"audio\""
              << "}"
              << "}";

    // 重要：json_body.str() 是临时对象，必须先落到string里再把c_str()给curl
    std::string request_body = json_body.str();
    LOG_DEBUG("TTS", "请求体: " + request_body.substr(0, 500));
    
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("创建请求失败");
    }
    
    WriteData response_data;
    
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
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    
    long response_code = 0;
    CURLcode res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        LOG_ERROR("TTS", "调用TTS接口失败: " + std::string(curl_easy_strerror(res)));
        throw std::runtime_error("调用TTS接口失败");
    }
    
    if (response_code != 200) {
        LOG_ERROR("TTS", "TTS接口返回错误，状态码: " + std::to_string(response_code));
        LOG_ERROR("TTS", "响应内容: " + response_data.data.substr(0, 800));
        throw std::runtime_error("TTS接口返回错误，状态码: " + std::to_string(response_code) +
                                 "，响应内容: " + response_data.data.substr(0, 300));
    }
    
    // 解析响应，提取URL字段
    std::regex url_regex(R"xxx("url"\s*:\s*"([^"]*)")xxx");
    std::smatch match;
    if (std::regex_search(response_data.data, match, url_regex)) {
        std::string audio_url = utils::JsonParser::unescapeJsonString(match[1].str());
        if (audio_url.empty()) {
            throw std::runtime_error("TTS接口未返回音频URL");
        }
        return audio_url;
    }
    
    throw std::runtime_error("响应格式错误，无法提取音频URL");
}

} // namespace tts
