#pragma once
#include <eris/algorithms.hpp>
#include <eris/InterOptimizer.hpp>

namespace eris { namespace interopt {

/** Inter-period optimizer that adjusts some value up or down by some increment depending on what
 * happened in the previous period.  The size of the step varies according to the past steps: when
 * subsequent steps change direction, the step size decreases; as steps consecutively continue in
 * the same direction, step sizes increase.
 *
 * This is an abstract base class and requires an implementation of the should_increase() and
 * take_step() methods.
 *
 * \sa eris::Stepper the class handling the step adjustment logic
 * \sa PriceStepper a Stepper implementation adapted to PriceFirms
 */
class JumpStepper;
class Stepper : public InterOptimizer {
    public:
        /** Constructs a new Stepper optimization object.
         *
         * \param step the initial size of a step, relative to the current value.  Defaults to 1/32
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
         * take a step every period; 2 would mean every second period, etc.
         */
        Stepper(double step = 1.0/32.0, int increase_count = 4, int period = 1);

        /** Determines whether the value should go up or down, and by how much.  Calls
         * (unimplemented) method should_increase() to determine the direction of change.
         */
        virtual void optimize() const override;

        /** Applies the value change calculated in optimize().  This will call take_step() with the
         * relative value change (e.g. 1.25 to increase value by 25%).
         */
        virtual void apply() override;

    protected:
        /// The eris::Stepper object used to handle step adjustment logic
        eris::Stepper stepper_;

        /** Called to determine whether the value next period should go up or down.  A true return
         * value indicates that a value increase should be tried, false indicates a decrease.  This
         * typically makes use of the prev_up and same members as required.
         */
        virtual bool should_increase() const = 0;

        /** Called to change the value to the given multiple of the current value.  This is called
         * by apply().  The relative value will always be a positive value not equal to 1.
         */
        virtual void take_step(double relative) = 0;

        /// Whether we are going to step up.  Calculated in optimize(), given to stepper_ in apply()
        mutable bool curr_up_ = false;
        /// The period; we only try to take a step every `period_` times.  1 means always.
        const int period_;
        /// Tracks how many steps we've taken since the last step, used for periodic stepping.
        mutable int last_step_ = 0;
        /// True if we're going to step this round.  Will be always true if `period_ == 1`.
        mutable bool stepping_ = false;

        friend class JumpStepper;
};

/** This class extends Stepper with the ability to take jumps instead of relative steps in
 * exceptional circumstances, and stepping via Stepper the rest of the time.
 */
class JumpStepper : public Stepper {
    public:
        /** Creates a new JumpStepper.  Arguments are passed to Stepper constructor.
         *
         * \sa Stepper::Stepper
         */
        JumpStepper(double step = 1.0/32.0, int increase_count = 4, int period = 1);

        /** Wraps Stepper::optimize() to check for should_jump().  If it returns false, calls
         * Stepper::optimize().
         */
        virtual void optimize() const override;

        /** Wraps Stepper::optimize(); if should_jump() returned false, Stepper::optimize() is
         * called; otherwise the eris::Stepper `same` parameter is reset and take_jump() is called.
         */
        virtual void apply() override;

    protected:
        /** Called to determine whether the value next period should jump instead of taking a step.
         * Typically this is used to identify when a custom jump is needed instead of the usual
         * step.  If this returns true, take_jump() will be called instead of take_step().  If this
         * returns false, the usual Stepper procedure occurs.
         *
         * If a jump occurs, the Stepper `same` parameter is reset to 0.
         */
        virtual bool should_jump() const = 0;

        /** Called when should_jump() returned true when applying the interopt change. */
        virtual void take_jump() = 0;

    private:
        mutable bool jump_ = false;
};

} }
