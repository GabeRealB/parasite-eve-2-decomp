#ifndef GAMEPLAY_DAMAGE_H
#define GAMEPLAY_DAMAGE_H

#include "types.h"

#include "gameplay/collision.h"
#include "gameplay/enemy.h"

struct GpEnemy;

/// Packed-id enemy damage roll. `arg0` must have high bits `0x20000`. Ids
/// without bit 0x8000 read `Gp_IdParamLo`, scale by a random 100..119 percent,
/// by the `D_80113568` row for `(arg0 >> 8) & 0x3F`, and by `arg3` when
/// `arg2` matches the record's `key`; ids with bit 0x8000 read
/// `Gp_IdParamHi` and scale by a random 100..109 percent. `arg1` is a hit
/// count that selects the `D_80113568` column through `D_80113864`.
u32 Gp_ComputeDamage(u32 arg0, u32 arg1, s32 arg2, s32 arg3);

/// Rolls a status/effect chance for `arg0` against the player. Returns 0 for
/// ids with bit 0x8000 set, when no slot 3 is active, or when
/// `EnemyParams.critChance` scaled by 1/100 is zero. Otherwise the enemy's world
/// distance to the player picks a `D_80113864` class, that class selects a
/// percentage from `D_80113858` (when `GpRec10.field_4` is 6) or from the
/// `D_80113568` row for `(arg1 >> 8) & 0x3F`, and column 6 (or 7 with bit
/// 0x4000) of that same row scales `critChance`. `GpEnemy.reactionFlags` bit 1
/// doubles the chance, `Gp_StateC08.field_D` applies a `D_80113D0C` percent,
/// and `arg2` multiplies it when non-zero. The result is compared against a
/// 12-bit `Gp_LcgState` draw.
s32 Gp_RollEnemyChance(struct GpEnemy* arg0, u32 arg1, s32 arg2);

s32 Gp_PackObjPair(struct GpEnemy* arg0, s32 arg1);

s32 Gp_PackPair(DamageAttack* pairs, s32 index);

void func_800E2C78(struct GpEnemy* arg0, s32 arg1, s32 arg2, s32 arg3);

#endif // GAMEPLAY_DAMAGE_H
