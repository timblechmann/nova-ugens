/*
 *
 *    Copyright (C) 2012 Tim Blechmann
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "SC_PlugIn.hpp"
#include <cmath>

#include <boost/config.hpp>

#include "nova-simd/vec.hpp"
#include "nova-simd/detail/define_macros.hpp"
#include "nova-simd/detail/wrap_arguments.hpp"

struct tanh_shaper
{
    template<typename FloatType>
    FloatType operator()(FloatType signal, FloatType pregain, FloatType scale) const
    {
        return std::tanh(signal * pregain) * scale;
    }

    template<typename FloatType>
    nova::vec<FloatType> operator()(nova::vec<FloatType> signal, nova::vec<FloatType> pregain, nova::vec<FloatType> scale) const
    {
        return tanh(signal * pregain) * scale;
    }
};

typedef nova::detail::ternary_functor<tanh_shaper> tanh_shaper_functor;


struct fast_tanh_shaper
{
    template<typename FloatType>
    static FloatType fast_tanh_approximation(FloatType arg)
    {
        FloatType ax = std::abs(arg);
        FloatType one = 1.0;

        FloatType sign = (arg > 0) ? 1.f : -1.f;

        FloatType x2 = arg * arg;
        FloatType a = 0.66422417311781;
        FloatType b = 0.36483285408241;

        FloatType ret = sign * (one - one / (one + ax + x2 + a * x2*ax + b * x2 * x2));

        return ret;
    }

    template<typename FloatType>
    static nova::vec<FloatType> fast_tanh_approximation(nova::vec<FloatType> arg)
    {
        typedef nova::vec<FloatType> vec;
        vec ax = abs(arg);
        vec one = vec::gen_one();

        vec sgn = sign(arg);

        vec x2 = arg * arg;
        vec a = 0.66422417311781;
        vec b = 0.36483285408241;

        vec ret = sgn * (one - reciprocal(one + ax + x2 + a * x2*ax + b * x2 * x2));

        return ret;
    }


    template<typename FloatType>
    FloatType operator()(FloatType signal, FloatType pregain, FloatType scale) const
    {
        return fast_tanh_approximation(signal * pregain) * scale;
    }

    template<typename FloatType>
    nova::vec<FloatType> operator()(nova::vec<FloatType> signal, nova::vec<FloatType> pregain, nova::vec<FloatType> scale) const
    {
        return fast_tanh_approximation(signal * pregain) * scale;
    }
};

typedef nova::detail::ternary_functor<fast_tanh_shaper> fast_tanh_shaper_functor;


namespace {

template <int index>
struct ControlRateInput
{
    float value;

    void init(const SCUnit * parent)
    {
        value = parent->in0(index);
    }

    bool changed(const SCUnit * parent) const
    {
        return value != parent->in0(index);
    }

    operator float(void)
    {
        return value;
    }
};



class NovaTanhShaper:
    public SCUnit
{
    typedef nova::vec<float> vec;

    ControlRateInput<1> pregain;
    float scale;

    static float calc_scale(float pregain)
    {
        return 1.f / std::tanh(pregain);
    }

    bool canUseSIMD (void) const
    {
        return (mBufLength & (nova::vec< float >::objects_per_cacheline - 1)) == 0;
    }

    template <typename UnitType, void (UnitType::*VectorCalcFunc)(int), void (UnitType::*ScalarCalcFunc)(int)>
    void set_vector_calc_function(void)
    {
        if (canUseSIMD())
            SCUnit::set_vector_calc_function<UnitType, VectorCalcFunc, ScalarCalcFunc>();
        else
            SCUnit::set_calc_function<UnitType, ScalarCalcFunc>();
    }

public:
    NovaTanhShaper(void)
    {
        pregain.init(this);
        scale = calc_scale(pregain);
        if (mCalcRate != calc_FullRate) {
            set_calc_function<NovaTanhShaper, &NovaTanhShaper::next_k<false> >();
            return;
        }

        set_vector_calc_function<NovaTanhShaper, &NovaTanhShaper::next_k<true>, &NovaTanhShaper::next_k<false> >();
    }

    template <bool simd>
    BOOST_FORCEINLINE
    void next_k(int inNumSamples)
    {
        const float * in_ptr = in(0);
        float * out_ptr = out(0);
        if (pregain.changed(this)) {
            float old_scale = scale;
            float old_pregain = pregain;
            pregain.init(this);
            scale = calc_scale(pregain);
            float scale_slope = calcSlope(scale, old_scale);
            float pregain_slope = calcSlope((float)pregain, old_pregain);
            if (simd)
                tanh_shaper_functor::perform_vec_simd(out_ptr, in_ptr, nova::slope_argument(old_pregain, pregain_slope),
                                     nova::slope_argument(old_scale, scale_slope), inNumSamples);
            else
                tanh_shaper_functor::perform_vec(out_ptr, in_ptr, pregain, scale, inNumSamples);
        } else {
            if (simd)
                tanh_shaper_functor::perform_vec_simd(out_ptr, in_ptr, pregain, scale, inNumSamples);
            else
                tanh_shaper_functor::perform_vec(out_ptr, in_ptr, pregain, scale, inNumSamples);
        }
    }

    template <bool simd>
    BOOST_FORCEINLINE
    void next_i(int inNumSamples)
    {
        const float * in_ptr = in(0);
        float * out_ptr = out(0);
        if (simd)
            tanh_shaper_functor::perform_vec_simd(out_ptr, in_ptr, pregain, scale, inNumSamples);
        else
            tanh_shaper_functor::perform_vec(out_ptr, in_ptr, pregain, scale, inNumSamples);
    }

};

DEFINE_XTORS(NovaTanhShaper)

class NovaFastTanhShaper:
    public SCUnit
{
    typedef nova::vec<float> vec;

    ControlRateInput<1> pregain;
    float scale;

    static float calc_scale(float pregain)
    {
        return 1.f / fast_tanh_shaper::fast_tanh_approximation(pregain);
    }

    bool canUseSIMD (void) const
    {
        return (mBufLength & (nova::vec< float >::objects_per_cacheline - 1)) == 0;
    }

    template <typename UnitType, void (UnitType::*VectorCalcFunc)(int), void (UnitType::*ScalarCalcFunc)(int)>
    void set_vector_calc_function(void)
    {
        if (canUseSIMD())
            SCUnit::set_vector_calc_function<UnitType, VectorCalcFunc, ScalarCalcFunc>();
        else
            SCUnit::set_calc_function<UnitType, ScalarCalcFunc>();
    }

public:
    NovaFastTanhShaper(void)
    {
        pregain.init(this);
        scale = calc_scale(pregain);
        if (mCalcRate != calc_FullRate) {
            set_calc_function<NovaFastTanhShaper, &NovaFastTanhShaper::next_k<false> >();
            return;
        }

        set_vector_calc_function<NovaFastTanhShaper, &NovaFastTanhShaper::next_k<true>, &NovaFastTanhShaper::next_k<false> >();
    }

    template <bool simd>
    BOOST_FORCEINLINE
    void next_k(int inNumSamples)
    {
        const float * in_ptr = in(0);
        float * out_ptr = out(0);
        if (pregain.changed(this)) {
            float old_scale = scale;
            float old_pregain = pregain;
            pregain.init(this);
            scale = calc_scale(pregain);
            float scale_slope = calcSlope(scale, old_scale);
            float pregain_slope = calcSlope((float)pregain, old_pregain);
            if (simd)
                fast_tanh_shaper_functor::perform_vec_simd(out_ptr, in_ptr, nova::slope_argument(old_pregain, pregain_slope),
                                                           nova::slope_argument(old_scale, scale_slope), inNumSamples);
            else
                fast_tanh_shaper_functor::perform_vec(out_ptr, in_ptr, pregain, scale, inNumSamples);
        } else {
            if (simd)
                fast_tanh_shaper_functor::perform_vec_simd(out_ptr, in_ptr, pregain, scale, inNumSamples);
            else
                fast_tanh_shaper_functor::perform_vec(out_ptr, in_ptr, pregain, scale, inNumSamples);
        }
    }

    template <bool simd>
    BOOST_FORCEINLINE
    void next_i(int inNumSamples)
    {
        const float * in_ptr = in(0);
        float * out_ptr = out(0);
        if (simd)
            fast_tanh_shaper_functor::perform_vec_simd(out_ptr, in_ptr, pregain, scale, inNumSamples);
        else
            fast_tanh_shaper_functor::perform_vec(out_ptr, in_ptr, pregain, scale, inNumSamples);
    }

};

DEFINE_XTORS(NovaFastTanhShaper)


}

InterfaceTable * ft;

PluginLoad(NovaTanhShaper)
{
    ft = inTable;
    DefineSimpleUnit(NovaTanhShaper);
    DefineSimpleUnit(NovaFastTanhShaper);
}
