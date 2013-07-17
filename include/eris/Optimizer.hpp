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
        /** Perform optimization, calculating (but not applying) any actions of an agent.  This will
         * be called once, but may be called again if some optimizers indicate changes in their
         * postOptimize() methods.
         *
         * \sa Simulation::run()
         */
        virtual void optimize() = 0;

        /** Performs an optimization run after all agents have had their optimize() methods called.
         * This should return true if it changes any state that requires any agent to optimize,
         * false if nothing needs to be changed.  This is typically used by Markets that require
         * price adjustments to induce market clearing.  A default implementation is provided which
         * does nothing and returns false.
         *
         * \sa Simulation::run()
         */
        virtual bool postOptimize() { return false; }

        /** Applies changes calculated by optimize() (and possibly postOptimize()) calls.  This will
         * always be called exactly once per simulation run.
         *
         * \sa Simulation::run()
         */
        virtual void apply() = 0;

        /** Called at the beginning of an optimization round before optimize() calls.  By default
         * does nothing.  This will always be called before optimize()---even for the very first
         * optimization period---and so can do any pre-optimization initialization required.  Note,
         * however, that this may be called multiple times in the same simulation round if some
         * optimizer has a postOptimize().
         *
         * This method is not suitable for inter-period optimization changes; you should use an
         * InterOptimizer instance for that.  This method is intended simply to reset calculated
         * assets, purchases, etc. when a new optimization calculation begins.
         *
         * The default implementation does nothing.
         */
        virtual void reset() {}
};

/// Namespace for inter-period optimization implementations.
namespace interopt {}

/** Base class for inter-period optimization.  This class has three primary methods: optimize(), which
 * calculates changes to apply for the next period, apply() which applies changes that agents might
 * need during period advancement, and postAdvance() for changes that need to happen after agents
 * advance.  All three methods do nothing in the default implementations; classes must override at
 * least one to be useful.
 *
 * For example, updating a firm's quantity target would be calculated in optimize() and actually
 * updated in apply().  Providing income to an agent should happen in postAdvance() (since
 * Agent::advance() typically clears assets).
 */
class InterOptimizer : public Member {
    public:
        /** Called once when advancing a period.  This should calculate any changes (e.g. setting a
         * new price), but not apply them until apply() is called.  This method is declared const;
         * subclasses will typically need to declare some mutable fields to store changes the be
         * applied in apply().
         *
         * This method is distinct from apply() because all optimizations are intended to be
         * independent: that is, no optimize() call should change anything that can affect any other
         * InterOptimizer's optimize() call.
         *
         * The default implementation does nothing.
         */
        virtual void optimize() const {}

        /** Called to apply any changes calculated in optimize(), before agents advance().  Any
         * changes that affect agent advance() behaviour should happen here; any changes that don't
         * affect the agent behaviour until the period begins (such as asset changes) should be
         * deferred until postAdvance().
         *
         * The default implementation does nothing.
         */
        virtual void apply() {}

        /** Called to apply any changes calculated in optimize() or apply(), after agents advance().
         *
         * The default implementation does nothing.
         */
        virtual void postAdvance() {}
};

}
