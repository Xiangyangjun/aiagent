#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <fstream>
#include <mutex>
#include <memory>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <ctime>

namespace utils {

enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR = 3
};

class Logger {
public:
    static Logger& getInstance();
    
    // 初始化日志系统（从配置文件读取设置）
    void init();
    
    // 设置日志级别
    void setLogLevel(LogLevel level);
    
    // 设置日志文件（可选，如果为空则只输出到控制台）
    void setLogFile(const std::string& filepath);
    
    // 日志输出接口
    void log(LogLevel level, const std::string& module, const std::string& message);
    
    // 便捷方法
    void debug(const std::string& module, const std::string& message);
    void info(const std::string& module, const std::string& message);
    void warn(const std::string& module, const std::string& message);
    void error(const std::string& module, const std::string& message);
    
    // 格式化输出（支持流式操作）
    class LogStream {
    public:
        LogStream(Logger* logger, LogLevel level, const std::string& module)
            : logger_(logger), level_(level), module_(module) {}
        
        ~LogStream() {
            logger_->log(level_, module_, stream_.str());
        }
        
        template<typename T>
        LogStream& operator<<(const T& value) {
            stream_ << value;
            return *this;
        }
        
    private:
        Logger* logger_;
        LogLevel level_;
        std::string module_;
        std::ostringstream stream_;
    };
    
    LogStream debug(const std::string& module) {
        return LogStream(this, LogLevel::DEBUG, module);
    }
    
    LogStream info(const std::string& module) {
        return LogStream(this, LogLevel::INFO, module);
    }
    
    LogStream warn(const std::string& module) {
        return LogStream(this, LogLevel::WARN, module);
    }
    
    LogStream error(const std::string& module) {
        return LogStream(this, LogLevel::ERROR, module);
    }

private:
    Logger();
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    std::string levelToString(LogLevel level);
    std::string getCurrentTime();
    void writeLog(const std::string& log_message);
    
    LogLevel current_level_;
    std::mutex mutex_;
    std::unique_ptr<std::ofstream> log_file_;
    std::string log_filepath_;
};

// 全局便捷宏
#define LOG_DEBUG(module, msg) utils::Logger::getInstance().debug(module, msg)
#define LOG_INFO(module, msg) utils::Logger::getInstance().info(module, msg)
#define LOG_WARN(module, msg) utils::Logger::getInstance().warn(module, msg)
#define LOG_ERROR(module, msg) utils::Logger::getInstance().error(module, msg)

// 流式日志宏
#define LOG_DEBUG_STREAM(module) utils::Logger::getInstance().debug(module)
#define LOG_INFO_STREAM(module) utils::Logger::getInstance().info(module)
#define LOG_WARN_STREAM(module) utils::Logger::getInstance().warn(module)
#define LOG_ERROR_STREAM(module) utils::Logger::getInstance().error(module)

} // namespace utils

#endif // LOGGER_H
