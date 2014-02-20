#include <eris/intraopt/MUPD.hpp>
#include <unordered_map>

using std::unordered_map;

namespace eris { namespace intraopt {

MUPD::MUPD(const Consumer::Differentiable &consumer, const eris_id_t &money, double tolerance) :
    tolerance(tolerance), con_id(consumer), money(money), money_unit(Bundle {{ money, 1 }})
    {}

double MUPD::price_ratio(const SharedMember<Market> &m) const {
    if (!price_ratio_cache.count(m))
        price_ratio_cache[m] = money_unit.coverage(m->price_unit);

    return price_ratio_cache[m];
}

MUPD::allocation MUPD::spending_allocation(const unordered_map<eris_id_t, double> &spending) const {
    allocation a = {};

    auto sim = simulation();

    for (auto &m : spending) {
        if (m.second > 0) {
            if (m.first == 0) {
                // Holding cash
                a.bundle += money_unit * m.second;
                a.quantity[m.first] += m.second;
            }
            else {
                // Otherwise query the market for the resulting quantity
                auto mkt = sim->market(m.first);

                auto q = mkt->quantity(m.second * price_ratio(mkt));

                a.quantity[m.first] = q.quantity;
                a.bundle += mkt->output_unit * q.quantity;

                if (q.constrained) {
                    // The market is constrained, so add any leftover (unspent) money back into the
                    // bundle
                    a.constrained.insert(mkt);
                    a.bundle += mkt->price_unit * q.unspent;
                    a.quantity[0] += q.unspent / price_ratio(mkt);
                }
            }
        }
    }

    return a;
} 

double MUPD::calc_mu_per_d(
        const SharedMember<Consumer::Differentiable> &con,
        Member::Lock &lock,
        const eris_id_t &mkt_id,
        const allocation &alloc,
        const Bundle &b) const {

    if (mkt_id == 0)
        return con->d(b, money);

    auto sim = simulation();
    auto mkt = sim->market(mkt_id);
    lock.add(mkt);

    double mu = 0.0;
    // Add together all of the marginal utilities weighted by the output level, since the market may
    // produce more than one good, and quantities may not equal 1.
    for (auto g : mkt->output_unit)
        mu += g.second * con->d(b, g.first);

    double q = alloc.quantity.count(mkt) ? alloc.quantity.at(mkt) : 0;
    auto pricing = mkt->price(q);

    lock.remove(mkt);

    if (!pricing.feasible) {
        throw market_exhausted_error(mkt_id);
    }

    return mu / pricing.marginal * price_ratio(mkt);
}

void MUPD::intraOptimize() {

    auto sim = simulation();
    auto consumer = sim->agent<Consumer::Differentiable>(con_id);


    // Before bothering with anything else, make sure the consumer actually has some money to spend
    {
        auto lock = consumer->readLock();
        if (consumer->assets()[money] <= 0)
            return;
    }

    unordered_map<eris_id_t, double> spending;

    spending[0] = 0.0; // 0 is the "don't spend"/"hold cash" option

    for (auto &market : sim->markets()) {
        auto mlock = market->readLock();

        if (not(market->price_unit.covers(money_unit) and money_unit.covers(market->price_unit))) {
            // price_unit is not (or not just) money; we can't handle that, so ignore this market
            continue;
        }

        if (market->output_unit[money] > 0) {
            // Something screwy about this market: it costs money, but also produces money.  Ignore.
            continue;
        }

        if (not market->price(0).feasible) {
            // The market cannot produce any output (i.e. it is exhausted/constrained), so don't
            // consider it.
            continue;
        }

        // We assign an exact value later, once we know how many eligible markets there are.
        spending[market] = 0.0;
    }

    unsigned int markets = spending.size()-1; // -1 to account for the cash non-market (id=0)

    DEBUG(markets << " markets being considered in MUPD");

    // If there are no viable markets, there's nothing to do.
    if (markets == 0) return;

    // Now hold a write lock on this optimizer and the consumer.  We'll add and remove market locks to this as needed.
    auto big_lock = writeLock(consumer);

    markets = spending.size()-1; // -1 for the id=0 cash pseudo-market
    if (markets == 0) return;

    Bundle &a = consumer->assets();
    Bundle a_no_money = a;
    double cash = a_no_money.remove(money);
    if (cash <= 0) {
        // No money (there was before, so something external changed), nothing to do.
        return;
    }

    // Start out with equal spending in every market, no spending in the 0 (don't spend)
    // pseudo-market
    for (auto &m : spending) {
        if (m.first != 0)
            m.second = cash / markets;
    }

    unordered_map <eris_id_t, double> mu_per_d;

    allocation final_alloc = {};

    while (true) {
        while (true) {
            try {
                allocation alloc = spending_allocation(spending);
                Bundle tryout = a_no_money + alloc.bundle;

                for (auto m : spending) {
                    mu_per_d[m.first] = calc_mu_per_d(consumer, big_lock, m.first, alloc, tryout);
                }

                eris_id_t highest = 0, lowest = 0;
                double highest_u = mu_per_d[0], lowest_u = std::numeric_limits<double>::infinity();
                for (auto m : mu_per_d) {
                    // Consider all markets (even eligible ones that we aren't currently spending in) for
                    // best return, but exclude markets that are constrained (since we can't spend any more
                    // in them).
                    // FIXME: do this last bit?
                    if (m.second > highest_u) {
                        highest = m.first;
                        highest_u = m.second;
                    }
                    // Only count markets where we are actually spending positive amounts as "lowest", since
                    // we can't transfer away from a market without any expenditure.
                    if (spending[m.first] > 0 and m.second < lowest_u) {
                        lowest = m.first;
                        lowest_u = m.second;
                    }
                }

                if (highest_u <= lowest_u or (highest_u - lowest_u) / highest_u < tolerance) {
                    final_alloc = alloc;
                    break; // Nothing more to optimize
                }

                double baseU = consumer->utility(tryout);
                // Attempt to transfer all of the low utility spending to the high-utility market.  If MU/$
                // are equal, we're done; if the lower utility is still lower, transfer 3/4, otherwise
                // transfer 1/4.  Repeat.
                //
                // We do have to be careful, however: transferring everything might screw things up (e.g.
                // consider u = xyz^2: setting z=0 will result in MU=0 for all three goods.  So we need to
                // check not just the marginal utilities, but that this reallocation actually increases
                // overall utility.
                unordered_map<eris_id_t, double> try_spending = spending;

                try_spending[highest] = spending[highest] + spending[lowest];
                try_spending[lowest] = 0;

                alloc = spending_allocation(try_spending);
                tryout = a_no_money + alloc.bundle;
                if (consumer->utility(tryout) < baseU or
                        calc_mu_per_d(consumer, big_lock, highest, alloc, tryout) < calc_mu_per_d(consumer, big_lock, lowest, alloc, tryout)) {
                    // Transferring *everything* from lowest to highest is too much (MU/$ for the highest
                    // good would end up lower than the lowest good, post-transfer, or else overall utility
                    // goes down entirely).
                    //
                    // We need to transfer less than everything, so use a binary search to figure out the
                    // optimum transfer.
                    //
                    // Take 10 binary steps (which means we get granularity of 1/1024).  However, since
                    // we'll probably come in here again (comparing this good to other goods) before
                    // optimize() finishes, this gets amplified.
                    //
                    // FIXME: this is a very good target for optimization; typically this loop will run
                    // around 53 times (which makes perfect sense, as that's about where step_size runs off
                    // the end of the least precise double bit--sometimes a bit more, if the transfer ratio
                    // is a very small number).
                    double step_size = 0.25;
                    double last_transfer = 1.0;
                    double transfer = 0.5;

                    for (int i = 0; transfer != last_transfer and i < 100; ++i) {
                        last_transfer = transfer;

                        double pre_try_h = try_spending[highest], pre_try_l = try_spending[lowest];

                        try_spending[highest] = spending[highest] + transfer * spending[lowest];
                        try_spending[lowest] = (1-transfer) * spending[lowest];

                        if (try_spending[highest] == pre_try_h and try_spending[lowest] == pre_try_l) {
                            // The transfer is too small to numerically affect things, so we're done.
                            break;
                        }

                        alloc = spending_allocation(try_spending);
                        tryout = a_no_money + alloc.bundle;
                        double delta = calc_mu_per_d(consumer, big_lock, highest, alloc, tryout) - calc_mu_per_d(consumer, big_lock, lowest, alloc, tryout);
                        if (delta == 0)
                            // We equalized MU/$.  Done.
                            break;
                        else if (delta > 0)
                            // MU/$ is still higher for `highest', so transfer more
                            transfer += step_size;
                        else
                            // Otherwise transfer less
                            transfer -= step_size;
                        // Eventually this step_size will become too small to change transfer
                        step_size /= 2;
                    }
                }

                final_alloc = alloc;

                if (spending[highest] == try_spending[highest] or spending[lowest] == try_spending[lowest]) {
                    // What we just identified isn't actually a change, probably because we're hitting the
                    // boundaries of storable double values, so end.
                    break;
                }

                spending[highest] = try_spending[highest];
                spending[lowest] = try_spending[lowest];
            }
            catch (market_exhausted_error &e) {
                // One of the markets has become exhausted.  If it's completely exhausted, take it out
                // of the spending set; otherwise just restart the whole thing (the new limit will be
                // taken care of in the initial spending_allocation() call).

                if (not sim->market(e.market)->price(0).feasible) {
                    // Completely exhausted market: transfer its spending to cash
                    spending[0] += spending[e.market];
                    spending.erase(e.market);
                    markets = spending.size() - 1;
                    if (markets == 0) return;
                }
            }
        }

        // Safety check: make sure we're actually increasing utility; if not, don't do anything.
        if (consumer->utility(a_no_money + final_alloc.bundle) <= consumer->currUtility()) {
            return;
        }

        // If we haven't held back on any spending, add a tiny fraction of the amount of cash we are
        // spending to assets (to prevent numerical errors causing insufficient assets exceptions), then
        // subtract it off (if possible) after reserving.
        double extra = 0;
        if (spending[0] == 0.0) {
            extra = cash * 1e-13;
            a += extra * money_unit;
        }

        bool restart = false; // Will become true if a reservation fails

        for (auto &m : final_alloc.quantity) {
            if (m.first != 0 and m.second > 0) {
                auto market = sim->market(m.first);
                big_lock.add(market);
                try {
                    reservations.push_front(market->reserve(consumer, m.second));
                }
                catch (Market::output_infeasible &e) {
                    restart = true;
                }
                catch (Market::insufficient_assets &e) {
                    restart = true;
                }
                big_lock.remove(market);
                if (restart) break;
            }
        }

        if (extra > 0) {
            a -= extra * money_unit;
        }

        if (not restart) break;
        // Else abort any established reservations and repeat the entire loop
        reservations.clear();
    }
}

void MUPD::intraReset() {
    auto lock = writeLock(simulation()->agent<Consumer::Differentiable>(con_id));

    reservations.clear();
}

void MUPD::intraApply() {
    auto con = simAgent<Consumer::Differentiable>(con_id);
    auto lock = writeLock(con);
    Bundle &a = con->assets();
    // Add a tiny bit to cash (to prevent numerical errors causing insufficient assets exceptions),
    // then subtract it off after purchasing.
    const Bundle tiny_extra = 1e-12 * a[money] * money_unit;
    a += tiny_extra;
    for (auto &res: reservations) {
        res->buy();
    }

    if (a[money] < 2*tiny_extra[money])
        // If leftover money isn't at least "2 epsilons" above 0, assume it's a numerical error and
        // reset it to zero.
        a.set(money, 0);
    else
        // Otherwise subtract off the tiny amount we added, above.
        a -= tiny_extra;

    reservations.clear();
}

void MUPD::added() {
    dependsOn(con_id);
    dependsOn(money);
}

} }
