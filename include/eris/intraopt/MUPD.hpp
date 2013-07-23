#pragma once

#include <eris/Consumer.hpp>
#include <eris/IntraOptimizer.hpp>
#include <eris/Market.hpp>
#include <unordered_map>
#include <unordered_set>
#include <forward_list>

namespace eris { namespace intraopt {

/** IntraOptimizer class that picks an optimal bundle by attempting to equate marginal utility per
 * marginal dollar across the available goods.  This is restricted to Consumer::Differentiable
 * consumers due to the need to calculate marginal utility, and is only capable of dealing with
 * markets with a single money good as purchase price.
 *
 * This class works by considering spending equally in every market, then transferring expenditure
 * from the lowest (potentially negative) market to the highest market, and iterating until the
 * marginal utility per money unit is equal in each available market.  Multi-good markets are
 * handled (the marginal utility is the sum of the marginal utility of the individual goods).
 */
class MUPD : public IntraOptimizer {
    public:
        /** Constructors a MUPD optimizer given a differentiable consumer instance and a money good.
         *
         * \param tolerance the relative tolerance of the algorithm; optimization will stop when the
         * difference between highest and lowest MU/$ values (calculated as \f$ \frac{highest -
         * lowest}{highest} \f$ is smaller than this value.  Defaults to 1.0e-10.
         */
        MUPD(const Consumer::Differentiable &consumer, const eris_id_t &money, double tolerance = 1.0e-10);

        /** Attempts to spend any available money optimally.  Returns false is no money was spent;
         * typically, with no other changes to the economy between calls, optimize() will return
         * false on the second call.
         */
        virtual void optimize() override;

        /// Resets optimization, discarding any reservations previously calculated in optimize().
        virtual void reset() override;

        /// Applies spending calculated and reserved in optimize().
        virtual void apply() override;

        /** The relative tolerance level at which optimization stops. */
        double tolerance;

    protected:
        const eris_id_t con_id;
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
            /// A set of constrained markets (i.e. where we can't increase quantity any more)
            std::unordered_set<eris_id_t> constrained;
        };

        /** Calculates the Bundle that the given spending allocation will buy.  A market id of 0 is
         * interpreted as a pseudomarket for holding onto cash, i.e. the "spending" is just held as
         * cash.
         */
        allocation spending_allocation(const std::unordered_map<eris_id_t, double> &spending) const;

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
                const Bundle &b) const;

        /// Returns the ratio between the market's output price and the optimizer's money unit.
        /// Results are cached for performance.
        double price_ratio(const SharedMember<Market> &m) const;

        /// Declares a dependency on the consumer when added to a simulation
        virtual void added() override;

        /// Reservations populated during optimize(), applied during apply().
        std::forward_list<Market::Reservation> reservations;

    private:
        /// Stores cached price ratios
        mutable std::unordered_map<eris_id_t, double> price_ratio_cache;

};

} }
