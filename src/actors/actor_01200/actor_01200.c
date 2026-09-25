#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "actors/actor.h"
#include "actors/actors_shared_80135990.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3E9C.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"
#include "psyq/abs.h"

/// Work block this overlay parks in `Task::work`. `field_0` is the
/// substate the message handler below switches on; the three bytes at 0x194
/// are the message echo the dispatcher copies in for every 0xB02 message.
typedef struct Actor01200Work {
    /* 0x000 */ s16        field_0;
    /* 0x002 */ s16        field_2;
    /* 0x004 */ s16        field_4;
    /* 0x006 */ s16        field_6; // frame counter within the substate
    /* 0x008 */ s16        field_8;
    /* 0x00A */ byte       pad_A[2];
    /* 0x00C */ GpAnimCtx  anim;
    /* 0x020 */ GpAnimSlot slots[1]; // `func_800B3F84` arg4; later slots overlap the fields below
    /* 0x048 */ byte       pad_48[0x2];
    /* 0x04A */ u16        field_4A; // low ten bits: slot 1's animation id
    /* 0x04C */ byte       pad_4C[0xC];
    /* 0x058 */ u16        field_58;
    /* 0x05A */ byte       pad_5A[0xB6];
    /* 0x110 */ byte       poses[0x60]; // `func_800B3F84` arg3
    /* 0x170 */ s16        field_170;
    /* 0x172 */ s16        field_172;
    /* 0x174 */ s16        field_174;
    /* 0x176 */ s16        field_176;
    /* 0x178 */ s16        field_178;
    /* 0x17A */ s16        field_17A;
    /* 0x17C */ s16        field_17C;
    /* 0x17E */ s16        field_17E;
    /* 0x180 */ byte       pad_180[0x14];
    /* 0x194 */ u8         field_194;
    /* 0x195 */ u8         field_195;
    /* 0x196 */ u8         field_196;
    /* 0x197 */ byte       pad_197[1];
    /* 0x198 */ u16        field_198;
    /* 0x19A */ u16        field_19A;
    /* 0x19C */ byte       pad_19C[0xC];
    /* 0x1A8 */ GpEffArg   eff1A8; // `func_800FDB18`'s argument record
    /* 0x1B0 */ SVECTOR    effOfs; // offset handed to `func_800FDB18`; `pad` picks the coordinate
    /* 0x1B8 */ GpRec18    rec1B8;
    /* 0x1D0 */ byte       pad_1D0[0x60];
    /* 0x230 */ GpObj      obj230;
    /* 0x250 */ GpRec18    rec250;
    /* 0x268 */ byte       pad_268[0x60];
    /* 0x2C8 */ GpObj      obj2C8;
    /* 0x2E8 */ GpRec18    rec2E8;
    /* 0x300 */ GpObj      obj300;
    /* 0x320 */ byte       pad_320[0x18];
    /* 0x338 */ GpObj      obj338;
    /* 0x358 */ SVECTOR    origin;    // model position at spawn
    /* 0x360 */ SVECTOR    patrol[2]; // spawn position plus (0) / minus (1) 1000 units along the facing (XZ)
    /* 0x370 */ s16        patrolIdx;
    /* 0x372 */ byte       pad_372[2];
    /* 0x374 */ MATRIX     lightMtx;      // installed at `TmdObject.lightMtx`
    /* 0x394 */ MATRIX     colorMtx;
    /* 0x3B4 */ MATRIX     savedColorMtx; // colorMtx as it was on entering the death state
    /* 0x3D4 */ u16        field_3D4;     // last animation id the sound check reported
    /* 0x3D6 */ byte       pad_3D6[0x2];
    /* 0x3D8 */ s8         field_3D8;     // nonzero rebuilds the color matrix each tick
    /* 0x3D9 */ byte       pad_3D9[3];
    /* 0x3DC */ s16        field_3DC;
    /* 0x3DE */ byte       pad_3DE[2];
} Actor01200Work;
STATIC_ASSERT_SIZEOF(Actor01200Work, 0x3E0);

/// The ten substate handlers the tick copies onto its stack before dispatching.
typedef struct Actor01200StateTable {
    /* 0x00 */ GpEnemyTaskFunc fn[10];
} Actor01200StateTable;

extern GpPairSrcE Actor01200_D04034;
extern s32        Actor01200_D04044;
extern s32        Actor01200_D04050;
extern u8         Actor01200_D06F98[]; // animation bank handed to `func_800B3F84`
extern u8         Actor01200_D07058[];

/// Integer part of the last movement step `Actor01200_Fn0067C` applied.
extern SVECTOR Actor01200_D07084;

void Actor01200_Fn03D58(GpEnemy* arg0, Task* arg1);
void Actor01200_Fn03DC0(GpEnemy* arg0, Task* arg1);
void Actor01200_Fn03E78(GpEnemy* arg0, Task* arg1);
void Actor01200_Fn03F30(GpEnemy* arg0, Task* arg1);

/// Pushes `coord` away from the obstacles in `recs`. Records of kind 0x10000
/// (which also raises the returned `blocked` flag) or 0x30000 each give a
/// bearing, at most eight; bearings more than 0x400 apart cancel each other.
/// Each survivor becomes a 10-unit step added to `push` and to the translation.
s16 Actor01200_Fn00130(GpCoord* coord, GpRec18* recs, s16 count, SVECTOR* push)
{
    u8*                  head;
    OverlayAvoidScratch* s;
    s16                  diff;
    s16                  t;
    s32                  mag;

    if (gGameSession->viewReady == 1 || Mc_SaveData.field_5C1 == 1) {
        return 0;
    }

    head             = SCRATCH_HEAD(u8);
    SCRATCH_HEAD(u8) = head - sizeof(OverlayAvoidScratch);
    s                = (OverlayAvoidScratch*)SCRATCH_HEAD(u8);
    s->blocked       = 0;
    push->vz         = 0;
    push->vy         = 0;
    push->vx         = 0;

    Gfx_MatrixCol1(&coord->workm, (SVECTOR*)(head - 0x34));
    VectorNormalSS((SVECTOR*)(head - 0x34), (SVECTOR*)(head - 0x34));

    if (ABS(s->dir.vz) < 0x818) {
        s->face = ratan2(-coord->workm.m[2][0], coord->workm.m[2][2]);
    } else {
        s->face = -ratan2(-coord->workm.m[0][2], coord->workm.m[1][2]);
    }

    s->eye.vx = (u16)coord->workm.t[0];
    s->eye.vy = (u16)coord->workm.t[1];
    s->eye.vz = (u16)coord->workm.t[2];
    s->count  = 0;

    for (s->i = 0; s->i < count; s->i++) {
        if (recs[s->i].key == 0) {
            break;
        }
        s->kind = recs[s->i].key & 0xFFFF0000;
        switch (s->kind) {
            case 0x10000:
                s->blocked = 1;
            case 0x30000:
                break;
            default:
                continue;
        }

        if (ABS(s->dir.vz) < 0x818) {
            s->angle[s->count] = overlayBearingXZ((SVECTOR3*)&recs[s->i].point, &s->eye);
        } else {
            s->angle[s->count] = overlayBearingXY((SVECTOR3*)&recs[s->i].point, &s->eye);
        }
        s->ok[s->count] = 1;
        s->count++;
        if (s->count >= 8) {
            break;
        }
    }

    for (s->i = 0; s->i < s->count; s->i++) {
        for (s->j = s->i + 1; s->j < s->count; s->j++) {
            diff = (u16)s->angle[s->i] - (u16)s->angle[s->j];
            t    = diff;
            if (diff < 0) {
            wrapUp:
                if (t < -0x800) {
                    t += 0x1000;
                    goto wrapUp;
                }
            } else {
            wrapDown:
                if (t > 0x800) {
                    t -= 0x1000;
                    goto wrapDown;
                }
            }
            mag     = t;
            s->diff = mag;
            SOFT_BARRIER();
            if (mag < 0) {
                mag = -mag;
            }
            if (mag >= 0x401) {
                s->ok[s->i] = 0;
                s->ok[s->j] = 0;
            }
        }
        if (s->ok[s->i] != 0) {
            diff = ((u16)s->angle[s->i] - (u16)s->face) +
                   ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
            s->diff = diff;
            Gfx_RotMatrixY(&s->m, diff, 1);
            Gfx_MatrixCol2(&s->m, &s->dir);
            VectorNormalSS(&s->dir, &s->dir);
            gte_lddp(-10);
            gte_ldsv(&s->dir);
            gte_gpf12();
            gte_stsv(&s->dir);
            push->vx          += s->dir.vx;
            push->vz          += s->dir.vz;
            coord->coord.t[0] += s->dir.vx;
            coord->coord.t[2] += s->dir.vz;
        }
    }

    SCRATCH_POP_BYTES(sizeof(OverlayAvoidScratch));
    return s->blocked != 0;
}

/// Steps `coord` by the push the first `count` contact records in `movement`
/// resolve to, and latches the integer part of that push in
/// `Actor01200_D07084`. When a component's fractional half is nonzero the
/// coordinate and the latched step move one unit further from zero. Returns
/// nonzero when the X or Z push is nonzero.
s32 Actor01200_Fn0067C(GpCoord* coord, GpRec18* movement, s16 count)
{
    void**            scratch;
    u8*               head;
    OverlayDeltaFlag* s;
    register void*    p asm("v1");
    s32               val;

    scratch                        = SCRATCH_HEAD_ADDR;
    head                           = SCRATCH_HEAD_AT(scratch, void);
    p                              = head - 0x14;
    s                              = p;
    SCRATCH_HEAD_AT(scratch, void) = p;
    s->moved                       = 0;
    if (func_800E0C10(movement, &s->delta, (s32)count, NULL) != 0) {
        coord->coord.t[0]    = coord->coord.t[0] + ((OverlayDeltaFlag*)(head - 0x14))->delta.vx.h.hi;
        coord->coord.t[2]    = coord->coord.t[2] + s->delta.vz.h.hi;
        Actor01200_D07084.vx = ((OverlayDeltaFlag*)(head - 0x14))->delta.vx.w >> 16;
        Actor01200_D07084.vy = s->delta.vy.w >> 16;
        Actor01200_D07084.vz = s->delta.vz.w >> 16;
        val                  = ((OverlayDeltaFlag*)(head - 0x14))->delta.vx.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[0]++;
                Actor01200_D07084.vx++;
            } else {
                coord->coord.t[0]--;
                Actor01200_D07084.vx--;
            }
        }
        val = s->delta.vz.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[2]++;
                Actor01200_D07084.vz++;
            } else {
                coord->coord.t[2]--;
                Actor01200_D07084.vz--;
            }
        }
    }
    if (s->delta.vx.w != 0 || s->delta.vz.w != 0) {
        s->moved = 1;
    }
    SCRATCH_POP_BYTES(0x14);
    return s->moved;
}

/// Restarts animation slots 1 to 5 on the requested animation `field_174`, at
/// the rate `field_176 + field_178`, and records it as the running one.
static __inline__ void Actor01200_ResetSlots(Actor01200Work* arg0)
{
    Actor01200Work* work = arg0;
    s32             i;

    for (i = 1; i < 6; i++) {
        work->slots[i].rate = work->field_176 + work->field_178;
        Gp_AnimResetSlot(&work->anim, i, work->field_174);
    }
    work->field_172 = work->field_174;
}

/// Advances animation slots 1 to 5 by one frame at the rate
/// `field_176 + field_178`.
static __inline__ void Actor01200_TickSlots(Task* arg0)
{
    Actor01200Work* work;
    s32             i;

    work = arg0->work;
    for (i = 1; i < 6; i++) {
        work->slots[i].rate = work->field_176 + work->field_178;
        Gp_AnimTickIndex(&work->anim, i);
    }
}

/// Motion driver the state handlers run every frame. A start request in
/// `field_170` (1 or 2) restarts the animation slots and clears the frame
/// counters `field_17A` / `field_17C`; while running (3) it advances the
/// slots, counts frames in `field_17A` and, while bit 1 of `field_58` is set,
/// in `field_17C` as well.
void Actor01200_Fn00820(Task* arg0)
{
    Actor01200Work* work;

    work = arg0->work;
    if (work->field_170 == 1) {
        Actor01200_ResetSlots(work);
        work->field_170 = 3;
        work->field_17A = 0;
        work->field_17C = 0;
    } else if (work->field_170 == 2) {
        Actor01200_ResetSlots(work);
        work->field_170 = 3;
        work->field_17A = 0;
        work->field_17C = 0;
    } else if (work->field_170 == 3) {
        work->field_17A++;
        Actor01200_TickSlots(arg0);
        if (work->field_58 & 2) {
            work->field_17C++;
        }
    }
}

/// Sound check for the tick: for animations 2 and 3 (`field_174`) it returns
/// sound 0x400C0001 the first time the low ten bits of `field_4A` reach one of
/// that animation's two trigger values, latching the value in `field_3D4` so
/// it reports once; for animation 4 it returns 0x400C0005 while bit 0 of
/// `field_58` is set. Returns 0 otherwise.
s32 Actor01200_Fn00990(Actor01200Work* arg0)
{
    u16 id;
    s32 v;

    switch (arg0->field_174) {
        case 2:
            id = arg0->field_4A & 0x3FF;
            v  = id;
            if (v != 0x15) {
                goto not15;
            }
        check:
            if (arg0->field_3D4 == v) {
                goto same;
            }
            arg0->field_3D4 = id;
            return 0x400C0001;
        not15:
            if (v == 0x11) {
                goto check;
            }
        clear:
            arg0->field_3D4 = 0;
            break;
        case 3:
            id = arg0->field_4A & 0x3FF;
            v  = id;
            if (v != 0xD && v != 0x12) {
                goto clear;
            }
            goto check;
        same:
            arg0->field_3D4 = id;
            break;
        case 4:
            if (arg0->field_58 & 1) {
                return 0x400C0005;
            }
            break;
    }
    return 0;
}

void Actor01200_Fn00A6C(GpEnemy* arg0, Task* arg1)
{
    TmdObject*      obj;
    GpCoord*        coord;
    GpCoord*        part;
    Actor01200Work* work;
    GpRec18*        hits;
    SVECTOR         sv;
    VECTOR          pos;
    SVECTOR*        p;
    SVECTOR*        q;
    GpObj*          o1;
    GpObj*          o2;
    GpObj*          o3;
    GpObj*          o4;

    obj        = arg1->extra.tmd;
    coord      = obj->coords;
    work       = memCalloc(sizeof(Actor01200Work), 0);
    arg1->work = work;
    if (work == NULL) {
        Gp_DestroyEnemy(arg0, arg1);
        return;
    }
    arg1->msgTable = Actor01200_D07058;
    coord->sub     = &gGfxViewCoord;
    obj->flags     = 0;
    func_800B3F84(&work->anim, Actor01200_D06F98, obj, work->poses, work->slots);

    o1           = &work->obj230;
    o1->ctx.recs = &work->rec1B8;
    o1->pos.vy   = -0x34;
    o1->coord    = coord;
    o1->pos.vx   = 0;
    o1->pos.vz   = 0;
    o1->key      = 0x3000C;
    o1->radius   = 0xB4;
    o1->flags    = 1;
    Gp_LinkObj(2, o1);
    o1->flags |= 0x4000;
    Gp_InitRec18Table(o1->ctx.recs, 5, 0);

    o2           = &work->obj2C8;
    sv.vx        = 0;
    sv.vy        = -0x168;
    sv.vz        = 0;
    p            = &sv;
    hits         = &work->rec250;
    o2->coord    = arg1->extra.tmd->coords + 2;
    o2->ctx.recs = hits;
    o2->pos.vx   = p->vx;
    o2->pos.vy   = p->vy;
    o2->pos.vz   = p->vz;
    o2->key      = 0x3000C;
    o2->radius   = 0x168;
    o2->flags    = 1;
    Gp_LinkObj(2, o2);
    o2->flags |= 0x8000;
    Gp_InitRec18Table(o2->ctx.recs, 5, 0);

    o3           = &work->obj300;
    sv.vx        = 0;
    sv.vy        = 0;
    sv.vz        = 0;
    o3->coord    = &gGfxViewCoord;
    o3->ctx.recs = &work->rec2E8;
    o3->pos.vx   = p->vx;
    o3->pos.vy   = p->vy;
    o3->pos.vz   = p->vz;
    o3->radius   = 0x500;
    o3->flags    = 1;
    Gp_LinkObj(3, o3);
    Gp_InitRec18Table(o3->ctx.recs, 1, 0);

    o4           = &work->obj338;
    o4->coord    = &gGfxViewCoord;
    o4->ctx.recs = (GpRec18*)work->pad_320;
    o4->pos.vx   = p->vx;
    o4->pos.vy   = p->vy;
    o4->pos.vz   = p->vz;
    o4->radius   = 0x80;
    o4->flags    = 1;
    Gp_LinkObj(8, o4);
    Gp_InitRec18Table(o4->ctx.recs, 1, 0);

    arg0->field_4    = &coord->coord;
    arg0->field_48   = 0;
    arg0->bodyPos.vx = 0;
    arg0->bodyPos.vy = 0;
    arg0->bodyPos.vz = 0;
    arg0->coord      = arg1->extra.tmd->coords + 2;
    Gp_LinkNode(&arg0->node);
    arg0->node.state.b.flags = 1;
    arg0->hp = arg0->hpMax = 1;
    arg0->reactionFlags    = 0;
    arg0->hp = arg0->hpMax = Actor01200_D04034.hpMax;
    arg0->param            = &Actor01200_D04034;
    arg0->recs             = hits;
    work->field_170        = 2;
    work->field_174        = 1;
    work->field_176        = 0x10;
    work->field_178        = 0;
    Actor01200_Fn00820(arg1);
    work->field_17E = 0;
    work->field_8   = 0;
    obj->lightMtx   = &work->lightMtx;
    obj->colorMtx   = &work->colorMtx;
    coord->flg      = 0;
    Gp_UpdateCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1];
    pos.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0, &pos, 0, 0);
    work->field_198 = 5;
    work->field_19A = 0x14;
    if ((u16)(arg0->placeKey >> 12) % 2 == 1) {
        work->field_176 += arg0->placeKey >> 12;
        work->field_19A += arg0->placeKey >> 12;
        work->field_198 += arg0->placeKey >> 12;
    } else {
        work->field_176 -= (u16)(arg0->placeKey >> 12) / 2;
        work->field_19A -= arg0->placeKey >> 13;
        work->field_198 -= arg0->placeKey >> 13;
    }
    work->origin.vx = arg1->extra.tmd->coords->coord.t[0];
    work->origin.vy = arg1->extra.tmd->coords->coord.t[1];
    work->origin.vz = arg1->extra.tmd->coords->coord.t[2];
    Gfx_MatrixCol2(&arg1->extra.tmd->coords->coord, &sv);
    sv.vy = 0;
    q     = &sv;
    VectorNormalSS(q, q);
    gte_lddp(1000);
    gte_ldsv(q);
    gte_gpf12();
    gte_stsv(q);
    work->patrol[0].vx = arg1->extra.tmd->coords->coord.t[0] + sv.vx;
    work->patrol[0].vy = arg1->extra.tmd->coords->coord.t[1];
    work->patrol[0].vz = arg1->extra.tmd->coords->coord.t[2] + sv.vz;
    work->patrol[1].vx = arg1->extra.tmd->coords->coord.t[0] - sv.vx;
    work->patrol[1].vy = arg1->extra.tmd->coords->coord.t[1];
    work->patrol[1].vz = arg1->extra.tmd->coords->coord.t[2] - sv.vz;
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    if ((arg1->spawnArg1 >> 16) == 0) {
        work->field_0 = 7;
    } else if ((arg1->spawnArg1 >> 16) == 1) {
        work->field_0 = 2;
    } else {
        work->field_0 = 7;
    }
    work->field_2           = -1;
    part                    = arg1->extra.tmd->coords;
    work->eff1A8.spawnArgLo = 0x80;
    work->eff1A8.spawnArgHi = 2;
    work->eff1A8.coord      = part + 1;
    arg1->state++;
}

void Actor01200_Fn01040(GpEnemy* arg0, Task* arg1)
{
    Actor01200Work* work;
    GpCoord*        coord;
    SVECTOR         delta;
    SVECTOR*        d;
    TmdObject*      obj;

    work = arg1->work;
    if (work->field_4 != 0) {
        obj                      = arg1->extra.tmd;
        arg0->node.state.b.flags = 0;
        obj->flags               = 0;
        work->field_174          = 5;
        work->field_170          = 1;
        work->field_178          = 0;
        work->obj2C8.flags      |= 0x8000;
        work->obj300.flags      &= 0x7FFF;
        work->obj338.flags      &= 0x7FFF;
        work->obj230.flags      |= 0x4000;
        Actor01200_Fn00820(arg1);
        return;
    }
    Actor01200_Fn00820(arg1);
    if ((work->field_58 & 2) && work->field_17C >= 0x19) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if (!((Gp_LcgState >> 0x10) & 7)) {
            work->field_0 = 3;
        }
    }
    coord    = arg1->extra.tmd->coords;
    d        = &delta;
    delta.vx = Player_Status.coordMtx->t[0] - coord->coord.t[0];
    d->vy    = Player_Status.coordMtx->t[1] - coord->coord.t[1];
    d->vz    = Player_Status.coordMtx->t[2] - coord->coord.t[2];
    if (!overlayOutOfRange(d, 2000)) {
        Gp_ArmStateF0(1);
        work->field_0 = 3;
    }
}

void Actor01200_Fn01234(GpEnemy* arg0, Task* arg1)
{
    Actor01200Work*   work;
    GpCoord*          coord;
    GpCoord*          facing;
    GpCoord*          part;
    TmdObject*        obj;
    ActorTurnScratch* head;
    ActorTurnScratch* s;

    work = arg1->work;
    if (work->field_4 != 0) {
        obj                      = arg1->extra.tmd;
        arg0->node.state.b.flags = 0;
        obj->flags               = 0;
        work->field_174          = 3;
        work->field_170          = 1;
        work->field_178          = 0x10;
        work->obj2C8.flags      |= 0x8000;
        work->obj300.flags      &= 0x7FFF;
        work->obj338.flags      &= 0x7FFF;
        work->obj230.flags      |= 0x4000;
        Actor01200_Fn00820(arg1);
        work->field_3DC = 0;
        Gp_ArmStateF0(1);
        return;
    }
    head                           = SCRATCH_HEAD(ActorTurnScratch);
    SCRATCH_HEAD(ActorTurnScratch) = head - 1;
    s                              = head - 1;
    Actor01200_Fn00820(arg1);
    coord             = arg1->extra.tmd->coords;
    head[-1].delta.vx = Player_Status.coordMtx->t[0] - coord->coord.t[0];
    s->delta.vy       = Player_Status.coordMtx->t[1] - coord->coord.t[1];
    s->delta.vz       = Player_Status.coordMtx->t[2] - coord->coord.t[2];
    facing            = arg1->extra.tmd->coords;
    s->angle          = actorNormalizeYaw(ratan2(head[-1].delta.vx, s->delta.vz) - ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]));
    if (s->angle > 0x10) {
        s->angle = 0x10;
    }
    if (s->angle < -0x10) {
        s->angle = -0x10;
    }
    part      = arg1->extra.tmd->coords;
    s->angle += ratan2(-part->coord.m[2][0], part->coord.m[2][2]);
    Gfx_RotMatrixY(&arg1->extra.tmd->coords->coord, s->angle, 1);
    actorStepForward(arg1->extra.tmd->coords, 0x14);
    Actor01200_Fn0067C(arg1->extra.tmd->coords, &work->rec1B8, 5);
    if (overlayOutOfRange(&s->delta, 1000)) {
        work->field_3DC++;
    } else {
        work->field_3DC = 0;
    }
    if (!overlayOutOfRange(&s->delta, 1000)) {
        work->field_8++;
    } else {
        work->field_8 = 0;
    }
    if (work->field_8 >= 0x15) {
        work->field_0 = 5;
    }
    if (Actor01200_Fn00130(arg1->extra.tmd->coords, &work->rec250, 5, &s->delta) == 1) {
        work->field_0 = 6;
    }
    arg1->extra.tmd->coords->flg = 0;
    s->delta.vx                  = work->origin.vx - arg1->extra.tmd->coords->coord.t[0];
    s->delta.vy                  = 0;
    s->delta.vz                  = work->origin.vz - arg1->extra.tmd->coords->coord.t[2];
    overlayOutOfRange(&s->delta, 3000);
    if (work->field_3DC >= 0xF1) {
        work->field_0 = 8;
    }
    SCRATCH_POP(ActorTurnScratch);
}

static __inline__ void Actor01200_FaceScale(GpCoord* coord, s16 s)
{
    ActorScaleRotScratch* head;
    ActorScaleRotScratch* sc;

    head                               = SCRATCH_HEAD(ActorScaleRotScratch);
    sc                                 = head - 1;
    SCRATCH_HEAD(ActorScaleRotScratch) = sc;
    sc->angle                          = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    Gfx_RotMatrixY(&sc->m, sc->angle, 1);
    sc->scale.vx = sc->scale.vy = sc->scale.vz = s;
    ScaleMatrix(&sc->m, &head[-1].scale);
    coord->coord.m[0][0] = head[-1].m.m[0][0];
    coord->coord.m[0][1] = sc->m.m[0][1];
    coord->coord.m[0][2] = sc->m.m[0][2];
    coord->coord.m[1][0] = sc->m.m[1][0];
    coord->coord.m[1][1] = sc->m.m[1][1];
    coord->coord.m[1][2] = sc->m.m[1][2];
    coord->coord.m[2][0] = sc->m.m[2][0];
    coord->coord.m[2][1] = sc->m.m[2][1];
    coord->coord.m[2][2] = sc->m.m[2][2];
    coord->flg           = 0;
    SCRATCH_POP(ActorScaleRotScratch);
}

void Actor01200_Fn017DC(GpEnemy* arg0, Task* arg1)
{
    SVECTOR         ofs;
    VECTOR          scale;
    Actor01200Work* work;
    TmdObject*      obj;
    s16             s;
    s32             pan;
    s32             id;

    work = arg1->work;
    obj  = arg1->extra.tmd;
    memset(&ofs, 0, 8);
    if (work->field_4 != 0) {
        arg0->node.state.b.flags = 1;
        obj->flags               = 0;
        work->obj2C8.flags      &= 0x7FFF;
        work->obj300.flags      &= 0x7FFF;
        work->obj338.flags      &= 0x7FFF;
        work->obj300.key         = Gp_PackObjPair(arg0, 0);
        work->obj338.key         = 0x22121;
        work->field_6            = 0;
        work->obj230.flags      |= 0x4000;
        work->savedColorMtx      = work->colorMtx;
        work->field_174          = 0xA;
        work->field_170          = 1;
        work->field_178          = 8;
        Actor01200_Fn00820(arg1);
        work->obj338.pos.vx = arg1->extra.tmd->coords->coord.t[0];
        work->obj338.pos.vy = arg1->extra.tmd->coords->coord.t[1] - 0x190;
        work->obj338.pos.vz = arg1->extra.tmd->coords->coord.t[2];
        work->obj300.pos.vx = arg1->extra.tmd->coords->coord.t[0];
        work->obj300.pos.vy = arg1->extra.tmd->coords->coord.t[1];
        work->obj300.pos.vz = arg1->extra.tmd->coords->coord.t[2];
        return;
    }
    Actor01200_Fn00820(arg1);
    switch ((s16)(work->field_6 - 0x29)) {
        case 0:
            arg1->extra.tmd->flags |= 2;
            ofs.vx                  = 0x1E;
            ofs.vz                  = 0x1E;
            ofs.vy                  = -0xA;
            Gp_SpawnEff(0x60030, arg1->extra.tmd->coords, 0x10100, &ofs);
            ofs.vy = -0x14;
            ofs.vz = -0x50;
            Gp_SpawnEff(0x60030, arg1->extra.tmd->coords, 0x10100, &ofs);
            break;
        case 1:
            work->eff1A8.coord      = &arg1->extra.tmd->coords[4];
            work->eff1A8.spawnArgLo = 0x120;
            work->eff1A8.spawnArgHi = 2;
            func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, &arg1->extra.tmd->coords[4], NULL, &work->eff1A8);
            Gp_SpawnScript18Ex((s32)&Actor01200_D04044, (s32)&Actor01200_D04050, (s16)gpGetObjDepth(arg1->extra.tmd->coords));
            work->obj300.radius = 0x320;
            work->obj338.radius = 0xC8;
            work->obj300.flags |= 0x8000;
            work->obj338.flags |= 0x8000;
            Gp_SpawnEff(0x6009C, &arg1->extra.tmd->coords[2], 1, NULL);
            break;
        case 2:
            work->obj338.radius = 0x190;
            work->obj300.flags &= 0x7FFF;
            break;
        case 3:
            work->eff1A8.coord      = &arg1->extra.tmd->coords[1];
            work->eff1A8.spawnArgLo = 0x80;
            work->eff1A8.spawnArgHi = 2;
            func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, &arg1->extra.tmd->coords[1], NULL, &work->eff1A8);
            work->obj338.radius = 0x320;
            break;
        case 5:
            work->obj338.flags &= 0x7FFF;
            break;
        case 7:
            work->eff1A8.coord      = &arg1->extra.tmd->coords[1];
            work->eff1A8.spawnArgLo = 0x200;
            work->eff1A8.spawnArgHi = 2;
            func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, &arg1->extra.tmd->coords[1], NULL, &work->eff1A8);
            Gp_SpawnEff(0x6009E, arg1->extra.tmd->coords, 0, &ofs);
            id  = ((arg0->placeKey >> 12) << 8) | 0x400C0004;
            pan = (s8)Gp_GetObjPan(arg1->extra.tmd->coords);
            SndEvt_EnqueueType6(id, pan, (s8)gpGetObjDepth(arg1->extra.tmd->coords));
            break;
        case 9:
            obj->flags = 0x80;
            break;
        case 28:
            Gp_ReleaseStateF0Add(arg1, 0xC);
            work->field_0 = 0;
            break;
        default:
            work->colorMtx = work->savedColorMtx;
            break;
    }
    work->colorMtx = work->savedColorMtx;
    if ((u16)(work->field_6 - 0x17) < 0x12) {
        work->colorMtx.t[0] += ((s16)work->field_6 - 0x16) * 0x60;
    }
    if ((u16)(work->field_6 - 0x2A) < 9) {
        s = 0xBB8 - ((s16)work->field_6 - 0x2A) * 0x258;
        if (s < 0x4B0) {
            scale.vx = scale.vy = scale.vz = 0;
            ScaleMatrix(&work->colorMtx, &scale);
            gte_lddp(0);
            gte_ldlvl(work->colorMtx.t);
            gte_gpf12();
            gte_stlvl(work->colorMtx.t);
            Actor01200_FaceScale(arg1->extra.tmd->coords, 0x1000);
        } else {
            scale.vx = scale.vy = scale.vz = s;
            work->colorMtx                 = work->savedColorMtx;
            ScaleMatrix(&work->colorMtx, &scale);
            gte_lddp(s);
            gte_ldlvl(work->colorMtx.t);
            gte_gpf12();
            gte_stlvl(work->colorMtx.t);
            s = ((s16)work->field_6 - 0x28) * 0x400 + 0x1000;
            if (s > 0x2000) {
                s = 0x2000;
            }
            Actor01200_FaceScale(arg1->extra.tmd->coords, s);
        }
    }
    if ((s16)work->field_6 < 0x400) {
        work->field_6++;
    } else {
        work->field_0 = 0;
    }
}

void Actor01200_Fn01FDC(GpEnemy* arg0, Task* arg1)
{
    SVECTOR         ofs;
    VECTOR          scale;
    Actor01200Work* work;
    TmdObject*      obj;
    s16             s;
    s32             pan;
    s32             id;

    work = arg1->work;
    obj  = arg1->extra.tmd;
    if (work->field_4 != 0) {
        arg0->node.state.b.flags = 1;
        obj->flags               = 0;
        work->obj2C8.flags      &= 0x7FFF;
        work->obj300.flags      &= 0x7FFF;
        work->obj338.flags      &= 0x7FFF;
        work->obj300.key         = Gp_PackObjPair(arg0, 0);
        work->obj338.key         = 0x22121;
        work->field_6            = 0;
        work->obj230.flags      &= 0xBFFF;
        work->savedColorMtx      = work->colorMtx;
        work->field_174          = 0xA;
        work->field_170          = 1;
        work->field_178          = 0;
        work->field_176          = 0x2C;
        Actor01200_Fn00820(arg1);
        work->obj338.pos.vx = arg1->extra.tmd->coords->coord.t[0];
        work->obj338.pos.vy = arg1->extra.tmd->coords->coord.t[1] - 0x190;
        work->obj338.pos.vz = arg1->extra.tmd->coords->coord.t[2];
        work->obj300.pos.vx = arg1->extra.tmd->coords->coord.t[0];
        work->obj300.pos.vy = arg1->extra.tmd->coords->coord.t[1];
        work->obj300.pos.vz = arg1->extra.tmd->coords->coord.t[2];
        return;
    }
    Actor01200_Fn00820(arg1);
    switch ((s16)(work->field_6 - 0xD)) {
        case 0:
            ofs.vx = 0x1E;
            ofs.vz = 0x1E;
            ofs.vy = -0x3C;
            Gp_SpawnEff(0x60030, arg1->extra.tmd->coords, 0x10080, &ofs);
            ofs.vy = -0xA;
            ofs.vz = -0x50;
            Gp_SpawnEff(0x60030, arg1->extra.tmd->coords, 0x10030, &ofs);
            id  = ((arg0->placeKey >> 12) << 8) | 0x400C0004;
            pan = (s8)Gp_GetObjPan(arg1->extra.tmd->coords);
            SndEvt_EnqueueType6(id, pan, (s8)gpGetObjDepth(arg1->extra.tmd->coords));
            arg1->extra.tmd->flags = 2;
            break;
        case 1:
            work->obj300.radius = 0x320;
            work->obj300.flags |= 0x8000;
            Gp_SpawnEff(0x6009C, &arg1->extra.tmd->coords[2], 1, NULL);
            Gp_SpawnScript18((s32)&Actor01200_D04044, (s32)&Actor01200_D04050);
            work->eff1A8.coord      = &arg1->extra.tmd->coords[4];
            work->eff1A8.spawnArgLo = 0x120;
            work->eff1A8.spawnArgHi = 2;
            func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, &arg1->extra.tmd->coords[4], NULL, &work->eff1A8);
            break;
        case 2:
            work->obj338.radius = 0xC8;
            work->obj300.flags &= 0x7FFF;
            work->obj338.flags |= 0x8000;
            break;
        case 3:
            work->obj338.radius     = 0x190;
            work->eff1A8.coord      = &arg1->extra.tmd->coords[2];
            work->eff1A8.spawnArgLo = 0x100;
            work->eff1A8.spawnArgHi = 2;
            func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, &arg1->extra.tmd->coords[2], NULL, &work->eff1A8);
            break;
        case 4:
            work->obj338.radius = 0x320;
            break;
        case 6:
            work->obj338.flags &= 0x7FFF;
            ofs.vx              = arg1->extra.tmd->coords->coord.t[0];
            ofs.vy              = arg1->extra.tmd->coords->coord.t[1];
            ofs.vz              = arg1->extra.tmd->coords->coord.t[2];
            Gp_SpawnEff(0x6009E, &gGfxViewCoord, 0, &ofs);
            work->eff1A8.coord      = &arg1->extra.tmd->coords[1];
            work->eff1A8.spawnArgLo = 0x200;
            work->eff1A8.spawnArgHi = 2;
            func_800FDB18(Gp_GetIdParam1(0x1001) & 0xFFFF, &arg1->extra.tmd->coords[1], NULL, &work->eff1A8);
            break;
        case 8:
            obj->flags = 0x80;
            break;
        case 25:
            Gp_ReleaseStateF0Add(arg1, 0xC);
            work->field_0 = 0;
            break;
    }
    if ((u16)(work->field_6 - 0xD) < 9) {
        s = 0xBB8 - ((s16)work->field_6 - 0xB) * 0x320;
        if (s < 0) {
            s = 0;
        }
        scale.vx = scale.vy = scale.vz = s;
        work->colorMtx                 = work->savedColorMtx;
        ScaleMatrix(&work->colorMtx, &scale);
        gte_lddp(s);
        gte_ldlvl(work->colorMtx.t);
        gte_gpf12();
        gte_stlvl(work->colorMtx.t);
        s = (s16)work->field_6 * 0xB4 + 0x1000;
        if (s > 0x2000) {
            s = 0x2000;
        }
        Actor01200_FaceScale(arg1->extra.tmd->coords, s);
    }
    if ((s16)work->field_6 < 0x400) {
        work->field_6++;
    } else {
        work->field_0 = 0;
    }
}

/// Hit spark: picks one of two offsets at random for the quadrant the hit
/// angle `arg1` falls in (front, back, right or left), each with the model
/// coordinate it is relative to, keeps it in `work->effOfs` and spawns hit id
/// `arg2`'s effect there.
void Actor01200_Fn026A0(Task* arg0, s16 arg1, u32 arg2)
{
    SVECTOR*        sc;
    Actor01200Work* work;
    s32             mag;
    GpCoord*        coord;

    sc   = (SVECTOR*)SCRATCH_PUSH_BYTES(sizeof(SVECTOR));
    mag  = (arg1 >= 0) ? arg1 : -arg1;
    work = arg0->work;
    if (mag < 0x200) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if (!((Gp_LcgState >> 16) & 1)) {
            sc->pad = 2;
            sc->vx  = 80;
            sc->vy  = -180;
            sc->vz  = 330;
        } else {
            sc->pad = 2;
            sc->vx  = -60;
            sc->vy  = -150;
            sc->vz  = 300;
        }
    } else if (mag > 0x600) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if (!((Gp_LcgState >> 16) & 1)) {
            sc->pad = 1;
            sc->vx  = 0;
            sc->vy  = 0;
            sc->vz  = -180;
        } else {
            sc->pad = 2;
            sc->vx  = 2;
            sc->vy  = -50;
            sc->vz  = -50;
        }
    } else if (arg1 > 0) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if (!((Gp_LcgState >> 16) & 1)) {
            sc->pad = 5;
            sc->vx  = 100;
            sc->vy  = 0;
            sc->vz  = 0;
        } else {
            sc->pad = 5;
            sc->vx  = 120;
            sc->vy  = 0;
            sc->vz  = 100;
        }
    } else {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if (!((Gp_LcgState >> 16) & 1)) {
            sc->pad = 4;
            sc->vx  = -100;
            sc->vy  = 0;
            sc->vz  = 0;
        } else {
            sc->pad = 4;
            sc->vx  = -120;
            sc->vy  = 0;
            sc->vz  = 100;
        }
    }
    coord                   = &arg0->extra.tmd->coords[1];
    work->eff1A8.spawnArgLo = 0x80;
    work->eff1A8.spawnArgHi = 2;
    work->eff1A8.coord      = coord;
    work->effOfs            = *sc;
    func_800FDB18(Gp_GetIdParam1(arg2) & 0xFFFF, &arg0->extra.tmd->coords[sc->pad], &work->effOfs, &work->eff1A8);
    SCRATCH_POP_BYTES(sizeof(SVECTOR));
}

/// Hit check: finds the first type-2 record among the five at `rec250`, and
/// on a hit applies its damage, turns the model toward it and, once the hit
/// points run out, moves to substate 6.
void Actor01200_Fn02918(GpEnemy* arg0, Task* arg1)
{
    ActorHitTakenScratch* sc;
    Actor01200Work*       work;
    GpRec18*              recs;
    SVECTOR*              pos;
    s32                   mask;
    s32                   kind;
    s32                   id;
    s16                   angle;
    s16                   i;

    work = arg1->work;
    sc   = (ActorHitTakenScratch*)SCRATCH_PUSH_BYTES(sizeof(ActorHitTakenScratch));
    pos  = &sc->pos;
    recs = &work->rec250;
    i    = 0;
    mask = 0xFFFF0000;
    kind = 0x20000;
scan:
    if (recs[i].key == 0) {
        goto missed;
    }
    if ((recs[i].key & mask) == kind) {
        pos->vx = recs[i].point.vx;
        pos->vy = recs[i].point.vy;
        pos->vz = recs[i].point.vz;
        id      = recs[i].key;
        goto found;
    }
    i++;
    if (i < 5) {
        goto scan;
    }
missed:
    id = 0;
found:
    sc->id = id;

    if (id != 0) {
        sc->dmg                      = Gp_ComputeDamage(sc->id, 0, 0, 0x1000);
        arg1->extra.tmd->coords->flg = 0;
        Gp_UpdateCoord(arg1->extra.tmd->coords);
        sc->d.vx = arg1->extra.tmd->coords->workm.t[0];
        sc->d.vy = arg1->extra.tmd->coords->workm.t[1];
        sc->d.vz = arg1->extra.tmd->coords->workm.t[2];
        sc->d.vx = sc->pos.vx - arg1->extra.tmd->coords->workm.t[0];
        sc->d.vy = sc->pos.vy - arg1->extra.tmd->coords->workm.t[1];
        sc->d.vz = sc->pos.vz - arg1->extra.tmd->coords->workm.t[2];
        angle    = ratan2(sc->d.vx, sc->d.vz) -
                ratan2(-arg1->extra.tmd->coords->workm.m[2][0], arg1->extra.tmd->coords->workm.m[2][2]);
        sc->angle = angle;
        sc->angle = actorWrapAngle(angle);
        Actor01200_Fn026A0(arg1, sc->angle, sc->id);
        func_800DA6E8(&arg0->node, sc->dmg, 0);
        arg0->hp -= sc->dmg;
        if (arg0->hp <= 0) {
            arg0->spawnState = 0;
            work->field_0    = 6;
        }
    }
    SCRATCH_POP_BYTES(sizeof(ActorHitTakenScratch));
}

/// Patrol between the two `patrol` points: turn at most 0x20 toward the current
/// one, step 5 units, and swap points within 400 units or after 0x60 blocked
/// frames; state 6 when `Actor01200_Fn00130` reports 1, state 4 when the
/// player is within 2000 units and inside a quarter turn or 1000 units.
void Actor01200_Fn02BE8(GpEnemy* arg0, Task* arg1)
{
    Actor01200Work*   work;
    ActorTurnScratch* head;
    ActorTurnScratch* sc;
    GpCoord*          coord;
    GpCoord*          target;
    TmdObject*        obj;
    s16               angle;

    work = arg1->work;
    if (work->field_4 != 0) {
        obj                      = arg1->extra.tmd;
        arg0->node.state.b.flags = 0;
        obj->flags               = 0;
        work->field_174          = 2;
        work->field_170          = 1;
        work->field_178          = 0;
        work->patrolIdx          = 0;
        work->obj2C8.flags      |= 0x8000;
        work->obj300.flags      &= 0x7FFF;
        work->obj338.flags      &= 0x7FFF;
        work->obj230.flags      |= 0x4000;
        Actor01200_Fn00820(arg1);
        work->field_6 = 0;
        return;
    }
    head                           = SCRATCH_HEAD(ActorTurnScratch);
    SCRATCH_HEAD(ActorTurnScratch) = head - 1;
    sc                             = head - 1;
    head[-1].delta.vx              = work->patrol[work->patrolIdx].vx - arg1->extra.tmd->coords->coord.t[0];
    sc->delta.vy                   = 0;
    sc->delta.vz                   = work->patrol[work->patrolIdx].vz - arg1->extra.tmd->coords->coord.t[2];
    coord                          = arg1->extra.tmd->coords;
    angle                          = ratan2(head[-1].delta.vx, sc->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    sc->angle                      = actorNormalizeYaw(angle);
    if (sc->angle > 0x20) {
        sc->angle = 0x20;
    }
    if (sc->angle < -0x20) {
        sc->angle = -0x20;
    }
    sc->angle += ratan2(-arg1->extra.tmd->coords->coord.m[2][0], arg1->extra.tmd->coords->coord.m[2][2]);
    Gfx_RotMatrixY(&arg1->extra.tmd->coords->coord, sc->angle, 1);
    actorStepForward(arg1->extra.tmd->coords, 5);
    if (Actor01200_Fn0067C(arg1->extra.tmd->coords, &work->rec1B8, 5)) {
        work->field_6++;
    }
    if (!overlayOutOfRange(&sc->delta, 400) || work->field_6 > 0x60) {
        if (work->patrolIdx == 0) {
            work->patrolIdx = 1;
        } else {
            work->patrolIdx = 0;
        }
        work->field_6 = 0;
    }
    if (Actor01200_Fn00130(arg1->extra.tmd->coords, &work->rec250, 5, &sc->delta) == 1) {
        work->field_0 = 6;
    }
    target       = arg1->extra.tmd->coords;
    sc->delta.vx = Player_Status.coordMtx->t[0] - target->coord.t[0];
    sc->delta.vy = Player_Status.coordMtx->t[1] - target->coord.t[1];
    sc->delta.vz = Player_Status.coordMtx->t[2] - target->coord.t[2];
    if (!overlayOutOfRange(&sc->delta, 2000)) {
        coord = arg1->extra.tmd->coords;
        angle = ratan2(sc->delta.vx, sc->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
        if (actorNormalizeYaw(angle) < 0x400 || !overlayOutOfRange(&sc->delta, 1000)) {
            work->field_0 = 4;
        }
    }
    Actor01200_Fn00820(arg1);
    arg1->extra.tmd->coords->flg = 0;
    if ((work->field_58 & 2) && work->field_17C > 0x14) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if (!((Gp_LcgState >> 0x10) & 7)) {
            work->field_0 = 1;
        }
    }
    SCRATCH_POP(ActorTurnScratch);
}

/// Walk back toward the spawn point: turn at most 0x10 toward it, step 8 units,
/// and hand over to state 7 once within 0x50 or after 0xDD frames (state 6 when
/// `Actor01200_Fn00130` reports 1).
void Actor01200_Fn03294(GpEnemy* arg0, Task* arg1)
{
    Actor01200Work*   work;
    GpCoord*          coord;
    GpCoord*          facing;
    TmdObject*        obj;
    ActorTurnScratch* head;
    ActorTurnScratch* s;

    work = arg1->work;
    if (work->field_4 != 0) {
        obj                      = arg1->extra.tmd;
        arg0->node.state.b.flags = 0;
        obj->flags               = 0;
        work->field_174          = 2;
        work->field_170          = 1;
        work->field_178          = 0;
        work->obj2C8.flags      |= 0x8000;
        work->obj300.flags      &= 0x7FFF;
        work->obj338.flags      &= 0x7FFF;
        work->obj230.flags      |= 0x4000;
        Actor01200_Fn00820(arg1);
        work->field_3DC = 0;
        work->field_6   = 0;
        return;
    }
    head                           = SCRATCH_HEAD(ActorTurnScratch);
    SCRATCH_HEAD(ActorTurnScratch) = head - 1;
    s                              = head - 1;
    Actor01200_Fn00820(arg1);
    arg1->extra.tmd->coords->flg = 0;
    head[-1].delta.vx            = work->origin.vx - arg1->extra.tmd->coords->coord.t[0];
    s->delta.vy                  = 0;
    s->delta.vz                  = work->origin.vz - arg1->extra.tmd->coords->coord.t[2];
    coord                        = arg1->extra.tmd->coords;
    s->angle                     = actorNormalizeYaw(ratan2(head[-1].delta.vx, s->delta.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    if (s->angle > 0x10) {
        s->angle = 0x10;
    }
    if (s->angle < -0x10) {
        s->angle = -0x10;
    }
    facing    = arg1->extra.tmd->coords;
    s->angle += ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
    Gfx_RotMatrixY(&arg1->extra.tmd->coords->coord, s->angle, 1);
    actorStepForward(arg1->extra.tmd->coords, 8);
    Actor01200_Fn0067C(arg1->extra.tmd->coords, &work->rec1B8, 5);
    work->field_6++;
    if (!overlayOutOfRange(&s->delta, 0x50) || work->field_6 >= 0xDD) {
        work->field_0 = 7;
    }
    if (Actor01200_Fn00130(arg1->extra.tmd->coords, &work->rec250, 5, &s->delta) == 1) {
        work->field_0 = 6;
    }
    SCRATCH_POP(ActorTurnScratch);
}

const Actor01200StateTable Actor01200_D000E4 = {
    {
        Actor01200_Fn03D58,
        Actor01200_Fn03DC0,
        Actor01200_Fn01040,
        Actor01200_Fn03E78,
        Actor01200_Fn01234,
        Actor01200_Fn017DC,
        Actor01200_Fn01FDC,
        Actor01200_Fn02BE8,
        Actor01200_Fn03294,
        Actor01200_Fn03F30,
    }
};

/// Per-frame tick: refreshes the coordinate and color, handles the render
/// mode in `Gp_StateF0.field_4`, dispatches the substate handler and plays its sound.
void Actor01200_Fn036B0(GpEnemy* arg0, Task* arg1)
{
    VECTOR               pos;
    Actor01200StateTable table;
    Actor01200Work*      work;
    s32                  snd;
    s32                  pan;
    s32                  id;

    work                         = arg1->work;
    table                        = Actor01200_D000E4;
    arg1->extra.tmd->coords->flg = 0;
    Gp_UpdateCoord(arg1->extra.tmd->coords);
    pos.vx = arg1->extra.tmd->coords->workm.t[0];
    pos.vy = arg1->extra.tmd->coords->workm.t[1];
    pos.vz = arg1->extra.tmd->coords->workm.t[2];
    Gp_UpdateActorColor((struct GpEnemy*)arg0, &pos, 0, 0);
    switch (Gp_StateF0.field_4) {
        case 0:
            if (work->field_0 != 0 && work->field_0 != 6 && work->field_0 != 5) {
                arg1->extra.tmd->flags = 0;
                Gp_DrawEffGroundQuad((VECTOR3*)arg1->extra.tmd->coords->workm.t, 0x180, Gp_State1C->groundShade);
            }
            break;
        case 1:
            if (work->field_0 != 0 && work->field_0 != 6 && work->field_0 != 5) {
                arg1->extra.tmd->flags = 0;
                Gp_DrawEffGroundQuad((VECTOR3*)arg1->extra.tmd->coords->workm.t, 0x180, Gp_State1C->groundShade);
            }
            Gp_ClearRec18Occupied(&work->rec1B8);
            Gp_ClearRec18Occupied(&work->rec250);
            Gp_ClearRec18Occupied(&work->rec2E8);
            return;
        case 2:
            arg1->extra.tmd->flags = 0x80;
            Gp_ClearRec18Occupied(&work->rec1B8);
            Gp_ClearRec18Occupied(&work->rec250);
            Gp_ClearRec18Occupied(&work->rec2E8);
            return;
    }
    if (work->field_2 != work->field_0) {
        work->field_4 = 1;
    } else {
        work->field_4 = 0;
    }
    work->field_2 = work->field_0;
    table.fn[work->field_0](arg0, arg1);
    if (arg0->hp > 0) {
        Actor01200_Fn02918(arg0, arg1);
        if (arg0->hp <= 0) {
            work->field_0 = 6;
        }
    }
    Gp_ClearRec18Occupied(&work->rec1B8);
    Gp_ClearRec18Occupied(&work->rec250);
    Gp_ClearRec18Occupied(&work->rec2E8);
    id = Actor01200_Fn00990(work);
    if (id != 0) {
        snd = id | ((arg0->placeKey >> 12) << 8);
        pan = (s8)Gp_GetObjPan(arg1->extra.tmd->coords);
        SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(arg1->extra.tmd->coords));
    }
    if (work->field_3D8 != 0) {
        func_800D7A9C(arg1->extra.tmd, (VECTOR*)arg1->extra.tmd->coords->workm.t, 0, 3);
    }
    if (gGameSession->viewReady != 0) {
        arg1->extra.tmd->coords->flg = 0;
    }
}

/// The actor's three task states: spawn, per-frame tick and teardown.
const GpEnemyTaskFuncTable3 Actor01200_D0010C = {
    {
        Actor01200_Fn00A6C,
        Actor01200_Fn036B0,
        Gp_DestroyEnemy,
    }
};

/// Display mode handler for the model (`Task::extra`), selected by `arg2`:
/// 0 hides it and 1 shows it, both reinstating its buffers and moving to
/// state 7; 2 sets its flag 0x4 and 3 replaces its flags with 0x4, both moving
/// to state 0. `arg1` is unused.
s32 Actor01200_Fn03A00(Task* task, s32 arg1, s32 arg2)
{
    TmdObject*      obj;
    Actor01200Work* work;

    obj  = task->extra.tmd;
    work = task->work;
    switch (arg2) {
        case 0:
            obj->flags = 0x80;
            Tmd_AllocBuffers(obj);
            work->field_0 = 7;
            break;
        case 1:
            obj->flags = 0;
            Tmd_AllocBuffers(obj);
            work->field_0 = 7;
            break;
        case 2:
            obj->flags   |= 4;
            work->field_0 = 0;
            break;
        case 3:
            obj->flags    = 0;
            work->field_0 = 0;
            obj->flags   |= 4;
            break;
    }
    return 0;
}

s32 Actor01200_Fn03ABC(Task* arg0, s32 arg1, GpCmdArg* arg2)
{
    Actor01200Work* work;
    GpEnemy*        ctx;

    work            = arg0->work;
    ctx             = arg0->spawnArg2;
    work->field_194 = arg2->from.loc.stage;
    work->field_195 = arg2->from.loc.area;
    work->field_196 = (u8)arg2->command;
    if (arg2->from.key == 0xB02) {
        switch (arg2->command) {
            case 0:
                work->field_0 = 0;
                break;
            case 1:
            case 2:
                break;
            case 3:
                if (ctx->hp > 0) {
                    work->field_0                = 4;
                    arg0->extra.tmd->coords->flg = 0;
                }
                break;
            case 4:
                if (ctx->hp > 0) {
                    work->field_0 = 9;
                }
                break;
        }
    }
    return 0;
}

/// Places the task's model from `placement`: the three longs become the
/// coordinate's translation, then the X, Y and Z angles are applied in that
/// order and the coordinate is marked dirty. Always returns 1.
s32 Actor01200_Fn03B70(Task* task, s32 arg1, GpXformArg* placement)
{
    task->extra.tmd->coords->coord.t[0] = placement->pos.vx;
    task->extra.tmd->coords->coord.t[1] = placement->pos.vy;
    task->extra.tmd->coords->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixX(&task->extra.tmd->coords->coord, placement->rot.vx, 1);
    Gfx_RotMatrixY(&task->extra.tmd->coords->coord, placement->rot.vy, 0);
    Gfx_RotMatrixZ(&task->extra.tmd->coords->coord, placement->rot.vz, 0);
    task->extra.tmd->coords->flg = 0;
    return 1;
}

/// Rebuilds `coord`'s rotation as its current yaw alone, uniformly scaled by
/// `scale`, working in a block borrowed from the scratch stack, and marks the
/// coordinate dirty.
void Actor01200_Fn03C40(GpCoord* coord, s16 scale)
{
    void**                scratch;
    ActorScaleRotScratch* head;
    ActorScaleRotScratch* blk;
    s16                   ang;
    u16                   m22;

    scratch                                        = SCRATCH_HEAD_ADDR;
    head                                           = SCRATCH_HEAD_AT(scratch, ActorScaleRotScratch);
    blk                                            = head - 1;
    SCRATCH_HEAD_AT(scratch, ActorScaleRotScratch) = blk;

    ang        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->angle = ang;
    Gfx_RotMatrixY(&blk->m, ang, 1);
    blk->scale.vz = scale;
    blk->scale.vy = scale;
    blk->scale.vx = scale;
    ScaleMatrix(&blk->m, &blk->scale);

    coord->coord.m[0][0] = (u16)(head - 1)->m.m[0][0];
    coord->coord.m[0][1] = (u16)blk->m.m[0][1];
    coord->coord.m[0][2] = (u16)blk->m.m[0][2];
    coord->coord.m[1][0] = (u16)blk->m.m[1][0];
    coord->coord.m[1][1] = (u16)blk->m.m[1][1];
    coord->coord.m[1][2] = (u16)blk->m.m[1][2];
    coord->coord.m[2][0] = (u16)blk->m.m[2][0];
    coord->coord.m[2][1] = (u16)blk->m.m[2][1];
    m22                  = (u16)blk->m.m[2][2];
    SCRATCH_POP_AT(scratch, ActorScaleRotScratch);
    coord->flg           = 0;
    coord->coord.m[2][2] = m22;
}

/// State 0, the idle state: on entry (`field_4` set) it marks the enemy not lockable,
/// hides the model, and turns off the collision bodies the other states
/// enable - the pair pass of `obj2C8`, `obj300` and `obj338`, and the grid
/// pass of `obj230`. Nothing happens afterwards.
void Actor01200_Fn03D58(GpEnemy* arg0, Task* arg1)
{
    Actor01200Work* work;
    TmdObject*      obj;

    work = arg1->work;
    if (work->field_4 != 0) {
        obj                      = arg1->extra.tmd;
        arg0->node.state.b.flags = 1;
        obj->flags               = (u16)(obj->flags | 0x80);
        work->obj2C8.flags       = (u16)(work->obj2C8.flags & 0x7FFF);
        work->obj300.flags       = (u16)(work->obj300.flags & 0x7FFF);
        work->obj338.flags       = (u16)(work->obj338.flags & 0x7FFF);
        work->obj230.flags       = (u16)(work->obj230.flags & 0xBFFF);
    }
}

/// State 1: on entry (`field_4` set) clear the actor and model flags, set
/// `field_174` to 4, and set or clear the high bits of the four sub-object
/// flags; afterwards run `Actor01200_Fn00820` and move to state 2 once bit 0
/// of `field_58` is set.
void Actor01200_Fn03DC0(GpEnemy* arg0, Task* arg1)
{
    Actor01200Work* work;
    TmdObject*      obj;

    work = arg1->work;
    if (work->field_4 != 0) {
        obj                      = arg1->extra.tmd;
        arg0->node.state.b.flags = 0;
        obj->flags               = 0;
        work->field_174          = 4;
        work->field_170          = 1;
        work->field_178          = 0;
        work->obj2C8.flags      |= 0x8000;
        work->obj300.flags      &= 0x7FFF;
        work->obj338.flags      &= 0x7FFF;
        work->obj230.flags      |= 0x4000;
        Actor01200_Fn00820(arg1);
        return;
    }
    Actor01200_Fn00820(arg1);
    if (work->field_58 & 1) {
        work->field_0 = 2;
    }
}

/// State 3: on entry (`field_4` set) clear the actor and model flags, set
/// `field_174` to 6, and set or clear the high bits of the four sub-object
/// flags; afterwards run `Actor01200_Fn00820` and move to state 7 once bit 0
/// of `field_58` is set.
void Actor01200_Fn03E78(GpEnemy* arg0, Task* arg1)
{
    Actor01200Work* work;
    TmdObject*      obj;

    work = arg1->work;
    if (work->field_4 != 0) {
        obj                      = arg1->extra.tmd;
        arg0->node.state.b.flags = 0;
        obj->flags               = 0;
        work->field_174          = 6;
        work->field_170          = 1;
        work->field_178          = 0;
        work->obj2C8.flags      |= 0x8000;
        work->obj300.flags      &= 0x7FFF;
        work->obj338.flags      &= 0x7FFF;
        work->obj230.flags      |= 0x4000;
        Actor01200_Fn00820(arg1);
        return;
    }
    Actor01200_Fn00820(arg1);
    if (work->field_58 & 1) {
        work->field_0 = 7;
    }
}

/// On entry (`field_4` set) clear the actor and model flags, set `field_174`
/// to 2, and set or clear the high bits of the four sub-object flags; then run
/// `Actor01200_Fn00820` and clear the model's coordinate flag every frame.
void Actor01200_Fn03F30(GpEnemy* arg0, Task* arg1)
{
    Actor01200Work* work;
    TmdObject*      obj;

    work = arg1->work;
    if (work->field_4 != 0) {
        obj                      = arg1->extra.tmd;
        arg0->node.state.b.flags = 0;
        obj->flags               = 0;
        work->field_174          = 2;
        work->field_170          = 1;
        work->field_178          = 0;
        work->obj2C8.flags      |= 0x8000;
        work->obj300.flags      &= 0x7FFF;
        work->obj338.flags      &= 0x7FFF;
        work->obj230.flags      |= 0x4000;
    }
    Actor01200_Fn00820(arg1);
    arg1->extra.tmd->coords->flg = 0;
}

/// Task entry point: runs the handler for the task's current state from a
/// stack copy of `Actor01200_D0010C`.
void Actor01200_Fn03FD4(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor01200_D0010C;
    sp.funcs[task->state](task->spawnArg2, task);
}
