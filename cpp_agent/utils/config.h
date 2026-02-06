#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <map>

namespace utils {

class Config {
public:
    static Config& getInstance();
    
    // 加载配置文件
    int loadFromFile(const std::string& filepath = "config.json");
    
    // 获取配置值
    std::string getString(const std::string& key, const std::string& default_value = "") const;
    int getInt(const std::string& key, int default_value = 0) const;
    bool getBool(const std::string& key, bool default_value = false) const;
    
    // 检查配置是否存在
    bool hasKey(const std::string& key) const;
    
    // 设置配置值（运行时修改）
    void setString(const std::string& key, const std::string& value);
    
    // 获取配置文件的路径
    std::string getConfigFilePath() const { return config_filepath_; }

private:
    Config();
    ~Config() = default;
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
    
    std::map<std::string, std::string> config_map_;
    std::string config_filepath_;
    
    // 简单的JSON解析（仅支持简单的key-value对）
    void parseSimpleJSON(const std::string& content);
    std::string trim(const std::string& str) const;
    std::string unescapeJsonString(const std::string& str) const;
};

} // namespace utils

#endif // CONFIG_H
