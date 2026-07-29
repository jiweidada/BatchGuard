#pragma once

#include <iosfwd>
#include <string>
#include <string_view>

namespace batchguard::cli {

// 配置当前 Windows 控制台，使其将程序输出解释为 UTF-8。
void configureConsoleEncoding();

// 将 Windows CLI 边界传入的 UTF-16 文本转换为 UTF-8 字节。
[[nodiscard]] std::string toUtf8(std::wstring_view text);

// 将 `text` 以 UTF-8 写入流，不依赖进程范围的 C++ 区域设置。
void writeUtf8(std::ostream& stream, std::wstring_view text);

}
