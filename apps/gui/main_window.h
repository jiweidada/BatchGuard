#pragma once

#include "scan_execution.h"

#include "batchguard/logging/logging.h"

#include <QMainWindow>

#include <memory>

QT_BEGIN_NAMESPACE
class QCloseEvent;
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

namespace batchguard::gui {

class DuplicateGroupModel;
class DuplicatePathModel;
class FailureModel;
class GuiLogModel;
class ScanController;

// 提供目录配置、扫描操作和状态反馈，并把用户命令转发给控制器。
class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    explicit MainWindow(
        std::unique_ptr<ScanExecution> execution,
        QWidget* parent = nullptr);
    MainWindow(
        std::unique_ptr<ScanExecution> execution,
        LogCallback terminalLogCallback,
        QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void chooseDirectory();
    void renderState();
    void renderProgress(const batchguard::ScanProgress& progress);
    void handleLogRecord(const batchguard::LogRecord& record);
    void showReport(const SharedScanReport& report);
    void selectDuplicateGroup(int row);
    void updatePathActions();
    void copySelectedPath();
    void openSelectedPathDirectory();
    [[nodiscard]] static std::unique_ptr<ScanExecution>
        createDefaultExecution();

    std::unique_ptr<Ui::MainWindow> ui_;
    std::unique_ptr<ScanController> controller_;
    std::unique_ptr<DuplicateGroupModel> duplicateGroupModel_;
    std::unique_ptr<DuplicatePathModel> duplicatePathModel_;
    std::unique_ptr<FailureModel> failureModel_;
    std::unique_ptr<GuiLogModel> guiLogModel_;
    LogCallback terminalLogCallback_;
    bool isClosePending_{};
};

}
