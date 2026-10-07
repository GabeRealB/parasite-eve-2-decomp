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
