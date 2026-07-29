#include "console_encoding.h"

#include <Windows.h>

#include <limits>
#include <ostream>
#include <stdexcept>

namespace batchguard::cli {

void configureConsoleEncoding() {
    SetConsoleOutputCP(CP_UTF8);
}

std::string toUtf8(std::wstring_view text) {
    if (text.empty()) {
        return {};
    }
    if (text.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
        throw std::length_error("Console text is too long to encode.");
    }

    // Windows 转码采用两次调用：第一次取得准确字节数，第二次填充已分配的缓冲区。
    const int wideCharacterCount = static_cast<int>(text.size());
    const int byteCount = WideCharToMultiByte(
        CP_UTF8,
        0,
        text.data(),
        wideCharacterCount,
        nullptr,
        0,
        nullptr,
        nullptr);
    if (byteCount == 0) {
        throw std::runtime_error("Unable to determine UTF-8 output size.");
    }

    std::string encodedText(static_cast<std::size_t>(byteCount), '\0');
    const int convertedByteCount = WideCharToMultiByte(
        CP_UTF8,
        0,
        text.data(),
        wideCharacterCount,
        encodedText.data(),
        byteCount,
        nullptr,
        nullptr);
    if (convertedByteCount != byteCount) {
        throw std::runtime_error("Unable to encode console output as UTF-8.");
    }

    return encodedText;
}

void writeUtf8(std::ostream& stream, std::wstring_view text) {
    const std::string encodedText = toUtf8(text);
    stream.write(encodedText.data(), static_cast<std::streamsize>(encodedText.size()));
}

}
