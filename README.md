# BatchGuard

BatchGuard 0.1.0 是一个使用 C++20 开发的 Windows 本地重复文件检查工具，支持
单目录输入验证、递归普通文件发现、并发 SHA-256 内容指纹、重复分组、中文扫描
报告和控制台动态进度。程序只读取输入目录，不删除、移动或修改文件。

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
ctest --test-dir build/debug --output-on-failure
.\build\debug\BatchGuard.exe --help
```

Release：

```powershell
cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/release
ctest --test-dir build/release --output-on-failure
.\build\release\BatchGuard.exe --version
```

## 使用方式

```text
BatchGuard <目录路径>
BatchGuard --hash-workers <1-64> <目录路径>
BatchGuard --help
BatchGuard --version
```

包含空格的路径需要使用双引号，例如：

```powershell
.\build\debug\BatchGuard.exe "E:\测试数据\待整理目录"
```

程序会验证输入路径、递归发现普通文件、读取文件大小，并只对至少包含两个文件的
同大小候选组计算 SHA-256。随后按“文件大小 + SHA-256”建立重复组，因此重复
文件可以位于不同子目录，文件名和路径不参与内容是否相同的判断。哈希使用
Windows CNG `BCrypt` 和 64 KiB 固定缓冲区分块读取，不一次性加载完整文件。
候选文件之间使用有界工作线程并发处理；未指定 `--hash-workers` 时自动使用最多
4 个线程。网络盘或机械硬盘可通过 `--hash-workers 1` 至
`--hash-workers 64` 调整并发度，线程数更高不一定更快。

参数缺失、目录不存在或输入为普通文件时返回退出码 `1`；发现、元数据或哈希过程
中存在局部失败时返回 `2`；不可恢复内部错误返回 `3`；帮助、版本和完整扫描返回
`0`。发现重复文件不是错误，不影响退出码。

直接在 Windows 控制台运行时，程序会动态显示以下进度：

```text
正在发现文件：127 个
读取文件信息：[████████████░░░░░░░░] 60%（76/127）
计算内容指纹：[████████████████░░░░] 80%（16/20 个文件，.../... 字节）
重复文件报告已生成。
```

文件发现完成前不知道最终总数，因此该阶段显示累计数量；元数据和 SHA-256 阶段
显示真实比例。标准输出重定向到文件或管道时，动态进度自动关闭，只保留最终报告。

## 设计摘要

核心处理流程为：

```text
输入验证
  → 递归发现普通文件
  → 读取文件大小并筛选同大小候选
  → 有界工作线程计算 SHA-256
  → 按“大小 + SHA-256”建立重复组
  → 输出稳定排序的扫描报告和退出码
```

`batchguard_core` 不依赖 CLI；预期文件系统失败写入报告并继续处理，非预期异常只在
程序入口统一兜底。工作线程只执行文件读取和哈希，进度由扫描调用线程汇聚后串行
通知展示层，避免并发写控制台。

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

- `CHANGELOG.md`：版本变更记录。
- `docs/规划/`：项目目标、完整大纲和第一版 MVP。
- `docs/设计/`：第一版实现、项目结构和代码规范。
- `docs/交付/`：项目介绍、面试问答和最终交付清单。
- `docs/测试/阶段1测试.md`：阶段 1 测试范围和实际结果。
- `docs/测试/阶段2测试.md`：阶段 2 参数和目录输入验证。
- `docs/测试/阶段3测试.md`：阶段 3 递归文件发现和路径处理。
- `docs/测试/阶段4测试.md`：阶段 4 大小筛选、SHA-256 和失败隔离。
- `docs/测试/阶段5测试.md`：阶段 5 重复分组、完整报告和退出码。
- `docs/测试/阶段6测试.md`：阶段 6 MVP 覆盖矩阵和健壮性回归。
- `docs/测试/阶段7测试.md`：阶段 7 扫描进度事件和控制台动态显示。
- `docs/测试/阶段8测试.md`：阶段 8 有界文件级并发和线程安全进度汇聚。
- `docs/测试/阶段9测试.md`：阶段 9 最终交付验收。
- `docs/学习/`：CMake、GoogleTest 和 Git 学习笔记。

项目对被扫描目录保持只读，不提交构建产物、IDE 私有配置或个人业务数据。
