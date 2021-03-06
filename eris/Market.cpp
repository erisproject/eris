#include <eris/Market.hpp>
#include <vector>
#include <sstream>

namespace eris {

Market::Market(Bundle output_unit, Bundle price_unit) : output_unit(output_unit), price_unit(price_unit) {}

void Market::addFirm(SharedMember<Firm> f) {
    auto lock = writeLock();
    suppliers_.insert(f->id());
    dependsWeaklyOn(f);
}

void Market::removeFirm(id_t fid) {
    auto lock = writeLock();
    suppliers_.erase(fid);
}

const std::unordered_set<id_t>& Market::firms() {
    return suppliers_;
}

void Market::weakDepRemoved(SharedMember<Member> firm) {
    removeFirm(firm->id());
}

void Market::buy(Reservation &res) {
    if (res.state != ReservationState::pending)
        throw Reservation::non_pending_exception();

    // Lock this market, the agent, and all the firm's involved in the reservation:
    std::vector<SharedMember<Member>> to_lock;
    to_lock.push_back(res.agent);
    for (auto &firm_res: res.firm_reservations_) to_lock.push_back(firm_res.firm);
    auto lock = writeLock(to_lock);

    res.state = ReservationState::complete;

    // Currently b_ contains the total payment; do firm transfers
    for (auto &firm_res: res.firm_reservations_)
        firm_res.transfer(res.b_);

    // Now b_ has had the payment removed, and the output added, so send it back to the agent
    res.agent->assets += res.b_;
    res.b_.clear();
}

void Market::release(Reservation &res) {
    if (res.state != ReservationState::pending)
        throw Reservation::non_pending_exception();

    // Lock this market, the agent, and all the firm's involved in the reservation:
    std::vector<SharedMember<Member>> to_lock;
    to_lock.push_back(res.agent);
    for (auto &firm_res: res.firm_reservations_) to_lock.push_back(firm_res.firm);
    auto lock = writeLock(to_lock);

    res.state = ReservationState::aborted;

    for (auto &firm_res: res.firm_reservations_)
        firm_res.release();

    // Refund that payment that was extracted when the reservation was made
    res.agent->assets += res.b_;
    res.b_.clear();
}

Market::Reservation::Reservation(SharedMember<Market> mkt, SharedMember<Agent> agt, double qty, double pr)
    : state(ReservationState::pending), quantity(qty), price(pr), market(mkt), agent(agt) {
    auto lock = agent->writeLock(market);

    Bundle payment = price * market->price_unit;
    agent->assets -= payment;
    b_ += payment;
}

Market::Reservation::~Reservation() {
    if (market and state == ReservationState::pending)
        release();
}

void Market::Reservation::firmReserve(id_t firm_id, BundleNegative transfer) {
    auto firm = market->simAgent<Firm>(firm_id);
    firm_reservations_.push_front(firm->reserve(transfer));
}

void Market::Reservation::buy() {
    market->buy(*this);
}

void Market::Reservation::release() {
    market->release(*this);
}

Market::Reservation Market::createReservation(SharedMember<Agent> agent, double q, double p) {
    return Reservation(sharedSelf(), agent, q, p);
}

const char* Market::Reservation::non_pending_exception::what() const noexcept {
    return "Attempt to buy/release a non-pending market Reservation";
}

SharedMember<Member> Market::sharedSelf() const { return simMarket(id()); }

Bundle& Market::reservationBundle_(Reservation &res) { return res.b_; }

const char* Market::output_infeasible::what() const noexcept { return "Requested output not available"; }
const char* Market::low_price::what() const noexcept { return "Requested output not available for given price"; }
const char* Market::insufficient_assets::what() const noexcept { return "Assets insufficient for purchasing requested output"; }

Market::operator std::string() const {
    if (price_unit.empty() && output_unit.empty())
        return "Market[" + std::to_string(id()) + "]";
    std::ostringstream ss;
    ss << "Market[" << id() << ": " << price_unit << " -> " << output_unit << "]";
    return ss.str();
}

}
