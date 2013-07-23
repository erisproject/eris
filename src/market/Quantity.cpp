#include <eris/market/Quantity.hpp>
#include <eris/intraopt/QMPricer.hpp>
#include <eris/Simulation.hpp>
#include <unordered_map>

namespace eris { namespace market {

Quantity::Quantity(Bundle output_unit, Bundle price_unit, double initial_price, int qmpricer_rounds) :
    Market(output_unit, price_unit), qmpricer_rounds_(qmpricer_rounds) {
    price_ = initial_price <= 0 ? 1 : initial_price;
}

Market::price_info Quantity::price(double q) const {
    double available = firmQuantities(q);
    if (q > available or (q == 0 and available <= 0)) return { .feasible=false };
    else return { .feasible=true, .total=q*price_, .marginal=price_, .marginalFirst=price_ };
}

double Quantity::price() const {
    return price_;
}

double Quantity::firmQuantities(double max) const {
    double q = 0;

    for (auto f : suppliers_) {
        q += simulation()->agent<firm::QFirm>(f)->assets().multiples(output_unit);
        if (q >= max) return q;
    }
    return q;
}

Market::quantity_info Quantity::quantity(double p) const {
    double q = p / price_;
    double available = firmQuantities(q);
    bool constrained = q > available;
    if (constrained) q = available;
    double spent = constrained ? price_*q : p;
    return { .constrained=constrained, .quantity=q, .spent=spent, .unspent=p-spent };
}

Market::Reservation Quantity::reserve(double q, Bundle *assets, double p_max) {
    double available = firmQuantities(q);
    if (q > available)
        throw output_infeasible();
    if (q * price_ > p_max)
        throw low_price();
    Bundle payment = q * price_ * price_unit;
    if (not(*assets >= payment))
        throw insufficient_assets();

    // Attempt to divide the purchase across all firms.  This might take more than one round,
    // however, if an equal share would exhaust one or more firms' assets.
    std::unordered_set<eris_id_t> qfirm;

    Reservation res = createReservation(q, q*price_, assets);

    std::unordered_map<eris_id_t, BundleNegative> firm_transfers;

    while (q > 0) {
        qfirm.clear();
        double qmin = 0; // Will store the maximum quantity that all firms can supply
        for (auto f : suppliers_) {
            double qi = simulation()->agent<firm::QFirm>(f)->assets().multiples(output_unit);
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

void Quantity::addFirm(SharedMember<Firm> f) {
    requireInstanceOf<firm::QFirm>(f, "Firm passed to Quantity.addFirm(...) is not a QFirm instance");
    Market::addFirm(f);
}

void Quantity::setPrice(double p) {
    price_ = p;
}

void Quantity::added() {
    if (qmpricer_rounds_ > 0)
        optimizer = simulation()->createIntraOpt<eris::intraopt::QMPricer>(*this);
}

} }
