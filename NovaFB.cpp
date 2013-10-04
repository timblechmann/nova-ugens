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

#include "NovaUtils.hpp"

static InterfaceTable *ft;

struct NovaFBIn:
	public SCUnit
{
public:
	NovaFBIn()
	{
		mChannelCount = in0(0);

		size_t allocSize = mBufLength * sizeof(float) * mChannelCount;
		mBuff = (float*)RTAlloc(mWorld, allocSize );
		memset(mBuff, 0, allocSize);

		set_calc_function<NovaFBIn, &NovaFBIn::next_i>();
	}

	~NovaFBIn()
	{
		RTFree(mWorld, mBuff);
	}

private:
	void next_i(int inNumSamples)
	{
		nova::loop(mChannelCount, [&](int index) {
			const float * buf = mBuff + mBufLength * index;
			float * dest = out(index + 1);

			memcpy(dest, buf, inNumSamples * sizeof(float));
		});
	}

	friend class NovaFBOut;
	float * mBuff;
	int mChannelCount;
};

struct NovaFBOut:
	public SCUnit
{
public:
	NovaFBOut()
	{
		mChannelCount = in0(1);

		NovaFBIn * inputNode = static_cast<NovaFBIn*>(mInput[0]->mFromUnit);
		mBuff = inputNode->mBuff;

		set_calc_function<NovaFBOut, &NovaFBOut::next_i>();
	}

private:
	void next_i(int inNumSamples)
	{
		nova::loop(mChannelCount, [&](int index) {
			const float * source = in( 2 + index );
			float * buf = mBuff + mBufLength * index;

			memcpy(buf, source, inNumSamples * sizeof(float));
		});
	}

	float * mBuff;
	int mChannelCount;
};

DEFINE_XTORS(NovaFBOut)
DEFINE_XTORS(NovaFBIn)

PluginLoad(NovaFB)
{
	ft = inTable;
	DefineDtorUnit(NovaFBIn);
	DefineSimpleUnit(NovaFBOut);
}
