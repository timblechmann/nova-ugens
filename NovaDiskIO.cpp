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

#include "sndfile.h"
#include "sndfile.hh"

#include "tlsf_allocator.hpp"

#include <array>
#include <atomic>
#include <map>
#include <thread>
#include <scoped_allocator>

#define BOOST_ERROR_CODE_HEADER_ONLY

#include "NovaUtils.hpp"

#include <boost/intrusive/set.hpp>
#include <boost/intrusive/unordered_set.hpp>
#include <boost/lockfree/queue.hpp>
#include <boost/lockfree/stack.hpp>
#include <boost/sync/semaphore.hpp>

#include <boost/sync/mutexes/spin_mutex.hpp>


static InterfaceTable *ft;

using nova::loop;
namespace intrusive = boost::intrusive;

namespace {



struct DiskOutThread
{
    typedef enum {
        OpenFile,
        WriteFrames,
        CloseFile
    } CmdType;

    struct Cmd
    {
        CmdType cmd;
        int ticket;
        int channels;
        int frames;
        int samplerate;
        void * payload;
    };

    static const size_t NumberOfElements = 4096;

    DiskOutThread()
    {
        using namespace std;

        loop(NumberOfElements, [&](){
            Cmd * cmd = new Cmd();
            memset(cmd, 0, sizeof(Cmd));
            mPool.push( cmd );
        });

        mRunning = true;
        mThread = thread( [this] { cmdLoop(); } );
    }

    ~DiskOutThread()
    {
        mRunning = false;

        mSemaphore.post();
        mThread.join();

        for(;;) {
            Cmd * cmd;
            bool success = mPool.pop(cmd);
            if (!success)
                break;
            delete cmd;
        }

        for (auto elem : mSndfileMap) {
            SNDFILE * handle = elem.second;
            sf_close(handle);
        }
    }

    int openFile(World * world, const char * fileName, int channels, int samplingRate)
    {
        int ret = mTicket.fetch_add(1);

        Cmd * cmd = allocateCmd(OpenFile, ret);
        if (!cmd)
            return -1;

        cmd->channels   = channels;
        cmd->samplerate = samplingRate;
        cmd->payload    = RTAlloc( world, strlen(fileName) + 1 );

        strcpy( (char*)cmd->payload, fileName );

        pushCommand(cmd);

        return ret;
    }

    template <typename Functor>
    bool pushFrames(World * world, int ticket, Functor const & fillCmdStruct)
    {
        Cmd * cmd = allocateCmd(WriteFrames, ticket);
        if (!cmd)
            return false;

        recycleChunks( world );
        fillCmdStruct( cmd );
        pushCommand( cmd );

        return true;
    }

    bool closeFile(int ticket)
    {
        Cmd * cmd = allocateCmd(CloseFile, ticket);
        if (!cmd)
            return false;

        pushCommand(cmd);

        return true;
    }

    void cmdLoop()
    {
        while (mRunning) {
            mSemaphore.wait();
            processQueueEvent();
        }

        while (processQueueEvent()) {
            // nop
        }
    }

private:
    void pushCommand( Cmd * cmd )
    {
        bool warningPosted = false;
        for (int attempts = 0; ; ++attempts) {
            bool success = mQueue.push(cmd);
            if (success)
                break;

            if (attempts < 256)
                continue;

            if (!warningPosted)
                printf("DiskOutThread: warning - cannot push command to queue\n");

            // no rt-safe!
            if (attempts < 2048)
                std::this_thread::yield();
            else
                std::this_thread::sleep_for( std::chrono::milliseconds(10) );
        }
        mSemaphore.post();
    }

    Cmd * allocateCmd(CmdType cmdType, int ticket)
    {
        Cmd * cmd;
        bool warningPosted = false;
        for (int attempts = 0; ; ++attempts) {
            bool success = mPool.pop(cmd);
            if (success)
               break;

            if (attempts < 256)
                continue;

            if (!warningPosted)
                printf("DiskOutThread: warning - cannot allocate command struct\n");

            if (attempts < 2048)
                std::this_thread::yield();
            else
                std::this_thread::sleep_for( std::chrono::milliseconds(10) );
        }

        cmd->cmd    = cmdType;
        cmd->ticket = ticket;
        return cmd;
    }

    bool processQueueEvent()
    {
        return mQueue.consume_one( [this] (Cmd * cmd) {
            switch (cmd->cmd) {
            case OpenFile:
            {
                SF_INFO info;
                info.samplerate = cmd->samplerate;
                info.channels = cmd->channels;
                info.format   = SF_FORMAT_CAF | SF_FORMAT_FLOAT;

                SNDFILE * handle = sf_open( (const char*)cmd->payload, SFM_WRITE, &info);
                if (!handle) {
                    printf("could not open file: %s %s\n", (const char*)cmd->payload, sf_error_number(sf_error(handle)));
                    break;
                }

                mSndfileMap.insert(std::make_pair(cmd->ticket, handle));
                mRecycleQueue.push( cmd->payload );
                break;
            }

            case CloseFile:
            {
                SNDFILE * handle = mSndfileMap[cmd->ticket];
                sf_close(handle);
                mSndfileMap.erase(cmd->ticket);
                break;
            }

            case WriteFrames:
            {
                mScratchpad.resize( cmd->channels * cmd->frames );

                interleave(cmd->channels, cmd->frames, (float*)cmd->payload, mScratchpad.data());

                SNDFILE * handle = mSndfileMap[cmd->ticket];
                int written = sf_writef_float(handle, mScratchpad.data(), cmd->frames);

                mRecycleQueue.push( cmd->payload );
                cmd->payload = nullptr;

                break;
            }

            default:
                assert(false);
            }

            bool pushSuccessful = mPool.push(cmd);
            assert(pushSuccessful);
        });
    }

    static void interleave(int channels, int frames, const float * source, float * destination)
    {
        nova::loop(frames, [&](int frame) {
            nova::loop(channels, [&](int channel) {
                destination[frame * channels + channel] = source[channel * frames + frame];
            });
        });
    }

    void recycleChunks( World * world )
    {
        for( int i = 0; i != 16; ++i ) {
            bool recycled = mRecycleQueue.consume_one( [this, world] (void* chunk) {
                RTFree( world, chunk );
            });

            if( !recycled )
                return;
        }
    }

    std::thread mThread;
    std::atomic_bool mRunning;

    boost::lockfree::queue<Cmd*>  mQueue { NumberOfElements };
    boost::lockfree::stack<Cmd*>  mPool  { NumberOfElements };

    boost::lockfree::queue<void*> mRecycleQueue { NumberOfElements + 8 };

    boost::sync::semaphore mSemaphore;

    std::atomic_int mTicket;

    std::map<int, SNDFILE*> mSndfileMap;
    std::vector<float> mScratchpad;
};

std::unique_ptr<DiskOutThread> gThread;

}

struct NovaDiskOut:
    public SCUnit
{
public:
    NovaDiskOut()
    {
        mChannelCount = in0(0);

        const int indexOfPathSize  = 1 + mChannelCount;
        const int indexOfPathStart = 2 + mChannelCount;
        const int pathSize  = (int)in0(indexOfPathSize);
        char path[16384];

        nova::loop(pathSize, [&](int index) {
            path[index] = (char)in0(indexOfPathStart + index);
        });
        path[pathSize] = 0;

        mTicket = gThread->openFile( mWorld, path, mChannelCount, (int)sampleRate());
        if (mTicket == -1) {
            Print("Cannot open file %s\n", path);
            mCalcFunc = ft->fClearUnitOutputs;
            return;
        }

        mCalcFunc = make_calc_function<NovaDiskOut, &NovaDiskOut::next>();
    }

    ~NovaDiskOut()
    {
        gThread->closeFile(mTicket);
    }

private:
    void next(int inNumSamples)
    {
        gThread->pushFrames( mWorld, mTicket, [&](DiskOutThread::Cmd * cmd) {
            cmd->frames   = inNumSamples;
            cmd->channels = mChannelCount;

            cmd->payload  = RTAlloc( mWorld, inNumSamples * mChannelCount * sizeof(float) );
            if( !cmd->payload ) {
                Print("out of rt memory");
                cmd->frames   = 0;
                cmd->channels = 0;
                return;
            }

            nova::loop(mChannelCount, [&](int channel) {
                const float * data = in(1 + channel);
                float * cmdData    = (float*)cmd->payload + channel * inNumSamples;

                memcpy(cmdData, data, inNumSamples * sizeof(float));
            });
        });
    }

    int mChannelCount;
    int mTicket;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace {

static const size_t framesPerChunk = 65536; // power of 2!
static const size_t chunksBefore   = 16;
static const size_t chunksAfter    = 32;
static const size_t chunksAhead    = 16;

typedef std::int64_t position_t;

struct Chunk:
    public intrusive::set_base_hook<>
{
    Chunk(SndfileHandle & sndfile, position_t currentPosition)
    {
        data_ = (float*)malloc( framesPerChunk * sndfile.channels() * sizeof(float) );
        startFrame_ = currentPosition;

        const position_t framesInFile = sndfile.frames();
        position_t remaining = std::min( (position_t)framesPerChunk, framesInFile - startFrame_ );

        while( remaining > 0 ) {
            size_t numberOfFrames = sndfile.readf(data_ + frames_, remaining);
            frames_              += numberOfFrames;
            remaining            -= numberOfFrames;
        }
    }

    ~Chunk()
    {
        free( data_ );
    }

    bool operator<=(Chunk const & rhs) const
    {
        return startFrame_ <= rhs.startFrame_;
    }

    bool operator<(Chunk const & rhs) const
    {
        return startFrame_ < rhs.startFrame_;
    }

    position_t startFrame() const { return startFrame_; }
    size_t  frames()        const { return frames_;     }
    float * data()          const { return data_;       }

private:
    float *            data_;
    position_t         startFrame_ = 0;
    size_t             frames_ = 0;
};

struct CompareChunkWithPosition
{
    static position_t getStartFrame( position_t startFrame ) { return startFrame;        }
    static position_t getStartFrame( Chunk const & chunk   ) { return chunk.startFrame(); }

    template <typename Lhs, typename Rhs>
    bool operator()(Lhs const & lhs, Rhs const & rhs) const
    {
        return getStartFrame( lhs ) < getStartFrame( rhs );
    }
};


class PlaybackQueue:
    public intrusive::unordered_set_base_hook< intrusive::store_hash<true> >
{
public:
    // nrt
    PlaybackQueue( bool isRealtime, int32_t id, std::string const & filename, position_t startIndex = 0 ):
        id(id), sf(filename.c_str(), SFM_READ), realTimeSynthesis(isRealtime)
    {
        if (!isRealtime)
            return;

        // we start queuing the first chunks immediately so they are available
        for (int i = 0; i != chunksAhead; ++i)
            queue( chunkAt(startIndex + i * framesPerChunk) );

        registerAvailableChunks(chunksAhead);

        workerThread = std::thread([&]() {
            this->readerLoop();
        });
    }

    // nrt
    ~PlaybackQueue()
    {
        if (realTimeSynthesis) {
            terminate = true;
            positionRequestSemaphore.post();
            workerThread.join();

            chunksForRt.consume_all(  std::default_delete<Chunk>() );
            chunks.clear_and_dispose( std::default_delete<Chunk>() );
            freeRetiredChunks();
        }
    }

    // nrt
    void queue(position_t queuePosition)
    {
        sf.seek(queuePosition, SEEK_SET);

        Chunk * chunk = new Chunk(sf, queuePosition);
        queuePosition += chunk->frames();

        for (;;) {
            bool success = chunksForRt.push(chunk);
            if (success)
                return;
            std::this_thread::yield();
        }
    }

    // nrt
    void freeRetiredChunks()
    {
        retiredChunks.consume_all( std::default_delete<Chunk>() );
    }

    // rt
    void readSamples(float ** out, position_t position, size_t numberOfFrames, int expectedChannels)
    {
        const int channels = sf.channels();
        if (channels != expectedChannels) { // channel mismatch!
            clearOutput(out, expectedChannels, 0, numberOfFrames);
            return;
        }

        const position_t chunk1Pos = chunkAt(position);

        auto chunk1It = chunks.find(chunk1Pos, CompareChunkWithPosition() );
        if (chunk1It == chunks.end()) {
            clearOutput(out, expectedChannels, 0, numberOfFrames);
            return;
        }

        Chunk & chunk1 = *chunk1It;

        const size_t firstFrameInChunk1  = position - chunk1Pos;
        const size_t availFramesInChunk1 = chunk1.frames() - firstFrameInChunk1;

        const size_t framesFromChunk1    = std::min(availFramesInChunk1, numberOfFrames);
        const size_t offset              = firstFrameInChunk1 * channels;

        deinterleaveToOutput( out, channels, 0, framesFromChunk1, chunk1.data() + offset );

        if (framesFromChunk1 == numberOfFrames)
            return; // we are done

        const size_t remainingFrames  = numberOfFrames - framesFromChunk1;

        const position_t chunk2Pos = chunkAt(position + numberOfFrames);
        auto chunk2It = chunks.find(chunk2Pos, CompareChunkWithPosition() );
        if (chunk2It == chunks.end()) {
            clearOutput( out, channels, framesFromChunk1, remainingFrames );
            return;
        }

        Chunk & chunk2 = *chunk2It;

        const size_t framesFromChunk2 = std::min(remainingFrames, chunk2.frames());
        const size_t framesToClear    = remainingFrames - framesFromChunk2;

        deinterleaveToOutput( out, channels, framesFromChunk1, framesFromChunk2, chunk2.data() );

        if (framesToClear) {
            assert( framesFromChunk1 + framesFromChunk2 + framesToClear == numberOfFrames );
            clearOutput( out, channels, framesFromChunk1 + framesFromChunk2, framesToClear );
        } else
            assert( framesFromChunk1 + framesFromChunk2 == numberOfFrames );
    }

    void readSamplesNRT(float ** out, position_t position, size_t numberOfFrames, int expectedChannels)
    {
        const int channels = sf.channels();
        if (channels != expectedChannels) { // channel mismatch!
            clearOutput(out, expectedChannels, 0, numberOfFrames);
            return;
        }

        sf.seek(position, SEEK_SET);

        std::vector<float> interleavedData(numberOfFrames * expectedChannels);
        size_t readFrames = sf.readf(interleavedData.data(), numberOfFrames);

        deinterleaveToOutput( out, expectedChannels, 0, readFrames, interleavedData.data() );

        if (readFrames != numberOfFrames)
            clearOutput( out, expectedChannels, readFrames, numberOfFrames - readFrames);
    }


    // nrt
    void readerLoop()
    {
        for(;;) {
            positionRequestSemaphore.wait();
            freeRetiredChunks();
            if (terminate.load(std::memory_order_relaxed) == true)
                return;

            requestQueue.consume_one([&](position_t requestPosition) {
                queue(requestPosition);
            });
        }
    }

    // rt
    void requestChunk(position_t position)
    {
        for (int i = 0; i != 32; ++i) {
            bool requestPosted = requestQueue.push(position);

            if (requestPosted) {
                positionRequestSemaphore.post();
                requestedChunks.insert(position);
                return;
            }
        }
    }

    // rt
    void performMaintenance(position_t currentPosition)
    {
        assert(realTimeSynthesis);

        requestReadaheadChunks(currentPosition);

        int allowedOperations = 16;
        allowedOperations -= registerAvailableChunks(allowedOperations);

        if (allowedOperations > 0)
            allowedOperations -= retireBefore(currentPosition - chunksBefore * framesPerChunk, allowedOperations);

        if (allowedOperations > 0)
            retireAfter(currentPosition + chunksAfter * framesPerChunk, allowedOperations);
    }

    // hashing
    static size_t hash_value(PlaybackQueue const & that)
    {
        return hash_value(that.id);
    }

    static int32_t hash_value(int32_t id)
    {
        return id * 2654435761U; // knuth hash, 32bit should be enough
    }

    const int32_t id;

private:
    //@{ helpers
    static void clearOutput(float ** outputRegions, int numberOfChannels, int sampleOffset, size_t framesToClear)
    {
        for( int channel = 0; channel != numberOfChannels; ++channel) {
            float * out = outputRegions[channel] + sampleOffset;
            std::memset(out, 0, sizeof(float) * framesToClear);
        }
    }

    static void deinterleaveToOutput(float ** outputRegions, int numberOfChannels, int sampleOffset, size_t numFrames,
                                     const float * __restrict__ interleavedInput)
    {
        if( numberOfChannels == 1 ) {
            for (size_t frame = 0; frame != numFrames; ++frame) {
                float frameData = *(interleavedInput + frame);

                (outputRegions[0] + sampleOffset)[frame] = frameData;
            }
            return;
        }

        if( numberOfChannels == 2 ) {
            for (size_t frame = 0; frame != numFrames; ++frame) {
                float frameData[2] = { *(interleavedInput + (2 * frame) + 0),
                                       *(interleavedInput + (2 * frame) + 1), };

                (outputRegions[0] + sampleOffset)[frame] = frameData[0];
                (outputRegions[1] + sampleOffset)[frame] = frameData[1];
            }
            return;
        }

        if( numberOfChannels == 4 ) {
            for (size_t frame = 0; frame != numFrames; ++frame) {
                float frameData[4] = { *(interleavedInput + (4 * frame) + 0),
                                       *(interleavedInput + (4 * frame) + 1),
                                       *(interleavedInput + (4 * frame) + 2),
                                       *(interleavedInput + (4 * frame) + 3), };

                (outputRegions[0] + sampleOffset)[frame] = frameData[0];
                (outputRegions[1] + sampleOffset)[frame] = frameData[1];
                (outputRegions[2] + sampleOffset)[frame] = frameData[2];
                (outputRegions[3] + sampleOffset)[frame] = frameData[3];
            }
            return;
        }


        for (size_t frame = 0; frame != numFrames; ++frame) {
            for( int channel = 0; channel != numberOfChannels; ++channel ) {
                float * __restrict__       out = outputRegions[channel] + sampleOffset;
                const float * __restrict__ sample = interleavedInput + (numberOfChannels * frame) + channel;
                out[frame] = *sample;
            }
        }
    }
    //@}

    //@{ maintenance
    int registerAvailableChunks(int rateLimit = 16)
    {
        for (int i = 0; i != rateLimit; ++i) {
            bool elementConsumed = chunksForRt.consume_one([&](Chunk * chunk) {
                chunks.insert(*chunk);
                requestedChunks.erase(chunk->startFrame());
            });
            if (!elementConsumed)
                return i;
        }
        return rateLimit;
    }

    int retireBefore(position_t position, int rateLimit = 16)
    {
        if( chunks.empty() )
            return 0;

        auto it = chunks.begin();
        for (int i = 0; i != rateLimit; ++i) {
            if (it->startFrame() < position) {
                Chunk & chunk = *it;
                auto next = std::next(it);
                chunks.erase(it);

                if( !retiredChunks.push(&chunk) ) {
                    printf("re-insert\n");
                    chunks.insert(chunk);
                    return i;
                }

                it = next;
                if (it == chunks.end())
                    return i;
            } else
                return i;
        }
        return rateLimit;
    }

    int retireAfter(position_t position, int rateLimit = 16)
    {
        if( chunks.empty() )
            return 0;

        auto it = chunks.rbegin();
        for (int i = 0; i != rateLimit; ++i) {
            if (it->startFrame() > position) {
                Chunk & chunk = *it;
                auto next = std::next(it);
                chunks.erase(chunk);

                if( !retiredChunks.push(&chunk) ) {
                    printf("re-insert\n");
                    chunks.insert(chunk);
                    return i;
                }

                it = next;
                if (it == chunks.rend())
                    return i;

            } else
                return i;
        }
        return rateLimit;
    }

    void requestReadaheadChunks(position_t currentPosition)
    {
        const position_t currentChunkPosition = chunkAt(currentPosition);
        const position_t readahead = currentChunkPosition + chunksAhead * framesPerChunk;

        for (position_t requestPosition = currentChunkPosition; requestPosition <= readahead; requestPosition += framesPerChunk) {
            if( requestPosition >= sf.frames() )
                return; // eof

            if ( chunks.find(requestPosition, CompareChunkWithPosition()) != chunks.end() )
                continue; // already queued

            if ( requestedChunks.find(requestPosition) != requestedChunks.end() )
                continue; // already requested

            requestChunk(requestPosition);
        }
    }
    //@}

    static position_t chunkAt(position_t position)
    {
        return position & (~(framesPerChunk - 1));
    }

    typedef boost::lockfree::queue<Chunk*, boost::lockfree::capacity<512>> ChunkQueue;
    typedef boost::lockfree::queue<position_t, boost::lockfree::capacity<512>> PositionQueue;
    typedef intrusive::set< Chunk, intrusive::constant_time_size<false> > ChunkSet;
    typedef nova::tlsf_allocator<char, 128 * 1024> RtAllocator; // 128k

    SndfileHandle sf;

    ChunkQueue chunksForRt;
    ChunkSet   chunks;
    ChunkQueue retiredChunks;

    PositionQueue                                             requestQueue;
    std::set<position_t, std::less<position_t>, RtAllocator > requestedChunks;
    boost::sync::semaphore                                    positionRequestSemaphore;

    bool realTimeSynthesis;

    std::thread      workerThread;
    std::atomic_bool terminate {false};
};

typedef std::unique_ptr<PlaybackQueue> PlaybackQueuePtr;


class NovaPlaybackManager
{
public:
    NovaPlaybackManager() = default;

    // nrt
    PlaybackQueuePtr openAndQueueFile( bool isRealtime, int32_t id, std::string const & filename, position_t queuePosition )
    {
        return PlaybackQueuePtr( new PlaybackQueue( isRealtime, id, filename, queuePosition ) );
    }

    // rt
    PlaybackQueuePtr registerPlaybackQueue( PlaybackQueuePtr && q )
    {
        PlaybackQueuePtr previousValue;

        // id collision check!
        auto it = playerQueues.find( q->id, HashFunctor(), EqualFunctor() );
        if ( it != playerQueues.end() ) {
            previousValue.reset( &*it );
            playerQueues.erase( it );
        }

        playerQueues.insert( *q.release() );
        return previousValue;
    }

    // rt
    PlaybackQueuePtr unregisterPlaybackQueue(int id)
    {
        auto it = playerQueues.find( id, HashFunctor(), EqualFunctor() );
        if ( it == playerQueues.end() )
            return nullptr;

        PlaybackQueuePtr ret( &*it );

        playerQueues.erase(it);
        return std::move( ret );
    }

    // rt
    size_t playbackQueueCount() const
    {
        return playerQueues.size();
    }

    // rt
    PlaybackQueuePtr unregisterFirstPlaybackQueue()
    {
        auto begin = playerQueues.begin();
        PlaybackQueuePtr queue( &*begin );
        playerQueues.erase( begin );
        return std::move( queue );
    }

    // rt
    void readFromChunk( int32_t id, position_t position, float ** out, int inNumSamples, int expectedChannels)
    {
        auto it = playerQueues.find( id, HashFunctor(), EqualFunctor() );
        if ( it == playerQueues.end() )
            return;

        it->readSamples(out, position, inNumSamples, expectedChannels);
        it->performMaintenance(position + inNumSamples);
    }

    void readNRT( int32_t id, position_t position, float ** out, int inNumSamples, int expectedChannels)
    {
        auto it = playerQueues.find( id, HashFunctor(), EqualFunctor() );
        if ( it == playerQueues.end() )
            return;

        it->readSamplesNRT(out, position, inNumSamples, expectedChannels);
    }

private:
    struct EqualFunctor
    {
        static int32_t getID(int32_t arg)               { return arg;    }
        static int32_t getID(PlaybackQueue const & arg) { return arg.id; }

        template <typename Arg1, typename Arg2>
        bool operator()(Arg1 const & lhs, Arg2 const & rhs) const
        {
            return getID(lhs) == getID(rhs);
        }
    };

    struct HashFunctor
    {
        template <typename ArgType>
        std::size_t operator()(ArgType const & value) const
        {
            return PlaybackQueue::hash_value(value);
        }
    };

    typedef intrusive::unordered_set< PlaybackQueue,
                                      intrusive::constant_time_size<false>,
                                      intrusive::power_2_buckets<true>,
                                      intrusive::hash<HashFunctor>,
                                      intrusive::equal<EqualFunctor>
                                    > PlaybackQueueSet;

    static const size_t bucketCount = 2048;
    PlaybackQueueSet::bucket_type PlayerQueueBuckets[bucketCount];
    PlaybackQueueSet playerQueues { PlaybackQueueSet::bucket_traits(PlayerQueueBuckets, bucketCount) };
};

NovaPlaybackManager gPlaybackManager;

enum {
    kOpen,
    kSeek,
    kClose,
    kCloseAll
};


struct NovaDiskInCmd
{
    void construct() { new(this) NovaDiskInCmd; }
    void destroy()   { this->~NovaDiskInCmd();    }

    int mode;
    char * path;
    int queuePosition;
    int id;
    PlaybackQueuePtr queue;

    PlaybackQueuePtr * queues;
    size_t numberOfQueues = 0;
};

static bool novaDiskInCmd2Nrt(World* world, void* inUserData)
{
    NovaDiskInCmd * cmd = reinterpret_cast<NovaDiskInCmd*>(inUserData);

    switch (cmd->mode) {
    case kOpen:
        cmd->queue = gPlaybackManager.openAndQueueFile( world->mRealTime, cmd->id, std::string(cmd->path), cmd->queuePosition );
        break;

    case kClose:
        cmd->queue.reset();
        break;

    case kCloseAll:
        for( size_t i = 0; i != cmd->numberOfQueues; ++i )
            cmd->queues[i].reset();

        break;

    default:
        break;
    }

    return true;
}

static bool novaDiskInCmd3Rt(World* world, void* inUserData)
{
    NovaDiskInCmd * cmd = reinterpret_cast<NovaDiskInCmd*>(inUserData);

    switch (cmd->mode) {
    case kOpen: {
        cmd->queue = gPlaybackManager.registerPlaybackQueue( std::move( cmd->queue) );
        break;
    }

    case kCloseAll:
        RTFree( world, cmd->queues );
        break;

    default:
        break;
    }

    return true; // scsynth will perform completion message after this returns
}

static bool novaDiskInCmd4NRt(World* world, void* inUserData)
{
    NovaDiskInCmd * cmd = reinterpret_cast<NovaDiskInCmd*>(inUserData);

    switch (cmd->mode) {
    case kOpen: {
        cmd->queue.reset();
        break;
    }
    default:
        break;
    }

    return true; // scsynth will send /done after this returns
}

static void cmdCleanup(World* world, void* inUserData)
{
    NovaDiskInCmd * cmd = reinterpret_cast<NovaDiskInCmd*>(inUserData);
    cmd->destroy();
}

void novaDiskInCmd(World *inWorld, void* inUserData, struct sc_msg_iter *args, void *replyAddr)
{
    NovaDiskInCmd * cmd = (NovaDiskInCmd*)RTAlloc(inWorld, sizeof(NovaDiskInCmd));
    cmd->construct();

    cmd->mode  = args->geti(0);
    cmd->id    = args->geti(0);
    const char * path = args->gets();
    if (path) {
        cmd->path = (char*)RTAlloc(inWorld, strlen(path) + 1);
        strcpy(cmd->path, path);
    }

    cmd->queuePosition = args->geti(0);

    // completion message
    int msgSize = args->getbsize();
    char* msgData = 0;
    if (msgSize) {
        msgData = (char*)RTAlloc(inWorld, msgSize);
        args->getb(msgData, msgSize);
    }

    switch(cmd->mode) {
    case kClose:
        cmd->queue = gPlaybackManager.unregisterPlaybackQueue(cmd->id);
        break;

    case kCloseAll: {
        cmd->numberOfQueues = gPlaybackManager.playbackQueueCount();
        cmd->queues = (PlaybackQueuePtr*)RTAlloc(inWorld, sizeof(PlaybackQueuePtr*) * cmd->numberOfQueues);

        for( size_t i = 0; i != cmd->numberOfQueues; ++i )
            new( cmd->queues + i )  PlaybackQueuePtr( gPlaybackManager.unregisterFirstPlaybackQueue() );

        break;
    }

    default:
        break;
    }

    DoAsynchronousCommand(inWorld, replyAddr, "NovaDiskIn", (void*)cmd,
                          (AsyncStageFn)novaDiskInCmd2Nrt,
                          (AsyncStageFn)novaDiskInCmd3Rt,
                          (AsyncStageFn)novaDiskInCmd4NRt,
                          cmdCleanup,
                          msgSize, msgData);
}

}

struct NovaDiskIn:
    public SCUnit
{
public:
    NovaDiskIn()
    {
        mChannelCount       = in0(0);
        mPlaybackQueueIndex = in0(1);

        if (mWorld->mRealTime)
            mCalcFunc = make_calc_function<NovaDiskIn, &NovaDiskIn::next>();
        else
            mCalcFunc = make_calc_function<NovaDiskIn, &NovaDiskIn::nextNRT>();

        ClearUnitOutputs(this, 1);
    }

private:
    void next(int inNumSamples)
    {
        gPlaybackManager.readFromChunk(mPlaybackQueueIndex, currentPosition, this->mOutBuf, inNumSamples, mChannelCount);
        currentPosition += inNumSamples;
    }

    void nextNRT(int inNumSamples)
    {
        gPlaybackManager.readNRT(mPlaybackQueueIndex, currentPosition, this->mOutBuf, inNumSamples, mChannelCount);
        currentPosition += inNumSamples;
    }

    int mChannelCount;
    int32_t mPlaybackQueueIndex;
    position_t currentPosition = 0;
};




DEFINE_XTORS(NovaDiskOut)
DEFINE_XTORS(NovaDiskIn)


PluginLoad(NovaDiskIO)
{
    gThread.reset(new DiskOutThread);

    ft = inTable;
    DefineDtorUnit(NovaDiskOut);

    DefineDtorUnit(NovaDiskIn);
    DefinePlugInCmd("NovaDiskIn", novaDiskInCmd, (void*)&gPlaybackManager);
}
