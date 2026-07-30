#pragma once

#include "scan_execution.h"

#include <QMainWindow>

#include <memory>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

namespace batchguard::gui {

class ScanController;

// 提供目录配置、扫描操作和状态反馈，并把用户命令转发给控制器。
class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    explicit MainWindow(
        std::unique_ptr<ScanExecution> execution,
        QWidget* parent = nullptr);
    ~MainWindow() override;

private:
    void chooseDirectory();
    void renderState();
    [[nodiscard]] static std::unique_ptr<ScanExecution>
        createDefaultExecution();

    std::unique_ptr<Ui::MainWindow> ui_;
    std::unique_ptr<ScanController> controller_;
};

}
