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

/** Simple interopt::Begin implementation that invokes a callback when invoked. */
class BeginCallback final : public Member, public Begin {
    public:
        BeginCallback() = delete;
        /** Constructs an BeginCallback with a given callable target that will be invoked when interBegin() is called. */
        BeginCallback(std::function<void()> callback);
        /** Invokes the callback when called. */
        virtual void interBegin() override;
    private:
        std::function<void()> callback_;
};

/** Simple interopt::Optimize implementation that invokes a callback when invoked. */
class OptimizeCallback final : public Member, public Optimize {
    public:
        OptimizeCallback() = delete;
        /** Constructs an OptimizeCallback with a given callable target that will be invoked when interOptimize() is called. */
        OptimizeCallback(std::function<void()> callback);
        /** Invokes the callback when called. */
        virtual void interOptimize() override;
    private:
        std::function<void()> callback_;
};

/** Simple interopt::Apply implementation that invokes a callback when invoked. */
class ApplyCallback final : public Member, public Apply {
    public:
        ApplyCallback() = delete;
        /** Constructs an ApplyCallback with a given callable target that will be invoked when interApply() is called. */
        ApplyCallback(std::function<void()> callback);
        /** Invokes the callback when called. */
        virtual void interApply() override;
    private:
        std::function<void()> callback_;
};

/** Simple interopt::Advance implementation that invokes a callback when invoked. */
class AdvanceCallback final : public Member, public Advance {
    public:
        AdvanceCallback() = delete;
        /** Constructs an AdvanceCallback with a given callable target that will be invoked when interAdvance() is called. */
        AdvanceCallback(std::function<void()> callback);
        /** Invokes the callback when called. */
        virtual void interAdvance() override;
    private:
        std::function<void()> callback_;
};

}}
