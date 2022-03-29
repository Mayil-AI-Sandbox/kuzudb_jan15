#include "include/database.h"

using namespace std;

namespace graphflow {
namespace main {

Database::Database(const DatabaseConfig& databaseConfig, const SystemConfig& systemConfig)
    : databaseConfig{databaseConfig}, systemConfig{systemConfig} {
    bufferManager = make_unique<BufferManager>(
        systemConfig.defaultPageBufferPoolSize, systemConfig.largePageBufferPoolSize);
    memoryManager = make_unique<MemoryManager>(bufferManager.get());
    queryProcessor = make_unique<QueryProcessor>(systemConfig.maxNumThreads);
    graph = make_unique<Graph>(
        databaseConfig.databasePath, *bufferManager, databaseConfig.inMemoryMode);
}

void Database::resizeBufferManager(uint64_t newSize) {
    // Currently we are resizing both buffer pools to the same size. If needed we can
    // extend this functionality.
    systemConfig.defaultPageBufferPoolSize = newSize;
    systemConfig.largePageBufferPoolSize = newSize;
    bufferManager->resize(newSize, newSize);
}

} // namespace main
} // namespace graphflow
