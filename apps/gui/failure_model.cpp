#include "failure_model.h"

#include <QString>

#include <array>
#include <utility>

namespace batchguard::gui {
namespace {

QString failureStageName(FailureStage stage) {
    switch (stage) {
    case FailureStage::Discovery:
        return QStringLiteral("文件发现");
    case FailureStage::Metadata:
        return QStringLiteral("读取信息");
    case FailureStage::Hashing:
        return QStringLiteral("内容指纹");
    }
    return QStringLiteral("未知阶段");
}

}

FailureModel::FailureModel(QObject* parent)
    : QAbstractTableModel{parent} {
}

int FailureModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() || !report_
        ? 0
        : static_cast<int>(report_->failures.size());
}

int FailureModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : 3;
}

QVariant FailureModel::data(
    const QModelIndex& index,
    int role) const {
    if (!report_ ||
        !index.isValid() ||
        index.row() < 0 ||
        index.row() >= rowCount() ||
        index.column() < 0 ||
        index.column() >= columnCount()) {
        return {};
    }
    const FileFailure& failure =
        report_->failures[static_cast<std::size_t>(index.row())];
    const QString path = QString::fromStdWString(failure.path.wstring());
    QString reason =
        QString::fromLocal8Bit(failure.errorCode.message().c_str());
    reason.replace(QLatin1Char('\r'), QLatin1Char(' '));
    reason.replace(QLatin1Char('\n'), QLatin1Char(' '));
    if (role == Qt::ToolTipRole) {
        return QStringLiteral("%1\n%2")
            .arg(path, reason);
    }
    if (role != Qt::DisplayRole) {
        return {};
    }
    switch (index.column()) {
    case 0:
        return failureStageName(failure.stage);
    case 1:
        return path;
    case 2:
        return reason;
    default:
        return {};
    }
}

QVariant FailureModel::headerData(
    int section,
    Qt::Orientation orientation,
    int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return QAbstractTableModel::headerData(section, orientation, role);
    }
    switch (section) {
    case 0:
        return QStringLiteral("处理阶段");
    case 1:
        return QStringLiteral("文件路径");
    case 2:
        return QStringLiteral("错误原因");
    default:
        return {};
    }
}

void FailureModel::setReport(SharedScanReport report) {
    beginResetModel();
    report_ = std::move(report);
    endResetModel();
}

QString FailureModel::stageSummary() const {
    std::array<std::size_t, 3> counts{};
    if (report_) {
        for (const FileFailure& failure : report_->failures) {
            switch (failure.stage) {
            case FailureStage::Discovery:
                ++counts[0];
                break;
            case FailureStage::Metadata:
                ++counts[1];
                break;
            case FailureStage::Hashing:
                ++counts[2];
                break;
            }
        }
    }
    return QStringLiteral("文件发现 %1，读取信息 %2，内容指纹 %3")
        .arg(counts[0])
        .arg(counts[1])
        .arg(counts[2]);
}

}
