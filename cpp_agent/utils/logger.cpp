#include "logger.h"
#include "config.h"
#include <iostream>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <algorithm>

namespace utils {

Logger::Logger() : current_level_(LogLevel::INFO) {
    // 构造函数中不读取配置，等待init()调用
}

Logger::~Logger() {
    if (log_file_ && log_file_->is_open()) {
        log_file_->close();
    }
}

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

void Logger::init() {
    // 从配置文件读取日志级别
    auto& config = Config::getInstance();
    std::string level_str = config.getString("log_level", "INFO");
    
    std::transform(level_str.begin(), level_str.end(), level_str.begin(), ::toupper);
    if (level_str == "DEBUG") {
        current_level_ = LogLevel::DEBUG;
    } else if (level_str == "INFO") {
        current_level_ = LogLevel::INFO;
    } else if (level_str == "WARN") {
        current_level_ = LogLevel::WARN;
    } else if (level_str == "ERROR") {
        current_level_ = LogLevel::ERROR;
    }
    
    // 从配置文件读取日志文件路径
    std::string log_file = config.getString("log_file", "");
    if (!log_file.empty()) {
        setLogFile(log_file);
    }
}

void Logger::setLogLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    current_level_ = level;
}

void Logger::setLogFile(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(mutex_);
    log_filepath_ = filepath;
    
    if (log_file_ && log_file_->is_open()) {
        log_file_->close();
    }
    
    if (!filepath.empty()) {
        log_file_ = std::make_unique<std::ofstream>(filepath, std::ios::app);
        if (!log_file_->is_open()) {
            std::cerr << "[Logger Error] 无法打开日志文件: " << filepath << std::endl;
            log_file_.reset();
        }
    } else {
        log_file_.reset();
    }
}

std::string Logger::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO ";
        case LogLevel::WARN:  return "WARN ";
        case LogLevel::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

std::string Logger::getCurrentTime() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::tm* tm_ptr = std::localtime(&time_t);
    if (!tm_ptr) {
        return "0000-00-00 00:00:00.000";
    }
    
    std::ostringstream oss;
    oss << std::put_time(tm_ptr, "%Y-%m-%d %H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

void Logger::writeLog(const std::string& log_message) {
    // 输出到控制台
    std::cout << log_message << std::endl;
    
    // 输出到文件（如果已设置）
    if (log_file_ && log_file_->is_open()) {
        *log_file_ << log_message << std::endl;
        log_file_->flush();
    }
}

void Logger::log(LogLevel level, const std::string& module, const std::string& message) {
    // 检查日志级别
    if (level < current_level_) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ostringstream log_stream;
    log_stream << "[" << getCurrentTime() << "] "
               << "[" << levelToString(level) << "] "
               << "[" << module << "] "
               << message;
    
    writeLog(log_stream.str());
}

void Logger::debug(const std::string& module, const std::string& message) {
    log(LogLevel::DEBUG, module, message);
}

void Logger::info(const std::string& module, const std::string& message) {
    log(LogLevel::INFO, module, message);
}

void Logger::warn(const std::string& module, const std::string& message) {
    log(LogLevel::WARN, module, message);
}

void Logger::error(const std::string& module, const std::string& message) {
    log(LogLevel::ERROR, module, message);
}

} // namespace utils
