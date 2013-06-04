#pragma once
#include <eris/Simulation.hpp>
#include <eris/Agent.hpp>
#include <eris/types.hpp>
#include <memory>

namespace eris {

/** Optimization base-class.  This abstract class defines the basic interface used by Eris to allow
 * agents to optimize.
 */
class Optimizer {
    public:
        virtual ~Optimizer() = default;

        /** Constructs an optimizer using the given agent.  The Agent must have been added to a
         * Simulation object already; otherwise (or if the Simulation has been destroyed), this will
         * throw a std::bad_weak_ptr exception.
         */
        Optimizer(const Agent &agent);

        /** Perform optimization.  Returns true if anything changed, false otherwise.  This will be
         * called repeatedly, across all simulation agents, until every agent returns false.  Any
         * Optimizer implementation that can return true for subsequent calls *without outside state
         * changes* is broken and will cause the simulation to fail.
         */
        virtual bool optimize() = 0;

        /** Called at the beginning of a period before any optimizations in that period.  By default
         * does nothing.  This will always be called before optimize()---even for the very first
         * optimization period---and so can do any pre-optimization initialization required.
         */
        virtual void reset();
    protected:
        /** Returns a shared pointer to the simulation object.  Throws an exception if the simulator
         * has gone away.
         */
        std::shared_ptr<Simulation> simulation();

        /** Returns the agent this optimizer is attached to.  Throws an exception if the simulator
         * has gone away.
         */
        SharedMember<Agent> agent();

        /** Returns a reference to the Agent's assets Bundle. */
        BundleNegative& assets();

        /** The id of the agent to which this optimizer is attached. */
        eris_id_t agent_id;

    private:
        std::weak_ptr<Simulation> sim;
};

}
