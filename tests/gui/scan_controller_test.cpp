#include "main_window.h"
#include "scan_controller.h"
#include "scan_execution.h"

#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>
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

    void finishCompleted(quint64 scanId) {
        emit completed(scanId, std::make_shared<ScanReport>());
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

class ScanControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void invalidDirectoryCannotStart();
    void validDirectoryStartsOnlyOnceAndLocksInputs();
    void cancelRequestIsSentOnlyOnce();
    void completionCancellationAndFailureReachTerminalStates();
    void staleCompletionCannotReplaceCurrentScan();
    void mainWindowRendersControllerAvailability();
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

}
}

QTEST_MAIN(batchguard::gui::ScanControllerTest)

#include "scan_controller_test.moc"
