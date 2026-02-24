#ifndef JSON_PARSER_H
#define JSON_PARSER_H

#include <string>

namespace utils {

/**
 * @brief 简单的JSON解析工具类
 * 
 * 注意：这是一个简化的JSON解析器，仅支持简单的key-value结构
 * 对于复杂的嵌套JSON，建议使用完整的JSON库（如nlohmann/json）
 */
class JsonParser {
public:
    /**
     * @brief 从JSON字符串中提取字符串值
     * @param json JSON字符串
     * @param key 要提取的键名
     * @param default_value 默认值（如果找不到）
     * @return 提取的值或默认值
     */
    static std::string extractString(const std::string& json, 
                                     const std::string& key,
                                     const std::string& default_value = "");
    
    /**
     * @brief 从嵌套JSON结构中提取content字段
     * 支持格式: {"output":{"choices":[{"message":{"content":"..."}}]}}
     * @param json JSON字符串
     * @return content字段的值，如果找不到返回空字符串
     */
    static std::string extractContentFromNestedJson(const std::string& json);
    
    /**
     * @brief 转义JSON字符串中的特殊字符
     * @param str 原始字符串
     * @return 转义后的字符串
     */
    static std::string escapeJsonString(const std::string& str);
    
    /**
     * @brief 反转义JSON字符串
     * @param str 转义后的字符串
     * @return 原始字符串
     */
    static std::string unescapeJsonString(const std::string& str);

private:
    /**
     * @brief 查找并提取JSON字符串值（支持转义字符）
     * @param json JSON字符串
     * @param key_start 键名的起始位置
     * @return 提取的值，如果失败返回空字符串
     */
    static std::string extractStringValue(const std::string& json, size_t key_start);
};

} // namespace utils

#endif // JSON_PARSER_H
