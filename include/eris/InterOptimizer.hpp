#pragma once
#include <eris/Member.hpp>

namespace eris {

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
        /** Called once when advancing a period.  This should calculate any changes (for example,
         * setting a new production level), but not apply them until apply() is called.
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
