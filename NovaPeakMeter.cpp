/*
    efficient peak meters for supercollider
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

#include "nova-simd/simd_peakmeter.hpp"

static InterfaceTable *ft;

using namespace nova;

namespace
{

class PeakRMS:
    public Unit
{
    // rate, level lag, replyid, channel count, [channels ], cmd name size, [cmdname]
    static const int rateIndex = 0;
    static const int levelLagIndex = 1;
    static const int replyIdIndex = 2;
    static const int channelCountIndex = 3;
    static const int signalStartIndex = 4;


public:
    PeakRMS(void)
    {
        PeakRMS * unit = this;

        mChannelCount = (unsigned int)IN0(channelCountIndex);
        size_t channelDataAllocSize = mChannelCount * 3 * sizeof(float);

        int cmdSizeIndex = signalStartIndex + mChannelCount;
        size_t cmdNameSize = IN0(cmdSizeIndex);
        size_t cmdNameAllocSize = (cmdNameSize + 1) * sizeof(char);

        void * allocData = RTAlloc(unit->mWorld, channelDataAllocSize + cmdNameAllocSize);

        memset(allocData, 0, channelDataAllocSize);
        mChannelData = (float*)allocData;

        char * cmdName = (char*)(allocData) + channelDataAllocSize;

        size_t cmdNameIndex = cmdSizeIndex + 1;
        for(int i = 0; i < cmdNameSize; i++)
            cmdName[i] = (char)IN0(cmdNameIndex + i);
        cmdName[cmdNameSize] = 0;

        if ((FULLBUFLENGTH & 15) == 0) {
            if (mCalcRate == calc_FullRate)
                SETCALC(PeakRMS::perform_a<true>);
            else
                SETCALC(PeakRMS::perform_k<true>);
        }else {
            if (mCalcRate == calc_FullRate)
                SETCALC(PeakRMS::perform_a<false>);
            else
                SETCALC(PeakRMS::perform_k<false>);
        }

        float replyRate = IN0(rateIndex);

        mAudioSamplesPerTick   = FULLRATE / replyRate;
        mControlSamplesPerTick = BUFRATE  / replyRate;

        mPhaseRemain = (mCalcRate == calc_FullRate) ? mAudioSamplesPerTick
                                                    : mControlSamplesPerTick;

        float32 lag = ZIN0(levelLagIndex);
        mB1 = (lag != 0.f) ? exp(log001 / (lag * replyRate))
                           : 0.f;
    }

    ~PeakRMS (void)
    {
        PeakRMS * unit = this;
        RTFree(unit->mWorld, mChannelData);
    }

    unsigned int mChannelCount;
    float * mChannelData;

    float mB1;
    int mAudioSamplesPerTick;
    int mControlSamplesPerTick;
    int mPhaseRemain;

    void performLevelLag(float & out, float y0, float & y1)
    {
        if (y0 >= y1)
            out = y1 = y0;
        else
            out = y1 = y0 + mB1 * (y1 - y0);
    }

    char * getCmdName (void)
    {
        void * buffer = mChannelData;
        return (char*)(buffer) + mChannelCount * 3 * sizeof(float);
    }

    void sendReply(void)
    {
        PeakRMS * unit = this;
        float * reply = (float*)alloca(mChannelCount * 2 * sizeof(float));

        for (int i = 0; i != mChannelCount; ++i) {
            float & maxLevel = reply[2*i];
            float & rms = reply[2*i + 1];

            performLevelLag(maxLevel, mChannelData[2*i], mChannelData[2*mChannelCount + i]);

            float samplesPerTick;

            if (INRATE(signalStartIndex + i) == calc_FullRate)
                rms = std::sqrt(mChannelData[2*i + 1] / (float)mAudioSamplesPerTick);
            else
                rms = std::sqrt(mChannelData[2*i + 1] / (float)mControlSamplesPerTick);
        }

        SendNodeReply(&unit->mParent->mNode, (int)ZIN0(replyIdIndex), getCmdName(), mChannelCount*2, reply);
        memset(mChannelData, 0, mChannelCount * 2 * sizeof(float));
    }

    template <bool simd>
    void analyzeFullBlock(void)
    {
        PeakRMS * unit = this;
        for (int i = 0; i != mChannelCount; ++i) {
            float * in = IN(signalStartIndex + i);
            int numSamples = INBUFLENGTH(signalStartIndex + i);

            float & level = mChannelData[2*i];
            float & sqrsum = mChannelData[2*i + 1];
            if (numSamples == 1)
                nova::peak_rms_vec(in, &level, &sqrsum, 1);
            else {
                if (simd)
                    nova::peak_rms_vec_simd(in, &level, &sqrsum, numSamples);
                else
                    nova::peak_rms_vec(in, &level, &sqrsum, numSamples);
            }
        }
    }

    void analyzePartialBlock(int firstSample, int samplesToAnalyze)
    {
        PeakRMS * unit = this;
        for (int i = 0; i != mChannelCount; ++i) {
            float * in = IN(signalStartIndex + i) + firstSample;
            int numSamples = INBUFLENGTH(signalStartIndex + i);

            float & level = mChannelData[2*i];
            float & sqrsum = mChannelData[2*i + 1];
            if (numSamples == 1) {
                if (firstSample == 0)
                    nova::peak_rms_vec(in, &level, &sqrsum, 1);
            } else {
                if (!(samplesToAnalyze & 15) && !(firstSample & 3)) // check for unrolling and alignment
                    nova::peak_rms_vec_simd(in, &level, &sqrsum, samplesToAnalyze);
                else
                    nova::peak_rms_vec(in, &level, &sqrsum, samplesToAnalyze);
            }
        }
    }


    template <bool simd>
    inline void next_k(int inNumSamples)
    {
        mPhaseRemain -= 1;

        if (mPhaseRemain <= 0) {
            mPhaseRemain += mControlSamplesPerTick;
            sendReply();
        }

        analyzeFullBlock<simd>();
    }

    template <bool simd>
    inline void next_a(int inNumSamples)
    {
        if (mPhaseRemain >= inNumSamples) {
            mPhaseRemain -= inNumSamples;
            analyzeFullBlock<simd>();
        } else {
            if (mPhaseRemain == 0) {
                sendReply();
                mPhaseRemain = mAudioSamplesPerTick;
            }

            int startSample = 0;
            int samplesToAnalyze = std::min(mPhaseRemain, inNumSamples);
            int remain = inNumSamples;

            do {
                analyzePartialBlock(startSample, samplesToAnalyze);

                startSample += samplesToAnalyze;
                mPhaseRemain -= samplesToAnalyze;
                if (mPhaseRemain == 0) {
                    sendReply();
                    mPhaseRemain = mAudioSamplesPerTick;
                }

                remain -= samplesToAnalyze;
                samplesToAnalyze = std::min(remain, mPhaseRemain);
            } while(remain);
        }
    }

    template <bool simd>
    static void perform_k(Unit * unit, int inNumSamples)
    {
        static_cast<PeakRMS*>(unit)->next_k<simd>(inNumSamples);
    }

    template <bool simd>
    static void perform_a(Unit * unit, int inNumSamples)
    {
        static_cast<PeakRMS*>(unit)->next_a<simd>(inNumSamples);
    }
};

} /* namespace */

PluginLoad(PeakRMS)
{
    ft = inTable;

    registerUnit< PeakRMS  >( ft, "PeakRMS"  );
}
