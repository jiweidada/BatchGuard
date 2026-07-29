# GoogleTest 学习：BatchGuard 测试体系

> 分类：学习。

> 目标：理解 GoogleTest、CTest、CMake 和 CLion 如何协作，并为每个开发阶段
> 留下一份可复查的测试文档。

## 1. 四个工具分别做什么

```text
GoogleTest 测试代码
        │ 编译
        ▼
batchguard_tests.exe
        │ gtest_discover_tests
        ▼
CTest 测试清单
        │ 运行
        ▼
CLion 测试窗口 / 终端结果
```

- GoogleTest：提供 `TEST`、`EXPECT_EQ` 等 C++ 测试宏和测试运行器。
- CMake：接入、编译并链接本地 GoogleTest，定义 `batchguard_tests` 目标。
- CTest：统一发现和执行测试，汇总通过与失败结果。
- CLion：提供运行、调试、过滤和查看测试结果的图形界面。

CLion 让操作更方便，但测试仍由同一个 `batchguard_tests.exe` 执行。

## 2. BatchGuard 的版本选择

第一版建议固定使用 GoogleTest 1.17.0，对应提交：

```text
52eb8108c5bdec04579160ae17225d66034bd723
```

固定完整提交哈希可以保证不同时间和不同机器获取相同代码。GoogleTest 1.17.0
要求至少 C++17，BatchGuard 使用 C++20，满足要求。

GoogleTest 使用 BSD-3-Clause 许可证，只作为构建和测试依赖，不进入
`BatchGuard.exe` 的产品运行逻辑。

## 3. 为什么使用 Third_Party 本地源码

GoogleTest 源码统一保存在：

```text
Third_Party/googletest/
```

项目只下载一次，之后所有 Debug、Release 和 CLion 构建目录都引用同一份源码：

- 重新配置和新建构建目录时不再访问网络。
- 固定版本，构建结果可复现。
- 第三方依赖集中管理，许可证容易检查。
- CMake 配置比本机全局安装更容易复用。

每个构建目录仍会分别编译与自身配置匹配的 GTest 库，这是 Debug/Release 二进制
不兼容所必需的；避免的是重复下载源码，不是复用不同配置的编译产物。

`Third_Party/googletest` 作为 vendored 源码提交 Git，保留上游 `LICENSE`。项目
代码不得直接修改第三方源码；升级必须使用独立提交并更新版本记录。

## 4. 根 CMakeLists.txt 当前配置

```cmake
option(BATCHGUARD_BUILD_TESTS "Build BatchGuard tests" ON)

if(BATCHGUARD_BUILD_TESTS)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Third_Party/googletest/CMakeLists.txt")
        message(FATAL_ERROR
            "GoogleTest is missing. Expected Third_Party/googletest/CMakeLists.txt"
        )
    endif()

    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    add_subdirectory(Third_Party/googletest EXCLUDE_FROM_ALL)

    enable_testing()
    add_subdirectory(tests)
endif()
```

各命令作用：

- `option`：允许使用者打开或关闭测试构建。
- `if(NOT EXISTS ...)`：依赖缺失时给出明确错误，不等到编译时才失败。
- `gtest_force_shared_crt`：让 Windows/MSVC 下 GTest 与主项目使用一致的运行库。
- `add_subdirectory(Third_Party/googletest ...)`：把本地 GTest 目标加入构建。
- `EXCLUDE_FROM_ALL`：默认不单独构建全部 GTest 目标，只构建测试实际依赖的部分。
- `enable_testing`：启用 CTest。
- `add_subdirectory(tests)`：读取测试目录的 CMake 配置。

关闭测试：

```powershell
cmake -S . -B build/release -DBATCHGUARD_BUILD_TESTS=OFF
```

## 5. tests/CMakeLists.txt 当前配置

```cmake
add_executable(batchguard_tests
    stage1/smoke_test.cpp
)

target_link_libraries(batchguard_tests
    PRIVATE
        batchguard_core
        GTest::gtest_main
)

set_target_properties(batchguard_tests
    PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)

include(GoogleTest)
gtest_discover_tests(batchguard_tests)
```

- `GTest::gtest_main` 提供 GoogleTest 实现和测试程序入口，不需要自己写 `main`。
- `batchguard_core` 是被测试目标。
- `gtest_discover_tests` 运行测试程序获取测试清单，并向 CTest 注册每个测试。

随着阶段推进，把新的测试源文件加入 `batchguard_tests`：

```cmake
add_executable(batchguard_tests
    stage1/smoke_test.cpp
    unit/input_validator_test.cpp
    unit/file_discovery_test.cpp
)
```

## 6. 第一个测试

```cpp
#include <gtest/gtest.h>

TEST(Stage1SmokeTest, UsesCxx20) {
    EXPECT_GE(__cplusplus, 202002L);
}
```

命名建议：

```text
TEST(被测模块或场景, 预期行为)
```

例如：

```cpp
TEST(InputValidatorTest, RejectsMissingPath) {
    // Arrange：准备输入
    // Act：调用被测函数
    // Assert：验证结果
}
```

测试名称使用清晰的英文标识符，避免模糊的 `Test1`、`Works`。

## 7. Arrange、Act、Assert

每个测试按三个步骤组织：

```cpp
TEST(CandidateGroupingTest, GroupsFilesWithEqualSize) {
    // Arrange
    const auto records = makeRecordsWithSizes({10, 20, 10});

    // Act
    const auto groups = groupCandidatesBySize(records);

    // Assert
    ASSERT_EQ(groups.size(), 1U);
    EXPECT_EQ(groups.front().size(), 2U);
}
```

- Arrange：准备输入和环境。
- Act：只执行一次主要行为。
- Assert：验证结果，不重新实现业务算法。

## 8. EXPECT 与 ASSERT

| 断言 | 失败后行为 | 使用场景 |
|---|---|---|
| `EXPECT_*` | 当前测试继续 | 后续检查仍然有意义 |
| `ASSERT_*` | 当前测试立即结束 | 前置条件失败后无法安全继续 |

常用断言：

```cpp
EXPECT_TRUE(value);
EXPECT_FALSE(value);
EXPECT_EQ(actual, expected);
EXPECT_NE(actual, unexpected);
EXPECT_LT(left, right);
EXPECT_STREQ(actualText, expectedText);
EXPECT_THROW(expression, ExceptionType);
EXPECT_NO_THROW(expression);
```

示例：

```cpp
ASSERT_FALSE(groups.empty());
EXPECT_EQ(groups.front().members.size(), 2U);
```

先用 `ASSERT_FALSE` 保证集合非空，才能安全访问 `front()`。

## 9. 测试夹具

多个测试共享初始化和清理逻辑时使用 `TEST_F`：

```cpp
class TemporaryDirectoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建唯一临时目录
    }

    void TearDown() override {
        // 清理临时目录
    }
};

TEST_F(TemporaryDirectoryTest, DiscoversNestedFile) {
    // 使用夹具准备的临时目录
}
```

BatchGuard 的文件测试需要一个 RAII 临时目录辅助类型。即使测试断言失败，也必须
自动清理测试数据。

## 10. 构建与运行

首次构建会从 `Third_Party/googletest` 编译 GoogleTest，但不会下载源码：

```powershell
cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug --target batchguard_tests
```

通过 CTest 运行全部测试：

```powershell
ctest --test-dir build/debug --output-on-failure
```

直接运行 GTest 程序：

```powershell
.\build\debug\batchguard_tests.exe
```

列出测试：

```powershell
.\build\debug\batchguard_tests.exe --gtest_list_tests
```

运行一个测试套件：

```powershell
.\build\debug\batchguard_tests.exe --gtest_filter=InputValidatorTest.*
```

运行单个测试：

```powershell
.\build\debug\batchguard_tests.exe --gtest_filter=InputValidatorTest.RejectsMissingPath
```

重复运行：

```powershell
.\build\debug\batchguard_tests.exe --gtest_repeat=10
```

随机顺序用于发现隐藏的顺序依赖：

```powershell
.\build\debug\batchguard_tests.exe --gtest_shuffle
```

## 11. 在 CLion 中使用

当前 CMake 已完成接入，在 CLion 中：

1. 执行 Reload CMake Project。
2. 打开测试源文件。
3. 点击 `TEST` 左侧绿色运行图标。
4. 选择 Run 运行或 Debug 单步调试。
5. 在测试树中查看每个用例的耗时和失败位置。
6. 右键测试目录或 `batchguard_tests` 运行全部测试。

CLion 运行单个测试时，本质上会为测试程序添加 `--gtest_filter`。

## 12. 每阶段一份测试文档

测试文档统一放入：

```text
docs/测试/
```

命名规则：

```text
阶段1测试.md
阶段2测试.md
阶段3测试.md
...
```

每份文档只在对应阶段开始后创建，并在阶段验收时填写真实结果。不要提前创建一批
空文档。

固定结构：

```markdown
# BatchGuard 阶段 N 测试

## 1. 测试目标
## 2. 变更范围
## 3. 测试环境
## 4. 测试用例
## 5. 执行命令
## 6. 实际结果
## 7. 已知问题
## 8. 阶段结论
```

测试用例表至少包含：

| 字段 | 含义 |
|---|---|
| 编号 | 阶段内稳定编号，例如 S2-01 |
| 场景 | 输入和前置条件 |
| 预期 | 应产生的行为 |
| 方式 | GTest、CTest 或人工检查 |
| 结果 | 通过、失败、阻塞、未执行 |

“计划执行”不能写成“通过”。实际结果应记录日期、构建类型和失败原因。

## 13. 各阶段测试重点

| 阶段 | 测试文档 | 重点 |
|---:|---|---|
| 1 | `阶段1测试.md` | CMake 目标、Debug/Release、启动输出 |
| 2 | `阶段2测试.md` | 参数、帮助、版本、输入验证 |
| 3 | `阶段3测试.md` | 空目录、嵌套目录、中文路径、错误隔离 |
| 4 | `阶段4测试.md` | 大小筛选、SHA-256、零字节、读取失败 |
| 5 | `阶段5测试.md` | 重复组、报告、排序、退出码 |
| 6 | `阶段6测试.md` | M-01 至 M-11 完整回归 |
| 7 | `阶段7测试.md` | 新环境构建、Release 交付、文档可用性 |

阶段内每个功能增量都同步增加或更新测试；阶段测试文档汇总这些自动化结果，不用
文档代替测试代码。

## 14. 测试质量规则

- 一个测试只验证一个主要行为。
- 测试名称说明场景和预期。
- 不依赖执行顺序。
- 不使用固定盘符。
- 不读取个人或公司业务文件。
- 不通过等待固定时间制造稳定性。
- 缺陷修复前先增加可复现测试。
- 测试失败时保留足够上下文，但不泄露敏感数据。
- Debug、Release 都必须运行 CTest。

## 15. 学习完成标准

你能够独立完成以下操作，就掌握了第一版需要的 GTest：

- 解释 GoogleTest、CTest、CMake 和 CLion 的分工。
- 使用 `TEST` 编写 Arrange、Act、Assert。
- 正确选择 `EXPECT_*` 和 `ASSERT_*`。
- 把测试源文件加入 `batchguard_tests`。
- 在 CLion 和终端运行单个或全部测试。
- 看懂失败信息并定位断言。
- 使用临时目录构造文件场景。
- 为每个阶段填写真实测试结果和结论。

## 16. 官方参考

- [GoogleTest CMake 快速入门](https://google.github.io/googletest/quickstart-cmake.html)
- [GoogleTest 用户指南](https://google.github.io/googletest/)
- [CMake GoogleTest 模块](https://cmake.org/cmake/help/latest/module/GoogleTest.html)
- [GoogleTest 1.17.0 发布页](https://github.com/google/googletest/releases/tag/v1.17.0)
