#include "gui_log_model.h"

#include <QDateTime>
#include <QString>

#include <chrono>

namespace batchguard::gui {
namespace {

QString simplifiedLayerName(LogLayer layer) {
    switch (layer) {
    case LogLayer::CoreDiscovery:
        return QStringLiteral("文件发现");
    case LogLayer::CoreMetadata:
        return QStringLiteral("文件信息");
    case LogLayer::CoreHashing:
        return QStringLiteral("内容指纹");
    case LogLayer::CoreGrouping:
        return QStringLiteral("重复分组");
    case LogLayer::Cli:
        return QStringLiteral("命令行");
    case LogLayer::GuiController:
        return QStringLiteral("扫描控制");
    case LogLayer::GuiExecution:
        return QStringLiteral("后台执行");
    }
    return QStringLiteral("应用");
}

}

GuiLogModel::GuiLogModel(QObject* parent)
    : QAbstractListModel{parent} {
}

int GuiLogModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(records_.size());
}

QVariant GuiLogModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() ||
        index.row() < 0 ||
        index.row() >= rowCount()) {
        return {};
    }
    const LogRecord& record =
        records_[static_cast<std::size_t>(index.row())];
    switch (role) {
    case Qt::DisplayRole:
    case Qt::ToolTipRole:
        return simplifiedText(record);
    case FullTextRole:
        return QString::fromUtf8(formatLogRecord(record).c_str());
    case LogLevelRole:
        return static_cast<int>(record.level);
    case LogLayerRole:
        return QString::fromLatin1(
            logLayerName(record.layer).data(),
            static_cast<int>(logLayerName(record.layer).size()));
    case SummaryRole:
        return QString::fromUtf8(record.summary.c_str());
    default:
        return {};
    }
}

void GuiLogModel::appendRecord(const LogRecord& record) {
    if (record.level == LogLevel::Debug) {
        return;
    }
    if (records_.size() == kMaximumRecordCount) {
        beginRemoveRows({}, 0, 0);
        records_.erase(records_.begin());
        endRemoveRows();
    }
    const int newRow = static_cast<int>(records_.size());
    beginInsertRows({}, newRow, newRow);
    records_.push_back(record);
    endInsertRows();
}

void GuiLogModel::clear() {
    if (records_.empty()) {
        return;
    }
    beginResetModel();
    records_.clear();
    endResetModel();
}

QString GuiLogModel::simplifiedText(const LogRecord& record) {
    const auto milliseconds = std::chrono::duration_cast<
        std::chrono::milliseconds>(record.timestamp.time_since_epoch());
    const QString timeText =
        QDateTime::fromMSecsSinceEpoch(milliseconds.count())
            .toLocalTime()
            .toString(QStringLiteral("HH:mm:ss"));
    const std::string_view levelName = logLevelName(record.level);
    return QStringLiteral("%1：%2：%3：%4")
        .arg(
            timeText,
            QString::fromLatin1(
                levelName.data(),
                static_cast<int>(levelName.size())),
            simplifiedLayerName(record.layer),
            QString::fromUtf8(record.summary.c_str()));
}

}
