# BatchGuard

BatchGuard 是一个使用 C++20 开发的本地重复文件检查工具。当前阶段支持命令行
帮助、版本信息、单目录输入验证和递归普通文件发现。文件哈希和重复判断将在后续
阶段实现。

## 项目结构

```text
apps/cli/                  命令行入口
include/batchguard/core/   核心库公共头文件
src/core/                  核心库实现
tests/                     GoogleTest 测试
Third_Party/               固定版本第三方源码
docs/                      设计、学习和测试文档
```

`batchguard_cli` 链接 `batchguard_core`，最终程序名为 `BatchGuard.exe`。

## 环境要求

- Windows 10 或更高版本
- 支持 C++20 的 MSVC
- CMake 4.2 或更高版本
- Ninja

GoogleTest 1.17.0 已保存在 `Third_Party/googletest/`，配置构建时不需要重复下载。

## 使用 CLion

1. 打开项目根目录。
2. 执行 **Reload CMake Project**。
3. 选择 `batchguard_cli`，在运行配置中填写目录参数后运行程序。
4. 打开 `tests/` 下的测试文件，点击测试左侧绿色按钮运行 GTest。

## 命令行构建

请在已经加载 MSVC 环境并且能找到 CMake、Ninja 的终端中执行。

Debug：

```powershell
cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug
.\build\debug\BatchGuard.exe
```

Release：

```powershell
cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/release
.\build\release\BatchGuard.exe
```

## 使用方式

```text
BatchGuard <目录路径>
BatchGuard --help
BatchGuard --version
```

包含空格的路径需要使用双引号，例如：

```powershell
.\build\debug\BatchGuard.exe "E:\测试数据\待整理目录"
```

当前阶段会验证输入路径，并递归统计其中的普通文件；不跟随符号链接，不读取文件
内容，也不计算哈希。参数缺失、目录不存在或输入为普通文件时返回退出码 `1`；
发现过程中存在局部失败时返回 `2`；帮助、版本和完整发现返回 `0`。

## 运行测试

通过 CTest 运行：

```powershell
ctest --test-dir build/debug --output-on-failure
ctest --test-dir build/release --output-on-failure
```

直接运行 GoogleTest：

```powershell
.\build\debug\tests\batchguard_tests.exe --gtest_color=no
```

不需要测试目标时，可以在配置阶段添加：

```powershell
-DBATCHGUARD_BUILD_TESTS=OFF
```

## 文档

- `docs/第一版MVP.md`：第一版产品行为和验收用例。
- `docs/第一版实现.md`：模块、技术方案和实施顺序。
- `docs/测试/阶段1测试.md`：阶段 1 测试范围和实际结果。
- `docs/测试/阶段2测试.md`：阶段 2 参数和目录输入验证。
- `docs/测试/阶段3测试.md`：阶段 3 递归文件发现和路径处理。
- `docs/cmake学习.md`、`docs/googletest学习.md`、`docs/git学习.md`：学习笔记。

项目对被扫描目录保持只读，不提交构建产物、IDE 私有配置或个人业务数据。
