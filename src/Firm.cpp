#include <eris/Firm.hpp>
#include <eris/Simulation.hpp>

namespace eris {

bool Firm::canSupply(const Bundle &b) const noexcept {
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

bool Firm::canProduce(const Bundle &b) const noexcept {
    return canProduceAny(b) >= 1.0;
}

double Firm::canProduceAny(const Bundle &b) const noexcept {
    return 0.0;
}

bool Firm::produces(const Bundle &b) const noexcept {
    return canProduceAny(b) > 0;
}

double Firm::canSupplyAny(const Bundle &b) const noexcept {
    // We can supply the entire thing from current assets:
    if (assets() >= b) return 1.0;

    // Otherwise try production to make up the difference
    Bundle onhand = Bundle::common(assets(), b);
    Bundle need = b - onhand;
    double c = canProduceAny(need);
    if (c >= 1.0) return 1.0;
    if (c <= 0.0) return onhand / b;

    // We can produce some, but not enough; figure out how much we can supply in total:
    need *= c;
    return (need + onhand) / b;
}

bool Firm::supplies(const Bundle &b) const noexcept {
    Bundle check_produce;
    const Bundle &a = assets();
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
    transfer(res, assets);
    return res;
}

void Firm::transfer(Reservation &res, Bundle &assets) {
    transfer_(*res, assets);
}

void Firm::release(Reservation &res) {
    release_(*res);
}

Firm::Reservation Firm::reserve(const BundleNegative &reserve) {
    Bundle res_pos = reserve.positive();
    // First see if current assets can handle any of the requested Bundle
    Bundle common = Bundle::common(assets(), res_pos);
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
        assets() -= common;
        reserves_ += common;
    }

    return createReservation(reserve);
}

void Firm::produceReserved(const Bundle &b) {
    if (!(b <= reserved_production_)) {
        // We might just be hitting a numerical precision problem, so let's try taking off a
        // miniscule amount to see if that helps.  If it does, we're just dealing with a numerical
        // error.
        double epsilon = 1e-14;
        if ((1-epsilon) * b <= reserved_production_) {
            // Figure out which values reserved_production_ is short of and "fix" them
            for (auto g : b) {
                if (reserved_production_[g.first] < g.second)
                    reserved_production_.set(g.first, g.second);
            }
        }
        else {
            throw production_unreserved();
        }
    }

    Bundle produced = produce(b);
    reserved_production_ -= b;

    if (produced != b)
        excess_production_ -= (produced - b);

    assets() += produced;
}

void Firm::transfer_(Reservation_ &res, Bundle &to) {
    if (!res.active)
        throw Reservation_::inactive_exception();

    res.active = false;
    res.completed = true;

    // Take payment:
    Bundle in = res.bundle.negative();
    to -= in;
    assets() += in;

    // Now transfer and/or produce output
    Bundle out = res.bundle.positive();
    Bundle epsilon = 1e-14 * out;

    Bundle common = Bundle::reduce(reserves_, out);
    to += common;

    if (!(out < epsilon)) {
        // Need to produce the rest
        produceReserved(out);
        assets() -= out;
        to += out;
    }

    // Call this in case any of the excess production and/or payment assets allow us to reduce
    // reserved production by transferring some assets to reserves.
    reduceProduction();
}

void Firm::release_(Reservation_ &res) {
    if (!res.active)
        throw Reservation_::inactive_exception();

    res.active = false;
    res.completed = true;

    Bundle res_pos = res.bundle.positive();
    if (res_pos == 0) // Nothing to do
        return;

    Bundle unreserved_prod = Bundle::reduce(reserved_production_, res_pos);

    // If we removed some from reserved production, add it to excess production
    if (unreserved_prod != 0) {
        excess_production_ += unreserved_prod;

        if (res_pos == 0) {
            // That reduction is everything we had to unreserve, so we can just call
            // reduceExcessProduction instead of reduceProduction.
            reduceExcessProduction();
            return;
        }
    }

    // Anything left should be transferrable from reserves to assets.  This could throw a negativity
    // exception if something got screwed up.
    reserves_ -= res_pos;
    assets() += res_pos;

    reduceProduction();
}

void Firm::reduceProduction() {
    Bundle common = Bundle::reduce(assets(), reserved_production_);

    if (common != 0) {
        reserves_ += common;
        excess_production_ += common;
    }

    reduceExcessProduction();
}

Firm::Reservation Firm::createReservation(BundleNegative bundle) {
    return Reservation(new Reservation_(simulation()->agent(*this), bundle));
}

Bundle FirmNoProd::produce(const Bundle &b) {
    throw production_unavailable();
}

bool FirmNoProd::supplies(const Bundle &b) const noexcept {
    return assets().covers(b);
}

double FirmNoProd::canSupplyAny(const Bundle &b) const noexcept {
    return assets() / b;
}

void FirmNoProd::ensureNext(const Bundle &b) {
    if (!(assets() >= b))
        produceNext(b - Bundle::common(assets(), b));
}

void FirmNoProd::reserveProduction(const Bundle &reserve) {
    throw production_unavailable();
}

void FirmNoProd::reduceProduction() {}

void FirmNoProd::reduceExcessProduction() {}

Firm::Reservation_::Reservation_(SharedMember<Firm> firm, BundleNegative bundle)
    : bundle(bundle), firm(firm) {}

Firm::Reservation_::~Reservation_() {
    if (!completed)
        release();
}

void Firm::Reservation_::transfer(Bundle &assets) {
    firm->transfer_(*this, assets);
}

void Firm::Reservation_::release() {
    firm->release_(*this);
}
}
