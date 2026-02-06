#!/bin/bash

# 一键构建和部署脚本
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "=========================================="
echo "  C++ AI Agent 一键构建和部署"
echo "=========================================="
echo ""

# 检查依赖
echo "[1/5] 检查依赖..."
if ! command -v cmake >/dev/null 2>&1; then
    echo "错误: 未找到 cmake，请先安装"
    echo "Ubuntu/Debian: sudo apt-get install cmake"
    echo "CentOS/RHEL: sudo yum install cmake"
    exit 1
fi

CMAKE_VERSION=$(cmake --version 2>/dev/null | head -n1 | cut -d' ' -f3)
echo "  - CMake: $CMAKE_VERSION"

# 检查libcurl
CURL_FOUND=false
if pkg-config --exists libcurl 2>/dev/null; then
    CURL_FOUND=true
    CURL_VERSION=$(pkg-config --modversion libcurl 2>/dev/null)
    echo "  - libcurl: $CURL_VERSION (via pkg-config)"
elif [ -f /usr/include/curl/curl.h ] || [ -f /usr/local/include/curl/curl.h ]; then
    CURL_FOUND=true
    echo "  - libcurl: 已找到 (系统库)"
fi

if [ "$CURL_FOUND" = false ]; then
    echo "错误: 未找到 libcurl 开发库，请先安装"
    echo "Ubuntu/Debian: sudo apt-get install libcurl4-openssl-dev"
    echo "CentOS/RHEL: sudo yum install libcurl-devel"
    exit 1
fi

echo "✓ 依赖检查通过"
echo ""

# 清理旧的构建
echo "[2/5] 清理旧的构建文件..."
rm -rf build
rm -rf output
echo "✓ 清理完成"
echo ""

# 创建构建目录
echo "[3/5] 配置CMake..."
mkdir -p build
cd build

# 检测构建类型
BUILD_TYPE="${1:-Release}"
if [ "$BUILD_TYPE" != "Release" ] && [ "$BUILD_TYPE" != "Debug" ]; then
    BUILD_TYPE="Release"
fi

echo "  构建类型: $BUILD_TYPE"
cmake .. -DCMAKE_BUILD_TYPE=$BUILD_TYPE
if [ $? -ne 0 ]; then
    echo "错误: CMake配置失败"
    exit 1
fi
echo "✓ CMake配置完成"
echo ""

# 编译
echo "[4/5] 开始编译..."
CPU_COUNT=$(nproc 2>/dev/null || echo "4")
make -j$CPU_COUNT
if [ $? -ne 0 ]; then
    echo "错误: 编译失败"
    exit 1
fi
echo "✓ 编译完成"
echo ""

# 部署到output目录
echo "[5/5] 部署到output目录..."
cd ..

# 创建output目录结构
mkdir -p output
mkdir -p output/data
mkdir -p output/logs
mkdir -p output/static

# 复制可执行文件
if [ ! -f build/cpp_agent ]; then
    echo "错误: 未找到编译产物 build/cpp_agent"
    exit 1
fi

cp build/cpp_agent output/
chmod +x output/cpp_agent
echo "✓ 已复制可执行文件"

# 复制静态文件
if [ -d static ]; then
    cp -r static/* output/static/ 2>/dev/null || true
    echo "✓ 已复制静态文件"
else
    echo "警告: 未找到static目录"
fi

# 复制配置文件（如果不存在）
if [ ! -f output/config.json ]; then
    if [ -f config.json ]; then
        cp config.json output/
        echo "✓ 已复制配置文件"
    else
        cp config.json.example output/config.json
        echo "警告: 未找到config.json，已复制config.json.example"
        echo "请编辑 output/config.json 填入你的配置信息"
    fi
else
    echo "✓ 配置文件已存在"
fi

# 创建启动脚本
cat > output/start.sh << 'EOF'
#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# 检查可执行文件
if [ ! -f cpp_agent ]; then
    echo "错误: 未找到可执行文件 cpp_agent"
    exit 1
fi

# 检查配置文件
if [ ! -f config.json ]; then
    echo "错误: 未找到config.json配置文件"
    if [ -f config.json.example ]; then
        echo "请复制config.json.example为config.json并填入配置信息:"
        echo "  cp config.json.example config.json"
        echo "  vi config.json"
    fi
    exit 1
fi

# 创建必要的目录
mkdir -p data
mkdir -p logs

# 启动服务
echo "启动 C++ AI Agent..."
echo "按 Ctrl+C 停止服务"
echo ""
./cpp_agent
EOF
chmod +x output/start.sh
echo "✓ 已创建启动脚本"

# 创建停止脚本
cat > output/stop.sh << 'EOF'
#!/bin/bash
PID=$(pgrep -f "./cpp_agent" | grep -v "$$")
if [ -n "$PID" ]; then
    kill $PID 2>/dev/null
    sleep 1
    # 如果还在运行，强制杀死
    if kill -0 $PID 2>/dev/null; then
        kill -9 $PID 2>/dev/null
    fi
    echo "已停止 C++ AI Agent (PID: $PID)"
else
    echo "C++ AI Agent 未运行"
fi
EOF
chmod +x output/stop.sh
echo "✓ 已创建停止脚本"

# 创建README
cat > output/README.md << 'EOF'
# C++ AI Agent 部署目录

## 快速开始

1. **配置**
   ```bash
   # 编辑配置文件
   vi config.json
   ```

2. **启动服务**
   ```bash
   ./start.sh
   ```

3. **停止服务**
   ```bash
   ./stop.sh
   ```

## 目录结构

```
output/
├── cpp_agent          # 可执行文件
├── config.json        # 配置文件（需要编辑）
├── start.sh           # 启动脚本
├── stop.sh            # 停止脚本
├── data/              # 数据目录
│   └── long_term_memory.json
├── logs/              # 日志目录
└── static/            # 静态文件
    └── index.html
```

## 配置说明

编辑 `config.json` 文件，填入以下配置：

- `dashscope_api_key`: 阿里通义千问API密钥（必需）
- `aliyun_tts_key`: 阿里云TTS API密钥（可选）
- `log_level`: 日志级别（DEBUG/INFO/WARN/ERROR）
- `log_file`: 日志文件路径（为空则输出到控制台）
- `server_port`: 服务器端口（默认8443）

## 访问

服务启动后，访问：http://localhost:8443
EOF
echo "✓ 已创建README文档"

echo ""
echo "=========================================="
echo "  构建和部署完成！"
echo "=========================================="
echo ""
echo "部署目录: $(pwd)/output"
echo ""
echo "下一步："
echo "  1. cd output"
if [ ! -f output/config.json ] || [ -f config.json.example ] && [ ! -f config.json ]; then
    echo "  2. 编辑 config.json 填入配置信息"
    echo "  3. ./start.sh 启动服务"
else
    echo "  2. ./start.sh 启动服务"
fi
echo ""
echo "访问地址: http://localhost:8443"
echo ""
