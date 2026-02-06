#include "short_term.h"
#include <sstream>
#include <algorithm>

namespace memory {

ShortTermMemory& ShortTermMemory::getInstance() {
    static ShortTermMemory instance;
    return instance;
}

void ShortTermMemory::saveShortTerm(const ChatRound& round) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto& rounds = store_[round.user_id];
    rounds.push_back(round);
    
    // 只保留最后10轮
    if (rounds.size() > max_short_rounds) {
        rounds.erase(rounds.begin(), rounds.end() - max_short_rounds);
    }
}

std::string ShortTermMemory::getShortTermContext(const std::string& user_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = store_.find(user_id);
    if (it == store_.end() || it->second.empty()) {
        return "无历史对话";
    }
    
    const auto& rounds = it->second;
    std::ostringstream context;
    
    for (size_t i = 0; i < rounds.size(); ++i) {
        context << "第" << (i + 1) << "轮用户输入：" << rounds[i].input << "；";
    }
    
    return context.str();
}

} // namespace memory
