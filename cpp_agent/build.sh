#!/bin/bash

# 构建脚本（开发用，快速构建）
set -e

echo "=== 开始构建 C++ AI Agent ==="

# 创建构建目录
mkdir -p build
cd build

# 运行CMake配置
echo "运行CMake配置..."
cmake .. -DCMAKE_BUILD_TYPE=Debug

# 编译
echo "开始编译..."
make -j$(nproc)

echo "=== 构建完成 ==="
echo "可执行文件位于: build/cpp_agent"
echo ""
echo "提示: 使用 ./build_and_deploy.sh 进行完整构建和部署"
