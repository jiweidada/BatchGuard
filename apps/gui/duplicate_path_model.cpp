#include "duplicate_path_model.h"

#include <QString>

#include <utility>

namespace batchguard::gui {

DuplicatePathModel::DuplicatePathModel(QObject* parent)
    : QAbstractTableModel{parent} {
}

int DuplicatePathModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid() ||
        !report_ ||
        !groupIndex_.has_value() ||
        *groupIndex_ >= report_->duplicateGroups.size()) {
        return 0;
    }
    return static_cast<int>(
        report_->duplicateGroups[*groupIndex_].filePaths.size());
}

int DuplicatePathModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : 1;
}

QVariant DuplicatePathModel::data(
    const QModelIndex& index,
    int role) const {
    if (!index.isValid() ||
        index.column() != 0 ||
        index.row() < 0 ||
        index.row() >= rowCount()) {
        return {};
    }
    const QString path = pathAt(index.row());
    if (role == Qt::DisplayRole ||
        role == Qt::ToolTipRole ||
        role == FullPathRole) {
        return path;
    }
    return {};
}

QVariant DuplicatePathModel::headerData(
    int section,
    Qt::Orientation orientation,
    int role) const {
    if (orientation == Qt::Horizontal &&
        section == 0 &&
        role == Qt::DisplayRole) {
        return QStringLiteral("完整路径");
    }
    return QAbstractTableModel::headerData(section, orientation, role);
}

void DuplicatePathModel::setReport(SharedScanReport report) {
    beginResetModel();
    report_ = std::move(report);
    groupIndex_.reset();
    endResetModel();
}

void DuplicatePathModel::setGroupIndex(
    std::optional<std::size_t> groupIndex) {
    beginResetModel();
    groupIndex_ = groupIndex;
    endResetModel();
}

QString DuplicatePathModel::pathAt(int row) const {
    if (!report_ ||
        !groupIndex_.has_value() ||
        *groupIndex_ >= report_->duplicateGroups.size() ||
        row < 0 ||
        static_cast<std::size_t>(row) >=
            report_->duplicateGroups[*groupIndex_].filePaths.size()) {
        return {};
    }
    return QString::fromStdWString(
        report_->duplicateGroups[*groupIndex_]
            .filePaths[static_cast<std::size_t>(row)]
            .wstring());
}

}
