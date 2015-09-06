/*
    utilities for nova ugens
    Copyright (C) 2010 Tim Blechmann

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "SC_Unit.h"

#include "nova-dsp/muladd_helpers.hpp"

struct muladd_ugen:
    public Unit
{
    float mul, add;
};

/** defines ugen function wrappers and selector, should be used within a ugen class
 *
 *  CLASS_NAME: name of the ugen class, should be derived from muladd_ugen
 *  FUNCTION_NAME: a member function of the ugen class, should take a muladd helper as second argument
 *  INDEX: INDEX of the `mul' input. the `add' input is expected to be at INDEX+1
 *
 */
#define DEFINE_UGEN_FUNCTION_WRAPPER(CLASS_NAME, FUNCTION_NAME, INDEX)  \
    static void s_##FUNCTION_NAME##_nop(CLASS_NAME * unit, int num_samples) \
    {                                                                   \
        detail::muladd_helper_nop<float> ma;                            \
        unit->FUNCTION_NAME(num_samples, ma);                                  \
    }                                                                   \
                                                                        \
    static void s_##FUNCTION_NAME##_mul_i(CLASS_NAME * unit, int num_samples) \
    {                                                                   \
        detail::muladd_helper_mul_c<float> ma(IN0(INDEX));              \
        unit->FUNCTION_NAME(num_samples, ma);                                  \
    }                                                                   \
                                                                        \
    static void s_##FUNCTION_NAME##_mul_k(CLASS_NAME * unit, int num_samples) \
    {                                                                   \
        float mul = IN0(INDEX);                                         \
        if (mul == unit->mul) {                                         \
            s_##FUNCTION_NAME##_mul_i(unit, num_samples);               \
        } else {                                                        \
            float slope = CALCSLOPE(mul, unit->mul);                    \
            detail::muladd_helper_mul_l<float> ma(unit->mul, slope);    \
            unit->mul = mul;                                            \
            unit->FUNCTION_NAME(num_samples, ma);                              \
        }                                                               \
    }                                                                   \
                                                                        \
    static void s_##FUNCTION_NAME##_mul_a(CLASS_NAME * unit, int num_samples) \
    {                                                                   \
        detail::muladd_helper_mul_v<float> ma(IN(INDEX));               \
        unit->FUNCTION_NAME(num_samples, ma);                                  \
    }                                                                   \
                                                                        \
    static void s_##FUNCTION_NAME##_add_i(CLASS_NAME * unit, int num_samples) \
    {                                                                   \
        detail::muladd_helper_add_c<float> ma(IN0(INDEX+1));            \
        unit->FUNCTION_NAME(num_samples, ma);                                  \
    }                                                                   \
                                                                        \
    static void s_##FUNCTION_NAME##_add_k(CLASS_NAME * unit, int num_samples) \
    {                                                                   \
        float add = IN0(INDEX+1);                                       \
        if (add == unit->add) {                                         \
            s_##FUNCTION_NAME##_add_i(unit, num_samples);               \
        } else {                                                        \
            float slope = CALCSLOPE(add, unit->add);                    \
            detail::muladd_helper_add_l<float> ma(unit->add, slope);    \
            unit->add = add;                                            \
            unit->FUNCTION_NAME(num_samples, ma);                              \
        }                                                               \
    }                                                                   \
                                                                        \
    static void s_##FUNCTION_NAME##_add_a(CLASS_NAME * unit, int num_samples) \
    {                                                                   \
        detail::muladd_helper_add_v<float> ma(IN(INDEX+1));             \
        unit->FUNCTION_NAME(num_samples, ma);                                  \
    }                                                                   \
                                                                        \
                                                                        \
    static void s_##FUNCTION_NAME##_mul_i_add_i(CLASS_NAME * unit, int num_samples) \
    {                                                                   \
        detail::muladd_helper_mul_c_add_c<float> ma(IN0(INDEX), IN0(INDEX+1)); \
        unit->FUNCTION_NAME(num_samples, ma);                                  \
    }                                                                   \
                                                                        \
    static void s_##FUNCTION_NAME##_mul_i_add_k(CLASS_NAME * unit, int num_samples) \
    {                                                                   \
        float add = IN0(INDEX+1);                                       \
        if (add == unit->add) {                                         \
            s_##FUNCTION_NAME##_mul_i_add_i(unit, num_samples);         \
        } else {                                                        \
            float slope = CALCSLOPE(add, unit->add);                    \
            detail::muladd_helper_mul_c_add_l<float> ma(IN0(INDEX), unit->add, slope); \
            unit->add = add;                                            \
            unit->FUNCTION_NAME(num_samples, ma);                              \
        }                                                               \
    }                                                                   \
                                                                        \
    static void s_##FUNCTION_NAME##_mul_k_add_i(CLASS_NAME * unit, int num_samples) \
    {                                                                   \
        float mul = IN0(INDEX);                                         \
        if (mul == unit->mul) {                                         \
            s_##FUNCTION_NAME##_mul_i_add_i(unit, num_samples);         \
        } else {                                                        \
            float slope = CALCSLOPE(mul, unit->mul);                    \
            detail::muladd_helper_mul_l_add_c<float> ma(unit->mul, slope, IN0(INDEX+1)); \
            unit->mul = mul;                                            \
            unit->FUNCTION_NAME(num_samples, ma);                              \
        }                                                               \
    }                                                                   \
                                                                        \
    static void s_##FUNCTION_NAME##_mul_k_add_k(CLASS_NAME * unit, int num_samples) \
    {                                                                   \
        float mul = IN0(INDEX);                                         \
        float add = IN0(INDEX+1);                                       \
        if (mul == unit->mul) {                                         \
            s_##FUNCTION_NAME##_mul_i_add_k(unit, num_samples);         \
        } else {                                                        \
            float mul_slope = CALCSLOPE(mul, unit->mul);                \
            if (add == unit->add) {                                     \
                detail::muladd_helper_mul_l_add_c<float> ma(unit->mul, mul_slope, add); \
                unit->mul = mul;                                        \
                unit->FUNCTION_NAME(num_samples, ma);                          \
            }                                                           \
            else                                                        \
            {                                                           \
                float add_slope = CALCSLOPE(add, unit->add);            \
                detail::muladd_helper_mul_l_add_l<float> ma(unit->mul, mul_slope, unit->add, add_slope); \
                unit->mul = mul;                                        \
                unit->add = add;                                        \
                unit->FUNCTION_NAME(num_samples, ma);                          \
            }                                                           \
        }                                                               \
    }                                                                   \
                                                                        \
    static void s_##FUNCTION_NAME##_mul_i_add_a(CLASS_NAME * unit, int num_samples) \
    {                                                                   \
        detail::muladd_helper_mul_c_add_v<float> ma(IN0(INDEX), IN(INDEX+1)); \
        unit->FUNCTION_NAME(num_samples, ma);                                  \
    }                                                                   \
                                                                        \
    static void s_##FUNCTION_NAME##_mul_a_add_i(CLASS_NAME * unit, int num_samples) \
    {                                                                   \
        detail::muladd_helper_mul_v_add_c<float> ma(IN(INDEX), IN0(INDEX+1)); \
        unit->FUNCTION_NAME(num_samples, ma);                                  \
    }                                                                   \
                                                                        \
    static void s_##FUNCTION_NAME##_mul_a_add_k(CLASS_NAME * unit, int num_samples) \
    {                                                                   \
        float add = IN0(INDEX+1);                                       \
        if (add == unit->add) {                                         \
            s_##FUNCTION_NAME##_mul_a_add_i(unit, num_samples);         \
        } else {                                                        \
            float slope = CALCSLOPE(add, unit->add);                    \
            detail::muladd_helper_mul_v_add_l<float> ma(IN(INDEX), unit->add, slope); \
            unit->add = add;                                            \
            unit->FUNCTION_NAME(num_samples, ma);                              \
        }                                                               \
    }                                                                   \
                                                                        \
    static void s_##FUNCTION_NAME##_mul_k_add_a(CLASS_NAME * unit, int num_samples) \
    {                                                                   \
        float mul = IN0(INDEX);                                         \
        if (mul == unit->mul) {                                         \
            s_##FUNCTION_NAME##_mul_i_add_a(unit, num_samples);         \
        } else {                                                        \
            float slope = CALCSLOPE(mul, unit->mul);                    \
            detail::muladd_helper_mul_l_add_v<float> ma(unit->mul, slope, IN(INDEX+1)); \
            unit->mul = mul;                                            \
            unit->FUNCTION_NAME(num_samples, ma);                              \
        }                                                               \
    }                                                                   \
                                                                        \
    static void s_##FUNCTION_NAME##_mul_a_add_a(CLASS_NAME * unit, int num_samples) \
    {                                                                   \
        detail::muladd_helper_mul_v_add_v<float> ma(IN(INDEX), IN(INDEX+1)); \
        unit->FUNCTION_NAME(num_samples, ma);                                  \
    }                                                                   \
                                                                        \
static UnitCalcFunc select_calcfunc(const CLASS_NAME * unit)            \
{                                                                       \
    switch(INRATE(INDEX))                                               \
    {                                                                   \
    case calc_ScalarRate:                                               \
        switch(INRATE(INDEX+1))                                         \
        {                                                               \
        case calc_ScalarRate:                                           \
            if ((IN0(INDEX) == 1.f) && (IN0(INDEX+1) == 0.f))           \
                return (UnitCalcFunc)(s_##FUNCTION_NAME##_nop);         \
            if (IN0(INDEX) == 1.f)                                      \
                return (UnitCalcFunc)(s_##FUNCTION_NAME##_add_i);       \
            if (IN0(INDEX+1) == 0.f)                                    \
                return (UnitCalcFunc)(s_##FUNCTION_NAME##_mul_i);       \
            return (UnitCalcFunc)(s_##FUNCTION_NAME##_mul_i_add_i);     \
                                                                        \
        case calc_BufRate:                                              \
            if (IN0(INDEX) == 1.f)                                      \
                return (UnitCalcFunc)(s_##FUNCTION_NAME##_add_k);       \
            else                                                        \
                return (UnitCalcFunc)(s_##FUNCTION_NAME##_mul_i_add_k); \
                                                                        \
        case calc_FullRate:                                             \
            if (IN0(INDEX) == 1.f)                                      \
                return (UnitCalcFunc)(s_##FUNCTION_NAME##_add_a);       \
            else                                                        \
                return (UnitCalcFunc)(s_##FUNCTION_NAME##_mul_i_add_a); \
                                                                        \
        default:                                                        \
            assert(false);                                              \
        }                                                               \
                                                                        \
    case calc_BufRate:                                                  \
        switch(INRATE(INDEX+1))                                         \
        {                                                               \
        case calc_ScalarRate:                                           \
            if (IN0(INDEX+1) == 0.f)                                    \
                return (UnitCalcFunc)(s_##FUNCTION_NAME##_mul_k);       \
            else                                                        \
                return (UnitCalcFunc)(s_##FUNCTION_NAME##_mul_k_add_i); \
                                                                        \
        case calc_BufRate:                                              \
            return (UnitCalcFunc)(s_##FUNCTION_NAME##_mul_k_add_k);     \
                                                                        \
        case calc_FullRate:                                             \
            return (UnitCalcFunc)(s_##FUNCTION_NAME##_mul_k_add_a);     \
                                                                        \
        default:                                                        \
            assert(false);                                              \
        }                                                               \
                                                                        \
    case calc_FullRate:                                                 \
        switch(INRATE(INDEX+1))                                         \
        {                                                               \
        case calc_ScalarRate:                                           \
            if (IN0(INDEX+1) == 0.f)                                    \
                return (UnitCalcFunc)(s_##FUNCTION_NAME##_mul_a);       \
            else                                                        \
                return (UnitCalcFunc)(s_##FUNCTION_NAME##_mul_a_add_i); \
                                                                        \
        case calc_BufRate:                                              \
            return (UnitCalcFunc)(s_##FUNCTION_NAME##_mul_a_add_k);     \
                                                                        \
        case calc_FullRate:                                             \
            return (UnitCalcFunc)(s_##FUNCTION_NAME##_mul_a_add_a);     \
                                                                        \
        default:                                                        \
            assert(false);                                              \
        }                                                               \
                                                                        \
    default:                                                            \
        assert(false);                                                  \
    }                                                                   \
}

/* define supercollider-style constructor and destructor functions */
#define DEFINE_XTORS(CLASSNAME)         \
void CLASSNAME##_Ctor(CLASSNAME * unit) \
{                                       \
    new(unit) CLASSNAME();              \
}                                       \
                                        \
void CLASSNAME##_Dtor(CLASSNAME * unit) \
{                                       \
    unit->~CLASSNAME();                 \
}
