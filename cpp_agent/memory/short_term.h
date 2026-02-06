#ifndef SHORT_TERM_H
#define SHORT_TERM_H

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <chrono>

namespace memory {

struct ChatRound {
    std::string session_id;
    std::string user_id;
    std::string input;
    std::string reply;
    std::chrono::system_clock::time_point timestamp;
};

class ShortTermMemory {
public:
    static ShortTermMemory& getInstance();
    
    void saveShortTerm(const ChatRound& round);
    std::string getShortTermContext(const std::string& user_id);

private:
    ShortTermMemory() = default;
    ~ShortTermMemory() = default;
    ShortTermMemory(const ShortTermMemory&) = delete;
    ShortTermMemory& operator=(const ShortTermMemory&) = delete;
    
    std::mutex mutex_;
    std::map<std::string, std::vector<ChatRound>> store_;
    static constexpr int max_short_rounds = 10;
};

} // namespace memory

#endif // SHORT_TERM_H
