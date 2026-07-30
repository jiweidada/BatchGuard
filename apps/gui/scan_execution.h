#pragma once

#include "batchguard/core/scan_progress.h"
#include "batchguard/core/scan_report.h"

#include <QObject>
#include <QString>

#include <cstddef>
#include <memory>

namespace batchguard::gui {

using SharedScanReport = std::shared_ptr<const ScanReport>;

// 保存一次 GUI 扫描请求。`scanId` 用于隔离已经结束的旧扫描事件。
struct ScanRequest {
    quint64 scanId{};
    QString directoryPath;
    std::size_t hashWorkerCount{};
};

// 定义控制器与扫描实现之间的可替换边界。
// 实现可以异步工作，但必须通过信号把事件送回控制器。
class ScanExecution : public QObject {
    Q_OBJECT

public:
    explicit ScanExecution(QObject* parent = nullptr)
        : QObject{parent} {
    }

    ~ScanExecution() override = default;

    virtual void start(const ScanRequest& request) = 0;
    virtual void requestCancel() = 0;

signals:
    void progressChanged(
        quint64 scanId,
        const batchguard::ScanProgress& progress);
    void completed(
        quint64 scanId,
        const batchguard::gui::SharedScanReport& report);
    void cancelled(quint64 scanId);
    void failed(quint64 scanId, const QString& message);
};

}

Q_DECLARE_METATYPE(batchguard::ScanProgress)
Q_DECLARE_METATYPE(batchguard::gui::SharedScanReport)
