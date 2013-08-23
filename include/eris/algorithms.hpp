#pragma once
#include <functional>
#include <iterator>
#include <stack>
#include <type_traits>
#include <vector>
#include <limits>

namespace eris {

/** Calculates all combinations of all sizes of the given container elements.  For example, given
 * input {1,2,3} this yields combinations: {}, {1}, {1,2}, {1,2,3}, {1,3}, {2}, {2,3}, {3}.  The
 * specific order of sets is not guaranteed (the order above matches the current implementation, but
 * could change); within each set, the order will be the same as occurs in the passed-in iterators.
 *
 * The specific algorithm used here (which is subject to change) works by keeping a stack of
 * iterators, starting with just one element at the first iterator value.  In each loop iteration:
 * - the current set of stack elements defines an as-yet unseen combination, so call the function.
 * - Then we consider the top stack element and:
 *   - if it is at the end, pop it off and increment the (new) end iterator (the stack will always
 *     be in increasing order, so that will never go push the new top element past the end)
 *   - otherwise, add a new iterator to the stack starting at the next position.
 *
 * This algorithm is efficient, requiring exactly 1 iteration for each possible non-empty
 * combination (of which there are 2^n-1 for a given input of size n).
 *
 * \param begin a forward iterator pointing to the first element of the container whose elements are to
 *        be recombined.  Typically this is begin() or cbegin(), but starting elsewhere (to
 *        recombine only a subset of the container) is permitted.
 * \param end an iterator pointing at the <i>past-the-end</i> element.  Typically this is end() or
 *        cend(), but can be something else for recombining subsets.
 * \param func a void function that takes a single argument of a const std::vector<T> &, where T is
 *        the same type used by the start and end iterators.  This will be called for each (unique)
 *        combination of parameters.  The argument is a new unique list of elements of the input
 *        set.  Despite being in a vector, these values should really be treated as a set.  Values
 *        will be in the same order as encountered in the input iterator.
 *
 * \param T the underlying data type template parameter is required, unfortunately.  This shouldn't
 *        be necessary at all, since it is obtainable from the class of the given iterator, but gcc
 *        (as of 4.8.1) chokes on that, hence the additional (required) template parameter.
 */
// FIXME: this first 'T' template parameter shouldn't be needed (and isn't, if using clang++), but
// g++ needs it, so everything has to deal with it.  Try again, eventually, to remove it, changing
// the 'T' types below to 'typename It::value_type'.
template <typename T, typename It>
typename std::enable_if<std::is_base_of<std::forward_iterator_tag, typename std::iterator_traits<It>::iterator_category>::value>::type
all_combinations(
            const It &begin,
            const It &end,
            std::function<void(const std::vector<T> &)> func
            ) {

    // Store the current combination being considered
    std::vector<T> combination; // Will store the current combination being considered
 
    // The first combination is always the empty set; call with it:
    func(combination);

    std::stack<It> it_stack;

    if (begin != end) it_stack.push(begin);

    while (!it_stack.empty()) {
        auto it = it_stack.top();
        combination.push_back(*it);

        // Call the caller-provided function
        func(combination);

        auto n = std::next(it);
        if (n != end) {
            it_stack.push(n);
        }
        else {
            // We're at the end, so we need to pop ourselves and our value off the stack
            it_stack.pop();
            combination.pop_back();

            // Also pop off the previous value and increment it (the next value is going to be
            // pushed back on in the next iteration).
            combination.pop_back();
            if (!it_stack.empty()) ++it_stack.top();
        }
    }
}

/** Wrapper class around a pair of iterators that converts the pair into a range, so that a
 * for-range statement can be used.  The primary target of this is multimap's equal_range method,
 * which returns just such a pair.  This class is typically invoked via the range() function.
 */
template <class Iter>
class range_ final : public std::pair<Iter, Iter> {
    public:
        range_(std::pair<Iter, Iter> const &pair) : std::pair<Iter, Iter>(pair) {}
        Iter begin() const { return this->first;  }
        Iter end()   const { return this->second; }
};
/** Takes a std::pair of iterators that represents a range, and returns an iterable object for that
 * range.  This is intended to allow for range-based for loops for methods that return a pair of
 * iterators representing a range, such as multimap's equal_range() method.
 *
 * Example:
 *
 *     for (auto &whatever : eris::range(mmap.equal_range(key))) {
 *         ...
 *     }
 */
template <class Iter>
range_<Iter> range(std::pair<Iter, Iter> const &pair) {
    return range_<Iter>(pair);
}

/** Generic class for a stepping a value up or down by a certain amount, increasing or decreasing
 * the step size based on the previous steps.  This is often used in optimizers to find an optimal
 * output/price level.
 */
class Stepper final {
    public:
        static constexpr double default_initial_step = 1.0/32.0;
        static constexpr int    default_increase_count = 4;
        static constexpr double default_min_step = std::numeric_limits<double>::epsilon();
        static constexpr bool   default_relative_steps = true;

        /** Constructs a new Stepper object.
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
         * \param min_step is the minimum step size that can be taken.  The default is equal to
         * machine epsilon (i.e. the smallest value v such that 1 + v is a value distinct from 1),
         * which is the smallest value possible for a step.
         *
         * \param rel_steps specifies whether the steps taken are relative to the current value, or
         * absolute changes.  The default (relative steps) is suitable for things like prices and
         * quantities, when relative changes are more important than absolute values; specifying
         * false here is suitable for spatial models, when movement shouldn't be affected by the
         * current variable value.  Note that the default min_step is too small to numerically
         * affect values much below -1.0 or above +1.0, so choosing a more appropriate min_step is
         * advised.
         */
        Stepper(double initial_step = default_initial_step,
                int increase_count = default_increase_count,
                double min_step = default_min_step,
                bool rel_steps = default_relative_steps);

        /** Called to signal that a step increase (`up=true`) or decrease (`up=false`) should be
         * taken.
         *
         * If relative_steps is true, this returns the relative step value, where 1 indicates no
         * change, 1.2 indicates a 20% increase, etc.  The relative step multiple will always be a
         * strictly positive value.  If \f$s\f$ is the current step size, the returned value will be
         * either \f$1+s\f$ for an upward step or \f$\frac{1}{1+s}\f$ for a downward step.
         *
         * If relative_steps is false, this returns the absolute change in the current value, which
         * could be any positive or negative value; the returned value is the amount by which the
         * variable should be changed.
         *
         * When called, this first checks whether the step size should be changed: if at least
         * `increase` steps in the same direction have occured, the step size is doubled (and the
         * count of previous steps in this direction is halved); if the last step was in the
         * opposite direction, the step size is halved.
         */
        double step(bool up);

        /// The number of steps in the same direction required to double the step size
        const int increase;

        /// The minimum (relative) step size allowed, specified in the constructor.
        const double min_step;

        /** If true, steps are relative; if false, steps are absolute.
         *
         * For example, a relative step size of 1/10 results in a step to either 11/10 or 10/11 for
         * an upward or downward step, respectively.  Thus consecutive downward steps approach but
         * never reach zero.  For variables that have a positive domain (e.g. prices and
         * quantities), this is exactly what is wanted, because typically relative changes are more
         * important than absolute changes.
         *
         * For some problems, however, steps should be considered as absolute.  For example, in
         * a linear spatial problem, a step of 0.1 should be +0.1 or -0.1; it makes no sense to
         * scale steps by the current position (since that is mathematically arbitrary).  Thus,
         * when this value is false, the value passed to take_step will be the absolute change,
         * *not* a multiple of the current value.
         */
        const bool relative_steps;

        /// The most recent step size
        double step_size;

        /// The most recent step direction: true if the last step was positive, false if negative.
        bool prev_up = true;

        /** The number of steps that have occurred in the same direction.  When a step size doubling
         * occurs, this value is halved (since the previous steps are only half the size of current
         * steps).
         */
        int same = 0;

};


}
