#pragma once
#include <eris/Member.hpp>
#include <string>

namespace eris {

/** Basic eris Good object.  A good is little more than a unique object with an optional name that
 * is used to notionally represent a distinct good.
 *
 * \sa Bundle
 */
class Good : public Member {
public:
    /// Constructor; takes an optional name.
    explicit Good(std::string name = "") : name{std::move(name)} {}

    /** The name of the good. */
    std::string name;

    /** Good::Continuous is a typedef for Good, provided for backwards compatibility.
     *
     * \deprecated in eris v0.5.0
     */
    [[deprecated("eris::Good::Continuous is deprecated; use eris::Good instead")]]
    typedef Good Continuous;

    /** Returns the smallest increment that this good should come in, if this good is discrete.  For
     * a continuous good (the default), this returns 0.
     *
     * Note that this increment is not actually enforced within eris: it is provided for convenience
     * for callers that which to explicitly handle discrete goods; any such handling is up to the
     * individual users.
     */
    virtual double atom() { return 0.0; }

protected:
    SharedMember<Member> sharedSelf() const override { return simGood(id()); }
};

}
