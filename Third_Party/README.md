# Third-Party Dependencies

本目录集中保存 BatchGuard 的第三方源码。CMake 从这里直接接入依赖，避免
Debug、Release 和 CLion 构建目录分别下载相同源码。

## 当前依赖

| 目录 | 版本 | 固定提交 | 许可证 | 用途 |
|---|---|---|---|---|
| `googletest/` | 1.17.0 | `52eb8108c5bdec04579160ae17225d66034bd723` | BSD-3-Clause | 测试 |

GoogleTest 上游：

```text
https://github.com/google/googletest
```

许可证文件：

```text
googletest/LICENSE
```

## 管理规则

- 第三方源码和许可证随项目提交，构建产物不提交。
- 项目代码不得直接修改第三方源码。
- CMake 使用 `add_subdirectory(Third_Party/<library>)` 接入。
- 新增依赖前评估必要性、许可证、体积和部署成本。
- 升级依赖时固定完整提交哈希，并使用独立 Git 提交。
- 升级后必须重新运行 Debug、Release 和全部 CTest。

每个构建目录会按自己的配置编译依赖，但不会重新下载源码。
