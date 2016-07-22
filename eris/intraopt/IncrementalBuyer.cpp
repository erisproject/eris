#include <eris/intraopt/IncrementalBuyer.hpp>
#include <eris/Consumer.hpp>
#include <eris/Market.hpp>
#include <eris/algorithms.hpp>
#include <cmath>
#include <map>
#include <limits>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

namespace eris { namespace intraopt {

IncrementalBuyer::IncrementalBuyer(const Consumer &consumer, eris_id_t money, int rounds) :
    con_id(consumer), money(money), money_unit(Bundle {{ money, 1 }}), rounds(rounds) {}

void IncrementalBuyer::permuteThreshold(const double &thresh) noexcept {
    threshold = (isnan(thresh) or thresh > 1.0) ? 1.0 : thresh;
}

void IncrementalBuyer::permuteAll() noexcept {
    permuteThreshold(-std::numeric_limits<double>::infinity());
}

void IncrementalBuyer::permuteZeros(const bool &pz) noexcept {
    permute_zeros = pz;
}

/** \todo need to worry about locking the markets until we decide which one to buy from.
 */
void IncrementalBuyer::intraOptimize() {
    round = 0;
    while (oneRound()) {}
}

void IncrementalBuyer::intraApply() {
    for (auto &res : reservations)
        res.buy();
}

void IncrementalBuyer::intraReset() {
    reservations.clear();
}

bool IncrementalBuyer::oneRound() {
    ++round;

    auto sim = simulation();
    SharedMember<Consumer> consumer = sim->agent(con_id);

    Bundle &a = consumer->assets;

    double cash = a[money];
    if (cash <= 0) {
        // All out of money
        return false;
    }

    // The amount of money to spend for this increment:
    const Bundle spending = (cash / (rounds-round+1)) * money_unit;
    Bundle remaining = a - spending;

    // Stores the utility changes for each market
    std::map<eris_id_t, double> delta_u;

    double current_utility = consumer->utility(a);

    // The base case: don't spend anything (0 is special for "don't spend")
    std::vector<eris_id_t> best {0};
    double best_delta_u = 0;

    // market->quantity() can be a relatively expensive operation, so cache its
    // results.  This stores the market m quantity for price spending/n in
    // q_cache[m][n]
    std::unordered_map<eris_id_t, std::unordered_map<int, Market::quantity_info>> q_cache;

    for (auto market : sim->markets()) {

        if (not(market->price_unit.covers(money_unit) and money_unit.covers(market->price_unit))) {
            // price_unit is not (or not just) money; we can't handle that, so ignore this market
            continue;
        }

        if (market->output_unit[money] > 0) {
            // Something screwy about this market: it costs money, but also produces money.  Ignore.
            continue;
        }

        // Figure out how much `spending' buys in this market:
        double spend = market->price_unit.multiples(spending);
        auto qinfo = market->quantity(spend);

        if (qinfo.quantity == 0) {
            // Don't consider a market that doesn't give any output (e.g. an exhausted market).
            continue;
        }

        // Cache the value, as we may need it again and ->quantity can be expensive
        q_cache[market].emplace(1, qinfo);

        Bundle cons = remaining + qinfo.quantity * market->output_unit;
        // If spending hit a constraint, we need to add the unused spending back in (as cash)
        if (qinfo.constrained) {
            cons += qinfo.unspent * market->price_unit;
        }

        double mkt_delta_u = consumer->utility(cons) - current_utility;
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

        const Bundle spend_each = spending / comb_size;

        // Ignore 0- or 1-element combinations (we already checked those above)
        if (comb_size < 2) return;

        Bundle comb = remaining;
        for (auto mkt_id : combination) {
            auto market = sim->market(mkt_id);

            // Get the market quantity we can afford (if we haven't already), spending an equal
            // share of the spending chunk on each good in the combination
            if (!q_cache[mkt_id].count(comb_size))
                q_cache[mkt_id].emplace(comb_size, market->quantity(market->price_unit.multiples(spend_each)));

            auto qinfo = q_cache[mkt_id][comb_size];

            comb += qinfo.quantity * market->output_unit;

            // Re-add any unspent income due to market constraints
            if (qinfo.constrained)
                comb += qinfo.unspent * market->price_unit;
        }

        double mkt_delta_u = consumer->utility(comb) - current_utility;
        if (mkt_delta_u > best_delta_u) {
            best = combination;
            best_delta_u = mkt_delta_u;
        }
    });

    // Finished: best contains the best set of market combinations, so reserve it and then we're done.

    int comb_size = best.size();
    if (comb_size == 1 and best[0] == 0) {
        // None or the markets, nor any combination of the markets, gave any positive utility
        // change, so don't buy anything; returning false indicates that nothing changed.  This also
        // bypasses any remaining steps, since they would just find the same thing.
        return false;
    }

    // Add a tiny extra bit of cash just to make sure we don't hit a negativity constraint when
    // reserving the quantity.  We subtract it off again after reserving (and add again during
    // apply()).
    const Bundle tiny_extra = 1e-13 * spending;
    a += tiny_extra;

    for (auto mkt_id : best) {
        auto market = sim->market(mkt_id);
        auto q = q_cache.at(mkt_id).at(comb_size);
        reservations.push_front(market->reserve(consumer, q.quantity));
    }

    if (a[money] < 2*tiny_extra[money])
        // If leftover money isn't at least "2 epsilons" above 0, assume it's a numerical error and
        // reset it to zero (thus allowing up to epsilon of error in either direction).
        a.set(money, 0);
    else
        // Otherwise subtract off the tiny amount we added, above.
        a -= tiny_extra;

    return true;
}

void IncrementalBuyer::added() {
    dependsOn(con_id);
    dependsOn(money);
}

}}
