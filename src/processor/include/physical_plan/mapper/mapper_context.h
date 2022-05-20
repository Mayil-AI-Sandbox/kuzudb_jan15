#pragma once

#include "src/processor/include/physical_plan/result/result_set_descriptor.h"

namespace graphflow {
namespace processor {

class MapperContext {

public:
    explicit MapperContext(unique_ptr<ResultSetDescriptor> resultSetDescriptor)
        : resultSetDescriptor{move(resultSetDescriptor)}, physicalOperatorID{0} {}

    inline ResultSetDescriptor* getResultSetDescriptor() const { return resultSetDescriptor.get(); }

    inline DataPos getDataPos(const string& name) const {
        return resultSetDescriptor->getDataPos(name);
    }

    // We keep track of computed expressions during a bottom-up mapping of logical plan so that we
    // can differentiate if an expression is leaf or not.
    inline void addComputedExpressions(const string& name) { computedExpressionNames.insert(name); }

    inline bool expressionHasComputed(const string& name) const {
        return computedExpressionNames.contains(name);
    }

    inline uint32_t getOperatorID() { return physicalOperatorID++; }

private:
    unique_ptr<ResultSetDescriptor> resultSetDescriptor;
    unordered_set<string> computedExpressionNames;

    uint32_t physicalOperatorID;
};

} // namespace processor
} // namespace graphflow
