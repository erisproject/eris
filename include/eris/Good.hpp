#pragma once
#include <eris/Member.hpp>
#include <string>

namespace eris {

/** Base class for Eris Good objects.  Goods should generally be a subclass of Good, typically
 * either a Good::Continuous or Good::Discrete.
 */
class Good : public Member {
    public:
        /// Deleted empty constructor; use Good::Discrete or Good::Continuous instead.
        Good() = delete;

        /** The name of the good. */
        std::string name;

        class Continuous;
        class Discrete;

    protected:
        /// Superclass constructor that stores the good name
        Good(std::string name);

        SharedMember<Member> sharedSelf() const override;
};

/** Continuous good.  This is a good that is (quasi-)infinitely divisible.
 */
class Good::Continuous : public Good {
    public:
        /// Constructor; takes an optional name.
        Continuous(std::string name = "");
};

/** Discrete good that may only take on integer quantities.
 *
 * Note that this class is currently not actually implemented in much of eris; be sure to only use
 * this with classes that explicitly support discrete goods.
 */
class Good::Discrete : public Good {
    public:
        /// Constructor; takes an optional name.
        Discrete(std::string name = "");
};


}
