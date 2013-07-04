#pragma once
#include <eris/market/Quantity.hpp>
#include <eris/interopt/Stepper.hpp>

namespace eris { namespace interopt {

using eris::market::Quantity;

/** Inter-period optimizer class for Quantity-based markets.  Quantity markets have a fixed price in
 * a given period; this optimizer adjusts that price upwards or downwards based on whether there was
 * a shortage or surplus in the previous period.
 *
 * \sa eris::market::Quantity
 */
class QMStepper : public Stepper {
    public:
        /** Creates a new Quantity market price stepper.
         *
         * \sa eris::interopt::Stepper
         */
        QMStepper(
                const Quantity &qm,
                double step = 1.0/32.0,
                int increase_count = 4
             );

    protected:
        /** Calculates whether or not the market price should be increased or decreased based on
         * whether the market sold out in the previous period.
         *
         * If it sold out, price is increased; if there was a surplus, price decreases.
         */
        virtual bool should_increase() const override;

        /// Takes a step, setting the new market price to the given multiple of the current price.
        virtual void take_step(double relative) override;

        /// Declares the handled market as a member this optimizer depends on
        virtual void added() override;

    private:
        eris_id_t market_;
};

} }
