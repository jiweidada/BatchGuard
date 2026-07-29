#include "batchguard/core/input_validator.h"

namespace batchguard {

InputValidationResult validateInputDirectory(const std::filesystem::path& directoryPath) {
    if (directoryPath.empty()) {
        return {InputValidationError::EmptyPath, {}};
    }

    // 单独检查路径是否存在，使调用方得到稳定的“不存在”结果，
    // 避免暴露不同平台的状态查询差异。
    std::error_code errorCode;
    const bool doesExist = std::filesystem::exists(directoryPath, errorCode);
    if (errorCode) {
        return {InputValidationError::CannotAccess, errorCode};
    }
    if (!doesExist) {
        return {InputValidationError::DoesNotExist, {}};
    }

    const bool isDirectory = std::filesystem::is_directory(directoryPath, errorCode);
    if (errorCode) {
        return {InputValidationError::CannotAccess, errorCode};
    }
    if (!isDirectory) {
        return {InputValidationError::NotDirectory, {}};
    }

    return {};
}

}
