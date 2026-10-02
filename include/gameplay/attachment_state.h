#ifndef GAMEPLAY_ATTACHMENT_STATE_H
#define GAMEPLAY_ATTACHMENT_STATE_H

#include "common.h"

#include "gameplay/weapon_data.h"

/// 16-byte records selected by `Gp_GetIdParam2` when the id's 0x8000 bit is
/// set. Indexed by `id & 0x7F`.
/// Both row and byte-offset access address this complete table.
typedef union GpIdParamTable {
    GpRec16 rows[55];
    u8      bytes[55 * sizeof(GpRec16)];
} GpIdParamTable;
STATIC_ASSERT_SIZEOF(GpIdParamTable, 0x370);

/// Bits of `AttachmentState.flags`.
#define ATTACHMENT_FLAG_EVENT_LOCK  1
#define ATTACHMENT_FLAG_SWAP_LOCK   2
#define ATTACHMENT_FLAG_RELEASE     4
#define ATTACHMENT_FLAG_APPLY_STATS 8
#define ATTACHMENT_FLAG_OPEN_WHEEL  0x10

/// Clears one flag and leaves the rest of the byte. Each value is the mask
/// the clear sites store.
#define ATTACHMENT_FLAG_CLEAR_EVENT_LOCK  0xFE
#define ATTACHMENT_FLAG_CLEAR_SWAP_LOCK   0xFD
#define ATTACHMENT_FLAG_CLEAR_RELEASE     0xFB
#define ATTACHMENT_FLAG_CLEAR_APPLY_STATS 0xF7
#define ATTACHMENT_FLAG_CLEAR_OPEN_WHEEL  0xEF

/// `AttachmentState.mode`.
#define ATTACHMENT_MODE_IDLE  0
#define ATTACHMENT_MODE_WHEEL 1
#define ATTACHMENT_MODE_ARMED 2
#define ATTACHMENT_MODE_CAST  3

/// `AttachmentState.effectPhase`.
#define ATTACHMENT_EFFECT_HELD      -2
#define ATTACHMENT_EFFECT_CHARGE    -1
#define ATTACHMENT_EFFECT_IDLE      0
#define ATTACHMENT_EFFECT_RELEASED  1
#define ATTACHMENT_EFFECT_CANCELLED 2

/// `AttachmentState.soundStep`.
#define ATTACHMENT_SOUND_IDLE   0
#define ATTACHMENT_SOUND_QUEUED 1
#define ATTACHMENT_SOUND_PLAYED 2

/// `AttachmentState.menuOpen`.
#define ATTACHMENT_MENU_CLOSED 0
#define ATTACHMENT_MENU_OPEN   1

/// Wheel slots. The twelve spells are `0 .. ATTACHMENT_SPELL_COUNT - 1`.
#define ATTACHMENT_SPELL_COUNT       0xC
#define ATTACHMENT_INDEX_METABOLISM  6
#define ATTACHMENT_INDEX_HEALING     7
#define ATTACHMENT_INDEX_ENERGY_BALL 0xB

/// Shortest cast `duration`, in frames.
#define ATTACHMENT_DURATION_MIN 1

/// Low nibble of a combo byte is the stack; the high nibble is the level.
#define ATTACHMENT_COMBO_STACK_MASK  0xF
#define ATTACHMENT_COMBO_LEVEL_SHIFT 4
#define ATTACHMENT_COMBO_STACK_CAP   2

/// Packed attachment ids. The opening six spells are below 300, the twelve
/// spells end at 600, and item attachments start at 601.
#define ATTACHMENT_ID_EARLY_SPELL_LIMIT   300
#define ATTACHMENT_ID_EARLY_SPELL_LIMIT_U 0x12CU
#define ATTACHMENT_ID_LAST_SPELL          600
#define ATTACHMENT_ID_ITEM                0x259U

/// `attachId / 10` for the healing family (321–323) and energy ball (431–433).
#define ATTACHMENT_ID_HEALING_FAMILY     0x20
#define ATTACHMENT_ID_ENERGY_BALL_FAMILY 0x2B

#define ATTACHMENT_ID_METABOLISM_1  311
#define ATTACHMENT_ID_METABOLISM_2  312
#define ATTACHMENT_ID_METABOLISM_3  313
#define ATTACHMENT_ID_HEALING_1     321
#define ATTACHMENT_ID_HEALING_2     322
#define ATTACHMENT_ID_HEALING_3     323
#define ATTACHMENT_ID_ANTIBODY_1    411
#define ATTACHMENT_ID_ANTIBODY_2    412
#define ATTACHMENT_ID_ANTIBODY_3    413
#define ATTACHMENT_ID_ENERGY_SHOT_1 421
#define ATTACHMENT_ID_ENERGY_SHOT_2 422
#define ATTACHMENT_ID_ENERGY_SHOT_3 423

/// Parasite Energy attachment being aimed, cast, or still applying.
///
/// `attachId` packs the family in the tens and the level in the ones.
/// `activeIndex` is the spell in use and `wheelIndex` the spell highlighted
/// on the wheel. `queuedIndex` becomes both once the pad is idle, unless an
/// event lock or a scripted actor is holding the queue. Outside combat the
/// wheel highlights healing when that spell is learned.
///
/// Antibody and energy shot stacks saturate at 2; metabolism steps from 0 to
/// 1. Their tick counters freeze while the wheel is open and clear the combo
/// byte at 0. A running metabolism timer blocks the same statuses as both
/// wards together. `mindWard` blocks silence, confusion and berserker;
/// `bodyWard` blocks darkness, paralysis and poison.
typedef struct {
    u16  attachId;        // Packed spell id: tens are the family, ones the level
    s8   duration;        // Cast length in frames; also the gauge width
    s8   effectPhase;     // -2 held, -1 charging, 0 idle, 1 released, 2 cancelled
    byte field_4;         // Unread. Role unproven
    s8   activeIndex;     // Spell in use. Below 12 is a spell; 12 still draws the gauge and consumes an item
    u8   flags;           // Bit 0 event lock, 1 swap lock, 2 release, 3 apply stats, 4 force the wheel open
    s8   previewSound;    // Wheel preview sound; cleared once that play returns
    s8   soundStep;       // 0 idle, 1 queued, 2 played
    s8   menuOpen;        // 1 while the wheel is open
    s8   mode;            // 0 idle, 1 wheel open, 2 armed, 3 counting `duration` down
    s8   wheelIndex;      // Spell highlighted on the wheel
    s8   antibodyCombo;   // High nibble level, low nibble stack, saturating at 2
    s8   energyShotCombo; // High nibble level, low nibble stack, saturating at 2
    s8   queuedIndex;     // Spell or item slot waiting to become the active spell
    u8   metabolismCombo; // High nibble level, low nibble stack, stepping from 0 to 1
    s16  antibodyTicks;   // Frames left before the antibody combo clears
    s16  energyShotTicks; // Frames left before the energy shot combo clears
    s16  metabolismTicks; // Frames of both immunities; clears the metabolism combo at 0
    s8   mindWard;        // Set while silence, confusion and berserker are blocked
    s8   bodyWard;        // Set while darkness, paralysis and poison are blocked
} AttachmentState;
STATIC_ASSERT_SIZEOF(AttachmentState, 0x18);

/// 8-byte dispatch record selected by `Gp_ApplyAttachStats` as
/// `Gp_AttachParams[idx * 3 + ret].dispatch`. `field_0` is the switch key
/// (0..4). `field_2` / `field_4` are scaled by 100 into the follow-up
/// calls. `field_6` is passed as `lh` and also read as `lbu` + 2 into
/// `GpIdMapC.field_16`.
typedef struct _GpRec8 {
    /* 0x0 */ s16 field_0;
    /* 0x2 */ s16 field_2;
    /* 0x4 */ s16 field_4;
    /* 0x6 */ s16 field_6;
} GpRec8;
STATIC_ASSERT_SIZEOF(GpRec8, 8);

/// 8-byte item-effect row used by `Gp_UpdateAttachCombo`. Indexed by
/// `Gp_StateC08.attachId % 10`. `field_6` is the duration copied into
/// `antibodyTicks`, `energyShotTicks` or `metabolismTicks`.
typedef struct _GpItemRec8 {
    /* 0x0 */ u16 pad_0[3];
    /* 0x6 */ u16 field_6;
} GpItemRec8;
STATIC_ASSERT_SIZEOF(GpItemRec8, 8);

/// Row zero holds four damage percentages; the next 54 rows hold
/// three upgrade levels for each of the eighteen attachment abilities.
/// The dispatch and combo paths read signed parameters and an unsigned count.
typedef union {
    u16        percentages[4];
    GpRec8     dispatch;
    GpItemRec8 combo;
} GpAttachParam;
STATIC_ASSERT_SIZEOF(GpAttachParam, 8);

#endif // GAMEPLAY_ATTACHMENT_STATE_H
