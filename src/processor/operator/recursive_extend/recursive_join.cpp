#include "processor/operator/recursive_extend/recursive_join.h"

namespace kuzu {
namespace processor {

bool ScanFrontier::getNextTuplesInternal(ExecutionContext* context) {
    if (!hasExecuted) {
        hasExecuted = true;
        return true;
    }
    return false;
}

void BaseRecursiveJoin::initLocalStateInternal(ResultSet* resultSet_, ExecutionContext* context) {
    for (auto& dataPos : dataInfo->vectorsToScanPos) {
        vectorsToScan.push_back(resultSet->getValueVector(dataPos).get());
    }
    srcNodeIDVector = resultSet->getValueVector(dataInfo->srcNodePos).get();
    dstNodeIDVector = resultSet->getValueVector(dataInfo->dstNodePos).get();
    pathLengthVector = resultSet->getValueVector(dataInfo->pathLengthPos).get();
    switch (dataInfo->joinType) {
    case planner::RecursiveJoinType::TRACK_PATH: {
        pathVector = resultSet->getValueVector(dataInfo->pathPos).get();
    } break;
    default: {
        pathVector = nullptr;
    } break;
    }
    initLocalRecursivePlan(context);
}

bool BaseRecursiveJoin::getNextTuplesInternal(ExecutionContext* context) {
    // There are two high level steps.
    //
    // (1) BFS Computation phase: Grab a new source to do a BFS and compute an entire BFS starting
    // from a single source;
    //
    // (2) Outputting phase: Once a BFS from a single source finishes, we output the results in
    // pieces of vectors to the parent operator.
    //
    // These 2 steps are repeated iteratively until all sources to do a BFS are exhausted. The first
    // if statement checks if we are in the outputting phase and if so, scans a vector to output and
    // returns true. Otherwise, we compute a new BFS.
    while (true) {
        if (scanOutput()) { // Phase 2
            return true;
        }
        auto inputFTableMorsel = sharedState->inputFTableSharedState->getMorsel(1 /* morselSize */);
        if (inputFTableMorsel->numTuples == 0) { // All src have been exhausted.
            return false;
        }
        sharedState->inputFTableSharedState->getTable()->scan(vectorsToScan,
            inputFTableMorsel->startTupleIdx, inputFTableMorsel->numTuples,
            dataInfo->colIndicesToScan);
        bfsMorsel->resetState();
        computeBFS(context); // Phase 1
        frontiersScanner->resetState(*bfsMorsel);
    }
}

bool BaseRecursiveJoin::scanOutput() {
    common::sel_t offsetVectorSize = 0u;
    common::sel_t dataVectorSize = 0u;
    if (pathVector != nullptr) {
        pathVector->resetAuxiliaryBuffer();
    }
    frontiersScanner->scan(
        pathVector, dstNodeIDVector, pathLengthVector, offsetVectorSize, dataVectorSize);
    if (offsetVectorSize == 0) {
        return false;
    }
    dstNodeIDVector->state->initOriginalAndSelectedSize(offsetVectorSize);
    return true;
}

void BaseRecursiveJoin::computeBFS(ExecutionContext* context) {
    auto nodeID = srcNodeIDVector->getValue<common::nodeID_t>(
        srcNodeIDVector->state->selVector->selectedPositions[0]);
    bfsMorsel->markSrc(nodeID.offset);
    while (!bfsMorsel->isComplete()) {
        auto boundNodeOffset = bfsMorsel->getNextNodeOffset();
        if (boundNodeOffset != common::INVALID_OFFSET) {
            // Found a starting node from current frontier.
            scanFrontier->setNodeID(common::nodeID_t{boundNodeOffset, nodeTable->getTableID()});
            while (recursiveRoot->getNextTuple(context)) { // Exhaust recursive plan.
                updateVisitedNodes(boundNodeOffset);
            }
        } else {
            // Otherwise move to the next frontier.
            bfsMorsel->finalizeCurrentLevel();
        }
    }
}

void BaseRecursiveJoin::updateVisitedNodes(common::offset_t boundNodeOffset) {
    auto boundNodeMultiplicity = bfsMorsel->currentFrontier->getMultiplicity(boundNodeOffset);
    for (auto i = 0u; i < tmpDstNodeIDVector->state->selVector->selectedSize; ++i) {
        auto pos = tmpDstNodeIDVector->state->selVector->selectedPositions[i];
        auto nodeID = tmpDstNodeIDVector->getValue<common::nodeID_t>(pos);
        auto edgeID = tmpEdgeIDVector->getValue<common::relID_t>(pos);
        bfsMorsel->markVisited(
            boundNodeOffset, nodeID.offset, edgeID.offset, boundNodeMultiplicity);
    }
}

void BaseRecursiveJoin::initLocalRecursivePlan(ExecutionContext* context) {
    auto op = recursiveRoot.get();
    while (!op->isSource()) {
        assert(op->getNumChildren() == 1);
        op = op->getChild(0);
    }
    scanFrontier = (ScanFrontier*)op;
    localResultSet = std::make_unique<ResultSet>(
        dataInfo->localResultSetDescriptor.get(), context->memoryManager);
    tmpDstNodeIDVector = localResultSet->getValueVector(dataInfo->tmpDstNodeIDPos).get();
    tmpEdgeIDVector = localResultSet->getValueVector(dataInfo->tmpEdgeIDPos).get();
    recursiveRoot->initLocalState(localResultSet.get(), context);
}

} // namespace processor
} // namespace kuzu
