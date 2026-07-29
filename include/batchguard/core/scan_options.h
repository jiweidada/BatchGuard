#pragma once

#include <cstddef>

namespace batchguard {

// 配置一次目录扫描使用的性能参数。
// `hashWorkerCount` 为零时自动选择有上限的哈希工作线程数。
struct ScanOptions {
    std::size_t hashWorkerCount{};
};

}
