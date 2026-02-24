#ifndef HTTP_UTILS_H
#define HTTP_UTILS_H

#include <string>

namespace utils {

/**
 * @brief HTTP工具函数
 */
class HttpUtils {
public:
    /**
     * @brief 转义JSON字符串用于HTTP响应
     * @param str 原始字符串
     * @return 转义后的字符串
     */
    static std::string escapeJsonForResponse(const std::string& str);
    
    /**
     * @brief 创建HTTP错误响应
     * @param code HTTP状态码
     * @param message 错误消息
     * @return HTTP响应字符串
     */
    static std::string createErrorResponse(int code, const std::string& message);
    
    /**
     * @brief 创建HTTP成功响应（JSON）
     * @param json_body JSON响应体
     * @return HTTP响应字符串
     */
    static std::string createJsonResponse(const std::string& json_body);
    
    /**
     * @brief 从HTTP请求中提取JSON body
     * @param request HTTP请求字符串
     * @return JSON body，如果找不到返回空字符串
     */
    static std::string extractJsonBody(const std::string& request);
    
    /**
     * @brief 从JSON字符串中提取字段值（简单解析）
     * @param json JSON字符串
     * @param key 字段名
     * @return 字段值，如果找不到返回空字符串
     */
    static std::string extractJsonField(const std::string& json, const std::string& key);
    
    /**
     * @brief 获取文件的Content-Type
     * @param filepath 文件路径
     * @return Content-Type字符串
     */
    static std::string getContentType(const std::string& filepath);
};

} // namespace utils

#endif // HTTP_UTILS_H
