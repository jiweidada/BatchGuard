#pragma once

#include "scan_execution.h"

#include <QMutex>

#include <stop_token>

QT_BEGIN_NAMESPACE
class QThread;
QT_END_NAMESPACE

namespace batchguard::gui {

// 在一个专用 Qt 线程中调用同步核心扫描，并管理取消和线程回收。
class ThreadedScanExecution final : public ScanExecution {
    Q_OBJECT

public:
    explicit ThreadedScanExecution(QObject* parent = nullptr);
    ~ThreadedScanExecution() override;

    void start(const ScanRequest& request) override;
    void requestCancel() override;

    [[nodiscard]] bool isRunning() const noexcept;

private slots:
    void handleThreadFinished();

private:
    enum class PendingStatus {
        None,
        Completed,
        Cancelled,
        Failed
    };

    void runScan(ScanRequest request, std::stop_token stopToken);
    void storeCompleted(SharedScanReport report);
    void storeCancelled();
    void storeFailed(QString message);

    QThread* thread_{};
    std::stop_source stopSource_;
    mutable QMutex resultMutex_;
    PendingStatus pendingStatus_{PendingStatus::None};
    quint64 pendingScanId_{};
    SharedScanReport pendingReport_;
    QString pendingError_;
};

}
