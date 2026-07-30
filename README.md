# BatchGuard

BatchGuard 0.1.0 是一个使用 C++20 开发的 Windows 本地重复文件检查工具，支持
单目录输入验证、递归普通文件发现、并发 SHA-256 内容指纹、重复分组、中文扫描
报告和控制台动态进度。程序只读取输入目录，不删除、移动或修改文件。

当前 v0.2.0 Debug 开发版还提供 Qt Widgets GUI：可选择目录、设置哈希线程数、
在后台扫描和安全取消，并查看结论摘要、重复组、完整路径、处理失败详情和简化
运行日志。

## 项目结构

```text
apps/cli/                  命令行入口
apps/gui/                  Qt 主窗口、控制器、后台执行和结果模型
include/batchguard/core/   核心库公共头文件
include/batchguard/logging/ 结构化日志公共头文件
src/core/                  核心库实现
src/logging/               日志格式化和终端输出实现
tests/                     GoogleTest 测试
Third_Party/               固定版本第三方源码
docs/                      设计、学习和测试文档
```

`batchguard_cli` 链接 `batchguard_core`，最终程序名为 `BatchGuard.exe`。
启用 GUI 后还会生成 `BatchGuardGui.exe`。

## 环境要求

- Windows 10 或更高版本
- 支持 C++20 的 MSVC
- CMake 4.2 或更高版本
- Ninja
- 构建 GUI 时需要 Qt 5.14.2 `msvc2017_64`

GoogleTest 1.17.0 已保存在 `Third_Party/googletest/`，配置构建时不需要重复下载。

## 使用 CLion

1. 打开项目根目录。
2. Debug Profile 使用 `cmake-build-debug`，不要另外创建 `build/`。
3. 开发 GUI 时添加 `-DBATCHGUARD_BUILD_GUI=ON`，并通过
   `CMAKE_PREFIX_PATH` 指向本机 Qt 5.14.2 `msvc2017_64` 目录。
4. 执行 **Reload CMake Project**。
5. 运行 GUI 时选择 CMake 目标 `batchguard_gui`，不要使用单文件 `main.cpp` 配置。
6. 打开 `tests/` 下的测试文件，点击测试左侧绿色按钮运行 GTest。

## 使用 CLion 构建目录

CLion 完成 Profile 配置后，也可以在已加载 MSVC 环境的终端复用同一构建目录。

Debug：

```powershell
cmake --build cmake-build-debug
ctest --test-dir cmake-build-debug --output-on-failure
.\cmake-build-debug\BatchGuard.exe --help
.\cmake-build-debug\BatchGuardGui.exe
```

当前 v0.2.0 开发阶段只维护 Debug Profile。Release 配置、部署和完整验收延后到
交付阶段，不要求本地持续保留 `cmake-build-release/`。

## 使用方式

GUI 直接运行：

```powershell
.\cmake-build-debug\BatchGuardGui.exe
```

选择有效目录后即可开始扫描。结果中的“理论可节省”是假设每个重复组只保留一个
文件的逻辑大小估算，未考虑硬链接、稀疏文件和文件系统压缩；GUI 不会执行清理。
GUI 日志面板只显示简化状态并保留最近 500 条。从已有 Windows 终端启动 GUI 时，
同一批记录还会在父终端显示完整字段；从资源管理器启动不会额外创建控制台窗口。

CLI 使用方式：

```text
BatchGuard <目录路径>
BatchGuard --hash-workers <1-64> <目录路径>
BatchGuard --help
BatchGuard --version
```

包含空格的路径需要使用双引号，例如：

```powershell
.\cmake-build-debug\BatchGuard.exe "E:\测试数据\待整理目录"
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
完整运行日志固定写入标准错误流，格式为
`时间：信息类型：所属层：信息内容`，不会记录输入目录或文件完整路径。

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
ctest --test-dir cmake-build-debug --output-on-failure
```

直接运行 GoogleTest：

```powershell
.\cmake-build-debug\tests\batchguard_tests.exe --gtest_color=no
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
- `docs/测试/阶段10测试.md` 至 `docs/测试/阶段16测试.md`：Qt 构建、取消、
  GUI 状态、后台扫描、结果展示、自动化补强和结构化日志。
- `docs/学习/`：CMake、GoogleTest 和 Git 学习笔记。

项目对被扫描目录保持只读，不提交构建产物、IDE 私有配置或个人业务数据。
