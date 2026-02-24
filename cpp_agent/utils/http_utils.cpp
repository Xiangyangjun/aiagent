#include "http_utils.h"
#include "json_parser.h"
#include <sstream>

namespace utils {

std::string HttpUtils::escapeJsonForResponse(const std::string& str) {
    return JsonParser::escapeJsonString(str);
}
static const char* reasonPhrase(int code) {
    switch (code) {
        case 200: return "OK";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 500: return "Internal Server Error";
        default: return "Error";
    }
}
std::string HttpUtils::createErrorResponse(int code, const std::string& message) {
    std::ostringstream json;
    json << "{\"code\":" << code
         << ",\"msg\":\"" << escapeJsonForResponse(message)
         << "\",\"data\":null}";
    std::ostringstream resp;
    resp << "HTTP/1.1 " << code << " " << reasonPhrase(code) << "\r\n"
         << "Content-Type: application/json; charset=utf-8\r\n"
         << "Access-Control-Allow-Origin: *\r\n"
         << "Access-Control-Allow-Methods: GET, POST\r\n"
         << "Access-Control-Allow-Headers: Content-Type\r\n"
         << "Content-Length: " << json.str().length() << "\r\n"
         << "\r\n"
         << json.str();

    return resp.str();
}

std::string HttpUtils::createJsonResponse(const std::string& json_body) {
    std::ostringstream resp;
    resp << "HTTP/1.1 200 OK\r\n"
         << "Content-Type: application/json; charset=utf-8\r\n"
         << "Access-Control-Allow-Origin: *\r\n"
         << "Access-Control-Allow-Methods: GET, POST\r\n"
         << "Access-Control-Allow-Headers: Content-Type\r\n"
         << "Content-Length: " << json_body.length() << "\r\n"
         << "\r\n"
         << json_body;
    return resp.str();
}

std::string HttpUtils::extractJsonBody(const std::string& request) {
    size_t body_start = request.find("\r\n\r\n");
    if (body_start == std::string::npos) {
        return "";
    }
    return request.substr(body_start + 4);
}

std::string HttpUtils::extractJsonField(const std::string& json, const std::string& key) {
    return JsonParser::extractString(json, key, "");
}

std::string HttpUtils::getContentType(const std::string& filepath) {
    auto hasSuffix = [&](const std::string& suffix) -> bool {
        if (filepath.length() < suffix.length()) return false;
        return filepath.compare(filepath.length() - suffix.length(), suffix.length(), suffix) == 0;
    };

    if (hasSuffix(".html") || hasSuffix(".htm")) return "text/html; charset=utf-8";
    if (hasSuffix(".css")) return "text/css; charset=utf-8";
    if (hasSuffix(".js")) return "application/javascript; charset=utf-8";
    if (hasSuffix(".json")) return "application/json; charset=utf-8";
    if (hasSuffix(".txt")) return "text/plain; charset=utf-8";
    if (hasSuffix(".svg")) return "image/svg+xml";
    if (hasSuffix(".png")) return "image/png";
    if (hasSuffix(".jpg") || hasSuffix(".jpeg")) return "image/jpeg";
    if (hasSuffix(".gif")) return "image/gif";
    if (hasSuffix(".ico")) return "image/x-icon";

    return "application/octet-stream";
}

} // namespace utils

