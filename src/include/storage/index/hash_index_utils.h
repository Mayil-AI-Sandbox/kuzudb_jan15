#pragma once

#include <functional>

#include "function/hash/hash_functions.h"
#include "storage/storage_structure/disk_overflow_file.h"
#include "storage/storage_structure/in_mem_file.h"

namespace kuzu {
namespace storage {

using insert_function_t =
    std::function<void(const uint8_t*, common::offset_t, uint8_t*, DiskOverflowFile*)>;
using hash_function_t = std::function<common::hash_t(const uint8_t*)>;
using equals_function_t = std::function<bool(
    transaction::TransactionType trxType, const uint8_t*, const uint8_t*, DiskOverflowFile*)>;

// NOLINTBEGIN(cert-err58-cpp): This is the best way to get the datatype size because it avoids
// refactoring.
static const uint32_t NUM_BYTES_FOR_INT64_KEY =
    storage::StorageUtils::getDataTypeSize(common::LogicalType{common::LogicalTypeID::INT64});
static const uint32_t NUM_BYTES_FOR_STRING_KEY =
    storage::StorageUtils::getDataTypeSize(common::LogicalType{common::LogicalTypeID::STRING});
// NOLINTEND(cert-err58-cpp)

using in_mem_insert_function_t =
    std::function<void(const uint8_t*, common::offset_t, uint8_t*, InMemFile*)>;
using in_mem_equals_function_t =
    std::function<bool(const uint8_t*, const uint8_t*, const InMemFile*)>;

const uint64_t NUM_HASH_INDEXES_LOG2 = 8;
const uint64_t NUM_HASH_INDEXES = 1 << NUM_HASH_INDEXES_LOG2;

inline uint64_t getHashIndexPosition(int64_t key) {
    common::hash_t hash;
    function::Hash::operation(key, hash);
    return (hash >> (64 - NUM_HASH_INDEXES_LOG2)) & (NUM_HASH_INDEXES - 1);
}
inline uint64_t getHashIndexPosition(const char* key) {
    auto view = std::string_view(key);
    return (std::hash<std::string_view>()(view) >> (64 - NUM_HASH_INDEXES_LOG2)) &
           (NUM_HASH_INDEXES - 1);
}

class InMemHashIndexUtils {
public:
    static in_mem_equals_function_t initializeEqualsFunc(common::LogicalTypeID dataTypeID);
    static in_mem_insert_function_t initializeInsertFunc(common::LogicalTypeID dataTypeID);

private:
    // InsertFunc
    inline static void insertFuncForInt64(const uint8_t* key, common::offset_t offset,
        uint8_t* entry, InMemFile* /*inMemOverflowFile*/ = nullptr) {
        memcpy(entry, key, NUM_BYTES_FOR_INT64_KEY);
        memcpy(entry + NUM_BYTES_FOR_INT64_KEY, &offset, sizeof(common::offset_t));
    }
    inline static void insertFuncForString(
        const uint8_t* key, common::offset_t offset, uint8_t* entry, InMemFile* inMemOverflowFile) {
        auto kuString = inMemOverflowFile->appendString(reinterpret_cast<const char*>(key));
        memcpy(entry, &kuString, NUM_BYTES_FOR_STRING_KEY);
        memcpy(entry + NUM_BYTES_FOR_STRING_KEY, &offset, sizeof(common::offset_t));
    }
    inline static bool equalsFuncForInt64(const uint8_t* keyToLookup, const uint8_t* keyInEntry,
        const InMemFile* /*inMemOverflowFile*/ = nullptr) {
        return memcmp(keyToLookup, keyInEntry, sizeof(int64_t)) == 0;
    }
    static bool equalsFuncForString(
        const uint8_t* keyToLookup, const uint8_t* keyInEntry, const InMemFile* inMemOverflowFile);
};

class HashIndexUtils {

public:
    // InsertFunc
    inline static void insertFuncForInt64(const uint8_t* key, common::offset_t offset,
        uint8_t* entry, DiskOverflowFile* /*overflowFile*/ = nullptr) {
        memcpy(entry, key, NUM_BYTES_FOR_INT64_KEY);
        memcpy(entry + NUM_BYTES_FOR_INT64_KEY, &offset, sizeof(common::offset_t));
    }
    inline static void insertFuncForString(const uint8_t* key, common::offset_t offset,
        uint8_t* entry, DiskOverflowFile* overflowFile) {
        auto kuString = overflowFile->writeString((const char*)key);
        memcpy(entry, &kuString, NUM_BYTES_FOR_STRING_KEY);
        memcpy(entry + NUM_BYTES_FOR_STRING_KEY, &offset, sizeof(common::offset_t));
    }
    static insert_function_t initializeInsertFunc(common::LogicalTypeID dataTypeID);

    // HashFunc
    inline static common::hash_t hashFuncForInt64(const uint8_t* key) {
        common::hash_t hash;
        function::Hash::operation(*(int64_t*)key, hash);
        return hash;
    }
    inline static common::hash_t hashFuncForString(const uint8_t* key) {
        common::hash_t hash;
        function::Hash::operation(std::string((char*)key), hash);
        return hash;
    }
    static hash_function_t initializeHashFunc(common::LogicalTypeID dataTypeID);

    // EqualsFunc
    static bool isStringPrefixAndLenEquals(
        const uint8_t* keyToLookup, const common::ku_string_t* keyInEntry);
    inline static bool equalsFuncForInt64(transaction::TransactionType /*trxType*/,
        const uint8_t* keyToLookup, const uint8_t* keyInEntry,
        DiskOverflowFile* /*diskOverflowFile*/) {
        return *(int64_t*)keyToLookup == *(int64_t*)keyInEntry;
    }
    static bool equalsFuncForString(transaction::TransactionType trxType,
        const uint8_t* keyToLookup, const uint8_t* keyInEntry, DiskOverflowFile* diskOverflowFile);
    static equals_function_t initializeEqualsFunc(common::LogicalTypeID dataTypeID);
};
} // namespace storage
} // namespace kuzu
