#include <eris/Firm.hpp>
#include <utility>

namespace eris {

bool Firm::canSupply(const Bundle &b) const {
    return canSupplyAny(b) >= 1.0;
}

Firm::supply_failure::supply_failure(std::string what) : std::runtime_error(what) {}

Firm::supply_mismatch::supply_mismatch(std::string what) : supply_failure(what) {}
Firm::supply_mismatch::supply_mismatch() : supply_failure("Firm does not supply requested goods") {}

Firm::production_constraint::production_constraint(std::string what) : supply_failure(what) {}
Firm::production_constraint::production_constraint()
    : production_constraint("Firm cannot supply requested bundle: capacity constraint would be violated")
{}

Firm::production_unavailable::production_unavailable(std::string what) : production_constraint(what) {}
Firm::production_unavailable::production_unavailable()
    : production_unavailable("Firm has no instantaneous production ability") {}

Firm::production_unreserved::production_unreserved(std::string what) : supply_failure(what) {}
Firm::production_unreserved::production_unreserved()
    : production_unreserved("Firm cannot produce requested bundle: production would exceed reserved production")
{}

bool Firm::canProduce(const Bundle &b) const {
    return canProduceAny(b) >= 1.0;
}

bool Firm::produces(const Bundle &b) const {
    return canProduceAny(b) > 0;
}

double Firm::canSupplyAny(const Bundle &b) const {
    // We can supply the entire thing from current assets:
    if (assets >= b) return 1.0;

    // Otherwise try production to make up the difference
    Bundle onhand = Bundle::common(assets, b);
    Bundle need = b - onhand;
    double c = canProduceAny(need);
    if (c >= 1.0) return 1.0;
    if (c <= 0.0) return onhand.multiples(b);

    // We can produce some, but not enough; figure out how much we can supply in total:
    need *= c;
    return (need + onhand).multiples(b);
}

bool Firm::supplies(const Bundle &b) const {
    Bundle check_produce;
    const Bundle &a = assets;
    // Look through everything in the requested Bundle; if our current assets don't contain any of
    // something requested, we need to check whether we can produce it.
    for (auto item : b) {
        if (item.second > 0 and a[item.first] <= 0)
            check_produce.set(item.first, 1);
    }

    if (check_produce.empty()) // Assets has positive quantities of everything requested
        return true;

    return produces(check_produce);
}

Firm::Reservation Firm::supply(const BundleNegative &b, Bundle &assets) {
    auto res = reserve(b);
    res.transfer(assets);
    return res;
}

Firm::Reservation Firm::reserve(const BundleNegative &reserve) {
    Bundle res_pos = reserve.positive();
    // First see if current assets can handle any of the requested Bundle
    Bundle common = Bundle::common(assets, res_pos);
    if (common != 0) res_pos -= common;

    if (res_pos != 0) {
        // Now see if excess production has any of what we need
        Bundle excess = Bundle::common(excess_production_, res_pos);
        if (excess != 0) res_pos -= excess;

        if (res_pos != 0) {
            // Couldn't reserve all of it with assets *or* excess production, so try to reserve new production
            reserveProduction(res_pos);
        }

        // We survived that, so we're good to go: transfer the excess production bundle
        if (excess != 0) {
            excess_production_ -= excess;
            reserved_production_ += excess;
        }
    }

    // Transfer any assets we matched above into reserves
    if (common != 0) {
        assets.transfer(common, reserves_, epsilon);
    }

    return createReservation(reserve);
}

void Firm::produceReserved(const Bundle &b) {
    reserved_production_.beginTransaction();

    Bundle to_produce;
    try {
        to_produce = reserved_production_.transfer(b, epsilon);
    }
    catch (Bundle::negativity_error& e) {
        // If the transfer throws a negativity error, we attempted to transfer more than
        // reserved_production_ actually has
        reserved_production_.abortTransaction();
        throw production_unreserved();
    }
    catch (...) {
        reserved_production_.abortTransaction();
        throw;
    }

    excess_production_.beginTransaction();
    assets.beginTransaction();

    try {
        Bundle produced = produce(to_produce);

        if (produced != to_produce) // Reduce planned excess production appropriately
            excess_production_.transfer(produced - to_produce, epsilon);

        assets += produced;
    }
    catch (...) {
        reserved_production_.abortTransaction();
        excess_production_.abortTransaction();
        assets.abortTransaction();
    }

    reserved_production_.commitTransaction();
    excess_production_.commitTransaction();
    assets.commitTransaction();
}

void Firm::Reservation::transfer(Bundle &to) {
    if (state != ReservationState::pending)
        throw Reservation::non_pending_exception();

    Bundle &assets = firm->assets;
    double &epsilon = firm->epsilon;
    to.beginTransaction();
    assets.beginTransaction();

    try {
        // Take payment:
        Bundle in = bundle.negative();
        to.transfer(bundle.negative(), assets, epsilon);

        // Now transfer and/or produce output
        Bundle out = bundle.positive();
        out.beginEncompassing();
        Bundle from_reserves = Bundle::common(firm->reserves_, out);

        Bundle done = firm->reserves_.transfer(from_reserves, to, epsilon);
        out.transfer(done, epsilon);

        if (out > 0) {
            // Need to produce the rest
            firm->produceReserved(out);
            assets.transfer(out, to, epsilon);
        }

        // Call this in case any of the excess production and/or payment assets allow us to reduce
        // reserved production by transferring some assets to reserves.
        firm->reduceProduction();
    }
    catch (...) {
        to.abortTransaction();
        assets.abortTransaction();
        throw;
    }

    to.commitTransaction();
    assets.commitTransaction();

    state = ReservationState::complete;
}

void Firm::Reservation::release() {
    if (state != ReservationState::pending)
        throw Reservation::non_pending_exception();

    state = ReservationState::aborted;

    Bundle res_pos = bundle.positive();
    if (res_pos == 0) // Nothing to do
        return;

    Bundle unreserved_prod = Bundle::reduce(firm->reserved_production_, res_pos);

    // If we removed some from reserved production, add it to excess production
    if (unreserved_prod != 0) {
        firm->excess_production_ += unreserved_prod;

        if (res_pos == 0) {
            // That reduction is everything we had to unreserve, so we can just call
            // reduceExcessProduction instead of reduceProduction.
            firm->reduceExcessProduction();
            return;
        }
    }

    // Anything left should be transferrable from reserves to assets.  This could throw a negativity
    // exception if something got screwed up.
    firm->reserves_.transfer(res_pos, firm->assets, firm->epsilon);

    firm->reduceProduction();
}

void Firm::reduceProduction() {
    Bundle common = Bundle::reduce(assets, reserved_production_);

    if (common != 0) {
        reserves_ += common;
        excess_production_ += common;
    }

    reduceExcessProduction();
}

Firm::Reservation Firm::createReservation(BundleNegative bundle) {
    return Reservation(sharedSelf(), bundle);
}

Bundle FirmNoProd::produce(const Bundle&) {
    throw production_unavailable();
}

bool FirmNoProd::supplies(const Bundle &b) const {
    return assets.covers(b);
}

double FirmNoProd::canSupplyAny(const Bundle &b) const {
    return assets.multiples(b);
}

void FirmNoProd::ensureNext(const Bundle &b) {
    if (!(assets >= b))
        produceNext(b - Bundle::common(assets, b));
}

void FirmNoProd::reserveProduction(const Bundle&) {
    throw production_unavailable();
}

void FirmNoProd::reduceExcessProduction() {}

Firm::Reservation::Reservation(SharedMember<Firm> firm, BundleNegative bundle)
    : bundle(bundle), firm(firm) {}

Firm::Reservation::~Reservation() {
    if (firm and state == ReservationState::pending)
        release();
}

Firm::operator std::string() const {
    return "Firm[" + std::to_string(id()) + "]";
}

}
