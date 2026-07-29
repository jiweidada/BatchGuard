#include "console_encoding.h"
#include "console_progress.h"

#include "batchguard/core/scan_progress.h"

#include <gtest/gtest.h>

#include <sstream>

namespace batchguard::cli {
namespace {

TEST(ConsoleProgressTest, DisabledRendererProducesNoOutput) {
    std::ostringstream output;
    ConsoleProgressRenderer renderer{output, false};

    renderer.render({
        ScanProgressStage::Discovery,
        3U,
        0U,
        0U,
        0U,
        true});
    renderer.finish();

    EXPECT_TRUE(output.str().empty());
}

TEST(ConsoleProgressTest, RendersAllStagesAndProgressBars) {
    std::ostringstream output;
    ConsoleProgressRenderer renderer{output, true};

    renderer.render({
        ScanProgressStage::Discovery,
        3U,
        0U,
        0U,
        0U,
        true});
    renderer.render({
        ScanProgressStage::Metadata,
        1U,
        2U,
        0U,
        0U,
        false});
    renderer.render({
        ScanProgressStage::Metadata,
        2U,
        2U,
        0U,
        0U,
        true});
    renderer.render({
        ScanProgressStage::Hashing,
        1U,
        2U,
        10U,
        20U,
        false});
    renderer.render({
        ScanProgressStage::Hashing,
        2U,
        2U,
        20U,
        20U,
        true});
    renderer.render({
        ScanProgressStage::Grouping,
        0U,
        1U,
        0U,
        0U,
        false});
    renderer.render({
        ScanProgressStage::Grouping,
        1U,
        1U,
        0U,
        0U,
        true});
    renderer.finish();

    const std::string rendered = output.str();
    EXPECT_NE(rendered.find(toUtf8(L"正在发现文件：3 个")), std::string::npos);
    EXPECT_NE(rendered.find(toUtf8(L"读取文件信息")), std::string::npos);
    EXPECT_NE(rendered.find(toUtf8(L"计算内容指纹")), std::string::npos);
    EXPECT_NE(rendered.find("50%"), std::string::npos);
    EXPECT_NE(rendered.find("100%"), std::string::npos);
    EXPECT_NE(rendered.find(toUtf8(L"重复文件报告已生成")), std::string::npos);
}

}
}
