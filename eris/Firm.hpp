#pragma once
#include <eris/agent/AssetAgent.hpp>
#include <exception>
#include <stdexcept>
#include <string>

namespace eris {

/** Namespace for all specific eris::Firm implementations. */
namespace firm {}

/** enum of reservation states for market-level and firm-level reservations.  A reservation
 * can either be pending, complete, or aborted.
 */
enum class ReservationState { pending, complete, aborted };

/** Abstract base class for representing a firm that uses some input (such as money) to supply some
 * output (such as a good).
 */
class Firm : public agent::AssetAgent {
    public:
        /** Returns true if the firm is able to supply the given Bundle.  Returning false thus 
         * indicates that the firm either cannot supply some of the items in the Bundle, or else
         * that producing the given quantities exceeds some production limit.  By default, this
         * simply calls canSupplyAny, and returns true or false depending on whether canSupplyAny
         * returned a value >= 1.
        */
        virtual bool canSupply(const Bundle &b) const;

        /** Returns a non-negative double indicating whether the firm is able to supply the given
         * Bundle.  Values of 1 (or greater) indicate that the firm can supply the entire Bundle
         * (and corresponds to a true return from canSupply); 0 indicates the firm cannot supply any
         * fraction of the bundle at all; something in between indicates that the firm can supply
         * that multiple of the bundle.
         *
         * The default implementation of this method calculates checks current assets and (if
         * insufficient) calls canProduceAny() to determine the value between 0 and 1.
         *
         * Subclasses may, but are not required to, return values larger than 1.0 to indicate that
         * capacity beyond the Bundle quantities can be supplied.  Note, however, that a return
         * value of exactly 1.0 DOES NOT indicate that no further amount can be supplied (though
         * specific subclasses may add that interpretation).
         */
        virtual double canSupplyAny(const Bundle &b) const;

        /** This is similar to canSupplyAny(), but only returns a true/false value indicating
         * whether the firm can supply any positive multiple of the given Bundle.  This is identical
         * in functionality to (canSupplyAny(b) > 0), but more efficient (as the calculations to
         * figure out the precise multiple that can be supplied are skipped).
         */
        virtual bool supplies(const Bundle &b) const;

        /** Contains a reservation of a BundleNegative net transfer from a firm.  The firm will
         * consider the reserved quantity unavailable until a call of either transfer() (which
         * completes the transfer) or release() (which cancels the transfer) is called.
         *
         * If the object is destroyed before having transfer() or release() called, release() will be
         * called automatically.
         *
         * This object is not intended to be used directly, but rather through the Reservation
         * unique_ptr typedef.
         */
        class Reservation final {
            private:
                Reservation(SharedMember<Firm> firm, BundleNegative transfer);
                friend class Firm;

                // Disable default and copy constructors
                Reservation() = delete;
                Reservation(const Reservation &) = delete;

                // Move/copy assignment not allowed
                Reservation& operator=(Reservation &&move) = delete;
                Reservation& operator=(const Reservation &copy) = delete;

            public:
                /// Move constructor
                Reservation(Reservation &&move) = default;
                /** Destructor.  If this Reservation is destroyed without having been completed or aborted
                 * (via transfer() or release()), it will be aborted (by calling release() on its Firm).
                 */
                ~Reservation();
                /// The state of this reservation.
                ReservationState state = ReservationState::pending;
                /** The Bundle that is reserved.  Positive amounts are transferred out of the firm;
                 * negative amounts are transferred into the firm.
                 */
                const BundleNegative bundle;
                /// The firm for which this Reservation applies.
                const SharedMember<Firm> firm;
                /** Calls transfer() on the firm with the Reservation object.  Calling
                 * obj->transfer(a) is equivalent to calling `obj->firm->transfer(obj, a)`.
                 */
                void transfer(Bundle &to);
                /** Calls release() on the firm.  This is equivalent to calling
                 * `obj->firm->release(obj)`.
                 */
                void release();

                /** Exception class thrown if attempting to transfer or release a Reservation that has
                 * already been transferred or released.
                 */
                class non_pending_exception : public std::exception {
                    /// Overridden to return a default "what" message for the exception
                    public: const char* what() const noexcept override { return "Attempt to transfer/release a non-pending firm Reservation"; }
                };
        };

        /** Tells the firm to supply the given Bundle and transfer it to the given assets bundle.
         * This method is simply a wrapper around reserve() and transfer(); see those methods for
         * details.  That is, `firm->supply(b, assets)` is identical to
         * `firm->reserve(b)->transfer(assets)`.
         *
         * \returns the (completed) Reservation
         *
         * \sa reserve()
         * \sa transfer()
         */
        virtual Reservation supply(const BundleNegative &b, Bundle &assets);

        /** An exception class that can be thrown by supply() to indicate a supply failure.
         * This may be subclassed as needed to provide for more specific supply errors.
         * \sa supply_mismatch
         * \sa production_constraint
         */
        class supply_failure : public std::runtime_error {
            public:
                /// Exception constructor: takes an error message
                supply_failure(std::string what);
        };
        /** Exception class indicating that the firm does not supply one of more of the goods in the
         * bundle.  This is *not* a constraint violation: throwing this exception indicates that the
         * firm, even if unconstrained, simply cannot produce some of the requested goods.
         */
        class supply_mismatch : public supply_failure {
            public:
                /// Exception constructor: takes an error message
                supply_mismatch(std::string what);
                /// Exception constructor, default error message
                supply_mismatch();
        };
         /** Exception class thrown when supplying the requested bundle would exceeds the firm's
          * capacity (e.g. a production constraint, or some other supply limit).
          */
        class production_constraint : public supply_failure {
            public:
                /// Constructs the exception with the specified message
                production_constraint(std::string what);
                /** Constructs the exception with a default message about a capacity constraint
                 * being violated.
                 */
                production_constraint();
        };
        /** Exception class thrown when asking a firm without instantaneous production ability to
         * produce.
         */
        class production_unavailable : public production_constraint {
            public:
                /// Constructs the exception with the specified message
                production_unavailable(std::string what);
                /** Constructs the exception with a default message about this firm having no
                 * instantaneous production ability.
                 */
                production_unavailable();
        };
        /** Exception class thrown when a firm is requested to produce an amount from production
         * reserves greater than the amount that has actually been reserved.
         *
         * \sa produceReserved
         */
        class production_unreserved : public supply_failure {
            public:
                /// Constructs the exception with the specified message
                production_unreserved(std::string what);
                /** Constructs the exception with a default message about requesting production that
                 * has not been reserved.
                 */
                production_unreserved();
        };


        /** Reserves the given quantities to be later transferred from the firm by a
         * transferReserves() call, or aborted via a release() call.
         *
         * This works with 4 internal Bundles to manage assets, reserves and production.  Those are:
         * assets, reserved assets, reserved production, and excess production.
         *
         * \sa assets_
         * \sa reserves_
         * \sa reserved_production_
         * \sa excess_production_
         *
         * These work as follows:
         * - Assets are the firm's current, unreserved assets on hand.
         * - Reserves are assets that are on hand, but have been reserved for a currently pending
         *   transfer Reservation.
         * - Reserved production is output that has been reserved for pending transfers.
         * - Excess production is byproduct output of currently pending reserved production.
         *
         * An overview of the reservation functionality is as follows:
         *
         * - If any portion of the requested Bundle is contained in current assets, that portion is
         *   transferred from assets into reserves.
         * - If any portion of the outstanding requested output can be provided from excess
         *   production, that amount is moved from excess production to reserved production.
         * - Any outstanding bundle is passed to reserveProduction(), which will increase reserved
         *   production by the outstanding amount (and possibly add to excess production, if there
         *   are unwanted production byproducts).  This last step may throw an exception if
         *   production cannot be reserved.
         *
         * (Note that the order isn't quite as described: the actual transfers are calculated but
         * don't occur until after the reserveProduction call, so that no changes occur in the case
         * of a failure).
         */
        virtual Reservation reserve(const BundleNegative &reserve);

        /** Transfers the given Bundle reservation out of reserves and into the provided Bundle.
         * Reserved production is performed if required.  Any negative quantities in the reservation
         * are removed from the given Bundle and added to the firm's assets.  When the transfer is
         * completed, reduceProduction() is called to see if any currently planned production can
         * instead be supplied from the newly-gained assets.
         */
        void transfer(Reservation &res, Bundle &assets);

        /** Cancels a reserved quantity previously reserved with reserve(), indicating that the
         * quantity will not be transferred via transferReserves.
         *
         * The default implementation attempts to transfer as much of the bundle to be released as
         * possible from reserved_production_ to excess_production.  If that was sufficient to cover
         * the whole request, it calls reduceExessProduction() and finishes.  Otherwise, any
         * remaining amount is transferred from reserves to assets, followed by a reduceProduction()
         * call.
         */
        virtual void release(Reservation &res);

        /** Controls the relative tolerance for handling invalid requests such as being asked to
         * produce more than is available.  Requested amounts can be changed by up to this amount to
         * avoid a constraint or to use up all of a resource (rather than leaving a miniscule amount
         * behind).  Defaults to 1e-10.
         */
        double epsilon = 1e-10;

    protected:
        // The following are internal methods that subclasses should provide, but should only be
        // called externally indirectly through a call to the analogous supply...() function.

        /** Returns true if the firm is able to produce the requested bundle.  By default, this
         * simply calls canProduceAny() and returns true iff it returns >= 1.0, which should be
         * sufficient in most cases.
         */
        virtual bool canProduce(const Bundle &b) const;

        /** Analogous to (and called by) canSupplyAny(), this method should return a value between 0
         * and 1 indicating the proportion of the bundle the firm can instantly produce.  By default
         * it returns 0, which should be appropriate for firms that don't instantaneously produce,
         * but will certainly need to be overridden by classes with within-period production.
         */
        virtual double canProduceAny(const Bundle &b) const;

        /** Analogous to (and called by) supplies(), this method returns true if the firm is able to
         * produce some positive quantity of each good of the given Bundle.  This is equivalent to
         * (canProduceAny(b) > 0), but may be more efficient when the specific value of
         * canProduceAny() isn't needed.
         *
         * Note to subclasses: by default, this simply calls (canProduceAny(b) > 0), where the
         * default canProduceAny(b) simply returns 0.  If overriding canProduceAny(b) with more
         * complicated logic, you should also contemplate overriding produces().
         */
        virtual bool produces(const Bundle &b) const;

        /** This method is called by transfer() with the portion of the Bundle that cannot be
         * transferred from the firm's current assets, if any.  This method uses up reserved
         * production, and so should usually be preceded by a call to reserveProduction().
         *
         * \throws supply_mismatch if the requested bundle is not produced by this firm.
         * \throws production_constraint if the requested bundle would violate a production
         * constraint.
         * \throws production_unavailable if the firm does not have instantaneous production
         * ability.
         * \throws production_unreserved if currently reserved production does not cover the
         * requested Bundle.
         *
         * If this method completes without throwing an exception, it produces (at least) the
         * requested Bundle (by calling produce()), removes the produced amount from
         * reserved_production_ (and excess_production_, if appropriate) and adds the produced
         * amount to the firm's current assets.
         */
        virtual void produceReserved(const Bundle &b);

        /** Abstract method called by produceReserved() to produce the actual Bundle.  Returns the
         * Bundle produced, which must be >= the requested bundle.  This method does not need to
         * worry about production constraints: those should be handled when the production is
         * reserved, in reserveProduction().
         */
        virtual Bundle produce(const Bundle &b) = 0;

        /** Creates a Reservation and returns it.  For internal subclass use only; external objects
         * create a reservation by calling reserve() (which uses this to create the actual object).
         */
        Reservation createReservation(BundleNegative bundle);

        /** Reserves the given quantities to be produced by the firm by a later produce() call, or
         * aborted via a releaseProduction() call.  Should increases reserved_production_ by the
         * given amount, and possibly increases excess_production_ if appropriate.  If appropriate,
         * this method could include other checks such as capacity constraints, depending on the
         * functionality of the subclass.
         *
         * This method is intentionally protected rather than public as it is intended to be called
         * by reserve() when needed.
         *
         * \sa reserve() for the details on how the overall assets/reserves/reserved production
         * mechanism works.
         * \throws the same exceptions as produce()
         */
        virtual void reserveProduction(const Bundle &reserve) = 0;

        /** This method checks currently planned production for any possible reductions.  The
         * default implementation does two things:
         * - If assets_ contains any of the currently reserved production, that amount is moved from
         *   assets_ to reserves_, and moved from reserved_production_ to excess_production_.
         * - reduceExcessProduction() is called to reduce any production levels as appropriate.
         */
        virtual void reduceProduction();

        /** Attempts to reduce currently excess production.  Typically this happens as the result of
         * the cancellation of some reserved amount, in which case some or all of the cancelled
         * amount may have been moved from reserved production to excess production.
         *
         * Essentially, this is responsible for "undoing" production that hasn't occurred yet,
         * ensuring that reserving production, then cancelling that reservation restores state as
         * appropriate.
         *
         * For example, suppose a firm produces a single good output, and has 4 of that good
         * currently on hand in assets_, and can produce 10 more before hitting a production
         * constraint.  The firm is called to reserve output twice, the first time for 5 units, the
         * second for 7 units of the output good.  The first call thus transfers the 4 on hand from
         * assets_ to reserves_, then reserves production of 1.  The second call reserves another 7
         * (for total reserved production of 8, remaining available production of 2).  The first
         * reservation is then cancelled; this results in 5 units being transferred from
         * reserved_production_ to excess_production_; it is then up to this method to set
         * excess_production_ to 1 and ensuring that 9 more units can be produced.
         */
        virtual void reduceExcessProduction() = 0;

        /** Reserves are assets that have been reserved for transfer away from the firm, but not yet
         * transferred.  Those assets will be either transferred or cancelled before the period
         * ends.  If cancelled, reserved assets are returned to regular assets.
         *
         * \sa reserve()
         * \sa transferReserves()
         * \sa assets_
         * \sa reserved_production_
         * \sa excess_production_
         */
        Bundle reserves_;

        /** Reserved production is production output that has been allocated (and is available) to
         * service requested output reservations, but not yet produced.  When previously reserved
         * amounts are transferred, production occurs.  If previously reserved amounts are instead
         * cancelled, reserved production becomes excess production (and then excess production is
         * reduced, if possible).
         *
         * \sa reserve()
         * \sa reserveProduction()
         * \sa excess_production_
         */
        Bundle reserved_production_;

        /** Excess production stores production output that is a side-effect of other, requested
         * production.  Typically this occurs when firms produce a multi-good Bundle in some
         * specific ratio, but the requested reserve amount doesn't require all of the produced
         * Bundle.
         *
         * For example, an orange juice producing firm might produce no-pulp, some-pulp, and
         * extra-pulp orange juice in a fixed 1-1-1 ratio; if a given customer reserves (2,0,1)
         * (that is: 2 units of no-pulp juice and 1 unit of extra-pulp juice), the firm will reserve
         * production of 2 units of each, thus ending up with reserved production of (2,0,1) and
         * excess production of (0,2,1).  A subsequent reservation of (0,1,0) would then be done by
         * transferring (0,1,0) from excess to reserved, without requiring any additional
         * production.
         *
         * This Bundle is also used, temporarily, when cancelling an earlier reservation: when a
         * cancellation reduces currently required reserved production, the cancelled amount is
         * moved from reserved_production_ to excess_production_, immediately followed by a call to
         * reduceProduction().
         *
         * \sa reserved_production_
         * \sa reserveProduction()
         * \sa reduceProduction()
         */
        Bundle excess_production_;
};

/** Abstract specialization of Firm intended for firms with no instantaneous production capacity.
 * This has optimized versions of some of Firm's methods, plus an abstract produceNext method
 * intended for inter-period production.
 */
class FirmNoProd : public Firm {
    public:
        /** Throws a Firm::production_unavailable exception if called.  FirmNoProd have no
         * instantaneous production capabilities.
         */
        virtual Bundle produce(const Bundle &b) override;

        /// Overridden to optimize by avoiding production checks.
        virtual bool supplies(const Bundle &b) const override;

        /** Overridden to optimized by skipping production method calculations and calls.  Note that
         * unlike the Firm version of this method, this will return values greater than 1 (when
         * appropriate).
         */
        virtual double canSupplyAny(const Bundle &b) const override;

        /** Calls to ensure that there is at least the given Bundle available in assets for the next
         * period.  If current assets are sufficient, this does nothing; otherwise it calls
         * produceNext with the Bundle of quantities required to hit the target Bundle.
         *
         * \param b the Bundle of quantities needed in assets() next period.
         */
        virtual void ensureNext(const Bundle &b);

        /** Called to produce at least b for next period.  This is typically called indirectly
         * through ensureNext(), which takes into account current assets to hit a target capacity.
         * Thus this method must not take into account the current level of assets(): the passed-in
         * Bundle is the *additional* amount of production required.
         *
         * \param b the (minimum) Bundle to be produced and added to current assets().
         *
         * \sa QFirm
         * \sa QFStepper
         */
        virtual void produceNext(const Bundle &b) = 0;

    protected:
        /** Overrides Firm::reserveProduction() to simply throw a Firm::production_unavailable
         * exception if called, since this class does not support intra-period production.
         */
        virtual void reserveProduction(const Bundle &reserve) override;

        /** Overrides Firm::reduceProduction() with a version that does nothing, since firms of this
         * base class have no production at all.
         */
        virtual void reduceProduction() override;
        /** Provides an implementation of Firm::reduceExcessProduction that does nothing (since
         * this class of firm has no production at all).
         */
        virtual void reduceExcessProduction() override;
};

}
