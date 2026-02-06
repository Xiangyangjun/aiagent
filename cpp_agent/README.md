# C++ AI Agent

这是原Go版本AI Agent的C++实现版本。

## 功能特性

- **LLM调用**: 调用阿里通义千问API生成对话回复
- **TTS语音合成**: 调用阿里云TTS API生成语音
- **长期记忆**: 保存用户的偏好关键词
- **短期记忆**: 保存最近10轮对话上下文
- **HTTP服务**: 提供RESTful API接口

## 依赖要求

### 系统依赖

- C++17或更高版本的编译器（GCC 7+ 或 Clang 5+）
- CMake 3.15或更高版本
- libcurl开发库
- pthread库（通常系统自带）

### Ubuntu/Debian安装依赖

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake libcurl4-openssl-dev
```

### CentOS/RHEL安装依赖

```bash
sudo yum install -y gcc-c++ cmake libcurl-devel
```

## 编译和部署

### 一键构建和部署（推荐）

```bash
cd cpp_agent
./build_and_deploy.sh
```

这将自动完成：
1. 检查依赖
2. 清理旧构建
3. 编译项目（Release模式）
4. 部署到 `output/` 目录
5. 创建启动脚本和配置文件

### 快速构建（开发用）

```bash
cd cpp_agent
./build.sh
```

编译完成后，可执行文件位于 `build/cpp_agent`

### 手动构建

```bash
cd cpp_agent
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

## 配置

### 配置文件

所有配置通过 `config.json` 文件进行设置。首次运行前，请复制示例配置文件：

```bash
cp config.json.example config.json
```

然后编辑 `config.json` 文件，填入你的配置信息：

```json
{
  "dashscope_api_key": "your_dashscope_api_key_here",
  "aliyun_tts_key": "sk-21c5679fdf204dc9928a322e2738a75f",
  "log_level": "INFO",
  "log_file": "",
  "server_port": 8443,
  "data_dir": "./data"
}
```

#### 配置项说明

- **dashscope_api_key** (必需): 阿里通义千问API密钥
- **aliyun_tts_key** (可选): 阿里云TTS API密钥，有默认值
- **log_level** (可选): 日志级别，可选值：`DEBUG`, `INFO`, `WARN`, `ERROR`（默认：`INFO`）
- **log_file** (可选): 日志文件路径，为空则只输出到控制台
- **server_port** (可选): HTTP服务器端口（默认：8443）
- **data_dir** (可选): 数据存储目录（默认：`./data`）

#### 日志配置示例

```json
{
  "log_level": "DEBUG",
  "log_file": "./logs/app.log"
}
```

日志格式：
```
[2026-02-06 14:30:25.123] [INFO ] [HTTP] C++ AI Agent服务启动成功
[2026-02-06 14:30:25.124] [INFO ] [HTTP] Web页面访问地址：http://localhost:8443
[2026-02-06 14:30:30.456] [DEBUG] [LLM] Prompt: ...
[2026-02-06 14:30:31.789] [WARN ] [TTS] 生成语音失败: ...
[2026-02-06 14:30:32.012] [ERROR] [HTTP] 创建socket失败
```

日志级别说明：
- **DEBUG**: 详细的调试信息（包括prompt内容等）
- **INFO**: 一般信息（服务启动、请求处理等）
- **WARN**: 警告信息（非致命错误，如TTS失败）
- **ERROR**: 错误信息（致命错误，如初始化失败）

## 运行

### 使用部署目录运行（推荐）

```bash
cd output
# 编辑配置文件（首次运行）
vi config.json
# 启动服务
./start.sh
```

### 开发模式运行

```bash
cd build
# 确保有config.json配置文件
./cpp_agent
```

服务将在配置的端口启动（默认 `http://localhost:8443`）。

**停止服务**: 按 `Ctrl+C` 或运行 `./stop.sh`（在output目录中）

## API接口

### POST /agent/chat

对话接口

**请求体:**
```json
{
  "session_id": "session456",
  "user_id": "user123",
  "input": "推荐一款适合我的饮品"
}
```

**响应:**
```json
{
  "code": 200,
  "msg": "success",
  "data": {
    "text": "AI回复文本",
    "audio_url": "音频临时URL",
    "tts_ok": true,
    "tts_err": ""
  }
}
```

### POST /agent/save-prefer

保存用户偏好

**请求体:**
```json
{
  "user_id": "user123",
  "key": "keywords",
  "value": "钓鱼,看电影"
}
```

## 项目结构

```
cpp_agent/
├── CMakeLists.txt          # CMake构建配置
├── main.cpp                # 主程序入口
├── memory/                 # 记忆模块
│   ├── long_term.h/cpp    # 长期记忆
│   └── short_term.h/cpp   # 短期记忆
├── llm/                   # LLM模块
│   ├── llm.h/cpp          # 大模型调用
├── tts/                   # TTS模块
│   ├── tts.h/cpp          # 语音合成
├── static/                # 静态文件
│   └── index.html         # Web前端页面
└── data/                  # 数据目录
    └── long_term_memory.json  # 长期记忆存储文件
```

## 注意事项

1. 本实现使用简单的HTTP服务器，仅支持基本的HTTP请求处理
2. JSON解析使用正则表达式，对于复杂JSON可能不够健壮
3. 长期记忆数据存储在 `data/long_term_memory.json` 文件中
4. 服务默认监听8443端口，如需修改请编辑 `main.cpp` 中的端口号

## 与Go版本的差异

1. **HTTP服务器**: 使用原生socket实现，而非Gin框架
2. **JSON处理**: 使用正则表达式简单解析，而非完整的JSON库
3. **并发处理**: 使用std::thread而非goroutine
4. **错误处理**: 使用C++异常机制

## 许可证

与原Go版本保持一致。
