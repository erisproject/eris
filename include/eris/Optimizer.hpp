#pragma once
#include <eris/Member.hpp>
#include <eris/Agent.hpp>
#include <eris/types.hpp>
#include <memory>

namespace eris {

/// Namespace for intra-period optimization implementations.
namespace intraopt {}

/** Optimization base-class for intra-period optimization.  This abstract class defines the basic
 * interface used by Eris to allow agents to optimize.  There are two primary methods: optimize(),
 * which has an agent optimize (e.g. a consumer picking an optimal bundle), and reset() which resets
 * variables before beginning a period.
 *
 * \sa InterOptimizer for inter-period optimization.
 */
class IntraOptimizer : public Member {
    public:
        /** Perform optimization.  Returns true if anything changed, false otherwise.  This will be
         * called repeatedly, across all simulation optimizers, until every optimizer returns false.
         * Any IntraOptimizer implementation that can return true indefinitely *without outside
         * state changes* is broken and will cause the simulation to fail.
         */
        virtual bool optimize() = 0;

        /** Called at the beginning of a period before any optimizations in that period.  By default
         * does nothing.  This will always be called before optimize()---even for the very first
         * optimization period---and so can do any pre-optimization initialization required.
         *
         * This method is not intended for inter-period optimization changes; you should use an
         * InterOptimizer instance for that.  This method is intended simply to reset things like
         * assets, consumption bundles, etc. when a new period begins.
         *
         * The default implementation does nothing.
         */
        virtual void reset() {}
};

/// Namespace for inter-period optimization implementations.
namespace intraopt {}

/** Base class for inter-period optimization.  This class has two primary methods: optimize(), which
 * calculates changes to apply for the next period, and apply() which applies those changes.
 */
class InterOptimizer : public Member {
    public:
        /** Called once when advancing a period.  This should calculate any changes (e.g. setting a
         * new price), but not apply them until apply() is called.  Should returns true if changes
         * need to be applied (i.e. if apply() needs to be called), false if no apply() call is
         * needed.
         */
        virtual bool optimize() = 0;

        /** Called to apply the changes calculated in optimize() if optimize() returned true.  If
         * optimize() returned false, calling this method is skipped.
         */
        virtual void apply() = 0;
};

}
