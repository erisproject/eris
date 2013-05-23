#pragma once
#include <eris/Firm.hpp>
#include <limits>

namespace eris { namespace firm {

/* This class captures a firm that sets an input price, and can instantly produce any amount at the
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
 * A PriceFirm has an optional capacity, which defaults to infinity, which limits the maximum the
 * firm can actually produce: the firm will produce, at most, 
 */
class PriceFirm : public Firm {
    public:
        PriceFirm(Bundle out, Bundle price, double capacity = std::numeric_limits<double>::infinity());

        virtual void setPrice(Bundle price) noexcept;
        virtual const Bundle price() const noexcept;
        virtual void setOutput(Bundle output) noexcept;
        virtual const Bundle output() const noexcept;

        virtual double canProduceAny(const Bundle &b) const noexcept;

        virtual void produce(const Bundle &b);
        virtual double produceAny(Bundle const &b);

        virtual void advance();

    protected:
        Bundle _price, _output;
        double capacity, capacityUsed = 0;
};

} }
