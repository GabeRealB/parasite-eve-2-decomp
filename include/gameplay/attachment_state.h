#ifndef GAMEPLAY_ATTACHMENT_STATE_H
#define GAMEPLAY_ATTACHMENT_STATE_H

#include "common.h"

/// Column numbers of `AttachmentLevelRow`, in stored order. Columns 5 to 7
/// (`outcome`, `effectId` and `hitCooldown`) are read only through their
/// fields and have no constant.
#define ATTACHMENT_LEVEL_EXP_COST     0
#define ATTACHMENT_LEVEL_MP_BONUS     1
#define ATTACHMENT_LEVEL_CAST_COST    2
#define ATTACHMENT_LEVEL_ATP_LOSS     3
#define ATTACHMENT_LEVEL_AMOUNT       4
#define ATTACHMENT_LEVEL_COLUMN_COUNT 8

/// Parameters of one attachment ability at one level, one row of
/// `AttachmentLevelTable`.
///
/// The ability menu labels the first four halfwords EXP cost, bonus MP,
/// casting cost and ATP loss.
typedef union {
    u16 value[ATTACHMENT_LEVEL_COLUMN_COUNT]; // Same halfwords, indexed by the column constants
    struct {
        u16 expCost;                          // EXP spent to reach this level. Other modes pay 4/5; a cleared normal game pays 2/5
        u16 mpBonus;                          // Bonus MP. Each learned level of a wheel spell adds this into max MP
        u16 castCost;                         // MP spent to cast. Taken from HP, doubled, while berserk
        u16 atpLoss;                          // Cast length in frames; the gauge counts it down
        u16 amount;                           // Hit damage, or HP restored. A heal in battle rolls upward from this
        union {
            u16 hitReaction;                  // Kind of reaction the victim takes
            u16 healMax;                      // High end of a heal rolled in battle
        } outcome;
        u16 effectId;                         // Effect spawned for the hit
        u16 hitCooldown;                      // Frames the hit imposes. Victims arm a cooldown or a stun from it
    } column;
} AttachmentLevelRow;
STATIC_ASSERT_SIZEOF(AttachmentLevelRow, 0x10);

/// Rows in `AttachmentLevelTable`. Row 0 is empty; each of the eighteen
/// abilities then takes three rows, level 1 then 2 then 3.
#define ATTACHMENT_LEVEL_ROW_COUNT 55

/// Parameters of every attachment ability at every level.
///
/// Ability `slot` at level `lvl` (1, 2 or 3) is `rows[slot * 3 + lvl]`.
/// An attack id with bit 0x8000 set selects that same row by its low 7 bits,
/// so a spell's level and that spell's hit record are one row.
///
/// `rows` is the row view. `bytes` is the same storage addressed by byte,
/// so a column is a byte offset from the table base.
typedef union {
    AttachmentLevelRow rows[ATTACHMENT_LEVEL_ROW_COUNT];                               // One ability level
    u8                 bytes[ATTACHMENT_LEVEL_ROW_COUNT * sizeof(AttachmentLevelRow)]; // Same rows, addressed by byte
} AttachmentLevelTable;
STATIC_ASSERT_SIZEOF(AttachmentLevelTable, 0x370);

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
#define ATTACHMENT_INDEX_PYROKINESIS 0
#define ATTACHMENT_INDEX_METABOLISM  6
#define ATTACHMENT_INDEX_HEALING     7
#define ATTACHMENT_INDEX_ANTIBODY    9
#define ATTACHMENT_INDEX_ENERGY_SHOT 0xA
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

/// `AttachmentAreaParam.shape`: how an ability picks its targets.
#define ATTACHMENT_AREA_SELF       0 // No area; the combo or healing update applies the effect
#define ATTACHMENT_AREA_PROJECTILE 1 // Projectile; only its path is previewed, its effect overlay finds the hits
#define ATTACHMENT_AREA_ELLIPSOID  2 // Enemies inside an ellipsoid around the player
#define ATTACHMENT_AREA_CYLINDER   3 // Enemies inside an upright cylinder around the player
#define ATTACHMENT_AREA_ALL        4 // Every lockable enemy

/// Area-of-effect view of an `AttachmentAreaRow`: the region an ability at one
/// level covers, previewed while aiming and used to pick its targets on release.
///
/// `radius` and `extent` are in units of 100 world units. `extent` is the
/// projectile's range, the ellipsoid's vertical semi-axis or the cylinder's
/// height above the player. Rows of shape `ATTACHMENT_AREA_ALL` store -1 in
/// both, and `ATTACHMENT_AREA_SELF` rows keep their combo duration where
/// `ahead` sits (see `AttachmentComboParam`).
typedef struct {
    s16 shape;  // ATTACHMENT_AREA_* (0 self, 1 projectile, 2 ellipsoid, 3 cylinder, 4 all)
    s16 radius; // Horizontal radius, in 100s of world units
    s16 extent; // Range, vertical semi-axis or height, in 100s of world units
    s16 ahead;  // Ellipsoid and cylinder: 0 centred on the player, 1 centred one radius ahead
} AttachmentAreaParam;
STATIC_ASSERT_SIZEOF(AttachmentAreaParam, 8);

/// Combo view of an `AttachmentAreaRow`: an ability of area shape
/// `ATTACHMENT_AREA_SELF` may run a timed combo, and the row's last halfword
/// is how long it lasts.
///
/// Metabolism, antibody and energy shot read it for levels 1 to 3, loading
/// `ticks` into `metabolismTicks`, `antibodyTicks` or `energyShotTicks`.
typedef struct {
    u16 unused[3]; // Area shape (`ATTACHMENT_AREA_SELF`) and two zero parameters; read only through `area`
    u16 ticks;     // Frames the combo lasts
} AttachmentComboParam;
STATIC_ASSERT_SIZEOF(AttachmentComboParam, 8);

/// Abilities with rows in the area table: the twelve wheel spells, then the
/// six slots past them that items cast from.
#define ATTACHMENT_AREA_ABILITY_COUNT 18

/// Levels each ability has a row for in the area table. Level `n` is row `n - 1`.
#define ATTACHMENT_AREA_LEVEL_COUNT 3

/// How one attachment ability at one level reaches its targets, one row of the
/// area table.
///
/// Every row is read as `area` first, and its shape decides how the ability
/// is applied. Rows of shape `ATTACHMENT_AREA_SELF` have no region to
/// describe; the abilities among them that run a timed combo keep its
/// duration in the last halfword, which `combo` reads unsigned where `area`
/// reads a signed flag.
typedef union {
    AttachmentAreaParam  area;  // Target shape and its dimensions
    AttachmentComboParam combo; // `ATTACHMENT_AREA_SELF` rows: how long the ability's combo lasts
} AttachmentAreaRow;
STATIC_ASSERT_SIZEOF(AttachmentAreaRow, 8);

#endif // GAMEPLAY_ATTACHMENT_STATE_H
