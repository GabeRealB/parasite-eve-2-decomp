#include "sound.h"

#include <psyq/sys/types.h>
#include <psyq/libspu.h>
#include <psyq/limits.h>

#include "common.h"

#include "cdaudio.h"
#include "sound_types.h"

/// Number of hardware voices the SPU mixes; voice indices run below it.
#define SPU_VOICE_COUNT 24

/// Audio ticks after a key-on request during which a voice reading
/// `SPU_ON_ENV_OFF` is not yet taken to have finished.
#define SPU_KEY_ON_GRACE_TICKS 5

/// SPU byte address and envelope words used when parking a voice on silence.
enum {
    SPU_SILENT_BLOCK_ADDRESS = 0x7B440,
    SPU_SILENT_ADSR1         = 0x80FF,
    SPU_SILENT_ADSR2         = 0xFFE0
};

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

/// Voice attribute changes waiting to be sent to the SPU in one batch.
///
/// Sound code does not write a voice's attributes to the SPU itself: it asks
/// for the voice's entry here and edits that. The first request for a voice
/// since the last flush appends an entry with an empty attribute mask; later
/// ones find the same entry again, so a voice is listed at most once and the
/// list never outgrows the voice count. Once per audio tick the list is handed
/// to `SpuLSetVoiceAttr` and emptied, which ends the life of every entry
/// pointer given out.
typedef struct {
    s16           count;                        // Entries of `attrs` in use
    SpuLVoiceAttr attrs[SPU_VOICE_COUNT];       // Queued changes, in the order their voices were first asked for
    s8            slotByVoice[SPU_VOICE_COUNT]; // Per voice, its index in `attrs` plus one; 0 while it has no entry
} _SpuVoiceUpdateList;
STATIC_ASSERT_SIZEOF(_SpuVoiceUpdateList, 0x67C);

/// Reverb changes waiting to be sent to the SPU.
///
/// Sound code does not switch a voice's reverb or set the reverb parameters on
/// the SPU itself: each request is recorded here and marks the record dirty,
/// and the audio tick's flush sends whatever has accumulated in one go, before
/// that tick's key changes, and empties it again. Requests made between two
/// flushes therefore collapse into the last one of each kind.
///
/// The voice masks hold one bit per voice, as `SPU_VOICECH` builds them, and a
/// voice is in at most one of them: asking for either state withdraws a pending
/// request for the other.
typedef struct {
    u32           enableVoices;  // Mask of voices to switch reverb on for
    u32           disableVoices; // Mask of voices to switch reverb off for
    u32           activeMode;    // `SPU_REV_MODE_*` recorded by the last mode change; the start-up configuration does not record its mode here
    bool          isDirty;       // A request has been recorded since the last flush
    SpuReverbAttr attr;          // Parameters to send; `mask` selects the ones changed since the last flush (`SPU_REV_MODE`, `SPU_REV_DEPTHL | SPU_REV_DEPTHR`)
} _SpuReverbConfig;
STATIC_ASSERT_SIZEOF(_SpuReverbConfig, 0x24);

/// A run of consecutive hardware voices set aside for one kind of sound.
///
/// Each client of the voice pool registers the run it plays on under a range
/// index, and a request for a voice names the ranges it may be served from, in
/// the order to try them. Ranges may overlap: a voice is free or held whichever
/// range it is reached through.
typedef struct {
    s16 first; // Index of the run's first voice
    s16 count; // Voices in the run; 0 while the range is unregistered
} _SpuVoiceRange;
STATIC_ASSERT_SIZEOF(_SpuVoiceRange, 0x4);

/// The asynchronous callback queue: a ring of jobs polled one at a time.
///
/// Jobs are queued at `writeIdx` and polled in order from `readIdx`. The ring
/// is empty when the two are equal, so one slot always stays free and a full
/// ring refuses the job. A queued job is known to its owner by its slot's
/// index plus one, zero meaning none; cancelling one marks its slot and leaves
/// the indices alone, so the slot is given up only when the poll reaches it.
typedef struct {
    s8           readIdx;    // slot of the job being polled
    s8           writeIdx;   // slot the next job is queued in
    AsyncCbEntry entries[4]; // the ring
} _AsyncCbQueue;
STATIC_ASSERT_SIZEOF(_AsyncCbQueue, 0x54);

/// The SPU ADPCM block uploaded to SPU address 0x7B440 at start-up.
static _AsyncCbQueue AsyncCb_Queue;

static _SpuVoiceState Spu_VoiceState;

/// Unreferenced.
static u8 D_8007E510[8];

static _SpuVoiceUpdateList Spu_LVoiceTable;

static _SpuVoiceRange Spu_VoiceRanges[4];

static u32 Spu_KeyOnMask;

static u32 Spu_KeyOnMaskExtra;

static u32 Spu_KeyOffMask;

static _SpuReverbConfig Spu_ReverbCfg;

static u8 Spu_InitialAdpcmBlock[];

static inline s32 _spuGetVoiceRef(s8 voiceIdx, SpuVoiceRef* ref);

static void _spuApplyReverbUpdates(void);

static void _spuKeyOnSilentBlock(u32 voiceIdx);

static u8 Spu_InitialAdpcmBlock[] = {
#include "assets/spu_voice_block.inc"
};

/// Selects the same SPU byte address for a queued voice's start and loop.
///
/// ref is a live SpuVoiceRef value, evaluated once; address is evaluated twice
/// and must have no side effects. The caller selects the attribute mask.
#define SPU_SET_VOICE_SAMPLE_ADDRESS(ref, address) \
    {                                              \
        SpuVoiceAttr* attr = (ref).attr;           \
        attr->loop_addr    = (address);            \
        attr->addr         = (address);            \
    }

/// Sets a queued voice's signed left and right volume register values.
///
/// ref is a live SpuVoiceRef value. Each argument is evaluated once, right
/// before left; values are narrowed to s16. The caller selects the mask.
#define SPU_SET_VOICE_VOLUMES(ref, leftVolume, rightVolume) \
    {                                                       \
        SpuVoiceAttr* attr = (ref).attr;                    \
        attr->volume.right = (rightVolume);                 \
        attr->volume.left  = (leftVolume);                  \
    }

/// Sets a queued voice's left and right volume sweep modes.
///
/// ref is a live SpuVoiceRef value. Each argument is evaluated once, right
/// before left; modes are SPU_VOICE_* values. The caller selects the mask.
#define SPU_SET_VOICE_VOLUME_MODES(ref, leftMode, rightMode) \
    {                                                        \
        SpuVoiceAttr* attr  = (ref).attr;                    \
        attr->volmode.right = (rightMode);                   \
        attr->volmode.left  = (leftMode);                    \
    }

/// Sets both packed ADSR register words in a queued voice's attributes.
///
/// ref is a live SpuVoiceRef value, evaluated twice, and must have no side
/// effects. Each word is evaluated once and narrowed to u16, adsr1 before
/// adsr2. The caller selects the mask. Use as a statement inside braces.
#define SPU_SET_VOICE_ADSR(ref, adsrWord1, adsrWord2) \
    {                                                 \
        SpuVoiceAttr* attr = (ref).attr;              \
        attr->adsr1        = (adsrWord1);             \
    }                                                 \
    {                                                 \
        SpuVoiceAttr* attr = (ref).attr;              \
        attr->adsr2        = (adsrWord2);             \
    }

void asyncCbPoll(void)
{
    AsyncCbEntry* entry;
    s8            readIdx;

    readIdx = AsyncCb_Queue.readIdx;
    if (AsyncCb_Queue.writeIdx != readIdx) {
        entry = &AsyncCb_Queue.entries[readIdx];
        if (entry->status.active) {
            if (entry->pollFn(entry) != 0) {
                if (entry->doneFn != NULL) {
                    entry->doneFn(entry);
                }
                entry->status.active    = 0;
                entry->status.cancelled = 0;
                if (++AsyncCb_Queue.readIdx >= ARRAY_SIZE(AsyncCb_Queue.entries)) {
                    AsyncCb_Queue.readIdx = 0;
                }
            }
        } else {
            // Only a job that started needs cancellation; an odd result keeps it queued.
            if (entry->status.cancelled) {
                if (!entry->status.firstPoll) {
                    if (entry->cancelFn != NULL) {
                        if ((entry->status.cancelPending = entry->cancelFn(entry))) {
                            return;
                        }
                    }
                }
            }
            entry->status.cancelled = 0;
            if (++AsyncCb_Queue.readIdx >= ARRAY_SIZE(AsyncCb_Queue.entries)) {
                AsyncCb_Queue.readIdx = 0;
            }
        }
    }
}

void asyncCbReset(void)
{
    u32  wordsCleared;
    s32* clearWord;

    clearWord    = (s32*)&AsyncCb_Queue;
    wordsCleared = 0;
    do {
        *clearWord = 0;
        wordsCleared++;
        clearWord++;
    } while (wordsCleared < sizeof(AsyncCb_Queue) / sizeof(*clearWord));
}

s16 asyncCbEnqueue(const AsyncCbEntry* callbacks)
{
    AsyncCbEntry* entry;
    s32           nextWriteIdx;
    s32           queueIndex;
    s8            writeIdx;

    writeIdx     = AsyncCb_Queue.writeIdx;
    queueIndex   = AsyncCb_Queue.readIdx;
    nextWriteIdx = writeIdx;
    nextWriteIdx++;
    if (nextWriteIdx >= ARRAY_SIZE(AsyncCb_Queue.entries)) {
        nextWriteIdx = 0;
    }
    if (nextWriteIdx == queueIndex) {
        return 0;
    } else {
        entry           = &AsyncCb_Queue.entries[writeIdx];
        entry->pollFn   = callbacks->pollFn;
        entry->doneFn   = callbacks->doneFn;
        entry->cancelFn = callbacks->cancelFn;
        // The status bits are the queue's own: the caller supplies only the callbacks.
        entry->status.active        = 1;
        entry->status.cancelled     = 0;
        entry->status.cancelPending = 0;
        entry->status.pollState     = 0;
        entry->status.firstPoll     = 1;
        queueIndex                  = AsyncCb_Queue.writeIdx;
        AsyncCb_Queue.writeIdx      = nextWriteIdx;
        return queueIndex + 1;
    }
}

void asyncCbCancel(s16 handle)
{
    AsyncCbEntry* entry;

    if ((handle << 0x10) != 0) {
        entry = &AsyncCb_Queue.entries[(s16)(handle - 1)];
        if (entry->status.active) {
            entry->status.active    = 0;
            entry->status.cancelled = 1;
        }
    }
}

void spuInitVoices(void)
{
    SpuVoiceRef voiceRef;
    s32*        clearWord;
    s32         index;
    s8          voiceIndex;
    u32         silentBlockAddr;

    // Install the silent loop before any voice can select it.
    silentBlockAddr = SPU_SILENT_BLOCK_ADDRESS;
    SpuSetTransferStartAddr(silentBlockAddr);
    SpuWrite(Spu_InitialAdpcmBlock, sizeof(Spu_InitialAdpcmBlock));
    SpuIsTransferCompleted(SPU_TRANSFER_WAIT);

    clearWord          = (s32*)&Spu_LVoiceTable;
    index              = 0;
    Spu_KeyOnMask      = 0;
    Spu_KeyOnMaskExtra = 0;
    Spu_KeyOffMask     = 0;
    do {
        *clearWord = 0;
        index++;
        clearWord++;
    } while ((u32)index < sizeof(Spu_LVoiceTable) / sizeof(*clearWord));

    clearWord = (s32*)&Spu_VoiceState;
    index     = 0;
    do {
        *clearWord = 0;
        index++;
        clearWord++;
    } while ((u32)index < sizeof(Spu_VoiceState) / sizeof(*clearWord));

    // Queue every voice on silence; the next flush applies this complete batch.
    index = 0;
    do {
        voiceIndex = index;
        spuGetVoiceRef(voiceIndex, &voiceRef);

        SPU_SET_VOICE_SAMPLE_ADDRESS(voiceRef, silentBlockAddr);
        SPU_SET_VOICE_VOLUMES(voiceRef, 0, 0);
        SPU_SET_VOICE_VOLUME_MODES(voiceRef, SPU_VOICE_DIRECT, SPU_VOICE_DIRECT);
        SPU_SET_VOICE_ADSR(voiceRef, SPU_SILENT_ADSR1, SPU_SILENT_ADSR2);
        voiceRef.attr->mask  = (u32)(SPU_VOICE_VOLL | SPU_VOICE_VOLR | SPU_VOICE_VOLMODEL | SPU_VOICE_VOLMODER |
                                    SPU_VOICE_WDSA | SPU_VOICE_LSAX | SPU_VOICE_ADSR_ADSR1 | SPU_VOICE_ADSR_ADSR2);
        voiceRef.attr->voice = SPU_VOICECH(index);

        _spuKeyOnSilentBlock(voiceIndex);
        index++;
    } while (index < SPU_VOICE_COUNT);

    spuSetVoiceRange(SPU_VOICE_RANGE_SHARED, 16, 2);
}

/// Notifies a voice's registered owner when both handler and borrowed context are present.
///
/// state must remain live and voiceIdx must be 0..23. The registration is
/// retained unless clearAfterNotification is true, which requires writable state.
/// A disabled notification leaves the registration intact in either case.
static inline void _spuNotifyVoiceOwner(_SpuVoiceState* state, s32 voiceIdx, bool clearAfterNotification)
{
    SpuVoiceCallback callback;
    void*            context;

    callback = state->callbacks[voiceIdx];
    if (callback != NULL) {
        context = state->callbackContexts[voiceIdx];
        if (context != NULL) {
            callback(context);
            if (clearAfterNotification) {
                state->callbacks[voiceIdx]        = NULL;
                state->callbackContexts[voiceIdx] = NULL;
            }
        }
    }
}

s32 spuAllocVoice(const s16* rangeIndices, s32 rangeCount, s32 priority)
{
    enum { SPU_VOICE_NONE = -1 };
    const _SpuVoiceRange* range;
    s32                   oldestComparedAge;
    s32                   candidatePriority;
    s32                   rangeIndex;
    s32                   rangeVoiceIndex;
    s8                    candidateVoiceIdx;
    s8                    voiceIdx;
    u8                    cachedKeyStatus;
    u32                   heldPriority;
    s32                   voiceAge;
    _SpuVoiceState*       state;

    oldestComparedAge = 0;
    candidatePriority = priority;
    candidateVoiceIdx = SPU_VOICE_NONE;
    rangeIndex        = 0;
    state             = &Spu_VoiceState;

    // Prefer the first free, inactive voice in the caller's range order.
    if (rangeCount > 0) {
        do {
            range           = &Spu_VoiceRanges[*rangeIndices];
            voiceIdx        = range->first;
            rangeVoiceIndex = 0;
            if (range->count > 0) {
                do {
                    if (state->allocated[voiceIdx] == false) {
                        cachedKeyStatus = state->keyStatus[voiceIdx];
                        if ((cachedKeyStatus == SPU_OFF) || (cachedKeyStatus == SPU_ON_ENV_OFF)) {
                            state->priorities[voiceIdx] = priority;
                            state->allocated[voiceIdx]  = true;
                            state->ages[voiceIdx]       = 0;
                            return voiceIdx;
                        }
                    } else {
                        heldPriority = state->priorities[voiceIdx];
                        if (heldPriority < (u32)candidatePriority) {
                            candidatePriority = heldPriority;
                            candidateVoiceIdx = voiceIdx;
                        } else if (candidatePriority == (s32)heldPriority) {
                            // The age maximum is retained even when a lower priority replaces the candidate.
                            voiceAge = state->ages[voiceIdx];
                            if (oldestComparedAge < voiceAge) {
                                oldestComparedAge = voiceAge;
                                candidateVoiceIdx = voiceIdx;
                            }
                        }
                    }
                    rangeVoiceIndex++;
                    voiceIdx++;
                } while (rangeVoiceIndex < range->count);
            }
            rangeIndex++;
            rangeIndices++;
        } while (rangeIndex < rangeCount);
    }

    // Ownership changes here; the new owner replaces the retained notification.
    if (candidateVoiceIdx >= 0) {
        _spuNotifyVoiceOwner(state, candidateVoiceIdx, false);
        state->priorities[candidateVoiceIdx] = priority;
        state->ages[candidateVoiceIdx]       = 0;
        state->keyStatus[candidateVoiceIdx]  = SPU_ON;
    }
    return candidateVoiceIdx;
}

/// Releases allocation, priority and age without touching key state or callbacks.
///
/// Requires voiceIdx in 0..23. Returns 0 on release or -1 on rejection;
/// the retained unsigned-byte guard also admits 24, beyond the voice arrays.
static inline s32 _spuReleaseVoiceSlot(u32 voiceIdx)
{
    s8 voiceIndex = (s8)voiceIdx;
    if ((u8)voiceIndex > (u32)SPU_VOICE_COUNT) {
        return -1;
    }
    Spu_VoiceState.allocated[voiceIndex]  = false;
    Spu_VoiceState.priorities[voiceIndex] = 0;
    Spu_VoiceState.ages[voiceIndex]       = 0;
    return 0;
}

/// Implements the queued-attribute lookup used by `spuGetVoiceRef` and the voice tick.
///
/// Requires voiceIdx in 0..23 and writable ref. Returns 1 for an existing entry
/// or 0 for a new entry; the attribute pointer expires at the next flush.
static inline s32 _spuGetVoiceRef(s8 voiceIdx, SpuVoiceRef* ref)
{
    s32                  slot;
    _SpuVoiceUpdateList* updates;
    SpuLVoiceAttr*       queuedEntry;
    updates = &Spu_LVoiceTable;
    slot    = updates->slotByVoice[voiceIdx];
    if (slot != 0) {
        // Already queued: the stored slot is one past the voice's entry.
        queuedEntry   = &updates->attrs[slot - 1];
        ref->voiceIdx = voiceIdx;
        ref->attr     = &queuedEntry->attr;
        return 1;
    } else {
        // Append an entry for the voice, with nothing selected for update yet.
        slot = updates->count;
        updates->count++;
        updates->attrs[slot].voiceNum  = voiceIdx;
        updates->slotByVoice[voiceIdx] = slot + 1;
        ref->voiceIdx                  = voiceIdx;
        ref->attr                      = &updates->attrs[slot].attr;
        ref->attr->mask                = 0;
        ref->field_1                   = 0;
        ref->field_3                   = 0;
        ref->field_2                   = 0;
        return 0;
    }
}

void spuTickVoices(void)
{
    SpuVoiceRef     voiceRef;
    _SpuVoiceState* state;
    s32             voiceIdx;
    s32             voiceAge;
    s8              keyStatus;

    state = &Spu_VoiceState;
    SpuGetAllKeysStatus(state->keyStatus);
    for (voiceIdx = 0; voiceIdx < SPU_VOICE_COUNT; voiceIdx++) {
        if (state->keyOnGraceTicks[voiceIdx] != 0) {
            state->keyOnGraceTicks[voiceIdx]--;
        }
        voiceAge = state->ages[voiceIdx];
        if (voiceAge < INT_MAX) {
            state->ages[voiceIdx] = voiceAge + 1;
        }
        keyStatus = state->keyStatus[voiceIdx];
        if (keyStatus != SPU_OFF) {
            if (keyStatus != SPU_ON_ENV_OFF || state->keyOnGraceTicks[voiceIdx] != 0) {
                continue;
            }
            if ((state->startedVoices >> voiceIdx) & 1) {
                spuKeyOff((s8)voiceIdx);
            }
        }
        // Release completed voices and clear only registrations that were notified.
        _spuReleaseVoiceSlot(voiceIdx);
        _spuNotifyVoiceOwner(state, voiceIdx, true);
        if ((state->startedVoices >> voiceIdx) & 1) {
            // Park voices marked by an ordinary sound start on silence.
            _spuGetVoiceRef((s8)voiceIdx, &voiceRef);
            SPU_SET_VOICE_SAMPLE_ADDRESS(voiceRef, SPU_SILENT_BLOCK_ADDRESS);
            SPU_SET_VOICE_VOLUMES(voiceRef, 0, 0);
            SPU_SET_VOICE_ADSR(voiceRef, SPU_SILENT_ADSR1, SPU_SILENT_ADSR2);
            voiceRef.attr->mask |= SPU_VOICE_VOLL | SPU_VOICE_VOLR | SPU_VOICE_WDSA |
                                   SPU_VOICE_LSAX | SPU_VOICE_ADSR_ADSR1 | SPU_VOICE_ADSR_ADSR2;
            _spuKeyOnSilentBlock((s8)voiceIdx);
        }
    }
}

void spuFlushVoiceUpdates(void)
{
    s32                  voiceIdx;
    _SpuVoiceUpdateList* list;

    if (Spu_ReverbCfg.isDirty) {
        _spuApplyReverbUpdates();
        Spu_ReverbCfg.isDirty = false;
    }

    // Stop pending sound key-ons first, then apply attributes before restarting them.
    Spu_KeyOffMask |= Spu_KeyOnMask;
    if (Spu_KeyOffMask != 0) {
        SpuSetKey(SPU_OFF, Spu_KeyOffMask);
        Spu_KeyOffMask = 0;
    }

    list = &Spu_LVoiceTable;
    if (list->count != 0) {
        SpuLSetVoiceAttr(list->count, list->attrs);

        // Empty the list: no voice has an entry any more.
        for (voiceIdx = 0; voiceIdx < SPU_VOICE_COUNT; voiceIdx++) {
            list->slotByVoice[voiceIdx] = 0;
        }
        list->count = 0;
    }

    // Only ordinary sound starts enter completion-reset tracking.
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

void spuSetVoiceCallback(u32 voiceIdx, SpuVoiceCallback callback, void* context)
{
    s8 voiceIndex = (s8)voiceIdx;

    Spu_VoiceState.callbacks[voiceIndex]        = callback;
    Spu_VoiceState.callbackContexts[voiceIndex] = context;
}

void spuClearVoiceCallback(u32 voiceIdx)
{
    s8 voiceIndex = (s8)voiceIdx;

    Spu_VoiceState.callbacks[voiceIndex]        = NULL;
    Spu_VoiceState.callbackContexts[voiceIndex] = NULL;
}

s32 spuSetVoiceRange(s32 rangeIndex, s32 firstVoice, s32 voiceCount)
{
    _SpuVoiceRange* range;
    s16             rangeSlot;

    rangeSlot    = rangeIndex;
    range        = &Spu_VoiceRanges[rangeSlot];
    range->first = firstVoice;
    range->count = voiceCount;
    return 0;
}

s32 spuGetVoiceRef(s8 voiceIdx, SpuVoiceRef* ref)
{
    return _spuGetVoiceRef(voiceIdx, ref);
}

s32 spuReleaseVoiceSlot(u32 voiceIdx)
{
    s8 voiceIndex = (s8)voiceIdx;
    if (voiceIndex > (u32)ARRAY_SIZE(Spu_VoiceState.allocated)) {
        return -1;
    }

    Spu_VoiceState.allocated[voiceIndex]  = false;
    Spu_VoiceState.priorities[voiceIndex] = 0;
    Spu_VoiceState.ages[voiceIndex]       = 0;
    return 0;
}

u8 spuGetVoiceKeyStatus(u32 voiceIdx)
{
    s8 voiceIndex = (s8)voiceIdx;

    return Spu_VoiceState.keyStatus[voiceIndex];
}

void spuKeyOn(u32 voiceIdx)
{
    _SpuVoiceState* state;
    u32*            keyOnMask;
    u32             voiceMask;

    state                            = &Spu_VoiceState;
    keyOnMask                        = &Spu_KeyOnMask;
    voiceIdx                         = (s8)voiceIdx;
    state->keyOnGraceTicks[voiceIdx] = SPU_KEY_ON_GRACE_TICKS;
    voiceMask                        = SPU_VOICECH(voiceIdx);
    *keyOnMask                      |= voiceMask;
    voiceMask                        = ~voiceMask;
    state->silentKeyOnVoices        &= voiceMask;
    Spu_KeyOffMask                  &= voiceMask;
}

void spuKeyOff(u32 voiceIdx)
{
    u32* keyOffMask;
    u32  voiceMask;

    keyOffMask = &Spu_KeyOffMask;
    voiceIdx   = (s8)voiceIdx;

    voiceMask           = SPU_VOICECH(voiceIdx);
    *keyOffMask        |= voiceMask;
    Spu_KeyOnMask      &= ~voiceMask;
    Spu_KeyOnMaskExtra &= ~voiceMask;
}

/// Refreshes the cached hardware reverb-routing mask; retained without callers.
static void _spuRefreshReverbVoiceStatus(void)
{
    Spu_VoiceState.reverbVoiceStatus = SpuGetReverbVoice();
}

void spuInitReverb(s32 mode)
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

void spuSetReverbDepth(s16 depth)
{
    Spu_ReverbCfg.isDirty          = true;
    Spu_ReverbCfg.attr.depth.right = depth;
    Spu_ReverbCfg.attr.depth.left  = depth;
    Spu_ReverbCfg.attr.mask       |= SPU_REV_DEPTHR | SPU_REV_DEPTHL;
}

/// Queues a mode change only away from a non-OFF mode recorded by this helper.
///
/// mode is an SPU_REV_MODE_* value. Clears the previously recorded mode's
/// work area immediately and defers the parameter update until the next flush.
/// Retained without callers: startup does not seed activeMode, so its zero
/// initial value prevents this helper from making a first change.
static void _spuSetReverbMode(u32 mode)
{
    if (Spu_ReverbCfg.activeMode != mode && Spu_ReverbCfg.activeMode != SPU_REV_MODE_OFF) {
        SpuClearReverbWorkArea(Spu_ReverbCfg.activeMode);
        Spu_ReverbCfg.isDirty    = true;
        Spu_ReverbCfg.attr.mask |= SPU_REV_MODE;
        Spu_ReverbCfg.attr.mode  = mode;
        Spu_ReverbCfg.activeMode = mode;
    }
}

void spuEnableVoiceReverb(u32 voiceIdx)
{
    u32 voiceMask;
    voiceIdx = (s8)voiceIdx;

    Spu_ReverbCfg.isDirty        = true;
    voiceMask                    = SPU_VOICECH(voiceIdx);
    Spu_ReverbCfg.enableVoices  |= voiceMask;
    Spu_ReverbCfg.disableVoices &= ~voiceMask;
}

void spuDisableVoiceReverb(u32 voiceIdx)
{
    u32 voiceMask;
    voiceIdx = (s8)voiceIdx;

    Spu_ReverbCfg.isDirty        = true;
    voiceMask                    = SPU_VOICECH(voiceIdx);
    Spu_ReverbCfg.disableVoices |= voiceMask;
    Spu_ReverbCfg.enableVoices  &= ~voiceMask;
}

/// Tests a voice's cached reverb routing; retained without callers.
///
/// Requires voiceIdx in 0..23 and a previously refreshed status mask.
/// Pending routing changes do not update this snapshot.
static bool _spuIsVoiceReverbEnabled(u32 voiceIdx)
{
    return (Spu_VoiceState.reverbVoiceStatus >> voiceIdx) & 1;
}

/// Applies pending reverb routing and parameters, then empties their masks.
///
/// The flush owns isDirty; depth requests always select both stereo channels.
static void _spuApplyReverbUpdates(void)
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

u16 spuCalcPitch(s32 key, s32 pitchOffset, s32 rootKey, s32 fineTune)
{
    enum {
        SPU_PITCH_FRACTION_BITS   = 8,
        SPU_PITCH_TABLE_UNITY_KEY = 72,
        SPU_PITCH_PRODUCT_SHIFT   = 8, // Q10 table factors to Q12 SPU pitch
        SPU_PITCH_REGISTER_MAX    = 0x3FFF
    };
    u32        pitchCoordinate;
    u32        semitoneIndex;
    u32        fineIndexAndPitch;
    u32        semitoneByteOffset;
    const u16* semitonePitches;

    // Translate the note and layer tuning into the table's Q8 coordinate.
    pitchCoordinate  = pitchOffset + (key << SPU_PITCH_FRACTION_BITS);
    pitchCoordinate  = pitchCoordinate - ((rootKey << SPU_PITCH_FRACTION_BITS) - (fineTune << 1));
    pitchCoordinate += SPU_PITCH_TABLE_UNITY_KEY << SPU_PITCH_FRACTION_BITS;

    // The fine table uses 1/128-semitone steps; the low coordinate bit is discarded.
    fineIndexAndPitch  = (pitchCoordinate & 0xFF) >> 1;
    semitoneByteOffset = 0;
    semitoneIndex      = pitchCoordinate & 0xFFFF;
    do {
        semitonePitches = Spu_SemitonePitchTable;
        semitoneIndex >>= SPU_PITCH_FRACTION_BITS;
        if (semitoneIndex != 0) {
            semitoneByteOffset = semitoneIndex << 1;
        }
    } while (0);
    // The compiled lookup scales the semitone index in bytes before loading a u16.
    fineIndexAndPitch = ((u32) * (const u16*)((const u8*)semitonePitches + semitoneByteOffset) * (u32)Spu_FinePitchTable[fineIndexAndPitch]) >> SPU_PITCH_PRODUCT_SHIFT;
    if ((fineIndexAndPitch & 0xFFFF) >= SPU_PITCH_REGISTER_MAX + 1) {
        fineIndexAndPitch = SPU_PITCH_REGISTER_MAX;
    }
    return fineIndexAndPitch;
}

SndBankLayer* sndBankGetLayer(SndBank* bank, u8 group, u8 layer)
{
    if (bank != NULL) {
        return &bank->layers[bank->groupFirstLayer[group] + layer];
    }
    return NULL;
}

/// Queues key-on for a voice whose attributes already select the silent ADPCM block.
///
/// Requires voiceIdx in 0..23. Starts the five-tick grace period and cancels
/// pending key-off. The silent mark excludes this key-on from startedVoices,
/// preventing the voice tick from resetting an already silent voice again.
static void _spuKeyOnSilentBlock(u32 voiceIdx)
{
    _SpuVoiceState* state;
    u32*            keyOnMask;
    u32             voiceMask;

    state                            = &Spu_VoiceState;
    keyOnMask                        = &Spu_KeyOnMask;
    voiceIdx                         = (s8)voiceIdx;
    state->keyOnGraceTicks[voiceIdx] = SPU_KEY_ON_GRACE_TICKS;
    voiceMask                        = SPU_VOICECH(voiceIdx);
    *keyOnMask                      |= voiceMask;
    state->silentKeyOnVoices        |= voiceMask;
    Spu_KeyOffMask                  &= ~voiceMask;
}

void spuKeyOnStreamVoice(u32 voiceIdx)
{
    _SpuVoiceState* state;
    u32*            keyOnMask;
    u32             voiceMask;

    state                            = &Spu_VoiceState;
    keyOnMask                        = &Spu_KeyOnMaskExtra;
    voiceIdx                         = (s8)voiceIdx;
    state->keyOnGraceTicks[voiceIdx] = SPU_KEY_ON_GRACE_TICKS;
    voiceMask                        = SPU_VOICECH(voiceIdx);
    *keyOnMask                      |= voiceMask;
    state->silentKeyOnVoices        &= ~voiceMask;
    Spu_KeyOnMask                   &= ~voiceMask;
    Spu_KeyOffMask                  &= ~voiceMask;
}
