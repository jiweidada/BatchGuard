#pragma once

#include "scan_execution.h"

#include <QAbstractTableModel>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace batchguard::gui {

// 以每个重复组一行的方式只读展示完整扫描报告。
class DuplicateGroupModel final : public QAbstractTableModel {
    Q_OBJECT

public:
    enum DataRole {
        RawValueRole = Qt::UserRole + 1,
        ReportGroupIndexRole
    };

    explicit DuplicateGroupModel(QObject* parent = nullptr);

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
    void sort(
        int column,
        Qt::SortOrder order = Qt::AscendingOrder) override;

    void setReport(SharedScanReport report);
    [[nodiscard]] std::size_t reportGroupIndexForRow(int row) const;

private:
    [[nodiscard]] std::uintmax_t numericValue(
        std::size_t reportGroupIndex,
        int column) const;
    void rebuildOrder();

    SharedScanReport report_;
    std::vector<std::size_t> orderedGroupIndices_;
    int sortColumn_{4};
    Qt::SortOrder sortOrder_{Qt::DescendingOrder};
};

}
