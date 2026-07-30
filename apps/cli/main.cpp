#include "cli_app.h"
#include "console_encoding.h"
#include "console_progress.h"

#include "batchguard/core/scan_progress.h"
#include "batchguard/logging/logging.h"

#include <exception>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

int wmain(int argumentCount, wchar_t* argumentValues[]) {
    batchguard::TerminalLogSink terminalLogSink{std::cerr};
    try {
        batchguard::cli::configureConsoleEncoding();

        // `wmain` 直接保留 Windows 提供的 Unicode 参数，避免经过传统代码页转换。
        std::vector<std::wstring> arguments;
        arguments.reserve(static_cast<std::size_t>(argumentCount - 1));
        for (int index = 1; index < argumentCount; ++index) {
            arguments.emplace_back(argumentValues[index]);
        }

        std::wostringstream output;
        std::wostringstream error;
        batchguard::cli::ConsoleProgressRenderer progressRenderer{
            std::cout,
            batchguard::cli::isStandardOutputConsole()};
        const batchguard::LogCallback logCallback =
            [&terminalLogSink,
             &progressRenderer](const batchguard::LogRecord& record) {
                progressRenderer.finish();
                terminalLogSink.write(record);
            };
        logCallback(batchguard::makeLogRecord(
            batchguard::LogLevel::Info,
            batchguard::LogLayer::Cli,
            "CLI 应用启动",
            {{"argumentCount", std::to_string(arguments.size())}}));
        batchguard::ScanProgressCallback progressCallback;
        if (progressRenderer.isEnabled()) {
            progressCallback =
                [&progressRenderer](const batchguard::ScanProgress& progress) {
                    progressRenderer.render(progress);
                };
        }
        const int exitCode = batchguard::cli::runCli(
            arguments,
            output,
            error,
            progressCallback,
            logCallback);
        progressRenderer.finish();

        // 在最终输出边界统一转换，使控制台和重定向流收到相同的 UTF-8 内容。
        batchguard::cli::writeUtf8(std::cout, output.str());
        batchguard::cli::writeUtf8(std::cerr, error.str());
        logCallback(batchguard::makeLogRecord(
            batchguard::LogLevel::Info,
            batchguard::LogLayer::Cli,
            "CLI 应用结束",
            {{"exitCode", std::to_string(exitCode)}}));
        return exitCode;
    } catch (const std::exception&) {
        terminalLogSink.write(batchguard::makeLogRecord(
            batchguard::LogLevel::Error,
            batchguard::LogLayer::Cli,
            "CLI 应用发生未处理异常"));
        std::cerr << "错误：程序发生无法继续的内部错误。\n";
        return static_cast<int>(batchguard::cli::ExitCode::InternalError);
    } catch (...) {
        terminalLogSink.write(batchguard::makeLogRecord(
            batchguard::LogLevel::Error,
            batchguard::LogLayer::Cli,
            "CLI 应用发生未知异常"));
        std::cerr << "错误：程序发生未知内部错误。\n";
        return static_cast<int>(batchguard::cli::ExitCode::InternalError);
    }
}
