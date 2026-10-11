#include "main/sound.h"

#include <psyq/sys/types.h>
#include <psyq/abs.h>
#include <psyq/libspu.h>
#include <psyq/libetc.h>

#include "common.h"

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

// Result of applying an optional image-relative ADSR override.
enum {
    SOUND_SCRIPT_ADSR_NOT_APPLIED = -1,
    SOUND_SCRIPT_ADSR_APPLIED     = 1
};

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

// A stable first duck request saves the master gain and ramps toward 48.
enum {
    SOUND_SCRIPT_DUCK_LEVEL = 48,
    SOUND_SCRIPT_DUCK_STEP  = 8
};

// Mix changes above 32 ramp by 8 per voice visit; pan uses quarter-offset units.
enum {
    SOUND_SCRIPT_MIX_RAMP_THRESHOLD = 32,
    SOUND_SCRIPT_MIX_RAMP_STEP      = 8,
    SOUND_SCRIPT_PAN_FRACTION_SCALE = 4
};

// Level 3 admits only level-3 notes; level 2 also admits those notes.
enum {
    SOUND_SCRIPT_REVERB_DISABLED        = -1,
    SOUND_SCRIPT_REVERB_DEFAULT_LEVEL   = 1,
    SOUND_SCRIPT_REVERB_HIGH_LEVEL      = 2,
    SOUND_SCRIPT_REVERB_EXCLUSIVE_LEVEL = 3
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

/// Admission gates for sound-script requests, indexed by the id's top nibble (0..15).
///
/// Each volatile byte stores 0 disabled or 1 enabled. BSS starts disabled;
/// sound boot enables all types and later policy can replace one or all gates.
/// Starts can bypass a disabled gate through the entry's allow-disabled flag;
/// mute/unmute and mix/volume requests cannot. Stop requests bypass these gates.
/// Starts test the resolved id; other gated producers test the original type.
static volatile u8 _gSndScriptTypeRequestsEnabled[16];

/// Stable sound-script bank slots owning images and borrowing sample descriptors.
///
/// Slot indices are 0..15. Bank types select them through `gSndBankSlotsByType`,
/// with type 4 using successive slots 4..6. Boot reserves image buffers for
/// types 2 and 14; completed script loads transfer their sound-heap images here.
/// MIDI sequence images are held separately.
///
/// Lookup reads the attached descriptor's id. Image release clears `image` and
/// sets the cached `bankId` to -1, retaining the descriptor and SPU origin.
/// Slot addresses remain stable across release and reload; borrowed image and
/// table pointers require their contents to remain loaded until their last use.
static SndBankSlot _gSndBankSlots[16];

/// Eight resident execution slots for sound-script instances, separate from SPU voices.
///
/// Slot indices are 0..7; mix/volume slot APIs mask their input to this range.
/// A slot owns its execution and voice-list state but borrows the loaded bank
/// image, entry controls and command bytes for playback. Audio updates advance
/// all slots; sound boot clears the complete array without releasing resources.
/// Addresses remain stable across starts, stops and bank reloads.
static _SndScript _gSndScriptInstances[8];

/// Resident sound-script voice records indexed by hardware voice minus 16.
///
/// Eight stable records cover voices 16..23 in the script and shared ranges.
/// An allocated record borrows its owner's bank bytes and script list; its SPU
/// callback detaches it on completion or takeover. Sound boot clears the full array.
static _SndVoice _gSndScriptVoices[8];

/// Current sound-script master gain (0 silent, 127 full), shared by script instances.
///
/// Sound boot selects 127. The signed-byte setter clamps a negative request
/// only after applying it to eligible voices, then stores this nonnegative gain.
/// Mute/stop ramps capture it as an unsigned starting or target level.
static s8 _gSndScriptMasterVolume;

/// Saved master gain while sound-script ducking is active (0 inactive, otherwise 48..127).
///
/// The first eligible duck captures the current gain. Exempt entries keep this
/// gain while other entries duck toward 48. The last release starts restoration;
/// the audio ramp clears this byte only after crossing the saved target.
static s8 _gSndScriptUnduckedVolume;

/// Signed master-gain step per audio update (0 idle, -8 duck, +8 restore).
///
/// First eligible acquisition starts ducking toward 48; last release starts
/// restoring the saved gain. Exact endpoint equality leaves the step active
/// for one more update; boot clears it independently of request nesting.
static s8 _gSndScriptDuckStep;

/// Sound-script reverb admission setting for notes keyed on after the change.
///
/// Negative requests store -1 (disabled), zero becomes 1, positive signed bytes
/// remain unchanged. Levels 1/2 admit note thresholds up to that level; level-3
/// notes also pass at level 2, while setting 3 admits only level-3 notes.
static s8 _gSndScriptReverbLevel;

/// Nested sound-script duck requests, independent of whether a ramp is active.
///
/// Acquire increments even when the gain is too low to duck. Release decrements
/// only a positive count and the last release starts restoration if a gain was
/// saved. Area reset and audio initialization clear the count; volatile accesses
/// preserve the request paths' repeated reads and writes.
static volatile s32 _gSndScriptDuckRequests;

/// Stage reverb rows contain two literal area IDs, including retained 255 slots.
enum {
    SOUND_SCRIPT_REVERB_AREAS_PER_STAGE = 2,
    SOUND_SCRIPT_REVERB_UNUSED_AREA     = 0xFF,
};

static u8 _gSndScriptHighReverbAreas[GAME_STAGE_SHELTER_NEO_ARK + 1][SOUND_SCRIPT_REVERB_AREAS_PER_STAGE];

static s16 _gSndScriptVoiceRangeIndices[];

static void _sndBankReleaseAmbientTables(void);

static void _sndBankSetAmbientRetention(s32 retainBank);

static void _sndScriptSetReverb(s8 reverbLevel);

static void _sndEvtRequestScriptKeyOff(void);

static void _sndScriptStepDucking(void);

static s32 _sndScriptTickSlots(s32* unused);

static void _sndScriptScanSlotCandidates(_SndScriptSlotPick* candidates, u16 requestPriority, s32 soundId, u16 requestFlags);

static inline void _sndScriptAdvanceClock(_SndScript* script);

static inline u8 _sndScriptUseReverb(const _SndScriptNote* note);

static s32 _sndScriptExecCommand(_SndScript* script);

static void _sndVoiceTickPitchEnvelope(_SndVoice* voice);

static void _sndScriptInit(void);

static void _sndScriptSetReverbLevel(s8 reverbLevel);

static s8 _sndScriptSelectReplacementSlot(_SndScriptSlotPick* candidates, s32 retriggerTicks);

static void _sndScriptStartSlot(s32 scriptSlotIndex, s8 panOffset, s8 attenuation, s32 soundId, SndBankSlot* bankSlot, SndScriptEntryControls* entryControls);

static void _sndVoiceDetach(void* context);

static SndBankSlot* _sndBankSlotFind(u16 bankId, s32 matchMode);

static _SndVoice* _sndVoiceAlloc(s32 voicePriority);

static void _sndVoiceAttach(_SndScript* script, _SndVoice* voice);

static s32 _sndVoiceTick(_SndVoice* voice);

static s32 _sndScriptReleaseVoices(_SndScript* script);

static void _sndVoiceCalcMixVolumes(s8 panOffset, s8 attenuation, _SndVoice* voice, LinInterp* volumeRamp, SpuVolume* panVolumes);

static void _sndVoiceSetupPitchEnvelope(_SndVoice* voice, s16 pitchEnvelopeOffset, u32 keyedPitch, const SndBankLayer* layer);

static s32 _sndScriptApplyAdsrOverride(const SndBankHdr* image, s16 adsrOffset, SpuVoiceAttr* attr);

static void _sndScriptResetStageSlots(void);

/// Two high-reverb area IDs per stage, indexed directly by stage 0..5.
///
/// A literal match selects level 2, except Mine Gorge which selects level 3
/// before lookup. The 255 slots are retained comparisons, not skipped entries;
/// callers should supply valid stage/area location bytes (area 255 can match).
static u8 _gSndScriptHighReverbAreas[GAME_STAGE_SHELTER_NEO_ARK + 1][SOUND_SCRIPT_REVERB_AREAS_PER_STAGE] = {
    [GAME_STAGE_NONE]            = { SOUND_SCRIPT_REVERB_UNUSED_AREA, SOUND_SCRIPT_REVERB_UNUSED_AREA },
    [GAME_STAGE_ACROPOLIS]       = { SOUND_SCRIPT_REVERB_UNUSED_AREA, SOUND_SCRIPT_REVERB_UNUSED_AREA },
    [GAME_STAGE_DRYFIELD]        = { GAME_AREA_DRYFIELD_WATER_HOLE, GAME_AREA_DRYFIELD_UNDERPASS },
    [GAME_STAGE_DRYFIELD_NIGHT]  = { GAME_AREA_DRYFIELD_NIGHT_WATER_HOLE, GAME_AREA_DRYFIELD_NIGHT_UNDERPASS },
    [GAME_STAGE_MINE_SHELTER]    = { GAME_AREA_SHELTER_B4_WATER_SUPPLY, GAME_AREA_MINE_GORGE },
    [GAME_STAGE_SHELTER_NEO_ARK] = { GAME_AREA_NEO_ARK_SUBMARINE_GALLERY, SOUND_SCRIPT_REVERB_UNUSED_AREA },
};
/// Boot reservations use a valid bank id with low bank number 255, not a free id.
/// Table/image capacities are sound-heap bytes; sample origins are SPU bytes.
enum {
    SOUND_BANK_BOOT_RESERVED_NUMBER       = 255,
    SOUND_BANK_BOOT_PE_TYPE               = (u32)SOUND_BANK_TYPE_PE_ALL >> 28,
    SOUND_BANK_BOOT_WEAPON_TABLE_BYTES    = 206,
    SOUND_BANK_BOOT_WEAPON_IMAGE_BYTES    = 528,
    SOUND_BANK_BOOT_PE_TABLE_BYTES        = 120,
    SOUND_BANK_BOOT_PE_IMAGE_BYTES        = 360,
    SOUND_BANK_BOOT_WEAPON_SAMPLE_ADDRESS = 0x73810,
    SOUND_BANK_BOOT_PE_SAMPLE_ADDRESS     = 0x6F810,
};

/// Weapon and PE script-bank reservations installed once at sound-system boot.
///
/// Each row seeds a type-mapped descriptor/slot and reserves uninitialized
/// table and script-image blocks; it supplies no playable bank contents.
/// Later loads release these blocks and allocate at least the same byte
/// capacities. The SPU origins are stored on the slots without uploading samples.
static _SndBankInitEntry _gSndBankBootReservations[] = {
    {
        .bankType   = SOUND_BANK_TYPE_WEAPON,
        .bankId     = (SOUND_BANK_TYPE_WEAPON << 12) | SOUND_BANK_BOOT_RESERVED_NUMBER,
        .tableBytes = SOUND_BANK_BOOT_WEAPON_TABLE_BYTES,
        .imageBytes = SOUND_BANK_BOOT_WEAPON_IMAGE_BYTES,
        .spuAddr    = SOUND_BANK_BOOT_WEAPON_SAMPLE_ADDRESS,
    },
    {
        .bankType   = SOUND_BANK_BOOT_PE_TYPE,
        .bankId     = ((u32)SOUND_BANK_TYPE_PE_ALL >> 16) | SOUND_BANK_BOOT_RESERVED_NUMBER,
        .tableBytes = SOUND_BANK_BOOT_PE_TABLE_BYTES,
        .imageBytes = SOUND_BANK_BOOT_PE_IMAGE_BYTES,
        .spuAddr    = SOUND_BANK_BOOT_PE_SAMPLE_ADDRESS,
    },
};
STATIC_ASSERT_SIZEOF(_gSndBankBootReservations, 2 * sizeof(_SndBankInitEntry));
volatile s32 gSndBankInitializationBusy = 0;
/// Ordered allocation preferences: script-exclusive voices, then shared voices.
///
/// These signed-halfword values index the SPU range directory, not hardware
/// voices or range records. Their configured runs must stay within voices 16..23
/// so the chosen hardware voice indexes the resident script-voice records.
static s16 _gSndScriptVoiceRangeIndices[] = {
    SPU_VOICE_RANGE_SOUND_SCRIPTS, SPU_VOICE_RANGE_SHARED
};

/// Selects the sound-script reverb setting for a stage and area.
///
/// `stage` must be 0..5; its two area-table entries are compared with `area`.
/// Mine Gorge selects level 3, table matches select level 2, and all other
/// areas select level 1. The caller supplies the unpacked location bytes.
static inline s32 _sndScriptSelectStageReverbLevel(s32 stage, s32 area)
{
    s32 areaIndex;

    if (area == GAME_AREA_MINE_GORGE && stage == GAME_STAGE_MINE_SHELTER) {
        return SOUND_SCRIPT_REVERB_EXCLUSIVE_LEVEL;
    }
    for (areaIndex = 0; areaIndex < SOUND_SCRIPT_REVERB_AREAS_PER_STAGE; areaIndex++) {
        if (_gSndScriptHighReverbAreas[stage][areaIndex] == area) {
            return SOUND_SCRIPT_REVERB_HIGH_LEVEL;
        }
    }
    return SOUND_SCRIPT_REVERB_DEFAULT_LEVEL;
}

void sndScriptResetForArea(s32 stage, s32 area)
{
    enum {
        SOUND_LOAD_CHARACTER_SAMPLE_BASE = 0x3D010,
        SOUND_SCRIPT_LOCATION_BYTE_MASK  = 0xFF,
        SOUND_SCRIPT_RESET_COMMON_ENTRY  = 13,
        SOUND_BANK_SLOT_TYPE_1           = 1,
        SOUND_BANK_SLOT_TYPE_3           = 3,
        SOUND_BANK_SLOT_CHARACTER_FIRST  = 4,
        SOUND_BANK_SLOT_CHARACTER_SECOND = 5,
        SOUND_BANK_SLOT_CHARACTER_THIRD  = 6,
        SOUND_BANK_SLOT_AREA             = 7
    };
    SndBank* bankBase;
    s32      loadMode;

    // Retain voice lists for deferred key-off before releasing transient images.
    _gSndScriptDuckRequests = 0;
    _sndScriptResetStageSlots();
    stage = stage & SOUND_SCRIPT_LOCATION_BYTE_MASK;
    _sndEvtRequestScriptKeyOff();
    sndEvtRequestScriptStop(SOUND_AREA_BANK_ALL, SOUND_SCRIPT_STOP_KEEP_RELEASE);
    sndEvtRequestScriptStop(SOUND_SCRIPT_REQUEST_TYPE_1, SOUND_SCRIPT_STOP_KEEP_RELEASE);
    sndEvtRequestScriptStop(SOUND_COMMON(SOUND_SCRIPT_RESET_COMMON_ENTRY) | SOUND_SCRIPT_STOP_ALL_INSTANCES, SOUND_SCRIPT_STOP_KEEP_RELEASE);
    sndEvtRequestScriptStop(SOUND_BANK_TYPE_WEAPON_ALL, SOUND_SCRIPT_STOP_KEEP_RELEASE);
    sndEvtRequestScriptStop(SOUND_BANK_TYPE_PE_ALL, SOUND_SCRIPT_STOP_KEEP_RELEASE);
    area            = area & SOUND_SCRIPT_LOCATION_BYTE_MASK;
    gSndScriptStage = stage;
    gSndScriptArea  = area;
    sndBankSlotReleaseImage(SOUND_BANK_SLOT_TYPE_1);
    sndBankSlotReleaseImage(SOUND_BANK_SLOT_AREA);

    _sndScriptSetReverb(_sndScriptSelectStageReverbLevel(stage, area));
    gSndLoadCharacterSampleEnd = SOUND_LOAD_CHARACTER_SAMPLE_BASE;
    gSndLoadType1SampleOrigin  = 0;
    gSndLoadAreaSampleOrigin   = gSndLoadType1SampleOrigin;

    // The character-load mode decides whether slot 4 survives the area change.
    loadMode = (s8)gSndLoadCharacterRestartState;
    switch (loadMode) {
        case SOUND_LOAD_CHARACTER_RESTART_NONE:
            sndBankFree(&gSndBankDescriptors[SOUND_BANK_SLOT_CHARACTER_FIRST]);
            sndBankSlotReleaseImage(SOUND_BANK_SLOT_CHARACTER_FIRST);
        case SOUND_LOAD_CHARACTER_RESTART_PENDING:
            gSndLoadCharacterUploadOrdinal = 0;
            break;
        case SOUND_LOAD_CHARACTER_RESTARTED:
            gSndLoadCharacterUploadOrdinal = 1;
            break;
    }
    bankBase = &gSndBankDescriptors[SOUND_BANK_SLOT_TYPE_1];

    // Reset the in-flight loader and discard the transient sample descriptors.
    gSndLoadState.imageBuffer           = NULL;
    gSndLoadState.bank                  = NULL;
    gSndLoadSavedCharacterUploadOrdinal = gSndLoadCharacterUploadOrdinal;
    gSndLoadSavedCharacterRestartState  = gSndLoadCharacterRestartState;
    sndBankFree(bankBase);
    sndBankFree(bankBase + (SOUND_BANK_SLOT_AREA - SOUND_BANK_SLOT_TYPE_1));
    sndBankFree(bankBase + (SOUND_BANK_SLOT_CHARACTER_SECOND - SOUND_BANK_SLOT_TYPE_1));
    sndBankSlotReleaseImage(SOUND_BANK_SLOT_CHARACTER_SECOND);
    sndBankFree(bankBase + (SOUND_BANK_SLOT_CHARACTER_THIRD - SOUND_BANK_SLOT_TYPE_1));
    sndBankSlotReleaseImage(SOUND_BANK_SLOT_CHARACTER_THIRD);
    sndBankFree(bankBase + (SOUND_BANK_SLOT_TYPE_3 - SOUND_BANK_SLOT_TYPE_1));
    sndBankSlotReleaseImage(SOUND_BANK_SLOT_TYPE_3);
    sndScriptSetTypeRequestsEnabled(1, SOUND_BANK_TYPE_CHARACTER_ALL);
}

s32 sndLoadPlaceScriptSamples(s32 waveBytes, s32 bankId)
{
    enum {
        SOUND_LOAD_SCRIPT_SAMPLE_BLOCK_BYTES = 64,
        SOUND_LOAD_COMMON_SAMPLE_BASE        = 0x63810,
        SOUND_LOAD_TYPE_3_SAMPLE_BASE        = 0x47010,
        SOUND_LOAD_CHARACTER_SAMPLE_BASE     = 0x3D010,
        SOUND_LOAD_HIGH_SAMPLE_END           = 0x7B010,
        SOUND_LOAD_PE_SAMPLE_BASE            = 0x6F810,
        SOUND_LOAD_CHARACTER_BANK_LIMIT      = 3,
        SOUND_LOAD_SAMPLE_NO_ADDRESS         = 0
    };
    s32 alignedWaveBytes;
    s32 spuAddr;

    alignedWaveBytes = (waveBytes + SOUND_LOAD_SCRIPT_SAMPLE_BLOCK_BYTES - 1) & ~(SOUND_LOAD_SCRIPT_SAMPLE_BLOCK_BYTES - 1);
    switch ((u32)(bankId & SOUND_BANK_TYPE_MASK) >> 0xC) {
        case SOUND_BANK_TYPE_COMMON:
            spuAddr = SOUND_LOAD_COMMON_SAMPLE_BASE;
            break;
        case SOUND_BANK_TYPE_1 >> 12:
            gSndLoadType1SampleOrigin = SOUND_LOAD_COMMON_SAMPLE_BASE - alignedWaveBytes;
            spuAddr                   = gSndLoadType1SampleOrigin;
            break;
        case 3:
            spuAddr = SOUND_LOAD_TYPE_3_SAMPLE_BASE;
            break;
        case SOUND_BANK_TYPE_CHARACTER:
            // Character banks append after the preceding descriptor's sample pool.
            // A pending mode change instead restarts placement at the first bank.
            if ((s8)gSndLoadCharacterRestartState == SOUND_LOAD_CHARACTER_RESTART_PENDING) {
                gSndLoadCharacterRestartState = SOUND_LOAD_CHARACTER_RESTARTED;
            } else {
                if ((s8)gSndLoadCharacterUploadOrdinal > 0 && (s8)gSndLoadCharacterUploadOrdinal < SOUND_LOAD_CHARACTER_BANK_LIMIT) {
                    spuAddr = gSndBankDescriptors[(s8)gSndLoadCharacterUploadOrdinal + 3].spuAddr +
                              gSndBankDescriptors[(s8)gSndLoadCharacterUploadOrdinal + 3].waveBytes;
                    gSndLoadCharacterUploadOrdinal += 1;
                    gSndLoadCharacterSampleEnd      = alignedWaveBytes + spuAddr;
                    break;
                }
                spuAddr = SOUND_LOAD_SAMPLE_NO_ADDRESS;
                if (gSndLoadCharacterUploadOrdinal != 0) {
                    goto clearPlacementEnd;
                }
            }
            spuAddr                        = SOUND_LOAD_CHARACTER_SAMPLE_BASE;
            gSndLoadCharacterUploadOrdinal = 1;
            gSndLoadCharacterSampleEnd     = alignedWaveBytes + spuAddr;
            break;
        clearPlacementEnd:
            gSndLoadCharacterSampleEnd = SOUND_LOAD_SAMPLE_NO_ADDRESS;
            break;
        case SOUND_BANK_TYPE_AREA: {
            s32 type1Base;

            // Area samples end below the type-1 pool, or below the common pool.
            type1Base = gSndLoadType1SampleOrigin;
            if (type1Base == 0) {
                type1Base = SOUND_LOAD_COMMON_SAMPLE_BASE;
            } else {
                type1Base = gSndLoadType1SampleOrigin;
            }
            gSndLoadAreaSampleOrigin = type1Base - alignedWaveBytes;
            spuAddr                  = gSndLoadAreaSampleOrigin;
            break;
        }
        case (u32)SOUND_STAGE_AMBIENT >> 28:
            spuAddr = SOUND_LOAD_CHARACTER_SAMPLE_BASE - alignedWaveBytes;
            break;
        case SOUND_BANK_TYPE_WEAPON:
        case (u32)SOUND_PLAYER_DEATH >> 28:
            spuAddr = SOUND_LOAD_HIGH_SAMPLE_END - alignedWaveBytes;
            break;
        case (u32)SOUND_BANK_TYPE_PE_ALL >> 28:
            spuAddr = SOUND_LOAD_PE_SAMPLE_BASE;
            break;
        default:
            spuAddr = SOUND_LOAD_SAMPLE_NO_ADDRESS;
            break;
    }
    return spuAddr;
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

/// Releases the ambient sample-bank tables and clears their retention flag.
///
/// The separate script image and uploaded SPU samples remain loaded. Call only
/// after users of the tables have finished; the volatile flag is cleared after
/// release. The flag's consumer role is unproven.
static void _sndBankReleaseAmbientTables(void)
{
    _sndBankSetAmbientRetention(0);
}

/// Keeps the ambient sample-bank tables on a nonzero request, or releases them on zero.
///
/// Zero releases the descriptor's tables before clearing a normalized volatile
/// byte; nonzero only sets the byte. Its consumer role is unproven. The separate
/// script image and uploaded SPU samples are retained. Call only after users
/// of released tables have finished.
static void _sndBankSetAmbientRetention(s32 retainBank)
{
    enum { SOUND_BANK_SLOT_AMBIENT = 12 };

    if (retainBank == 0) {
        sndBankFree(&gSndBankDescriptors[SOUND_BANK_SLOT_AMBIENT]);
        D_80082134 = false;
        return;
    }
    D_80082134 = true;
}

void sndLoadSetFirstCharacterBankRetention(s32 retainFirstBank)
{
    s8 retentionState;

    // Interpret the stored byte as signed, leaving unsupported states untouched.
    retentionState = (s8)gSndLoadCharacterRestartState;
    if (retentionState == SOUND_LOAD_CHARACTER_RESTART_NONE) {
        if (retainFirstBank != 0) {
            gSndLoadCharacterRestartState = SOUND_LOAD_CHARACTER_RESTART_PENDING;
        }
    } else if (retentionState >= 0) {
        if ((retentionState < SOUND_LOAD_CHARACTER_RESTARTED + 1) && (retainFirstBank == 0)) {
            gSndLoadCharacterRestartState = SOUND_LOAD_CHARACTER_RESTART_NONE;
        }
    }
}

void asyncCbPollMainLoop(s32 unused)
{
    asyncCbPoll();
}

void audioTickInitPlayback(void)
{
    enum {
        SOUND_LOAD_CHARACTER_SAMPLE_BASE = 0x3D010,
        SOUND_LOAD_COMMON_SAMPLE_BASE    = 0x63810
    };

    audioTickInsert(midiTick, NULL, AUDIO_TICK_ID_MIDI, NULL);
    audioTickInsert(_sndScriptTickSlots, NULL, AUDIO_TICK_ID_SOUND_SCRIPTS, NULL);

    // Reset upward character allocation and downward type-1/area allocation.
    gSndLoadCharacterSampleEnd          = SOUND_LOAD_CHARACTER_SAMPLE_BASE;
    gSndLoadType1SampleOrigin           = SOUND_LOAD_COMMON_SAMPLE_BASE;
    gSndLoadAreaSampleOrigin            = gSndLoadType1SampleOrigin;
    gSndLoadCharacterUploadOrdinal      = 0;
    gSndLoadSavedCharacterUploadOrdinal = 0;
    gSndLoadCharacterRestartState       = SOUND_LOAD_CHARACTER_RESTART_NONE;
    gSndLoadSavedCharacterRestartState  = SOUND_LOAD_CHARACTER_RESTART_NONE;
    _gSndScriptDuckRequests             = 0;
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

s32 spuTickReverbWarmup(s32* updatesSinceInit)
{
    enum {
        SPU_REVERB_WARMUP_UPDATES = 61,
        SPU_REVERB_WARMUP_DEPTH   = 0x2800,
        AUDIO_TICK_POLL_CONTINUE  = 0,
        AUDIO_TICK_POLL_FINISHED  = -1
    };
    s32 elapsedUpdates;

    elapsedUpdates    = *updatesSinceInit + 1;
    *updatesSinceInit = elapsedUpdates;
    if (elapsedUpdates < SPU_REVERB_WARMUP_UPDATES) {
        return AUDIO_TICK_POLL_CONTINUE;
    }
    spuSetReverbDepth(SPU_REVERB_WARMUP_DEPTH);
    return AUDIO_TICK_POLL_FINISHED;
}

void sndVolumeSetReducedMode(s32 reducedModeEnabled)
{
    enum {
        SOUND_SCRIPT_REDUCED_MASTER_VOLUME = 40,
        MIDI_NORMAL_MASTER_VOLUME          = 64,
        MIDI_REDUCED_MASTER_VOLUME         = 0
    };
    s32 midiMasterLevel;

    // Publish the request policy before applying either master gain.
    if (reducedModeEnabled == 0) {
        gSndVolumeReducedMode = SOUND_VOLUME_MODE_NORMAL;
        sndScriptSetMasterVolume(SOUND_SCRIPT_VOLUME_UNITY);
        midiMasterLevel = MIDI_NORMAL_MASTER_VOLUME;
    } else {
        gSndVolumeReducedMode = SOUND_VOLUME_MODE_REDUCED;
        sndScriptSetMasterVolume(SOUND_SCRIPT_REDUCED_MASTER_VOLUME);
        midiMasterLevel = MIDI_REDUCED_MASTER_VOLUME;
    }
    midiSetMasterVolume(midiMasterLevel);
}

/// Reserves one boot script-bank image and its sample descriptor's table storage.
///
/// Call once per slot at sound boot, with the sound heap initialized and audio
/// updates quiescent. `entry->bankType` must be 0..15 and map to a slot 0..15
/// whose descriptor and image hold no allocations. `tableBytes` and `imageBytes`
/// are heap byte capacities; `spuAddr` is an SPU byte origin. The entry is borrowed
/// only for this call.
///
/// The descriptor owns the table block and the slot owns the image block. Both
/// are uninitialized; the three table pointers provisionally alias the raw
/// block's start. Later loads release these reservations and allocate fresh
/// image and partitioned table storage. Allocation failure is unchecked.
static inline void _sndScriptReserveBootBank(const _SndBankInitEntry* entry)
{
    s8           slotIndex;
    SndBankSlot* bankSlot;
    SndBank*     bank;
    s32          bankId;

    slotIndex                       = gSndBankSlotsByType[entry->bankType];
    bankSlot                        = sndBankSlotGet(slotIndex);
    bankId                          = entry->bankId;
    bank                            = &gSndBankDescriptors[slotIndex];
    bankSlot->bank                  = bank;
    bankSlot->bankId                = bankId;
    bank->bankId                    = entry->bankId;
    bankSlot->bank->heapBlock       = sndHeapAlloc(entry->tableBytes);
    bankSlot->bank->groups          = bankSlot->bank->heapBlock;
    bankSlot->bank->layers          = bankSlot->bank->heapBlock;
    bankSlot->bank->groupFirstLayer = bankSlot->bank->heapBlock;
    bankSlot->image                 = sndHeapAlloc(entry->imageBytes);
    bankSlot->spuAddr               = entry->spuAddr;
}

s32 sndScriptInitSystem(u32 unused)
{
    enum {
        SOUND_BANK_INIT_BUSY               = 0xFF,
        SOUND_BANK_INIT_IDLE               = 0,
        SOUND_SCRIPT_FIRST_EXCLUSIVE_VOICE = 18,
        SOUND_SCRIPT_EXCLUSIVE_VOICE_COUNT = 6
    };
    s32 entryIndex;

    // Hold sector feeding off until boot reservations are published.
    gSndBankInitializationBusy = SOUND_BANK_INIT_BUSY;
    spuSetVoiceRange(SPU_VOICE_RANGE_SOUND_SCRIPTS, SOUND_SCRIPT_FIRST_EXCLUSIVE_VOICE, SOUND_SCRIPT_EXCLUSIVE_VOICE_COUNT);
    _sndScriptInit();
    _sndScriptSetReverb(SOUND_SCRIPT_REVERB_DEFAULT_LEVEL);
    sndScriptSetTypeRequestsEnabled(1, SOUND_BANK_TYPE_ALL_NON_AMBIENT);

    // Reserve buffers for types 2 and 14; a later bank load fills their contents.
    for (entryIndex = 0; entryIndex < (s32)ARRAY_SIZE(_gSndBankBootReservations); entryIndex++) {
        _sndScriptReserveBootBank(&_gSndBankBootReservations[entryIndex]);
    }

    gSndBankInitializationBusy = SOUND_BANK_INIT_IDLE;
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
    if (_gSndScriptTypeRequestsEnabled[(u32)soundId >> 28] == 0) {
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

void sndEvtRequestScriptMute(s32 soundSelector)
{
    enum { SOUND_SCRIPT_REQUEST_TYPE_SHIFT = 28 };
    SndEvt*           event;
    SndEvtScriptArgs* args;

    // Admission uses the requested type; type-1 resolution happens only after allocation.
    if (_gSndScriptTypeRequestsEnabled[(u32)soundSelector >> SOUND_SCRIPT_REQUEST_TYPE_SHIFT] != 0) {
        event = sndEvtAlloc();
        if (event != NULL) {
            event->command = SOUND_EVENT_SCRIPT_MUTE;
            args           = &event->args.script;
            args->soundId  = _sndScriptRemapType1Id(soundSelector);
            sndEvtEnqueue(event);
        }
    }
}

void sndEvtRequestScriptUnmute(s32 soundSelector)
{
    enum { SOUND_SCRIPT_REQUEST_TYPE_SHIFT = 28 };
    SndEvt*           event;
    SndEvtScriptArgs* args;

    // Admission uses the requested type; type-1 resolution follows allocation.
    if (_gSndScriptTypeRequestsEnabled[(u32)soundSelector >> SOUND_SCRIPT_REQUEST_TYPE_SHIFT] != 0) {
        event = sndEvtAlloc();
        if (event != NULL) {
            event->command = SOUND_EVENT_SCRIPT_UNMUTE;
            args           = &event->args.script;
            args->soundId  = _sndScriptRemapType1Id(soundSelector);
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
    if (_gSndScriptTypeRequestsEnabled[(u32)soundId >> SOUND_SCRIPT_REQUEST_TYPE_SHIFT] != 0) {
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

void sndEvtRequestScriptVolume(s32 soundId, s32 volumeScale)
{
    enum { SOUND_SCRIPT_REQUEST_TYPE_SHIFT = 28 };
    SndEvt*           event;
    SndEvtScriptArgs* args;

    if (_gSndScriptTypeRequestsEnabled[(u32)soundId >> SOUND_SCRIPT_REQUEST_TYPE_SHIFT] != 0) {
        event = sndEvtAlloc();
        if (event != NULL) {
            event->command          = SOUND_EVENT_SCRIPT_SET_VOLUME;
            args                    = &event->args.script;
            args->soundId           = _sndScriptRemapType1Id(soundId);
            args->level.volumeScale = volumeScale;
            if ((s8)volumeScale < 0) {
                args->level.volumeScale = SOUND_SCRIPT_VOLUME_UNITY;
            }
            sndEvtEnqueue(event);
        }
    }
}

void sndScriptSetTypeRequestsEnabled(s32 enabled, s32 typeSelector)
{
    enum {
        SOUND_SCRIPT_REQUEST_TYPE_MASK  = 0xF0000000,
        SOUND_SCRIPT_REQUEST_TYPE_SHIFT = 28
    };
    SndEvt*           event;
    SndEvtScriptArgs* args;
    s32               typeIndex;

    if (typeSelector == SOUND_BANK_TYPE_ALL_NON_AMBIENT) {
        for (typeIndex = 0; typeIndex < ARRAY_SIZE(_gSndScriptTypeRequestsEnabled); typeIndex++) {
            _gSndScriptTypeRequestsEnabled[typeIndex] = enabled & 1;
        }
    } else {
        _gSndScriptTypeRequestsEnabled[(u32)(typeSelector & SOUND_SCRIPT_REQUEST_TYPE_MASK) >> SOUND_SCRIPT_REQUEST_TYPE_SHIFT] = enabled & 1;
        // The raw zero request also stops character scripts, keeping their release.
        if (enabled == 0 && (typeSelector & SOUND_SCRIPT_REQUEST_TYPE_MASK) == SOUND_BANK_TYPE_CHARACTER_ALL) {
            event = sndEvtAlloc();
            if (event != NULL) {
                event->command    = SOUND_EVENT_SCRIPT_STOP;
                args              = &event->args.script;
                args->soundId     = _sndScriptRemapType1Id(SOUND_BANK_TYPE_CHARACTER_ALL);
                args->stopControl = SOUND_SCRIPT_STOP_KEEP_RELEASE;
                sndEvtEnqueue(event);
            }
        }
    }
}

/// Applies a signed-byte sound-script reverb setting through its normalization policy.
///
/// Negative levels disable reverb, zero selects level 1, and positive levels
/// are retained. This changes note admission at later key-ons, not active sends.
static void _sndScriptSetReverb(s8 reverbLevel)
{
    _sndScriptSetReverbLevel(reverbLevel);
}

s32 sndScriptHasActiveId(s32 soundId)
{
    return ~sndScriptFindInstanceById(_sndScriptRemapType1Id(soundId)) != 0;
}

void sndEvtRequestScriptDuckAcquire(void)
{
    SndEvt* event;

    event = sndEvtAlloc();
    if (event != NULL) {
        event->command = SOUND_EVENT_SCRIPT_DUCK_ACQUIRE;
        sndEvtEnqueue(event);
    }
}

void sndEvtRequestScriptDuckRelease(void)
{
    SndEvt* event;

    event = sndEvtAlloc();
    if (event != NULL) {
        event->command = SOUND_EVENT_SCRIPT_DUCK_RELEASE;
        sndEvtEnqueue(event);
    }
}

/// Queues type-1 and area-script key-off for the next sound-event dispatch.
///
/// A full event pool drops the request. Dispatch adjusts release and keys off
/// their attached voices; script-state clearing before dispatch is separate.
static void _sndEvtRequestScriptKeyOff(void)
{
    SndEvt* event;

    event = sndEvtAlloc();
    if (event != NULL) {
        event->command = SOUND_EVENT_SCRIPT_KEY_OFF;
        sndEvtEnqueue(event);
    }
}

s32 sndScriptStopMatching(s32 soundSelector, s32 stopControl)
{
    enum {
        SOUND_SCRIPT_REQUEST_ENTRY_MASK = 0xFF,
        SOUND_SCRIPT_REQUEST_TYPE_MASK  = 0xF0000000,
        SOUND_SCRIPT_STOP_TYPE_RESULT   = -2
    };
    _SndScript* script;
    s32         slotIndex;
    s32         bankType;
    s32         lastSlotIndex;

    // Type-only stops bypass the entry fade and replace the release policy.
    if (!(soundSelector & SOUND_SCRIPT_REQUEST_ENTRY_MASK)) {
        for (slotIndex = 0; slotIndex < ARRAY_SIZE(_gSndScriptInstances); slotIndex++) {
            script   = &_gSndScriptInstances[slotIndex];
            bankType = script->soundId & SOUND_SCRIPT_REQUEST_TYPE_MASK;
            if ((bankType == soundSelector) || ((soundSelector == SOUND_BANK_TYPE_ALL_NON_AMBIENT) && (bankType != (SOUND_STAGE_AMBIENT & SOUND_SCRIPT_REQUEST_TYPE_MASK)))) {
                if ((script->state != SOUND_SCRIPT_STOPPING) && (script->state != SOUND_SCRIPT_IDLE)) {
                    script->keepRelease = (stopControl == SOUND_SCRIPT_KEEP_RELEASE);
                    script->state       = SOUND_SCRIPT_STOPPING;
                }
            }
        }
        return SOUND_SCRIPT_STOP_TYPE_RESULT;
    }

    lastSlotIndex = 0;
    for (slotIndex = 0; slotIndex < ARRAY_SIZE(_gSndScriptInstances); slotIndex++) {
        script = &_gSndScriptInstances[slotIndex];
        if ((script->soundId == soundSelector) || ((script->soundId | SOUND_SCRIPT_STOP_ALL_INSTANCES) == soundSelector)) {
            switch (script->state) {
                case SOUND_SCRIPT_RUNNING:
                    if (stopControl != SOUND_SCRIPT_STOP_NO_FADE) {
                        if (stopControl != SOUND_SCRIPT_KEEP_RELEASE) {
                            linInterpSetup(&script->volumeRamp, (u8)_gSndScriptMasterVolume, 0, stopControl);
                            script->state = SOUND_SCRIPT_FADING_OUT;
                            break;
                        }
                        script->keepRelease = stopControl;
                    }
                    /* fallthrough */
                case SOUND_SCRIPT_STOPPING:
                case SOUND_SCRIPT_MUTING:
                case SOUND_SCRIPT_UNMUTING:
                    script->state = SOUND_SCRIPT_STOPPING;
                    break;
                case SOUND_SCRIPT_STARTING:
                    script->state = SOUND_SCRIPT_IDLE;
                    break;
            }
        }
        lastSlotIndex = slotIndex;
    }
    return lastSlotIndex;
}

/// Advances the sound-script master duck or restore ramp by one audio update.
///
/// The signed step moves toward 48 when ducking and toward the saved master
/// gain when restoring. Strict endpoint comparisons keep the step active for
/// one more update at exact equality. Restoration clears the saved level only
/// after crossing a nonzero target; mixing then uses the restored master gain.
static void _sndScriptStepDucking(void)
{
    s16 masterVolume;
    s8  unduckedVolume;

    masterVolume = sndScriptGetMasterVolume();
    if (_gSndScriptDuckStep > 0) {
        masterVolume   = masterVolume + _gSndScriptDuckStep;
        unduckedVolume = (u8)_gSndScriptUnduckedVolume;
        if (unduckedVolume < masterVolume) {
            if (unduckedVolume != 0) {
                masterVolume              = unduckedVolume;
                _gSndScriptUnduckedVolume = 0;
            }
            _gSndScriptDuckStep = 0;
        }
    } else if (_gSndScriptDuckStep < 0) {
        masterVolume = masterVolume + _gSndScriptDuckStep;
        if (masterVolume < SOUND_SCRIPT_DUCK_LEVEL) {
            masterVolume        = SOUND_SCRIPT_DUCK_LEVEL;
            _gSndScriptDuckStep = 0;
        }
    }
    sndScriptSetMasterVolume(masterVolume);
}

/// Runs every sound-script instance and its attached voices for one audio update.
///
/// This permanent `AudioTickPoll` ignores its argument and returns 0. Starting
/// slots reset their clocks and mix state before executing commands; runnable
/// slots execute until a command waits, ends or declines to advance. Muting
/// pauses commands while voice gates and ramps continue; unmuting resumes them
/// on reaching its target. A completed fade enters the stop path next update.
///
/// Stops key voices off and retain the slot while pitch envelopes remain on
/// its list. Other ADSR releases can outlive the slot: clearing it removes the
/// script association, while the SPU callback later releases the voice record.
/// All borrowed images, controls and voice-list links must remain valid during
/// the update. Pan and attenuation each step once per visited voice.
static s32 _sndScriptTickSlots(s32* unused)
{
    SpuVoiceRef   ref;
    SpuVolume     panVolumes;
    SpuVoiceAttr* attr;
    _SndScript*   script;
    _SndVoice*    voice;
    _SndVoice*    orphanVoice;
    s32           slotIndex;
    s32           remainingVoices;
    s32           panQuarterOffset;
    s32           roundedPanQuarterOffset;
    s8            mixStep;
    s16           nextAttenuation;

    if (_gSndScriptDuckStep != 0) {
        _sndScriptStepDucking();
    }

    for (slotIndex = 0; slotIndex < ARRAY_SIZE(_gSndScriptInstances); slotIndex++) {
        script = &_gSndScriptInstances[slotIndex];
        switch (script->state) {
            case SOUND_SCRIPT_IDLE:
                break;

            case SOUND_SCRIPT_STARTING:
                script->keepRelease        = 0;
                script->ended              = 0;
                script->tickClock          = 0;
                script->runningTicks       = 0;
                script->voices             = NULL;
                script->state              = SOUND_SCRIPT_RUNNING;
                script->volumeRamp.enabled = LINEAR_INTERPOLATOR_BYPASS;
                script->panStep            = 0;
                script->attenuationStep    = 0;
                script->mixDirty           = 0;
                goto run;

            case SOUND_SCRIPT_FADING_OUT:
                if (script->volumeRamp.gain == script->volumeRamp.targetGain) {
                    script->state = SOUND_SCRIPT_STOPPING;
                    goto stop;
                }
                linInterpStep(&script->volumeRamp);
                script->mixDirty = 1;
                /* fallthrough */
            case SOUND_SCRIPT_RUNNING:
            run:
                script->runningTicks++;
                while (_sndScriptExecCommand(script) != 0) {
                }
            update:
                remainingVoices = 0;
                if (script->voices != NULL) {
                    voice = script->voices;
                    do {
                        _sndVoiceTick(voice);
                        // Each visited voice advances the instance's shared mix ramps.
                        mixStep = script->panStep;
                        remainingVoices++;
                        if (mixStep != 0) {
                            panQuarterOffset = mixStep + (s8)script->panOffset * SOUND_SCRIPT_PAN_FRACTION_SCALE;
                            if (mixStep > 0) {
                                if ((s8)script->panTarget * SOUND_SCRIPT_PAN_FRACTION_SCALE < panQuarterOffset) {
                                    script->panOffset = script->panTarget;
                                    script->panStep   = 0;
                                } else {
                                    /* panQuarterOffset / 4, rounded toward zero */
                                    roundedPanQuarterOffset = panQuarterOffset;
                                    if (roundedPanQuarterOffset < 0) {
                                        roundedPanQuarterOffset += 3;
                                    }
                                    script->panOffset = roundedPanQuarterOffset >> 2;
                                }
                            } else if (panQuarterOffset < (s8)script->panTarget * SOUND_SCRIPT_PAN_FRACTION_SCALE) {
                                script->panOffset = script->panTarget;
                                script->panStep   = 0;
                            } else {
                                roundedPanQuarterOffset = panQuarterOffset;
                                if (roundedPanQuarterOffset < 0) {
                                    roundedPanQuarterOffset += 3;
                                }
                                script->panOffset = roundedPanQuarterOffset >> 2;
                            }
                            script->mixDirty = 1;
                        }
                        mixStep = script->attenuationStep;
                        if (mixStep != 0) {
                            nextAttenuation = (s8)script->attenuation + mixStep;
                            if (mixStep > 0) {
                                if ((s8)script->attenuationTarget < nextAttenuation) {
                                    script->attenuation     = script->attenuationTarget;
                                    script->attenuationStep = 0;
                                } else {
                                    script->attenuation = nextAttenuation;
                                }
                            } else if (nextAttenuation < (s8)script->attenuationTarget) {
                                script->attenuation     = script->attenuationTarget;
                                script->attenuationStep = 0;
                            } else {
                                script->attenuation = nextAttenuation;
                            }
                            script->mixDirty = 1;
                        }
                        if (script->mixDirty == 1) {
                            spuGetVoiceRef(voice->spuVoice, &ref);
                            attr = ref.attr;
                            _sndVoiceCalcMixVolumes(script->panOffset, script->attenuation, voice, &script->volumeRamp, &panVolumes);
                            attr->volume.left   = panVolumes.left;
                            attr->volume.right  = panVolumes.right;
                            attr->volmode.left  = SPU_VOICE_DIRECT;
                            attr->volmode.right = SPU_VOICE_DIRECT;
                            attr->mask         |= SPU_VOICE_VOLL | SPU_VOICE_VOLR | SPU_VOICE_VOLMODEL | SPU_VOICE_VOLMODER;
                        }
                        voice = voice->next;
                    } while (voice != NULL);
                    script->mixDirty = 0;
                }
                if (remainingVoices == 0 && script->ended == 1) {
                    goto release;
                }
                break;

            case SOUND_SCRIPT_MUTING:
                linInterpStep(&script->volumeRamp);
                script->mixDirty = 1;
                goto update;

            case SOUND_SCRIPT_UNMUTING:
                script->mixDirty = 1;
                linInterpStep(&script->volumeRamp);
                if (script->volumeRamp.gain == script->volumeRamp.targetGain) {
                    script->state = SOUND_SCRIPT_RUNNING;
                    goto run;
                }
                goto update;

            case SOUND_SCRIPT_STOPPING:
            stop:
                script->ended = 1;
                if (_sndScriptReleaseVoices(script) != 0) {
                    script->state = SOUND_SCRIPT_RELEASING;
                    break;
                }
                goto release;

            case SOUND_SCRIPT_RELEASING:
                // Only pitch-envelope voices keep a stopped instance alive.
                remainingVoices = 0;
                if (script->voices != NULL) {
                    voice = script->voices;
                    do {
                        if (voice->envelope.active != 0) {
                            remainingVoices++;
                            _sndVoiceTickPitchEnvelope(voice);
                        }
                        voice = voice->next;
                    } while (voice != NULL);
                }
                if (remainingVoices == 0 && script->ended == 1) {
                release:
                    script->ended   = 0;
                    script->soundId = -1;
                    script->state   = SOUND_SCRIPT_IDLE;
                    for (orphanVoice = script->voices; orphanVoice != NULL; orphanVoice = orphanVoice->next) {
                        orphanVoice->script = 0;
                    }
                    script->voices             = NULL;
                    script->volumeRamp.enabled = LINEAR_INTERPOLATOR_BYPASS;
                }
                break;
        }
    }
    return 0;
}

/// Clears a script-slot survey for a request with the given entry priority.
///
/// `candidates` must hold one writable `_SndScriptSlotPick`; no instance is
/// changed. Slot and oldest-age sentinels are -1, group count starts at zero,
/// and the lowest priority starts at `requestPriority`. No-member newest age
/// is 65535, above every nonnegative signed-halfword retrigger limit.
static inline void _sndScriptInitSlotPick(_SndScriptSlotPick* candidates, u16 requestPriority)
{
    enum {
        SOUND_SCRIPT_RETRIGGER_AGE_NO_MEMBER = 0xFFFF
    };

    candidates->slot               = SOUND_SCRIPT_SLOT_NONE;
    candidates->equalPriorityTicks = SOUND_SCRIPT_SLOT_NONE;
    candidates->equalPrioritySlot  = SOUND_SCRIPT_SLOT_NONE;
    candidates->oldestGroupTicks   = SOUND_SCRIPT_SLOT_NONE;
    candidates->oldestGroupSlot    = SOUND_SCRIPT_SLOT_NONE;
    candidates->lowerPrioritySlot  = SOUND_SCRIPT_SLOT_NONE;
    candidates->idleSlot           = SOUND_SCRIPT_SLOT_NONE;
    candidates->lastGroupSlot      = SOUND_SCRIPT_SLOT_NONE;
    candidates->newestGroupSlot    = SOUND_SCRIPT_SLOT_NONE;
    candidates->lowestPriority     = requestPriority;
    candidates->newestGroupTicks   = SOUND_SCRIPT_RETRIGGER_AGE_NO_MEMBER;
    candidates->groupCount         = 0;
}

/// Surveys all script slots for one resolved start request's replacement policy.
///
/// Returns candidates through caller-owned storage, without changing instances.
/// Idle slots qualify as free; stopping slots are excluded. Every other slot
/// requires live entry controls. The group is the same id ignoring its instance
/// byte, or equal full flags when the existing entry requests flag grouping.
/// Ages are running audio updates; equal ages retain the earlier slot, while
/// the last idle slot wins. No candidate is represented by -1.
static void _sndScriptScanSlotCandidates(_SndScriptSlotPick* candidates, u16 requestPriority, s32 soundId, u16 requestFlags)
{
    enum {
        SOUND_SCRIPT_SLOT_NONE     = -1,
        SOUND_SCRIPT_GROUP_ID_MASK = 0xFFFF00FF
    };
    s8          slotIndex;
    _SndScript* script;
    u16         entryPriority;
    u16         groupFlags;
    s32         runningTicks;

    _sndScriptInitSlotPick(candidates, requestPriority);

    for (slotIndex = 0; slotIndex < ARRAY_SIZE(_gSndScriptInstances); slotIndex++) {
        script = &_gSndScriptInstances[slotIndex];
        if (script->state == SOUND_SCRIPT_IDLE) {
            candidates->idleSlot = slotIndex;
        } else if (script->state != SOUND_SCRIPT_STOPPING) {
            entryPriority = script->entryControls->priority;
            if (entryPriority < (u32)candidates->lowestPriority) {
                candidates->lowestPriority    = entryPriority;
                candidates->lowerPrioritySlot = slotIndex;
            } else if (candidates->lowestPriority == entryPriority) {
                if ((candidates->equalPrioritySlot == SOUND_SCRIPT_SLOT_NONE) || (candidates->equalPriorityTicks < script->runningTicks)) {
                    runningTicks                   = script->runningTicks;
                    candidates->equalPrioritySlot  = slotIndex;
                    candidates->equalPriorityTicks = runningTicks;
                }
            }
            // Grouping ignores only the instance byte, unless the existing flags opt in.
            if (((script->soundId & SOUND_SCRIPT_GROUP_ID_MASK) == (soundId & SOUND_SCRIPT_GROUP_ID_MASK)) ||
                (((groupFlags = script->entryControls->flags) & SOUND_SCRIPT_GROUP_BY_FLAGS) && (requestFlags == groupFlags))) {
                candidates->lastGroupSlot = slotIndex;
                if ((candidates->newestGroupSlot == SOUND_SCRIPT_SLOT_NONE) || (candidates->newestGroupTicks > script->runningTicks)) {
                    runningTicks                 = script->runningTicks;
                    candidates->newestGroupSlot  = slotIndex;
                    candidates->newestGroupTicks = runningTicks;
                }
                candidates->groupCount += 1;
                if ((candidates->oldestGroupSlot == SOUND_SCRIPT_SLOT_NONE) || (candidates->oldestGroupTicks < script->runningTicks)) {
                    runningTicks                 = script->runningTicks;
                    candidates->oldestGroupSlot  = slotIndex;
                    candidates->oldestGroupTicks = runningTicks;
                }
            }
        }
    }
}

void sndScriptKeyOffType1AndArea(void)
{
    enum {
        SOUND_SCRIPT_REQUEST_TYPE_MASK            = 0xF0000000,
        SOUND_SCRIPT_ID_IDLE                      = -1,
        SOUND_SCRIPT_ADSR_RELEASE_RATE_MASK       = 0x1F,
        SOUND_SCRIPT_ADSR_TRANSITION_RELEASE_RATE = 11,
        SOUND_SCRIPT_ADSR_EXPONENTIAL_RELEASE     = 0x20
    };
    SpuVoiceRef voiceRef;
    s32         slotIndex;
    _SndScript* script;
    _SndVoice*  voiceHead;
    _SndVoice*  voice;
    s32         bankType;
    s32         emptyBankType;

    /// Queues rate-11 exponential release using this function's ADSR constants.
    ///
    /// `ref` must be a side-effect-free SpuVoiceRef lvalue whose attributes are
    /// live in the pending SPU batch. Evaluates it repeatedly; preserves every
    /// ADSR bit outside the release rate and exponential-mode bit. Expands to
    /// a compound statement, used only as a standalone statement in this walk.
#define SOUND_SCRIPT_SET_TRANSITION_RELEASE(ref)                                                                                     \
    {                                                                                                                                \
        (ref).attr->adsr2  = ((ref).attr->adsr2 & ~SOUND_SCRIPT_ADSR_RELEASE_RATE_MASK) | SOUND_SCRIPT_ADSR_TRANSITION_RELEASE_RATE; \
        (ref).attr->adsr2 |= SOUND_SCRIPT_ADSR_EXPONENTIAL_RELEASE;                                                                  \
        (ref).attr->mask  |= SPU_VOICE_ADSR_ADSR2;                                                                                   \
    }

    for (slotIndex = 0; slotIndex < (s32)ARRAY_SIZE(_gSndScriptInstances); slotIndex++) {
        script    = &_gSndScriptInstances[slotIndex];
        voiceHead = script->voices;
        if (voiceHead != NULL) {
            bankType = script->soundId & SOUND_SCRIPT_REQUEST_TYPE_MASK;
            if (bankType != (SOUND_STAGE_AMBIENT & SOUND_SCRIPT_REQUEST_TYPE_MASK)) {
                voice = voiceHead;
                if ((bankType == SOUND_SCRIPT_REQUEST_TYPE_1) || (bankType == SOUND_AREA_BANK_ALL)) {
                    do {
                        // Retain the head-only ADSR update while keying off every node.
                        spuGetVoiceRef(script->voices->spuVoice, &voiceRef);
                        SOUND_SCRIPT_SET_TRANSITION_RELEASE(voiceRef);
                        spuKeyOff(voice->spuVoice);
                        voice = voice->next;
                    } while (voice != NULL);
                    script->state   = SOUND_SCRIPT_IDLE;
                    script->soundId = SOUND_SCRIPT_ID_IDLE;
                }
            }
        } else {
            emptyBankType = script->soundId & SOUND_SCRIPT_REQUEST_TYPE_MASK;
            if ((emptyBankType == SOUND_AREA_BANK_ALL) || (emptyBankType == SOUND_SCRIPT_REQUEST_TYPE_1)) {
                script->state   = SOUND_SCRIPT_IDLE;
                script->soundId = SOUND_SCRIPT_ID_IDLE;
            }
        }
    }
#undef SOUND_SCRIPT_SET_TRANSITION_RELEASE
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

/// Returns whether a note's send is admitted by the sound-script reverb setting.
///
/// Level-3 notes are admitted at settings 2 and above. Setting 3 excludes every
/// other note; other settings admit nonnegative note levels up to that setting.
/// The note is borrowed for this call, and the result is exactly 0 or 1.
static inline u8 _sndScriptUseReverb(const _SndScriptNote* note)
{
    s32 reverbEnabled;

    if (note->reverbLevel == SOUND_SCRIPT_REVERB_EXCLUSIVE_LEVEL && _gSndScriptReverbLevel >= SOUND_SCRIPT_REVERB_HIGH_LEVEL) {
        reverbEnabled = 1;
    } else if (note->reverbLevel != SOUND_SCRIPT_REVERB_EXCLUSIVE_LEVEL && _gSndScriptReverbLevel == SOUND_SCRIPT_REVERB_EXCLUSIVE_LEVEL) {
        reverbEnabled = 0;
    } else {
        reverbEnabled = 0;
        if (note->reverbLevel >= 0) {
            reverbEnabled = _gSndScriptReverbLevel >= note->reverbLevel;
        }
    }
    return reverbEnabled;
}

/// Executes the command at one live sound-script instance's byte cursor.
///
/// Returns 1 when the driver should immediately dispatch again and 0 when it
/// should tick voices instead. Delays consume 16.16 script ticks; a waiting
/// command adds one tick per update, or 39321/65536 on PAL. Nested loops use
/// the slot's eight-entry stack; excess depth or an unmatched endL ends it.
///
/// The bank image must contain aligned, complete commands and every addressed
/// ADSR/envelope chunk. Offsets are bytes from that image's start, and no image
/// bounds are checked here. oneC reloads controls using the sound id's low byte
/// and falls through to read the next 24 bytes as a note without a tag check.
/// A missing note bank retries the same command after releasing its allocation;
/// failure to allocate a voice consumes the note's delay and skips that note.
static s32 _sndScriptExecCommand(_SndScript* script)
{
    enum {
        SOUND_BANK_PAN_CENTER            = 64,
        SOUND_BANK_PAN_MAX               = 127,
        SOUND_BANK_KEY_FRACTION_BITS     = 7,
        SOUND_BANK_KEY_FRACTION_MASK     = (1 << SOUND_BANK_KEY_FRACTION_BITS) - 1,
        SOUND_SCRIPT_CLOCK_FRACTION_BITS = 16
    };
    SpuVoiceRef     voiceRef;
    SpuVolume       panVolumes;
    _SndScriptCmd*  command;
    _SndScriptNote* note;
    _SndVoice*      voice;
    SndBankLayer*   bankLayer;
    SpuVoiceAttr*   attr;
    SndBankSlot*    bankSlot;
    SndBank*        bank;
    SndBankHdr*     header;
    s32             result;
    s32             tickClock;
    s32             waitTicks;
    s32             loopIndex;
    s32             masterVolume;
    u8              layerVolume;
    s32             biasedPan;
    s16             entryPanBias;
    s16             narrowedPan;
    s32             keyedPitch;
    u16             keyedPitch16;
    s32             gateClock;
    s16             pitchEnvelopeOffset;

    command = (_SndScriptCmd*)script->cursor;
    switch ((u32)command->magic) {
        case SOUND_SCRIPT_PITCH_ENVELOPE_TAG:
            // Envelope data is not a command; halt without advancing the cursor.
            result = 0;
            break;
        case SOUND_SCRIPT_END_TAG:
            script->ended = 1;
            result        = 0;
            break;
        case SOUND_SCRIPT_LOOP_TAG:
            if (script->loopDepth >= ARRAY_SIZE(script->loopCounts)) {
                script->ended = 1;
                result        = 0;
                break;
            }
            tickClock = script->tickClock;
            if ((tickClock >> SOUND_SCRIPT_CLOCK_FRACTION_BITS) >= command->data.loop.delayTicks) {
                script->loopCounts[script->loopDepth]  = command->data.loop.repeatCount;
                script->cursor                         = script->cursor + sizeof(_SndScriptCmd);
                script->loopCursors[script->loopDepth] = script->cursor;
                script->loopDepth++;
                script->tickClock -= command->data.loop.delayTicks << SOUND_SCRIPT_CLOCK_FRACTION_BITS;
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
            loopIndex = script->loopDepth - 1;
            if (script->loopCounts[loopIndex] == 1) {
                script->cursor = script->cursor + sizeof(command->magic);
                script->loopDepth--;
            } else {
                script->cursor = script->loopCursors[loopIndex];
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
            note      = (_SndScriptNote*)script->cursor;
            tickClock = script->tickClock;
            if ((tickClock >> SOUND_SCRIPT_CLOCK_FRACTION_BITS) < note->delayTicks) {
                _sndScriptAdvanceClock(script);
                result = 0;
                break;
            }
            voice  = _sndVoiceAlloc(note->voicePriority);
            result = 1;
            if (voice != NULL) {
                bankSlot = script->bankSlot;
                if (note->bankId != 0) {
                    bank = sndBankFind(note->bankId);
                    if (bank == 0) {
                        // Not loaded yet: release the voice and retry this command.
                        voice->allocated = 0;
                        spuReleaseVoiceSlot(voice->spuVoice);
                        spuClearVoiceCallback(voice->spuVoice);
                        voice->spuVoice = 0;
                        return 0;
                    }
                } else {
                    bank = bankSlot->bank;
                }
                spuGetVoiceRef(voice->spuVoice, &voiceRef);
                bankLayer    = sndBankGetLayer(bank, note->program, note->layer);
                attr         = voiceRef.attr;
                masterVolume = _gSndScriptMasterVolume;
                attr->addr   = bankLayer->waveAddr;
                if ((_gSndScriptUnduckedVolume != 0) && (script->entryControls->flags & SOUND_SCRIPT_USE_UNDUCKED_VOLUME)) {
                    masterVolume = _gSndScriptUnduckedVolume;
                }
                // Choose the layer value or override before applying the entry pan bias.
                layerVolume = (u8)note->volumeOverride;
                if (note->volumeOverride < 0) {
                    layerVolume = bankLayer->volume;
                }
                voice->baseVolume   = layerVolume;
                voice->scaledVolume = (s8)((masterVolume * script->entryControls->volumeScale * voice->baseVolume) / (SOUND_SCRIPT_VOLUME_UNITY * SOUND_SCRIPT_VOLUME_UNITY));
                entryPanBias        = script->entryControls->panBias;
                biasedPan           = note->panOverride;
                if (biasedPan < 0) {
                    biasedPan = bankLayer->pan;
                }
                biasedPan  += (s16)(entryPanBias - SOUND_BANK_PAN_CENTER);
                narrowedPan = biasedPan;
                if (narrowedPan <= SOUND_BANK_PAN_MAX) {
                    if (narrowedPan >= 0) {
                        voice->basePan = biasedPan;
                    } else {
                        voice->basePan = 0;
                    }
                } else {
                    voice->basePan = SOUND_BANK_PAN_MAX;
                }
                if (_sndScriptApplyAdsrOverride(script->bankSlot->image, note->adsrOffset, attr) == SOUND_SCRIPT_ADSR_NOT_APPLIED) {
                    attr->adsr1 = bankLayer->adsr1;
                    attr->adsr2 = bankLayer->adsr2;
                }
                // Narrow the keyed pitch to Q7 before splitting its key and fraction.
                keyedPitch = keyedPitch16 = note->pitchOffset + (bankLayer->keyMin << SOUND_BANK_KEY_FRACTION_BITS);
                attr->pitch               = spuCalcPitch((u32)(keyedPitch16 & 0xFFFF) >> SOUND_BANK_KEY_FRACTION_BITS, (keyedPitch & SOUND_BANK_KEY_FRACTION_MASK) * 2, bankLayer->rootKey, bankLayer->fineTune);
                if (_sndScriptUseReverb(note) == 0) {
                    spuDisableVoiceReverb(voice->spuVoice);
                    voice->field_1 = 1;
                } else {
                    spuEnableVoiceReverb(voice->spuVoice);
                    voice->field_1 = 1;
                }
                _sndVoiceCalcMixVolumes(script->panOffset, script->attenuation, voice, &script->volumeRamp, &panVolumes);
                attr->volume.left   = panVolumes.left;
                attr->volume.right  = panVolumes.right;
                attr->volmode.left  = SPU_VOICE_DIRECT;
                attr->volmode.right = SPU_VOICE_DIRECT;
                attr->mask          = SPU_VOICE_VOLL | SPU_VOICE_VOLR | SPU_VOICE_VOLMODEL | SPU_VOICE_VOLMODER | SPU_VOICE_PITCH | SPU_VOICE_WDSA | SPU_VOICE_ADSR_ADSR1 | SPU_VOICE_ADSR_ADSR2;
                spuKeyOn(voice->spuVoice);
                voice->note      = note;
                gateClock        = note->gateTicks == 0 ? SOUND_SCRIPT_NOTE_HELD : note->gateTicks << SOUND_SCRIPT_CLOCK_FRACTION_BITS;
                voice->gateClock = gateClock;
                _sndVoiceAttach(script, voice);
                pitchEnvelopeOffset = note->pitchEnvelopeOffset;
                if (pitchEnvelopeOffset != SOUND_SCRIPT_NOTE_NO_ENVELOPE) {
                    _sndVoiceSetupPitchEnvelope(voice, pitchEnvelopeOffset, keyedPitch16 & 0xFFFF, bankLayer);
                    result = 1;
                } else {
                    voice->envelope.active = 0;
                    result                 = 1;
                }
            }
            script->tickClock = (s32)(script->tickClock - (note->delayTicks << SOUND_SCRIPT_CLOCK_FRACTION_BITS));
            script->cursor    = script->cursor + sizeof(_SndScriptNote);
            break;
        case SOUND_SCRIPT_WAIT_TAG:
            tickClock = script->tickClock;
            waitTicks = command->data.waitTicks;
            if ((tickClock >> SOUND_SCRIPT_CLOCK_FRACTION_BITS) < waitTicks) {
                _sndScriptAdvanceClock(script);
                result = 0;
                break;
            }
            script->tickClock = tickClock - (waitTicks << SOUND_SCRIPT_CLOCK_FRACTION_BITS);
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

/// Plays one audio update of a scripted voice's active pitch envelope.
///
/// The caller must supply an active player with a live oneE chunk and hardware
/// voice. Keyed pitch is Q7; stage offsets and the resulting pitch are Q8
/// semitones. Pending release recaptures its step from the last ramp offset,
/// even if a prior release already began. Exhausted stages fall through within
/// this update. Delay leaves hardware pitch untouched; all sounding stages
/// queue pitch using the cached root key and Q7 fine tune.
static void _sndVoiceTickPitchEnvelope(_SndVoice* voice)
{
    enum {
        SOUND_VOICE_PITCH_FRACTION_BITS = 8,
        SOUND_VOICE_PITCH_FRACTION_MASK = 0xFF
    };
    SpuVoiceRef        voiceRef;
    _SndVoiceEnvelope* player;
    _SndPitchEnvelope* envelope;
    s32                pitchQ8;
    s32                releaseDirection;
    s32                decayStartOffset;
    SpuVoiceAttr*      attr;

    player   = &voice->envelope;
    envelope = player->envelope;

    // A pending release replaces the current stage and starts from the last
    // ramp offset, not the decayStartOffset being played. An exhausted stage falls
    // through and plays the next stage on this update.
    if (player->releaseRequest == SOUND_VOICE_ENVELOPE_RELEASE_PENDING) {
        player->stage    = SOUND_VOICE_ENVELOPE_RELEASE;
        releaseDirection = (player->rampOffset - envelope->releaseLevel) * envelope->releaseSlope;
        if (releaseDirection > 0) {
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
            pitchQ8 = (player->keyedPitch << 1) + player->attackOffset;
            if (player->stageUpdates < envelope->attackUpdates) {
                player->stageUpdates++;
                player->rampOffset = player->attackOffset += envelope->attackSlope;
                break;
            }
            player->stage        = SOUND_VOICE_ENVELOPE_HOLD;
            player->stageUpdates = 0;
        case SOUND_VOICE_ENVELOPE_HOLD:
            pitchQ8 = (player->keyedPitch << 1) + envelope->attackLevel;
            if (player->stageUpdates < envelope->holdUpdates) {
                player->stageUpdates++;
                break;
            }
            player->stage        = SOUND_VOICE_ENVELOPE_DECAY;
            player->decayOffset  = envelope->attackLevel;
            decayStartOffset     = envelope->attackLevel;
            player->stageUpdates = 0;
            player->rampOffset   = decayStartOffset;
        case SOUND_VOICE_ENVELOPE_DECAY:
            pitchQ8 = (player->keyedPitch << 1) + player->decayOffset;
            if (player->stageUpdates < envelope->decayUpdates) {
                player->stageUpdates++;
                player->rampOffset = player->decayOffset += envelope->decaySlope;
                break;
            }
            player->stage = SOUND_VOICE_ENVELOPE_SUSTAIN;
        case SOUND_VOICE_ENVELOPE_SUSTAIN:
            pitchQ8 = (player->keyedPitch << 1) + envelope->sustainLevel;
            break;
        case SOUND_VOICE_ENVELOPE_RELEASE:
            releaseDirection = (player->rampOffset - envelope->releaseLevel) * envelope->releaseSlope;
            if (releaseDirection >= 0) {
                player->stage = SOUND_VOICE_ENVELOPE_RELEASE_HOLD;
            } else {
                player->rampOffset = player->releaseOffset += player->releaseStep;
            }
            pitchQ8 = (player->keyedPitch << 1) + player->releaseOffset;
            break;
        case SOUND_VOICE_ENVELOPE_RELEASE_HOLD:
            pitchQ8 = (player->keyedPitch << 1) + envelope->releaseLevel;
            break;
        default:
            return;
    }
    spuGetVoiceRef(voice->spuVoice, &voiceRef);
    attr = voiceRef.attr;
    attr->pitch =
        spuCalcPitch((pitchQ8 >> SOUND_VOICE_PITCH_FRACTION_BITS) & 0xFFFF, pitchQ8 & SOUND_VOICE_PITCH_FRACTION_MASK, player->rootKey, player->fineTune);
    attr->mask |= SPU_VOICE_PITCH;
}

s32 sndScriptTryStart(s32 soundId, s8 panOffset, s8 attenuation, SndBankSlot* bankSlot, SndScriptEntryControls* entryControls)
{
    _SndScriptSlotPick slotPick;

    // Survey before changing ownership; replacement applies only without a usable idle slot.
    _sndScriptScanSlotCandidates(&slotPick, entryControls->priority, soundId, entryControls->flags);
    if ((slotPick.groupCount < entryControls->maxInstances) && (slotPick.idleSlot != SOUND_SCRIPT_SLOT_NONE)) {
        slotPick.slot = slotPick.idleSlot;
    } else {
        slotPick.slot = _sndScriptSelectReplacementSlot(&slotPick, entryControls->retriggerTicks);
    }
    if (slotPick.slot >= 0) {
        _sndScriptStartSlot(slotPick.slot, panOffset, attenuation, soundId, bankSlot, entryControls);
    }
    return slotPick.slot;
}

void sndScriptSetMuteMatching(s32 soundSelector, s32 muted)
{
    enum {
        SOUND_SCRIPT_REQUEST_TYPE_MASK = 0xF0000000,
        SOUND_SCRIPT_MUTE_RAMP_UPDATES = 8
    };
    s32         slotIndex;
    _SndScript* script;

    for (slotIndex = 0; slotIndex < ARRAY_SIZE(_gSndScriptInstances); slotIndex++) {
        script = &_gSndScriptInstances[slotIndex];
        if ((soundSelector == script->soundId) || ((script->soundId & SOUND_SCRIPT_REQUEST_TYPE_MASK) == soundSelector)) {
            if (muted == 0) {
                if (script->state == SOUND_SCRIPT_MUTING) {
                    script->state = SOUND_SCRIPT_UNMUTING;
                    linInterpSetup(&script->volumeRamp, 0, (u8)_gSndScriptMasterVolume, SOUND_SCRIPT_MUTE_RAMP_UPDATES);
                }
            } else {
                if (script->state & SOUND_SCRIPT_MUTABLE) {
                    script->state = SOUND_SCRIPT_MUTING;
                    linInterpSetup(&script->volumeRamp, (u8)_gSndScriptMasterVolume, 0, SOUND_SCRIPT_MUTE_RAMP_UPDATES);
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

    scriptSlotIndex &= ARRAY_SIZE(_gSndScriptInstances) - 1;
    script           = &_gSndScriptInstances[scriptSlotIndex];

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

    script = &_gSndScriptInstances[scriptSlotIndex & (ARRAY_SIZE(_gSndScriptInstances) - 1)];
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

void sndScriptAcquireDuck(void)
{
    s8 masterVolume;

    _gSndScriptDuckRequests += 1;
    if (_gSndScriptDuckRequests == 1) {
        if (_gSndScriptDuckStep == 0) {
            if (_gSndScriptUnduckedVolume == 0) {
                masterVolume = sndScriptGetMasterVolume();
                if (masterVolume >= SOUND_SCRIPT_DUCK_LEVEL) {
                    _gSndScriptUnduckedVolume = masterVolume;
                    _gSndScriptDuckStep       = -SOUND_SCRIPT_DUCK_STEP;
                }
            }
        }
    }
}

void sndScriptReleaseDuck(void)
{
    if (_gSndScriptDuckRequests > 0) {
        _gSndScriptDuckRequests -= 1;
        if (_gSndScriptDuckRequests == 0) {
            if (_gSndScriptUnduckedVolume != 0) {
                _gSndScriptDuckStep = SOUND_SCRIPT_DUCK_STEP;
            }
        }
    }
}

/// Zeroes a nonempty word-aligned storage representation without releasing resources.
///
/// `storage` must provide `wordCount` writable s32 words; the count must be
/// positive, since the first word is always written. The void pointer admits
/// complete script, bank-slot and voice arrays as storage representations.
/// Call only during sound boot, with their resource users quiescent.
static inline void _sndScriptClearWords(void* storage, u32 wordCount)
{
    u32  wordIndex;
    s32* words = storage;

    wordIndex = 0;
    do {
        *words = 0;
        wordIndex++;
        words++;
    } while (wordIndex < wordCount);
}

/// Clears sound-script slots, bank-slot bindings and voice records at sound boot.
///
/// Call before bank reservations and playback, with audio updates quiescent.
/// These full-array word clears discard ownership without freeing images or
/// notifying the SPU. Restores master gain 127, no duck ramp or saved gain,
/// and reverb level 1. Duck-request nesting is initialized separately.
static void _sndScriptInit(void)
{
    _sndScriptClearWords(_gSndScriptInstances, sizeof(_gSndScriptInstances) / sizeof(s32));
    _sndScriptClearWords(_gSndBankSlots, sizeof(_gSndBankSlots) / sizeof(s32));
    _sndScriptClearWords(_gSndScriptVoices, sizeof(_gSndScriptVoices) / sizeof(s32));

    _gSndScriptDuckStep       = 0;
    _gSndScriptUnduckedVolume = 0;
    sndScriptSetMasterVolume(SOUND_SCRIPT_VOLUME_UNITY);
    _sndScriptSetReverbLevel(SOUND_SCRIPT_REVERB_DEFAULT_LEVEL);
}

/// Stores the sound-script reverb setting used when later notes are keyed on.
///
/// Negative inputs normalize to -1 (disabled), zero to 1, and positive signed
/// bytes remain unchanged. Setting 3 admits only level-3 notes. Other positive
/// settings follow the note threshold, also admitting level 3 at settings >=2.
static void _sndScriptSetReverbLevel(s8 reverbLevel)
{
    if (reverbLevel < 0) {
        _gSndScriptReverbLevel = SOUND_SCRIPT_REVERB_DISABLED;
        return;
    }
    _gSndScriptReverbLevel = reverbLevel;
    if (reverbLevel == 0) {
        _gSndScriptReverbLevel = SOUND_SCRIPT_REVERB_DEFAULT_LEVEL;
    }
}

s32 sndScriptFindInstanceById(s32 soundId)
{
    enum { SOUND_SCRIPT_SLOT_NONE = -1 };
    s32         slotIndex;
    _SndScript* script;

    slotIndex = 0;
    script    = _gSndScriptInstances;
    do {
        if ((script->state & SOUND_SCRIPT_PLAYING) && (script->soundId == soundId)) {
            return slotIndex;
        }
        slotIndex++;
        script++;
    } while (slotIndex < ARRAY_SIZE(_gSndScriptInstances));
    return SOUND_SCRIPT_SLOT_NONE;
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
    for (slotIndex = 0; slotIndex < ARRAY_SIZE(_gSndScriptInstances); slotIndex++) {
        script = &_gSndScriptInstances[slotIndex];
        if ((script->useUnduckedVolume != 1) || (_gSndScriptUnduckedVolume == 0)) {
            _sndScriptRescaleVoices(script, masterVolume);
            script->mixDirty = 1;
        }
    }
    // Clamp the saved level only after applying the original signed request.
    storedVolume = masterVolume;
    if (storedVolume < 0) {
        storedVolume = 0;
    }
    _gSndScriptMasterVolume = storedVolume;
}

s8 sndScriptGetMasterVolume(void)
{
    return _gSndScriptMasterVolume;
}

/// Selects a script-slot replacement from a completed start-request survey.
///
/// `retriggerTicks` is the entry's signed-halfword age limit in running audio
/// updates, passed as a word; -1 refuses every replacement. A newest group
/// member below the limit also refuses it. Otherwise choose the oldest group
/// member, then a lower-priority slot, then an equal-priority slot. Returns a
/// slot 0..7 or a SOUND_SCRIPT_SLOT_ refusal; only success writes `slot`.
static s8 _sndScriptSelectReplacementSlot(_SndScriptSlotPick* candidates, s32 retriggerTicks)
{
    s32 slotIndex;
    u8  slotByte;
    s32 noSlot;

    noSlot = SOUND_SCRIPT_SLOT_NONE;
    if (retriggerTicks == SOUND_SCRIPT_RETRIGGER_DISABLED) {
        return SOUND_SCRIPT_SLOT_REPLACEMENT_DISABLED;
    }
    if (candidates->newestGroupTicks < retriggerTicks) {
        return SOUND_SCRIPT_SLOT_RETRIGGER_TOO_SOON;
    }
    if (candidates->newestGroupSlot != noSlot) {
        goto replaceGroup;
    }
    slotIndex = candidates->lowerPrioritySlot;
    slotByte  = candidates->lowerPrioritySlot;
    if (slotIndex != noSlot) {
        goto selectSlot;
    }
    slotIndex = candidates->equalPrioritySlot;
    slotByte  = candidates->equalPrioritySlot;
checkCandidate:
    if (slotIndex == noSlot) {
        goto noReplacement;
    }
selectSlot:
    candidates->slot = slotByte;
    return slotIndex;
replaceGroup:
    slotIndex = candidates->oldestGroupSlot;
    slotByte  = candidates->oldestGroupSlot;
    goto checkCandidate;
noReplacement:
    return SOUND_SCRIPT_SLOT_NO_REPLACEMENT;
}

/// Gives up every SPU slot on a script's old voice chain before slot reuse.
///
/// NULL is an empty chain. Otherwise all records and links must stay live,
/// each holding a valid SPU voice. Key-off is queued; callbacks are cleared
/// before returning allocations so completion cannot unlink the walked chain.
/// Clears allocation marks and voice numbers, retaining owner and neighbor
/// links and note/envelope data. The caller must discard its old list head
/// before reusing any record; run with audio updates and allocation serialized.
static inline void _sndScriptDiscardVoices(_SndVoice* voice)
{
    if (voice != NULL) {
        do {
            spuKeyOff(voice->spuVoice);
            voice->allocated = 0;
            spuClearVoiceCallback(voice->spuVoice);
            spuReleaseVoiceSlot(voice->spuVoice);
            voice->spuVoice = 0;
            voice           = voice->next;
        } while (voice != NULL);
    }
}

/// Replaces one chosen script slot and schedules its new entry for the next update.
///
/// `scriptSlotIndex` must be 0..7. Existing voices are keyed off, unregistered
/// and released before the slot borrows the new bank and its oneC byte cursor.
/// `soundId` is already resolved; pan and attenuation retain signed-byte mix
/// units. The bank image and sample tables must stay loaded through playback.
/// This stores the new cursor and unducked-volume policy; the interpreter
/// reloads `entryControls` when oneC executes, rather than on this call.
static void _sndScriptStartSlot(s32 scriptSlotIndex, s8 panOffset, s8 attenuation, s32 soundId, SndBankSlot* bankSlot, SndScriptEntryControls* entryControls)
{
    _SndScript*             script;
    SndScriptEntryControls* controls;
    u16                     flags;

    // Release the previous hardware allocations before publishing the new instance.
    controls = entryControls;
    script   = &_gSndScriptInstances[scriptSlotIndex];
    _sndScriptDiscardVoices(script->voices);
    script->state             = SOUND_SCRIPT_STARTING;
    script->voices            = NULL;
    script->bankSlot          = bankSlot;
    script->soundId           = soundId;
    script->runningTicks      = 0;
    script->panOffset         = panOffset;
    script->attenuation       = attenuation;
    script->loopDepth         = 0;
    flags                     = controls->flags;
    script->cursor            = (u8*)entryControls;
    script->useUnduckedVolume = (flags & SOUND_SCRIPT_USE_UNDUCKED_VOLUME) != 0;
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

void sndBankSlotReleaseImage(s32 slotIndex)
{
    enum { SOUND_BANK_SLOT_ID_FREE = -1 };
    SndBankSlot* slot;
    SndBankSlot* base;

    if ((u8)slotIndex < ARRAY_SIZE(_gSndBankSlots)) {
        base = _gSndBankSlots;
        slot = &base[(s8)slotIndex];
        sndHeapFree(slot->image);
        slot->bankId = SOUND_BANK_SLOT_ID_FREE;
        slot->image  = NULL;
    }
}

/// Takes an SPU voice from the script/shared ranges and binds its resident record.
///
/// Only the low unsigned halfword of `voicePriority` participates. The ranges
/// must remain within hardware voices 16..23: script voices 18..23 are tried
/// before shared voices 16..17. Returns the corresponding live record or NULL
/// on refusal. Reassignment detaches its prior owner through the SPU callback;
/// the caller must key on and attach it, or release and clear the registration.
static _SndVoice* _sndVoiceAlloc(s32 voicePriority)
{
    enum { SOUND_SCRIPT_FIRST_SPU_VOICE     = 16,
           SOUND_SCRIPT_VOICE_PRIORITY_MASK = 0xFFFF };
    s32        spuVoiceIndex;
    _SndVoice* voice;

    spuVoiceIndex = (s8)spuAllocVoice(_gSndScriptVoiceRangeIndices, ARRAY_SIZE(_gSndScriptVoiceRangeIndices), voicePriority & SOUND_SCRIPT_VOICE_PRIORITY_MASK);
    if (spuVoiceIndex < 0) {
        return NULL;
    }
    /* Ranges 1 and 2 cover hardware voices 16..23. */
    voice           = &_gSndScriptVoices[spuVoiceIndex - SOUND_SCRIPT_FIRST_SPU_VOICE];
    voice->spuVoice = spuVoiceIndex;
    spuSetVoiceCallback(spuVoiceIndex, _sndVoiceDetach, voice);
    voice->allocated = 1;
    return voice;
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

/// Advances a scripted voice's gate and active pitch envelope for one audio update.
///
/// Gate time is 16.16 script ticks; each update subtracts one tick, or
/// 39321/65536 on PAL. `SOUND_SCRIPT_NOTE_HELD` never counts down. Key-off is
/// requested on the update after the clock reaches or crosses zero and on
/// later expired-gate updates. Only a held envelope is armed for release here;
/// active envelope playback continues on both sides of gate expiry. Returns 0.
static s32 _sndVoiceTick(_SndVoice* voice)
{
    enum {
        SOUND_SCRIPT_GATE_STEP_PAL   = 0xFFFF6667,
        SOUND_SCRIPT_GATE_STEP_WHOLE = 0xFFFF0000
    };
    s32 remainingGateTicks;

    remainingGateTicks = voice->gateClock;
    if (remainingGateTicks <= 0) {
        voice->gateClock = 0;
        spuKeyOff(voice->spuVoice);
        if (voice->envelope.active != 0) {
            if (voice->envelope.releaseRequest == SOUND_VOICE_ENVELOPE_HELD) {
                voice->envelope.releaseRequest = SOUND_VOICE_ENVELOPE_RELEASE_PENDING;
            }
            if (voice->envelope.active != 0) {
                _sndVoiceTickPitchEnvelope(voice);
            }
        }
    } else {
        if (remainingGateTicks <= SOUND_SCRIPT_NOTE_HELD - 1) {
            if (gDisplayState.region == MODE_PAL) {
                voice->gateClock = remainingGateTicks + SOUND_SCRIPT_GATE_STEP_PAL;
            } else {
                voice->gateClock = remainingGateTicks + SOUND_SCRIPT_GATE_STEP_WHOLE;
            }
        }
        if (voice->envelope.active != 0) {
            _sndVoiceTickPitchEnvelope(voice);
        }
    }
    return 0;
}

/// Requests key-off and pitch-envelope release for the voices of a stopped script.
///
/// Keep-release stops key off directly. Other stops set ADSR release rate 5
/// while preserving the exponential-mode bit, unless cached key status is
/// already off; an off key with an active ADSR envelope needs no second key-off.
/// Every active pitch player is armed for release again, including an already
/// releasing one, so its next update recaptures the step from its ramp offset.
/// Returns the number of active pitch-envelope voices, which keep the script
/// slot alive. The script and attached voice list must remain valid throughout.
static s32 _sndScriptReleaseVoices(_SndScript* script)
{
    enum {
        SOUND_SCRIPT_ADSR_RELEASE_RATE_MASK = 0x1F,
        SOUND_SCRIPT_ADSR_FAST_RELEASE_RATE = 5
    };
    SpuVoiceRef voiceRef;
    _SndVoice*  voice;
    _SndVoice*  head;
    s32         activeEnvelopeCount;
    u8          keyStatus;
    u16         adsr2;

    /// Queues fast release using the caller's halfword scratch variable.
    ///
    /// `ref` must be a side-effect-free SpuVoiceRef lvalue and `releaseBits` a
    /// u16 lvalue; both are evaluated repeatedly. Uses this function's release
    /// constants and preserves every ADSR bit outside the five-bit rate.
#define SOUND_SCRIPT_SET_FAST_RELEASE(ref, releaseBits)                          \
    do {                                                                         \
        (releaseBits) = (ref).attr->adsr2;                                       \
        (releaseBits) = ((releaseBits) & ~SOUND_SCRIPT_ADSR_RELEASE_RATE_MASK) | \
                        SOUND_SCRIPT_ADSR_FAST_RELEASE_RATE;                     \
        (ref).attr->adsr2 = (releaseBits);                                       \
        (ref).attr->mask |= SPU_VOICE_ADSR_ADSR2;                                \
    } while (0)

    head                = script->voices;
    activeEnvelopeCount = 0;
    if (head != NULL) {
        voice = head;
        do {
            if (voice->spuVoice >= 0) {
                // Keep the voice's ADSR only for an explicit keep-release stop.
                if (script->keepRelease != SOUND_SCRIPT_KEEP_RELEASE) {
                    keyStatus = spuGetVoiceKeyStatus(voice->spuVoice);
                    if (keyStatus != SPU_OFF) {
                        spuGetVoiceRef(voice->spuVoice, &voiceRef);
                        SOUND_SCRIPT_SET_FAST_RELEASE(voiceRef, adsr2);
                        if (keyStatus != SPU_OFF_ENV_ON) {
                            spuKeyOff(voice->spuVoice);
                        }
                    }
                } else {
                    spuKeyOff(voice->spuVoice);
                }
                if (voice->envelope.active != 0) {
                    activeEnvelopeCount           += 1;
                    voice->envelope.releaseRequest = SOUND_VOICE_ENVELOPE_RELEASE_PENDING;
                }
            }
            voice = voice->next;
        } while (voice != NULL);
    }
    return activeEnvelopeCount;
}
#undef SOUND_SCRIPT_SET_FAST_RELEASE

/// Calculates the current stereo SPU volumes for one scripted voice.
///
/// Pan adds three steps per signed offset unit to the voice's 0..127 base pan.
/// Attenuation magnitudes 0..127 scale the gain index by (127 - magnitude)/127;
/// -128 instead scales it by 1/127. The index is clamped to 0..127 before
/// applying the velocity table and normalized volume ramp. A negative hardware
/// voice index leaves the output untouched. The caller submits the volumes;
/// applying the ramp can clear its completed step.
static void _sndVoiceCalcMixVolumes(s8 panOffset, s8 attenuation, _SndVoice* voice, LinInterp* volumeRamp, SpuVolume* panVolumes)
{
    enum { SOUND_SCRIPT_PAN_STEPS_PER_OFFSET = 3 };
    s32 gainIndex;

    if (voice->spuVoice >= 0) {
        gainIndex = SOUND_SCRIPT_VOLUME_UNITY - abs(attenuation);
        gainIndex = voice->scaledVolume * abs(gainIndex) / SOUND_SCRIPT_VOLUME_UNITY;
        gainIndex = (gainIndex < SOUND_SCRIPT_VOLUME_UNITY + 1) ? ((gainIndex < 0) ? 0 : gainIndex) : SOUND_SCRIPT_VOLUME_UNITY;
        spuCalcPanVolumes(panVolumes, (s8)voice->basePan + panOffset * SOUND_SCRIPT_PAN_STEPS_PER_OFFSET,
                          linInterpApply(volumeRamp, gSpuVelocityGainTable[gainIndex]));
    }
}

/// Arms a scripted voice's pitch-envelope player from an image-relative oneE chunk.
///
/// `keyedPitch` supplies its low 16 bits in Q7 semitone units. Offset -1 or a
/// missing parent script disables playback; otherwise the script's bank image
/// must hold an aligned, complete chunk at the signed byte offset. A matching
/// tag resets the player and caches the borrowed layer's tuning. A mismatched
/// tag still replaces the chunk pointer, retaining all other player state.
/// The image must remain loaded while the voice's envelope is being played.
static void _sndVoiceSetupPitchEnvelope(_SndVoice* voice, s16 pitchEnvelopeOffset, u32 keyedPitch, const SndBankLayer* layer)
{
    _SndVoiceEnvelope* player;
    u8*                imageBytes;
    _SndPitchEnvelope* envelope;
    s32                magic;
    s16                fineTune;

    player = &voice->envelope;
    if (pitchEnvelopeOffset == SOUND_SCRIPT_NOTE_NO_ENVELOPE) {
        voice->envelope.active = 0;
        return;
    }
    if (voice->script == NULL) {
        voice->envelope.active = 0;
        return;
    }
    // pitchEnvelopeOffset is a byte offset from the start of the bank image.
    // A tag other than oneE keeps the pointer and leaves the player flags unchanged.
    imageBytes       = (u8*)voice->script->bankSlot->image;
    envelope         = (_SndPitchEnvelope*)&imageBytes[pitchEnvelopeOffset];
    player->envelope = envelope;
    magic            = envelope->magic;
    if (magic == SOUND_SCRIPT_PITCH_ENVELOPE_TAG) {
        voice->envelope.active = 1;
        player->stage          = SOUND_VOICE_ENVELOPE_DELAY;
        player->releaseRequest = SOUND_VOICE_ENVELOPE_HELD;
        player->keyedPitch     = keyedPitch & 0xFFFF;
        player->rootKey        = layer->rootKey;
        fineTune               = layer->fineTune;
        player->stageUpdates   = 0;
        player->attackOffset   = 0;
        player->decayOffset    = 0;
        player->releaseOffset  = 0;
        player->fineTune       = fineTune;
    }
}

/// Applies an image-relative oneA ADSR override to a caller-owned voice attribute.
///
/// Offset -1 keeps the layer ADSR. Every other signed byte offset must address
/// an aligned, readable chunk within the live bank image; no bounds are checked.
/// Returns 1 after replacing both registers, or -1 for the sentinel or another
/// tag, leaving the attribute unchanged. It does not set the SPU update mask.
static s32 _sndScriptApplyAdsrOverride(const SndBankHdr* image, s16 adsrOffset, SpuVoiceAttr* attr)
{
    const u8*             imageBytes;
    const _SndScriptAdsr* chunk;

    if (adsrOffset != SOUND_SCRIPT_NOTE_LAYER_ADSR) {
        imageBytes = (const u8*)image;
        chunk      = (const _SndScriptAdsr*)&imageBytes[adsrOffset];
        if (chunk->magic == SOUND_SCRIPT_ADSR_TAG) {
            attr->adsr1 = chunk->adsr1;
            attr->adsr2 = chunk->adsr2;
            return SOUND_SCRIPT_ADSR_APPLIED;
        }
        return SOUND_SCRIPT_ADSR_NOT_APPLIED;
    }
    return SOUND_SCRIPT_ADSR_NOT_APPLIED;
}

/// Marks type-1 and area-script slots idle before a stage's deferred key-off.
///
/// Only the state changes. Ids, voice lists, bank references and SPU state
/// remain intact for the later queued key-off; this does not release resources.
static void _sndScriptResetStageSlots(void)
{
    s32         slotIndex;
    s32         typeMask;
    s32         ambientType;
    s32         areaType;
    s32         type1;
    _SndScript* script;
    s32         bankType;

    slotIndex   = 0;
    typeMask    = 0xF0000000;
    ambientType = SOUND_STAGE_AMBIENT & typeMask;
    areaType    = SOUND_AREA_BANK_ALL;
    type1       = SOUND_SCRIPT_REQUEST_TYPE_1;
    script      = _gSndScriptInstances;
    do {
        bankType = script->soundId & typeMask;
        if (bankType != ambientType) {
            if ((bankType == areaType) || (bankType == type1)) {
                script->state = SOUND_SCRIPT_IDLE;
            }
        }
        slotIndex++;
        script++;
    } while (slotIndex < ARRAY_SIZE(_gSndScriptInstances));
}
