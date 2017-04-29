#include <eris/market/QMarket.hpp>
#include <eris/firm/QFirm.hpp>
#include <eris/algorithms.hpp>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>


namespace eris { namespace market {

QMarket::QMarket(Bundle output_unit, Bundle price_unit, double initial_price, unsigned int pricing_tries, unsigned int pricing_tries_first) :
    Market(output_unit, price_unit), tries_(pricing_tries), tries_first_(pricing_tries_first) {
    price_ = initial_price <= 0 ? 1 : initial_price;
}

Market::price_info QMarket::price(double q) const {
    double available = firmQuantities(q);
    if (q > available or (q == 0 and available <= 0)) return price_info();
    else return price_info(q*price_, price_, price_);
}

double QMarket::price() const {
    return price_;
}

double QMarket::firmQuantities(double max) const {
    double q = 0;

    for (auto f : suppliers_) {
        auto firm = simAgent<firm::QFirm>(f);
        auto lock = firm->readLock();
        q += firm->assets.multiples(output_unit);
        if (q >= max) return q;
    }
    return q;
}

Market::quantity_info QMarket::quantity(double p) const {
    double q = p / price_;
    double available = firmQuantities(q);
    bool constrained = q > available;
    if (constrained) q = available;
    double spent = constrained ? price_*q : p;
    return { q, constrained, spent, p-spent };
}

Market::Reservation QMarket::reserve(SharedMember<Agent> agent, double q, double p_max) {
    std::vector<SharedMember<firm::QFirm>> supply;
    for (auto &sid : suppliers_) {
        supply.push_back(simAgent<firm::QFirm>(sid));
    }
    auto lock = agent->writeLock(supply);

    double available = firmQuantities(q);
    if (q > available)
        throw output_infeasible();
    if (q * price_ > p_max)
        throw low_price();
    Bundle payment = q * price_ * price_unit;
    if (not(agent->assets >= payment))
        throw insufficient_assets();

    // Attempt to divide the purchase across all firms.  This might take more than one round,
    // however, if an equal share would exhaust one or more firms' assets.
    std::unordered_set<eris_id_t> qfirm;

    Reservation res = createReservation(agent, q, q*price_);

    std::unordered_map<eris_id_t, BundleNegative> firm_transfers;

    const double threshold = q * std::numeric_limits<double>::epsilon();

    while (q > threshold) {
        qfirm.clear();
        double qmin = 0; // Will store the maximum quantity that all firms can supply
        for (auto f : suppliers_) {
            double qi = simAgent<firm::QFirm>(f)->assets.multiples(output_unit);
            if (qi > 0) {
                if (qi < qmin or qfirm.empty())
                    qmin = qi;
                qfirm.insert(f);
            }
        }

        if (qfirm.empty()) {
            // This shouldn't happen, since firmQuantities said we had enough aggregate capacity!
            throw output_infeasible();
        }

        double qeach = qmin;
        if (qeach*qfirm.size() > q) {
            // No firm is constrained, so just divide everything still to be supplied evenly
            qeach = q / qfirm.size();
        }
        // else one or more firm is constrained, so supply qmin then repeat the loop

        BundleNegative transfer = qeach * (price_ * -price_unit + output_unit);
        for (auto f : qfirm) {
            firm_transfers[f] += transfer;
            q -= qeach;
        }
    }

    for (auto &ft : firm_transfers)
        res.firmReserve(ft.first, ft.second);

    return res;
}

void QMarket::addFirm(SharedMember<Firm> f) {
    requireInstanceOf<firm::QFirm>(f, "Firm passed to QMarket.addFirm(...) is not a QFirm instance");
    Market::addFirm(f);
}

void QMarket::setPrice(double p) {
    price_ = p;
}

void QMarket::intraInitialize() {
    tried_ = 0;
}

bool QMarket::intraReoptimize() {
    // If there are no firms, there's nothing to do
    if (firms().empty()) return false;

    unsigned int max_tries = first_period_ ? tries_first_ : tries_;

    if (tried_ >= max_tries)

    // If we're all out of adjustments, don't change the price
    if (++tried_ > max_tries) return false;

    auto qlock = writeLock();
    double excess_capacity = firmQuantities();

    bool increase_price = excess_capacity <= 0;

    last_excess_ = excess_capacity;

    double new_price = stepper.step(increase_price);

    // If we're just stepping back and forth by the minimum step size, we're basically done; prefer
    // the slightly lower price (by not taking a minimum positive step).  The ending price won't be
    // lower by much: the minimum step size is very small, but assume firms would rather have no
    // excess inventory than a tiny bit of excess inventory.
    if (stepper.oscillating_min > 0 and new_price > 1) {
        return false;
    }

    if (new_price != 1) {
        setPrice(new_price * price());
        return true;
    }
    return false;
}

void QMarket::added() {
    Market::added();
    first_period_ = true;
}

void QMarket::intraFinish() {
    if (not firms().empty())
        first_period_ = false;
}


} }
