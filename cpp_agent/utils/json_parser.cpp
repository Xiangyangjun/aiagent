#include "json_parser.h"
#include <algorithm>
#include <cctype>
#include <cstdio>

namespace utils {

std::string JsonParser::escapeJsonString(const std::string& str) {
    std::string escaped;
    escaped.reserve(str.length() * 2); // 预分配空间
    
    for (size_t i = 0; i < str.length(); ++i) {
        unsigned char c = static_cast<unsigned char>(str[i]);
        
        // 控制字符转义
        if (c < 0x20) {
            switch (c) {
                case '\n': escaped += "\\n"; break;
                case '\r': escaped += "\\r"; break;
                case '\t': escaped += "\\t"; break;
                case '\b': escaped += "\\b"; break;
                case '\f': escaped += "\\f"; break;
                default:
                    // 其他控制字符使用Unicode转义
                    {
                        char hex[7];
                        snprintf(hex, sizeof(hex), "\\u%04x", c);
                        escaped += hex;
                    }
                    break;
            }
        } else {
            switch (c) {
                case '"': escaped += "\\\""; break;
                case '\\': escaped += "\\\\"; break;
                case '/': escaped += "\\/"; break; // 可选，但更安全
                default:
                    // UTF-8字符直接添加（包括中文字符）
                    escaped += c;
                    break;
            }
        }
    }
    
    return escaped;
}

std::string JsonParser::unescapeJsonString(const std::string& str) {
    std::string unescaped;
    unescaped.reserve(str.length());
    
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '\\' && i + 1 < str.length()) {
            switch (str[i + 1]) {
                case 'n': unescaped += '\n'; i++; break;
                case 'r': unescaped += '\r'; i++; break;
                case 't': unescaped += '\t'; i++; break;
                case 'b': unescaped += '\b'; i++; break;
                case 'f': unescaped += '\f'; i++; break;
                case '"': unescaped += '"'; i++; break;
                case '\\': unescaped += '\\'; i++; break;
                case '/': unescaped += '/'; i++; break;
                case 'u':
                    // Unicode转义（简化处理）
                    if (i + 5 < str.length()) {
                        unescaped += '?'; // 占位符
                        i += 5;
                    } else {
                        unescaped += str[i];
                    }
                    break;
                default: unescaped += str[i]; break;
            }
        } else {
            unescaped += str[i];
        }
    }
    
    return unescaped;
}

std::string JsonParser::extractStringValue(const std::string& json, size_t key_start) {
    // 找到冒号
    size_t colon_pos = json.find(':', key_start);
    if (colon_pos == std::string::npos) {
        return "";
    }
    
    // 跳过空白字符
    size_t value_start = colon_pos + 1;
    while (value_start < json.length() && 
           (json[value_start] == ' ' || 
            json[value_start] == '\t' ||
            json[value_start] == '\n' ||
            json[value_start] == '\r')) {
        value_start++;
    }
    
    // 检查是否是字符串值（以引号开始）
    if (value_start >= json.length() || json[value_start] != '"') {
        return "";
    }
    
    value_start++; // 跳过开始引号
    size_t value_end = value_start;
    bool escaped = false;
    
    // 查找结束引号（考虑转义）
    while (value_end < json.length()) {
        if (escaped) {
            escaped = false;
            value_end++;
            continue;
        }
        if (json[value_end] == '\\') {
            escaped = true;
            value_end++;
            continue;
        }
        if (json[value_end] == '"') {
            break; // 找到结束引号
        }
        value_end++;
    }
    
    if (value_end > value_start) {
        std::string value = json.substr(value_start, value_end - value_start);
        return unescapeJsonString(value);
    }
    
    return "";
}

std::string JsonParser::extractString(const std::string& json, 
                                      const std::string& key,
                                      const std::string& default_value) {
    std::string search_key = "\"" + key + "\"";
    size_t key_pos = json.find(search_key);
    
    if (key_pos == std::string::npos) {
        return default_value;
    }
    
    std::string value = extractStringValue(json, key_pos);
    return value.empty() ? default_value : value;
}

std::string JsonParser::extractContentFromNestedJson(const std::string& json) {
    // 查找 "message" -> "content" 路径
    size_t message_pos = json.find("\"message\"");
    if (message_pos == std::string::npos) {
        return "";
    }
    
    size_t content_pos = json.find("\"content\"", message_pos);
    if (content_pos == std::string::npos) {
        return "";
    }
    
    return extractStringValue(json, content_pos);
}

} // namespace utils
