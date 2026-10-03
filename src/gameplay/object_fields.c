#include "gameplay/object_fields.h"

#include "common.h"

#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "attachments.h"
#include "gameplay/enemy.h"
#include "item_menu.h"
#include "gameplay/enemy_params.h"
#include "gameplay/damage.h"
#include "weapon_data.h"

#include "main/random.h"
#include "main/session_types.h"
#include "main/task_types.h"

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

/// 4-byte records selected by `Gp_LookupIdField(..., 0)`.
extern _HazardPlayerDamage Gp_IdField0[];

/// 6-byte records selected by `Gp_LookupIdField(..., 1)`.
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

s32 Gp_LookupIdField(s32 arg0, s32 arg1)
{
    s32 ret;

    ret = 0;
    switch (arg1) {
        case 0:
            ret = Gp_IdField0[(u16)arg0].damage;
            break;
        case 1:
            ret = Gp_IdField1[(u16)arg0].damage;
            break;
    }
    return ret;
}

s32 Gp_GetIdParam0(s32 arg0)
{
    s32 ret;

    if ((arg0 & 0x8000) == 0) {
        ret = Gp_IdParamLo[arg0 & 0x7F].hitReaction;
    } else {
        ret = Gp_IdParamHi.rows[arg0 & 0x7F].column.outcome.hitReaction;
    }
    return ret;
}

s32 Gp_GetIdParam1(s32 arg0)
{
    s32 ret;

    if ((arg0 & 0x8000) == 0) {
        ret = Gp_IdParamLo[arg0 & 0x7F].effectId;
    } else {
        ret = Gp_IdParamHi.rows[arg0 & 0x7F].column.effectId;
    }
    return ret;
}

void Gp_SetObjFlag4(Enemy* arg0, s32 arg1, s32 arg2)
{
    s32 val;
    s32 limit;
    s32 rand;

    val             = arg0->param->damageOverTimeChance;
    limit           = (val << 12) / 100;
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    rand            = gRandomLcgState >> 16 & 0xFFF;
    if (rand < limit) {
        arg0->damageOverTimePulse = 0;
        arg0->reactionFlags      |= ENEMY_REACTION_DAMAGE_OVER_TIME;
        gRandomLcgState           = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        arg0->damageOverTimeDelay = (gRandomLcgState >> 16 & ENEMY_DAMAGE_OVER_TIME_DELAY_JITTER) + ENEMY_DAMAGE_OVER_TIME_DELAY_BASE;
        if ((arg1 & 0x8000) == 0) {
            arg0->damageOverTimeGrade = 0;
            return;
        }
        arg0->damageOverTimeGrade = Gp_StateC08.attachId % 10U;
    }
}

s32 Gp_TickObjFlag4(Enemy* arg0)
{
    s32 ret;
    s32 val;
    s32 scale;

    ret = 0;
    arg0->damageOverTimeDelay--;
    if (arg0->damageOverTimeDelay == 0) {
        arg0->damageOverTimePulse++;
        gRandomLcgState           = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        arg0->damageOverTimeDelay = (gRandomLcgState >> 16 & ENEMY_DAMAGE_OVER_TIME_DELAY_JITTER) + ENEMY_DAMAGE_OVER_TIME_DELAY_BASE;
        val                       = arg0->param->hpMax;
        scale                     = D_80113D38[arg0->damageOverTimeGrade];
        ret                       = (val * scale) / 100;
        if (ret == 0) {
            ret = 1;
        }
    }
    return ret;
}

s32 Gp_ObjFlag4Expired(Enemy* arg0)
{
    s32 val;
    s32 ret;

    ret = 0;
    val = arg0->param->damageOverTimeTicks;
    if (!(arg0->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME)) {
        return 1;
    }
    if (val == 0) {
        return 0;
    }
    val = (val * D_80113D28[arg0->damageOverTimeGrade]) / 100;
    if (arg0->damageOverTimePulse >= val) {
        ret = 1;
    }
    return ret;
}

void Gp_SetObjFlag1(Enemy* arg0)
{
    arg0->reactionFlags |= ENEMY_REACTION_STAGGER;
}

void Gp_SetObjFlag2(Enemy* arg0, s32 arg1, s32 arg2)
{
    arg0->buildupStep    = 0;
    arg0->buildupTimer   = 0;
    arg0->reactionFlags |= ENEMY_REACTION_BUILDUP;
    if ((arg1 & 0x8000) == 0) {
        arg0->buildupGrade = 0;
        return;
    }
    if ((arg1 & 0x3F) == 0x31) {
        arg0->buildupGrade = 0;
        return;
    }
    arg0->buildupGrade = Gp_StateC08.attachId % 10U;
}

s32 Gp_TickObjFlag2(Enemy* arg0)
{
    s32 ret;
    s32 limit;
    s32 val;
    s32 scale;

    ret = 0;
    val = arg0->param->buildupSteps;
    if (val == 0) {
        return ret;
    }
    scale = D_80113D30[arg0->buildupGrade];
    limit = (val * scale) / 100;
    if (arg0->buildupStep < limit) {
        arg0->buildupTimer++;
        if (arg0->buildupTimer >= ENEMY_BUILDUP_STEP_FRAMES) {
            arg0->buildupStep++;
            if (arg0->buildupStep >= limit) {
                gRandomLcgState    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                arg0->buildupTimer = gRandomLcgState >> 16 & ENEMY_BUILDUP_COUNTDOWN_MASK;
            } else {
                arg0->buildupTimer = 0;
            }
        }
    } else {
        arg0->buildupTimer--;
        if (arg0->buildupTimer == 0) {
            ret = 1;
        }
    }
    return ret;
}

s32 Gp_GetIdParam2(s32 arg0)
{
    s32 ret;

    if ((arg0 & 0x8000) == 0) {
        ret = Gp_IdParamLo[arg0 & 0x7F].hitCooldown;
    } else {
        ret = Gp_IdParamHi.rows[arg0 & 0x7F].column.hitCooldown;
    }
    return ret;
}
