#pragma once
#include <cstdint>
#include <type_traits>

/** \file eris/types.hpp basic types
 *
 * This header includes the basic typedefs for eris_id_t and eris_time_t.
 */

namespace eris {
/** Integer type that stores a unique id for each Member (Agent, Market, or Good) in an Eris
 * simulation.  Member instances (and their SharedMember wrappers) can be used directly anywhere
 * an eris_id_t is called for.
 *
 * Special properties of eris_id_t values:
 * - Assigned eris_id_t values are always strictly positive.  The underlying type is currently
 *   unsigned, but that could change in a future implementation.
 * - An id of 0 indicates a Member that has not been added to a Simulation or has been removed
 *   from a simulation.
 * - An id is assigned to a Member when it is added to the Simulation, *not* when it is created.
 * - The eris_id_t value assigned to a Member object is unique for that Simulation object: no
 *   two members will have the same ID.
 *   - This applies across different Member types; e.g. a Good and a Market will always have
 *     distinct eris_id_t values
 * - eris_id_t values are not reused, even if Member objects have been removed from the
 *   Simulation.
 * - ids are *currently* allocated sequentially, starting at 1.  This behaviour is not
 *   guaranteed.
 */
using eris_id_t = uint64_t;

/** Signed integer type that stores an eris time period.  This is a signed type that can also be
 * used for time period deltas. */
using eris_time_t = int32_t;

/** std::size_t alias primarily for internal eris use. */
using size_t = std::size_t;

/** Simple class used for methods that need to accept an eris_id_t but wants to allow any of an
 * eris_id_t, Member, Member pointer, or a SharedMember<T> to be provided (more precisely, anything
 * with either a `.id()` or `->id()` that returns an eris_id_t).  Wherever this is accepted you can
 * pass the actual id, a Member-derived object, or a SharedMember<T>.  This object is typically not
 * constructed explicitly but instead simply provides intermediate conversion between members and
 * associated eris_id_t values.
 */
struct MemberID {
private:
    eris_id_t id;
public:
    /// Not default constructible
    MemberID() = delete;
    /// Implicit construction from a raw eris_id_t
    MemberID(eris_id_t id) : id{id} {}
    /// Implicit construction from anything with an `id()` method that returns an eris_id_t, most
    /// notably Member (and derived)
    template <typename T, std::enable_if_t<std::is_same<
        decltype(std::declval<const T>().id()), eris_id_t>::value, int> = 0>
    MemberID(const T &member) : id{member.id()} {}
    /// Implicit construction from anything with a `->id()` indirect method that returns an
    /// eris_id_t, most notably SharedMember<T> and Member pointer
    template <typename T, std::enable_if_t<std::is_same<
        decltype(std::declval<const T>()->id()), eris_id_t>::value, int> = 0>
    MemberID(const T &shared_member) : id{shared_member->id()} {}

    /// Implicit conversion to an eris_id_t
    operator eris_id_t() const { return id; }
};

}
