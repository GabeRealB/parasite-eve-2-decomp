#ifndef ACTOR_101900_FACING_H
#define ACTOR_101900_FACING_H

#include "actors/actor_101900.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/wipsys.h"
#include <psyq/inline_c.h>

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

/// 0xC-byte scratch `Actor01900_Fn07810` takes from `G_SCRATCH_HEAD`: the
/// offset from the actor to the player, then the clamped turn.
typedef struct Actor01900TurnScratch {
    /* 0x0 */ SVECTOR delta;
    /* 0x8 */ s16     angle;
    /* 0xA */ s16     pad_A;
} Actor01900TurnScratch;
STATIC_ASSERT_SIZEOF(Actor01900TurnScratch, 0xC);

/// 0x10-byte scratch `Actor01900_Fn04D14` takes from `G_SCRATCH_HEAD`: the
/// offset from the actor to the player, the player's facing yaw, the yaw from
/// the player back to the actor, the wrapped turn toward the player and the
/// clamped turn applied to the root coordinate.
typedef struct Actor01900ChaseScratch {
    /* 0x0 */ SVECTOR delta;
    /* 0x8 */ s16     playerYaw;
    /* 0xA */ s16     yaw;
    /* 0xC */ s16     turn;
    /* 0xE */ s16     angle;
} Actor01900ChaseScratch;
STATIC_ASSERT_SIZEOF(Actor01900ChaseScratch, 0x10);

extern u8 D_80072729;

/// Step `coord` `amount` units along its local Z axis unless movement is
/// frozen. Same body as `Actor00100_MoveForwardNonzero`.
static __inline__ void Actor01900_MoveForward(GsCOORDINATE2* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;
    SVECTOR* gteVec;

    if (D_80072729 != 1) {
        head                       = *(SVECTOR**)G_SCRATCH_HEAD;
        vec                        = head - 1;
        *(SVECTOR**)G_SCRATCH_HEAD = vec;
        gteVec                     = vec;
        if (amount != 0) {
            SOFT_TOUCH_REG(vec);
            Gfx_MatrixCol2(&coord->coord, vec);
            VectorNormalSS(vec, vec);
            gte_lddp(amount);
            gte_ldsv(gteVec);
            __asm__ volatile("nop; nop; .word 0x4B98003D");
            gte_stsv(gteVec);
            coord->coord.t[0] += head[-1].vx;
            coord->coord.t[1] += vec->vy;
            coord->coord.t[2] += vec->vz;
            coord->flg         = 0;
        }
        *(SVECTOR**)G_SCRATCH_HEAD += 1;
    }
}

/// Step `coord` `amount` units along its local Z axis unless movement is
/// frozen, without `Actor01900_MoveForward`'s zero-amount guard. Same body as
/// `Actor00100_MoveForward`.
static __inline__ void Actor01900_StepForward(GsCOORDINATE2* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;

    if (D_80072729 != 1) {
        head                       = *(SVECTOR**)G_SCRATCH_HEAD;
        vec                        = head - 1;
        *(SVECTOR**)G_SCRATCH_HEAD = vec;
        Gfx_MatrixCol2(&coord->coord, vec);
        VectorNormalSS(vec, vec);
        gte_lddp(amount);
        gte_ldsv(vec);
        __asm__ volatile("nop; nop; .word 0x4B98003D");
        gte_stsv(vec);
        coord->coord.t[0]          += vec->vx;
        coord->coord.t[1]          += vec->vy;
        coord->coord.t[2]          += vec->vz;
        coord->flg                  = 0;
        *(SVECTOR**)G_SCRATCH_HEAD += 1;
    }
}

/// `Actor01900_StepForward` with the X component read back through `head`,
/// as `Actor01900_MoveForward` does.
static __inline__ void Actor01900_StepForwardHead(GsCOORDINATE2* coord, s16 amount)
{
    SVECTOR* head;
    SVECTOR* vec;

    if (D_80072729 != 1) {
        head                       = *(SVECTOR**)G_SCRATCH_HEAD;
        vec                        = head - 1;
        *(SVECTOR**)G_SCRATCH_HEAD = vec;
        Gfx_MatrixCol2(&coord->coord, vec);
        VectorNormalSS(vec, vec);
        gte_lddp(amount);
        gte_ldsv(vec);
        __asm__ volatile("nop; nop; .word 0x4B98003D");
        gte_stsv(vec);
        coord->coord.t[0]          += head[-1].vx;
        coord->coord.t[1]          += vec->vy;
        coord->coord.t[2]          += vec->vz;
        coord->flg                  = 0;
        *(SVECTOR**)G_SCRATCH_HEAD += 1;
    }
}

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

/// `Actor01900_RescaleYaw` with a separate Y scale.
static __inline__ void Actor01900_RescaleYawY(GsCOORDINATE2* coord, s32 scale, s16 scaleY)
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
    blk->scale.vx = scale;
    blk->scale.vy = scaleY;
    blk->scale.vz = scale;
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

/// Rebuild `coord`'s Y rotation from its current yaw at unit scale.
static __inline__ void Actor01900_ResetYaw(GsCOORDINATE2* coord)
{
    void**                scratch;
    void*                 head;
    Actor01900RotScratch* blk;
    s16                   ang;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    blk      = (Actor01900RotScratch*)((u8*)head - 0x34);
    *scratch = blk;

    ang        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->angle = ang;
    Gfx_RotMatrixY(&blk->m, ang, 1);
    blk->scale.vz = 1;
    blk->scale.vy = 1;
    blk->scale.vx = 1;
    ScaleMatrix(&blk->m, &blk->scale);

    coord->coord.m[0][0] = *(u16*)&blk->m.m[0][0];
    coord->coord.m[0][1] = *(u16*)&blk->m.m[0][1];
    coord->coord.m[0][2] = *(u16*)&blk->m.m[0][2];
    coord->coord.m[1][0] = *(u16*)&blk->m.m[1][0];
    coord->coord.m[1][1] = *(u16*)&blk->m.m[1][1];
    coord->coord.m[1][2] = *(u16*)&blk->m.m[1][2];
    coord->coord.m[2][0] = *(u16*)&blk->m.m[2][0];
    coord->coord.m[2][1] = *(u16*)&blk->m.m[2][1];
    coord->coord.m[2][2] = *(u16*)&blk->m.m[2][2];
    coord->flg           = 0;
    *scratch             = (u8*)*scratch + 0x34;
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

static __inline__ void Actor01900_ConfigPositionDelta(PlayerStatus* config, GsCOORDINATE2* coord, SVECTOR* pos)
{
    pos->vx = config->field_4->t[0] - coord->coord.t[0];
    pos->vy = config->field_4->t[1] - coord->coord.t[1];
    pos->vz = config->field_4->t[2] - coord->coord.t[2];
}

/// Yaw from the actor's facing to the player, wrapped; `pos` receives the offset.
static __inline__ s16 Actor01900_PositionYaw(Actor01900* actor, SVECTOR* pos, PlayerStatus* config)
{
    GsCOORDINATE2* coord;
    s32            angle;
    Actor01900_ConfigPositionDelta(config, actor->field_2C->field_8, pos);
    coord = actor->field_2C->field_8;
    angle = ratan2(pos->vx, pos->vz);
    return Actor01900_NormalizeYaw(angle - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
}

/// Walk `p` up its parent chain to `Gfx_ViewCoord`, carrying `out` through
/// each local matrix. Same body as `Actor00100_TransformToView`.
static __inline__ void Actor01900_TransformToView(GsCOORDINATE2* p, SVECTOR* out)
{
    SVECTOR        sv;
    VECTOR         vec;
    s32            flag;
    SVECTOR*       svp   = &sv;
    GsCOORDINATE2* view  = &Gfx_ViewCoord;
    VECTOR*        vecp  = &vec;
    s32*           flagp = &flag;
    sv.vx                = out->vx;
    sv.vy                = out->vy;
    sv.vz                = out->vz;
loop:
    if (p->sub != NULL) {
        if (p != view) {
            gte_SetTransMatrix(&p->coord);
            gte_SetRotMatrix(&p->coord);
            gte_ldv0(svp);
            __asm__ volatile("nop; nop; .word 0x4A480012");
            gte_stlvnl(vecp);
            gte_stflg(flagp);
            sv.vx = vec.vx;
            sv.vy = vec.vy;
            sv.vz = vec.vz;
            p     = p->sub;
            goto loop;
        }
        out->vx = sv.vx;
        out->vy = sv.vy;
        out->vz = sv.vz;
    }
}

#endif
