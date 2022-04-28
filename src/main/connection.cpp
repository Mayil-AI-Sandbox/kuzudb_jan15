#include "include/connection.h"

#include "include/database.h"
#include "include/plan_printer.h"

#include "src/binder/include/query_binder.h"
#include "src/parser/include/parser.h"
#include "src/planner/include/planner.h"
#include "src/processor/include/physical_plan/mapper/plan_mapper.h"

using namespace std;
using namespace graphflow::parser;
using namespace graphflow::binder;
using namespace graphflow::planner;
using namespace graphflow::processor;

namespace graphflow {
namespace main {

Connection::Connection(Database* database) {
    assert(database != nullptr);
    this->database = database;
    clientContext = make_unique<ClientContext>();
}

unique_ptr<QueryResult> Connection::query(const string& query) {
    unique_ptr<PreparedStatement> preparedStatement;
    preparedStatement = prepare(query);
    if (preparedStatement->success) {
        return execute(preparedStatement.get());
    }
    auto queryResult = make_unique<QueryResult>();
    queryResult->success = false;
    queryResult->errMsg = preparedStatement->errMsg;
    return queryResult;
}

std::unique_ptr<PreparedStatement> Connection::prepare(const std::string& query) {
    auto preparedStatement = make_unique<PreparedStatement>();
    if (query.empty()) {
        throw Exception("Input query is empty.");
    }
    auto querySummary = preparedStatement->querySummary.get();
    auto compilingTimer = TimeMetric(true /* enable */);
    compilingTimer.start();
    unique_ptr<ExecutionContext> executionContext;
    unique_ptr<PhysicalPlan> physicalPlan;
    try {
        // parsing
        auto parsedQuery = Parser::parseQuery(query);
        querySummary->isExplain = parsedQuery->isEnableExplain();
        querySummary->isProfile = parsedQuery->isEnableProfile();
        // binding
        auto binder = QueryBinder(*database->catalog);
        auto boundQuery = binder.bind(*parsedQuery);
        preparedStatement->parameterMap = binder.getParameterMap();
        // planning
        auto logicalPlan = Planner::getBestPlan(*database->catalog, *boundQuery);
        preparedStatement->createResultHeader(logicalPlan->getExpressionsToCollectDataTypes());
        // mapping
        auto mapper = PlanMapper(*database->catalog, *database->storageManager);
        executionContext = make_unique<ExecutionContext>(*preparedStatement->profiler,
            database->memoryManager.get(), database->bufferManager.get());
        physicalPlan = mapper.mapLogicalPlanToPhysical(move(logicalPlan), *executionContext);
        preparedStatement->physicalIDToLogicalOperatorMap = mapper.physicalIDToLogicalOperatorMap;
    } catch (Exception& exception) {
        preparedStatement->success = false;
        preparedStatement->errMsg = exception.what();
    }
    compilingTimer.stop();
    querySummary->compilingTime = compilingTimer.getElapsedTimeMS();
    preparedStatement->executionContext = move(executionContext);
    preparedStatement->physicalPlan = move(physicalPlan);
    return preparedStatement;
}

vector<unique_ptr<planner::LogicalPlan>> Connection::enumeratePlans(const string& query) {
    auto parsedQuery = Parser::parseQuery(query);
    auto boundQuery = QueryBinder(*database->catalog).bind(*parsedQuery);
    return Planner::getAllPlans(*database->catalog, *boundQuery);
}

unique_ptr<QueryResult> Connection::executePlan(unique_ptr<LogicalPlan> logicalPlan) {
    auto profiler = make_unique<Profiler>();
    profiler->resetMetrics();
    profiler->enabled = false;
    auto header = make_unique<QueryResultHeader>(logicalPlan->getExpressionsToCollectDataTypes());
    auto mapper = PlanMapper(*database->catalog, *database->storageManager);
    auto executionContext = make_unique<ExecutionContext>(
        *profiler, database->memoryManager.get(), database->bufferManager.get());
    auto physicalPlan = mapper.mapLogicalPlanToPhysical(move(logicalPlan), *executionContext);
    auto table = database->queryProcessor->execute(
        physicalPlan.get(), clientContext->numThreadsForExecution);
    auto queryResult = make_unique<QueryResult>();
    queryResult->setResultHeaderAndTable(move(header), move(table));
    queryResult->querySummary =
        make_unique<QuerySummary>(); // TODO(Xiyang): remove this line after fixing iterator.
    return queryResult;
}

std::unique_ptr<QueryResult> Connection::executeWithParams(
    PreparedStatement* preparedStatement, unordered_map<string, shared_ptr<Literal>>& inputParams) {
    auto queryResult = make_unique<QueryResult>();
    if (!preparedStatement->isSuccess()) {
        queryResult->success = false;
        queryResult->errMsg = preparedStatement->getErrorMessage();
        return queryResult;
    }
    try {
        bindParameters(preparedStatement, inputParams);
    } catch (Exception& exception) {
        queryResult->success = false;
        queryResult->errMsg = exception.what();
    }
    if (queryResult->success) {
        return execute(preparedStatement);
    }
    return queryResult;
}

void Connection::bindParameters(
    PreparedStatement* preparedStatement, unordered_map<string, shared_ptr<Literal>>& inputParams) {
    auto& parameterMap = preparedStatement->parameterMap;
    for (auto& [name, literal] : inputParams) {
        if (!parameterMap.contains(name)) {
            throw Exception("Parameter " + name + " not found.");
        }
        auto expectParam = parameterMap.at(name);
        if (expectParam->dataType.typeID != literal->dataType.typeID) {
            throw Exception("Parameter " + name + " has data type " +
                            Types::dataTypeToString(literal->dataType) + " but expect " +
                            Types::dataTypeToString(expectParam->dataType) + ".");
        }
        parameterMap.at(name)->bind(*literal);
    }
}

std::unique_ptr<QueryResult> Connection::execute(PreparedStatement* preparedStatement) {
    auto lck = acquireLock();
    auto queryResult = make_unique<QueryResult>();
    auto querySummary = preparedStatement->querySummary.get();
    // executing if not EXPLAIN
    if (!querySummary->isExplain) {
        preparedStatement->profiler->enabled = querySummary->isProfile;
        auto executingTimer = TimeMetric(true /* enable */);
        executingTimer.start();
        shared_ptr<FactorizedTable> table;
        try {
            table = database->queryProcessor->execute(
                preparedStatement->physicalPlan.get(), clientContext->numThreadsForExecution);
        } catch (Exception& exception) {
            queryResult->success = false;
            queryResult->errMsg = exception.what();
        }
        executingTimer.stop();
        querySummary->executionTime = executingTimer.getElapsedTimeMS();
        if (queryResult->success) {
            queryResult->setResultHeaderAndTable(
                move(preparedStatement->resultHeader), move(table));
            preparedStatement->createPlanPrinter();
            queryResult->querySummary = move(preparedStatement->querySummary);
        }
        return queryResult;
    }
    // create printable plan and return if EXPLAIN
    preparedStatement->createPlanPrinter();
    queryResult->querySummary = move(preparedStatement->querySummary);
    return queryResult;
}

} // namespace main
} // namespace graphflow