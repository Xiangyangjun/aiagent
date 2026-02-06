#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <curl/curl.h>
#include "memory/long_term.h"
#include "memory/short_term.h"
#include "llm/llm.h"
#include "tts/tts.h"
#include "utils/logger.h"
#include "utils/config.h"

// 简单的HTTP服务器实现（基于socket）
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <regex>
#include <signal.h>
#include <limits.h>
#include <sys/stat.h>

// 前向声明
class SimpleHTTPServer;

// 全局变量用于信号处理
static SimpleHTTPServer* g_server = nullptr;

// 信号处理函数声明（实现在类定义之后）
static void handleSignal(int sig);

class SimpleHTTPServer {
public:
    SimpleHTTPServer(int port) : port_(port), running_(false), server_fd_(-1) {
        LOG_INFO("HTTP", "HTTP服务器初始化，端口: " + std::to_string(port));
    }
    
    ~SimpleHTTPServer() {
        stop();
    }
    
    void stop() {
        if (running_) {
            running_ = false;
            if (server_fd_ >= 0) {
                shutdown(server_fd_, SHUT_RDWR);
                close(server_fd_);
                server_fd_ = -1;
            }
            if (g_server == this) {
                g_server = nullptr;
            }
            LOG_INFO("HTTP", "HTTP服务器已停止");
        }
    }
    
    void start() {
        server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd_ < 0) {
            LOG_ERROR("HTTP", "创建socket失败");
            return;
        }
        
        int opt = 1;
        setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        struct sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port_);
        
        if (bind(server_fd_, (struct sockaddr*)&address, sizeof(address)) < 0) {
            LOG_ERROR("HTTP", "绑定端口失败，端口: " + std::to_string(port_) + " (可能已被占用)");
            close(server_fd_);
            server_fd_ = -1;
            return;
        }
        
        if (listen(server_fd_, 10) < 0) {
            LOG_ERROR("HTTP", "监听失败，端口: " + std::to_string(port_));
            close(server_fd_);
            server_fd_ = -1;
            return;
        }
        
        running_ = true;
        g_server = this;
        
        // 注册信号处理（使用普通函数，因为lambda无法捕获全局变量）
        signal(SIGINT, handleSignal);
        signal(SIGTERM, handleSignal);
        
        LOG_INFO("HTTP", "C++ AI Agent服务启动成功");
        LOG_INFO("HTTP", "Web页面访问地址：http://localhost:" + std::to_string(port_));
        LOG_INFO("HTTP", "按 Ctrl+C 停止服务");
        
        while (running_) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int client_fd = accept(server_fd_, (struct sockaddr*)&client_addr, &client_len);
            
            if (client_fd < 0) {
                if (running_) {
                    continue;
                } else {
                    break;
                }
            }
            
            std::thread(&SimpleHTTPServer::handleClient, this, client_fd).detach();
        }
        
        if (server_fd_ >= 0) {
            close(server_fd_);
            server_fd_ = -1;
        }
    }
    
private:
    void handleClient(int client_fd) {
        char buffer[4096] = {0};
        ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
        
        if (bytes_read <= 0) {
            close(client_fd);
            return;
        }
        
        std::string request(buffer, bytes_read);
        std::string response;
        
        // 解析请求
        if (request.find("GET / ") == 0 || request.find("GET / HTTP") == 0 || 
            request.find("GET /index.html") != std::string::npos) {
            // 返回静态HTML页面
            response = serveStaticFile("index.html");
        } else if (request.find("POST /agent/chat") != std::string::npos) {
            // 处理聊天请求
            response = handleChatRequest(request);
        } else if (request.find("POST /agent/save-prefer") != std::string::npos) {
            // 处理保存偏好请求
            response = handleSavePreferRequest(request);
        } else {
            response = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\nNot Found";
        }
        
        send(client_fd, response.c_str(), response.length(), 0);
        close(client_fd);
    }
    
    std::string serveStaticFile(const std::string& filepath) {
        // 尝试多个路径
        std::vector<std::string> paths = {
            filepath,
            "static/" + filepath,
            "./static/" + filepath
        };
        
        // 如果filepath是index.html，尝试从可执行文件目录查找
        if (filepath == "index.html" || filepath == "/index.html" || filepath == "/") {
            char exe_path[1024];
            ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
            if (len != -1) {
                exe_path[len] = '\0';
                std::string exe_dir = std::string(exe_path);
                size_t last_slash = exe_dir.find_last_of("/");
                if (last_slash != std::string::npos) {
                    exe_dir = exe_dir.substr(0, last_slash + 1);
                    paths.insert(paths.begin(), exe_dir + "static/index.html");
                }
            }
        }
        
        std::string actual_path;
        std::ifstream file;
        bool found = false;
        
        for (const auto& path : paths) {
            file.open(path, std::ios::binary);
            if (file.is_open()) {
                actual_path = path;
                found = true;
                break;
            }
        }
        
        if (!found) {
            return "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\nFile Not Found";
        }
        
        std::ostringstream content;
        content << file.rdbuf();
        file.close();
        
        // 确定Content-Type
        std::string content_type = "text/html; charset=utf-8";
        if (actual_path.find(".css") != std::string::npos) {
            content_type = "text/css";
        } else if (actual_path.find(".js") != std::string::npos) {
            content_type = "application/javascript";
        } else if (actual_path.find(".json") != std::string::npos) {
            content_type = "application/json";
        }
        
        std::ostringstream response;
        response << "HTTP/1.1 200 OK\r\n"
                 << "Content-Type: " << content_type << "\r\n"
                 << "Content-Length: " << content.str().length() << "\r\n"
                 << "Access-Control-Allow-Origin: *\r\n"
                 << "\r\n"
                 << content.str();
        
        return response.str();
    }
    
    std::string handleChatRequest(const std::string& request) {
        // 提取JSON body
        size_t body_start = request.find("\r\n\r\n");
        if (body_start == std::string::npos) {
            return createErrorResponse(400, "参数错误：缺少请求体");
        }
        
        std::string body = request.substr(body_start + 4);
        
        // 简单的JSON解析
        std::regex session_regex(R"xxx("session_id"\s*:\s*"([^"]*)")xxx");
        std::regex user_regex(R"xxx("user_id"\s*:\s*"([^"]*)")xxx");
        std::regex input_regex(R"xxx("input"\s*:\s*"([^"]*)")xxx");
        
        std::smatch match;
        std::string session_id, user_id, user_input;
        
        if (!std::regex_search(body, match, session_regex)) {
            return createErrorResponse(400, "参数错误：缺少session_id");
        }
        session_id = match[1].str();
        
        if (!std::regex_search(body, match, user_regex)) {
            return createErrorResponse(400, "参数错误：缺少user_id");
        }
        user_id = match[1].str();
        
        if (!std::regex_search(body, match, input_regex)) {
            return createErrorResponse(400, "参数错误：缺少input");
        }
        user_input = match[1].str();
        
        if (user_id.empty()) {
            return createErrorResponse(400, "UserID不能为空");
        }
        
        try {
            LOG_INFO("HTTP", "收到聊天请求 (会话: " + session_id + ", 用户: " + user_id + ")");
            // 1. 调用大模型生成文本回复
            std::string reply_text = llm::callLLM(session_id, user_id, user_input);
            
            // 2. 调用TTS生成语音（异步）
            std::string audio_url;
            std::string tts_error;
            bool tts_ok = false;
            
            try {
                LOG_DEBUG("TTS", "开始生成语音 (文本长度: " + std::to_string(reply_text.length()) + ")");
                audio_url = tts::generateSpeech(reply_text);
                tts_ok = true;
                LOG_INFO("TTS", "语音生成成功 (URL: " + audio_url + ")");
            } catch (const std::exception& e) {
                tts_error = e.what();
                LOG_WARN("TTS", "生成语音失败: " + std::string(e.what()));
            }
            
            // 3. 保存短期记忆
            auto& short_mem = memory::ShortTermMemory::getInstance();
            memory::ChatRound round;
            round.session_id = session_id;
            round.user_id = user_id;
            round.input = user_input;
            round.reply = reply_text;
            round.timestamp = std::chrono::system_clock::now();
            short_mem.saveShortTerm(round);
            
            // 4. 构造返回数据
            std::ostringstream json_response;
            json_response << "{"
                          << "\"code\":200,"
                          << "\"msg\":\"success\","
                          << "\"data\":{"
                          << "\"text\":\"" << escapeJson(reply_text) << "\","
                          << "\"audio_url\":\"" << escapeJson(audio_url) << "\","
                          << "\"tts_ok\":" << (tts_ok ? "true" : "false") << ","
                          << "\"tts_err\":\"" << escapeJson(tts_error) << "\""
                          << "}"
                          << "}";
            
            std::ostringstream response;
            response << "HTTP/1.1 200 OK\r\n"
                     << "Content-Type: application/json; charset=utf-8\r\n"
                     << "Access-Control-Allow-Origin: *\r\n"
                     << "Access-Control-Allow-Methods: GET, POST\r\n"
                     << "Access-Control-Allow-Headers: Content-Type\r\n"
                     << "Content-Length: " << json_response.str().length() << "\r\n"
                     << "\r\n"
                     << json_response.str();
            
            return response.str();
            
        } catch (const std::exception& e) {
            return createErrorResponse(500, "生成回复失败：" + std::string(e.what()));
        }
    }
    
    std::string handleSavePreferRequest(const std::string& request) {
        size_t body_start = request.find("\r\n\r\n");
        if (body_start == std::string::npos) {
            return createErrorResponse(400, "参数错误：缺少请求体");
        }
        
        std::string body = request.substr(body_start + 4);
        
        std::regex user_regex(R"xxx("user_id"\s*:\s*"([^"]*)")xxx");
        std::regex key_regex(R"xxx("key"\s*:\s*"([^"]*)")xxx");
        std::regex value_regex(R"xxx("value"\s*:\s*"([^"]*)")xxx");
        
        std::smatch match;
        std::string user_id, key, value;
        
        if (std::regex_search(body, match, user_regex)) {
            user_id = match[1].str();
        }
        if (std::regex_search(body, match, key_regex)) {
            key = match[1].str();
        }
        if (std::regex_search(body, match, value_regex)) {
            value = match[1].str();
        }
        
        if (key == "keywords") {
            auto& long_mem = memory::LongTermMemory::getInstance();
            long_mem.mergeAndSaveLongTerm(user_id, value);
        }
        
        std::ostringstream json_response;
        json_response << "{\"code\":200,\"msg\":\"偏好保存成功\"}";
        
        std::ostringstream response;
        response << "HTTP/1.1 200 OK\r\n"
                 << "Content-Type: application/json; charset=utf-8\r\n"
                 << "Access-Control-Allow-Origin: *\r\n"
                 << "Content-Length: " << json_response.str().length() << "\r\n"
                 << "\r\n"
                 << json_response.str();
        
        return response.str();
    }
    
    std::string createErrorResponse(int code, const std::string& msg) {
        std::ostringstream json_response;
        json_response << "{\"code\":" << code << ",\"msg\":\"" << escapeJson(msg) << "\",\"data\":null}";
        
        std::ostringstream response;
        response << "HTTP/1.1 " << code << " " << (code == 400 ? "Bad Request" : "Internal Server Error") << "\r\n"
                 << "Content-Type: application/json; charset=utf-8\r\n"
                 << "Access-Control-Allow-Origin: *\r\n"
                 << "Content-Length: " << json_response.str().length() << "\r\n"
                 << "\r\n"
                 << json_response.str();
        
        return response.str();
    }
    
    std::string escapeJson(const std::string& str) {
        std::string escaped;
        for (char c : str) {
            switch (c) {
                case '"': escaped += "\\\""; break;
                case '\\': escaped += "\\\\"; break;
                case '\n': escaped += "\\n"; break;
                case '\r': escaped += "\\r"; break;
                case '\t': escaped += "\\t"; break;
                default: escaped += c; break;
            }
        }
        return escaped;
    }
    
    int port_;
    bool running_;
    int server_fd_;
};

// 信号处理函数实现（在类定义之后）
static void handleSignal(int sig) {
    (void)sig; // 避免未使用参数警告
    if (g_server) {
        g_server->stop();
    }
    exit(0);
}

int main() {
    // 加载配置文件（优先从当前目录，其次从可执行文件目录）
    auto& config = utils::Config::getInstance();
    std::string config_file = "config.json";
    
    // 尝试从当前目录加载
    if (config.loadFromFile(config_file) != 0) {
        // 如果失败，尝试从可执行文件所在目录加载
        // 获取可执行文件路径
        char exe_path[1024];
        ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
        if (len != -1) {
            exe_path[len] = '\0';
            std::string exe_dir = std::string(exe_path);
            size_t last_slash = exe_dir.find_last_of("/");
            if (last_slash != std::string::npos) {
                exe_dir = exe_dir.substr(0, last_slash + 1);
                std::string exe_config = exe_dir + "config.json";
                if (config.loadFromFile(exe_config) == 0) {
                    config_file = exe_config;
                }
            }
        }
        
        if (config.getConfigFilePath() == "config.json") {
            std::cerr << "警告: 无法加载配置文件，将使用默认配置" << std::endl;
        }
    }
    
    // 初始化日志系统（从配置文件读取设置）
    auto& logger = utils::Logger::getInstance();
    logger.init();
    
    LOG_INFO("Main", "=== C++ AI Agent 启动 ===");
    LOG_INFO("Main", "配置文件路径: " + config.getConfigFilePath());
    
    // 初始化长期记忆模块
    auto& long_mem = memory::LongTermMemory::getInstance();
    if (long_mem.init() != 0) {
        LOG_ERROR("Main", "长期记忆模块初始化失败");
        return 1;
    }
    
    // 初始化curl
    curl_global_init(CURL_GLOBAL_DEFAULT);
    LOG_INFO("Main", "CURL库初始化完成");
    
    // 从配置文件读取端口
    int server_port = config.getInt("server_port", 8443);
    
    // 启动HTTP服务器
    SimpleHTTPServer server(server_port);
    
    try {
        server.start();
    } catch (const std::exception& e) {
        LOG_ERROR("Main", "服务器启动失败: " + std::string(e.what()));
        curl_global_cleanup();
        long_mem.close();
        return 1;
    }
    
    // 清理（通常不会执行到这里，因为server.start()会阻塞）
    curl_global_cleanup();
    long_mem.close();
    
    return 0;
}
