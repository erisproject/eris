#include <eris/optimizer/SimpleBuyer.hpp>
#include <eris/algorithms.hpp>
#include <cmath>
#include <limits>
#include <set>
#include <unordered_map>

namespace eris { namespace optimizer {

SimpleBuyer::SimpleBuyer(const Consumer &consumer, eris_id_t money, int spending_chunks) :
    Optimizer(consumer), money(money), spending_chunks(spending_chunks) {}

void SimpleBuyer::reset() {
    increment = assets()[money] / (double) spending_chunks;
}

void SimpleBuyer::permuteThreshold(const double &thresh) noexcept {
    threshold = (isnan(thresh) or thresh > 1.0) ? 1.0 : thresh;
}

void SimpleBuyer::permuteAll() noexcept {
    permuteThreshold(-std::numeric_limits<double>::infinity());
}

void SimpleBuyer::permuteZeros(const bool &pz) noexcept {
    permute_zeros = pz;
}

/** \todo need to worry about locking the markets until we decide which one to buy from.
 */
bool SimpleBuyer::optimize() {
    auto sim = simulation();
    SharedMember<Consumer> consumer = sim->agent(agent_id);

    const Bundle money_unit {{ money, 1 }};

    BundleNegative &a = assets();

    double cash = a[money];
    if (cash <= 0) {
        // All out of money
        return false;
    }

    // The amount of money to spend for this increment:
    Bundle spending = (cash < increment ? cash : increment) * money_unit;
    BundleNegative remaining = a - spending;

    // Stores the utility changes for each market
    std::map<eris_id_t, double> delta_u;

    double current_utility = consumer->utility(a);

    // The base case: don't spend anything (0 is special for "don't spend")
    std::vector<eris_id_t> best {0};
    double best_delta_u = 0;

    // market->quantity() can be a relatively expensive operation, so cache its
    // results.  This stores the market m quantity for price spending/n in
    // q_cache[m][n]
    std::unordered_map<eris_id_t, std::unordered_map<int, double>> q_cache;

    for (auto mkt : sim->markets()) {
        auto mkt_id = mkt.first;
        auto market = mkt.second;

        Bundle priceUnit = market->priceUnit();
        if (not(priceUnit.covers(money_unit) and money_unit.covers(priceUnit))) {
            // priceUnit is not (or not just) money; we can't handle that, so ignore this market
            continue;
        }

        Bundle output = market->output();
        if (output[money] > 0) {
            // Something screwy about this market: it costs money, but also produces money.  Ignore.
            continue;
        }

        // Figure out how much `spending' buys in this market:
        double q = market->quantity(spending / priceUnit);
        // Cache the value, as we may need it again and ->quantity can be expensive
        q_cache[mkt_id].emplace(1, q);

        double mkt_delta_u = consumer->utility(remaining + q*market->output()) - current_utility;
        delta_u[market] = mkt_delta_u;
        if (mkt_delta_u > best_delta_u) {
            best[0] = market;
            best_delta_u = mkt_delta_u;
        }
    }

    // Now figure out which, if any, permutations we also need to consider
    std::set<eris_id_t> permute;
    for (auto du : delta_u) {
        eris_id_t market_id = du.first;
        double delta_u = du.second;

        if (threshold == -std::numeric_limits<double>::infinity() // Permute all
                or (delta_u == 0 and permute_zeros) // Permute 0's explicitly
                or (delta_u >= threshold*best_delta_u) // it exceeds the threshold (or both are 0, for implicit 0 permutations)
           ) {
            permute.insert(market_id);
        }
    }

    // From everything added into permute, above, we need to build all possible multi-element
    // combinations; e.g. if permute = {1,2,3} we have 4 possibilities: {1,2}, {1,3}, {2,3}, {1,2,3}

    eris::all_combinations(permute.cbegin(), permute.cend(),
            [&](const std::vector<eris_id_t> &combination) -> void {

        int comb_size = combination.size();

        // Ignore 0- or 1-element combinations (we already checked those above)
        if (comb_size < 2) return;

        BundleNegative comb = remaining;
        for (auto mkt_id : combination) {
            auto market = sim->market(mkt_id);
            // Get the market quantity we can afford (if we haven't already), spending an equal
            // share of the spending chunk on each good in the combination
            if (!q_cache[mkt_id].count(comb_size))
                q_cache[mkt_id].emplace(comb_size, market->quantity(spending / comb_size / market->priceUnit()));

            comb += q_cache[mkt_id][comb_size] * market->output();
        }

        double mkt_delta_u = consumer->utility(comb) - current_utility;
        if (mkt_delta_u > best_delta_u) {
            best = combination;
            best_delta_u = mkt_delta_u;
        }
    });

    // Finished: best contains the best set of market combinations, so buy it and then we're done.
    
    int comb_size = best.size();
    if (comb_size == 1 and best[0] == 0) {
        // None or the markets, nor any combination of the markets, gave any positive utility
        // change, so don't buy anything; returning false indicates that nothing changed.
        return false;
    }


    for (auto mkt_id : best) {
        auto market = sim->market(mkt_id);
        double q = q_cache[mkt_id][comb_size];
        market->buy(q, a);
    }
    return true;
}

}}
