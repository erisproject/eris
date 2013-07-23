#pragma once
#include <eris/Consumer.hpp>
#include <eris/IntraOptimizer.hpp>
#include <eris/Market.hpp>
#include <forward_list>

namespace eris { namespace intraopt {

/** Simple buyer that has a single 'money' good and uses it to buy from the Simulation's Markets
 * in small increments until all money is spent.
 *
 * When optimize() is called, the consumer attempts to spend available assets of the 'money'
 * good in the markets offering goods for the money good.  The consumer will spend until the utility
 * benefit of spending exceeds the utility decrease of the income (which could be zero, if money
 * doesn't enter the consumer's utility function; or could not be, e.g. for a partial equilibrium
 * model with an outside numeraire good).
 *
 * In making the decision, the consumer divides income in several pieces, deciding to spend on
 * whichever market results in the largest utility gain.
 *
 * In the case of ties, the consumer contemplates spending equally across permutations of the tied
 * markets (e.g. if 1,2,3 are tied, the consumer considers {1,2}, {1,3}, {2,3}, {1,2,3}); if there
 * are still ties for highest utility, the consumer chooses an option randomly.
 *
 * This algorithm will not obtain an optimum allocation in some cases: particularly when goods are
 * substitutes.  In particular, choosing a single step optimum at every stage may involve spending on
 * good A initially, and good B later, but when A and B are substitutes, the spending on good B
 * reduces the marginal utility of good A.  Since this algorithm does not backtrack, this will
 * result in a situation where too much has been spent on good A and too little on good B.
 *
 * You can, optionally, always consider all equal-spending permutations by using the
 * permutations=true option.  This will, of course, be slower, particularly when there are many
 * markets, but may work better when there are close (and, especially, perfect) substitutes.  Equal
 * spending may not, of course, be optimal (e.g. consider \f$u(x,y)=min(x,2y)\f$), but as long as
 * spending_chunks is reasonably high, this will get close to an optimal solution.
 *
 * \todo This optimizer should now be able to back-track by cancelling reservations, which should
 * allow working around the failure of this class to handle substitute goods properly.
 *
 * \todo This optimizer doesn't currently consider Good::Discrete goods (i.e. goods with an
 * increment not equal to 0).  Here's how, I think, they could be handled:
 * - Pick one of the discrete goods.
 *   - Loop through all possible values of it that we can afford.
 *     - For each remaining discrete good, recursively do the same thing (taking into account the
 *       current discrete choices further up the recursive chain)
 *     - Once we've run out of discrete goods, optimize the remaining continuous goods
 * - then unwind everything and pick the highest
 */

class IncrementalBuyer : public IntraOptimizer {
    public:
        /** Constructs a new IncrementalBuyer optimization object for a given agent.
         *
         * \param consumer The Consumer object (or subclass thereof) that this optimizer works on.
         * \param money The eris_id_t for the money good for this agent.  Only markets that have a
         *        price consisting only of this good will be considered.
         * \param rounds How many rounds to take to spend all income when optimizing.  A value of
         *        100 (the default) means each round will use up approximately 1% of income.  Note
         *        that the actual allocation here is slightly more complicated: in round 1, 1/100 of
         *        income is spent; in round 2, 1/99 in income is spent, and so on until all
         *        remaining income is spent in round 100.  Higher values result in a more optimal
         *        choice, but require more computational time.
         */
        IncrementalBuyer(
                const Consumer &consumer,
                eris_id_t money,
                int rounds = 100
                );

        /// No default constructor
        IncrementalBuyer() = delete;

        /** Specifies the threshold for considering buying from a combination of markets, as a
         * proportion of the largest utility gain.  Thus 1.0 means to only consider combinations
         * when there are multiple individual options tied for highest utility gain; 0.9 considers
         * permutations of options that gain at least 90% of the utility gain of the best option;
         * 0.0 considers permutations of all options that have non-negative utility changes.
         *
         * Negative values are also supported.  \f$-\infty\f$ is allowed to consider all
         * permutations all the time, (but calling the equivalent permuteAll() for that case is
         * preferred).  Values larger than 1.0 and NaN are treated as 1.0.
         *
         * Defaults to 1.0 - 1e-8, which only checks choices that are within a numerical error of
         * the maximum.
         */
        void permuteThreshold(const double &thresh) noexcept;
        /** Always consider all permutations of market combinations in addition to the individual
         * market choices.  This is equivalent to calling permuteThreshold with \f$-\infty\f$.
         */
        void permuteAll() noexcept;
        /** If enabled, in addition to the most preferred (as interpreted using the permuteThreshold() or
         * permuteAll() values) markets, permutations involving markets with a utility change of 0
         * will be considered, even when there are positive utility options.
         */
        void permuteZeros(const bool &pz) noexcept;

        /** Performs optimization.  When this is called the consumer takes the number of steps
         * specified in the constructor, purchasing whichever goods yields the highest utility gain
         * at each step.
         */
        virtual void optimize() override;

        /** Completes any purchases calculated and reserved in optimize(). */
        virtual void apply() override;

        /** Resets any optimization, discarding reservations previously calculated in optimize(), if
         * any.
         */
        virtual void reset() override;

    protected:
        const eris_id_t con_id;
        const eris_id_t money;
        const Bundle money_unit;
        const int rounds = 100;
        int round = -1;
        double threshold = 1.0 - 1e-8;
        bool permute_zeros = false;
        std::forward_list<Market::Reservation> reservations;

        virtual void added() override;

        /** Performance one step of optimization, reserving (approximately) 1/rounds of income each
         * time.  This is called repeatedly by optimize().
         */
        virtual bool oneRound();
};

}}
