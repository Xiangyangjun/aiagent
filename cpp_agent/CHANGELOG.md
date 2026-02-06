# C++版本转换日志

## 2026-02-06

### 初始版本

将Go版本的AI Agent完整转换为C++版本。

#### 主要变更

1. **项目结构**
   - 创建了独立的 `cpp_agent` 目录
   - 使用CMake构建系统
   - 模块化设计，保持与Go版本相同的目录结构

2. **模块转换**

   - **memory模块**
     - `long_term.cpp/h`: 长期记忆管理，支持异步文件写入
     - `short_term.cpp/h`: 短期记忆管理，保存最近10轮对话
   
   - **llm模块**
     - `llm.cpp/h`: 调用阿里通义千问API
     - 关键词提取功能
     - LLM对话生成功能
   
   - **tts模块**
     - `tts.cpp/h`: 调用阿里云TTS API生成语音
     - 返回音频临时URL

3. **HTTP服务器**
   - 使用原生socket实现简单的HTTP服务器
   - 支持CORS跨域
   - 提供 `/agent/chat` 和 `/agent/save-prefer` 接口
   - 静态文件服务支持

4. **技术栈**
   - C++17标准
   - libcurl用于HTTP客户端请求
   - pthread用于多线程
   - 标准库实现JSON解析（正则表达式）

#### 与Go版本的差异

1. **HTTP框架**: 使用原生socket而非Gin框架
2. **JSON处理**: 使用正则表达式简单解析，而非完整的JSON库
3. **并发模型**: 使用std::thread和std::mutex而非goroutine
4. **错误处理**: 使用C++异常机制
5. **文件系统**: 使用std::filesystem（C++17）或experimental版本

#### 已知限制

1. JSON解析使用正则表达式，对复杂嵌套JSON支持有限
2. HTTP服务器实现较简单，不支持HTTPS（可后续添加）
3. 错误处理相对简单，可进一步优化

#### 下一步改进建议

1. 集成完整的JSON库（如nlohmann/json）以提高健壮性
2. 添加HTTPS支持
3. 改进错误处理和日志记录
4. 添加单元测试
5. 优化性能（连接池、缓存等）
