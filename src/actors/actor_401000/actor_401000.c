#include "common.h"

#include <psyq/inline_c.h>
#include "gte.h"
#include <psyq/abs.h>

#include "actors/actor_401000.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/wipsys.h"

/// `Actor401300_MoveForward`: the plain forward-step helper, the shape the
/// state-8 body `func_actor_401000_801388F4` inlines. The `Nonzero` variant
/// below is the same block with an `amount != 0` test and a `gteVec` copy
/// wrapped around it.
static __inline__ void Actor401000_MoveForward(GsCOORDINATE2* coord, s16 amount)
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
        gte_gpf12();
        gte_stsv(vec);
        coord->coord.t[0]          += head[-1].vx;
        coord->coord.t[1]          += vec->vy;
        coord->coord.t[2]          += vec->vz;
        coord->flg                  = 0;
        *(SVECTOR**)G_SCRATCH_HEAD += 1;
    }
}

/// `Actor401300_MoveForwardNonzero` and `Actor00100_MoveForwardNonzero`, down
/// to the `head[-1].vx` read-back; the step lands in `coord` directly rather
/// than being reported back through the caller's local.
static __inline__ void Actor401000_MoveForwardNonzero(GsCOORDINATE2* coord, s16 amount)
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
            gte_gpf12();
            gte_stsv(gteVec);
            coord->coord.t[0] += head[-1].vx;
            coord->coord.t[1] += vec->vy;
            coord->coord.t[2] += vec->vz;
            coord->flg         = 0;
        }
        *(SVECTOR**)G_SCRATCH_HEAD += 1;
    }
}

/// Integer part of the last delta `func_actor_401000_801323EC` resolved.
extern SVECTOR D_actor_401000_80155010;

/// Moves `coord` in X and Z by the delta `func_800E0C10` resolves from the
/// first `count` records of `recs`, and keeps the integer part of the full
/// delta in `D_actor_401000_80155010`. A nonzero fractional part rounds both
/// the coordinate and the kept step one unit away from zero. Returns 1 when the
/// X or Z delta is nonzero.
s32 func_actor_401000_801323EC(GsCOORDINATE2* coord, GpRec18* recs, s16 count)
{
    void**                scratch;
    u8*                   head;
    Actor401000DeltaFlag* s;
    register void*        p asm("v1");
    s32                   val;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    p        = head - 0x14;
    s        = p;
    *scratch = p;
    s->moved = 0;
    if (func_800E0C10(recs, &s->delta, (s32)count, NULL) != 0) {
        coord->coord.t[0]          = coord->coord.t[0] + ((Actor401000DeltaFlag*)(head - 0x14))->delta.vx.h.hi;
        coord->coord.t[2]          = coord->coord.t[2] + s->delta.vz.h.hi;
        D_actor_401000_80155010.vx = ((Actor401000DeltaFlag*)(head - 0x14))->delta.vx.w >> 16;
        D_actor_401000_80155010.vy = s->delta.vy.w >> 16;
        D_actor_401000_80155010.vz = s->delta.vz.w >> 16;
        val                        = ((Actor401000DeltaFlag*)(head - 0x14))->delta.vx.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[0]++;
                D_actor_401000_80155010.vx++;
            } else {
                coord->coord.t[0]--;
                D_actor_401000_80155010.vx--;
            }
        }
        val = s->delta.vz.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[2]++;
                D_actor_401000_80155010.vz++;
            } else {
                coord->coord.t[2]--;
                D_actor_401000_80155010.vz--;
            }
        }
    }
    if (s->delta.vx.w != 0 || s->delta.vz.w != 0) {
        s->moved = 1;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
    return s->moved;
}

/// Tests whether stepping `coord` forward by `step` keeps the player out of
/// reach. The turn from the actor's facing to the player decides first: for a
/// forward step the player must be within 0x400 of dead ahead, for a backward
/// one outside it, or the answer is 1 at once. Otherwise the root is moved
/// `step` along its facing and the result is whether the player's root is at
/// least `range` + 0x96 away from that point.
s32 func_actor_401000_80132590(GsCOORDINATE2* coord, s16 range, s16 step)
{
    SVECTOR  v;
    SVECTOR  d;
    VECTOR   e;
    Task*    player;
    s16      angle;
    SVECTOR* pv;
    s32      x;

    player = gameGetPtrSlot(3);
    d.vx   = ((GpCoordXZ*)((TmdObject*)player->extra)->coords)->field_18 - ((GpCoordXZ*)coord)->field_18;
    d.vy   = (u16)((TmdObject*)player->extra)->coords->coord.t[1] - (u16)coord->coord.t[1];
    d.vz   = ((GpCoordXZ*)((TmdObject*)player->extra)->coords)->field_20 - ((GpCoordXZ*)coord)->field_20;
    angle  = ratan2(d.vx, d.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    if (angle < 0) {
    loop_neg:
        if (angle < -0x800) {
            angle += 0x1000;
            goto loop_neg;
        }
    } else {
    loop_pos:
        if (angle > 0x800) {
            angle -= 0x1000;
            goto loop_pos;
        }
    }
    x = angle << 16;
    if (step >= 0) {
        if (abs(x >> 16) > 0x400) {
            return 1;
        }
    } else {
        if (abs(x >> 16) < 0x400) {
            return 1;
        }
    }
    Gfx_MatrixCol2(&coord->coord, &v);
    pv = &v;
    VectorNormalSS(pv, pv);
    gte_lddp(step);
    gte_ldsv(pv);
    gte_gpf12();
    gte_stsv(pv);
    v.vx += (u16)coord->coord.t[0];
    v.vy += (u16)coord->coord.t[1];
    v.vz += (u16)coord->coord.t[2];
    e.vx  = ((TmdObject*)player->extra)->coords->coord.t[0] - v.vx;
    e.vy  = ((TmdObject*)player->extra)->coords->coord.t[1] - v.vy;
    e.vz  = ((TmdObject*)player->extra)->coords->coord.t[2] - v.vz;
    return SquareRoot0(e.vx * e.vx + e.vy * e.vy + e.vz * e.vz) >= range + 0x96;
}

/// Rotates the player's and this actor's root positions, each raised by 1000,
/// into world space and returns `func_800E0308` on the pair.
s32 func_actor_401000_80132824(Actor401000* arg0)
{
    Task*                    player;
    u8*                      head;
    Actor401000SightScratch* s;
    SVECTOR*                 local;
    SVECTOR*                 v;
    SVECTOR*                 out;

    player                = gameGetPtrSlot(3);
    head                  = *(u8**)G_SCRATCH_HEAD;
    local                 = (SVECTOR*)(head - 0xC);
    s                     = (Actor401000SightScratch*)(head - 0x1C);
    s->local.vx           = ((Actor401000*)player)->field_2C->coords->coord.t[0];
    s->local.vy           = ((Actor401000*)player)->field_2C->coords->coord.t[1] - 1000;
    *(u8**)G_SCRATCH_HEAD = (u8*)s;
    s->local.vz           = ((Actor401000*)player)->field_2C->coords->coord.t[2];
    Gp_UpdateCoord(&gGfxViewCoord);
    v = local;
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(v);
    gte_rtv0();
    gte_stsv(&s->out);
    s->out.vx += gGfxViewCoord.workm.t[0];
    s->out.vy += gGfxViewCoord.workm.t[1];
    s->out.vz += gGfxViewCoord.workm.t[2];

    s->local.vx = arg0->field_2C->coords->coord.t[0];
    s->local.vy = arg0->field_2C->coords->coord.t[1] - 1000;
    s->local.vz = arg0->field_2C->coords->coord.t[2];
    Gp_UpdateCoord(&gGfxViewCoord);
    out = (SVECTOR*)(head - 0x14);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(v);
    gte_rtv0();
    gte_stsv(out);
    s->from.vx           += gGfxViewCoord.workm.t[0];
    s->from.vy           += gGfxViewCoord.workm.t[1];
    s->from.vz           += gGfxViewCoord.workm.t[2];
    s->hit                = func_800E0308(&s->out, out);
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x1C;
    return s->hit;
}
