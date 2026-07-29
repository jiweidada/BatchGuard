#pragma once

#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>

namespace batchguard::test_support {

// 独占一个隔离的测试目录，并在析构时删除其中生成的全部内容。
class TemporaryDirectory {
public:
    TemporaryDirectory() {
        std::error_code errorCode;
        const std::filesystem::path temporaryRoot =
            std::filesystem::temp_directory_path(errorCode);
        if (errorCode) {
            throw std::runtime_error("Unable to find the temporary directory.");
        }

        // 有限次数重试可以避免多个进程在同一时钟周期创建夹具时相互影响。
        const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (int attempt = 0; attempt < 100; ++attempt) {
            path_ = temporaryRoot /
                ("batchguard-test-" + std::to_string(timestamp) + "-" + std::to_string(attempt));
            if (std::filesystem::create_directory(path_, errorCode)) {
                return;
            }
            if (errorCode) {
                throw std::runtime_error("Unable to create a temporary directory.");
            }
        }

        throw std::runtime_error("Unable to allocate a unique temporary directory.");
    }

    ~TemporaryDirectory() {
        std::error_code errorCode;
        std::filesystem::remove_all(path_, errorCode);
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;
    TemporaryDirectory(TemporaryDirectory&&) = delete;
    TemporaryDirectory& operator=(TemporaryDirectory&&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const noexcept {
        return path_;
    }

private:
    std::filesystem::path path_;
};

}
