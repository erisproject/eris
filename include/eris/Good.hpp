#pragma once
#include <eris/Member.hpp>
#include <string>

namespace eris {

/** Base class for Eris Good objects.  Goods should generally be a subclass of Good, typically
 * either a Good::Continuous or Good::Discrete.
 */
class Good : public Member {
    public:
        /** Returns the increment that this good exists as multiples of. */
        virtual double increment() = 0;

        /** The name of the good. */
        std::string name;

        class Continuous;
        class Discrete;

    protected:
        /// Superclass constructor that stores the good name
        Good(std::string name);

        SharedMember<Member> sharedSelf() const override { return simGood<Member>(id()); }
};

/** Continuous good.  This is a good with a fixed increment of 0, usable for any good which is
 * (quasi-)infinitely divisible.
 */
class Good::Continuous : public Good {
    public:
        /// Constructor; takes an optional name.
        Continuous(std::string name = "");
        /// Returns 0.0
        double increment();
};

/** Discrete good that should only exist as multiples of the given value.
 *
 * Note that this class is currently not actually implemented in much of eris; the increment value
 * will not be respected.
 */
class Good::Discrete : public Good {
    public:
        /// The default increment if not specified in the constructor
        static constexpr double default_increment = 1.0;
        /** Constructs a new discrete good with the given increment and name.  Increment must be
         * non-negative.
         */
        Discrete(double increment = default_increment, std::string name = "");
        /// Updates the increment of the good.  Increment must be >= 0.
        void setIncrement(double increment);
        /// Returns the current increment of the good.
        double increment();
    private:
        double incr_ = default_increment;
};


}
