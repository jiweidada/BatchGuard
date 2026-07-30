#include "duplicate_group_model.h"
#include "duplicate_path_model.h"
#include "failure_model.h"
#include "main_window.h"
#include "result_summary.h"
#include "scan_controller.h"
#include "scan_execution.h"
#include "threaded_scan_execution.h"

#include <QDir>
#include <QFile>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTableView>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QtTest>

#include <algorithm>
#include <filesystem>
#include <limits>
#include <memory>
#include <system_error>
#include <utility>
#include <vector>

namespace batchguard::gui {
namespace {

class FakeScanExecution final : public ScanExecution {
public:
    using ScanExecution::ScanExecution;

    void start(const ScanRequest& request) override {
        requests.push_back(request);
    }

    void requestCancel() override {
        ++cancelRequestCount;
    }

    void finishCompleted(
        quint64 scanId,
        SharedScanReport report = std::make_shared<ScanReport>()) {
        emit completed(scanId, report);
    }

    void finishCancelled(quint64 scanId) {
        emit cancelled(scanId);
    }

    void finishFailed(quint64 scanId, const QString& message) {
        emit failed(scanId, message);
    }

    void sendProgress(
        quint64 scanId,
        const ScanProgress& progress) {
        emit progressChanged(scanId, progress);
    }

    std::vector<ScanRequest> requests;
    int cancelRequestCount{};
};

void createFile(
    const QString& path,
    int blockCount,
    char fillCharacter) {
    QFile file{path};
    QVERIFY(file.open(QIODevice::WriteOnly));
    const QByteArray block{1024 * 1024, fillCharacter};
    for (int index = 0; index < blockCount; ++index) {
        QCOMPARE(file.write(block), static_cast<qint64>(block.size()));
    }
}

class ScanControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void initialWindowStateMatchesIdleContract();
    void invalidDirectoryCannotStart();
    void unicodeAndSpaceDirectoryFlowsThroughControllerAndModels();
    void validDirectoryStartsOnlyOnceAndLocksInputs();
    void cancelRequestIsSentOnlyOnce();
    void completionAfterCancellationIsTreatedAsCancelled();
    void completionCancellationAndFailureReachTerminalStates();
    void staleCompletionCannotReplaceCurrentScan();
    void mainWindowRendersControllerAvailability();
    void progressEventsMapToWindowTextAndRange();
    void realBackgroundScanCompletesAndReportsProgress();
    void realBackgroundScanCanBeCancelled();
    void closingWindowCancelsAndReclaimsBackgroundThread();
    void destroyingExecutionReclaimsThreadObject();
    void resultSummaryUsesDocumentedStatistics();
    void resultSummaryHandlesZeroByteDuplicates();
    void resultSummarySaturatesLargeByteTotals();
    void resultModelsExposeStableRowsAndRawSortValues();
    void duplicateGroupModelExposesAllColumnsAndRoles();
    void failureModelExposesRowsColumnsAndErrorText();
    void resultModelsHandleManyRowsWithoutCopyingReports();
    void mainWindowShowsCompleteReportAndHidesCancelledResults();
    void sortingDuplicateGroupsPreservesSelectedGroup();
};

void ScanControllerTest::initialWindowStateMatchesIdleContract() {
    auto execution = std::make_unique<FakeScanExecution>();
    MainWindow window{std::move(execution)};
    auto* directoryInput =
        window.findChild<QLineEdit*>(QStringLiteral("directoryLineEdit"));
    auto* workerInput =
        window.findChild<QSpinBox*>(QStringLiteral("hashWorkerSpinBox"));
    auto* startButton =
        window.findChild<QPushButton*>(QStringLiteral("startButton"));
    auto* cancelButton =
        window.findChild<QPushButton*>(QStringLiteral("cancelButton"));
    auto* validationLabel =
        window.findChild<QLabel*>(QStringLiteral("validationLabel"));
    auto* statusLabel =
        window.findChild<QLabel*>(QStringLiteral("statusLabel"));
    auto* progressBar =
        window.findChild<QProgressBar*>(QStringLiteral("progressBar"));
    auto* resultGroup =
        window.findChild<QGroupBox*>(QStringLiteral("resultGroup"));
    auto* resultPlaceholder = window.findChild<QLabel*>(
        QStringLiteral("resultPlaceholderLabel"));

    QVERIFY(directoryInput);
    QVERIFY(workerInput);
    QVERIFY(startButton);
    QVERIFY(cancelButton);
    QVERIFY(validationLabel);
    QVERIFY(statusLabel);
    QVERIFY(progressBar);
    QVERIFY(resultGroup);
    QVERIFY(resultPlaceholder);
    QVERIFY(directoryInput->isEnabled());
    QVERIFY(workerInput->isEnabled());
    QVERIFY(!startButton->isEnabled());
    QVERIFY(!cancelButton->isEnabled());
    QCOMPARE(
        validationLabel->text(),
        QStringLiteral("请选择需要扫描的目录。"));
    QCOMPARE(statusLabel->text(), QStringLiteral("等待开始扫描"));
    QVERIFY(progressBar->isHidden());
    QVERIFY(resultGroup->isHidden());
    QVERIFY(!resultPlaceholder->isHidden());
}

void ScanControllerTest::invalidDirectoryCannotStart() {
    auto execution = std::make_unique<FakeScanExecution>();
    FakeScanExecution* executionPointer = execution.get();
    ScanController controller{std::move(execution)};

    controller.setDirectoryPath(QStringLiteral("Z:/BatchGuard/not-existing"));
    controller.startScan();

    QCOMPARE(controller.state(), ScanState::Idle);
    QVERIFY(!controller.canStart());
    QVERIFY(!controller.validationMessage().isEmpty());
    QCOMPARE(executionPointer->requests.size(), std::size_t{0U});
}

void ScanControllerTest::unicodeAndSpaceDirectoryFlowsThroughControllerAndModels() {
    QTemporaryDir rootDirectory;
    QVERIFY(rootDirectory.isValid());
    const QString directoryPath =
        rootDirectory.filePath(QStringLiteral("中文 目录"));
    QVERIFY(QDir{}.mkpath(directoryPath));

    auto execution = std::make_unique<FakeScanExecution>();
    FakeScanExecution* executionPointer = execution.get();
    ScanController controller{std::move(execution)};
    controller.setDirectoryPath(
        QStringLiteral("  %1  ").arg(directoryPath));
    controller.startScan();

    QCOMPARE(executionPointer->requests.size(), std::size_t{1U});
    QCOMPARE(
        executionPointer->requests.front().directoryPath,
        directoryPath);

    auto report = std::make_shared<ScanReport>();
    const QString filePath =
        QDir{directoryPath}.filePath(QStringLiteral("重复 文件.bin"));
    report->duplicateGroups = {{
        0U,
        "empty",
        {
            std::filesystem::path{filePath.toStdWString()},
            std::filesystem::path{
                QDir{directoryPath}
                    .filePath(QStringLiteral("副本 文件.bin"))
                    .toStdWString()}}}};

    DuplicatePathModel pathModel;
    pathModel.setReport(report);
    pathModel.setGroupIndex(0U);

    QCOMPARE(pathModel.rowCount(), 2);
    QCOMPARE(pathModel.pathAt(0), filePath);
    QCOMPARE(
        pathModel.index(0, 0).data(DuplicatePathModel::FullPathRole)
            .toString(),
        filePath);
}

void ScanControllerTest::validDirectoryStartsOnlyOnceAndLocksInputs() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    auto execution = std::make_unique<FakeScanExecution>();
    FakeScanExecution* executionPointer = execution.get();
    ScanController controller{std::move(execution)};
    controller.setDirectoryPath(directory.path());

    QVERIFY(controller.canStart());
    controller.startScan();
    controller.startScan();

    QCOMPARE(controller.state(), ScanState::Scanning);
    QVERIFY(!controller.canEditInputs());
    QVERIFY(!controller.canStart());
    QVERIFY(controller.canCancel());
    QCOMPARE(executionPointer->requests.size(), std::size_t{1U});
    QCOMPARE(executionPointer->requests.front().directoryPath, directory.path());
}

void ScanControllerTest::cancelRequestIsSentOnlyOnce() {
    QTemporaryDir directory;
    auto execution = std::make_unique<FakeScanExecution>();
    FakeScanExecution* executionPointer = execution.get();
    ScanController controller{std::move(execution)};
    controller.setDirectoryPath(directory.path());
    controller.startScan();

    controller.cancelScan();
    controller.cancelScan();

    QCOMPARE(controller.state(), ScanState::Cancelling);
    QVERIFY(!controller.canCancel());
    QCOMPARE(executionPointer->cancelRequestCount, 1);

    executionPointer->finishCancelled(controller.currentScanId());
    QCOMPARE(controller.state(), ScanState::Cancelled);
    QVERIFY(controller.canEditInputs());
    QVERIFY(controller.canStart());
}

void ScanControllerTest::completionAfterCancellationIsTreatedAsCancelled() {
    QTemporaryDir directory;
    auto execution = std::make_unique<FakeScanExecution>();
    FakeScanExecution* executionPointer = execution.get();
    ScanController controller{std::move(execution)};
    controller.setDirectoryPath(directory.path());
    controller.startScan();
    const quint64 scanId = controller.currentScanId();

    controller.cancelScan();
    executionPointer->finishCompleted(
        scanId,
        std::make_shared<ScanReport>());

    QCOMPARE(controller.state(), ScanState::Cancelled);
    QVERIFY(!controller.report());
    QVERIFY(controller.canStart());
}

void ScanControllerTest::completionCancellationAndFailureReachTerminalStates() {
    QTemporaryDir directory;
    auto execution = std::make_unique<FakeScanExecution>();
    FakeScanExecution* executionPointer = execution.get();
    ScanController controller{std::move(execution)};
    controller.setDirectoryPath(directory.path());

    controller.startScan();
    executionPointer->finishCompleted(controller.currentScanId());
    QCOMPARE(controller.state(), ScanState::Completed);
    QVERIFY(controller.report());

    controller.startScan();
    executionPointer->finishCancelled(controller.currentScanId());
    QCOMPARE(controller.state(), ScanState::Cancelled);
    QVERIFY(!controller.report());

    controller.startScan();
    executionPointer->finishFailed(
        controller.currentScanId(),
        QStringLiteral("可控错误"));
    QCOMPARE(controller.state(), ScanState::Failed);
    QCOMPARE(controller.failureMessage(), QStringLiteral("可控错误"));
}

void ScanControllerTest::staleCompletionCannotReplaceCurrentScan() {
    QTemporaryDir directory;
    auto execution = std::make_unique<FakeScanExecution>();
    FakeScanExecution* executionPointer = execution.get();
    ScanController controller{std::move(execution)};
    controller.setDirectoryPath(directory.path());

    controller.startScan();
    const quint64 oldScanId = controller.currentScanId();
    executionPointer->finishCancelled(oldScanId);
    controller.startScan();
    const quint64 currentScanId = controller.currentScanId();

    executionPointer->finishCompleted(oldScanId);
    QCOMPARE(controller.state(), ScanState::Scanning);
    QCOMPARE(controller.currentScanId(), currentScanId);
    QVERIFY(!controller.report());
}

void ScanControllerTest::mainWindowRendersControllerAvailability() {
    QTemporaryDir directory;
    auto execution = std::make_unique<FakeScanExecution>();
    MainWindow window{std::move(execution)};
    auto* directoryInput =
        window.findChild<QLineEdit*>(QStringLiteral("directoryLineEdit"));
    auto* workerInput =
        window.findChild<QSpinBox*>(QStringLiteral("hashWorkerSpinBox"));
    auto* startButton =
        window.findChild<QPushButton*>(QStringLiteral("startButton"));
    auto* cancelButton =
        window.findChild<QPushButton*>(QStringLiteral("cancelButton"));

    QVERIFY(directoryInput);
    QVERIFY(workerInput);
    QVERIFY(startButton);
    QVERIFY(cancelButton);
    QVERIFY(!startButton->isEnabled());
    QVERIFY(!cancelButton->isEnabled());

    directoryInput->setText(directory.path());
    QVERIFY(startButton->isEnabled());
    QTest::mouseClick(startButton, Qt::LeftButton);

    QVERIFY(!directoryInput->isEnabled());
    QVERIFY(!workerInput->isEnabled());
    QVERIFY(!startButton->isEnabled());
    QVERIFY(cancelButton->isEnabled());
}

void ScanControllerTest::progressEventsMapToWindowTextAndRange() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    auto execution = std::make_unique<FakeScanExecution>();
    FakeScanExecution* executionPointer = execution.get();
    MainWindow window{std::move(execution)};
    auto* directoryInput =
        window.findChild<QLineEdit*>(QStringLiteral("directoryLineEdit"));
    auto* startButton =
        window.findChild<QPushButton*>(QStringLiteral("startButton"));
    auto* statusLabel =
        window.findChild<QLabel*>(QStringLiteral("statusLabel"));
    auto* progressBar =
        window.findChild<QProgressBar*>(QStringLiteral("progressBar"));
    auto* progressDetails = window.findChild<QLabel*>(
        QStringLiteral("progressDetailsLabel"));
    QVERIFY(directoryInput);
    QVERIFY(startButton);
    QVERIFY(statusLabel);
    QVERIFY(progressBar);
    QVERIFY(progressDetails);

    directoryInput->setText(directory.path());
    QTest::mouseClick(startButton, Qt::LeftButton);
    const quint64 scanId = executionPointer->requests.front().scanId;

    executionPointer->sendProgress(
        scanId,
        {
            ScanProgressStage::Discovery,
            3U,
            0U,
            0U,
            0U,
            false});
    QCOMPARE(statusLabel->text(), QStringLiteral("正在发现文件"));
    QCOMPARE(progressBar->minimum(), 0);
    QCOMPARE(progressBar->maximum(), 0);
    QCOMPARE(
        progressDetails->text(),
        QStringLiteral("已发现 3 个文件"));

    executionPointer->sendProgress(
        scanId,
        {
            ScanProgressStage::Metadata,
            1U,
            4U,
            0U,
            0U,
            false});
    QCOMPARE(statusLabel->text(), QStringLiteral("正在读取文件信息"));
    QCOMPARE(progressBar->maximum(), 1000);
    QCOMPARE(progressBar->value(), 250);
    QCOMPARE(progressDetails->text(), QStringLiteral("1 / 4 个文件"));

    constexpr std::uintmax_t kEightEiB = std::uintmax_t{1U} << 63U;
    executionPointer->sendProgress(
        scanId,
        {
            ScanProgressStage::Hashing,
            1U,
            2U,
            kEightEiB / 2U,
            kEightEiB,
            false});
    QCOMPARE(statusLabel->text(), QStringLiteral("正在计算内容指纹"));
    QCOMPARE(progressBar->value(), 500);
    QVERIFY(progressDetails->text().contains(
        QString::number(kEightEiB)));

    executionPointer->sendProgress(
        scanId,
        {
            ScanProgressStage::Hashing,
            1U,
            2U,
            0U,
            0U,
            false});
    QCOMPARE(progressBar->value(), 500);
    QCOMPARE(progressDetails->text(), QStringLiteral("1 / 2 个文件"));

    executionPointer->sendProgress(
        scanId,
        {
            ScanProgressStage::Grouping,
            1U,
            1U,
            0U,
            0U,
            true});
    QCOMPARE(statusLabel->text(), QStringLiteral("正在整理重复组"));
    QCOMPARE(progressBar->value(), 1000);
}

void ScanControllerTest::realBackgroundScanCompletesAndReportsProgress() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    createFile(directory.filePath(QStringLiteral("first.bin")), 2, 'a');
    createFile(directory.filePath(QStringLiteral("second.bin")), 2, 'a');

    auto execution = std::make_unique<ThreadedScanExecution>();
    ThreadedScanExecution* executionPointer = execution.get();
    ScanController controller{std::move(execution)};
    QSignalSpy progressSpy{
        &controller,
        &ScanController::scanProgressChanged};
    bool didGuiTimerRun = false;
    QTimer::singleShot(0, &controller, [&didGuiTimerRun]() {
        didGuiTimerRun = true;
    });

    controller.setDirectoryPath(directory.path());
    controller.setHashWorkerCount(2);
    controller.startScan();

    QTRY_COMPARE_WITH_TIMEOUT(
        controller.state(),
        ScanState::Completed,
        10000);
    QVERIFY(didGuiTimerRun);
    QVERIFY(!executionPointer->isRunning());
    const SharedScanReport report = controller.report();
    QVERIFY(report);
    QCOMPARE(report->duplicateGroups.size(), std::size_t{1U});
    QVERIFY(progressSpy.count() >= 4);

    std::vector<ScanProgressStage> stages;
    for (const QList<QVariant>& arguments : progressSpy) {
        const ScanProgress progress =
            qvariant_cast<ScanProgress>(arguments.at(0));
        if (stages.empty() || stages.back() != progress.stage) {
            stages.push_back(progress.stage);
        }
    }
    const std::vector<ScanProgressStage> expectedStages{
        ScanProgressStage::Discovery,
        ScanProgressStage::Metadata,
        ScanProgressStage::Hashing,
        ScanProgressStage::Grouping};
    QVERIFY(stages == expectedStages);
}

void ScanControllerTest::realBackgroundScanCanBeCancelled() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    createFile(directory.filePath(QStringLiteral("first.bin")), 4, 'a');
    createFile(directory.filePath(QStringLiteral("second.bin")), 4, 'a');

    auto execution = std::make_unique<ThreadedScanExecution>();
    ThreadedScanExecution* executionPointer = execution.get();
    ScanController controller{std::move(execution)};
    connect(
        executionPointer,
        &ScanExecution::progressChanged,
        executionPointer,
        [executionPointer](quint64, const ScanProgress&) {
            executionPointer->requestCancel();
        },
        Qt::DirectConnection);

    controller.setDirectoryPath(directory.path());
    controller.startScan();

    QTRY_COMPARE_WITH_TIMEOUT(
        controller.state(),
        ScanState::Cancelled,
        10000);
    QVERIFY(!controller.report());
    QVERIFY(!executionPointer->isRunning());
    QVERIFY(controller.canStart());
}

void ScanControllerTest::closingWindowCancelsAndReclaimsBackgroundThread() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    createFile(directory.filePath(QStringLiteral("first.bin")), 8, 'a');
    createFile(directory.filePath(QStringLiteral("second.bin")), 8, 'a');

    auto execution = std::make_unique<ThreadedScanExecution>();
    ThreadedScanExecution* executionPointer = execution.get();
    MainWindow window{std::move(execution)};
    auto* directoryInput =
        window.findChild<QLineEdit*>(QStringLiteral("directoryLineEdit"));
    auto* startButton =
        window.findChild<QPushButton*>(QStringLiteral("startButton"));
    QVERIFY(directoryInput);
    QVERIFY(startButton);

    window.show();
    directoryInput->setText(directory.path());
    QTest::mouseClick(startButton, Qt::LeftButton);
    QVERIFY(executionPointer->isRunning());
    window.close();

    QTRY_VERIFY_WITH_TIMEOUT(!window.isVisible(), 10000);
    QVERIFY(!executionPointer->isRunning());
}

void ScanControllerTest::destroyingExecutionReclaimsThreadObject() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    auto execution = std::make_unique<ThreadedScanExecution>();
    execution->start({1U, directory.path(), 1U});
    QThread* workerThread = execution->findChild<QThread*>();
    QVERIFY(workerThread);
    QSignalSpy destroyedSpy{workerThread, &QObject::destroyed};

    execution.reset();

    QCOMPARE(destroyedSpy.count(), 1);
}

void ScanControllerTest::resultSummaryUsesDocumentedStatistics() {
    ScanReport report;
    report.discoveredFileCount = 8U;
    report.successfulFileCount = 7U;
    report.totalLogicalBytes = 1000U;
    report.candidateFileCount = 5U;
    report.candidateBytes = 850U;
    report.hashedFileCount = 4U;
    report.duplicateGroups = {
        {100U, "first", {"a", "b", "c"}},
        {50U, "second", {"d", "e"}}};
    report.failures.push_back({
        "failed",
        FailureStage::Metadata,
        std::make_error_code(std::errc::permission_denied)});

    const ResultSummary summary = calculateResultSummary(report);

    QCOMPARE(summary.scannedFileCount, std::size_t{8U});
    QCOMPARE(summary.successfulFileCount, std::size_t{7U});
    QCOMPARE(summary.duplicateGroupCount, std::size_t{2U});
    QCOMPARE(summary.duplicateFileCount, std::size_t{5U});
    QCOMPARE(summary.redundantCopyCount, std::size_t{3U});
    QCOMPARE(summary.duplicateLogicalBytes, std::uintmax_t{400U});
    QCOMPARE(summary.reclaimableBytes, std::uintmax_t{250U});
    QCOMPARE(summary.failureCount, std::size_t{1U});
    QVERIFY(summary.conclusion.contains(QStringLiteral("理论可节省")));
}

void ScanControllerTest::resultSummaryHandlesZeroByteDuplicates() {
    ScanReport report;
    report.discoveredFileCount = 3U;
    report.successfulFileCount = 3U;
    report.duplicateGroups = {{
        0U,
        "empty",
        {"first", "second", "third"}}};

    const ResultSummary summary = calculateResultSummary(report);

    QCOMPARE(summary.duplicateGroupCount, std::size_t{1U});
    QCOMPARE(summary.duplicateFileCount, std::size_t{3U});
    QCOMPARE(summary.redundantCopyCount, std::size_t{2U});
    QCOMPARE(summary.duplicateLogicalBytes, std::uintmax_t{0U});
    QCOMPARE(summary.reclaimableBytes, std::uintmax_t{0U});
    QVERIFY(summary.conclusion.contains(QStringLiteral("1 组重复内容")));
    QVERIFY(summary.conclusion.contains(QStringLiteral("理论可节省 0 B")));
}

void ScanControllerTest::resultSummarySaturatesLargeByteTotals() {
    ScanReport report;
    const std::uintmax_t maximum =
        (std::numeric_limits<std::uintmax_t>::max)();
    report.duplicateGroups = {
        {maximum, "large", {"a", "b", "c"}}};

    const ResultSummary summary = calculateResultSummary(report);

    QCOMPARE(summary.duplicateLogicalBytes, maximum);
    QCOMPARE(summary.reclaimableBytes, maximum);
}

void ScanControllerTest::resultModelsExposeStableRowsAndRawSortValues() {
    auto report = std::make_shared<ScanReport>();
    report->duplicateGroups = {
        {100U, "first", {"a", "b"}},
        {70U, "second", {"c", "d", "e"}}};
    report->failures = {
        {
            "missing",
            FailureStage::Discovery,
            std::make_error_code(std::errc::no_such_file_or_directory)},
        {
            "locked",
            FailureStage::Hashing,
            std::make_error_code(std::errc::permission_denied)}};

    DuplicateGroupModel groupModel;
    groupModel.setReport(report);
    QCOMPARE(groupModel.rowCount(), 2);
    const QModelIndex firstSavingsIndex = groupModel.index(0, 4);
    QCOMPARE(
        firstSavingsIndex.data(DuplicateGroupModel::ReportGroupIndexRole)
            .toULongLong(),
        qulonglong{1U});
    QCOMPARE(
        firstSavingsIndex.data(DuplicateGroupModel::RawValueRole)
            .toULongLong(),
        qulonglong{140U});
    groupModel.sort(2, Qt::AscendingOrder);
    QCOMPARE(groupModel.reportGroupIndexForRow(0), std::size_t{1U});

    DuplicatePathModel pathModel;
    pathModel.setReport(report);
    pathModel.setGroupIndex(1U);
    QCOMPARE(pathModel.rowCount(), 3);
    QCOMPARE(pathModel.pathAt(0), QStringLiteral("c"));

    FailureModel failureModel;
    failureModel.setReport(report);
    QCOMPARE(failureModel.rowCount(), 2);
    QVERIFY(failureModel.stageSummary().contains(
        QStringLiteral("文件发现 1")));
    QVERIFY(failureModel.stageSummary().contains(
        QStringLiteral("内容指纹 1")));
}

void ScanControllerTest::duplicateGroupModelExposesAllColumnsAndRoles() {
    auto report = std::make_shared<ScanReport>();
    report->duplicateGroups = {
        {0U, "empty", {"zero-a", "zero-b", "zero-c"}},
        {1024U, "large", {"large-a", "large-b"}}};

    DuplicateGroupModel model;
    model.setReport(report);

    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.columnCount(), 5);
    QCOMPARE(
        model.headerData(0, Qt::Horizontal).toString(),
        QStringLiteral("重复组"));
    QCOMPARE(
        model.headerData(4, Qt::Horizontal).toString(),
        QStringLiteral("理论可节省"));
    QCOMPARE(model.reportGroupIndexForRow(0), std::size_t{1U});
    QCOMPARE(model.index(0, 0).data().toULongLong(), qulonglong{2U});
    QCOMPARE(model.index(0, 1).data().toULongLong(), qulonglong{2U});
    QCOMPARE(model.index(0, 2).data().toString(), QStringLiteral("1.0 KiB"));
    QCOMPARE(model.index(0, 3).data().toULongLong(), qulonglong{1U});
    QCOMPARE(model.index(0, 4).data().toString(), QStringLiteral("1.0 KiB"));
    QCOMPARE(
        model.index(0, 2).data(DuplicateGroupModel::RawValueRole)
            .toULongLong(),
        qulonglong{1024U});
    QVERIFY(
        model.index(0, 4).data(Qt::ToolTipRole).toString().contains(
            QStringLiteral("未考虑硬链接")));

    model.sort(1, Qt::DescendingOrder);
    QCOMPARE(model.reportGroupIndexForRow(0), std::size_t{0U});
    QCOMPARE(
        model.index(0, 4).data(DuplicateGroupModel::RawValueRole)
            .toULongLong(),
        qulonglong{0U});
}

void ScanControllerTest::failureModelExposesRowsColumnsAndErrorText() {
    auto report = std::make_shared<ScanReport>();
    report->failures = {
        {
            std::filesystem::path{L"中文 发现"},
            FailureStage::Discovery,
            std::make_error_code(std::errc::no_such_file_or_directory)},
        {
            std::filesystem::path{L"中文 信息"},
            FailureStage::Metadata,
            std::make_error_code(std::errc::permission_denied)},
        {
            std::filesystem::path{L"中文 指纹"},
            FailureStage::Hashing,
            std::make_error_code(std::errc::io_error)}};

    FailureModel model;
    model.setReport(report);

    QCOMPARE(model.rowCount(), 3);
    QCOMPARE(model.columnCount(), 3);
    QCOMPARE(
        model.headerData(0, Qt::Horizontal).toString(),
        QStringLiteral("处理阶段"));
    QCOMPARE(
        model.headerData(1, Qt::Horizontal).toString(),
        QStringLiteral("文件路径"));
    QCOMPARE(
        model.headerData(2, Qt::Horizontal).toString(),
        QStringLiteral("错误原因"));
    QCOMPARE(
        model.index(0, 0).data().toString(),
        QStringLiteral("文件发现"));
    QCOMPARE(
        model.index(1, 0).data().toString(),
        QStringLiteral("读取信息"));
    QCOMPARE(
        model.index(2, 0).data().toString(),
        QStringLiteral("内容指纹"));
    QCOMPARE(
        model.index(2, 1).data().toString(),
        QStringLiteral("中文 指纹"));
    QVERIFY(!model.index(2, 2).data().toString().isEmpty());
    QVERIFY(model.index(2, 2).data().toString().indexOf(
        QRegularExpression{QStringLiteral("[\\r\\n]")}) < 0);
    QVERIFY(model.index(2, 1).data(Qt::ToolTipRole).toString().contains(
        QStringLiteral("中文 指纹")));
    QCOMPARE(
        model.stageSummary(),
        QStringLiteral("文件发现 1，读取信息 1，内容指纹 1"));
}

void ScanControllerTest::resultModelsHandleManyRowsWithoutCopyingReports() {
    auto report = std::make_shared<ScanReport>();
    constexpr std::size_t kGroupCount = 5000U;
    report->duplicateGroups.reserve(kGroupCount);
    for (std::size_t index = 0; index < kGroupCount; ++index) {
        report->duplicateGroups.push_back({
            static_cast<std::uintmax_t>(index),
            "hash",
            {
                std::filesystem::path{"first"},
                std::filesystem::path{"second"}}});
    }

    DuplicateGroupModel groupModel;
    groupModel.setReport(report);
    QCOMPARE(groupModel.rowCount(), static_cast<int>(kGroupCount));
    QCOMPARE(
        groupModel.reportGroupIndexForRow(0),
        kGroupCount - 1U);

    DuplicatePathModel pathModel;
    pathModel.setReport(report);
    pathModel.setGroupIndex(kGroupCount - 1U);
    QCOMPARE(pathModel.rowCount(), 2);
}

void ScanControllerTest::mainWindowShowsCompleteReportAndHidesCancelledResults() {
    QTemporaryDir directory;
    auto execution = std::make_unique<FakeScanExecution>();
    FakeScanExecution* executionPointer = execution.get();
    MainWindow window{std::move(execution)};
    auto* directoryInput =
        window.findChild<QLineEdit*>(QStringLiteral("directoryLineEdit"));
    auto* startButton =
        window.findChild<QPushButton*>(QStringLiteral("startButton"));
    auto* resultGroup =
        window.findChild<QGroupBox*>(QStringLiteral("resultGroup"));
    auto* groupTable =
        window.findChild<QTableView*>(QStringLiteral("duplicateGroupTable"));
    auto* conclusion =
        window.findChild<QLabel*>(QStringLiteral("conclusionLabel"));
    QVERIFY(directoryInput);
    QVERIFY(startButton);
    QVERIFY(resultGroup);
    QVERIFY(groupTable);
    QVERIFY(conclusion);

    directoryInput->setText(directory.path());
    QTest::mouseClick(startButton, Qt::LeftButton);
    auto report = std::make_shared<ScanReport>();
    report->discoveredFileCount = 2U;
    report->successfulFileCount = 2U;
    report->duplicateGroups = {
        {4U, "same", {"first", "second"}}};
    executionPointer->finishCompleted(1U, report);

    QVERIFY(!resultGroup->isHidden());
    QCOMPARE(groupTable->model()->rowCount(), 1);
    QVERIFY(conclusion->text().contains(QStringLiteral("1 组重复内容")));

    QTest::mouseClick(startButton, Qt::LeftButton);
    executionPointer->finishCancelled(2U);
    QVERIFY(!resultGroup->isVisible());

    QTest::mouseClick(startButton, Qt::LeftButton);
    executionPointer->finishCompleted(
        3U,
        std::make_shared<ScanReport>());
    auto* duplicateEmpty = window.findChild<QLabel*>(
        QStringLiteral("duplicateEmptyLabel"));
    auto* failureEmpty = window.findChild<QLabel*>(
        QStringLiteral("failureEmptyLabel"));
    QVERIFY(duplicateEmpty);
    QVERIFY(failureEmpty);
    QVERIFY(!duplicateEmpty->isHidden());
    QVERIFY(!failureEmpty->isHidden());
}

void ScanControllerTest::sortingDuplicateGroupsPreservesSelectedGroup() {
    QTemporaryDir directory;
    auto execution = std::make_unique<FakeScanExecution>();
    FakeScanExecution* executionPointer = execution.get();
    MainWindow window{std::move(execution)};
    auto* directoryInput =
        window.findChild<QLineEdit*>(QStringLiteral("directoryLineEdit"));
    auto* startButton =
        window.findChild<QPushButton*>(QStringLiteral("startButton"));
    auto* groupTable =
        window.findChild<QTableView*>(QStringLiteral("duplicateGroupTable"));
    auto* pathTable =
        window.findChild<QTableView*>(QStringLiteral("duplicatePathTable"));
    QVERIFY(directoryInput);
    QVERIFY(startButton);
    QVERIFY(groupTable);
    QVERIFY(pathTable);

    directoryInput->setText(directory.path());
    QTest::mouseClick(startButton, Qt::LeftButton);
    auto report = std::make_shared<ScanReport>();
    report->duplicateGroups = {
        {100U, "first", {"a", "b"}},
        {70U, "second", {"c", "d", "e"}}};
    executionPointer->finishCompleted(1U, report);

    groupTable->selectRow(1);
    QCOMPARE(
        groupTable->currentIndex()
            .data(DuplicateGroupModel::ReportGroupIndexRole)
            .toULongLong(),
        qulonglong{0U});
    QCOMPARE(
        pathTable->model()->index(0, 0).data().toString(),
        QStringLiteral("a"));

    groupTable->sortByColumn(2, Qt::DescendingOrder);

    QCOMPARE(
        groupTable->currentIndex()
            .data(DuplicateGroupModel::ReportGroupIndexRole)
            .toULongLong(),
        qulonglong{0U});
    QCOMPARE(groupTable->currentIndex().row(), 0);
    QCOMPARE(
        pathTable->model()->index(0, 0).data().toString(),
        QStringLiteral("a"));
}

}
}

QTEST_MAIN(batchguard::gui::ScanControllerTest)

#include "scan_controller_test.moc"
