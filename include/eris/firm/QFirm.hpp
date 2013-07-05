#pragma once
#include <eris/Firm.hpp>
#include <eris/Bundle.hpp>

namespace eris { namespace firm {

/** This class provides a firm that produced a fixed output quantity in each period, and sells that
 * output through a market such as eris::market::Quantity.  During each period, the firm receives
 * payment for sold goods.  Unsold quantities can be kept for the following period or depreciated
 * partially or entirely.
 *
 * This class is ideally coupled with an inter-period optimizer such as QFStepper that adjusts the
 * production level each period based on the previous period's results.
 */
class QFirm : public FirmNoProd {
    public:
        /** Constructs a QFirm that produces multiples of Bundle out and sells to the given market.
         *
         * \param out the Bundle of output the firm produces multiples of.
         *
         * \param initial_quantity the quantity to produce initially when the Firm is added to the
         * simulation.  For future periods this value will be adjusted (typically by QFStepper)
         * upwards or downwards depending on the previous period's results and change in market
         * price.
         *
         * \param depreciation the depreciation that applies to unsold quantity, a value between 0
         * and 1 (inclusive).  1 means all returned quantity will be destroyed; 0 means all returned
         * quantity will be kept for the following period.  Defaults to 1 (total depreciation).
         */
        QFirm(Bundle out, double initial_capacity, double depreciation = 1.0);

        /** Advances to the next period, calling depreciate() and produceNext().
         * When called, any unsold output is depreciated (according
         * to the depreciation parameter given during construction), and the next period's quantity
         * is produced.  Typically there will be a QFStepper controlling the changes to the
         * production level parameter.
         *
         * Note that the production level will take account of undepreciated stock; thus if the
         * capacity to be produced is 20, and 4 remains after depreciation, only 16 will actually be
         * produced.
         */
        virtual void advance() override;

        /** Called to depreciate any current on-hand stock according to the depreciation parameter
         * given during construction.  Only good quantities that are in the output unit given during
         * construction will be changed.  Note that zero-quantity goods in the output bundle *are*
         * depreciated as well.  Returns the post-depreciation quantities.
         */
        virtual Bundle depreciate() const;

        /** Called to produce at least b for next period.  This is typically called indirectly via
         * ensureNext().  The default implementation produces multiples of output() at no cost;
         * this is intended to be subclassed and overridden.
         */
        virtual void produceNext(const Bundle &b) override;

        /** Calls produceNext() when added to a simulation to produce the initial period's output.
         */
        virtual void added() override;

        /** The capacity for the next period, as a multiple of the output bundle.  The firm will
         * ensure its assets contains at least capacity times the output bundle given in the
         * constructor.
         */
        double capacity;

        /** Returns the Bundle that this QFirm produces (when producing before the beginning of a
         * period).
         */
        const Bundle& output() const noexcept;

        /** Returns the current depreciation value.  This will be a value in \f$[0, 1]\f$: 1
         * indicates total depreciation, 0 indicates no depreciation of unsold assets.
         */
        virtual double depreciation() const noexcept;
        /** Sets the depreciation value for the next depreciate() call.  Must be a value in \f$[0,
         * 1]\f$; values less than 0 will be set to 0, values greater than 1 will be set to 1.
         */
        virtual void depreciation(const double &depr) noexcept;

    private:
        const Bundle output_unit_;
        double depreciation_ = 1;
};

} }
