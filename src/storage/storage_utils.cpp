#include "storage/storage_utils.h"

#include <cctype>
#include <filesystem>

#include "common/null_buffer.h"
#include "common/types/list_t.h"
#include "common/types/string_t.h"
#include "common/types/types.h"
#include "main/client_context.h"
#include "main/db_config.h"
#include "main/settings.h"
#include <format>

using namespace lbug::common;

namespace lbug {
namespace storage {

static bool isPathSeparator(char ch) {
    return ch == '/' || ch == '\\';
}

static bool isWindowsDrivePath(const std::string& path) {
    if (path.size() >= 2 && std::isalpha(static_cast<unsigned char>(path[0])) && path[1] == ':') {
        return true;
    }
    return false;
}

static bool isWindowsAbsolutePath(const std::string& path) {
    if (path.size() >= 3 && isWindowsDrivePath(path) && isPathSeparator(path[2])) {
        return true;
    }
    return path.size() >= 2 && isPathSeparator(path[0]) && isPathSeparator(path[1]);
}

std::string StorageUtils::getColumnName(const std::string& propertyName, ColumnType type,
    const std::string& prefix) {
    switch (type) {
    case ColumnType::DATA: {
        return std::format("{}_data", propertyName);
    }
    case ColumnType::NULL_MASK: {
        return std::format("{}_null", propertyName);
    }
    case ColumnType::INDEX: {
        return std::format("{}_index", propertyName);
    }
    case ColumnType::OFFSET: {
        return std::format("{}_offset", propertyName);
    }
    case ColumnType::CSR_OFFSET: {
        return std::format("{}_csr_offset", prefix);
    }
    case ColumnType::CSR_LENGTH: {
        return std::format("{}_csr_length", prefix);
    }
    case ColumnType::STRUCT_CHILD: {
        return std::format("{}_{}_child", propertyName, prefix);
    }
    default: {
        if (prefix.empty()) {
            return propertyName;
        }
        return std::format("{}_{}", prefix, propertyName);
    }
    }
}

std::string StorageUtils::expandPath(const main::ClientContext* context, const std::string& path) {
    if (main::DBConfig::isDBPathInMemory(path)) {
        return path;
    }
    auto fullPath = path;
    // Handle '~' for home directory expansion
    if (path.starts_with('~')) {
        fullPath =
            context->getCurrentSetting(main::HomeDirectorySetting::name).getValue<std::string>() +
            fullPath.substr(1);
    }
    std::filesystem::path fullFsPath{fullPath};
    if (fullFsPath.is_absolute() || isWindowsAbsolutePath(fullPath) ||
        isWindowsDrivePath(fullPath)) {
        return fullFsPath.lexically_normal().string();
    }
    // Normalize the path to resolve '.' and '..'
    std::filesystem::path normalizedPath = std::filesystem::absolute(fullPath).lexically_normal();
    return normalizedPath.string();
}

uint32_t StorageUtils::getDataTypeSize(const LogicalType& type) {
    switch (type.getPhysicalType()) {
    case PhysicalTypeID::STRING:
    case PhysicalTypeID::JSON: {
        return sizeof(string_t);
    }
    case PhysicalTypeID::ARRAY:
    case PhysicalTypeID::LIST: {
        return sizeof(list_t);
    }
    case PhysicalTypeID::STRUCT: {
        uint32_t size = 0;
        const auto fieldsTypes = StructType::getFieldTypes(type);
        for (const auto& fieldType : fieldsTypes) {
            size += getDataTypeSize(*fieldType);
        }
        size += NullBuffer::getNumBytesForNullValues(fieldsTypes.size());
        return size;
    }
    default: {
        return PhysicalTypeUtils::getFixedTypeSize(type.getPhysicalType());
    }
    }
}

} // namespace storage
} // namespace lbug
