# Repository Guidelines

## 项目结构与模块组织

BatchGuard 是一个 C++20/CMake 项目：`apps/cli/main.cpp` 是程序入口，
`batchguard_core` 是静态核心库，`batchguard_cli` 生成 `BatchGuard.exe`，
`batchguard_cli_support` 封装可测试的 CLI 应用层，`batchguard_tests` 承载
GoogleTest 测试。`docs/` 保存设计、学习和测试文档。

开始新的开发会话或切换阶段前，必须先阅读 `docs/交接文档.md`，核对当前阶段、
Git 状态、未提交文件和下一步边界。每个阶段实现、验证或提交状态发生变化后，
同步更新该交接文档，避免依赖聊天记录传递上下文。

项目扩展时遵循 `docs/设计/项目结构.md`：公共核心头文件放入
`include/batchguard/core/`，实现放入 `src/core/`，命令行代码放入
`apps/cli/`，测试分别放入 `tests/unit/` 和 `tests/integration/`。核心模块
不得依赖 CLI 或未来的 GUI，也不要在 `main.cpp` 中堆积业务逻辑。

第三方源码统一放入 `Third_Party/<library>/`，由 CMake 从本地目录接入，避免
不同构建目录重复下载。保留上游许可证和版本记录，不直接修改第三方源码；依赖
升级必须使用独立提交。

## 构建、测试与本地开发

本机开发统一使用 CLion CMake Profile，不再另外创建 `build/`。当前 v0.2.0
开发阶段只维护 `cmake-build-debug/`；Release 配置、部署和完整验收延后到交付
阶段。开发 Qt GUI 时，Debug Profile 需要设置 `BATCHGUARD_BUILD_GUI=ON`，并通过
`CMAKE_PREFIX_PATH` 指向与 MSVC 匹配的 Qt 5.14.2 `msvc2017_64` 套件。

- `cmake --build cmake-build-debug`：构建 Debug 程序和测试。
- `.\cmake-build-debug\BatchGuard.exe`：运行 Debug CLI。
- `.\cmake-build-debug\BatchGuardGui.exe`：运行 Debug GUI。
- `ctest --test-dir cmake-build-debug --output-on-failure`：运行 Debug 测试。

不要提交构建产物或个人 IDE 设置。

## 编码风格与命名约定

统一使用 C++20 和 UTF-8；缩进为 4 个空格，禁止 Tab，单行建议不超过
100 个字符。左花括号与声明同行，头文件使用 `#pragma once`。优先采用
RAII、值语义、`enum class`、显式单参数构造函数，并用 `std::error_code`
处理可恢复的文件系统错误。

类型使用 `PascalCase`，函数、参数和局部变量使用 `lowerCamelCase`，私有
成员使用 `lowerCamelCase_`，常量使用 `kPascalCase`，命名空间和文件名
使用 `snake_case`。布尔值以 `is`、`has`、`can` 等开头。完整规则见
`docs/设计/代码规范.md`；项目目前尚未配置自动格式化工具。

项目自有代码的注释必须使用中文，并遵循 Google C++ Style Guide 的 Comments
章节。公共接口和非显然的类型必须说明用途、输入、输出及约束；实现注释重点
解释关键步骤和设计原因，不得逐行复述代码。统一优先使用 `//`，注释采用语法
完整、标点正确的中文句子，并与代码同步更新。`TODO` 必须带 Issue、负责人或
其他可追踪标识。第三方源码保持上游原貌，不翻译或修改其注释。

## 测试规范

核心逻辑必须能够脱离 CLI 测试。测试文件命名为 `<module>_test.cpp`，每个
测试只验证一种行为。文件系统测试应使用临时目录和运行时生成的数据，
不得依赖测试顺序、固定盘符或个人路径。修复缺陷前，先添加可复现问题的
测试。

## 提交与合并请求

提交信息采用 `<类型>: <简短说明>`，现有类型包括 `feat`、`docs`、`build` 和
`chore`，例如 `feat: 添加目录输入验证`。每个提交只表达一个目的；未经讨论
不要新增第三方依赖。

合并请求需说明目标、影响文件、设计理由及验证命令和结果，并关联相关
Issue。只有涉及可见界面变化时才需要截图。

## 安全与配置

不要提交凭据、敏感路径、公司代码、图片、模型或业务数据。日志不得泄露
敏感文件路径或密钥。
