#include "cli_app.h"

#include "batchguard/core/input_validator.h"
#include "batchguard/core/scanner.h"

#include <Windows.h>

#include <array>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <system_error>

namespace batchguard::cli {
namespace {

constexpr std::wstring_view kHelpOption = L"--help";
constexpr std::wstring_view kHashWorkersOption = L"--hash-workers";
constexpr std::wstring_view kVersionOption = L"--version";
constexpr std::size_t kMaximumHashWorkerCount = 64U;

void printUsage(std::wostream& output) {
    output << L"BatchGuard - 本地重复文件检查工具\n"
           << L"\n"
           << L"用法：\n"
           << L"  BatchGuard <目录路径>\n"
           << L"  BatchGuard --hash-workers <1-64> <目录路径>\n"
           << L"  BatchGuard --help\n"
           << L"  BatchGuard --version\n"
           << L"\n"
           << L"递归扫描并报告重复文件，不修改任何输入文件。\n"
           << L"未指定哈希线程数时自动使用最多 4 个工作线程。\n"
           << L"交互式控制台会显示发现、元数据、哈希和报告生成进度。\n";
}

std::optional<std::size_t> parseHashWorkerCount(std::wstring_view text) {
    if (text.empty()) {
        return std::nullopt;
    }

    std::size_t value = 0U;
    for (const wchar_t character : text) {
        if (character < L'0' || character > L'9') {
            return std::nullopt;
        }
        const std::size_t digit = static_cast<std::size_t>(character - L'0');
        if (value > (kMaximumHashWorkerCount - digit) / 10U) {
            return std::nullopt;
        }
        value = value * 10U + digit;
    }
    if (value == 0U || value > kMaximumHashWorkerCount) {
        return std::nullopt;
    }
    return value;
}

void printInputError(
    InputValidationError validationError,
    const std::filesystem::path& directoryPath,
    std::wostream& error) {
    switch (validationError) {
        case InputValidationError::EmptyPath:
            error << L"错误：目录路径不能为空。\n";
            break;
        case InputValidationError::CannotAccess:
            error << L"错误：无法访问输入路径：" << directoryPath.wstring() << L'\n';
            break;
        case InputValidationError::DoesNotExist:
            error << L"错误：目录不存在：" << directoryPath.wstring() << L'\n';
            break;
        case InputValidationError::NotDirectory:
            error << L"错误：输入路径不是目录：" << directoryPath.wstring() << L'\n';
            break;
        case InputValidationError::None:
            break;
    }
}

std::wstring_view describeFailureStage(FailureStage stage) {
    switch (stage) {
        case FailureStage::Discovery:
            return L"发现文件";
        case FailureStage::Metadata:
            return L"读取元数据";
        case FailureStage::Hashing:
            return L"读取内容";
    }
    return L"未知阶段";
}

std::wstring formatSystemError(int errorValue) {
    std::array<wchar_t, 512> messageBuffer{};
    const DWORD messageLength = FormatMessageW(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        static_cast<DWORD>(errorValue),
        0,
        messageBuffer.data(),
        static_cast<DWORD>(messageBuffer.size()),
        nullptr);
    if (messageLength == 0) {
        return {};
    }

    std::wstring message(messageBuffer.data(), messageLength);
    while (!message.empty() &&
           (message.back() == L'\r' || message.back() == L'\n' ||
            message.back() == L' ' || message.back() == L'\t')) {
        message.pop_back();
    }
    return message;
}

std::wstring describeFailureReason(const std::error_code& errorCode) {
    if (errorCode == std::errc::permission_denied) {
        return L"权限不足。";
    }
    if (errorCode == std::errc::no_such_file_or_directory) {
        return L"文件或目录已不存在。";
    }
    if (errorCode == std::errc::io_error) {
        return L"文件输入输出失败。";
    }

    if (&errorCode.category() == &std::system_category()) {
        const std::wstring systemMessage = formatSystemError(errorCode.value());
        if (!systemMessage.empty()) {
            return systemMessage;
        }
    }

    const std::string_view categoryName = errorCode.category().name();
    if (categoryName == "bcrypt") {
        return L"CNG SHA-256 操作失败（状态码 " +
            std::to_wstring(static_cast<unsigned int>(errorCode.value())) +
            L"）。";
    }
    return L"底层操作失败（错误码 " + std::to_wstring(errorCode.value()) + L"）。";
}

void printScanReport(const ScanReport& report, std::wostream& output) {
    output << L"扫描目录：" << report.rootPath.wstring() << L'\n'
           << L"发现文件：" << report.discoveredFileCount << L'\n'
           << L"成功处理：" << report.successfulFileCount << L'\n'
           << L"处理失败：" << report.failures.size() << L'\n'
           << L"重复组数：" << report.duplicateGroups.size() << L"\n\n";

    if (report.duplicateGroups.empty()) {
        output << L"未发现重复文件。\n";
    } else {
        for (std::size_t index = 0; index < report.duplicateGroups.size(); ++index) {
            const DuplicateGroup& group = report.duplicateGroups[index];
            const std::wstring sha256(group.sha256.begin(), group.sha256.end());
            output << L"[重复组 " << index + 1U << L"]\n"
                   << L"  文件大小：" << group.fileSize << L" 字节\n"
                   << L"  SHA-256：" << sha256 << L'\n';
            for (const std::filesystem::path& filePath : group.filePaths) {
                output << L"  " << filePath.wstring() << L'\n';
            }
            if (index + 1U < report.duplicateGroups.size()) {
                output << L'\n';
            }
        }
    }

    if (!report.failures.empty()) {
        output << L"\n[失败文件]\n";
        for (std::size_t index = 0; index < report.failures.size(); ++index) {
            const FileFailure& failure = report.failures[index];
            output << L"  路径：" << failure.path.wstring() << L'\n'
                   << L"  阶段：" << describeFailureStage(failure.stage) << L'\n'
                   << L"  原因：" << describeFailureReason(failure.errorCode) << L'\n';
            if (index + 1U < report.failures.size()) {
                output << L'\n';
            }
        }
    }
}

}

int runCli(
    const std::vector<std::wstring>& arguments,
    std::wostream& output,
    std::wostream& error,
    const ScanProgressCallback& progressCallback) {
    if (arguments.empty()) {
        error << L"错误：缺少目录参数。\n"
              << L"使用 BatchGuard --help 查看用法。\n";
        return static_cast<int>(ExitCode::InvalidInput);
    }

    // 帮助和版本信息必须独立使用，避免忽略同一命令中的其他参数。
    if (arguments.size() == 1U && arguments.front() == kHelpOption) {
        printUsage(output);
        return static_cast<int>(ExitCode::Success);
    }
    if (arguments.size() == 1U && arguments.front() == kVersionOption) {
        output << L"BatchGuard " << BATCHGUARD_VERSION << L'\n';
        return static_cast<int>(ExitCode::Success);
    }

    ScanOptions scanOptions;
    std::optional<std::filesystem::path> directoryPath;
    bool hasHashWorkerOption = false;
    for (std::size_t index = 0; index < arguments.size(); ++index) {
        const std::wstring_view argument = arguments[index];
        if (argument == kHashWorkersOption) {
            if (hasHashWorkerOption) {
                error << L"错误：--hash-workers 只能指定一次。\n"
                      << L"使用 BatchGuard --help 查看用法。\n";
                return static_cast<int>(ExitCode::InvalidInput);
            }
            if (index + 1U >= arguments.size()) {
                error << L"错误：--hash-workers 后缺少线程数。\n"
                      << L"使用 BatchGuard --help 查看用法。\n";
                return static_cast<int>(ExitCode::InvalidInput);
            }

            const std::optional<std::size_t> workerCount =
                parseHashWorkerCount(arguments[index + 1U]);
            if (!workerCount.has_value()) {
                error << L"错误：哈希线程数必须是 1 至 64 的整数。\n"
                      << L"使用 BatchGuard --help 查看用法。\n";
                return static_cast<int>(ExitCode::InvalidInput);
            }
            scanOptions.hashWorkerCount = *workerCount;
            hasHashWorkerOption = true;
            ++index;
            continue;
        }
        if (argument.starts_with(L"--")) {
            error << L"错误：不支持的选项：" << argument << L'\n'
                  << L"使用 BatchGuard --help 查看用法。\n";
            return static_cast<int>(ExitCode::InvalidInput);
        }
        if (directoryPath.has_value()) {
            error << L"错误：只允许提供一个目录路径。\n"
                  << L"使用 BatchGuard --help 查看用法。\n";
            return static_cast<int>(ExitCode::InvalidInput);
        }
        directoryPath = std::filesystem::path{argument};
    }

    if (!directoryPath.has_value()) {
        error << L"错误：缺少目录参数。\n"
              << L"使用 BatchGuard --help 查看用法。\n";
        return static_cast<int>(ExitCode::InvalidInput);
    }

    const InputValidationResult validationResult =
        validateInputDirectory(*directoryPath);
    if (!validationResult.isValid()) {
        printInputError(validationResult.error, *directoryPath, error);
        return static_cast<int>(ExitCode::InvalidInput);
    }

    const ScanReport report =
        scanDirectory(*directoryPath, scanOptions, progressCallback);
    printScanReport(report, output);
    if (!report.isComplete()) {
        return static_cast<int>(ExitCode::PartialFailure);
    }
    return static_cast<int>(ExitCode::Success);
}

}
