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

// Every value of the channel selector has its own runtime control record.
enum { MIDI_CHANNEL_COUNT = MIDI_CHANNEL_STATUS_MASK + 1 };

/// Runtime controls for all sixteen MIDI channels of one song.
///
/// Channel indices are the status byte's low nibble (0..15), also retained by
/// active voice slots. Slot initialization and sequence startup reset the full
/// table; event handlers then update controls shared by that channel's notes.
/// The word view covers every record, including its uninterpreted byte, for
/// initialization with two 32-bit stores per channel.
typedef union {
    _MidiChannel entries[MIDI_CHANNEL_COUNT];                                   // Per-channel runtime controls, indexed 0..15
    u32          words[sizeof(_MidiChannel[MIDI_CHANNEL_COUNT]) / sizeof(u32)]; // Complete table representation as 32-bit reset words
} _MidiChannelTable;
STATIC_ASSERT_SIZEOF(_MidiChannelTable, 0x80);

// Track storage extent and the region-dependent denominator of fractional MIDI ticks.
enum {
    MIDI_TRACK_RETURN_CAPACITY       = 9,
    MIDI_TRACK_PAL_TICK_DIVISOR      = 6000,
    MIDI_TRACK_NTSC_TICK_DIVISOR     = 3600,
    MIDI_TRACK_INITIAL_TICK_FRACTION = MIDI_TRACK_NTSC_TICK_DIVISOR - 1
};

// Custom NRPN selectors: reverb depth, a saved loop start, and a counted loop jump.
enum {
    MIDI_TRACK_NRPN_IDLE         = 0,
    MIDI_TRACK_NRPN_REVERB_DEPTH = 0x10,
    MIDI_TRACK_NRPN_LOOP_START   = 0x14,
    MIDI_TRACK_NRPN_LOOP_END     = 0x1E,
    MIDI_TRACK_LOOP_FOREVER      = 0x7F
};

/// Playback state for one MIDI sequence track, including timing and custom control flow.
///
/// A song owns eighteen records; the sequence header must not exceed that count.
/// Sequence startup clears its active records.
/// Cursors borrow the loaded sequence buffer and remain valid until it is reused.
/// Startup saves each track's first delta cursor in the ninth return slot so the
/// next chunk can be located. Playback then reuses all nine slots for calls.
/// Valid streams call at depths 0..8 and return at depths 1..9; the signed depth
/// and its byte arithmetic retain the existing malformed-stream behavior.
/// The delta countdown uses MIDI ticks, not audio frames. The fractional tick
/// numerator is carried between updates with a region-dependent denominator.
typedef struct {
    u8 nrpnMsb;                                                  // CC 99 selector (0 cleared, 16 reverb depth, 20 loop start, 30 loop end)
    u8 nrpnLsb;                                                  // CC 98 selector (16 with MSB 16 applies reverb depth); cleared to 0
    u8 runningChannel;                                           // Last explicit channel (0..15), reused by implicit note-on data
    u8 implicitNoteOn;                                           // Last event form (0 explicit status, 1 implicit note-on); no reader
    u8 loopRepeatsLeft;                                          // Further controller-loop jumps (0 exhausted, 127 unbounded)
    u8 ended;                                                    // Playback gate (0 active, 1 end-of-track or explicitly stopped)
    u8 callLatched;                                              // Call latch (0 reset/negative-depth return, 1 call seen); no reader
    s8 callDepth;                                                // Occupied return slots (0..9 for valid streams); signed error check
    union {
        u8* returnAddresses[MIDI_TRACK_RETURN_CAPACITY];         // Delta cursors to resume after custom calls
        struct {
            u8* returnAddresses[MIDI_TRACK_RETURN_CAPACITY - 1]; // The same first eight return slots
            u8* dataStart;                                       // First delta cursor, aliasing the ninth return slot
        } startup;                                               // Track-chunk lookup before playback starts
    } savedCursors;                                              // Startup chunk cursor and playback return-stack views
    u8* eventCursor;                                             // Event cursor; handlers return the next delta or NULL on error
    u8* loopCursor;                                              // Delta cursor saved by the loop-start data entry and resumed at loop end
    s32 ticksUntilEvent;                                         // Remaining MIDI ticks; may become negative after the track ends
    s32 tickFraction;                                            // Tick numerator remainder (PAL modulo 6000, NTSC modulo 3600); starts 3599
} _MidiTrack;
STATIC_ASSERT_SIZEOF(_MidiTrack, 0x3C);

// Both selectors are invalidated when a note slot is reset or its voice released.
enum { MIDI_NOTE_SLOT_FREE = -1 };

/// One bank sample layer playing a MIDI note on an allocated SPU voice.
///
/// A song owns eighteen slots, indexed by SPU voice number (0..17). Each matching
/// bank layer gets its own slot, so one note may occupy several voices. Note-off
/// retains the slot through the release envelope; voice release or stealing
/// clears the record and sets both voice and channel to `MIDI_NOTE_SLOT_FREE`.
/// The SPU callback retains the slot's address while that voice is allocated.
/// Program and layer retain the original sample selection across program changes;
/// volume and pan updates combine the saved bank controls with live channel controls.
typedef struct {
    s8  voice;         // SPU voice number (0..17), or -1 when free
    s8  channel;       // MIDI channel (0..15), or -1 when free
    s8  key;           // MIDI note key (0..127); compared for note-off and used for pitch
    u8  velocity;      // Note-on velocity (1..127), indexing the velocity gain curve
    s8  volumeScale;   // Low signed byte of (program gain * layer gain) / 128; mixed / 127
    s8  pan;           // Combined program/layer pan, clamped to 0..127 before channel offset
    u8  program;       // Bank program/group index, below the loaded bank's groupCount
    u8  layer;         // Layer index within that program, below its layerCount
    s16 pitchOffset;   // Signed Q8 semitone offset after scaling the channel's pitch wheel
    s16 reverbEnabled; // Saved layer reverb switch (0 disabled, 1 enabled); no individual reader
} _MidiNoteSlot;
STATIC_ASSERT_SIZEOF(_MidiNoteSlot, 0xC);

// Playback phase stored in a song's status byte.
enum {
    MIDI_SONG_IDLE       = 0,    // Stopped; a loaded sequence can start
    MIDI_SONG_PLAYING    = 2,    // Tracks advance at the published tempo
    MIDI_SONG_STOPPING   = 4,    // Release voices and return to idle
    MIDI_SONG_MUTED      = 8,    // Tracks continue while gain falls to silence
    MIDI_SONG_UNMUTING   = 0x10, // Tracks continue while gain returns
    MIDI_SONG_FADING_IN  = 0x40, // Gain rises, then playback continues
    MIDI_SONG_FADING_OUT = 0x80, // Gain falls, then the sequence stops
    // Phases from which a mute fade is accepted.
    MIDI_SONG_MUTABLE = MIDI_SONG_PLAYING | MIDI_SONG_UNMUTING,
    // Playing, muted, or fading. Stopping and unmuting are excluded.
    MIDI_SONG_BUSY = MIDI_SONG_PLAYING | MIDI_SONG_MUTED | MIDI_SONG_FADING_IN | MIDI_SONG_FADING_OUT
};

// Tempo, channel-dirty mask, and the fixed track and voice capacities.
enum {
    MIDI_SONG_INITIAL_TEMPO_BPM  = 0xFF,     // Quarter notes per minute before a tempo event
    MIDI_MICROSECONDS_PER_MINUTE = 60000000, // SMF tempo: quarter notes per minute = this / microseconds per quarter
    MIDI_SONG_ALL_CHANNELS_DIRTY = 0xFFFF,   // Every channel bit in volumeDirtyChannels
    MIDI_SONG_TRACK_CAPACITY     = 18,       // Track records; the header count is not checked against this
    MIDI_SONG_VOICE_COUNT        = 18        // Voice slots, indexed by SPU voice 0..17
};

/// Resident MIDI sequence: its image, tempo, gain, tracks, channels and voices.
///
/// One record is resident. Its sequence id is 255 until a sequence is loaded;
/// command id 0 addresses every loaded sequence. The image is borrowed from the
/// loader and remains valid until that buffer is reused.
///
/// Tempo is quarter notes per minute, truncated to a byte. A tempo event writes
/// the pending byte, and playback publishes it after every track has advanced,
/// so the new tempo starts on the next update. The offset bytes are added to
/// that tempo and published with it; every writer stores zero.
///
/// Startup gain is (mix-table level * 3) << 5. A volume command replaces it
/// with the mix-table level times the requested gain. Playback zero-extends
/// the stored value. Sequence 0x5A ignores it and repeats the startup formula;
/// sequence 0x4F refreshes every voice on each track advance.
///
/// `groups` caches the bank's program table. `layers` caches its layer table
/// and has no reader; playback looks layers up through the bank.
typedef struct {
    u8                status;                            // Playback phase (MIDI_SONG_IDLE through MIDI_SONG_FADING_OUT)
    u8                sequenceId;                        // Loaded sequence id (255 none)
    u8                format;                            // SMF format low byte; stored, no reader
    u8                trackCount;                        // Active tracks; low byte of the SMF track count
    u8                currentTempoBpm;                   // Quarter notes per minute used while advancing tracks
    u8                currentTempoOffsetBpm;             // Added to the tempo; every writer stores 0
    u8                pendingTempoBpm;                   // Tempo published after this update's tracks advance
    u8                pendingTempoOffsetBpm;             // Offset published with the pending tempo; writers store 0
    s16               volumeScale;                       // Sequence gain, zero-extended and divided by 127 with master volume
    s16               sequenceBytes;                     // 4-byte-aligned image length in bytes; startup 16, no reader
    s32               volumeDirtyChannels;               // Bit per channel (0..15) whose playing voices need new volumes
    u8*               sequenceData;                      // Borrowed sequence image, header included
    LinInterp         volumeRamp;                        // Fade, mute and unmute gain (0 silence, 65535 unity)
    u8                unknown_24[0x10];                  // No individual access; role unproven
    s32               ticksPerQuarter;                   // SMF division, applied as MIDI ticks per quarter note
    s32               songTicks;                         // Sum of whole MIDI ticks each driven track advances; no other reader
    s32               waveBytes;                         // Cached sample-pool byte length; startup 16, no reader
    SndBank*          bank;                              // Program and layer tables for this sequence
    SndBankGroup*     groups;                            // Cached program table, indexed by the channel's program
    SndBankLayer*     layers;                            // Cached layer table; no reader
    _MidiTrack        tracks[MIDI_SONG_TRACK_CAPACITY];  // Sequence tracks; capacity is not enforced on the header count
    _MidiChannelTable channels;                          // Sixteen channel control records
    _MidiNoteSlot     voiceSlots[MIDI_SONG_VOICE_COUNT]; // One slot per SPU voice 0..17
} _MidiSong;
STATIC_ASSERT_SIZEOF(_MidiSong, 0x5DC);

/// Decoder for one class of MIDI track event, chosen by the status byte's high nibble.
///
/// `status` is the event's status byte: the high nibble is the event class
/// (0x8..0xF) and, for a channel event, the low nibble is the channel (0..15).
/// `event` addresses that byte in the song's borrowed sequence image, so the
/// event's data starts at `event[1]`. Where the stream omits the status byte,
/// the caller supplies one and passes the address one byte before the data,
/// which keeps those offsets. `song` holds the channel controls, voice slots
/// and tempo the event acts on; `track` is the track being advanced, for
/// events that change its own control flow.
///
/// Returns the cursor the track's next delta time is read from, which a loop,
/// call or return places elsewhere in the image, or `NULL` when the event
/// cannot be decoded, which stops the song.
typedef u8* (*_MidiEventHandler)(s32 status, u8* event, _MidiSong* song, _MidiTrack* track);

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

static _MidiSong Midi_Song;

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

static _MidiEventHandler Midi_EventFns[];

static volatile s32 D_800689E8;

static u8 D_800689F0[];

extern s32 func_map_neo_ark_80179BE4(u32 arg0, u8 arg1, LinInterp* ramp);

static void _sndEvtRelease(SndEvt* event);

static void _sndEvtHandleNoOp(SndEvt* event);

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

static _MidiSong* Midi_GetSlot(s32 unused);

static void* Midi_GetFixedBuffer(s32 unused1, s32 unused2);

static void Midi_ClearVoiceEntry(void* context);

static void Midi_InitSlot(s32 arg0);

/* Returns where the event data of track `arg1` starts. Every chunk is an 8-byte
 * id/length header followed by `length` bytes, so track 0 follows the file
 * header chunk at `arg2`, and each later track follows the one before it,
 * whose data pointer must already be set. */
static u8* Midi_ResolveTrackData(_MidiSong* song, s32 arg1, u8* arg2);

static void _midiEndTracks(_MidiSong* song);

static void Midi_KeyOffVoices(_MidiSong* song);

static void Midi_DriveTrack(_MidiSong* song, _MidiTrack* track);

static void Midi_UpdateVoiceVolumes(_MidiSong* song);

/* Note off: keys off every voice slot playing this channel's key, unless the
 * channel's noteEventsDisabled flag is set. A note on with zero velocity is a note off
 * too, and its event is one byte longer. Returns the cursor past the event. */
static inline u8* _midiNoteOff(s32 status, u8* data, _MidiSong* song);

static u8* Midi_Event1(s32 arg0, u8* arg1, _MidiSong* song, _MidiTrack* unused);

static u8* Midi_Event3(s32 arg0, u8* arg1, _MidiSong* song, _MidiTrack* track);

static inline s32 _midiReadVlq(const u8* data, u8* byteCount);

static u8* Midi_HandleMetaSysex(s32 unused1, u8* arg1, _MidiSong* song, _MidiTrack* track);

static s32 _midiReadDeltaTime(const u8* data, u8* byteCount);

static void Midi_InitChannelTable(_MidiChannelTable* channels);

static u8* _midiHandleUnsupportedPressure(s32 status, u8* event, _MidiSong* song, _MidiTrack* track);

static u8* Midi_KeyOffChannel(s32 arg0, u8* arg1, _MidiSong* song, _MidiTrack* unused);

static u8* Midi_SetProgram(s32 arg0, u8* arg1, _MidiSong* song, _MidiTrack* unused);

static u8* Midi_PitchBend(s32 arg0, u8* arg1, _MidiSong* song, _MidiTrack* unused);

static s32 SndBank_SetupFromLoad(SndLoadState* load);

static inline void _sndBankRebaseLayerWaveAddresses(SndBank* bank, s32 layerCount);

static s32 SndLoad_Complete(SndLoadState* load);

static void* SndLoad_AllocBuffer(s32 arg0, s32 arg1, u32 arg2);

static s32 SndLoad_LookupMode(s32 arg0, s32 arg1, s32 arg2);

static void SndLoad_Init(s32 arg0, void* arg1);

static s32 SndBank_FreeById(u16 arg0, s32 arg1);

void (*SndEvt_Handlers[])(SndEvt*) = {
    _sndEvtHandleNoOp,            // SOUND_EVENT_NO_OP
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
    _sndEvtHandleNoOp,            // SOUND_EVENT_RESERVED_NO_OP
    SndEvt_HandleRefCountInc,     // SOUND_EVENT_SCRIPT_DUCK_ACQUIRE
    SndEvt_HandleRefCountDec,     // SOUND_EVENT_SCRIPT_DUCK_RELEASE
    SndEvt_HandleKeyOffMatching,  // SOUND_EVENT_SCRIPT_KEY_OFF
};

static _MidiEventHandler Midi_EventFns[] = {
    Midi_KeyOffChannel,
    Midi_Event1,
    _midiHandleUnsupportedPressure,
    Midi_Event3,
    Midi_SetProgram,
    _midiHandleUnsupportedPressure,
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
        _sndEvtRelease(event);
        if (nextEvent == NULL) {
            _gSndEvtTail = NULL;
            _gSndEvtHead = NULL;
            break;
        }
        _gSndEvtHead = nextEvent;
    } while (nextEvent != NULL);
}

/// Clears every reservation, payload and link in the resident sound-event pool.
static inline void _sndEvtClearPool(void)
{
    u32  wordIndex;
    s32* poolWord;

    poolWord  = (s32*)_gSndEvtPool;
    wordIndex = 0;
    do {
        *poolWord = 0;
        wordIndex++;
        poolWord++;
    } while (wordIndex < sizeof(_gSndEvtPool) / sizeof(*poolWord));
}

void sndEvtReset(void)
{
    _sndEvtClearPool();
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

/// Releases a sound-event pool reservation, retaining its command and arguments.
///
/// `event` is a processed pool slot, or `NULL`, which is ignored. The caller
/// saves any queue link it needs and updates the queue endpoints separately.
/// Clearing the slot's links and allocation marker makes it reusable immediately.
static void _sndEvtRelease(SndEvt* event)
{
    if (event != NULL) {
        event->allocated = SOUND_EVENT_SLOT_FREE;
        event->prev      = NULL;
        event->next      = NULL;
    }
}

/// Ignores the default-reservation and reserved sound-event no-op opcodes.
///
/// Dispatch slots `SOUND_EVENT_NO_OP` (0) and `SOUND_EVENT_RESERVED_NO_OP` (12)
/// share this handler; slot 0 is the default assigned to new reservations.
/// `event` is unused and remains reserved until the dispatcher releases it.
static void _sndEvtHandleNoOp(SndEvt* event)
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
        sndScriptRampMix(temp_v0, args->panOffset, args->level.attenuation);
    }
}

static void SndEvt_HandleVolumeRamp(SndEvt* event)
{
    s32               temp_v0;
    SndEvtScriptArgs* args;

    args    = &event->args.script;
    temp_v0 = SndVoice_FindById(args->soundId);
    if (temp_v0 >= 0) {
        sndScriptRampVolume(temp_v0, args->level.volumeScale);
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
    s32        i;
    _MidiSong* song;
    SndBank*   bank;

    for (i = 0; i <= 0; i++) {
        Midi_InitSlot(i & 0xFF);
    }
    D_8007F2F0 = 0x40;
    D_800820E9 = 0;
    D_800820E0 = 0;
    D_800820E4 = 0;
    Spu_SetVoiceRange(0, 0, 0x10);
    song                = Midi_GetSlot(SOUND_EVENT_MIDI_INVALID_SEQUENCE);
    song->sequenceId    = SOUND_EVENT_MIDI_INVALID_SEQUENCE;
    song->sequenceBytes = 0x10;
    song->sequenceData  = D_8007F8E0;
    do {
        bank                  = &Snd_Banks[Snd_BankSlotsByType[15]];
        song->bank            = bank;
        bank->bankId          = 0xF0FF;
        song->bank->heapBlock = SndHeap_Malloc(SOUND_BANK_SEQUENCE_TABLE_BYTES);
    } while (0);
    song->bank->groups          = song->bank->heapBlock;
    song->bank->layers          = song->bank->heapBlock;
    song->bank->groupFirstLayer = song->bank->heapBlock;
    Snd_SequenceBankBuffer      = song->bank->heapBlock;
    song->waveBytes             = 0x10;
    return -1;
}

static s32 Midi_InitSequence(u8 arg0, u16 arg1)
{
    s32         i;
    s32         j;
    _MidiSong*  song;
    _MidiTrack* tracks;
    _MidiTrack* track;
    u8*         data;
    u8*         trackPtr;
    s32*        clearPtr;
    u8          len;

    i = 0;
    do {
        song = &Midi_Song + i;
        if (song->sequenceId != SOUND_EVENT_MIDI_INVALID_SEQUENCE) {
            if ((song->sequenceId == arg0) && (song->status == MIDI_SONG_IDLE)) {
                Midi_InitChannelTable(&song->channels);
                data = song->sequenceData;
                if (((data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3]) != 0x4D546864) {
                    return -1;
                }

                tracks           = song->tracks;
                song->format     = data[9];
                clearPtr         = (s32*)tracks;
                song->trackCount = data[0xB];

                // Clear each complete active record through its 32-bit representation.
                for (j = 0; j < song->trackCount * (sizeof(_MidiTrack) / sizeof(s32)); j++) {
                    *clearPtr++ = 0;
                }

                if (song->trackCount != 0) {
                    j     = 0;
                    track = tracks;
                    do {
                        trackPtr                              = Midi_ResolveTrackData(song, j & 0xFF, song->sequenceData);
                        track->savedCursors.startup.dataStart = trackPtr;
                        track->eventCursor                    = trackPtr;
                        if ((trackPtr < D_8007F8E0) || (trackPtr >= (u8*)&D_800820E0)) {
                            return -1;
                        }
                        track->ticksUntilEvent = _midiReadDeltaTime(trackPtr, &len);
                        track->eventCursor    += len;
                        track->tickFraction    = MIDI_TRACK_INITIAL_TICK_FRACTION;
                        j++;
                        track++;
                    } while (j < song->trackCount);
                }

                song->groups                = song->bank->groups;
                song->layers                = song->bank->layers;
                song->ticksPerQuarter       = (data[0xC] << 8) | data[0xD];
                song->pendingTempoBpm       = MIDI_SONG_INITIAL_TEMPO_BPM;
                song->currentTempoBpm       = MIDI_SONG_INITIAL_TEMPO_BPM;
                song->pendingTempoOffsetBpm = 0;
                song->currentTempoOffsetBpm = 0;
                song->volumeScale           = (D_800689F0[song->sequenceId] * 3) << 5;
                LinInterp_Setup(&song->volumeRamp, 0, D_8007F2F0, arg1);

                if (arg1 != 0) {
                    song->status = MIDI_SONG_FADING_IN;
                } else {
                    song->status = MIDI_SONG_PLAYING;
                }

                song->volumeDirtyChannels = MIDI_SONG_ALL_CHANNELS_DIRTY;
                song->songTicks           = 0;
                for (j = 0; j < ARRAY_SIZE(song->voiceSlots); j++) {
                    Midi_ClearVoiceEntry(&song->voiceSlots[j]);
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
    _MidiSong* song;
    s32        i;
    s32        j;

    for (i = 0; i < 1; i++) {
        song = &Midi_Song + i;
        if (song->sequenceId == SOUND_EVENT_MIDI_INVALID_SEQUENCE) {
            break;
        }
        switch (song->status) {
            case MIDI_SONG_IDLE:
                break;
            case MIDI_SONG_FADING_IN:
            case MIDI_SONG_FADING_OUT:
                if (song->volumeRamp.gain == song->volumeRamp.targetGain) {
                    if (song->status == MIDI_SONG_FADING_IN) {
                        song->status = MIDI_SONG_PLAYING;
                    } else {
                        song->status = MIDI_SONG_STOPPING;
                        goto stop;
                    }
                }
                /* fallthrough */
            case MIDI_SONG_MUTED:
                LinInterp_Step(&song->volumeRamp);
                song->volumeDirtyChannels = MIDI_SONG_ALL_CHANNELS_DIRTY;
                /* fallthrough */
            case MIDI_SONG_PLAYING:
            play:
                for (j = 0; j < song->trackCount; j++) {
                    if (song->tracks[j].ended == false) {
                        Midi_DriveTrack(song, &song->tracks[j]);
                    }
                }
                // The tempo event's pending bytes apply on the next update.
                song->currentTempoBpm       = song->pendingTempoBpm;
                song->currentTempoOffsetBpm = song->pendingTempoOffsetBpm;
                break;
            case MIDI_SONG_STOPPING:
            stop:
                _midiEndTracks(song);
                Midi_KeyOffVoices(song);
                song->status = MIDI_SONG_IDLE;
                break;
            case MIDI_SONG_UNMUTING:
                if (song->status == MIDI_SONG_MUTED && song->volumeRamp.gain >= song->volumeRamp.targetGain) {
                    song->volumeDirtyChannels = MIDI_SONG_ALL_CHANNELS_DIRTY;
                    song->status              = MIDI_SONG_PLAYING;
                    goto play;
                }
                LinInterp_Step(&song->volumeRamp);
                song->volumeDirtyChannels = MIDI_SONG_ALL_CHANNELS_DIRTY;
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
    if (arg0 == SOUND_EVENT_MIDI_INVALID_SEQUENCE) {
        return 0;
    }
    for (i = 0; i <= 0; i++) {
        if ((arg0 == (&Midi_Song)[i].sequenceId) || (arg0 == SOUND_EVENT_MIDI_ALL_SEQUENCES)) {
            if ((&Midi_Song)[i].status & MIDI_SONG_BUSY) {
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
    if (arg0 == SOUND_EVENT_MIDI_INVALID_SEQUENCE) {
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
    s32        i;
    _MidiSong* song;

    for (i = 0; i <= 0; i++) {
        song = &Midi_Song + i;
        if ((arg0 == song->sequenceId) || (arg0 == SOUND_EVENT_MIDI_ALL_SEQUENCES)) {
            if (song->status == MIDI_SONG_PLAYING) {
                song->status = MIDI_SONG_FADING_OUT;
                LinInterp_Setup(&song->volumeRamp, D_8007F2F0, 0, arg1);
            } else {
                song->status = MIDI_SONG_STOPPING;
            }
        }
    }
}

static void Midi_FadeVolume(u8 arg0, s32 arg1)
{
    s32        i;
    _MidiSong* song;

    for (i = 0; i <= 0; i++) {
        song = &Midi_Song + i;
        if ((arg0 == song->sequenceId) || (arg0 == SOUND_EVENT_MIDI_ALL_SEQUENCES)) {
            if (arg1 == 0) {
                if (song->status == MIDI_SONG_MUTED) {
                    song->status = MIDI_SONG_UNMUTING;
                    LinInterp_Setup(&song->volumeRamp, 0, D_8007F2F0, 8);
                }
            } else {
                if (song->status & MIDI_SONG_MUTABLE) {
                    song->status = MIDI_SONG_MUTED;
                    LinInterp_Setup(&song->volumeRamp, D_8007F2F0, 0, 8);
                }
            }
        }
    }
}

static void Midi_SetVolumeScale(u8 arg0, u8 arg1)
{
    s32        i;
    u8*        table;
    _MidiSong* song;
    s32        product;

    i    = 0;
    song = &Midi_Song;
    for (; i <= 0; i++) {
        if ((arg0 == song[i].sequenceId) || (arg0 == SOUND_EVENT_MIDI_ALL_SEQUENCES)) {
            table                       = D_800689F0;
            product                     = table[song[i].sequenceId] * arg1;
            song[i].volumeDirtyChannels = MIDI_SONG_ALL_CHANNELS_DIRTY;
            song[i].volumeScale         = product;
        }
    }
}

void midiSetMasterVolume(s32 volume)
{
    s32 songIndex;
    s32 dirtyChannels;
    u8* masterVolume;

    masterVolume = &D_8007F2F0;
    if ((s8)volume >= 0) {
        *masterVolume = volume;
    } else {
        *masterVolume = SOUND_EVENT_MIDI_VOLUME_FULL;
    }

    // Refresh every channel of the resident song on the next volume update.
    songIndex     = 0;
    dirtyChannels = MIDI_SONG_ALL_CHANNELS_DIRTY;
    for (; songIndex <= 0; songIndex++) {
        (&Midi_Song)[songIndex].volumeDirtyChannels = dirtyChannels;
    }
}

s32 midiGetMasterVolume(void)
{
    return D_8007F2F0;
}

static _MidiSong* Midi_GetSlot(s32 unused)
{
    if (Midi_Song.status != MIDI_SONG_IDLE) {
        _midiEndTracks(&Midi_Song);
        Midi_Song.status = MIDI_SONG_STOPPING;
    }
    return &Midi_Song;
}

static void* Midi_GetFixedBuffer(s32 unused1, s32 unused2)
{
    return D_8007F8E0;
}

static void Midi_ClearVoiceEntry(void* context)
{
    _MidiNoteSlot* slot = context;
    u32            wordIndex;
    s32*           words;

    words     = (s32*)slot;
    wordIndex = 0;
    do {
        *words = 0;
        wordIndex++;
        words++;
    } while (wordIndex < sizeof(*slot) / sizeof(*words));
    slot->channel = MIDI_NOTE_SLOT_FREE;
    slot->voice   = MIDI_NOTE_SLOT_FREE;
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

/// Zeroes `slot` and marks it free.
static inline void Midi_ResetNoteSlot(_MidiNoteSlot* slot)
{
    s32* slotWords;
    u32  slotWordIndex;

    slotWords     = (s32*)slot;
    slotWordIndex = 0;
    do {
        *slotWords = 0;
        slotWordIndex++;
        slotWords++;
    } while (slotWordIndex < sizeof(*slot) / sizeof(*slotWords));
    slot->channel = MIDI_NOTE_SLOT_FREE;
    slot->voice   = MIDI_NOTE_SLOT_FREE;
}

static void Midi_InitSlot(s32 arg0)
{
    _MidiSong* song;
    s32*       p;
    u32        i;

    arg0 &= 0xFF;
    song  = &(&Midi_Song)[arg0];

    p = (s32*)song;
    i = 0;
    do {
        *p = 0;
        i++;
        p++;
    } while (i < sizeof(*song) / sizeof(*p));

    LinInterp_Setup(&song->volumeRamp, 0, 0, 0);
    Midi_InitChannelTable(&song->channels);

    for (i = 0; (s32)i < ARRAY_SIZE(song->voiceSlots); i++) {
        Midi_ResetNoteSlot(&song->voiceSlots[i]);
    }
}

/// Reads the big-endian 32-bit value at `p`, the form every length in a
/// Standard MIDI File takes.
#define MIDI_READ_BE32(p) (((p)[0] << 24) | ((p)[1] << 16) | ((p)[2] << 8) | (p)[3])

/* Returns where the event data of track `arg1` starts. Every chunk is an 8-byte
 * id/length header followed by `length` bytes, so track 0 follows the file
 * header chunk at `arg2`, and each later track follows the one before it,
 * whose data pointer must already be set. */
static u8* Midi_ResolveTrackData(_MidiSong* song, s32 arg1, u8* arg2)
{
    u32 len;

    if ((u8)arg1 != 0) {
        arg2 = song->tracks[(u8)arg1 - 1].savedCursors.startup.dataStart;
        len  = MIDI_READ_BE32(arg2 - 4);
        return arg2 + len + 8;
    }
    len = MIDI_READ_BE32(arg2 + 4);
    return arg2 + 8 + len + 8;
}

/// Marks every active track ended so the song's tracks stop advancing.
///
/// `song->trackCount` must fit its track array. Cursors and timing are retained;
/// releasing the playing voices is a separate operation.
static void _midiEndTracks(_MidiSong* song)
{
    s32 trackIndex;

    for (trackIndex = 0; trackIndex < song->trackCount; trackIndex++) {
        song->tracks[trackIndex].ended = true;
    }
}

static void Midi_KeyOffVoices(_MidiSong* song)
{
    s32            i;
    _MidiNoteSlot* slot;
    u8             status;
    SpuVoiceRef    voiceRef;
    u16            temp;

    i    = 0;
    slot = song->voiceSlots;
    do {
        if (slot->voice >= 0) {
            status = Spu_GetVoiceStatus(slot->voice);
            if (status != 0) {
                Spu_GetVoiceRef(slot->voice, &voiceRef);
                temp                 = voiceRef.attr->adsr2;
                temp                 = (temp & 0xFFE0) | 5;
                voiceRef.attr->adsr2 = temp;
                voiceRef.attr->mask |= SPU_VOICE_ADSR_ADSR2;
                if (status != 2) {
                    Spu_KeyOff(slot->voice);
                }
            }
        }
        i++;
        slot++;
    } while (i < ARRAY_SIZE(song->voiceSlots));
}

static void Midi_DriveTrack(_MidiSong* song, _MidiTrack* track)
{
    u8  len;
    u32 temp;
    s32 ticks;
    s32 quot;
    u32 rem;
    u8  status;

    // Carry the fractional numerator while advancing by whole MIDI ticks.
    temp = track->tickFraction + (song->currentTempoBpm + song->currentTempoOffsetBpm) * song->ticksPerQuarter;
    if (gDisplayState.region == MODE_PAL) {
        quot = temp / MIDI_TRACK_PAL_TICK_DIVISOR;
    } else {
        quot = temp / MIDI_TRACK_NTSC_TICK_DIVISOR;
    }
    if (gDisplayState.region == MODE_PAL) {
        rem = temp % MIDI_TRACK_PAL_TICK_DIVISOR;
    } else {
        rem = temp % MIDI_TRACK_NTSC_TICK_DIVISOR;
    }
    track->tickFraction = rem;
    song->songTicks    += quot;
    ticks               = quot;
    if (song->sequenceId == 0x4F) {
        song->volumeDirtyChannels = MIDI_SONG_ALL_CHANNELS_DIRTY;
    }
    while (ticks >= track->ticksUntilEvent) {
        ticks                 -= track->ticksUntilEvent;
        track->ticksUntilEvent = 0;
        do {
            status = *track->eventCursor;
            if (status & 0x80) {
                track->implicitNoteOn = false;
                if ((status & 0xF0) != 0xF0) {
                    track->runningChannel = status & MIDI_CHANNEL_STATUS_MASK;
                }
                track->eventCursor = Midi_EventFns[((status & 0xF0) >> 4) - 8](status, track->eventCursor, song, track);
            } else {
                track->implicitNoteOn = true;
                track->eventCursor    = Midi_EventFns[1](track->runningChannel | 0x90, track->eventCursor - 1, song, track);
            }
            if (track->ended != false) {
                goto end;
            }
            if (track->eventCursor == NULL) {
                track->nrpnMsb      = MIDI_TRACK_NRPN_IDLE;
                track->tickFraction = 0;
                song->status        = MIDI_SONG_STOPPING;
                return;
            }
            track->ticksUntilEvent = _midiReadDeltaTime(track->eventCursor, &len);
            track->eventCursor    += len;
        } while (track->ticksUntilEvent == 0);
    }
end:
    track->ticksUntilEvent -= ticks;
}

static void Midi_UpdateVoiceVolumes(_MidiSong* song)
{
    // The velocity curve reaches 16383; retain a 0..127 channel gain before
    // combining it with the note's gain and the song's SPU volume.
    enum {
        MIDI_VELOCITY_GAIN_FULL   = 0x3FFF,
        MIDI_CHANNEL_GAIN_DIVISOR = SOUND_EVENT_MIDI_VOLUME_FULL * MIDI_VELOCITY_GAIN_FULL,
        MIDI_VOICE_GAIN_DIVISOR   = SOUND_EVENT_MIDI_VOLUME_FULL * SOUND_EVENT_MIDI_VOLUME_FULL
    };
    SpuVoiceRef    voiceRef;
    s16            sp18[2];
    LinInterp*     interp;
    s32            volume;
    s32            i;
    _MidiNoteSlot* slot;
    _MidiChannel*  channelControls;
    s32            product;
    u32            vol;
    s32            channel;
    s32            mask;
    s32            pan;
    s8             voice;

    interp = &song->volumeRamp;
    if (song->sequenceId == 0x4F && D_80082120 == 5) {
        volume = func_map_neo_ark_80179BE4((u16)song->volumeScale, D_80082136, interp);
    } else if (song->sequenceId == 0x5A) {
        volume = LinInterp_Apply(interp, (u32)((midiGetMasterVolume() & 0xFF) * ((D_800689F0[0x5A] * 3) << 5)) / 127U);
    } else {
        volume = LinInterp_Apply(interp, (u32)((midiGetMasterVolume() & 0xFF) * (u16)song->volumeScale) / 127U);
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
                Spu_GetVoiceRef(voice, &voiceRef);
                if (D_800820E9 == 1 && song->sequenceId != 0x5A) {
                    voiceRef.attr->volume.left  = 0;
                    voiceRef.attr->volume.right = 0;
                } else {
                    voiceRef.attr->volume.left  = sp18[0];
                    voiceRef.attr->volume.right = sp18[1];
                }
                voiceRef.attr->volmode.left  = 0;
                voiceRef.attr->volmode.right = 0;
                voiceRef.attr->mask         |= 0xF;
            }
        }
        i++;
        slot++;
    } while (i < ARRAY_SIZE(song->voiceSlots));
}

/* Note off: keys off every voice slot playing this channel's key, unless the
 * channel's noteEventsDisabled flag is set. A note on with zero velocity is a note off
 * too, and its event is one byte longer. Returns the cursor past the event. */
static inline u8* _midiNoteOff(s32 status, u8* data, _MidiSong* song)
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
    for (i = 0; i < ARRAY_SIZE(song->voiceSlots); i++) {
        if ((song->voiceSlots[i].key == key) && (song->voiceSlots[i].channel == channel)) {
            Spu_KeyOff(song->voiceSlots[i].voice);
        }
    }
    return ptr + 2;
}

static u8* Midi_Event1(s32 arg0, u8* arg1, _MidiSong* song, _MidiTrack* unused)
{
    enum {
        SOUND_BANK_VOLUME_FRACTION_BITS = 7,
        SOUND_BANK_PAN_CENTER           = 64,
        SOUND_BANK_PAN_MAX              = 127
    };
    s16            priorities[2];
    SpuVoiceRef    ref;
    u8             channel;
    u8             program;
    u8             key;
    u8             velocity;
    u8             layer;
    s8             voice;
    u16            priority;
    s32            i;
    s16            pan;
    s32            reverb;
    s32            bend;
    s32            product;
    s32            scale;
    SndBankGroup*  group;
    SndBankLayer*  bankLayer;
    _MidiNoteSlot* slot;
    SpuVoiceAttr*  attr;

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
                        slot->reverbEnabled = reverb;
                    } else {
                        Spu_DisableReverbVoice(slot->voice);
                        slot->reverbEnabled = SPU_OFF;
                    }
                    bend = song->channels.entries[channel].pitchBend;
                    if (bend != 0) {
                        if (bend > 0) {
                            scale = bankLayer->bendUp;
                        } else {
                            scale = bankLayer->bendDown;
                        }
                        product           = (scale << MIDI_PITCH_FRACTION_BITS) * bend;
                        slot->pitchOffset = product / MIDI_PITCH_BEND_MAX;
                    }
                    attr        = ref.attr;
                    attr->addr  = bankLayer->waveAddr;
                    attr->adsr1 = bankLayer->adsr1;
                    attr->adsr2 = bankLayer->adsr2;
                    attr->pitch = Spu_CalcVolume(key, slot->pitchOffset, bankLayer->rootKey, bankLayer->fineTune);
                    attr->mask  = SPU_VOICE_WDSA | SPU_VOICE_ADSR_ADSR1 | SPU_VOICE_ADSR_ADSR2 | SPU_VOICE_PITCH;
                    Spu_KeyOn(slot->voice);
                }
            }
        }
        arg1 += 3;
    }
    return arg1;
}

static u8* Midi_Event3(s32 arg0, u8* arg1, _MidiSong* song, _MidiTrack* track)
{
    enum {
        MIDI_TRACK_CONTROL_DATA_ENTRY   = 6,
        MIDI_TRACK_CONTROL_NRPN_LSB     = 0x62,
        MIDI_TRACK_CONTROL_NRPN_MSB     = 0x63,
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
        case MIDI_TRACK_CONTROL_DATA_ENTRY:
            status = track->nrpnMsb;
            if (status != MIDI_TRACK_NRPN_REVERB_DEPTH) {
                if (status != MIDI_TRACK_NRPN_LOOP_START) {
                    return arg1 + 3;
                }
                if (track->loopRepeatsLeft != 0) {
                    return arg1 + 3;
                }
                // Count later jumps from the delta immediately after this data entry.
                track->loopCursor = arg1 + 3;
                if ((s8)arg1[2] >= 0) {
                    track->loopRepeatsLeft = arg1[2];
                } else {
                    track->loopRepeatsLeft = MIDI_TRACK_LOOP_FOREVER;
                }
                track->nrpnMsb = MIDI_TRACK_NRPN_IDLE;
            } else {
                if (track->nrpnLsb != status) {
                    return arg1 + 3;
                }
                Spu_SetReverbDepth((s16)(arg1[2] << 8));
                track->nrpnMsb = MIDI_TRACK_NRPN_IDLE;
                track->nrpnLsb = MIDI_TRACK_NRPN_IDLE;
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

        case MIDI_TRACK_CONTROL_NRPN_LSB:
            track->nrpnLsb = arg1[2];
            break;

        case MIDI_TRACK_CONTROL_NRPN_MSB:
            value          = arg1[2];
            track->nrpnMsb = value;
            if ((value & 0xFF) == MIDI_TRACK_NRPN_LOOP_START) {
                break;
            }
            if ((value & 0xFF) != MIDI_TRACK_NRPN_LOOP_END) {
                return arg1 + 3;
            }
            if (track->loopRepeatsLeft < MIDI_TRACK_LOOP_FOREVER) {
                if (track->loopRepeatsLeft == 0) {
                    track->loopRepeatsLeft = 0;
                    break;
                }
                track->loopRepeatsLeft = track->loopRepeatsLeft - 1;
            }
            return track->loopCursor;

        default:
            return arg1 + 3;
    }

    return arg1 + 3;
}

/// Decodes a MIDI variable-length quantity and reports its encoded byte length.
///
/// `data` borrows a readable stream containing a terminating byte; `byteCount`
/// supplies one writable byte. Valid MIDI quantities occupy one to four bytes
/// and return 0..0x0FFFFFFF. No stream bound or length limit is checked, and the
/// stored count retains byte truncation for longer input.
static inline s32 _midiReadVlq(const u8* data, u8* byteCount)
{
    enum {
        MIDI_VLQ_PAYLOAD_BITS = 7,
        MIDI_VLQ_PAYLOAD_MASK = 0x7F,
        MIDI_VLQ_CONTINUATION = 0x80
    };
    s32 result;

    result     = 0;
    *byteCount = 0;
    do {
        result   <<= MIDI_VLQ_PAYLOAD_BITS;
        result    |= *data & MIDI_VLQ_PAYLOAD_MASK;
        *byteCount = *byteCount + 1;
    } while (*data++ & MIDI_VLQ_CONTINUATION);
    return result;
}

static u8* Midi_HandleMetaSysex(s32 unused1, u8* arg1, _MidiSong* song, _MidiTrack* track)
{
    enum {
        MIDI_TRACK_COMMAND_CALL   = 0xF5,
        MIDI_TRACK_COMMAND_RETURN = 0xF6,
        MIDI_TRACK_META_END       = 0x2F
    };
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
        case MIDI_TRACK_COMMAND_CALL:
            // Save the return delta, then jump relative to the end of the call command.
            if (track->callDepth < ARRAY_SIZE(track->savedCursors.returnAddresses)) {
                track->callLatched                                    = true;
                track->savedCursors.returnAddresses[track->callDepth] = var_t0 + 3;
                track->callDepth                                      = (u8)track->callDepth + 1;
                var_t0 =
                    var_t0 + ((s16)((var_t0[1] << 8) | var_t0[2]) + 3);
            } else {
                var_t0 = NULL;
            }
            break;
        case MIDI_TRACK_COMMAND_RETURN:
            if (track->callDepth < 0) {
                track->callLatched = false;
                var_t0             = NULL;
            } else {
                temp_v0          = (u8)track->callDepth - 1;
                track->callDepth = temp_v0;
                var_t0           = track->savedCursors.returnAddresses[temp_v0];
            }
            break;
        case 0xF7:
            goto f7_body;
        case 0xFF:
            var_t0 += 1;
            temp_v1 = *var_t0;
            if (temp_v1 == MIDI_TRACK_META_END) {
                goto eot;
            }
            if (temp_v1 == 0x51) {
                goto tempo;
            }
            goto vlq;
        eot:
            track->ended = true;
        f7_body:
            var_t0 += 1;
            break;
        tempo: {
            u32 tempo_val;
            tempo_val  = var_t0[2] << 16;
            tempo_val |= var_t0[3] << 8;
            tempo_val |= var_t0[4];
            var_t0    += 5;
            // Quarter notes per minute, truncated to a byte on store.
            song->pendingTempoOffsetBpm = 0;
            song->pendingTempoBpm       = MIDI_MICROSECONDS_PER_MINUTE / tempo_val;
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

/// Reads a track delta in MIDI ticks and writes its encoded byte count.
///
/// `data` borrows the loaded sequence image; it and `byteCount` must satisfy
/// `_midiReadVlq`'s readable-stream and writable-output requirements.
static s32 _midiReadDeltaTime(const u8* data, u8* byteCount)
{
    return _midiReadVlq(data, byteCount);
}

static void Midi_InitChannelTable(_MidiChannelTable* channels)
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

/// Advances past only the status byte of an unsupported MIDI pressure event.
///
/// Handles classes 0xA (polyphonic pressure) and 0xD (channel pressure).
/// `event` borrows the sequence image and addresses the status byte. Returns
/// `event + 1` without consuming pressure data; the driver reads its next delta
/// there. `status`, `song` and `track` are unused.
static u8* _midiHandleUnsupportedPressure(s32 status, u8* event, _MidiSong* song, _MidiTrack* track)
{
    return event + 1;
}

static u8* Midi_KeyOffChannel(s32 arg0, u8* arg1, _MidiSong* song, _MidiTrack* unused)
{
    return _midiNoteOff(arg0, arg1, song);
}

static u8* Midi_SetProgram(s32 arg0, u8* arg1, _MidiSong* song, _MidiTrack* unused)
{
    song->channels.entries[arg0 & MIDI_CHANNEL_STATUS_MASK].program = arg1[1];
    return arg1 + 2;
}

static u8* Midi_PitchBend(s32 arg0, u8* arg1, _MidiSong* song, _MidiTrack* unused)
{
    enum { MIDI_PITCH_WHEEL_CENTER = 0x2000 };
    SpuVoiceRef    voiceRef;
    u8             channel;
    s32            i;
    s16            pitchBend;
    _MidiNoteSlot* slot;
    SndBankLayer*  bankLayer;
    s32            scale;
    s16            pitch;
    SpuVoiceAttr*  attr;

    channel                                   = arg0 & MIDI_CHANNEL_STATUS_MASK;
    i                                         = 0;
    pitchBend                                 = (arg1[1] | (arg1[2] << 7)) - MIDI_PITCH_WHEEL_CENTER;
    song->channels.entries[channel].pitchBend = pitchBend;
    do {
        slot = &song->voiceSlots[i];
        if (slot->channel == channel) {
            Spu_GetVoiceRef(slot->voice, &voiceRef);
            bankLayer = Snd_GetNote(song->bank, slot->program, slot->layer);
            if (pitchBend >= 0) {
                scale   = bankLayer->bendUp;
                scale <<= MIDI_PITCH_FRACTION_BITS;
            } else {
                scale   = bankLayer->bendDown;
                scale <<= MIDI_PITCH_FRACTION_BITS;
            }
            scale            *= pitchBend;
            pitch             = scale / MIDI_PITCH_BEND_MAX;
            slot->pitchOffset = pitch;
            attr              = voiceRef.attr;
            attr->pitch       = Spu_CalcVolume((u16)slot->key, pitch, bankLayer->rootKey, bankLayer->fineTune);
            attr->mask       |= SPU_VOICE_PITCH;
        }
        i += 1;
    } while (i < ARRAY_SIZE(song->voiceSlots));
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
    switch (state->phase) {
        case SOUND_LOAD_PHASE_HEADER:
            // Retain the hSPK header. Group and layer tables follow it in this sector.
            src = arg0;
            dst = state->payload.words;
            i   = 0;
            do {
                *dst = *src;
                src++;
                i++;
                dst++;
            } while (i < ARRAY_SIZE(state->payload.words));

            nibble = state->payload.header.bankId & SOUND_BANK_TYPE_MASK;
            if ((u32)(nibble - 0x8000) < 0x5001U) {
                D_800689E8   = 1;
                state->phase = SOUND_LOAD_PHASE_ERROR;
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
                if (SndBank_FreeById(state->payload.header.bankId, state->payload.header.imageKind) == -1) {
                    state->phase = SOUND_LOAD_PHASE_WAIT_FAIL;
                    break;
                }
            }
            {
                SndBank* tmp;
                tmp         = Snd_AllocBank(&state->payload.header);
                state->bank = tmp;
                if (tmp == 0) {
                    state->phase = SOUND_LOAD_PHASE_WAIT_FAIL;
                    break;
                }
                src = arg0 + ARRAY_SIZE(state->payload.words);
                dst = tmp->heapBlock;
            }
            count = (state->payload.header.layerCount * (s32)(sizeof(*state->bank->layers) / sizeof(*dst))) + state->payload.header.groupCount * (s32)(sizeof(*state->bank->groups) / sizeof(*dst));
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
            (state->bank)->layerCount = state->payload.header.layerCount;
            (state->bank)->bankId     = state->payload.header.bankId;
            (state->bank)->waveBytes  = state->payload.header.waveBytes;
            state->phase              = SOUND_LOAD_PHASE_ALLOC_IMAGE;
            break;

        case SOUND_LOAD_PHASE_ALLOC_IMAGE:
            aligned               = (state->payload.header.imageBytes + 3) & 0xFFFC;
            state->bytesRemaining = aligned;
            mem                   = SndLoad_AllocBuffer(state->payload.header.bankId, state->payload.header.imageKind, aligned);
            state->imageBuffer    = mem;
            if (mem == 0) {
                state->phase = SOUND_LOAD_PHASE_WAIT_FAIL;
                Snd_FreeBank(state->bank);
                state->bank = 0;
                break;
            }
            state->writeCursor = mem;
            state->phase       = SOUND_LOAD_PHASE_COPY_IMAGE;
            /* fallthrough */
        case SOUND_LOAD_PHASE_COPY_IMAGE:
            len = (u32)state->bytesRemaining >> 2;
            if ((u32)state->bytesRemaining < (u32)state->sectorBytes) {
                state->phase = SOUND_LOAD_PHASE_BEGIN_WAVE;
            } else {
                len                    = (u32)state->sectorBytes >> 2;
                state->bytesRemaining -= state->sectorBytes;
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

        case SOUND_LOAD_PHASE_BEGIN_WAVE: {
            s32 size;
            size                   = state->payload.header.waveBytes;
            state->bytesRemaining  = size;
            (state->bank)->spuAddr = SndLoad_LookupMode(
                state->payload.header.imageKind, (state->bank)->bankId, size);
            spuAddr = (state->bank)->spuAddr;
        }
            if (spuAddr == 0) {
                D_800689E8   = 4;
                state->phase = SOUND_LOAD_PHASE_WAIT_FAIL;
                Snd_FreeBank(state->bank);
                state->bank = 0;
                break;
            }
            SpuSetTransferStartAddr(spuAddr + (state->payload.header.waveBlockOffset << 6));
            state->phase = SOUND_LOAD_PHASE_UPLOAD_WAVE;
            /* fallthrough */
        case SOUND_LOAD_PHASE_UPLOAD_WAVE: {
            s32 rem;
            s32 step;
            rem  = state->bytesRemaining;
            step = state->sectorBytes;
            if ((u32)step >= (u32)rem) {
                len          = rem;
                state->phase = SOUND_LOAD_PHASE_DONE;
            } else {
                len                   = step;
                state->bytesRemaining = rem - step;
            }
        }
            // A polling feed requires the previous DMA to have finished.
            // CD audio keeps its bank; every other feed releases it.
            if (state->syncUpload == 0) {
                if (SpuIsTransferCompleted(SPU_TRANSFER_PEEK) == 0) {
                    if (state->feedMode != SOUND_LOAD_FEED_CD_AUDIO) {
                        Snd_FreeBank(state->bank);
                        state->bank = 0;
                    }
                    D_800689E8   = 5;
                    state->phase = SOUND_LOAD_PHASE_ERROR;
                    break;
                }
                SpuWritePartly((u8*)arg0, len);
            } else {
                SpuWritePartly((u8*)arg0, len);
                SpuIsTransferCompleted(SPU_TRANSFER_WAIT);
            }
            break;

        case SOUND_LOAD_PHASE_DONE:
            break;

        case SOUND_LOAD_PHASE_WAIT_FAIL:
            // The failure stands until `transferSectors` sectors have arrived.
            if ((state->sectorsArrived + 1) >= (s32)state->payload.header.transferSectors) {
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
                state->phase = SOUND_LOAD_PHASE_DONE;
            }
            break;
    }

    state->sectorsArrived += 1;
    return state->phase;
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
    bankSlot = sndBankSlotGet(slot);
    if (bankSlot == NULL) {
        goto fail;
    }
    bankSlot->bankId  = bank->bankId;
    bankSlot->bank    = bank;
    bankSlot->image   = load->imageBuffer;
    bankSlot->spuAddr = bank->spuAddr;
    i                 = load->payload.header.layerCount;
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

/// Converts a bank's sample-layer offsets to absolute SPU byte addresses.
///
/// `layerCount` is a nonnegative element count within the initialized layer
/// table (the load header supplies it). Apply once, before playback, while
/// `waveAddr` still holds offsets from `bank->spuAddr`. A zero count touches no
/// layer; the descriptor and its table must remain live throughout the call.
static inline void _sndBankRebaseLayerWaveAddresses(SndBank* bank, s32 layerCount)
{
    u32           spuBaseAddr = bank->spuAddr;
    SndBankLayer* layer       = bank->layers;

    while (--layerCount != -1) {
        layer->waveAddr += spuBaseAddr;
        layer++;
    }
}

static s32 SndLoad_Complete(SndLoadState* load)
{
    SndBank*   bank;
    _MidiSong* song;
    s32        id;
    s32        ret;

    if (D_800689E8 == 6) {
        gSndLoadBankId = SOUND_LOAD_BANK_NONE;
        ret            = 0;
    } else {
        ret = -1;
        switch (load->payload.header.imageKind) {
            case SOUND_BANK_IMAGE_SEQUENCE:
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
                    _sndBankRebaseLayerWaveAddresses(bank, load->payload.header.layerCount);
                    Snd_BuildGroupIndex(song->bank);
                    ret               = 0;
                    gSndLoadBankId    = SOUND_LOAD_BANK_NONE;
                    load->bank        = 0;
                    load->imageBuffer = 0;
                }
                break;
            case SOUND_BANK_IMAGE_SCRIPT:
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
    SndLoad_Init(SOUND_LOAD_FEED_SECTOR, arg0);
}

void SndLoad_BeginFromBuffer(u8 arg0, void* arg1)
{
    SndLoad_State.syncUpload = arg0;
    D_8008212C               = D_80082122;
    D_80082121               = D_80082135;
    SndLoad_Init(SOUND_LOAD_FEED_CHUNK, arg1);
}

void SndLoad_Teardown(void)
{
    SndLoadState* state;

    D_80082122 = D_8008212C;
    D_80082135 = D_80082121;
    state      = &SndLoad_State;
    if (state->phase != SOUND_LOAD_PHASE_WAIT_FAIL) {
        state->phase = SOUND_LOAD_PHASE_TORN_DOWN;
        SndHeap_Free(state->imageBuffer);
        state->imageBuffer = 0;
        Snd_FreeBank(state->bank);
        state->bank = 0;
    }
}

s32 SndLoad_FeedSector(void* arg0)
{
    SndLoadState* state;
    s32           temp_s0;

    if (D_80068A78 != 0) {
        return -1;
    }
    state = &SndLoad_State;
    if (state->syncUpload != 0) {
        state->sectorBytes = SOUND_LOAD_SECTOR_BYTES;
    } else {
        // The header, image and sample regions each open on a sector with a 16-byte prefix.
        switch (state->phase) {
            case SOUND_LOAD_PHASE_HEADER:
            case SOUND_LOAD_PHASE_ALLOC_IMAGE:
            case SOUND_LOAD_PHASE_BEGIN_WAVE:
                state->sectorBytes = SOUND_LOAD_SECTION_BYTES;
                arg0               = (u8*)arg0 + SOUND_LOAD_SECTION_HEADER_BYTES;
                break;
            case SOUND_LOAD_PHASE_COPY_IMAGE:
            case SOUND_LOAD_PHASE_UPLOAD_WAVE:
            case SOUND_LOAD_PHASE_ERROR:
                state->sectorBytes = SOUND_LOAD_SECTOR_BYTES;
                break;
            case SOUND_LOAD_PHASE_DONE:
                return SOUND_LOAD_PHASE_DONE;
            case SOUND_LOAD_PHASE_TORN_DOWN:
                // Teardown already released the image and the bank.
                return 0;
        }
    }
    temp_s0 = SndLoad_ProcessSector(arg0);
    if (temp_s0 == SOUND_LOAD_PHASE_ERROR) {
        return -1;
    }
    if (temp_s0 == SOUND_LOAD_PHASE_DONE) {
        SndLoad_Complete(state);
    }
    return temp_s0;
}

s32 SndLoad_FeedSectorOrError(void* arg0)
{
    s32 temp;

    temp = SndLoad_ProcessSector(arg0);
    if (temp == SOUND_LOAD_PHASE_ERROR) {
        return -1;
    }
    return temp;
}

s32 SndBank_FinalizeLoad(SndLoadState* load)
{
    SndBank*      bank;
    _MidiSong*    song;
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
    index              &= 0xFF;
    song                = Midi_GetSlot(index);
    song->sequenceId    = index;
    song->sequenceBytes = (load->payload.header.imageBytes + 3) & 0xFFFC;
    temp                = load->imageBuffer;
    song->bank          = bank;
    song->sequenceData  = temp;
    song->waveBytes     = load->payload.header.waveBytes;
    i                   = load->payload.header.layerCount;
    base                = ((volatile SndBank*)bank)->spuAddr;
    bankLayer           = ((volatile SndBank*)bank)->layers;
    i                   = i - 1;
    if (i != -1) {
        end = -1;
        do {
            i                   -= 1;
            bankLayer->waveAddr += base;
            bankLayer++;
        } while (i != end);
    }
    Snd_BuildGroupIndex(song->bank);
    gSndLoadBankId    = SOUND_LOAD_BANK_NONE;
    load->bank        = 0;
    load->imageBuffer = 0;
    return 0;
}

static void* SndLoad_AllocBuffer(s32 arg0, s32 arg1, u32 arg2)
{
    u16 x;

    x = arg0;
    if ((arg1 & 0xFF) == SOUND_BANK_IMAGE_SEQUENCE) {
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

    arg0  &= SOUND_BANK_IMAGE_KIND_MASK;
    result = 0;
    switch (arg0) {
        case SOUND_BANK_IMAGE_SEQUENCE:
            result = 0x1010;
            break;
        case SOUND_BANK_IMAGE_SCRIPT:
            result = SndLoad_ResolveSpuAddr(arg2, arg1 & 0xFFFF);
            break;
    }
    return result;
}

static void SndLoad_Init(s32 arg0, void* arg1)
{
    SndLoadState* state;
    s32           size;

    D_800689E8 = 0;
    state      = &SndLoad_State;
    if (arg0 == SOUND_LOAD_FEED_SECTOR) {
        size            = SOUND_LOAD_SECTOR_BYTES;
        state->feedMode = arg0;
    } else {
        size            = SOUND_LOAD_SECTION_BYTES;
        state->feedMode = SOUND_LOAD_FEED_CHUNK;
    }
    state->sectorBytes    = size;
    state->phase          = SOUND_LOAD_PHASE_HEADER;
    state->sectorsArrived = 0;
    state->sectorBuffer   = arg1;
    state->imageBuffer    = 0;
    state->bank           = 0;
    state->writeCursor    = 0;
    state->bytesRemaining = 0;
}

static s32 SndBank_FreeById(u16 arg0, s32 arg1)
{
    u16      x;
    u8       slot;
    s32      i;
    SndBank* base;
    SndBank* ptr;

    x = arg0;
    if ((arg1 & 0xFF) == SOUND_BANK_IMAGE_SEQUENCE) {
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
