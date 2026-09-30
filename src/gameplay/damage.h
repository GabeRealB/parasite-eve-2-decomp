#ifndef GAMEPLAY_PRIVATE_DAMAGE_H
#define GAMEPLAY_PRIVATE_DAMAGE_H

#include "types.h"

/// Packed-id damage scale. `arg0` must carry `DAMAGE_ATTACK_CATEGORY` in its
/// high halfword; the low 12 bits are the attack's power and bits 12-15, the
/// reaction, are written to `*arg2` when it is non-NULL.
/// `arg3 == 0` uses `Player_Status.hp` and `GpDmgRow.field_A`;
/// otherwise `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionHp` and `GpDmgRow.field_0`.
s32 Gp_ScaleDamage(s32 arg0, s32 arg1, s32* arg2, s32 arg3);

#endif // GAMEPLAY_PRIVATE_DAMAGE_H
