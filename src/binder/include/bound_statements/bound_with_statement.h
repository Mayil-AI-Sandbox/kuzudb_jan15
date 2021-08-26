#pragma once

#include "src/binder/include/bound_statements/bound_return_statement.h"

namespace graphflow {
namespace binder {

/**
 * BoundWithStatement may not have whereExpression
 */
class BoundWithStatement : public BoundReturnStatement {

public:
    explicit BoundWithStatement(unique_ptr<BoundProjectionBody> projectionBody)
        : BoundReturnStatement{move(projectionBody)} {}

    inline void setWhereExpression(shared_ptr<Expression> expression) {
        whereExpression = move(expression);
    }

    inline bool hasWhereExpression() const { return whereExpression != nullptr; }
    inline shared_ptr<Expression> getWhereExpression() const { return whereExpression; }

    vector<shared_ptr<Expression>> getDependentProperties() const override;

private:
    shared_ptr<Expression> whereExpression;
};

} // namespace binder
} // namespace graphflow
