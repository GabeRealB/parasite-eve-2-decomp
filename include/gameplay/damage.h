#ifndef GAMEPLAY_DAMAGE_H
#define GAMEPLAY_DAMAGE_H

#include "types.h"

#include "gameplay/collision.h"
#include "gameplay/enemy.h"

/// Packed-id enemy damage roll. `arg0` must have high bits `0x20000`. Ids
/// without bit 0x8000 read `Gp_IdParamLo`, scale by a random 100..119 percent,
/// by the `D_80113568` row for `(arg0 >> 8) & 0x3F`, and by `arg3` when
/// `arg2` matches the row's `hitReaction`; ids with bit 0x8000 read
/// `Gp_IdParamHi` and scale by a random 100..109 percent. `arg1` is a hit
/// count that selects the `D_80113568` column through `D_80113864`.
u32 Gp_ComputeDamage(u32 arg0, u32 arg1, s32 arg2, s32 arg3);

/// Returns 1 when a player attack scores a critical hit on `enemy`, otherwise 0.
///
/// Requires live enemy parameters and coordinates. A weapon key has bit 15
/// clear, a low-seven-bit weapon row in 0..46, and a distance-scale row in
/// bits 8..13 in 0..46. Attachment keys, an absent player task, or a zero base
/// critical chance return 0 without a random draw. Eligible attacks compose
/// the enemy coordinate and consume one 12-bit draw.
///
/// Distance scales the chance, explosions use their own distance scale, and
/// bit 14 selects the alternate critical percentage. Buildup doubles the
/// chance and Energy Shot applies its current combo percentage.
/// `chanceMultiplier` is a factor, with 0 meaning no extra scaling; the chance
/// is not clamped, so a value of at least 4096 always succeeds.
///
/// The retained distance calculation rotates (low16 of body X, high16 of body
/// X, low16 of body Y) as signed components, ignores body Z, adds the enemy's
/// world translation, and measures from the player's world origin in game
/// units. Its distance/1000 bucket is narrowed to a halfword before lookup.
s32 damageRollCriticalHit(const Enemy* enemy, u32 attackKey, s32 chanceMultiplier);

/// Packs one enemy attack into a category-4 collision key for its victim.
///
/// `enemy` must be live. NULL parameters return the zero/no-contact key;
/// otherwise `param->attacks` must contain `attackIndex`, a nonnegative element
/// index. A NULL attack table is not accepted when the parameters exist.
/// The table is borrowed and unchanged. The low 12 power bits and low 4
/// reaction bits are packed; a zero-power attack still has category 4.
s32 damagePackEnemyAttackKey(const Enemy* enemy, s32 attackIndex);

/// Packs an attack-table entry into a category-4 collision key for its victim.
///
/// NULL `attacks` returns the zero/no-contact key. Otherwise `attackIndex` is
/// a nonnegative element index within the caller's table; no length is stored
/// or checked. The table is borrowed and unchanged. Power is masked to 12 bits
/// and reaction to 4 bits; a zero-power entry still has category 4.
s32 damagePackAttackKey(const DamageAttack* attacks, s32 attackIndex);

/// Credits a Life Drain hit to the current cast's pending player healing.
///
/// Call with the enemy's HP before subtracting the hit and a nonnegative
/// damage amount in HP. Low-seven-bit rows 25..27 identify Life Drain levels
/// 1..3; other rows do nothing. Category and attachment-selector bits are not
/// checked. Credit is the lesser of damage and remaining HP, compared as
/// unsigned words, and is added to `SceneCombatState.lifeDrainHp`. The cast
/// later pays that total to the player; this call changes no enemy HP.
/// `unused` is retained by the interface and ignored.
void damageAccumulateLifeDrainHp(const Enemy* enemy, s32 attackKey, s32 damage, s32 unused);

#endif // GAMEPLAY_DAMAGE_H
