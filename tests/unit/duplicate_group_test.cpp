#include "batchguard/core/duplicate_group.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace batchguard {
namespace {

FileRecord makeRecord(
    const std::filesystem::path& path,
    std::uintmax_t fileSize,
    std::optional<std::string> sha256) {
    return {path, fileSize, std::move(sha256)};
}

TEST(DuplicateGroupTest, GroupsMatchingSizeAndHash) {
    const std::vector<FileRecord> records{
        makeRecord("first.txt", 4U, "same"),
        makeRecord("second.txt", 4U, "same")};

    const std::vector<DuplicateGroup> groups = findDuplicateGroups(records);

    ASSERT_EQ(groups.size(), 1U);
    EXPECT_EQ(groups.front().fileSize, 4U);
    EXPECT_EQ(groups.front().sha256, "same");
    EXPECT_EQ(
        groups.front().filePaths,
        (std::vector<std::filesystem::path>{"first.txt", "second.txt"}));
}

TEST(DuplicateGroupTest, DoesNotGroupSameSizeWithDifferentHashes) {
    const std::vector<FileRecord> records{
        makeRecord("first.txt", 4U, "first"),
        makeRecord("second.txt", 4U, "second")};

    const std::vector<DuplicateGroup> groups = findDuplicateGroups(records);

    EXPECT_TRUE(groups.empty());
}

TEST(DuplicateGroupTest, DoesNotGroupSameHashWithDifferentSizes) {
    const std::vector<FileRecord> records{
        makeRecord("first.txt", 4U, "same"),
        makeRecord("second.txt", 5U, "same")};

    const std::vector<DuplicateGroup> groups = findDuplicateGroups(records);

    EXPECT_TRUE(groups.empty());
}

TEST(DuplicateGroupTest, IgnoresRecordsWithoutFingerprint) {
    const std::vector<FileRecord> records{
        makeRecord("first.txt", 4U, std::nullopt),
        makeRecord("second.txt", 4U, std::nullopt)};

    const std::vector<DuplicateGroup> groups = findDuplicateGroups(records);

    EXPECT_TRUE(groups.empty());
}

TEST(DuplicateGroupTest, KeepsAllMembersWithMatchingContentKey) {
    const std::vector<FileRecord> records{
        makeRecord("third.txt", 4U, "same"),
        makeRecord("first.txt", 4U, "same"),
        makeRecord("second.txt", 4U, "same")};

    const std::vector<DuplicateGroup> groups = findDuplicateGroups(records);

    ASSERT_EQ(groups.size(), 1U);
    EXPECT_EQ(
        groups.front().filePaths,
        (std::vector<std::filesystem::path>{
            "first.txt",
            "second.txt",
            "third.txt"}));
}

TEST(DuplicateGroupTest, SortsGroupsBySizeHashAndFirstPath) {
    const std::vector<FileRecord> records{
        makeRecord("z.txt", 9U, "bb"),
        makeRecord("y.txt", 9U, "bb"),
        makeRecord("d.txt", 2U, "cc"),
        makeRecord("c.txt", 2U, "cc"),
        makeRecord("b.txt", 2U, "aa"),
        makeRecord("a.txt", 2U, "aa")};

    const std::vector<DuplicateGroup> groups = findDuplicateGroups(records);

    ASSERT_EQ(groups.size(), 3U);
    EXPECT_EQ(groups[0].fileSize, 2U);
    EXPECT_EQ(groups[0].sha256, "aa");
    EXPECT_EQ(groups[0].filePaths.front(), std::filesystem::path{"a.txt"});
    EXPECT_EQ(groups[1].fileSize, 2U);
    EXPECT_EQ(groups[1].sha256, "cc");
    EXPECT_EQ(groups[1].filePaths.front(), std::filesystem::path{"c.txt"});
    EXPECT_EQ(groups[2].fileSize, 9U);
    EXPECT_EQ(groups[2].sha256, "bb");
    EXPECT_EQ(groups[2].filePaths.front(), std::filesystem::path{"y.txt"});
}

}
}
