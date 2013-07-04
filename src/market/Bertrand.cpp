#include <eris/market/Bertrand.hpp>
#include <limits>

namespace eris { namespace market {

Bertrand::Bertrand(Bundle output_unit, Bundle price_unit, bool randomize)
    : Market(output_unit, price_unit), randomize(randomize) {}

Market::price_info Bertrand::price(double q) const {
    return allocate(q).p;
}

double Bertrand::quantity(double price) const {
    // Keys are prices, values are aggregate quantities available
    std::map<double, double> price_quantity;

    for (auto f : suppliers) {
        SharedMember<firm::PriceFirm> firm = f.second;
        double s = firm->canSupplyAny(output_unit);
        if (s > 0) {
            double firm_price = (output_unit / firm->output()) * (firm->price() / price_unit);
            price_quantity[firm_price] += s;
        }
    }

    double quantity = 0;
    for (auto pf : price_quantity) {
        double p = pf.first;
        double q = pf.second;
        if (price > p*q) {
            // Buying everything at this price level doesn't exhaust income
            quantity += q;
            price -= p*q;
        }
        else {
            // Otherwise buy as much as possible at this price level, and we're done.
            quantity += price/p;
            break;
        }
    }

    return quantity;
}

Bertrand::allocation Bertrand::allocate(double q) const {
    // Keys are prices, values are fraction of q*output_unit available at that price
    std::map<double, double> price_agg_q;
    // Keys are prices, values are maps of <firm -> fraction of q*output_unit available>
    std::map<double, std::vector<std::pair<eris_id_t, double>>> price_firm;

    double agg_quantity = 0.0;
    Bundle q_bundle = q * output_unit;

    for (auto f : suppliers) {
        SharedMember<firm::PriceFirm> firm = f.second;
        // Make sure the "price" object in this market can pay for the units the firm wants
        if (price_unit.covers(firm->price())) {
            double productivity = firm->canSupplyAny(q_bundle);
            if (productivity > 0) {
                agg_quantity += productivity;
                // First we need the market output supplied per firm output bundle unit, then we
                // multiple that by the firm's price per market price.  This is because one firm
                // could have (price=2,output=2), while another has (price=3,output=3) and another
                // has (price=1,output=1); all three have the same per-unit price.
                // Intuitively we want:
                //     (market.output/market.price) / (firm.output/firm.price)
                // but we have to actually compute it as:
                //     (market.outout/firm.output) * (firm.price / market.price)
                // because those divisions are lossy when Bundles aren't scaled versions of each
                // other (see Bundle.hpp's description of Bundle division)
                double firm_price = (output_unit / firm->output()) * (firm->price() / price_unit);
                price_agg_q[firm_price] += productivity;
                price_firm[firm_price].push_back(std::pair<eris_id_t,double>(firm, productivity));
            }
        }
    }

    // Figure out how we're going to allocate this.
    allocation a = {};

    double need_q = q;
    bool first_price = true;
    for (auto pf : price_firm) {
        double price = pf.first;
        if (first_price) {
            a.p.marginalFirst = price;
            first_price = false;
        }
        double agg_q = price_agg_q[price];
        a.p.total += price * (agg_q <= need_q ? agg_q : need_q);

        auto firms = pf.second;

        if (agg_q <= need_q) {
            // The aggregate quantity at this price does not exceed the needed aggregate quantity,
            // so allocation is easy: every firm supplies their full reported productivity value.
            for (auto firmprod : firms) {
                a.shares[firmprod.first].q += firmprod.second;
                a.shares[firmprod.first].p += price*firmprod.second;
            }
            need_q -= agg_q;
        }
        else if (firms.size() == 1) {
            // There is excess capacity, but all from one firm, so allocation is easy again.
            a.shares[firms[0].first].q += need_q;
            a.shares[firms[0].first].p += price*need_q;
            need_q = 0;
        }
        else {
            // Otherwise life is more complicated: there is excess capacity, so we need to worry
            // about allocation rules among multiple firms.
            while (need_q > 0) {
                int nFirms = firms.size();
                if (nFirms == 1) {
                    // Only one firm left, use it for everything remaining (we're guaranteed,
                    // by the above if (agg_q ..), that there is enough quantity available).
                    a.shares[firms[0].first].q += need_q;
                    a.shares[firms[0].first].p += price*need_q;
                    need_q = 0;
                }
                else if (randomize) {
                    // We're going to randomize among the available firms.  There's a wrinkle here,
                    // though: if the firm we randomly select doesn't have enough capacity, we use up
                    // its capacity and them randomly select from the remaining firms until we have the
                    // needed capacity.
                    std::uniform_int_distribution<unsigned int> randFirmDist(0, nFirms-1);
                    int luckyFirm = randFirmDist(rng());
                    auto f = firms[luckyFirm];
                    if (f.second >= need_q) {
                        a.shares[f.first].q += need_q;
                        a.shares[f.first].p += price*need_q;
                        need_q = 0;
                    }
                    else {
                        a.shares[f.first].q += f.second;
                        a.shares[f.first].p += price*f.second;
                        need_q -= f.second;
                        firms.erase(firms.begin() + luckyFirm);
                    }
                }
                else {
                    // Split the desired capacity evenly among firms.  There's a complication here,
                    // too: there may be a firm whose capacity is insufficient.  For example,
                    // suppose:
                    //
                    // need_q = 1.0, capacities: firm1 = 1, firm2 = 0.5, firm3 = 0.2, firm4 = 0.1
                    //
                    // An even split would assign 0.25 to each, but that exceeds 3 and 4's
                    // capacities; so we need to assign 0.1 to each of them, then reconsider
                    // (leaving off firm4):
                    //
                    // need_q = 0.6, capacities: firm1 = 0.9, firm2 = 0.4, firm3 = 0.1
                    //
                    // Now 3 has a binding constraint, so again assign 0.1 to each firm, to get:
                    //
                    // need_q = 0.3, capacities: firm1 = 0.8, firm2 = 0.3
                    //
                    // Now the even split, 0.15 each, isn't a problem, so assign it and we're done
                    // with final firm quantities of:
                    //
                    // firm1 = 0.35, firm2 = 0.35, firm3 = 0.2, firm4 = 0.1

                    double evenSplit = need_q / nFirms;
                    double q_each = evenSplit;

                    // Find the maximum quantity all firms can handle:
                    for (auto f : firms) if (f.second < q_each) q_each = f.second;

                    if (q_each == evenSplit) {
                        // No binding constraints, easy:
                        for (auto f : firms) {
                            a.shares[f.first].q += q_each;
                            a.shares[f.first].p += price*q_each;
                        }
                        need_q = 0.0;
                    }
                    else {
                        // At least one firm had a (strictly) binding constraint, so we need to
                        // assign the maximum common capacity to each firm and remove any firms
                        // whose constraints bind
                        need_q -= nFirms * q_each;

                        for (auto f = firms.begin(); f != firms.end(); ) {
                            a.shares[f->first].q += q_each;
                            a.shares[f->first].p += price*q_each;
                            f->second -= q_each;
                            if (f->second <= 0)
                                f = firms.erase(f);
                            else
                                ++f;
                        }
                    }
                }
            }
        }

        a.p.marginal = price;
        // If we've allocated all the needed quantity, we're done.
        if (need_q <= 0) break;
    }

    a.p.feasible = (need_q <= 0);

    return a;
}

void Bertrand::buy(double q, Bundle &assets, double p_max) {
    // FIXME: need to lock the economy until this transaction completes; otherwise supply() could
    // fail.
    allocation a = allocate(q);
    if (!a.p.feasible) throw output_infeasible();

    if (a.p.total > p_max) throw low_price();

    Bundle cost = a.p.total*price_unit;
    if (!(assets >= cost)) throw insufficient_assets();

    // Remove the customer's payment:
    assets -= cost;

    // Get the firms to produce:
    for (auto firm_share : a.shares) {
        auto firm = suppliers.at(firm_share.first);
        auto sh = firm_share.second;
        firm->supply(sh.q*output_unit);
        firm->assets() += price_unit*sh.p;
    }

    // Transfer the production to the customer
    assets += q*output_unit;
}

double Bertrand::buy(Bundle &assets) {
    Bundle assets_pos;
    Bundle *a = dynamic_cast<Bundle*>(&assets);
    if (!a) {
        a = &assets_pos;
        for (auto g : assets) {
            if (g.second > 0)
                assets_pos.set(g.first, g.second);
        }
    }

    // FIXME: need to lock the market between the quantity call and the end of the buy() call
    double p_max = *a / price_unit;
    double q = quantity(p_max);
    buy(q, assets, p_max);
    return q;
}

void Bertrand::addFirm(SharedMember<Firm> f) {
    if (!dynamic_cast<firm::PriceFirm*>(f.ptr.get()))
        throw std::invalid_argument("Firm passed to Bertrand.addFirm(...) is not a PriceFirm instance");
    Market::addFirm(f);
}

} }

