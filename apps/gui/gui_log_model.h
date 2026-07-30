#pragma once

#include "batchguard/logging/logging.h"

#include <QAbstractListModel>

#include <cstddef>
#include <vector>

namespace batchguard::gui {

// 只读展示简化运行状态，隐藏调试细节并限制内存中的记录数量。
class GuiLogModel final : public QAbstractListModel {
    Q_OBJECT

public:
    enum DataRole {
        LogLevelRole = Qt::UserRole + 1,
        LogLayerRole,
        SummaryRole,
        FullTextRole
    };

    static constexpr std::size_t kMaximumRecordCount = 500U;

    explicit GuiLogModel(QObject* parent = nullptr);

    [[nodiscard]] int rowCount(
        const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(
        const QModelIndex& index,
        int role = Qt::DisplayRole) const override;

    void appendRecord(const LogRecord& record);
    void clear();

private:
    [[nodiscard]] static QString simplifiedText(const LogRecord& record);

    std::vector<LogRecord> records_;
};

}
