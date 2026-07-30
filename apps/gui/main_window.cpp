#include "main_window.h"

#include "scan_controller.h"
#include "ui_main_window.h"

#include <QFileDialog>
#include <QTimer>

#include <memory>
#include <utility>

namespace batchguard::gui {
namespace {

// 阶段 12 的默认执行器只验证界面状态流转，阶段 13 将替换为真实后台扫描。
class DeferredEmptyScanExecution final : public ScanExecution {
public:
    using ScanExecution::ScanExecution;

    void start(const ScanRequest& request) override {
        QTimer::singleShot(0, this, [this, scanId = request.scanId]() {
            auto report = std::make_shared<ScanReport>();
            emit completed(scanId, report);
        });
    }

    void requestCancel() override {
    }
};

}

MainWindow::MainWindow(QWidget* parent)
    : MainWindow{createDefaultExecution(), parent} {
}

MainWindow::MainWindow(
    std::unique_ptr<ScanExecution> execution,
    QWidget* parent)
    : QMainWindow{parent},
      ui_{std::make_unique<Ui::MainWindow>()},
      controller_{std::make_unique<ScanController>(
          std::move(execution))} {
    ui_->setupUi(this);
    ui_->hashWorkerSpinBox->setValue(controller_->hashWorkerCount());

    connect(
        ui_->directoryLineEdit,
        &QLineEdit::textChanged,
        controller_.get(),
        &ScanController::setDirectoryPath);
    connect(
        ui_->hashWorkerSpinBox,
        qOverload<int>(&QSpinBox::valueChanged),
        controller_.get(),
        &ScanController::setHashWorkerCount);
    connect(
        ui_->browseButton,
        &QPushButton::clicked,
        this,
        &MainWindow::chooseDirectory);
    connect(
        ui_->startButton,
        &QPushButton::clicked,
        controller_.get(),
        &ScanController::startScan);
    connect(
        ui_->cancelButton,
        &QPushButton::clicked,
        controller_.get(),
        &ScanController::cancelScan);
    connect(
        controller_.get(),
        &ScanController::viewStateChanged,
        this,
        &MainWindow::renderState);

    renderState();
}

MainWindow::~MainWindow() = default;

void MainWindow::chooseDirectory() {
    const QString directoryPath = QFileDialog::getExistingDirectory(
        this,
        QStringLiteral("选择需要扫描的目录"),
        ui_->directoryLineEdit->text());
    if (!directoryPath.isEmpty()) {
        ui_->directoryLineEdit->setText(directoryPath);
    }
}

void MainWindow::renderState() {
    const bool canEditInputs = controller_->canEditInputs();
    ui_->directoryLineEdit->setEnabled(canEditInputs);
    ui_->browseButton->setEnabled(canEditInputs);
    ui_->hashWorkerSpinBox->setEnabled(canEditInputs);
    ui_->startButton->setEnabled(controller_->canStart());
    ui_->cancelButton->setEnabled(controller_->canCancel());
    ui_->validationLabel->setText(controller_->validationMessage());
    ui_->statusLabel->setText(controller_->statusMessage());

    const bool isFailed = controller_->state() == ScanState::Failed;
    ui_->failureLabel->setVisible(isFailed);
    ui_->failureLabel->setText(controller_->failureMessage());

    const bool isBusy =
        controller_->state() == ScanState::Scanning ||
        controller_->state() == ScanState::Cancelling;
    ui_->progressBar->setVisible(isBusy);
    if (isBusy) {
        ui_->progressBar->setRange(0, 0);
    }
}

std::unique_ptr<ScanExecution> MainWindow::createDefaultExecution() {
    return std::make_unique<DeferredEmptyScanExecution>();
}

}
