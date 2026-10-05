#include "cdstream.h"

#include <psyq/sys/types.h>
#include <psyq/kernel.h>
#include <psyq/libapi.h>
#include <psyq/libcd.h>
#include <psyq/libspu.h>
#include <psyq/libetc.h>

#include "common.h"

#include "cdaudio.h"
#include "main/display.h"
#include "main/display_types.h"
#include "sound.h"
#include "sound_types.h"
#include "main/task.h"
#include "task.h"
#include "main/task_types.h"

#include "gameplay/scene_runtime.h"

/// High three bytes of `_MtsHeader::magic` (`'M'`, `'T'`, `'S'`). The low byte is the channel count.
#define CD_STREAM_MTS_SIGNATURE 0x4D545300

/// `_MtsHeader::flags` bit 7: `gapSectors` is the disc gap after this chunk.
#define CD_STREAM_MTS_FLAG_GAP 0x80

/// `_MtsHeader::flags` bits 5 and 6, selecting where the final chunk stops.
#define CD_STREAM_MTS_FLAG_END_MASK 0x60

/// `_MtsHeader::flags` bit 6: stop halfway through the final chunk. Wins over bit 5.
#define CD_STREAM_MTS_FLAG_END_HALF 0x40

/// Header in the first 16 bytes of an MTS audio sector.
///
/// `CdStreamState::sector` addresses the whole sector buffer. Header sectors
/// recur every `period` sectors; the bytes after this header, and the sectors
/// between headers, are SPU-ADPCM for `channelIndex`. `chunkIndex` numbers
/// chunks from zero and is shared by every channel of the chunk. `chunkCount`
/// is the stream length in chunks. `gapSectors` is the unused disc gap after
/// the chunk.
typedef struct {
    s32 chunkIndex;   // chunk number, shared by every channel of the chunk
    s32 chunkCount;   // total chunks in the stream
    u32 magic;        // high 3 bytes are 'M','T','S'; low byte is the channel count
    s8  channelIndex; // which channel this header's audio belongs to (0, 1)
    u8  period;       // sectors until the next header
    u8  gapSectors;   // disc sectors skipped after this chunk
    s8  flags;        // bit7 apply gapSectors; bit6 stop halfway through the final chunk;
                      // bit5 stop on entering it (bit6 wins if both are set); neither plays it through.
                      // Other bits are unread.
} _MtsHeader;
STATIC_ASSERT_SIZEOF(_MtsHeader, 0x10);

/// `CdStreamState::flags0`. Bit 7 is unread.
#define CD_STREAM_ACTIVE      1    // the vsync tick services the stream
#define CD_STREAM_ENGAGED     2    // the opening read finished; the voices are not keyed yet
#define CD_STREAM_CHUNK_READY 4    // the queued chunk read has completed
#define CD_STREAM_SPU_IRQ     8    // the SPU interrupt snapped the playhead to this chunk
#define CD_STREAM_VOICES_ON   0x10 // the voices are keyed on
#define CD_STREAM_KEY_ON      0x20 // key the voices on at the next tick
#define CD_STREAM_STOP        0x40 // stop was requested

/// Shift index of a `flags0` test.
#define CD_STREAM_ENGAGED_BIT     1
#define CD_STREAM_CHUNK_READY_BIT 2
#define CD_STREAM_SPU_IRQ_BIT     3
#define CD_STREAM_VOICES_ON_BIT   4
#define CD_STREAM_KEY_ON_BIT      5
#define CD_STREAM_STOP_BIT        6

/// Byte mask that clears one `flags0` bit.
#define CD_STREAM_CLEAR_ACTIVE      0xFE
#define CD_STREAM_CLEAR_ENGAGED     0xFD
#define CD_STREAM_CLEAR_CHUNK_READY 0xFB
#define CD_STREAM_CLEAR_SPU_IRQ     0xF7
#define CD_STREAM_CLEAR_VOICES_ON   0xEF
#define CD_STREAM_CLEAR_KEY_ON      0xDF
#define CD_STREAM_CLEAR_STOP        0xBF

/// `CdStreamState::flags1`.
#define CD_STREAM_VOICE_COPY   1    // voice attributes need copying
#define CD_STREAM_MODE_SET     2    // the CD streaming mode is already latched
#define CD_STREAM_APPLY_GAP    4    // add `gapSectors` to the disc cursor and skip that gap
#define CD_STREAM_SEEK         8    // a requested playhead is waiting to be applied
#define CD_STREAM_END_ON_ENTRY 0x10 // stop on entering the final chunk
#define CD_STREAM_END_HALFWAY  0x20 // stored for a halfway stop; the tick tests `END_THROUGH` instead
#define CD_STREAM_END_THROUGH  0x40 // play the final chunk through
#define CD_STREAM_SEEK_ENABLED 0x80 // a playhead request may be accepted

/// Shift index of a `flags1` test.
#define CD_STREAM_MODE_SET_BIT     1
#define CD_STREAM_APPLY_GAP_BIT    2
#define CD_STREAM_SEEK_BIT         3
#define CD_STREAM_END_ON_ENTRY_BIT 4
#define CD_STREAM_END_THROUGH_BIT  6
#define CD_STREAM_SEEK_ENABLED_BIT 7

/// Byte mask that clears one `flags1` bit.
#define CD_STREAM_CLEAR_VOICE_COPY 0xFE
#define CD_STREAM_CLEAR_APPLY_GAP  0xFB
#define CD_STREAM_CLEAR_SEEK       0xF7

/// `CdStreamState::flags2`. Bits 4–7 are unread. Bits 1 and 2 are independent flags.
#define CD_STREAM_REQUEUE       1 // arm another opening read
#define CD_STREAM_FAULT         2 // abort playback and report the error
#define CD_STREAM_CHUNK_ADVANCE 4 // the tick should advance to the next chunk
#define CD_STREAM_HALT          8 // a stream error has raised the game halt

/// Shift index of a `flags2` test.
#define CD_STREAM_FAULT_BIT         1
#define CD_STREAM_CHUNK_ADVANCE_BIT 2
#define CD_STREAM_HALT_BIT          3

/// Byte mask that clears one `flags2` bit.
#define CD_STREAM_CLEAR_REQUEUE       0xFE
#define CD_STREAM_CLEAR_FAULT         0xFD
#define CD_STREAM_CLEAR_CHUNK_ADVANCE 0xFB
#define CD_STREAM_CLEAR_HALT          0xF7

/// `CdStreamState::flags`. Bits 2–7 are unread.
#define CD_STREAM_DISC_FAULT 1 // the shell opened or the disc failed; reinit the drive
#define CD_STREAM_MONO       2 // mix both voices to the centre

#define CD_STREAM_CLEAR_DISC_FAULT 0xFE
#define CD_STREAM_CLEAR_MONO       0xFD

/// Vsyncs one chunk occupies, and the byte size of one SPU ring half.
/// A header period of 5 uses the short pair (24/20 vsyncs, NTSC then PAL).
#define CD_STREAM_CHUNK_VSYNCS_NTSC       0x30
#define CD_STREAM_CHUNK_VSYNCS_PAL        0x28
#define CD_STREAM_CHUNK_VSYNCS_SHORT_NTSC 0x18
#define CD_STREAM_CHUNK_VSYNCS_SHORT_PAL  0x14
#define CD_STREAM_RING_HALF               0x4ED0
#define CD_STREAM_RING_HALF_SHORT         0x2770

/// `CdStreamState::readPhase`.
#define CD_STREAM_READ_IDLE     0 // no sector read is in progress
#define CD_STREAM_READ_AUDIO    1 // accepting audio sectors
#define CD_STREAM_READ_GAP      2 // skipping the disc gap
#define CD_STREAM_READ_FAULT    3 // the read failed
#define CD_STREAM_READ_COMPLETE 4 // the chunk's sectors have arrived
#define CD_STREAM_READ_SETLOC   5 // a setloc is in flight

/// `CdStreamState::phase`. Zero is idle, before a read has been queued.
#define CD_STREAM_PHASE_READY   1 // the opening read finished and the voices may engage
#define CD_STREAM_PHASE_READING 2 // a chunk read is queued

/// Polls after the streaming-mode command is accepted before disc init finishes.
#define CD_STREAM_SETTLE_POLLS 4

/// Header period that selects the short chunk length and ring half.
#define CD_STREAM_SHORT_PERIOD 5

/// Control block for one CD-to-SPU MTS audio stream.
///
/// It sits immediately before the stream's two `SpuVoiceAttr` records. The
/// vsync tick advances `playhead` once per vsync while playback is active, and
/// one chunk lasts `chunkVsyncs` vsyncs. Disc position is separate:
/// `startSector` is the stream origin, `readSector` is where the next chunk
/// read begins, and `expectedSector` is the sector the data callback must
/// observe next. The low bit of `chunkIndex` selects which half of the SPU
/// ring receives the chunk.
typedef struct {
    u8 flags0;                       // bit0 active, bit1 engaged, bit2 chunk ready, bit3 SPU IRQ,
                                     // bit4 voices on, bit5 key-on pending, bit6 stop requested.
                                     // Bit 7 is unread.
    u8 flags1;                       // bit0 voice attributes dirty, bit1 CD mode latched,
                                     // bit2 apply gapSectors (set from the first header even when the count is 0),
                                     // bit3 playhead override pending, bit4 stop on entering the final chunk,
                                     // bit5 halfway stop stored but not read (halfway is bit 6 clear),
                                     // bit6 play the final chunk through, bit7 playhead override enabled.
    u8 flags2;                       // bit0 requeue the opening read, bit1 fault, bit2 advance the chunk,
                                     // bit3 game halt raised by a stream error. Bits 4–7 are unread.
    u8          phase;               // 0 idle, 1 opening read finished, 2 a chunk read is queued
    u8          advanceDelay;        // vsyncs until the next chunk advance
    u8          pad;                 // unreferenced; keeps readySlot 2-byte aligned
    s16         readySlot;           // 1-based CdReady_Queue index, 0 = none
    void        (*doneCb)(s32);      // 1 when the opening read finishes, 0 when a read is abandoned
    void        (*startCb)(s32);     // voice mask when the voices are keyed on
    void        (*voiceFreeCb)(s32); // voice mask when the voices are keyed off
    s32         requestedPlayhead;   // playhead applied when the override bit is set
    s32         playhead;            // vsyncs since playback started
    s32         chunkIndex;          // current MTS chunk; the low bit selects the SPU ring half
    s32         queuedChunk;         // chunk index of the read most recently queued
    s32         spuAddr;             // next SPU address for the ADPCM transfer
    s32         startSector;         // disc sector of the first chunk
    s32         expectedSector;      // disc sector the ready callback expects next
    s32         readSector;          // disc sector where the next chunk read starts
    s32         expectedChunk;       // incremented on each advance; must equal the playhead's next chunk
    s32         chunkCount;          // MTS chunks in the stream, from the first header; 1 until it arrives
    s32         spuBase;             // SPU address of ring half 0
    s16         chunkVsyncs;         // vsyncs per chunk: 48/40, or 24/20 when the header period is 5
    s16         ringHalf;            // bytes in one SPU ring half: 0x2770 when the period is 5, else 0x4ED0
    s16         gapRemaining;        // disc-gap sectors still to skip; loaded as (s16)(s8)gapSectors
    s8          mtsPeriod;           // sectors from one channel header to the next, from the first header
    u8          gapSectors;          // signed disc-gap length; applied only while flags1 bit 2 is set
    _MtsHeader* sector;              // sector buffer; the header occupies its first 16 bytes
    s16         readPhase;           // 0 idle, 1 reading audio, 2 skipping the gap, 3 fault,
                                     // 4 chunk complete, 5 setloc in flight
    s16 sectorsLeft;                 // audio sectors left before the chunk ends or the gap skip starts
    u8  voiceL;                      // -1 is stored as 0xFF and sign-extended where the index is used
    u8  voiceR;                      // -1 is stored as 0xFF and sign-extended where the index is used
    s8  channelCount;                // from the setup block, then replaced by the header's low magic byte
    u8  flags;                       // bit0 disc fault (reinit the drive), bit1 mono mix. Bits 2–7 unread.
    u16 reinitSlot;                  // 1-based disc-reinit callback slot, 0 when none is in flight
    u16 settleCounter;               // polls after the streaming-mode command; disc init finishes at 4
} CdStreamState;
STATIC_ASSERT_SIZEOF(CdStreamState, 0x58);

// Channel records are copied whole into the PsyQ voice-update queue.
STATIC_ASSERT_SIZEOF(SpuVoiceAttr, 0x40);

/// Stereo SPU voice attributes for one CD-to-SPU stream.
///
/// Each record is programmed for its allocated voice and copied whole into
/// the PsyQ voice-update queue.
typedef struct {
    SpuVoiceAttr voiceAttr[2]; // 0 left voice, 1 right voice
} _CdStreamChannels;
STATIC_ASSERT_SIZEOF(_CdStreamChannels, 0x80);

/// The one live CD-to-SPU stream.
///
/// The control block and both voice-attribute records are a single allocation.
/// The block is volatile because the vsync tick, the disc callback and the SPU
/// interrupt all update it. The records are the left and right channels, and
/// each is copied whole into the SPU voice queue. The allocation is one object:
/// wiping it clears the records with the block.
typedef struct {
    volatile CdStreamState state;    // control block shared by the tick and the callbacks
    _CdStreamChannels      channels; // left and right SPU voice attributes
} CdStreamRuntime;
STATIC_ASSERT_SIZEOF(CdStreamRuntime, 0xD8);
STATIC_ASSERT(OFFSET_OF(CdStreamRuntime, channels) == 0x58, cd_stream_channels_offset);

/// `_CdReadyEntry::phase` while a chunk read owns the entry, in the order a
/// read passes through them.
///
/// Zero is an entry that has not been polled. The two cancel steps are entered
/// only when a read is cancelled with the drive or the SPU still busy.
#define CD_STREAM_STEP_SETMODE         1  // issue the streaming drive mode
#define CD_STREAM_STEP_SETMODE_SYNC    2  // wait for the mode command
#define CD_STREAM_STEP_SPEED_SETTLE    3  // count down the polls after a speed change
#define CD_STREAM_STEP_SETLOC          4  // target the entry's sector
#define CD_STREAM_STEP_SETLOC_SYNC     5  // wait for the setloc
#define CD_STREAM_STEP_READ            6  // install the data callback and start the read
#define CD_STREAM_STEP_READ_SYNC       7  // wait for the read command
#define CD_STREAM_STEP_SECTORS         8  // wait for the chunk's sectors
#define CD_STREAM_STEP_PAUSE           9  // pause the drive
#define CD_STREAM_STEP_PAUSE_SYNC      11 // wait for the pause
#define CD_STREAM_STEP_TRANSFER        12 // wait for the SPU transfer
#define CD_STREAM_STEP_IDLE_SYNC       10 // wait for the drive to go idle, which finishes the read
#define CD_STREAM_STEP_CANCEL_TRANSFER 13 // cancelled: wait for the SPU transfer
#define CD_STREAM_STEP_CANCEL_PAUSE    14 // cancelled among the sectors: the pause is issued, the flush is not

/// One queued job of the CD-ready queue: a disc operation polled to completion.
///
/// The queue polls the entry at its head each time the stream is driven.
/// `pollFn` advances the job and returns nonzero once it has finished,
/// successfully or not; the queue then calls `doneFn` and moves on.
/// Cancelling an active entry clears `active` and sets `cancelled`: one that
/// was never polled is dropped, one already under way gets `cancelFn`, again
/// on every later poll for as long as that leaves `cancelPending` set. Either
/// handler may be null.
///
/// Queueing copies the caller's handlers and `sector` and keeps no reference
/// to the caller's entry; the status bits belong to the queue and to `pollFn`.
/// The 19 bits above `phase` are written only by the queue's reset.
typedef struct _CdReadyEntry {
    u32  active        : 1;                  // queued and still to be polled
    u32  firstPoll     : 1;                  // set on queueing, cleared by `pollFn` on its first call
    u32  cancelled     : 1;                  // cancelled; cleared when the queue moves past the entry
    u32  cancelPending : 1;                  // `cancelFn` has not finished and is to be called again
    u32  speedChange   : 1;                  // the drive was not at double speed: let it settle after the mode command
    u32  phase         : 8;                  // step of `pollFn`'s state machine, zeroed on queueing
    s32  sector;                             // absolute disc sector the read starts at
    s32  (*pollFn)(struct _CdReadyEntry*);   // advances the job; nonzero once it has finished
    void (*doneFn)(void);                    // called when `pollFn` reports the job finished
    void (*cancelFn)(struct _CdReadyEntry*); // winds down a cancelled job that had started
} _CdReadyEntry;
STATIC_ASSERT_SIZEOF(_CdReadyEntry, 0x14);

/// The CD-ready queue: a ring of disc jobs polled one at a time, with the state
/// of the data-ready handler those jobs install.
///
/// Jobs are queued at `writeIdx` and polled in order from `readIdx`. The ring
/// is empty when the two are equal, so one slot always stays free and a full
/// ring refuses the job. A queued job is known to its owner by its slot's
/// index plus one, zero meaning none.
///
/// The header is volatile because the vsync tick polls the queue while the
/// main thread queues and cancels jobs. The slots are not: a job's status bits
/// are updated as one word.
typedef struct {
    volatile u8    locked;            // nonzero while a caller edits the queue; the poll then skips its turn
    volatile u8    callbackInstalled; // a job has the data-ready handler installed (0 no, 1 yes)
    volatile s8    readIdx;           // slot of the job being polled
    volatile s8    writeIdx;          // slot the next job is queued in
    volatile CdlCB prevCallback;      // handler `CdReadyCallback` held before a job's first install; saved, never restored
    _CdReadyEntry  entries[4];        // the ring
} _CdReadyQueue;
STATIC_ASSERT_SIZEOF(_CdReadyQueue, 0x58);

/// Clears `count` words at `dst`, in place like `MEM_CLEAR`.
#define MEM_CLEAR_WORDS(dst, count)                       \
    {                                                     \
        s32* _clearPtr = (s32*)(dst);                     \
        u32  _clearI;                                     \
        for (_clearI = 0; _clearI < (count); _clearI++) { \
            *_clearPtr++ = 0;                             \
        }                                                 \
    }

static s32 CdStream_PhaseTimeout;

/// Unreferenced.
static u8 D_800827F0[8];

static CdlLOC D_800827F8;

/// Unreferenced.
static u8 D_80082800[8];

static volatile u16 CdStream_ErrorCode;

static volatile s32 CdStream_CurrentPhase;

static volatile u16 CdStream_LastErrorCode;

static volatile s32 CdStream_LastPhase;

static CdStreamRuntime CdStream_Runtime;

static _CdReadyQueue CdReady_Queue;

static volatile s32 D_80068B54;

static volatile s32 D_80068B58;

static volatile u8 D_80068B5C;

/// Shell-open errors reported by the stream poller and CD-ready callback.
static volatile u8 CdStream_ShellOpenErrors;

static u8 D_80068B5E;

static volatile u8 D_80068B5F;

static volatile u8 D_80068B60;

static volatile u8 D_80068B61;

static volatile u8 D_80068B62;

static volatile u8 D_80068B63;

static volatile u8 D_80068B64;

/// Unreferenced.
static u8 D_80068B65;

/// Copy of CdStream_InitDisc's current step, refreshed on every poll; nothing
/// reads it back.
static volatile u8 D_80068B66;

static volatile u8 D_80068B67;

/// Unreferenced.
static s16 D_80068B68;

static volatile s16 CdStream_ReadyCallbackActive;

static void* CdStream_LastTransferBuffer;

static s32 CdStream_LastTransferSpuAddress;

static s32 D_80068B74;

static u16 D_80068B78;

void func_80725BB8(Task* arg0);

static s32 CdReady_Enqueue(_CdReadyEntry* arg0);

static void CdReady_Poll(void);

static void CdStream_Continue(void);

static void CdStream_TeardownVoices(void);

/// Starts playback after the initial read, or queues another read while waiting.
static void CdStream_CompleteInitialRead(void);

static void CdStream_CleanupIrq(void);

/// Updates stream voices and queues the next chunk as playback advances.
static void CdStream_TickPlayback(void);

/// Handles a completed chunk read, including underrun and end-of-stream state.
static void CdStream_CompleteChunkRead(void);

/// Advances the queued MTS read through mode, seek, read, pause and completion.
static s32 CdStream_PollMtsRead(_CdReadyEntry* entry);

static void CdStream_ReadyMts(u8 interrupt, u8* result);

static s32 CdStream_InitDisc(AsyncCbEntry* entry);

static void CdReady_InstallCallback(CdlCB arg0);

static void CdReady_ClearCallback(void);

static void CdStream_SpuIrqHandler(void);

static void CdStream_SetFlag14(s32 playhead);

static void CdStream_AbortPhase(_CdReadyEntry* entry);

static void CdStream_FinishQueueEntry(_CdReadyEntry* entry);

static void CdReady_Cancel(s16 arg0);

static void _cdStreamClearReadySlot(void);

static void CdStream_MarkEnding(AsyncCbEntry* unused);

static s32 _cdStreamCancelDiscInit(AsyncCbEntry* entry);

static void CdStream_ConfigureSpuIrq(s32 arg0, u32 arg1);

/* The second byte of D_80068B5C and of D_80068B64 is written as the first
 * symbol's address plus one. Declaring either pair as a struct or an array
 * compiles the byte as an offset from a base register, where the original
 * folds the whole address into the load and store. */

static volatile s32 D_80068B54 = 0;
static volatile s32 D_80068B58 = 0;
static volatile u8  D_80068B5C = 0;
/// Shell-open errors reported by the stream poller and CD-ready callback.
static volatile u8 CdStream_ShellOpenErrors = 0;
static u8          D_80068B5E               = 0;
static volatile u8 D_80068B5F               = 0;
static volatile u8 D_80068B60               = 0;
static volatile u8 D_80068B61               = 0;
static volatile u8 D_80068B62               = 0;
static volatile u8 D_80068B63               = 0;
static volatile u8 D_80068B64               = 0;
/// Unreferenced.
static u8 D_80068B65 = 0;
/// Copy of CdStream_InitDisc's current step, refreshed on every poll; nothing
/// reads it back.
static volatile u8 D_80068B66 = 0;
static volatile u8 D_80068B67 = 0;
/// Unreferenced.
static s16          D_80068B68                      = 0;
static volatile s16 CdStream_ReadyCallbackActive    = 0;
static void*        CdStream_LastTransferBuffer     = NULL;
static s32          CdStream_LastTransferSpuAddress = 0;
static s32          D_80068B74                      = 0;
static u16          D_80068B78                      = 0;
TaskDesc            D_80068B7C[]                    = {
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, taskKill },
    { { { TASK_BODY_NONE, 0xC0 } }, func_80725BB8 },
};
u16 Spu_SemitonePitchTable[] = {
    0x0010,
    0x0010,
    0x0011,
    0x0013,
    0x0014,
    0x0015,
    0x0016,
    0x0017,
    0x0019,
    0x001A,
    0x001C,
    0x001E,
    0x0020,
    0x0021,
    0x0023,
    0x0026,
    0x0028,
    0x002A,
    0x002D,
    0x002F,
    0x0032,
    0x0035,
    0x0039,
    0x003C,
    0x0040,
    0x0043,
    0x0047,
    0x004C,
    0x0050,
    0x0055,
    0x005A,
    0x005F,
    0x0065,
    0x006B,
    0x0072,
    0x0078,
    0x0080,
    0x0087,
    0x008F,
    0x0098,
    0x00A1,
    0x00AA,
    0x00B5,
    0x00BF,
    0x00CB,
    0x00D7,
    0x00E4,
    0x00F1,
    0x0100,
    0x010F,
    0x011F,
    0x0130,
    0x0142,
    0x0155,
    0x016A,
    0x017F,
    0x0196,
    0x01AE,
    0x01C8,
    0x01E3,
    0x0200,
    0x021E,
    0x023E,
    0x0260,
    0x0285,
    0x02AB,
    0x02D4,
    0x02FF,
    0x032C,
    0x035D,
    0x0390,
    0x03C6,
    0x0400,
    0x043C,
    0x047D,
    0x04C1,
    0x050A,
    0x0556,
    0x05A8,
    0x05FE,
    0x0659,
    0x06BA,
    0x0720,
    0x078D,
    0x0800,
    0x0879,
    0x08FA,
    0x0983,
    0x0A14,
    0x0AAD,
    0x0B50,
    0x0BFC,
    0x0CB2,
    0x0D74,
    0x0E41,
    0x0F1A,
};
u16 Spu_FinePitchTable[] = {
    0x0400,
    0x0400,
    0x0400,
    0x0401,
    0x0401,
    0x0402,
    0x0402,
    0x0403,
    0x0403,
    0x0404,
    0x0404,
    0x0405,
    0x0405,
    0x0406,
    0x0406,
    0x0406,
    0x0407,
    0x0407,
    0x0408,
    0x0408,
    0x0409,
    0x0409,
    0x040A,
    0x040A,
    0x040B,
    0x040B,
    0x040C,
    0x040C,
    0x040D,
    0x040D,
    0x040D,
    0x040E,
    0x040E,
    0x040F,
    0x040F,
    0x0410,
    0x0410,
    0x0411,
    0x0411,
    0x0412,
    0x0412,
    0x0413,
    0x0413,
    0x0414,
    0x0414,
    0x0415,
    0x0415,
    0x0415,
    0x0416,
    0x0416,
    0x0417,
    0x0417,
    0x0418,
    0x0418,
    0x0419,
    0x0419,
    0x041A,
    0x041A,
    0x041B,
    0x041B,
    0x041C,
    0x041C,
    0x041D,
    0x041D,
    0x041E,
    0x041E,
    0x041E,
    0x041F,
    0x041F,
    0x0420,
    0x0420,
    0x0421,
    0x0421,
    0x0422,
    0x0422,
    0x0423,
    0x0423,
    0x0424,
    0x0424,
    0x0425,
    0x0425,
    0x0426,
    0x0426,
    0x0427,
    0x0427,
    0x0428,
    0x0428,
    0x0429,
    0x0429,
    0x0429,
    0x042A,
    0x042A,
    0x042B,
    0x042B,
    0x042C,
    0x042C,
    0x042D,
    0x042D,
    0x042E,
    0x042E,
    0x042F,
    0x042F,
    0x0430,
    0x0430,
    0x0431,
    0x0431,
    0x0432,
    0x0432,
    0x0433,
    0x0433,
    0x0434,
    0x0434,
    0x0435,
    0x0435,
    0x0436,
    0x0436,
    0x0437,
    0x0437,
    0x0438,
    0x0438,
    0x0438,
    0x0439,
    0x0439,
    0x043A,
    0x043A,
    0x043B,
    0x043B,
    0x043C,
};
u16 Snd_PanGainTable[] = {
    0x1000,
    0x0FFF,
    0x0FFE,
    0x0FFD,
    0x0FFA,
    0x0FF8,
    0x0FF4,
    0x0FF0,
    0x0FEB,
    0x0FE6,
    0x0FE0,
    0x0FD9,
    0x0FD2,
    0x0FCA,
    0x0FC1,
    0x0FB8,
    0x0FAE,
    0x0FA4,
    0x0F99,
    0x0F8D,
    0x0F81,
    0x0F74,
    0x0F66,
    0x0F58,
    0x0F4A,
    0x0F3A,
    0x0F2A,
    0x0F1A,
    0x0F08,
    0x0EF7,
    0x0EE4,
    0x0ED1,
    0x0EBE,
    0x0EAA,
    0x0E95,
    0x0E80,
    0x0E6A,
    0x0E53,
    0x0E3C,
    0x0E25,
    0x0E0D,
    0x0DF4,
    0x0DDB,
    0x0DC1,
    0x0DA7,
    0x0D8C,
    0x0D70,
    0x0D54,
    0x0D38,
    0x0D1B,
    0x0CFD,
    0x0CDF,
    0x0CC1,
    0x0CA1,
    0x0C82,
    0x0C62,
    0x0C41,
    0x0C20,
    0x0BFF,
    0x0BDD,
    0x0BBA,
    0x0B97,
    0x0B74,
    0x0B50,
    0x0B2B,
    0x0B07,
    0x0AE1,
    0x0ABC,
    0x0A96,
    0x0A6F,
    0x0A48,
    0x0A21,
    0x09F9,
    0x09D1,
    0x09A9,
    0x0980,
    0x0957,
    0x092D,
    0x0903,
    0x08D8,
    0x08AE,
    0x0883,
    0x0857,
    0x082C,
    0x0800,
    0x07D3,
    0x07A6,
    0x0779,
    0x074C,
    0x071F,
    0x06F1,
    0x06C3,
    0x0694,
    0x0665,
    0x0637,
    0x0607,
    0x05D8,
    0x05A8,
    0x0578,
    0x0548,
    0x0518,
    0x04E8,
    0x04B7,
    0x0486,
    0x0455,
    0x0424,
    0x03F2,
    0x03C1,
    0x038F,
    0x035D,
    0x032B,
    0x02F9,
    0x02C7,
    0x0294,
    0x0262,
    0x022F,
    0x01FD,
    0x01CA,
    0x0197,
    0x0164,
    0x0132,
    0x00FF,
    0x00CC,
    0x0099,
    0x0066,
    0x0033,
    0x0000,
    0x0000,
};
u16 Snd_VelocityGainTable[] = {
    0x0000,
    0x0001,
    0x0004,
    0x0009,
    0x0010,
    0x0019,
    0x0024,
    0x0031,
    0x0041,
    0x0052,
    0x0065,
    0x007A,
    0x0092,
    0x00AB,
    0x00C7,
    0x00E4,
    0x0104,
    0x0125,
    0x0149,
    0x016E,
    0x0196,
    0x01BF,
    0x01EB,
    0x0219,
    0x0249,
    0x027A,
    0x02AE,
    0x02E4,
    0x031C,
    0x0356,
    0x0392,
    0x03D0,
    0x0410,
    0x0452,
    0x0496,
    0x04DC,
    0x0524,
    0x056E,
    0x05BA,
    0x0608,
    0x0659,
    0x06AB,
    0x06FF,
    0x0756,
    0x07AE,
    0x0808,
    0x0865,
    0x08C3,
    0x0924,
    0x0986,
    0x09EB,
    0x0A51,
    0x0ABA,
    0x0B25,
    0x0B91,
    0x0C00,
    0x0C71,
    0x0CE4,
    0x0D58,
    0x0DCF,
    0x0E48,
    0x0EC3,
    0x0F40,
    0x0FBF,
    0x1040,
    0x10C3,
    0x1148,
    0x11CF,
    0x1258,
    0x12E3,
    0x1371,
    0x1400,
    0x1491,
    0x1524,
    0x15BA,
    0x1651,
    0x16EA,
    0x1786,
    0x1823,
    0x18C3,
    0x1964,
    0x1A08,
    0x1AAD,
    0x1B55,
    0x1BFF,
    0x1CAA,
    0x1D58,
    0x1E08,
    0x1EB9,
    0x1F6D,
    0x2023,
    0x20DB,
    0x2195,
    0x2251,
    0x230F,
    0x23CF,
    0x2491,
    0x2555,
    0x261B,
    0x26E3,
    0x27AD,
    0x2879,
    0x2947,
    0x2A18,
    0x2AEA,
    0x2BBE,
    0x2C94,
    0x2D6D,
    0x2E47,
    0x2F24,
    0x3002,
    0x30E3,
    0x31C5,
    0x32AA,
    0x3390,
    0x3479,
    0x3563,
    0x3650,
    0x373F,
    0x3830,
    0x3922,
    0x3A17,
    0x3B0E,
    0x3C07,
    0x3D02,
    0x3DFF,
    0x3EFE,
    0x3FFF,
};

static s32 CdReady_Enqueue(_CdReadyEntry* arg0)
{
    u8             saved;
    s32            field2;
    s32            next;
    _CdReadyEntry* entry;
    _CdReadyQueue* queue;

    queue                = &CdReady_Queue;
    saved                = CdReady_Queue.locked;
    CdReady_Queue.locked = 1;

    field2 = queue->readIdx;
    next   = queue->writeIdx;
    next   = next + 1;
    if (next >= ARRAY_SIZE(queue->entries)) {
        next = 0;
    }

    if (next == field2) {
        CdReady_Queue.locked = saved;
        return 0;
    }

    entry           = &CdReady_Queue.entries[queue->writeIdx];
    entry->pollFn   = arg0->pollFn;
    entry->sector   = arg0->sector;
    entry->doneFn   = arg0->doneFn;
    entry->cancelFn = arg0->cancelFn;
    // The status bits are the queue's own: the caller supplies only the handlers and the sector.
    entry->active        = 1;
    entry->cancelled     = 0;
    entry->cancelPending = 0;
    entry->phase         = 0;
    entry->firstPoll     = 1;
    field2               = queue->writeIdx;
    queue->writeIdx      = next;
    CdReady_Queue.locked = saved;
    return field2 + 1;
}

static void CdReady_Poll(void)
{
    _CdReadyQueue* queue;
    _CdReadyEntry* entry;

    queue = &CdReady_Queue;
    if (queue->locked == 0 && queue->writeIdx != queue->readIdx) {
        entry = &CdReady_Queue.entries[queue->readIdx];
        if (entry->active) {
            if (entry->pollFn(entry) != 0) {
                if (entry->doneFn != NULL) {
                    entry->doneFn();
                }
                entry->active    = 0;
                entry->cancelled = 0;
                queue->readIdx   = queue->readIdx + 1;
                if (queue->readIdx >= ARRAY_SIZE(queue->entries)) {
                    queue->readIdx = 0;
                }
            }
        } else {
            // A cancelled job that had started is wound down before the queue moves on; one
            // that never ran is dropped. The two tests stay nested: joined by `&&` they
            // compile to a single masked compare.
            if (entry->cancelled) {
                if (!entry->firstPoll) {
                    if (entry->cancelFn != NULL) {
                        entry->cancelFn(entry);
                    }
                    if (entry->cancelPending) {
                        return;
                    }
                }
            }
            entry->cancelled      = 0;
            CdReady_Queue.readIdx = CdReady_Queue.readIdx + 1;
            if (CdReady_Queue.readIdx >= ARRAY_SIZE(CdReady_Queue.entries)) {
                CdReady_Queue.readIdx = 0;
            }
        }
    }
}

void CdStream_Start(CdStreamParams* params)
{
    _CdReadyEntry           entry;
    volatile CdStreamState* p;
    s32                     flag;
    volatile CdStreamState* ap;
    union {
        volatile CdStreamState* state;
        _CdReadyQueue*          queue;
    } a3;
    SpuVoiceAttr*  t0;
    SpuVoiceAttr*  ch1;
    s32            sectors;
    s16            volume;
    u8             saved;
    s16            f6;
    s16            idx;
    _CdReadyEntry* e;
    s32            one;
    s32            cflags;
    s16            vff;
    s16            v1fc3;
    s16            v1000;
    s32            temp;
    s8             channelCount;
    s32            spuBase;
    _CdReadyEntry* rem_tmp;
    s32            startSector;

    p         = &CdStream_Runtime.state;
    p->voiceL = params->voiceL;
    p->voiceR = params->voiceR;
    flag      = ((u8)CdStream_Runtime.state.flags0 >> CD_STREAM_VOICES_ON_BIT) & 1;
    if (flag == 1) {
        Spu_KeyOff((s8)p->voiceL);
        Spu_KeyOff((s8)p->voiceR);
        if (params->voiceFreeCb != NULL) {
            params->voiceFreeCb(
                (flag << (s8)p->voiceL) | (flag << (s8)p->voiceR));
        }
    }
    SpuSetIRQ(0);
    SpuSetIRQCallback(NULL);
    SpuSetTransferCallback(NULL);
    CdSyncCallback(NULL);

    ap                             = &CdStream_Runtime.state;
    *(s32*)&CdStream_Runtime.state = 0;
    if (ap->readySlot != 0) {
        a3.queue = &CdReady_Queue;
        f6       = ap->readySlot;
        saved    = CdReady_Queue.locked;
        if (f6 != 0) {
            idx = f6 - 1;
            e   = &CdReady_Queue.entries[idx];
            if (e->active) {
                e->active    = 0;
                e->cancelled = 1;
            }
            CdReady_Queue.locked = saved;
        }
        CdStream_Runtime.state.readySlot = 0;
    }

    a3.state                 = &CdStream_Runtime.state;
    a3.state->startCb        = params->startCb;
    a3.state->voiceFreeCb    = params->voiceFreeCb;
    a3.state->advanceDelay   = 0;
    a3.state->doneCb         = params->doneCb;
    a3.state->playhead       = 0;
    a3.state->startSector    = params->startSector;
    a3.state->expectedSector = params->startSector;
    one                      = 1;
    a3.state->readSector     = params->startSector;
    a3.state->expectedChunk  = 0;
    a3.state->chunkCount     = one;
    spuBase                  = params->spuBase;
    sectors                  = CD_STREAM_CHUNK_VSYNCS_SHORT_NTSC;
    {
        s32 ds            = gDisplayState.region;
        a3.state->spuBase = spuBase;
        if (ds == one) {
            sectors = CD_STREAM_CHUNK_VSYNCS_SHORT_PAL;
        }
    }
    a3.state->chunkVsyncs  = sectors;
    a3.state->ringHalf     = CD_STREAM_RING_HALF_SHORT;
    t0                     = PARENT_OF(a3.state, CdStreamRuntime, state)->channels.voiceAttr;
    a3.state->sector       = params->sectorBuf;
    a3.state->voiceL       = params->voiceL;
    vff                    = 0xFF;
    a3.state->voiceR       = params->voiceR;
    channelCount           = params->channelCount;
    v1fc3                  = 0x1FC3;
    v1000                  = 0x1000;
    cflags                 = 0x6009F;
    t0->mask               = cflags;
    t0[1].mask             = cflags;
    t0->volmode.left       = 0;
    t0->volmode.right      = 0;
    t0->pitch              = v1000;
    t0->adsr1              = vff;
    t0->adsr2              = v1fc3;
    t0[1].volmode.left     = 0;
    t0[1].volmode.right    = 0;
    t0[1].pitch            = v1000;
    a3.state->channelCount = channelCount;
    a3.state->chunkIndex   = 0;
    a3.state->queuedChunk  = 0;
    a3.state->reinitSlot   = 0;
    t0->voice              = one << a3.state->voiceL;
    t0->addr               = a3.state->spuBase;
    {
        s32 addr      = a3.state->spuBase + 0x10;
        s32 mask      = one << a3.state->voiceR;
        t0->loop_addr = addr;
        t0[1].voice   = mask;
    }
    temp       = a3.state->spuBase;
    temp       = temp + 0x40;
    temp       = temp + ((s32)((u16)a3.state->ringHalf << 16) >> 15);
    t0[1].addr = temp;
    temp       = a3.state->spuBase;
    {
        s32 shift   = (s32)((u16)a3.state->ringHalf << 16) >> 15;
        t0[1].adsr1 = vff;
        t0[1].adsr2 = v1fc3;
        temp        = temp + shift;
    }
    {
        u8 f53          = a3.state->flags;
        temp            = temp + 0x50;
        t0[1].loop_addr = temp;
        if (f53 & CD_STREAM_MONO) {
            ch1               = &t0[1];
            volume            = (params->volume * 0xB5) >> 8;
            ch1->volume.right = volume;
            ch1->volume.left  = volume;
            t0->volume.right  = volume;
            t0->volume.left   = volume;
        } else {
            u16 leftVolume;
            leftVolume         = params->volume;
            t0->volume.right   = 0;
            t0[1].volume.left  = 0;
            t0->volume.left    = leftVolume;
            t0[1].volume.right = params->volume;
        }
    }

    rem_tmp                          = &entry;
    entry.pollFn                     = CdStream_PollMtsRead;
    startSector                      = params->startSector;
    entry.doneFn                     = CdStream_Continue;
    entry.cancelFn                   = CdStream_FinishQueueEntry;
    entry.sector                     = startSector;
    CdStream_Runtime.state.readySlot = CdReady_Enqueue(rem_tmp);
    CdStream_Runtime.state.phase     = CD_STREAM_PHASE_READING;
    D_80068B74                       = -1;
}

static void CdStream_Continue(void)
{
    _CdReadyEntry           entry;
    volatile CdStreamState* p;

    CdStream_Runtime.state.readySlot = 0;

    if ((CdStream_Runtime.state.flags0 >> CD_STREAM_CHUNK_READY_BIT) & 1) {
        if (CdStream_Runtime.state.chunkIndex == 0) {
            CdStream_Runtime.state.phase  = CD_STREAM_PHASE_READY;
            CdStream_Runtime.state.flags1 = CdStream_Runtime.state.flags1 | CD_STREAM_VOICE_COPY;
            CdStream_Runtime.state.flags2 = CdStream_Runtime.state.flags2 & CD_STREAM_CLEAR_FAULT;
            CdStream_Runtime.state.flags2 = CdStream_Runtime.state.flags2 & CD_STREAM_CLEAR_CHUNK_ADVANCE;
            if (CdStream_Runtime.state.doneCb != NULL) {
                CdStream_Runtime.state.doneCb(1);
            }
            return;
        }
    }

    p              = &CdStream_Runtime.state;
    entry.pollFn   = CdStream_PollMtsRead;
    entry.doneFn   = CdStream_Continue;
    p->flags2      = p->flags2 & CD_STREAM_CLEAR_FAULT;
    entry.cancelFn = CdStream_FinishQueueEntry;
    entry.sector   = p->startSector;
    D_80068B63     = D_80068B63 + 1;
    p->readySlot   = CdReady_Enqueue(&entry);
}

void CdStream_Stop(void)
{
    u8                      saved;
    u8                      temp;
    s16                     arg0;
    s16                     idx;
    _CdReadyEntry*          entry;
    volatile CdStreamState* p;

    saved                = CdReady_Queue.locked;
    CdReady_Queue.locked = 1;

    if (CdStream_Runtime.state.flags0 & CD_STREAM_ACTIVE) {
        CdStream_Runtime.state.flags0 = CdStream_Runtime.state.flags0 | CD_STREAM_STOP;
        SpuSetIRQ(0);
        SpuSetIRQCallback(0);
    } else {
        if (CdStream_Runtime.state.readySlot != 0) {
            arg0 = CdStream_Runtime.state.readySlot;
            temp = CdReady_Queue.locked;
            if (arg0 != 0) {
                idx   = arg0 - 1;
                entry = &CdReady_Queue.entries[idx];
                if (entry->active) {
                    entry->active    = 0;
                    entry->cancelled = 1;
                }
                CdReady_Queue.locked = temp;
            }
            CdStream_Runtime.state.readySlot = 0;
        }

        p = &CdStream_Runtime.state;
        if ((p->flags2 >> CD_STREAM_HALT_BIT) & 1) {
            func_800B0118(0, 0);
            p->flags2 = p->flags2 & CD_STREAM_CLEAR_HALT;
        }
    }

    CdReady_Queue.locked = saved;
}

static void CdStream_TeardownVoices(void)
{
    _CdReadyEntry  entry;
    s32            flag;
    s16            slot;
    _CdReadyEntry* e;
    u8             saved;

    if (CdStream_Runtime.state.flags2 & CD_STREAM_REQUEUE) {
        CdStream_Runtime.state.queuedChunk;
        CdStream_Runtime.state.flags2 &= CD_STREAM_CLEAR_REQUEUE;
        CdStream_Runtime.state.flags0 &= CD_STREAM_CLEAR_ACTIVE;
        CdStream_Runtime.state.flags2 &= CD_STREAM_CLEAR_FAULT;
        CdStream_Runtime.state.flags0 &= CD_STREAM_CLEAR_KEY_ON;
        flag                           = (CdStream_Runtime.state.flags0 >> CD_STREAM_VOICES_ON_BIT) & 1;
        if (flag == 1) {
            Spu_KeyOff((s8)CdStream_Runtime.state.voiceL);
            Spu_KeyOff((s8)CdStream_Runtime.state.voiceR);
            CdStream_Runtime.state.flags0 &= CD_STREAM_CLEAR_VOICES_ON;
            if (CdStream_Runtime.state.voiceFreeCb != NULL) {
                CdStream_Runtime.state.voiceFreeCb((flag << (s8)CdStream_Runtime.state.voiceL) | (flag << (s8)CdStream_Runtime.state.voiceR));
            }
        }
        SpuSetIRQ(0);
        SpuSetIRQCallback(0);
        if (CdStream_Runtime.state.readySlot != 0) {
            slot  = CdStream_Runtime.state.readySlot;
            saved = CdReady_Queue.locked;
            if (slot != 0) {
                e = &CdReady_Queue.entries[(s16)(slot - 1)];
                if (e->active) {
                    e->active    = 0;
                    e->cancelled = 1;
                }
                CdReady_Queue.locked = saved;
            }
            CdStream_Runtime.state.readySlot = 0;
        }
        entry.pollFn                     = CdStream_PollMtsRead;
        entry.doneFn                     = CdStream_CompleteInitialRead;
        entry.sector                     = CdStream_Runtime.state.readSector;
        CdStream_Runtime.state.flags0   &= CD_STREAM_CLEAR_CHUNK_READY;
        entry.cancelFn                   = CdStream_FinishQueueEntry;
        CdStream_Runtime.state.readySlot = CdReady_Enqueue(&entry);
        CdStream_Runtime.state.phase     = CD_STREAM_PHASE_READING;
    }
}

static void CdStream_CompleteInitialRead(void)
{
    _CdReadyEntry           entry;
    volatile CdStreamState* p;
    s32                     pos;
    s32                     chunk;

    p            = &CdStream_Runtime.state;
    pos          = p->playhead;
    chunk        = pos / p->chunkVsyncs + 1;
    p->readySlot = 0;

    if ((CdStream_Runtime.state.flags0 >> CD_STREAM_CHUNK_READY_BIT) & 1) {
        p->phase  = CD_STREAM_PHASE_READY;
        p->flags1 = p->flags1 | CD_STREAM_VOICE_COPY;
        p->flags2 = p->flags2 & CD_STREAM_CLEAR_FAULT;
        if (p->chunkIndex & 1) {
            p->advanceDelay = 0;
        } else {
            p->advanceDelay               = p->chunkVsyncs - pos % p->chunkVsyncs + 1;
            CdStream_Runtime.state.flags0 = CdStream_Runtime.state.flags0 | CD_STREAM_KEY_ON;
        }
        CdStream_CleanupIrq();
    } else {
        p->flags2      = p->flags2 & CD_STREAM_CLEAR_FAULT;
        D_80068B63     = D_80068B63 + 1;
        entry.pollFn   = CdStream_PollMtsRead;
        entry.doneFn   = CdStream_CompleteInitialRead;
        entry.cancelFn = CdStream_FinishQueueEntry;
        entry.sector   = p->readSector;
        p->queuedChunk = chunk;
        p->readySlot   = CdReady_Enqueue(&entry);
    }
}

static void CdStream_CleanupIrq(void)
{
    volatile CdStreamState* p;
    u8                      temp;

    if (D_80068B5C != 0) {
        SpuSetIRQ(0);
        SpuSetIRQCallback(0);
        D_80068B5C = 0;
    }
    CdStream_Runtime.state.flags0 = CdStream_Runtime.state.flags0 & CD_STREAM_CLEAR_SPU_IRQ;
    CdStream_Runtime.state.flags0 = CdStream_Runtime.state.flags0 & CD_STREAM_CLEAR_STOP;
    func_800B0118(0, 0);
    p                             = &CdStream_Runtime.state;
    temp                          = p->flags2;
    p->flags2                     = temp & 0xF7;
    CdStream_ErrorCode            = 0;
    CdStream_Runtime.state.flags0 = CdStream_Runtime.state.flags0 | CD_STREAM_ACTIVE;
}

static void CdStream_TickPlayback(void)
{
    _CdReadyEntry entry;
    union {
        volatile CdStreamState* state;
        s32                     position;
    } stateOrPosition;
    s16            slot;
    u8             saved;
    _CdReadyEntry* queued;
    SpuVoiceAttr*  channels;
    s32            nextChunk;

    stateOrPosition.position = CdStream_Runtime.state.playhead;
    if (!(((u8)CdStream_Runtime.state.flags0 >> CD_STREAM_VOICES_ON_BIT) & 1) && (((u8)CdStream_Runtime.state.flags0 >> CD_STREAM_KEY_ON_BIT) & 1)) {
        CdAudio_AllocVoices((s8*)&CdStream_Runtime.state.voiceL, (s8*)&CdStream_Runtime.state.voiceR);
        channels          = PARENT_OF(&CdStream_Runtime.state, CdStreamRuntime, state)->channels.voiceAttr;
        channels->voice   = (s32)(1 << (s8)CdStream_Runtime.state.voiceL);
        channels[1].voice = (s32)(1 << CdStream_Runtime.state.voiceR);
        Spu_ArmKeyOn((s8)CdStream_Runtime.state.voiceL);
        Spu_ArmKeyOn((s8)CdStream_Runtime.state.voiceR);
        channels->mask    = 0x6009F;
        channels[1].mask  = 0x6009F;
        channels->adsr1   = 0xFF;
        channels->adsr2   = 0x1FC3;
        channels[1].adsr1 = 0xFF;
        channels[1].adsr2 = 0x1FC3;
        CdAudio_CopyVoiceData((s8)CdStream_Runtime.state.voiceL, channels);
        /* The channels follow CdStream_Runtime.state; addressing them from its symbol
         * shares its high half, where &CdStream_Runtime.channels would load another. */
        CdAudio_CopyVoiceData((s8)CdStream_Runtime.state.voiceR, PARENT_OF(&CdStream_Runtime.state, CdStreamRuntime, state)->channels.voiceAttr + 1);
        CdStream_Runtime.state.flags1 &= CD_STREAM_CLEAR_VOICE_COPY;
        if (CdStream_Runtime.state.startCb != NULL) {
            CdStream_Runtime.state.startCb((1 << CdStream_Runtime.state.voiceL) | (1 << CdStream_Runtime.state.voiceR));
        }
        CdStream_Runtime.state.flags0 |= CD_STREAM_VOICES_ON;
        CdStream_Runtime.state.flags0 &= CD_STREAM_CLEAR_KEY_ON;
    }
    stateOrPosition.state = &CdStream_Runtime.state;
    if ((stateOrPosition.state->queuedChunk + 1) < stateOrPosition.state->chunkCount) {
        if (((u8)stateOrPosition.state->flags0 >> CD_STREAM_SPU_IRQ_BIT) & 1) {
            stateOrPosition.state->playhead = stateOrPosition.state->chunkIndex * (s16)(u16)stateOrPosition.state->chunkVsyncs;
            stateOrPosition.position        = stateOrPosition.state->playhead;
        } else {
            stateOrPosition.position = stateOrPosition.state->playhead;
        }
        if (!(((u8)CdStream_Runtime.state.flags0 >> CD_STREAM_CHUNK_READY_BIT) & 1)) {
            D_80068B5E = (u8)(D_80068B5E + 1);
        stopVoices:
            if (((u8)CdStream_Runtime.state.flags0 >> CD_STREAM_VOICES_ON_BIT) & 1) {
                Spu_KeyOff((u32)(s8)CdStream_Runtime.state.voiceL);
                Spu_KeyOff((u32)(s8)CdStream_Runtime.state.voiceR);
                if (CdStream_Runtime.state.voiceFreeCb != NULL) {
                    CdStream_Runtime.state.voiceFreeCb((1 << (s8)CdStream_Runtime.state.voiceL) | (1 << CdStream_Runtime.state.voiceR));
                }
                CdStream_Runtime.state.flags0 &= CD_STREAM_CLEAR_VOICES_ON;
            }
            if (CdStream_Runtime.state.flags0 & CD_STREAM_ACTIVE) {
                if (CdStream_ErrorCode == 0) {
                    CdStream_ErrorCode = 6;
                }
                CdStream_LastErrorCode         = CdStream_ErrorCode;
                CdStream_Runtime.state.flags0 &= CD_STREAM_CLEAR_ACTIVE;
                func_800B0118((s32)(s16)CdStream_ErrorCode, 0);
                CdStream_ErrorCode             = 0;
                CdStream_Runtime.state.flags2 |= CD_STREAM_HALT;
                CdStream_Runtime.state.flags2 |= CD_STREAM_REQUEUE;
                CdStream_Runtime.state.flags2 |= CD_STREAM_CHUNK_ADVANCE;
                CdStream_TeardownVoices();
            }
            CdStream_Runtime.state.flags0 &= CD_STREAM_CLEAR_KEY_ON;
            CdStream_Runtime.state.flags0 &= CD_STREAM_CLEAR_SPU_IRQ;
            return;
        }
        nextChunk                             = stateOrPosition.position / CdStream_Runtime.state.chunkVsyncs;
        CdStream_Runtime.state.expectedChunk += 1;
        nextChunk                            += 1;
        if (nextChunk != CdStream_Runtime.state.expectedChunk) {
            D_80068B67                          += 1;
            nextChunk                           -= 1;
            CdStream_Runtime.state.expectedChunk = nextChunk;
            CdStream_Runtime.state.queuedChunk   = nextChunk;
            CdStream_Runtime.state.readSector    = CdStream_Runtime.state.startSector + ((s8)(u8)CdStream_Runtime.state.mtsPeriod * (s8)(u8)CdStream_Runtime.state.channelCount * nextChunk);
            if (CdStream_ErrorCode == 0) {
                CdStream_ErrorCode = 0xD;
            }
            goto stopVoices;
        }
        if (nextChunk < CdStream_Runtime.state.chunkCount) {
            entry.pollFn   = CdStream_PollMtsRead;
            entry.doneFn   = _cdStreamClearReadySlot;
            entry.cancelFn = CdStream_AbortPhase;
            if ((u16)CdStream_Runtime.state.readySlot != 0) {
                slot  = CdStream_Runtime.state.readySlot;
                saved = CdReady_Queue.locked;
                if (slot != 0) {
                    queued = &CdReady_Queue.entries[(s16)(slot - 1)];
                    if (queued->active) {
                        queued->active    = 0;
                        queued->cancelled = 1;
                    }
                    CdReady_Queue.locked = saved;
                }
                CdStream_Runtime.state.readySlot = 0;
            }
            CdStream_Runtime.state.readSector += (s8)(u8)CdStream_Runtime.state.mtsPeriod * (s8)(u8)CdStream_Runtime.state.channelCount;
            if (((u8)CdStream_Runtime.state.flags1 >> CD_STREAM_APPLY_GAP_BIT) & 1) {
                CdStream_Runtime.state.readSector += (s8)CdStream_Runtime.state.gapSectors;
            }
            entry.sector                       = CdStream_Runtime.state.readSector;
            CdStream_Runtime.state.queuedChunk = nextChunk;
            CdStream_Runtime.state.flags0     &= CD_STREAM_CLEAR_CHUNK_READY;
            CdStream_Runtime.state.readySlot   = CdReady_Enqueue(&entry);
            CdStream_Runtime.state.phase       = CD_STREAM_PHASE_READING;
        }
        CdStream_Runtime.state.flags0 &= CD_STREAM_CLEAR_SPU_IRQ;
    }
}

static void CdStream_CompleteChunkRead(void)
{
    _CdReadyEntry  entry;
    s32            position;
    u16            slot;
    u8             saved;
    _CdReadyEntry* queued;

    position = CdStream_Runtime.state.playhead;
    if ((CdStream_Runtime.state.queuedChunk + 1) < CdStream_Runtime.state.chunkCount) {
        if (((u8)CdStream_Runtime.state.flags0 >> CD_STREAM_SPU_IRQ_BIT) & 1) {
            CdStream_Runtime.state.advanceDelay = (u16)CdStream_Runtime.state.chunkVsyncs + 1;
            CdStream_Runtime.state.playhead     = CdStream_Runtime.state.chunkIndex * (s16)(u16)CdStream_Runtime.state.chunkVsyncs;
            position                            = CdStream_Runtime.state.playhead;
        } else {
            CdStream_Runtime.state.advanceDelay = (u16)CdStream_Runtime.state.chunkVsyncs - 2;
            position                            = CdStream_Runtime.state.playhead;
        }
        if (!(((u8)CdStream_Runtime.state.flags0 >> CD_STREAM_CHUNK_READY_BIT) & 1)) {
            D_80068B5E += 1;
        stopVoices:
            if (((u8)CdStream_Runtime.state.flags0 >> CD_STREAM_VOICES_ON_BIT) & 1) {
                Spu_KeyOff((u32)(s8)CdStream_Runtime.state.voiceL);
                Spu_KeyOff((u32)(s8)CdStream_Runtime.state.voiceR);
                if (CdStream_Runtime.state.voiceFreeCb != NULL) {
                    CdStream_Runtime.state.voiceFreeCb((1 << (s8)CdStream_Runtime.state.voiceL) | (1 << CdStream_Runtime.state.voiceR));
                }
                CdStream_Runtime.state.flags0 &= CD_STREAM_CLEAR_VOICES_ON;
            }
            CdStream_Runtime.state.advanceDelay = 0;
            CdStream_Runtime.state.flags0      &= CD_STREAM_CLEAR_KEY_ON;
            if (CdStream_Runtime.state.flags0 & CD_STREAM_ACTIVE) {
                if (CdStream_ErrorCode == 0) {
                    CdStream_ErrorCode = 6;
                }
                CdStream_LastErrorCode         = CdStream_ErrorCode;
                CdStream_Runtime.state.flags0 &= CD_STREAM_CLEAR_ACTIVE;
                func_800B0118((s32)(s16)CdStream_ErrorCode, 0);
                CdStream_ErrorCode             = 0;
                CdStream_Runtime.state.flags2 |= CD_STREAM_HALT;
                CdStream_Runtime.state.flags2 |= CD_STREAM_REQUEUE;
                CdStream_Runtime.state.flags2 |= CD_STREAM_CHUNK_ADVANCE;
                if (((u8)CdStream_Runtime.state.flags0 >> CD_STREAM_SPU_IRQ_BIT) & 1) {
                    CdStream_Runtime.state.playhead = CdStream_Runtime.state.chunkIndex * (s16)(u16)CdStream_Runtime.state.chunkVsyncs;
                }
                CdStream_TeardownVoices();
            }
        } else {
            position                              = position / (s16)(u16)CdStream_Runtime.state.chunkVsyncs;
            CdStream_Runtime.state.expectedChunk += 1;
            position                             += 1;
            if (position != CdStream_Runtime.state.expectedChunk) {
                D_80068B67                          += 1;
                position                            -= 1;
                CdStream_Runtime.state.expectedChunk = position;
                CdStream_Runtime.state.queuedChunk   = position;
                CdStream_Runtime.state.readSector    = CdStream_Runtime.state.startSector + ((s8)(u8)CdStream_Runtime.state.mtsPeriod * (s8)(u8)CdStream_Runtime.state.channelCount * position);
                if (CdStream_ErrorCode == 0) {
                    CdStream_ErrorCode = 0xD;
                }
                goto stopVoices;
            }
            if (position < CdStream_Runtime.state.chunkCount) {
                entry.pollFn   = CdStream_PollMtsRead;
                entry.doneFn   = _cdStreamClearReadySlot;
                entry.cancelFn = CdStream_AbortPhase;
                if ((u16)CdStream_Runtime.state.readySlot != 0) {
                    slot  = (u16)CdStream_Runtime.state.readySlot;
                    saved = CdReady_Queue.locked;
                    if (slot != 0) {
                        queued = &CdReady_Queue.entries[(s16)(slot - 1)];
                        if (queued->active) {
                            queued->active    = 0;
                            queued->cancelled = 1;
                        }
                        CdReady_Queue.locked = saved;
                    }
                    CdStream_Runtime.state.readySlot = 0;
                }
                CdStream_Runtime.state.readSector += (s8)(u8)CdStream_Runtime.state.mtsPeriod * (s8)(u8)CdStream_Runtime.state.channelCount;
                if (((u8)CdStream_Runtime.state.flags1 >> CD_STREAM_APPLY_GAP_BIT) & 1) {
                    CdStream_Runtime.state.readSector += (s8)CdStream_Runtime.state.gapSectors;
                }
                entry.sector                       = CdStream_Runtime.state.readSector;
                CdStream_Runtime.state.queuedChunk = position;
                CdStream_Runtime.state.flags0     &= CD_STREAM_CLEAR_CHUNK_READY;
                CdStream_Runtime.state.readySlot   = CdReady_Enqueue(&entry);
                CdStream_Runtime.state.phase       = CD_STREAM_PHASE_READING;
            }
        }
        CdStream_Runtime.state.flags0 &= CD_STREAM_CLEAR_SPU_IRQ;
    }
}

void CdStream_Drive(void)
{
    _CdReadyEntry* restartEntry;
    _CdReadyEntry* stopEntry;
    _CdReadyEntry* endEntry;
    s32            position;
    u16            restartSlot, stopSlot, endSlot;
    u8             restartLock, stopLock, endLock;
    s32            phase;
    SpuVoiceAttr*  channels;

    if (D_80068B58 == 0) {
        D_80068B58 = 1;
        if (CdStream_Runtime.state.flags0 & CD_STREAM_ACTIVE) {
            if (((u8)CdStream_Runtime.state.flags2 >> CD_STREAM_FAULT_BIT) & 1) {
                CdStream_Runtime.state.flags0 &= CD_STREAM_CLEAR_ACTIVE;
                func_800B0118((s32)(s16)CdStream_ErrorCode, 0);
                CdStream_Runtime.state.flags2 |= CD_STREAM_HALT;
                CdStream_ErrorCode             = 0;
                CdStream_Runtime.state.flags2 |= CD_STREAM_REQUEUE;
                CdStream_TeardownVoices();
            } else {
                if (((u8)CdStream_Runtime.state.flags1 >> CD_STREAM_SEEK_BIT) & 1) {
                    if (((u8)CdStream_Runtime.state.flags0 >> CD_STREAM_VOICES_ON_BIT) & 1) {
                        Spu_KeyOff((u32)(s8)CdStream_Runtime.state.voiceL);
                        Spu_KeyOff((u32)(s8)CdStream_Runtime.state.voiceR);
                        if (CdStream_Runtime.state.voiceFreeCb != NULL) {
                            CdStream_Runtime.state.voiceFreeCb((1 << CdStream_Runtime.state.voiceL) | (1 << CdStream_Runtime.state.voiceR));
                        }
                        CdStream_Runtime.state.flags0 &= CD_STREAM_CLEAR_VOICES_ON;
                    }
                    if ((u16)CdStream_Runtime.state.readySlot != 0) {
                        restartSlot = (u16)CdStream_Runtime.state.readySlot;
                        restartLock = CdReady_Queue.locked;
                        if (restartSlot != 0) {
                            restartEntry = &CdReady_Queue.entries[(s16)(restartSlot - 1)];
                            if (restartEntry->active) {
                                restartEntry->active    = 0;
                                restartEntry->cancelled = 1;
                            }
                            CdReady_Queue.locked = restartLock;
                        }
                        CdStream_Runtime.state.readySlot = 0;
                    }
                    CdStream_Runtime.state.flags0      &= CD_STREAM_CLEAR_CHUNK_READY;
                    CdStream_Runtime.state.flags0      &= CD_STREAM_CLEAR_KEY_ON;
                    CdStream_Runtime.state.advanceDelay = 0;
                    CdStream_Runtime.state.playhead     = CdStream_Runtime.state.requestedPlayhead;
                    CdStream_Runtime.state.flags1      &= CD_STREAM_CLEAR_SEEK;
                    CdStream_Runtime.state.flags0      |= CD_STREAM_ENGAGED;
                }
                if (((u8)CdStream_Runtime.state.flags0 >> CD_STREAM_STOP_BIT) & 1) {
                    if (((u8)CdStream_Runtime.state.flags0 >> CD_STREAM_VOICES_ON_BIT) & 1) {
                        Spu_KeyOff((u32)(s8)CdStream_Runtime.state.voiceL);
                        Spu_KeyOff((u32)(s8)CdStream_Runtime.state.voiceR);
                        if (CdStream_Runtime.state.voiceFreeCb != NULL) {
                            CdStream_Runtime.state.voiceFreeCb((1 << (s8)CdStream_Runtime.state.voiceL) | (1 << CdStream_Runtime.state.voiceR));
                        }
                        CdStream_Runtime.state.flags0 &= CD_STREAM_CLEAR_VOICES_ON;
                    }
                    CdStream_Runtime.state.flags0 &= CD_STREAM_CLEAR_CHUNK_READY;
                    CdStream_Runtime.state.flags0 &= CD_STREAM_CLEAR_KEY_ON;
                    CdStream_Runtime.state.flags0 &= CD_STREAM_CLEAR_ACTIVE;
                    if ((u16)CdStream_Runtime.state.readySlot != 0) {
                        stopSlot = (u16)CdStream_Runtime.state.readySlot;
                        stopLock = CdReady_Queue.locked;
                        if (stopSlot != 0) {
                            stopEntry = &CdReady_Queue.entries[(s16)(stopSlot - 1)];
                            if (stopEntry->active) {
                                stopEntry->active    = 0;
                                stopEntry->cancelled = 1;
                            }
                            CdReady_Queue.locked = stopLock;
                        }
                        CdStream_Runtime.state.readySlot = 0;
                    }
                    if (((u8)CdStream_Runtime.state.flags2 >> CD_STREAM_HALT_BIT) & 1) {
                        func_800B0118(0, 0);
                        CdStream_Runtime.state.flags2 &= CD_STREAM_CLEAR_HALT;
                    }
                    CdStream_Runtime.state.flags0 &= CD_STREAM_CLEAR_STOP;
                    CdStream_Runtime.state.flags2 |= CD_STREAM_REQUEUE;
                } else if (!(((u8)CdStream_Runtime.state.flags0 >> CD_STREAM_ENGAGED_BIT) & 1)) {
                    phase = (s8)CdStream_Runtime.state.phase;
                    if (phase == CD_STREAM_PHASE_READY) {
                        CdStream_Runtime.state.flags0      |= CD_STREAM_ENGAGED;
                        CdStream_Runtime.state.advanceDelay = phase;
                        D_80068B60                          = phase;
                    }
                } else {
                    position = CdStream_Runtime.state.playhead;
                    if (position == 0) {
                        channels = PARENT_OF(&CdStream_Runtime.state, CdStreamRuntime, state)->channels.voiceAttr;
                        CdAudio_AllocVoices((s8*)&CdStream_Runtime.state.voiceL, (s8*)&CdStream_Runtime.state.voiceR);
                        channels->voice   = (s32)(1 << (s8)CdStream_Runtime.state.voiceL);
                        channels[1].voice = (s32)(1 << CdStream_Runtime.state.voiceR);
                        if (!(((u8)CdStream_Runtime.state.flags0 >> CD_STREAM_VOICES_ON_BIT) & 1)) {
                            Spu_ArmKeyOn((u32)(s8)CdStream_Runtime.state.voiceL);
                            Spu_ArmKeyOn((u32)(s8)CdStream_Runtime.state.voiceR);
                        }
                        if (CdStream_Runtime.state.startCb != NULL) {
                            CdStream_Runtime.state.startCb((1 << CdStream_Runtime.state.voiceL) | (1 << CdStream_Runtime.state.voiceR));
                        }
                        CdStream_Runtime.state.flags0 |= CD_STREAM_VOICES_ON;
                        CdStream_Runtime.state.flags1 |= CD_STREAM_SEEK_ENABLED;
                    }
                    if (CdStream_Runtime.state.flags1 & CD_STREAM_VOICE_COPY) {
                        CdAudio_CopyVoiceData((s8)CdStream_Runtime.state.voiceL, PARENT_OF(&CdStream_Runtime.state, CdStreamRuntime, state)->channels.voiceAttr);
                        /* The channels follow CdStream_Runtime.state; addressing them from its symbol
                         * shares its high half, where &CdStream_Runtime.channels would load another. */
                        CdAudio_CopyVoiceData((s8)CdStream_Runtime.state.voiceR, PARENT_OF(&CdStream_Runtime.state, CdStreamRuntime, state)->channels.voiceAttr + 1);
                        CdStream_Runtime.state.flags1 &= CD_STREAM_CLEAR_VOICE_COPY;
                    }
                    // On the final chunk, stop on entry, halfway through, or after it plays out.
                    if ((position / (s16)(u16)CdStream_Runtime.state.chunkVsyncs >= CdStream_Runtime.state.chunkCount - 1) &&
                        ((((u8)CdStream_Runtime.state.flags1 >> CD_STREAM_END_ON_ENTRY_BIT) & 1) ||
                         ((((u8)CdStream_Runtime.state.flags1 >> CD_STREAM_END_THROUGH_BIT) & 1) ? (position / (s16)(u16)CdStream_Runtime.state.chunkVsyncs >= CdStream_Runtime.state.chunkCount) : (position % (s16)(u16)CdStream_Runtime.state.chunkVsyncs >= (CdStream_Runtime.state.chunkVsyncs >> 1))) ||
                         (position / (s16)(u16)CdStream_Runtime.state.chunkVsyncs >= CdStream_Runtime.state.chunkCount))) {
                        if (((u8)CdStream_Runtime.state.flags0 >> CD_STREAM_VOICES_ON_BIT) & 1) {
                            Spu_KeyOff((u32)(s8)CdStream_Runtime.state.voiceL);
                            Spu_KeyOff((u32)(s8)CdStream_Runtime.state.voiceR);
                            if (CdStream_Runtime.state.voiceFreeCb != NULL) {
                                CdStream_Runtime.state.voiceFreeCb((1 << (s8)CdStream_Runtime.state.voiceL) | (1 << CdStream_Runtime.state.voiceR));
                            }
                            CdStream_Runtime.state.flags0 &= CD_STREAM_CLEAR_VOICES_ON;
                        }
                        if ((u16)CdStream_Runtime.state.readySlot != 0) {
                            endSlot = (u16)CdStream_Runtime.state.readySlot;
                            endLock = CdReady_Queue.locked;
                            if (endSlot != 0) {
                                endEntry = &CdReady_Queue.entries[(s16)(endSlot - 1)];
                                if (endEntry->active) {
                                    endEntry->active    = 0;
                                    endEntry->cancelled = 1;
                                }
                                CdReady_Queue.locked = endLock;
                            }
                            CdStream_Runtime.state.readySlot = 0;
                        }
                        if (((u8)CdStream_Runtime.state.flags2 >> CD_STREAM_HALT_BIT) & 1) {
                            func_800B0118(0, 0);
                            CdStream_Runtime.state.flags2 &= CD_STREAM_CLEAR_HALT;
                        }
                        if (D_80068B5C != 0) {
                            SpuSetIRQ(0);
                            SpuSetIRQCallback(NULL);
                            D_80068B5C = 0;
                        }
                        CdStream_Runtime.state.flags0 &= CD_STREAM_CLEAR_CHUNK_READY;
                        CdStream_Runtime.state.flags0 &= CD_STREAM_CLEAR_KEY_ON;
                        CdStream_Runtime.state.flags0 &= CD_STREAM_CLEAR_ACTIVE;
                    } else {
                        if (((u8)CdStream_Runtime.state.flags2 >> CD_STREAM_CHUNK_ADVANCE_BIT) & 1) {
                            if (CdStream_Runtime.state.chunkIndex & 1) {
                                if (D_80068B5C != 0) {
                                    SpuSetIRQ(0);
                                    SpuSetIRQCallback(NULL);
                                    D_80068B5C = 0;
                                }
                                CdStream_CompleteChunkRead();
                                CdStream_Runtime.state.advanceDelay += 2;
                            } else {
                                if (D_80068B5C != 0) {
                                    SpuSetIRQ(0);
                                    SpuSetIRQCallback(NULL);
                                    D_80068B5C = 0;
                                }
                                CdStream_Runtime.state.advanceDelay = 0;
                                CdStream_TickPlayback();
                            }
                            CdStream_Runtime.state.flags2 &= CD_STREAM_CLEAR_CHUNK_ADVANCE;
                        } else if (CdStream_Runtime.state.advanceDelay != 0) {
                            CdStream_Runtime.state.advanceDelay -= 1;
                            if ((s8)CdStream_Runtime.state.advanceDelay < (CdStream_Runtime.state.chunkVsyncs >> 2)) {
                                if ((((u8)CdStream_Runtime.state.flags0 >> CD_STREAM_SPU_IRQ_BIT) & 1) || (CdStream_Runtime.state.advanceDelay == 0)) {
                                    if ((u8)D_80068B60 != 0) {
                                        D_80068B60 = 0;
                                    } else if (!(((u8)CdStream_Runtime.state.flags0 >> CD_STREAM_SPU_IRQ_BIT) & 1)) {
                                        D_80068B61 += 1;
                                    }
                                    if (D_80068B5C != 0) {
                                        SpuSetIRQ(0);
                                        SpuSetIRQCallback(NULL);
                                        D_80068B5C = 0;
                                    }
                                    CdStream_Runtime.state.advanceDelay = 0;
                                    CdStream_TickPlayback();
                                }
                            }
                        } else if (((CdStream_Runtime.state.playhead % (CdStream_Runtime.state.chunkVsyncs * 2)) >= ((s16)(u16)CdStream_Runtime.state.chunkVsyncs - 4)) && ((((u8)CdStream_Runtime.state.flags0 >> CD_STREAM_SPU_IRQ_BIT) & 1) || ((CdStream_Runtime.state.playhead % (CdStream_Runtime.state.chunkVsyncs * 2)) == ((s16)(u16)CdStream_Runtime.state.chunkVsyncs + 3)))) {
                            if (!(((u8)CdStream_Runtime.state.flags0 >> CD_STREAM_SPU_IRQ_BIT) & 1)) {
                                D_80068B61 += 1;
                            }
                            if (D_80068B5C != 0) {
                                SpuSetIRQ(0);
                                SpuSetIRQCallback(NULL);
                                D_80068B5C = 0;
                            }
                            CdStream_CompleteChunkRead();
                        }
                        if (CdStream_Runtime.state.flags0 & CD_STREAM_ACTIVE) {
                            CdStream_Runtime.state.playhead += 1;
                        }
                    }
                }
            }
        }
        CdReady_Poll();
        D_80068B58 = 0;
    }
}

static s32 CdStream_PollMtsRead(_CdReadyEntry* entry)
{
    struct {
        CdlLOC       loc;
        u8           pad[4];
        s8           mode[8];
        u8           result[8];
        AsyncCbEntry entry;
    } sp;
    s32 phase;
    s32 modeSync;
    s32 locationSync;
    s32 readSync;
    s32 pauseSync;
    u32 irqAddr1;
    u32 irqAddr2;
    u32 irqOffset;

    if (entry->firstPoll) {
        entry->firstPoll      = 0;
        CdStream_PhaseTimeout = 0;
        if (!(CdStream_Runtime.state.flags & CD_STREAM_DISC_FAULT) && CdStream_Runtime.state.reinitSlot == 0) {
            if ((CdStream_Runtime.state.flags1 >> CD_STREAM_MODE_SET_BIT) & 1) {
                entry->speedChange = 0;
                entry->phase       = CD_STREAM_STEP_SETLOC;
            } else {
                if (!(CdMode() & CdlModeSpeed)) {
                    entry->speedChange = 1;
                } else {
                    entry->speedChange = 0;
                }
                entry->phase = CD_STREAM_STEP_SETMODE;
            }
        } else {
            entry->phase = CD_STREAM_STEP_SETLOC;
            if (CdStream_Runtime.state.reinitSlot != 0) {
                AsyncCb_Cancel((s16)CdStream_Runtime.state.reinitSlot);
            }
            sp.entry.pollFn                   = CdStream_InitDisc;
            sp.entry.doneFn                   = CdStream_MarkEnding;
            sp.entry.cancelFn                 = _cdStreamCancelDiscInit;
            CdStream_Runtime.state.reinitSlot = AsyncCb_Enqueue(&sp.entry);
        }
    }
    CdStream_CurrentPhase = entry->phase;
    if (CdStream_Runtime.state.reinitSlot != 0) {
        return 0;
    }
    phase = entry->phase;
    switch (phase) {
        case CD_STREAM_STEP_SETMODE:
            sp.mode[0] = -0x60;
        retry_mode:
            CdControlF(CdlSetmode, (u8*)&sp.mode[0]);
            entry->phase = CD_STREAM_STEP_SETMODE_SYNC;
            goto phase_advanced;
        case CD_STREAM_STEP_SETMODE_SYNC:
            modeSync = CdSync(1, &sp.result[0]);
            if (modeSync == CdlDiskError) {
                D_80068B64 += 1;
                if (sp.result[0] & CdlStatShellOpen) {
                    if (CdStream_ErrorCode == 0) {
                        CdStream_ErrorCode = (u16)modeSync;
                    }
                    CdStream_LastErrorCode        = CdStream_ErrorCode;
                    CdStream_Runtime.state.flags |= CD_STREAM_DISC_FAULT;
                    CdStream_ShellOpenErrors++;
                    goto stream_error;
                }
                if (CdStatus() & CdlStatShellOpen) {
                    if (CdStream_ErrorCode == 0) {
                        CdStream_ErrorCode = (u16)modeSync;
                    }
                    CdStream_LastErrorCode        = CdStream_ErrorCode;
                    CdStream_Runtime.state.flags |= CD_STREAM_DISC_FAULT;
                    CdStream_ShellOpenErrors++;
                    goto stream_error;
                }
                if (CdStream_PhaseTimeout++ < 0x258) {
                    goto retry_mode;
                }
                goto stream_error;
            }
            CdStream_Runtime.state.flags1 |= CD_STREAM_MODE_SET;
            if (!entry->speedChange) {
                entry->phase = CD_STREAM_STEP_SETLOC;
            } else {
                entry->phase          = CD_STREAM_STEP_SPEED_SETTLE;
                CdStream_PhaseTimeout = 3;
                case CD_STREAM_STEP_SPEED_SETTLE:
                    if (--CdStream_PhaseTimeout != 0) {
                        goto phase_advanced;
                    }
                    entry->phase = CD_STREAM_STEP_SETLOC;
            }
            CdStream_PhaseTimeout = 0;
        case CD_STREAM_STEP_SETLOC:
            D_80068B54                            = 0;
            CdStream_Runtime.state.readPhase      = CD_STREAM_READ_SETLOC;
            CdStream_Runtime.state.expectedSector = entry->sector;
        set_location:
            CdIntToPos(entry->sector, &sp.loc);
            CdControlF(CdlSetloc, &sp.loc.minute);
            CdStream_PhaseTimeout = 0;
            entry->phase          = CD_STREAM_STEP_SETLOC_SYNC;
        case CD_STREAM_STEP_SETLOC_SYNC:
            locationSync = CdSync(1, &sp.result[0]);
            if (locationSync == CdlDiskError) {
                D_80068B64 += 1;
                if (sp.result[0] & CdlStatShellOpen) {
                    if (CdStream_ErrorCode == 0) {
                        CdStream_ErrorCode = (u16)locationSync;
                    }
                    CdStream_LastErrorCode        = CdStream_ErrorCode;
                    CdStream_Runtime.state.flags |= CD_STREAM_DISC_FAULT;
                    CdStream_ShellOpenErrors++;
                    goto stream_error;
                }
                if (CdStatus() & CdlStatShellOpen) {
                    if (CdStream_ErrorCode == 0) {
                        CdStream_ErrorCode = (u16)locationSync;
                    }
                    CdStream_LastErrorCode        = CdStream_ErrorCode;
                    CdStream_Runtime.state.flags |= CD_STREAM_DISC_FAULT;
                    CdStream_ShellOpenErrors++;
                    goto stream_error;
                }
                if (CdStream_PhaseTimeout++ < 0x258) {
                    goto set_location;
                }
                goto stream_error;
            }
            if (locationSync != CdlComplete) {
                goto wait_for_progress;
            }
            entry->phase = CD_STREAM_STEP_READ;
        case CD_STREAM_STEP_READ:
            CdStream_PhaseTimeout = 0;
        start_read:
            CdReady_InstallCallback(CdStream_ReadyMts);
            CdStream_Runtime.state.sectorsLeft = 0;
            CdStream_Runtime.state.readPhase   = CD_STREAM_READ_AUDIO;
            CdControlF(CdlReadN, NULL);
            entry->phase = CD_STREAM_STEP_READ_SYNC;
            goto phase_advanced;
        case CD_STREAM_STEP_READ_SYNC:
            D_80068B54 = 0;
            readSync   = CdSync(1, &sp.result[0]);
            if (readSync == CdlDiskError) {
                CdReady_ClearCallback();
                entry->phase = CD_STREAM_STEP_READ;
                if (sp.result[0] & CdlStatShellOpen) {
                    if (CdStream_ErrorCode == 0) {
                        CdStream_ErrorCode = (u16)readSync;
                    }
                    CdStream_LastErrorCode        = CdStream_ErrorCode;
                    CdStream_Runtime.state.flags |= CD_STREAM_DISC_FAULT;
                    CdStream_ShellOpenErrors++;
                    goto stream_error;
                }
                if (CdStatus() & CdlStatShellOpen) {
                    if (CdStream_ErrorCode == 0) {
                        CdStream_ErrorCode = (u16)readSync;
                    }
                    CdStream_LastErrorCode        = CdStream_ErrorCode;
                    CdStream_Runtime.state.flags |= CD_STREAM_DISC_FAULT;
                    CdStream_ShellOpenErrors++;
                    goto stream_error;
                }
                if (CdStream_PhaseTimeout++ < 0x258) {
                    goto start_read;
                }
                goto stream_error;
            }
            entry->phase = CD_STREAM_STEP_SECTORS;
        case CD_STREAM_STEP_SECTORS:
            if (CdStream_Runtime.state.readPhase == CD_STREAM_READ_AUDIO) {
                goto wait_for_progress;
            }
            if (CdStream_Runtime.state.readPhase == CD_STREAM_READ_IDLE) {
                goto wait_for_progress;
            }
            if (CdStream_Runtime.state.readPhase == CD_STREAM_READ_COMPLETE) {
                CdStream_Runtime.state.flags0 |= CD_STREAM_CHUNK_READY;
                CdReady_ClearCallback();
                if (CdStream_Runtime.state.chunkIndex & 1) {
                    irqOffset = ((CdStream_Runtime.state.ringHalf + 0x3F) & ~0x3F);
                    irqAddr1  = CdStream_Runtime.state.spuBase + irqOffset;
                    if (D_80068B5C != 0) {
                        SpuSetIRQ(0);
                        SpuSetIRQCallback(NULL);
                    }
                    D_80068B5C = 1;
                    SpuSetIRQCallback(CdStream_SpuIrqHandler);
                    SpuSetIRQAddr(irqAddr1);
                    SpuSetIRQ(1);
                } else if (!((CdStream_Runtime.state.flags0 >> CD_STREAM_VOICES_ON_BIT) & 1)) {
                    CdStream_Runtime.state.flags0 |= CD_STREAM_KEY_ON;
                } else if (CdStream_Runtime.state.chunkIndex != 0) {
                    irqAddr2 = CdStream_Runtime.state.spuBase + 0x40;
                    if (D_80068B5C != 0) {
                        SpuSetIRQ(0);
                        SpuSetIRQCallback(NULL);
                    }
                    D_80068B5C = 1;
                    SpuSetIRQCallback(CdStream_SpuIrqHandler);
                    SpuSetIRQAddr(irqAddr2);
                    SpuSetIRQ(1);
                }
            } else if (CdStream_Runtime.state.readPhase != CD_STREAM_READ_GAP) {
                if (CdStatus() & CdlStatShellOpen) {
                    if (CdStream_ErrorCode == 0) {
                        CdStream_ErrorCode = 5;
                    }
                    CdStream_LastErrorCode        = CdStream_ErrorCode;
                    CdStream_Runtime.state.flags |= CD_STREAM_DISC_FAULT;
                    CdStream_ShellOpenErrors++;
                    goto stream_error;
                }
                if (CdStream_ErrorCode == 0) {
                    CdStream_ErrorCode = 9;
                }
                CdStream_Runtime.state.flags2 |= CD_STREAM_FAULT;
                CdStream_Runtime.state.flags0 &= CD_STREAM_CLEAR_CHUNK_READY;
            } else {
                goto wait_for_progress;
            }
            CdStream_PhaseTimeout            = 0;
            CdStream_Runtime.state.readPhase = CD_STREAM_READ_IDLE;
            CdReady_ClearCallback();
            entry->phase = CD_STREAM_STEP_PAUSE;
        case CD_STREAM_STEP_PAUSE:
            CdStream_PhaseTimeout = 0;
        pause_read:
            CdControlF(CdlPause, NULL);
            entry->phase = CD_STREAM_STEP_PAUSE_SYNC;
            goto phase_advanced;
        case CD_STREAM_STEP_PAUSE_SYNC:
            D_80068B54 = 0;
            pauseSync  = CdSync(1, &sp.result[0]);
            if (pauseSync == CdlDiskError) {
                D_80068B64 += 1;
                if ((sp.result[0] & CdlStatShellOpen) || (CdStatus() & CdlStatShellOpen)) {
                    CdStream_Runtime.state.flags |= CD_STREAM_DISC_FAULT;
                    CdStream_ShellOpenErrors++;
                    if (CdStream_ErrorCode == 0) {
                        CdStream_ErrorCode = (u16)pauseSync;
                    }
                    CdStream_LastErrorCode = CdStream_ErrorCode;
                    goto stream_error;
                }
                entry->phase = CD_STREAM_STEP_PAUSE;
                if (CdStream_PhaseTimeout >= 0x258) {
                    goto stream_error;
                }
                CdStream_PhaseTimeout += 1;
                goto pause_read;
            }
            entry->phase = CD_STREAM_STEP_TRANSFER;
        case CD_STREAM_STEP_TRANSFER:
            if (SpuIsTransferCompleted(0) == 0) {
                goto wait_for_progress;
            }
            entry->phase = CD_STREAM_STEP_IDLE_SYNC;
        case CD_STREAM_STEP_IDLE_SYNC:
            if (CdSync(1, &sp.result[0]) == 0) {
                goto wait_for_progress;
            }
            CdStream_CurrentPhase = 0;
            return 1;
    }
phase_advanced:
    CdStream_LastPhase = entry->phase;
    return 0;
wait_for_progress:
    CdStream_LastPhase = entry->phase;
    if (++CdStream_PhaseTimeout < 0x259) {
        return 0;
    }
stream_error:
    CdStream_Runtime.state.flags2 |= CD_STREAM_FAULT;
    if (CdStream_ErrorCode == 0) {
        CdStream_ErrorCode = (u16)(entry->phase << 4) | 0xA;
    }
    CdStream_Runtime.state.readPhase = CD_STREAM_READ_IDLE;
    CdReady_ClearCallback();
    CdFlush();
    return 1;
}

static void CdStream_ReadyMts(u8 interrupt, u8* result)
{
    volatile CdStreamState* state;
    s16                     chunkSectors;
    s32                     intr;
    s32                     skipIndex;
    s32                     skipStamp;
    s32                     timer;
    s32                     sectorPos;
    s32                     writeSize;
    u32                     timerPhase;
    u32                     previousPhase;
    volatile CdStreamState* channelState;
    volatile CdStreamState* regionState;
    _CdStreamChannels*      channels;
    s32                     channelCount;

    if ((u16)CdStream_ReadyCallbackActive != 0) {
        CdStream_ErrorCode             = 0xC;
        CdStream_LastErrorCode         = CdStream_ErrorCode;
        CdStream_Runtime.state.flags2 |= CD_STREAM_FAULT;
        return;
    }
    CdStream_ReadyCallbackActive = 1;
    intr                         = interrupt & 0xFF;
    if (intr == 1) {
        if ((s16)(u16)CdStream_Runtime.state.readPhase != CD_STREAM_READ_GAP) {
            if ((s16)(u16)CdStream_Runtime.state.readPhase == intr) {
                if (SpuIsTransferCompleted(0) == 0) {
                    D_80068B62 += 1;
                    ResetRCnt(0xF2000002U);
                    previousPhase = 0;
                    while (1) {
                        timer = GetRCnt(0xF2000002U);
                        if ((u32)(timer & 0xFFFF) < 0x52B0U) {
                            timerPhase = timer & 0x7F;
                            if (timerPhase < previousPhase) {
                                previousPhase = timerPhase;
                                if (SpuIsTransferCompleted(0) != 0) {
                                    previousPhase = timerPhase;
                                    goto read_header;
                                }
                            }
                            previousPhase = timerPhase;
                        } else {
                            break;
                        }
                    }
                    CdStream_ErrorCode     = 4;
                    CdStream_LastErrorCode = CdStream_ErrorCode;
                    goto check_status;
                }
                goto read_header;
            }
        } else {
        read_header:
            if (CdGetSector(&D_800827F8, 3) == 0) {
                *(&D_80068B64 + 1) = (u8)(*(&D_80068B64 + 1) + 1);
                if (CdStream_ErrorCode == 0) {
                    CdStream_ErrorCode = 2;
                }
                CdStream_LastErrorCode = CdStream_ErrorCode;
                goto check_status;
            }
            sectorPos = CdPosToInt(&D_800827F8);
            if (sectorPos != CdStream_Runtime.state.expectedSector) {
                if (sectorPos < CdStream_Runtime.state.expectedSector) {
                    if (CdStream_Runtime.state.expectedSector >= (sectorPos + 4)) {
                        goto sector_mismatch;
                    }
                } else {
                sector_mismatch:
                    D_80068B5F += 1;
                    if (CdStream_ErrorCode == 0) {
                        CdStream_ErrorCode = 3;
                    }
                    CdStream_LastErrorCode = CdStream_ErrorCode;
                    goto check_status;
                }
            } else {
                CdStream_Runtime.state.expectedSector += 1;
                if ((s16)(u16)CdStream_Runtime.state.readPhase == CD_STREAM_READ_GAP) {
                    skipIndex = D_80068B78++ & 0xFF;
                    skipStamp = skipIndex | ((CdStream_Runtime.state.chunkIndex << 8) & 0xFFFF00);
                    if (D_80068B74 < skipStamp) {
                        if ((func_800AF590(0, 0) << 0x10) == 0) {
                            *(volatile s32*)&D_80068B74 = skipStamp;
                            goto count_gap;
                        }
                        goto check_status;
                    }
                    CdGetSector(CdStream_Runtime.state.sector, 0x200);
                count_gap:
                    state                               = &CdStream_Runtime.state;
                    CdStream_Runtime.state.gapRemaining = (u16)CdStream_Runtime.state.gapRemaining - 1;
                    if ((u16)CdStream_Runtime.state.gapRemaining == 0) {
                        CdStream_Runtime.state.readPhase = CD_STREAM_READ_COMPLETE;
                    }
                } else {
                    if (CdGetSector(CdStream_Runtime.state.sector, 0x200) == 0) {
                        *(&D_80068B64 + 1) = (u8)(*(&D_80068B64 + 1) + 1);
                        if (CdStream_ErrorCode == 0) {
                            CdStream_ErrorCode = 2;
                        }
                        CdStream_LastErrorCode = CdStream_ErrorCode;
                        goto check_status;
                    }
                    if ((u16)CdStream_Runtime.state.sectorsLeft == 0) {
                        if ((CdStream_Runtime.state.sector->magic & ~0xFF) != CD_STREAM_MTS_SIGNATURE) {
                            CdStream_ErrorCode     = 8;
                            CdStream_LastErrorCode = CdStream_ErrorCode;
                            goto check_status;
                        }
                        if (CdStream_Runtime.state.sector->chunkIndex == 0) {
                            CdStream_Runtime.state.mtsPeriod   = (s8)CdStream_Runtime.state.sector->period;
                            CdStream_Runtime.state.sectorsLeft = (s16)(s8)(u8)CdStream_Runtime.state.mtsPeriod;
                            CdStream_Runtime.state.gapSectors  = CdStream_Runtime.state.sector->gapSectors;
                            /* Header bit 7 records the disc gap. The count is sign-extended later, and a zero count still sets the apply-gap bit on this first header. */
                            if (CdStream_Runtime.state.sector->flags & CD_STREAM_MTS_FLAG_GAP) {
                                CdStream_Runtime.state.flags1 |= CD_STREAM_APPLY_GAP;
                            } else {
                                CdStream_Runtime.state.flags1 &= CD_STREAM_CLEAR_APPLY_GAP;
                            }
                            regionState  = &CdStream_Runtime.state;
                            chunkSectors = CD_STREAM_CHUNK_VSYNCS_NTSC;
                            if ((s8)(u8)regionState->mtsPeriod == CD_STREAM_SHORT_PERIOD) {
                                chunkSectors          = CD_STREAM_CHUNK_VSYNCS_SHORT_NTSC;
                                regionState->ringHalf = CD_STREAM_RING_HALF_SHORT;
                                if (gDisplayState.region == MODE_PAL) {
                                    chunkSectors = CD_STREAM_CHUNK_VSYNCS_SHORT_PAL;
                                }
                            } else {
                                regionState->ringHalf = CD_STREAM_RING_HALF;
                                if (gDisplayState.region == MODE_PAL) {
                                    chunkSectors = CD_STREAM_CHUNK_VSYNCS_PAL;
                                }
                            }
                            regionState->chunkVsyncs = chunkSectors;
                            channels                 = &CdStream_Runtime.channels;
                            /* Recover the common allocation from its channel member. */
                            channelState                = &PARENT_OF(channels, CdStreamRuntime, channels)->state;
                            channels->voiceAttr[0].addr = channelState->spuBase;
                            /* The channels' SPU buffers sit back to back, ringHalf * 2 + 0x40 bytes apart. */
                            channels->voiceAttr[1].addr  = channelState->spuBase + (channelState->ringHalf * 2 + 0x40);
                            channelState->flags1        |= CD_STREAM_VOICE_COPY;
                            channels->voiceAttr[0].mask |= 0x80;
                            channels->voiceAttr[1].mask  = channels->voiceAttr[0].mask;
                            channelState->chunkIndex     = channelState->sector->chunkIndex;
                            channelState->chunkCount     = channelState->sector->chunkCount;
                            /* Header bits 5 and 6 select how the final chunk ends and are copied into flags1. */
                            if (!((u8)channelState->sector->flags & CD_STREAM_MTS_FLAG_END_MASK)) {
                                channelState->flags1 = (u8)(channelState->flags1 | CD_STREAM_END_THROUGH);
                            } else if ((u8)channelState->sector->flags & CD_STREAM_MTS_FLAG_END_HALF) {
                                channelState->flags1 = (u8)(channelState->flags1 | CD_STREAM_END_HALFWAY);
                            } else {
                                channelState->flags1 = (u8)(channelState->flags1 | CD_STREAM_END_ON_ENTRY);
                            }
                            goto start_chunk;
                        }
                        if (CdStream_Runtime.state.sector->flags & CD_STREAM_MTS_FLAG_GAP) {
                            CdStream_Runtime.state.gapSectors = CdStream_Runtime.state.sector->gapSectors;
                            if (CdStream_Runtime.state.gapSectors != 0) {
                                CdStream_Runtime.state.flags1 |= CD_STREAM_APPLY_GAP;
                            } else {
                                CdStream_Runtime.state.flags1 &= CD_STREAM_CLEAR_APPLY_GAP;
                            }
                        } else {
                            CdStream_Runtime.state.flags1    &= CD_STREAM_CLEAR_APPLY_GAP;
                            CdStream_Runtime.state.gapSectors = 0;
                        }
                        CdStream_Runtime.state.sectorsLeft = (s16)(s8)(u8)CdStream_Runtime.state.mtsPeriod;
                    }
                    if (((s16)(u16)CdStream_Runtime.state.sectorsLeft % (s8)(u8)CdStream_Runtime.state.mtsPeriod) == 0) {
                        CdStream_Runtime.state.chunkIndex = CdStream_Runtime.state.sector->chunkIndex;
                        if (CdStream_Runtime.state.chunkIndex == 0) {
                            CdStream_Runtime.state.chunkCount = CdStream_Runtime.state.sector->chunkCount;
                        }
                    start_chunk:
                        channelCount = (u8)CdStream_Runtime.state.sector->magic;
                        /* Preserve the signed comparison of the channel count. */
                        if ((channelCount >= 2) && (CdStream_Runtime.state.sector->channelIndex == 0)) {
                            CdStream_Runtime.state.channelCount = (s8)(u8)CdStream_Runtime.state.sector->magic;
                            CdStream_Runtime.state.sectorsLeft  = (s8)(u8)CdStream_Runtime.state.mtsPeriod * (s8)(u8)CdStream_Runtime.state.channelCount;
                        }
                        if (CdStream_Runtime.state.chunkIndex & 1) {
                            CdStream_Runtime.state.spuAddr = CdStream_Runtime.state.spuBase + (s16)(u16)CdStream_Runtime.state.ringHalf;
                        } else {
                            CdStream_Runtime.state.spuAddr = CdStream_Runtime.state.spuBase;
                        }
                        CdStream_Runtime.state.spuAddr += CdStream_Runtime.state.sector->channelIndex * (CdStream_Runtime.state.ringHalf * 2 + 0x40);
                        SpuSetTransferStartAddr((u32)CdStream_Runtime.state.spuAddr);
                        *(volatile s32*)&CdStream_LastTransferSpuAddress = CdStream_Runtime.state.spuAddr;
                        /* ADPCM in a header sector starts immediately after this header. */
                        SpuWrite((u8*)(CdStream_Runtime.state.sector + 1), 0x800U);
                        *(void* volatile*)&CdStream_LastTransferBuffer = CdStream_Runtime.state.sector + 1;
                        CdStream_Runtime.state.spuAddr                += 0x7F0;
                    } else {
                        SpuSetTransferStartAddr((u32)CdStream_Runtime.state.spuAddr);
                        *(volatile s32*)&CdStream_LastTransferSpuAddress = CdStream_Runtime.state.spuAddr;
                        if (((s16)(u16)CdStream_Runtime.state.sectorsLeft % (s8)(u8)CdStream_Runtime.state.mtsPeriod) == 1) {
                            writeSize = ((s16)(u16)CdStream_Runtime.state.ringHalf - 0x7F0) % 0x800;
                            if (writeSize == 0) {
                                writeSize = 0x800;
                            }
                            if (!(CdStream_Runtime.state.chunkIndex & 1)) {
                                /* The two ring halves retain separate transfer paths. */
                                SpuWrite((u8*)CdStream_Runtime.state.sector, (writeSize + 0x3F) & ~0x3F);
                                *(void* volatile*)&CdStream_LastTransferBuffer = CdStream_Runtime.state.sector;
                                CdStream_Runtime.state.spuAddr                += writeSize;
                            } else {
                                SpuWrite((u8*)CdStream_Runtime.state.sector, (writeSize + 0x3F) & ~0x3F);
                                *(void* volatile*)&CdStream_LastTransferBuffer = CdStream_Runtime.state.sector;
                                CdStream_Runtime.state.spuAddr                += writeSize;
                            }
                        } else {
                            SpuWrite((u8*)CdStream_Runtime.state.sector, 0x800U);
                            *(void* volatile*)&CdStream_LastTransferBuffer = CdStream_Runtime.state.sector;
                            CdStream_Runtime.state.spuAddr                += 0x800;
                        }
                    }
                    state                              = &CdStream_Runtime.state;
                    CdStream_Runtime.state.sectorsLeft = (u16)CdStream_Runtime.state.sectorsLeft - 1;
                    if ((u16)CdStream_Runtime.state.sectorsLeft == 0) {
                        if (((u8)CdStream_Runtime.state.flags1 >> CD_STREAM_APPLY_GAP_BIT) & 1) {
                            CdStream_Runtime.state.gapRemaining = (s16)(s8)CdStream_Runtime.state.gapSectors;
                            if ((u16)CdStream_Runtime.state.gapRemaining != 0) {
                                CdStream_Runtime.state.readPhase = CD_STREAM_READ_GAP;
                                D_80068B78                       = 0;
                                goto unlock;
                            } else {
                                goto mark_complete;
                            }
                        } else {
                        mark_complete:
                            state->readPhase = CD_STREAM_READ_COMPLETE;
                        }
                    }
                }
            }
        }
    } else {
        CdStream_ErrorCode     = 7;
        CdStream_LastErrorCode = CdStream_ErrorCode;
    check_status:
        if (*result & 0x10) {
            if (CdStream_ErrorCode == 0) {
                CdStream_ErrorCode = 5;
            }
            CdStream_LastErrorCode           = CdStream_ErrorCode;
            CdStream_Runtime.state.flags    |= CD_STREAM_DISC_FAULT;
            CdStream_Runtime.state.readPhase = CD_STREAM_READ_FAULT;
            CdStream_Runtime.state.flags2   |= CD_STREAM_FAULT;
            *(&D_80068B5C + 1)               = (u8)(*(&D_80068B5C + 1) + 1);
        } else if ((u16)CdStream_Runtime.state.readPhase != CD_STREAM_READ_IDLE) {
            if ((s16)(u16)CdStream_Runtime.state.readPhase != CD_STREAM_READ_COMPLETE) {
                CdStream_Runtime.state.readPhase = CD_STREAM_READ_FAULT;
                if (CdStream_ErrorCode == 0) {
                    CdStream_ErrorCode = 0xB;
                }
                CdStream_LastErrorCode         = CdStream_ErrorCode;
                CdStream_Runtime.state.flags2 |= CD_STREAM_FAULT;
            }
        }
    }
unlock:
    CdStream_ReadyCallbackActive = 0;
}

static s32 CdStream_InitDisc(AsyncCbEntry* entry)
{
    struct {
        u8     result[8];
        s8     mode;
        u8     pad[7];
        CdlLOC loc;
    } sp;
    s32 sync;

    if (entry->status.firstPoll) {
        entry->status.firstPoll = 0;
        entry->status.pollState = 1;
    }

    D_80068B66 = entry->status.pollState;
    switch (entry->status.pollState) {
        case 1:
            if (CdControlB(CdlNop, NULL, sp.result) == 0) {
                return 0;
            }
            if (sp.result[0] & CdlStatShellOpen) {
                return 0;
            }
            if (sp.result[0] & CdlStatStandby) {
                entry->status.pollState = 2;
                case 2:
                    if (CdControl(CdlGetTN, NULL, sp.result) != 0) {
                        entry->status.pollState = 4;
                        case 3:
                            sync = CdSync(1, sp.result);
                            if (sync == CdlDiskError) {
                                entry->status.pollState = 2;
                            } else if (sync == CdlComplete) {
                                entry->status.pollState = 4;
                                case 4:
                                    CdIntToPos(0, &sp.loc);
                                    if (CdControl(CdlSeekL, (u8*)&sp.loc, sp.result) != 0) {
                                        entry->status.pollState = 5;
                                        case 5:
                                            sync = CdSync(1, sp.result);
                                            if ((sync == CdlDiskError) && (sp.result[0] & CdlStatError) &&
                                                (sp.result[1] & 0x40)) {
                                                entry->status.pollState = 1;
                                            } else if (sync == CdlComplete) {
                                                entry->status.pollState = 6;
                                                case 6:
                                                    sp.mode = -0x60;
                                                    if (CdControl(CdlSetmode, (u8*)&sp.mode, NULL) != 0) {
                                                        entry->status.pollState              = 7;
                                                        CdStream_Runtime.state.settleCounter = 0;
                                                    }
                                            }
                                    }
                            }
                    }
            }
            break;
        case 7:
            CdStream_Runtime.state.settleCounter = CdStream_Runtime.state.settleCounter + 1;
            if (CdStream_Runtime.state.settleCounter >= CD_STREAM_SETTLE_POLLS) {
                return 1;
            }
            break;
    }
    return 0;
}

static void CdReady_InstallCallback(CdlCB arg0)
{
    _CdReadyQueue* queue;

    queue = &CdReady_Queue;
    if (queue->callbackInstalled == 0) {
        queue->prevCallback = CdReadyCallback(arg0);
    } else {
        CdReadyCallback(arg0);
    }
    CdReady_Queue.callbackInstalled = 1;
}

static void CdReady_ClearCallback(void)
{
    _CdReadyQueue* queue;

    queue = &CdReady_Queue;
    if (queue->callbackInstalled != 0) {
        CdReadyCallback(0);
        queue->prevCallback      = 0;
        queue->callbackInstalled = 0;
    }
}

void CdStream_Reset(void)
{
    MEM_CLEAR_WORDS(&CdReady_Queue, sizeof(CdReady_Queue) / 4);
    MEM_CLEAR_WORDS(&CdStream_Runtime, sizeof(CdStreamRuntime) / sizeof(u32));

    SetRCnt(RCntCNT2, 0xFFFF, RCntMdNOINTR);
    StartRCnt(RCntCNT2);
    D_80068B58                   = 0;
    CdStream_ErrorCode           = 0;
    CdStream_LastErrorCode       = CdStream_ErrorCode;
    CdStream_ReadyCallbackActive = 0;
    D_80068B5C                   = 0;
}

void CdStream_ArmSpuIrq(void)
{
    volatile CdStreamState* p;

    CdStream_Runtime.state.flags0 = CdStream_Runtime.state.flags0 & CD_STREAM_CLEAR_ENGAGED;
    CdStream_Runtime.state.flags0 = CdStream_Runtime.state.flags0 & CD_STREAM_CLEAR_SPU_IRQ;
    p                             = &CdStream_Runtime.state;
    p->advanceDelay               = 1;
    p->playhead                   = 0;
    SpuSetIRQ(0);
    SpuSetIRQCallback(CdStream_SpuIrqHandler);
    SpuSetIRQAddr((p->spuBase + p->ringHalf + 0x4F) & ~0x3F);
    CdStream_Runtime.state.flags0 = CdStream_Runtime.state.flags0 & CD_STREAM_CLEAR_STOP;
    CdStream_Runtime.state.flags0 = CdStream_Runtime.state.flags0 | CD_STREAM_ACTIVE;
}

static void CdStream_SpuIrqHandler(void)
{
    CdStream_Runtime.state.flags0 = CdStream_Runtime.state.flags0 | CD_STREAM_SPU_IRQ;
}

void CdStream_SetVolume(s16 volume)
{
    _CdStreamChannels*      channels;
    volatile CdStreamState* q;
    SpuVoiceAttr*           ch1b;
    SpuVoiceAttr*           ch1;
    s16                     val;
    s32                     t0;
    s32                     t1;

    channels = &CdStream_Runtime.channels;
    /* CdStream_Runtime.state sits directly before the channels; reaching it back from
     * `channels` keeps one base register for both objects. */
    q = &PARENT_OF(channels, CdStreamRuntime, channels)->state;

    if ((q->flags0 >> CD_STREAM_ENGAGED_BIT) & 1) {
        if (q->flags1 & CD_STREAM_VOICE_COPY) {
            t0                          = channels->voiceAttr[0].mask;
            t1                          = channels->voiceAttr[1].mask;
            channels->voiceAttr[0].mask = t0 | 3;
            channels->voiceAttr[1].mask = t1 | 3;
        } else {
            channels->voiceAttr[1].mask = 3;
            channels->voiceAttr[0].mask = 3;
            q->flags1                   = q->flags1 | CD_STREAM_VOICE_COPY;
        }
    }

    if (CdStream_Runtime.state.flags & CD_STREAM_MONO) {
        ch1b                                = &channels->voiceAttr[1];
        val                                 = (s16)((volume * 0xB5) >> 8);
        ch1b->volume.left                   = val;
        channels->voiceAttr[0].volume.right = val;
        ch1b->volume.right                  = val;
        channels->voiceAttr[0].volume.left  = val;
        return;
    }
    ch1                                 = &channels->voiceAttr[1];
    ch1->volume.right                   = volume;
    channels->voiceAttr[0].volume.left  = volume;
    ch1->volume.left                    = 0;
    channels->voiceAttr[0].volume.right = 0;
}

static void CdStream_SetFlag14(s32 playhead)
{
    volatile CdStreamState* p;
    u8                      temp;

    p    = &CdStream_Runtime.state;
    temp = p->flags1;
    // Accept a new playhead only after playback has enabled seeks.
    if (temp >> CD_STREAM_SEEK_ENABLED_BIT) {
        p->requestedPlayhead = playhead;
        p->flags1            = p->flags1 | CD_STREAM_SEEK;
        p->flags0            = p->flags0 | CD_STREAM_ACTIVE;
    }
}

static void CdStream_AbortPhase(_CdReadyEntry* entry)
{
    if (entry->firstPoll) {
        entry->cancelPending = 0;
        return;
    }
    entry->cancelPending             = 0;
    CdStream_Runtime.state.readPhase = CD_STREAM_READ_IDLE;
    switch (entry->phase) {
        case CD_STREAM_STEP_READ:
        case CD_STREAM_STEP_READ_SYNC:
            CdReady_ClearCallback();
            goto shared_flush;
        case CD_STREAM_STEP_SECTORS:
            CdReady_ClearCallback();
            CdFlush();
            CdControlF(CdlPause, NULL);
            entry->cancelPending = 1;
            entry->phase         = CD_STREAM_STEP_CANCEL_PAUSE;
            break;
        case CD_STREAM_STEP_PAUSE:
        case CD_STREAM_STEP_PAUSE_SYNC:
        case CD_STREAM_STEP_TRANSFER:
        case CD_STREAM_STEP_CANCEL_PAUSE:
            CdFlush();
            entry->phase = CD_STREAM_STEP_CANCEL_TRANSFER;
            /* fallthrough */
        case CD_STREAM_STEP_CANCEL_TRANSFER:
            if (SpuIsTransferCompleted(0) == 0) {
                entry->cancelPending = 1;
            }
            break;
        case CD_STREAM_STEP_SETMODE:
        case CD_STREAM_STEP_SETMODE_SYNC:
        case CD_STREAM_STEP_SPEED_SETTLE:
        case CD_STREAM_STEP_SETLOC:
        case CD_STREAM_STEP_SETLOC_SYNC:
        case CD_STREAM_STEP_IDLE_SYNC:
        shared_flush:
            CdFlush();
            break;
    }
}

static void CdStream_FinishQueueEntry(_CdReadyEntry* entry)
{
    CdStream_AbortPhase(entry);
    if (!entry->cancelPending && (CdStream_Runtime.state.doneCb != NULL)) {
        CdStream_Runtime.state.doneCb(0);
    }
}

static void CdReady_Cancel(s16 arg0)
{
    u8             temp;
    s16            idx;
    _CdReadyEntry* entry;

    temp = CdReady_Queue.locked;
    if (arg0 != 0) {
        idx   = arg0 - 1;
        entry = &CdReady_Queue.entries[idx];
        if (entry->active) {
            entry->active    = 0;
            entry->cancelled = 1;
        }
        CdReady_Queue.locked = temp;
    }
}

s32 CdStream_IsBusy(void)
{
    if (CdStream_Runtime.state.flags0 & CD_STREAM_ACTIVE) {
        return 1;
    }
    return (CdReady_Queue.writeIdx != CdReady_Queue.readIdx) ? 1 : (CdStream_Runtime.state.reinitSlot != 0);
}

/// Clears the live stream's queued-read handle when a chunk-read job finishes.
///
/// The handle is a 1-based CD-ready queue slot; zero means no outstanding read.
/// The queue retires the completed entry after invoking this callback.
static void _cdStreamClearReadySlot(void)
{
    enum { CD_STREAM_READY_SLOT_NONE = 0 };

    CdStream_Runtime.state.readySlot = CD_STREAM_READY_SLOT_NONE;
}

void cdStreamSetMono(s32 enabled)
{
    if ((s8)enabled) {
        CdStream_Runtime.state.flags |= CD_STREAM_MONO;
    } else {
        CdStream_Runtime.state.flags &= CD_STREAM_CLEAR_MONO;
    }
}

static void CdStream_MarkEnding(AsyncCbEntry* unused)
{
    CdStream_Runtime.state.flags      = CdStream_Runtime.state.flags & CD_STREAM_CLEAR_DISC_FAULT;
    CdStream_Runtime.state.flags1     = CdStream_Runtime.state.flags1 | CD_STREAM_MODE_SET;
    CdStream_Runtime.state.reinitSlot = 0;
}

/// Flushes CD library state when a disc-reinitialization job is cancelled.
///
/// `entry` is the queue-owned job and is unused. Returning zero finishes
/// cancellation immediately, allowing the asynchronous queue to retire the job.
static s32 _cdStreamCancelDiscInit(AsyncCbEntry* entry)
{
    enum { CD_STREAM_CANCEL_COMPLETE = 0 };

    CdFlush();
    return CD_STREAM_CANCEL_COMPLETE;
}

static void CdStream_ConfigureSpuIrq(s32 arg0, u32 arg1)
{
    if (arg0 == 1) {
        if (D_80068B5C != 0) {
            SpuSetIRQ(0);
            SpuSetIRQCallback(0);
        }
        D_80068B5C = arg0;
        SpuSetIRQCallback(CdStream_SpuIrqHandler);
        SpuSetIRQAddr(arg1);
        SpuSetIRQ(1);
    } else if (D_80068B5C != 0) {
        SpuSetIRQ(0);
        SpuSetIRQCallback(0);
        D_80068B5C = 0;
    }
}

/// Unreferenced no-op retained for the original image layout.
static void _cdStreamUnusedStub(void)
{
}
