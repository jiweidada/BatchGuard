#pragma once

#include "batchguard/core/scan_progress.h"
#include "batchguard/logging/logging.h"

#include <iosfwd>
#include <string>
#include <vector>

namespace batchguard::cli {

// 定义 CLI 流程使用的进程退出码。
enum class ExitCode {
    Success = 0,
    InvalidInput = 1,
    PartialFailure = 2,
    InternalError = 3
};

// 使用不包含可执行文件名的参数运行 CLI 流程。
// 正常信息写入 `output`，验证错误写入 `error`，并返回对应的 `ExitCode`。
[[nodiscard]] int runCli(
    const std::vector<std::wstring>& arguments,
    std::wostream& output,
    std::wostream& error,
    const ScanProgressCallback& progressCallback = {},
    const LogCallback& logCallback = {});

}
