/*
    nova filters for supercollider
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

#include "SC_PlugIn.hpp"

#include "nova-dsp/mitra_regalia_filters.hpp"

#include "NovaMulAddUGenHelper.hpp"

using namespace nova;

extern "C" void load(InterfaceTable *inTable);

namespace
{
InterfaceTable *ft;

typedef float sample;

#ifndef RESTRICTED_POINTERS
#define __restrict__ /*__restrict__*/
#endif /* RESTRICTED_POINTERS */

#ifdef ALIGNED_POINTERS
typedef sample aligned_sample __attribute__((aligned(4 * sizeof(sample))));
#else
typedef sample aligned_sample;
#endif /* ALIGNED_POINTERS */

typedef aligned_sample *__restrict__ restricted_sample_ptr;
typedef const aligned_sample *__restrict__ const_restricted_sample_ptr;

typedef aligned_sample * aligned_sample_ptr;
typedef const aligned_sample * const_aligned_sample_ptr;

template <typename filter_type>
class nova_shelf:
    public muladd_ugen
{
public:
    nova_shelf(void)
    {
        nova_shelf * unit = this;

        float freq = IN0(1);
        float factor = IN0(2);
        filter.set_frequency(freq * float(SAMPLEDUR));
        filter.set_factor(factor);

        mCalcFunc = select_calcfunc(this);
        (mCalcFunc)(this, 1);
    }

    filter_type filter;

    template <typename muladd_helper>
    inline void next_a(int num_samples, muladd_helper & ma)
    {
        nova_shelf * unit = this;

        float *out = OUT(0);
        float *in = IN(0);
        float freq = IN0(1);
        float factor = IN0(2);

        filter.reset_frequency(freq * float(SAMPLEDUR));
        filter.reset_factor(factor);
        filter.perform(in, out, num_samples, ma);
    }

    DEFINE_UGEN_FUNCTION_WRAPPER(nova_shelf, next_a, 3)
};

typedef nova_shelf<nova::mitra_regalia_low_shelf<float, float, false, true> > NovaLowshelf;
typedef nova_shelf<nova::mitra_regalia_high_shelf<float, float, false, true> > NovaHighshelf;


template <typename filter_type>
class nova_band_filter:
    public muladd_ugen
{
public:
    nova_band_filter(void)
    {
        nova_band_filter * unit = this;

        float freq = IN0(1);
        float bw = IN0(2);
        filter.set_frequency(freq * float(SAMPLEDUR));
        filter.set_bandwidth(bw);

        mCalcFunc = select_calcfunc(this);
        (mCalcFunc)(this, 1);
    }

    filter_type filter;

    template <typename muladd_helper>
    inline void next_a(int num_samples, muladd_helper & ma)
    {
        nova_band_filter * unit = this;

        float *out = OUT(0);
        float *in = IN(0);
        float freq = IN0(1);
        float bw = IN0(2);

        filter.reset_frequency(freq * float(SAMPLEDUR));
        filter.reset_bandwidth(bw);
        filter.perform(in, out, num_samples, ma);
    }

    DEFINE_UGEN_FUNCTION_WRAPPER(nova_band_filter, next_a, 3)
};

typedef nova_band_filter<nova::mitra_regalia_band_pass<float, float, false, true> > NovaBPF;
typedef nova_band_filter<nova::mitra_regalia_band_reject<float, float, false, true> > NovaBRF;


template <typename filter_type>
class nova_filter:
    public muladd_ugen
{
public:
    nova_filter(void)
    {
        nova_filter * unit = this;

        float freq = IN0(1);
        filter.set_frequency(freq * float(SAMPLEDUR));

        mCalcFunc = select_calcfunc(this);
        (mCalcFunc)(this, 1);
    }

    filter_type filter;

    template <typename muladd_helper>
    inline void next_a(int num_samples, muladd_helper & ma)
    {
        nova_filter * unit = this;

        float *out = OUT(0);
        float *in = IN(0);
        float freq = IN0(1);

        filter.reset_frequency(freq * float(SAMPLEDUR));
        filter.perform(in, out, num_samples, ma);
    }

    DEFINE_UGEN_FUNCTION_WRAPPER(nova_filter, next_a, 2)
};

typedef nova_filter<nova::mitra_regalia_high_pass<float, float, false, true> > NovaHPF;
typedef nova_filter<nova::mitra_regalia_low_pass<float, float, false, true> > NovaLPF;


template <typename filter_type>
class nova_eq:
    public muladd_ugen
{
public:
    nova_eq(void)
    {
        nova_eq * unit = this;

        float freq = IN0(1);
        float q = IN0(2);
        float factor = IN0(2);
        filter.set_frequency(freq * float(SAMPLEDUR));
        filter.set_bandwidth(q);
        filter.set_factor(factor);

        mCalcFunc = select_calcfunc(this);
        (mCalcFunc)(this, 1);
    }

    filter_type filter;

    template <typename muladd_helper>
    inline void next_a(int num_samples, muladd_helper & ma)
    {
        nova_eq * unit = this;

        float *out = OUT(0);
        const float *in = IN(0);
        float freq = IN0(1);
        float q = IN0(2);
        float factor = IN0(2);

        filter.reset_frequency(freq * float(SAMPLEDUR));
        filter.reset_bandwidth(q);
        filter.reset_factor(factor);
        filter.perform(in, out, num_samples, ma);
    }

    DEFINE_UGEN_FUNCTION_WRAPPER(nova_eq, next_a, 3)
};

typedef nova_filter<nova::mitra_regalia_high_pass<float, float, false, true> > NovaEQ;

} /* namespace */

PluginLoad(NovaRMFilters)
{
    ft = inTable;

    registerUnit< NovaLowshelf  >( ft, "NovaLowshelf"  );
    registerUnit< NovaHighshelf >( ft, "NovaHighshelf" );
    registerUnit< NovaBPF       >( ft, "NovaBPF"       );
    registerUnit< NovaBRF       >( ft, "NovaBRF"       );
    registerUnit< NovaLPF       >( ft, "NovaLPF"       );
    registerUnit< NovaHPF       >( ft, "NovaHPF"       );
    registerUnit< NovaEQ        >( ft, "NovaEQ"        );
}

