#include "main_window.h"

#include "scan_controller.h"
#include "threaded_scan_execution.h"
#include "ui_main_window.h"

#include <QCloseEvent>
#include <QFileDialog>
#include <QTimer>

#include <algorithm>
#include <memory>
#include <utility>

namespace batchguard::gui {
namespace {

QString stageName(ScanProgressStage stage) {
    switch (stage) {
    case ScanProgressStage::Discovery:
        return QStringLiteral("正在发现文件");
    case ScanProgressStage::Metadata:
        return QStringLiteral("正在读取文件信息");
    case ScanProgressStage::Hashing:
        return QStringLiteral("正在计算内容指纹");
    case ScanProgressStage::Grouping:
        return QStringLiteral("正在整理重复组");
    }
    return QStringLiteral("正在扫描");
}

int progressValue(const ScanProgress& progress) {
    constexpr int kProgressMaximum = 1000;
    if (progress.totalBytes > 0U) {
        const std::uintmax_t boundedBytes =
            (std::min)(progress.completedBytes, progress.totalBytes);
        return static_cast<int>(
            static_cast<long double>(boundedBytes) /
            static_cast<long double>(progress.totalBytes) *
            kProgressMaximum);
    }
    if (progress.totalItems > 0U) {
        const std::size_t boundedItems =
            (std::min)(progress.completedItems, progress.totalItems);
        return static_cast<int>(
            static_cast<long double>(boundedItems) /
            static_cast<long double>(progress.totalItems) *
            kProgressMaximum);
    }
    return 0;
}

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
    connect(
        controller_.get(),
        &ScanController::scanProgressChanged,
        this,
        &MainWindow::renderProgress);

    renderState();
}

MainWindow::~MainWindow() = default;

void MainWindow::closeEvent(QCloseEvent* event) {
    if (controller_->state() == ScanState::Scanning ||
        controller_->state() == ScanState::Cancelling) {
        isClosePending_ = true;
        controller_->cancelScan();
        event->ignore();
        return;
    }
    event->accept();
}

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
    ui_->progressDetailsLabel->setVisible(isBusy);
    if (controller_->state() == ScanState::Scanning) {
        ui_->progressBar->setRange(0, 0);
        ui_->progressDetailsLabel->setText(QStringLiteral("正在准备扫描…"));
    }

    if (isClosePending_ && !isBusy) {
        QTimer::singleShot(0, this, &QWidget::close);
    }
}

void MainWindow::renderProgress(const ScanProgress& progress) {
    ui_->statusLabel->setText(stageName(progress.stage));
    if (progress.totalItems == 0U && progress.totalBytes == 0U) {
        ui_->progressBar->setRange(0, 0);
        ui_->progressDetailsLabel->setText(
            progress.isStageComplete
                ? QStringLiteral("当前阶段已完成")
                : QStringLiteral("正在计算工作量…"));
        return;
    }

    ui_->progressBar->setRange(0, 1000);
    ui_->progressBar->setValue(progressValue(progress));
    if (progress.totalBytes > 0U) {
        ui_->progressDetailsLabel->setText(
            QStringLiteral("%1 / %2 字节，%3 / %4 个文件")
                .arg(progress.completedBytes)
                .arg(progress.totalBytes)
                .arg(progress.completedItems)
                .arg(progress.totalItems));
    } else {
        ui_->progressDetailsLabel->setText(
            QStringLiteral("%1 / %2 个文件")
                .arg(progress.completedItems)
                .arg(progress.totalItems));
    }
}

std::unique_ptr<ScanExecution> MainWindow::createDefaultExecution() {
    return std::make_unique<ThreadedScanExecution>();
}

}
