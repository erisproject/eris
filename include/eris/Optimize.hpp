#pragma once

namespace eris {

/** Namespace for inter-period optimization implementations, and for the generic interfaces for the
 * different types of dedicated inter-period optimizers.
 *
 * The core optimization interfaces should be inherited using multiple inheritance.  Be careful to
 * inherit each as `public virtual` (for example, `class MyAgent : public Agent, public virtual
 * interopt::Advance`): inheriting multiply *without* virtual will fail if both a base and derived
 * class inherit from the same optimization class, since objects would then not be uniquely
 * castable to the optimizer interface.
 *
 * \sa Simulation::run()
 */
namespace interopt {

/** Interface for an inter-optimizing member with an interOptimize() method.  This will called
 * before starting a new period, before interApply().
 *
 * You should implement this class by inheriting it as `public virtual`.
 *
 * \sa Simulation::run()
 */
class Optimize {
    public:
        /** This method is intended to calculate changed to be applied in an interApply() method,
         * but does not apply them.  It is deliberately marked const to make it clear that it
         * shouldn't change the object; an object storing information for apply will need to use a
         * mutable property for this purpose.
         */
        virtual void interOptimize() const = 0;
    protected:
        ~Optimize() = default;
};

/** Interface for an inter-optimizing member with an interApply() method.  This will called
 * before starting a new period, after interOptimize() and before interAdvance().
 *
 * You should implement this class by inheriting it as `public virtual`.
 *
 * \sa Simulation::run()
 */
class Apply {
    public:
        /// This method is intended to apply any changes calculated in interOptimize().
        virtual void interApply() = 0;
    protected:
        ~Apply() = default;
};

/** Shortcut for inheriting from both interopt::Optimize and interopt::Apply
 */
class OptApply : public virtual Optimize, public virtual Apply {};

/** Interface for an inter-optimizing member with an interAdvance() method.  This will called
 * before starting a new period, after interApply() and before interPostAdvance().
 *
 * You should implement this class by inheriting it as `public virtual`.
 *
 * \sa Simulation::run()
 */
class Advance {
    public:
        /** This method is called to take care of any actions that need to be performed when
         * advancing.  For example, eris::agent::AssetAgent uses interAdvance to clear the agent's
         * assets bundle.
         */
        virtual void interAdvance() = 0;
    protected:
        ~Advance() = default;
};

/** Interface for an inter-optimizing member with an interPostAdvance() method.  This will called
 * before starting a new period, after interAdvance().
 *
 * You should implement this class by inheriting it as `public virtual`.
 *
 * \sa Simulation::run()
 */
class PostAdvance {
    public:
        /** This method is an extra stage that takes place after interAdvance and can be used to
         * perform extra advance tasks that depend on completion of the Advance stage.
         */
        virtual void interPostAdvance() = 0;
    protected:
        ~PostAdvance() = default;
};

}

/** Namespace for intra-period optimization implementations, and for the generic interfaces for the
 * different types of dedicated intra-period optimizers.
 */
namespace intraopt {

/** Interface for an intra-optimizing member with an intraInitialize() method.  This will called
 * once at the beginning of a new period, after inter-period optimization has completed.
 *
 * You should implement this class by inheriting it as `public virtual`.
 *
 * \sa Simulation::run()
 */
class Initialize {
    public:
        /** Called before optimization begins, once per period.  Unlike reset(), this is called once
         * before the optimization rounds for a period begin.  
         */
        virtual void intraInitialize() = 0;
    protected:
        ~Initialize() = default;
};

/** Interface for an intra-optimizing member with an intraReset() method.  This will called after
 * intraInitialize(), and may be called after intraReoptimize() to reset the initialization
 * procedure.
 *
 * You should implement this class by inheriting it as `public virtual`.
 *
 * \sa Simulation::run()
 */
class Reset {
    public:
        /** Called after intraInitialize() and possibly after intraReoptimize().  Called before
         * intraOptimize().
         */
        virtual void intraReset() = 0;
    protected:
        ~Reset() = default;
};

/** Interface for an intra-optimizing member with an intraOptimize() method.  This will called
 * after intraReset() to calculate an optimum decision for the period that will be finalized in
 * intraApply() or aborted by intraReset().
 *
 * You should implement this class by inheriting it as `public virtual`.
 *
 * \sa Simulation::run()
 */
class Optimize {
    public:
        /** Perform optimization, calculating (but not finalizing) any actions (typically of an
         * agent).  This will be called once, but may be called again if some optimizers indicate
         * changes in their intraReoptimize() methods, thus restarting the intra-period
         * optimization.
         *
         * As an example, an Optimize-implementing class for a consumer could create and store
         * Market Reservation objects, then complete the reservations during apply(), or abort them
         * (or just let them be destroyed, and thus automatically aborted) during reset().
         *
         * This method should not make any irreversible changes to the simulation state; in
         * particular, anything established in optimize() must be undone if reset() is called.  Thus
         * establishing reservations, for example, is acceptable, but completing those reservations
         * must be delayed until intraApply() and aborted if intraReset() is called.
         *
         * \sa Simulation::run()
         */
        virtual void intraOptimize() = 0;
    protected:
        ~Optimize() = default;
};

/** Interface for an intra-optimizing member with an intraReoptimize() method.  This will after
 * any Optimize classes, and can return true to restart intra-period optimization.  This is called
 * after Optimize, and before Apply (if all intra-period optimizers return false here), or else
 * before Reset (if any intra-period optimizer returns true).
 *
 * Note that IntraReoptimize-implementing classes are *not* short-circuited: intraReoptimize() is
 * called on every implementing class, regardless of whether earlier implementors returned a true
 * value.
 *
 * You should implement this class by inheriting it as `public virtual`.
 *
 * \sa Simulation::run()
 */
class Reoptimize {
    public:
        /** Called after optimization to determine whether the entire intra-period optimization
         * round needs to be restarted.  If *any* intra-period optimizer returns true here, the
         * round will be restarted (and so intraReset() will be called next); if all return false,
         * intraApply() is called next.
         *
         * This should return true if it changes any state that requires any agent to optimize,
         * false if nothing needs to be changed.  This is typically used by Markets that require
         * price adjustments to induce market clearing.
         *
         * This method is intended to be retrospective in that it looks at what happened during the
         * optimize() calls; it generally should not make changes that are visible to other, later
         * intraReoptimize() calls.
         */
        virtual bool intraReoptimize() = 0;
    protected:
        ~Reoptimize() = default;
};

/** Called after Reoptimize, when all Reoptimize-implementors returned false, indicating that no
 * further reoptimization is required.
 *
 * You should implement this class by inheriting it as `public virtual`.
 *
 * \sa Simulation::run()
 */
class Apply {
    public:
        /** Applies changes calculated by optimize() (and possibly postOptimize()) calls.  This will
         * always be called exactly once per simulation period.
         */
        virtual void intraApply() = 0;
    protected:
        ~Apply() = default;
};
/// Shortcut for inheriting from intraopt::Optimize intraopt::Apply, and intraopt::Reset
class OptApplyReset : public virtual Optimize, public virtual Apply, public virtual Reset {};
/// Shortcut for inheriting from intraopt::Optimize intraopt::Apply
class OptApply : public virtual Optimize, public virtual Apply {};

}
}
