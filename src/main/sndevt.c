#include <psyq/sys/types.h>
#include <psyq/libspu.h>
#include <psyq/libetc.h>

#include "common.h"

#include "cdaudio.h"
#include "main/display.h"
#include "main/display_types.h"
#include "fs.h"
#include "main/sound_types.h"
#include "sound_types.h"

// Layer bend ranges produce Q8 semitone offsets; both signs use the positive
// endpoint of the signed MIDI wheel range (-8192..8191) as the denominator.
enum {
    MIDI_PITCH_FRACTION_BITS = 8,
    MIDI_PITCH_BEND_MAX      = 8191
};

// Sequence selectors and gain limits carried by deferred MIDI events.
enum {
    SOUND_EVENT_MIDI_ALL_SEQUENCES    = 0,
    SOUND_EVENT_MIDI_INVALID_SEQUENCE = 255,
    SOUND_EVENT_MIDI_VOLUME_SILENT    = 0,
    SOUND_EVENT_MIDI_VOLUME_FULL      = 127
};

// Reservation states stored in each deferred command slot.
enum {
    /// Marks a deferred sound-event pool slot as available for reservation.
    ///
    /// Stored in `SndEvt::allocated`; zeroing the pool makes every slot available.
    /// Releasing a processed event restores this state without clearing its
    /// command or arguments. Availability does not depend on the command value.
    SOUND_EVENT_SLOT_FREE = 0,
    /// Occupied reservation stored in `SndEvt::allocated`.
    ///
    /// Written when a free slot is taken. It remains through the unqueued
    /// hold and the later queue until release or a whole-pool clear. A slot
    /// counts as free only when that halfword holds the free value, so this
    /// is the only occupied value written.
    SOUND_EVENT_SLOT_ALLOCATED = 1
};

// Channel selection and the neutral offset shared by pan reset and mixing.
enum {
    MIDI_CHANNEL_STATUS_MASK = 0xF,
    MIDI_CHANNEL_PAN_CENTER  = 64
};

/// Runtime controls shared by the notes playing on one MIDI channel.
///
/// A song owns sixteen entries, indexed by the status byte's low nibble (0..15).
/// Slot initialization and sequence startup reset them. Controller and program
/// bytes are retained without validation: MIDI gains and pan must be 0..127,
/// and the program must be below the loaded bank's `groupCount`.
/// Volume, expression and pan affect playing voices; program selects new notes.
/// The signed wheel value is scaled by each sample layer's semitone bend range
/// for both playing voices and new notes.
typedef struct {
    u8  noteEventsDisabled; // Note-event gate (0 enabled, nonzero suppresses on/off); reset 0
    u8  volume;             // CC 7 gain (0 silent, 127 full); reset 64
    u8  expression;         // CC 11 gain multiplying volume (0 silent, 127 full); reset 127
    u8  pan;                // CC 10 offset from layer/group pan (64 unchanged); reset 64
    u8  program;            // Bank program/group index for new notes; reset 0
    u8  unknown_5;          // Zeroed on reset; no individual access, role unproven
    s16 pitchBend;          // Signed wheel offset (-8192..8191, reset 0), before layer scaling
} _MidiChannel;
STATIC_ASSERT_SIZEOF(_MidiChannel, 0x8);

/// The initializer writes the complete table as words; event handlers use
/// individual channel controls. Both views cover exactly the same 0x80 bytes.
typedef union {
    _MidiChannel entries[16];
    u32          words[32];
} MidiChannelTable;
STATIC_ASSERT_SIZEOF(MidiChannelTable, 0x80);

/// Track/channel entry inside MidiSong (stride 0x3C). field_5 is a per-entry flag
/// written by Midi_ResetTrackFlags; absolute offset of first entry's field_5 is 0x51.
/// field_0 / field_1 / field_4 are NRPN/RPN state used by the MIDI CC handler
/// (Midi_Event3). field_6 / field_7 and field_8[] form a loop stack for the
/// 0xF5/0xF6 meta opcodes (Midi_HandleMetaSysex); field_8[8] is also the track data
/// pointer resolved by Midi_ResolveTrackData (absolute offset 0x74). field_2C is the
/// current track cursor advanced by the MIDI event driver (Midi_DriveTrack).
/// field_30 is a saved event cursor for looped CC 0x63. field_34 is the
/// remaining delta-time for the next event; field_38 is a fractional tick
/// accumulator (mod 6000/3600 per gDisplayState.region).
typedef struct _MidiTrack {
    /* 0x00 */ u8  field_0;
    /* 0x01 */ u8  field_1;
    /* 0x02 */ u8  field_2;
    /* 0x03 */ u8  field_3;
    /* 0x04 */ u8  field_4;
    /* 0x05 */ u8  field_5;
    /* 0x06 */ u8  field_6;
    /* 0x07 */ s8  field_7;
    /* 0x08 */ u8* field_8[9];
    /* 0x2C */ u8* field_2C;
    /* 0x30 */ u8* field_30;
    /* 0x34 */ s32 field_34;
    /* 0x38 */ s32 field_38;
} MidiTrack;
STATIC_ASSERT_SIZEOF(MidiTrack, 0x3C);

/// Active SPU voice slot (18 records at song offset 0x504). A negative voice
/// or channel marks a free entry. Program/layer identify the bank note;
/// velocity and volumeScale contribute to the volume update. Pitch bend is
/// scaled using the selected note's bend range, and reverb records its send.
typedef struct _MidiNoteSlot {
    /* 0x0 */ s8  voice;
    /* 0x1 */ s8  channel;
    /* 0x2 */ s8  key;
    /* 0x3 */ u8  velocity;
    /* 0x4 */ s8  volumeScale;
    /* 0x5 */ s8  pan;
    /* 0x6 */ u8  program;
    /* 0x7 */ u8  layer;
    /* 0x8 */ s16 pitchBend;
    /* 0xA */ s16 reverb;
} MidiNoteSlot;
STATIC_ASSERT_SIZEOF(MidiNoteSlot, 0xC);

/// Resident sequence, its timing/volume state, eighteen tracks, sixteen MIDI
/// channels and eighteen SPU voice slots. The sequence buffer is byte data;
/// sequenceBytes is its aligned length and waveBytes describes its bank data.
typedef struct _MidiSong {
    /* 0x00 */ u8                status;
    /* 0x01 */ u8                sequenceId;
    /* 0x02 */ u8                format;
    /* 0x03 */ u8                trackCount;
    /* 0x04 */ u8                field_4;
    /* 0x05 */ u8                field_5;
    /* 0x06 */ u8                field_6;
    /* 0x07 */ u8                field_7;
    /* 0x08 */ s16               volumeScale;
    /* 0x0A */ s16               sequenceBytes;
    /* 0x0C */ s32               volumeDirtyChannels;
    /* 0x10 */ u8*               sequenceData;
    /* 0x14 */ LinInterp         volumeRamp;
    /* 0x24 */ u8                unknown_24[0x10];
    /* 0x34 */ s32               ticksPerQuarter;
    /* 0x38 */ s32               songTicks;
    /* 0x3C */ s32               waveBytes;
    /* 0x40 */ SndBank*          bank;
    /* 0x44 */ SndBankGroup*     groups;
    /* 0x48 */ SndBankLayer*     notes;
    /* 0x4C */ MidiTrack         entries[18];
    /* 0x484 */ MidiChannelTable channels;
    /* 0x504 */ MidiNoteSlot     voiceSlots[0x12];
} MidiSong;
STATIC_ASSERT_SIZEOF(MidiSong, 0x5DC);

typedef u8* (*MidiHandler)(s32, u8*, MidiSong*, MidiTrack*);

/* Define BSS before API headers to preserve first-declaration order. */
/// Permission for the audio interrupt to drain deferred sound events (0 defer, 1 process).
///
/// Starts closed until the queue is reset. Appending an event closes the gate
/// while its links are updated and reopens it once the event is fully linked;
/// resetting the queue, including recovery from an invalid command, opens it.
static bool _gSndEvtProcessEnabled;

/// First pool slot awaiting deferred audio dispatch, or `NULL` when empty.
///
/// Queued slots belong to `_gSndEvtPool` and remain reserved until their handlers
/// return. Processing saves the next link before releasing the slot for reuse,
/// then advances the head; appends at the tail preserve FIFO order. Reset or an
/// invalid command discards the queue and clears both endpoints.
static SndEvt* _gSndEvtHead;

/// Last pool slot in the deferred audio-command FIFO, or `NULL` when empty.
///
/// Refers into `_gSndEvtPool`; the slot stays reserved through dispatch and its
/// `next` link is `NULL` after an append completes. Saving this endpoint permits
/// appending without traversing `_gSndEvtHead`. The audio interrupt's drain is
/// held off while append links are updated. Draining earlier slots leaves the
/// tail in place; releasing the last slot, reset and invalid-command recovery
/// clear both endpoints.
static SndEvt* _gSndEvtTail;

/// Maximum number of simultaneous sound-event reservations, queued or unqueued.
enum { SOUND_EVENT_POOL_CAPACITY = 64 };

/// Fixed resident storage for deferred MIDI and sound-script commands.
///
/// `sndEvtAlloc` reserves the first free slot, or returns `NULL` when all slots
/// are taken; unqueued reservations count against the same capacity. Dispatch
/// releases each slot for reuse. Allocation and release retain payload bytes,
/// so producers must initialize every argument their command reads.
/// Reset and invalid-command recovery zero the entire array, discarding queued
/// commands and unqueued reservations together.
static SndEvt _gSndEvtPool[SOUND_EVENT_POOL_CAPACITY];

static u8 D_8007F2F0;

/// Unreferenced.
static u8 D_8007F2F8[8];

static MidiSong Midi_Song;

static u8 D_8007F8E0[0x2800];

s32 D_800820E0;

s16 D_800820E4;

s16 D_800820E6;

static u8 D_800820E8;

static s8 D_800820E9;

SndLoadState SndLoad_State;

volatile u8 D_80082120;

volatile u8 D_80082121;

volatile u8 D_80082122;

volatile s32 D_80082124;

volatile s32 D_80082128;

volatile u8 D_8008212C;

volatile s32 D_80082130;

volatile s8 D_80082134;

volatile u8 D_80082135;

volatile u8 D_80082136;

#include "main/sound.h"

#include "sound.h"

extern void (*SndEvt_Handlers[])(SndEvt*);

static MidiHandler Midi_EventFns[];

static volatile s32 D_800689E8;

static u8 D_800689F0[];

extern s32 func_80179BE4(u16 arg0, u8 arg1, LinInterp* ramp);

static void SndEvt_Free(SndEvt* event);

/// Ignores reserved sound-event opcodes.
static void SndEvt_HandleNoOp(SndEvt* unused);

static void SndEvt_HandleInitSequence(SndEvt* event);

static void SndEvt_HandleStartFadeOut(SndEvt* event);

static void SndEvt_HandleFadeOn(SndEvt* event);

static void SndEvt_HandleFadeOff(SndEvt* event);

static void SndEvt_HandleSetVolume(SndEvt* event);

static void SndEvt_HandleAllocVoice(SndEvt* event);

static void SndEvt_HandleType7(SndEvt* event);

static void SndEvt_HandleFadeMatchingOn(SndEvt* event);

static void SndEvt_HandleFadeMatchingOff(SndEvt* event);

static void SndEvt_HandlePanRamp(SndEvt* event);

static void SndEvt_HandleVolumeRamp(SndEvt* event);

static void SndEvt_HandleRefCountInc(SndEvt* unused);

static void SndEvt_HandleRefCountDec(SndEvt* unused);

static void SndEvt_HandleKeyOffMatching(SndEvt* unused);

static s32 Midi_InitSequence(u8 arg0, u16 arg1);

static s32 SndEvt_EnqueueType3(s32 arg0);

static s32 SndEvt_EnqueueType4(s32 arg0);

static void Midi_StartFadeOut(u8 arg0, u16 arg1);

static void Midi_FadeVolume(u8 arg0, s32 arg1);

static void Midi_SetVolumeScale(u8 arg0, u8 arg1);

static MidiSong* Midi_GetSlot(s32 unused);

static void* Midi_GetFixedBuffer(s32 unused1, s32 unused2);

static void Midi_ClearVoiceEntry(void* context);

static void Midi_InitSlot(s32 arg0);

/* Returns where the event data of track `arg1` starts. Every chunk is an 8-byte
 * id/length header followed by `length` bytes, so track 0 follows the file
 * header chunk at `arg2`, and each later track follows the one before it,
 * whose data pointer must already be set. */
static u8* Midi_ResolveTrackData(MidiSong* song, s32 arg1, u8* arg2);

static void Midi_ResetTrackFlags(MidiSong* song);

static void Midi_KeyOffVoices(MidiSong* song);

static void Midi_DriveTrack(MidiSong* song, MidiTrack* track);

static void Midi_UpdateVoiceVolumes(MidiSong* song);

/* Note off: keys off every voice slot playing this channel's key, unless the
 * channel's noteEventsDisabled flag is set. A note on with zero velocity is a note off
 * too, and its event is one byte longer. Returns the cursor past the event. */
static inline u8* _midiNoteOff(s32 status, u8* data, MidiSong* song);

static u8* Midi_Event1(s32 arg0, u8* arg1, MidiSong* song, MidiTrack* unused);

static u8* Midi_Event3(s32 arg0, u8* arg1, MidiSong* song, MidiTrack* track);

/* Decodes the variable-length quantity at `p`, storing how many bytes it
 * took in `*len`. */
static inline s32 _midiReadVlq(u8* p, u8* len);

static u8* Midi_HandleMetaSysex(s32 unused1, u8* arg1, MidiSong* song, MidiTrack* track);

static s32 Midi_ReadVlq(u8* arg0, u8* arg1);

static void Midi_InitChannelTable(MidiChannelTable* channels);

static u8* Midi_IncPtr(s32 unused1, u8* arg1, MidiSong* unusedSong, MidiTrack* unusedTrack);

static u8* Midi_KeyOffChannel(s32 arg0, u8* arg1, MidiSong* song, MidiTrack* unused);

static u8* Midi_SetProgram(s32 arg0, u8* arg1, MidiSong* song, MidiTrack* unused);

static u8* Midi_PitchBend(s32 arg0, u8* arg1, MidiSong* song, MidiTrack* unused);

static s32 SndBank_SetupFromLoad(SndLoadState* load);

/// Turn the waveform addresses of the first `count` sample layers of `bank` from
/// offsets into its waveform data into absolute SPU RAM addresses.
static inline void _sndBankRebaseNotes(SndBank* bank, s32 count);

static s32 SndLoad_Complete(SndLoadState* load);

static void* SndLoad_AllocBuffer(s32 arg0, s32 arg1, u32 arg2);

static s32 SndLoad_LookupMode(s32 arg0, s32 arg1, s32 arg2);

static void SndLoad_Init(s32 arg0, void* arg1);

static s32 SndBank_FreeById(u16 arg0, s32 arg1);

void (*SndEvt_Handlers[])(SndEvt*) = {
    SndEvt_HandleNoOp,            // SOUND_EVENT_NO_OP
    SndEvt_HandleInitSequence,    // SOUND_EVENT_MIDI_START
    SndEvt_HandleStartFadeOut,    // SOUND_EVENT_MIDI_STOP
    SndEvt_HandleFadeOn,          // SOUND_EVENT_MIDI_MUTE
    SndEvt_HandleFadeOff,         // SOUND_EVENT_MIDI_UNMUTE
    SndEvt_HandleSetVolume,       // SOUND_EVENT_MIDI_SET_VOLUME
    SndEvt_HandleAllocVoice,      // SOUND_EVENT_SCRIPT_START
    SndEvt_HandleType7,           // SOUND_EVENT_SCRIPT_STOP
    SndEvt_HandleFadeMatchingOn,  // SOUND_EVENT_SCRIPT_MUTE
    SndEvt_HandleFadeMatchingOff, // SOUND_EVENT_SCRIPT_UNMUTE
    SndEvt_HandlePanRamp,         // SOUND_EVENT_SCRIPT_SET_PAN_ATTENUATION
    SndEvt_HandleVolumeRamp,      // SOUND_EVENT_SCRIPT_SET_VOLUME
    SndEvt_HandleNoOp,            // SOUND_EVENT_RESERVED_NO_OP
    SndEvt_HandleRefCountInc,     // SOUND_EVENT_SCRIPT_DUCK_ACQUIRE
    SndEvt_HandleRefCountDec,     // SOUND_EVENT_SCRIPT_DUCK_RELEASE
    SndEvt_HandleKeyOffMatching,  // SOUND_EVENT_SCRIPT_KEY_OFF
};

static MidiHandler Midi_EventFns[] = {
    Midi_KeyOffChannel,
    Midi_Event1,
    Midi_IncPtr,
    Midi_Event3,
    Midi_SetProgram,
    Midi_IncPtr,
    Midi_PitchBend,
    Midi_HandleMetaSysex,
};
volatile s32        gSndLoadBankId        = SOUND_LOAD_BANK_NONE;
static volatile s32 D_800689E8            = 0;
volatile s16        gSndVolumeReducedMode = SOUND_VOLUME_MODE_NORMAL;
static u8           D_800689F0[]          = {
    0x60,
    0x0,
    0x0,
    0x3E,
    0x5A,
    0x46,
    0x79,
    0x58,
    0x5A,
    0x7F,
    0x6A,
    0x6A,
    0x64,
    0x0,
    0x0,
    0x54,
    0x0,
    0x50,
    0x6D,
    0x0,
    0x50,
    0x50,
    0x50,
    0x50,
    0x0,
    0x0,
    0x0,
    0x0,
    0x0,
    0x60,
    0x60,
    0x60,
    0x61,
    0x62,
    0x63,
    0x64,
    0x50,
    0x33,
    0x7A,
    0x0,
    0x50,
    0x3A,
    0x5E,
    0x7F,
    0x45,
    0x5A,
    0x62,
    0x55,
    0x50,
    0x7F,
    0x5A,
    0x50,
    0x50,
    0x50,
    0x5E,
    0x55,
    0x7F,
    0x55,
    0x5E,
    0x72,
    0x55,
    0x50,
    0x7F,
    0x5A,
    0x50,
    0x5A,
    0x76,
    0x7D,
    0x78,
    0x74,
    0x56,
    0x7F,
    0x7F,
    0x7F,
    0x7F,
    0x69,
    0x60,
    0x50,
    0x70,
    0x7F,
    0x7B,
    0x52,
    0x6A,
    0x60,
    0x60,
    0x60,
    0x57,
    0x60,
    0x60,
    0x62,
    0x7F,
    0x60,
    0x60,
    0x56,
    0x78,
    0x7C,
    0x7C,
    0x60,
    0x5A,
    0x60,
};

void SndEvt_Process(void)
{
    SndEvt* nextEvent;
    SndEvt* event;
    u32     i;
    s32*    poolWord;

    if (_gSndEvtProcessEnabled == false) {
        return;
    }
    if (_gSndEvtHead == NULL) {
        return;
    }

    do {
        event = _gSndEvtHead;
        // Unsigned narrowing rejects negative stored commands as well as high ones.
        if ((u16)event->command >= (u32)ARRAY_SIZE(SndEvt_Handlers)) {
            // Clear every slot as words, including payloads and unqueued reservations.
            poolWord = (s32*)_gSndEvtPool;
            i        = 0;
            do {
                *poolWord = 0;
                i++;
                poolWord++;
            } while (i < sizeof(_gSndEvtPool) / sizeof(*poolWord));
            _gSndEvtHead           = NULL;
            _gSndEvtTail           = NULL;
            _gSndEvtProcessEnabled = true;
            return;
        }
        SndEvt_Handlers[event->command](event);
        event     = _gSndEvtHead;
        nextEvent = event->next;
        SndEvt_Free(event);
        if (nextEvent == NULL) {
            _gSndEvtTail = NULL;
            _gSndEvtHead = NULL;
            break;
        }
        _gSndEvtHead = nextEvent;
    } while (nextEvent != NULL);
}

void SndEvt_Reset(void)
{
    u32  i;
    s32* poolWord;

    // Clear reservations, payloads and queue links through a whole-pool word view.
    poolWord = (s32*)_gSndEvtPool;
    i        = 0;
    do {
        *poolWord = 0;
        i++;
        poolWord++;
    } while (i < sizeof(_gSndEvtPool) / sizeof(*poolWord));
    _gSndEvtHead           = NULL;
    _gSndEvtTail           = NULL;
    _gSndEvtProcessEnabled = true;
}

SndEvt* sndEvtAlloc(void)
{
    s32     slotIndex;
    s32     allocated;
    SndEvt* event;

    // Clear the slot index and materialize the occupied marker before the pool address.
    slotIndex = 0;
    allocated = SOUND_EVENT_SLOT_ALLOCATED;
    for (event = _gSndEvtPool; slotIndex < ARRAY_SIZE(_gSndEvtPool); slotIndex++, event++) {
        if (event->allocated == SOUND_EVENT_SLOT_FREE) {
            event->allocated = allocated;
            event->command   = SOUND_EVENT_NO_OP;
            return event;
        }
    }
    return NULL;
}

void sndEvtEnqueue(SndEvt* event)
{
    SndEvt* previousTail;

    if (event != NULL) {
        // Keep the audio interrupt from draining the queue until this slot is linked.
        _gSndEvtProcessEnabled = false;
        if (_gSndEvtHead == NULL) {
            _gSndEvtTail = event;
            _gSndEvtHead = event;
            event->prev  = NULL;
        } else {
            previousTail       = _gSndEvtTail;
            _gSndEvtTail       = event;
            event->prev        = previousTail;
            previousTail->next = event;
        }
        event->next            = NULL;
        _gSndEvtProcessEnabled = true;
    }
}

static void SndEvt_Free(SndEvt* event)
{
    if (event != NULL) {
        event->allocated = SOUND_EVENT_SLOT_FREE;
        event->prev      = NULL;
        event->next      = NULL;
    }
}

static void SndEvt_HandleNoOp(SndEvt* unused)
{
}

static void SndEvt_HandleInitSequence(SndEvt* event)
{
    Midi_InitSequence(event->args.midi.sequenceId, event->args.midi.fadeTicks);
}

static void SndEvt_HandleStartFadeOut(SndEvt* event)
{
    Midi_StartFadeOut(event->args.midi.sequenceId, event->args.midi.fadeTicks);
}

static void SndEvt_HandleFadeOn(SndEvt* event)
{
    Midi_FadeVolume(event->args.midi.sequenceId, 1);
}

static void SndEvt_HandleFadeOff(SndEvt* event)
{
    Midi_FadeVolume(event->args.midi.sequenceId, 0);
}

static void SndEvt_HandleSetVolume(SndEvt* event)
{
    Midi_SetVolumeScale(event->args.midi.sequenceId, event->args.midi.volumeScale);
}

static void SndEvt_HandleAllocVoice(SndEvt* event)
{
    SndEvtScriptArgs* args;

    args = &event->args.script;
    SndVoice_AllocSlot(args->soundId, args->panOffset, args->level.attenuation, args->bankSlot, args->entryControls);
}

static void SndEvt_HandleType7(SndEvt* event)
{
    SndEvtScriptArgs* args;

    args = &event->args.script;
    SndScript_StopMatching(args->soundId, args->stopControl);
}

static void SndEvt_HandleFadeMatchingOn(SndEvt* event)
{
    SndVoice_FadeMatching(event->args.script.soundId, 1);
}

static void SndEvt_HandleFadeMatchingOff(SndEvt* event)
{
    SndVoice_FadeMatching(event->args.script.soundId, 0);
}

static void SndEvt_HandlePanRamp(SndEvt* event)
{
    s32               temp_v0;
    SndEvtScriptArgs* args;

    args    = &event->args.script;
    temp_v0 = SndVoice_FindById(args->soundId);
    if (temp_v0 >= 0) {
        SndVoice_SetPanRamp(temp_v0, args->panOffset, args->level.attenuation);
    }
}

static void SndEvt_HandleVolumeRamp(SndEvt* event)
{
    s32               temp_v0;
    SndEvtScriptArgs* args;

    args    = &event->args.script;
    temp_v0 = SndVoice_FindById(args->soundId);
    if (temp_v0 >= 0) {
        SndVoice_SetVolumeRamp(temp_v0, args->level.volumeScale);
    }
}

static void SndEvt_HandleRefCountInc(SndEvt* unused)
{
    SndVoice_IncRefCount();
}

static void SndEvt_HandleRefCountDec(SndEvt* unused)
{
    SndVoice_TickRefCount();
}

static void SndEvt_HandleKeyOffMatching(SndEvt* unused)
{
    SndVoice_KeyOffMatching();
}

s32 Midi_InitSystem(u32 unused)
{
    s32       i;
    MidiSong* state;
    SndBank*  bank;

    for (i = 0; i <= 0; i++) {
        Midi_InitSlot(i & 0xFF);
    }
    D_8007F2F0 = 0x40;
    D_800820E9 = 0;
    D_800820E0 = 0;
    D_800820E4 = 0;
    Spu_SetVoiceRange(0, 0, 0x10);
    state                = Midi_GetSlot(0xFF);
    state->sequenceId    = 0xFF;
    state->sequenceBytes = 0x10;
    state->sequenceData  = D_8007F8E0;
    do {
        bank                   = &Snd_Banks[Snd_BankSlotsByType[15]];
        state->bank            = bank;
        bank->bankId           = 0xF0FF;
        state->bank->heapBlock = SndHeap_Malloc(SOUND_BANK_SEQUENCE_TABLE_BYTES);
    } while (0);
    state->bank->groups          = state->bank->heapBlock;
    state->bank->layers          = state->bank->heapBlock;
    state->bank->groupFirstLayer = state->bank->heapBlock;
    Snd_SequenceBankBuffer       = state->bank->heapBlock;
    state->waveBytes             = 0x10;
    return -1;
}

static s32 Midi_InitSequence(u8 arg0, u16 arg1)
{
    s32        i;
    s32        j;
    MidiSong*  obj;
    MidiTrack* tracks;
    MidiTrack* track;
    u8*        data;
    u8*        trackPtr;
    s32*       clearPtr;
    u8         len;

    i = 0;
    do {
        obj = &Midi_Song + i;
        if (obj->sequenceId != 0xFF) {
            if ((obj->sequenceId == arg0) && (obj->status == 0)) {
                Midi_InitChannelTable(&obj->channels);
                data = obj->sequenceData;
                if (((data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3]) != 0x4D546864) {
                    return -1;
                }

                tracks          = obj->entries;
                obj->format     = data[9];
                clearPtr        = (s32*)tracks;
                obj->trackCount = data[0xB];

                for (j = 0; j < obj->trackCount * (sizeof(MidiTrack) / sizeof(s32)); j++) {
                    *clearPtr++ = 0;
                }

                if (obj->trackCount != 0) {
                    j     = 0;
                    track = tracks;
                    do {
                        trackPtr          = Midi_ResolveTrackData(obj, j & 0xFF, obj->sequenceData);
                        track->field_8[8] = trackPtr;
                        track->field_2C   = trackPtr;
                        if ((trackPtr < D_8007F8E0) || (trackPtr >= (u8*)&D_800820E0)) {
                            return -1;
                        }
                        track->field_34  = Midi_ReadVlq(trackPtr, &len);
                        track->field_2C += len;
                        track->field_38  = 0xE0F;
                        j++;
                        track++;
                    } while (j < obj->trackCount);
                }

                obj->groups          = obj->bank->groups;
                obj->notes           = obj->bank->layers;
                obj->ticksPerQuarter = (data[0xC] << 8) | data[0xD];
                obj->field_6         = 0xFF;
                obj->field_4         = 0xFF;
                obj->field_7         = 0;
                obj->field_5         = 0;
                obj->volumeScale     = (D_800689F0[obj->sequenceId] * 3) << 5;
                LinInterp_Setup(&obj->volumeRamp, 0, D_8007F2F0, arg1);

                if (arg1 != 0) {
                    obj->status = 0x40;
                } else {
                    obj->status = 2;
                }

                obj->volumeDirtyChannels = 0xFFFF;
                obj->songTicks           = 0;
                for (j = 0; j < 0x12; j++) {
                    Midi_ClearVoiceEntry(&obj->voiceSlots[j]);
                }

                return i;
            }
        } else {
            break;
        }
        i++;
    } while (i <= 0);

    return -5;
}

s32 Midi_Tick(s32* unused)
{
    MidiSong* song;
    s32       i;
    s32       j;

    for (i = 0; i < 1; i++) {
        song = &Midi_Song + i;
        if (song->sequenceId == 0xFF) {
            break;
        }
        switch (song->status) {
            case 0:
                break;
            case 0x40:
            case 0x80:
                if (song->volumeRamp.gain == song->volumeRamp.targetGain) {
                    if (song->status == 0x40) {
                        song->status = 2;
                    } else {
                        song->status = 4;
                        goto stop;
                    }
                }
                /* fallthrough */
            case 8:
                LinInterp_Step(&song->volumeRamp);
                song->volumeDirtyChannels = 0xFFFF;
                /* fallthrough */
            case 2:
            play:
                for (j = 0; j < song->trackCount; j++) {
                    if (song->entries[j].field_5 == 0) {
                        Midi_DriveTrack(song, &song->entries[j]);
                    }
                }
                song->field_4 = song->field_6;
                song->field_5 = song->field_7;
                break;
            case 4:
            stop:
                Midi_ResetTrackFlags(song);
                Midi_KeyOffVoices(song);
                song->status = 0;
                break;
            case 0x10:
                if (song->status == 8 && song->volumeRamp.gain >= song->volumeRamp.targetGain) {
                    song->volumeDirtyChannels = 0xFFFF;
                    song->status              = 2;
                    goto play;
                }
                LinInterp_Step(&song->volumeRamp);
                song->volumeDirtyChannels = 0xFFFF;
                goto play;
        }
        if (song->volumeDirtyChannels != 0) {
            Midi_UpdateVoiceVolumes(song);
            song->volumeDirtyChannels = 0;
        }
    }
    return 0;
}

s32 SndEvt_EnqueueType1(s32 arg0, s32 arg1)
{
    SndEvt* event;

    if ((arg0 & 0xFF) == SOUND_EVENT_MIDI_INVALID_SEQUENCE) {
        return -3;
    }
    event = sndEvtAlloc();
    if (event == NULL) {
        return -2;
    }
    event->command              = SOUND_EVENT_MIDI_START;
    event->args.midi.sequenceId = arg0;
    event->args.midi.fadeTicks  = arg1;
    sndEvtEnqueue(event);
    return 0;
}

s32 SndEvt_EnqueueType2(s32 arg0, s32 arg1)
{
    // Stop fades truncate to 16 bits and round down to a multiple of four ticks.
    enum { SOUND_EVENT_MIDI_STOP_FADE_TICK_MASK = 0xFFFC };
    SndEvt* event;

    if ((arg0 & 0xFF) == SOUND_EVENT_MIDI_INVALID_SEQUENCE) {
        return -3;
    }
    event = sndEvtAlloc();
    if (event == NULL) {
        return -2;
    }
    event->command              = SOUND_EVENT_MIDI_STOP;
    event->args.midi.sequenceId = arg0;
    event->args.midi.fadeTicks  = arg1 & SOUND_EVENT_MIDI_STOP_FADE_TICK_MASK;
    sndEvtEnqueue(event);
    return 0;
}

static s32 SndEvt_EnqueueType3(s32 arg0)
{
    SndEvt* event;

    if ((arg0 & 0xFF) == SOUND_EVENT_MIDI_INVALID_SEQUENCE) {
        return -3;
    }
    event = sndEvtAlloc();
    if (event == NULL) {
        return -2;
    }
    event->command              = SOUND_EVENT_MIDI_MUTE;
    event->args.midi.sequenceId = arg0;
    sndEvtEnqueue(event);
    return 0;
}

static s32 SndEvt_EnqueueType4(s32 arg0)
{
    SndEvt* event;

    if ((arg0 & 0xFF) == SOUND_EVENT_MIDI_INVALID_SEQUENCE) {
        return -3;
    }
    event = sndEvtAlloc();
    if (event == NULL) {
        return -2;
    }
    event->command              = SOUND_EVENT_MIDI_UNMUTE;
    event->args.midi.sequenceId = arg0;
    sndEvtEnqueue(event);
    return 0;
}

s32 SndEvt_EnqueueType5(s32 arg0, s32 arg1)
{
    SndEvt*         event;
    SndEvtMidiArgs* args;

    if ((arg0 & 0xFF) == SOUND_EVENT_MIDI_INVALID_SEQUENCE) {
        return -3;
    }
    event = sndEvtAlloc();
    if (event == NULL) {
        return -2;
    }
    event->command   = SOUND_EVENT_MIDI_SET_VOLUME;
    args             = &event->args.midi;
    args->sequenceId = arg0;
    if ((s8)arg1 >= 0) {
        args->volumeScale = arg1;
    } else {
        args->volumeScale = SOUND_EVENT_MIDI_VOLUME_FULL;
    }
    sndEvtEnqueue(event);
    D_800820E8 = args->volumeScale;
    return 0;
}

s32 Midi_IsBusy(s32 arg0)
{
    s32 i;

    arg0 &= 0xFF;
    if (arg0 == 0xFF) {
        return 0;
    }
    for (i = 0; i <= 0; i++) {
        if ((arg0 == (&Midi_Song)[i].sequenceId) || (arg0 == 0)) {
            if ((&Midi_Song)[i].status & 0xCA) {
                return 1;
            }
        }
    }
    return 0;
}

s32 Midi_IsChannelFree(u8 arg0)
{
    s32 i;

    if (gSndVolumeReducedMode == SOUND_VOLUME_MODE_REDUCED) {
        return 0;
    }
    if (arg0 == 0xFF) {
        return 1;
    }
    for (i = 0; i <= 0; i++) {
        if ((&Midi_Song)[i].sequenceId == arg0) {
            return 0;
        }
    }
    return 1;
}

static void Midi_StartFadeOut(u8 arg0, u16 arg1)
{
    s32       i;
    MidiSong* ptr;

    for (i = 0; i <= 0; i++) {
        ptr = &Midi_Song + i;
        if ((arg0 == ptr->sequenceId) || (arg0 == 0)) {
            if (ptr->status == 2) {
                ptr->status = 0x80;
                LinInterp_Setup(&ptr->volumeRamp, D_8007F2F0, 0, arg1);
            } else {
                ptr->status = 4;
            }
        }
    }
}

static void Midi_FadeVolume(u8 arg0, s32 arg1)
{
    s32       i;
    MidiSong* ptr;

    for (i = 0; i <= 0; i++) {
        ptr = &Midi_Song + i;
        if ((arg0 == ptr->sequenceId) || (arg0 == 0)) {
            if (arg1 == 0) {
                if (ptr->status == 8) {
                    ptr->status = 0x10;
                    LinInterp_Setup(&ptr->volumeRamp, 0, D_8007F2F0, 8);
                }
            } else {
                if (ptr->status & 0x12) {
                    ptr->status = 8;
                    LinInterp_Setup(&ptr->volumeRamp, D_8007F2F0, 0, 8);
                }
            }
        }
    }
}

static void Midi_SetVolumeScale(u8 arg0, u8 arg1)
{
    s32       i;
    u8*       table;
    MidiSong* arr;
    s32       product;

    i   = 0;
    arr = &Midi_Song;
    for (; i <= 0; i++) {
        if ((arg0 == arr[i].sequenceId) || (arg0 == 0)) {
            table                      = D_800689F0;
            product                    = table[arr[i].sequenceId] * arg1;
            arr[i].volumeDirtyChannels = 0xFFFF;
            arr[i].volumeScale         = product;
        }
    }
}

void Midi_SetMasterVolume(s32 arg0)
{
    s32 i;
    s32 val;
    u8* flag;

    flag = &D_8007F2F0;
    if ((s8)arg0 >= 0) {
        *flag = arg0;
    } else {
        *flag = 0x7F;
    }

    i   = 0;
    val = 0xFFFF;
    for (; i <= 0; i++) {
        (&Midi_Song)[i].volumeDirtyChannels = val;
    }
}

s32 Midi_GetMasterVolume(void)
{
    return D_8007F2F0;
}

static MidiSong* Midi_GetSlot(s32 unused)
{
    if (Midi_Song.status != 0) {
        Midi_ResetTrackFlags(&Midi_Song);
        Midi_Song.status = 4;
    }
    return &Midi_Song;
}

static void* Midi_GetFixedBuffer(s32 unused1, s32 unused2)
{
    return D_8007F8E0;
}

static void Midi_ClearVoiceEntry(void* context)
{
    MidiNoteSlot* slot = context;
    u32           i;
    s32*          ptr;

    ptr = (s32*)slot;
    i   = 0;
    do {
        *ptr = 0;
        i++;
        ptr++;
    } while (i < 3U);
    slot->channel = -1;
    slot->voice   = -1;
}

void SndEvt_EnqueueType5Pending(void)
{
    SndEvt*         event;
    SndEvtMidiArgs* args;

    D_800820E9 = 1;
    event      = sndEvtAlloc();
    if (event != NULL) {
        args              = &event->args.midi;
        event->command    = SOUND_EVENT_MIDI_SET_VOLUME;
        args->sequenceId  = SOUND_EVENT_MIDI_ALL_SEQUENCES;
        args->volumeScale = SOUND_EVENT_MIDI_VOLUME_SILENT;
        sndEvtEnqueue(event);
        D_800820E8 = args->volumeScale;
    }
}

void SndEvt_FlushType5Pending(void)
{
    SndEvt*         event;
    SndEvtMidiArgs* args;
    u8              saved;

    if (D_800820E9 != 0) {
        saved      = D_800820E8;
        D_800820E9 = 0;
        event      = sndEvtAlloc();
        if (event != NULL) {
            args             = &event->args.midi;
            event->command   = SOUND_EVENT_MIDI_SET_VOLUME;
            args->sequenceId = SOUND_EVENT_MIDI_ALL_SEQUENCES;
            if ((s8)saved >= 0) {
                args->volumeScale = saved;
            } else {
                args->volumeScale = SOUND_EVENT_MIDI_VOLUME_FULL;
            }
            sndEvtEnqueue(event);
            D_800820E8 = args->volumeScale;
        }
    }
}

static void Midi_InitSlot(s32 arg0)
{
    MidiSong*     obj;
    s32*          p;
    u32           i;
    s32           offset;
    u32           k;
    MidiNoteSlot* slot;
    s32*          q;
    s8            freemark;

    arg0 &= 0xFF;
    obj   = &(&Midi_Song)[arg0];

    p = (s32*)obj;
    i = 0;
    do {
        *p = 0;
        i++;
        p++;
    } while (i < 0x177U);

    LinInterp_Setup(&obj->volumeRamp, 0, 0, 0);
    Midi_InitChannelTable(&obj->channels);

    i        = 0;
    freemark = -1;
    offset   = 0;
    do {
        slot = (MidiNoteSlot*)(offset + (s32)obj);
        slot = ((MidiSong*)slot)->voiceSlots;
        q    = (s32*)slot;
        k    = 0;
        do {
            *q = 0;
            k++;
            q++;
        } while (k < 3U);
        offset += 0xC;
        i++;
        slot->channel = freemark;
        slot->voice   = freemark;
    } while ((s32)i < 0x12);
}

/// Reads the big-endian 32-bit value at `p`, the form every length in a
/// Standard MIDI File takes.
#define MIDI_READ_BE32(p) (((p)[0] << 24) | ((p)[1] << 16) | ((p)[2] << 8) | (p)[3])

/* Returns where the event data of track `arg1` starts. Every chunk is an 8-byte
 * id/length header followed by `length` bytes, so track 0 follows the file
 * header chunk at `arg2`, and each later track follows the one before it,
 * whose data pointer must already be set. */
static u8* Midi_ResolveTrackData(MidiSong* song, s32 arg1, u8* arg2)
{
    u32 len;

    if ((u8)arg1 != 0) {
        arg2 = song->entries[(u8)arg1 - 1].field_8[8];
        len  = MIDI_READ_BE32(arg2 - 4);
        return arg2 + len + 8;
    }
    len = MIDI_READ_BE32(arg2 + 4);
    return arg2 + 8 + len + 8;
}

static void Midi_ResetTrackFlags(MidiSong* song)
{
    s32 i;

    for (i = 0; i < song->trackCount; i++) {
        song->entries[i].field_5 = 1;
    }
}

static void Midi_KeyOffVoices(MidiSong* song)
{
    s32           i;
    MidiNoteSlot* slot;
    u8            status;
    SpuVoiceRef   sp10;
    u16           temp;

    i    = 0;
    slot = song->voiceSlots;
    do {
        if (slot->voice >= 0) {
            status = Spu_GetVoiceStatus(slot->voice);
            if (status != 0) {
                Spu_GetVoiceRef(slot->voice, &sp10);
                temp                = sp10.field_4->adsr2;
                temp                = (temp & 0xFFE0) | 5;
                sp10.field_4->adsr2 = temp;
                sp10.field_4->mask |= SPU_VOICE_ADSR_ADSR2;
                if (status != 2) {
                    Spu_KeyOff(slot->voice);
                }
            }
        }
        i++;
        slot++;
    } while (i < 0x12);
}

static void Midi_DriveTrack(MidiSong* song, MidiTrack* track)
{
    u8  len;
    u32 temp;
    s32 ticks;
    s32 quot;
    u32 rem;
    u8  status;

    temp = track->field_38 + (song->field_4 + song->field_5) * song->ticksPerQuarter;
    if (gDisplayState.region == MODE_PAL) {
        quot = temp / 6000U;
    } else {
        quot = temp / 3600U;
    }
    if (gDisplayState.region == MODE_PAL) {
        rem = temp % 6000U;
    } else {
        rem = temp % 3600U;
    }
    track->field_38  = rem;
    song->songTicks += quot;
    ticks            = quot;
    if (song->sequenceId == 0x4F) {
        song->volumeDirtyChannels = 0xFFFF;
    }
    while (ticks >= track->field_34) {
        ticks          -= track->field_34;
        track->field_34 = 0;
        do {
            status = *track->field_2C;
            if (status & 0x80) {
                track->field_3 = 0;
                if ((status & 0xF0) != 0xF0) {
                    track->field_2 = status & MIDI_CHANNEL_STATUS_MASK;
                }
                track->field_2C = Midi_EventFns[((status & 0xF0) >> 4) - 8](status, track->field_2C, song, track);
            } else {
                track->field_3  = 1;
                track->field_2C = Midi_EventFns[1](track->field_2 | 0x90, track->field_2C - 1, song, track);
            }
            if (track->field_5 != 0) {
                goto end;
            }
            if (track->field_2C == NULL) {
                track->field_0  = 0;
                track->field_38 = 0;
                song->status    = 4;
                return;
            }
            track->field_34  = Midi_ReadVlq(track->field_2C, &len);
            track->field_2C += len;
        } while (track->field_34 == 0);
    }
end:
    track->field_34 -= ticks;
}

static void Midi_UpdateVoiceVolumes(MidiSong* song)
{
    // The velocity curve reaches 16383; retain a 0..127 channel gain before
    // combining it with the note's gain and the song's SPU volume.
    enum {
        MIDI_VELOCITY_GAIN_FULL   = 0x3FFF,
        MIDI_CHANNEL_GAIN_DIVISOR = SOUND_EVENT_MIDI_VOLUME_FULL * MIDI_VELOCITY_GAIN_FULL,
        MIDI_VOICE_GAIN_DIVISOR   = SOUND_EVENT_MIDI_VOLUME_FULL * SOUND_EVENT_MIDI_VOLUME_FULL
    };
    SpuVoiceRef   sp10;
    s16           sp18[2];
    LinInterp*    interp;
    s32           volume;
    s32           i;
    MidiNoteSlot* slot;
    _MidiChannel* channelControls;
    s32           product;
    u32           vol;
    s32           channel;
    s32           mask;
    s32           pan;
    s8            voice;

    interp = &song->volumeRamp;
    if (song->sequenceId == 0x4F && D_80082120 == 5) {
        volume = func_80179BE4((u16)song->volumeScale, D_80082136, interp);
    } else if (song->sequenceId == 0x5A) {
        volume = LinInterp_Apply(interp, (u32)((Midi_GetMasterVolume() & 0xFF) * ((D_800689F0[0x5A] * 3) << 5)) / 127U);
    } else {
        volume = LinInterp_Apply(interp, (u32)((Midi_GetMasterVolume() & 0xFF) * (u16)song->volumeScale) / 127U);
    }
    i    = 0;
    slot = song->voiceSlots;
    do {
        voice = slot->voice;
        if (voice >= 0) {
            channel = (u8)slot->channel;
            mask    = 1 << channel;
            if (song->volumeDirtyChannels & mask) {
                channelControls = &song->channels.entries[channel];
                product         = channelControls->volume * channelControls->expression * Snd_VelocityGainTable[slot->velocity];
                product         = product / MIDI_CHANNEL_GAIN_DIVISOR;
                vol             = (u32)(volume * slot->volumeScale * product) / (u32)MIDI_VOICE_GAIN_DIVISOR;
                pan             = channelControls->pan - MIDI_CHANNEL_PAN_CENTER;
                Spu_ApplyPanVolume(sp18, slot->pan + pan, vol);
                Spu_GetVoiceRef(voice, &sp10);
                if (D_800820E9 == 1 && song->sequenceId != 0x5A) {
                    sp10.field_4->volume.left  = 0;
                    sp10.field_4->volume.right = 0;
                } else {
                    sp10.field_4->volume.left  = sp18[0];
                    sp10.field_4->volume.right = sp18[1];
                }
                sp10.field_4->volmode.left  = 0;
                sp10.field_4->volmode.right = 0;
                sp10.field_4->mask         |= 0xF;
            }
        }
        i++;
        slot++;
    } while (i < 0x12);
}

/* Note off: keys off every voice slot playing this channel's key, unless the
 * channel's noteEventsDisabled flag is set. A note on with zero velocity is a note off
 * too, and its event is one byte longer. Returns the cursor past the event. */
static inline u8* _midiNoteOff(s32 status, u8* data, MidiSong* song)
{
    s32 i;
    u8  channel;
    u8  key;
    u8* ptr;

    ptr     = data;
    channel = status & MIDI_CHANNEL_STATUS_MASK;
    key     = ptr[1];
    if ((status & 0xF0) == 0x90) {
        ptr += 1;
    }
    if (song->channels.entries[channel].noteEventsDisabled != 0) {
        return ptr + 2;
    }
    for (i = 0; i < 0x12; i++) {
        if ((song->voiceSlots[i].key == key) && (song->voiceSlots[i].channel == channel)) {
            Spu_KeyOff(song->voiceSlots[i].voice);
        }
    }
    return ptr + 2;
}

static u8* Midi_Event1(s32 arg0, u8* arg1, MidiSong* song, MidiTrack* unused)
{
    enum {
        SOUND_BANK_VOLUME_FRACTION_BITS = 7,
        SOUND_BANK_PAN_CENTER           = 64,
        SOUND_BANK_PAN_MAX              = 127
    };
    s16           priorities[2];
    SpuVoiceRef   ref;
    u8            channel;
    u8            program;
    u8            key;
    u8            velocity;
    u8            layer;
    s8            voice;
    u16           priority;
    s32           i;
    s16           pan;
    s32           reverb;
    s32           bend;
    s32           product;
    s32           scale;
    SndBankGroup* group;
    SndBankLayer* bankLayer;
    MidiNoteSlot* slot;
    SpuVoiceAttr* attr;

    velocity = arg1[2];
    if (velocity == 0) {
        arg1 = _midiNoteOff(arg0, arg1, song);
    } else {
        channel = arg0 & MIDI_CHANNEL_STATUS_MASK;
        if (song->channels.entries[channel].noteEventsDisabled != 0) {
            return arg1 + 3;
        }
        program   = song->channels.entries[channel].program;
        group     = &song->groups[program];
        key       = arg1[1];
        bankLayer = Snd_GetNote(song->bank, program, 0);
        for (layer = 0; layer < group->layerCount; layer++, bankLayer++) {
            priority = bankLayer->priority;
            if (key >= bankLayer->keyMin && bankLayer->keyMax >= key) {
                if (priority == 0) {
                    priorities[0] = 2;
                    priorities[1] = 0;
                } else {
                    priorities[0] = 0;
                    priorities[1] = 2;
                }
                voice = Spu_AllocVoice(priorities, 2, priority);
                if (voice >= 0) {
                    slot                       = &song->voiceSlots[voice];
                    song->volumeDirtyChannels |= 1 << channel;
                    Spu_SetVoiceCallbacks(voice, Midi_ClearVoiceEntry, slot);
                    Spu_GetVoiceRef(voice, &ref);
                    slot->voice       = voice;
                    slot->channel     = channel;
                    slot->velocity    = velocity;
                    slot->key         = key;
                    slot->volumeScale = (group->volume * bankLayer->volume) >> SOUND_BANK_VOLUME_FRACTION_BITS;
                    pan               = group->pan + bankLayer->pan - SOUND_BANK_PAN_CENTER;
                    if (pan <= SOUND_BANK_PAN_MAX) {
                        if (pan >= 0) {
                            slot->pan = pan;
                        } else {
                            slot->pan = 0;
                        }
                    } else {
                        slot->pan = SOUND_BANK_PAN_MAX;
                    }
                    slot->program = program;
                    slot->layer   = layer;
                    reverb        = bankLayer->reverb;
                    if (reverb == SPU_ON) {
                        Spu_EnableReverbVoice(slot->voice);
                        slot->reverb = reverb;
                    } else {
                        Spu_DisableReverbVoice(slot->voice);
                        slot->reverb = 0;
                    }
                    bend = song->channels.entries[channel].pitchBend;
                    if (bend != 0) {
                        if (bend > 0) {
                            scale = bankLayer->bendUp;
                        } else {
                            scale = bankLayer->bendDown;
                        }
                        product         = (scale << MIDI_PITCH_FRACTION_BITS) * bend;
                        slot->pitchBend = product / MIDI_PITCH_BEND_MAX;
                    }
                    attr        = ref.field_4;
                    attr->addr  = bankLayer->waveAddr;
                    attr->adsr1 = bankLayer->adsr1;
                    attr->adsr2 = bankLayer->adsr2;
                    attr->pitch = Spu_CalcVolume(key, slot->pitchBend, bankLayer->rootKey, bankLayer->fineTune);
                    attr->mask  = SPU_VOICE_WDSA | SPU_VOICE_ADSR_ADSR1 | SPU_VOICE_ADSR_ADSR2 | SPU_VOICE_PITCH;
                    Spu_KeyOn(slot->voice);
                }
            }
        }
        arg1 += 3;
    }
    return arg1;
}

static u8* Midi_Event3(s32 arg0, u8* arg1, MidiSong* song, MidiTrack* track)
{
    enum {
        MIDI_CHANNEL_CONTROL_VOLUME     = 7,
        MIDI_CHANNEL_CONTROL_PAN        = 10,
        MIDI_CHANNEL_CONTROL_EXPRESSION = 11
    };
    u8  channel;
    u8  ctrl;
    s32 value;
    u8  status;

    channel = arg0 & MIDI_CHANNEL_STATUS_MASK;
    ctrl    = arg1[1];

    switch (ctrl) {
        case 6:
            status = track->field_0;
            if (status != 0x10) {
                if (status != 0x14) {
                    return arg1 + 3;
                }
                if (track->field_4 != 0) {
                    return arg1 + 3;
                }
                track->field_30 = arg1 + 3;
                if ((s8)arg1[2] >= 0) {
                    track->field_4 = arg1[2];
                } else {
                    track->field_4 = 0x7F;
                }
                track->field_0 = 0;
            } else {
                if (track->field_1 != status) {
                    return arg1 + 3;
                }
                Spu_SetReverbDepth((s16)(arg1[2] << 8));
                track->field_0 = 0;
                track->field_1 = 0;
            }
            break;

        case MIDI_CHANNEL_CONTROL_VOLUME:
            song->channels.entries[channel].volume = arg1[2];
            song->volumeDirtyChannels             |= 1 << channel;
            break;

        case MIDI_CHANNEL_CONTROL_PAN:
            if (CdVol_GetMixMode() & 0xFF) {
                song->channels.entries[channel].pan = arg1[2];
            } else {
                song->channels.entries[channel].pan = MIDI_CHANNEL_PAN_CENTER;
            }
            song->volumeDirtyChannels |= 1 << channel;
            break;

        case MIDI_CHANNEL_CONTROL_EXPRESSION:
            song->channels.entries[channel].expression = arg1[2];
            song->volumeDirtyChannels                 |= 1 << channel;
            break;

        case 0x62:
            track->field_1 = arg1[2];
            break;

        case 0x63:
            value          = arg1[2];
            track->field_0 = value;
            if ((value & 0xFF) == 0x14) {
                break;
            }
            if ((value & 0xFF) != 0x1E) {
                return arg1 + 3;
            }
            if ((track->field_4 & 0xFF) < 0x7F) {
                if ((track->field_4 & 0xFF) == 0) {
                    track->field_4 = 0;
                    break;
                }
                track->field_4 = track->field_4 - 1;
            }
            return track->field_30;

        default:
            return arg1 + 3;
    }

    return arg1 + 3;
}

/* Decodes the variable-length quantity at `p`, storing how many bytes it
 * took in `*len`. */
static inline s32 _midiReadVlq(u8* p, u8* len)
{
    s32 result;

    result = 0;
    *len   = 0;
    do {
        result <<= 7;
        result  |= *p & 0x7F;
        *len     = *len + 1;
    } while (*p++ & 0x80);
    return result;
}

static u8* Midi_HandleMetaSysex(s32 unused1, u8* arg1, MidiSong* song, MidiTrack* track)
{
    u8  sp0;
    s32 var_a0;
    u8* var_t0;
    u8  temp_v1;
    s8  temp_v0;

    var_t0 = arg1;
    switch (*var_t0) {
        case 0xF0:
            temp_v1 = *var_t0;
            var_t0 += 1;
            if (temp_v1 != 0xF7) {
                do {
                } while (*var_t0++ != 0xF7);
            }
            goto f7_body;
        case 0xF5:
            if (track->field_7 < 9) {
                track->field_6                 = 1;
                track->field_8[track->field_7] = var_t0 + 3;
                track->field_7                 = (u8)track->field_7 + 1;
                var_t0 =
                    var_t0 + ((s16)((var_t0[1] << 8) | var_t0[2]) + 3);
            } else {
                var_t0 = NULL;
            }
            break;
        case 0xF6:
            if (track->field_7 < 0) {
                track->field_6 = 0;
                var_t0         = NULL;
            } else {
                temp_v0        = (u8)track->field_7 - 1;
                track->field_7 = temp_v0;
                var_t0         = track->field_8[temp_v0];
            }
            break;
        case 0xF7:
            goto f7_body;
        case 0xFF:
            var_t0 += 1;
            temp_v1 = *var_t0;
            if (temp_v1 == 0x2F) {
                goto eot;
            }
            if (temp_v1 == 0x51) {
                goto tempo;
            }
            goto vlq;
        eot:
            track->field_5 = 1;
        f7_body:
            var_t0 += 1;
            break;
        tempo: {
            u32 tempo_val;
            tempo_val     = var_t0[2] << 16;
            tempo_val    |= var_t0[3] << 8;
            tempo_val    |= var_t0[4];
            var_t0       += 5;
            song->field_7 = 0;
            song->field_6 = 0x3938700U / tempo_val;
        } break;
        vlq: {
            s32 hdrLen;

            var_a0 = _midiReadVlq(var_t0 + 1, &sp0);
            /* The meta type byte and the length's own bytes precede the data. */
            hdrLen  = sp0 + 1;
            var_t0 += var_a0 + hdrLen;
        } break;
        default:
            var_t0 = NULL;
            break;
    }
    return var_t0;
}

static s32 Midi_ReadVlq(u8* arg0, u8* arg1)
{
    return _midiReadVlq(arg0, arg1);
}

static void Midi_InitChannelTable(MidiChannelTable* channels)
{
    // Little-endian first word: note events enabled, volume 64, expression
    // 127 and neutral pan. The second word resets program and bend to zero.
    enum {
        MIDI_CHANNEL_VOLUME_DEFAULT      = 64,
        MIDI_CHANNEL_RESET_CONTROLS_WORD = (MIDI_CHANNEL_PAN_CENTER << 24) |
                                           (SOUND_EVENT_MIDI_VOLUME_FULL << 16) |
                                           (MIDI_CHANNEL_VOLUME_DEFAULT << 8)
    };
    s32  i;
    u32* words;

    if (channels != NULL) {
        words = channels->words;
        for (i = 0; i < ARRAY_SIZE(channels->entries); i++) {
            *words++ = MIDI_CHANNEL_RESET_CONTROLS_WORD;
            *words++ = 0;
        }
    }
}

static u8* Midi_IncPtr(s32 unused1, u8* arg1, MidiSong* unusedSong, MidiTrack* unusedTrack)
{
    return arg1 + 1;
}

static u8* Midi_KeyOffChannel(s32 arg0, u8* arg1, MidiSong* song, MidiTrack* unused)
{
    return _midiNoteOff(arg0, arg1, song);
}

static u8* Midi_SetProgram(s32 arg0, u8* arg1, MidiSong* song, MidiTrack* unused)
{
    song->channels.entries[arg0 & MIDI_CHANNEL_STATUS_MASK].program = arg1[1];
    return arg1 + 2;
}

static u8* Midi_PitchBend(s32 arg0, u8* arg1, MidiSong* song, MidiTrack* unused)
{
    enum { MIDI_PITCH_WHEEL_CENTER = 0x2000 };
    SpuVoiceRef   sp10;
    u8            channel;
    s32           i;
    s16           pitchBend;
    MidiNoteSlot* slot;
    SndBankLayer* bankLayer;
    s32           scale;
    s16           pitch;
    SpuVoiceAttr* attr;

    channel                                   = arg0 & MIDI_CHANNEL_STATUS_MASK;
    i                                         = 0;
    pitchBend                                 = (arg1[1] | (arg1[2] << 7)) - MIDI_PITCH_WHEEL_CENTER;
    song->channels.entries[channel].pitchBend = pitchBend;
    do {
        slot = &song->voiceSlots[i];
        if (slot->channel == channel) {
            Spu_GetVoiceRef(slot->voice, &sp10);
            bankLayer = Snd_GetNote(song->bank, slot->program, slot->layer);
            if (pitchBend >= 0) {
                scale   = bankLayer->bendUp;
                scale <<= MIDI_PITCH_FRACTION_BITS;
            } else {
                scale   = bankLayer->bendDown;
                scale <<= MIDI_PITCH_FRACTION_BITS;
            }
            scale          *= pitchBend;
            pitch           = scale / MIDI_PITCH_BEND_MAX;
            slot->pitchBend = pitch;
            attr            = sp10.field_4;
            attr->pitch     = Spu_CalcVolume((u16)slot->key, pitch, bankLayer->rootKey, bankLayer->fineTune);
            attr->mask     |= SPU_VOICE_PITCH;
        }
        i += 1;
    } while (i < 0x12);
    return arg1 + 3;
}

s32 SndLoad_ProcessSector(u32* arg0)
{
    SndLoadState* state;
    u32*          src;
    u32           i;
    u32*          dst;
    s32           nibble;
    s32           count;
    s32           aligned;
    void*         mem;
    s32           len;
    s32           spuAddr;

    state = &SndLoad_State;
    switch (state->field_2) {
        case 0:
            src = arg0;
            dst = state->payload.words;
            i   = 0;
            do {
                *dst = *src;
                src++;
                i++;
                dst++;
            } while (i < 5U);

            nibble = state->payload.header.bankId & SOUND_BANK_TYPE_MASK;
            if ((u32)(nibble - 0x8000) < 0x5001U) {
                D_800689E8     = 1;
                state->field_2 = 7;
                break;
            }
            if (nibble == SOUND_BANK_TYPE_1) {
                D_80082128 = 0;
            }
            {
                // Publish the accepted id before the previous bank is released.
                s32 bankId;
                bankId           = state->payload.header.bankId;
                *&gSndLoadBankId = bankId;
                if (SndBank_FreeById(state->payload.header.bankId, state->payload.header.variant) == -1) {
                    state->field_2 = 6;
                    break;
                }
            }
            {
                SndBank* tmp;
                tmp         = Snd_AllocBank(&state->payload.header);
                state->bank = tmp;
                if (tmp == 0) {
                    state->field_2 = 6;
                    break;
                }
                src = arg0 + 5;
                dst = tmp->heapBlock;
            }
            count = (state->payload.header.noteCount * (s32)(sizeof(*state->bank->layers) / sizeof(*dst))) + state->payload.header.groupCount * (s32)(sizeof(*state->bank->groups) / sizeof(*dst));
            i     = 0;
            if (count != 0) {
                do {
                    *dst = *src;
                    src++;
                    i++;
                    dst++;
                } while ((s32)i < count);
            }
            (state->bank)->groupCount = state->payload.header.groupCount;
            (state->bank)->layerCount = state->payload.header.noteCount;
            (state->bank)->bankId     = state->payload.header.bankId;
            (state->bank)->waveBytes  = state->payload.header.waveBytes;
            state->field_2            = 1;
            break;

        case 1:
            aligned            = (state->payload.header.imageBytes + 3) & 0xFFFC;
            state->field_C     = aligned;
            mem                = SndLoad_AllocBuffer(state->payload.header.bankId, state->payload.header.variant, aligned);
            state->imageBuffer = mem;
            if (mem == 0) {
                state->field_2 = 6;
                Snd_FreeBank(state->bank);
                state->bank = 0;
                break;
            }
            state->writeCursor = mem;
            state->field_2     = 2;
            /* fallthrough */
        case 2:
            len = (u32)state->field_C >> 2;
            if ((u32)state->field_C < (u32)state->field_10) {
                state->field_2 = 3;
            } else {
                len             = (u32)state->field_10 >> 2;
                state->field_C -= state->field_10;
            }
            src = arg0;
            dst = (u32*)state->writeCursor;
            i   = 0;
            if (len != 0) {
                do {
                    *dst = *src;
                    src++;
                    i++;
                    dst++;
                } while (i < (u32)len);
            }
            state->writeCursor += len * 4;
            break;

        case 3: {
            s32 size;
            size                   = state->payload.header.waveBytes;
            state->field_C         = size;
            (state->bank)->spuAddr = SndLoad_LookupMode(
                state->payload.header.variant, (state->bank)->bankId, size);
            spuAddr = (state->bank)->spuAddr;
        }
            if (spuAddr == 0) {
                D_800689E8     = 4;
                state->field_2 = 6;
                Snd_FreeBank(state->bank);
                state->bank = 0;
                break;
            }
            SpuSetTransferStartAddr(spuAddr + (state->payload.header.waveBlockOffset << 6));
            state->field_2 = 4;
            /* fallthrough */
        case 4: {
            s32 rem;
            s32 step;
            rem  = state->field_C;
            step = state->field_10;
            if ((u32)step >= (u32)rem) {
                len            = rem;
                state->field_2 = 5;
            } else {
                len            = step;
                state->field_C = rem - step;
            }
        }
            if (state->field_3 == 0) {
                if (SpuIsTransferCompleted(0) == 0) {
                    if (state->field_0 != 0x10) {
                        Snd_FreeBank(state->bank);
                        state->bank = 0;
                    }
                    D_800689E8     = 5;
                    state->field_2 = 7;
                    break;
                }
                SpuWritePartly((u8*)arg0, len);
            } else {
                SpuWritePartly((u8*)arg0, len);
                SpuIsTransferCompleted(1);
            }
            break;

        case 5:
            break;

        case 6:
            if ((state->field_1 + 1) >= (s32)state->payload.header.transferSectors) {
                D_800689E8 = 6;
                if ((state->payload.header.bankId & SOUND_BANK_TYPE_MASK) == 0x5000) {
                    if (D_80082128 == 0) {
                        D_80082124 = 0x63810 - ((state->payload.header.waveBytes + 0x3F) & ~0x3F);
                    } else {
                        D_80082124 = D_80082128 - ((state->payload.header.waveBytes + 0x3F) & ~0x3F);
                    }
                }
                if ((state->payload.header.bankId & SOUND_BANK_TYPE_MASK) == SOUND_BANK_TYPE_1) {
                    D_80082128 = 0x63810 - ((state->payload.header.waveBytes + 0x3F) & ~0x3F);
                }
                state->field_2 = 5;
            }
            break;
    }

    state->field_1 += 1;
    return state->field_2;
}

static s32 SndBank_SetupFromLoad(SndLoadState* load)
{
    SndBank*      bank;
    SndBankSlot*  bankSlot;
    u16           id;
    s8            slot;
    s32           i;
    u32           spuAddr;
    SndBankLayer* bankLayer;

    bank = load->bank;
    if (D_800689E8 != 0 || (id = bank->bankId) == SOUND_BANK_ID_FREE) {
    fail:
        gSndLoadBankId = SOUND_LOAD_BANK_NONE;
        return -1;
    }
    slot = Snd_BankSlotsByType[id >> 12];
    if (slot == -1) {
        goto fail;
    }
    if ((id & SOUND_BANK_TYPE_MASK) == 0x4000) {
        slot = slot - 1 + D_80082122;
    }
    bankSlot = SndBankSlot_Get(slot);
    if (bankSlot == NULL) {
        goto fail;
    }
    bankSlot->bankId  = bank->bankId;
    bankSlot->bank    = bank;
    bankSlot->image   = load->imageBuffer;
    bankSlot->spuAddr = bank->spuAddr;
    i                 = load->payload.header.noteCount;
    spuAddr           = bank->spuAddr;
    bankLayer         = bank->layers;
    for (i--; i != -1; i--) {
        bankLayer->waveAddr += spuAddr;
        bankLayer++;
    }
    Snd_BuildGroupIndex(bankSlot->bank);
    gSndLoadBankId    = SOUND_LOAD_BANK_NONE;
    load->bank        = 0;
    load->imageBuffer = 0;
    D_8008212C        = D_80082122;
    D_80082121        = D_80082135;
    return 0;
}

/// Turn the waveform addresses of the first `count` sample layers of `bank` from
/// offsets into its waveform data into absolute SPU RAM addresses.
static inline void _sndBankRebaseNotes(SndBank* bank, s32 count)
{
    u32           base      = bank->spuAddr;
    SndBankLayer* bankLayer = bank->layers;

    while (--count != -1) {
        bankLayer->waveAddr += base;
        bankLayer++;
    }
}

static s32 SndLoad_Complete(SndLoadState* load)
{
    SndBank*  bank;
    MidiSong* song;
    s32       id;
    s32       ret;

    if (D_800689E8 == 6) {
        gSndLoadBankId = SOUND_LOAD_BANK_NONE;
        ret            = 0;
    } else {
        ret = -1;
        switch (load->payload.header.variant) {
            case 0:
                bank = load->bank;
                if (D_800689E8 != 0 || (id = bank->bankId) == SOUND_BANK_ID_FREE) {
                    gSndLoadBankId = SOUND_LOAD_BANK_NONE;
                } else {
                    id                 &= 0xFF;
                    song                = Midi_GetSlot(id);
                    song->sequenceId    = id;
                    song->sequenceBytes = (load->payload.header.imageBytes + 3) & 0xFFFC;
                    song->sequenceData  = load->imageBuffer;
                    song->bank          = bank;
                    song->waveBytes     = load->payload.header.waveBytes;
                    _sndBankRebaseNotes(bank, load->payload.header.noteCount);
                    Snd_BuildGroupIndex(song->bank);
                    ret               = 0;
                    gSndLoadBankId    = SOUND_LOAD_BANK_NONE;
                    load->bank        = 0;
                    load->imageBuffer = 0;
                }
                break;
            case 2:
                ret = SndBank_SetupFromLoad(load);
                if (ret == -1) {
                    SndHeap_Free(load->imageBuffer);
                }
                break;
            default:
                ret = -1;
                break;
        }
    }
    load->imageBuffer = 0;
    load->bank        = 0;
    return ret;
}

void SndLoad_FromSectorMode8(void* arg0)
{
    SndLoad_Init(8, arg0);
}

void SndLoad_BeginFromBuffer(u8 arg0, void* arg1)
{
    SndLoad_State.field_3 = arg0;
    D_8008212C            = D_80082122;
    D_80082121            = D_80082135;
    SndLoad_Init(0, arg1);
}

void SndLoad_Teardown(void)
{
    SndLoadState* temp;

    D_80082122 = D_8008212C;
    D_80082135 = D_80082121;
    temp       = &SndLoad_State;
    if (temp->field_2 != 6) {
        temp->field_2 = 8;
        SndHeap_Free(temp->imageBuffer);
        temp->imageBuffer = 0;
        Snd_FreeBank(temp->bank);
        temp->bank = 0;
    }
}

s32 SndLoad_FeedSector(void* arg0)
{
    SndLoadState* temp_s1;
    s32           temp_s0;

    if (D_80068A78 != 0) {
        return -1;
    }
    temp_s1 = &SndLoad_State;
    if (temp_s1->field_3 != 0) {
        temp_s1->field_10 = 0x800;
    } else {
        switch (temp_s1->field_2) {
            case 0:
            case 1:
            case 3:
                temp_s1->field_10 = 0x7F0;
                arg0              = (u8*)arg0 + 0x10;
                break;
            case 2:
            case 4:
            case 7:
                temp_s1->field_10 = 0x800;
                break;
            case 5:
                return 5;
            case 8:
                return 0;
        }
    }
    temp_s0 = SndLoad_ProcessSector(arg0);
    if (temp_s0 == 7) {
        return -1;
    }
    if (temp_s0 == 5) {
        SndLoad_Complete(temp_s1);
    }
    return temp_s0;
}

s32 SndLoad_FeedSectorOrError(void* arg0)
{
    s32 temp;

    temp = SndLoad_ProcessSector(arg0);
    if (temp == 7) {
        return -1;
    }
    return temp;
}

s32 SndBank_FinalizeLoad(SndLoadState* load)
{
    SndBank*      bank;
    MidiSong*     state;
    u16           index;
    s32           i;
    SndBankLayer* bankLayer;
    s32           base;
    void*         temp;
    s32           end;

    bank = load->bank;
    if (D_800689E8 == 0) {
        index = bank->bankId;
        if (index != SOUND_BANK_ID_FREE) {
            goto success;
        }
    }
    gSndLoadBankId = SOUND_LOAD_BANK_NONE;
    return -1;

success:
    index               &= 0xFF;
    state                = Midi_GetSlot(index);
    state->sequenceId    = index;
    state->sequenceBytes = (load->payload.header.imageBytes + 3) & 0xFFFC;
    temp                 = load->imageBuffer;
    state->bank          = bank;
    state->sequenceData  = temp;
    state->waveBytes     = load->payload.header.waveBytes;
    i                    = load->payload.header.noteCount;
    base                 = ((volatile SndBank*)bank)->spuAddr;
    bankLayer            = ((volatile SndBank*)bank)->layers;
    i                    = i - 1;
    if (i != -1) {
        end = -1;
        do {
            i                   -= 1;
            bankLayer->waveAddr += base;
            bankLayer++;
        } while (i != end);
    }
    Snd_BuildGroupIndex(state->bank);
    gSndLoadBankId    = SOUND_LOAD_BANK_NONE;
    load->bank        = 0;
    load->imageBuffer = 0;
    return 0;
}

static void* SndLoad_AllocBuffer(s32 arg0, s32 arg1, u32 arg2)
{
    u16 x;

    x = arg0;
    if ((arg1 & 0xFF) == 0) {
        return Midi_GetFixedBuffer(0, arg2 & 0xFFFF);
    }
    if (Snd_BankSlotsByType[x >> 12] == -1) {
        return 0;
    }
    switch (arg0 & SOUND_BANK_TYPE_MASK) {
        case 0x2000:
            if (arg2 < 0x210U) {
                arg2 = 0x210;
            }
            break;
        case 0xE000:
            if (arg2 < 0x168U) {
                arg2 = 0x168;
            }
            break;
    }
    return SndHeap_Malloc(arg2);
}

static s32 SndLoad_LookupMode(s32 arg0, s32 arg1, s32 arg2)
{
    s32 result;

    arg0  &= 3;
    result = 0;
    switch (arg0) {
        case 0:
            result = 0x1010;
            break;
        case 2:
            result = SndLoad_ResolveSpuAddr(arg2, arg1 & 0xFFFF);
            break;
    }
    return result;
}

static void SndLoad_Init(s32 arg0, void* arg1)
{
    SndLoadState* temp;
    s32           size;

    D_800689E8 = 0;
    temp       = &SndLoad_State;
    if (arg0 == 8) {
        size          = 0x800;
        temp->field_0 = arg0;
    } else {
        size          = 0x7F0;
        temp->field_0 = 0;
    }
    temp->field_10     = size;
    temp->field_2      = 0;
    temp->field_1      = 0;
    temp->sectorBuffer = arg1;
    temp->imageBuffer  = 0;
    temp->bank         = 0;
    temp->writeCursor  = 0;
    temp->field_C      = 0;
}

static s32 SndBank_FreeById(u16 arg0, s32 arg1)
{
    u16      x;
    u8       slot;
    s32      i;
    SndBank* base;
    SndBank* ptr;

    x = arg0;
    if ((arg1 & 0xFF) == 0) {
        return 0;
    }
    slot = Snd_BankSlotsByType[x >> 12];
    if ((s8)slot == -1) {
        return -1;
    }
    switch ((u32)(arg0 & SOUND_BANK_TYPE_MASK) >> 12) {
        case 4:
            i    = 4;
            base = Snd_Banks;
            ptr  = base + 4;
            do {
                if (ptr->bankId == x) {
                    return -1;
                }
                i++;
                ptr++;
            } while (i < 7);
            slot = D_80082122 + 4;
            break;
        case 0xF:
            break;
        default:
            if (Snd_Banks[(s8)slot].bankId == (arg0 & 0xFFFF)) {
                return -1;
            }
            break;
    }
    SndBankSlot_Free((s8)slot);
    return 0;
}
