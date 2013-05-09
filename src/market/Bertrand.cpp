#include "market/Bertrand.hpp"

#include <iostream>

namespace eris { namespace market {

Bertrand::Bertrand(Bundle output, Bundle priceUnit, bool randomize)
    : Market(output, priceUnit), randomize(randomize) {}

Market::price_info Bertrand::price(double q) const {
    return allocate(q).p;
}

Bertrand::allocation Bertrand::allocate(double q) const {
    // Keys are prices, values are fraction of q*_output available at that price
    std::map<double, double> priceAggQ;
    // Keys are prices, values are maps of <firm -> fraction of q*_output available>
    std::map<double, std::vector<std::pair<eris_id_t, double>>> priceFirm;

    double aggQuantity = 0.0;
    Bundle qBundle = q * _output;

    for (auto f : suppliers) {
        SharedMember<firm::PriceFirm> firm = f.second;
        // Make sure the "price" object in this market can pay for the units the firm wants
        if (_price.covers(firm->price())) {
            double productivity = firm->canSupplyAny(qBundle);
            if (productivity > 0) {
                aggQuantity += productivity;
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
                double firmPrice = (_output / firm->output()) * (firm->price() / _price);
                priceAggQ[firmPrice] += productivity;
                priceFirm[firmPrice].push_back(std::pair<eris_id_t,double>(firm, productivity));
            }
        }
    }

    allocation a = { .p={ .feasible=false } };

    // aggQuantity is in terms of the requested output bundle; if it doesn't add up to at least
    // 1, the entire market cannot supply the requested quantity at any price.
    if (aggQuantity < 1.0) { return a; }

    a.p.feasible = true;

    double needQ = 1.0;
    bool firstPrice = true;
    for (auto pf : priceFirm) {
        double price = pf.first;
        if (firstPrice) {
            a.p.marginalFirst = price;
            firstPrice = false;
        }
        double aggQ = priceAggQ[price];
        a.p.total += price * (aggQ <= needQ ? aggQ : needQ);

        auto firms = pf.second;

        if (aggQ <= needQ) {
            // The aggregate quantity at this price does not exceed the needed aggregate quantity,
            // so allocation is easy: every firm supplies their full reported productivity value.
            for (auto firmprod : firms) {
                a.shares[firmprod.first].p = price;
                a.shares[firmprod.first].q += firmprod.second;
            }
            needQ -= aggQ;
        }
        else if (firms.size() == 1) {
            // There is excess capacity, but all from one firm, so allocation is easy again.
            a.shares[firms[0].first] = { .p=price, .q=needQ };
            needQ = 0;
        }
        else {
            // Otherwise life is more complicated: there is excess capacity, so we need to worry
            // about allocation rules among multiple firms.
            while (needQ > 0) {
                int nFirms = firms.size();
                if (nFirms == 1) {
                    // Only one firm left, use it for everything remaining (we're guaranteed,
                    // by the above if (aggQ ..), that there is enough quantity available).
                    a.shares[firms[0].first].p = price;
                    a.shares[firms[0].first].q += needQ;
                    needQ = 0;
                }
                else if (randomize) {
                    // We're going to randomize among the available firms.  There's a wrinkle here,
                    // though: if the firm we randomly select doesn't have enough capacity, we use up
                    // its capacity and them randomly select from the remaining firms until we have the
                    // needed capacity.
                    std::uniform_int_distribution<unsigned int> randFirmDist(0, nFirms-1);
                    int luckyFirm = randFirmDist(rng());
                    auto f = firms[luckyFirm];
                    if (f.second >= needQ) {
                        a.shares[f.first].p = price;
                        a.shares[f.first].q += needQ;
                        needQ = 0;
                    }
                    else {
                        a.shares[f.first].p = price;
                        a.shares[f.first].q += f.second;
                        needQ -= f.second;
                        firms.erase(firms.begin() + luckyFirm);
                    }
                }
                else {
                    // Split the desired capacity evenly among firms.  There's a complication here,
                    // too: there may be a firm whose capacity is insufficient.  For example,
                    // suppose:
                    //
                    // needQ = 1.0, capacities: firm1 = 1, firm2 = 0.5, firm3 = 0.2, firm4 = 0.1
                    //
                    // An even split would assign 0.25 to each, but that exceeds 3 and 4's
                    // capacities; so we need to assign 0.1 to each of them, then reconsider
                    // (leaving off firm4):
                    //
                    // needQ = 0.6, capacities: firm1 = 0.9, firm2 = 0.4, firm3 = 0.1
                    //
                    // Now 3 has a binding constraint, so again assign 0.1 to each firm, to get:
                    //
                    // needQ = 0.3, capacities: firm1 = 0.8, firm2 = 0.3
                    //
                    // Now the even split, 0.15 each, isn't a problem, so assign it and we're done
                    // with final firm quantities of:
                    //
                    // firm1 = 0.35, firm2 = 0.35, firm3 = 0.2, firm4 = 0.1

                    double evenSplit = needQ / nFirms;
                    double qEach = evenSplit;

                    // Find the maximum quantity all firms can handle:
                    for (auto f : firms) if (f.second < qEach) qEach = f.second;

                    if (qEach == evenSplit) {
                        // No binding constraints, easy:
                        for (auto f : firms) {
                            a.shares[f.first].p = price;
                            a.shares[f.first].q += qEach;
                        }
                        needQ = 0.0;
                    }
                    else {
                        // At least one firm had a (strictly) binding constraint, so we need to
                        // assign the max capacity to each firm and remove any firms whose
                        // constraints bind
                        needQ -= nFirms * qEach;

                        for (auto f = firms.begin(); f != firms.end(); ) {
                            a.shares[f->first].p = price;
                            a.shares[f->first].q += qEach;
                            f->second -= qEach;
                            if (f->second <= 0)
                                f = firms.erase(f);
                            else
                                ++f;
                        }
                    }
                }
            }
        }

        if (needQ <= 0) {
            a.p.marginal = price;
            break;
        }
    }

    return a;
}

void Bertrand::buy(double q, double pMax, Bundle &assets) {
    // FIXME: need to lock the economy until this transaction completes; otherwise supply() could
    // fail.
    allocation a = allocate(q);
    if (!a.p.feasible) throw output_infeasible();

    if (a.p.total > pMax) throw low_price();

    Bundle cost = a.p.total*_price;
    if (!(assets >= cost)) throw insufficient_assets();

    assets -= cost;
    assets += q*_output;
}

void Bertrand::addFirm(SharedMember<Firm> f) {
    if (!dynamic_cast<firm::PriceFirm*>(f.ptr.get()))
        throw std::invalid_argument("Firm passed to Bertrand.addFirm(...) is not a PriceFirm instance");
    Market::addFirm(f);
}

} }

