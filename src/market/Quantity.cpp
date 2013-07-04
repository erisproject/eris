#include <eris/market/Quantity.hpp>
#include <eris/interopt/QMStepper.hpp>
#include <eris/Simulation.hpp>

namespace eris { namespace market {

Quantity::Quantity(Bundle output_unit, Bundle price_unit, bool add_qmstepper) :
    Market(output_unit, price_unit), add_qmstepper_(add_qmstepper) {}

Market::price_info Quantity::price(double q) const {
    if (q > quantity_ or (q == 0 and quantity_ <= 0)) return { .feasible=false };
    else return { .feasible=true, .total=q*price_, .marginal=price_, .marginalFirst=price_ };
}

double Quantity::quantity(double p) const {
    double q = p / price_;
    if (q > quantity_) q = quantity_;
    return q;
}

void Quantity::buy(double q, Bundle &assets, double p_max) {
    if (q > quantity_)
        throw output_infeasible();
    if (q * price_ > p_max)
        throw low_price();
    Bundle payment = q * price_ * price_unit;
    if (not(assets >= payment))
        throw insufficient_assets();

    assets -= payment;
    assets += q * output_unit;
    quantity_ -= q;
    // FIXME: send payment to firms
}

double Quantity::buy(Bundle &assets) {
    if (quantity_ <= 0)
        throw output_infeasible();

    double q = assets.positive() / price_unit;
    if (q > quantity_) q = quantity_;

    Bundle payment = q * price_ * price_unit;
    assets -= payment;
    assets += q * output_unit;
    quantity_ -= q;
    // FIXME: send payment to firms

    return q;
}

void Quantity::added() {
    if (add_qmstepper_)
        optimizer = simulation()->createInterOpt<eris::interopt::QMStepper>(*this);
}

} }
