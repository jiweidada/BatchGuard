# CMake 学习：读懂 BatchGuard 的构建配置

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
batchguard_core.lib + BatchGuard.exe
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
```

`cmake-build-debug/` 和 `build/` 是构建目录，保存缓存、目标文件、库和可执行
程序。它们都可以由源码重新生成，不应手动修改，也不应提交 Git。

一个构建目录只能稳定地对应一套生成器、编译器和构建配置。切换生成器时应使用
新的构建目录，不要在旧目录中混用。

## 3. 当前项目的目标关系

当前项目包含两个 CMake 目标：

```text
batchguard_cli
      │ 链接
      ▼
batchguard_core
```

- `batchguard_core` 是静态库，未来承载文件扫描、哈希和重复分组等核心逻辑。
- `batchguard_cli` 是命令行程序，只负责程序入口和终端交互。
- CLI 可以依赖核心库，核心库不能反向依赖 CLI。

## 4. 逐行理解当前 CMakeLists.txt

### 4.1 指定最低 CMake 版本

```cmake
cmake_minimum_required(VERSION 4.2)
```

低于 4.2 的 CMake 会拒绝配置。它声明的是构建工具版本，不是 C++ 版本。

### 4.2 定义项目和语言

```cmake
project(BatchGuard LANGUAGES CXX)
```

项目名是 `BatchGuard`，只启用 C++。不启用未使用的 C 语言可以减少不必要的
编译器检测。

### 4.3 创建核心静态库

```cmake
add_library(batchguard_core STATIC
    src/core/batchguard_core.cpp
)
```

`add_library` 创建库目标，`STATIC` 表示静态库。在 Windows/MSVC 下通常生成
`batchguard_core.lib`。括号中的 `.cpp` 是构建该目标所需的源文件。

以后新增核心实现，例如 `src/core/file_scanner.cpp`，必须把它加入该目标：

```cmake
add_library(batchguard_core STATIC
    src/core/batchguard_core.cpp
    src/core/file_scanner.cpp
)
```

### 4.4 设置公共头文件搜索路径

```cmake
target_include_directories(batchguard_core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
```

这让编译器从项目的 `include/` 开始寻找头文件，因此代码可以写：

```cpp
#include <batchguard/core/batchguard_core.h>
```

磁盘上的完整相对路径是 `include/batchguard/core/batchguard_core.h`。编译器已经
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
        batchguard_core
)
```

CLI 链接核心静态库。`PRIVATE` 表示这个依赖只属于 CLI，不需要继续传递给其他
目标。CMake 会确保先构建核心库，再链接 CLI。

因为核心库的头文件目录是 `PUBLIC`，CLI 链接核心库后也能包含其公共头文件。

### 4.7 固定 C++20

```cmake
set_target_properties(
    batchguard_core
    batchguard_cli
    PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)
```

- `CXX_STANDARD 20`：两个目标使用 C++20。
- `CXX_STANDARD_REQUIRED YES`：不允许 CMake 自动降级到旧标准。
- `CXX_EXTENSIONS NO`：关闭非标准编译器扩展，提升可移植性。

这些属性同时应用于两个目标，避免核心库和 CLI 使用不同语言标准。

### 4.8 修改最终程序名

```cmake
set_target_properties(batchguard_cli
    PROPERTIES
        OUTPUT_NAME BatchGuard
)
```

CMake 内部目标仍叫 `batchguard_cli`，最终文件名改为 `BatchGuard.exe`。因此：

```powershell
cmake --build build/debug --target batchguard_cli
.\build\debug\BatchGuard.exe
```

一个使用“目标名”，另一个使用“输出文件名”。

## 5. 如何配置和构建

### 使用 CLion

CLion 会读取 `CMakeLists.txt`，并在 `cmake-build-debug/` 中维护 Debug 构建。
修改 CMake 配置后执行“Reload CMake Project”，再构建 `batchguard_cli`。

### 使用终端

先使用已经加载 MSVC 环境且能找到 Ninja 的终端，然后执行：

```powershell
cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug
.\build\debug\BatchGuard.exe
```

参数含义：

- `-S .`：源码目录是当前目录。
- `-B build/debug`：把生成文件放到指定构建目录。
- `-G Ninja`：使用 Ninja 生成器。
- `-DCMAKE_BUILD_TYPE=Debug`：生成便于调试的版本。

Release 使用独立目录：

```powershell
cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/release
.\build\release\BatchGuard.exe
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
2. 修改核心库源文件后，为什么 CLI 需要重新链接？
3. 删除 `target_link_libraries` 后，CLI 是否还能继承核心库的公共头文件路径？

### 练习二：观察增量构建

1. 先完整构建一次。
2. 将启动文字改为 `BatchGuard stage 1.`。
3. 再次执行 `cmake --build`。
4. 观察构建工具只处理发生变化的目标，而不是全部重建。

### 练习三：添加一个源文件

新建 `src/core/example.cpp`，将它加入 `batchguard_core` 的源文件列表，重新配置并
构建。练习结束后删除该文件，并同步删除 CMake 中对应的一行。

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
- `add_library`、`add_executable`、`target_link_libraries` 分别做什么。
- `PUBLIC` 和 `PRIVATE` 如何影响依赖传播。
- 为什么公共头文件在 `include/batchguard/` 下。
- 如何新增 `.cpp`，并让它进入正确目标。
- 如何独立配置、构建和运行 Debug/Release。
