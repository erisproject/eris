#pragma once
#include <eris/Optimizer.hpp>

namespace eris { namespace interopt {

/** Inter-period optimizer that adjusts some value up or down by some increment depending on what
 * happened in the previous period.  The size of the step varies according to the past steps: when
 * subsequent steps change direction, the step size decreases; as steps consecutively continue in
 * the same direction, step sizes increase.
 *
 * This is an abstract base class and requires an implementation of the should_increase() and
 * take_step() methods.
 *
 * \sa PriceStepper a Stepper implementation adapted to PriceFirms
 * \sa QMOpt a Stepper implementation adapted to the price of Quantity markets
 */
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
         */
        Stepper(double step = 1.0/32.0, int increase_count = 4);

        /** Determines whether the price should go up or down, and by how much.  Calls
         * (unimplemented) method should_increase() to determine the direction of change.
         */
        virtual void optimize() const override;

        /** Applies the price change calculated in optimize().  This will call take_step() with the
         * relative price change (e.g. 1.25 to increase price by 25%).
         */
        virtual void apply() override;

    protected:
        /** Called to determine whether the price next period should go up or down.  A true return
         * value indicates that a price increase should be tried, false indicates a decrease.  This
         * typically makes use of the prev_up and same members as required.
         */
        virtual bool should_increase() const = 0;

        /** Called to change the price to the given multiple of the current price.  This is called
         * by apply().  The relative value will always be a positive value not equal to 1.
         */
        virtual void take_step(double relative) = 0;

        /** Called to check whether the step needs to be updated, and updates it (and related
         * variables) if so.  Note that the step will never be smaller than min_step.
         *
         * The step size is cut in half when the step direction is reversed; it is doubled if there
         * have been 
         */
        virtual void update_step();

        /** The minimum (relative) step size allowed: this is equal to machine epsilon (i.e. the
         * smallest value v such that 1 + v is a value distinct from 1.
         */
        const double min_step = std::numeric_limits<double>::epsilon();

    protected:
        /** Will be true when calling should_increase() if the previous step was an increase, false
         * otherwise.  Should not be used when step equals 0 (in other words, for the first step).
         */
        bool prev_up = true;
        /** When should_increase() is called, this contains the number of times of consecutive times
         * that the quantity has moved taken a step in the same direction.  The value is
         * approximate, however, as it is divided by 2 (rounding down) each time the step size is
         * doubled.  This will always be equal to at least 1 except for the very first optimization,
         * when it will equal 0.
         */
        int same = 0;

    private:
        double step_;
        const int increase_;
        mutable bool curr_up_ = false;
};

} }
