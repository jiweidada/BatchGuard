#pragma once

#include <filesystem>
#include <system_error>

namespace batchguard {

// 标识输入路径不能作为扫描根目录的原因。
enum class InputValidationError {
    None,
    EmptyPath,
    CannotAccess,
    DoesNotExist,
    NotDirectory
};

// 描述输入验证结果。仅当操作系统阻止 BatchGuard 查询路径时，
// `errorCode` 才包含具体的系统错误。
struct InputValidationResult {
    InputValidationError error{InputValidationError::None};
    std::error_code errorCode;

    [[nodiscard]] bool isValid() const noexcept {
        return error == InputValidationError::None;
    }
};

// 验证 `directoryPath` 不为空、确实存在且表示目录。
// 此函数只查询路径元数据，不扫描或修改目录。
[[nodiscard]] InputValidationResult validateInputDirectory(
    const std::filesystem::path& directoryPath);

}
