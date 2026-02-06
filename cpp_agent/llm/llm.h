#ifndef LLM_H
#define LLM_H

#include <string>

namespace llm {

std::string extractHabitKeywords(const std::string& user_id);
std::string callLLM(const std::string& session_id, 
                    const std::string& user_id, 
                    const std::string& user_input);

} // namespace llm

#endif // LLM_H
