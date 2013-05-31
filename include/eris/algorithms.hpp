#pragma once
#include <functional>
#include <iterator>
#include <stack>
#include <type_traits>
#include <vector>

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
 */
template <typename It>
typename std::enable_if<std::is_base_of<std::forward_iterator_tag, typename std::iterator_traits<It>::iterator_category>::value>::type
all_combinations(
            const It &begin,
            const It &end,
            std::function<void(const std::vector<typename It::value_type> &)> func
            ) {

    // Store the current combination being considered
    std::vector<typename It::value_type> combination; // Will store the current combination being considered
 
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

}
