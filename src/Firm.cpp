#include <eris/Firm.hpp>

namespace eris {

bool Firm::canSupply(const Bundle &b) const noexcept {
    return canSupplyAny(b) >= 1.0;
}

void Firm::advance() {}

Firm::supply_failure::supply_failure(std::string what) : std::runtime_error(what) {}
Firm::supply_mismatch::supply_mismatch(std::string what) : supply_failure(what) {}
Firm::supply_mismatch::supply_mismatch() : supply_failure("Firm does not supply requested goods") {}
Firm::supply_constraint::supply_constraint(std::string what) : supply_failure(what) {}
Firm::supply_constraint::supply_constraint()
    : supply_failure("Firm cannot supply requested bundle: capacity constraint would be violated")
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

double Firm::produceAny(const Bundle &b) {
    try { produce(b); }
    catch (supply_constraint&) { return 0.0; }
    return 1.0;
}

bool Firm::supplies(const Bundle &b) const noexcept {
    Bundle checkProduce;
    const BundleNegative &a = assets();
    for (auto item : b) {
        eris_id_t g = item.first;
        if (a[g] <= 0)
            checkProduce.set(g, 1);
    }

    if (checkProduce.empty()) // Assets has positive quantities of everything requested
        return true;

    return produces(checkProduce);
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

void Firm::supply(const Bundle &b) {
    // Check to see if we have enough assets to cover the demand
    Bundle onhand = Bundle::common(assets(), b);
    Bundle need = b - onhand;

    // If needed, produce what we can't supply from assets
    if (need != 0) produce(need);

    // If we survived this far, either assets has enough or production succeeded; take the onhand
    // amount out of assets as well.
    assets() -= onhand;
}

double Firm::supplyAny(const Bundle &b) {
    // Check to see if we have enough assets to cover the demand
    Bundle onhand = Bundle::common(assets(), b);
    Bundle need = b - onhand;

    if (need == 0) {
        // Supply it all from assets
        assets() -= onhand;
        return 1.0;
    }

    // Otherwise produce what we can't supply from assets
    // (NB: can't reduce assets yet, because produceAny might throw a supply_mismatch)
    double c = 0.0;
    try {
        c = produceAny(need);
    }
    catch (supply_mismatch &e) {
        if (!(onhand > 0)) {
            // We can't supply any, and can't produce any, and produceAny says it's a
            // supply_mismatch, so it really is a supply_mismatch.
            throw e;
        }
        // Otherwise ignore the exception: it isn't actually a supply mismatch since we have
        // some positive supply from surplus.
    }

    if (c > 0) {
        // produceAny() produced at least some fraction of what we need; add it to the onhand quantity.
        if (c < 1.0) need *= c;
        onhand += need;
    }
    // else produceAny() didn't produce anything

    // Add any onhand surplus back into assets
    assets() -= onhand % b;
    return c >= 1.0 ? 1.0 : onhand / b;
}

}
