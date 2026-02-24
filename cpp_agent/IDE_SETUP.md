# IDE配置指南

## VS Code / Cursor

### 1. 安装C++扩展

- **C/C++** (Microsoft) - 提供代码跳转和智能提示
- **clangd** (可选) - 更强大的代码分析工具

### 2. 生成编译数据库

运行CMake配置时会自动生成 `compile_commands.json`：

```bash
cd cpp_agent
mkdir -p build
cd build
cmake .. -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

或者直接运行构建脚本，会自动生成：

```bash
./build_and_deploy.sh
```

### 3. 配置VS Code

项目已包含 `.vscode/c_cpp_properties.json` 配置文件，VS Code会自动读取。

如果代码跳转仍然不工作：

1. **检查compile_commands.json是否存在**：
   ```bash
   ls build/compile_commands.json
   ```

2. **重新加载窗口**：
   - 按 `Ctrl+Shift+P`
   - 输入 "Reload Window"
   - 或重启VS Code

3. **手动指定compile_commands.json路径**：
   - 按 `Ctrl+Shift+P`
   - 输入 "C/C++: Edit Configurations"
   - 设置 `compileCommands` 为 `${workspaceFolder}/build/compile_commands.json`

### 4. 使用clangd（推荐）

如果使用clangd扩展：

1. 安装 **clangd** 扩展
2. 禁用 **C/C++** 扩展（避免冲突）
3. clangd会自动读取 `build/compile_commands.json`

## CLion

1. 打开项目根目录
2. CLion会自动检测CMakeLists.txt
3. 在设置中启用 "Compilation Database"：
   - Settings → Build, Execution, Deployment → CMake
   - 确保 "Export compile commands" 已启用

## 其他IDE

### Vim/Neovim (coc-clangd)

```bash
# 安装coc-clangd
:CocInstall coc-clangd

# 确保compile_commands.json存在
```

### Emacs (lsp-mode)

```elisp
;; 配置lsp-clangd
(setq lsp-clients-clangd-args '("--compile-commands-dir=build"))
```

## 常见问题

### 问题1：无法跳转到定义

**解决方案**：
1. 确保 `build/compile_commands.json` 存在
2. 重新运行CMake配置
3. 重启IDE

### 问题2：智能提示不工作

**解决方案**：
1. 检查C++标准设置（应为C++17）
2. 检查include路径配置
3. 清除IDE缓存并重新索引

### 问题3：找不到头文件

**解决方案**：
1. 检查 `.vscode/c_cpp_properties.json` 中的includePath
2. 确保所有源文件目录都在includePath中
3. 运行 `./build_and_deploy.sh` 重新生成编译数据库

## 验证配置

运行以下命令验证配置：

```bash
cd cpp_agent/build
# 检查compile_commands.json是否存在
ls -lh compile_commands.json

# 查看内容（应该包含所有源文件的编译命令）
head -20 compile_commands.json
```

如果文件存在且包含编译命令，IDE应该能够正常进行代码跳转。
