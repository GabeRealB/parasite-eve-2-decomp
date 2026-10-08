#include <psyq/sys/types.h>
#include <psyq/libspu.h>
#include <psyq/libetc.h>

#include "common.h"

#include "cdaudio.h"
#include "main/areas.h"
#include "main/display.h"
#include "main/display_types.h"
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

#include "mapui/map_neo_ark.h"

// Loader policies shared by sector processing and image installation.
enum {
    SOUND_LOAD_IMAGE_LENGTH_MASK         = 0xFFFC, // Word alignment with the serialized 16-bit length retained
    SOUND_LOAD_SEQUENCE_ID_MASK          = 0xFF,
    SOUND_LOAD_IMAGE_KIND_BYTE_MASK      = 0xFF,
    SOUND_LOAD_BANK_ID_MASK              = 0xFFFF,
    SOUND_BANK_SLOT_UNSUPPORTED          = -1,
    SOUND_LOAD_FAILURE_NONE              = 0,
    SOUND_LOAD_FAILURE_INVALID_TYPE      = 1,
    SOUND_LOAD_FAILURE_NO_SAMPLE_ADDRESS = 4,
    SOUND_LOAD_FAILURE_TRANSFER_BUSY     = 5,
    SOUND_LOAD_FAILURE_DRAINED           = 6
};

// One resident song; these sequence ids select the two special mixing policies.
enum {
    MIDI_RESIDENT_SONG_COUNT     = 1,
    MIDI_SEQUENCE_AREA_VOLUME    = 0x4F,
    MIDI_SEQUENCE_FIXED_GAIN     = 0x5A,
    MIDI_STARTUP_GAIN_MULTIPLIER = 3,
    MIDI_STARTUP_GAIN_SHIFT      = 5
};

extern void (*SndEvt_Handlers[])(SndEvt*);

static _MidiEventHandler Midi_EventFns[];

static volatile s32 D_800689E8;

static u8 D_800689F0[];

static void _sndEvtRelease(SndEvt* event);

static void _sndEvtHandleNoOp(SndEvt* event);

static void _sndEvtHandleMidiStart(SndEvt* event);

static void _sndEvtHandleMidiStop(SndEvt* event);

static void _sndEvtHandleMidiMute(SndEvt* event);

static void _sndEvtHandleMidiUnmute(SndEvt* event);

static void _sndEvtHandleMidiVolume(SndEvt* event);

static void _sndEvtHandleScriptStart(SndEvt* event);

static void _sndEvtHandleScriptStop(SndEvt* event);

static void _sndEvtHandleScriptMute(SndEvt* event);

static void _sndEvtHandleScriptUnmute(SndEvt* event);

static void _sndEvtHandleScriptMix(SndEvt* event);

static void _sndEvtHandleScriptVolume(SndEvt* event);

static void _sndEvtHandleScriptDuckAcquire(SndEvt* event);

static void _sndEvtHandleScriptDuckRelease(SndEvt* event);

static void _sndEvtHandleScriptKeyOff(SndEvt* event);

static s32 _midiStartSequence(u8 sequenceId, u16 fadeTicks);

static s32 SndEvt_EnqueueType3(s32 arg0);

static s32 SndEvt_EnqueueType4(s32 arg0);

static void _midiStopMatching(u8 sequenceSelector, u16 fadeTicks);

static void _midiSetMuteMatching(u8 sequenceSelector, s32 muted);

static void _midiSetSequenceVolume(u8 sequenceSelector, u8 volumeScale);

static _MidiSong* _midiPrepareSongForLoad(s32 sequenceId);

static u8* _midiGetSequenceBuffer(s32 unused, s32 requestedBytes);

static void _midiOnVoiceReleased(void* context);

static void _midiResetSongSlot(s32 songIndex);

static u8* _midiResolveTrackData(_MidiSong* song, s32 trackIndex, u8* sequenceData);

static void _midiEndTracks(_MidiSong* song);

static void _midiKeyOffSongVoices(_MidiSong* song);

static void _midiAdvanceTrack(_MidiSong* song, _MidiTrack* track);

static void _midiUpdateVoiceVolumes(_MidiSong* song);

static u8* _midiHandleNoteOn(s32 status, u8* event, _MidiSong* song, _MidiTrack* track);

static u8* _midiHandleControlChange(s32 status, u8* event, _MidiSong* song, _MidiTrack* track);

static inline s32 _midiReadVlq(const u8* data, u8* byteCount);

static u8* _midiHandleSystemEvent(s32 status, u8* event, _MidiSong* song, _MidiTrack* track);

static s32 _midiReadDeltaTime(const u8* data, u8* byteCount);

static void _midiResetChannelTable(_MidiChannelTable* channels);

static u8* _midiHandleUnsupportedPressure(s32 status, u8* event, _MidiSong* song, _MidiTrack* track);

static u8* _midiHandleNoteOff(s32 status, u8* event, _MidiSong* song, _MidiTrack* track);

static u8* _midiHandleProgramChange(s32 status, u8* event, _MidiSong* song, _MidiTrack* track);

static u8* _midiHandlePitchBend(s32 status, u8* event, _MidiSong* song, _MidiTrack* track);

static s32 _sndLoadInstallScriptBank(SndLoadState* load);

static s32 _sndLoadComplete(SndLoadState* load);

static void* _sndLoadAllocImageBuffer(s32 bankId, s32 imageKind, u32 imageBytes);

static s32 _sndLoadResolveSampleAddress(s32 imageKind, s32 bankId, s32 waveBytes);

static void _sndLoadResetState(s32 feedMode, void* sectorBuffer);

static s32 _sndLoadPrepareBankSlot(u16 bankId, s32 imageKind);

void (*SndEvt_Handlers[])(SndEvt*) = {
    _sndEvtHandleNoOp,              // SOUND_EVENT_NO_OP
    _sndEvtHandleMidiStart,         // SOUND_EVENT_MIDI_START
    _sndEvtHandleMidiStop,          // SOUND_EVENT_MIDI_STOP
    _sndEvtHandleMidiMute,          // SOUND_EVENT_MIDI_MUTE
    _sndEvtHandleMidiUnmute,        // SOUND_EVENT_MIDI_UNMUTE
    _sndEvtHandleMidiVolume,        // SOUND_EVENT_MIDI_SET_VOLUME
    _sndEvtHandleScriptStart,       // SOUND_EVENT_SCRIPT_START
    _sndEvtHandleScriptStop,        // SOUND_EVENT_SCRIPT_STOP
    _sndEvtHandleScriptMute,        // SOUND_EVENT_SCRIPT_MUTE
    _sndEvtHandleScriptUnmute,      // SOUND_EVENT_SCRIPT_UNMUTE
    _sndEvtHandleScriptMix,         // SOUND_EVENT_SCRIPT_SET_PAN_ATTENUATION
    _sndEvtHandleScriptVolume,      // SOUND_EVENT_SCRIPT_SET_VOLUME
    _sndEvtHandleNoOp,              // SOUND_EVENT_RESERVED_NO_OP
    _sndEvtHandleScriptDuckAcquire, // SOUND_EVENT_SCRIPT_DUCK_ACQUIRE
    _sndEvtHandleScriptDuckRelease, // SOUND_EVENT_SCRIPT_DUCK_RELEASE
    _sndEvtHandleScriptKeyOff,      // SOUND_EVENT_SCRIPT_KEY_OFF
};

static _MidiEventHandler Midi_EventFns[] = {
    _midiHandleNoteOff,
    _midiHandleNoteOn,
    _midiHandleUnsupportedPressure,
    _midiHandleControlChange,
    _midiHandleProgramChange,
    _midiHandleUnsupportedPressure,
    _midiHandlePitchBend,
    _midiHandleSystemEvent,
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
///
/// Covers the complete fixed array with aligned 32-bit stores. All queued and
/// unqueued reservations are invalidated; separately stored FIFO endpoints and
/// the drain gate are left for the caller to reset before audio processing resumes.
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

/// Dispatches a MIDI start request, discarding the slot or refusal result.
///
/// The event selects an already loaded, idle sequence by exact id and supplies
/// a fade duration in audio updates. Its reservation remains owned by dispatch.
static void _sndEvtHandleMidiStart(SndEvt* event)
{
    _midiStartSequence(event->args.midi.sequenceId, event->args.midi.fadeTicks);
}

/// Schedules the event's matching MIDI sequences to fade out or stop.
///
/// Selector zero addresses every sequence. Normally playing songs use the
/// unsigned fade duration in audio updates; other phases stop immediately.
/// Zero duration or master gain bypasses interpolation. Dispatch retains the
/// event reservation until this handler returns.
static void _sndEvtHandleMidiStop(SndEvt* event)
{
    _midiStopMatching(event->args.midi.sequenceId, event->args.midi.fadeTicks);
}

/// Starts the fixed mute ramp for the event's matching MIDI sequence selector.
///
/// Selector zero addresses every sequence. Tracks continue during the ramp;
/// this command does not change the separate music-output gate.
static void _sndEvtHandleMidiMute(SndEvt* event)
{
    _midiSetMuteMatching(event->args.midi.sequenceId, true);
}

/// Starts the fixed unmute ramp for the event's matching MIDI sequence selector.
///
/// Selector zero addresses every sequence. Only songs in the muted phase
/// accept the request; the separate music-output gate is unaffected.
static void _sndEvtHandleMidiUnmute(SndEvt* event)
{
    _midiSetMuteMatching(event->args.midi.sequenceId, false);
}

/// Applies the event's sequence gain and marks every matching channel for remixing.
///
/// Selector zero addresses every sequence. The unsigned gain byte multiplies
/// the sequence's mix-table level; producers supply 0..127. Matching sequences
/// must have loaded ids in 0..99 for that table. Master gain and fades apply
/// separately, and sequence 0x5A uses its fixed mixing gain instead.
static void _sndEvtHandleMidiVolume(SndEvt* event)
{
    _midiSetSequenceVolume(event->args.midi.sequenceId, event->args.midi.volumeScale);
}

/// Starts a sound-script instance from the event's resolved, borrowed bank entry.
///
/// Signed pan and attenuation bytes are forwarded unchanged. The bank slot,
/// image and entry controls must stay loaded through dispatch and execution.
/// A script can start several SPU voices; the script-slot result is discarded.
static void _sndEvtHandleScriptStart(SndEvt* event)
{
    SndEvtScriptArgs* scriptArgs;

    scriptArgs = &event->args.script;
    sndScriptTryStart(scriptArgs->soundId, scriptArgs->panOffset, scriptArgs->level.attenuation, scriptArgs->bankSlot, scriptArgs->entryControls);
}

/// Applies the event's resolved stop selector and control to sound-script instances.
///
/// A nonzero entry selects an exact id or its all-instance form; a zero entry
/// compares the whole selector against bank-type nibbles, with the all-types
/// selector excluding ambient scripts. No bank remapping occurs here.
/// Running entry matches stop without fading for controls 0 and 1 (0 retains
/// their release policy, 1 keeps their ADSR); 2..65535 fade in audio updates.
/// Type stops bypass the fade and keep ADSR only for control 1. Other phases
/// follow `sndScriptStopMatching`'s stop rules. The scan result is discarded;
/// dispatch retains the event reservation.
static void _sndEvtHandleScriptStop(SndEvt* event)
{
    const SndEvtScriptArgs* scriptArgs;

    scriptArgs = &event->args.script;
    sndScriptStopMatching(scriptArgs->soundId, scriptArgs->stopControl);
}

/// Requests the fixed mute ramp for scripts matching an exact id or bank type.
///
/// The selector is already resolved. Accepted instances pause script commands
/// during the ramp while their existing voices continue updating.
static void _sndEvtHandleScriptMute(SndEvt* event)
{
    sndScriptSetMuteMatching(event->args.script.soundId, true);
}

/// Requests the fixed unmute ramp for scripts matching an exact id or bank type.
///
/// The selector is already resolved; only muting instances accept the request.
/// Existing voices keep updating while script-command execution is paused.
static void _sndEvtHandleScriptUnmute(SndEvt* event)
{
    sndScriptSetMuteMatching(event->args.script.soundId, false);
}

/// Applies the event's pan and attenuation to the first live script with its exact id.
///
/// Starting, running, releasing and fading-out slots qualify; idle, stopping,
/// muting and unmuting slots do not. A missing id is ignored. Both mix controls
/// retain their signed bytes: pan adds three SPU steps per unit, attenuation
/// uses its magnitude, and -128 targets the unattenuated level. Larger changes
/// ramp per voice visit during later audio updates; dispatch retains the event.
static void _sndEvtHandleScriptMix(SndEvt* event)
{
    s32                     scriptSlotIndex;
    const SndEvtScriptArgs* scriptArgs;

    scriptArgs      = &event->args.script;
    scriptSlotIndex = sndScriptFindInstanceById(scriptArgs->soundId);
    if (scriptSlotIndex >= 0) {
        sndScriptRampMix(scriptSlotIndex, scriptArgs->panOffset, scriptArgs->level.attenuation);
    }
}

/// Applies an unsigned volume-scale request to the first live script with its exact id.
///
/// The producer resolves the id and normalizes negative low bytes to full scale.
/// Dispatch promotes the stored unsigned byte to the ramp's s32 input; ordinary
/// producer values are 0..127. Starting, running, releasing and fading-out slots
/// qualify; a missing id is ignored. Larger gain changes ramp on later voice
/// updates. The handler borrows the event; dispatch releases it on return.
static void _sndEvtHandleScriptVolume(SndEvt* event)
{
    s32                     scriptSlotIndex;
    const SndEvtScriptArgs* scriptArgs;

    scriptArgs      = &event->args.script;
    scriptSlotIndex = sndScriptFindInstanceById(scriptArgs->soundId);
    if (scriptSlotIndex >= 0) {
        sndScriptRampVolume(scriptSlotIndex, scriptArgs->level.volumeScale);
    }
}

/// Acquires one nested request to duck the sound-script master gain toward 48.
///
/// The first eligible request saves the current gain and starts the downward
/// ramp; further requests only increment the count. The count must remain in
/// signed-word range. The event payload is unused, and dispatch releases it.
static void _sndEvtHandleScriptDuckAcquire(SndEvt* event)
{
    sndScriptAcquireDuck();
}

/// Releases one nested sound-script duck request, restoring gain on the last release.
///
/// Extra releases do nothing. A last release with a saved gain starts restoration
/// on subsequent audio updates. The event payload is unused; dispatch releases it.
static void _sndEvtHandleScriptDuckRelease(SndEvt* event)
{
    sndScriptReleaseDuck();
}

/// Keys off type-1 and area-script voices and makes their script slots idle.
///
/// The head voice receives rate-11 exponential release; other voices retain
/// their release settings. Voice links and ownership remain through completion
/// or slot reuse, and hardware changes await the SPU flush. No selector is read:
/// the event payload is unused, and dispatch releases the event afterward.
static void _sndEvtHandleScriptKeyOff(SndEvt* event)
{
    sndScriptKeyOffType1AndArea();
}

s32 midiInitSystem(u32 unused)
{
    enum {
        MIDI_INITIAL_MASTER_VOLUME    = 64,
        MIDI_MUSIC_FIRST_VOICE        = 0,
        MIDI_MUSIC_VOICE_COUNT        = 16,
        MIDI_INITIAL_LENGTH_BYTES     = 16,
        MIDI_SEQUENCE_BANK_TYPE_INDEX = SOUND_BANK_TYPE_SEQUENCE >> 12,
        MIDI_SEQUENCE_BANK_BOOT_ID    = SOUND_BANK_TYPE_SEQUENCE | 255
    };
    s32        songIndex;
    _MidiSong* song;
    SndBank*   bank;

    for (songIndex = 0; songIndex < MIDI_RESIDENT_SONG_COUNT; songIndex++) {
        _midiResetSongSlot(songIndex & 0xFF);
    }
    D_8007F2F0 = MIDI_INITIAL_MASTER_VOLUME;
    D_800820E9 = 0;
    D_800820E0 = 0;
    D_800820E4 = 0;
    spuSetVoiceRange(SPU_VOICE_RANGE_MUSIC, MIDI_MUSIC_FIRST_VOICE, MIDI_MUSIC_VOICE_COUNT);
    song                = _midiPrepareSongForLoad(SOUND_EVENT_MIDI_INVALID_SEQUENCE);
    song->sequenceId    = SOUND_EVENT_MIDI_INVALID_SEQUENCE;
    song->sequenceBytes = MIDI_INITIAL_LENGTH_BYTES;
    song->sequenceData  = D_8007F8E0;
    // Reserve the type-F tables; a later load partitions the retained block.
    bank                        = &Snd_Banks[Snd_BankSlotsByType[MIDI_SEQUENCE_BANK_TYPE_INDEX]];
    song->bank                  = bank;
    bank->bankId                = MIDI_SEQUENCE_BANK_BOOT_ID;
    song->bank->heapBlock       = sndHeapAlloc(SOUND_BANK_SEQUENCE_TABLE_BYTES);
    song->bank->groups          = song->bank->heapBlock;
    song->bank->layers          = song->bank->heapBlock;
    song->bank->groupFirstLayer = song->bank->heapBlock;
    Snd_SequenceBankBuffer      = song->bank->heapBlock;
    song->waveBytes             = MIDI_INITIAL_LENGTH_BYTES;
    return -1;
}

/// Seeds a track countdown from its first delta and advances to its first event.
///
/// `track->eventCursor` must borrow the first delta in a loaded sequence image,
/// readable through its terminating VLQ byte. The countdown is in MIDI ticks;
/// the encoded length advances the cursor in bytes. No stream bound is checked.
/// The fractional numerator starts at 3599 in both PAL and NTSC.
static inline void _midiInitializeTrackClock(_MidiTrack* track)
{
    u8 deltaBytes;

    track->ticksUntilEvent = _midiReadDeltaTime(track->eventCursor, &deltaBytes);
    track->eventCursor    += deltaBytes;
    track->tickFraction    = MIDI_TRACK_INITIAL_TICK_FRACTION;
}

/// Starts an idle, loaded sequence and initializes its tracks and note slots.
///
/// `sequenceId` matches exactly, including zero. The image and completed bank
/// tables must remain loaded through playback. The SMF header must have at
/// most eighteen tracks and a ticks-per-quarter division; only the low bytes
/// of format and track count are retained. Chunks, events and VLQs must stay
/// inside the resident sequence buffer; the cursor check covers only each
/// track's first delta. The id must be 0..99 for the sequence mix table.
/// `fadeTicks` counts audio updates, with zero bypassing the fade; rounding
/// can extend it. Returns slot zero on success,
/// -1 for a bad header tag or initial cursor, or -5 with no matching idle song.
/// Header rejection still resets channels; a later rejection can leave tracks
/// partially initialized without starting playback.
static s32 _midiStartSequence(u8 sequenceId, u16 fadeTicks)
{
    enum {
        MIDI_FILE_HEADER_TAG        = 0x4D546864,
        MIDI_START_INVALID_IMAGE    = -1,
        MIDI_START_NO_IDLE_SEQUENCE = -5
    };
    s32         songIndex;
    s32         entryIndex;
    _MidiSong*  song;
    _MidiTrack* tracks;
    _MidiTrack* track;
    u8*         sequenceData;
    u8*         trackData;
    s32*        trackWords;

    songIndex = 0;
    do {
        song = &Midi_Song + songIndex;
        if (song->sequenceId != SOUND_EVENT_MIDI_INVALID_SEQUENCE) {
            if ((song->sequenceId == sequenceId) && (song->status == MIDI_SONG_IDLE)) {
                _midiResetChannelTable(&song->channels);
                sequenceData = song->sequenceData;
                if (((sequenceData[0] << 24) | (sequenceData[1] << 16) | (sequenceData[2] << 8) | sequenceData[3]) != MIDI_FILE_HEADER_TAG) {
                    return MIDI_START_INVALID_IMAGE;
                }

                tracks           = song->tracks;
                song->format     = sequenceData[9];
                trackWords       = (s32*)tracks;
                song->trackCount = sequenceData[0xB];

                // Clear each complete active record through its 32-bit representation.
                for (entryIndex = 0; entryIndex < song->trackCount * (sizeof(_MidiTrack) / sizeof(s32)); entryIndex++) {
                    *trackWords++ = 0;
                }

                if (song->trackCount != 0) {
                    entryIndex = 0;
                    track      = tracks;
                    do {
                        trackData                             = _midiResolveTrackData(song, entryIndex & 0xFF, song->sequenceData);
                        track->savedCursors.startup.dataStart = trackData;
                        track->eventCursor                    = trackData;
                        if ((trackData < D_8007F8E0) || (trackData >= D_8007F8E0 + sizeof(D_8007F8E0))) {
                            return MIDI_START_INVALID_IMAGE;
                        }
                        _midiInitializeTrackClock(track);
                        entryIndex++;
                        track++;
                    } while (entryIndex < song->trackCount);
                }

                song->groups                = song->bank->groups;
                song->layers                = song->bank->layers;
                song->ticksPerQuarter       = (sequenceData[0xC] << 8) | sequenceData[0xD];
                song->pendingTempoBpm       = MIDI_SONG_INITIAL_TEMPO_BPM;
                song->currentTempoBpm       = MIDI_SONG_INITIAL_TEMPO_BPM;
                song->pendingTempoOffsetBpm = 0;
                song->currentTempoOffsetBpm = 0;
                song->volumeScale           = (D_800689F0[song->sequenceId] * MIDI_STARTUP_GAIN_MULTIPLIER) << MIDI_STARTUP_GAIN_SHIFT;
                linInterpSetup(&song->volumeRamp, 0, D_8007F2F0, fadeTicks);

                if (fadeTicks != 0) {
                    song->status = MIDI_SONG_FADING_IN;
                } else {
                    song->status = MIDI_SONG_PLAYING;
                }

                song->volumeDirtyChannels = MIDI_SONG_ALL_CHANNELS_DIRTY;
                song->songTicks           = 0;
                for (entryIndex = 0; entryIndex < ARRAY_SIZE(song->voiceSlots); entryIndex++) {
                    _midiOnVoiceReleased(&song->voiceSlots[entryIndex]);
                }

                return songIndex;
            }
        } else {
            break;
        }
        songIndex++;
    } while (songIndex < MIDI_RESIDENT_SONG_COUNT);

    return MIDI_START_NO_IDLE_SEQUENCE;
}

s32 midiTick(s32* unused)
{
    _MidiSong* song;
    s32        songIndex;
    s32        trackIndex;

    for (songIndex = 0; songIndex < MIDI_RESIDENT_SONG_COUNT; songIndex++) {
        song = &Midi_Song + songIndex;
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
                linInterpStep(&song->volumeRamp);
                song->volumeDirtyChannels = MIDI_SONG_ALL_CHANNELS_DIRTY;
                /* fallthrough */
            case MIDI_SONG_PLAYING:
            play:
                for (trackIndex = 0; trackIndex < song->trackCount; trackIndex++) {
                    if (song->tracks[trackIndex].ended == false) {
                        _midiAdvanceTrack(song, &song->tracks[trackIndex]);
                    }
                }
                // The tempo event's pending bytes apply on the next update.
                song->currentTempoBpm       = song->pendingTempoBpm;
                song->currentTempoOffsetBpm = song->pendingTempoOffsetBpm;
                break;
            case MIDI_SONG_STOPPING:
            stop:
                // End playback now; note slots remain live until voice release.
                _midiEndTracks(song);
                _midiKeyOffSongVoices(song);
                song->status = MIDI_SONG_IDLE;
                break;
            case MIDI_SONG_UNMUTING:
                // Retain the original phase test, even at the unmute ramp's target.
                if (song->status == MIDI_SONG_MUTED && song->volumeRamp.gain >= song->volumeRamp.targetGain) {
                    song->volumeDirtyChannels = MIDI_SONG_ALL_CHANNELS_DIRTY;
                    song->status              = MIDI_SONG_PLAYING;
                    goto play;
                }
                linInterpStep(&song->volumeRamp);
                song->volumeDirtyChannels = MIDI_SONG_ALL_CHANNELS_DIRTY;
                goto play;
        }
        if (song->volumeDirtyChannels != 0) {
            _midiUpdateVoiceVolumes(song);
            song->volumeDirtyChannels = 0;
        }
    }
    return 0;
}

s32 sndEvtRequestMidiStart(s32 sequenceId, s32 fadeTicks)
{
    SndEvt* event;

    if ((sequenceId & 0xFF) == SOUND_EVENT_MIDI_INVALID_SEQUENCE) {
        return SOUND_EVENT_MIDI_REQUEST_INVALID_SEQUENCE;
    }
    event = sndEvtAlloc();
    if (event == NULL) {
        return SOUND_EVENT_MIDI_REQUEST_POOL_FULL;
    }
    event->command              = SOUND_EVENT_MIDI_START;
    event->args.midi.sequenceId = sequenceId;
    event->args.midi.fadeTicks  = fadeTicks;
    sndEvtEnqueue(event);
    return 0;
}

s32 sndEvtRequestMidiStop(s32 sequenceSelector, s32 fadeTicks)
{
    // Stop fades truncate to 16 bits and round down to a multiple of four ticks.
    enum { SOUND_EVENT_MIDI_STOP_FADE_TICK_MASK = 0xFFFC };
    SndEvt* event;

    if ((sequenceSelector & 0xFF) == SOUND_EVENT_MIDI_INVALID_SEQUENCE) {
        return SOUND_EVENT_MIDI_REQUEST_INVALID_SEQUENCE;
    }
    event = sndEvtAlloc();
    if (event == NULL) {
        return SOUND_EVENT_MIDI_REQUEST_POOL_FULL;
    }
    event->command              = SOUND_EVENT_MIDI_STOP;
    event->args.midi.sequenceId = sequenceSelector;
    event->args.midi.fadeTicks  = fadeTicks & SOUND_EVENT_MIDI_STOP_FADE_TICK_MASK;
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

/// Queues a reserved MIDI sequence-gain request and records its normalized gain.
///
/// `event` must be a non-NULL, unqueued slot reserved by `sndEvtAlloc`; the FIFO
/// owns it through dispatch and release. `sequenceSelector` is a byte: zero
/// selects all sequences, and 1..254 selects a matching loaded id. The caller
/// must exclude 255. `volumeScale` is a byte: 0..127 requests that gain
/// (0 silent, 127 full), and 128..255 requests full gain.
///
/// Dispatch multiplies the matching sequence's mix-table level by the gain and
/// marks every channel for refresh; master gain and the fade apply separately.
/// A matching loaded id must be in 0..99 when dispatched. Sequence 0x5A uses
/// its fixed gain instead. The shared cache keeps the latest queued gain,
/// including silence, for music unmuting even if no sequence matches. The music
/// output gate is independent of this request.
static inline void _sndEvtQueueMidiVolume(SndEvt* event, u8 sequenceSelector, u8 volumeScale)
{
    SndEvtMidiArgs* midiArgs;

    event->command       = SOUND_EVENT_MIDI_SET_VOLUME;
    midiArgs             = &event->args.midi;
    midiArgs->sequenceId = sequenceSelector;
    if ((s8)volumeScale >= 0) {
        midiArgs->volumeScale = volumeScale;
    } else {
        midiArgs->volumeScale = SOUND_EVENT_MIDI_VOLUME_FULL;
    }
    sndEvtEnqueue(event);
    // Read after reopening audio dispatch; releasing a slot retains its payload.
    D_800820E8 = midiArgs->volumeScale;
}

s32 sndEvtRequestMidiVolume(s32 sequenceSelector, s32 volumeScale)
{
    SndEvt* event;

    if ((sequenceSelector & 0xFF) == SOUND_EVENT_MIDI_INVALID_SEQUENCE) {
        return SOUND_EVENT_MIDI_VOLUME_INVALID_SEQUENCE;
    }
    event = sndEvtAlloc();
    if (event == NULL) {
        return SOUND_EVENT_MIDI_VOLUME_POOL_FULL;
    }
    _sndEvtQueueMidiVolume(event, sequenceSelector, volumeScale);
    return 0;
}

s32 midiIsSequenceBusy(s32 sequenceSelector)
{
    s32 songIndex;

    sequenceSelector &= 0xFF;
    if (sequenceSelector == SOUND_EVENT_MIDI_INVALID_SEQUENCE) {
        return 0;
    }
    for (songIndex = 0; songIndex <= 0; songIndex++) {
        if ((sequenceSelector == (&Midi_Song)[songIndex].sequenceId) || (sequenceSelector == SOUND_EVENT_MIDI_ALL_SEQUENCES)) {
            if ((&Midi_Song)[songIndex].status & MIDI_SONG_BUSY) {
                return 1;
            }
        }
    }
    return 0;
}

s32 midiCanSelectSequence(u8 sequenceId)
{
    s32 songIndex;

    if (gSndVolumeReducedMode == SOUND_VOLUME_MODE_REDUCED) {
        return 0;
    }
    if (sequenceId == SOUND_EVENT_MIDI_INVALID_SEQUENCE) {
        return 1;
    }
    for (songIndex = 0; songIndex <= 0; songIndex++) {
        if ((&Midi_Song)[songIndex].sequenceId == sequenceId) {
            return 0;
        }
    }
    return 1;
}

/// Schedules stopping for matching sequences, fading only the playing phase.
///
/// Selector zero addresses every sequence; other bytes match the loaded id.
/// Playing songs fade for `fadeTicks` audio updates, with zero bypassing the
/// ramp; rounding can extend positive durations. Every other matching phase
/// enters stopping immediately. The audio driver performs track end/key-off.
static void _midiStopMatching(u8 sequenceSelector, u16 fadeTicks)
{
    s32        songIndex;
    _MidiSong* song;

    for (songIndex = 0; songIndex < MIDI_RESIDENT_SONG_COUNT; songIndex++) {
        song = &Midi_Song + songIndex;
        if ((sequenceSelector == song->sequenceId) || (sequenceSelector == SOUND_EVENT_MIDI_ALL_SEQUENCES)) {
            if (song->status == MIDI_SONG_PLAYING) {
                song->status = MIDI_SONG_FADING_OUT;
                linInterpSetup(&song->volumeRamp, D_8007F2F0, 0, fadeTicks);
            } else {
                song->status = MIDI_SONG_STOPPING;
            }
        }
    }
}

/// Starts an eight-audio-update mute or unmute ramp on matching sequences.
///
/// Selector zero addresses every sequence; other bytes match the loaded id.
/// Nonzero `muted` accepts playing or unmuting songs; zero accepts muted songs.
/// Tracks continue during either ramp, and voices retain their ownership.
/// The normalized gain step can extend the requested duration by truncation.
static void _midiSetMuteMatching(u8 sequenceSelector, s32 muted)
{
    enum { MIDI_MUTE_RAMP_UPDATES = 8 };
    s32        songIndex;
    _MidiSong* song;

    for (songIndex = 0; songIndex <= 0; songIndex++) {
        song = &Midi_Song + songIndex;
        if ((sequenceSelector == song->sequenceId) || (sequenceSelector == SOUND_EVENT_MIDI_ALL_SEQUENCES)) {
            if (muted == 0) {
                if (song->status == MIDI_SONG_MUTED) {
                    song->status = MIDI_SONG_UNMUTING;
                    linInterpSetup(&song->volumeRamp, 0, D_8007F2F0, MIDI_MUTE_RAMP_UPDATES);
                }
            } else {
                if (song->status & MIDI_SONG_MUTABLE) {
                    song->status = MIDI_SONG_MUTED;
                    linInterpSetup(&song->volumeRamp, D_8007F2F0, 0, MIDI_MUTE_RAMP_UPDATES);
                }
            }
        }
    }
}

/// Sets the matching sequence's mix-table gain and requests a full channel refresh.
///
/// Selector zero addresses every sequence; other bytes match the loaded id.
/// A matching song must have an id in 0..99 for the mix table. The requested
/// byte gain is multiplied without clamping; the event producer supplies 0..127.
/// Master gain and the fade ramp apply separately. Sequence 0x5A's mixer uses
/// its fixed gain instead, though this stored gain is still updated.
static void _midiSetSequenceVolume(u8 sequenceSelector, u8 volumeScale)
{
    s32        songIndex;
    u8*        mixLevels;
    _MidiSong* song;
    s32        gainProduct;

    songIndex = 0;
    song      = &Midi_Song;
    for (; songIndex <= 0; songIndex++) {
        if ((sequenceSelector == song[songIndex].sequenceId) || (sequenceSelector == SOUND_EVENT_MIDI_ALL_SEQUENCES)) {
            mixLevels                           = D_800689F0;
            gainProduct                         = mixLevels[song[songIndex].sequenceId] * volumeScale;
            song[songIndex].volumeDirtyChannels = MIDI_SONG_ALL_CHANNELS_DIRTY;
            song[songIndex].volumeScale         = gainProduct;
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

/// Returns the sole resident song, ending its tracks if playback has not stopped.
///
/// `sequenceId` is ignored: there is one slot. An active song enters the
/// stopping phase; voice release waits for the next MIDI update. The returned
/// record is resident and is also used during system initialization.
static _MidiSong* _midiPrepareSongForLoad(s32 sequenceId)
{
    if (Midi_Song.status != MIDI_SONG_IDLE) {
        _midiEndTracks(&Midi_Song);
        Midi_Song.status = MIDI_SONG_STOPPING;
    }
    return &Midi_Song;
}

/// Returns the resident 10 KiB sequence-image buffer without allocating or clearing it.
///
/// Both request arguments are ignored. Loads must fit `sizeof(D_8007F8E0)`;
/// reusing the buffer invalidates the previous sequence's borrowed track cursors.
static u8* _midiGetSequenceBuffer(s32 unused, s32 requestedBytes)
{
    return D_8007F8E0;
}

/// Clears a complete note slot before marking its channel and SPU voice free.
///
/// `slot` must point to a writable, word-aligned record. The three-word clear
/// also resets its saved note and bank-layer controls; both signed selectors
/// are then set to `MIDI_NOTE_SLOT_FREE`.
static inline void _midiResetNoteSlot(_MidiNoteSlot* slot)
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

/// Releases the MIDI note record when its SPU voice is freed or stolen.
///
/// `context` is the non-NULL, word-aligned `_MidiNoteSlot` retained by the SPU
/// voice callback. Sequence startup also calls this handler directly to clear
/// its slots. The callback signature requires the untyped context pointer.
static void _midiOnVoiceReleased(void* context)
{
    _MidiNoteSlot* slot = context;

    _midiResetNoteSlot(slot);
}

void midiMuteMusic(void)
{
    enum { MIDI_MUSIC_MUTE_ENABLED = 1 };
    SndEvt*         event;
    SndEvtMidiArgs* midiArgs;

    // Keep the output gate enabled even when no command slot is available.
    D_800820E9 = MIDI_MUSIC_MUTE_ENABLED;
    event      = sndEvtAlloc();
    if (event != NULL) {
        midiArgs              = &event->args.midi;
        event->command        = SOUND_EVENT_MIDI_SET_VOLUME;
        midiArgs->sequenceId  = SOUND_EVENT_MIDI_ALL_SEQUENCES;
        midiArgs->volumeScale = SOUND_EVENT_MIDI_VOLUME_SILENT;
        sndEvtEnqueue(event);
        D_800820E8 = midiArgs->volumeScale;
    }
}

void midiUnmuteMusic(void)
{
    enum { MIDI_MUSIC_MUTE_DISABLED = 0 };
    SndEvt* event;
    u8      lastRequestedVolumeScale;

    if (D_800820E9 != MIDI_MUSIC_MUTE_DISABLED) {
        // Release the gate even if the cached-gain request cannot be queued.
        lastRequestedVolumeScale = D_800820E8;
        D_800820E9               = MIDI_MUSIC_MUTE_DISABLED;
        event                    = sndEvtAlloc();
        if (event != NULL) {
            _sndEvtQueueMidiVolume(event, SOUND_EVENT_MIDI_ALL_SEQUENCES, lastRequestedVolumeScale);
        }
    }
}

/// Clears one resident song, resets channel controls and marks every note slot free.
///
/// Only the low byte of `songIndex` is used; zero is the sole valid slot.
/// Call while playback and release callbacks are quiescent: this clears the
/// record without releasing hardware voices or unregistering their contexts.
/// The full word-aligned song and all eighteen note records are reset.
static void _midiResetSongSlot(s32 songIndex)
{
    _MidiSong* song;
    s32*       songWords;
    u32        entryIndex;

    songIndex &= 0xFF;
    song       = &(&Midi_Song)[songIndex];

    songWords  = (s32*)song;
    entryIndex = 0;
    do {
        *songWords = 0;
        entryIndex++;
        songWords++;
    } while (entryIndex < sizeof(*song) / sizeof(*songWords));

    linInterpSetup(&song->volumeRamp, 0, 0, 0);
    _midiResetChannelTable(&song->channels);

    for (entryIndex = 0; (s32)entryIndex < ARRAY_SIZE(song->voiceSlots); entryIndex++) {
        _midiResetNoteSlot(&song->voiceSlots[entryIndex]);
    }
}

/// Reads the big-endian 32-bit value at `p`, the form every length in a
/// Standard MIDI File takes.
#define MIDI_READ_BE32(p) (((p)[0] << 24) | ((p)[1] << 16) | ((p)[2] << 8) | (p)[3])

/// Locates a track's first delta-time byte in a loaded MIDI chunk image.
///
/// Only the low byte of `trackIndex` matters; valid indices are 0..17 within
/// the active track count. Track zero follows the file-header chunk in
/// `sequenceData`. Later tracks follow the preceding track's complete chunk,
/// whose startup cursor must already be set. Chunk lengths are big-endian byte
/// counts. Headers and resulting cursors must lie within the borrowed image;
/// this helper does not validate tags, lengths or bounds.
static u8* _midiResolveTrackData(_MidiSong* song, s32 trackIndex, u8* sequenceData)
{
    enum { MIDI_CHUNK_HEADER_BYTES = 8 };
    u32 chunkBytes;

    if ((u8)trackIndex != 0) {
        sequenceData = song->tracks[(u8)trackIndex - 1].savedCursors.startup.dataStart;
        chunkBytes   = MIDI_READ_BE32(sequenceData - sizeof(chunkBytes));
        return sequenceData + chunkBytes + MIDI_CHUNK_HEADER_BYTES;
    }
    chunkBytes = MIDI_READ_BE32(sequenceData + sizeof(chunkBytes));
    return sequenceData + MIDI_CHUNK_HEADER_BYTES + chunkBytes + MIDI_CHUNK_HEADER_BYTES;
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

/// Queues ADSR release rate 5 and key-off for a song's allocated note voices.
///
/// The exponential-release bit and other ADSR2 bits are retained. An already
/// off key with an active envelope gets the rate update without another key-off;
/// fully off voices are skipped. Slots and callbacks stay live through release.
static void _midiKeyOffSongVoices(_MidiSong* song)
{
    enum {
        MIDI_ADSR_RELEASE_RATE_MASK = 0x1F,
        MIDI_ADSR_FAST_RELEASE_RATE = 5
    };
    s32            voiceIndex;
    _MidiNoteSlot* slot;
    u8             keyStatus;
    SpuVoiceRef    voiceRef;
    u16            adsr2;

    /// Queues rate 5 using a halfword scratch lvalue, preserving all other bits.
    ///
    /// `ref` is a SpuVoiceRef with writable attributes; `releaseBits` is a u16
    /// scratch lvalue. Both arguments must be side-effect-free lvalues and are
    /// evaluated repeatedly. Captures this function's release-rate constants.
#define MIDI_SET_FAST_RELEASE(ref, releaseBits)                                                           \
    do {                                                                                                  \
        (releaseBits)     = (ref).attr->adsr2;                                                            \
        (releaseBits)     = ((releaseBits) & ~MIDI_ADSR_RELEASE_RATE_MASK) | MIDI_ADSR_FAST_RELEASE_RATE; \
        (ref).attr->adsr2 = (releaseBits);                                                                \
        (ref).attr->mask |= SPU_VOICE_ADSR_ADSR2;                                                         \
    } while (0)

    voiceIndex = 0;
    slot       = song->voiceSlots;
    do {
        if (slot->voice >= 0) {
            keyStatus = spuGetVoiceKeyStatus(slot->voice);
            if (keyStatus != SPU_OFF) {
                spuGetVoiceRef(slot->voice, &voiceRef);
                MIDI_SET_FAST_RELEASE(voiceRef, adsr2);
                if (keyStatus != SPU_OFF_ENV_ON) {
                    spuKeyOff(slot->voice);
                }
            }
        }
        voiceIndex++;
        slot++;
    } while (voiceIndex < ARRAY_SIZE(song->voiceSlots));
#undef MIDI_SET_FAST_RELEASE
}

/// Advances one active track's fractional clock and dispatches every due event.
///
/// Delta counts are MIDI ticks, with a fractional denominator of 6000 in PAL
/// and 3600 otherwise. Zero deltas dispatch within the same update. A byte
/// without bit 7 is implicit note-on data on the last explicit channel, even
/// after another channel-event class. The image must contain readable events
/// and terminating VLQs, with loop/call targets inside it. A NULL handler
/// result schedules stopping; end-of-track retains the remaining-tick subtraction.
/// Zero-delta event chains must reach a positive delta or end before cycling.
static void _midiAdvanceTrack(_MidiSong* song, _MidiTrack* track)
{
    enum {
        MIDI_STATUS_FLAG           = 0x80,
        MIDI_STATUS_CLASS_MASK     = 0xF0,
        MIDI_STATUS_CLASS_SHIFT    = 4,
        MIDI_STATUS_FIRST_CLASS    = 8,
        MIDI_STATUS_NOTE_ON        = 0x90,
        MIDI_STATUS_SYSTEM         = 0xF0,
        MIDI_NOTE_ON_HANDLER_INDEX = (MIDI_STATUS_NOTE_ON >> MIDI_STATUS_CLASS_SHIFT) - MIDI_STATUS_FIRST_CLASS
    };
    u8  deltaBytes;
    u32 tickNumerator;
    s32 ticksLeft;
    s32 ticksAdvanced;
    u32 tickRemainder;
    u8  status;

    // Carry the fractional numerator while advancing by whole MIDI ticks.
    tickNumerator = track->tickFraction + (song->currentTempoBpm + song->currentTempoOffsetBpm) * song->ticksPerQuarter;
    if (gDisplayState.region == MODE_PAL) {
        ticksAdvanced = tickNumerator / MIDI_TRACK_PAL_TICK_DIVISOR;
    } else {
        ticksAdvanced = tickNumerator / MIDI_TRACK_NTSC_TICK_DIVISOR;
    }
    if (gDisplayState.region == MODE_PAL) {
        tickRemainder = tickNumerator % MIDI_TRACK_PAL_TICK_DIVISOR;
    } else {
        tickRemainder = tickNumerator % MIDI_TRACK_NTSC_TICK_DIVISOR;
    }
    track->tickFraction = tickRemainder;
    song->songTicks    += ticksAdvanced;
    ticksLeft           = ticksAdvanced;
    if (song->sequenceId == MIDI_SEQUENCE_AREA_VOLUME) {
        song->volumeDirtyChannels = MIDI_SONG_ALL_CHANNELS_DIRTY;
    }
    while (ticksLeft >= track->ticksUntilEvent) {
        ticksLeft             -= track->ticksUntilEvent;
        track->ticksUntilEvent = 0;
        do {
            status = *track->eventCursor;
            // Only explicit channel statuses replace the saved implicit-note channel.
            if (status & MIDI_STATUS_FLAG) {
                track->implicitNoteOn = false;
                if ((status & MIDI_STATUS_CLASS_MASK) != MIDI_STATUS_SYSTEM) {
                    track->runningChannel = status & MIDI_CHANNEL_STATUS_MASK;
                }
                track->eventCursor = Midi_EventFns[((status & MIDI_STATUS_CLASS_MASK) >> MIDI_STATUS_CLASS_SHIFT) - MIDI_STATUS_FIRST_CLASS](status, track->eventCursor, song, track);
            } else {
                track->implicitNoteOn = true;
                track->eventCursor    = Midi_EventFns[MIDI_NOTE_ON_HANDLER_INDEX](track->runningChannel | MIDI_STATUS_NOTE_ON, track->eventCursor - 1, song, track);
            }
            if (track->ended != false) {
                track->ticksUntilEvent -= ticksLeft;
                return;
            }
            if (track->eventCursor == NULL) {
                track->nrpnMsb      = MIDI_TRACK_NRPN_IDLE;
                track->tickFraction = 0;
                song->status        = MIDI_SONG_STOPPING;
                return;
            }
            track->ticksUntilEvent = _midiReadDeltaTime(track->eventCursor, &deltaBytes);
            track->eventCursor    += deltaBytes;
        } while (track->ticksUntilEvent == 0);
    }
    track->ticksUntilEvent -= ticksLeft;
}

/// Queues volume and pan changes for allocated notes on dirty MIDI channels.
///
/// Live channel volume/expression and velocity multiply saved program/layer
/// gain. Master gain and the song fade apply independently. Note channels must
/// be 0..15, velocities 1..127, and ordinary sequence ids 0..99 for the mix table.
/// Sequence 0x4F uses Neo Ark area gain only in stage 5; 0x5A always uses startup
/// gain and ignores the music-output mute gate. The caller clears the dirty mask.
static void _midiUpdateVoiceVolumes(_MidiSong* song)
{
    // The velocity curve reaches 16383; retain a 0..127 channel gain before
    // combining it with the note's gain and the song's SPU volume.
    enum {
        MIDI_VELOCITY_GAIN_FULL   = 0x3FFF,
        MIDI_CHANNEL_GAIN_DIVISOR = SOUND_EVENT_MIDI_VOLUME_FULL * MIDI_VELOCITY_GAIN_FULL,
        MIDI_VOICE_GAIN_DIVISOR   = SOUND_EVENT_MIDI_VOLUME_FULL * SOUND_EVENT_MIDI_VOLUME_FULL,
        MIDI_MUSIC_MUTE_ENABLED   = 1
    };
    SpuVoiceRef    voiceRef;
    SpuVolume      panVolumes;
    LinInterp*     volumeRamp;
    s32            songVolume;
    s32            slotIndex;
    _MidiNoteSlot* slot;
    _MidiChannel*  channelControls;
    s32            channelGain;
    u32            voiceVolume;
    s32            channelIndex;
    s32            channelMask;
    s32            panOffset;
    s8             voiceIndex;

    volumeRamp = &song->volumeRamp;
    // Apply the sequence's policy before mixing the individual notes.
    if (song->sequenceId == MIDI_SEQUENCE_AREA_VOLUME && D_80082120 == GAME_STAGE_SHELTER_NEO_ARK) {
        songVolume = mapNeoArkUpdateMusicVolume((u16)song->volumeScale, D_80082136, volumeRamp);
    } else if (song->sequenceId == MIDI_SEQUENCE_FIXED_GAIN) {
        songVolume = linInterpApply(volumeRamp, (u32)((midiGetMasterVolume() & 0xFF) * ((D_800689F0[MIDI_SEQUENCE_FIXED_GAIN] * MIDI_STARTUP_GAIN_MULTIPLIER) << MIDI_STARTUP_GAIN_SHIFT)) / (u32)SOUND_EVENT_MIDI_VOLUME_FULL);
    } else {
        songVolume = linInterpApply(volumeRamp, (u32)((midiGetMasterVolume() & 0xFF) * (u16)song->volumeScale) / (u32)SOUND_EVENT_MIDI_VOLUME_FULL);
    }
    slotIndex = 0;
    slot      = song->voiceSlots;
    do {
        voiceIndex = slot->voice;
        if (voiceIndex >= 0) {
            channelIndex = (u8)slot->channel;
            channelMask  = 1 << channelIndex;
            if (song->volumeDirtyChannels & channelMask) {
                channelControls = &song->channels.entries[channelIndex];
                channelGain     = channelControls->volume * channelControls->expression * Snd_VelocityGainTable[slot->velocity];
                channelGain     = channelGain / MIDI_CHANNEL_GAIN_DIVISOR;
                voiceVolume     = (u32)(songVolume * slot->volumeScale * channelGain) / (u32)MIDI_VOICE_GAIN_DIVISOR;
                panOffset       = channelControls->pan - MIDI_CHANNEL_PAN_CENTER;
                spuCalcPanVolumes(&panVolumes, slot->pan + panOffset, voiceVolume);
                spuGetVoiceRef(voiceIndex, &voiceRef);
                if (D_800820E9 == MIDI_MUSIC_MUTE_ENABLED && song->sequenceId != MIDI_SEQUENCE_FIXED_GAIN) {
                    voiceRef.attr->volume.left  = 0;
                    voiceRef.attr->volume.right = 0;
                } else {
                    voiceRef.attr->volume.left  = panVolumes.left;
                    voiceRef.attr->volume.right = panVolumes.right;
                }
                voiceRef.attr->volmode.left  = SPU_VOICE_DIRECT;
                voiceRef.attr->volmode.right = SPU_VOICE_DIRECT;
                voiceRef.attr->mask         |= SPU_VOICE_VOLL | SPU_VOICE_VOLR | SPU_VOICE_VOLMODEL | SPU_VOICE_VOLMODER;
            }
        }
        slotIndex++;
        slot++;
    } while (slotIndex < ARRAY_SIZE(song->voiceSlots));
}

/// Queues key-off for every note slot playing the event's key on its channel.
///
/// `event` borrows the sequence image and provides the key in `event[1]`.
/// The channel's note-event gate suppresses release. Slots and callbacks remain
/// live through their release envelopes. Note-off consumes two bytes in this
/// format; a zero-velocity note-on consumes three. Returns the next delta cursor.
static inline u8* _midiNoteOff(s32 status, u8* event, _MidiSong* song)
{
    enum {
        MIDI_STATUS_CLASS_MASK    = 0xF0,
        MIDI_STATUS_NOTE_ON       = 0x90,
        MIDI_NOTE_OFF_EVENT_BYTES = 2
    };
    s32 voiceIndex;
    u8  channel;
    u8  key;
    u8* cursor;

    cursor  = event;
    channel = status & MIDI_CHANNEL_STATUS_MASK;
    key     = cursor[1];
    if ((status & MIDI_STATUS_CLASS_MASK) == MIDI_STATUS_NOTE_ON) {
        cursor += 1;
    }
    if (song->channels.entries[channel].noteEventsDisabled != 0) {
        return cursor + MIDI_NOTE_OFF_EVENT_BYTES;
    }
    for (voiceIndex = 0; voiceIndex < ARRAY_SIZE(song->voiceSlots); voiceIndex++) {
        if ((song->voiceSlots[voiceIndex].key == key) && (song->voiceSlots[voiceIndex].channel == channel)) {
            spuKeyOff(song->voiceSlots[voiceIndex].voice);
        }
    }
    return cursor + MIDI_NOTE_OFF_EVENT_BYTES;
}

/// Stores combined program/layer pan, clamped to the stereo range 0..127.
///
/// `pan` is the signed-halfword sum of both bank pans minus centre (64).
/// `slot` is writable and retains this base pan; the live channel's offset is
/// added during voice mixing, after this clamp. Only the pan byte is changed.
static inline void _midiStoreClampedNotePan(_MidiNoteSlot* slot, s16 pan)
{
    enum { SOUND_BANK_PAN_MAX = 127 };

    if (pan <= SOUND_BANK_PAN_MAX) {
        if (pan >= 0) {
            slot->pan = pan;
        } else {
            slot->pan = 0;
        }
    } else {
        slot->pan = SOUND_BANK_PAN_MAX;
    }
}

/// Starts one SPU voice per matching bank layer for a channel's note-on event.
///
/// `event[1]` is a MIDI key and `event[2]` its velocity, both 0..127; zero
/// velocity releases that key instead. The channel program must select a live
/// bank group, and its layers must fit the bank tables. The note-event gate
/// suppresses both paths. Priority zero tries the shared voice range before
/// the music range; other priorities reverse that order. Each allocated voice
/// retains its song slot as callback context until release or reassignment.
/// The loaded bank must remain valid while those notes play. Returns the next
/// delta cursor, `event + 3`; `track` is unused.
static u8* _midiHandleNoteOn(s32 status, u8* event, _MidiSong* song, _MidiTrack* track)
{
    enum {
        SOUND_BANK_VOLUME_FRACTION_BITS  = 7,
        SOUND_BANK_PAN_CENTER            = 64,
        MIDI_VOICE_RANGE_MUSIC           = 0,
        MIDI_VOICE_RANGE_SHARED          = 2,
        MIDI_VOICE_PRIORITY_SHARED_FIRST = 0,
        MIDI_NOTE_VELOCITY_RELEASE       = 0,
        MIDI_NOTE_ON_EVENT_BYTES         = 3
    };
    s16            rangeIndices[2];
    SpuVoiceRef    voiceRef;
    u8             channel;
    u8             program;
    u8             key;
    u8             velocity;
    u8             layerIndex;
    s8             voiceIndex;
    u16            priority;
    s16            pan;
    s32            reverb;
    s32            pitchBend;
    s32            pitchProduct;
    s32            bendRangeSemitones;
    SndBankGroup*  group;
    SndBankLayer*  bankLayer;
    _MidiNoteSlot* slot;
    SpuVoiceAttr*  attr;

    velocity = event[2];
    if (velocity == MIDI_NOTE_VELOCITY_RELEASE) {
        event = _midiNoteOff(status, event, song);
    } else {
        channel = status & MIDI_CHANNEL_STATUS_MASK;
        if (song->channels.entries[channel].noteEventsDisabled != 0) {
            return event + MIDI_NOTE_ON_EVENT_BYTES;
        }
        program   = song->channels.entries[channel].program;
        group     = &song->groups[program];
        key       = event[1];
        bankLayer = sndBankGetLayer(song->bank, program, 0);
        for (layerIndex = 0; layerIndex < group->layerCount; layerIndex++, bankLayer++) {
            priority = bankLayer->priority;
            if (key >= bankLayer->keyMin && bankLayer->keyMax >= key) {
                if (priority == MIDI_VOICE_PRIORITY_SHARED_FIRST) {
                    rangeIndices[0] = MIDI_VOICE_RANGE_SHARED;
                    rangeIndices[1] = MIDI_VOICE_RANGE_MUSIC;
                } else {
                    rangeIndices[0] = MIDI_VOICE_RANGE_MUSIC;
                    rangeIndices[1] = MIDI_VOICE_RANGE_SHARED;
                }
                voiceIndex = spuAllocVoice(rangeIndices, ARRAY_SIZE(rangeIndices), priority);
                if (voiceIndex >= 0) {
                    slot                       = &song->voiceSlots[voiceIndex];
                    song->volumeDirtyChannels |= 1 << channel;
                    spuSetVoiceCallback(voiceIndex, _midiOnVoiceReleased, slot);
                    spuGetVoiceRef(voiceIndex, &voiceRef);
                    slot->voice       = voiceIndex;
                    slot->channel     = channel;
                    slot->velocity    = velocity;
                    slot->key         = key;
                    slot->volumeScale = (group->volume * bankLayer->volume) >> SOUND_BANK_VOLUME_FRACTION_BITS;
                    pan               = group->pan + bankLayer->pan - SOUND_BANK_PAN_CENTER;
                    _midiStoreClampedNotePan(slot, pan);
                    slot->program = program;
                    slot->layer   = layerIndex;
                    reverb        = bankLayer->reverb;
                    if (reverb == SPU_ON) {
                        spuEnableVoiceReverb(slot->voice);
                        slot->reverbEnabled = reverb;
                    } else {
                        spuDisableVoiceReverb(slot->voice);
                        slot->reverbEnabled = SPU_OFF;
                    }
                    // Scale the signed wheel by this layer's semitone range into Q8 pitch.
                    pitchBend = song->channels.entries[channel].pitchBend;
                    if (pitchBend != 0) {
                        if (pitchBend > 0) {
                            bendRangeSemitones = bankLayer->bendUp;
                        } else {
                            bendRangeSemitones = bankLayer->bendDown;
                        }
                        pitchProduct      = (bendRangeSemitones << MIDI_PITCH_FRACTION_BITS) * pitchBend;
                        slot->pitchOffset = pitchProduct / MIDI_PITCH_BEND_MAX;
                    }
                    attr        = voiceRef.attr;
                    attr->addr  = bankLayer->waveAddr;
                    attr->adsr1 = bankLayer->adsr1;
                    attr->adsr2 = bankLayer->adsr2;
                    attr->pitch = spuCalcPitch(key, slot->pitchOffset, bankLayer->rootKey, bankLayer->fineTune);
                    attr->mask  = SPU_VOICE_WDSA | SPU_VOICE_ADSR_ADSR1 | SPU_VOICE_ADSR_ADSR2 | SPU_VOICE_PITCH;
                    spuKeyOn(slot->voice);
                }
            }
        }
        event += MIDI_NOTE_ON_EVENT_BYTES;
    }
    return event;
}

/// Decodes channel mix controllers and the sequence's reverb/loop NRPN commands.
///
/// `event` provides status plus two data bytes in the borrowed image; `status`
/// selects channel 0..15. Gains and pan require 0..127. CC 99 selects a loop
/// start (20), loop end (30), or reverb depth (16, also requiring CC 98 = 16).
/// Loop-start data entry saves the following delta and a further-jump count;
/// bit 7 maps that count to 127, and 127 means unbounded. A loop end resumes
/// the saved delta while the count permits. Reverb data scales by 256 into a
/// signed-halfword depth for both SPU channels. Returns the next delta cursor,
/// normally `event + 3`, or the saved loop cursor, which must stay in the image.
static u8* _midiHandleControlChange(s32 status, u8* event, _MidiSong* song, _MidiTrack* track)
{
    enum {
        MIDI_TRACK_CONTROL_DATA_ENTRY   = 6,
        MIDI_TRACK_CONTROL_NRPN_LSB     = 0x62,
        MIDI_TRACK_CONTROL_NRPN_MSB     = 0x63,
        MIDI_CHANNEL_CONTROL_VOLUME     = 7,
        MIDI_CHANNEL_CONTROL_PAN        = 10,
        MIDI_CHANNEL_CONTROL_EXPRESSION = 11,
        MIDI_CONTROL_CHANGE_EVENT_BYTES = 3,
        MIDI_REVERB_DEPTH_SHIFT         = 8
    };
    u8  channel;
    u8  controller;
    s32 nrpnValue;
    u8  nrpnSelector;

    channel    = status & MIDI_CHANNEL_STATUS_MASK;
    controller = event[1];

    switch (controller) {
        case MIDI_TRACK_CONTROL_DATA_ENTRY:
            nrpnSelector = track->nrpnMsb;
            if (nrpnSelector != MIDI_TRACK_NRPN_REVERB_DEPTH) {
                if (nrpnSelector != MIDI_TRACK_NRPN_LOOP_START) {
                    return event + MIDI_CONTROL_CHANGE_EVENT_BYTES;
                }
                if (track->loopRepeatsLeft != 0) {
                    return event + MIDI_CONTROL_CHANGE_EVENT_BYTES;
                }
                // Count later jumps from the delta immediately after this data entry.
                track->loopCursor = event + MIDI_CONTROL_CHANGE_EVENT_BYTES;
                if ((s8)event[2] >= 0) {
                    track->loopRepeatsLeft = event[2];
                } else {
                    track->loopRepeatsLeft = MIDI_TRACK_LOOP_FOREVER;
                }
                track->nrpnMsb = MIDI_TRACK_NRPN_IDLE;
            } else {
                if (track->nrpnLsb != nrpnSelector) {
                    return event + MIDI_CONTROL_CHANGE_EVENT_BYTES;
                }
                spuSetReverbDepth((s16)(event[2] << MIDI_REVERB_DEPTH_SHIFT));
                track->nrpnMsb = MIDI_TRACK_NRPN_IDLE;
                track->nrpnLsb = MIDI_TRACK_NRPN_IDLE;
            }
            break;

        case MIDI_CHANNEL_CONTROL_VOLUME:
            song->channels.entries[channel].volume = event[2];
            song->volumeDirtyChannels             |= 1 << channel;
            break;

        case MIDI_CHANNEL_CONTROL_PAN:
            if (sndOutputIsStereo()) {
                song->channels.entries[channel].pan = event[2];
            } else {
                song->channels.entries[channel].pan = MIDI_CHANNEL_PAN_CENTER;
            }
            song->volumeDirtyChannels |= 1 << channel;
            break;

        case MIDI_CHANNEL_CONTROL_EXPRESSION:
            song->channels.entries[channel].expression = event[2];
            song->volumeDirtyChannels                 |= 1 << channel;
            break;

        case MIDI_TRACK_CONTROL_NRPN_LSB:
            track->nrpnLsb = event[2];
            break;

        case MIDI_TRACK_CONTROL_NRPN_MSB:
            nrpnValue      = event[2];
            track->nrpnMsb = nrpnValue;
            if ((nrpnValue & 0xFF) == MIDI_TRACK_NRPN_LOOP_START) {
                break;
            }
            if ((nrpnValue & 0xFF) != MIDI_TRACK_NRPN_LOOP_END) {
                return event + MIDI_CONTROL_CHANGE_EVENT_BYTES;
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
            return event + MIDI_CONTROL_CHANGE_EVENT_BYTES;
    }

    return event + MIDI_CONTROL_CHANGE_EVENT_BYTES;
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

/// Converts a tempo meta event to the pending BPM and advances to the next delta.
///
/// `cursor` borrows five readable bytes starting at the meta type: type, length
/// and the three-byte big-endian microseconds-per-quarter value, which must be
/// nonzero. The BPM is truncated to a byte, its offset is cleared, and the new
/// tempo is published after all tracks advance. Returns `cursor + 5`.
static inline u8* _midiApplyTempoMeta(_MidiSong* song, u8* cursor)
{
    enum { MIDI_TEMPO_META_BYTES = 5 };
    u32 microsecondsPerQuarter;

    microsecondsPerQuarter      = cursor[2] << 16;
    microsecondsPerQuarter     |= cursor[3] << 8;
    microsecondsPerQuarter     |= cursor[4];
    cursor                     += MIDI_TEMPO_META_BYTES;
    song->pendingTempoOffsetBpm = 0;
    song->pendingTempoBpm       = MIDI_MICROSECONDS_PER_MINUTE / microsecondsPerQuarter;
    return cursor;
}

/// Decodes meta events, SysEx boundaries and the sequence format's relative calls and returns.
///
/// `event` addresses the status byte in the borrowed sequence image; `status`
/// is unused. Returns the next delta cursor or `NULL` on an unsupported system
/// event, call-stack overflow or a negative-depth return. Valid streams call
/// at depths 0..8 and return at depths 1..9: a return at zero is not rejected
/// before the retained decrement. Relative call displacements are signed,
/// big-endian 16-bit byte offsets from the end of the three-byte command.
/// All cursors, length-delimited payloads and return targets must remain in
/// the image. SysEx must have a terminator and a following byte to skip. Tempo
/// events must carry three bytes with a nonzero microseconds-per-quarter value;
/// the converted BPM is truncated to the song's pending byte for its next update.
static u8* _midiHandleSystemEvent(s32 status, u8* event, _MidiSong* song, _MidiTrack* track)
{
    enum {
        MIDI_TRACK_SYSTEM_EXCLUSIVE     = 0xF0,
        MIDI_TRACK_COMMAND_CALL         = 0xF5,
        MIDI_TRACK_COMMAND_RETURN       = 0xF6,
        MIDI_TRACK_SYSTEM_END_EXCLUSIVE = 0xF7,
        MIDI_TRACK_META_EVENT           = 0xFF,
        MIDI_TRACK_META_END             = 0x2F,
        MIDI_TRACK_META_TEMPO           = 0x51,
        MIDI_TRACK_CALL_BYTES           = 3
    };
    u8  lengthBytes;
    s32 payloadBytes;
    u8* cursor;
    u8  systemStatus;
    s8  returnIndex;

    cursor = event;
    switch (*cursor) {
        case MIDI_TRACK_SYSTEM_EXCLUSIVE:
            // Skip the SysEx terminator and the byte immediately following it.
            systemStatus = *cursor;
            cursor      += 1;
            if (systemStatus != MIDI_TRACK_SYSTEM_END_EXCLUSIVE) {
                do {
                } while (*cursor++ != MIDI_TRACK_SYSTEM_END_EXCLUSIVE);
            }
            cursor += 1;
            break;
        case MIDI_TRACK_COMMAND_CALL:
            // Save the return delta, then jump relative to the end of the call command.
            if (track->callDepth < ARRAY_SIZE(track->savedCursors.returnAddresses)) {
                track->callLatched                                    = true;
                track->savedCursors.returnAddresses[track->callDepth] = cursor + MIDI_TRACK_CALL_BYTES;
                track->callDepth                                      = (u8)track->callDepth + 1;
                cursor                                                = cursor + ((s16)((cursor[1] << 8) | cursor[2]) + MIDI_TRACK_CALL_BYTES);
            } else {
                cursor = NULL;
            }
            break;
        case MIDI_TRACK_COMMAND_RETURN:
            if (track->callDepth < 0) {
                track->callLatched = false;
                cursor             = NULL;
            } else {
                returnIndex      = (u8)track->callDepth - 1;
                track->callDepth = returnIndex;
                cursor           = track->savedCursors.returnAddresses[returnIndex];
            }
            break;
        case MIDI_TRACK_SYSTEM_END_EXCLUSIVE:
            cursor += 1;
            break;
        case MIDI_TRACK_META_EVENT:
            cursor += 1;
            switch (*cursor) {
                case MIDI_TRACK_META_END:
                    track->ended = true;
                    cursor      += 1;
                    break;
                case MIDI_TRACK_META_TEMPO:
                    cursor = _midiApplyTempoMeta(song, cursor);
                    break;
                default: {
                    s32 headerBytes;

                    payloadBytes = _midiReadVlq(cursor + 1, &lengthBytes);
                    // Skip the meta type, encoded length and the complete payload.
                    headerBytes = lengthBytes + 1;
                    cursor     += payloadBytes + headerBytes;
                } break;
            }
            break;
        default:
            cursor = NULL;
            break;
    }
    return cursor;
}

/// Reads a track delta in MIDI ticks and writes its encoded byte count.
///
/// `data` borrows the loaded sequence image; it and `byteCount` must satisfy
/// `_midiReadVlq`'s readable-stream and writable-output requirements.
static s32 _midiReadDeltaTime(const u8* data, u8* byteCount)
{
    return _midiReadVlq(data, byteCount);
}

/// Restores the complete sixteen-channel control table, or ignores `NULL`.
///
/// A non-NULL argument must be writable. Notes start enabled, volume is 64,
/// expression 127, pan centered at 64, and program and pitch bend zero. The
/// uninterpreted byte in each record is cleared along with the known controls.
static void _midiResetChannelTable(_MidiChannelTable* channels)
{
    // Little-endian first word: note events enabled, volume 64, expression
    // 127 and neutral pan. The second word resets program and bend to zero.
    enum {
        MIDI_CHANNEL_VOLUME_DEFAULT      = 64,
        MIDI_CHANNEL_RESET_CONTROLS_WORD = (MIDI_CHANNEL_PAN_CENTER << 24) |
                                           (SOUND_EVENT_MIDI_VOLUME_FULL << 16) |
                                           (MIDI_CHANNEL_VOLUME_DEFAULT << 8)
    };
    s32  channelIndex;
    u32* channelWords;

    if (channels != NULL) {
        channelWords = channels->words;
        for (channelIndex = 0; channelIndex < ARRAY_SIZE(channels->entries); channelIndex++) {
            *channelWords++ = MIDI_CHANNEL_RESET_CONTROLS_WORD;
            *channelWords++ = 0;
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

/// Decodes the sequence format's two-byte note-off event for one channel key.
///
/// `event[1]` is the key (0..127); no release-velocity byte is consumed.
/// Queues release for all matching layers unless the channel gate suppresses
/// note events. Returns `event + 2`, the next delta cursor; `track` is unused.
static u8* _midiHandleNoteOff(s32 status, u8* event, _MidiSong* song, _MidiTrack* track)
{
    return _midiNoteOff(status, event, song);
}

/// Selects the bank program for subsequent notes on the status byte's channel.
///
/// `event` addresses the status byte and supplies the program in `event[1]`,
/// which must be below the loaded bank's group count. Existing notes retain
/// their original program. Returns the next delta cursor, `event + 2`;
/// `track` is unused.
static u8* _midiHandleProgramChange(s32 status, u8* event, _MidiSong* song, _MidiTrack* track)
{
    song->channels.entries[status & MIDI_CHANNEL_STATUS_MASK].program = event[1];
    return event + 2;
}

/// Saves a channel's pitch wheel and retunes all of its playing sample layers.
///
/// `event[1]` and `event[2]` are low/high seven-bit wheel bytes, giving the
/// signed offset -8192..8191 about centre. Each layer's upward or downward
/// semitone range scales it into Q8 pitch with denominator 8191 for both signs.
/// The wheel also applies to later notes. Voice slots retain live bank layers
/// and valid voice indices 0..17. Returns `event + 3`; `track` is unused.
static u8* _midiHandlePitchBend(s32 status, u8* event, _MidiSong* song, _MidiTrack* track)
{
    enum {
        MIDI_PITCH_WHEEL_CENTER     = 0x2000,
        MIDI_PITCH_WHEEL_DATA_BITS  = 7,
        MIDI_PITCH_BEND_EVENT_BYTES = 3
    };
    SpuVoiceRef    voiceRef;
    u8             channel;
    s32            voiceIndex;
    s16            pitchBend;
    _MidiNoteSlot* slot;
    SndBankLayer*  bankLayer;
    s32            pitchProduct;
    s16            pitchOffset;
    SpuVoiceAttr*  attr;

    channel                                   = status & MIDI_CHANNEL_STATUS_MASK;
    voiceIndex                                = 0;
    pitchBend                                 = (event[1] | (event[2] << MIDI_PITCH_WHEEL_DATA_BITS)) - MIDI_PITCH_WHEEL_CENTER;
    song->channels.entries[channel].pitchBend = pitchBend;
    do {
        slot = &song->voiceSlots[voiceIndex];
        if (slot->channel == channel) {
            spuGetVoiceRef(slot->voice, &voiceRef);
            bankLayer = sndBankGetLayer(song->bank, slot->program, slot->layer);
            if (pitchBend >= 0) {
                pitchProduct   = bankLayer->bendUp;
                pitchProduct <<= MIDI_PITCH_FRACTION_BITS;
            } else {
                pitchProduct   = bankLayer->bendDown;
                pitchProduct <<= MIDI_PITCH_FRACTION_BITS;
            }
            pitchProduct     *= pitchBend;
            pitchOffset       = pitchProduct / MIDI_PITCH_BEND_MAX;
            slot->pitchOffset = pitchOffset;
            attr              = voiceRef.attr;
            attr->pitch       = spuCalcPitch((u16)slot->key, pitchOffset, bankLayer->rootKey, bankLayer->fineTune);
            attr->mask       |= SPU_VOICE_PITCH;
        }
        voiceIndex += 1;
    } while (voiceIndex < ARRAY_SIZE(song->voiceSlots));
    return event + MIDI_PITCH_BEND_EVENT_BYTES;
}

s32 sndLoadProcessSector(u32* payloadWords)
{
    enum {
        SOUND_LOAD_SAMPLE_BLOCK_SHIFT          = 6,
        SOUND_LOAD_SAMPLE_BLOCK_BYTES          = 1 << SOUND_LOAD_SAMPLE_BLOCK_SHIFT,
        SOUND_LOAD_COMMON_SAMPLE_BASE          = 0x63810,
        SOUND_LOAD_FIRST_UNSUPPORTED_BANK_TYPE = 8,
        SOUND_LOAD_LAST_UNSUPPORTED_BANK_TYPE  = 13
    };
    SndLoadState* state;
    u32*          sourceWords;
    u32           wordIndex;
    u32*          destinationWords;
    s32           bankType;
    s32           tableWordCount;
    s32           alignedImageBytes;
    void*         imageBuffer;
    s32           transferCount; // Image words in COPY_IMAGE; sample bytes in UPLOAD_WAVE
    s32           spuAddr;

    state = &SndLoad_State;
    switch (state->phase) {
        case SOUND_LOAD_PHASE_HEADER:
            // Retain the hSPK header. Group and layer tables follow it in this sector.
            sourceWords      = payloadWords;
            destinationWords = state->payload.words;
            wordIndex        = 0;
            do {
                *destinationWords = *sourceWords;
                sourceWords++;
                wordIndex++;
                destinationWords++;
            } while (wordIndex < ARRAY_SIZE(state->payload.words));

            bankType = state->payload.header.bankId & SOUND_BANK_TYPE_MASK;
            // Slot-map types 8 through 13 cannot install a sound bank.
            if ((u32)(bankType - (SOUND_LOAD_FIRST_UNSUPPORTED_BANK_TYPE << 12)) <
                (u32)((SOUND_LOAD_LAST_UNSUPPORTED_BANK_TYPE - SOUND_LOAD_FIRST_UNSUPPORTED_BANK_TYPE) * (1 << 12) + 1)) {
                D_800689E8   = SOUND_LOAD_FAILURE_INVALID_TYPE;
                state->phase = SOUND_LOAD_PHASE_ERROR;
                break;
            }
            if (bankType == SOUND_BANK_TYPE_1) {
                D_80082128 = 0;
            }
            {
                // Publish the accepted id before the previous bank is released.
                s32 bankId;
                bankId         = state->payload.header.bankId;
                gSndLoadBankId = bankId;
                if (_sndLoadPrepareBankSlot(state->payload.header.bankId, state->payload.header.imageKind) == SOUND_LOAD_RESULT_ERROR) {
                    state->phase = SOUND_LOAD_PHASE_WAIT_FAIL;
                    break;
                }
            }
            {
                SndBank* allocatedBank;
                allocatedBank = sndBankAllocTables(&state->payload.header);
                state->bank   = allocatedBank;
                if (allocatedBank == 0) {
                    state->phase = SOUND_LOAD_PHASE_WAIT_FAIL;
                    break;
                }
                sourceWords      = payloadWords + ARRAY_SIZE(state->payload.words);
                destinationWords = allocatedBank->heapBlock;
            }
            tableWordCount = (state->payload.header.layerCount * (s32)(sizeof(*state->bank->layers) / sizeof(*destinationWords))) + state->payload.header.groupCount * (s32)(sizeof(*state->bank->groups) / sizeof(*destinationWords));
            wordIndex      = 0;
            if (tableWordCount != 0) {
                do {
                    *destinationWords = *sourceWords;
                    sourceWords++;
                    wordIndex++;
                    destinationWords++;
                } while ((s32)wordIndex < tableWordCount);
            }
            state->bank->groupCount = state->payload.header.groupCount;
            state->bank->layerCount = state->payload.header.layerCount;
            state->bank->bankId     = state->payload.header.bankId;
            state->bank->waveBytes  = state->payload.header.waveBytes;
            state->phase            = SOUND_LOAD_PHASE_ALLOC_IMAGE;
            break;

        case SOUND_LOAD_PHASE_ALLOC_IMAGE:
            alignedImageBytes     = (state->payload.header.imageBytes + sizeof(u32) - 1) & SOUND_LOAD_IMAGE_LENGTH_MASK;
            state->bytesRemaining = alignedImageBytes;
            imageBuffer           = _sndLoadAllocImageBuffer(state->payload.header.bankId, state->payload.header.imageKind, alignedImageBytes);
            state->imageBuffer    = imageBuffer;
            if (imageBuffer == 0) {
                state->phase = SOUND_LOAD_PHASE_WAIT_FAIL;
                sndBankFree(state->bank);
                state->bank = 0;
                break;
            }
            state->writeCursor = imageBuffer;
            state->phase       = SOUND_LOAD_PHASE_COPY_IMAGE;
            /* fallthrough */
        case SOUND_LOAD_PHASE_COPY_IMAGE:
            transferCount = (u32)state->bytesRemaining / sizeof(u32);
            if ((u32)state->bytesRemaining < (u32)state->sectorBytes) {
                state->phase = SOUND_LOAD_PHASE_BEGIN_WAVE;
            } else {
                transferCount          = (u32)state->sectorBytes / sizeof(u32);
                state->bytesRemaining -= state->sectorBytes;
            }
            sourceWords      = payloadWords;
            destinationWords = (u32*)state->writeCursor;
            wordIndex        = 0;
            if (transferCount != 0) {
                do {
                    *destinationWords = *sourceWords;
                    sourceWords++;
                    wordIndex++;
                    destinationWords++;
                } while (wordIndex < (u32)transferCount);
            }
            state->writeCursor += transferCount * (s32)sizeof(*destinationWords);
            break;

        case SOUND_LOAD_PHASE_BEGIN_WAVE: {
            s32 waveBytes;
            waveBytes             = state->payload.header.waveBytes;
            state->bytesRemaining = waveBytes;
            state->bank->spuAddr  = _sndLoadResolveSampleAddress(
                state->payload.header.imageKind, state->bank->bankId, waveBytes);
            spuAddr = state->bank->spuAddr;
        }
            if (spuAddr == 0) {
                D_800689E8   = SOUND_LOAD_FAILURE_NO_SAMPLE_ADDRESS;
                state->phase = SOUND_LOAD_PHASE_WAIT_FAIL;
                sndBankFree(state->bank);
                state->bank = 0;
                break;
            }
            SpuSetTransferStartAddr(spuAddr + (state->payload.header.waveBlockOffset << SOUND_LOAD_SAMPLE_BLOCK_SHIFT));
            state->phase = SOUND_LOAD_PHASE_UPLOAD_WAVE;
            /* fallthrough */
        case SOUND_LOAD_PHASE_UPLOAD_WAVE: {
            s32 remainingBytes;
            s32 sectorBytes;
            remainingBytes = state->bytesRemaining;
            sectorBytes    = state->sectorBytes;
            if ((u32)sectorBytes >= (u32)remainingBytes) {
                transferCount = remainingBytes;
                state->phase  = SOUND_LOAD_PHASE_DONE;
            } else {
                transferCount         = sectorBytes;
                state->bytesRemaining = remainingBytes - sectorBytes;
            }
        }
            // A polling feed requires the previous DMA to have finished.
            // CD audio keeps its bank; every other feed releases it.
            if (state->syncUpload == 0) {
                if (SpuIsTransferCompleted(SPU_TRANSFER_PEEK) == 0) {
                    if (state->feedMode != SOUND_LOAD_FEED_CD_AUDIO) {
                        sndBankFree(state->bank);
                        state->bank = 0;
                    }
                    D_800689E8   = SOUND_LOAD_FAILURE_TRANSFER_BUSY;
                    state->phase = SOUND_LOAD_PHASE_ERROR;
                    break;
                }
                SpuWritePartly((u8*)payloadWords, transferCount);
            } else {
                SpuWritePartly((u8*)payloadWords, transferCount);
                SpuIsTransferCompleted(SPU_TRANSFER_WAIT);
            }
            break;

        case SOUND_LOAD_PHASE_DONE:
            break;

        case SOUND_LOAD_PHASE_WAIT_FAIL:
            // The failure stands until `transferSectors` sectors have arrived.
            if ((state->sectorsArrived + 1) >= (s32)state->payload.header.transferSectors) {
                D_800689E8 = SOUND_LOAD_FAILURE_DRAINED;
                if ((state->payload.header.bankId & SOUND_BANK_TYPE_MASK) == (SOUND_BANK_TYPE_AREA << 12)) {
                    if (D_80082128 == 0) {
                        D_80082124 = SOUND_LOAD_COMMON_SAMPLE_BASE - ((state->payload.header.waveBytes + SOUND_LOAD_SAMPLE_BLOCK_BYTES - 1) & ~(SOUND_LOAD_SAMPLE_BLOCK_BYTES - 1));
                    } else {
                        D_80082124 = D_80082128 - ((state->payload.header.waveBytes + SOUND_LOAD_SAMPLE_BLOCK_BYTES - 1) & ~(SOUND_LOAD_SAMPLE_BLOCK_BYTES - 1));
                    }
                }
                if ((state->payload.header.bankId & SOUND_BANK_TYPE_MASK) == SOUND_BANK_TYPE_1) {
                    D_80082128 = SOUND_LOAD_COMMON_SAMPLE_BASE - ((state->payload.header.waveBytes + SOUND_LOAD_SAMPLE_BLOCK_BYTES - 1) & ~(SOUND_LOAD_SAMPLE_BLOCK_BYTES - 1));
                }
                state->phase = SOUND_LOAD_PHASE_DONE;
            }
            break;
    }

    state->sectorsArrived += 1;
    return state->phase;
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

/// Binds a submitted script-bank load to its mapped playback slot.
///
/// The load must hold a live descriptor and image, with all declared layers
/// still carrying relative sample addresses. Character-bank placement has
/// already incremented its ordinal; installation selects the preceding slot.
/// Success rebases addresses, builds program indices, transfers the held
/// resources to the slot and checkpoints the placement selectors. Returns zero
/// on success or `SOUND_LOAD_RESULT_ERROR` on loader/slot failure, clearing the
/// published load id in either case. Failure retains the resource pointers.
/// Sample DMA must finish before playback or reuse of its source buffer.
static s32 _sndLoadInstallScriptBank(SndLoadState* load)
{
    SndBank*     bank;
    SndBankSlot* bankSlot;
    u16          bankId;
    s8           slotIndex;

    bank = load->bank;
    if (D_800689E8 != SOUND_LOAD_FAILURE_NONE || (bankId = bank->bankId) == SOUND_BANK_ID_FREE) {
    fail:
        gSndLoadBankId = SOUND_LOAD_BANK_NONE;
        return SOUND_LOAD_RESULT_ERROR;
    }
    slotIndex = Snd_BankSlotsByType[bankId >> 12];
    if (slotIndex == SOUND_BANK_SLOT_UNSUPPORTED) {
        goto fail;
    }
    if ((bankId & SOUND_BANK_TYPE_MASK) == (SOUND_BANK_TYPE_CHARACTER << 12)) {
        // Placement already advanced the ordinal; bind the bank it just placed.
        slotIndex = slotIndex - 1 + D_80082122;
    }
    bankSlot = sndBankSlotGet(slotIndex);
    if (bankSlot == NULL) {
        goto fail;
    }
    bankSlot->bankId  = bank->bankId;
    bankSlot->bank    = bank;
    bankSlot->image   = load->imageBuffer;
    bankSlot->spuAddr = bank->spuAddr;
    _sndBankRebaseLayerWaveAddresses(bank, load->payload.header.layerCount);
    sndBankBuildLayerIndex(bankSlot->bank);
    gSndLoadBankId    = SOUND_LOAD_BANK_NONE;
    load->bank        = 0;
    load->imageBuffer = 0;
    D_8008212C        = D_80082122;
    D_80082121        = D_80082135;
    return 0;
}

/// Installs a completed chunk load or accepts a drained early failure.
///
/// Valid loads select a sequence or script image. Success transfers its bank
/// and image to playback; a drained failure returns zero without installation.
/// Other failures return `SOUND_LOAD_RESULT_ERROR`; a failed script installation
/// releases its image. All paths discard the load's held pointers. Apply once
/// after DONE with live tables on an installable load. The final sample DMA can
/// still be running; playback and source-buffer reuse must wait for it to finish.
static s32 _sndLoadComplete(SndLoadState* load)
{
    SndBank*   bank;
    _MidiSong* song;
    s32        sequenceId;
    s32        result;

    if (D_800689E8 == SOUND_LOAD_FAILURE_DRAINED) {
        gSndLoadBankId = SOUND_LOAD_BANK_NONE;
        result         = 0;
    } else {
        result = SOUND_LOAD_RESULT_ERROR;
        switch (load->payload.header.imageKind) {
            case SOUND_BANK_IMAGE_SEQUENCE:
                bank = load->bank;
                if (D_800689E8 != SOUND_LOAD_FAILURE_NONE || (sequenceId = bank->bankId) == SOUND_BANK_ID_FREE) {
                    gSndLoadBankId = SOUND_LOAD_BANK_NONE;
                } else {
                    sequenceId         &= SOUND_LOAD_SEQUENCE_ID_MASK;
                    song                = _midiPrepareSongForLoad(sequenceId);
                    song->sequenceId    = sequenceId;
                    song->sequenceBytes = (load->payload.header.imageBytes + sizeof(u32) - 1) & SOUND_LOAD_IMAGE_LENGTH_MASK;
                    song->sequenceData  = load->imageBuffer;
                    song->bank          = bank;
                    song->waveBytes     = load->payload.header.waveBytes;
                    _sndBankRebaseLayerWaveAddresses(bank, load->payload.header.layerCount);
                    sndBankBuildLayerIndex(song->bank);
                    result            = 0;
                    gSndLoadBankId    = SOUND_LOAD_BANK_NONE;
                    load->bank        = 0;
                    load->imageBuffer = 0;
                }
                break;
            case SOUND_BANK_IMAGE_SCRIPT:
                result = _sndLoadInstallScriptBank(load);
                if (result == SOUND_LOAD_RESULT_ERROR) {
                    sndHeapFree(load->imageBuffer);
                }
                break;
            default:
                result = SOUND_LOAD_RESULT_ERROR;
                break;
        }
    }
    load->imageBuffer = 0;
    load->bank        = 0;
    return result;
}

void sndLoadBeginSectorLoad(void* sectorBuffer)
{
    _sndLoadResetState(SOUND_LOAD_FEED_SECTOR, sectorBuffer);
}

void sndLoadBeginChunkLoad(u8 syncUpload, void* sectorBuffer)
{
    SndLoad_State.syncUpload = syncUpload;
    D_8008212C               = D_80082122;
    D_80082121               = D_80082135;
    _sndLoadResetState(SOUND_LOAD_FEED_CHUNK, sectorBuffer);
}

void sndLoadTeardown(void)
{
    SndLoadState* state;

    D_80082122 = D_8008212C;
    D_80082135 = D_80082121;
    state      = &SndLoad_State;
    if (state->phase != SOUND_LOAD_PHASE_WAIT_FAIL) {
        state->phase = SOUND_LOAD_PHASE_TORN_DOWN;
        sndHeapFree(state->imageBuffer);
        state->imageBuffer = 0;
        sndBankFree(state->bank);
        state->bank = 0;
    }
}

s32 sndLoadFeedChunkSector(u8* sector)
{
    SndLoadState* state;
    s32           phase;

    if (D_80068A78 != 0) {
        return SOUND_LOAD_RESULT_ERROR;
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
                sector            += SOUND_LOAD_SECTION_HEADER_BYTES;
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
    phase = sndLoadProcessSector((u32*)sector);
    if (phase == SOUND_LOAD_PHASE_ERROR) {
        return SOUND_LOAD_RESULT_ERROR;
    }
    if (phase == SOUND_LOAD_PHASE_DONE) {
        _sndLoadComplete(state);
    }
    return phase;
}

s32 sndLoadFeedSector(u32* payload)
{
    s32 phase;

    phase = sndLoadProcessSector(payload);
    if (phase == SOUND_LOAD_PHASE_ERROR) {
        return SOUND_LOAD_RESULT_ERROR;
    }
    return phase;
}

s32 sndLoadInstallSequence(SndLoadState* load)
{
    SndBank*   bank;
    _MidiSong* song;
    u16        sequenceId;
    u8*        sequenceData;

    bank = load->bank;
    if (D_800689E8 != SOUND_LOAD_FAILURE_NONE || (sequenceId = bank->bankId) == SOUND_BANK_ID_FREE) {
        gSndLoadBankId = SOUND_LOAD_BANK_NONE;
        return SOUND_LOAD_RESULT_ERROR;
    }
    sequenceId         &= SOUND_LOAD_SEQUENCE_ID_MASK;
    song                = _midiPrepareSongForLoad(sequenceId);
    song->sequenceId    = sequenceId;
    song->sequenceBytes = (load->payload.header.imageBytes + sizeof(u32) - 1) & SOUND_LOAD_IMAGE_LENGTH_MASK;
    sequenceData        = load->imageBuffer;
    song->bank          = bank;
    song->sequenceData  = sequenceData;
    song->waveBytes     = load->payload.header.waveBytes;
    _sndBankRebaseLayerWaveAddresses(bank, load->payload.header.layerCount);
    sndBankBuildLayerIndex(song->bank);
    gSndLoadBankId    = SOUND_LOAD_BANK_NONE;
    load->bank        = 0;
    load->imageBuffer = 0;
    return 0;
}

/// Obtains the resident sequence buffer or allocates a bank's script image.
///
/// The low byte of `imageKind` selects the sequence path, which borrows a
/// 10 KiB resident buffer and ignores the requested length. Script loads
/// require a mapped bank type and reserve at least 528 bytes for weapon banks
/// or 360 for PE banks. `imageBytes` is a byte count, word-aligned by the caller;
/// it must fit the resident buffer on the sequence path. Returns NULL on an
/// unsupported script type or allocation failure. Heap images pass to a bank
/// slot on success and must be released on failure or interruption.
static void* _sndLoadAllocImageBuffer(s32 bankId, s32 imageKind, u32 imageBytes)
{
    enum {
        SOUND_LOAD_IMAGE_SIZE_MASK        = 0xFFFF,
        SOUND_LOAD_WEAPON_MIN_IMAGE_BYTES = 0x210,
        SOUND_LOAD_PE_MIN_IMAGE_BYTES     = 0x168
    };
    u16 truncatedBankId;

    truncatedBankId = bankId;
    if ((imageKind & SOUND_LOAD_IMAGE_KIND_BYTE_MASK) == SOUND_BANK_IMAGE_SEQUENCE) {
        return _midiGetSequenceBuffer(0, imageBytes & SOUND_LOAD_IMAGE_SIZE_MASK);
    }
    if (Snd_BankSlotsByType[truncatedBankId >> 12] == SOUND_BANK_SLOT_UNSUPPORTED) {
        return 0;
    }
    switch (bankId & SOUND_BANK_TYPE_MASK) {
        case SOUND_BANK_TYPE_WEAPON << 12:
            if (imageBytes < SOUND_LOAD_WEAPON_MIN_IMAGE_BYTES) {
                imageBytes = SOUND_LOAD_WEAPON_MIN_IMAGE_BYTES;
            }
            break;
        case (u32)SOUND_BANK_TYPE_PE_ALL >> 16:
            if (imageBytes < SOUND_LOAD_PE_MIN_IMAGE_BYTES) {
                imageBytes = SOUND_LOAD_PE_MIN_IMAGE_BYTES;
            }
            break;
    }
    return sndHeapAlloc(imageBytes);
}

/// Resolves the SPU byte origin of a sequence or script sample pool.
///
/// The low two image-kind bits select a fixed sequence origin or script
/// placement. Script placement uses the bank id's low half and a nonnegative
/// wave byte count, rounded by the placement policy to 64-byte blocks; it can
/// advance character-bank selection. Returns zero for an unsupported kind or
/// failed placement. The caller owns validating capacity and uploading bytes.
static s32 _sndLoadResolveSampleAddress(s32 imageKind, s32 bankId, s32 waveBytes)
{
    enum { SOUND_LOAD_SEQUENCE_SAMPLE_BASE = 0x1010 };
    s32 result;

    imageKind &= SOUND_BANK_IMAGE_KIND_MASK;
    result     = 0;
    switch (imageKind) {
        case SOUND_BANK_IMAGE_SEQUENCE:
            result = SOUND_LOAD_SEQUENCE_SAMPLE_BASE;
            break;
        case SOUND_BANK_IMAGE_SCRIPT:
            result = sndLoadPlaceScriptSamples(waveBytes, bankId & SOUND_LOAD_BANK_ID_MASK);
            break;
    }
    return result;
}

/// Resets the resident bank loader to await a header in the selected feed mode.
///
/// Mode `SOUND_LOAD_FEED_SECTOR` selects 2048 bytes; every other argument selects
/// `SOUND_LOAD_FEED_CHUNK` and 2032 bytes. `sectorBuffer` is retained without
/// consuming it. The previous bank and image pointers are discarded, so their
/// ownership must already have been resolved by completion or teardown. Upload
/// policy, the retained header and the published load-bank id are left intact.
/// The loader failure status is cleared for the new transfer.
static void _sndLoadResetState(s32 feedMode, void* sectorBuffer)
{
    enum { SOUND_LOAD_STATUS_OK = 0 };
    SndLoadState* state;
    s32           sectorBytes;

    D_800689E8 = SOUND_LOAD_STATUS_OK;
    state      = &SndLoad_State;
    if (feedMode == SOUND_LOAD_FEED_SECTOR) {
        sectorBytes     = SOUND_LOAD_SECTOR_BYTES;
        state->feedMode = feedMode;
    } else {
        sectorBytes     = SOUND_LOAD_SECTION_BYTES;
        state->feedMode = SOUND_LOAD_FEED_CHUNK;
    }
    state->sectorBytes    = sectorBytes;
    state->phase          = SOUND_LOAD_PHASE_HEADER;
    state->sectorsArrived = 0;
    state->sectorBuffer   = sectorBuffer;
    state->imageBuffer    = NULL;
    state->bank           = NULL;
    state->writeCursor    = NULL;
    state->bytesRemaining = 0;
}

/// Releases a previous script image unless the requested bank is already resident.
///
/// A sequence image kind (low byte zero) returns zero without touching a slot.
/// Other kinds require a mapped type. Character ids are checked across
/// descriptors 4..6 before choosing the current upload ordinal's slot; ordinary
/// types check their mapped descriptor, while sequence-bank types skip that
/// duplicate check. Returns `SOUND_LOAD_RESULT_ERROR` for duplicates or unmapped
/// types, otherwise releases the selected slot's image and returns zero. Bank
/// table release/reallocation is separate. The type-4 ordinal must be in 0..2
/// for an installable batch; its existing allocation timing is retained.
static s32 _sndLoadPrepareBankSlot(u16 bankId, s32 imageKind)
{
    enum {
        SOUND_LOAD_CHARACTER_FIRST_SLOT = 4,
        SOUND_LOAD_CHARACTER_SLOT_END   = 7
    };
    u16      requestedBankId;
    u8       slotIndex;
    s32      descriptorIndex;
    SndBank* descriptors;
    SndBank* descriptor;

    requestedBankId = bankId;
    if ((imageKind & SOUND_LOAD_IMAGE_KIND_BYTE_MASK) == SOUND_BANK_IMAGE_SEQUENCE) {
        return 0;
    }
    slotIndex = Snd_BankSlotsByType[requestedBankId >> 12];
    if ((s8)slotIndex == SOUND_BANK_SLOT_UNSUPPORTED) {
        return SOUND_LOAD_RESULT_ERROR;
    }
    switch ((u32)(bankId & SOUND_BANK_TYPE_MASK) >> 12) {
        case SOUND_BANK_TYPE_CHARACTER:
            descriptorIndex = SOUND_LOAD_CHARACTER_FIRST_SLOT;
            descriptors     = Snd_Banks;
            descriptor      = descriptors + SOUND_LOAD_CHARACTER_FIRST_SLOT;
            do {
                if (descriptor->bankId == requestedBankId) {
                    return SOUND_LOAD_RESULT_ERROR;
                }
                descriptorIndex++;
                descriptor++;
            } while (descriptorIndex < SOUND_LOAD_CHARACTER_SLOT_END);
            slotIndex = D_80082122 + SOUND_LOAD_CHARACTER_FIRST_SLOT;
            break;
        case SOUND_BANK_TYPE_SEQUENCE >> 12:
            break;
        default:
            if (Snd_Banks[(s8)slotIndex].bankId == (bankId & SOUND_LOAD_BANK_ID_MASK)) {
                return SOUND_LOAD_RESULT_ERROR;
            }
            break;
    }
    sndBankSlotReleaseImage((s8)slotIndex);
    return 0;
}
