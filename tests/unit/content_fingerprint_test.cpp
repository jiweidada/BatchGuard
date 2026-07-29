#include "batchguard/core/content_fingerprint.h"
#include "batchguard/core/file_hasher.h"

#include "test_support/temporary_directory.h"

#include <gtest/gtest.h>

#include <Windows.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace batchguard {
namespace {

void createBinaryFile(
    const std::filesystem::path& filePath,
    const std::string& contents) {
    std::ofstream file{filePath, std::ios::binary};
    file.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    file.close();
    ASSERT_TRUE(std::filesystem::is_regular_file(filePath));
}

const FileRecord* findRecord(
    const ContentFingerprintResult& result,
    const std::filesystem::path& filePath) {
    const auto iterator = std::find_if(
        result.fileRecords.begin(),
        result.fileRecords.end(),
        [&filePath](const FileRecord& record) {
            return record.path == filePath;
        });
    return iterator == result.fileRecords.end() ? nullptr : &*iterator;
}

TEST(FileHasherTest, ProducesKnownSha256ForText) {
    const test_support::TemporaryDirectory temporaryDirectory;
    const std::filesystem::path filePath = temporaryDirectory.path() / "abc.txt";
    createBinaryFile(filePath, "abc");

    const FileHashResult result = calculateFileSha256(filePath);

    ASSERT_TRUE(result.isSuccess());
    EXPECT_EQ(
        result.sha256,
        "ba7816bf8f01cfea414140de5dae2223"
        "b00361a396177a9cb410ff61f20015ad");
}

TEST(FileHasherTest, ProducesKnownSha256ForEmptyFile) {
    const test_support::TemporaryDirectory temporaryDirectory;
    const std::filesystem::path filePath = temporaryDirectory.path() / "empty";
    createBinaryFile(filePath, "");

    const FileHashResult result = calculateFileSha256(filePath);

    ASSERT_TRUE(result.isSuccess());
    EXPECT_EQ(
        result.sha256,
        "e3b0c44298fc1c149afbf4c8996fb924"
        "27ae41e4649b934ca495991b7852b855");
}

TEST(FileHasherTest, HashesContentAcrossMultipleReadBlocks) {
    const test_support::TemporaryDirectory temporaryDirectory;
    const std::filesystem::path filePath = temporaryDirectory.path() / "large.bin";
    createBinaryFile(filePath, std::string(1'000'000, 'a'));

    const FileHashResult result = calculateFileSha256(filePath);

    ASSERT_TRUE(result.isSuccess());
    EXPECT_EQ(
        result.sha256,
        "cdc76e5c9914fb9281a1c7e284d73e67"
        "f1809a48a497200e046d39ccc7112cd0");
}

TEST(FileHasherTest, SupportsUnicodeAndSpacesInPath) {
    const test_support::TemporaryDirectory temporaryDirectory;
    const std::filesystem::path unicodeDirectory =
        temporaryDirectory.path() / L"\u4e2d\u6587 \u76ee\u5f55";
    ASSERT_TRUE(std::filesystem::create_directory(unicodeDirectory));
    const std::filesystem::path filePath =
        unicodeDirectory / L"\u6d4b\u8bd5 \u6587\u4ef6.bin";
    createBinaryFile(filePath, "abc");

    const FileHashResult result = calculateFileSha256(filePath);

    ASSERT_TRUE(result.isSuccess());
    EXPECT_EQ(
        result.sha256,
        "ba7816bf8f01cfea414140de5dae2223"
        "b00361a396177a9cb410ff61f20015ad");
}

TEST(ContentFingerprintTest, HashesOnlyFilesSharingTheSameSize) {
    const test_support::TemporaryDirectory temporaryDirectory;
    const std::filesystem::path firstCandidate = temporaryDirectory.path() / "first.bin";
    const std::filesystem::path secondCandidate = temporaryDirectory.path() / "second.bin";
    const std::filesystem::path uniqueFile = temporaryDirectory.path() / "unique.bin";
    createBinaryFile(firstCandidate, "abcd");
    createBinaryFile(secondCandidate, "wxyz");
    createBinaryFile(uniqueFile, "unique");

    const ContentFingerprintResult result =
        fingerprintFileCandidates({uniqueFile, secondCandidate, firstCandidate});

    ASSERT_EQ(result.fileRecords.size(), 3U);
    const FileRecord* firstRecord = findRecord(result, firstCandidate);
    const FileRecord* secondRecord = findRecord(result, secondCandidate);
    const FileRecord* uniqueRecord = findRecord(result, uniqueFile);
    ASSERT_NE(firstRecord, nullptr);
    ASSERT_NE(secondRecord, nullptr);
    ASSERT_NE(uniqueRecord, nullptr);
    EXPECT_TRUE(firstRecord->sha256.has_value());
    EXPECT_TRUE(secondRecord->sha256.has_value());
    EXPECT_NE(firstRecord->sha256, secondRecord->sha256);
    EXPECT_FALSE(uniqueRecord->sha256.has_value());
    EXPECT_TRUE(result.failures.empty());
}

TEST(ContentFingerprintTest, HashesTwoEmptyCandidateFiles) {
    const test_support::TemporaryDirectory temporaryDirectory;
    const std::filesystem::path firstFile = temporaryDirectory.path() / "first.empty";
    const std::filesystem::path secondFile = temporaryDirectory.path() / "second.empty";
    createBinaryFile(firstFile, "");
    createBinaryFile(secondFile, "");

    const ContentFingerprintResult result =
        fingerprintFileCandidates({firstFile, secondFile});

    const FileRecord* firstRecord = findRecord(result, firstFile);
    const FileRecord* secondRecord = findRecord(result, secondFile);
    ASSERT_NE(firstRecord, nullptr);
    ASSERT_NE(secondRecord, nullptr);
    ASSERT_TRUE(firstRecord->sha256.has_value());
    ASSERT_TRUE(secondRecord->sha256.has_value());
    EXPECT_EQ(firstRecord->sha256, secondRecord->sha256);
    EXPECT_TRUE(result.failures.empty());
}

TEST(ContentFingerprintTest, EqualContentsProduceEqualFingerprints) {
    const test_support::TemporaryDirectory temporaryDirectory;
    const std::filesystem::path firstFile = temporaryDirectory.path() / "first.bin";
    const std::filesystem::path secondFile = temporaryDirectory.path() / "second.bin";
    createBinaryFile(firstFile, "same contents");
    createBinaryFile(secondFile, "same contents");

    const ContentFingerprintResult result =
        fingerprintFileCandidates({firstFile, secondFile});

    const FileRecord* firstRecord = findRecord(result, firstFile);
    const FileRecord* secondRecord = findRecord(result, secondFile);
    ASSERT_NE(firstRecord, nullptr);
    ASSERT_NE(secondRecord, nullptr);
    ASSERT_TRUE(firstRecord->sha256.has_value());
    ASSERT_TRUE(secondRecord->sha256.has_value());
    EXPECT_EQ(firstRecord->sha256, secondRecord->sha256);
    EXPECT_TRUE(result.failures.empty());
}

TEST(ContentFingerprintTest, RecordsMetadataFailureAndContinues) {
    const test_support::TemporaryDirectory temporaryDirectory;
    const std::filesystem::path existingFile = temporaryDirectory.path() / "existing.bin";
    const std::filesystem::path missingFile = temporaryDirectory.path() / "missing.bin";
    createBinaryFile(existingFile, "existing");

    const ContentFingerprintResult result =
        fingerprintFileCandidates({missingFile, existingFile});

    ASSERT_EQ(result.fileRecords.size(), 1U);
    EXPECT_EQ(result.fileRecords.front().path, existingFile);
    ASSERT_EQ(result.failures.size(), 1U);
    EXPECT_EQ(result.failures.front().path, missingFile);
    EXPECT_EQ(result.failures.front().stage, FailureStage::Metadata);
    EXPECT_TRUE(result.failures.front().errorCode);
}

TEST(ContentFingerprintTest, RecordsHashingFailureAndContinues) {
    const test_support::TemporaryDirectory temporaryDirectory;
    const std::filesystem::path lockedFile = temporaryDirectory.path() / "locked.bin";
    const std::filesystem::path readableFile = temporaryDirectory.path() / "readable.bin";
    createBinaryFile(lockedFile, "locked");
    createBinaryFile(readableFile, "public");

    const HANDLE lockedHandle = CreateFileW(
        lockedFile.c_str(),
        GENERIC_READ,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    ASSERT_NE(lockedHandle, INVALID_HANDLE_VALUE);

    const ContentFingerprintResult result =
        fingerprintFileCandidates({lockedFile, readableFile});
    CloseHandle(lockedHandle);

    const FileRecord* lockedRecord = findRecord(result, lockedFile);
    const FileRecord* readableRecord = findRecord(result, readableFile);
    ASSERT_NE(lockedRecord, nullptr);
    ASSERT_NE(readableRecord, nullptr);
    EXPECT_FALSE(lockedRecord->sha256.has_value());
    EXPECT_TRUE(readableRecord->sha256.has_value());
    ASSERT_EQ(result.failures.size(), 1U);
    EXPECT_EQ(result.failures.front().path, lockedFile);
    EXPECT_EQ(result.failures.front().stage, FailureStage::Hashing);
    EXPECT_TRUE(result.failures.front().errorCode);
}

TEST(ContentFingerprintTest, ReturnsRecordsInStableLexicalOrder) {
    const test_support::TemporaryDirectory temporaryDirectory;
    const std::filesystem::path lastFile = temporaryDirectory.path() / "z.bin";
    const std::filesystem::path firstFile = temporaryDirectory.path() / "a.bin";
    createBinaryFile(lastFile, "z");
    createBinaryFile(firstFile, "a");

    const ContentFingerprintResult result =
        fingerprintFileCandidates({lastFile, firstFile});

    ASSERT_EQ(result.fileRecords.size(), 2U);
    EXPECT_EQ(result.fileRecords[0].path, firstFile);
    EXPECT_EQ(result.fileRecords[1].path, lastFile);
}

}
}
