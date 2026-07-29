#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
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

// 接收当前文件已经完成哈希的累计字节数。
using FileHashProgressCallback = std::function<void(std::uintmax_t)>;

// 使用 Windows CNG 分块读取 `filePath` 并计算小写十六进制 SHA-256。
// 此函数只读取文件；打开、读取或 CNG 操作失败时通过 `errorCode` 返回原因。
[[nodiscard]] FileHashResult calculateFileSha256(
    const std::filesystem::path& filePath);

// 计算 SHA-256，并在每个读取块完成后同步报告当前文件的累计字节进度。
[[nodiscard]] FileHashResult calculateFileSha256(
    const std::filesystem::path& filePath,
    const FileHashProgressCallback& progressCallback);

}
