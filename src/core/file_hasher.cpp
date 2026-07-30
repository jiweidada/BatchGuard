#include "batchguard/core/file_hasher.h"

#include <Windows.h>
#include <bcrypt.h>

#include <array>
#include <cstddef>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

namespace batchguard {
namespace {

constexpr std::size_t kReadBufferSize = 64U * 1024U;

// 将 CNG 的原始 NTSTATUS 保存在 `std::error_code` 中，避免丢失底层失败值。
class BCryptErrorCategory final : public std::error_category {
public:
    [[nodiscard]] const char* name() const noexcept override {
        return "bcrypt";
    }

    [[nodiscard]] std::string message(int condition) const override {
        std::ostringstream message;
        message << "BCrypt NTSTATUS 0x"
                << std::hex
                << std::uppercase
                << static_cast<unsigned int>(condition);
        return message.str();
    }
};

const std::error_category& bcryptErrorCategory() {
    static const BCryptErrorCategory category;
    return category;
}

std::error_code makeBCryptError(NTSTATUS status) {
    return {
        static_cast<int>(status),
        bcryptErrorCategory()};
}

// 独占 Windows 文件句柄，并保证所有返回路径都会关闭句柄。
class FileHandle {
public:
    explicit FileHandle(HANDLE handle) noexcept
        : handle_(handle) {
    }

    ~FileHandle() {
        if (handle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(handle_);
        }
    }

    FileHandle(const FileHandle&) = delete;
    FileHandle& operator=(const FileHandle&) = delete;

    [[nodiscard]] HANDLE get() const noexcept {
        return handle_;
    }

private:
    HANDLE handle_{INVALID_HANDLE_VALUE};
};

// 独占 CNG 算法提供者句柄。
class AlgorithmHandle {
public:
    AlgorithmHandle() = default;

    ~AlgorithmHandle() {
        if (handle_ != nullptr) {
            BCryptCloseAlgorithmProvider(handle_, 0);
        }
    }

    AlgorithmHandle(const AlgorithmHandle&) = delete;
    AlgorithmHandle& operator=(const AlgorithmHandle&) = delete;

    [[nodiscard]] BCRYPT_ALG_HANDLE get() const noexcept {
        return handle_;
    }

    [[nodiscard]] BCRYPT_ALG_HANDLE* address() noexcept {
        return &handle_;
    }

private:
    BCRYPT_ALG_HANDLE handle_{nullptr};
};

// 独占 CNG 哈希句柄。
class HashHandle {
public:
    HashHandle() = default;

    ~HashHandle() {
        if (handle_ != nullptr) {
            BCryptDestroyHash(handle_);
        }
    }

    HashHandle(const HashHandle&) = delete;
    HashHandle& operator=(const HashHandle&) = delete;

    [[nodiscard]] BCRYPT_HASH_HANDLE get() const noexcept {
        return handle_;
    }

    [[nodiscard]] BCRYPT_HASH_HANDLE* address() noexcept {
        return &handle_;
    }

private:
    BCRYPT_HASH_HANDLE handle_{nullptr};
};

std::string toLowerHex(const std::vector<unsigned char>& digest) {
    constexpr char kHexDigits[] = "0123456789abcdef";
    std::string result(digest.size() * 2U, '0');
    for (std::size_t index = 0; index < digest.size(); ++index) {
        result[index * 2U] = kHexDigits[digest[index] >> 4U];
        result[index * 2U + 1U] = kHexDigits[digest[index] & 0x0FU];
    }
    return result;
}

}

FileHashResult calculateFileSha256(const std::filesystem::path& filePath) {
    return calculateFileSha256(filePath, {}, {});
}

FileHashResult calculateFileSha256(
    const std::filesystem::path& filePath,
    const FileHashProgressCallback& progressCallback) {
    return calculateFileSha256(filePath, progressCallback, {});
}

FileHashResult calculateFileSha256(
    const std::filesystem::path& filePath,
    const FileHashProgressCallback& progressCallback,
    std::stop_token stopToken) {
    if (stopToken.stop_requested()) {
        return {{}, {}, true};
    }

    const HANDLE rawFileHandle = CreateFileW(
        filePath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_SEQUENTIAL_SCAN,
        nullptr);
    if (rawFileHandle == INVALID_HANDLE_VALUE) {
        return {{}, std::error_code(
            static_cast<int>(GetLastError()),
            std::system_category())};
    }
    const FileHandle fileHandle{rawFileHandle};

    AlgorithmHandle algorithmHandle;
    NTSTATUS status = BCryptOpenAlgorithmProvider(
        algorithmHandle.address(),
        BCRYPT_SHA256_ALGORITHM,
        nullptr,
        0);
    if (!BCRYPT_SUCCESS(status)) {
        return {{}, makeBCryptError(status)};
    }

    DWORD hashObjectSize = 0;
    DWORD bytesCopied = 0;
    status = BCryptGetProperty(
        algorithmHandle.get(),
        BCRYPT_OBJECT_LENGTH,
        reinterpret_cast<unsigned char*>(&hashObjectSize),
        sizeof(hashObjectSize),
        &bytesCopied,
        0);
    if (!BCRYPT_SUCCESS(status)) {
        return {{}, makeBCryptError(status)};
    }

    DWORD hashLength = 0;
    status = BCryptGetProperty(
        algorithmHandle.get(),
        BCRYPT_HASH_LENGTH,
        reinterpret_cast<unsigned char*>(&hashLength),
        sizeof(hashLength),
        &bytesCopied,
        0);
    if (!BCRYPT_SUCCESS(status)) {
        return {{}, makeBCryptError(status)};
    }

    std::vector<unsigned char> hashObject(hashObjectSize);
    HashHandle hashHandle;
    status = BCryptCreateHash(
        algorithmHandle.get(),
        hashHandle.address(),
        hashObject.data(),
        hashObjectSize,
        nullptr,
        0,
        0);
    if (!BCRYPT_SUCCESS(status)) {
        return {{}, makeBCryptError(status)};
    }

    std::array<unsigned char, kReadBufferSize> buffer{};
    std::uintmax_t completedBytes = 0;
    while (true) {
        if (stopToken.stop_requested()) {
            return {{}, {}, true};
        }

        DWORD bytesRead = 0;
        if (!ReadFile(
                fileHandle.get(),
                buffer.data(),
                static_cast<DWORD>(buffer.size()),
                &bytesRead,
                nullptr)) {
            return {{}, std::error_code(
                static_cast<int>(GetLastError()),
                std::system_category())};
        }
        if (bytesRead == 0) {
            break;
        }
        if (stopToken.stop_requested()) {
            return {{}, {}, true};
        }

        status = BCryptHashData(
            hashHandle.get(),
            buffer.data(),
            bytesRead,
            0);
        if (!BCRYPT_SUCCESS(status)) {
            return {{}, makeBCryptError(status)};
        }
        completedBytes += bytesRead;
        if (progressCallback) {
            progressCallback(completedBytes);
        }
    }

    if (stopToken.stop_requested()) {
        return {{}, {}, true};
    }

    std::vector<unsigned char> digest(hashLength);
    status = BCryptFinishHash(
        hashHandle.get(),
        digest.data(),
        hashLength,
        0);
    if (!BCRYPT_SUCCESS(status)) {
        return {{}, makeBCryptError(status)};
    }

    return {toLowerHex(digest), {}};
}

}
