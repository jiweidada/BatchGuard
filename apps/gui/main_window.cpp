#include "main_window.h"

#include "duplicate_group_model.h"
#include "duplicate_path_model.h"
#include "failure_model.h"
#include "gui_log_model.h"
#include "result_summary.h"
#include "scan_controller.h"
#include "threaded_scan_execution.h"
#include "ui_main_window.h"

#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QTimer>
#include <QUrl>

#include <algorithm>
#include <limits>
#include <memory>
#include <optional>
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
    : MainWindow{createDefaultExecution(), {}, parent} {
}

MainWindow::MainWindow(
    std::unique_ptr<ScanExecution> execution,
    QWidget* parent)
    : MainWindow{std::move(execution), {}, parent} {
}

MainWindow::MainWindow(
    std::unique_ptr<ScanExecution> execution,
    LogCallback terminalLogCallback,
    QWidget* parent)
    : QMainWindow{parent},
      ui_{std::make_unique<Ui::MainWindow>()},
      controller_{std::make_unique<ScanController>(
          std::move(execution))},
      duplicateGroupModel_{std::make_unique<DuplicateGroupModel>()},
      duplicatePathModel_{std::make_unique<DuplicatePathModel>()},
      failureModel_{std::make_unique<FailureModel>()},
      guiLogModel_{std::make_unique<GuiLogModel>()},
      terminalLogCallback_{std::move(terminalLogCallback)} {
    ui_->setupUi(this);
    ui_->hashWorkerSpinBox->setValue(controller_->hashWorkerCount());
    ui_->duplicateGroupTable->setModel(duplicateGroupModel_.get());
    ui_->duplicatePathTable->setModel(duplicatePathModel_.get());
    ui_->failureTable->setModel(failureModel_.get());
    ui_->logListView->setModel(guiLogModel_.get());
    ui_->duplicateGroupTable->horizontalHeader()->setStretchLastSection(true);
    ui_->duplicatePathTable->horizontalHeader()->setStretchLastSection(true);
    ui_->failureTable->horizontalHeader()->setStretchLastSection(true);
    ui_->duplicateGroupTable->setSortingEnabled(true);
    ui_->duplicateGroupTable->sortByColumn(4, Qt::DescendingOrder);

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
    connect(
        controller_.get(),
        &ScanController::logRecordCreated,
        this,
        &MainWindow::handleLogRecord);
    connect(
        controller_.get(),
        &ScanController::reportChanged,
        this,
        &MainWindow::showReport);
    connect(
        ui_->duplicateGroupTable->selectionModel(),
        &QItemSelectionModel::currentRowChanged,
        this,
        [this](const QModelIndex& current) {
            selectDuplicateGroup(current.row());
        });
    connect(
        ui_->duplicatePathTable->selectionModel(),
        &QItemSelectionModel::selectionChanged,
        this,
        &MainWindow::updatePathActions);
    connect(
        ui_->copyPathButton,
        &QPushButton::clicked,
        this,
        &MainWindow::copySelectedPath);
    connect(
        ui_->openDirectoryButton,
        &QPushButton::clicked,
        this,
        &MainWindow::openSelectedPathDirectory);
    connect(
        ui_->clearLogButton,
        &QPushButton::clicked,
        guiLogModel_.get(),
        &GuiLogModel::clear);
    connect(
        guiLogModel_.get(),
        &QAbstractItemModel::rowsInserted,
        ui_->logListView,
        &QListView::scrollToBottom);

    renderState();
    showReport({});
    handleLogRecord(makeLogRecord(
        LogLevel::Info,
        LogLayer::GuiController,
        "界面已就绪"));
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
        if (progress.stage == ScanProgressStage::Discovery) {
            ui_->progressDetailsLabel->setText(
                progress.isStageComplete
                    ? QStringLiteral("文件发现完成，共 %1 个文件")
                        .arg(progress.completedItems)
                    : QStringLiteral("已发现 %1 个文件")
                        .arg(progress.completedItems));
        } else {
            ui_->progressDetailsLabel->setText(
                progress.isStageComplete
                    ? QStringLiteral("当前阶段已完成")
                    : QStringLiteral("正在计算工作量…"));
        }
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

void MainWindow::handleLogRecord(const LogRecord& record) {
    if (terminalLogCallback_) {
        terminalLogCallback_(record);
    }
    guiLogModel_->appendRecord(record);
}

void MainWindow::showReport(const SharedScanReport& report) {
    duplicateGroupModel_->setReport(report);
    duplicatePathModel_->setReport(report);
    failureModel_->setReport(report);

    const bool hasReport = static_cast<bool>(report);
    ui_->resultPlaceholderLabel->setVisible(!hasReport);
    ui_->resultGroup->setVisible(hasReport);
    if (!hasReport) {
        return;
    }

    const ResultSummary summary = calculateResultSummary(*report);
    ui_->conclusionLabel->setText(summary.conclusion);
    ui_->scannedValueLabel->setText(
        QString::number(summary.scannedFileCount));
    ui_->successfulValueLabel->setText(
        QString::number(summary.successfulFileCount));
    ui_->groupValueLabel->setText(
        QString::number(summary.duplicateGroupCount));
    ui_->redundantValueLabel->setText(
        QString::number(summary.redundantCopyCount));
    ui_->reclaimableValueLabel->setText(
        formatByteSize(summary.reclaimableBytes));
    ui_->failureValueLabel->setText(
        QString::number(summary.failureCount));
    ui_->detailsLabel->setText(
        QStringLiteral(
            "逻辑大小 %1 · 候选 %2 个 / %3 · 完成指纹 %4 个 · 用时 %5 秒")
            .arg(formatByteSize(summary.totalLogicalBytes))
            .arg(summary.candidateFileCount)
            .arg(formatByteSize(summary.candidateBytes))
            .arg(summary.hashedFileCount)
            .arg(
                static_cast<double>(controller_->elapsedMilliseconds()) /
                    1000.0,
                0,
                'f',
                2));
    ui_->resultTabs->setTabText(
        0,
        QStringLiteral("重复内容（%1）")
            .arg(summary.duplicateGroupCount));
    ui_->resultTabs->setTabText(
        1,
        QStringLiteral("处理失败（%1）")
            .arg(summary.failureCount));

    const bool hasDuplicates = duplicateGroupModel_->rowCount() > 0;
    ui_->duplicateEmptyLabel->setVisible(!hasDuplicates);
    ui_->duplicateTablesWidget->setVisible(hasDuplicates);
    if (hasDuplicates) {
        ui_->duplicateGroupTable->selectRow(0);
        selectDuplicateGroup(0);
    } else {
        duplicatePathModel_->setGroupIndex(std::nullopt);
    }

    const bool hasFailures = failureModel_->rowCount() > 0;
    ui_->failureEmptyLabel->setVisible(!hasFailures);
    ui_->failureTable->setVisible(hasFailures);
    ui_->failureSummaryLabel->setText(
        hasFailures
            ? failureModel_->stageSummary()
            : QStringLiteral("所有已发现文件均完成处理。"));
    updatePathActions();
}

void MainWindow::selectDuplicateGroup(int row) {
    const std::size_t groupIndex =
        duplicateGroupModel_->reportGroupIndexForRow(row);
    if (groupIndex == (std::numeric_limits<std::size_t>::max)()) {
        duplicatePathModel_->setGroupIndex(std::nullopt);
    } else {
        duplicatePathModel_->setGroupIndex(groupIndex);
        if (duplicatePathModel_->rowCount() > 0) {
            ui_->duplicatePathTable->selectRow(0);
        }
    }
    updatePathActions();
}

void MainWindow::updatePathActions() {
    const bool hasSelection =
        ui_->duplicatePathTable->selectionModel()->hasSelection();
    ui_->copyPathButton->setEnabled(hasSelection);
    ui_->openDirectoryButton->setEnabled(hasSelection);
}

void MainWindow::copySelectedPath() {
    const QModelIndex currentIndex =
        ui_->duplicatePathTable->currentIndex();
    const QString path = duplicatePathModel_->pathAt(currentIndex.row());
    if (!path.isEmpty()) {
        QApplication::clipboard()->setText(path);
    }
}

void MainWindow::openSelectedPathDirectory() {
    const QModelIndex currentIndex =
        ui_->duplicatePathTable->currentIndex();
    const QString path = duplicatePathModel_->pathAt(currentIndex.row());
    if (!path.isEmpty()) {
        QDesktopServices::openUrl(
            QUrl::fromLocalFile(QFileInfo{path}.absolutePath()));
    }
}

std::unique_ptr<ScanExecution> MainWindow::createDefaultExecution() {
    return std::make_unique<ThreadedScanExecution>();
}

}
