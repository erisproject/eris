#include <eris/intraopt/MUPD.hpp>
#include <unordered_map>

using std::unordered_map;

namespace eris { namespace intraopt {

MUPD::MUPD(const Consumer::Differentiable &consumer, const eris_id_t &money, double tolerance) :
    tolerance(tolerance), con_id(consumer), money(money), money_unit(Bundle {{ money, 1 }})
    {}

double MUPD::price_ratio(const SharedMember<Market> &m) {
    if (!price_ratio_cache.count(m))
        price_ratio_cache[m] = money_unit / m->priceUnit();

    return price_ratio_cache[m];
}

MUPD::allocation MUPD::spending_allocation(const unordered_map<eris_id_t, double> &spending) {
    allocation a = {};

    auto sim = simulation();

    for (auto m : spending) {
        if (m.second > 0) {
            if (m.first == 0) {
                // Holding cash
                a.bundle += money_unit * m.second;
                a.quantity[m.first] = m.second;
            }
            else {
                // Otherwise query the market for the resulting quantity
                auto mkt = sim->market(m.first);

                // FIXME: feasible?
                a.quantity[m.first] = mkt->quantity(m.second * price_ratio(mkt));

                a.bundle += mkt->output() * a.quantity[m.first];
            }
        }
    }

    return a;
} 

double MUPD::calc_mu_per_d(
        const SharedMember<Consumer::Differentiable> &con,
        const eris_id_t &mkt_id,
        const allocation &a,
        const Bundle &b) {

    if (mkt_id == 0)
        return con->d(b, money);

    auto sim = simulation();
    auto mkt = sim->market(mkt_id);

    double mu = 0.0;
    // Add together all of the marginal utilities weighted by the output level, since the market may
    // produce more than one good, and quantities may not equal 1.
    for (auto g : mkt->output())
        mu += g.second * con->d(b, g.first);

    double q = a.quantity.count(mkt) ? a.quantity.at(mkt) : 0;
    // FIXME: make sure market->price(0) actually gives back a marginal price.
    auto pricing = mkt->price(q);
    // FIXME: check feasible?

    return mu / pricing.marginal * price_ratio(mkt);
}

bool MUPD::optimize() {
    
    auto sim = simulation();
    SharedMember<Consumer::Differentiable> consumer = sim->agent(con_id);

    Bundle &a = consumer->assetsB();

    Bundle a_no_money = a;
    double cash = a_no_money.remove(money);
    if (cash <= 0) {
        // All out of money
        return false;
    }


    unordered_map<eris_id_t, double> spending;

    spending[0] = 0.0; // 0 is the "don't spend"/"hold cash" option

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

        // We assign an exact value later, once we know how many eligible markets there are.
        spending[mkt_id] = 0.0;
    }

    unsigned int markets = spending.size()-1; // -1 to account for the cash non-market (id=0)

    // If there are no viable markets, there's nothing to do.
    if (markets == 0) return false;

    // Start out with equal spending in every market, no spending in the 0 (don't spend)
    // pseudo-market
    for (auto &m : spending) {
        if (m.first != 0)
            m.second = cash / markets;
    }

    unordered_map <eris_id_t, double> mu_per_d;

    allocation final_alloc = {};

    while (true) {
        allocation alloc = spending_allocation(spending);
        Bundle tryout = a_no_money + alloc.bundle;

        for (auto m : spending) {
            mu_per_d[m.first] = calc_mu_per_d(consumer, m.first, alloc, tryout);
        }

        eris_id_t highest = 0, lowest = 0;
        double highest_u = mu_per_d[0], lowest_u = std::numeric_limits<double>::infinity();
        for (auto m : mu_per_d) {
            // Consider all markets (even ones we aren't currently spending in) for best return
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
                calc_mu_per_d(consumer, highest, alloc, tryout) < calc_mu_per_d(consumer, lowest, alloc, tryout)) {
            // Transferring *everything* from lowest to highest is too much (MU/$ for the highest
            // good would end up lower than the lowest good, post-transfer, or else overall utility
            // goes down entirely).
            //
            // We need to transfer less than everything, so use a binary search to figure out the
            // optimum transfer.
            //
            // FIXME: is 10 here the best choice?  Perhaps 5 or 15 or some other number would be
            // better?  This should probably be configurable.
            //
            // Take 10 binary steps (which means we get granularity of 1/1024).  However, since
            // we'll probably come in here again (comparing this good to other goods) before
            // optimize() finishes, this gets amplified.
            //
            // FIXME: this is a very good target for optimization; typically this loop will run
            // around 53 times (which makes perfect sense, as that's when step_size runs off the end
            // of the least precise double bit--sometimes a bit more, if the transfer ratio is a
            // very small number).
            double step_size = 0.25;
            double last_transfer = 1.0;
            double transfer = 0.5;

            int debugiters = 0;
            // 0.25^-100 == 3.9e-31, so this 3e-31 corresponds to 100 iterations
            for (int i = 0; transfer != last_transfer and i < 100; ++i) {
                debugiters++;
                last_transfer = transfer;

                try_spending[highest] = spending[highest] + transfer * spending[lowest];
                try_spending[lowest] = (1-transfer) * spending[lowest];

                alloc = spending_allocation(try_spending);
                tryout = a_no_money + alloc.bundle;
                double delta = calc_mu_per_d(consumer, highest, alloc, tryout) - calc_mu_per_d(consumer, lowest, alloc, tryout);
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
        spending[highest] = try_spending[highest];
        spending[lowest] = try_spending[lowest];
        final_alloc = alloc;
    }

    // Safety check: make sure we're actually increasing utility.
    if (consumer->utility(a_no_money + final_alloc.bundle) <= consumer->currUtility()) {
        return false;
    }

    // If we haven't held back on any spending, add a tiny fraction of the amount of cash we are
    // spending to assets (to prevent numerical errors causing insufficient assets exceptions), then
    // subtract it off after purchasing.
    double extra = 0;
    if (spending[0] == 0.0) {
        extra = cash * 1e-10;
        a += extra * money_unit;
    }

    bool purchased = false;
    for (auto m : final_alloc.quantity) {
        if (m.first != 0 and m.second > 0) {
            sim->market(m.first)->buy(m.second, a);
            purchased = true;
        }
    }

    // If we added an extra bit (which means we didn't mean to hold back any money), take it off
    // now, setting money to exactly 0 if taking it off would leave us within extra (again) of 0.
    if (purchased and extra > 0) {
        if (a[money] < 2*extra)
            a.set(money, 0);
        else
            a -= extra * money_unit;
    }

    return purchased;
}

void MUPD::added() {
    dependsOn(con_id);
    dependsOn(money);
}

} }
