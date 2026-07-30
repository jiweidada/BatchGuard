#pragma once

namespace batchguard::gui {

// 仅在 GUI 由终端启动时连接父控制台，不为资源管理器启动额外创建窗口。
[[nodiscard]] bool connectToParentTerminal() noexcept;

}
