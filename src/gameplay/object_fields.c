#include "gameplay/damage.h"

#include "common.h"

#include "gameplay/attachments.h"
#include "attachments.h"
#include "item_menu.h"
#include "gameplay/enemy_params.h"
#include "damage.h"

#include "main/random.h"

/// What a hazard contact does to the player, one row per hazard id.
///
/// A hazard is a collision body whose key carries contact category 5 in its
/// high halfword: scenery that rooms and actors place, as opposed to an
/// attack, whose category-4 key packs its own power. The key's low halfword
/// is the hazard id, and that id selects the row. The damage is fixed per id
/// and is not scaled the way an attack's power is. An id whose row is zero
/// costs the player nothing.
typedef struct {
    u16 damage;  // Damage the contact deals to the player, taken as stored; 0 for a harmless id
    u16 field_2; // Never read, role unproven. Observed 5, 0, 5 in the three damaging rows, equal to the hit reaction the player's contact handler picks in code for those ids
} _HazardPlayerDamage;
STATIC_ASSERT_SIZEOF(_HazardPlayerDamage, 0x4);

/// What a hazard contact does to an enemy, one row per hazard id.
///
/// The enemy-side counterpart of `_HazardPlayerDamage`, selected by the same
/// hazard id. The damage is fixed per id and comes off the enemy's hit points
/// as stored, with none of the rolls or scaling an attack's power goes
/// through. An id whose row is zero costs the enemy nothing.
typedef struct {
    u16 damage;  // Hit points the contact takes from the enemy, taken as stored; 0 for a harmless id
    u16 field_2; // Never read, role unproven. Observed 7, 0, 7 in the three damaging rows
    u16 field_4; // Never read, role unproven. Observed equal to field_2 in every row
} _HazardEnemyDamage;
STATIC_ASSERT_SIZEOF(_HazardEnemyDamage, 0x6);

/// 4-byte records selected by `damageGetHazardDamage(..., 0)`.
extern _HazardPlayerDamage Gp_IdField0[];

/// 6-byte records selected by `damageGetHazardDamage(..., 1)`.
extern _HazardEnemyDamage Gp_IdField1[];

/// Unreferenced nonzero halfword following the ID-field table.
extern u16 D_80114096;

_HazardPlayerDamage Gp_IdField0[11] = {
    { 0, 0 },
    { 0, 0 },
    { 10, 5 },
    { 15, 0 },
    { 20, 5 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
};
_HazardEnemyDamage Gp_IdField1[11] = {
    { 0, 0, 0 },
    { 0, 0, 0 },
    { 180, 7, 7 },
    { 240, 0, 0 },
    { 520, 7, 7 },
    { 0, 0, 0 },
    { 0, 0, 0 },
    { 0, 0, 0 },
    { 0, 0, 0 },
    { 0, 0, 0 },
    { 0, 0, 0 },
};
/// Unreferenced nonzero halfword following the ID-field table.
u16 D_80114096 = 0x3430;

/// Draws the next 83..98-frame pulse delay from the shared random sequence.
///
/// Updates the live enemy's byte delay and advances the shared state once.
static inline void _damageReseedEnemyDamageOverTimeDelay(Enemy* enemy)
{
    gRandomLcgState            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    enemy->damageOverTimeDelay = (gRandomLcgState >> 16 & ENEMY_DAMAGE_OVER_TIME_DELAY_JITTER) + ENEMY_DAMAGE_OVER_TIME_DELAY_BASE;
}

s32 damageGetHazardDamage(s32 hazardKey, s32 victim)
{
    s32 damage;

    damage = 0;
    switch (victim) {
        case DAMAGE_HAZARD_VICTIM_PLAYER:
            damage = Gp_IdField0[(u16)hazardKey].damage;
            break;
        case DAMAGE_HAZARD_VICTIM_ENEMY:
            damage = Gp_IdField1[(u16)hazardKey].damage;
            break;
    }
    return damage;
}

s32 damageGetPlayerAttackReaction(s32 attackKey)
{
    u16 reaction;

    if ((attackKey & DAMAGE_PLAYER_ATTACK_ATTACHMENT) == 0) {
        reaction = Gp_IdParamLo[attackKey & DAMAGE_PLAYER_ATTACK_ROW_MASK].hitReaction;
    } else {
        reaction = Gp_IdParamHi.rows[attackKey & DAMAGE_PLAYER_ATTACK_ROW_MASK].column.outcome.hitReaction;
    }
    return reaction;
}

u16 damageGetPlayerAttackEffectId(s32 attackKey)
{
    u16 effectId;

    if ((attackKey & DAMAGE_PLAYER_ATTACK_ATTACHMENT) == 0) {
        effectId = Gp_IdParamLo[attackKey & DAMAGE_PLAYER_ATTACK_ROW_MASK].effectId;
    } else {
        effectId = Gp_IdParamHi.rows[attackKey & DAMAGE_PLAYER_ATTACK_ROW_MASK].column.effectId;
    }
    return effectId;
}

void damageTryStartEnemyDamageOverTime(Enemy* enemy, s32 attackKey, s32 unused)
{
    s32 poisonPercent;
    s32 poisonChance;
    s32 roll;

    // Roll the kind's percent chance before changing any reaction state.
    poisonPercent   = enemy->param->damageOverTimeChance;
    poisonChance    = (poisonPercent << DAMAGE_CHANCE_FRACTION_BITS) / 100;
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    roll            = gRandomLcgState >> 16 & DAMAGE_CHANCE_DRAW_MASK;
    if (roll < poisonChance) {
        enemy->damageOverTimePulse = 0;
        enemy->reactionFlags      |= ENEMY_REACTION_DAMAGE_OVER_TIME;
        _damageReseedEnemyDamageOverTimeDelay(enemy);
        if ((attackKey & DAMAGE_PLAYER_ATTACK_ATTACHMENT) == 0) {
            enemy->damageOverTimeGrade = 0;
            return;
        }
        enemy->damageOverTimeGrade = Gp_StateC08.attachId % 10U;
    }
}

s32 damageTickEnemyDamageOverTime(Enemy* enemy)
{
    s32 damage;
    s32 maxHp;
    s32 damagePercent;

    damage = 0;
    enemy->damageOverTimeDelay--;
    if (enemy->damageOverTimeDelay == 0) {
        enemy->damageOverTimePulse++;
        _damageReseedEnemyDamageOverTimeDelay(enemy);
        maxHp         = enemy->param->hpMax;
        damagePercent = D_80113D38[enemy->damageOverTimeGrade];
        damage        = (maxHp * damagePercent) / 100;
        if (damage == 0) {
            damage = 1;
        }
    }
    return damage;
}

s32 damageIsEnemyDamageOverTimeExpired(const Enemy* enemy)
{
    s32 pulseLimit;
    s32 expired;

    expired    = 0;
    pulseLimit = enemy->param->damageOverTimeTicks;
    if (!(enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME)) {
        return 1;
    }
    if (pulseLimit == 0) {
        return 0;
    }
    pulseLimit = (pulseLimit * D_80113D28[enemy->damageOverTimeGrade]) / 100;
    if (enemy->damageOverTimePulse >= pulseLimit) {
        expired = 1;
    }
    return expired;
}

void damageStartEnemyStagger(Enemy* enemy)
{
    enemy->reactionFlags |= ENEMY_REACTION_STAGGER;
}

void damageStartEnemyBuildup(Enemy* enemy, s32 attackKey, s32 unused)
{
    enemy->buildupStep    = 0;
    enemy->buildupTimer   = 0;
    enemy->reactionFlags |= ENEMY_REACTION_BUILDUP;
    if ((attackKey & DAMAGE_PLAYER_ATTACK_ATTACHMENT) == 0) {
        enemy->buildupGrade = 0;
        return;
    }
    // This attachment attack always uses grade zero.
    if ((attackKey & DAMAGE_BUILDUP_UNGRADED_ROW_MASK) == DAMAGE_BUILDUP_UNGRADED_ROW) {
        enemy->buildupGrade = 0;
        return;
    }
    enemy->buildupGrade = Gp_StateC08.attachId % 10U;
}

s32 damageTickEnemyBuildup(Enemy* enemy)
{
    s32 expired;
    s32 stepLimit;
    s32 baseSteps;
    s32 stepPercent;

    expired   = 0;
    baseSteps = enemy->param->buildupSteps;
    if (baseSteps == 0) {
        return expired;
    }
    stepPercent = D_80113D30[enemy->buildupGrade];
    stepLimit   = (baseSteps * stepPercent) / 100;
    // Accumulate full steps before starting the final hold countdown.
    if (enemy->buildupStep < stepLimit) {
        enemy->buildupTimer++;
        if (enemy->buildupTimer >= ENEMY_BUILDUP_STEP_FRAMES) {
            enemy->buildupStep++;
            if (enemy->buildupStep >= stepLimit) {
                gRandomLcgState     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                enemy->buildupTimer = gRandomLcgState >> 16 & ENEMY_BUILDUP_COUNTDOWN_MASK;
            } else {
                enemy->buildupTimer = 0;
            }
        }
    } else {
        // Keep the byte wraparound of a zero countdown.
        enemy->buildupTimer--;
        if (enemy->buildupTimer == 0) {
            expired = 1;
        }
    }
    return expired;
}

s32 damageGetPlayerAttackHitCooldown(s32 attackKey)
{
    u16 cooldownFrames;

    if ((attackKey & DAMAGE_PLAYER_ATTACK_ATTACHMENT) == 0) {
        cooldownFrames = Gp_IdParamLo[attackKey & DAMAGE_PLAYER_ATTACK_ROW_MASK].hitCooldown;
    } else {
        cooldownFrames = Gp_IdParamHi.rows[attackKey & DAMAGE_PLAYER_ATTACK_ROW_MASK].column.hitCooldown;
    }
    return cooldownFrames;
}
