#pragma once
#include <eris/algorithms.hpp>
#include <eris/Optimize.hpp>
#include <eris/Member.hpp>

namespace eris { namespace interopt {

/** Inter-period optimizer that adjusts some value up or down by some increment depending on what
 * happened in the previous period.  The size of the step varies according to the past steps: when
 * subsequent steps change direction, the step size decreases; as steps consecutively continue in
 * the same direction, step sizes increase.
 *
 * This is an abstract base class and requires an implementation of the should_increase() and
 * take_step() methods.
 *
 * This class also supports jumping (instead of stepping) by overriding its should_jump() and
 * take_jump() methods.  Jumps are checked *every* period, even when the period parameter is such
 * that steps don't occur every period.
 *
 * \sa Stepper the class handling the step adjustment logic
 * \sa PriceStepper a Stepper implementation adapted to PriceFirms
 */
class InterStepper : public Member, public virtual OptApply {
    public:
        /// The default initial step, if not given in the constructor
        static constexpr double default_initial_step = Stepper::default_initial_step;
        /// The default increase count, if not given in the constructor
        static constexpr int default_increase_count = Stepper::default_increase_count;
        /// The default period, if not given in the constructor
        static constexpr int default_period = 1;
        /// The period offset, which must be less than the period.
        static constexpr int default_period_offset = 0;

        /** Constructs a new InterStepper optimization object.
         *
         * \param initial_step the initial size of a step, relative to the current value.  Defaults to 1/32
         * (about 3.1%).  Over time the step size will change based on the following options.  When
         * increasing, the new value is \f$(1 + step)\f$ times the current value; when decreasing
         * the value is \f$\frac{1}{1 + step}\f$ times the current value.  This ensures that an
         * increase followed by a decrease will result in the same value (at least within numerical
         * precision).
         *
         * \param increase_count if the value moves consistently in one direction this many times,
         * the step size will be doubled.  Defaults to 4 (i.e. the 4th step will be doubled).  Must
         * be at least 2, though values less than 4 aren't recommended in practice.  When
         * increasing, the previous increase count is halved; thus, with the default value of 4, it
         * takes only two additional steps in the same direction before increasing the step size
         * again.  With a value of 6, it would take 6 changes for the first increase, then 3 for
         * subsequent increases, etc.
         *
         * \param period is how often a step should take place.  The default, 1, means we should
         * take a step every period; 2 would mean every second period, etc.  Note that jumping is
         * checked and can occur in every period, regardless of this value, and that a jump occuring
         * resets the period counter.
         *
         * \param period_offset If period is greater than 1, this allows control over which periods
         * optimization happens in.  Specifically, optimization happens in periods \f$n + o, 2n + o,
         * 3n + o, \hdots\f$ (where n is the period and o is the offset).  For example, if offset is
         * 0 (the default) and period is 3, optimization occurs in the 3rd, 6th, 9th, etc. periods;
         * if offset was instead 1, optimization would happen in 1st, 4th, 7th, etc. periods.  The
         * given value must be less than the period.
         */
        InterStepper(
                double initial_step = default_initial_step,
                int increase_count = default_increase_count,
                int period = default_period,
                int period_offset = default_period_offset);

        /** Constructs a new InterStepper optimization object using the given Stepper object instead
         * of creating a new one.
         *
         * \param stepper Stepper object controlling the steps.
         *
         * \param period same as in InterStepper(double, int, int) constructor.
         */
        InterStepper(Stepper stepper, int period = default_period, int period_offset = default_period_offset);

        /** Determines whether the value should go up or down, and by how much.  Calls
         * (unimplemented) method should_increase() to determine the direction of change.
         */
        virtual void interOptimize() const override;

        /** Applies the value change calculated in optimize().  This will call take_step() with the
         * relative value change (e.g. 1.25 to increase value by 25%).
         */
        virtual void interApply() override;

    protected:
        /// The Stepper object used to handle step adjustment logic
        Stepper stepper_;

        /** Called to determine whether the value next period should go up or down.  A true return
         * value indicates that a value increase should be tried, false indicates a decrease.  This
         * typically makes use of the prev_up and same members as required.
         */
        virtual bool should_increase() const = 0;

        /** Called to change the value to the given multiple of the current value, or to change the
         * value by the given amount (see the `rel_steps` parameter for Stepper).  This is called by
         * apply().  The relative value will always be a positive value not equal to 1; an absolute
         * value can be any value.
         */
        virtual void take_step(double step) = 0;

        /** Called to determine whether the value next period should jump instead of taking a step.
         * Typically this is used to identify when a custom jump is needed instead of the usual
         * step.  If this returns true, take_jump() will be called instead of take_step().  If this
         * returns false, the usual stepping procedure occurs.
         *
         * The default implementation of this method always returns false.
         *
         * If a jump occurs, the Stepper `same` parameter is reset to 0, and the period counter (for
         * stepping that doesn't occur in every period) is reset.
         */
        virtual bool should_jump() const;

        /** Called when should_jump() returned true when applying the interopt change.
         *
         * The default implementation does nothing.
         */
        virtual void take_jump();

        /// Whether we are going to step up.  Calculated in optimize(), given to stepper_ in apply()
        mutable bool curr_up_ = false;
        /// The period; we only try to take a step every `period_` times.  1 means always.
        const int period_;
        /// The offset; we take a step in periods in which `last_step_ % period_ == period_offset_`
        const int period_offset_;
        mutable long last_step_ = 0;
        /// True if we're going to step this round.  Will be always true if `period_ == 1`.
        mutable bool stepping_ = false;
        /// True if we're going to jump this round instead of stepping.
        mutable bool jump_ = false;
};


} }
