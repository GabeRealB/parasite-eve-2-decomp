#include "cdstream.h"

#include <psyq/sys/types.h>
#include <psyq/kernel.h>
#include <psyq/libapi.h>
#include <psyq/libcd.h>
#include <psyq/libspu.h>

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

/// Sector payload pointed to by CdStreamState::sector (MTS audio stream sector).
typedef struct _MtsSector {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ u32 magic; // high 3 bytes = "MTS", low byte = channel count
    /* 0x0C */ s8  field_C;
    /* 0x0D */ u8  field_D;
    /* 0x0E */ u8  field_E;
    /* 0x0F */ s8  field_F;
} MtsSector;

/// CD/SPU streaming control block, followed by its two voice channels.
typedef struct _CdStreamState {
    /* 0x00 */ u8         flags0; // bit0 busy, bit1 voice-on, bit3 IRQ, bit4 voices alloc, bit6 stop
    /* 0x01 */ u8         flags1; // bit0 voices started, bit1 ending, bit3 param pending, bit7 enable param
    /* 0x02 */ u8         flags2; // bit0 continue-arm, bit1/2 stream phase, bit3 XA pause
    /* 0x03 */ u8         phase;  // 1 = completing, 2 = streaming
    /* 0x04 */ u8         field_4;
    /* 0x05 */ u8         pad_5;
    /* 0x06 */ s16        readySlot; // 1-based CdReady_Queue index, 0 = none
    /* 0x08 */ void       (*doneCb)(s32);
    /* 0x0C */ void       (*startCb)(s32);
    /* 0x10 */ void       (*voiceFreeCb)(s32);
    /* 0x14 */ s32        field_14;
    /* 0x18 */ s32        field_18;
    /* 0x1C */ s32        field_1C;
    /* 0x20 */ s32        field_20;
    /* 0x24 */ s32        spuAddr;
    /* 0x28 */ s32        startSector;
    /* 0x2C */ s32        field_2C;
    /* 0x30 */ s32        field_30;
    /* 0x34 */ s32        field_34;
    /* 0x38 */ s32        field_38;
    /* 0x3C */ s32        spuBase;
    /* 0x40 */ s16        sectorsPerChunk; // 0x18 (NTSC) or 0x14 (PAL)
    /* 0x42 */ s16        ringHalf;        // 0x2770; half ring used in SPU addr math
    /* 0x44 */ s16        countdown;
    /* 0x46 */ s8         mtsPeriod;       // divisor for remaining % period SpuWrite cadence
    /* 0x47 */ u8         mtsParam;
    /* 0x48 */ MtsSector* sector;
    /* 0x4C */ s16        field_4C;
    /* 0x4E */ s16        remaining;
    /* 0x50 */ u8         voiceL;
    /* 0x51 */ u8         voiceR;
    /* 0x52 */ s8         mode;
    /* 0x53 */ u8         flags;         // bit1 = mono mix
    /* 0x54 */ u16        pending;
    /* 0x56 */ u16        settleCounter; // disc init settle ticks
} CdStreamState;
STATIC_ASSERT_SIZEOF(CdStreamState, 0x58);

// Channel records are copied whole into the PsyQ voice-update queue.
STATIC_ASSERT_SIZEOF(SpuVoiceAttr, 0x40);

/// The streaming left/right voice channels.
typedef struct _CdStreamChannels {
    /* 0x00 */ SpuVoiceAttr ch[2];
} CdStreamChannels;
STATIC_ASSERT_SIZEOF(CdStreamChannels, 0x80);

/// One allocation: control state and both channels. Reset clears all 0xD8 bytes;
/// the IRQ and read callbacks recover the control state from the channel member.
typedef struct {
    volatile CdStreamState state;
    CdStreamChannels       channels;
} CdStreamRuntime;
STATIC_ASSERT_SIZEOF(CdStreamRuntime, 0xD8);
STATIC_ASSERT(OFFSET_OF(CdStreamRuntime, channels) == 0x58, cd_stream_channels_offset);

/// One slot in CdReady_Queue.entries (stride 0x14).
/// flags: bit0 active, bit1 armed, bit2 cancel/pending, bit3 result.
/// Callback words are copied alongside the queue flags; dispatch uses their typed view.
typedef struct _CdReadyEntry {
    /* 0x00 */ u32 flags;
    /* 0x04 */ s32 sectorPos;
    /* 0x08 */ s32 (*pollFn)(struct _CdReadyEntry*);
    /* 0x0C */ union {
        void (*callback)(void);
        u32  word;
    } doneFn;
    /* 0x10 */ union {
        void (*callback)(struct _CdReadyEntry*);
        u32  word;
    } errorFn;
} CdReadyEntry;
STATIC_ASSERT_SIZEOF(CdReadyEntry, 0x14);

/// BSS object CdReady_Queue (size 0x58). Ring of CD ready work items + callback state.
typedef struct _CdReadyQueue {
    /* 0x00 */ u8           locked;            // re-entrancy guard
    /* 0x01 */ u8           callbackInstalled; // CdReadyCallback currently ours
    /* 0x02 */ u8           readIdx;
    /* 0x03 */ u8           writeIdx;
    /* 0x04 */ CdlCB        prevCallback; // previous CdReadyCallback
    /* 0x08 */ CdReadyEntry entries[4];
} CdReadyQueue;
STATIC_ASSERT_SIZEOF(CdReadyQueue, 0x58);

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

static volatile CdReadyQueue CdReady_Queue;

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

static s32 CdReady_Enqueue(CdReadyEntry* arg0);

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
static s32 CdStream_PollMtsRead(CdReadyEntry* entry);

static void CdStream_ReadyMts(u8 interrupt, u8* result);

static s32 CdStream_InitDisc(AsyncCbEntry* entry);

static void CdReady_InstallCallback(CdlCB arg0);

static void CdReady_ClearCallback(void);

static void CdStream_SpuIrqHandler(void);

static void CdStream_SetFlag14(s32 arg0);

static void CdStream_AbortPhase(CdReadyEntry* entry);

static void CdStream_FinishQueueEntry(CdReadyEntry* entry);

static void CdReady_Cancel(s16 arg0);

static void CdStream_ClearReadySlot(void);

static void CdStream_MarkEnding(AsyncCbEntry* unused);

static s32 CdStream_Flush(AsyncCbEntry* unused);

static void CdStream_ConfigureSpuIrq(s32 arg0, u32 arg1);

/// Unused stream-module entry point; retained for the original image layout.
static void CdStream_UnusedStub(void);

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
    { 0, 0xC0, taskKill },
    { 0, 0xC0, taskKill },
    { 0, 0xC0, taskKill },
    { 0, 0xC0, taskKill },
    { 0, 0xC0, func_80725BB8 },
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

static s32 CdReady_Enqueue(CdReadyEntry* arg0)
{
    u8                     saved;
    s32                    field2;
    s32                    next;
    u32                    flags;
    s32                    temp;
    volatile CdReadyEntry* entry;
    volatile CdReadyQueue* p;

    p                    = &CdReady_Queue;
    saved                = CdReady_Queue.locked;
    CdReady_Queue.locked = 1;

    field2 = (s8)p->readIdx;
    next   = (s8)p->writeIdx;
    next   = next + 1;
    if (next >= 4) {
        next = 0;
    }

    if (next == field2) {
        CdReady_Queue.locked = saved;
        return 0;
    }

    entry                = &CdReady_Queue.entries[(s8)p->writeIdx];
    entry->pollFn        = arg0->pollFn;
    entry->sectorPos     = arg0->sectorPos;
    temp                 = arg0->doneFn.word;
    flags                = entry->flags;
    flags                = flags | 1;
    entry->doneFn.word   = temp;
    temp                 = ~4;
    flags                = flags & temp;
    flags                = flags & ~8;
    flags                = flags & ~0x1FE0;
    temp                 = arg0->errorFn.word;
    flags                = flags | 2;
    entry->flags         = flags;
    entry->errorFn.word  = temp;
    field2               = (s8)p->writeIdx;
    p->writeIdx          = next;
    CdReady_Queue.locked = saved;
    return field2 + 1;
}

static void CdReady_Poll(void)
{
    volatile CdReadyQueue* p;
    CdReadyEntry*          entry;
    u32                    flags;

    p = &CdReady_Queue;
    if (p->locked == 0 && p->writeIdx != p->readIdx) {
        entry = (CdReadyEntry*)&CdReady_Queue.entries[(s8)p->readIdx];
        flags = entry->flags;
        if (flags & 1) {
            if (entry->pollFn(entry) != 0) {
                if (entry->doneFn.callback != 0) {
                    entry->doneFn.callback();
                }
                entry->flags &= ~1;
                entry->flags &= ~4;
                p->readIdx    = p->readIdx + 1;
                if ((s8)p->readIdx >= 4) {
                    p->readIdx = 0;
                }
            }
        } else if (((flags >> 2) & 1) && !((flags >> 1) & 1)) {
            if (entry->errorFn.callback != 0) {
                entry->errorFn.callback(entry);
            }
            if (!((entry->flags >> 3) & 1)) {
                goto advance;
            }
        } else {
        advance:
            entry->flags         &= ~4;
            CdReady_Queue.readIdx = CdReady_Queue.readIdx + 1;
            if ((s8)CdReady_Queue.readIdx >= 4) {
                CdReady_Queue.readIdx = 0;
            }
        }
    }
}

void CdStream_Start(CdStreamParams* arg0)
{
    CdReadyEntry            entry;
    volatile CdStreamState* p;
    s32                     flag;
    volatile CdStreamState* ap;
    union {
        volatile CdStreamState* state;
        volatile CdReadyQueue*  queue;
    } a3;
    SpuVoiceAttr* t0;
    SpuVoiceAttr* ch1;
    s32           sectors;
    s16           volume;
    u8            saved;
    s16           f6;
    s16           idx;
    u32           flags;
    CdReadyEntry* e;
    s32           one;
    s32           cflags;
    s16           vff;
    s16           v1fc3;
    s16           v1000;
    s32           temp;
    u8            mode;
    s32           base;
    CdReadyEntry* rem_tmp;
    s32           temp_v1;

    p         = &CdStream_Runtime.state;
    p->voiceL = arg0->voiceL;
    p->voiceR = arg0->voiceR;
    flag      = ((u8)CdStream_Runtime.state.flags0 >> 4) & 1;
    if (flag == 1) {
        Spu_KeyOff((s8)p->voiceL);
        Spu_KeyOff((s8)p->voiceR);
        if (arg0->voiceFreeCb != 0) {
            arg0->voiceFreeCb(
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
            idx   = f6 - 1;
            e     = (CdReadyEntry*)&CdReady_Queue.entries[idx];
            flags = e->flags;
            if (flags & 1) {
                e->flags = (flags & ~1) | 4;
            }
            CdReady_Queue.locked = saved;
        }
        CdStream_Runtime.state.readySlot = 0;
    }

    a3.state              = &CdStream_Runtime.state;
    a3.state->startCb     = arg0->startCb;
    a3.state->voiceFreeCb = arg0->voiceFreeCb;
    a3.state->field_4     = 0;
    a3.state->doneCb      = arg0->doneCb;
    a3.state->field_18    = 0;
    a3.state->startSector = arg0->startSector;
    a3.state->field_2C    = arg0->startSector;
    one                   = 1;
    a3.state->field_30    = arg0->startSector;
    a3.state->field_34    = 0;
    a3.state->field_38    = one;
    base                  = arg0->spuBase;
    sectors               = 0x18;
    {
        s32 ds            = gDisplayState.region;
        a3.state->spuBase = base;
        if (ds == one) {
            sectors = 0x14;
        }
    }
    a3.state->sectorsPerChunk = sectors;
    a3.state->ringHalf        = 0x2770;
    t0                        = PARENT_OF(a3.state, CdStreamRuntime, state)->channels.ch;
    a3.state->sector          = (MtsSector*)arg0->sectorBuf;
    a3.state->voiceL          = arg0->voiceL;
    vff                       = 0xFF;
    a3.state->voiceR          = arg0->voiceR;
    mode                      = arg0->mode;
    v1fc3                     = 0x1FC3;
    v1000                     = 0x1000;
    cflags                    = 0x6009F;
    t0->mask                  = cflags;
    t0[1].mask                = cflags;
    t0->volmode.left          = 0;
    t0->volmode.right         = 0;
    t0->pitch                 = v1000;
    t0->adsr1                 = vff;
    t0->adsr2                 = v1fc3;
    t0[1].volmode.left        = 0;
    t0[1].volmode.right       = 0;
    t0[1].pitch               = v1000;
    a3.state->mode            = mode;
    a3.state->field_1C        = 0;
    a3.state->field_20        = 0;
    a3.state->pending         = 0;
    t0->voice                 = one << a3.state->voiceL;
    t0->addr                  = a3.state->spuBase;
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
        if (f53 & 2) {
            ch1               = &t0[1];
            volume            = (arg0->volume * 0xB5) >> 8;
            ch1->volume.right = volume;
            ch1->volume.left  = volume;
            t0->volume.right  = volume;
            t0->volume.left   = volume;
        } else {
            u16 volume_u;
            volume_u           = (u16)arg0->volume;
            t0->volume.right   = 0;
            t0[1].volume.left  = 0;
            t0->volume.left    = volume_u;
            t0[1].volume.right = (u16)arg0->volume;
        }
    }

    rem_tmp                          = &entry;
    entry.pollFn                     = CdStream_PollMtsRead;
    temp_v1                          = arg0->startSector;
    entry.doneFn.callback            = CdStream_Continue;
    entry.errorFn.callback           = CdStream_FinishQueueEntry;
    entry.sectorPos                  = temp_v1;
    CdStream_Runtime.state.readySlot = CdReady_Enqueue(rem_tmp);
    CdStream_Runtime.state.phase     = 2;
    D_80068B74                       = -1;
}

static void CdStream_Continue(void)
{
    CdReadyEntry            entry;
    volatile CdStreamState* p;

    CdStream_Runtime.state.readySlot = 0;

    if ((CdStream_Runtime.state.flags0 >> 2) & 1) {
        if (CdStream_Runtime.state.field_1C == 0) {
            CdStream_Runtime.state.phase  = 1;
            CdStream_Runtime.state.flags1 = CdStream_Runtime.state.flags1 | 1;
            CdStream_Runtime.state.flags2 = CdStream_Runtime.state.flags2 & 0xFD;
            CdStream_Runtime.state.flags2 = CdStream_Runtime.state.flags2 & 0xFB;
            if (CdStream_Runtime.state.doneCb != NULL) {
                CdStream_Runtime.state.doneCb(1);
            }
            return;
        }
    }

    p                      = &CdStream_Runtime.state;
    entry.pollFn           = CdStream_PollMtsRead;
    entry.doneFn.callback  = CdStream_Continue;
    p->flags2              = p->flags2 & 0xFD;
    entry.errorFn.callback = CdStream_FinishQueueEntry;
    entry.sectorPos        = p->startSector;
    D_80068B63             = D_80068B63 + 1;
    p->readySlot           = CdReady_Enqueue(&entry);
}

void CdStream_Stop(void)
{
    u8                      saved;
    u8                      temp;
    s16                     arg0;
    s16                     idx;
    u32                     flags;
    CdReadyEntry*           entry;
    volatile CdStreamState* p;

    saved                = CdReady_Queue.locked;
    CdReady_Queue.locked = 1;

    if (CdStream_Runtime.state.flags0 & 1) {
        CdStream_Runtime.state.flags0 = CdStream_Runtime.state.flags0 | 0x40;
        SpuSetIRQ(0);
        SpuSetIRQCallback(0);
    } else {
        if (CdStream_Runtime.state.readySlot != 0) {
            arg0 = CdStream_Runtime.state.readySlot;
            temp = CdReady_Queue.locked;
            if (arg0 != 0) {
                idx   = arg0 - 1;
                entry = (CdReadyEntry*)&CdReady_Queue.entries[idx];
                flags = entry->flags;
                if (flags & 1) {
                    entry->flags = (flags & ~1) | 4;
                }
                CdReady_Queue.locked = temp;
            }
            CdStream_Runtime.state.readySlot = 0;
        }

        p = &CdStream_Runtime.state;
        if ((p->flags2 >> 3) & 1) {
            func_800B0118(0, 0);
            p->flags2 = p->flags2 & 0xF7;
        }
    }

    CdReady_Queue.locked = saved;
}

static void CdStream_TeardownVoices(void)
{
    CdReadyEntry  entry;
    s32           flag;
    s16           slot;
    u32           flags;
    CdReadyEntry* e;
    u8            saved;

    if (CdStream_Runtime.state.flags2 & 1) {
        CdStream_Runtime.state.field_20;
        CdStream_Runtime.state.flags2 &= 0xFE;
        CdStream_Runtime.state.flags0 &= 0xFE;
        CdStream_Runtime.state.flags2 &= 0xFD;
        CdStream_Runtime.state.flags0 &= 0xDF;
        flag                           = (CdStream_Runtime.state.flags0 >> 4) & 1;
        if (flag == 1) {
            Spu_KeyOff((s8)CdStream_Runtime.state.voiceL);
            Spu_KeyOff((s8)CdStream_Runtime.state.voiceR);
            CdStream_Runtime.state.flags0 &= 0xEF;
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
                e     = &CdReady_Queue.entries[(s16)(slot - 1)];
                flags = e->flags;
                if (flags & 1) {
                    e->flags = (flags & ~1) | 4;
                }
                CdReady_Queue.locked = saved;
            }
            CdStream_Runtime.state.readySlot = 0;
        }
        entry.pollFn                     = CdStream_PollMtsRead;
        entry.doneFn.callback            = CdStream_CompleteInitialRead;
        entry.sectorPos                  = CdStream_Runtime.state.field_30;
        CdStream_Runtime.state.flags0   &= 0xFB;
        entry.errorFn.callback           = CdStream_FinishQueueEntry;
        CdStream_Runtime.state.readySlot = CdReady_Enqueue(&entry);
        CdStream_Runtime.state.phase     = 2;
    }
}

static void CdStream_CompleteInitialRead(void)
{
    CdReadyEntry            entry;
    volatile CdStreamState* p;
    s32                     pos;
    s32                     chunk;

    p            = &CdStream_Runtime.state;
    pos          = p->field_18;
    chunk        = pos / p->sectorsPerChunk + 1;
    p->readySlot = 0;

    if ((CdStream_Runtime.state.flags0 >> 2) & 1) {
        p->phase  = 1;
        p->flags1 = p->flags1 | 1;
        p->flags2 = p->flags2 & 0xFD;
        if (p->field_1C & 1) {
            p->field_4 = 0;
        } else {
            p->field_4                    = p->sectorsPerChunk - pos % p->sectorsPerChunk + 1;
            CdStream_Runtime.state.flags0 = CdStream_Runtime.state.flags0 | 0x20;
        }
        CdStream_CleanupIrq();
    } else {
        p->flags2              = p->flags2 & 0xFD;
        D_80068B63             = D_80068B63 + 1;
        entry.pollFn           = CdStream_PollMtsRead;
        entry.doneFn.callback  = CdStream_CompleteInitialRead;
        entry.errorFn.callback = CdStream_FinishQueueEntry;
        entry.sectorPos        = p->field_30;
        p->field_20            = chunk;
        p->readySlot           = CdReady_Enqueue(&entry);
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
    CdStream_Runtime.state.flags0 = CdStream_Runtime.state.flags0 & 0xF7;
    CdStream_Runtime.state.flags0 = CdStream_Runtime.state.flags0 & 0xBF;
    func_800B0118(0, 0);
    p                             = &CdStream_Runtime.state;
    temp                          = p->flags2;
    p->flags2                     = temp & 0xF7;
    CdStream_ErrorCode            = 0;
    CdStream_Runtime.state.flags0 = CdStream_Runtime.state.flags0 | 1;
}

static void CdStream_TickPlayback(void)
{
    CdReadyEntry entry;
    union {
        volatile CdStreamState* state;
        s32                     position;
    } stateOrPosition;
    s16           slot;
    u8            saved;
    CdReadyEntry* queued;
    SpuVoiceAttr* channels;
    s32           nextChunk;
    u32           flags;

    stateOrPosition.position = CdStream_Runtime.state.field_18;
    if (!(((u8)CdStream_Runtime.state.flags0 >> 4) & 1) && (((u8)CdStream_Runtime.state.flags0 >> 5) & 1)) {
        CdAudio_AllocVoices((s8*)&CdStream_Runtime.state.voiceL, (s8*)&CdStream_Runtime.state.voiceR);
        channels          = PARENT_OF(&CdStream_Runtime.state, CdStreamRuntime, state)->channels.ch;
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
        CdAudio_CopyVoiceData((s8)CdStream_Runtime.state.voiceR, PARENT_OF(&CdStream_Runtime.state, CdStreamRuntime, state)->channels.ch + 1);
        CdStream_Runtime.state.flags1 &= 0xFE;
        if (CdStream_Runtime.state.startCb != NULL) {
            CdStream_Runtime.state.startCb((1 << CdStream_Runtime.state.voiceL) | (1 << CdStream_Runtime.state.voiceR));
        }
        CdStream_Runtime.state.flags0 |= 0x10;
        CdStream_Runtime.state.flags0 &= 0xDF;
    }
    stateOrPosition.state = &CdStream_Runtime.state;
    if ((stateOrPosition.state->field_20 + 1) < stateOrPosition.state->field_38) {
        if (((u8)stateOrPosition.state->flags0 >> 3) & 1) {
            stateOrPosition.state->field_18 = stateOrPosition.state->field_1C * (s16)(u16)stateOrPosition.state->sectorsPerChunk;
            stateOrPosition.position        = stateOrPosition.state->field_18;
        } else {
            stateOrPosition.position = stateOrPosition.state->field_18;
        }
        if (!(((u8)CdStream_Runtime.state.flags0 >> 2) & 1)) {
            D_80068B5E = (u8)(D_80068B5E + 1);
        stopVoices:
            if (((u8)CdStream_Runtime.state.flags0 >> 4) & 1) {
                Spu_KeyOff((u32)(s8)CdStream_Runtime.state.voiceL);
                Spu_KeyOff((u32)(s8)CdStream_Runtime.state.voiceR);
                if (CdStream_Runtime.state.voiceFreeCb != NULL) {
                    CdStream_Runtime.state.voiceFreeCb((1 << (s8)CdStream_Runtime.state.voiceL) | (1 << CdStream_Runtime.state.voiceR));
                }
                CdStream_Runtime.state.flags0 &= 0xEF;
            }
            if (CdStream_Runtime.state.flags0 & 1) {
                if (CdStream_ErrorCode == 0) {
                    CdStream_ErrorCode = 6;
                }
                CdStream_LastErrorCode         = CdStream_ErrorCode;
                CdStream_Runtime.state.flags0 &= 0xFE;
                func_800B0118((s32)(s16)CdStream_ErrorCode, 0);
                CdStream_ErrorCode             = 0;
                CdStream_Runtime.state.flags2 |= 8;
                CdStream_Runtime.state.flags2 |= 1;
                CdStream_Runtime.state.flags2 |= 4;
                CdStream_TeardownVoices();
            }
            CdStream_Runtime.state.flags0 &= 0xDF;
            CdStream_Runtime.state.flags0 &= 0xF7;
            return;
        }
        nextChunk                        = stateOrPosition.position / CdStream_Runtime.state.sectorsPerChunk;
        CdStream_Runtime.state.field_34 += 1;
        nextChunk                       += 1;
        if (nextChunk != CdStream_Runtime.state.field_34) {
            D_80068B67                     += 1;
            nextChunk                      -= 1;
            CdStream_Runtime.state.field_34 = nextChunk;
            CdStream_Runtime.state.field_20 = nextChunk;
            CdStream_Runtime.state.field_30 = CdStream_Runtime.state.startSector + ((s8)(u8)CdStream_Runtime.state.mtsPeriod * (s8)(u8)CdStream_Runtime.state.mode * nextChunk);
            if (CdStream_ErrorCode == 0) {
                CdStream_ErrorCode = 0xD;
            }
            goto stopVoices;
        }
        if (nextChunk < CdStream_Runtime.state.field_38) {
            entry.pollFn           = CdStream_PollMtsRead;
            entry.doneFn.callback  = CdStream_ClearReadySlot;
            entry.errorFn.callback = CdStream_AbortPhase;
            if ((u16)CdStream_Runtime.state.readySlot != 0) {
                slot  = CdStream_Runtime.state.readySlot;
                saved = CdReady_Queue.locked;
                if (slot != 0) {
                    queued = (CdReadyEntry*)&CdReady_Queue.entries[(s16)(slot - 1)];
                    flags  = queued->flags;
                    if (flags & 1) {
                        queued->flags = (flags & ~1) | 4;
                    }
                    CdReady_Queue.locked = saved;
                }
                CdStream_Runtime.state.readySlot = 0;
            }
            CdStream_Runtime.state.field_30 += (s8)(u8)CdStream_Runtime.state.mtsPeriod * (s8)(u8)CdStream_Runtime.state.mode;
            if (((u8)CdStream_Runtime.state.flags1 >> 2) & 1) {
                CdStream_Runtime.state.field_30 += (s8)CdStream_Runtime.state.mtsParam;
            }
            entry.sectorPos                  = CdStream_Runtime.state.field_30;
            CdStream_Runtime.state.field_20  = nextChunk;
            CdStream_Runtime.state.flags0   &= 0xFB;
            CdStream_Runtime.state.readySlot = CdReady_Enqueue(&entry);
            CdStream_Runtime.state.phase     = 2;
        }
        CdStream_Runtime.state.flags0 &= 0xF7;
    }
}

static void CdStream_CompleteChunkRead(void)
{
    CdReadyEntry  entry;
    s32           position;
    u16           slot;
    u8            saved;
    CdReadyEntry* queued;
    u32           flags;

    position = CdStream_Runtime.state.field_18;
    if ((CdStream_Runtime.state.field_20 + 1) < CdStream_Runtime.state.field_38) {
        if (((u8)CdStream_Runtime.state.flags0 >> 3) & 1) {
            CdStream_Runtime.state.field_4  = (u16)CdStream_Runtime.state.sectorsPerChunk + 1;
            CdStream_Runtime.state.field_18 = CdStream_Runtime.state.field_1C * (s16)(u16)CdStream_Runtime.state.sectorsPerChunk;
            position                        = CdStream_Runtime.state.field_18;
        } else {
            CdStream_Runtime.state.field_4 = (u16)CdStream_Runtime.state.sectorsPerChunk - 2;
            position                       = CdStream_Runtime.state.field_18;
        }
        if (!(((u8)CdStream_Runtime.state.flags0 >> 2) & 1)) {
            D_80068B5E += 1;
        stopVoices:
            if (((u8)CdStream_Runtime.state.flags0 >> 4) & 1) {
                Spu_KeyOff((u32)(s8)CdStream_Runtime.state.voiceL);
                Spu_KeyOff((u32)(s8)CdStream_Runtime.state.voiceR);
                if (CdStream_Runtime.state.voiceFreeCb != NULL) {
                    CdStream_Runtime.state.voiceFreeCb((1 << (s8)CdStream_Runtime.state.voiceL) | (1 << CdStream_Runtime.state.voiceR));
                }
                CdStream_Runtime.state.flags0 &= 0xEF;
            }
            CdStream_Runtime.state.field_4 = 0;
            CdStream_Runtime.state.flags0 &= 0xDF;
            if (CdStream_Runtime.state.flags0 & 1) {
                if (CdStream_ErrorCode == 0) {
                    CdStream_ErrorCode = 6;
                }
                CdStream_LastErrorCode         = CdStream_ErrorCode;
                CdStream_Runtime.state.flags0 &= 0xFE;
                func_800B0118((s32)(s16)CdStream_ErrorCode, 0);
                CdStream_ErrorCode             = 0;
                CdStream_Runtime.state.flags2 |= 8;
                CdStream_Runtime.state.flags2 |= 1;
                CdStream_Runtime.state.flags2 |= 4;
                if (((u8)CdStream_Runtime.state.flags0 >> 3) & 1) {
                    CdStream_Runtime.state.field_18 = CdStream_Runtime.state.field_1C * (s16)(u16)CdStream_Runtime.state.sectorsPerChunk;
                }
                CdStream_TeardownVoices();
            }
        } else {
            position                         = position / (s16)(u16)CdStream_Runtime.state.sectorsPerChunk;
            CdStream_Runtime.state.field_34 += 1;
            position                        += 1;
            if (position != CdStream_Runtime.state.field_34) {
                D_80068B67                     += 1;
                position                       -= 1;
                CdStream_Runtime.state.field_34 = position;
                CdStream_Runtime.state.field_20 = position;
                CdStream_Runtime.state.field_30 = CdStream_Runtime.state.startSector + ((s8)(u8)CdStream_Runtime.state.mtsPeriod * (s8)(u8)CdStream_Runtime.state.mode * position);
                if (CdStream_ErrorCode == 0) {
                    CdStream_ErrorCode = 0xD;
                }
                goto stopVoices;
            }
            if (position < CdStream_Runtime.state.field_38) {
                entry.pollFn           = CdStream_PollMtsRead;
                entry.doneFn.callback  = CdStream_ClearReadySlot;
                entry.errorFn.callback = CdStream_AbortPhase;
                if ((u16)CdStream_Runtime.state.readySlot != 0) {
                    slot  = (u16)CdStream_Runtime.state.readySlot;
                    saved = CdReady_Queue.locked;
                    if (slot != 0) {
                        queued = (CdReadyEntry*)&CdReady_Queue.entries[(s16)(slot - 1)];
                        flags  = queued->flags;
                        if (flags & 1) {
                            queued->flags = (flags & ~1) | 4;
                        }
                        CdReady_Queue.locked = saved;
                    }
                    CdStream_Runtime.state.readySlot = 0;
                }
                CdStream_Runtime.state.field_30 += (s8)(u8)CdStream_Runtime.state.mtsPeriod * (s8)(u8)CdStream_Runtime.state.mode;
                if (((u8)CdStream_Runtime.state.flags1 >> 2) & 1) {
                    CdStream_Runtime.state.field_30 += (s8)CdStream_Runtime.state.mtsParam;
                }
                entry.sectorPos                  = CdStream_Runtime.state.field_30;
                CdStream_Runtime.state.field_20  = position;
                CdStream_Runtime.state.flags0   &= 0xFB;
                CdStream_Runtime.state.readySlot = CdReady_Enqueue(&entry);
                CdStream_Runtime.state.phase     = 2;
            }
        }
        CdStream_Runtime.state.flags0 &= 0xF7;
    }
}

void CdStream_Drive(void)
{
    CdReadyEntry* restartEntry;
    CdReadyEntry* stopEntry;
    CdReadyEntry* endEntry;
    s32           position;
    u16           restartSlot, stopSlot, endSlot;
    u8            restartLock, stopLock, endLock;
    s32           phase;
    SpuVoiceAttr* channels;
    u32           restartFlags;
    u32           stopFlags;
    u32           endFlags;

    if (D_80068B58 == 0) {
        D_80068B58 = 1;
        if (CdStream_Runtime.state.flags0 & 1) {
            if (((u8)CdStream_Runtime.state.flags2 >> 1) & 1) {
                CdStream_Runtime.state.flags0 &= 0xFE;
                func_800B0118((s32)(s16)CdStream_ErrorCode, 0);
                CdStream_Runtime.state.flags2 |= 8;
                CdStream_ErrorCode             = 0;
                CdStream_Runtime.state.flags2 |= 1;
                CdStream_TeardownVoices();
            } else {
                if (((u8)CdStream_Runtime.state.flags1 >> 3) & 1) {
                    if (((u8)CdStream_Runtime.state.flags0 >> 4) & 1) {
                        Spu_KeyOff((u32)(s8)CdStream_Runtime.state.voiceL);
                        Spu_KeyOff((u32)(s8)CdStream_Runtime.state.voiceR);
                        if (CdStream_Runtime.state.voiceFreeCb != NULL) {
                            CdStream_Runtime.state.voiceFreeCb((1 << CdStream_Runtime.state.voiceL) | (1 << CdStream_Runtime.state.voiceR));
                        }
                        CdStream_Runtime.state.flags0 &= 0xEF;
                    }
                    if ((u16)CdStream_Runtime.state.readySlot != 0) {
                        restartSlot = (u16)CdStream_Runtime.state.readySlot;
                        restartLock = CdReady_Queue.locked;
                        if (restartSlot != 0) {
                            restartEntry = (CdReadyEntry*)&CdReady_Queue.entries[(s16)(restartSlot - 1)];
                            restartFlags = restartEntry->flags;
                            if (restartFlags & 1) {
                                restartEntry->flags = (restartFlags & ~1) | 4;
                            }
                            CdReady_Queue.locked = restartLock;
                        }
                        CdStream_Runtime.state.readySlot = 0;
                    }
                    CdStream_Runtime.state.flags0  &= 0xFB;
                    CdStream_Runtime.state.flags0  &= 0xDF;
                    CdStream_Runtime.state.field_4  = 0;
                    CdStream_Runtime.state.field_18 = CdStream_Runtime.state.field_14;
                    CdStream_Runtime.state.flags1  &= 0xF7;
                    CdStream_Runtime.state.flags0  |= 2;
                }
                if (((u8)CdStream_Runtime.state.flags0 >> 6) & 1) {
                    if (((u8)CdStream_Runtime.state.flags0 >> 4) & 1) {
                        Spu_KeyOff((u32)(s8)CdStream_Runtime.state.voiceL);
                        Spu_KeyOff((u32)(s8)CdStream_Runtime.state.voiceR);
                        if (CdStream_Runtime.state.voiceFreeCb != NULL) {
                            CdStream_Runtime.state.voiceFreeCb((1 << (s8)CdStream_Runtime.state.voiceL) | (1 << CdStream_Runtime.state.voiceR));
                        }
                        CdStream_Runtime.state.flags0 &= 0xEF;
                    }
                    CdStream_Runtime.state.flags0 &= 0xFB;
                    CdStream_Runtime.state.flags0 &= 0xDF;
                    CdStream_Runtime.state.flags0 &= 0xFE;
                    if ((u16)CdStream_Runtime.state.readySlot != 0) {
                        stopSlot = (u16)CdStream_Runtime.state.readySlot;
                        stopLock = CdReady_Queue.locked;
                        if (stopSlot != 0) {
                            stopEntry = (CdReadyEntry*)&CdReady_Queue.entries[(s16)(stopSlot - 1)];
                            stopFlags = stopEntry->flags;
                            if (stopFlags & 1) {
                                stopEntry->flags = (stopFlags & ~1) | 4;
                            }
                            CdReady_Queue.locked = stopLock;
                        }
                        CdStream_Runtime.state.readySlot = 0;
                    }
                    if (((u8)CdStream_Runtime.state.flags2 >> 3) & 1) {
                        func_800B0118(0, 0);
                        CdStream_Runtime.state.flags2 &= 0xF7;
                    }
                    CdStream_Runtime.state.flags0 &= 0xBF;
                    CdStream_Runtime.state.flags2 |= 1;
                } else if (!(((u8)CdStream_Runtime.state.flags0 >> 1) & 1)) {
                    phase = (s8)CdStream_Runtime.state.phase;
                    if (phase == 1) {
                        CdStream_Runtime.state.flags0 |= 2;
                        CdStream_Runtime.state.field_4 = phase;
                        D_80068B60                     = phase;
                    }
                } else {
                    position = CdStream_Runtime.state.field_18;
                    if (position == 0) {
                        channels = PARENT_OF(&CdStream_Runtime.state, CdStreamRuntime, state)->channels.ch;
                        CdAudio_AllocVoices((s8*)&CdStream_Runtime.state.voiceL, (s8*)&CdStream_Runtime.state.voiceR);
                        channels->voice   = (s32)(1 << (s8)CdStream_Runtime.state.voiceL);
                        channels[1].voice = (s32)(1 << CdStream_Runtime.state.voiceR);
                        if (!(((u8)CdStream_Runtime.state.flags0 >> 4) & 1)) {
                            Spu_ArmKeyOn((u32)(s8)CdStream_Runtime.state.voiceL);
                            Spu_ArmKeyOn((u32)(s8)CdStream_Runtime.state.voiceR);
                        }
                        if (CdStream_Runtime.state.startCb != NULL) {
                            CdStream_Runtime.state.startCb((1 << CdStream_Runtime.state.voiceL) | (1 << CdStream_Runtime.state.voiceR));
                        }
                        CdStream_Runtime.state.flags0 |= 0x10;
                        CdStream_Runtime.state.flags1 |= 0x80;
                    }
                    if (CdStream_Runtime.state.flags1 & 1) {
                        CdAudio_CopyVoiceData((s8)CdStream_Runtime.state.voiceL, PARENT_OF(&CdStream_Runtime.state, CdStreamRuntime, state)->channels.ch);
                        /* The channels follow CdStream_Runtime.state; addressing them from its symbol
                         * shares its high half, where &CdStream_Runtime.channels would load another. */
                        CdAudio_CopyVoiceData((s8)CdStream_Runtime.state.voiceR, PARENT_OF(&CdStream_Runtime.state, CdStreamRuntime, state)->channels.ch + 1);
                        CdStream_Runtime.state.flags1 &= 0xFE;
                    }
                    if ((position / (s16)(u16)CdStream_Runtime.state.sectorsPerChunk >= CdStream_Runtime.state.field_38 - 1) &&
                        ((((u8)CdStream_Runtime.state.flags1 >> 4) & 1) ||
                         ((((u8)CdStream_Runtime.state.flags1 >> 6) & 1) ? (position / (s16)(u16)CdStream_Runtime.state.sectorsPerChunk >= CdStream_Runtime.state.field_38) : (position % (s16)(u16)CdStream_Runtime.state.sectorsPerChunk >= (CdStream_Runtime.state.sectorsPerChunk >> 1))) ||
                         (position / (s16)(u16)CdStream_Runtime.state.sectorsPerChunk >= CdStream_Runtime.state.field_38))) {
                        if (((u8)CdStream_Runtime.state.flags0 >> 4) & 1) {
                            Spu_KeyOff((u32)(s8)CdStream_Runtime.state.voiceL);
                            Spu_KeyOff((u32)(s8)CdStream_Runtime.state.voiceR);
                            if (CdStream_Runtime.state.voiceFreeCb != NULL) {
                                CdStream_Runtime.state.voiceFreeCb((1 << (s8)CdStream_Runtime.state.voiceL) | (1 << CdStream_Runtime.state.voiceR));
                            }
                            CdStream_Runtime.state.flags0 &= 0xEF;
                        }
                        if ((u16)CdStream_Runtime.state.readySlot != 0) {
                            endSlot = (u16)CdStream_Runtime.state.readySlot;
                            endLock = CdReady_Queue.locked;
                            if (endSlot != 0) {
                                endEntry = (CdReadyEntry*)&CdReady_Queue.entries[(s16)(endSlot - 1)];
                                endFlags = endEntry->flags;
                                if (endFlags & 1) {
                                    endEntry->flags = (endFlags & ~1) | 4;
                                }
                                CdReady_Queue.locked = endLock;
                            }
                            CdStream_Runtime.state.readySlot = 0;
                        }
                        if (((u8)CdStream_Runtime.state.flags2 >> 3) & 1) {
                            func_800B0118(0, 0);
                            CdStream_Runtime.state.flags2 &= 0xF7;
                        }
                        if (D_80068B5C != 0) {
                            SpuSetIRQ(0);
                            SpuSetIRQCallback(NULL);
                            D_80068B5C = 0;
                        }
                        CdStream_Runtime.state.flags0 &= 0xFB;
                        CdStream_Runtime.state.flags0 &= 0xDF;
                        CdStream_Runtime.state.flags0 &= 0xFE;
                    } else {
                        if (((u8)CdStream_Runtime.state.flags2 >> 2) & 1) {
                            if (CdStream_Runtime.state.field_1C & 1) {
                                if (D_80068B5C != 0) {
                                    SpuSetIRQ(0);
                                    SpuSetIRQCallback(NULL);
                                    D_80068B5C = 0;
                                }
                                CdStream_CompleteChunkRead();
                                CdStream_Runtime.state.field_4 += 2;
                            } else {
                                if (D_80068B5C != 0) {
                                    SpuSetIRQ(0);
                                    SpuSetIRQCallback(NULL);
                                    D_80068B5C = 0;
                                }
                                CdStream_Runtime.state.field_4 = 0;
                                CdStream_TickPlayback();
                            }
                            CdStream_Runtime.state.flags2 &= 0xFB;
                        } else if (CdStream_Runtime.state.field_4 != 0) {
                            CdStream_Runtime.state.field_4 -= 1;
                            if ((s8)CdStream_Runtime.state.field_4 < (CdStream_Runtime.state.sectorsPerChunk >> 2)) {
                                if ((((u8)CdStream_Runtime.state.flags0 >> 3) & 1) || (CdStream_Runtime.state.field_4 == 0)) {
                                    if ((u8)D_80068B60 != 0) {
                                        D_80068B60 = 0;
                                    } else if (!(((u8)CdStream_Runtime.state.flags0 >> 3) & 1)) {
                                        D_80068B61 += 1;
                                    }
                                    if (D_80068B5C != 0) {
                                        SpuSetIRQ(0);
                                        SpuSetIRQCallback(NULL);
                                        D_80068B5C = 0;
                                    }
                                    CdStream_Runtime.state.field_4 = 0;
                                    CdStream_TickPlayback();
                                }
                            }
                        } else if (((CdStream_Runtime.state.field_18 % (CdStream_Runtime.state.sectorsPerChunk * 2)) >= ((s16)(u16)CdStream_Runtime.state.sectorsPerChunk - 4)) && ((((u8)CdStream_Runtime.state.flags0 >> 3) & 1) || ((CdStream_Runtime.state.field_18 % (CdStream_Runtime.state.sectorsPerChunk * 2)) == ((s16)(u16)CdStream_Runtime.state.sectorsPerChunk + 3)))) {
                            if (!(((u8)CdStream_Runtime.state.flags0 >> 3) & 1)) {
                                D_80068B61 += 1;
                            }
                            if (D_80068B5C != 0) {
                                SpuSetIRQ(0);
                                SpuSetIRQCallback(NULL);
                                D_80068B5C = 0;
                            }
                            CdStream_CompleteChunkRead();
                        }
                        if (CdStream_Runtime.state.flags0 & 1) {
                            CdStream_Runtime.state.field_18 += 1;
                        }
                    }
                }
            }
        }
        CdReady_Poll();
        D_80068B58 = 0;
    }
}

static s32 CdStream_PollMtsRead(CdReadyEntry* entry)
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
    u32 flags;
    u32 modeFlags;
    u32 irqAddr1;
    u32 irqAddr2;
    u32 irqOffset;
    u32 initFlags;
    u32 errorCode;

    flags = entry->flags;
    if ((flags >> 1) & 1) {
        entry->flags          = flags & ~2;
        CdStream_PhaseTimeout = 0;
        if (!(CdStream_Runtime.state.flags & 1) && CdStream_Runtime.state.pending == 0) {
            if ((CdStream_Runtime.state.flags1 >> 1) & 1) {
                initFlags    = entry->flags & ~0x10;
                initFlags   &= ~0x1FE0;
                entry->flags = initFlags | 0x80;
            } else {
                if (!(CdMode() & 0x80)) {
                    entry->flags |= 0x10;
                } else {
                    entry->flags &= ~0x10;
                }
                entry->flags = (entry->flags & ~0x1FE0) | 0x20;
            }
        } else {
            entry->flags = (entry->flags & ~0x1FE0) | 0x80;
            if (CdStream_Runtime.state.pending != 0) {
                AsyncCb_Cancel((s16)CdStream_Runtime.state.pending);
            }
            sp.entry.field_8               = CdStream_InitDisc;
            sp.entry.field_C               = CdStream_MarkEnding;
            sp.entry.field_10              = CdStream_Flush;
            CdStream_Runtime.state.pending = AsyncCb_Enqueue(&sp.entry);
        }
    }
    CdStream_CurrentPhase = (entry->flags >> 5) & 0xFF;
    if (CdStream_Runtime.state.pending != 0) {
        return 0;
    }
    phase = (entry->flags >> 5) & 0xFF;
    switch (phase) {
        case 1:
            sp.mode[0] = -0x60;
        retry_mode:
            CdControlF(CdlSetmode, (u8*)&sp.mode[0]);
            entry->flags = (entry->flags & ~0x1FE0) | 0x40;
            goto phase_advanced;
        case 2:
            modeSync = CdSync(1, &sp.result[0]);
            if (modeSync == CdlDiskError) {
                D_80068B64 += 1;
                if (sp.result[0] & CdlStatShellOpen) {
                    if (CdStream_ErrorCode == 0) {
                        CdStream_ErrorCode = (u16)modeSync;
                    }
                    CdStream_LastErrorCode        = CdStream_ErrorCode;
                    CdStream_Runtime.state.flags |= 1;
                    CdStream_ShellOpenErrors++;
                    goto stream_error;
                }
                if (CdStatus() & CdlStatShellOpen) {
                    if (CdStream_ErrorCode == 0) {
                        CdStream_ErrorCode = (u16)modeSync;
                    }
                    CdStream_LastErrorCode        = CdStream_ErrorCode;
                    CdStream_Runtime.state.flags |= 1;
                    CdStream_ShellOpenErrors++;
                    goto stream_error;
                }
                if (CdStream_PhaseTimeout++ < 0x258) {
                    goto retry_mode;
                }
                goto stream_error;
            }
            CdStream_Runtime.state.flags1 |= 2;
            modeFlags                      = entry->flags;
            if (!((modeFlags >> 4) & 1)) {
                entry->flags = (modeFlags & ~0x1FE0) | 0x80;
            } else {
                entry->flags          = (modeFlags & ~0x1FE0) | 0x60;
                CdStream_PhaseTimeout = 3;
                case 3:
                    if (--CdStream_PhaseTimeout != 0) {
                        goto phase_advanced;
                    }
                    entry->flags = (entry->flags & ~0x1FE0) | 0x80;
            }
            CdStream_PhaseTimeout = 0;
        case 4:
            D_80068B54                      = 0;
            CdStream_Runtime.state.field_4C = 5;
            CdStream_Runtime.state.field_2C = entry->sectorPos;
        set_location:
            CdIntToPos(entry->sectorPos, &sp.loc);
            CdControlF(CdlSetloc, &sp.loc.minute);
            CdStream_PhaseTimeout = 0;
            entry->flags          = (entry->flags & ~0x1FE0) | 0xA0;
        case 5:
            locationSync = CdSync(1, &sp.result[0]);
            if (locationSync == CdlDiskError) {
                D_80068B64 += 1;
                if (sp.result[0] & CdlStatShellOpen) {
                    if (CdStream_ErrorCode == 0) {
                        CdStream_ErrorCode = (u16)locationSync;
                    }
                    CdStream_LastErrorCode        = CdStream_ErrorCode;
                    CdStream_Runtime.state.flags |= 1;
                    CdStream_ShellOpenErrors++;
                    goto stream_error;
                }
                if (CdStatus() & CdlStatShellOpen) {
                    if (CdStream_ErrorCode == 0) {
                        CdStream_ErrorCode = (u16)locationSync;
                    }
                    CdStream_LastErrorCode        = CdStream_ErrorCode;
                    CdStream_Runtime.state.flags |= 1;
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
            entry->flags = (entry->flags & ~0x1FE0) | 0xC0;
        case 6:
            CdStream_PhaseTimeout = 0;
        start_read:
            CdReady_InstallCallback(CdStream_ReadyMts);
            CdStream_Runtime.state.remaining = 0;
            CdStream_Runtime.state.field_4C  = 1;
            CdControlF(CdlReadN, NULL);
            entry->flags = (entry->flags & ~0x1FE0) | 0xE0;
            goto phase_advanced;
        case 7:
            D_80068B54 = 0;
            readSync   = CdSync(1, &sp.result[0]);
            if (readSync == CdlDiskError) {
                CdReady_ClearCallback();
                entry->flags = (entry->flags & ~0x1FE0) | 0xC0;
                if (sp.result[0] & CdlStatShellOpen) {
                    if (CdStream_ErrorCode == 0) {
                        CdStream_ErrorCode = (u16)readSync;
                    }
                    CdStream_LastErrorCode        = CdStream_ErrorCode;
                    CdStream_Runtime.state.flags |= 1;
                    CdStream_ShellOpenErrors++;
                    goto stream_error;
                }
                if (CdStatus() & CdlStatShellOpen) {
                    if (CdStream_ErrorCode == 0) {
                        CdStream_ErrorCode = (u16)readSync;
                    }
                    CdStream_LastErrorCode        = CdStream_ErrorCode;
                    CdStream_Runtime.state.flags |= 1;
                    CdStream_ShellOpenErrors++;
                    goto stream_error;
                }
                if (CdStream_PhaseTimeout++ < 0x258) {
                    goto start_read;
                }
                goto stream_error;
            }
            entry->flags = (entry->flags & ~0x1FE0) | 0x100;
        case 8:
            if (CdStream_Runtime.state.field_4C == 1) {
                goto wait_for_progress;
            }
            if (CdStream_Runtime.state.field_4C == 0) {
                goto wait_for_progress;
            }
            if (CdStream_Runtime.state.field_4C == 4) {
                CdStream_Runtime.state.flags0 |= 4;
                CdReady_ClearCallback();
                if (CdStream_Runtime.state.field_1C & 1) {
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
                } else if (!((CdStream_Runtime.state.flags0 >> 4) & 1)) {
                    CdStream_Runtime.state.flags0 |= 0x20;
                } else if (CdStream_Runtime.state.field_1C != 0) {
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
            } else if (CdStream_Runtime.state.field_4C != 2) {
                if (CdStatus() & CdlStatShellOpen) {
                    if (CdStream_ErrorCode == 0) {
                        CdStream_ErrorCode = 5;
                    }
                    CdStream_LastErrorCode        = CdStream_ErrorCode;
                    CdStream_Runtime.state.flags |= 1;
                    CdStream_ShellOpenErrors++;
                    goto stream_error;
                }
                if (CdStream_ErrorCode == 0) {
                    CdStream_ErrorCode = 9;
                }
                CdStream_Runtime.state.flags2 |= 2;
                CdStream_Runtime.state.flags0 &= 0xFB;
            } else {
                goto wait_for_progress;
            }
            CdStream_PhaseTimeout           = 0;
            CdStream_Runtime.state.field_4C = 0;
            CdReady_ClearCallback();
            entry->flags = (entry->flags & ~0x1FE0) | 0x120;
        case 9:
            CdStream_PhaseTimeout = 0;
        pause_read:
            CdControlF(CdlPause, NULL);
            entry->flags = (entry->flags & ~0x1FE0) | 0x160;
            goto phase_advanced;
        case 11:
            D_80068B54 = 0;
            pauseSync  = CdSync(1, &sp.result[0]);
            if (pauseSync == CdlDiskError) {
                D_80068B64 += 1;
                if ((sp.result[0] & CdlStatShellOpen) || (CdStatus() & CdlStatShellOpen)) {
                    CdStream_Runtime.state.flags |= 1;
                    CdStream_ShellOpenErrors++;
                    if (CdStream_ErrorCode == 0) {
                        CdStream_ErrorCode = (u16)pauseSync;
                    }
                    CdStream_LastErrorCode = CdStream_ErrorCode;
                    goto stream_error;
                }
                entry->flags = (entry->flags & ~0x1FE0) | 0x120;
                if (CdStream_PhaseTimeout >= 0x258) {
                    goto stream_error;
                }
                CdStream_PhaseTimeout += 1;
                goto pause_read;
            }
            entry->flags = (entry->flags & ~0x1FE0) | 0x180;
        case 12:
            if (SpuIsTransferCompleted(0) == 0) {
                goto wait_for_progress;
            }
            entry->flags = (entry->flags & ~0x1FE0) | 0x140;
        case 10:
            if (CdSync(1, &sp.result[0]) == 0) {
                goto wait_for_progress;
            }
            CdStream_CurrentPhase = 0;
            return 1;
    }
phase_advanced:
    CdStream_LastPhase = (entry->flags >> 5) & 0xFF;
    return 0;
wait_for_progress:
    CdStream_LastPhase = (entry->flags >> 5) & 0xFF;
    if (++CdStream_PhaseTimeout < 0x259) {
        return 0;
    }
stream_error:
    CdStream_Runtime.state.flags2 |= 2;
    if (CdStream_ErrorCode == 0) {
        errorCode          = entry->flags;
        errorCode          = (errorCode >> 5) & 0xFF;
        errorCode         *= 0x10;
        CdStream_ErrorCode = errorCode | 0xA;
    }
    CdStream_Runtime.state.field_4C = 0;
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
    CdStreamChannels*       channels;
    s32                     channelCount;

    if ((u16)CdStream_ReadyCallbackActive != 0) {
        CdStream_ErrorCode             = 0xC;
        CdStream_LastErrorCode         = CdStream_ErrorCode;
        CdStream_Runtime.state.flags2 |= 2;
        return;
    }
    CdStream_ReadyCallbackActive = 1;
    intr                         = interrupt & 0xFF;
    if (intr == 1) {
        if ((s16)(u16)CdStream_Runtime.state.field_4C != 2) {
            if ((s16)(u16)CdStream_Runtime.state.field_4C == intr) {
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
            if (sectorPos != CdStream_Runtime.state.field_2C) {
                if (sectorPos < CdStream_Runtime.state.field_2C) {
                    if (CdStream_Runtime.state.field_2C >= (sectorPos + 4)) {
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
                CdStream_Runtime.state.field_2C += 1;
                if ((s16)(u16)CdStream_Runtime.state.field_4C == 2) {
                    skipIndex = D_80068B78++ & 0xFF;
                    skipStamp = skipIndex | ((CdStream_Runtime.state.field_1C << 8) & 0xFFFF00);
                    if (D_80068B74 < skipStamp) {
                        if ((func_800AF590(0, 0) << 0x10) == 0) {
                            *(volatile s32*)&D_80068B74 = skipStamp;
                            goto advance_countdown;
                        }
                        goto check_status;
                    }
                    CdGetSector(CdStream_Runtime.state.sector, 0x200);
                advance_countdown:
                    state                            = &CdStream_Runtime.state;
                    CdStream_Runtime.state.countdown = (u16)CdStream_Runtime.state.countdown - 1;
                    if ((u16)CdStream_Runtime.state.countdown == 0) {
                        CdStream_Runtime.state.field_4C = 4;
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
                    if ((u16)CdStream_Runtime.state.remaining == 0) {
                        if ((CdStream_Runtime.state.sector->magic & ~0xFF) != 0x4D545300) {
                            CdStream_ErrorCode     = 8;
                            CdStream_LastErrorCode = CdStream_ErrorCode;
                            goto check_status;
                        }
                        if (CdStream_Runtime.state.sector->field_0 == 0) {
                            CdStream_Runtime.state.mtsPeriod = (s8)CdStream_Runtime.state.sector->field_D;
                            CdStream_Runtime.state.remaining = (s16)(s8)(u8)CdStream_Runtime.state.mtsPeriod;
                            CdStream_Runtime.state.mtsParam  = CdStream_Runtime.state.sector->field_E;
                            if (CdStream_Runtime.state.sector->field_F & 0x80) {
                                CdStream_Runtime.state.flags1 |= 4;
                            } else {
                                CdStream_Runtime.state.flags1 &= 0xFB;
                            }
                            regionState  = &CdStream_Runtime.state;
                            chunkSectors = 0x30;
                            if ((s8)(u8)regionState->mtsPeriod == 5) {
                                chunkSectors          = 0x18;
                                regionState->ringHalf = 0x2770;
                                if (gDisplayState.region == 1) {
                                    chunkSectors = 0x14;
                                }
                            } else {
                                regionState->ringHalf = 0x4ED0;
                                if (gDisplayState.region == 1) {
                                    chunkSectors = 0x28;
                                }
                            }
                            regionState->sectorsPerChunk = chunkSectors;
                            channels                     = &CdStream_Runtime.channels;
                            /* Recover the common allocation from its channel member. */
                            channelState         = &PARENT_OF(channels, CdStreamRuntime, channels)->state;
                            channels->ch[0].addr = channelState->spuBase;
                            /* The channels' SPU buffers sit back to back, ringHalf * 2 + 0x40 bytes apart. */
                            channels->ch[1].addr   = channelState->spuBase + (channelState->ringHalf * 2 + 0x40);
                            channelState->flags1  |= 1;
                            channels->ch[0].mask  |= 0x80;
                            channels->ch[1].mask   = channels->ch[0].mask;
                            channelState->field_1C = (s32)channelState->sector->field_0;
                            channelState->field_38 = (s32)channelState->sector->field_4;
                            if (!((u8)channelState->sector->field_F & 0x60)) {
                                channelState->flags1 = (u8)(channelState->flags1 | 0x40);
                            } else if ((u8)channelState->sector->field_F & 0x40) {
                                channelState->flags1 = (u8)(channelState->flags1 | 0x20);
                            } else {
                                channelState->flags1 = (u8)(channelState->flags1 | 0x10);
                            }
                            goto start_chunk;
                        }
                        if (CdStream_Runtime.state.sector->field_F & 0x80) {
                            CdStream_Runtime.state.mtsParam = CdStream_Runtime.state.sector->field_E;
                            if (CdStream_Runtime.state.mtsParam != 0) {
                                CdStream_Runtime.state.flags1 |= 4;
                            } else {
                                CdStream_Runtime.state.flags1 &= 0xFB;
                            }
                        } else {
                            CdStream_Runtime.state.flags1  &= 0xFB;
                            CdStream_Runtime.state.mtsParam = 0;
                        }
                        CdStream_Runtime.state.remaining = (s16)(s8)(u8)CdStream_Runtime.state.mtsPeriod;
                    }
                    if (((s16)(u16)CdStream_Runtime.state.remaining % (s8)(u8)CdStream_Runtime.state.mtsPeriod) == 0) {
                        CdStream_Runtime.state.field_1C = CdStream_Runtime.state.sector->field_0;
                        if (CdStream_Runtime.state.field_1C == 0) {
                            CdStream_Runtime.state.field_38 = CdStream_Runtime.state.sector->field_4;
                        }
                    start_chunk:
                        channelCount = (u8)CdStream_Runtime.state.sector->magic;
                        /* Preserve the signed comparison of the channel count. */
                        if ((channelCount >= 2) && (CdStream_Runtime.state.sector->field_C == 0)) {
                            CdStream_Runtime.state.mode      = (s8)(u8)CdStream_Runtime.state.sector->magic;
                            CdStream_Runtime.state.remaining = (s8)(u8)CdStream_Runtime.state.mtsPeriod * (s8)(u8)CdStream_Runtime.state.mode;
                        }
                        if (CdStream_Runtime.state.field_1C & 1) {
                            CdStream_Runtime.state.spuAddr = CdStream_Runtime.state.spuBase + (s16)(u16)CdStream_Runtime.state.ringHalf;
                        } else {
                            CdStream_Runtime.state.spuAddr = CdStream_Runtime.state.spuBase;
                        }
                        CdStream_Runtime.state.spuAddr += CdStream_Runtime.state.sector->field_C * (CdStream_Runtime.state.ringHalf * 2 + 0x40);
                        SpuSetTransferStartAddr((u32)CdStream_Runtime.state.spuAddr);
                        *(volatile s32*)&CdStream_LastTransferSpuAddress = CdStream_Runtime.state.spuAddr;
                        /* The audio in a header sector starts after the MtsSector header. */
                        SpuWrite((u8*)(CdStream_Runtime.state.sector + 1), 0x800U);
                        *(void* volatile*)&CdStream_LastTransferBuffer = CdStream_Runtime.state.sector + 1;
                        CdStream_Runtime.state.spuAddr                += 0x7F0;
                    } else {
                        SpuSetTransferStartAddr((u32)CdStream_Runtime.state.spuAddr);
                        *(volatile s32*)&CdStream_LastTransferSpuAddress = CdStream_Runtime.state.spuAddr;
                        if (((s16)(u16)CdStream_Runtime.state.remaining % (s8)(u8)CdStream_Runtime.state.mtsPeriod) == 1) {
                            writeSize = ((s16)(u16)CdStream_Runtime.state.ringHalf - 0x7F0) % 0x800;
                            if (writeSize == 0) {
                                writeSize = 0x800;
                            }
                            if (!(CdStream_Runtime.state.field_1C & 1)) {
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
                    state                            = &CdStream_Runtime.state;
                    CdStream_Runtime.state.remaining = (u16)CdStream_Runtime.state.remaining - 1;
                    if ((u16)CdStream_Runtime.state.remaining == 0) {
                        if (((u8)CdStream_Runtime.state.flags1 >> 2) & 1) {
                            CdStream_Runtime.state.countdown = (s16)(s8)CdStream_Runtime.state.mtsParam;
                            if ((u16)CdStream_Runtime.state.countdown != 0) {
                                CdStream_Runtime.state.field_4C = 2;
                                D_80068B78                      = 0;
                                goto unlock;
                            } else {
                                goto mark_complete;
                            }
                        } else {
                        mark_complete:
                            state->field_4C = 4;
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
            CdStream_Runtime.state.flags    |= 1;
            CdStream_Runtime.state.field_4C  = 3;
            CdStream_Runtime.state.flags2   |= 2;
            *(&D_80068B5C + 1) = (u8)(*(&D_80068B5C + 1) + 1);
        } else if ((u16)CdStream_Runtime.state.field_4C != 0) {
            if ((s16)(u16)CdStream_Runtime.state.field_4C != 4) {
                CdStream_Runtime.state.field_4C = 3;
                if (CdStream_ErrorCode == 0) {
                    CdStream_ErrorCode = 0xB;
                }
                CdStream_LastErrorCode         = CdStream_ErrorCode;
                CdStream_Runtime.state.flags2 |= 2;
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

    if (entry->field_0.bits.firstPoll) {
        entry->field_0.bits.firstPoll = 0;
        entry->field_0.bits.pollState = 1;
    }

    D_80068B66 = entry->field_0.bits.pollState;
    switch (entry->field_0.bits.pollState) {
        case 1:
            if (CdControlB(CdlNop, NULL, sp.result) == 0) {
                return 0;
            }
            if (sp.result[0] & CdlStatShellOpen) {
                return 0;
            }
            if (sp.result[0] & CdlStatStandby) {
                entry->field_0.bits.pollState = 2;
                case 2:
                    if (CdControl(CdlGetTN, NULL, sp.result) != 0) {
                        entry->field_0.bits.pollState = 4;
                        case 3:
                            sync = CdSync(1, sp.result);
                            if (sync == CdlDiskError) {
                                entry->field_0.bits.pollState = 2;
                            } else if (sync == CdlComplete) {
                                entry->field_0.bits.pollState = 4;
                                case 4:
                                    CdIntToPos(0, &sp.loc);
                                    if (CdControl(CdlSeekL, (u8*)&sp.loc, sp.result) != 0) {
                                        entry->field_0.bits.pollState = 5;
                                        case 5:
                                            sync = CdSync(1, sp.result);
                                            if ((sync == CdlDiskError) && (sp.result[0] & CdlStatError) &&
                                                (sp.result[1] & 0x40)) {
                                                entry->field_0.bits.pollState = 1;
                                            } else if (sync == CdlComplete) {
                                                entry->field_0.bits.pollState = 6;
                                                case 6:
                                                    sp.mode = -0x60;
                                                    if (CdControl(CdlSetmode, (u8*)&sp.mode, NULL) != 0) {
                                                        entry->field_0.bits.pollState        = 7;
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
            if (CdStream_Runtime.state.settleCounter >= 4) {
                return 1;
            }
            break;
    }
    return 0;
}

static void CdReady_InstallCallback(CdlCB arg0)
{
    volatile CdReadyQueue* p;

    p = &CdReady_Queue;
    if (p->callbackInstalled == 0) {
        p->prevCallback = CdReadyCallback(arg0);
    } else {
        CdReadyCallback(arg0);
    }
    CdReady_Queue.callbackInstalled = 1;
}

static void CdReady_ClearCallback(void)
{
    volatile CdReadyQueue* p;

    p = &CdReady_Queue;
    if (p->callbackInstalled != 0) {
        CdReadyCallback(0);
        p->prevCallback      = 0;
        p->callbackInstalled = 0;
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

    CdStream_Runtime.state.flags0 = CdStream_Runtime.state.flags0 & 0xFD;
    CdStream_Runtime.state.flags0 = CdStream_Runtime.state.flags0 & 0xF7;
    p                             = &CdStream_Runtime.state;
    p->field_4                    = 1;
    p->field_18                   = 0;
    SpuSetIRQ(0);
    SpuSetIRQCallback(CdStream_SpuIrqHandler);
    SpuSetIRQAddr((p->spuBase + p->ringHalf + 0x4F) & ~0x3F);
    CdStream_Runtime.state.flags0 = CdStream_Runtime.state.flags0 & 0xBF;
    CdStream_Runtime.state.flags0 = CdStream_Runtime.state.flags0 | 1;
}

static void CdStream_SpuIrqHandler(void)
{
    CdStream_Runtime.state.flags0 = CdStream_Runtime.state.flags0 | 8;
}

void CdStream_SetVolume(s16 volume)
{
    CdStreamChannels*       p;
    volatile CdStreamState* q;
    SpuVoiceAttr*           ch1b;
    SpuVoiceAttr*           ch1;
    s16                     val;
    s32                     t0;
    s32                     t1;

    p = &CdStream_Runtime.channels;
    /* CdStream_Runtime.state sits directly before the channels; reaching it back from
     * `p` keeps one base register for both objects. */
    q = &PARENT_OF(p, CdStreamRuntime, channels)->state;

    if ((q->flags0 >> 1) & 1) {
        if (q->flags1 & 1) {
            t0            = p->ch[0].mask;
            t1            = p->ch[1].mask;
            p->ch[0].mask = t0 | 3;
            p->ch[1].mask = t1 | 3;
        } else {
            p->ch[1].mask = 3;
            p->ch[0].mask = 3;
            q->flags1     = q->flags1 | 1;
        }
    }

    if (CdStream_Runtime.state.flags & 2) {
        ch1b                  = &p->ch[1];
        val                   = (s16)((volume * 0xB5) >> 8);
        ch1b->volume.left     = val;
        p->ch[0].volume.right = val;
        ch1b->volume.right    = val;
        p->ch[0].volume.left  = val;
        return;
    }
    ch1                   = &p->ch[1];
    ch1->volume.right     = volume;
    p->ch[0].volume.left  = volume;
    ch1->volume.left      = 0;
    p->ch[0].volume.right = 0;
}

static void CdStream_SetFlag14(s32 arg0)
{
    volatile CdStreamState* p;
    u8                      temp;

    p    = &CdStream_Runtime.state;
    temp = p->flags1;
    if (temp >> 7) {
        p->field_14 = arg0;
        p->flags1   = p->flags1 | 8;
        p->flags0   = p->flags0 | 1;
    }
}

static void CdStream_AbortPhase(CdReadyEntry* entry)
{
    u32 temp_v1;

    temp_v1 = entry->flags;
    if ((temp_v1 >> 1) & 1) {
        entry->flags = temp_v1 & ~8;
        return;
    }
    entry->flags                    = temp_v1 & ~8;
    CdStream_Runtime.state.field_4C = 0;
    switch ((entry->flags >> 5) & 0xFF) {
        case 6:
        case 7:
            CdReady_ClearCallback();
            goto shared_flush;
        case 8:
            CdReady_ClearCallback();
            CdFlush();
            CdControlF(CdlPause, NULL);
            entry->flags = ((entry->flags | 8) & ~0x1FE0) | 0x1C0;
            break;
        case 9:
        case 11:
        case 12:
        case 14:
            CdFlush();
            entry->flags = (entry->flags & ~0x1FE0) | 0x1A0;
            /* fallthrough */
        case 13:
            if (SpuIsTransferCompleted(0) == 0) {
                entry->flags |= 8;
            }
            break;
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 10:
        shared_flush:
            CdFlush();
            break;
    }
}

static void CdStream_FinishQueueEntry(CdReadyEntry* entry)
{
    CdStream_AbortPhase(entry);
    if (!((entry->flags >> 3) & 1) && (CdStream_Runtime.state.doneCb != NULL)) {
        CdStream_Runtime.state.doneCb(0);
    }
}

static void CdReady_Cancel(s16 arg0)
{
    u8            temp;
    s16           idx;
    u32           flags;
    CdReadyEntry* entry;

    temp = CdReady_Queue.locked;
    if (arg0 != 0) {
        idx   = arg0 - 1;
        entry = (CdReadyEntry*)&CdReady_Queue.entries[idx];
        flags = entry->flags;
        if (flags & 1) {
            entry->flags = (flags & ~1) | 4;
        }
        CdReady_Queue.locked = temp;
    }
}

s32 CdStream_IsBusy(void)
{
    if (CdStream_Runtime.state.flags0 & 1) {
        return 1;
    }
    return (CdReady_Queue.writeIdx != CdReady_Queue.readIdx) ? 1 : (CdStream_Runtime.state.pending != 0);
}

static void CdStream_ClearReadySlot(void)
{
    CdStream_Runtime.state.readySlot = 0;
}

void CdStream_SetMono(s32 enabled)
{
    if ((s8)enabled) {
        CdStream_Runtime.state.flags = CdStream_Runtime.state.flags | 2;
    } else {
        CdStream_Runtime.state.flags = CdStream_Runtime.state.flags & 0xFD;
    }
}

static void CdStream_MarkEnding(AsyncCbEntry* unused)
{
    CdStream_Runtime.state.flags   = CdStream_Runtime.state.flags & 0xFE;
    CdStream_Runtime.state.flags1  = CdStream_Runtime.state.flags1 | 2;
    CdStream_Runtime.state.pending = 0;
}

static s32 CdStream_Flush(AsyncCbEntry* unused)
{
    CdFlush();
    return 0;
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

static void CdStream_UnusedStub(void)
{
}
