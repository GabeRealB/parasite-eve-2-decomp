#ifndef GAMEPLAY_ENEMY_PARAMS_H
#define GAMEPLAY_ENEMY_PARAMS_H

#include "common.h"

struct DamageAttack;

/// Shared parameters of one enemy kind: attacks, starting hit points, the
/// experience, battle points and magic points credited on release, and the
/// critical-hit and reaction values a landed attack reads.
///
/// `Enemy.param` points here. Several enemies of one kind share one record,
/// and the record does not point back at them. The type has its own header so a
/// unit can name the record without including the enemy API.
///
/// `exp`, `bp` and `mp` are added into the battle-result totals on each release
/// while that result is still open. The result screen adds those totals to the
/// player's experience, battle points and magic points. A kind may change the
/// three amounts on the record before the add.
///
/// `critChance` is the percent base of the critical-hit roll against this kind.
/// `buildupSteps` is the base number of 31-frame steps in the buildup reaction.
/// `damageOverTimeChance` and `damageOverTimeTicks` are the percent chance and
/// the base pulse count of damage over time. Both counts are scaled by the
/// attack's grade. Zero steps never complete the buildup, and zero pulses
/// never let the damage expire.
typedef struct {
    struct DamageAttack* attacks;              // Rows packed into this kind's contact keys; null where the kind packs none
    u16                  hpMax;                // Hit points this kind starts with; damage over time deals a fraction of them
    u16                  exp;                  // Experience credited to the battle result when the enemy is released
    u16                  bp;                   // Battle points credited to the battle result when the enemy is released
    u8                   mp;                   // Magic points credited to the battle result when the enemy is released
    u8                   critChance;           // Percent base of the critical-hit roll; the stored percent can exceed 100
    u8                   buildupSteps;         // Base buildup steps, 31 frames each, before the grade scale; 0 never completes it
    u8                   damageOverTimeChance; // Percent chance damage over time starts; 0 never, 100 always
    u8                   damageOverTimeTicks;  // Base damage pulses before expiry, before the grade scale; 0 never expires
} EnemyParams;
STATIC_ASSERT_SIZEOF(EnemyParams, 0x10);

#endif // GAMEPLAY_ENEMY_PARAMS_H
