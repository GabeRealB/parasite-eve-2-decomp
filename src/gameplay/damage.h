#ifndef GAMEPLAY_PRIVATE_DAMAGE_H
#define GAMEPLAY_PRIVATE_DAMAGE_H

#include "common.h"

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

/// Packed-id damage scale. `arg0` must carry `DAMAGE_ATTACK_CATEGORY` in its
/// high halfword; the low 12 bits are the attack's power and bits 12-15, the
/// reaction, are written to `*arg2` when it is non-NULL.
/// `arg3 == 0` uses `gPlayerStatus.hp` and `DamageReceivedScaleRow.playerPercent`;
/// otherwise `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionHp` and `DamageReceivedScaleRow.companionPercent`.
s32 Gp_ScaleDamage(s32 arg0, s32 arg1, s32* arg2, s32 arg3);

#endif // GAMEPLAY_PRIVATE_DAMAGE_H
