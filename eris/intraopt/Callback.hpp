#pragma once
/** \file
 * This file contains simple classes that wrap a function (or function-like object) into an
 * intra-period optimization object.
 *
 * For example:
 *     simulation->spawn<intraopt::ApplyCallback>([] { ... code here ...});
 */
#include <eris/Optimize.hpp>
#include <eris/Member.hpp>
#include <functional>

namespace eris { namespace intraopt {

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

/** Simple intraopt::Initialize implementation that invokes a callback when invoked. */
class InitializeCallback final : public CallbackBase<>, public Initialize {
public:
    using CallbackBase::CallbackBase;
    /// Invokes the callback when called.
    void intraInitialize() override { callback_(); }
    /// Returns the callback priority, as given in in the constructor
    double intraInitializePriority() const override { return priority_; }
};

/** Simple intraopt::Reset implementation that invokes a callback when invoked. */
class ResetCallback final : public CallbackBase<>, public Reset {
public:
    using CallbackBase::CallbackBase;
    /// Invokes the callback when called.
    void intraReset() override { callback_(); }
    /// Returns the callback priority, as given in the constructor
    double intraResetPriority() const override { return priority_; }
};

/** Simple intraopt::Optimize implementation that invokes a callback when invoked. */
class OptimizeCallback final : public CallbackBase<>, public Optimize {
public:
    using CallbackBase::CallbackBase;
    /// Invokes the callback when called.
    void intraOptimize() override { callback_(); }
    /// Returns the callback priority, as given in the constructor
    double intraOptimizePriority() const override { return priority_; }
};

/** Simple intraopt::Reoptimize implementation that invokes a callback when invoked. */
class ReoptimizeCallback final : public CallbackBase<bool>, public Reoptimize {
public:
    using CallbackBase::CallbackBase;
    /// Invokes the callback when called.
    bool intraReoptimize() override { return callback_(); }
    /// Returns the callback priority, as given in the constructor
    double intraReoptimizePriority() const override { return priority_; }
};

/** Simple intraopt::Apply implementation that invokes a callback when invoked. */
class ApplyCallback final : public CallbackBase<>, public Apply {
public:
    using CallbackBase::CallbackBase;
    /// Invokes the callback when called.
    void intraApply() override { callback_(); }
    /// Returns the callback priority, as given in the constructor
    double intraApplyPriority() const override { return priority_; }
};

/** Simple intraopt::Finish implementation that invokes a callback when invoked. */
class FinishCallback final : public CallbackBase<>, public Finish {
public:
    using CallbackBase::CallbackBase;
    /// Invokes the callback when called.
    void intraFinish() override { callback_(); }
    /// Returns the callback priority, as given in the constructor
    double intraFinishPriority() const override { return priority_; }
};

}}
