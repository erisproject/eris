#pragma once
#include <eris/Consumer.hpp>
#include <eris/Optimizer.hpp>
#include <eris/Simulation.hpp>

namespace eris { namespace optimizer {

/** Simple buyer that has a single 'money' good and uses it to buy from the Simulation's Markets.
 *
 * Requires a consumer with differentiable utility.
 *
 * When optimize() is called, the consumer attempts to spend available assets of the 'money'
 * good in the markets offering goods for the money good.  The consumer will spend until the utility
 * benefit of spending exceeds the utility decrease of the income (which could be zero, if money
 * doesn't enter the consumer's utility function; or could not be, e.g. for a partial equilibrium
 * model with an outside numeraire good).
 *
 * In making the decision, the consumer divides income in 'spending_chunks' pieces, deciding to
 * spend on whichever market results in the largest utility gain.
 *
 * In the case of ties, the consumer contemplates spending equally across permutations of the tied
 * markets (e.g. if 1,2,3 are tied, the consumer considers {1,2}, {1,3}, {2,3}, {1,2,3}); if there
 * are still ties for highest utility, the consumer chooses an option randomly.
 *
 * You can, optionally, always consider all equal-spending permutations by using the
 * permutations=true option.  This will, of course, be slower, particularly when there are many
 * markets, but may work better when there are close (and, especially, perfect) substitutes.  Equal
 * spending may not, of course, be optimal (e.g. consider \f$u(x,y)=min(x,2y)\f$), but as long as
 * spending_chunks is reasonably high, this will get close to an optimal solution.
 *
 * \todo This optimizer doesn't currently consider Good::Discrete goods (i.e. goods with an
 * increment not equal to 0).  Here's how, I think, they could be handled:
 * - Pick one of the discrete goods.
 *   - Loop through all possible values of it that we can afford.
 *     - For each remaining discrete good, recursively do the same thing (taking into account the
 *       current discrete choices further up the recursive chain)
 *     - Once we've run out of discrete goods, optimize the remaining continuous goods
 * - then unwind everything and pick the highest
 *
 * \todo This optimizer has a rather nice little combinator generation code that really should be
 * moved into sort sort of utility class, as it will undoubtedly be useful elsewhere in the future.
 */

class SimpleBuyer : public Optimizer {
    public:
        /** Constructs a new SimpleBuyer optimization object for a given agent.
         *
         * \param consumer The Consumere object (or subclass thereof) that this optimizer works on.
         * \param money The eris_id_t for the money good for this agent.  Only markets that have a
         * price consisting only of this good will be considered.
         * \param spending_chunks How many chunks income should be divided into when deciding on how
         * to optimize.  A higher value results in (potentially) more accurate results, but requires
         * more computational time to calculate.  Defaults to 100.
         * \param permutations If true, considers both purchases from individual markets as well as
         * purchases splitting income equally across market combinations.  When false (the default),
         * permutations are only considered among markets with equal and maximum utility benefits.
         */
        SimpleBuyer(
                const Consumer &consumer,
                eris_id_t money,
                int spending_chunks = 100
                );
        /** Specifies the threshold for considering buying from a combination of markets, as a
         * proportion of the largest utility gain.  Thus 1.0 means to only consider combinations
         * when there are multiple individual options tied for highest utility gain; 0.9 considers
         * permutations of options that gain at least 90% of the utility gain of the best option;
         * 0.0 considers permutations of all options that have non-negative utility changes.
         *
         * Negative values are also supported.  \f$-\infty\f$ is allowed to consider all
         * permutations all the time, (but calling the equivalent permuteAll() for that case is
         * preferred).  Values larger than 1.0 and NaN are treated as 1.0.
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
        virtual bool optimize() override;
        // FIXME: Consumer needs to call this somehow when advancing, which probably means
        // eris::Firm->advanced needs to move to eris::Agent, or possibly even eris::Member.
        virtual void reset() override;
    protected:
        const eris_id_t money;
        const int spending_chunks;
        double increment, threshold;
        bool permute_zeros = false;
};

}}
