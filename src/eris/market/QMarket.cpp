#include <eris/market/QMarket.hpp>
#include <eris/intraopt/WalrasianPricer.hpp>
#include <eris/Simulation.hpp>
#include <unordered_map>

namespace eris { namespace market {

QMarket::QMarket(Bundle output_unit, Bundle price_unit, double initial_price, int qmpricer_tries) :
    Market(output_unit, price_unit), qmpricer_tries_(qmpricer_tries) {
    price_ = initial_price <= 0 ? 1 : initial_price;
}

Market::price_info QMarket::price(double q) const {
    double available = firmQuantities(q);
    if (q > available or (q == 0 and available <= 0)) return { .feasible=false };
    else return { .feasible=true, .total=q*price_, .marginal=price_, .marginalFirst=price_ };
}

double QMarket::price() const {
    return price_;
}

double QMarket::firmQuantities(double max) const {
    double q = 0;

    for (auto f : suppliers_) {
        auto firm = simAgent<firm::QFirm>(f);
        auto lock = firm->readLock();
        q += firm->assets().multiples(output_unit);
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
    return { .quantity=q, .constrained=constrained, .spent=spent, .unspent=p-spent };
}

Market::Reservation QMarket::reserve(SharedMember<Agent> agent, double q, double p_max) {
    std::vector<SharedMember<firm::QFirm>> supply;
    for (auto &sid : suppliers_) {
        supply.push_back(simAgent<firm::QFirm>(sid));
    }
    agent->writeLock(supply);

    double available = firmQuantities(q);
    if (q > available)
        throw output_infeasible();
    if (q * price_ > p_max)
        throw low_price();
    Bundle payment = q * price_ * price_unit;
    if (not(agent->assets() >= payment))
        throw insufficient_assets();

    // Attempt to divide the purchase across all firms.  This might take more than one round,
    // however, if an equal share would exhaust one or more firms' assets.
    std::unordered_set<eris_id_t> qfirm;

    Reservation res = createReservation(agent, q, q*price_);

    std::unordered_map<eris_id_t, BundleNegative> firm_transfers;

    while (q > 0) {
        qfirm.clear();
        double qmin = 0; // Will store the maximum quantity that all firms can supply
        for (auto f : suppliers_) {
            double qi = simAgent<firm::QFirm>(f)->assets().multiples(output_unit);
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
        res->firmReserve(ft.first, ft.second);

    return res;
}

void QMarket::addFirm(SharedMember<Firm> f) {
    requireInstanceOf<firm::QFirm>(f, "Firm passed to QMarket.addFirm(...) is not a QFirm instance");
    Market::addFirm(f);
}

void QMarket::setPrice(double p) {
    price_ = p;
}

void QMarket::added() {
    if (qmpricer_tries_ > 0)
        optimizer = simulation()->createIntraOpt<eris::intraopt::WalrasianPricer>(*this, qmpricer_tries_);
}

} }
