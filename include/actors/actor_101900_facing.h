#ifndef ACTOR_101900_FACING_H
#define ACTOR_101900_FACING_H

#include "actors/actor_101900.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/wipsys.h"

/* Inline bodies behind `Actor01900_Fn080A8`. Same shapes as
 * `actor_400100_facing.h` and `ActorsShared80135a60`; inlining is what keeps
 * each `G_SCRATCH_HEAD` access out of a register CSE would share. */

/// 0x10-byte scratch `Actor01900_Fn080A8` takes from `G_SCRATCH_HEAD`: the
/// offset from the actor to the player, then the clamped turn applied to the
/// root coordinate.
typedef struct Actor01900AimScratch {
    /* 0x0 */ SVECTOR delta;
    /* 0x8 */ s16     pad_8;
    /* 0xA */ s16     pad_A;
    /* 0xC */ s16     angle;
    /* 0xE */ s16     pad_E;
} Actor01900AimScratch;
STATIC_ASSERT_SIZEOF(Actor01900AimScratch, 0x10);

/// `ActorsShared80135a60`'s body, inlined: rebuild `coord`'s Y rotation from
/// its current yaw, uniformly scaled by `scale`.
static __inline__ void Actor01900_RescaleYaw(GsCOORDINATE2* coord, s16 scale)
{
    void**                scratch;
    void*                 head;
    Actor01900RotScratch* blk;
    s16                   ang;
    u16                   m22;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    blk      = (Actor01900RotScratch*)((u8*)head - 0x34);
    *scratch = blk;

    ang        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->angle = ang;
    Gfx_RotMatrixY(&blk->m, ang, 1);
    blk->scale.vz = scale;
    blk->scale.vy = scale;
    blk->scale.vx = scale;
    ScaleMatrix(&blk->m, &blk->scale);

    coord->coord.m[0][0] = *(u16*)&((Actor01900RotScratch*)((u8*)head - 0x34))->m.m[0][0];
    coord->coord.m[0][1] = *(u16*)&blk->m.m[0][1];
    coord->coord.m[0][2] = *(u16*)&blk->m.m[0][2];
    coord->coord.m[1][0] = *(u16*)&blk->m.m[1][0];
    coord->coord.m[1][1] = *(u16*)&blk->m.m[1][1];
    coord->coord.m[1][2] = *(u16*)&blk->m.m[1][2];
    coord->coord.m[2][0] = *(u16*)&blk->m.m[2][0];
    coord->coord.m[2][1] = *(u16*)&blk->m.m[2][1];
    m22                  = *(u16*)&blk->m.m[2][2];
    *scratch             = (u8*)*scratch + 0x34;
    coord->flg           = 0;
    coord->coord.m[2][2] = m22;
}

/// Wraps a 12-bit angle difference into `[-0x800, 0x800]`.
static __inline__ s16 Actor01900_NormalizeYaw(s16 input)
{
    s16 value = input;
    if (input < 0) {
        while (1) {
            if (value >= -0x800)
                break;
            value += 0x1000;
        }
    } else {
        while (1) {
            if (value <= 0x800)
                break;
            value -= 0x1000;
        }
    }
    return value;
}

static __inline__ void Actor01900_ConfigPositionDelta(WipSysConfig* config, GsCOORDINATE2* coord, SVECTOR* pos)
{
    pos->vx = config->field_4->t[0] - coord->coord.t[0];
    pos->vy = config->field_4->t[1] - coord->coord.t[1];
    pos->vz = config->field_4->t[2] - coord->coord.t[2];
}

/// Yaw from the actor's facing to the player, wrapped; `pos` receives the offset.
static __inline__ s16 Actor01900_PositionYaw(Actor01900* actor, SVECTOR* pos, WipSysConfig* config)
{
    GsCOORDINATE2* coord;
    s32            angle;
    Actor01900_ConfigPositionDelta(config, actor->field_2C->field_8, pos);
    coord = actor->field_2C->field_8;
    angle = ratan2(pos->vx, pos->vz);
    return Actor01900_NormalizeYaw(angle - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
}

#endif
