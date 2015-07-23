#pragma once
#include <cstdint>
#include <functional>

/** \file eris/types.hpp basic type and class forward declarations
 *
 * This header includes some basic types such as eris_id_t and RunStage and ReservationState enums,
 * and forward-declares various top-level eris types (so as to avoid nested header problems).
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
typedef uint64_t eris_id_t;

/** Integer type that stores an eris time period. */
typedef uint32_t eris_time_t;

/** enum of the different stages of the simulation, primarily used for synchronizing threads.
 *
 * (The explicit ': int' is here because the stages are used internally as indicies in
 * Simulation.hpp)
 */
enum class RunStage : int {
    idle, // between-period/initial thread state
    kill, // When a thread sees this, it checks thr_kill_, and if it is the current thread id, it finishes.
    kill_all, // When a thread sees this, it finishes.
    // Inter-period optimization stages:
    inter_Begin, inter_Optimize, inter_Apply, inter_Advance,
    // Intra-period optimization stages:
    intra_Initialize, intra_Reset, intra_Optimize, intra_Reoptimize, intra_Apply, intra_Finish
};

/// The highest RunStage value
const RunStage RunStage_LAST = RunStage::intra_Finish;

/** enum of reservation states for market-level and firm-level reservations.  A reservation can
 * either be pending, complete, or aborted.
 */
enum class ReservationState { pending, complete, aborted };

}
