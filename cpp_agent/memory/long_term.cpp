#include "long_term.h"
#include "../utils/logger.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <regex>
#include <cstring>
#include <cctype>
#if __cplusplus >= 201703L && defined(__has_include)
  #if __has_include(<filesystem>)
    #include <filesystem>
    namespace fs = std::filesystem;
  #else
    #include <experimental/filesystem>
    namespace fs = std::experimental::filesystem;
  #endif
#else
  #include <experimental/filesystem>
  namespace fs = std::experimental::filesystem;
#endif

namespace memory {

LongTermMemory::LongTermMemory() 
    : initialized_(false), should_stop_(false) {
}

LongTermMemory::~LongTermMemory() {
    close();
}

LongTermMemory& LongTermMemory::getInstance() {
    static LongTermMemory instance;
    return instance;
}

int LongTermMemory::init() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_) {
        return 0;
    }
    
    // 创建数据目录
    fs::path file_path(persist_file);
    fs::create_directories(file_path.parent_path());
    
    // 加载历史数据
    if (loadFromFile() != 0) {
        LOG_WARN("LongTermMemory", "加载数据失败，将使用空存储");
        store_.clear();
    }
    
    // 启动异步写文件线程
    should_stop_ = false;
    write_thread_ = std::thread(&LongTermMemory::asyncWriteLoop, this);
    
    initialized_ = true;
    LOG_INFO("LongTermMemory", "长期记忆模块初始化完成");
    return 0;
}

int LongTermMemory::loadFromFile() {
    std::ifstream file(persist_file);
    if (!file.is_open()) {
        // 文件不存在，初始化空存储
        store_.clear();
        return saveToFile();
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();
    
    if (content.empty()) {
        store_.clear();
        return 0;
    }
    
    // 简单的JSON解析（仅处理简单的key-value map）
    // 格式: {"user_id": "keywords", ...}
    store_.clear();
    
    // 移除空格和换行
    content.erase(std::remove_if(content.begin(), content.end(),
        [](char c) { return std::isspace(c); }), content.end());
    
    // 移除外层大括号
    if (content.front() == '{') content.erase(0, 1);
    if (content.back() == '}') content.pop_back();
    
    // 解析键值对
    std::regex pair_regex(R"xxx("([^"]+)"\s*:\s*"([^"]*)")xxx");
    std::sregex_iterator iter(content.begin(), content.end(), pair_regex);
    std::sregex_iterator end;
    
    for (; iter != end; ++iter) {
        std::smatch match = *iter;
        store_[match[1].str()] = match[2].str();
    }
    
    return 0;
}

int LongTermMemory::saveToFile() {
    std::ofstream file(persist_file);
    if (!file.is_open()) {
        return -1;
    }
    
    file << "{\n";
    bool first = true;
    for (const auto& pair : store_) {
        if (!first) file << ",\n";
        file << "  \"" << pair.first << "\": \"" << pair.second << "\"";
        first = false;
    }
    file << "\n}\n";
    
    file.close();
    return 0;
}

std::vector<std::string> LongTermMemory::splitKeywords(const std::string& str) {
    if (str.empty() || str == "无") {
        return {};
    }
    
    std::string processed = str;
    // 替换中文标点为英文逗号
    // 注意：中文字符在UTF-8中是3字节，需要按字符串处理
    size_t pos = 0;
    const std::string comma_cn = "，";
    const std::string comma_jp = "、";
    while ((pos = processed.find(comma_cn, pos)) != std::string::npos) {
        processed.replace(pos, comma_cn.length(), ",");
        pos += 1;
    }
    pos = 0;
    while ((pos = processed.find(comma_jp, pos)) != std::string::npos) {
        processed.replace(pos, comma_jp.length(), ",");
        pos += 1;
    }
    
    std::vector<std::string> result;
    std::istringstream iss(processed);
    std::string token;
    
    while (std::getline(iss, token, ',')) {
        // 去除首尾空格
        token.erase(0, token.find_first_not_of(" \t"));
        token.erase(token.find_last_not_of(" \t") + 1);
        
        if (!token.empty()) {
            result.push_back(token);
        }
    }
    
    return result;
}

std::string LongTermMemory::mergeAndSaveLongTerm(const std::string& user_id, 
                                                  const std::string& new_keywords) {
    std::map<std::string, std::string> data_copy;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (new_keywords.empty() || new_keywords == "无") {
            auto it = store_.find(user_id);
            if (it == store_.end() || it->second.empty()) {
                return "无";
            }
            return it->second;
        }
        
        // 拆分新旧关键词
        std::string existing_str = store_[user_id];
        auto existing_keys = splitKeywords(existing_str);
        auto new_keys = splitKeywords(new_keywords);
        
        // 去重合并
        std::map<std::string, bool> merged_map;
        for (const auto& k : existing_keys) {
            merged_map[k] = true;
        }
        for (const auto& k : new_keys) {
            merged_map[k] = true;
        }
        
        // 转为向量，限制最多50个
        std::vector<std::string> merged_vec;
        for (const auto& pair : merged_map) {
            merged_vec.push_back(pair.first);
        }
        
        if (merged_vec.size() > max_long_keys) {
            merged_vec.erase(merged_vec.begin() + max_long_keys, merged_vec.end());
        }
        
        // 拼接并更新内存
        std::ostringstream oss;
        for (size_t i = 0; i < merged_vec.size(); ++i) {
            if (i > 0) oss << "，";
            oss << merged_vec[i];
        }
        store_[user_id] = oss.str();
        
        // 复制数据用于异步写入
        data_copy = store_;
    }
    
    // 异步写入
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        write_queue_.push(data_copy);
    }
    queue_cv_.notify_one();
    
    return data_copy[user_id];
}

std::string LongTermMemory::getLongTerm(const std::string& user_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = store_.find(user_id);
    if (it == store_.end() || it->second.empty()) {
        return "无";
    }
    return it->second;
}

void LongTermMemory::asyncWriteLoop() {
    while (!should_stop_) {
        std::map<std::string, std::string> data_to_write;
        
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait_for(lock, std::chrono::seconds(1), [this] {
                return !write_queue_.empty() || should_stop_;
            });
            
            if (should_stop_ && write_queue_.empty()) {
                break;
            }
            
            if (!write_queue_.empty()) {
                data_to_write = write_queue_.front();
                write_queue_.pop();
            }
        }
        
        if (!data_to_write.empty()) {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                store_ = data_to_write;
            }
            if (saveToFile() != 0) {
                LOG_WARN("LongTermMemory", "异步写文件失败");
            }
        }
    }
    
    // 写入最后一批数据
    std::lock_guard<std::mutex> lock(mutex_);
    saveToFile();
}

void LongTermMemory::close() {
    if (initialized_) {
        should_stop_ = true;
        queue_cv_.notify_all();
        if (write_thread_.joinable()) {
            write_thread_.join();
        }
        initialized_ = false;
    }
}

} // namespace memory
