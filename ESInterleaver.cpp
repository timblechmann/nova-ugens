/*
 *
 *    Copyright (C) 2013 Tim Blechmann
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

static InterfaceTable *ft;

struct ES3Interleaver_96k:
    public SCUnit
{
public:
    ES3Interleaver_96k()
    {
        set_calc_function<ES3Interleaver_96k, &ES3Interleaver_96k::next>();
    }

private:
    void next(int inNumSamples)
    {
        const size_t outputChannels = 4;
        const bool esMode = in0(8);
        const size_t evenFrameOffset = esMode ? 1 : 0;
        const size_t oddFrameOffset  = esMode ? 0 : 1;

        for (int frame = 0; frame != inNumSamples; ++frame) {
            const bool frameIsEven = ((frame % 2) == 0);

            const size_t offset = (frameIsEven ^ esMode) ? 1 : 0;

            const float in0 = in(0 + offset)[frame];
            const float in1 = in(2 + offset)[frame];
            const float in2 = in(4 + offset)[frame];
            const float in3 = in(6 + offset)[frame];

            out(0)[frame] = in0;
            out(1)[frame] = in1;
            out(2)[frame] = in2;
            out(3)[frame] = in3;
        }
    }
};

DEFINE_XTORS(ES3Interleaver_96k)

PluginLoad(ES3Interleaver_96k)
{
    ft = inTable;
    DefineSimpleUnit(ES3Interleaver_96k);
}
