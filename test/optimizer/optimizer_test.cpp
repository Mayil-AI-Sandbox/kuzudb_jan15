#include "graph_test/graph_test.h"

namespace kuzu {
namespace testing {

class OptimizerTest : public DBTest {
public:
    std::string getInputDir() override {
        return TestHelper::appendKuzuRootPath("dataset/tinysnb/");
    }

    std::shared_ptr<planner::LogicalOperator> getRoot(const std::string& query) {
        return TestHelper::getLogicalPlan(query, *conn)->getLastOperator();
    }
};

TEST_F(OptimizerTest, FilterPushDownTest) {
    auto op = getRoot("MATCH (a:person) WHERE a.ID < 0 AND a.fName='Alice' RETURN a.gender;");
    ASSERT_EQ(op->getOperatorType(), planner::LogicalOperatorType::PROJECTION);
    op = op->getChild(0);
    ASSERT_EQ(op->getOperatorType(), planner::LogicalOperatorType::SCAN_NODE_PROPERTY);
    op = op->getChild(0);
    ASSERT_EQ(op->getOperatorType(), planner::LogicalOperatorType::FILTER);
    op = op->getChild(0);
    ASSERT_EQ(op->getOperatorType(), planner::LogicalOperatorType::SCAN_NODE_PROPERTY);
    op = op->getChild(0);
    ASSERT_EQ(op->getOperatorType(), planner::LogicalOperatorType::FILTER);
    op = op->getChild(0);
    ASSERT_EQ(op->getOperatorType(), planner::LogicalOperatorType::SCAN_NODE_PROPERTY);
}

TEST_F(OptimizerTest, IndexScanTest) {
    auto op = getRoot("MATCH (a:person) WHERE a.ID = 0 AND a.fName='Alice' RETURN a.gender;");
    ASSERT_EQ(op->getOperatorType(), planner::LogicalOperatorType::PROJECTION);
    op = op->getChild(0);
    ASSERT_EQ(op->getOperatorType(), planner::LogicalOperatorType::SCAN_NODE_PROPERTY);
    op = op->getChild(0);
    ASSERT_EQ(op->getOperatorType(), planner::LogicalOperatorType::FILTER);
    op = op->getChild(0);
    ASSERT_EQ(op->getOperatorType(), planner::LogicalOperatorType::SCAN_NODE_PROPERTY);
    op = op->getChild(0);
    ASSERT_EQ(op->getOperatorType(), planner::LogicalOperatorType::INDEX_SCAN_NODE);
}

TEST_F(OptimizerTest, RemoveUnnecessaryJoinTest) {
    auto op = getRoot("MATCH (a:person)-[e:knows]->(b:person) RETURN e.date;");
    ASSERT_EQ(op->getOperatorType(), planner::LogicalOperatorType::PROJECTION);
    op = op->getChild(0);
    ASSERT_EQ(op->getOperatorType(), planner::LogicalOperatorType::EXTEND);
    op = op->getChild(0);
    ASSERT_EQ(op->getOperatorType(), planner::LogicalOperatorType::FLATTEN);
    op = op->getChild(0);
    ASSERT_EQ(op->getOperatorType(), planner::LogicalOperatorType::SCAN_NODE);
}

TEST_F(OptimizerTest, ProjectionPushDownJoinTest) {
    auto op = getRoot(
        "MATCH (a:person)-[e:knows]->(b:person) WHERE a.age > 0 AND b.age>0 RETURN a.ID, b.ID;");
    ASSERT_EQ(op->getOperatorType(), planner::LogicalOperatorType::PROJECTION);
    op = op->getChild(0);
    ASSERT_EQ(op->getOperatorType(), planner::LogicalOperatorType::HASH_JOIN);
    op = op->getChild(1);
    ASSERT_EQ(op->getOperatorType(), planner::LogicalOperatorType::PROJECTION);
}

} // namespace testing
} // namespace kuzu