#pragma once
#include <eris/Member.hpp>
#include <eris/Firm.hpp>
#include <exception>
#include <limits>
#include <unordered_set>
#include <forward_list>

namespace eris {

/// Namespace for eris market implementations inheriting from eris::Market.
namespace market {}

/** Abstract base class for markets in Eris.
 *
 * At this basic level, a market has an output bundle and a price unit bundle (typically a bundle of
 * a single good with quantity normalized to 1).  The output bundle is often a single good, but
 * could be a "bundle" of goods in the usual economic sense as well.  A market also has a set of
 * suppliers (Firms) and a maximum quantity (potentially infinite), and (abstract) abilities to
 * price and purchase a output quantity of an arbitrary size.
 *
 * Purchases are made as multiples of the output bundle, with a price determined as a multiple of
 * the price unit bundle.  Thus the specific quantities in each bundle are scale invariant--only
 * relative differences of quantities within a multi-good price or output bundle are relevant.  For
 * ease of interpretation, it is thus recommended to normalize one of the quantities in each bundle
 * to 1, particularly when the price or output bundle consists of a single good.
 *
 * Subclasses must, at a minimum, define the price(q), quantity(p), and (both) buy(...) methods:
 * this abstract class has no allocation implementation.
 */
class Market : public Member {
    public:
        virtual ~Market() = default;

        /** Constructs a market that provides units of output_unit in exchange for units of
         * price_unit.  All exchanged quantities will be in scalar multiples of these bundles.
         */
        Market(const Bundle output_unit, const Bundle price_unit);

        /** The "price" of a given quantity of the output is a little bit complicated: there is, of
         * course, the total price of the output, but decisions may also depend on marginal prices.
         * Thus the following, which includes both.
         */
        struct price_info {
            /// Default constructor: yields a price_info with `.feasible` set to false.
            price_info() = default;
            /// Constructor for feasible quantities; sets `feasible` to true.
            price_info(double total, double marginal, double marginalFirst)
                : feasible{true}, total{total}, marginal{marginal}, marginalFirst{marginalFirst} {}
            /// True if the requested quantity is available.
            bool feasible = false;
            /// Total price 
            double total = std::numeric_limits<double>::quiet_NaN();
            /// marginal price of the last quantity purchased.
            double marginal = std::numeric_limits<double>::quiet_NaN();
            /// marginal price of the first quantity purchased.
            double marginalFirst = std::numeric_limits<double>::quiet_NaN();
        };

        /** Represents the "quantity" that a given price can buy.  In addition to the quantity of
         * output, this carries information on whether the requested spending amount can actually be
         * spent, or whether a market capacity constraint would be hit.
         *
         * \sa quantity(double)
         */
        struct quantity_info {
            /** Default construct: yields a quantity_info with `.constrained` set to false, and
             * numeric values set to 'nan'.
             */
            quantity_info() = default;
            /** Constructor for a set of quantities; initializes quantity, constrained, spent,
             * unspent to the given values.
             */
            quantity_info(double quantity, bool constrained, double spent, double unspent)
                : quantity{quantity}, constrained{constrained}, spent{spent}, unspent{unspent} {}
            /// The quantity purchasable
            double quantity = std::numeric_limits<double>::quiet_NaN();
            /// True if the purchase would hit a market constraint
            bool constrained = false;
            /** The price amount that would be actually spent.  This is simply the provided spending
             * amount when .contrained is false, but will be less when a constraint would be hit.
             */
            double spent = std::numeric_limits<double>::quiet_NaN();
            /** The amount (in multiples of the market's price Bundle) of unspent income.  Exactly
             * equal to .price - input_price.  Will be 0 when .constrained is false.
             */
            double unspent = std::numeric_limits<double>::quiet_NaN();
        };

        /** Contains a reservation of market purchase.  The market will consider the reserved
         * quantity unavailable until a call of either buy() (which completes the transfer) or
         * release() (which cancels the transfer) is called.
         *
         * If the object is destroyed before being passed to buy() or release(), release() will be
         * called automatically.
         *
         * This object is not intended to be used directly, but rather through the Reservation
         * unique_ptr typedef.
         */
        class Reservation final {
            private:
                friend class Market;

                Reservation(SharedMember<Market> market, SharedMember<agent::AssetAgent> agent, double quantity, double price);

                // Default/copy construction not allowed
                Reservation() = delete;
                Reservation(const Reservation&) = delete;

                // Move/copy assignment not allowed
                Reservation& operator=(Reservation &&move) = delete;
                Reservation& operator=(const Reservation &copy) = delete;

                std::forward_list<Firm::Reservation> firm_reservations_;

                Bundle b_;

            public:
                /// Move constructor
                Reservation(Reservation &&move) = default;
                /** Destructor.  If this Reservation is destroyed without having been completed or aborted
                 * (via buy() or release()), it will be aborted (by calling release() on its Market).
                 */
                ~Reservation();
                /// The state (pending, completed, or aborted) of this Reservation
                ReservationState state = ReservationState::pending;
                /// The quantity (as a multiple of the Market's output Bundle) that this reservation is for.
                const double quantity;
                /// The price (as a multiple of the Market's price Bundle) of this reservation.
                const double price;
                /// The market to which this Reservation applies.
                const SharedMember<Market> market;
                /// The agent for which this Reservation is being held.
                const SharedMember<agent::AssetAgent> agent;
                /** Reserves the given BundleNegative transfer from the given firm and stores the
                 * result, to be transferred if buy() is called, and aborted if release() is called.
                 * Positive amounts are to be transferred from the firm, negative amounts are to be
                 * transferred to the firm.  This is intended to be called only by Market subclasses.
                 */
                void firmReserve(eris_id_t firm_id, BundleNegative transfer);
                /** Calls buy() on the market.  Calling obj->buy() is a shortcut for calling
                 * `obj->market->buy(obj)`.
                 */
                void buy();
                /** Calls release() on the market.  This is equivalent to calling
                 * `obj->market->release(obj)`.
                 */
                void release();

                /** Exception class thrown if attempting to buy or release a Reservation that has
                 * already been bought or released.
                 */
                class non_pending_exception : public std::exception {
                    /// Overrides what() with an appropriate exception message
                    public: const char* what() const noexcept override;
                };
        };

        /** Returns the price information for buying the given multiple of the output bundle.
         * Returned is a price_info struct with .feasible set to true iff the quantity can be
         * produced in this market.  If feasible, .total, .marginal, and .marginalFirst are set to
         * the total price and marginal prices of the given quantity.  .marginal is the marginal
         * price of the last infinitesimal unit (which is not necessarily the same as the marginal
         * price for the *next* unit sold, as the quantity could just hit a threshold where marginal
         * price jumps).  .marginalFirst is the marginal price of the very first (fractional) unit
         * produced.  Essentially, marginalFirst should not depend on the quantity requested (but
         * often will depend on past market purchases), and won't change until some internal economy
         * state changes (such change need not occur in this market, however: transactions in other
         * markets may affect the participating firms of this market).
         *
         * Often .marginalFirst <= .marginal will hold, but it doesn't have to: a supplier could,
         * for example, have increasing returns to scale.
         *
         * A quantity of 0 may be provided, which is interpreted specially:
         * - .feasible will be true iff there is some positive quantity available
         * - if feasible, marginal and marginalFirst will be set to the marginal cost of the first
         *   available unit, and total will be set to 0.
         *
         * In all cases, if .feasible is false, the other values should not be used.
         */
        virtual price_info price(double q) const = 0;

        /** Returns the quantity that (in terms of multiples of the output Bundle) that p units of the
         * price Bundle will purchase.  The information is returned in a quantity_info struct, which
         * has two important fields:
         * - .quantity the quantity that can be bought for the given price
         * - .constrained will be true if the provided price would violate a supply constraint in
         *   this market.  In such a case .quantity is the constrained amount and .spend contains
         *   the actual amount spent (which will be less than the input amount).  Note that it is
         *   possible for .constrained to be false even when not all of p is spent: see below.
         * - .spent equals the multiple of the price Bundle required to buy .quantity units of the
         *   market's output Bundle.  This usually equals the input price when .constrained is false;
         *   when .constrained is true this will be a value less than the provided price.
         * - .unspent is the amount of p that would be leftover.  When .constrained is true, this is
         *   usually positive, and usually false otherwise.
         *
         * .spent and .unspent may differ from the description above in a market of discrete goods.
         * For example, a market setting integer increments of x for $2 each called with
         * quantity(13.8) should give {.quantity=6, .constrained=false, .spent=12, .unspent=1.8}.
         */
        virtual quantity_info quantity(double p) const = 0;

        /** Reserves q times the output Bundle for price at most p_max * price Bundle.  Removes the
         * purchase price (which could be less than p_max * price) from the assets() bundle of the
         * provided AssetAgent.  When the reservation is completed, the amount is transfered to the
         * firm; if aborted, the amount is returned to the AssetAgent's assets() Bundle.  Returns a
         * Reservation object that should be passed in to buy() to complete the transfer, or
         * release() to abort the sale.
         *
         * \param q the quantity to reserve, as a multiple of the market's output_unit.
         * \param agent a SharedMember<AssetAgent> (or AssetAgent subclass) from whose assets()
         * Bundle the payment will be taken.  The removed amount will be held until either buy() or
         * release() is called, at which point it will be transferred to the seller(s) or returned
         * to the AssetAgent, respectively.
         * \param p_max the maximum price to pay, as a multiple of the market's price_unit.
         * Optional; if omitted, defaults to infinity (i.e. no limit).
         *
         * This method is intended to be called in the optimize() method of an IntraOptimizer
         * instance, stored, and completed via buy() in the Optimizer's apply() method.
         *
         * The following shows the intended code design:
         *
         *     class ... : IntraOptimizer {
         *         public:
         *             void optimize() { ...; reservation = market->reserve(q, agent); }
         *             void apply() { ...; reservation->buy(); }
         *         private:
         *             Market::Reservation &reservation;
         *     }
         *
         * \throws Market::output_infeasible if q*output is not available in this market
         * \throws Market::low_price if q*output is available, but its cost would exceed p_max
         * \throws Market::insufficient_assets if q and p_max are acceptable, but the assets Bundle
         * doesn't contain the price of the output Bundle.  Note that this is not necessarily thrown
         * when assets are less than p_max*price: the actual transaction price could be low enough
         * that assets is sufficient.
         */
        virtual Reservation reserve(SharedMember<agent::AssetAgent> agent, double q, double p_max = std::numeric_limits<double>::infinity()) = 0;

        /** Completes a reservation made with reserve().  Transfers the reserved assets into the
         * assets() Bundle of the AssetAgent supplied when creating the reservation, and transfers
         * the reserved payment to the firm(s) supplying the output.
         *
         * The provided Reservation object is updated to record that it has been purchased.
         *
         * \throws Market::Reservation::non_pending_exception if the given reservation has already been completed or
         * cancelled.
         */
        virtual void buy(Reservation &res);

        /** Aborts a reservation made with reserve().
         */
        void release(Reservation &res);

    protected:
        /** Returns a SharedMember<Member> for the current object, via the simulation.
         */
        SharedMember<Member> sharedSelf() const override;

        /** Exposes access to Reservation.b_ to subclasses. */
        Bundle& reservationBundle_(Reservation &res);

    public:
        /** Adds f to the firms supplying in this market.  Subclasses that require a particular type
         * of firm should override this method, calling `requireInstanceOf<Base>(f, "...")` followed by
         * `Market::addFirm(f)`; see eris::market::Bertrand as an example.
         */
        virtual void addFirm(SharedMember<Firm> f);

        /** Removes f from the firms supplying in this market.  This is called automatically if a
         * firm passed to addFirm is removed from the simulation; manual calling is only needed if a
         * firm exits the market but stays in the simulation. */
        virtual void removeFirm(eris_id_t fid);

        /** Returns the std::unordered_set of the eris_id_t's of firms supplying this market.
         */
        virtual const std::unordered_set<eris_id_t>& firms();

        /** Exception class thrown when a quantity that exceeds the market capacity has been
         * requested.
         */
        class output_infeasible : public std::exception {
            /// Overrides what() with an appropriate exception message
            public: const char* what() const noexcept override;
        };
        /** Exception class throw when the market price required exceeds the maximum price
         * specified.
         */
        class low_price : public std::exception {
            /// Overrides what() with an appropriate exception message
            public: const char* what() const noexcept override;
        };
        /** Exception class thrown when the buyer's assets are not sufficient to purchase the
         * requested output.
         */
        class insufficient_assets : public std::exception {
            /// Overrides what() with an appropriate exception message
            public: const char* what() const noexcept override;
        };


        /** The base unit of this market's output; quantities produced/purchased are in terms of
         * multiples of this Bundle.  Subclasses are not required to make use of this bundle.
         */
        const Bundle output_unit;
        /** The base unit of this market's price; prices paid for output are in terms of multiples
         * of this Bundle.  Subclasses are not required to make use of this bundle.
         */
        const Bundle price_unit;

    protected:
        /// Firms participating in this market
        std::unordered_set<eris_id_t> suppliers_;

        /** Creates a Reservation and returns it.  For use by subclasses only.  The reserved payment
         * (i.e. p*price_unit) will be transferred out of the AssetAgent's assets() Bundle and held
         * in the Reservation until completed or cancelled.
         */
        Reservation createReservation(SharedMember<agent::AssetAgent> agent, double q, double p);

        /** Overridden to automatically remove a firm from the market when the firm is removed from
         * the simulation.
         */
        virtual void weakDepRemoved(SharedMember<Member>, eris_id_t old_id) override;

};



}
