#pragma once

#include "scan_execution.h"

#include <QAbstractTableModel>

namespace batchguard::gui {

// 以阶段、完整路径和系统原因三列展示单文件失败。
class FailureModel final : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit FailureModel(QObject* parent = nullptr);

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
    [[nodiscard]] QString stageSummary() const;

private:
    SharedScanReport report_;
};

}
