#include "duplicate_group_model.h"
#include "duplicate_path_model.h"
#include "failure_model.h"
#include "main_window.h"
#include "result_summary.h"
#include "scan_controller.h"
#include "scan_execution.h"
#include "threaded_scan_execution.h"

#include <QFile>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
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
    void invalidDirectoryCannotStart();
    void validDirectoryStartsOnlyOnceAndLocksInputs();
    void cancelRequestIsSentOnlyOnce();
    void completionAfterCancellationIsTreatedAsCancelled();
    void completionCancellationAndFailureReachTerminalStates();
    void staleCompletionCannotReplaceCurrentScan();
    void mainWindowRendersControllerAvailability();
    void realBackgroundScanCompletesAndReportsProgress();
    void realBackgroundScanCanBeCancelled();
    void closingWindowCancelsAndReclaimsBackgroundThread();
    void destroyingExecutionReclaimsThreadObject();
    void resultSummaryUsesDocumentedStatistics();
    void resultSummarySaturatesLargeByteTotals();
    void resultModelsExposeStableRowsAndRawSortValues();
    void resultModelsHandleManyRowsWithoutCopyingReports();
    void mainWindowShowsCompleteReportAndHidesCancelledResults();
    void sortingDuplicateGroupsPreservesSelectedGroup();
};

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
