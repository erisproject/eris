#pragma once
#include <eris/Optimize.hpp>
#include <eris/Firm.hpp>
#include <limits>

namespace eris { namespace firm {

/** This class captures a firm that sets an input price and can instantly produce any amount at the
 * given price, optionally up to a maximum capacity.  Both the input price and outputs are Bundles;
 * most commonly the input price is a Bundle containing just a "money" good, while output is often
 * just a single good.
 *
 * The input bundle defines the price per multiple of the output bundle.  Typically this will be
 * single Good bundles.  e.g. if the input bundle is (1,1.5,0) and the output bundle is (0,0,1), then
 * buying 3 units of good three will cost 3 units of good 1 plus 4.5 units of good 2.
 *
 * For single good outputs, it is recommended (but not required) to normalize the bundle quantities
 * so that the output bundle quantity to 1, as in the above example.  The previous example would be
 * identical to one with input bundle (2,3,0) and output bundle (0,0,2).
 *
 * A PriceFirm has an optional capacity, defaulting to infinity, which limits the maximum the firm
 * can actually produce: the firm will produce, at most, this value and be unable to produce beyond
 * that point until advance() is called to signal a new period.
 */
class PriceFirm : public Firm, public virtual interopt::Advance {
    public:
        /** Constructs a PriceFirm that produces any multiple of Bundle out for Bundle price, up to
         * a maximum (cumulative) capacity of capacity*out.  If omitted, capacity defaults to
         * infinity.
         */
        PriceFirm(Bundle out, Bundle price, double capacity = std::numeric_limits<double>::infinity());

        /// Sets the Bundle at which this firm sells per output Bundle.
        virtual void setPrice(Bundle price) noexcept;
        /// Returns the Bundle at which this firm sells output
        virtual const Bundle& price() const noexcept;
        /// Sets the Bundle that this firm produces.
        virtual void setOutput(Bundle output) noexcept;
        /// Returns the Bundle that this firm produces.
        virtual const Bundle& output() const noexcept;

        /** Returns the maximum quantity, as a multiple of the passed-in Bundle, that the firm is
         * able to supply.  Note that, unlike the base class Firm, this value always returns the
         * exact multiple; in particular, a value of 1.0 indicates that supply exactly the
         * requested Bundle will exhaust the firm's ability to supply further positive quantities
         * of the Bundle.  For an unconstrained firm, this will return positive infinity, as long as
         * the requested Bundle is covered by the firm's output Bundle.
         */
        virtual double canSupplyAny(const Bundle &b) const noexcept override;

        /** Advances to the next period.  In particular, this resets the used capacity to 0, thus
         * restoring a constrained firm's ability to produce.
         */
        virtual void interAdvance() override;

    protected:
        /// The base price bundle; the firm accepts multiples of this bundle as payment
        Bundle price_;
        /// The output bundle: the firm produces multiples of this bundle as output
        Bundle output_;
        /// The per-period capacity of the firm.  May be infinity.
        double capacity_;
        /// The capacity of the firm already used up in the current period.
        double capacity_used_ = 0;

        /** Returns the multiple, potentially infinity, of Bundle b that this firm is able to
         * produce before hitting its capacity constraint.  Returns 0 if the firm has already hit
         * its capacity constraint, or the requested Bundle cannot be produced with the firm's
         * output Bundle.
         */
        virtual double canProduceAny(const Bundle &b) const noexcept override;

        /** Produces the requested Bundle and adds it to assets.  If this succeeded without throwing an exception, the
         * firm's available capacity will be appropriately lowered.
         */
        virtual Bundle produce(const Bundle &b) override;

        /** Reserves production of the given Bundle.
         * \sa Firm::reserveProduction
         */
        virtual void reserveProduction(const Bundle &reserve) override;

        /// Checks current excess production to see if production levels can be decreased.
        virtual void reduceExcessProduction() override;
};

} }
