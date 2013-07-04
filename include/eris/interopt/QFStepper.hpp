#pragma once
#include <eris/firm/QFirm.hpp>
#include <eris/interopt/Stepper.hpp>

namespace eris { namespace interopt {

using eris::firm::QFirm;

/** Inter-period optimizer class for QFirm instances.  QFirms perform all production before a period
 * begins; this optimizer adjusts the firm's per-period production level up or down depending on
 * whether or not the firm had a surplus or not in the previous period.  This class does *not* make
 * the firm actually carry out production; that occurs in QFirm's advance() method.
 *
 * \sa eris::market::QFirm
 * \sa eris::market::Quantity
 * \sa eris::interopt::QMStepper
 */
class QFStepper : public Stepper {
    public:
        /** Creates a new QFirm capacity stepper.
         *
         * \param qf the QFirm whose quantity this optimizer adjusts
         * \param step the initial step size
         * \param increase_count the number of consecutive moves before the step size is increased
         *
         * \sa eris::interopt::Stepper
         */
        QFStepper(
                const QFirm &qf,
                double step = 1.0/32.0,
                int increase_count = 4
             );

    protected:
        /** Calculates whether or not the firm quantity should be increased or decreased based on
         * whether the firm sold all its product in the previous period.
         *
         * If it sold out, quantity is increased; if there was a surplus, quantity decreases.
         */
        virtual bool should_increase() const override;

        /// Takes a step, setting the new market price to the given multiple of the current price.
        virtual void take_step(double relative) override;

        /// Declares the handled market as a member this optimizer depends on
        virtual void added() override;

    private:
        eris_id_t firm_;
};

} }

