#include "threaded_scan_execution.h"

#include "batchguard/core/scanner.h"

#include <QMutexLocker>
#include <QThread>

#include <chrono>
#include <exception>
#include <filesystem>
#include <memory>
#include <optional>
#include <utility>

namespace batchguard::gui {
namespace {

constexpr auto kProgressInterval = std::chrono::milliseconds{100};

}

ThreadedScanExecution::ThreadedScanExecution(QObject* parent)
    : ScanExecution{parent} {
    qRegisterMetaType<ScanProgress>("batchguard::ScanProgress");
    qRegisterMetaType<SharedScanReport>("batchguard::gui::SharedScanReport");
}

ThreadedScanExecution::~ThreadedScanExecution() {
    requestCancel();
    if (thread_) {
        thread_->wait();
    }
}

void ThreadedScanExecution::start(const ScanRequest& request) {
    if (thread_) {
        return;
    }

    stopSource_ = std::stop_source{};
    {
        const QMutexLocker lock{&resultMutex_};
        pendingStatus_ = PendingStatus::None;
        pendingScanId_ = request.scanId;
        pendingReport_.reset();
        pendingError_.clear();
    }

    const std::stop_token stopToken = stopSource_.get_token();
    thread_ = QThread::create([this, request, stopToken]() {
        runScan(request, stopToken);
    });
    connect(
        thread_,
        &QThread::finished,
        this,
        &ThreadedScanExecution::handleThreadFinished);
    thread_->start();
}

void ThreadedScanExecution::requestCancel() {
    stopSource_.request_stop();
}

bool ThreadedScanExecution::isRunning() const noexcept {
    return thread_ != nullptr;
}

void ThreadedScanExecution::handleThreadFinished() {
    QThread* finishedThread = thread_;
    if (!finishedThread) {
        return;
    }
    finishedThread->wait();
    thread_ = nullptr;
    finishedThread->deleteLater();

    PendingStatus status;
    quint64 scanId;
    SharedScanReport report;
    QString errorMessage;
    {
        const QMutexLocker lock{&resultMutex_};
        status = pendingStatus_;
        scanId = pendingScanId_;
        report = std::move(pendingReport_);
        errorMessage = std::move(pendingError_);
        pendingStatus_ = PendingStatus::None;
    }

    switch (status) {
    case PendingStatus::Completed:
        emit completed(scanId, report);
        break;
    case PendingStatus::Cancelled:
        emit cancelled(scanId);
        break;
    case PendingStatus::Failed:
        emit failed(scanId, errorMessage);
        break;
    case PendingStatus::None:
        emit failed(scanId, QStringLiteral("扫描线程未返回有效结果。"));
        break;
    }
}

void ThreadedScanExecution::runScan(
    ScanRequest request,
    std::stop_token stopToken) {
    try {
        std::optional<ScanProgressStage> lastStage;
        auto lastProgressTime =
            std::chrono::steady_clock::now() - kProgressInterval;
        const ScanProgressCallback progressCallback =
            [this,
             scanId = request.scanId,
             &lastStage,
             &lastProgressTime](const ScanProgress& progress) {
                const auto currentTime = std::chrono::steady_clock::now();
                const bool isStageChange =
                    !lastStage.has_value() || *lastStage != progress.stage;
                const bool hasIntervalElapsed =
                    currentTime - lastProgressTime >= kProgressInterval;
                if (!isStageChange &&
                    !progress.isStageComplete &&
                    !hasIntervalElapsed) {
                    return;
                }

                lastStage = progress.stage;
                lastProgressTime = currentTime;
                emit progressChanged(scanId, progress);
            };

        ScanResult result = scanDirectory(
            std::filesystem::path{request.directoryPath.toStdWString()},
            ScanOptions{request.hashWorkerCount},
            progressCallback,
            stopToken);
        if (result.isCancelled()) {
            storeCancelled();
            return;
        }
        if (!result.report.has_value()) {
            storeFailed(QStringLiteral("核心扫描没有返回完整报告。"));
            return;
        }
        storeCompleted(std::make_shared<const ScanReport>(
            std::move(*result.report)));
    } catch (const std::exception& exception) {
        storeFailed(QString::fromUtf8(exception.what()));
    } catch (...) {
        storeFailed(QStringLiteral("扫描遇到未知错误。"));
    }
}

void ThreadedScanExecution::storeCompleted(SharedScanReport report) {
    const QMutexLocker lock{&resultMutex_};
    pendingStatus_ = PendingStatus::Completed;
    pendingReport_ = std::move(report);
}

void ThreadedScanExecution::storeCancelled() {
    const QMutexLocker lock{&resultMutex_};
    pendingStatus_ = PendingStatus::Cancelled;
}

void ThreadedScanExecution::storeFailed(QString message) {
    const QMutexLocker lock{&resultMutex_};
    pendingStatus_ = PendingStatus::Failed;
    pendingError_ = std::move(message);
}

}
