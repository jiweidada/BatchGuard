#pragma once

#include <filesystem>
#include <system_error>

namespace batchguard {

// 标识文件处理失败时所处的阶段。
enum class FailureStage {
    Discovery
};

// 保存单个路径的失败信息，供应用层统一转换为用户可理解的提示。
struct FileFailure {
    std::filesystem::path path;
    FailureStage stage{FailureStage::Discovery};
    std::error_code errorCode;
};

}
