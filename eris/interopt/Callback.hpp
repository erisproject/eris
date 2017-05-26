#pragma once
/** \file
 * This file contains simple classes that wrap a function (or function-like object) into an
 * inter-period optimization object.
 *
 * For example:
 *     simulation->spawn<interopt::ApplyCallback>([] { ... code here ...});
 */
#include <eris/Optimize.hpp>
#include <eris/Member.hpp>
#include <functional>

namespace eris { namespace interopt {

/** Base class for callbacks */
template <typename Return = void>
class CallbackBase : public Member {
public:
    /// Not default constructible
    CallbackBase() = delete;
    /// Constructs a callback from a function or callable object.  Takes an optional priority; if
    /// omitted, the optimizer runs at the default priority (0).
    explicit CallbackBase(std::function<Return()> func, double priority = 0.0)
        : callback_{std::move(func)}, priority_{priority} {}
protected:
    /// Stores the callback
    const std::function<Return()> callback_;
    /// Stores the priority
    const double priority_;
};

/** Simple interopt::Begin implementation that invokes a callback when invoked. */
class BeginCallback final : public CallbackBase<>, public Begin {
public:
    using CallbackBase::CallbackBase;
    /// Invokes the callback when called.
    void interBegin() override { callback_(); }
    /// Returns the callback priority, as given in the constructor
    double interBeginPriority() const override { return priority_; }
};

/** Simple interopt::Optimize implementation that invokes a callback when invoked. */
class OptimizeCallback final : public CallbackBase<>, public Optimize {
public:
    using CallbackBase::CallbackBase;
    /// Invokes the callback when called.
    void interOptimize() override { callback_(); }
    /// Returns the callback priority, as given in the constructor
    double interOptimizePriority() const override { return priority_; }
};

/** Simple interopt::Apply implementation that invokes a callback when invoked. */
class ApplyCallback final : public CallbackBase<>, public Apply {
public:
    using CallbackBase::CallbackBase;
    /// Invokes the callback when called.
    void interApply() override { callback_(); }
    /// Returns the callback priority, as given in the constructor
    double interApplyPriority() const override { return priority_; }
};

/** Simple interopt::Advance implementation that invokes a callback when invoked. */
class AdvanceCallback final : public CallbackBase<>, public Advance {
public:
    using CallbackBase::CallbackBase;
    /// Invokes the callback when called.
    void interAdvance() override { callback_(); }
    /// Returns the callback priority, as given in the constructor
    double interAdvancePriority() const override { return priority_; }
};

}}
