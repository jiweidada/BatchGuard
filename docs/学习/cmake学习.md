# CMake 学习：读懂 BatchGuard 的构建配置

> 分类：学习。

本文不追求记住所有 CMake 命令，目标是让你能够读懂、修改并解释当前项目的
`CMakeLists.txt`。

## 1. CMake 在项目中负责什么

C++ 源文件不能直接变成程序，需要编译器和链接器处理。CMake 本身不是编译器，
它读取 `CMakeLists.txt`，再为 Ninja、Visual Studio 等构建工具生成规则。

```text
CMakeLists.txt
      │ cmake 配置、生成
      ▼
Ninja/Visual Studio 构建规则
      │ 编译、链接
      ▼
核心/支持静态库 + BatchGuard.exe + BatchGuardGui.exe + 测试程序
```

常用流程分为四步：

1. 配置：选择编译器、生成器和 Debug/Release。
2. 生成：CMake 写出构建规则。
3. 构建：编译 `.cpp` 并链接库和程序。
4. 运行：启动生成的 `BatchGuard.exe`。

## 2. 源码目录与构建目录

源码目录保存需要维护和提交 Git 的内容：

```text
CMakeLists.txt
apps/
include/
src/
tests/
Third_Party/
```

`cmake-build-debug/` 和 `cmake-build-release/` 是当前约定的构建目录，保存缓存、
目标文件、库、可执行程序和本机部署依赖。它们都可以由源码重新生成，不应手动
修改，也不应提交 Git。早期文档中的 `build/debug`、`build/release` 只保留为
历史记录，后续开发不要重新创建独立 `build/`。

一个构建目录只能稳定地对应一套生成器、编译器和构建配置。切换生成器时应使用
新的构建目录，不要在旧目录中混用。

## 3. 当前项目的目标关系

当前项目包含五个基础目标，并在 GUI 选项开启时增加三个目标：

```text
batchguard_cli ──────> batchguard_cli_support ──────> batchguard_core
                              │
                              └─────────────────────> batchguard_logging

batchguard_tests ────> batchguard_cli_support / batchguard_core / batchguard_logging

batchguard_gui ───────> batchguard_gui_support ──────> batchguard_core
                                      │
                                      ├──────────────> batchguard_logging
                                      └──────────────> Qt5::Widgets

batchguard_gui_tests ─> batchguard_gui_support ──────> Qt5::Test
```

`batchguard_core` 承载扫描、哈希、取消和重复分组；`batchguard_logging` 提供
标准 C++ 结构化日志；CLI/GUI 支持层负责各自应用逻辑；两个入口目标只做对象组装。
基础测试使用 GoogleTest，GUI 测试使用 Qt Test，并由 CTest 统一运行。依赖始终
从应用层指向核心层，核心库不能反向依赖 CLI、GUI 或日志输出设施。

## 4. 逐行理解当前 CMakeLists.txt

### 4.1 指定最低 CMake 版本

```cmake
cmake_minimum_required(VERSION 4.2)
```

低于 4.2 的 CMake 会拒绝配置。它声明的是构建工具版本，不是 C++ 版本。

### 4.2 定义项目和语言

```cmake
project(BatchGuard VERSION 0.2.0 LANGUAGES CXX)
```

项目名是 `BatchGuard`，版本是 `0.2.0`，只启用 C++。项目版本通过编译定义进入
CLI 的 `--version` 输出。

### 4.3 创建核心静态库

```cmake
add_library(batchguard_core STATIC
    src/core/content_fingerprint.cpp
    src/core/duplicate_group.cpp
    src/core/file_discovery.cpp
    src/core/file_hash_scheduler.cpp
    src/core/file_hasher.cpp
    src/core/input_validator.cpp
    src/core/scanner.cpp
)
```

`add_library` 创建库目标，`STATIC` 表示静态库。在 Windows/MSVC 下通常生成
`batchguard_core.lib`。括号中的 `.cpp` 是构建该目标所需的源文件。

新增核心实现时，必须把 `.cpp` 加入这个目标；公共头文件放入
`include/batchguard/core/`，私有辅助头文件可留在 `src/core/`。

### 4.4 设置公共头文件搜索路径

```cmake
target_include_directories(batchguard_core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
```

这让编译器从项目的 `include/` 开始寻找头文件，因此代码可以写：

```cpp
#include <batchguard/core/scanner.h>
```

磁盘上的完整相对路径是 `include/batchguard/core/scanner.h`。编译器已经
把 `include/` 当作搜索起点，所以 `#include` 中不再重复写 `include/`。

`CMAKE_CURRENT_SOURCE_DIR` 是当前 `CMakeLists.txt` 所在的源码目录。`PUBLIC`
表示两层含义：

- 编译 `batchguard_core` 自身时使用该头文件路径。
- 链接 `batchguard_core` 的目标也继承该路径。

常见可见性还有：

- `PRIVATE`：只给当前目标使用。
- `INTERFACE`：当前目标自身不用，只传递给使用者。

### 4.5 创建 CLI 可执行目标

```cmake
add_executable(batchguard_cli
    apps/cli/main.cpp
)
```

`add_executable` 创建可执行目标。目标名 `batchguard_cli` 用于 CMake 命令和目标
依赖，不一定等于最后生成的文件名。

### 4.6 建立目标依赖

```cmake
target_link_libraries(batchguard_cli
    PRIVATE
        batchguard_cli_support
)
```

CLI 入口只链接 `batchguard_cli_support`。支持层再以 `PUBLIC` 方式链接核心库和
日志库，因此 CMake 会按依赖顺序先构建静态库，再链接最终程序。

`PRIVATE` 表示入口自身使用支持层，但不会继续向其他消费者传播这个依赖。

### 4.7 固定 C++20

```cmake
set_target_properties(
    batchguard_core
    batchguard_logging
    batchguard_cli_support
    batchguard_cli
    PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)
```

- `CXX_STANDARD 20`：这些目标使用 C++20。
- `CXX_STANDARD_REQUIRED YES`：不允许 CMake 自动降级到旧标准。
- `CXX_EXTENSIONS NO`：关闭非标准编译器扩展，提升可移植性。

GUI 支持层和 GUI 入口也设置相同属性，避免不同目标使用不同语言标准。

### 4.8 修改最终程序名

```cmake
set_target_properties(batchguard_cli
    PROPERTIES
        OUTPUT_NAME BatchGuard
)
```

CMake 内部目标仍叫 `batchguard_cli`，最终文件名改为 `BatchGuard.exe`。因此：

```powershell
cmake --build cmake-build-debug --target batchguard_cli
.\cmake-build-debug\BatchGuard.exe
```

一个使用“目标名”，另一个使用“输出文件名”。

## 5. 如何配置和构建

### 使用 CLion

CLion 会读取 `CMakeLists.txt`，并在 `cmake-build-debug/` 中维护 Debug 构建。
修改 CMake 配置后执行“Reload CMake Project”。开发 GUI 时，Profile 还需要设置
`BATCHGUARD_BUILD_GUI=ON` 和匹配 Qt 套件的 `CMAKE_PREFIX_PATH`。

### 使用终端

先使用已经加载 MSVC 环境且能找到 Ninja 的终端，然后复用 CLion Profile 已配置
的目录：

```powershell
cmake --build cmake-build-debug
ctest --test-dir cmake-build-debug --output-on-failure
.\cmake-build-debug\BatchGuard.exe
.\cmake-build-debug\BatchGuardGui.exe
```

如果需要从命令行首次配置 Debug：

```powershell
cmake -S . -B cmake-build-debug -G Ninja `
    -DCMAKE_BUILD_TYPE=Debug `
    -DBATCHGUARD_BUILD_GUI=ON `
    -DBATCHGUARD_BUILD_TESTS=ON `
    -DCMAKE_PREFIX_PATH="<Qt 5.14.2 msvc2017_64 目录>"
```

Release 使用独立的 `cmake-build-release/`，详细配置和部署边界见
[v0.2.0部署说明.md](../交付/v0.2.0部署说明.md)：

```powershell
cmake --build cmake-build-release
ctest --test-dir cmake-build-release --output-on-failure
.\cmake-build-release\BatchGuard.exe
.\cmake-build-release\BatchGuardGui.exe
```

## 6. 修改项目时如何同步 CMake

- 只修改已有 `.cpp` 内容：直接重新构建。
- 新增 `.cpp`：把文件加入对应的 `add_library` 或 `add_executable`。
- 新增公共头文件：放到 `include/batchguard/...`；通常不必为了编译单独列出。
- 修改 `CMakeLists.txt`：重新运行配置，CLion 中重新加载 CMake。
- 移动或删除源文件：同步修改目标的源文件列表。

不要在构建目录中修代码，因为重新配置后这些内容可能被覆盖。

## 7. 配套练习

### 练习一：预测构建结果

阅读配置并回答：

1. 修改 `apps/cli/main.cpp` 后，哪个目标需要重新编译？
- [1/2] Building CXX object CMakeFiles\batchguard_cli.dir\apps\cli\main.cpp.obj
- [2/2] Linking CXX executable BatchGuard.exe
- 只有他自己

<p style="color: red;">
<strong>标准参考：</strong>重新编译 main.cpp，并重新链接 batchguard_cli；batchguard_core 没有变化，不会重新构建。
</p>

2. 修改核心库源文件后，为什么 CLI 需要重新链接？
- CLI 依赖于 Core ，core 修改之后需要重新编译，那CLI就需要重新链接

<p style="color: red;">
<strong>标准参考：</strong>核心源文件重新编译后会生成新的 batchguard_core.lib。batchguard_cli_support 公开依赖核心库，最终 BatchGuard.exe 必须重新链接才能包含最新代码；main.cpp 本身不一定重新编译。构建 GUI 或测试目标时，对应可执行程序也会按依赖关系重新链接。
</p>

3. 删除 CLI 的 `target_link_libraries` 后，入口还能继承支持层的头文件路径吗？
- 不行，入口通过支持层目标获得头文件路径和实现依赖。

<p style="color: red;">
<strong>标准参考：</strong>不能。target_link_libraries 建立了 batchguard_cli 对 batchguard_cli_support 的依赖，支持层通过 PUBLIC 公开 apps/cli 头文件路径并继续传播核心依赖。删除后，main.cpp 无法获得 cli_app.h 的包含路径，链接时也没有 CLI 应用层实现。
</p>

### 练习二：观察增量构建

1. 先完整构建一次。
2. 修改 `apps/cli/main.cpp` 中的入口代码。
3. 再次执行 `cmake --build`。
4. 观察构建工具只处理发生变化的目标，而不是全部重建。
- 按照依赖顺序只有main 会重新构建

<p style="color: red;">
<strong>标准参考：</strong>修改 main.cpp 后，Ninja 只重新编译 main.cpp，再重新链接 batchguard_cli；核心库不变，因此不会重新编译或重新生成。这里要区分“编译源文件”和“链接目标”。
</p>

### 练习三：添加一个源文件

新建 `src/core/example.cpp`，将它加入 `batchguard_core` 的源文件列表，重新配置并
构建。练习结束后删除该文件，并同步删除 CMake 中对应的一行。
- [1/3] Building CXX object CMakeFiles\batchguard_core.dir\src\core\example.cpp.obj
- [2/3] Linking CXX static library batchguard_core.lib
- [3/3] Linking CXX executable BatchGuard.exe

<p style="color: red;">
<strong>标准参考：</strong>example.cpp 被编译后，batchguard_core.lib 需要重新生成。构建全部目标时，依赖核心库的 CLI、GUI 和测试可执行程序会按需重新链接；未变化的现有源文件不会重复编译。
</p>

## 8. 常见问题

### 为什么修改 CMake 后没有生效？

可能仍在使用旧构建目录或没有重新配置。先确认实际构建路径，再重新运行
`cmake -S ... -B ...`。

### 为什么出现生成器不一致？

同一个构建目录曾被不同生成器使用。为新生成器换一个目录，或者确认无需保留后
删除旧构建目录再配置。

### 构建目录可以删除吗？

可以。删除只会丢失缓存和编译产物，不会删除源码。下次配置和构建会重新生成。

## 9. 学习完成标准

你能够不查资料说明以下问题，就算掌握了当前阶段所需的 CMake：

- CMake 与编译器、Ninja 的职责分别是什么。
- cmake 是阅读 cmakelist.txt  为Ninja 配置构建规则 ，编译器按照规则构建

<p style="color: red;">
<strong>标准参考：</strong>CMake 读取 CMakeLists.txt 并生成 Ninja 构建规则；Ninja 判断哪些步骤需要执行并调用工具；编译器把 .cpp 编译成 .obj，链接器再把目标文件和库组合成 .lib 或 .exe。
</p>

- `add_library`、`add_executable`、`target_link_libraries` 分别做什么。
- add_library 应该是生成 .lib
- add_executable 生成 .exe
- target_link_libraries 声明 batchguard_cli 对 batchguard_cli_support 等目标依赖

<p style="color: red;">
<strong>标准参考：</strong>add_library 定义库目标及其源文件，构建时生成 .lib；add_executable 定义可执行目标及其源文件，构建时生成 .exe；target_link_libraries 声明目标之间的链接和依赖关系。
</p>

- `PUBLIC` 和 `PRIVATE` 如何影响依赖传播。

<p style="color: red;">
<strong>标准参考：</strong>PRIVATE 只供当前目标使用，不传递给依赖者；PUBLIC 既供当前目标使用，也传递给依赖者；INTERFACE 只传递给依赖者，当前目标自身不使用。
</p>

- 为什么公共头文件在 `include/batchguard/` 下。

<p style="color: red;">
<strong>标准参考：</strong>include 是公共头文件根目录，batchguard 是项目专属前缀，可以避免与其他库的同名头文件冲突，并形成稳定的包含路径，例如 &lt;batchguard/core/scanner.h&gt;。
</p>

- 如何新增 `.cpp`，并让它进入正确目标。
先确认源码属于核心、日志、CLI 支持层、GUI 支持层还是入口目标，再加入对应目标。

<p style="color: red;">
<strong>标准参考：</strong>核心实现加入 add_library(batchguard_core ...)；CLI 应用实现加入 batchguard_cli_support；GUI 窗口或模型实现加入 batchguard_gui_support；只有程序入口或平台入口代码才加入相应 add_executable。重新配置后，CMake 才会把新文件纳入正确目标。
</p>

- 如何独立配置、构建和运行 Debug/Release。

<p style="color: red;">
<strong>标准参考：</strong>Debug 和 Release 使用 cmake-build-debug 与 cmake-build-release 两个独立目录。Profile 配置完成后分别执行 cmake --build 和 ctest --test-dir；GUI 目标还需要 BATCHGUARD_BUILD_GUI=ON 和匹配 Qt 套件的 CMAKE_PREFIX_PATH。
</p>
