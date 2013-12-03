#pragma once
#include <eris/firm/QFirm.hpp>
#include <eris/interopt/ProfitStepper.hpp>

namespace eris { namespace interopt {

using eris::firm::QFirm;

/** Inter-period optimizer class for QFirm instances.  QFirms perform all production before a period
 * begins; this optimizer adjusts the firm's per-period production level up or down depending on
 * whether or not the firm had a surplus or not in the previous period.  This class does *not* make
 * the firm actually carry out production; that occurs in QFirm's advance() method.
 *
 * There are two kinds of steps that occur: a natural, relative step (as handled by the Stepper
 * superclass), and a jump that occurs when the quantity sold in a period was less than half the
 * capacity (in which case we jump to the last period's sales).
 *
 * \sa eris::market::QFirm
 * \sa eris::market::Quantity
 * \sa eris::interopt::QMStepper
 */
class QFStepper : public ProfitStepper {
    public:
        /** Creates a new QFirm capacity stepper.
         *
         * \param qf the QFirm whose quantity this optimizer adjusts
         * \param profit_basis the basis Bundle for measuring profits
         * \param step the initial step size
         * \param increase_count the number of consecutive moves before the step size is increased
         *
         * \sa eris::interopt::Stepper
         */
        QFStepper(
                const QFirm &qf,
                const Bundle &profit_basis,
                double step = 1.0/32.0,
                int increase_count = 4
             );

    protected:
        /// Takes a step, setting the new market price to the given multiple of the current price.
        virtual void take_step(double relative) override;

        /** If quantity sold in the previous period was half of the firm's capacity or less (but
         * greater than 0), we cut capacity in half.
         */
        virtual bool should_jump() override;

        /** Sets the capacity to whatever we sold in the previous period.
         * \sa should_jump()
         */
        virtual void take_jump() override;

    private:
        eris_id_t firm_;
        double jump_cap_;
};

} }

