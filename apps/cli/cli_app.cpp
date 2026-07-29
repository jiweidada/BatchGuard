#include "cli_app.h"

#include "batchguard/core/input_validator.h"

#include <filesystem>
#include <ostream>
#include <string_view>

namespace batchguard::cli {
namespace {

constexpr std::wstring_view kHelpOption = L"--help";
constexpr std::wstring_view kVersionOption = L"--version";

void printUsage(std::wostream& output) {
    output << L"BatchGuard - 本地重复文件检查工具\n"
           << L"\n"
           << L"用法：\n"
           << L"  BatchGuard <目录路径>\n"
           << L"  BatchGuard --help\n"
           << L"  BatchGuard --version\n"
           << L"\n"
           << L"阶段 2 只验证输入目录，不执行目录扫描。\n";
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

}

int runCli(
    const std::vector<std::wstring>& arguments,
    std::wostream& output,
    std::wostream& error) {
    if (arguments.empty()) {
        error << L"错误：缺少目录参数。\n"
              << L"使用 BatchGuard --help 查看用法。\n";
        return static_cast<int>(ExitCode::InvalidInput);
    }

    if (arguments.size() > 1U) {
        error << L"错误：只允许提供一个目录路径。\n"
              << L"使用 BatchGuard --help 查看用法。\n";
        return static_cast<int>(ExitCode::InvalidInput);
    }

    const std::wstring_view argument = arguments.front();

    // CLI 只接受一个选项或一个目录位置参数，因此帮助和版本必须独立使用。
    if (argument == kHelpOption) {
        printUsage(output);
        return static_cast<int>(ExitCode::Success);
    }
    if (argument == kVersionOption) {
        output << L"BatchGuard " << BATCHGUARD_VERSION << L'\n';
        return static_cast<int>(ExitCode::Success);
    }
    if (argument.starts_with(L"--")) {
        error << L"错误：不支持的选项：" << argument << L'\n'
              << L"使用 BatchGuard --help 查看用法。\n";
        return static_cast<int>(ExitCode::InvalidInput);
    }

    const std::filesystem::path directoryPath{argument};
    const InputValidationResult validationResult = validateInputDirectory(directoryPath);
    if (!validationResult.isValid()) {
        printInputError(validationResult.error, directoryPath, error);
        return static_cast<int>(ExitCode::InvalidInput);
    }

    // 阶段 2 在元数据验证后主动结束，不枚举已通过验证的目录内容。
    output << L"目录验证通过：" << directoryPath.wstring() << L'\n'
           << L"阶段 2 仅验证输入，尚未执行目录扫描。\n";
    return static_cast<int>(ExitCode::Success);
}

}
