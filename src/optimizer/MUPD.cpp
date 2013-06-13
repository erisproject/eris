#include <eris/optimizer/MUPD.hpp>
#include <unordered_map>

#include <iostream>

#define PRINT if (false) std::cout

using std::unordered_map;

namespace eris { namespace optimizer {

MUPD::MUPD(const Consumer::Differentiable &consumer, const eris_id_t &money, double tolerance) :
    Optimizer(consumer), tolerance(tolerance), money(money), money_unit(Bundle {{ money, 1 }})
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
        PRINT << "m.f=" << m.first << ", .s=" << m.second << "\n";
        if (m.first == 0) {
            // Holding cash
            a.bundle += money_unit * m.second;
            a.quantity[m.first] = m.second;
        }
        else if (m.second > 0) {
            // Otherwise query the market for the resulting quantity
            auto mkt = sim->market(m.first);

            // FIXME: feasible?
            a.quantity[m.first] = mkt->quantity(m.second * price_ratio(mkt));

            a.bundle += mkt->output() * a.quantity[m.first];
        }
    }

    return a;
} 

double MUPD::calc_mu_per_d(
        const SharedMember<Consumer::Differentiable> &con,
        const eris_id_t &mkt_id,
        const allocation &a,
        const Bundle &b) {

    PRINT << "Looking for mu/d for mkt_id=" << mkt_id << "; con->d(b, "<<money<<")=" << con->d(b,money) << "\n";
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
    SharedMember<Consumer::Differentiable> consumer = sim->agent(agent_id);

    Bundle &a = consumer->assetsB();

    double cash = a[money];
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
    PRINT << "c/m = " << cash / markets << "; spending[12]=" << spending[12] << "\n";

    unordered_map <eris_id_t, double> mu_per_d;

    allocation final_alloc = {};

    while (true) {
        allocation alloc = spending_allocation(spending);
        Bundle tryout = a + alloc.bundle;

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

        PRINT << "Highest: [" << highest << "]=" << highest_u << ", lowest: [" << lowest << "]=" << lowest_u << "\n";

        if (highest_u <= lowest_u or (highest_u - lowest_u) / highest_u < tolerance) {
            PRINT << "So ending, with bundle: " << alloc.bundle << "\n";
            final_alloc = alloc;
            break; // Nothing more to optimize
        }

        // Attempt to transfer half of the low utility spending to the high-utility market.
        // If MU/$ are equal, we're done; if the lower utility is still lower, transfer 3/4, otherwise transfer 1/4.
        // Repeat (how much?)
        unordered_map<eris_id_t, double> try_spending = spending;

        try_spending[highest] = spending[highest] + spending[lowest];
        try_spending[lowest] = 0;

        alloc = spending_allocation(try_spending);
        tryout = a + alloc.bundle;
        if (calc_mu_per_d(consumer, highest, alloc, tryout) < calc_mu_per_d(consumer, lowest, alloc, tryout)) {
            // Transferring *everything* from lowest to highest is too much (MU/$ for the highest
            // good would end up lower than the lowest good, post-transfer).
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
            double step_size = 0.25;
            double transfer = 0.5;
            for (int i = 0; i < 10; i++) {
                try_spending[highest] = spending[highest] + transfer * spending[lowest];
                try_spending[lowest] = spending[lowest] * (1-transfer) * spending[lowest];

                alloc = spending_allocation(try_spending);
                tryout = a + alloc.bundle;
                double delta = calc_mu_per_d(consumer, highest, alloc, tryout) - calc_mu_per_d(consumer, lowest, alloc, tryout);
                if (delta == 0) {
                    // We equalized MU/$.  Cool.
                    break;
                }
                else if (delta > 0) {
                    // MU/$ is still higher for `highest', so transfer more
                    transfer += step_size;
                }
                else {
                    transfer -= step_size;
                }
                step_size /= 2;
            }
        }
        spending[highest] = try_spending[highest];
        spending[lowest] = try_spending[lowest];
        final_alloc = alloc;
    }

    PRINT << "final alloc: Bundle: " << final_alloc.bundle << "\n";
    for (auto m : final_alloc.quantity) {
        PRINT << "    [" << m.first << "] = " << m.second << "\n";
    }

    bool purchased = false;
    for (auto m : final_alloc.quantity) {
        if (m.first != 0 and m.second > 0) {
            sim->market(m.first)->buy(m.second, a);
            purchased = true;
        }
    }

    return purchased;
}

} }
