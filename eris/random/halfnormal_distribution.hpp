#pragma once
/* eris random/halfnormal_distribution.hpp header file
 */

#include <eris/random/normal_distribution.hpp>

namespace eris { namespace random {

/**
 * Instantiations of class template halfnormal_distribution model a
 * \random_distribution. Such a distribution produces random numbers
 * @c x distributed with probability density function
 * \f$\displaystyle p(x) =
 *   \frac{2}{\sqrt{2\pi}\sigma} e^{-\frac{(x-\mu)^2}{2\sigma^2}}
 * \f$ for \f$x \geq \mu\f$ and 0 otherwise, where \f$\mu\f$ and \f$\sigma\f$ are the
 * mu and sigma parameters of the distribution.  These parameters correspond to the mean and
 * standard deviation of the full normal distribution; they are not the mean and standard deviation
 * of the (truncated) halfnormal distribution.
 */
template<class RealType = double>
class halfnormal_distribution
{
public:
    typedef RealType input_type;
    typedef RealType result_type;

    class param_type {
    public:
        typedef halfnormal_distribution distribution_type;

        /**
         * Constructs a @c param_type with a given mu and sigma.
         *
         * Requires: sigma >= 0
         */
        explicit param_type(RealType mu_arg = RealType(0.0),
                            RealType sigma_arg = RealType(1.0))
          : _mu(mu_arg),
            _sigma(sigma_arg)
        {}

        /** Returns the mu parameter of the distribution, which is also the mode and minimum value
         * of the distribution.  If this were a full normal distribution, this would be the mean. */
        RealType mu() const { return _mu; }

        /** Returns the standand deviation of the distribution. */
        RealType sigma() const { return _sigma; }

        /** Writes a @c param_type to a @c std::ostream. */
        BOOST_RANDOM_DETAIL_OSTREAM_OPERATOR(os, param_type, parm)
        { os << parm._mu << " " << parm._sigma ; return os; }

        /** Reads a @c param_type from a @c std::istream. */
        BOOST_RANDOM_DETAIL_ISTREAM_OPERATOR(is, param_type, parm)
        { is >> parm._mu >> std::ws >> parm._sigma; return is; }

        /** Returns true if the two sets of parameters are the same. */
        BOOST_RANDOM_DETAIL_EQUALITY_OPERATOR(param_type, lhs, rhs)
        { return lhs._mu == rhs._mu && lhs._sigma == rhs._sigma; }
        
        /** Returns true if the two sets of parameters are the different. */
        BOOST_RANDOM_DETAIL_INEQUALITY_OPERATOR(param_type)

    private:
        RealType _mu;
        RealType _sigma;
    };

    /**
     * Constructs a @c halfnormal_distribution object. @c mu and @c sigma are
     * the parameters for the distribution.
     *
     * Requires: sigma >= 0
     */
    explicit halfnormal_distribution(const RealType& mu_arg = RealType(0.0),
                                 const RealType& sigma_arg = RealType(1.0))
      : _mu(mu_arg), _sigma(sigma_arg)
    {
        BOOST_ASSERT(_sigma >= RealType(0));
    }

    /**
     * Constructs a @c normal_distribution object from its parameters.
     */
    explicit halfnormal_distribution(const param_type& parm)
      : _mu(parm.mu()), _sigma(parm.sigma())
    {}

    /**  Returns the mu parameter of the distribution. */
    RealType mu() const { return _mu; }
    /** Returns the standard deviation of the distribution. */
    RealType sigma() const { return _sigma; }

    /** Returns the smallest value that the distribution can produce. */
    RealType min BOOST_PREVENT_MACRO_SUBSTITUTION () const
    { return -std::numeric_limits<RealType>::infinity(); }
    /** Returns the largest value that the distribution can produce. */
    RealType max BOOST_PREVENT_MACRO_SUBSTITUTION () const
    { return std::numeric_limits<RealType>::infinity(); }

    /** Returns the parameters of the distribution. */
    param_type param() const { return param_type(_mu, _sigma); }
    /** Sets the parameters of the distribution. */
    void param(const param_type& parm)
    {
        _mu = parm.mu();
        _sigma = parm.sigma();
    }

    /**
     * Effects: Subsequent uses of the distribution do not depend
     * on values produced by any engine prior to invoking reset.
     */
    void reset() { }

    /**  Returns a halfnormal variate. */
    template<class Engine>
    result_type operator()(Engine& eng)
    {
        detail::unit_normal_distribution<RealType, true> impl;
        return impl(eng) * _sigma + _mu;
    }

    /** Returns a halfnormal variate with parameters specified by @c param. */
    template<class URNG>
    result_type operator()(URNG& urng, const param_type& parm)
    {
        return halfnormal_distribution(parm)(urng);
    }

    /** Writes a @c halfnormal_distribution to a @c std::ostream. */
    BOOST_RANDOM_DETAIL_OSTREAM_OPERATOR(os, halfnormal_distribution, nd)
    {
        os << nd._mu << " " << nd._sigma;
        return os;
    }

    /** Reads a @c halfnormal_distribution from a @c std::istream. */
    BOOST_RANDOM_DETAIL_ISTREAM_OPERATOR(is, halfnormal_distribution, nd)
    {
        is >> std::ws >> nd._mu >> std::ws >> nd._sigma;
        return is;
    }

    /**
     * Returns true if the two instances of @c halfnormal_distribution will
     * return identical sequences of values given equal generators.
     */
    BOOST_RANDOM_DETAIL_EQUALITY_OPERATOR(halfnormal_distribution, lhs, rhs)
    {
        return lhs._mu == rhs._mu && lhs._sigma == rhs._sigma;
    }

    /**
     * Returns true if the two instances of @c halfnormal_distribution will
     * return different sequences of values given equal generators.
     */
    BOOST_RANDOM_DETAIL_INEQUALITY_OPERATOR(halfnormal_distribution)

private:
    RealType _mu, _sigma;

};


} // namespace random

} // namespace eris
