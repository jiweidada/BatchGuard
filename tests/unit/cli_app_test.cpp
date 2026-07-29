#include "cli_app.h"
#include "console_encoding.h"

#include "test_support/temporary_directory.h"

#include <gtest/gtest.h>

#include <Windows.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace batchguard::cli {
namespace {

TEST(CliAppTest, MissingArgumentReturnsOne) {
    std::wostringstream output;
    std::wostringstream error;

    const int exitCode = runCli({}, output, error);

    EXPECT_EQ(exitCode, 1);
    EXPECT_TRUE(output.str().empty());
    EXPECT_NE(error.str().find(L"缺少目录参数"), std::wstring::npos);
}

TEST(CliAppTest, HelpReturnsZero) {
    std::wostringstream output;
    std::wostringstream error;

    const int exitCode = runCli({L"--help"}, output, error);

    EXPECT_EQ(exitCode, 0);
    EXPECT_NE(output.str().find(L"BatchGuard <目录路径>"), std::wstring::npos);
    EXPECT_NE(output.str().find(L"阶段 4 递归发现普通文件"), std::wstring::npos);
    EXPECT_TRUE(error.str().empty());
}

TEST(CliAppTest, VersionReturnsZero) {
    std::wostringstream output;
    std::wostringstream error;

    const int exitCode = runCli({L"--version"}, output, error);

    EXPECT_EQ(exitCode, 0);
    EXPECT_EQ(output.str().find(L"BatchGuard "), 0U);
    EXPECT_TRUE(error.str().empty());
}

TEST(CliAppTest, ConsoleEncodingPreservesUnicodeText) {
    const std::string encodedText = toUtf8(L"\u4e2d\u6587 \u76ee\u5f55");

    EXPECT_EQ(encodedText, "\xE4\xB8\xAD\xE6\x96\x87 \xE7\x9B\xAE\xE5\xBD\x95");
}

TEST(CliAppTest, ExistingDirectoryReturnsZero) {
    const test_support::TemporaryDirectory temporaryDirectory;
    std::wostringstream output;
    std::wostringstream error;

    const int exitCode =
        runCli({temporaryDirectory.path().wstring()}, output, error);

    EXPECT_EQ(exitCode, 0);
    EXPECT_NE(output.str().find(L"目录验证通过"), std::wstring::npos);
    EXPECT_NE(output.str().find(L"发现普通文件：0"), std::wstring::npos);
    EXPECT_NE(output.str().find(L"读取文件大小：0"), std::wstring::npos);
    EXPECT_NE(output.str().find(L"计算内容指纹：0"), std::wstring::npos);
    EXPECT_TRUE(error.str().empty());
}

TEST(CliAppTest, ExistingDirectoryReportsDiscoveredFileCount) {
    const test_support::TemporaryDirectory temporaryDirectory;
    const std::filesystem::path nestedDirectory = temporaryDirectory.path() / "nested";
    ASSERT_TRUE(std::filesystem::create_directory(nestedDirectory));
    std::ofstream firstFile{temporaryDirectory.path() / "first.txt"};
    firstFile << "first";
    firstFile.close();
    std::ofstream secondFile{nestedDirectory / "second"};
    secondFile << "second";
    secondFile.close();
    std::wostringstream output;
    std::wostringstream error;

    const int exitCode =
        runCli({temporaryDirectory.path().wstring()}, output, error);

    EXPECT_EQ(exitCode, 0);
    EXPECT_NE(output.str().find(L"发现普通文件：2"), std::wstring::npos);
    EXPECT_NE(output.str().find(L"发现失败：0"), std::wstring::npos);
    EXPECT_NE(output.str().find(L"读取文件大小：2"), std::wstring::npos);
    EXPECT_NE(output.str().find(L"计算内容指纹：0"), std::wstring::npos);
    EXPECT_TRUE(error.str().empty());
}

TEST(CliAppTest, SameSizeFilesReportCalculatedFingerprintCount) {
    const test_support::TemporaryDirectory temporaryDirectory;
    std::ofstream firstFile{temporaryDirectory.path() / "first.bin"};
    firstFile << "first";
    firstFile.close();
    std::ofstream secondFile{temporaryDirectory.path() / "second.bin"};
    secondFile << "other";
    secondFile.close();
    std::wostringstream output;
    std::wostringstream error;

    const int exitCode =
        runCli({temporaryDirectory.path().wstring()}, output, error);

    EXPECT_EQ(exitCode, 0);
    EXPECT_NE(output.str().find(L"读取文件大小：2"), std::wstring::npos);
    EXPECT_NE(output.str().find(L"计算内容指纹：2"), std::wstring::npos);
    EXPECT_NE(output.str().find(L"内容处理失败：0"), std::wstring::npos);
    EXPECT_TRUE(error.str().empty());
}

TEST(CliAppTest, HashingFailureReturnsTwoAndOtherFileContinues) {
    const test_support::TemporaryDirectory temporaryDirectory;
    const std::filesystem::path lockedFile = temporaryDirectory.path() / "locked.bin";
    const std::filesystem::path readableFile = temporaryDirectory.path() / "readable.bin";
    std::ofstream lockedOutput{lockedFile};
    lockedOutput << "locked";
    lockedOutput.close();
    std::ofstream readableOutput{readableFile};
    readableOutput << "public";
    readableOutput.close();
    const HANDLE lockedHandle = CreateFileW(
        lockedFile.c_str(),
        GENERIC_READ,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    ASSERT_NE(lockedHandle, INVALID_HANDLE_VALUE);
    std::wostringstream output;
    std::wostringstream error;

    const int exitCode =
        runCli({temporaryDirectory.path().wstring()}, output, error);
    CloseHandle(lockedHandle);

    EXPECT_EQ(exitCode, 2);
    EXPECT_NE(output.str().find(L"计算内容指纹：1"), std::wstring::npos);
    EXPECT_NE(output.str().find(L"内容处理失败：1"), std::wstring::npos);
    EXPECT_TRUE(error.str().empty());
}

TEST(CliAppTest, DirectoryWithUnicodeAndSpacesReturnsZero) {
    const test_support::TemporaryDirectory temporaryDirectory;

    // 使用与 Windows 程序入口接收到的参数相同的 UTF-16 路径表示。
    const std::filesystem::path unicodePath =
        temporaryDirectory.path() / L"\u4e2d\u6587 \u6d4b\u8bd5\u76ee\u5f55";
    ASSERT_TRUE(std::filesystem::create_directory(unicodePath));
    std::wostringstream output;
    std::wostringstream error;

    const int exitCode = runCli({unicodePath.wstring()}, output, error);

    EXPECT_EQ(exitCode, 0);
    EXPECT_NE(output.str().find(unicodePath.wstring()), std::wstring::npos);
    EXPECT_TRUE(error.str().empty());
}

TEST(CliAppTest, MissingDirectoryReturnsOne) {
    const test_support::TemporaryDirectory temporaryDirectory;
    const std::filesystem::path missingPath = temporaryDirectory.path() / "missing";
    std::wostringstream output;
    std::wostringstream error;

    const int exitCode = runCli({missingPath.wstring()}, output, error);

    EXPECT_EQ(exitCode, 1);
    EXPECT_TRUE(output.str().empty());
    EXPECT_NE(error.str().find(L"目录不存在"), std::wstring::npos);
}

TEST(CliAppTest, RegularFileReturnsOne) {
    const test_support::TemporaryDirectory temporaryDirectory;
    const std::filesystem::path filePath = temporaryDirectory.path() / "file.txt";
    std::ofstream file{filePath};
    file << "BatchGuard";
    file.close();
    ASSERT_TRUE(std::filesystem::is_regular_file(filePath));
    std::wostringstream output;
    std::wostringstream error;

    const int exitCode = runCli({filePath.wstring()}, output, error);

    EXPECT_EQ(exitCode, 1);
    EXPECT_TRUE(output.str().empty());
    EXPECT_NE(error.str().find(L"不是目录"), std::wstring::npos);
}

TEST(CliAppTest, UnsupportedOptionReturnsOne) {
    std::wostringstream output;
    std::wostringstream error;

    const int exitCode = runCli({L"--unknown"}, output, error);

    EXPECT_EQ(exitCode, 1);
    EXPECT_TRUE(output.str().empty());
    EXPECT_NE(error.str().find(L"不支持的选项"), std::wstring::npos);
}

TEST(CliAppTest, MultipleArgumentsReturnOne) {
    std::wostringstream output;
    std::wostringstream error;

    const int exitCode = runCli({L"first", L"second"}, output, error);

    EXPECT_EQ(exitCode, 1);
    EXPECT_TRUE(output.str().empty());
    EXPECT_NE(error.str().find(L"只允许提供一个目录路径"), std::wstring::npos);
}

}
}
