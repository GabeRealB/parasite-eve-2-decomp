#ifndef GAMEPLAY_PRIVATE_DAMAGE_H
#define GAMEPLAY_PRIVATE_DAMAGE_H

#include "common.h"

/// Player attack row selection, and the low-six-bit buildup grade exception.
enum {
    DAMAGE_PLAYER_ATTACK_ROW_MASK    = 0x7F,
    DAMAGE_PLAYER_ATTACK_ATTACHMENT  = 0x8000,
    DAMAGE_BUILDUP_UNGRADED_ROW_MASK = 0x3F,
    DAMAGE_BUILDUP_UNGRADED_ROW      = 0x31,
};

/// Chance calculations use twelve fractional bits and a draw in 0..4095.
enum {
    DAMAGE_CHANCE_FRACTION_BITS = 12,
    DAMAGE_CHANCE_DRAW_MASK     = (1 << DAMAGE_CHANCE_FRACTION_BITS) - 1,
};

/// HP bands a `DamageReceivedScaleRow` distinguishes, weakest first.
#define DAMAGE_RECEIVED_HP_BAND_COUNT 5

/// Percentages of an attack's power that reach the player or the companion at
/// one difficulty.
///
/// Each victim has one percentage per HP band, and the band rises with the
/// victim's current HP, so a hit takes less from a victim who is nearly dead
/// than from a healthy one.
typedef struct {
    u16 companionPercent[DAMAGE_RECEIVED_HP_BAND_COUNT]; // Applied to a hit on the companion, by the companion's HP band
    u16 playerPercent[DAMAGE_RECEIVED_HP_BAND_COUNT];    // Applied to a hit on the player, by the player's HP band
} DamageReceivedScaleRow;
STATIC_ASSERT_SIZEOF(DamageReceivedScaleRow, 0x14);

/// Enemy poison pulse-count percentages, indexed by reaction grade 0..3.
///
/// Grade 0 is a non-PE attack; grades 1..3 are the active PE cast's level.
/// Scales the enemy kind's base pulse count, not its inter-pulse frame delay.
/// The reaction must retain a valid grade until it ends; rows live with gameplay.
extern u16 gDamageEnemyPoisonPulseLimitPercent[4];

/// Enemy buildup step-count percentages, indexed by reaction grade 0..3.
///
/// Grade 0 is a non-PE attack or the ungraded PE-row exception; 1..3 are
/// the active cast's level. Scales the base count of 31-frame buildup steps.
extern u16 gDamageEnemyBuildupStepLimitPercent[4];

/// Percent of maximum enemy HP removed by each poison pulse, by grade 0..3.
///
/// Grade 0 uses 16 percent; PE levels 1/2/3 use 4/6/8 percent. The tick
/// consumer rounds down and substitutes one HP when the result is zero.
extern u16 gDamageEnemyPoisonPulseDamagePercent[4];

/// Final outgoing weapon/PE damage percentages, indexed by difficulty 0..4.
///
/// Rows are Normal, Bounty, Scavenger, Nightmare and replay (100/75/75/60/100).
/// The sixth stored halfword is zero; it is outside the live difficulty domain.
extern u16 gDamageAttackDifficultyPercent[6];

/// Computes HP damage received by the player or companion from an attack key.
///
/// Category 4 keys carry 12-bit power and a 4-bit victim reaction. Other
/// categories return 0 and leave `outReaction` untouched. For category 4,
/// a non-NULL output receives the reaction even when power is zero.
/// `victimIsCompanion` selects the player at 0 and the companion otherwise;
/// the victim's current HP must be in 0..299 and difficulty in 0..4.
/// Difficulty and the current HP band scale the power; Antibody further
/// reduces player damage only. A nonzero power deals at least 1 HP after
/// scaling. This calculates damage without changing either victim's HP.
/// `unused` is retained by the interface and ignored.
s32 damageComputeReceived(s32 attackKey, s32 unused, s32* outReaction, s32 victimIsCompanion);

#endif // GAMEPLAY_PRIVATE_DAMAGE_H
