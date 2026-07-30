#pragma once

#include "scan_execution.h"

#include <QAbstractTableModel>

#include <cstddef>
#include <optional>

namespace batchguard::gui {

// 展示当前重复组的全部完整路径，不复制核心路径集合。
class DuplicatePathModel final : public QAbstractTableModel {
    Q_OBJECT

public:
    enum DataRole {
        FullPathRole = Qt::UserRole + 1
    };

    explicit DuplicatePathModel(QObject* parent = nullptr);

    [[nodiscard]] int rowCount(
        const QModelIndex& parent = {}) const override;
    [[nodiscard]] int columnCount(
        const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(
        const QModelIndex& index,
        int role = Qt::DisplayRole) const override;
    [[nodiscard]] QVariant headerData(
        int section,
        Qt::Orientation orientation,
        int role = Qt::DisplayRole) const override;

    void setReport(SharedScanReport report);
    void setGroupIndex(std::optional<std::size_t> groupIndex);
    [[nodiscard]] QString pathAt(int row) const;

private:
    SharedScanReport report_;
    std::optional<std::size_t> groupIndex_;
};

}
