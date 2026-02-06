#ifndef LONG_TERM_H
#define LONG_TERM_H

#include <string>
#include <map>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <queue>
#include <condition_variable>
#include <vector>

namespace memory {

class LongTermMemory {
public:
    static LongTermMemory& getInstance();
    
    int init();
    std::string mergeAndSaveLongTerm(const std::string& user_id, const std::string& new_keywords);
    std::string getLongTerm(const std::string& user_id);
    void close();

private:
    LongTermMemory();
    ~LongTermMemory();
    LongTermMemory(const LongTermMemory&) = delete;
    LongTermMemory& operator=(const LongTermMemory&) = delete;
    
    int loadFromFile();
    int saveToFile();
    std::vector<std::string> splitKeywords(const std::string& str);
    
    void asyncWriteLoop();
    
    std::mutex mutex_;
    std::map<std::string, std::string> store_;
    static constexpr int max_long_keys = 50;
    static constexpr const char* persist_file = "./data/long_term_memory.json";
    
    std::atomic<bool> initialized_;
    std::atomic<bool> should_stop_;
    std::thread write_thread_;
    std::queue<std::map<std::string, std::string>> write_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
};

} // namespace memory

#endif // LONG_TERM_H
