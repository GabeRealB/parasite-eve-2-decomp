#include "gameplay/damage.h"

#include <psyq/libgte.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "gameplay/actor_render.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "attachments.h"
#include "gameplay/collision.h"
#include "damage.h"
#include "gameplay/enemy.h"
#include "item_menu.h"
#include "item_pickup.h"
#include "items.h"
#include "gameplay/enemy_params.h"
#include "gameplay/weapon_data.h"
#include "weapon_data.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/random.h"
#include "main/mc.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/task_types.h"
#include "main/wipsys.h"

/// 0x20-byte scratch from the scratch stack used by `Gp_RollEnemyChance`.
/// `local` first holds `Enemy.bodyPos`, which `field_18->workm` rotates
/// into `world`; `world` then gets `workm.t[]` added to become a world
/// position, and `local` is reused for the delta against the player
/// coordinate whose length feeds `SquareRoot0`.
typedef struct _GpDistScratch {
    /* 0x00 */ VECTOR3 local;
    /* 0x0C */ s32     pad_C;
    /* 0x10 */ VECTOR3 world;
    /* 0x1C */ s32     pad_1C;
} GpDistScratch;
STATIC_ASSERT_SIZEOF(GpDistScratch, 0x20);

/// Per-sub-id damage rows used by `Gp_ComputeDamage`. The row is the id's
/// `(id >> 8) & 0x3F` nibble pair; the column is the class picked from
/// `D_80113864` (or 5). The selected entry is scaled `<< 8` then / 100.
extern u16 D_80113568[][8];

/// Column table used when `GpRec10.field_4` is 6, indexed by the distance
/// class picked from `D_80113864`. Scaled `<< 12` then / 100 by
/// `Gp_RollEnemyChance`.
extern u16 D_80113858[];

/// Distance/hit class table for `D_80113568`, indexed by `hits / 1000` (or by
/// `SquareRoot0(distance) / 1000` in `Gp_RollEnemyChance`) when that value is
/// below 0x10. `Gp_ComputeDamage` only keeps the low byte of the entry.
extern u16 D_80113864[];

static inline u16 _gpIdParam0(s32 id);

static void Gp_ApplyObjKind(Enemy* arg0, s32 arg1);

static inline u16 _gpIdParam0(s32 id)
{
    if ((id & 0x8000) == 0) {
        return Gp_IdParamLo[id & 0x7F].params[2];
    }
    return Gp_IdParamHi.rows[id & 0x7F].field[5];
}

u16 D_80113568[47][8] = {
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 100, 100, 90, 80, 80, 80, 40, 0 },
    { 100, 80, 70, 60, 50, 50, 30, 10 },
    { 70, 70, 60, 50, 40, 40, 0, 0 },
    { 100, 100, 90, 80, 80, 80, 40, 0 },
    { 100, 100, 90, 80, 80, 80, 40, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 100, 100, 90, 80, 80, 80, 40, 40 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 70, 70, 70, 70, 70, 70, 0, 0 },
    { 70, 70, 70, 70, 70, 70, 0, 0 },
    { 120, 120, 80, 80, 80, 80, 0, 0 },
    { 120, 120, 80, 80, 80, 80, 0, 0 },
    { 120, 120, 80, 80, 80, 80, 0, 0 },
    { 70, 70, 70, 70, 70, 70, 25, 5 },
    { 60, 60, 50, 40, 30, 30, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 100, 100, 100, 100, 100, 100, 0, 15 },
    { 70, 70, 70, 70, 70, 70, 25, 5 },
    { 70, 70, 70, 70, 70, 70, 25, 5 },
    { 100, 100, 100, 100, 100, 100, 0, 0 },
    { 80, 80, 80, 80, 80, 80, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 60, 60, 60, 60, 60, 60, 0, 0 },
    { 70, 70, 70, 70, 70, 70, 20, 5 },
    { 60, 60, 60, 60, 60, 60, 0, 0 },
    { 60, 60, 60, 60, 60, 60, 0, 0 },
    { 70, 70, 70, 70, 70, 70, 0, 5 },
    { 70, 70, 60, 50, 40, 40, 0, 0 },
    { 70, 70, 60, 50, 40, 40, 0, 0 },
    { 70, 70, 60, 50, 40, 40, 0, 0 },
    { 100, 100, 100, 100, 100, 100, 0, 0 },
    { 100, 100, 100, 100, 100, 100, 0, 0 },
    { 100, 100, 100, 100, 100, 100, 0, 0 },
    { 100, 100, 100, 100, 100, 100, 0, 0 },
    { 100, 100, 100, 100, 100, 100, 0, 0 },
    { 100, 100, 100, 100, 100, 100, 0, 0 },
    { 100, 100, 100, 100, 100, 100, 0, 0 },
    { 100, 100, 100, 100, 100, 100, 0, 0 },
    { 100, 100, 100, 100, 100, 100, 0, 0 },
    { 100, 100, 100, 100, 100, 100, 0, 0 },
    { 100, 100, 100, 100, 100, 100, 0, 0 },
    { 100, 100, 100, 100, 100, 100, 0, 0 },
    { 100, 100, 100, 100, 100, 100, 0, 0 },
    { 100, 100, 100, 100, 100, 100, 0, 0 },
};
u16 D_80113858[6] = {
    120,
    100,
    80,
    40,
    40,
    40,
};
u16 D_80113864[16] = {
    0,
    1,
    2,
    2,
    3,
    3,
    3,
    3,
    4,
    4,
    4,
    4,
    4,
    4,
    4,
    4
};

u32 Gp_ComputeDamage(u32 arg0, u32 arg1, s32 arg2, s32 arg3)
{
    u8  flag;
    u32 dmg;

    flag = 0;
    if ((arg0 & 0xFFFF0000) != 0x20000) {
        return 0;
    }

    if ((arg0 & 0x8000) == 0) {
        u8  lo;
        u32 base;
        u32 raw;
        u32 rand;
        s32 pct;
        u8  col;
        s32 sel;
        s32 val;
        s32 mult;
        u32 tmp;
        s32 extra;

        if ((arg0 & 0x80) == 0) {
            if ((arg0 & 0x7F) < 0x21) {
                flag = 1;
            }
        }
        lo   = arg0 & 0x7F;
        arg0 = (arg0 >> 8) & 0x3F;
        raw  = Gp_IdParamLo[lo].params[0];
        base = raw << 8;
        if (flag != 0) {
            if ((Player_Status.statusFlags & PLAYER_STATUS_BERSERKER) != 0) {
                base = base * 150 / 100;
            }
        }

        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        rand            = gRandomLcgState >> 16;
        pct             = (u16)(rand % 20) + 100;
        base            = base * pct / 100;

        col = arg1 / 1000;
        if (col < 0x10) {
            sel = (u8)D_80113864[col];
        } else {
            sel = 5;
        }
        val = (D_80113568[arg0][sel] << 8) / 100;

        if (arg2 == 0) {
            mult = 0x100;
        } else if (Gp_IdParamLo[lo].params[2] == arg2) {
            mult = arg3;
        } else {
            mult = 0x100;
        }

        tmp = base * val >> 8;
        dmg = tmp * mult >> 16;

        if (flag != 0) {
            extra = Gp_StateC08.field_D;
            if (extra != 0) {
                dmg = dmg * D_80113D0C[(extra / 16 - 1) * 2 + (s8)(extra % 16)][0] / 100;
            }
            if (func_800B9D80(0x10000) != 0) {
                dmg = dmg * 120 / 100;
            }
        }

        dmg = dmg * D_80113F90[Gp_StateF0.field_2B] / 100;
        if (dmg == 0) {
            if (base != 0) {
                dmg = 1;
            }
        }
    } else {
        u32 rnd;
        s32 pc;

        dmg = Gp_IdParamHi.rows[arg0 & 0x7F].field[4];
        if ((arg0 & 0x7F) >= 0x19 && (arg0 & 0x7F) < 0x1C) {
            if (Gp_StateF0.field_5 != 0) {
                dmg = dmg / Gp_StateF0.field_5;
            } else {
                dmg = 0;
            }
        }

        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        rnd             = gRandomLcgState >> 16;
        pc              = (u16)(rnd % 10) + 100;
        dmg             = dmg * pc / 100;

        if (func_800B9D80(0x20000) != 0) {
            dmg = dmg * 150 / 100;
        }

        dmg = dmg * D_80113F90[Gp_StateF0.field_2B] / 100;
    }
    return dmg;
}

s32 Gp_ScaleDamage(s32 arg0, s32 arg1, s32* arg2, s32 arg3)
{
    u32 ret;
    s32 lo;
    u32 val;
    s32 extra;
    s32 hp;
    u16 col;

    if ((arg0 & WORLD_COLLISION_CONTACT_KIND_MASK) != DAMAGE_ATTACK_CATEGORY) {
        return 0;
    }

    lo = arg0 & DAMAGE_ATTACK_POWER_MASK;
    if (arg2 != NULL) {
        *arg2 = ((u32)arg0 >> DAMAGE_ATTACK_REACTION_SHIFT) & DAMAGE_ATTACK_REACTION_MASK;
    }

    if (arg3 == 0) {
        hp    = Player_Status.hp;
        col   = D_80113F54[hp / 10];
        val   = Gp_DmgRows[Gp_StateF0.field_2B].field_A[col] << 8;
        extra = Gp_StateC08.field_C;
        if (extra != 0) {
            val = val * D_80113CFC[(extra / 16 - 1) * 2 + (s8)(extra % 16)] / 100;
        }
    } else {
        hp  = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionHp;
        col = D_80113F54[hp / 10];
        val = Gp_DmgRows[Gp_StateF0.field_2B].field_0[col] << 8;
    }

    val = val / 100;
    ret = lo * val >> 8;
    if (ret == 0) {
        if (lo != 0) {
            ret = 1;
        }
    }
    return ret;
}

s32 Gp_RollEnemyChance(Enemy* arg0, u32 arg1, s32 arg2)
{
    Task*          slot;
    GfxCoord*      pcoord;
    u8*            head;
    GpDistScratch* blk;
    s32            dist;
    u16            sel;
    s32            kind;
    s32            col;
    s32            val;
    s32            chance;
    u16            base;
    s32            extra;
    s32            rand;

    slot = gameGetPtrSlot(3);
    if (slot == NULL) {
        return 0;
    }
    if ((arg1 & 0x8000) != 0) {
        return 0;
    }

    base = (arg0->param->critChance << 12) / 100;
    if (base == 0) {
        return 0;
    }

    head                                = SCRATCH_STACK_CURSOR(u8);
    blk                                 = (GpDistScratch*)(head - 0x20);
    SCRATCH_STACK_CURSOR(GpDistScratch) = blk;
    Gp_UpdateCoord(arg0->coord);

    ((VECTOR3*)(head - 0x20))->vx = arg0->bodyPos.vx;
    blk->local.vy                 = arg0->bodyPos.vy;
    blk->local.vz                 = arg0->bodyPos.vz;

    gte_SetRotMatrix(&arg0->coord->workm);
    gte_ldv0(&blk->local);
    gte_rtv0();
    gte_stlvnl(head - 0x10);

    blk->world.vx = arg0->coord->workm.t[0] + blk->world.vx;
    blk->world.vy = arg0->coord->workm.t[1] + blk->world.vy;
    blk->world.vz = arg0->coord->workm.t[2] + blk->world.vz;

    pcoord                        = slot->extra.tmd->coords;
    ((VECTOR3*)(head - 0x20))->vx = blk->world.vx - pcoord->workm.t[0];
    blk->local.vy                 = blk->world.vy - pcoord->workm.t[1];
    blk->local.vz                 = blk->world.vz - pcoord->workm.t[2];

    dist = SquareRoot0(((VECTOR3*)(head - 0x20))->vx * ((VECTOR3*)(head - 0x20))->vx +
                       blk->local.vy * blk->local.vy + blk->local.vz * blk->local.vz);

    sel = dist / 1000;
    sel = sel < 0x10 ? D_80113864[sel] : 5;

    kind = (arg1 >> 8) & 0x3F;
    if (Gp_IdParamLo[arg1 & 0x7F].params[2] == 6) {
        val = (D_80113858[sel] << 12) / 100;
    } else {
        val = (D_80113568[kind][sel] << 12) / 100;
    }

    if ((arg1 & 0x4000) != 0) {
        col = 7;
    } else {
        col = 6;
    }

    chance = (((D_80113568[kind][col] << 12) / 100) * base >> 12) * val >> 12;
    if ((arg0->reactionFlags & ENEMY_REACTION_BUILDUP) != 0) {
        chance <<= 1;
    }

    extra = Gp_StateC08.field_D;
    if (extra != 0) {
        chance = chance * D_80113D0C[(extra / 16 - 1) * 2 + (s8)(extra % 16)][1] / 100;
    }
    if (arg2 != 0) {
        chance *= arg2;
    }

    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    rand            = gRandomLcgState >> 16 & 0xFFF;
    SCRATCH_STACK_RELEASE_BYTES(0x20);
    return rand < chance;
}

static void Gp_ApplyObjKind(Enemy* arg0, s32 arg1)
{
    s32 val;
    s32 limit;
    s32 rand;

    switch (_gpIdParam0(arg1)) {
        case 0:
            break;
        case 1:
            arg0->reactionFlags |= ENEMY_REACTION_STAGGER;
            break;
        case 2:
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
            arg0->buildupGrade = Gp_StateC08.field_0 % 10U;
            break;
        case 3:
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
                arg0->damageOverTimeGrade = Gp_StateC08.field_0 % 10U;
            }
            break;
    }
}

s32 Gp_PackObjPair(Enemy* arg0, s32 arg1)
{
    DamageAttack* pairs;
    s32           ret;

    if (arg0->param == NULL) {
        return 0;
    }
    pairs = arg0->param->attacks;
    ret   = pairs[arg1].power & DAMAGE_ATTACK_POWER_MASK;
    ret  |= (pairs[arg1].reaction & DAMAGE_ATTACK_REACTION_MASK) << DAMAGE_ATTACK_REACTION_SHIFT;
    ret  |= DAMAGE_ATTACK_CATEGORY;
    return ret;
}

s32 Gp_PackPair(DamageAttack* pairs, s32 index)
{
    s32 ret;

    if (pairs == NULL) {
        return 0;
    }
    ret  = pairs[index].power & DAMAGE_ATTACK_POWER_MASK;
    ret |= (pairs[index].reaction & DAMAGE_ATTACK_REACTION_MASK) << DAMAGE_ATTACK_REACTION_SHIFT;
    ret |= DAMAGE_ATTACK_CATEGORY;
    return ret;
}

void func_800E2C78(Enemy* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    s32 val;

    if ((u32)((arg1 & 0x7F) - 0x19) < 3U) {
        val = arg0->hp;
        if ((u32)val < (u32)arg2) {
            Gp_StateF0.field_14 += val;
            return;
        }
        Gp_StateF0.field_14 += arg2;
    }
}
