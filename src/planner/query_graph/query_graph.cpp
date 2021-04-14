#include "src/planner/include/query_graph/query_graph.h"

namespace graphflow {
namespace planner {

void SubqueryGraph::addQueryNode(uint32_t nodePos) {
    queryNodesSelector[nodePos] = true;
}

void SubqueryGraph::addQueryRel(uint32_t relPos) {
    queryRelsSelector[relPos] = true;
    queryNodesSelector[queryGraph.getQueryNodePos(queryGraph.queryRels[relPos]->getSrcNodeName())] =
        true;
    queryNodesSelector[queryGraph.getQueryNodePos(queryGraph.queryRels[relPos]->getDstNodeName())] =
        true;
}

void SubqueryGraph::addSubqueryGraph(const SubqueryGraph& other) {
    queryRelsSelector |= other.queryRelsSelector;
    queryNodesSelector |= other.queryNodesSelector;
}

bool SubqueryGraph::containAllVars(unordered_set<string>& vars) const {
    for (auto& var : vars) {
        if (queryGraph.containsQueryNode(var) &&
            !queryNodesSelector[queryGraph.getQueryNodePos(var)]) {
            return false;
        }
        if (queryGraph.containsQueryRel(var) &&
            !queryRelsSelector[queryGraph.getQueryRelPos(var)]) {
            return false;
        }
    }
    return true;
}

bool SubqueryGraph::operator==(const SubqueryGraph& other) const {
    return queryRelsSelector == other.queryRelsSelector &&
           queryNodesSelector == other.queryNodesSelector;
}

bool QueryGraph::containsQueryNode(const string& queryNodeName) const {
    return end(queryNodeNameToPosMap) != queryNodeNameToPosMap.find(queryNodeName);
}

QueryNode* QueryGraph::getQueryNode(const string& queryNodeName) const {
    return queryNodes.at(queryNodeNameToPosMap.at(queryNodeName)).get();
}

uint32_t QueryGraph::getQueryNodePos(const string& queryNodeName) const {
    return queryNodeNameToPosMap.at(queryNodeName);
}

void QueryGraph::addQueryNode(unique_ptr<QueryNode> queryNode) {
    queryNodeNameToPosMap.insert({queryNode->name, queryNodes.size()});
    queryNodes.push_back(move(queryNode));
}

bool QueryGraph::containsQueryRel(const string& queryRelName) const {
    return end(queryRelNameToPosMap) != queryRelNameToPosMap.find(queryRelName);
}

QueryRel* QueryGraph::getQueryRel(const string& queryRelName) const {
    return queryRels.at(queryRelNameToPosMap.at(queryRelName)).get();
}

uint32_t QueryGraph::getQueryRelPos(const string& queryRelName) const {
    return queryRelNameToPosMap.at(queryRelName);
}

void QueryGraph::addQueryRel(unique_ptr<QueryRel> queryRel) {
    queryRelNameToPosMap.insert({queryRel->name, queryRels.size()});
    queryRels.push_back(move(queryRel));
}

vector<tuple<uint32_t, bool, bool>> QueryGraph::getConnectedQueryRelsWithDirection(
    const SubqueryGraph& subqueryGraph) const {
    vector<tuple<uint32_t, bool, bool>> result;
    for (auto relPos = 0u; relPos < queryRels.size(); ++relPos) {
        if (subqueryGraph.queryRelsSelector[relPos]) {
            continue;
        }
        auto srcNodePos = queryNodeNameToPosMap.at(queryRels.at(relPos)->getSrcNodeName());
        auto dstNodePos = queryNodeNameToPosMap.at(queryRels.at(relPos)->getDstNodeName());
        if (subqueryGraph.queryNodesSelector[srcNodePos] ||
            subqueryGraph.queryNodesSelector[dstNodePos]) {
            result.emplace_back(make_tuple(relPos, subqueryGraph.queryNodesSelector[srcNodePos],
                subqueryGraph.queryNodesSelector[dstNodePos]));
        }
    }
    return result;
}

unordered_set<pair<SubqueryGraph, uint32_t>, SubqueryGraphJoinNodePairHasher>
QueryGraph::getSingleNodeJoiningSubgraph(
    const SubqueryGraph& matchedSubqueryGraph, uint32_t subgraphSize) const {
    auto subgraphAndJoinNodePairs = initSubgraphWithSingleJoinNode(matchedSubqueryGraph);
    for (auto i = 1ul; i < subgraphSize; ++i) {
        auto nextSubgraphAndJoinNodePairs =
            unordered_set<pair<SubqueryGraph, uint32_t>, SubqueryGraphJoinNodePairHasher>();
        for (auto& subgraphAndJoinNodePair : subgraphAndJoinNodePairs) {
            auto tmp = extendSubgraphByOneQueryRel(matchedSubqueryGraph, subgraphAndJoinNodePair);
            nextSubgraphAndJoinNodePairs.insert(begin(tmp), end(tmp));
        }
        subgraphAndJoinNodePairs = move(nextSubgraphAndJoinNodePairs);
    }
    return subgraphAndJoinNodePairs;
}

bool QueryGraph::isConnected() const {
    auto visited = unordered_set<string>();
    auto frontier = unordered_set<string>();
    auto src = queryNodes.at(0)->name;
    frontier.insert(src);
    visited.insert(src);
    while (!frontier.empty()) {
        auto nextFrontier = unordered_set<string>();
        for (auto& nodeInFrontier : frontier) {
            auto nbrs = getNeighbourNodeNames(nodeInFrontier);
            for (auto& nbr : nbrs) {
                if (end(visited) == visited.find(nbr)) {
                    visited.insert(nbr);
                    nextFrontier.insert(nbr);
                }
            }
        }
        if (visited.size() == queryNodes.size()) {
            return true;
        }
        frontier = nextFrontier;
    }
    return false;
}

bool QueryGraph::operator==(const QueryGraph& other) const {
    auto result =
        queryNodes.size() == other.queryNodes.size() && queryRels.size() == other.queryRels.size();
    for (auto i = 0u; i < queryNodes.size(); ++i) {
        result &= *queryNodes.at(i) == *other.queryNodes.at(i);
    }
    for (auto i = 0u; i < queryRels.size(); ++i) {
        result &= *queryRels.at(i) == *other.queryRels.at(i);
    }
    return result;
}

unordered_set<string> QueryGraph::getNeighbourNodeNames(const string& queryNodeName) const {
    auto nbrs = unordered_set<string>();
    for (auto& rel : queryRels) {
        if (getQueryNode(rel->getSrcNodeName())->name == queryNodeName) {
            nbrs.insert(rel->getDstNodeName());
        }
        if (getQueryNode(rel->getDstNodeName())->name == queryNodeName) {
            nbrs.insert(rel->getSrcNodeName());
        }
    }
    return nbrs;
}

unordered_set<pair<SubqueryGraph, uint32_t>, SubqueryGraphJoinNodePairHasher>
QueryGraph::initSubgraphWithSingleJoinNode(const SubqueryGraph& matchedSubgraph) const {
    auto result = unordered_set<pair<SubqueryGraph, uint32_t>, SubqueryGraphJoinNodePairHasher>();
    for (auto relPos = 0u; relPos < queryRels.size(); ++relPos) {
        if (matchedSubgraph.queryRelsSelector[relPos]) {
            continue;
        }
        auto srcNodePos = queryNodeNameToPosMap.at(queryRels[relPos]->getSrcNodeName());
        auto dstNodePos = queryNodeNameToPosMap.at(queryRels[relPos]->getDstNodeName());
        // check join on single node
        if (matchedSubgraph.queryNodesSelector[srcNodePos] ||
            !matchedSubgraph.queryNodesSelector[dstNodePos]) {
            auto subgraph = SubqueryGraph(*this);
            subgraph.addQueryRel(relPos);
            result.emplace(make_pair(subgraph, srcNodePos));
        } else if (!matchedSubgraph.queryNodesSelector[srcNodePos] &&
                   matchedSubgraph.queryNodesSelector[dstNodePos]) {
            auto subgraph = SubqueryGraph(*this);
            subgraph.addQueryRel(relPos);
            result.emplace(make_pair(subgraph, dstNodePos));
        }
    }
    return result;
}

unordered_set<pair<SubqueryGraph, uint32_t>, SubqueryGraphJoinNodePairHasher>
QueryGraph::extendSubgraphByOneQueryRel(const SubqueryGraph& matchedSubgraph,
    const pair<SubqueryGraph, uint32_t>& subgraphWithSingleJoinNode) const {
    auto result = unordered_set<pair<SubqueryGraph, uint32_t>, SubqueryGraphJoinNodePairHasher>();
    auto& subgraph = subgraphWithSingleJoinNode.first;
    for (auto relPos = 0u; relPos < queryRels.size(); ++relPos) {
        if (matchedSubgraph.queryRelsSelector[relPos] || subgraph.queryRelsSelector[relPos]) {
            continue;
        }
        auto srcNodePos = queryNodeNameToPosMap.at(queryRels.at(relPos)->getSrcNodeName());
        auto dstNodePos = queryNodeNameToPosMap.at(queryRels.at(relPos)->getDstNodeName());
        if (subgraph.queryNodesSelector[srcNodePos] || subgraph.queryNodesSelector[dstNodePos]) {
            if (matchedSubgraph.queryNodesSelector[srcNodePos] &&
                srcNodePos != subgraphWithSingleJoinNode.second) { // single join node check on src
                continue;
            }
            if (matchedSubgraph.queryNodesSelector[dstNodePos] &&
                dstNodePos != subgraphWithSingleJoinNode.second) { // single join node check on dst
                continue;
            }
            auto newSubgraph = subgraph;
            newSubgraph.addQueryRel(relPos);
            result.emplace(make_pair(newSubgraph, subgraphWithSingleJoinNode.second));
        }
    }
    return result;
}

} // namespace planner
} // namespace graphflow