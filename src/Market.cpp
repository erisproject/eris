#include <eris/Market.hpp>
#include <eris/Simulation.hpp>

namespace eris {

Market::Market(Bundle output_unit, Bundle price_unit) : output_unit(output_unit), price_unit(price_unit) {}

void Market::addFirm(SharedMember<Firm> f) {
    suppliers_.insert(f);
}

void Market::removeFirm(eris_id_t fid) {
    suppliers_.erase(fid);
}

void Market::buy(Reservation &res) {
    buy_(*res);
}
void Market::buy_(Reservation_ &res) {
    if (!res.active)
        throw Reservation_::inactive_exception();

    res.completed = true;
    res.active = false;
    // Currently b_ contains the total payment; do firm transfers
    for (auto &firm_res: res.firm_reservations_)
        firm_res->transfer(res.b_);

    // Now b_ has had the payment removed, and the output added, so send it back to the agent
    *res.assets += res.b_;
    res.b_.clear();
}

void Market::release_(Reservation_ &res) {
    if (!res.active)
        throw Reservation_::inactive_exception();

    res.completed = false;
    res.active = false;

    for (auto &firm_res: res.firm_reservations_)
        firm_res->release();

    // Refund that payment that was extracted when the reservation was made
    *res.assets += res.b_;
    res.b_.clear();
}

Market::Reservation_::Reservation_(SharedMember<Market> market, double quantity, double price, Bundle *assets)
    : assets(assets), quantity(quantity), price(price), market(market) {
    Bundle payment = price * market->price_unit;
    *assets -= payment;
    b_ += payment;
}

Market::Reservation_::~Reservation_() {
    if (!completed)
        release();
}

void Market::Reservation_::firmReserve(const eris_id_t &firm_id, BundleNegative transfer) {
    auto firm = market->simAgent<Firm>(firm_id);
    firm_reservations_.push_front(firm->reserve(transfer));
}

void Market::Reservation_::buy() {
    market->buy_(*this);
}

void Market::Reservation_::release() {
    market->release_(*this);
}

Market::Reservation Market::createReservation(double q, double p, Bundle *assets) {
    return Reservation(new Reservation_(simMarket(*this), q, p, assets));
}

}
