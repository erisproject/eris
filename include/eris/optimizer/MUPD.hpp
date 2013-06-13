#pragma once

#include <eris/Consumer.hpp>
#include <eris/Optimizer.hpp>
#include <eris/Simulation.hpp>

namespace eris { namespace optimizer {

/** Optimizer class that picks an optimal bundle by attempting to equate marginal utility per
 * marginal dollar across the available goods.  This is restricted to Consumer::Differentiable
 * consumers due to the need to calculate marginal utility, and is only capable of dealing with
 * markets with a single money good as purchase price.
 *
 * This class works by considering spending equally in every market, then transferring expenditure
 * from the lowest (potentially negative) market to the highest market, and iterating until the
 * marginal utility per money unit is equal in each available market.  Multi-good markets are
 * handled (the marginal utility is the sum of the marginal utility of the individual goods).
 */
class MUPD : public Optimizer {
    public:
        /** Constructors a MUPD optimizer given a differentiable consumer instance and a money good.
         *
         * \param tolerance the relative tolerance of the algorithm; optimization will stop when the
         * difference between highest and lowest MU/$ values (calculated as \f$ \frac{highest -
         * lowest}{highest} \f$ is smaller than this value.  Defaults to 1.0e-8.
         */
        MUPD(const Consumer::Differentiable &consumer, const eris_id_t &money, double tolerance = 1.0e-8);

        /** Attempts to spend any available money optimally.  Returns false is no money was spent;
         * typically, with no other changes to the economy between calls, optimize() will return
         * false on the second call.
         */
        virtual bool optimize() override;

        /** The relative tolerance level at which optimization stops. */
        double tolerance;

    protected:
        const eris_id_t money;
        const Bundle money_unit;

        /** Stores the quantity allocation information for a particularly spending allocation.
         * Returned by spending_allocation().
         */
        struct allocation {
            /// The Bundle of final quantities that would be purchased
            Bundle bundle;
            /// A map of market -> quantities purchased
            std::unordered_map<eris_id_t, double> quantity;
        };

        /** Calculates the Bundle that the given spending allocation will buy.  A market id of 0 is
         * interpreted as a pseudomarket for holding onto cash, i.e. the "spending" is just held as
         * cash.
         */
        allocation spending_allocation(const std::unordered_map<eris_id_t, double> &spending);

        /** Calculates the marginal utility per money unit evaluated at the given Bundle.
         * \param con the shared consumer object
         * \param mkt_id the market id for which to calculate
         * \param a the allocation as returned by spending_allocation
         * \param b the Bundle at which to evaluate marginal utility
         */
        double calc_mu_per_d(
                const SharedMember<Consumer::Differentiable> &con,
                const eris_id_t &mkt_id,
                const allocation &a,
                const Bundle &b);

        std::unordered_map<eris_id_t, double> price_ratio_cache;
        /// Returns the ratio between the market's output price and the optimizer's money unit.
        /// Results are cached for performance.
        double price_ratio(const SharedMember<Market> &m);
};

} }
