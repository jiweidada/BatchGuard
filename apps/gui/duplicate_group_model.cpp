#include "duplicate_group_model.h"

#include "result_summary.h"

#include <QString>

#include <algorithm>
#include <limits>
#include <numeric>
#include <utility>

namespace batchguard::gui {
namespace {

std::uintmax_t reclaimableBytes(const DuplicateGroup& group) {
    const std::size_t redundantCount =
        group.filePaths.empty() ? 0U : group.filePaths.size() - 1U;
    const std::uintmax_t maximum = (std::numeric_limits<std::uintmax_t>::max)();
    if (group.fileSize == 0U || redundantCount == 0U) {
        return 0U;
    }
    return redundantCount > maximum / group.fileSize
        ? maximum
        : group.fileSize * redundantCount;
}

}

DuplicateGroupModel::DuplicateGroupModel(QObject* parent)
    : QAbstractTableModel{parent} {
}

int DuplicateGroupModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid()
        ? 0
        : static_cast<int>(orderedGroupIndices_.size());
}

int DuplicateGroupModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : 5;
}

QVariant DuplicateGroupModel::data(
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

    const std::size_t groupIndex =
        orderedGroupIndices_[static_cast<std::size_t>(index.row())];
    const DuplicateGroup& group = report_->duplicateGroups[groupIndex];
    if (role == ReportGroupIndexRole) {
        return QVariant::fromValue<qulonglong>(groupIndex);
    }
    if (role == RawValueRole) {
        return QVariant::fromValue<qulonglong>(
            numericValue(groupIndex, index.column()));
    }
    if (role == Qt::ToolTipRole) {
        if (index.column() == 2) {
            return QStringLiteral("%1 字节").arg(group.fileSize);
        }
        if (index.column() == 4) {
            return QStringLiteral(
                "%1 字节。该估算未考虑硬链接、稀疏文件和文件系统压缩。")
                .arg(reclaimableBytes(group));
        }
        return {};
    }
    if (role != Qt::DisplayRole) {
        return {};
    }

    switch (index.column()) {
    case 0:
        return static_cast<qulonglong>(groupIndex + 1U);
    case 1:
        return static_cast<qulonglong>(group.filePaths.size());
    case 2:
        return formatByteSize(group.fileSize);
    case 3:
        return static_cast<qulonglong>(
            group.filePaths.empty() ? 0U : group.filePaths.size() - 1U);
    case 4:
        return formatByteSize(reclaimableBytes(group));
    default:
        return {};
    }
}

QVariant DuplicateGroupModel::headerData(
    int section,
    Qt::Orientation orientation,
    int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return QAbstractTableModel::headerData(section, orientation, role);
    }
    switch (section) {
    case 0:
        return QStringLiteral("重复组");
    case 1:
        return QStringLiteral("文件数量");
    case 2:
        return QStringLiteral("单文件大小");
    case 3:
        return QStringLiteral("冗余副本");
    case 4:
        return QStringLiteral("理论可节省");
    default:
        return {};
    }
}

void DuplicateGroupModel::sort(int column, Qt::SortOrder order) {
    if (column < 0 || column >= columnCount()) {
        return;
    }
    beginResetModel();
    sortColumn_ = column;
    sortOrder_ = order;
    rebuildOrder();
    endResetModel();
}

void DuplicateGroupModel::setReport(SharedScanReport report) {
    beginResetModel();
    report_ = std::move(report);
    rebuildOrder();
    endResetModel();
}

std::size_t DuplicateGroupModel::reportGroupIndexForRow(int row) const {
    if (row < 0 || row >= rowCount()) {
        return (std::numeric_limits<std::size_t>::max)();
    }
    return orderedGroupIndices_[static_cast<std::size_t>(row)];
}

std::uintmax_t DuplicateGroupModel::numericValue(
    std::size_t reportGroupIndex,
    int column) const {
    const DuplicateGroup& group =
        report_->duplicateGroups[reportGroupIndex];
    switch (column) {
    case 0:
        return reportGroupIndex + 1U;
    case 1:
        return group.filePaths.size();
    case 2:
        return group.fileSize;
    case 3:
        return group.filePaths.empty() ? 0U : group.filePaths.size() - 1U;
    case 4:
        return reclaimableBytes(group);
    default:
        return 0U;
    }
}

void DuplicateGroupModel::rebuildOrder() {
    orderedGroupIndices_.clear();
    if (!report_) {
        return;
    }
    orderedGroupIndices_.resize(report_->duplicateGroups.size());
    std::iota(
        orderedGroupIndices_.begin(),
        orderedGroupIndices_.end(),
        std::size_t{0U});
    std::stable_sort(
        orderedGroupIndices_.begin(),
        orderedGroupIndices_.end(),
        [this](std::size_t left, std::size_t right) {
            const std::uintmax_t leftValue =
                numericValue(left, sortColumn_);
            const std::uintmax_t rightValue =
                numericValue(right, sortColumn_);
            if (leftValue == rightValue) {
                return left < right;
            }
            return sortOrder_ == Qt::AscendingOrder
                ? leftValue < rightValue
                : leftValue > rightValue;
        });
}

}
