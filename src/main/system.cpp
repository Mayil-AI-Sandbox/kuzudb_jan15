#include "src/main/include/system.h"

using namespace graphflow::planner;

namespace graphflow {
namespace main {

System::System(const SystemConfig& config, const string path) {
    if (path.empty()) {
        throw invalid_argument("Given path is empty.");
    }
    this->config = config;
    initialize(path);
}

void System::restart(const SystemConfig& config) {
    auto path = graph->getPath();
    this->config = config;
    initialize(path);
}

void System::initialize(string path) {
    graph.reset(new Graph(path, config.bufferPoolSize));
    transactionManager.reset(new TransactionManager());
    processor.reset(new QueryProcessor(config.numProcessorThreads));
}

vector<unique_ptr<LogicalPlan>> System::enumerateLogicalPlans(string query) {
    if (!isInitialized()) {
        throw invalid_argument("System is not initialized");
    }
    return QueryPlanner::enumeratePlans(query, graph->getCatalog());
}

unique_ptr<QueryResult> System::execute(unique_ptr<LogicalPlan> plan, uint32_t numThreads) {
    if (!isInitialized()) {
        throw invalid_argument("System is not initialized");
    }
    auto result = processor->execute(move(plan), numThreads, *graph);
    return result;
}

unique_ptr<nlohmann::json> System::debugInfo() {
    auto json = make_unique<nlohmann::json>();
    (*json)["config"] = *config.debugInfo();
    if (nullptr != graph.get()) {
        (*json)["graph"] = *graph->debugInfo();
    } else {
        (*json)["graph"] = "";
    }
    return json;
}

} // namespace main
} // namespace graphflow