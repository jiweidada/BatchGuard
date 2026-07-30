#include "scan_controller.h"

#include "batchguard/core/input_validator.h"

#include <QThread>

#include <algorithm>
#include <filesystem>
#include <utility>

namespace batchguard::gui {
namespace {

QString validationMessage(const InputValidationResult& result) {
    switch (result.error) {
    case InputValidationError::None:
        return QStringLiteral("目录有效，可以开始扫描。");
    case InputValidationError::EmptyPath:
        return QStringLiteral("请选择需要扫描的目录。");
    case InputValidationError::CannotAccess:
        return QStringLiteral("无法访问该路径：%1")
            .arg(QString::fromLocal8Bit(result.errorCode.message().c_str()));
    case InputValidationError::DoesNotExist:
        return QStringLiteral("指定的目录不存在。");
    case InputValidationError::NotDirectory:
        return QStringLiteral("指定路径不是目录。");
    }
    return QStringLiteral("目录校验失败。");
}

}

ScanController::ScanController(
    std::unique_ptr<ScanExecution> execution,
    QObject* parent)
    : QObject{parent},
      execution_{std::move(execution)},
      hashWorkerCount_{std::clamp(QThread::idealThreadCount(), 1, 64)} {
    Q_ASSERT(execution_);
    connect(
        execution_.get(),
        &ScanExecution::completed,
        this,
        &ScanController::handleCompleted);
    connect(
        execution_.get(),
        &ScanExecution::cancelled,
        this,
        &ScanController::handleCancelled);
    connect(
        execution_.get(),
        &ScanExecution::failed,
        this,
        &ScanController::handleFailed);
    connect(
        execution_.get(),
        &ScanExecution::progressChanged,
        this,
        &ScanController::handleProgress);
    validateDirectory();
}

ScanController::~ScanController() = default;

ScanState ScanController::state() const noexcept {
    return state_;
}

QString ScanController::directoryPath() const {
    return directoryPath_;
}

int ScanController::hashWorkerCount() const noexcept {
    return hashWorkerCount_;
}

bool ScanController::isDirectoryValid() const noexcept {
    return isDirectoryValid_;
}

QString ScanController::validationMessage() const {
    return validationMessage_;
}

QString ScanController::statusMessage() const {
    switch (state_) {
    case ScanState::Idle:
        return QStringLiteral("等待开始扫描");
    case ScanState::Scanning:
        return QStringLiteral("正在扫描");
    case ScanState::Cancelling:
        return QStringLiteral("正在安全取消…");
    case ScanState::Completed:
        return report_ && !report_->isComplete()
            ? QStringLiteral("扫描完成，存在部分失败")
            : QStringLiteral("扫描完成");
    case ScanState::Cancelled:
        return QStringLiteral("扫描已取消，可以重新开始");
    case ScanState::Failed:
        return QStringLiteral("扫描失败");
    }
    return {};
}

QString ScanController::failureMessage() const {
    return failureMessage_;
}

bool ScanController::canEditInputs() const noexcept {
    return state_ != ScanState::Scanning && state_ != ScanState::Cancelling;
}

bool ScanController::canStart() const noexcept {
    return canEditInputs() && isDirectoryValid_;
}

bool ScanController::canCancel() const noexcept {
    return state_ == ScanState::Scanning;
}

quint64 ScanController::currentScanId() const noexcept {
    return currentScanId_;
}

qint64 ScanController::elapsedMilliseconds() const noexcept {
    return elapsedMilliseconds_;
}

SharedScanReport ScanController::report() const {
    return report_;
}

void ScanController::setDirectoryPath(const QString& directoryPath) {
    if (!canEditInputs() || directoryPath_ == directoryPath) {
        return;
    }
    directoryPath_ = directoryPath.trimmed();
    validateDirectory();
    emit viewStateChanged();
}

void ScanController::setHashWorkerCount(int hashWorkerCount) {
    if (!canEditInputs()) {
        return;
    }
    const int boundedWorkerCount = std::clamp(hashWorkerCount, 1, 64);
    if (hashWorkerCount_ == boundedWorkerCount) {
        return;
    }
    hashWorkerCount_ = boundedWorkerCount;
    emit viewStateChanged();
}

void ScanController::startScan() {
    if (!canStart()) {
        return;
    }

    ++currentScanId_;
    failureMessage_.clear();
    elapsedMilliseconds_ = 0;
    scanTimer_.start();
    report_.reset();
    emit reportChanged({});
    setState(ScanState::Scanning);
    execution_->start({
        currentScanId_,
        directoryPath_,
        static_cast<std::size_t>(hashWorkerCount_)});
}

void ScanController::cancelScan() {
    if (!canCancel()) {
        return;
    }
    setState(ScanState::Cancelling);
    execution_->requestCancel();
}

void ScanController::handleCompleted(
    quint64 scanId,
    const SharedScanReport& report) {
    if (!acceptsEvent(scanId) || !report) {
        return;
    }
    report_ = report;
    elapsedMilliseconds_ = scanTimer_.elapsed();
    failureMessage_.clear();
    setState(ScanState::Completed);
    emit reportChanged(report_);
}

void ScanController::handleCancelled(quint64 scanId) {
    if (!acceptsEvent(scanId)) {
        return;
    }
    report_.reset();
    elapsedMilliseconds_ = scanTimer_.elapsed();
    failureMessage_.clear();
    setState(ScanState::Cancelled);
}

void ScanController::handleFailed(quint64 scanId, const QString& message) {
    if (!acceptsEvent(scanId)) {
        return;
    }
    report_.reset();
    elapsedMilliseconds_ = scanTimer_.elapsed();
    failureMessage_ = message.isEmpty()
        ? QStringLiteral("扫描遇到未预期错误。")
        : message;
    setState(ScanState::Failed);
}

void ScanController::handleProgress(
    quint64 scanId,
    const ScanProgress& progress) {
    if (!acceptsEvent(scanId) || state_ != ScanState::Scanning) {
        return;
    }
    emit scanProgressChanged(progress);
}

void ScanController::validateDirectory() {
    const std::filesystem::path path{directoryPath_.toStdWString()};
    const InputValidationResult result = validateInputDirectory(path);
    isDirectoryValid_ = result.isValid();
    validationMessage_ = ::batchguard::gui::validationMessage(result);
}

void ScanController::setState(ScanState state) {
    if (state_ == state) {
        return;
    }
    state_ = state;
    emit viewStateChanged();
}

bool ScanController::acceptsEvent(quint64 scanId) const noexcept {
    const bool isActive =
        state_ == ScanState::Scanning || state_ == ScanState::Cancelling;
    return isActive && scanId == currentScanId_;
}

}
