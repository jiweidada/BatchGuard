# Repository Guidelines

## 项目结构与模块组织

BatchGuard 目前是一个最小化的 C++20/CMake 项目：`main.cpp` 是程序入口，
`CMakeLists.txt` 定义 `BatchGuard` 可执行目标，`docs/` 保存项目目标、MVP、
结构和代码规范。`cmake-build-debug/` 属于生成目录，不应提交。

项目扩展时遵循 `docs/项目结构.md`：公共核心头文件放入
`include/batchguard/core/`，实现放入 `src/core/`，命令行代码放入
`apps/cli/`，测试分别放入 `tests/unit/` 和 `tests/integration/`。核心模块
不得依赖 CLI 或未来的 GUI，也不要在 `main.cpp` 中堆积业务逻辑。

## 构建、测试与本地开发

- `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug`：配置源码外 Debug 构建。
- `cmake --build build`：编译当前配置。
- `.\build\BatchGuard.exe`：在 Windows 上运行 Ninja 构建产物。
- `cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release`：配置 Release 构建。
- `ctest --test-dir build --output-on-failure`：运行 CTest；当前尚未配置测试目标。

不要提交构建产物或个人 IDE 设置。

## 编码风格与命名约定

统一使用 C++20 和 UTF-8；缩进为 4 个空格，禁止 Tab，单行建议不超过
100 个字符。左花括号与声明同行，头文件使用 `#pragma once`。优先采用
RAII、值语义、`enum class`、显式单参数构造函数，并用 `std::error_code`
处理可恢复的文件系统错误。

类型使用 `PascalCase`，函数、参数和局部变量使用 `lowerCamelCase`，私有
成员使用 `lowerCamelCase_`，常量使用 `kPascalCase`，命名空间和文件名
使用 `snake_case`。布尔值以 `is`、`has`、`can` 等开头。完整规则见
`docs/代码规范.md`；项目目前尚未配置自动格式化工具。

## 测试规范

核心逻辑必须能够脱离 CLI 测试。测试文件命名为 `<module>_test.cpp`，每个
测试只验证一种行为。文件系统测试应使用临时目录和运行时生成的数据，
不得依赖测试顺序、固定盘符或个人路径。修复缺陷前，先添加可复现问题的
测试。

## 提交与合并请求

当前 `.git` 中没有可用历史，无法归纳既有提交规范。提交标题应简短并使用
祈使语气，例如 `Add directory scanner`。每次改动尽量限制在 1～3 个文件；
未经讨论不要新增第三方依赖。

合并请求需说明目标、影响文件、设计理由及验证命令和结果，并关联相关
Issue。只有涉及可见界面变化时才需要截图。

## 安全与配置

不要提交凭据、敏感路径、公司代码、图片、模型或业务数据。日志不得泄露
敏感文件路径或密钥。
