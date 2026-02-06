#include "config.h"
#include "logger.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <regex>

namespace utils {

Config::Config() : config_filepath_("config.json") {
}

Config& Config::getInstance() {
    static Config instance;
    return instance;
}

std::string Config::trim(const std::string& str) const {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) {
        return "";
    }
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, last - first + 1);
}

std::string Config::unescapeJsonString(const std::string& str) const {
    std::string result;
    result.reserve(str.length());
    
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '\\' && i + 1 < str.length()) {
            switch (str[i + 1]) {
                case '"': result += '"'; ++i; break;
                case '\\': result += '\\'; ++i; break;
                case '/': result += '/'; ++i; break;
                case 'b': result += '\b'; ++i; break;
                case 'f': result += '\f'; ++i; break;
                case 'n': result += '\n'; ++i; break;
                case 'r': result += '\r'; ++i; break;
                case 't': result += '\t'; ++i; break;
                default: result += str[i]; break;
            }
        } else {
            result += str[i];
        }
    }
    
    return result;
}

void Config::parseSimpleJSON(const std::string& content) {
    config_map_.clear();
    
    // 移除所有空白字符（简化处理）
    std::string cleaned;
    for (char c : content) {
        if (!std::isspace(c) || c == ' ') {
            cleaned += c;
        }
    }
    
    // 移除外层大括号
    size_t start = cleaned.find('{');
    if (start != std::string::npos) {
        cleaned = cleaned.substr(start + 1);
    }
    size_t end = cleaned.rfind('}');
    if (end != std::string::npos) {
        cleaned = cleaned.substr(0, end);
    }
    
    // 解析键值对，支持嵌套对象（简化版）
    // 格式: "key": "value" 或 "key": { "nested": "value" }
    std::regex pair_regex(R"xxx("([^"]+)"\s*:\s*"([^"]*)")xxx");
    std::sregex_iterator iter(cleaned.begin(), cleaned.end(), pair_regex);
    std::sregex_iterator end_iter;
    
    for (; iter != end_iter; ++iter) {
        std::smatch match = *iter;
        std::string key = match[1].str();
        std::string value = match[2].str();
        config_map_[key] = unescapeJsonString(value);
    }
    
    // 也支持数字和布尔值
    std::regex num_pair_regex(R"xxx("([^"]+)"\s*:\s*([0-9]+))xxx");
    iter = std::sregex_iterator(cleaned.begin(), cleaned.end(), num_pair_regex);
    for (; iter != end_iter; ++iter) {
        std::smatch match = *iter;
        std::string key = match[1].str();
        std::string value = match[2].str();
        config_map_[key] = value;
    }
    
    std::regex bool_pair_regex(R"xxx("([^"]+)"\s*:\s*(true|false))xxx");
    iter = std::sregex_iterator(cleaned.begin(), cleaned.end(), bool_pair_regex);
    for (; iter != end_iter; ++iter) {
        std::smatch match = *iter;
        std::string key = match[1].str();
        std::string value = match[2].str();
        config_map_[key] = value;
    }
}

int Config::loadFromFile(const std::string& filepath) {
    config_filepath_ = filepath;
    
    std::ifstream file(filepath);
    if (!file.is_open()) {
        // 注意：这里不能使用LOG_WARN，因为logger可能依赖config
        // 如果配置文件加载失败，使用默认配置继续运行
        return -1;
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();
    
    if (content.empty()) {
        return -1;
    }
    
    parseSimpleJSON(content);
    
    // 如果logger已经初始化，可以记录日志
    // 但这里不记录，因为logger.init()在main中调用，此时config已加载
    
    return 0;
}

std::string Config::getString(const std::string& key, const std::string& default_value) const {
    auto it = config_map_.find(key);
    if (it != config_map_.end()) {
        return it->second;
    }
    return default_value;
}

int Config::getInt(const std::string& key, int default_value) const {
    auto it = config_map_.find(key);
    if (it != config_map_.end()) {
        try {
            return std::stoi(it->second);
        } catch (...) {
            return default_value;
        }
    }
    return default_value;
}

bool Config::getBool(const std::string& key, bool default_value) const {
    auto it = config_map_.find(key);
    if (it != config_map_.end()) {
        std::string value = it->second;
        std::transform(value.begin(), value.end(), value.begin(), ::tolower);
        return (value == "true" || value == "1" || value == "yes");
    }
    return default_value;
}

bool Config::hasKey(const std::string& key) const {
    return config_map_.find(key) != config_map_.end();
}

void Config::setString(const std::string& key, const std::string& value) {
    config_map_[key] = value;
}

} // namespace utils
