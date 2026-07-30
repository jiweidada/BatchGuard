#include "batchguard/logging/logging.h"

#include <gtest/gtest.h>

#include <chrono>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace batchguard {
namespace {

TEST(LoggingTest, FormatsStableSingleLineStructuredRecord) {
    LogRecord record;
    record.timestamp = std::chrono::system_clock::time_point{
        std::chrono::milliseconds{1'700'000'000'123LL}};
    record.level = LogLevel::Warning;
    record.layer = LogLayer::CoreHashing;
    record.summary = "读取\n失败";
    record.details = {{"reason", "io\rerror"}};

    const std::string text = formatLogRecord(record);

    EXPECT_NE(
        text.find("：WARN：CORE.Hashing：读取 失败，reason=io error"),
        std::string::npos);
    EXPECT_EQ(text.find('\n'), std::string::npos);
    EXPECT_EQ(text.find('\r'), std::string::npos);
}

TEST(LoggingTest, ProgressRecordContainsOnlyCountsAndStageData) {
    const LogRecord record = makeProgressLogRecord({
        ScanProgressStage::Hashing,
        2U,
        4U,
        1024U,
        4096U,
        false});

    EXPECT_EQ(record.level, LogLevel::Info);
    EXPECT_EQ(record.layer, LogLayer::CoreHashing);
    EXPECT_EQ(record.summary, "开始计算内容指纹");
    ASSERT_EQ(record.details.size(), std::size_t{5U});
    EXPECT_EQ(record.details[0].key, "completedItems");
    EXPECT_EQ(record.details[0].value, "2");
    EXPECT_EQ(record.details[3].value, "4096");
}

TEST(LoggingTest, TerminalSinkKeepsConcurrentRecordsOnSeparateLines) {
    std::ostringstream output;
    TerminalLogSink sink{output};
    constexpr int kThreadCount = 4;
    constexpr int kRecordsPerThread = 25;
    std::vector<std::thread> threads;
    threads.reserve(kThreadCount);
    for (int threadIndex = 0; threadIndex < kThreadCount; ++threadIndex) {
        threads.emplace_back([threadIndex, &sink]() {
            for (int recordIndex = 0;
                 recordIndex < kRecordsPerThread;
                 ++recordIndex) {
                sink.write(makeLogRecord(
                    LogLevel::Debug,
                    LogLayer::GuiExecution,
                    "并发日志",
                    {
                        {"thread", std::to_string(threadIndex)},
                        {"record", std::to_string(recordIndex)}}));
            }
        });
    }
    for (std::thread& thread : threads) {
        thread.join();
    }

    std::istringstream lines{output.str()};
    std::string line;
    int lineCount = 0;
    while (std::getline(lines, line)) {
        EXPECT_NE(
            line.find("：DEBUG：GUI.ScanExecution：并发日志"),
            std::string::npos);
        ++lineCount;
    }
    EXPECT_EQ(lineCount, kThreadCount * kRecordsPerThread);
}

}
}
