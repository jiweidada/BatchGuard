#pragma once

#include "scan_execution.h"
#include "scan_state.h"

#include "batchguard/logging/logging.h"

#include <QElapsedTimer>
#include <QObject>
#include <QString>

#include <memory>
#include <optional>

namespace batchguard::gui {

// 维护 GUI 扫描状态、输入有效性和当前结果，是界面状态的唯一来源。
class ScanController final : public QObject {
    Q_OBJECT

public:
    explicit ScanController(
        std::unique_ptr<ScanExecution> execution,
        QObject* parent = nullptr);
    ~ScanController() override;

    [[nodiscard]] ScanState state() const noexcept;
    [[nodiscard]] QString directoryPath() const;
    [[nodiscard]] int hashWorkerCount() const noexcept;
    [[nodiscard]] bool isDirectoryValid() const noexcept;
    [[nodiscard]] QString validationMessage() const;
    [[nodiscard]] QString statusMessage() const;
    [[nodiscard]] QString failureMessage() const;
    [[nodiscard]] bool canEditInputs() const noexcept;
    [[nodiscard]] bool canStart() const noexcept;
    [[nodiscard]] bool canCancel() const noexcept;
    [[nodiscard]] quint64 currentScanId() const noexcept;
    [[nodiscard]] qint64 elapsedMilliseconds() const noexcept;
    [[nodiscard]] SharedScanReport report() const;

public slots:
    void setDirectoryPath(const QString& directoryPath);
    void setHashWorkerCount(int hashWorkerCount);
    void startScan();
    void cancelScan();

signals:
    void viewStateChanged();
    void scanProgressChanged(const batchguard::ScanProgress& progress);
    void reportChanged(const batchguard::gui::SharedScanReport& report);
    void logRecordCreated(const batchguard::LogRecord& record);

private slots:
    void handleCompleted(
        quint64 scanId,
        const batchguard::gui::SharedScanReport& report);
    void handleCancelled(quint64 scanId);
    void handleFailed(quint64 scanId, const QString& message);
    void handleProgress(
        quint64 scanId,
        const batchguard::ScanProgress& progress);

private:
    void validateDirectory();
    void setState(ScanState state);
    void publishLog(LogRecord record);
    [[nodiscard]] bool acceptsEvent(quint64 scanId) const noexcept;

    std::unique_ptr<ScanExecution> execution_;
    ScanState state_{ScanState::Idle};
    QString directoryPath_;
    int hashWorkerCount_{1};
    bool isDirectoryValid_{};
    QString validationMessage_;
    QString failureMessage_;
    quint64 currentScanId_{};
    QElapsedTimer scanTimer_;
    qint64 elapsedMilliseconds_{};
    SharedScanReport report_;
    std::optional<ScanProgressStage> lastLoggedProgressStage_;
};

}

Q_DECLARE_METATYPE(batchguard::LogRecord)
