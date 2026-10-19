#ifndef GAMEPLAY_ATTACHMENT_STATE_H
#define GAMEPLAY_ATTACHMENT_STATE_H

#include "common.h"

/// Wheel spells in slots 0..11, excluding the six item-ability slots.
#define ATTACHMENT_SPELL_COUNT 0xC

/// Ability slots in the parameter catalogs: twelve wheel spells and six item abilities.
#define ATTACHMENT_ABILITY_COUNT 18

/// Parameter rows per ability, and the maximum learned spell level.
///
/// Learned levels are 0..3, with zero meaning unlearned. Cast levels are 1..3;
/// the area table indexes level minus one, while level and sound tables use
/// ability slot times this count plus the cast level.
#define ATTACHMENT_LEVEL_COUNT 3

/// Column 0 of `AttachmentLevelRow.value`: base EXP cost to learn this level.
///
/// Positive game modes pay 4/5; a cleared normal game pays 2/5, rounded down.
#define ATTACHMENT_LEVEL_EXP_COST 0

/// Column 1 of `AttachmentLevelRow.value`: MP capacity added by learning this level.
///
/// Capacity includes each learned level's bonus, rather than only the latest one.
#define ATTACHMENT_LEVEL_MP_BONUS 1

/// Column 2 of `AttachmentLevelRow.value`: MP spent on release, doubled as HP while berserk.
#define ATTACHMENT_LEVEL_CAST_COST 2

/// Column 3 of `AttachmentLevelRow.value`: cast duration in HUD updates, labelled ATP loss.
///
/// The same count sizes the gauge in pixels; arming narrows it to a signed byte
/// and clamps nonpositive durations to one update.
#define ATTACHMENT_LEVEL_ATP_LOSS 3

/// Unsigned halfwords in one level record, including the four hit/healing columns.
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

/// Level records including the empty row zero and three levels of each ability.
#define ATTACHMENT_LEVEL_ROW_COUNT (ATTACHMENT_ABILITY_COUNT * ATTACHMENT_LEVEL_COUNT + 1)

/// Parameters of every attachment ability at every level.
///
/// Ability `slot` at level `lvl` (1, 2 or 3) is
/// `rows[slot * ATTACHMENT_LEVEL_COUNT + lvl]`.
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

/// Script lock in `AttachmentState.flags`: blocks queued casts and ordinary wheel entry.
///
/// An armed or charging cast is cancelled when this bit is observed.
#define ATTACHMENT_FLAG_EVENT_LOCK 1

/// HUD category-switch lock in `AttachmentState.flags`, used by training scripts.
///
/// Category checks may explicitly bypass it; it does not itself cancel a cast.
#define ATTACHMENT_FLAG_SWAP_LOCK 2

/// PE task request to apply the current ability's target or self effects on the next HUD update.
///
/// The HUD dispatches release targeting, then clears this bit. Multiple requests
/// before that update coalesce; resource spending occurs separately at cast release.
#define ATTACHMENT_FLAG_APPLY_STATS 8

/// Script request to open the PE wheel on an idle, unqueued HUD update.
///
/// Bypasses the pad, category-switch, display and event-lock entry gates.
/// Opening consumes this bit; a non-idle mode or queued selection retains it.
#define ATTACHMENT_FLAG_FORCE_OPEN_WHEEL 0x10

/// Byte mask for `flags &= mask`: removes only the HUD category-switch lock.
///
/// Its positive int value is 0xFD, preserving the other seven stored flag bits.
#define ATTACHMENT_FLAG_CLEAR_SWAP_LOCK (0xFF ^ ATTACHMENT_FLAG_SWAP_LOCK)

/// No open wheel or armed/counting cast; a selection may be queued and released PE tasks may run.
#define ATTACHMENT_MODE_IDLE 0

/// Spell-selection wheel mode, holding player-state and scene-actor updates.
///
/// Combo timers pause in this mode; cancel or cast selection resumes actors.
#define ATTACHMENT_MODE_WHEEL 1

/// Selected cast awaiting the player's release request; its duration has not started counting down.
///
/// Armed and casting are the consecutive values 2 and 3, also tested as a range.
#define ATTACHMENT_MODE_ARMED 2

/// Cast countdown mode, entered on the release request and left on expiry or cancellation.
#define ATTACHMENT_MODE_CAST 3

/// New-cast signal in `AttachmentState.effectPhase`, awaiting player action entry.
///
/// PE tasks use it to retire effects superseded by the new cast. The HUD resets
/// phase signals to idle before the next update; this is independent of cast mode.
#define ATTACHMENT_EFFECT_HELD (-2)

/// No phase signal for this update; ongoing PE tasks and cast mode may remain active.
#define ATTACHMENT_EFFECT_IDLE 0

/// Cancellation signal for the player's PE action and its charge visuals.
///
/// Published when an armed/counting cast is interrupted; the next HUD update
/// resets the signal to idle. Running release effects also obey room-effect control.
#define ATTACHMENT_EFFECT_CANCELLED 2

/// Wheel menu is closed; room event gates may proceed, subject to their other holds.
#define ATTACHMENT_MENU_CLOSED 0

/// Pyrokinesis ability slot: fire group's first spell, decimal cast family 11.
#define ATTACHMENT_INDEX_PYROKINESIS 0

/// Metabolism ability slot: water group's first spell, decimal cast family 31.
///
/// Also the exclusive upper bound on the six offensive spells allowed while berserk.
#define ATTACHMENT_INDEX_METABOLISM 6

/// Healing ability slot: water group's second spell, decimal cast family 32.
///
/// The wheel's only spell cast outside combat; its amount restores HP.
#define ATTACHMENT_INDEX_HEALING 7

/// Energy Shot ability slot: earth group's second spell, decimal cast family 42.
#define ATTACHMENT_INDEX_ENERGY_SHOT 0xA

/// Energy Ball ability slot: earth group's third spell, decimal cast family 43.
///
/// Its cast eligibility also checks the live projectile count.
#define ATTACHMENT_INDEX_ENERGY_BALL 0xB

/// Bit position of cast level 1..3 in a combo byte; the low nibble retains the stack count.
///
/// Refresh replaces the high nibble. Dividing a nonzero byte by `1 << shift`
/// recovers the level for damage scaling; the stored values fit in signed bytes.
#define ATTACHMENT_COMBO_LEVEL_SHIFT 4

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
    u8   flags;           // Bit 0 event lock, 1 swap lock, 2 start cast, 3 apply stats, 4 force the wheel open
    s8   previewSound;    // Wheel preview sound; cleared once that play returns
    s8   soundStep;       // 0 idle, 1 queued, 2 load request handled
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
