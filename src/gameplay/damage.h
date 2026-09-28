#ifndef GAMEPLAY_PRIVATE_DAMAGE_H
#define GAMEPLAY_PRIVATE_DAMAGE_H

#include "types.h"

/// Packed-id damage scale. `arg0` must have high bits `0x40000`; low 12 bits
/// are the power and bits 12-15 are written to `*arg2` when it is non-NULL.
/// `arg3 == 0` uses `Player_Status.hp` and `GpDmgRow.field_A`;
/// otherwise `Mc_SaveData[0].companionHp` and `GpDmgRow.field_0`.
s32 Gp_ScaleDamage(s32 arg0, s32 arg1, s32* arg2, s32 arg3);

#endif // GAMEPLAY_PRIVATE_DAMAGE_H
