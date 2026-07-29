#pragma once

#include <filesystem>
#include <string>
#include <system_error>

namespace batchguard {

// 保存单个文件的 SHA-256 计算结果；失败时摘要为空并携带底层错误码。
struct FileHashResult {
    std::string sha256;
    std::error_code errorCode;

    [[nodiscard]] bool isSuccess() const noexcept {
        return !errorCode;
    }
};

// 使用 Windows CNG 分块读取 `filePath` 并计算小写十六进制 SHA-256。
// 此函数只读取文件；打开、读取或 CNG 操作失败时通过 `errorCode` 返回原因。
[[nodiscard]] FileHashResult calculateFileSha256(
    const std::filesystem::path& filePath);

}
