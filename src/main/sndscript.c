#include "main/sound.h"

#include <psyq/sys/types.h>
#include <psyq/abs.h>
#include <psyq/libspu.h>
#include <psyq/libetc.h>

#include "common.h"

#include "cdaudio.h"
#include "main/display.h"
#include "main/display_types.h"
#include "sound.h"
#include "main/sound_types.h"
#include "sound_types.h"
#include "stage.h"

typedef struct _SndVoice _SndVoice;

struct _SndScript;

/// `oneE` FourCC. Setup arms a `_SndPitchEnvelope` only when its `magic` matches.
enum { SOUND_SCRIPT_PITCH_ENVELOPE_TAG = 0x45656E6F };

/// Pitch envelope for one scripted voice.
///
/// This is the 24-byte `oneE` chunk in a loaded `hONE` bank image. The voice
/// command names it with an image-relative byte offset; -1 leaves the voice at
/// the pitch it was keyed on with. The image owns the bytes, and a pointer to
/// the chunk remains valid until that image is released or reloaded. The
/// script interpreter treats the tag as data: landing on it stops command
/// processing without advancing the cursor.
///
/// Setup stores the addressed block on the voice before testing `magic`. It
/// arms modulation, and restarts at the delay, only when `magic` is
/// `SOUND_SCRIPT_PITCH_ENVELOPE_TAG`. Any other signature leaves the voice's
/// modulation flag and stage as they were.
///
/// Levels and slopes are signed offsets from the keyed pitch, in 1/256
/// semitone steps (256 is one semitone). Durations count audio updates, one
/// per voice tick. `delayUpdates` and `holdUpdates` are signed, and a negative
/// count skips that stage.
///
/// Playback waits out the delay, then ramps from the keyed pitch by
/// `attackSlope`. The update that exhausts a stage's count plays the next
/// stage, so a zero count skips the stage. The attack is followed by a hold at
/// `attackLevel`, a ramp from that level by `decaySlope`, and then
/// `sustainLevel` until the note is released. A ramp and the level after it
/// are separate values; finishing the ramp switches to the level directly.
///
/// Release replaces the current stage. It adds `releaseSlope` to the offset
/// last stored by a ramp until that offset reaches `releaseLevel`, then holds
/// the level. During the attack hold that stored offset is still the attack
/// ramp's end, and during sustain it is the decay ramp's end, rather than the
/// level being played when the two differ. A zero slope, or a sign that points
/// away from `releaseLevel`, snaps to the release level on the next update.
typedef struct {
    s32 magic;         // Serialized oneE FourCC; any other value does not arm modulation
    s16 delayUpdates;  // Updates to keep the keyed pitch; a negative count skips the wait
    s16 attackLevel;   // Offset held after the attack, and the offset decay starts from
    s16 attackSlope;   // Added to the attack offset after each attack update is played
    u16 attackUpdates; // Attack length in audio updates; zero skips the ramp
    s16 holdUpdates;   // Updates to hold attackLevel; a non-positive count skips the hold
    u16 decayUpdates;  // Decay length in audio updates; zero skips the ramp
    s16 decaySlope;    // Added to the decay offset after each decay update is played
    s16 sustainLevel;  // Offset held after decay until the note is released
    s16 releaseLevel;  // Offset held once the tracked release reaches it
    s16 releaseSlope;  // Signed release step per update; zero snaps to releaseLevel
} _SndPitchEnvelope;
STATIC_ASSERT_SIZEOF(_SndPitchEnvelope, 0x18);

/// Stage of a `_SndVoiceEnvelope`.
///
/// Delay through sustain run in order. Release replaces whichever of those is
/// current. Once the tracked ramp reaches `releaseLevel`, the next stage holds
/// that level.
enum {
    SOUND_VOICE_ENVELOPE_DELAY        = 0,
    SOUND_VOICE_ENVELOPE_ATTACK       = 1,
    SOUND_VOICE_ENVELOPE_HOLD         = 2,
    SOUND_VOICE_ENVELOPE_DECAY        = 3,
    SOUND_VOICE_ENVELOPE_SUSTAIN      = 4,
    SOUND_VOICE_ENVELOPE_RELEASE      = 5,
    SOUND_VOICE_ENVELOPE_RELEASE_HOLD = 6
};

/// `releaseRequest` on a `_SndVoiceEnvelope`.
///
/// Pending asks the next update to capture the release step and enter release.
/// Started means that step has been captured, so a later gate expiry does not
/// capture it again.
enum {
    SOUND_VOICE_ENVELOPE_HELD            = 0,
    SOUND_VOICE_ENVELOPE_RELEASE_PENDING = 1,
    SOUND_VOICE_ENVELOPE_RELEASE_STARTED = 2
};

/// Pitch-envelope player for one scripted voice.
///
/// `_SndVoice` embeds one. Setup copies the layer's root key and fine tune
/// because the voice does not keep the layer, and arms playback only when the
/// addressed chunk is an `oneE` (`_SndPitchEnvelope`). No chunk, or no owning
/// script, clears `active`. Any other tag still stores `envelope` and leaves
/// `active`, `stage` and `releaseRequest` as they were.
///
/// `keyedPitch` is the note's Q7 pitch, where 128 is one semitone. Each stage
/// doubles it before adding an offset, so it shares the chunk's 1/256 semitone
/// scale. `stageUpdates` counts audio updates already played in the current
/// stage.
///
/// `attackOffset`, `decayOffset` and `releaseOffset` are the running ramps.
/// `rampOffset` is the offset last stored by a ramp, in the same units. Hold
/// and sustain play their levels without storing them there, so release starts
/// from the ramp's end rather than from the level being played, and then
/// `rampOffset` tracks the release ramp. `releaseStep` is the signed step
/// captured when release starts, chosen so the ramp moves toward `releaseLevel`.
typedef struct {
    s8                 active;         // 0 off, 1 running; a mismatched oneE tag does not change it
    s8                 stage;          // SOUND_VOICE_ENVELOPE_DELAY through RELEASE_HOLD
    s8                 releaseRequest; // SOUND_VOICE_ENVELOPE_HELD, RELEASE_PENDING or RELEASE_STARTED
    u8                 pad;            // No reader or writer; byte before the word-aligned keyed pitch
    s32                keyedPitch;     // Keyed Q7 pitch; low 16 bits of keyMin plus the note's pitch offset
    u16                rootKey;        // Cached layer root key, for converting the sounding pitch
    u16                fineTune;       // Cached layer fine tune, in 1/128 semitone steps
    u16                stageUpdates;   // Audio updates already played in the current stage
    s16                releaseStep;    // Signed step added on each release update, toward releaseLevel
    s32                rampOffset;     // Offset last stored by a ramp, in 1/256 semitone steps
    s32                attackOffset;   // Running attack ramp; the pitch offset during attack
    s32                decayOffset;    // Running decay ramp; the pitch offset during decay
    s32                releaseOffset;  // Running release ramp; the pitch offset during release
    _SndPitchEnvelope* envelope;       // Addressed oneE chunk; stored even when its tag does not arm playback
} _SndVoiceEnvelope;
STATIC_ASSERT_SIZEOF(_SndVoiceEnvelope, 0x24);

/// `oneV` FourCC. A command with this tag keys on one voice.
///
/// `SOUND_SCRIPT_NOTE_LAYER_ADSR` and `SOUND_SCRIPT_NOTE_NO_ENVELOPE` are the
/// image-relative offset sentinels. `SOUND_SCRIPT_NOTE_HELD` is the countdown
/// stored for a note whose gate does not time out.
enum {
    SOUND_SCRIPT_NOTE_TAG         = 0x56656E6F,
    SOUND_SCRIPT_NOTE_LAYER_ADSR  = -1,
    SOUND_SCRIPT_NOTE_NO_ENVELOPE = -1,
    SOUND_SCRIPT_NOTE_HELD        = 0x7FFFFFFF
};

/// One voice-on command in a sound-script entry.
///
/// This is the 24-byte `oneV` command in a loaded `hONE` bank image. The
/// interpreter reads it when the command tag is `SOUND_SCRIPT_NOTE_TAG`. It
/// also reads the 24 bytes immediately after an `oneC` entry header through
/// this layout, without checking their tag. The image owns the bytes, and a
/// pointer to the command remains valid until that image is released or
/// reloaded.
///
/// `bankId` 0 plays the script slot's bank. Any other id selects a loaded
/// bank; 0xFFFF is looked up as bank 0 rather than selecting the script bank.
/// A missing bank releases the voice just allocated and leaves the cursor
/// unmoved, so the command is tried again. `program` and `layer` select the
/// sample in that bank.
///
/// `delayTicks` is how many script ticks must have accumulated before key-on;
/// zero keys the voice on this step, and the clock then consumes that delay.
/// `gateTicks` is how long the voice stays keyed, in the same ticks. Zero does
/// not time out.
///
/// A nonnegative `panOverride` replaces the layer pan (0 left, 64 centre, 127
/// right). A nonnegative `volumeOverride` replaces the layer gain (0 silent,
/// 127 full). A negative value keeps the layer's. The entry's pan bias is
/// applied after the pan choice, and the entry's volume scale combines with
/// the chosen gain.
///
/// `reverbLevel` is compared with the global reverb setting. A negative level
/// leaves reverb off. Levels 0..2 enable reverb once the setting reaches them,
/// except that setting 3 enables only level 3. Level 3 also enables reverb
/// when the setting is at least 2.
///
/// `voicePriority` is the allocation priority: a higher value can take an SPU
/// voice from a lower one. `adsrOffset` and `pitchEnvelopeOffset` are signed
/// byte offsets from the start of the bank image. The ADSR sentinel keeps the
/// layer's registers, and any other offset is used only when it addresses an
/// `oneA` chunk. The envelope sentinel leaves pitch modulation off; setup arms
/// modulation only when the addressed chunk is `oneE`.
///
/// `pitchOffset` is a Q7 offset from the layer's minimum key, where 128 is one
/// semitone. It is stored and added as an unsigned halfword, and the sounding
/// key and fraction come from the low 16 bits of that sum.
typedef struct {
    s32 magic;               // Serialized oneV FourCC
    u16 bankId;              // 0 uses the script's bank; any other id is looked up
    u8  program;             // Program index for the layer lookup
    u8  layer;               // Layer index within that program
    u16 delayTicks;          // Script ticks to wait before key-on; zero keys immediately
    u16 gateTicks;           // Keyed length in script ticks; zero does not time out
    s8  panOverride;         // Absolute pan 0..127, or negative to keep the layer pan
    s8  volumeOverride;      // Absolute gain 0..127, or negative to keep the layer gain
    s8  reverbLevel;         // Threshold against the global reverb setting; negative leaves reverb off
    u8  pad;                 // Always zero in retail banks; aligns the following halfword
    u16 voicePriority;       // Higher values can take an SPU voice from a lower priority
    s16 adsrOffset;          // Image-relative oneA byte offset; -1 keeps the layer ADSR
    u16 pitchOffset;         // Q7 offset from the layer's minimum key, added as an unsigned halfword
    s16 pitchEnvelopeOffset; // Image-relative oneE byte offset; -1 leaves pitch modulation off
} _SndScriptNote;
STATIC_ASSERT_SIZEOF(_SndScriptNote, 0x18);

/// One scripted SPU voice and the pitch envelope playing on it.
///
/// Records exist only for hardware voices 16..23. The MIDI sequencer owns
/// 0..15, and this allocator never takes them. While a record holds a voice
/// it sits on its script's list; detach unlinks it and clears that link.
/// The bank image owns the note and envelope bytes the voice points at, and
/// those pointers stay valid only while that image remains loaded.
struct _SndVoice {
    s8                 spuVoice;     // Hardware voice 16..23 while held; 0 after release
    u8                 field_1;      // Written as 1 on both key-on reverb paths; no matched reader, role unproven
    s8                 scaledVolume; // Master, entry and base gain combined / 127^2 (0 silent, 127 full)
    u8                 basePan;      // Pan after the entry bias (0 left, 64 centre, 127 right)
    s32                gateClock;    // Remaining key time in 16.16 script ticks; the held sentinel does not expire
    s16                allocated;    // 1 while this record holds an SPU voice, 0 after release; no matched reader
    u8                 baseVolume;   // Layer or override gain before master and entry scaling (0 silent, 127 full)
    u8                 pad;          // No reader or writer; aligns the note pointer
    _SndScriptNote*    note;         // oneV command that keyed this voice; stored at key-on, no matched reader
    _SndVoiceEnvelope  envelope;     // Pitch-envelope player; inactive when the note names no oneE chunk
    struct _SndScript* script;       // Owning script, or null once the voice is detached
    _SndVoice*         prev;         // Previous voice on that script's list
    _SndVoice*         next;         // Next voice on that script's list
};
STATIC_ASSERT_SIZEOF(_SndVoice, 0x40);
STATIC_ASSERT(OFFSET_OF(_SndVoice, envelope) == 0x10, snd_voice_envelope_offset);
STATIC_ASSERT(OFFSET_OF(_SndVoice, script) == 0x34, snd_voice_script_offset);

/// FourCCs of the flow-control commands in a sound-script entry.
///
/// `Loop` and `Wait` are eight-byte `_SndScriptCmd` records. `endL` closes the
/// innermost loop and `endC` ends the entry; both are the tag word alone.
enum {
    SOUND_SCRIPT_LOOP_TAG     = 0x706F6F4C,
    SOUND_SCRIPT_WAIT_TAG     = 0x74696157,
    SOUND_SCRIPT_LOOP_END_TAG = 0x4C646E65,
    SOUND_SCRIPT_END_TAG      = 0x43646E65
};

/// One flow-control command in a sound-script entry.
///
/// The interpreter reads the bytes at its cursor in a loaded `hONE` bank image
/// through this layout and dispatches on `magic`. Only `Loop` and `Wait` use
/// the second word, and each advances the cursor by this record's eight
/// bytes. `endL` and `endC` carry no payload: `endL` advances by the tag word
/// alone when its loop is done. The image owns the bytes.
///
/// `Loop` opens a loop once `delayTicks` script ticks have accumulated, and
/// the clock then consumes that delay. The commands up to the matching `endL`
/// run `repeatCount` times; a count of zero is never decremented, so that loop
/// repeats until the script is stopped. Loops nest eight deep. A `Loop` beyond
/// that, or an `endL` with no loop open, ends the script as `endC` does.
///
/// `Wait` holds the cursor until `waitTicks` script ticks have accumulated and
/// then consumes them. The count is signed, and a negative one never waits.
typedef struct {
    s32 magic; // Serialized command FourCC
    union {
        struct {
            u8  repeatCount; // Times the loop body runs; zero repeats without end
            u8  pad;         // No reader; byte before the halfword delay
            u16 delayTicks;  // Script ticks to wait before the loop opens; zero opens immediately
        } loop;              // Loop payload
        s32 waitTicks;       // Wait payload: script ticks to hold the cursor; negative does not wait
    } data;
} _SndScriptCmd;
STATIC_ASSERT_SIZEOF(_SndScriptCmd, 0x8);

/// Lifecycle of one `_SndScript` slot.
///
/// Idle is free. Starting initializes the slot and then runs. Running executes
/// commands and ticks voices. Fading out ramps the shared gain to silence and
/// then stops. Stopping keys the voices off. Releasing waits for any pitch
/// envelope still running. Muting and unmuting ramp that gain without stopping
/// the command stream.
///
/// `SOUND_SCRIPT_PLAYING` is the set an id lookup still treats as owning the
/// request: starting, running, releasing and fading out. `SOUND_SCRIPT_MUTABLE`
/// is the set a mute request will fade.
enum {
    SOUND_SCRIPT_IDLE       = 0,
    SOUND_SCRIPT_STARTING   = 1,
    SOUND_SCRIPT_RUNNING    = 2,
    SOUND_SCRIPT_STOPPING   = 4,
    SOUND_SCRIPT_MUTING     = 8,
    SOUND_SCRIPT_UNMUTING   = 0x10,
    SOUND_SCRIPT_RELEASING  = 0x20,
    SOUND_SCRIPT_FADING_OUT = 0x80,
    SOUND_SCRIPT_PLAYING    = 0xA3,
    SOUND_SCRIPT_MUTABLE    = 0x22
};

/// Stop request that keys a voice off without replacing its ADSR release.
enum { SOUND_SCRIPT_KEEP_RELEASE = 1 };

/// One playing sound-script instance, and the voices it has keyed.
///
/// Eight slots share SPU voices 16..23. `soundId` selects the bank, the entry
/// and the instance; an idle slot stores -1. The slot borrows `bankSlot` for
/// the script image and the default sample bank, and both must stay loaded
/// until the slot is idle again. `cursor` walks that image's commands.
/// `entryControls` is the entry's `oneC` block, reloaded when that command
/// runs rather than when playback is requested.
///
/// The command clock and each voice gate share a 16.16 tick: one whole tick,
/// or 0.6 of one on PAL. Pan and attenuation ramps advance once for each voice
/// visited in an update, so they are not a once-per-frame step. `volumeRamp`
/// is the fade applied to every voice of this instance.
typedef struct _SndScript {
    s32                     soundId;           // Request id, or -1 when the slot is idle
    s32                     runningTicks;      // Audio updates spent executing; a larger count is older
    s32                     tickClock;         // 16.16 script ticks since the last command consumed them
    s8                      keepRelease;       // SOUND_SCRIPT_KEEP_RELEASE, or any other value to force release rate 5
    s8                      ended;             // 1 when endC or a stop lets the slot go idle once its voices finish
    s8                      mixDirty;          // 1 when voice volume and pan must be written to the SPU
    s8                      useUnduckedVolume; // 1 to keep the saved master level while ducking (entry flag 0x02)
    u8                      panOffset;         // Signed mix-pan offset in a byte (0 unchanged, 3 SPU steps per unit)
    u8                      panTarget;         // Signed pan offset the ramp walks toward, same byte storage
    s8                      panStep;           // Added to panOffset times 4 per voice visit; ±8 moves the offset by ±2
    u8                      attenuation;       // Signed attenuation in a byte (0 full, magnitude 127 silent)
    u8                      attenuationTarget; // Signed attenuation the ramp walks toward, same byte storage
    s8                      attenuationStep;   // Added to the current attenuation once per voice visit
    u8                      state;             // SOUND_SCRIPT_IDLE through SOUND_SCRIPT_FADING_OUT
    u8                      loopDepth;         // Nested Loop commands, at most 8
    u8                      loopCounts[8];     // Remaining repeats for each open loop
    u8*                     loopCursors[8];    // Command cursor restarted by each open loop
    _SndVoice*              voices;            // Head of the voices this instance keyed, or null
    SndBankSlot*            bankSlot;          // Borrowed slot whose image holds this entry; the descriptor is not copied
    u8*                     cursor;            // Next command byte in the entry; starts at the oneC block
    SndScriptEntryControls* entryControls;     // Borrowed oneC controls, reloaded by the oneC command
    LinInterp               volumeRamp;        // Shared fade gain for this instance and every voice it starts
} _SndScript;
STATIC_ASSERT_SIZEOF(_SndScript, 0x60);

/// `oneA` FourCC. A note's ADSR offset is used only when it addresses this tag.
enum { SOUND_SCRIPT_ADSR_TAG = 0x41656E6F };

/// ADSR override for one scripted voice.
///
/// This is the 8-byte `oneA` chunk in a loaded `hONE` bank image. The voice
/// command names it with an image-relative byte offset; -1 keeps the layer's
/// registers, and so does an offset whose `magic` is not
/// `SOUND_SCRIPT_ADSR_TAG`. A matching chunk's two halfwords replace the
/// layer's packed SPU registers for that voice, unchanged. The image owns the
/// bytes. The script interpreter treats the tag as data: landing on it stops
/// command processing without advancing the cursor.
typedef struct {
    s32 magic; // Serialized oneA FourCC; any other value keeps the layer ADSR
    u16 adsr1; // Packed SPU attack/decay/sustain-level register
    u16 adsr2; // Packed SPU sustain/release register
} _SndScriptAdsr;
STATIC_ASSERT_SIZEOF(_SndScriptAdsr, 0x8);

STATIC_ASSERT(OFFSET_OF(SpuVoiceAttr, adsr1) == 0x3A, snd_voice_adsr1_offset);
STATIC_ASSERT(OFFSET_OF(SpuVoiceAttr, adsr2) == 0x3C, snd_voice_adsr2_offset);

/// Survey of the eight script slots for one start request, and the slot chosen.
///
/// A start request scans every `_SndScript` slot once. Idle slots are free. A
/// stopping slot is neither free nor replaceable. Every other instance is
/// ranked by its entry's priority against the request's, and counted when it
/// belongs to the request's group: the same request id ignoring bits 8..15,
/// or, where the instance's entry sets `SOUND_SCRIPT_GROUP_BY_FLAGS`, a flags
/// word equal to the request's.
///
/// A slot member is an index 0..7, or -1 when no slot qualified. An age is a
/// count of running audio updates, so a larger one is older. The slot's age is
/// meaningful only while its slot member is set.
///
/// `slot` is the result. It is the idle slot when there is one and the group
/// is below the entry's instance limit. Otherwise the request replaces the
/// oldest group member, or with no group member a lower-priority instance, or
/// the oldest one at the request's own priority. A negative result refuses the
/// request, and nothing is started.
typedef struct {
    s8  slot;               // Chosen slot 0..7; -1 after the scan, negative when refused
    s8  lastGroupSlot;      // Last group member scanned; written only, no reader
    s8  newestGroupSlot;    // Group member with the fewest running updates
    s8  idleSlot;           // Last idle slot scanned
    s8  lowerPrioritySlot;  // First slot holding the lowest priority below the request's
    s8  equalPrioritySlot;  // Oldest slot whose priority equalled lowestPriority when scanned
    s8  oldestGroupSlot;    // Group member with the most running updates
    u8  groupCount;         // Group members found, compared with the entry's instance limit
    s32 lowestPriority;     // Starts at the request's priority and falls to the lowest one below it
    s32 newestGroupTicks;   // Age of newestGroupSlot; 0xFFFF, above any retrigger limit, with no member
    s32 equalPriorityTicks; // Age of equalPrioritySlot
    s32 oldestGroupTicks;   // Age of oldestGroupSlot
} _SndScriptSlotPick;
STATIC_ASSERT_SIZEOF(_SndScriptSlotPick, 0x18);

/// One sound bank reserved at boot, before any bank file has been read.
///
/// Sound initialization binds the bank type's script slot to its descriptor,
/// stamps both with `bankId`, and takes two blocks from the sound heap: the
/// descriptor's table block and the slot's script image. Neither block holds
/// loaded contents yet, so the bank cannot play until a load completes.
///
/// The two byte counts equal the minimums a later load of the same bank type
/// asks for, so a reload never requests less than was reserved here.
typedef struct {
    u16 bankType;   // Bank type 0..15, the index into the type-to-slot map
    u16 bankId;     // Placeholder id for the descriptor and slot: the type nibble over 0xFF
    u16 tableBytes; // Sound-heap bytes for the descriptor's program and layer table block
    u16 imageBytes; // Sound-heap bytes for the slot's script image
    u32 spuAddr;    // SPU sample-pool origin in bytes, stored on the slot
} _SndBankInitEntry;
STATIC_ASSERT_SIZEOF(_SndBankInitEntry, 0xC);

// oneC controls apply to a whole script instance and all voices it starts.
enum {
    SOUND_SCRIPT_ENTRY_TAG           = 0x43656E6F,
    SOUND_SCRIPT_VOLUME_UNITY        = 127,
    SOUND_SCRIPT_USE_UNDUCKED_VOLUME = 0x02,
    SOUND_SCRIPT_GROUP_BY_FLAGS      = 0x10,
    SOUND_SCRIPT_RETRIGGER_DISABLED  = -1
};

// Mix changes above 32 ramp by 8 per voice visit; pan uses quarter-offset units.
enum {
    SOUND_SCRIPT_MIX_RAMP_THRESHOLD = 32,
    SOUND_SCRIPT_MIX_RAMP_STEP      = 8,
    SOUND_SCRIPT_PAN_FRACTION_SCALE = 4
};

// Descriptor lookup modes; all other values produce no match.
enum {
    /// Selects exact matching of a sound-script slot's attached sample-bank id.
    ///
    /// All 16 bits of `SndBank::bankId` participate, including the bank type.
    /// Script-start requests supply their upper 16 bits after bank remapping.
    /// The cached `SndBankSlot::bankId` is not consulted; a free descriptor id
    /// can match, and matching does not establish image or sample-table readiness.
    SOUND_BANK_SLOT_MATCH_ID = 0,
    /// Selects matching by the attached sample descriptor's encoded bank type.
    ///
    /// `SOUND_BANK_TYPE_MASK` selects bits 12..15 of both the requested id and
    /// `SndBank::bankId`; the low 12 bits are ignored. The first matching slot
    /// wins, without consulting its cached id or validating its image or tables.
    /// Type 15 also matches `SOUND_BANK_ID_FREE`, so this is not a readiness test.
    SOUND_BANK_SLOT_MATCH_TYPE = 1
};

static volatile u8 D_80082138[0x10];

/// Stable sound-script bank slots owning images and borrowing sample descriptors.
///
/// Slot indices are 0..15. Bank types select them through `Snd_BankSlotsByType`,
/// with type 4 using successive slots 4..6. Boot reserves image buffers for
/// types 2 and 14; completed script loads transfer their sound-heap images here.
/// MIDI sequence images are held separately.
///
/// Lookup reads the attached descriptor's id. Image release clears `image` and
/// sets the cached `bankId` to -1, retaining the descriptor and SPU origin.
/// Slot addresses remain stable across release and reload; borrowed image and
/// table pointers require their contents to remain loaded until their last use.
static SndBankSlot _gSndBankSlots[16];

static _SndScript SndScript_Slots[8];

/// Script voices use SPU slots 16..23, after the MIDI sequencer's sixteen slots.
static _SndVoice SndScript_Voices[8];

static s8 D_80082748;

static s8 D_80082749;

static s8 D_8008274A;

static s8 D_8008274B;

static volatile s32 D_8008274C;

static u8 D_80068A54[];

static _SndBankInitEntry Snd_BankInitTable[];

static s16 SndScript_VoiceRanges[];

static void Snd_ClearBusy(void);

static void Snd_SetBusyFlag(s32 arg0);

static void SndVoice_SetPriority(s8 arg0);

static void SndEvt_EnqueueTypeF(void);

static void SndVoice_StepMasterLevel(void);

static s32 SndVoice_DriveSlots(s32* unused);

static void SndVoice_ScanCandidates(_SndScriptSlotPick* candidates, u16 arg1, s32 arg2, u16 arg3);

static inline void _sndScriptAdvanceClock(_SndScript* script);

/// Decides whether a note plays with reverb, from its own level against a
/// global one. A note at level 3 gets reverb whenever the global level is at
/// least 2; a global level of 3 turns it off for every other note; otherwise a
/// note with a non-negative level gets reverb once the global level reaches it.
static inline u8 _sndScriptUseReverb(_SndScriptNote* note);

static s32 SndScript_Exec(_SndScript* script);

static void SndVoice_TickEnvelope(_SndVoice* voice);

static void SndVoice_Init(void);

static void SndVoice_SetPriorityLevel(s8 arg0);

/// Selects an eligible voice candidate, respecting the retrigger-age limit.
static s8 SndVoice_SelectStealCandidate(_SndScriptSlotPick* candidates, s32 retriggerTicks);

static void SndScript_Play(s32 arg0, s8 arg1, s8 arg2, s32 arg3, SndBankSlot* slot, SndScriptEntryControls* entryControls);

static void _sndVoiceDetach(void* context);

static SndBankSlot* _sndBankSlotFind(u16 bankId, s32 matchMode);

static _SndVoice* SndVoice_Alloc(s32 arg0);

static void _sndVoiceAttach(_SndScript* script, _SndVoice* voice);

static s32 SndVoice_Tick(_SndVoice* voice);

static s32 SndScript_TickVoices(_SndScript* script);

static void SndVoice_ScaleVolume(s8 arg0, s8 arg1, _SndVoice* voice, LinInterp* ramp, SpuVolume* panVolumes);

static void SndVoice_SetupEnvelope(_SndVoice* voice, s16 envelopeOffset, u32 pitch, SndBankLayer* bankLayer);

static s32 SndScript_FindOneA(u8* arg0, s16 arg1, SpuVoiceAttr* arg2);

static void SndVoice_ClearActive(void);

static u8                D_80068A54[]        = { 0xFF, 0xFF, 0xFF, 0xFF, 0x20, 0x26, 0x20, 0x26, 0x2E, 0x05, 0x1E, 0xFF };
static _SndBankInitEntry Snd_BankInitTable[] = {
    { 0x0002, 0x20FF, 0x00CE, 0x0210, 0x73810 },
    { 0x000E, 0xE0FF, 0x0078, 0x0168, 0x6F810 },
};
s32        D_80068A78              = 0;
static s16 SndScript_VoiceRanges[] = { 1, 2 };

static inline s32 _sndStagePriority(s32 stage, s32 area)
{
    s32 i;

    if (area == 5 && stage == 4) {
        return 3;
    }
    for (i = 0; i < 2; i++) {
        if (D_80068A54[i + stage * 2] == area) {
            return 2;
        }
    }
    return 1;
}

void Snd_InitFromStage(s32 arg0, s32 arg1)
{
    SndBank* var_s0;
    s32      temp_v1;

    D_8008274C = 0;
    SndVoice_ClearActive();
    arg0 = arg0 & 0xFF;
    SndEvt_EnqueueTypeF();
    sndEvtRequestScriptStop(SOUND_AREA_BANK_ALL, SOUND_SCRIPT_STOP_KEEP_RELEASE);
    sndEvtRequestScriptStop(SOUND_SCRIPT_REQUEST_TYPE_1, SOUND_SCRIPT_STOP_KEEP_RELEASE);
    sndEvtRequestScriptStop(SOUND_COMMON(0x0D) | SOUND_SCRIPT_STOP_ALL_INSTANCES, SOUND_SCRIPT_STOP_KEEP_RELEASE);
    sndEvtRequestScriptStop(SOUND_BANK_TYPE_WEAPON_ALL, SOUND_SCRIPT_STOP_KEEP_RELEASE);
    sndEvtRequestScriptStop(SOUND_BANK_TYPE_PE_ALL, SOUND_SCRIPT_STOP_KEEP_RELEASE);
    arg1       = arg1 & 0xFF;
    D_80082120 = arg0;
    D_80082136 = arg1;
    SndBankSlot_Free(1);
    SndBankSlot_Free(7);

    SndVoice_SetPriority(_sndStagePriority(arg0, arg1));
    D_80082130 = 0x3D010;
    D_80082128 = 0;
    D_80082124 = D_80082128;

    temp_v1 = (s8)D_80082135;
    switch (temp_v1) {
        case 0:
            sndBankFree(&Snd_Banks[4]);
            SndBankSlot_Free(4);
        case 1:
            D_80082122 = 0;
            break;
        case 2:
            D_80082122 = 1;
            break;
    }
    var_s0 = &Snd_Banks[1];

    SndLoad_State.imageBuffer = 0;
    SndLoad_State.bank        = 0;
    D_8008212C                = D_80082122;
    D_80082121                = D_80082135;
    sndBankFree(var_s0);
    sndBankFree(var_s0 + 6);
    sndBankFree(var_s0 + 4);
    SndBankSlot_Free(5);
    sndBankFree(var_s0 + 5);
    SndBankSlot_Free(6);
    sndBankFree(var_s0 + 2);
    SndBankSlot_Free(3);
    SndBank_SetEnableFlags(1, 0x40000000);
}

s32 SndLoad_ResolveSpuAddr(s32 arg0, s32 arg1)
{
    s32 temp_a2;

    temp_a2 = (arg0 + 0x3F) & ~0x3F;
    switch ((u32)(arg1 & SOUND_BANK_TYPE_MASK) >> 0xC) {
        case 0:
            arg0 = 0x63810;
            break;
        case 1:
            D_80082128 = 0x63810 - temp_a2;
            arg0       = D_80082128;
            break;
        case 3:
            arg0 = 0x47010;
            break;
        case 4:
            if ((s8)D_80082135 == 1) {
                D_80082135 = 2;
            } else {
                if ((s8)D_80082122 > 0 && (s8)D_80082122 < 3) {
                    arg0 = Snd_Banks[(s8)D_80082122 + 3].spuAddr +
                           Snd_Banks[(s8)D_80082122 + 3].waveBytes;
                    D_80082122 += 1;
                    D_80082130  = temp_a2 + arg0;
                    break;
                }
                arg0 = 0;
                if (D_80082122 != 0) {
                    goto clear_ret;
                }
            }
            arg0       = 0x3D010;
            D_80082122 = 1;
            D_80082130 = temp_a2 + arg0;
            break;
        clear_ret:
            D_80082130 = 0;
            break;
        case 5: {
            s32 top;

            top = D_80082128;
            if (top == 0) {
                top = 0x63810;
            } else {
                top = D_80082128;
            }
            D_80082124 = top - temp_a2;
            arg0       = D_80082124;
            break;
        }
        case 6:
            arg0 = 0x3D010 - temp_a2;
            break;
        case 2:
        case 7:
            arg0 = 0x7B010 - temp_a2;
            break;
        case 14:
            arg0 = 0x6F810;
            break;
        default:
            arg0 = 0;
            break;
    }
    return arg0;
}

s32 stageMusicSelectColumn(s32 stage, s32 sceneEvent, s32 sceneEventBase)
{
    enum { STAGE_MUSIC_DEFAULT_COLUMN = 0 };
    // Columns per area's row, indexed by stage. Stage 0 has no music table.
    u8  columnCounts[] = { 0, 8, 7, 11, 12, 10 };
    s32 signedSceneEvent;
    s32 beforeSceneEventBase;
    s32 sceneEventOffset;

    sceneEventOffset = sceneEventBase - 1;

    switch (stage & 0xFF) {
        case GAME_STAGE_ACROPOLIS:
        case GAME_STAGE_DRYFIELD:
            break;
        case GAME_STAGE_DRYFIELD_NIGHT:
            // Later night-time events share the last two columns.
            signedSceneEvent = (s8)sceneEvent;
            if (signedSceneEvent >= 9) {
                if ((signedSceneEvent == 0x1A) || (signedSceneEvent == 0x1D)) {
                    sceneEvent = 0xA;
                } else {
                    sceneEvent = 9;
                }
            }
            break;
        case GAME_STAGE_MINE_SHELTER:
            // Translate the Shelter event range, including its late-event overrides.
            signedSceneEvent = (s8)sceneEvent;
            if (signedSceneEvent >= 0x14) {
                switch ((s8)(sceneEvent - 0x17)) {
                    case 0:
                        sceneEvent = 0xB - sceneEventOffset;
                        break;
                    case 3:
                        sceneEvent = 0x10 - sceneEventOffset;
                        break;
                    case 5:
                        sceneEvent = 0x11 - sceneEventOffset;
                        break;
                    case 6:
                        sceneEvent = 0x12 - sceneEventOffset;
                        break;
                    case 7:
                        sceneEvent = 0x13 - sceneEventOffset;
                        break;
                    default:
                        sceneEvent = 0xF - sceneEventOffset;
                        break;
                }
            } else if (signedSceneEvent < 9) {
                sceneEvent = STAGE_MUSIC_DEFAULT_COLUMN;
            } else {
                sceneEvent = sceneEvent - sceneEventOffset;
            }
            break;
        case GAME_STAGE_SHELTER_NEO_ARK:
            // Events 20 and 29 select columns 0 and 1; the rest use the event base.
            signedSceneEvent = (s8)sceneEvent;
            switch (signedSceneEvent) {
                case 0x14:
                    sceneEvent = STAGE_MUSIC_DEFAULT_COLUMN;
                    break;
                case 0x1D:
                    sceneEvent = 1;
                    break;
                default:
                    signedSceneEvent = sceneEvent << 24;
                    signedSceneEvent = signedSceneEvent >> 24;
                    sceneEvent       = sceneEvent - sceneEventOffset;
                    // Keep the signed comparison result in the event temporary's register.
                    signedSceneEvent     = signedSceneEvent < ((sceneEventOffset & 0xFF) + 1);
                    beforeSceneEventBase = signedSceneEvent;
                    if (beforeSceneEventBase != 0) {
                        sceneEvent = STAGE_MUSIC_DEFAULT_COLUMN;
                    }
                    break;
            }
            break;
        default:
            sceneEvent = STAGE_MUSIC_DEFAULT_COLUMN;
            break;
    }

    // Clamp the narrowed column, including negative or wrapped event results.
    if ((u32)(sceneEvent & 0xFF) >= columnCounts[stage & 0xFF]) {
        sceneEvent = STAGE_MUSIC_DEFAULT_COLUMN;
    }
    return sceneEvent & 0xFF;
}

static void Snd_ClearBusy(void)
{
    Snd_SetBusyFlag(0);
}

static void Snd_SetBusyFlag(s32 arg0)
{
    if (arg0 == 0) {
        sndBankFree(&Snd_Banks[12]);
        D_80082134 = 0;
        return;
    }
    D_80082134 = 1;
}

void Snd_SetModeFlag(s32 arg0)
{
    s8 temp;

    temp = (s8)D_80082135;
    if (temp == 0) {
        if (arg0 != 0) {
            D_80082135 = 1;
        }
    } else if (temp >= 0) {
        if ((temp < 3) && (arg0 == 0)) {
            D_80082135 = 0;
        }
    }
}

void Snd_PollAsync(s32 unused)
{
    AsyncCb_Poll();
}

void Snd_RegisterTickCallbacks(void)
{
    audioTickInsert(Midi_Tick, NULL, AUDIO_TICK_ID_MIDI, NULL);
    audioTickInsert(SndVoice_DriveSlots, NULL, AUDIO_TICK_ID_SOUND_SCRIPTS, NULL);
    D_80082130 = 0x3D010;
    D_80082128 = 0x63810;
    D_80082124 = D_80082128;
    D_80082122 = 0;
    D_8008212C = 0;
    D_80082135 = 0;
    D_80082121 = 0;
    D_8008274C = 0;
}

/// Stamps the loaded type-1 script bank onto a request whose top nibble is 1.
///
/// That nibble equals `SOUND_SCRIPT_REQUEST_TYPE_1` for every type-1 request,
/// whatever bank number and entry index it carries.
/// `SOUND_SCRIPT_REQUEST_ENTRY_INSTANCE_MASK` keeps the entry index and
/// instance tag, and the loaded script image's bank id replaces the high
/// half. A request with another top nibble is returned unchanged, as is a
/// type-1 request when no slot has a type-1 sample descriptor. The search reads
/// that descriptor's type; the stamp reads `image->bankId`, so the slot must
/// hold a completed script image.
static s32 _sndScriptRemapType1Id(s32 requestId)
{
    /// Mask keeping a script request's entry index and instance tag.
    ///
    /// Bits 0..7 select the script entry and bits 8..15 distinguish instances.
    /// Type-1 remapping replaces the bank id above these bits.
    enum { SOUND_SCRIPT_REQUEST_ENTRY_INSTANCE_MASK = 0xFFFF };
    s32          soundId;
    SndBankSlot* bankSlot;

    soundId = requestId;
    // Bits 28..31 select the bank type.
    if ((soundId & 0xF0000000) == SOUND_SCRIPT_REQUEST_TYPE_1) {
        bankSlot = _sndBankSlotFind(SOUND_BANK_TYPE_1, SOUND_BANK_SLOT_MATCH_TYPE);
        if (bankSlot != NULL) {
            soundId = (bankSlot->image->bankId << 16) + (soundId & SOUND_SCRIPT_REQUEST_ENTRY_INSTANCE_MASK);
        }
    }
    return soundId;
}

s32 Snd_ReverbWarmupCb(s32* arg0)
{
    s32 temp;

    temp  = *arg0 + 1;
    *arg0 = temp;
    if (temp < 0x3D) {
        return 0;
    }
    Spu_SetReverbDepth(0x2800);
    return -1;
}

void Snd_SetMutedVolumes(s32 arg0)
{
    s32 var_a0;

    if (arg0 == 0) {
        gSndVolumeReducedMode = SOUND_VOLUME_MODE_NORMAL;
        sndScriptSetMasterVolume(0x7F);
        var_a0 = 0x40;
    } else {
        gSndVolumeReducedMode = SOUND_VOLUME_MODE_REDUCED;
        sndScriptSetMasterVolume(0x28);
        var_a0 = 0;
    }
    midiSetMasterVolume(var_a0);
}

s32 Snd_InitBanks(u32 unused)
{
    s32                i;
    s8                 slot;
    SndBankSlot*       bankSlot;
    SndBank*           bank;
    _SndBankInitEntry* entry;
    s32                id;

    *(volatile s32*)&D_80068A78 = 0xFF;
    Spu_SetVoiceRange(1, 0x12, 6);
    SndVoice_Init();
    SndVoice_SetPriority(1);
    SndBank_SetEnableFlags(1, 0x80000000);

    for (i = 0; i < 2; i++) {
        entry                           = &Snd_BankInitTable[i];
        slot                            = Snd_BankSlotsByType[entry->bankType];
        bankSlot                        = sndBankSlotGet(slot);
        id                              = entry->bankId;
        bank                            = &Snd_Banks[slot];
        bankSlot->bank                  = bank;
        bankSlot->bankId                = id;
        bank->bankId                    = entry->bankId;
        bankSlot->bank->heapBlock       = sndHeapAlloc(entry->tableBytes);
        bankSlot->bank->groups          = bankSlot->bank->heapBlock;
        bankSlot->bank->layers          = bankSlot->bank->heapBlock;
        bankSlot->bank->groupFirstLayer = bankSlot->bank->heapBlock;
        bankSlot->image                 = sndHeapAlloc(entry->imageBytes);
        bankSlot->spuAddr               = entry->spuAddr;
    }

    *(volatile s32*)&D_80068A78 = 0;
    return -1;
}

s32 sndEvtRequestScriptStart(s32 soundId, s32 panOffset, s32 attenuation)
{
    /// Entry flag refusing a script-start request under the reduced-volume policy.
    ///
    /// Bit 7 of `SndScriptEntryControls::flags` rejects the request with -5
    /// while `gSndVolumeReducedMode` is nonzero, before an event is allocated.
    /// Normal mode ignores this bit; an unflagged entry still undergoes the
    /// other start checks. This gate does not depend on current master gains
    /// or an existing script's mute/fade state, and is not checked again when
    /// queued playback starts. The bit remains part of full-word flag grouping.
    enum { SOUND_SCRIPT_REJECT_IN_REDUCED_VOLUME_MODE = 0x80 };

    /// Entry flag permitting a script-start request while its bank type is disabled.
    ///
    /// Bit 0 of `SndScriptEntryControls::flags` bypasses the -4 rejection for
    /// the request's type (bits 28..31 after type-1 bank remapping). The gate
    /// stays unchanged; mute/unmute, pan and volume requests still require it
    /// to be open. Load, bank/entry, reduced-volume and event-capacity checks
    /// still apply, as does later script-slot allocation. The stored flags
    /// word remains unchanged for same-sound grouping.
    enum { SOUND_SCRIPT_ALLOW_DISABLED_TYPE = 0x01 };

    /// Empty-slot value in a sound-script bank image's entry-offset table.
    ///
    /// `SndBankHdr::entryOffsets` stores a byte offset from the image start to
    /// each slot's `oneC` block. Zero means that slot has no script, so a start
    /// request that reads it is rejected before an entry pointer is formed.
    /// Several slots may share one nonzero offset. Callers still need a
    /// completed `hONE` image; this comparison only interprets the stored offset.
    enum { SOUND_BANK_ENTRY_ABSENT = 0 };
    enum { SOUND_SCRIPT_REQUEST_ENTRY_MASK = 0xFF };
    s32                     originalSoundId;
    SndBankSlot*            bankSlot;
    SndBankHdr*             header;
    SndScriptEntryControls* entry;
    u16                     entryOffset;
    u32                     entryIndex;
    SndEvt*                 event;
    SndEvtScriptArgs*       args;

    // Preserve the caller id for the result while resolving the queued bank id.
    originalSoundId = soundId;
    if ((soundId == SOUND_SCRIPT_REQUEST_NO_OP) || (soundId == SOUND_SCRIPT_REQUEST_NO_OP_8)) {
        return originalSoundId;
    }
    // A published load refuses script starts of that bank type.
    if (gSndLoadBankId != SOUND_LOAD_BANK_NONE) {
        if ((gSndLoadBankId & SOUND_BANK_TYPE_MASK) == (((u32)soundId >> 16) & SOUND_BANK_TYPE_MASK)) {
            return SOUND_SCRIPT_START_UNAVAILABLE;
        }
    }
    soundId  = _sndScriptRemapType1Id(soundId);
    bankSlot = _sndBankSlotFind((u32)soundId >> 16, SOUND_BANK_SLOT_MATCH_ID);
    if (bankSlot == NULL) {
        return SOUND_SCRIPT_START_INVALID_ENTRY;
    }
    entryIndex = (u32)soundId & SOUND_SCRIPT_REQUEST_ENTRY_MASK;
    header     = bankSlot->image;
    if (entryIndex >= header->entryCount) {
        return SOUND_SCRIPT_START_INVALID_ENTRY;
    }
    entryOffset = *(header->entryOffsets + entryIndex);
    if (entryOffset == SOUND_BANK_ENTRY_ABSENT) {
        return SOUND_SCRIPT_START_ENTRY_ABSENT;
    }
    // A nonzero offset addresses the slot's oneC block within this loaded image.
    entry = (SndScriptEntryControls*)((u8*)header + entryOffset);
    if (gSndVolumeReducedMode != SOUND_VOLUME_MODE_NORMAL) {
        if ((entry->flags & SOUND_SCRIPT_REJECT_IN_REDUCED_VOLUME_MODE) != 0) {
            return SOUND_SCRIPT_START_REDUCED_VOLUME;
        }
    }
    if (D_80082138[(u32)soundId >> 28] == 0) {
        if ((entry->flags & SOUND_SCRIPT_ALLOW_DISABLED_TYPE) == 0) {
            return SOUND_SCRIPT_START_TYPE_DISABLED;
        }
    }
    event = sndEvtAlloc();
    if (event == NULL) {
        return SOUND_SCRIPT_START_UNAVAILABLE;
    }
    event->command          = SOUND_EVENT_SCRIPT_START;
    args                    = &event->args.script;
    args->soundId           = soundId;
    args->panOffset         = panOffset;
    args->level.attenuation = attenuation;
    args->bankSlot          = bankSlot;
    args->entryControls     = entry;
    sndEvtEnqueue(event);
    return originalSoundId;
}

void sndEvtRequestScriptStop(s32 soundSelector, u16 stopControl)
{
    SndEvt*           event;
    SndEvtScriptArgs* args;

    event = sndEvtAlloc();
    if (event != NULL) {
        event->command    = SOUND_EVENT_SCRIPT_STOP;
        args              = &event->args.script;
        args->soundId     = _sndScriptRemapType1Id(soundSelector);
        args->stopControl = stopControl;
        sndEvtEnqueue(event);
    }
}

void SndEvt_EnqueueType8(s32 arg0)
{
    SndEvt*           event;
    SndEvtScriptArgs* args;

    if (D_80082138[(u32)arg0 >> 28] != 0) {
        event = sndEvtAlloc();
        if (event != NULL) {
            event->command = SOUND_EVENT_SCRIPT_MUTE;
            args           = &event->args.script;
            args->soundId  = _sndScriptRemapType1Id(arg0);
            sndEvtEnqueue(event);
        }
    }
}

void SndEvt_EnqueueType9(s32 arg0)
{
    SndEvt*           event;
    SndEvtScriptArgs* args;

    if (D_80082138[(u32)arg0 >> 28] != 0) {
        event = sndEvtAlloc();
        if (event != NULL) {
            event->command = SOUND_EVENT_SCRIPT_UNMUTE;
            args           = &event->args.script;
            args->soundId  = _sndScriptRemapType1Id(arg0);
            sndEvtEnqueue(event);
        }
    }
}

void sndEvtRequestScriptMix(s32 soundId, s32 panOffset, s32 attenuation)
{
    /// Shift from a packed script id to its 0..15 bank-type gate index.
    enum { SOUND_SCRIPT_REQUEST_TYPE_SHIFT = 28 };
    SndEvt*           event;
    SndEvtScriptArgs* args;

    // Check the requested type before reserving an event or remapping the bank.
    if (D_80082138[(u32)soundId >> SOUND_SCRIPT_REQUEST_TYPE_SHIFT] != 0) {
        event = sndEvtAlloc();
        if (event != NULL) {
            event->command          = SOUND_EVENT_SCRIPT_SET_PAN_ATTENUATION;
            args                    = &event->args.script;
            args->soundId           = _sndScriptRemapType1Id(soundId);
            args->panOffset         = panOffset;
            args->level.attenuation = attenuation;
            sndEvtEnqueue(event);
        }
    }
}

void SndEvt_EnqueueTypeB(s32 arg0, s32 arg1)
{
    SndEvt*           event;
    SndEvtScriptArgs* args;

    if (D_80082138[(u32)arg0 >> 28] != 0) {
        event = sndEvtAlloc();
        if (event != NULL) {
            event->command          = SOUND_EVENT_SCRIPT_SET_VOLUME;
            args                    = &event->args.script;
            args->soundId           = _sndScriptRemapType1Id(arg0);
            args->level.volumeScale = arg1;
            if ((s8)arg1 < 0) {
                args->level.volumeScale = SOUND_SCRIPT_VOLUME_UNITY;
            }
            sndEvtEnqueue(event);
        }
    }
}

void SndBank_SetEnableFlags(s32 arg0, s32 arg1)
{
    enum { SOUND_EVENT_STOP_KEEP_RELEASE = 1 };
    SndEvt*           event;
    SndEvtScriptArgs* args;

    if (arg1 == 0x80000000) {
        for (arg1 = 0; arg1 < 0x10; arg1++) {
            D_80082138[arg1] = arg0 & 1;
        }
    } else {
        D_80082138[(u32)(arg1 & 0xF0000000) >> 28] = arg0 & 1;
        if (arg0 == 0 && (arg1 & 0xF0000000) == 0x40000000) {
            event = sndEvtAlloc();
            if (event != NULL) {
                event->command    = SOUND_EVENT_SCRIPT_STOP;
                args              = &event->args.script;
                args->soundId     = _sndScriptRemapType1Id(0x40000000);
                args->stopControl = SOUND_EVENT_STOP_KEEP_RELEASE;
                sndEvtEnqueue(event);
            }
        }
    }
}

static void SndVoice_SetPriority(s8 arg0)
{
    SndVoice_SetPriorityLevel(arg0);
}

s32 SndVoice_HasActiveId(s32 arg0)
{
    return ~SndVoice_FindById(_sndScriptRemapType1Id(arg0)) != 0;
}

void SndEvt_EnqueueTypeD(void)
{
    SndEvt* event;

    event = sndEvtAlloc();
    if (event != NULL) {
        event->command = SOUND_EVENT_SCRIPT_DUCK_ACQUIRE;
        sndEvtEnqueue(event);
    }
}

void SndEvt_EnqueueTypeE(void)
{
    SndEvt* event;

    event = sndEvtAlloc();
    if (event != NULL) {
        event->command = SOUND_EVENT_SCRIPT_DUCK_RELEASE;
        sndEvtEnqueue(event);
    }
}

static void SndEvt_EnqueueTypeF(void)
{
    SndEvt* event;

    event = sndEvtAlloc();
    if (event != NULL) {
        event->command = SOUND_EVENT_SCRIPT_KEY_OFF;
        sndEvtEnqueue(event);
    }
}

s32 SndScript_StopMatching(s32 arg0, s32 arg1)
{
    _SndScript* p;
    s32         i;
    s32         group;
    s32         ret;

    if (!(arg0 & 0xFF)) {
        for (i = 0; i < 8; i++) {
            p     = &SndScript_Slots[i];
            group = p->soundId & 0xF0000000;
            if ((group == arg0) || ((arg0 == 0x80000000) && (group != 0x60000000))) {
                if ((p->state != SOUND_SCRIPT_STOPPING) && (p->state != SOUND_SCRIPT_IDLE)) {
                    p->keepRelease = (arg1 == SOUND_SCRIPT_KEEP_RELEASE);
                    p->state       = SOUND_SCRIPT_STOPPING;
                }
            }
        }
        return -2;
    }

    ret = 0;
    for (i = 0; i < 8; i++) {
        p = &SndScript_Slots[i];
        if ((p->soundId == arg0) || ((p->soundId | 0xFF00) == arg0)) {
            switch (p->state) {
                case SOUND_SCRIPT_RUNNING:
                    if (arg1 != 0) {
                        if (arg1 != SOUND_SCRIPT_KEEP_RELEASE) {
                            linInterpSetup(&p->volumeRamp, (u8)D_80082748, 0, arg1);
                            p->state = SOUND_SCRIPT_FADING_OUT;
                            break;
                        }
                        p->keepRelease = arg1;
                    }
                    /* fallthrough */
                case SOUND_SCRIPT_STOPPING:
                case SOUND_SCRIPT_MUTING:
                case SOUND_SCRIPT_UNMUTING:
                    p->state = SOUND_SCRIPT_STOPPING;
                    break;
                case SOUND_SCRIPT_STARTING:
                    p->state = SOUND_SCRIPT_IDLE;
                    break;
            }
        }
        ret = i;
    }
    return ret;
}

static void SndVoice_StepMasterLevel(void)
{
    s16 var_a0;
    s8  bound;

    var_a0 = sndScriptGetMasterVolume();
    if (D_8008274A > 0) {
        var_a0 = var_a0 + D_8008274A;
        bound  = (u8)D_80082749;
        if (bound < var_a0) {
            if (bound != 0) {
                var_a0     = bound;
                D_80082749 = 0;
            }
            D_8008274A = 0;
        }
    } else if (D_8008274A < 0) {
        var_a0 = var_a0 + D_8008274A;
        if (var_a0 < 0x30) {
            var_a0     = 0x30;
            D_8008274A = 0;
        }
    }
    sndScriptSetMasterVolume(var_a0);
}

static s32 SndVoice_DriveSlots(s32* unused)
{
    SpuVoiceRef   ref;
    SpuVolume     panVolumes;
    SpuVoiceAttr* attr;
    _SndScript*   p;
    _SndVoice*    node;
    _SndVoice*    voice;
    s32           i;
    s32           count;
    s32           level;
    s32           temp;
    s8            step;
    s16           atten;

    if (D_8008274A != 0) {
        SndVoice_StepMasterLevel();
    }

    for (i = 0; i < 8; i++) {
        p = &SndScript_Slots[i];
        switch (p->state) {
            case SOUND_SCRIPT_IDLE:
                break;

            case SOUND_SCRIPT_STARTING:
                p->keepRelease        = 0;
                p->ended              = 0;
                p->tickClock          = 0;
                p->runningTicks       = 0;
                p->voices             = NULL;
                p->state              = SOUND_SCRIPT_RUNNING;
                p->volumeRamp.enabled = LINEAR_INTERPOLATOR_BYPASS;
                p->panStep            = 0;
                p->attenuationStep    = 0;
                p->mixDirty           = 0;
                goto run;

            case SOUND_SCRIPT_FADING_OUT:
                if (p->volumeRamp.gain == p->volumeRamp.targetGain) {
                    p->state = SOUND_SCRIPT_STOPPING;
                    goto stop;
                }
                linInterpStep(&p->volumeRamp);
                p->mixDirty = 1;
                /* fallthrough */
            case SOUND_SCRIPT_RUNNING:
            run:
                p->runningTicks++;
                while (SndScript_Exec(p) != 0) {
                }
            update:
                count = 0;
                if (p->voices != NULL) {
                    node = p->voices;
                    do {
                        SndVoice_Tick(node);
                        // Pan and attenuation advance once per voice, not once per update.
                        step = p->panStep;
                        count++;
                        if (step != 0) {
                            level = step + (s8)p->panOffset * 4;
                            if (step > 0) {
                                if ((s8)p->panTarget * 4 < level) {
                                    p->panOffset = p->panTarget;
                                    p->panStep   = 0;
                                } else {
                                    /* level / 4, rounded toward zero */
                                    temp = level;
                                    if (temp < 0) {
                                        temp += 3;
                                    }
                                    p->panOffset = temp >> 2;
                                }
                            } else if (level < (s8)p->panTarget * 4) {
                                p->panOffset = p->panTarget;
                                p->panStep   = 0;
                            } else {
                                temp = level;
                                if (temp < 0) {
                                    temp += 3;
                                }
                                p->panOffset = temp >> 2;
                            }
                            p->mixDirty = 1;
                        }
                        step = p->attenuationStep;
                        if (step != 0) {
                            atten = (s8)p->attenuation + step;
                            if (step > 0) {
                                if ((s8)p->attenuationTarget < atten) {
                                    p->attenuation     = p->attenuationTarget;
                                    p->attenuationStep = 0;
                                } else {
                                    p->attenuation = atten;
                                }
                            } else if (atten < (s8)p->attenuationTarget) {
                                p->attenuation     = p->attenuationTarget;
                                p->attenuationStep = 0;
                            } else {
                                p->attenuation = atten;
                            }
                            p->mixDirty = 1;
                        }
                        if (p->mixDirty == 1) {
                            Spu_GetVoiceRef(node->spuVoice, &ref);
                            attr = ref.attr;
                            SndVoice_ScaleVolume(p->panOffset, p->attenuation, node, &p->volumeRamp, &panVolumes);
                            attr->volume.left   = panVolumes.left;
                            attr->volume.right  = panVolumes.right;
                            attr->volmode.left  = 0;
                            attr->volmode.right = 0;
                            attr->mask         |= 0xF;
                        }
                        node = node->next;
                    } while (node != NULL);
                    p->mixDirty = 0;
                }
                if (count == 0 && p->ended == 1) {
                    goto release;
                }
                break;

            case SOUND_SCRIPT_MUTING:
                linInterpStep(&p->volumeRamp);
                p->mixDirty = 1;
                goto update;

            case SOUND_SCRIPT_UNMUTING:
                p->mixDirty = 1;
                linInterpStep(&p->volumeRamp);
                if (p->volumeRamp.gain == p->volumeRamp.targetGain) {
                    p->state = SOUND_SCRIPT_RUNNING;
                    goto run;
                }
                goto update;

            case SOUND_SCRIPT_STOPPING:
            stop:
                p->ended = 1;
                if (SndScript_TickVoices(p) != 0) {
                    p->state = SOUND_SCRIPT_RELEASING;
                    break;
                }
                goto release;

            case SOUND_SCRIPT_RELEASING:
                count = 0;
                if (p->voices != NULL) {
                    node = p->voices;
                    do {
                        if (node->envelope.active != 0) {
                            count++;
                            SndVoice_TickEnvelope(node);
                        }
                        node = node->next;
                    } while (node != NULL);
                }
                if (count == 0 && p->ended == 1) {
                release:
                    p->ended   = 0;
                    p->soundId = -1;
                    p->state   = SOUND_SCRIPT_IDLE;
                    for (voice = p->voices; voice != NULL; voice = voice->next) {
                        voice->script = 0;
                    }
                    p->voices             = NULL;
                    p->volumeRamp.enabled = LINEAR_INTERPOLATOR_BYPASS;
                }
                break;
        }
    }
    return 0;
}

static void SndVoice_ScanCandidates(_SndScriptSlotPick* candidates, u16 arg1, s32 arg2, u16 arg3)
{
    s8          i;
    _SndScript* p;
    u16         temp;
    s32         score;

    candidates->slot               = -1;
    candidates->equalPriorityTicks = -1;
    candidates->equalPrioritySlot  = -1;
    candidates->oldestGroupTicks   = -1;
    candidates->oldestGroupSlot    = -1;
    candidates->lowerPrioritySlot  = -1;
    candidates->idleSlot           = -1;
    candidates->lastGroupSlot      = -1;
    candidates->newestGroupSlot    = -1;
    candidates->lowestPriority     = arg1;
    candidates->newestGroupTicks   = 0xFFFF;
    candidates->groupCount         = 0;

    for (i = 0; i < 8; i++) {
        p = &SndScript_Slots[i];
        if (p->state == SOUND_SCRIPT_IDLE) {
            candidates->idleSlot = i;
        } else if (p->state != SOUND_SCRIPT_STOPPING) {
            temp = p->entryControls->priority;
            if (temp < (u32)candidates->lowestPriority) {
                candidates->lowestPriority    = temp;
                candidates->lowerPrioritySlot = i;
            } else if (candidates->lowestPriority == temp) {
                if ((candidates->equalPrioritySlot == -1) || (candidates->equalPriorityTicks < p->runningTicks)) {
                    score                          = p->runningTicks;
                    candidates->equalPrioritySlot  = i;
                    candidates->equalPriorityTicks = score;
                }
            }
            if (((p->soundId & 0xFFFF00FF) == (arg2 & 0xFFFF00FF)) ||
                (((temp = p->entryControls->flags) & SOUND_SCRIPT_GROUP_BY_FLAGS) && (arg3 == temp))) {
                candidates->lastGroupSlot = i;
                if ((candidates->newestGroupSlot == -1) || (candidates->newestGroupTicks > p->runningTicks)) {
                    score                        = p->runningTicks;
                    candidates->newestGroupSlot  = i;
                    candidates->newestGroupTicks = score;
                }
                candidates->groupCount += 1;
                if ((candidates->oldestGroupSlot == -1) || (candidates->oldestGroupTicks < p->runningTicks)) {
                    score                        = p->runningTicks;
                    candidates->oldestGroupSlot  = i;
                    candidates->oldestGroupTicks = score;
                }
            }
        }
    }
}

void SndVoice_KeyOffMatching(void)
{
    SpuVoiceRef ref;
    s32         i;
    _SndScript* p;
    _SndVoice*  head;
    _SndVoice*  node;
    s32         type;
    s32         emptyType;

    for (i = 0; i < 8; i++) {
        p    = &SndScript_Slots[i];
        head = p->voices;
        if (head != NULL) {
            type = p->soundId & 0xF0000000;
            if (type != 0x60000000) {
                node = head;
                if ((type == SOUND_SCRIPT_REQUEST_TYPE_1) || (type == 0x50000000)) {
                    do {
                        Spu_GetVoiceRef(p->voices->spuVoice, &ref);
                        ref.attr->adsr2  = (ref.attr->adsr2 & 0xFFE0) | 0xB;
                        ref.attr->adsr2 |= 0x20;
                        ref.attr->mask  |= SPU_VOICE_ADSR_ADSR2;
                        Spu_KeyOff(node->spuVoice);
                        node = node->next;
                    } while (node != NULL);
                    p->state   = SOUND_SCRIPT_IDLE;
                    p->soundId = -1;
                }
            }
        } else {
            emptyType = p->soundId & 0xF0000000;
            if ((emptyType == 0x50000000) || (emptyType == SOUND_SCRIPT_REQUEST_TYPE_1)) {
                p->state   = SOUND_SCRIPT_IDLE;
                p->soundId = -1;
            }
        }
    }
}

/// Adds one waiting update to the script's 16.16 command clock.
///
/// PAL adds 39321/65536 of a tick (0.6 rounded down); every other display
/// region adds one tick. Called only while a Loop, Wait or note delay is still
/// pending, so immediately executable commands consume time without adding it.
static inline void _sndScriptAdvanceClock(_SndScript* script)
{
    enum {
        SOUND_SCRIPT_CLOCK_STEP_PAL   = 0x9999,
        SOUND_SCRIPT_CLOCK_STEP_WHOLE = 0x10000
    };
    script->tickClock += (gDisplayState.region == MODE_PAL ? SOUND_SCRIPT_CLOCK_STEP_PAL : SOUND_SCRIPT_CLOCK_STEP_WHOLE);
}

/// Decides whether a note plays with reverb, from its own level against a
/// global one. A note at level 3 gets reverb whenever the global level is at
/// least 2; a global level of 3 turns it off for every other note; otherwise a
/// note with a non-negative level gets reverb once the global level reaches it.
static inline u8 _sndScriptUseReverb(_SndScriptNote* note)
{
    s32 on;

    if (note->reverbLevel == 3 && D_8008274B >= 2) {
        on = 1;
    } else if (note->reverbLevel != 3 && D_8008274B == 3) {
        on = 0;
    } else {
        on = 0;
        if (note->reverbLevel >= 0) {
            on = D_8008274B >= note->reverbLevel;
        }
    }
    return on;
}

static s32 SndScript_Exec(_SndScript* script)
{
    enum {
        SOUND_BANK_PAN_CENTER        = 64,
        SOUND_BANK_PAN_MAX           = 127,
        SOUND_BANK_KEY_FRACTION_BITS = 7
    };
    SpuVoiceRef     voiceRef;
    SpuVolume       panVolumes;
    _SndScriptCmd*  cmd;
    _SndScriptNote* note;
    _SndVoice*      voice;
    SndBankLayer*   bankLayer;
    SpuVoiceAttr*   attr;
    SndBankSlot*    bankSlot;
    SndBank*        bank;
    SndBankHdr*     header;
    s32             result;
    s32             ticks;
    s32             wait;
    s32             index;
    s32             masterVolume;
    u8              layerVolume;
    s32             panSum;
    s16             pan;
    s16             voicePan;
    s32             pitchValue;
    u16             pitch;
    s32             countdown;
    s16             envelopeOffset;

    cmd = (_SndScriptCmd*)script->cursor;
    switch ((u32)cmd->magic) {
        case SOUND_SCRIPT_PITCH_ENVELOPE_TAG:
            // Envelope data is not a command; halt without advancing the cursor.
            result = 0;
            break;
        case SOUND_SCRIPT_END_TAG:
            script->ended = 1;
            result        = 0;
            break;
        case SOUND_SCRIPT_LOOP_TAG:
            if (script->loopDepth >= 8U) {
                script->ended = 1;
                result        = 0;
                break;
            }
            ticks = script->tickClock;
            if ((ticks >> 16) >= cmd->data.loop.delayTicks) {
                script->loopCounts[script->loopDepth]  = cmd->data.loop.repeatCount;
                script->cursor                         = script->cursor + sizeof(_SndScriptCmd);
                script->loopCursors[script->loopDepth] = script->cursor;
                script->loopDepth++;
                script->tickClock -= cmd->data.loop.delayTicks << 16;
                result             = 1;
            } else {
                _sndScriptAdvanceClock(script);
                result = 0;
            }
            break;
        case SOUND_SCRIPT_LOOP_END_TAG:
            if (script->loopDepth == 0) {
                script->ended = 1;
                result        = 0;
                break;
            }
            index = script->loopDepth - 1;
            if (script->loopCounts[index] == 1) {
                script->cursor = (u8*)cmd + sizeof(cmd->magic);
                script->loopDepth--;
            } else {
                script->cursor = script->loopCursors[index];
                if (script->loopCounts[script->loopDepth - 1] != 0) {
                    script->loopCounts[script->loopDepth - 1]--;
                }
            }
            result = 1;
            break;
        case SOUND_SCRIPT_ENTRY_TAG:
            header = script->bankSlot->image;
            // Reload this entry's controls. The next 24 bytes are then read as a
            // note even when their tag is not oneV.
            script->entryControls = (SndScriptEntryControls*)((u8*)header + *(header->entryOffsets + (u8)script->soundId));
            script->cursor        = script->cursor + sizeof(SndScriptEntryControls);
        case SOUND_SCRIPT_NOTE_TAG:
            note  = (_SndScriptNote*)script->cursor;
            ticks = script->tickClock;
            if ((ticks >> 16) < note->delayTicks) {
                _sndScriptAdvanceClock(script);
                result = 0;
                break;
            }
            voice  = SndVoice_Alloc(note->voicePriority);
            result = 1;
            if (voice != NULL) {
                bankSlot = script->bankSlot;
                if (note->bankId != 0) {
                    bank = sndBankFind(note->bankId);
                    if (bank == 0) {
                        // Not loaded yet: release the voice and retry this command.
                        voice->allocated = 0;
                        Spu_ReleaseVoiceSlot(voice->spuVoice);
                        Spu_ClearVoiceCallbacks(voice->spuVoice);
                        voice->spuVoice = 0;
                        return 0;
                    }
                } else {
                    bank = bankSlot->bank;
                }
                Spu_GetVoiceRef(voice->spuVoice, &voiceRef);
                bankLayer    = Snd_GetNote(bank, (u8)note->program, note->layer);
                attr         = voiceRef.attr;
                masterVolume = D_80082748;
                attr->addr   = bankLayer->waveAddr;
                if ((D_80082749 != 0) && (script->entryControls->flags & SOUND_SCRIPT_USE_UNDUCKED_VOLUME)) {
                    masterVolume = D_80082749;
                }
                // A negative override keeps the layer value. Entry pan bias is applied after.
                layerVolume = (u8)note->volumeOverride;
                if (note->volumeOverride < 0) {
                    layerVolume = bankLayer->volume;
                }
                voice->baseVolume   = layerVolume;
                voice->scaledVolume = (s8)((masterVolume * script->entryControls->volumeScale * voice->baseVolume) / (SOUND_SCRIPT_VOLUME_UNITY * SOUND_SCRIPT_VOLUME_UNITY));
                pan                 = script->entryControls->panBias;
                panSum              = note->panOverride;
                if (panSum < 0) {
                    panSum = bankLayer->pan;
                }
                panSum  += (s16)(pan - SOUND_BANK_PAN_CENTER);
                voicePan = panSum;
                if (voicePan <= SOUND_BANK_PAN_MAX) {
                    if (voicePan >= 0) {
                        voice->basePan = panSum;
                    } else {
                        voice->basePan = 0;
                    }
                } else {
                    voice->basePan = SOUND_BANK_PAN_MAX;
                }
                if (SndScript_FindOneA((u8*)script->bankSlot->image, note->adsrOffset, attr) == -1) {
                    attr->adsr1 = bankLayer->adsr1;
                    attr->adsr2 = bankLayer->adsr2;
                }
                // Script pitch is a Q7 offset from the layer's minimum key.
                pitchValue = pitch = note->pitchOffset + (bankLayer->keyMin << SOUND_BANK_KEY_FRACTION_BITS);
                attr->pitch        = Spu_CalcVolume((u32)(pitch & 0xFFFF) >> SOUND_BANK_KEY_FRACTION_BITS, (pitchValue & 0x7F) * 2, bankLayer->rootKey, bankLayer->fineTune);
                if (_sndScriptUseReverb(note) == 0) {
                    Spu_DisableReverbVoice(voice->spuVoice);
                    voice->field_1 = 1;
                } else {
                    Spu_EnableReverbVoice(voice->spuVoice);
                    voice->field_1 = 1;
                }
                SndVoice_ScaleVolume(script->panOffset, script->attenuation, voice, &script->volumeRamp, &panVolumes);
                attr->volume.left   = panVolumes.left;
                attr->volume.right  = panVolumes.right;
                attr->volmode.left  = 0;
                attr->volmode.right = 0;
                attr->mask          = 0x6009F;
                Spu_KeyOn(voice->spuVoice);
                voice->note      = note;
                countdown        = note->gateTicks == 0 ? SOUND_SCRIPT_NOTE_HELD : note->gateTicks << 16;
                voice->gateClock = countdown;
                _sndVoiceAttach(script, voice);
                envelopeOffset = note->pitchEnvelopeOffset;
                if (envelopeOffset != SOUND_SCRIPT_NOTE_NO_ENVELOPE) {
                    SndVoice_SetupEnvelope(voice, envelopeOffset, pitch & 0xFFFF, bankLayer);
                    result = 1;
                } else {
                    voice->envelope.active = 0;
                    result                 = 1;
                }
            }
            script->tickClock = (s32)(script->tickClock - (note->delayTicks << 0x10));
            script->cursor    = script->cursor + sizeof(_SndScriptNote);
            break;
        case SOUND_SCRIPT_WAIT_TAG:
            ticks = script->tickClock;
            wait  = cmd->data.waitTicks;
            if ((ticks >> 16) < wait) {
                _sndScriptAdvanceClock(script);
                result = 0;
                break;
            }
            script->tickClock = ticks - (wait << 16);
            script->cursor    = script->cursor + sizeof(_SndScriptCmd);
            result            = 1;
            break;
        case SOUND_SCRIPT_ADSR_TAG:
        default:
            result = 0;
            break;
    }
    return result;
}

static void SndVoice_TickEnvelope(_SndVoice* voice)
{
    SpuVoiceRef        voiceRef;
    _SndVoiceEnvelope* player;
    _SndPitchEnvelope* envelope;
    s32                pitch;
    s32                temp;
    s32                level;
    SpuVoiceAttr*      attr;

    player   = &voice->envelope;
    envelope = player->envelope;

    // A pending release replaces the current stage and starts from the last
    // ramp offset, not the level being played. An exhausted stage falls
    // through and plays the next stage on this update.
    if (player->releaseRequest == SOUND_VOICE_ENVELOPE_RELEASE_PENDING) {
        player->stage = SOUND_VOICE_ENVELOPE_RELEASE;
        temp          = (player->rampOffset - envelope->releaseLevel) * envelope->releaseSlope;
        if (temp > 0) {
            player->releaseStep = -envelope->releaseSlope;
        } else {
            player->releaseStep = envelope->releaseSlope;
        }

        player->stageUpdates   = 0;
        player->releaseRequest = SOUND_VOICE_ENVELOPE_RELEASE_STARTED;
        player->releaseOffset  = player->rampOffset;
    }

    switch (player->stage) {
        case SOUND_VOICE_ENVELOPE_DELAY:
            if (player->stageUpdates < envelope->delayUpdates) {
                player->stageUpdates++;
                return;
            }
            player->stage        = SOUND_VOICE_ENVELOPE_ATTACK;
            player->stageUpdates = 0;
            player->attackOffset = 0;
            player->rampOffset   = 0;
        case SOUND_VOICE_ENVELOPE_ATTACK:
            pitch = (player->keyedPitch << 1) + player->attackOffset;
            if (player->stageUpdates < envelope->attackUpdates) {
                player->stageUpdates++;
                player->rampOffset = player->attackOffset += envelope->attackSlope;
                break;
            }
            player->stage        = SOUND_VOICE_ENVELOPE_HOLD;
            player->stageUpdates = 0;
        case SOUND_VOICE_ENVELOPE_HOLD:
            pitch = (player->keyedPitch << 1) + envelope->attackLevel;
            if (player->stageUpdates < envelope->holdUpdates) {
                player->stageUpdates++;
                break;
            }
            player->stage        = SOUND_VOICE_ENVELOPE_DECAY;
            player->decayOffset  = envelope->attackLevel;
            level                = envelope->attackLevel;
            player->stageUpdates = 0;
            player->rampOffset   = level;
        case SOUND_VOICE_ENVELOPE_DECAY:
            pitch = (player->keyedPitch << 1) + player->decayOffset;
            if (player->stageUpdates < envelope->decayUpdates) {
                player->stageUpdates++;
                player->rampOffset = player->decayOffset += envelope->decaySlope;
                break;
            }
            player->stage = SOUND_VOICE_ENVELOPE_SUSTAIN;
        case SOUND_VOICE_ENVELOPE_SUSTAIN:
            pitch = (player->keyedPitch << 1) + envelope->sustainLevel;
            break;
        case SOUND_VOICE_ENVELOPE_RELEASE:
            temp = (player->rampOffset - envelope->releaseLevel) * envelope->releaseSlope;
            if (temp >= 0) {
                player->stage = SOUND_VOICE_ENVELOPE_RELEASE_HOLD;
            } else {
                player->rampOffset = player->releaseOffset += player->releaseStep;
            }
            pitch = (player->keyedPitch << 1) + player->releaseOffset;
            break;
        case SOUND_VOICE_ENVELOPE_RELEASE_HOLD:
            pitch = (player->keyedPitch << 1) + envelope->releaseLevel;
            break;
        default:
            return;
    }
    Spu_GetVoiceRef(voice->spuVoice, &voiceRef);
    attr = voiceRef.attr;
    attr->pitch =
        Spu_CalcVolume((pitch >> 8) & 0xFFFF, pitch & 0xFF, player->rootKey, player->fineTune);
    attr->mask |= SPU_VOICE_PITCH;
}

s32 SndVoice_AllocSlot(s32 arg0, s8 arg1, s8 arg2, SndBankSlot* slot, SndScriptEntryControls* entryControls)
{
    _SndScriptSlotPick pick;

    SndVoice_ScanCandidates(&pick, entryControls->priority, arg0, entryControls->flags);
    if ((pick.groupCount < entryControls->maxInstances) && (pick.idleSlot != -1)) {
        pick.slot = pick.idleSlot;
    } else {
        pick.slot = SndVoice_SelectStealCandidate(&pick, entryControls->retriggerTicks);
    }
    if (pick.slot >= 0) {
        SndScript_Play(pick.slot, arg1, arg2, arg0, slot, entryControls);
    }
    return pick.slot;
}

void SndVoice_FadeMatching(s32 arg0, s32 arg1)
{
    s32         i;
    _SndScript* p;

    for (i = 0; i < 8; i++) {
        p = &SndScript_Slots[i];
        if ((arg0 == p->soundId) || ((p->soundId & 0xF0000000) == arg0)) {
            if (arg1 == 0) {
                if (p->state == SOUND_SCRIPT_MUTING) {
                    p->state = SOUND_SCRIPT_UNMUTING;
                    linInterpSetup(&p->volumeRamp, 0, (u8)D_80082748, 8);
                }
            } else {
                if (p->state & SOUND_SCRIPT_MUTABLE) {
                    p->state = SOUND_SCRIPT_MUTING;
                    linInterpSetup(&p->volumeRamp, (u8)D_80082748, 0, 8);
                }
            }
        }
    }
}

void sndScriptRampMix(s32 scriptSlotIndex, s32 panOffset, s32 attenuation)
{
    _SndScript* script;
    s32         panDelta;
    s32         panStep;
    s32         volumeScale;

    scriptSlotIndex &= ARRAY_SIZE(SndScript_Slots) - 1;
    script           = &SndScript_Slots[scriptSlotIndex];

    // Wrap the pan delta to a signed byte before choosing a snap or ramp.
    panDelta        = *(volatile u8*)&script->panOffset;
    panDelta        = panOffset - panDelta;
    script->panStep = panDelta;
    panDelta        = (s8)panDelta;
    if (panDelta < 0) {
        panDelta = -panDelta;
    }
    if ((panDelta * SOUND_SCRIPT_PAN_FRACTION_SCALE) >= SOUND_SCRIPT_MIX_RAMP_THRESHOLD + 1) {
        script->panTarget = panOffset;
        if (script->panStep <= 0) {
            if (script->panStep < 0) {
                panStep = -SOUND_SCRIPT_MIX_RAMP_STEP;
            } else {
                panStep = 0;
            }
        } else {
            panStep = SOUND_SCRIPT_MIX_RAMP_STEP;
        }
        script->panStep = panStep;
    } else {
        script->panOffset = panOffset;
        script->panStep   = 0;
    }
    script->mixDirty = 1;

    // Convert signed attenuation to the volume request used by the shared ramp.
    volumeScale = (s8)attenuation;
    if (volumeScale < 0) {
        volumeScale = volumeScale + SOUND_SCRIPT_VOLUME_UNITY;
    } else {
        volumeScale = SOUND_SCRIPT_VOLUME_UNITY - volumeScale;
    }
    sndScriptRampVolume(scriptSlotIndex, volumeScale);
    script->mixDirty = 1;
}

void sndScriptRampVolume(s32 scriptSlotIndex, s32 volumeScale)
{
    _SndScript* script;
    u8          targetAttenuation;
    s16         attenuationDelta;

    script = &SndScript_Slots[scriptSlotIndex & (ARRAY_SIZE(SndScript_Slots) - 1)];
    // Keep the unsigned target byte separate from the signed ramp difference.
    attenuationDelta  = ~volumeScale & SOUND_SCRIPT_VOLUME_UNITY;
    targetAttenuation = attenuationDelta;
    attenuationDelta -= (s8)script->attenuation;
    if (ABS(attenuationDelta) > SOUND_SCRIPT_MIX_RAMP_THRESHOLD) {
        script->attenuationTarget = targetAttenuation;
        if (attenuationDelta <= 0) {
            if (attenuationDelta < 0) {
                script->attenuationStep = -SOUND_SCRIPT_MIX_RAMP_STEP;
            } else {
                script->attenuationStep = 0;
            }
        } else {
            script->attenuationStep = SOUND_SCRIPT_MIX_RAMP_STEP;
        }
    } else {
        script->attenuation     = targetAttenuation;
        script->attenuationStep = 0;
    }
    script->mixDirty = 1;
}

void SndVoice_IncRefCount(void)
{
    s8 temp;

    D_8008274C += 1;
    if (D_8008274C == 1) {
        if (D_8008274A == 0) {
            if (D_80082749 == 0) {
                temp = sndScriptGetMasterVolume();
                if (temp >= 0x30) {
                    D_80082749 = temp;
                    D_8008274A = -8;
                }
            }
        }
    }
}

void SndVoice_TickRefCount(void)
{
    if (D_8008274C > 0) {
        D_8008274C -= 1;
        if (D_8008274C == 0) {
            if (D_80082749 != 0) {
                D_8008274A = 8;
            }
        }
    }
}

static void SndVoice_Init(void)
{
    u32  i;
    s32* ptr;
    s32* bankSlotWords;

    ptr = (s32*)SndScript_Slots;
    i   = 0;
    do {
        *ptr = 0;
        i++;
        ptr++;
    } while (i < sizeof(SndScript_Slots) / sizeof(*ptr));

    // Reset image ownership and descriptor references before boot reservations.
    bankSlotWords = (s32*)_gSndBankSlots;
    i             = 0;
    do {
        *bankSlotWords = 0;
        i++;
        bankSlotWords++;
    } while (i < sizeof(_gSndBankSlots) / sizeof(*bankSlotWords));

    ptr = (s32*)SndScript_Voices;
    i   = 0;
    do {
        *ptr = 0;
        i++;
        ptr++;
    } while (i < sizeof(SndScript_Voices) / sizeof(*ptr));

    D_8008274A = 0;
    D_80082749 = 0;
    sndScriptSetMasterVolume(0x7F);
    SndVoice_SetPriorityLevel(1);
}

static void SndVoice_SetPriorityLevel(s8 arg0)
{
    if (arg0 < 0) {
        D_8008274B = -1;
        return;
    }
    D_8008274B = arg0;
    if (arg0 == 0) {
        D_8008274B = 1;
    }
}

s32 SndVoice_FindById(s32 arg0)
{
    s32         i;
    _SndScript* p;

    i = 0;
    p = SndScript_Slots;
    do {
        if ((p->state & SOUND_SCRIPT_PLAYING) && (p->soundId == arg0)) {
            return i;
        }
        i++;
        p++;
    } while (i < 8);
    return -1;
}

/// Recomputes the stored gain index of every voice attached to one script instance.
///
/// `masterVolume` is normally 0..127 (0 silent, 127 full). Each index is
/// master * entry gain * base gain / 127^2, truncated toward zero and narrowed
/// to a signed byte. Negative requests are applied without clamping.
/// The voice chain and, for a nonempty chain, its loaded entry controls must
/// remain valid throughout the walk. A nonempty chain clears `mixDirty`;
/// an empty chain leaves it intact. No SPU state is written: the caller must
/// mark the instance for remixing to apply these gains on the next voice visit.
static inline void _sndScriptRescaleVoices(_SndScript* script, s8 masterVolume)
{
    _SndVoice* voice;

    if (script->voices != NULL) {
        voice = script->voices;
        do {
            voice->scaledVolume = (masterVolume * script->entryControls->volumeScale * voice->baseVolume) / (SOUND_SCRIPT_VOLUME_UNITY * SOUND_SCRIPT_VOLUME_UNITY);
            voice               = voice->next;
        } while (voice != NULL);
        script->mixDirty = 0;
    }
}

void sndScriptSetMasterVolume(s8 masterVolume)
{
    _SndScript* script;
    s32         slotIndex;
    s8          storedVolume;

    // Entries exempt from active ducking keep their previously scaled gain.
    for (slotIndex = 0; slotIndex < ARRAY_SIZE(SndScript_Slots); slotIndex++) {
        script = &SndScript_Slots[slotIndex];
        if ((script->useUnduckedVolume != 1) || (D_80082749 == 0)) {
            _sndScriptRescaleVoices(script, masterVolume);
            script->mixDirty = 1;
        }
    }
    // Clamp the saved level only after applying the original signed request.
    storedVolume = masterVolume;
    if (storedVolume < 0) {
        storedVolume = 0;
    }
    D_80082748 = storedVolume;
}

s8 sndScriptGetMasterVolume(void)
{
    return D_80082748;
}

static s8 SndVoice_SelectStealCandidate(_SndScriptSlotPick* candidates, s32 retriggerTicks)
{
    s32 v;
    u8  u;
    s32 none;

    none = -1;
    if (retriggerTicks == SOUND_SCRIPT_RETRIGGER_DISABLED) {
        return -9;
    }
    if (candidates->newestGroupTicks < retriggerTicks) {
        return -5;
    }
    if (candidates->newestGroupSlot != none) {
        goto field6;
    }
    v = candidates->lowerPrioritySlot;
    u = candidates->lowerPrioritySlot;
    if (v != none) {
        goto store;
    }
    v = candidates->equalPrioritySlot;
    u = candidates->equalPrioritySlot;
join:
    if (v == none) {
        goto ret_m6;
    }
store:
    candidates->slot = u;
    return v;
field6:
    v = candidates->oldestGroupSlot;
    u = candidates->oldestGroupSlot;
    goto join;
ret_m6:
    return -6;
}

static void SndScript_Play(s32 arg0, s8 arg1, s8 arg2, s32 arg3, SndBankSlot* slot, SndScriptEntryControls* entryControls)
{
    _SndScript*             p;
    _SndVoice*              node;
    SndScriptEntryControls* controls;
    u16                     flags;

    controls = entryControls;
    p        = &SndScript_Slots[arg0];
    node     = p->voices;
    if (node != NULL) {
        do {
            Spu_KeyOff(node->spuVoice);
            node->allocated = 0;
            Spu_ClearVoiceCallbacks(node->spuVoice);
            Spu_ReleaseVoiceSlot(node->spuVoice);
            node->spuVoice = 0;
            node           = node->next;
        } while (node != NULL);
    }
    p->state             = SOUND_SCRIPT_STARTING;
    p->voices            = NULL;
    p->bankSlot          = slot;
    p->soundId           = arg3;
    p->runningTicks      = 0;
    p->panOffset         = arg1;
    p->attenuation       = arg2;
    p->loopDepth         = 0;
    flags                = controls->flags;
    p->cursor            = (u8*)entryControls;
    p->useUnduckedVolume = (flags & SOUND_SCRIPT_USE_UNDUCKED_VOLUME) != 0;
}

/// Releases and unlinks a script voice when the SPU voice ends or is stolen.
///
/// `context` is the `_SndVoice` registered with the SPU; `NULL` is ignored.
/// The live script and neighbors must still exist. Clears the allocation mark,
/// hardware voice number and neighbor links, retaining the script pointer and
/// note/envelope data. The SPU caller manages its hardware slot and callback
/// registration; this callback only releases the script's voice record.
static void _sndVoiceDetach(void* context)
{
    _SndVoice* voice = context;
    union {
        _SndVoice*  voice;
        _SndScript* script;
    } neighborOrOwner;
    _SndVoice* nextVoice;

    /// Unlinks `voice` using its captured previous link in `neighborOrOwner.voice`.
    ///
    /// Requires the local `_SndVoice* voice`, `_SndVoice* nextVoice` and the
    /// voice/script pointer union `neighborOrOwner`; updates the latter two
    /// temporaries and clears only the voice's neighbor links.
#define SOUND_SCRIPT_UNLINK_VOICE()                                  \
    do {                                                             \
        if (neighborOrOwner.voice == NULL) {                         \
            nextVoice = voice->next;                                 \
            if (nextVoice == NULL) {                                 \
                neighborOrOwner.script = voice->script;              \
                if (neighborOrOwner.script != NULL) {                \
                    neighborOrOwner.script->voices = NULL;           \
                }                                                    \
            } else {                                                 \
                neighborOrOwner.script = voice->script;              \
                if (neighborOrOwner.script != NULL) {                \
                    neighborOrOwner.script->voices = nextVoice;      \
                }                                                    \
                neighborOrOwner.voice       = voice->next;           \
                neighborOrOwner.voice->prev = NULL;                  \
            }                                                        \
        } else {                                                     \
            nextVoice = voice->next;                                 \
            if (nextVoice == NULL) {                                 \
                neighborOrOwner.voice->next = NULL;                  \
            } else {                                                 \
                neighborOrOwner.voice->next = nextVoice;             \
                nextVoice                   = voice->next;           \
                neighborOrOwner.voice       = voice->prev;           \
                nextVoice->prev             = neighborOrOwner.voice; \
            }                                                        \
        }                                                            \
        voice->prev = NULL;                                          \
        voice->next = NULL;                                          \
    } while (0)

    if (voice != NULL) {
        neighborOrOwner.voice = voice->prev;
        voice->allocated      = 0;
        voice->spuVoice       = 0;
        SOUND_SCRIPT_UNLINK_VOICE();
    }
}
#undef SOUND_SCRIPT_UNLINK_VOICE

/// Finds the first sound-script slot whose attached descriptor matches `bankId`.
///
/// `SOUND_BANK_SLOT_MATCH_ID` compares the complete 16-bit descriptor id;
/// `SOUND_BANK_SLOT_MATCH_TYPE` compares only bits 12..15 selected by
/// `SOUND_BANK_TYPE_MASK`. Slots are searched in ascending index order;
/// no match or an unsupported `matchMode` returns `NULL`.
///
/// The result borrows a stable slot. Matching includes `SOUND_BANK_ID_FREE`
/// descriptors and slots whose image was released. Callers reading `image`
/// require a completed image; script playback also requires initialized sample
/// tables. These resources must remain loaded through their last use.
static SndBankSlot* _sndBankSlotFind(u16 bankId, s32 matchMode)
{
    s32            slotIndex;
    SndBankSlot*   slot;
    const SndBank* bank;
    s32            matchKey;

    switch (matchMode) {
        case SOUND_BANK_SLOT_MATCH_ID:
            slotIndex = 0;
            matchKey  = bankId;
            slot      = _gSndBankSlots;
            do {
                bank = slot->bank;
                if (bank != NULL && bank->bankId == matchKey) {
                    return slot;
                }
                slotIndex++;
                slot++;
            } while (slotIndex < ARRAY_SIZE(_gSndBankSlots));
            return NULL;
        case SOUND_BANK_SLOT_MATCH_TYPE:
            slotIndex = 0;
            matchKey  = bankId & SOUND_BANK_TYPE_MASK;
            slot      = _gSndBankSlots;
            do {
                bank = slot->bank;
                if (bank != NULL && (bank->bankId & SOUND_BANK_TYPE_MASK) == matchKey) {
                    return slot;
                }
                slotIndex++;
                slot++;
            } while (slotIndex < ARRAY_SIZE(_gSndBankSlots));
            break;
    }
    return NULL;
}

SndBankSlot* sndBankSlotGet(s32 slotIndex)
{
    if ((u8)slotIndex < ARRAY_SIZE(_gSndBankSlots)) {
        return &_gSndBankSlots[(s8)slotIndex];
    }
    return NULL;
}

void SndBankSlot_Free(s32 arg0)
{
    enum { SOUND_BANK_SLOT_ID_FREE = -1 };
    SndBankSlot* slot;
    SndBankSlot* base;

    if ((u8)arg0 < ARRAY_SIZE(_gSndBankSlots)) {
        base = _gSndBankSlots;
        slot = &base[(s8)arg0];
        sndHeapFree(slot->image);
        slot->bankId = SOUND_BANK_SLOT_ID_FREE;
        slot->image  = NULL;
    }
}

static _SndVoice* SndVoice_Alloc(s32 arg0)
{
    s32        voiceIdx;
    _SndVoice* ptr;

    voiceIdx = (s8)Spu_AllocVoice(SndScript_VoiceRanges, 2, arg0 & 0xFFFF);
    if (voiceIdx < 0) {
        return NULL;
    }
    /* Ranges 1 and 2 cover hardware voices 16..23. */
    ptr           = &SndScript_Voices[voiceIdx - 16];
    ptr->spuVoice = voiceIdx;
    Spu_SetVoiceCallbacks(voiceIdx, _sndVoiceDetach, ptr);
    ptr->allocated = 1;
    return ptr;
}

/// Links a script voice at the head of its owner's doubly linked voice list.
///
/// `voice` must be live and outside any active voice list, and `script` must
/// remain live until the voice is detached. A `NULL` script clears only its three list fields;
/// it does not unlink an existing owner. Allocation and SPU state are untouched.
static void _sndVoiceAttach(_SndScript* script, _SndVoice* voice)
{
    _SndVoice* previousHead;

    if (script != NULL) {
        previousHead = script->voices;
        if (previousHead != NULL) {
            script->voices     = voice;
            voice->next        = previousHead;
            previousHead->prev = voice;
            voice->prev        = NULL;
            voice->script      = script;
            return;
        }
        script->voices = voice;
        voice->script  = script;
        voice->next    = NULL;
        voice->prev    = NULL;
        return;
    }
    voice->next   = NULL;
    voice->prev   = NULL;
    voice->script = NULL;
}

static s32 SndVoice_Tick(_SndVoice* voice)
{
    s32 temp;

    temp = voice->gateClock;
    if (temp <= 0) {
        voice->gateClock = 0;
        Spu_KeyOff(voice->spuVoice);
        if (voice->envelope.active != 0) {
            if (voice->envelope.releaseRequest == SOUND_VOICE_ENVELOPE_HELD) {
                voice->envelope.releaseRequest = SOUND_VOICE_ENVELOPE_RELEASE_PENDING;
            }
            if (voice->envelope.active != 0) {
                SndVoice_TickEnvelope(voice);
            }
        }
    } else {
        if (temp <= 0x7FFFFFFE) {
            if (gDisplayState.region == MODE_PAL) {
                voice->gateClock = temp + 0xFFFF6667;
            } else {
                voice->gateClock = temp + 0xFFFF0000;
            }
        }
        if (voice->envelope.active != 0) {
            SndVoice_TickEnvelope(voice);
        }
    }
    return 0;
}

static s32 SndScript_TickVoices(_SndScript* script)
{
    SpuVoiceRef voiceRef;
    _SndVoice*  node;
    _SndVoice*  head;
    s32         count;
    u8          status;
    u16         temp;

    head  = script->voices;
    count = 0;
    if (head != NULL) {
        node = head;
        do {
            if (node->spuVoice >= 0) {
                // Keep the voice's ADSR only for an explicit keep-release stop.
                if (script->keepRelease != SOUND_SCRIPT_KEEP_RELEASE) {
                    status = Spu_GetVoiceStatus(node->spuVoice);
                    if (status != 0) {
                        Spu_GetVoiceRef(node->spuVoice, &voiceRef);
                        temp                 = voiceRef.attr->adsr2;
                        temp                 = (temp & 0xFFE0) | 5;
                        voiceRef.attr->adsr2 = temp;
                        voiceRef.attr->mask |= SPU_VOICE_ADSR_ADSR2;
                        if (status != 2) {
                            Spu_KeyOff(node->spuVoice);
                        }
                    }
                } else {
                    Spu_KeyOff(node->spuVoice);
                }
                if (node->envelope.active != 0) {
                    count                        += 1;
                    node->envelope.releaseRequest = SOUND_VOICE_ENVELOPE_RELEASE_PENDING;
                }
            }
            node = node->next;
        } while (node != NULL);
    }
    return count;
}

static void SndVoice_ScaleVolume(s8 arg0, s8 arg1, _SndVoice* voice, LinInterp* ramp, SpuVolume* panVolumes)
{
    s32 vol;

    if (voice->spuVoice >= 0) {
        vol = 0x7F - abs(arg1);
        vol = voice->scaledVolume * abs(vol) / 127;
        vol = (vol < 0x80) ? ((vol < 0) ? 0 : vol) : 0x7F;
        spuCalcPanVolumes(panVolumes, (s8)voice->basePan + arg0 * 3,
                          linInterpApply(ramp, Snd_VelocityGainTable[vol]));
    }
}

static void SndVoice_SetupEnvelope(_SndVoice* voice, s16 envelopeOffset, u32 pitch, SndBankLayer* bankLayer)
{
    _SndVoiceEnvelope* player;
    u8*                base;
    _SndPitchEnvelope* envelope;
    s32                magic;
    s16                temp;

    player = &voice->envelope;
    if (envelopeOffset == SOUND_SCRIPT_NOTE_NO_ENVELOPE) {
        voice->envelope.active = 0;
        return;
    }
    if (voice->script == NULL) {
        voice->envelope.active = 0;
        return;
    }
    // pitchEnvelopeOffset is a byte offset from the start of the bank image.
    // A tag other than oneE keeps the pointer and leaves the player flags unchanged.
    base             = (u8*)voice->script->bankSlot->image;
    envelope         = (_SndPitchEnvelope*)&base[envelopeOffset];
    player->envelope = envelope;
    magic            = envelope->magic;
    if (magic == SOUND_SCRIPT_PITCH_ENVELOPE_TAG) {
        voice->envelope.active = 1;
        player->stage          = SOUND_VOICE_ENVELOPE_DELAY;
        player->releaseRequest = SOUND_VOICE_ENVELOPE_HELD;
        player->keyedPitch     = pitch & 0xFFFF;
        player->rootKey        = bankLayer->rootKey;
        temp                   = bankLayer->fineTune;
        player->stageUpdates   = 0;
        player->attackOffset   = 0;
        player->decayOffset    = 0;
        player->releaseOffset  = 0;
        player->fineTune       = temp;
    }
}

static s32 SndScript_FindOneA(u8* arg0, s16 arg1, SpuVoiceAttr* arg2)
{
    _SndScriptAdsr* chunk;

    if (arg1 != SOUND_SCRIPT_NOTE_LAYER_ADSR) {
        chunk = (_SndScriptAdsr*)&arg0[arg1];
        if (chunk->magic == SOUND_SCRIPT_ADSR_TAG) {
            arg2->adsr1 = chunk->adsr1;
            arg2->adsr2 = chunk->adsr2;
            return 1;
        }
        return -1;
    }
    return -1;
}

static void SndVoice_ClearActive(void)
{
    s32         i;
    s32         mask;
    s32         c600;
    s32         c500;
    s32         c100;
    _SndScript* p;
    s32         temp;

    i    = 0;
    mask = 0xF0000000;
    c600 = 0x60000000;
    c500 = 0x50000000;
    c100 = SOUND_SCRIPT_REQUEST_TYPE_1;
    p    = SndScript_Slots;
    do {
        temp = p->soundId & mask;
        if (temp != c600) {
            if ((temp == c500) || (temp == c100)) {
                p->state = SOUND_SCRIPT_IDLE;
            }
        }
        i++;
        p++;
    } while (i < 8);
}
