#pragma once
#include <eris/Member.hpp>

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
        /** Perform optimization, calculating (but not finalizing) any actions of an agent.  This
         * will be called once, but may be called again if some optimizers indicate changes in their
         * postOptimize() methods, thus restarting the intra-period optimization.
         *
         * As an example, and IntraOptimizer for a consumer would create and store Market
         * Reservation objects, then complete the reservations during apply(), or abort them (or
         * just let them be destroyed, and thus automatically aborted) during reset().
         *
         * This method should not make any irrevesible changes to the simulation state; in
         * particular, anything established in optimize() must be undone if reset() is called.  Thus
         * establishing reservations is acceptable, but completing those reservations is not.
         *
         * A default implementation of this method is provided that does nothing.
         *
         * \sa Simulation::run()
         */
        virtual void optimize() {}

        /** Performs an optimization run after all agents have had their optimize() methods called.
         * This should return true if it changes any state that requires any agent to optimize,
         * false if nothing needs to be changed.  This is typically used by Markets that require
         * price adjustments to induce market clearing.
         *
         * This method is intended to be retrospective in that it looks at what happened during the
         * optimize() calls; it generally should not consider what other postOptimize() calls have
         * done, where feasible.
         *
         * A default implementation is provided which does nothing and returns false.
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

        /** Called before optimization begins, once per period.  Unlike reset(), this is called once
         * before the optimization rounds for a period begin.  
         *
         * The default implementation does nothing.
         */
        virtual void initialize() {}

        /** Called at the beginning of an optimization round before optimize() calls.  By default
         * does nothing.  This will always be called before optimize()---even for the very first
         * optimization period---and so can do any pre-optimization initialization required.  Note,
         * however, that this may be called multiple times in the same simulation round if some
         * optimizer has a postOptimize() that returns true.
         *
         * The default implementation does nothing.
         */
        virtual void reset() {}
};

}
