#include "main_window.h"
#include "terminal_console.h"
#include "threaded_scan_execution.h"

#include "batchguard/logging/logging.h"

#include <QApplication>

#include <iostream>
#include <memory>
#include <string>

namespace {

constexpr int kInternalErrorExitCode = 3;

}

int main(int argc, char* argv[]) {
    const bool hasParentTerminal =
        batchguard::gui::connectToParentTerminal();
    batchguard::TerminalLogSink terminalSink{std::cerr};
    const batchguard::LogCallback terminalLogCallback =
        hasParentTerminal
        ? batchguard::LogCallback{
            [&terminalSink](const batchguard::LogRecord& record) {
                terminalSink.write(record);
            }}
        : batchguard::LogCallback{};
    try {
        if (terminalLogCallback) {
            terminalLogCallback(batchguard::makeLogRecord(
                batchguard::LogLevel::Info,
                batchguard::LogLayer::GuiExecution,
                "GUI 应用启动"));
        }
        QApplication application{argc, argv};
        batchguard::gui::MainWindow mainWindow{
            std::make_unique<batchguard::gui::ThreadedScanExecution>(),
            terminalLogCallback};
        mainWindow.show();
        const int exitCode = application.exec();
        if (terminalLogCallback) {
            terminalLogCallback(batchguard::makeLogRecord(
                batchguard::LogLevel::Info,
                batchguard::LogLayer::GuiExecution,
                "GUI 应用结束",
                {{"exitCode", std::to_string(exitCode)}}));
        }
        return exitCode;
    } catch (...) {
        // 最外层只负责阻止异常越过进程边界，运行期错误由扫描控制器展示。
        if (terminalLogCallback) {
            terminalLogCallback(batchguard::makeLogRecord(
                batchguard::LogLevel::Error,
                batchguard::LogLayer::GuiExecution,
                "GUI 应用发生未处理异常"));
        }
        return kInternalErrorExitCode;
    }
}
