#include "gameplay/damage.h"

#include "common.h"

#include "gameplay/attachments.h"
#include "weapon_data.h"
#include "attachments.h"
#include "item_menu.h"
#include "gameplay/enemy_params.h"
#include "damage.h"

#include "main/random.h"

/// Calls to the buildup tick function accumulated before advancing one step.
#define DAMAGE_ENEMY_BUILDUP_STEP_TICKS 0x1F

/// Mask selecting the final buildup hold's random byte-countdown seed.
///
/// LCG bits 16..21 yield 0..63. Seeds 1..63 take that many subsequent
/// decrement calls; zero wraps through 255 and takes 256 calls to expire.
#define DAMAGE_ENEMY_BUILDUP_HOLD_SEED_MASK 0x3F

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

/// Fixed hazard damage to the player, indexed by the contact key's low halfword.
///
/// The id must be 0..10. Only ids 2, 3 and 4 have nonzero rows; these rows
/// live with gameplay. There is no sentinel or bounds check.
static _HazardPlayerDamage _gDamageHazardPlayerRows[] = {
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
/// Fixed hazard damage to the enemy, indexed by the contact key's low halfword.
///
/// The id must be 0..10. Only ids 2, 3 and 4 have nonzero rows; these rows
/// live with gameplay. There is no sentinel or bounds check.
static _HazardEnemyDamage _gDamageHazardEnemyRows[] = {
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
/// Retained bytes between the hazard-damage and new-game map-mark tables.
///
/// No identified access establishes their meaning or original object boundary.
static u16 D_80114096 = 0x3430;

/// Restarts the countdown to an enemy's next damage-over-time pulse.
///
/// `enemy` must be non-NULL, live and writable. Consumes one shared random draw,
/// advancing `gRandomLcgState` with unsigned 32-bit wraparound before taking
/// bits 16..19. Overwrites the byte delay with 83..98 ticks, inclusive; one
/// tick is one call to `damageTickEnemyDamageOverTime`.
/// Callers own reaction activation, pulse counting and expiry.
static inline void _damageReseedEnemyDamageOverTimeDelay(Enemy* enemy)
{
    RANDOM_LCG_ADVANCE();
    enemy->damageOverTimeDelay = (RANDOM_LCG_DRAW() & DAMAGE_ENEMY_POISON_DELAY_JITTER_MASK) + DAMAGE_ENEMY_POISON_DELAY_BASE_TICKS;
}

s32 damageGetHazardDamage(s32 hazardKey, s32 victim)
{
    s32 damage;

    damage = 0;
    switch (victim) {
        case DAMAGE_HAZARD_VICTIM_PLAYER:
            damage = _gDamageHazardPlayerRows[(u16)hazardKey].damage;
            break;
        case DAMAGE_HAZARD_VICTIM_ENEMY:
            damage = _gDamageHazardEnemyRows[(u16)hazardKey].damage;
            break;
    }
    return damage;
}

s32 damageGetPlayerAttackReaction(s32 attackKey)
{
    u16 reaction;

    if ((attackKey & DAMAGE_PLAYER_ATTACK_ATTACHMENT) == 0) {
        reaction = gWeaponAttackRows[attackKey & DAMAGE_PLAYER_ATTACK_ROW_MASK].hitReaction;
    } else {
        reaction = gAttachmentLevelTable.rows[attackKey & DAMAGE_PLAYER_ATTACK_ROW_MASK].column.outcome.hitReaction;
    }
    return reaction;
}

u16 damageGetPlayerAttackEffectId(s32 attackKey)
{
    u16 effectId;

    if ((attackKey & DAMAGE_PLAYER_ATTACK_ATTACHMENT) == 0) {
        effectId = gWeaponAttackRows[attackKey & DAMAGE_PLAYER_ATTACK_ROW_MASK].effectId;
    } else {
        effectId = gAttachmentLevelTable.rows[attackKey & DAMAGE_PLAYER_ATTACK_ROW_MASK].column.effectId;
    }
    return effectId;
}

void damageTryStartEnemyDamageOverTime(Enemy* enemy, s32 attackKey, s32 unused)
{
    s32 poisonPercent;
    s32 poisonChance;
    s32 roll;

    // Roll the kind's percent chance before changing any reaction state.
    poisonPercent = enemy->param->damageOverTimeChance;
    poisonChance  = (poisonPercent << DAMAGE_CHANCE_FRACTION_BITS) / 100;
    RANDOM_LCG_ADVANCE();
    roll = RANDOM_LCG_DRAW() & DAMAGE_CHANCE_DRAW_MASK;
    if (roll < poisonChance) {
        enemy->damageOverTimePulse = 0;
        enemy->reactionFlags      |= ENEMY_REACTION_DAMAGE_OVER_TIME;
        _damageReseedEnemyDamageOverTimeDelay(enemy);
        if ((attackKey & DAMAGE_PLAYER_ATTACK_ATTACHMENT) == 0) {
            enemy->damageOverTimeGrade = 0;
            return;
        }
        enemy->damageOverTimeGrade = gAttachmentState.attachId % 10U;
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
        damagePercent = gDamageEnemyPoisonPulseDamagePercent[enemy->damageOverTimeGrade];
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
    pulseLimit = (pulseLimit * gDamageEnemyPoisonPulseLimitPercent[enemy->damageOverTimeGrade]) / 100;
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
    enemy->buildupGrade = gAttachmentState.attachId % 10U;
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
    stepPercent = gDamageEnemyBuildupStepLimitPercent[enemy->buildupGrade];
    stepLimit   = (baseSteps * stepPercent) / 100;
    // Accumulate full steps before starting the final hold countdown.
    if (enemy->buildupStep < stepLimit) {
        enemy->buildupTimer++;
        if (enemy->buildupTimer >= DAMAGE_ENEMY_BUILDUP_STEP_TICKS) {
            enemy->buildupStep++;
            if (enemy->buildupStep >= stepLimit) {
                RANDOM_LCG_ADVANCE();
                enemy->buildupTimer = RANDOM_LCG_DRAW() & DAMAGE_ENEMY_BUILDUP_HOLD_SEED_MASK;
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
        cooldownFrames = gWeaponAttackRows[attackKey & DAMAGE_PLAYER_ATTACK_ROW_MASK].hitCooldown;
    } else {
        cooldownFrames = gAttachmentLevelTable.rows[attackKey & DAMAGE_PLAYER_ATTACK_ROW_MASK].column.hitCooldown;
    }
    return cooldownFrames;
}
