#include "sound.h"

#include <psyq/sys/types.h>
#include <psyq/libspu.h>

#include "common.h"

#include "cdaudio.h"
#include "sound_types.h"

typedef struct _SpuReverbConfig {
    u32           enableVoices;
    u32           disableVoices;
    u32           reverbMode;
    u32           isDirty;
    SpuReverbAttr attr;
} SpuReverbConfig;
STATIC_ASSERT_SIZEOF(SpuReverbConfig, 0x24);

/// Number of hardware voices the SPU mixes; voice indices run below it.
#define SPU_VOICE_COUNT 24

/// Audio ticks after a key-on request during which a voice reading
/// `SPU_ON_ENV_OFF` is not yet taken to have finished.
#define SPU_KEY_ON_GRACE_TICKS 5

/// Allocation and key state of the SPU's hardware voices.
///
/// An owner takes a voice at a priority. A request that finds no free voice
/// takes over a held one of the lowest priority not above its own, the longest
/// held first. Every audio tick reads the key status back from the SPU and
/// returns each finished voice to the pool, telling its owner through the
/// registered `SpuVoiceCallback`.
///
/// The masks hold one bit per voice, as `SPU_VOICECH` builds them.
typedef struct {
    s32              reverbVoiceStatus;                 // Mask of voices with reverb on, as last read back from the SPU
    s32              ages[SPU_VOICE_COUNT];             // Ticks since the voice was taken or released, saturating
    char             keyStatus[SPU_VOICE_COUNT];        // Read back each tick (0 SPU_OFF, 1 SPU_ON, 2 SPU_OFF_ENV_ON, 3 SPU_ON_ENV_OFF)
    s8               keyOnGraceTicks[SPU_VOICE_COUNT];  // Ticks left in which SPU_ON_ENV_OFF does not release the voice
    s8               allocated[SPU_VOICE_COUNT];        // Held by an owner (0 free, 1 held)
    u32              priorities[SPU_VOICE_COUNT];       // Priority the voice is held at; 0 while free
    SpuVoiceCallback callbacks[SPU_VOICE_COUNT];        // Tells the owner it has lost the voice; NULL for none
    void*            callbackContexts[SPU_VOICE_COUNT]; // Passed to the callback, which is skipped while this is NULL
    s32              startedVoices;                     // Mask of voices keyed on with a sound and not yet reset onto the silent block
    u32              silentKeyOnVoices;                 // Mask of voices whose queued key-on plays the silent block
} _SpuVoiceState;
STATIC_ASSERT_SIZEOF(_SpuVoiceState, 0x1D4);

typedef struct _SpuLVoiceTable {
    /* 0x000 */ s16           count;         // active attr count
    /* 0x002 */ SpuLVoiceAttr attrs[24];
    /* 0x664 */ u8            field_664[24]; // per-voice flags
} SpuLVoiceTable;
STATIC_ASSERT_SIZEOF(SpuLVoiceTable, 0x67C);

/// 4-byte entry at Spu_VoiceRanges (see Spu_SetVoiceRange).
typedef struct _SpuVoiceRange {
    /* 0x0 */ s16 first;
    /* 0x2 */ s16 count;
} SpuVoiceRange;
STATIC_ASSERT_SIZEOF(SpuVoiceRange, 0x4);

/// Ring buffer of 4 AsyncCbEntry callback slots (AsyncCb_Queue, size 0x54).
/// field_0 = readIdx; field_1 = writeIdx.
typedef struct _AsyncCbQueue {
    /* 0x00 */ s8           field_0; // readIdx
    /* 0x01 */ s8           field_1; // writeIdx
    /* 0x02 */ u8           pad_2[2];
    /* 0x04 */ AsyncCbEntry entries[4];
} AsyncCbQueue;
STATIC_ASSERT_SIZEOF(AsyncCbQueue, 0x54);

/// The SPU ADPCM block uploaded to SPU address 0x7B440 at start-up.
static AsyncCbQueue AsyncCb_Queue;

static _SpuVoiceState Spu_VoiceState;

/// Unreferenced.
static u8 D_8007E510[8];

static SpuLVoiceTable Spu_LVoiceTable;

static SpuVoiceRange Spu_VoiceRanges[4];

static u32 Spu_KeyOnMask;

static u32 Spu_KeyOnMaskExtra;

static u32 Spu_KeyOffMask;

static SpuReverbConfig Spu_ReverbCfg;

static u8 Spu_InitialAdpcmBlock[];

static inline s32 Spu_ReleaseVoiceSlotInline(u32 voiceIdx);

static inline s32 Spu_GetVoiceRefInline(s8 voiceIdx, SpuVoiceRef* ref);

static void Spu_QueryReverbVoices(void);

static void Spu_SetReverbMode(u32 mode);

static bool Spu_ReverbVoiceIsEnabled(u32 voiceIdx);

static void Spu_ApplyReverbConfig(void);

static void Spu_KeyOnClearOff(u32 voiceIdx);

static u8 Spu_InitialAdpcmBlock[] = {
#include "assets/spu_voice_block.inc"
};

void AsyncCb_Poll(void)
{
    AsyncCbEntry* entry;
    u32           flags;
    s32           ret;
    s32           mask;
    s8            idx;
    s8            current;

    current = AsyncCb_Queue.field_0;
    if (AsyncCb_Queue.field_1 != current) {
        entry = &AsyncCb_Queue.entries[current];
        flags = entry->field_0.word;
        if (flags & 1) {
            if (entry->field_8(entry) != 0) {
                if (entry->field_C != NULL) {
                    entry->field_C(entry);
                }
                entry->field_0.word  &= ~1;
                entry->field_0.word  &= ~4;
                idx                   = (u8)AsyncCb_Queue.field_0 + 1;
                AsyncCb_Queue.field_0 = idx;
                if (idx >= 4) {
                    AsyncCb_Queue.field_0 = 0;
                }
            }
        } else if (!((flags >> 2) & 1) || ((flags >> 1) & 1) || (entry->field_10 == NULL) ||
                   (ret = entry->field_10(entry), mask = ~8,
                    entry->field_0.word = (entry->field_0.word & mask) | ((ret & 1) * 8), ((ret & 1) == 0))) {
            entry->field_0.word  &= ~4;
            idx                   = (u8)AsyncCb_Queue.field_0 + 1;
            AsyncCb_Queue.field_0 = idx;
            if (idx >= 4) {
                AsyncCb_Queue.field_0 = 0;
            }
        }
    }
}

void AsyncCb_Reset(void)
{
    u32  i;
    s32* ptr;

    ptr = (s32*)&AsyncCb_Queue;
    i   = 0;
    do {
        *ptr = 0;
        i++;
        ptr++;
    } while (i < 0x15U);
}

s16 AsyncCb_Enqueue(AsyncCbEntry* callbacks)
{
    AsyncCbEntry* entry;
    s32           next;
    s32           current;
    s8            writeIdx;

    writeIdx = AsyncCb_Queue.field_1;
    current  = AsyncCb_Queue.field_0;
    next     = writeIdx;
    next++;
    if (next >= 4) {
        next = 0;
    }
    if (next == current) {
        return 0;
    } else {
        entry                 = &AsyncCb_Queue.entries[writeIdx];
        entry->field_8        = callbacks->field_8;
        entry->field_C        = callbacks->field_C;
        entry->field_10       = callbacks->field_10;
        entry->field_0.word  |= 1;
        entry->field_0.word  &= ~4;
        entry->field_0.word  &= ~8;
        entry->field_0.word  &= ~0xFF0;
        entry->field_0.word  |= 2;
        current               = AsyncCb_Queue.field_1;
        AsyncCb_Queue.field_1 = next;
        return current + 1;
    }
}

void AsyncCb_Cancel(s32 arg0)
{
    AsyncCbEntry* entry;
    s32           flags;

    if ((arg0 << 0x10) != 0) {
        entry = &AsyncCb_Queue.entries[(s16)(arg0 - 1)];
        flags = entry->field_0.word;
        if (flags & 1) {
            entry->field_0.word = (flags & ~1) | 4;
        }
    }
}

void Spu_InitVoices(void)
{
    SpuVoiceRef sp10;
    s32*        ptr;
    s32         i;
    s8          sVoiceIdx;
    u32         spuAddr;

    spuAddr = 0x7B440;
    SpuSetTransferStartAddr(spuAddr);
    SpuWrite(Spu_InitialAdpcmBlock, 0x30U);
    SpuIsTransferCompleted(1);

    ptr                = (s32*)&Spu_LVoiceTable;
    i                  = 0;
    Spu_KeyOnMask      = 0;
    Spu_KeyOnMaskExtra = 0;
    Spu_KeyOffMask     = 0;
    do {
        *ptr = 0;
        i++;
        ptr++;
    } while ((u32)i < 0x19FU);

    ptr = (s32*)&Spu_VoiceState;
    i   = 0;
    do {
        *ptr = 0;
        i++;
        ptr++;
    } while ((u32)i < sizeof(Spu_VoiceState) / sizeof(*ptr));

    i = 0;
    do {
        sVoiceIdx = i;
        Spu_GetVoiceRef(sVoiceIdx, &sp10);

        {
            SpuVoiceAttr* attr = sp10.field_4;
            attr->loop_addr    = spuAddr;
            attr->addr         = spuAddr;
        }
        {
            SpuVoiceAttr* attr = sp10.field_4;
            attr->volume.right = 0;
            attr->volume.left  = 0;
        }
        {
            SpuVoiceAttr* attr  = sp10.field_4;
            attr->volmode.right = 0;
            attr->volmode.left  = 0;
        }
        {
            SpuVoiceAttr* attr = sp10.field_4;
            attr->adsr1        = 0x80FF;
        }
        {
            SpuVoiceAttr* attr = sp10.field_4;
            attr->adsr2        = 0xFFE0;
        }
        {
            SpuVoiceAttr* attr = sp10.field_4;
            attr->mask         = 0x7008FU;
        }
        {
            SpuVoiceAttr* attr = sp10.field_4;
            attr->voice        = 1 << i;
        }

        Spu_KeyOnClearOff(sVoiceIdx);
        i++;
    } while (i < 0x18);

    Spu_SetVoiceRange(2, 0x10, 2);
}

s32 Spu_AllocVoice(s16* arg0, s32 arg1, s32 arg2)
{
    SpuVoiceRange*   entry;
    s32              oldestAge;
    s32              bestPriority;
    s32              i;
    s32              j;
    s8               bestVoice;
    u8               voice;
    u8               keyStatus;
    u32              heldPriority;
    s32              age;
    SpuVoiceCallback callback;
    void*            context;
    _SpuVoiceState*  state;

    oldestAge    = 0;
    bestPriority = arg2;
    bestVoice    = -1;
    i            = 0;
    state        = &Spu_VoiceState;

    if (arg1 > 0) {
        do {
            entry = &Spu_VoiceRanges[*arg0];
            voice = *(u8*)entry;
            j     = 0;
            if (entry->count > 0) {
                do {
                    if (state->allocated[(s8)voice] == false) {
                        keyStatus = state->keyStatus[(s8)voice];
                        if ((keyStatus == SPU_OFF) || (keyStatus == SPU_ON_ENV_OFF)) {
                            state->priorities[(s8)voice] = arg2;
                            state->allocated[(s8)voice]  = true;
                            state->ages[(s8)voice]       = 0;
                            return (s8)voice;
                        }
                    } else {
                        heldPriority = state->priorities[(s8)voice];
                        if (heldPriority < (u32)bestPriority) {
                            bestPriority = heldPriority;
                            bestVoice    = voice;
                        } else if (bestPriority == (s32)heldPriority) {
                            age = state->ages[(s8)voice];
                            if (oldestAge < age) {
                                oldestAge = age;
                                bestVoice = voice;
                            }
                        }
                    }
                    j++;
                    voice++;
                } while (j < entry->count);
            }
            i++;
            arg0++;
        } while (i < arg1);
    }

    if (bestVoice >= 0) {
        callback = state->callbacks[bestVoice];
        if (callback != NULL) {
            context = state->callbackContexts[bestVoice];
            if (context != NULL) {
                callback(context);
            }
        }
        state->priorities[bestVoice] = arg2;
        state->ages[bestVoice]       = 0;
        state->keyStatus[bestVoice]  = SPU_ON;
    }
    return bestVoice;
}

static inline s32 Spu_ReleaseVoiceSlotInline(u32 voiceIdx)
{
    s8 sVoiceIdx = (s8)voiceIdx;
    if ((u8)sVoiceIdx > (u32)SPU_VOICE_COUNT) {
        return -1;
    }
    Spu_VoiceState.allocated[sVoiceIdx]  = false;
    Spu_VoiceState.priorities[sVoiceIdx] = 0;
    Spu_VoiceState.ages[sVoiceIdx]       = 0;
    return 0;
}

static inline s32 Spu_GetVoiceRefInline(s8 voiceIdx, SpuVoiceRef* ref)
{
    s32             slot;
    SpuLVoiceTable* table;
    SpuLVoiceAttr*  entry;
    table = &Spu_LVoiceTable;
    slot  = (s8)table->field_664[voiceIdx];
    if (slot != 0) {
        entry        = &table->attrs[slot];
        ref->field_0 = voiceIdx;
        ref->field_4 = &(entry - 1)->attr;
        return 1;
    } else {
        slot = table->count;
        table->count++;
        table->attrs[slot].voiceNum = voiceIdx;
        table->field_664[voiceIdx]  = slot + 1;
        ref->field_0                = voiceIdx;
        ref->field_4                = &table->attrs[slot].attr;
        ref->field_4->mask          = 0;
        ref->field_1                = 0;
        ref->field_3                = 0;
        ref->field_2                = 0;
        return 0;
    }
}

void Spu_TickVoices(void)
{
    SpuVoiceRef      ref;
    _SpuVoiceState*  state;
    s32              i;
    s32              age;
    s8               status;
    SpuVoiceCallback callback;
    void*            context;

    state = &Spu_VoiceState;
    SpuGetAllKeysStatus(state->keyStatus);
    for (i = 0; i < SPU_VOICE_COUNT; i++) {
        if (state->keyOnGraceTicks[i] != 0) {
            state->keyOnGraceTicks[i]--;
        }
        age = state->ages[i];
        if (age < 0x7FFFFFFF) {
            state->ages[i] = age + 1;
        }
        status = state->keyStatus[i];
        if (status != SPU_OFF) {
            if (status != SPU_ON_ENV_OFF || state->keyOnGraceTicks[i] != 0) {
                continue;
            }
            if ((state->startedVoices >> i) & 1) {
                Spu_KeyOff((s8)i);
            }
        }
        Spu_ReleaseVoiceSlotInline(i);
        callback = state->callbacks[i];
        if (callback != NULL) {
            context = state->callbackContexts[i];
            if (context != NULL) {
                callback(context);
                state->callbacks[i]        = NULL;
                state->callbackContexts[i] = NULL;
            }
        }
        if ((state->startedVoices >> i) & 1) {
            Spu_GetVoiceRefInline((s8)i, &ref);
            {
                SpuVoiceAttr* attr = ref.field_4;
                attr->loop_addr    = 0x7B440;
                attr->addr         = 0x7B440;
            }
            {
                SpuVoiceAttr* attr = ref.field_4;
                attr->volume.right = 0;
                attr->volume.left  = 0;
            }
            ref.field_4->adsr1 = 0x80FF;
            ref.field_4->adsr2 = 0xFFE0;
            ref.field_4->mask |= 0x70083;
            Spu_KeyOnClearOff((s8)i);
        }
    }
}

void Spu_FlushVoiceUpdates(void)
{
    i32             remaining;
    u8*             current;
    SpuLVoiceTable* dataPtr;

    if (Spu_ReverbCfg.isDirty) {
        Spu_ApplyReverbConfig();
        Spu_ReverbCfg.isDirty = false;
    }

    Spu_KeyOffMask |= Spu_KeyOnMask;
    if (Spu_KeyOffMask != 0) {
        SpuSetKey(SPU_OFF, Spu_KeyOffMask);
        Spu_KeyOffMask = 0;
    }

    // We take a pointer, as otherwise GCC will reload the address
    // when we reset the count to zero below.
    dataPtr = &Spu_LVoiceTable;
    if (dataPtr->count != 0) {
        SpuLSetVoiceAttr(dataPtr->count, dataPtr->attrs);

        // Clear the list. For some reason GCC does not like to cooperate with
        // the array indexing. Ideally we'd have the following:
        //
        // current = &dataPtr->field_664[remaining];
        // ...
        // *current-- = 0;
        //
        // This produces the following assembly:
        //
        // addu     v0, s0, 0x67B
        // sb       zero, 0(v0)
        //
        // Instead of what we actually want:
        //
        // addu     v0, s0, v1
        // sb       zero, 0x664(v0)
        //
        // Writing it this way forces GCC to perform the offsets in the correct
        // order.
        remaining = ARRAY_SIZE(dataPtr->field_664) - 1;
        current   = (u8*)dataPtr + remaining;
        do {
            current[OFFSET_OF(SpuLVoiceTable, field_664)] = 0;
            current                                      -= 1;
            remaining                                    -= 1;
        } while (remaining >= 0);
        dataPtr->count = 0;
    }

    if ((Spu_KeyOnMask | Spu_KeyOnMaskExtra) != 0) {
        SpuSetKey(SPU_ON, Spu_KeyOnMask | Spu_KeyOnMaskExtra);
        if (Spu_KeyOnMask != 0) {
            Spu_VoiceState.startedVoices |= Spu_KeyOnMask;
            Spu_VoiceState.startedVoices &= ~Spu_VoiceState.silentKeyOnVoices;
        }

        Spu_VoiceState.silentKeyOnVoices = 0;
        Spu_KeyOnMask                    = 0;
        Spu_KeyOnMaskExtra               = 0;
    }
}

void Spu_SetVoiceCallbacks(u32 voiceIdx, SpuVoiceCallback callback, void* context)
{
    s8 sVoiceIdx = (s8)voiceIdx;

    Spu_VoiceState.callbacks[sVoiceIdx]        = callback;
    Spu_VoiceState.callbackContexts[sVoiceIdx] = context;
}

void Spu_ClearVoiceCallbacks(u32 voiceIdx)
{
    s8 sVoiceIdx = (s8)voiceIdx;

    Spu_VoiceState.callbacks[sVoiceIdx]        = NULL;
    Spu_VoiceState.callbackContexts[sVoiceIdx] = NULL;
}

s32 Spu_SetVoiceRange(s32 idx, s32 arg1, s32 arg2)
{
    SpuVoiceRange* p;
    s16            sIdx;

    sIdx     = idx;
    p        = &Spu_VoiceRanges[sIdx];
    p->first = arg1;
    p->count = arg2;
    return 0;
}

s32 Spu_GetVoiceRef(s8 arg0, SpuVoiceRef* arg1)
{
    return Spu_GetVoiceRefInline(arg0, arg1);
}

s32 Spu_ReleaseVoiceSlot(u32 voiceIdx)
{
    s8 sVoiceIdx = (s8)voiceIdx;
    if (sVoiceIdx > (u32)ARRAY_SIZE(Spu_VoiceState.allocated)) {
        return -1;
    }

    Spu_VoiceState.allocated[sVoiceIdx]  = false;
    Spu_VoiceState.priorities[sVoiceIdx] = 0;
    Spu_VoiceState.ages[sVoiceIdx]       = 0;
    return 0;
}

u8 Spu_GetVoiceStatus(u32 voiceIdx)
{
    s8 sVoiceIdx = (s8)voiceIdx;

    return Spu_VoiceState.keyStatus[sVoiceIdx];
}

void Spu_KeyOn(u32 voiceIdx)
{
    _SpuVoiceState* state;
    u32*            pKeyOn;
    u32             channel;

    state                            = &Spu_VoiceState;
    pKeyOn                           = &Spu_KeyOnMask;
    voiceIdx                         = (s8)voiceIdx;
    state->keyOnGraceTicks[voiceIdx] = SPU_KEY_ON_GRACE_TICKS;
    channel                          = SPU_VOICECH(voiceIdx);
    *pKeyOn                         |= channel;
    channel                          = ~channel;
    state->silentKeyOnVoices        &= channel;
    Spu_KeyOffMask                  &= channel;
}

void Spu_KeyOff(u32 voiceIdx)
{
    u32* pKeyOff;
    u32  channel;

    pKeyOff  = &Spu_KeyOffMask;
    voiceIdx = (s8)voiceIdx;

    channel             = SPU_VOICECH(voiceIdx);
    *pKeyOff           |= channel;
    Spu_KeyOnMask      &= ~channel;
    Spu_KeyOnMaskExtra &= ~channel;
}

static void Spu_QueryReverbVoices(void)
{
    Spu_VoiceState.reverbVoiceStatus = SpuGetReverbVoice();
}

void Spu_ConfigReverb(s32 mode)
{
    SpuReserveReverbWorkArea(SPU_ON);
    SpuSetReverbVoice(SPU_OFF, SPU_ALLCH);
    SpuSetReverb(SPU_ON);

    Spu_ReverbCfg.attr.mask = SPU_REV_MODE;
    Spu_ReverbCfg.attr.mode = mode;
    SpuSetReverbModeParam(&Spu_ReverbCfg.attr);

    Spu_ReverbCfg.attr.mask        = SPU_REV_DEPTHR | SPU_REV_DEPTHL;
    Spu_ReverbCfg.attr.depth.right = 0;
    Spu_ReverbCfg.attr.depth.left  = 0;
    SpuSetReverbDepth(&Spu_ReverbCfg.attr);

    Spu_ReverbCfg.attr.mask = 0;
}

void Spu_SetReverbDepth(s16 depth)
{
    Spu_ReverbCfg.isDirty          = true;
    Spu_ReverbCfg.attr.depth.right = depth;
    Spu_ReverbCfg.attr.depth.left  = depth;
    Spu_ReverbCfg.attr.mask       |= SPU_REV_DEPTHR | SPU_REV_DEPTHL;
}

static void Spu_SetReverbMode(u32 mode)
{
    if (Spu_ReverbCfg.reverbMode != mode && Spu_ReverbCfg.reverbMode != SPU_REV_MODE_OFF) {
        SpuClearReverbWorkArea(Spu_ReverbCfg.reverbMode);
        Spu_ReverbCfg.isDirty    = true;
        Spu_ReverbCfg.attr.mask |= SPU_REV_MODE;
        Spu_ReverbCfg.attr.mode  = mode;
        Spu_ReverbCfg.reverbMode = mode;
    }
}

void Spu_EnableReverbVoice(u32 voiceIdx)
{
    u32 channel;
    voiceIdx = (s8)voiceIdx;

    Spu_ReverbCfg.isDirty        = true;
    channel                      = SPU_VOICECH(voiceIdx);
    Spu_ReverbCfg.enableVoices  |= channel;
    Spu_ReverbCfg.disableVoices &= ~channel;
}

void Spu_DisableReverbVoice(u32 voiceIdx)
{
    u32 channel;
    voiceIdx = (s8)voiceIdx;

    Spu_ReverbCfg.isDirty        = true;
    channel                      = SPU_VOICECH(voiceIdx);
    Spu_ReverbCfg.disableVoices |= channel;
    Spu_ReverbCfg.enableVoices  &= ~channel;
}

static bool Spu_ReverbVoiceIsEnabled(u32 voiceIdx)
{
    return (Spu_VoiceState.reverbVoiceStatus >> voiceIdx) & 1;
}

static void Spu_ApplyReverbConfig(void)
{
    if (Spu_ReverbCfg.disableVoices != 0) {
        SpuSetReverbVoice(SPU_OFF, Spu_ReverbCfg.disableVoices);
        Spu_ReverbCfg.disableVoices = 0;
    }

    if (Spu_ReverbCfg.enableVoices != 0) {
        SpuSetReverbVoice(SPU_ON, Spu_ReverbCfg.enableVoices);
        Spu_ReverbCfg.enableVoices = 0;
    }

    if ((Spu_ReverbCfg.attr.mask & SPU_REV_MODE) != 0) {
        SpuSetReverbModeParam(&Spu_ReverbCfg.attr);
    }
    if ((Spu_ReverbCfg.attr.mask & SPU_REV_DEPTHL) != 0) {
        SpuSetReverbDepth(&Spu_ReverbCfg.attr);
    }

    Spu_ReverbCfg.attr.mask = 0;
}

u16 Spu_CalcVolume(s32 arg0, s32 arg1, s32 arg2, s32 arg3)
{
    u32  temp;
    u32  hi;
    u32  lo;
    u32  offset;
    u16* base;

    temp  = arg1 + (arg0 << 8);
    temp  = temp - ((arg2 << 8) - (arg3 << 1));
    temp += 0x4800;

    lo     = (temp & 0xFF) >> 1;
    offset = 0;
    hi     = temp & 0xFFFF;
    do {
        base = Spu_SemitonePitchTable;
        hi >>= 8;
        if (hi != 0) {
            offset = hi << 1;
        }
    } while (0);
    /* The table is indexed by a byte offset: shifting the index inside the
     * branch is what the original does, and indexing `base` moves the shift. */
    lo = ((u32) * (u16*)((u8*)base + offset) * (u32)Spu_FinePitchTable[lo]) >> 8;
    if ((lo & 0xFFFF) >= 0x4000) {
        lo = 0x3FFF;
    }
    return lo;
}

SndBankLayer* Snd_GetNote(SndBank* bank, u8 group, u8 layer)
{
    if (bank != NULL) {
        return &bank->layers[bank->groupFirstLayer[group] + layer];
    }
    return NULL;
}

static void Spu_KeyOnClearOff(u32 voiceIdx)
{
    _SpuVoiceState* state;
    u32*            pKeyOn;
    u32             channel;

    state                            = &Spu_VoiceState;
    pKeyOn                           = &Spu_KeyOnMask;
    voiceIdx                         = (s8)voiceIdx;
    state->keyOnGraceTicks[voiceIdx] = SPU_KEY_ON_GRACE_TICKS;
    channel                          = SPU_VOICECH(voiceIdx);
    *pKeyOn                         |= channel;
    state->silentKeyOnVoices        |= channel;
    Spu_KeyOffMask                  &= ~channel;
}

void Spu_ArmKeyOn(u32 voiceIdx)
{
    _SpuVoiceState* state;
    u32*            pKeyOn;
    u32             channel;

    state                            = &Spu_VoiceState;
    pKeyOn                           = &Spu_KeyOnMaskExtra;
    voiceIdx                         = (s8)voiceIdx;
    state->keyOnGraceTicks[voiceIdx] = SPU_KEY_ON_GRACE_TICKS;
    channel                          = SPU_VOICECH(voiceIdx);
    *pKeyOn                         |= channel;
    state->silentKeyOnVoices        &= ~channel;
    Spu_KeyOnMask                   &= ~channel;
    Spu_KeyOffMask                  &= ~channel;
}
