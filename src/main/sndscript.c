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
#include "task.h"

/// 6-byte block assigned via unaligned lwl/lwr + lb/sb (see TaskIdMap_RemapIndex).
typedef struct _GBytes6 {
    u8 data[6];
} GBytes6;

typedef struct _SndVoice SndVoice;

struct _SndScript;

/// "oneE" (0x45656E6F) pitch-envelope chunk pointed at by SndVoiceFx.field_20.
/// Consumed by the state machine in SndVoice_TickEnvelope.
typedef struct _SndOneE {
    /* 0x00 */ s32 magic;
    /* 0x04 */ s16 field_4;
    /* 0x06 */ s16 field_6;
    /* 0x08 */ s16 field_8;
    /* 0x0A */ u16 field_A;
    /* 0x0C */ s16 field_C;
    /* 0x0E */ u16 field_E;
    /* 0x10 */ s16 field_10;
    /* 0x12 */ s16 field_12;
    /* 0x14 */ s16 field_14;
    /* 0x16 */ s16 field_16;
} SndOneE;
STATIC_ASSERT_SIZEOF(SndOneE, 0x18);

/// FX/envelope sub-block embedded at SndVoice + 0x10 (SndVoice_SetupEnvelope / SndVoice_TickEnvelope).
/// field_0 is an active flag; field_1 is the state-machine index; field_2 is a
/// secondary gate; field_20 points at the current "oneE" (0x45656E6F) chunk.
typedef struct _SndVoiceFx {
    /* 0x00 */ s8       field_0;
    /* 0x01 */ s8       field_1;
    /* 0x02 */ s8       field_2;
    /* 0x03 */ u8       pad_3;
    /* 0x04 */ s32      field_4;
    /* 0x08 */ s16      field_8;
    /* 0x0A */ s16      field_A;
    /* 0x0C */ u16      field_C;
    /* 0x0E */ s16      field_E;
    /* 0x10 */ s32      field_10;
    /* 0x14 */ s32      field_14;
    /* 0x18 */ s32      field_18;
    /* 0x1C */ s32      field_1C;
    /* 0x20 */ SndOneE* field_20;
} SndVoiceFx;
STATIC_ASSERT_SIZEOF(SndVoiceFx, 0x24);

/// Voice/FX object for one SPU voice, allocated by `SndVoice_Alloc`.
///
/// SPU voices 16..23 have records in SndScript_Voices; voices 0..15 belong to
/// the MIDI sequencer and are never allocated here.
/// field_0 is the SPU voice index; field_4 is a countdown/timer (SndVoice_Tick).
/// field_10 contains the FX state, including its active and secondary gates.
/// field_34/field_38/field_3C are parent/prev/next list links (SndVoice_Detach free).
struct _SndVoice {
    /* 0x00 */ s8                 field_0;
    /* 0x01 */ u8                 field_1;
    /* 0x02 */ s8                 field_2;
    /* 0x03 */ u8                 field_3;
    /* 0x04 */ s32                field_4;
    /* 0x08 */ s16                field_8;
    /* 0x0A */ u8                 field_A;
    /* 0x0B */ u8                 pad_0B;
    /* 0x0C */ struct _SndOneV*   field_C; // current oneV/script command (SndScript_Exec)
    /* 0x10 */ SndVoiceFx         field_10;
    /* 0x34 */ struct _SndScript* field_34;
    /* 0x38 */ SndVoice*          field_38;
    /* 0x3C */ SndVoice*          field_3C;
};
STATIC_ASSERT_SIZEOF(SndVoice, 0x40);
STATIC_ASSERT(OFFSET_OF(SndVoice, field_10) == 0x10, snd_voice_fx_offset);
STATIC_ASSERT(OFFSET_OF(SndVoice, field_34) == 0x34, snd_voice_owner_offset);

/// "oneV" (0x56656E6F) voice-on script command consumed by SndScript_Exec.
/// Also the 0x18-byte payload after a "oneC" (0x43656E6F) command.
typedef struct _SndOneV {
    /* 0x00 */ s32 magic;
    /* 0x04 */ u16 field_4;  // bank id for Snd_FindBank (0 = use ctx bank)
    /* 0x06 */ u8  field_6;  // note group for Snd_GetNote
    /* 0x07 */ u8  field_7;  // note index for Snd_GetNote
    /* 0x08 */ u16 field_8;  // duration (high half of field_8 timer units)
    /* 0x0A */ u16 field_A;  // voice countdown (0 → 0x7FFFFFFF)
    /* 0x0C */ s8  field_C;  // pan bias (<0 → use SndBankLayer::pan)
    /* 0x0D */ s8  field_D;  // volume scale (<0 → use SndBankLayer::volume)
    /* 0x0E */ s8  field_E;  // reverb gate vs D_8008274B
    /* 0x0F */ u8  pad_F;
    /* 0x10 */ u16 field_10; // voice-alloc priority for SndVoice_Alloc
    /* 0x12 */ s16 field_12; // oneA offset for SndScript_FindOneA
    /* 0x14 */ u16 field_14; // base pitch
    /* 0x16 */ s16 field_16; // oneE offset for SndVoice_SetupEnvelope (-1 disables)
} SndOneV;
STATIC_ASSERT_SIZEOF(SndOneV, 0x18);

/// "Loop" (0x706F6F4C) / "Wait" (0x74696157) / "endL" (0x4C646E65) script cmds.
/// Loop: repeat count and minimum wait; Wait: signed duration.
/// endL contains only the magic word and advances the byte cursor by four.
typedef struct _SndScriptCmd {
    /* 0x0 */ s32 magic;
    /* 0x4 */ union {
        struct {
            u8  field_4;
            u8  pad_5;
            u16 field_6;
        } loop;
        s32 duration;
    } data;
} SndScriptCmd;
STATIC_ASSERT_SIZEOF(SndScriptCmd, 0x8);

/// 0x60-byte slot in SndScript_Slots[8]. field_0 is an ID looked up by
/// SndVoice_FindById; field_16 holds status flags (mask 0xA3 selects active entries).
/// field_E is a dirty flag; field_10/11/12 and field_13/14/15 are paired ramps
/// (current/target/step) updated by SndVoice_SetPanRamp and SndVoice_SetVolumeRamp respectively.
/// field_17/field_18/field_20 are a loop stack (depth, remaining counts, restart
/// positions) used by Loop/endL in SndScript_Exec.
/// field_40 heads the doubly-linked voice list (SndVoice_Attach/Detach);
/// SndScript_TickVoices walks the list and SndScript_Play clears it;
/// field_44 is the `SndBankSlot` whose bank the script plays (its image holds the
/// `oneC` entry offsets, its bank is the default for a `oneV` with bank id 0);
/// field_48 is a byte cursor over variable-length tagged commands;
/// field_F is bit1 of SndScriptEntryControls::flags.
/// field_4C is the `SndScriptEntryControls` block of the sound being played, reloaded by
/// its `oneC` command.
/// field_50 is a volume interpolator driven by SndVoice_FadeMatching via LinInterp_Setup.
typedef struct _SndScript {
    /* 0x00 */ s32                     field_0;
    /* 0x04 */ s32                     field_4;
    /* 0x08 */ s32                     field_8;
    /* 0x0C */ s8                      field_C;
    /* 0x0D */ s8                      field_D;
    /* 0x0E */ s8                      field_E;
    /* 0x0F */ s8                      field_F;
    /* 0x10 */ u8                      field_10;
    /* 0x11 */ u8                      field_11;
    /* 0x12 */ s8                      field_12;
    /* 0x13 */ u8                      field_13;
    /* 0x14 */ u8                      field_14;
    /* 0x15 */ s8                      field_15;
    /* 0x16 */ u8                      field_16;
    /* 0x17 */ u8                      field_17;
    /* 0x18 */ u8                      field_18[8];
    /* 0x20 */ u8*                     field_20[8];
    /* 0x40 */ SndVoice*               field_40;
    /* 0x44 */ SndBankSlot*            field_44;
    /* 0x48 */ u8*                     field_48;
    /* 0x4C */ SndScriptEntryControls* field_4C;
    /* 0x50 */ LinInterp               field_50;
} SndScript;
STATIC_ASSERT_SIZEOF(SndScript, 0x60);

/// "oneA" (0x41656E6F) tagged chunk header read by SndScript_FindOneA.
/// Located at a signed byte offset into a raw buffer.
typedef struct _SndOneA {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ u16 field_4;
    /* 0x6 */ u16 field_6;
} SndOneA;
STATIC_ASSERT_SIZEOF(SndOneA, 0x8);

STATIC_ASSERT(OFFSET_OF(SpuVoiceAttr, adsr1) == 0x3A, snd_voice_adsr1_offset);
STATIC_ASSERT(OFFSET_OF(SpuVoiceAttr, adsr2) == 0x3C, snd_voice_adsr2_offset);

/// 0x18-byte voice-slot lookup result filled by SndVoice_ScanCandidates and consumed by
/// SndVoice_AllocSlot / SndVoice_SelectStealCandidate. field_0 is the chosen slot index (or error);
/// field_1..field_6 are candidate slot indices (-1 = empty); field_7 is the
/// candidate count; field_8/C/10/14 hold ranking scores / IDs.
typedef struct _SndVoicePick {
    /* 0x00 */ s8  field_0;
    /* 0x01 */ s8  field_1;
    /* 0x02 */ s8  field_2;
    /* 0x03 */ s8  field_3;
    /* 0x04 */ s8  field_4;
    /* 0x05 */ s8  field_5;
    /* 0x06 */ s8  field_6;
    /* 0x07 */ u8  field_7;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
    /* 0x10 */ s32 field_10;
    /* 0x14 */ s32 field_14;
} SndVoicePick;
STATIC_ASSERT_SIZEOF(SndVoicePick, 0x18);

/// 0xC-byte init-table entry at Snd_BankInitTable (two entries used by Snd_InitBanks).
/// field_0 indexes Snd_BankSlotsByType for a slot id; field_2 is written to SndBankSlot.bankId
/// and `SndBank::bankId`; field_4/field_6 are SndHeap_Malloc sizes; field_8 is stored
/// to SndBankSlot.spuAddr.
typedef struct _SndBankInitEntry {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
    /* 0x4 */ u16 field_4;
    /* 0x6 */ u16 field_6;
    /* 0x8 */ s32 field_8;
} SndBankInitEntry;
STATIC_ASSERT_SIZEOF(SndBankInitEntry, 0xC);

// oneC controls apply to a whole script instance and all voices it starts.
enum {
    SOUND_SCRIPT_ENTRY_TAG           = 0x43656E6F,
    SOUND_SCRIPT_VOLUME_UNITY        = 127,
    SOUND_SCRIPT_ALLOW_DISABLED_TYPE = 0x01,
    SOUND_SCRIPT_USE_UNDUCKED_VOLUME = 0x02,
    SOUND_SCRIPT_GROUP_BY_FLAGS      = 0x10,
    SOUND_SCRIPT_REJECT_WHILE_MUTED  = 0x80,
    SOUND_SCRIPT_RETRIGGER_DISABLED  = -1
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

static SndScript SndScript_Slots[8];

/// Script voices use SPU slots 16..23, after the MIDI sequencer's sixteen slots.
static SndVoice SndScript_Voices[8];

static s8 D_80082748;

static s8 D_80082749;

static s8 D_8008274A;

static s8 D_8008274B;

static volatile s32 D_8008274C;

static u8 D_80068A54[];

static SndBankInitEntry Snd_BankInitTable[];

static s16 SndScript_VoiceRanges[];

/* Per-type arg1 limits for TaskIdMap_RemapIndex; sits between this TU's first
 * jtbl (SndLoad_ResolveSpuAddr) and TaskIdMap's jtbl at 0x80014130. */
static const GBytes6 D_80014124;

static void Snd_ClearBusy(void);

static void Snd_SetBusyFlag(s32 arg0);

static void SndVoice_SetPriority(s8 arg0);

static void SndEvt_EnqueueTypeF(void);

static void SndVoice_StepMasterLevel(void);

static s32 SndVoice_DriveSlots(s32* unused);

static void SndVoice_ScanCandidates(SndVoicePick* candidates, u16 arg1, s32 arg2, u16 arg3);

/// Advances a script's 16.16 tick clock by one step: a whole tick, or 0.6 of
/// one when the display region is 1.
static inline void _sndScriptAdvanceClock(SndScript* script);

/// Decides whether a note plays with reverb, from its own level against a
/// global one. A note at level 3 gets reverb whenever the global level is at
/// least 2; a global level of 3 turns it off for every other note; otherwise a
/// note with a non-negative level gets reverb once the global level reaches it.
static inline u8 _sndScriptUseReverb(SndOneV* oneV);

static s32 SndScript_Exec(SndScript* script);

static void SndVoice_TickEnvelope(SndVoice* voice);

static void SndVoice_Init(void);

static void SndVoice_SetPriorityLevel(s8 arg0);

/// Selects an eligible voice candidate, respecting the retrigger-age limit.
static s8 SndVoice_SelectStealCandidate(SndVoicePick* candidates, s32 retriggerTicks);

static void SndScript_Play(s32 arg0, s8 arg1, s8 arg2, s32 arg3, SndBankSlot* slot, SndScriptEntryControls* entryControls);

static void SndVoice_Detach(void* context);

static SndBankSlot* _sndBankSlotFind(u16 bankId, s32 matchMode);

static SndVoice* SndVoice_Alloc(s32 arg0);

static void SndVoice_Attach(SndScript* arg0, SndVoice* voice);

static s32 SndVoice_Tick(SndVoice* voice);

static s32 SndScript_TickVoices(SndScript* script);

static void SndVoice_ScaleVolume(s8 arg0, s8 arg1, SndVoice* voice, LinInterp* arg3, s16* arg4);

static void SndVoice_SetupEnvelope(SndVoice* voice, s16 envelopeOffset, u32 pitch, SndBankLayer* bankLayer);

static s32 SndScript_FindOneA(u8* arg0, s16 arg1, SpuVoiceAttr* arg2);

static void SndVoice_ClearActive(void);

static u8               D_80068A54[]        = { 0xFF, 0xFF, 0xFF, 0xFF, 0x20, 0x26, 0x20, 0x26, 0x2E, 0x05, 0x1E, 0xFF };
static SndBankInitEntry Snd_BankInitTable[] = {
    { 0x0002, 0x20FF, 0x00CE, 0x0210, 0x73810 },
    { 0x000E, 0xE0FF, 0x0078, 0x0168, 0x6F810 },
};
s32        D_80068A78              = 0;
static s16 SndScript_VoiceRanges[] = { 1, 2 };

void Snd_InitFromStage(s32 arg0, s32 arg1)
{
    SndBank* var_s0;
    s32      var_a0;
    s32      var_v1;
    s32      temp_v1;

    D_8008274C = 0;
    SndVoice_ClearActive();
    arg0 = arg0 & 0xFF;
    SndEvt_EnqueueTypeF();
    SndEvt_EnqueueType7(0x50000000, 1);
    SndEvt_EnqueueType7(0x10000000, 1);
    SndEvt_EnqueueType7(0xFF0D, 1);
    SndEvt_EnqueueType7(0x20000000, 1);
    SndEvt_EnqueueType7(0xE0000000, 1);
    arg1       = arg1 & 0xFF;
    D_80082120 = arg0;
    D_80082136 = arg1;
    SndBankSlot_Free(1);
    SndBankSlot_Free(7);

    var_v1 = 0;
    if (arg1 == 5) {
        if (arg0 == 4) {
            var_a0 = 3;
        } else {
            goto block_5;
        }
    } else {
    block_5:
        do {
            if (D_80068A54[var_v1 + arg0 * 2] == arg1) {
                var_a0 = 2;
                goto block_done;
            }
            var_v1++;
        } while (var_v1 < 2);
        var_a0 = 1;
    }
block_done:
    SndVoice_SetPriority(var_a0);
    D_80082130 = 0x3D010;
    D_80082128 = 0;
    D_80082124 = D_80082128;

    temp_v1 = (s8)D_80082135;
    switch (temp_v1) {
        case 0:
            Snd_FreeBank(&Snd_Banks[4]);
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
    Snd_FreeBank(var_s0);
    Snd_FreeBank(var_s0 + 6);
    Snd_FreeBank(var_s0 + 4);
    SndBankSlot_Free(5);
    Snd_FreeBank(var_s0 + 5);
    SndBankSlot_Free(6);
    Snd_FreeBank(var_s0 + 2);
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
                arg0       = 0x3D010;
                goto set_slot;
            }
            if ((s8)D_80082122 > 0 && (s8)D_80082122 < 3) {
                arg0 = Snd_Banks[(s8)D_80082122 + 3].spuAddr +
                       Snd_Banks[(s8)D_80082122 + 3].waveBytes;
                D_80082122 += 1;
                goto store_size;
            }
            arg0 = 0;
            if (D_80082122 != 0) {
                goto clear_ret;
            }
            arg0 = 0x3D010;
        set_slot:
            D_80082122 = 1;
        store_size:
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

/* Per-type arg1 limits for TaskIdMap_RemapIndex; sits between this TU's first
 * jtbl (SndLoad_ResolveSpuAddr) and TaskIdMap's jtbl at 0x80014130. */
static const GBytes6 D_80014124 = { { 0x00, 0x08, 0x07, 0x0B, 0x0C, 0x0A } };

s32 TaskIdMap_RemapIndex(s32 arg0, s32 arg1, s32 arg2)
{
    GBytes6 sp;
    s32     temp;

    sp   = D_80014124;
    arg2 = arg2 - 1;

    switch (arg0 & 0xFF) {
        case 1:
        case 2:
            break;
        case 3:
            temp = (s8)arg1;
            if (temp >= 9) {
                if ((temp == 0x1A) || (temp == 0x1D)) {
                    arg1 = 0xA;
                } else {
                    arg1 = 9;
                }
            }
            break;
        case 4:
            temp = (s8)arg1;
            if (temp >= 0x14) {
                switch ((s8)(arg1 - 0x17)) {
                    case 0:
                        arg1 = 0xB - arg2;
                        break;
                    case 3:
                        arg1 = 0x10 - arg2;
                        break;
                    case 5:
                        arg1 = 0x11 - arg2;
                        break;
                    case 6:
                        arg1 = 0x12 - arg2;
                        break;
                    case 7:
                        arg1 = 0x13 - arg2;
                        break;
                    default:
                        arg1 = 0xF - arg2;
                        break;
                }
            } else if (temp < 9) {
                arg1 = 0;
            } else {
                arg1 = arg1 - arg2;
            }
            break;
        case 5:
            temp = (s8)arg1;
            switch (temp) {
                case 0x14:
                    arg1 = 0;
                    break;
                case 0x1D:
                    arg1 = 1;
                    break;
                default:
                    temp = arg1 << 24;
                    temp = temp >> 24;
                    arg1 = arg1 - arg2;
                    temp = temp < ((arg2 & 0xFF) + 1);
                    if (temp != 0) {
                        arg1 = 0;
                    }
                    break;
            }
            break;
        default:
            arg1 = 0;
            break;
    }

    if ((u32)(arg1 & 0xFF) >= (u32)sp.data[arg0 & 0xFF]) {
        arg1 = 0;
    }
    return arg1 & 0xFF;
}

static void Snd_ClearBusy(void)
{
    Snd_SetBusyFlag(0);
}

static void Snd_SetBusyFlag(s32 arg0)
{
    if (arg0 == 0) {
        Snd_FreeBank(&Snd_Banks[12]);
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
    AudioTick_Insert(Midi_Tick, 0, 0x4800, 0);
    AudioTick_Insert(SndVoice_DriveSlots, 0, 0x8800, 0);
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
/// Callers select that bank by placing `SOUND_BANK_TYPE_1` in the request's
/// high half, with the rest of the bank id clear.
/// `SOUND_SCRIPT_REQUEST_ENTRY_INSTANCE_MASK` keeps the entry index and
/// instance tag, and the loaded script image's bank id replaces the high
/// half. Any other request is returned unchanged, including a type-1
/// request when no slot has a type-1 sample descriptor. The search reads that
/// descriptor's type; the stamp reads `image->bankId`, so the slot must hold
/// a completed script image.
static s32 _sndScriptRemapType1Id(s32 requestId)
{
    enum {
        SOUND_SCRIPT_REQUEST_TYPE_1 = 0x10000000,
        /// Mask keeping a script request's entry index and instance tag.
        ///
        /// Bits 0..7 select the script entry and bits 8..15 distinguish instances.
        /// Type-1 remapping replaces the bank id above these bits.
        SOUND_SCRIPT_REQUEST_ENTRY_INSTANCE_MASK = 0xFFFF
    };
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
        D_800689EC = 0;
        SndVoice_ApplyMasterVolume(0x7F);
        var_a0 = 0x40;
    } else {
        D_800689EC = 1;
        SndVoice_ApplyMasterVolume(0x28);
        var_a0 = 0;
    }
    Midi_SetMasterVolume(var_a0);
}

s32 Snd_InitBanks(u32 unused)
{
    s32               i;
    s8                slot;
    SndBankSlot*      bankSlot;
    SndBank*          bank;
    SndBankInitEntry* entry;
    s8*               map;
    SndBank*          banks;
    s32               id;

    *(volatile s32*)&D_80068A78 = 0xFF;
    Spu_SetVoiceRange(1, 0x12, 6);
    i = 0;
    SndVoice_Init();
    SndVoice_SetPriority(1);
    SndBank_SetEnableFlags(1, 0x80000000);

    map   = Snd_BankSlotsByType;
    banks = Snd_Banks;
    entry = Snd_BankInitTable;
loop:
    slot     = *(s8*)(entry->field_0 + (s32)map);
    bankSlot = SndBankSlot_Get(slot);
    id       = entry->field_2;
    // Subtraction preserves the scaled slot first in the address addition.
    bank             = banks - -slot;
    bankSlot->bank   = bank;
    bankSlot->bankId = id;
    bank->bankId     = entry->field_2;
    i++;
    bankSlot->bank->heapBlock       = SndHeap_Malloc(entry->field_4);
    bankSlot->bank->groups          = bankSlot->bank->heapBlock;
    bankSlot->bank->layers          = bankSlot->bank->heapBlock;
    bankSlot->bank->groupFirstLayer = bankSlot->bank->heapBlock;
    bankSlot->image                 = SndHeap_Malloc(entry->field_6);
    bankSlot->spuAddr               = entry->field_8;
    entry++;
    if (i < 2) {
        goto loop;
    }

    *(volatile s32*)&D_80068A78 = 0;
    return -1;
}

s32 SndEvt_EnqueueType6(s32 arg0, s32 arg1, s32 arg2)
{
    /// Empty-slot value in a sound-script bank image's entry-offset table.
    ///
    /// `SndBankHdr::entryOffsets` stores a byte offset from the image start to
    /// each slot's `oneC` block. Zero means that slot has no script, so a start
    /// request that reads it is rejected before an entry pointer is formed.
    /// Several slots may share one nonzero offset. Callers still need a
    /// completed `hONE` image; this comparison only interprets the stored offset.
    enum { SOUND_BANK_ENTRY_ABSENT = 0 };
    s32                     orig;
    SndBankSlot*            bankSlot;
    SndBankHdr*             header;
    SndScriptEntryControls* entry;
    u16                     offset;
    u32                     index;
    SndEvt*                 event;
    SndEvtScriptArgs*       args;

    orig = arg0;
    if ((arg0 == 0) || (arg0 == 8)) {
        return orig;
    }
    if (D_800689E4 != 0xFF) {
        if ((D_800689E4 & SOUND_BANK_TYPE_MASK) == (((u32)arg0 >> 16) & SOUND_BANK_TYPE_MASK)) {
            return -1;
        }
    }
    arg0     = _sndScriptRemapType1Id(arg0);
    bankSlot = _sndBankSlotFind((u32)arg0 >> 16, SOUND_BANK_SLOT_MATCH_ID);
    if (bankSlot == NULL) {
        return -2;
    }
    index  = (u32)arg0 & 0xFF;
    header = bankSlot->image;
    if (index >= header->entryCount) {
        return -2;
    }
    offset = *(header->entryOffsets + index);
    if (offset == SOUND_BANK_ENTRY_ABSENT) {
        return -3;
    }
    // A nonzero offset addresses the slot's oneC block within this loaded image.
    entry = (SndScriptEntryControls*)((u8*)header + offset);
    if (D_800689EC != 0) {
        if ((entry->flags & SOUND_SCRIPT_REJECT_WHILE_MUTED) != 0) {
            return -5;
        }
    }
    if (D_80082138[(u32)arg0 >> 28] == 0) {
        if ((entry->flags & SOUND_SCRIPT_ALLOW_DISABLED_TYPE) == 0) {
            return -4;
        }
    }
    event = sndEvtAlloc();
    if (event == NULL) {
        return -1;
    }
    event->command          = SOUND_EVENT_SCRIPT_START;
    args                    = &event->args.script;
    args->soundId           = arg0;
    args->panOffset         = arg1;
    args->level.attenuation = arg2;
    args->bankSlot          = bankSlot;
    args->entryControls     = entry;
    sndEvtEnqueue(event);
    return orig;
}

void SndEvt_EnqueueType7(s32 arg0, s32 arg1)
{
    SndEvt*           event;
    SndEvtScriptArgs* args;

    event = sndEvtAlloc();
    if (event != NULL) {
        event->command    = SOUND_EVENT_SCRIPT_STOP;
        args              = &event->args.script;
        args->soundId     = _sndScriptRemapType1Id(arg0);
        args->stopControl = arg1;
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

void SndEvt_EnqueueTypeA(s32 arg0, s32 arg1, s32 arg2)
{
    SndEvt*           event;
    SndEvtScriptArgs* args;

    if (D_80082138[(u32)arg0 >> 28] != 0) {
        event = sndEvtAlloc();
        if (event != NULL) {
            event->command          = SOUND_EVENT_SCRIPT_SET_PAN_ATTENUATION;
            args                    = &event->args.script;
            args->soundId           = _sndScriptRemapType1Id(arg0);
            args->panOffset         = arg1;
            args->level.attenuation = arg2;
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
    SndScript* p;
    s32        i;
    s32        group;
    s32        ret;

    if (!(arg0 & 0xFF)) {
        for (i = 0; i < 8; i++) {
            p     = &SndScript_Slots[i];
            group = p->field_0 & 0xF0000000;
            if ((group == arg0) || ((arg0 == 0x80000000) && (group != 0x60000000))) {
                if ((p->field_16 != 4) && (p->field_16 != 0)) {
                    p->field_C  = (arg1 == 1);
                    p->field_16 = 4;
                }
            }
        }
        return -2;
    }

    ret = 0;
    for (i = 0; i < 8; i++) {
        p = &SndScript_Slots[i];
        if ((p->field_0 == arg0) || ((p->field_0 | 0xFF00) == arg0)) {
            switch (p->field_16) {
                case 2:
                    if (arg1 != 0) {
                        if (arg1 != 1) {
                            LinInterp_Setup(&p->field_50, (u8)D_80082748, 0, arg1);
                            p->field_16 = 0x80;
                            break;
                        }
                        p->field_C = arg1;
                    }
                    /* fallthrough */
                case 4:
                case 8:
                case 16:
                    p->field_16 = 4;
                    break;
                case 1:
                    p->field_16 = 0;
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

    var_a0 = SndVoice_GetMasterVolume();
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
    SndVoice_ApplyMasterVolume(var_a0);
}

static s32 SndVoice_DriveSlots(s32* unused)
{
    SpuVoiceRef   ref;
    s16           vol[2];
    SpuVoiceAttr* attr;
    SndScript*    p;
    SndVoice*     node;
    SndVoice*     voice;
    s32           i;
    s32           count;
    s32           level;
    s32           temp;
    s8            step;
    s16           pan;

    if (D_8008274A != 0) {
        SndVoice_StepMasterLevel();
    }

    for (i = 0; i < 8; i++) {
        p = &SndScript_Slots[i];
        switch (p->field_16) {
            case 0:
                break;

            case 1:
                p->field_C          = 0;
                p->field_D          = 0;
                p->field_8          = 0;
                p->field_4          = 0;
                p->field_40         = NULL;
                p->field_16         = 2;
                p->field_50.field_E = 0;
                p->field_12         = 0;
                p->field_15         = 0;
                p->field_E          = 0;
                goto run;

            case 0x80:
                if (p->field_50.field_0 == p->field_50.field_4) {
                    p->field_16 = 4;
                    goto stop;
                }
                LinInterp_Step(&p->field_50);
                p->field_E = 1;
                /* fallthrough */
            case 2:
            run:
                p->field_4++;
                while (SndScript_Exec(p) != 0) {
                }
            update:
                count = 0;
                if (p->field_40 != NULL) {
                    node = p->field_40;
                    do {
                        SndVoice_Tick(node);
                        step = p->field_12;
                        count++;
                        if (step != 0) {
                            level = step + (s8)p->field_10 * 4;
                            if (step > 0) {
                                if ((s8)p->field_11 * 4 < level) {
                                    p->field_10 = p->field_11;
                                    p->field_12 = 0;
                                } else {
                                    /* level / 4, rounded toward zero */
                                    temp = level;
                                    if (temp < 0) {
                                        temp += 3;
                                    }
                                    p->field_10 = temp >> 2;
                                }
                            } else if (level < (s8)p->field_11 * 4) {
                                p->field_10 = p->field_11;
                                p->field_12 = 0;
                            } else {
                                temp = level;
                                if (temp < 0) {
                                    temp += 3;
                                }
                                p->field_10 = temp >> 2;
                            }
                            p->field_E = 1;
                        }
                        step = p->field_15;
                        if (step != 0) {
                            pan = (s8)p->field_13 + step;
                            if (step > 0) {
                                if ((s8)p->field_14 < pan) {
                                    p->field_13 = p->field_14;
                                    p->field_15 = 0;
                                } else {
                                    p->field_13 = pan;
                                }
                            } else if (pan < (s8)p->field_14) {
                                p->field_13 = p->field_14;
                                p->field_15 = 0;
                            } else {
                                p->field_13 = pan;
                            }
                            p->field_E = 1;
                        }
                        if (p->field_E == 1) {
                            Spu_GetVoiceRef(node->field_0, &ref);
                            attr = ref.field_4;
                            SndVoice_ScaleVolume(p->field_10, p->field_13, node, &p->field_50, vol);
                            attr->volume.left   = vol[0];
                            attr->volume.right  = vol[1];
                            attr->volmode.left  = 0;
                            attr->volmode.right = 0;
                            attr->mask         |= 0xF;
                        }
                        node = node->field_3C;
                    } while (node != NULL);
                    p->field_E = 0;
                }
                if (count == 0 && p->field_D == 1) {
                    goto release;
                }
                break;

            case 8:
                LinInterp_Step(&p->field_50);
                p->field_E = 1;
                goto update;

            case 0x10:
                p->field_E = 1;
                LinInterp_Step(&p->field_50);
                if (p->field_50.field_0 == p->field_50.field_4) {
                    p->field_16 = 2;
                    goto run;
                }
                goto update;

            case 4:
            stop:
                p->field_D = 1;
                if (SndScript_TickVoices(p) != 0) {
                    p->field_16 = 0x20;
                    break;
                }
                goto release;

            case 0x20:
                count = 0;
                if (p->field_40 != NULL) {
                    node = p->field_40;
                    do {
                        if (node->field_10.field_0 != 0) {
                            count++;
                            SndVoice_TickEnvelope(node);
                        }
                        node = node->field_3C;
                    } while (node != NULL);
                }
                if (count == 0 && p->field_D == 1) {
                release:
                    p->field_D  = 0;
                    p->field_0  = -1;
                    p->field_16 = 0;
                    for (voice = p->field_40; voice != NULL; voice = voice->field_3C) {
                        voice->field_34 = 0;
                    }
                    p->field_40         = NULL;
                    p->field_50.field_E = 0;
                }
                break;
        }
    }
    return 0;
}

static void SndVoice_ScanCandidates(SndVoicePick* candidates, u16 arg1, s32 arg2, u16 arg3)
{
    s8         i;
    SndScript* p;
    u16        temp;
    s32        score;

    candidates->field_0  = -1;
    candidates->field_10 = -1;
    candidates->field_5  = -1;
    candidates->field_14 = -1;
    candidates->field_6  = -1;
    candidates->field_4  = -1;
    candidates->field_3  = -1;
    candidates->field_1  = -1;
    candidates->field_2  = -1;
    candidates->field_8  = arg1;
    candidates->field_C  = 0xFFFF;
    candidates->field_7  = 0;

    for (i = 0; i < 8; i++) {
        p = &SndScript_Slots[i];
        if (p->field_16 == 0) {
            candidates->field_3 = i;
        } else if (p->field_16 != 4) {
            temp = p->field_4C->priority;
            if (temp < (u32)candidates->field_8) {
                candidates->field_8 = temp;
                candidates->field_4 = i;
            } else if (candidates->field_8 == temp) {
                if ((candidates->field_5 == -1) || (candidates->field_10 < p->field_4)) {
                    score                = p->field_4;
                    candidates->field_5  = i;
                    candidates->field_10 = score;
                }
            }
            if (((p->field_0 & 0xFFFF00FF) == (arg2 & 0xFFFF00FF)) ||
                (((temp = p->field_4C->flags) & SOUND_SCRIPT_GROUP_BY_FLAGS) && (arg3 == temp))) {
                candidates->field_1 = i;
                if ((candidates->field_2 == -1) || (candidates->field_C > p->field_4)) {
                    score               = p->field_4;
                    candidates->field_2 = i;
                    candidates->field_C = score;
                }
                candidates->field_7 += 1;
                if ((candidates->field_6 == -1) || (candidates->field_14 < p->field_4)) {
                    score                = p->field_4;
                    candidates->field_6  = i;
                    candidates->field_14 = score;
                }
            }
        }
    }
}

void SndVoice_KeyOffMatching(void)
{
    SpuVoiceRef ref;
    s32         i;
    SndScript*  p;
    SndVoice*   head;
    SndVoice*   node;
    s32         type;
    s32         emptyType;

    for (i = 0; i < 8; i++) {
        p    = &SndScript_Slots[i];
        head = p->field_40;
        if (head != NULL) {
            type = p->field_0 & 0xF0000000;
            if (type != 0x60000000) {
                node = head;
                if ((type == 0x10000000) || (type == 0x50000000)) {
                    do {
                        Spu_GetVoiceRef(p->field_40->field_0, &ref);
                        ref.field_4->adsr2  = (ref.field_4->adsr2 & 0xFFE0) | 0xB;
                        ref.field_4->adsr2 |= 0x20;
                        ref.field_4->mask  |= SPU_VOICE_ADSR_ADSR2;
                        Spu_KeyOff(node->field_0);
                        node = node->field_3C;
                    } while (node != NULL);
                    p->field_16 = 0;
                    p->field_0  = -1;
                }
            }
        } else {
            emptyType = p->field_0 & 0xF0000000;
            if ((emptyType == 0x50000000) || (emptyType == 0x10000000)) {
                p->field_16 = 0;
                p->field_0  = -1;
            }
        }
    }
}

/// Advances a script's 16.16 tick clock by one step: a whole tick, or 0.6 of
/// one when the display region is 1.
static inline void _sndScriptAdvanceClock(SndScript* script)
{
    script->field_8 += (gDisplayState.region == MODE_PAL ? 0x9999 : 0x10000);
}

/// Decides whether a note plays with reverb, from its own level against a
/// global one. A note at level 3 gets reverb whenever the global level is at
/// least 2; a global level of 3 turns it off for every other note; otherwise a
/// note with a non-negative level gets reverb once the global level reaches it.
static inline u8 _sndScriptUseReverb(SndOneV* oneV)
{
    s32 on;

    if (oneV->field_E == 3 && D_8008274B >= 2) {
        on = 1;
    } else if (oneV->field_E != 3 && D_8008274B == 3) {
        on = 0;
    } else {
        on = 0;
        if (oneV->field_E >= 0) {
            on = D_8008274B >= oneV->field_E;
        }
    }
    return on;
}

static s32 SndScript_Exec(SndScript* script)
{
    enum {
        SOUND_BANK_PAN_CENTER        = 64,
        SOUND_BANK_PAN_MAX           = 127,
        SOUND_BANK_KEY_FRACTION_BITS = 7
    };
    SpuVoiceRef   voiceRef;
    s16           volume[2];
    SndScriptCmd* cmd;
    SndOneV*      oneV;
    SndVoice*     voice;
    SndBankLayer* bankLayer;
    SpuVoiceAttr* attr;
    SndBankSlot*  bankSlot;
    SndBank*      bank;
    SndBankHdr*   header;
    s32           result;
    s32           ticks;
    s32           wait;
    s32           index;
    s32           masterVolume;
    u8            layerVolume;
    s32           panSum;
    s16           pan;
    s16           voicePan;
    s32           pitchValue;
    u16           pitch;
    s32           countdown;
    s16           envelopeOffset;

    cmd = (SndScriptCmd*)script->field_48;
    switch ((u32)cmd->magic) {
        case 0x45656E6F:
            break;
        case 0x43646E65:
        stop:
            script->field_D = 1;
            break;
        case 0x706F6F4C:
            if (script->field_17 >= 8U) {
                goto stop;
            }
            ticks = script->field_8;
            if ((ticks >> 16) >= cmd->data.loop.field_6) {
                script->field_18[script->field_17] = cmd->data.loop.field_4;
                script->field_48                   = script->field_48 + sizeof(SndScriptCmd);
                script->field_20[script->field_17] = script->field_48;
                script->field_17++;
                script->field_8 -= cmd->data.loop.field_6 << 16;
                result           = 1;
                goto done;
            } else {
                _sndScriptAdvanceClock(script);
            }
            break;
        case 0x4C646E65:
            if (script->field_17 == 0) {
                goto stop;
            }
            index = script->field_17 - 1;
            if (script->field_18[index] == 1) {
                script->field_48 = (u8*)cmd + sizeof(cmd->magic);
                script->field_17--;
            } else {
                script->field_48 = script->field_20[index];
                if (script->field_18[script->field_17 - 1] != 0) {
                    script->field_18[script->field_17 - 1]--;
                }
            }
            result = 1;
            goto done;
        case SOUND_SCRIPT_ENTRY_TAG:
            header = script->field_44->image;
            // Reload this entry's controls before executing its first voice command.
            script->field_4C = (SndScriptEntryControls*)((u8*)header + *(header->entryOffsets + (u8)script->field_0));
            script->field_48 = script->field_48 + sizeof(SndScriptEntryControls);
        case 0x56656E6F:
            oneV  = (SndOneV*)script->field_48;
            ticks = script->field_8;
            if ((ticks >> 16) < oneV->field_8) {
                _sndScriptAdvanceClock(script);
                result = 0;
                goto done;
            }
            voice  = SndVoice_Alloc(oneV->field_10);
            result = 1;
            if (voice != NULL) {
                bankSlot = script->field_44;
                if (oneV->field_4 != 0) {
                    bank = Snd_FindBank(oneV->field_4);
                    if (bank == 0) {
                        voice->field_8 = 0;
                        Spu_ReleaseVoiceSlot(voice->field_0);
                        Spu_ClearVoiceCallbacks(voice->field_0);
                        voice->field_0 = 0;
                        return 0;
                    }
                    goto setup_voice;
                }
                bank = bankSlot->bank;
            setup_voice:
                Spu_GetVoiceRef(voice->field_0, &voiceRef);
                bankLayer    = Snd_GetNote(bank, (u8)oneV->field_6, oneV->field_7);
                attr         = voiceRef.field_4;
                masterVolume = D_80082748;
                attr->addr   = bankLayer->waveAddr;
                if ((D_80082749 != 0) && (script->field_4C->flags & SOUND_SCRIPT_USE_UNDUCKED_VOLUME)) {
                    masterVolume = D_80082749;
                }
                layerVolume = (u8)oneV->field_D;
                if (oneV->field_D < 0) {
                    layerVolume = bankLayer->volume;
                }
                voice->field_A = layerVolume;
                voice->field_2 = (s8)((masterVolume * script->field_4C->volumeScale * voice->field_A) / (SOUND_SCRIPT_VOLUME_UNITY * SOUND_SCRIPT_VOLUME_UNITY));
                pan            = script->field_4C->panBias;
                panSum         = oneV->field_C;
                if (panSum < 0) {
                    panSum = bankLayer->pan;
                }
                panSum  += (s16)(pan - SOUND_BANK_PAN_CENTER);
                voicePan = panSum;
                if (voicePan <= SOUND_BANK_PAN_MAX) {
                    if (voicePan >= 0) {
                        voice->field_3 = panSum;
                    } else {
                        voice->field_3 = 0;
                    }
                } else {
                    voice->field_3 = SOUND_BANK_PAN_MAX;
                }
                if (SndScript_FindOneA((u8*)script->field_44->image, oneV->field_12, attr) == -1) {
                    attr->adsr1 = bankLayer->adsr1;
                    attr->adsr2 = bankLayer->adsr2;
                }
                // Script pitch is a Q7 offset from the layer's minimum key.
                pitchValue = pitch = oneV->field_14 + (bankLayer->keyMin << SOUND_BANK_KEY_FRACTION_BITS);
                attr->pitch        = Spu_CalcVolume((u32)(pitch & 0xFFFF) >> SOUND_BANK_KEY_FRACTION_BITS, (pitchValue & 0x7F) * 2, bankLayer->rootKey, bankLayer->fineTune);
                if (_sndScriptUseReverb(oneV) == 0) {
                    Spu_DisableReverbVoice(voice->field_0);
                    voice->field_1 = 1;
                } else {
                    Spu_EnableReverbVoice(voice->field_0);
                    voice->field_1 = 1;
                }
                SndVoice_ScaleVolume(script->field_10, script->field_13, voice, &script->field_50, volume);
                attr->volume.left   = volume[0];
                attr->volume.right  = volume[1];
                attr->volmode.left  = 0;
                attr->volmode.right = 0;
                attr->mask          = 0x6009F;
                Spu_KeyOn(voice->field_0);
                voice->field_C = oneV;
                countdown      = oneV->field_A == 0 ? 0x7FFFFFFF : oneV->field_A << 16;
                voice->field_4 = countdown;
                SndVoice_Attach(script, voice);
                envelopeOffset = oneV->field_16;
                if (envelopeOffset != -1) {
                    SndVoice_SetupEnvelope(voice, envelopeOffset, pitch & 0xFFFF, bankLayer);
                    result = 1;
                } else {
                    voice->field_10.field_0 = 0;
                    result                  = 1;
                }
            }
            script->field_8  = (s32)(script->field_8 - (oneV->field_8 << 0x10));
            script->field_48 = script->field_48 + sizeof(SndOneV);

            goto done;
        case 0x74696157:
            ticks = script->field_8;
            wait  = cmd->data.duration;
            if ((ticks >> 16) < wait) {
                _sndScriptAdvanceClock(script);
                result = 0;
                goto done;
            }
            script->field_8  = ticks - (wait << 16);
            script->field_48 = script->field_48 + sizeof(SndScriptCmd);
            result           = 1;
            goto done;
        case 0x41656E6F:
        default:
            result = 0;
            goto done;
    }
    result = 0;
done:
    return result;
}

static void SndVoice_TickEnvelope(SndVoice* voice)
{
    SpuVoiceRef   sp10;
    SndVoiceFx*   fx;
    SndOneE*      chunk;
    s32           pitch;
    s32           temp;
    s32           level;
    SpuVoiceAttr* attr;

    fx    = &voice->field_10;
    chunk = fx->field_20;

    if (fx->field_2 == 1) {
        fx->field_1 = 5;
        temp        = (fx->field_10 - chunk->field_14) * chunk->field_16;
        if (temp > 0) {
            fx->field_E = -chunk->field_16;
        } else {
            fx->field_E = chunk->field_16;
        }

        fx->field_C  = 0;
        fx->field_2  = 2;
        fx->field_1C = fx->field_10;
    }

    switch (fx->field_1) {
        case 0:
            if (fx->field_C < chunk->field_4) {
                fx->field_C++;
                break;
            }
            fx->field_1  = 1;
            fx->field_C  = 0;
            fx->field_14 = 0;
            fx->field_10 = 0;
        case 1:
            pitch = (fx->field_4 << 1) + fx->field_14;
            if (fx->field_C < chunk->field_A) {
                fx->field_C++;
                fx->field_10 = fx->field_14 += chunk->field_8;
                goto apply;
            }
            fx->field_1 = 2;
            fx->field_C = 0;
        case 2:
            pitch = (fx->field_4 << 1) + chunk->field_6;
            if (fx->field_C < chunk->field_C) {
                fx->field_C++;
                goto apply;
            }
            fx->field_1  = 3;
            fx->field_18 = chunk->field_6;
            level        = chunk->field_6;
            fx->field_C  = 0;
            fx->field_10 = level;
        case 3:
            pitch = (fx->field_4 << 1) + fx->field_18;
            if (fx->field_C < chunk->field_E) {
                fx->field_C++;
                fx->field_10 = fx->field_18 += chunk->field_10;
                goto apply;
            }
            fx->field_1 = 4;
        case 4:
            pitch = (fx->field_4 << 1) + chunk->field_12;
            goto apply;
        case 5:
            temp = (fx->field_10 - chunk->field_14) * chunk->field_16;
            if (temp >= 0) {
                fx->field_1 = 6;
            } else {
                fx->field_10 = fx->field_1C += fx->field_E;
            }
            pitch = (fx->field_4 << 1) + fx->field_1C;
            goto apply;
        case 6:
            pitch = (fx->field_4 << 1) + chunk->field_14;
            goto apply;
        default:
            break;
    }
    return;

apply:
    Spu_GetVoiceRef(voice->field_0, &sp10);
    attr = sp10.field_4;
    attr->pitch =
        Spu_CalcVolume((pitch >> 8) & 0xFFFF, pitch & 0xFF, (u16)fx->field_8, (u16)fx->field_A);
    attr->mask |= SPU_VOICE_PITCH;
}

s32 SndVoice_AllocSlot(s32 arg0, s8 arg1, s8 arg2, SndBankSlot* slot, SndScriptEntryControls* entryControls)
{
    SndVoicePick sp18;

    SndVoice_ScanCandidates(&sp18, entryControls->priority, arg0, entryControls->flags);
    if ((sp18.field_7 < entryControls->maxInstances) && (sp18.field_3 != -1)) {
        sp18.field_0 = sp18.field_3;
    } else {
        sp18.field_0 = SndVoice_SelectStealCandidate(&sp18, entryControls->retriggerTicks);
    }
    if (sp18.field_0 >= 0) {
        SndScript_Play(sp18.field_0, arg1, arg2, arg0, slot, entryControls);
    }
    return sp18.field_0;
}

void SndVoice_FadeMatching(s32 arg0, s32 arg1)
{
    s32        i;
    SndScript* p;

    for (i = 0; i < 8; i++) {
        p = &SndScript_Slots[i];
        if ((arg0 == p->field_0) || ((p->field_0 & 0xF0000000) == arg0)) {
            if (arg1 == 0) {
                if (p->field_16 == 8) {
                    p->field_16 = 0x10;
                    LinInterp_Setup(&p->field_50, 0, (u8)D_80082748, 8);
                }
            } else {
                if (p->field_16 & 0x22) {
                    p->field_16 = 8;
                    LinInterp_Setup(&p->field_50, (u8)D_80082748, 0, 8);
                }
            }
        }
    }
}

void SndVoice_SetPanRamp(s32 arg0, s32 arg1, s32 arg2)
{
    SndScript* p;
    s32        t;

    arg0       &= 7;
    p           = &SndScript_Slots[arg0];
    t           = *(volatile u8*)&p->field_10;
    t           = arg1 - t;
    p->field_12 = t;
    t           = (s8)t;
    if (t < 0) {
        t = -t;
    }
    if ((t * 4) >= 0x21) {
        p->field_11 = arg1;
        if (p->field_12 <= 0) {
            if (p->field_12 < 0) {
                t = -8;
            } else {
                t = 0;
            }
        } else {
            t = 8;
        }
        p->field_12 = t;
    } else {
        p->field_10 = arg1;
        p->field_12 = 0;
    }
    p->field_E = 1;

    arg1 = (s8)arg2;
    if (arg1 < 0) {
        arg1 = arg1 + 0x7F;
    } else {
        arg1 = 0x7F - arg1;
    }
    SndVoice_SetVolumeRamp(arg0, arg1);
    p->field_E = 1;
}

void SndVoice_SetVolumeRamp(s32 arg0, s32 arg1)
{
    SndScript* p;
    u8         vol;
    s16        diff;

    p     = &SndScript_Slots[arg0 & 7];
    diff  = ~arg1 & 0x7F;
    vol   = diff;
    diff -= (s8)p->field_13;
    if (ABS(diff) > 0x20) {
        p->field_14 = vol;
        if (diff <= 0) {
            if (diff < 0) {
                p->field_15 = -8;
            } else {
                p->field_15 = 0;
            }
        } else {
            p->field_15 = 8;
        }
    } else {
        p->field_13 = vol;
        p->field_15 = 0;
    }
    p->field_E = 1;
}

void SndVoice_IncRefCount(void)
{
    s8 temp;

    D_8008274C += 1;
    if (D_8008274C == 1) {
        if (D_8008274A == 0) {
            if (D_80082749 == 0) {
                temp = SndVoice_GetMasterVolume();
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
    } while (i < 0xC0U);

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
    } while (i < 0x80U);

    D_8008274A = 0;
    D_80082749 = 0;
    SndVoice_ApplyMasterVolume(0x7F);
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
    s32        i;
    SndScript* p;

    i = 0;
    p = SndScript_Slots;
    do {
        if ((p->field_16 & 0xA3) && (p->field_0 == arg0)) {
            return i;
        }
        i++;
        p++;
    } while (i < 8);
    return -1;
}

void SndVoice_ApplyMasterVolume(s8 arg0)
{
    SndScript* p;
    s32        i;
    SndVoice*  node;
    SndVoice*  temp;
    s8         vol;

    for (i = 0; i < 8; i++) {
        p = &SndScript_Slots[i];
        if ((p->field_F != 1) || (D_80082749 == 0)) {
            temp = p->field_40;
            if (temp != NULL) {
                node = temp;
                do {
                    node->field_2 = (arg0 * p->field_4C->volumeScale * node->field_A) / (SOUND_SCRIPT_VOLUME_UNITY * SOUND_SCRIPT_VOLUME_UNITY);
                    node          = node->field_3C;
                } while (node != NULL);
                p->field_E = 0;
            }
            p->field_E = 1;
        }
    }
    vol = arg0;
    if (vol < 0) {
        vol = 0;
    }
    D_80082748 = vol;
}

s8 SndVoice_GetMasterVolume(void)
{
    return D_80082748;
}

static s8 SndVoice_SelectStealCandidate(SndVoicePick* candidates, s32 retriggerTicks)
{
    s32 v;
    u8  u;
    s32 none;

    none = -1;
    if (retriggerTicks == SOUND_SCRIPT_RETRIGGER_DISABLED) {
        return -9;
    }
    if (candidates->field_C < retriggerTicks) {
        return -5;
    }
    if (candidates->field_2 != none) {
        goto field6;
    }
    v = candidates->field_4;
    u = candidates->field_4;
    if (v != none) {
        goto store;
    }
    v = candidates->field_5;
    u = candidates->field_5;
join:
    if (v == none) {
        goto ret_m6;
    }
store:
    candidates->field_0 = u;
    return v;
field6:
    v = candidates->field_6;
    u = candidates->field_6;
    goto join;
ret_m6:
    return -6;
}

static void SndScript_Play(s32 arg0, s8 arg1, s8 arg2, s32 arg3, SndBankSlot* slot, SndScriptEntryControls* entryControls)
{
    SndScript*              p;
    SndVoice*               node;
    SndScriptEntryControls* controls;
    u16                     flags;

    controls = entryControls;
    p        = &SndScript_Slots[arg0];
    node     = p->field_40;
    if (node != NULL) {
        do {
            Spu_KeyOff(node->field_0);
            node->field_8 = 0;
            Spu_ClearVoiceCallbacks(node->field_0);
            Spu_ReleaseVoiceSlot(node->field_0);
            node->field_0 = 0;
            node          = node->field_3C;
        } while (node != NULL);
    }
    p->field_16 = 1;
    p->field_40 = NULL;
    p->field_44 = slot;
    p->field_0  = arg3;
    p->field_4  = 0;
    p->field_10 = arg1;
    p->field_13 = arg2;
    p->field_17 = 0;
    flags       = controls->flags;
    p->field_48 = (u8*)entryControls;
    p->field_F  = (flags & SOUND_SCRIPT_USE_UNDUCKED_VOLUME) != 0;
}

static void SndVoice_Detach(void* context)
{
    SndVoice* arg0 = context;
    union {
        SndVoice*  voice;
        SndScript* script;
    } temp_v0;
    SndVoice* temp_v1;

    if (arg0 != NULL) {
        temp_v0.voice = arg0->field_38;
        arg0->field_8 = 0;
        arg0->field_0 = 0;
        if (temp_v0.voice == NULL) {
            temp_v1 = arg0->field_3C;
            if (temp_v1 == NULL) {
                temp_v0.script = arg0->field_34;
                if (temp_v0.script != NULL) {
                    temp_v0.script->field_40 = NULL;
                }
            } else {
                temp_v0.script = arg0->field_34;
                if (temp_v0.script != NULL) {
                    temp_v0.script->field_40 = temp_v1;
                }
                temp_v0.voice           = arg0->field_3C;
                temp_v0.voice->field_38 = NULL;
            }
        } else {
            temp_v1 = arg0->field_3C;
            if (temp_v1 == NULL) {
                temp_v0.voice->field_3C = NULL;
            } else {
                temp_v0.voice->field_3C = temp_v1;
                temp_v1                 = arg0->field_3C;
                temp_v0.voice           = arg0->field_38;
                temp_v1->field_38       = temp_v0.voice;
            }
        }
        arg0->field_38 = NULL;
        arg0->field_3C = NULL;
    }
}

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

SndBankSlot* SndBankSlot_Get(s32 arg0)
{
    if ((u8)arg0 < ARRAY_SIZE(_gSndBankSlots)) {
        return &_gSndBankSlots[(s8)arg0];
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
        SndHeap_Free(slot->image);
        slot->bankId = SOUND_BANK_SLOT_ID_FREE;
        slot->image  = NULL;
    }
}

static SndVoice* SndVoice_Alloc(s32 arg0)
{
    s32       voiceIdx;
    SndVoice* ptr;

    voiceIdx = (s8)Spu_AllocVoice(SndScript_VoiceRanges, 2, arg0 & 0xFFFF);
    if (voiceIdx < 0) {
        return NULL;
    }
    /* Ranges 1 and 2 cover hardware voices 16..23. */
    ptr          = &SndScript_Voices[voiceIdx - 16];
    ptr->field_0 = voiceIdx;
    Spu_SetVoiceCallbacks(voiceIdx, SndVoice_Detach, ptr);
    ptr->field_8 = 1;
    return ptr;
}

static void SndVoice_Attach(SndScript* arg0, SndVoice* voice)
{
    SndVoice* temp_v0;

    if (arg0 != NULL) {
        temp_v0 = arg0->field_40;
        if (temp_v0 != NULL) {
            arg0->field_40    = voice;
            voice->field_3C   = temp_v0;
            temp_v0->field_38 = voice;
            voice->field_38   = NULL;
            voice->field_34   = arg0;
            return;
        }
        arg0->field_40  = voice;
        voice->field_34 = arg0;
        voice->field_3C = NULL;
        voice->field_38 = NULL;
        return;
    }
    voice->field_3C = NULL;
    voice->field_38 = NULL;
    voice->field_34 = NULL;
}

static s32 SndVoice_Tick(SndVoice* voice)
{
    s32 temp;

    temp = voice->field_4;
    if (temp <= 0) {
        voice->field_4 = 0;
        Spu_KeyOff(voice->field_0);
        if (voice->field_10.field_0 != 0) {
            if (voice->field_10.field_2 == 0) {
                voice->field_10.field_2 = 1;
            }
            goto block_8;
        }
    } else {
        if (temp <= 0x7FFFFFFE) {
            if (gDisplayState.region == MODE_PAL) {
                voice->field_4 = temp + 0xFFFF6667;
            } else {
                voice->field_4 = temp + 0xFFFF0000;
            }
        }
    block_8:
        if (voice->field_10.field_0 != 0) {
            SndVoice_TickEnvelope(voice);
        }
    }
    return 0;
}

static s32 SndScript_TickVoices(SndScript* script)
{
    SpuVoiceRef sp10;
    SndVoice*   node;
    SndVoice*   head;
    s32         count;
    u8          status;
    u16         temp;

    head  = script->field_40;
    count = 0;
    if (head != NULL) {
        node = head;
        do {
            if (node->field_0 >= 0) {
                if (script->field_C != 1) {
                    status = Spu_GetVoiceStatus(node->field_0);
                    if (status != 0) {
                        Spu_GetVoiceRef(node->field_0, &sp10);
                        temp                = sp10.field_4->adsr2;
                        temp                = (temp & 0xFFE0) | 5;
                        sp10.field_4->adsr2 = temp;
                        sp10.field_4->mask |= SPU_VOICE_ADSR_ADSR2;
                        if (status != 2) {
                            Spu_KeyOff(node->field_0);
                        }
                    }
                } else {
                    Spu_KeyOff(node->field_0);
                }
                if (node->field_10.field_0 != 0) {
                    count                 += 1;
                    node->field_10.field_2 = 1;
                }
            }
            node = node->field_3C;
        } while (node != NULL);
    }
    return count;
}

static void SndVoice_ScaleVolume(s8 arg0, s8 arg1, SndVoice* voice, LinInterp* arg3, s16* arg4)
{
    s32 vol;

    if (voice->field_0 >= 0) {
        vol = 0x7F - abs(arg1);
        vol = voice->field_2 * abs(vol) / 127;
        vol = (vol < 0x80) ? ((vol < 0) ? 0 : vol) : 0x7F;
        Spu_ApplyPanVolume(arg4, (s8)voice->field_3 + arg0 * 3,
                           LinInterp_Apply(arg3, Snd_VelocityGainTable[vol]));
    }
}

static void SndVoice_SetupEnvelope(SndVoice* voice, s16 envelopeOffset, u32 pitch, SndBankLayer* bankLayer)
{
    SndVoiceFx* p;
    u8*         base;
    SndOneE*    chunk;
    s32         magic;
    s16         temp;

    p = &voice->field_10;
    if (envelopeOffset == -1) {
        voice->field_10.field_0 = 0;
        return;
    }
    if (voice->field_34 == NULL) {
        voice->field_10.field_0 = 0;
        return;
    }
    base        = (u8*)voice->field_34->field_44->image;
    chunk       = (SndOneE*)&base[envelopeOffset];
    p->field_20 = chunk;
    magic       = chunk->magic;
    if (magic == 0x45656E6F) {
        voice->field_10.field_0 = 1;
        p->field_1              = 0;
        p->field_2              = 0;
        p->field_4              = pitch & 0xFFFF;
        p->field_8              = bankLayer->rootKey;
        temp                    = bankLayer->fineTune;
        p->field_C              = 0;
        p->field_14             = 0;
        p->field_18             = 0;
        p->field_1C             = 0;
        p->field_A              = temp;
    }
}

static s32 SndScript_FindOneA(u8* arg0, s16 arg1, SpuVoiceAttr* arg2)
{
    SndOneA* chunk;

    if (arg1 != -1) {
        chunk = (SndOneA*)&arg0[arg1];
        if (chunk->field_0 == 0x41656E6F) {
            arg2->adsr1 = chunk->field_4;
            arg2->adsr2 = chunk->field_6;
            return 1;
        }
        return -1;
    }
    return -1;
}

static void SndVoice_ClearActive(void)
{
    s32        i;
    s32        mask;
    s32        c600;
    s32        c500;
    s32        c100;
    SndScript* p;
    s32        temp;

    i    = 0;
    mask = 0xF0000000;
    c600 = 0x60000000;
    c500 = 0x50000000;
    c100 = 0x10000000;
    p    = SndScript_Slots;
    do {
        temp = p->field_0 & mask;
        if (temp != c600) {
            if ((temp == c500) || (temp == c100)) {
                p->field_16 = 0;
            }
        }
        i++;
        p++;
    } while (i < 8);
}
