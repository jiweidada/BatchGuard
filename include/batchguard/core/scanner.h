#pragma once

#include "batchguard/core/scan_report.h"

#include <filesystem>

namespace batchguard {

// 对已经通过输入验证的目录执行发现、元数据读取、候选哈希和重复分组。
// 预期文件系统失败进入返回报告；函数不会修改扫描目录中的任何内容。
[[nodiscard]] ScanReport scanDirectory(const std::filesystem::path& rootPath);

}
