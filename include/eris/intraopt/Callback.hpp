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

/** Simple intraopt::Initialize implementation that invokes a callback when invoked. */
class InitializeCallback final : public Member, public Initialize {
    public:
        InitializeCallback() = delete;
        /** Constructs an InitializeCallback with a given callable target that will be invoked when intraInitialize() is called. */
        InitializeCallback(std::function<void()> callback);
        /** Invokes the callback when called. */
        virtual void intraInitialize() override;
    private:
        std::function<void()> callback_;
};

/** Simple intraopt::Reset implementation that invokes a callback when invoked. */
class ResetCallback final : public Member, public Reset {
    public:
        ResetCallback() = delete;
        /** Constructs an ResetCallback with a given callable target that will be invoked when intraReset() is called. */
        ResetCallback(std::function<void()> callback);
        /** Invokes the callback when called. */
        virtual void intraReset() override;
    private:
        std::function<void()> callback_;
};

/** Simple intraopt::Optimize implementation that invokes a callback when invoked. */
class OptimizeCallback final : public Member, public Optimize {
    public:
        OptimizeCallback() = delete;
        /** Constructs an OptimizeCallback with a given callable target that will be invoked when intraOptimize() is called. */
        OptimizeCallback(std::function<void()> callback);
        /** Invokes the callback when called. */
        virtual void intraOptimize() override;
    private:
        std::function<void()> callback_;
};

/** Simple intraopt::Reoptimize implementation that invokes a callback when invoked. */
class ReoptimizeCallback final : public Member, public Reoptimize {
    public:
        ReoptimizeCallback() = delete;
        /** Constructs an ReoptimizeCallback with a given callable target that will be invoked when intraReoptimize() is called. */
        ReoptimizeCallback(std::function<bool()> callback);
        /** Invokes the callback when called. */
        virtual bool intraReoptimize() override;
    private:
        std::function<bool()> callback_;
};

/** Simple intraopt::Apply implementation that invokes a callback when invoked. */
class ApplyCallback final : public Member, public Apply {
    public:
        ApplyCallback() = delete;
        /** Constructs an ApplyCallback with a given callable target that will be invoked when intraApply() is called. */
        ApplyCallback(std::function<void()> callback);
        /** Invokes the callback when called. */
        virtual void intraApply() override;
    private:
        std::function<void()> callback_;
};

/** Simple intraopt::Finish implementation that invokes a callback when invoked. */
class FinishCallback final : public Member, public Finish {
    public:
        FinishCallback() = delete;
        /** Constructs an FinishCallback with a given callable target that will be invoked when intraFinish() is called. */
        FinishCallback(std::function<void()> callback);
        /** Invokes the callback when called. */
        virtual void intraFinish() override;
    private:
        std::function<void()> callback_;
};

}}
