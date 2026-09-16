#include "common.h"

#include "actors/actor_110600.h"
#include "actors/actors_shared_8013411c.h"
#include "actors/actors_shared_80135a60.h"

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

#include <psyq/inline_c.h>

/// Global freeze flag the walker's turn step bails out on: 1 while the game is
/// paused.
extern u8 D_80072728;

/// Global freeze flag the walker's per-tick step zeroes the movement of: 1
/// while the game is paused.
extern u8 D_80072729;

/// Table of 0x80-byte actor config blocks the walker's `field_6E` byte indexes
/// for the position state 1 steers towards.
extern WipSysConfig D_80073B08[];

#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_801322CC);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80132470);

void func_actor_110600_80132654(Actor110600Walker* work, SVECTOR3* pos)
{
    Actor110600WalkerRoute* route;
    Actor110600WalkerRoute* step;
    Actor110600WalkerRoute* wrap;
    Actor110600WalkerRoute* next;
    u8                      node;

    route      = work->route;
    work->node = route->nodes[route->cursor];
    if (func_actor_110600_80132470(work) == 0) {
        pos->vx              = work->nav->nodes[work->node].x;
        pos->vy              = work->nav->nodes[work->node].y;
        pos->vz              = work->nav->nodes[work->node].z;
        work->route->arrived = 0;
        return;
    }

    work->route->arrived = 1;
    step                 = work->route;
    work->field_62       = 0;
    work->field_64       = 0;
    step->cursor++;

    wrap = work->route;
    if (wrap->nodes[wrap->cursor] == 0xFF) {
        wrap->cursor = 0;
    }

    next       = work->route;
    node       = next->nodes[next->cursor];
    work->node = node;
    pos->vx    = work->nav->nodes[node].x;
    pos->vy    = work->nav->nodes[work->node].y;
    pos->vz    = work->nav->nodes[work->node].z;
}

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_801327EC);

/// Returns the patrol node nearest the walker: the squared XZ distance between
/// each node and the low halfwords of the walker coordinate's translation,
/// with the running best and the cursor staged in a scratch block. Same body
/// as the acropolis bridge room's `func_acropolis_bridge_8018450C`.
u8 func_actor_110600_80132958(Actor110600Walker* work)
{
    Actor110600NearScratch* block;
    u8*                     head;
    s16                     dz;

    head                  = *(u8**)G_SCRATCH_HEAD;
    *(u8**)G_SCRATCH_HEAD = head - 0x14;
    block                 = (Actor110600NearScratch*)*(u8**)G_SCRATCH_HEAD;

    block->best = -1;
    for (block->node = 0; block->node < work->nav->count; block->node++) {
        block->dx   = *(u16*)&work->coord->coord.t[0] - work->nav->nodes[block->node].x;
        dz          = *(u16*)&work->coord->coord.t[2] - work->nav->nodes[block->node].z;
        block->dz   = dz;
        block->dist = block->dx * block->dx + dz * dz;
        if (block->dist < block->best || block->best == -1) {
            block->best    = block->dist;
            block->nearest = block->node;
        }
    }
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x14;
    return block->nearest;
}

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80132A84);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80132D54);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80132FE0);

/// Bearing of `pos` from the walker's coordinate translation in the XZ plane,
/// measured in a delta block of its own that is released before `ratan2` runs.
static __inline__ s32 Actor110600CoordBearingXZ(SVECTOR3* pos, GsCOORDINATE2* coord)
{
    u8*                    head;
    Actor110600AvoidDelta* d;

    head                  = *(u8**)G_SCRATCH_HEAD;
    d                     = (Actor110600AvoidDelta*)(head - 0x10);
    d->vx                 = pos->vx - coord->coord.t[0];
    *(u8**)G_SCRATCH_HEAD = (u8*)d;
    d->vy                 = pos->vy - coord->coord.t[1];
    d->vz                 = pos->vz - coord->coord.t[2];
    *(u8**)G_SCRATCH_HEAD = head;
    return ratan2(d->vx, d->vz);
}

/// Turns the walker towards `pos` by at most `field_5A` angle units a frame.
/// The wrapped relative bearing drives the consecutive-turn counter, then
/// becomes the absolute yaw the model's saved scale matrix is rebuilt around.
void func_actor_110600_80133550(Actor110600Walker* work, SVECTOR3* pos)
{
    Actor110600TurnScratch* s;
    GsCOORDINATE2*          coord;
    u8*                     head;
    s16                     diff, t;
    s32                     angle;
    u16                     frames;

    if (D_80072728 == 1)
        return;
    head                  = *(u8**)G_SCRATCH_HEAD;
    *(u8**)G_SCRATCH_HEAD = head - 0x1C;
    s                     = (Actor110600TurnScratch*)(head - 0x1C);
    coord                 = work->coord;
    diff                  = Actor110600CoordBearingXZ(pos, coord) -
           ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    t = diff;
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
    angle    = t;
    s->angle = angle;
    if (angle != 0)
        work->field_62++;
    else
        work->field_62 = 0;
    frames = (u16)work->field_62;
    CLOBBER_REG(v0);
    SOFT_USE_REG(frames);
    work->field_64 = 0;
    SOFT_COMPILER_BARRIER();
    if ((u16)work->field_5A + (u16)work->field_64 < s->angle)
        s->angle = (u16)work->field_5A + (u16)work->field_64;
    if (s->angle < -((u16)work->field_5A + (u16)work->field_64))
        s->angle = -((u16)work->field_5A + (u16)work->field_64);
    if ((u16)work->field_5A == 0)
        s->angle = 0;
    s->angle += ratan2(-work->coord->coord.m[2][0], work->coord->coord.m[2][2]);
    __builtin_memcpy(work->coord->coord.m, work->scaleMtx.m, sizeof(work->scaleMtx.m));
    Gfx_RotMatrixY(&work->coord->coord, s->angle, 0);
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x1C;
}

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80133778);

/// The walker's per-tick body, open on the scratch frame `func_actor_110600_80133A94`
/// hands it. State 1 heads straight for the position the `D_80073B08` motion
/// config indexed by `field_6E` holds, state 2 re-runs the patrol steering and
/// re-reads `nav`'s byte table at `cursor` whenever the step or one of the
/// three node bytes changed, and state 3 follows the patrol route proper. The
/// scalar at `field_5E` then ramps towards `field_5C` by `field_60` a frame;
/// while it is non-zero it scales (`GPF`) the normalised facing column of the
/// model matrix into the per-frame world step, which is added to the
/// coordinate's translation and kept in `moveStep`. `D_80072729` (a global
/// freeze flag) zeroes the step instead. Written as an inline so the two
/// scratch-head accesses inside one frame stay absolute; see
/// `func_acropolis_bridge_8018532C` in `acropolis_bridge_12.c`, the same body.
static __inline__ void Actor110600_WalkerStep(Actor110600Walker* walker, u8* head,
                                              Actor110600WalkScratch* block)
{
    u8*            head2;
    SVECTOR3*      pos;
    WipSysConfig*  cfg;
    SVECTOR*       sv;
    SVECTOR*       gsv;
    SVECTOR*       step;
    GsCOORDINATE2* coord;
    s16            sdiff;
    s32            diff;
    s16            speed;
    s32            cur;
    s32            target;
    s32            result;

    switch (walker->state) {
        case 0:
            break;
        case 1:
            cfg                            = &D_80073B08[walker->field_6E];
            pos                            = (SVECTOR3*)(head - 0x24);
            ((SVECTOR3*)(head - 0x24))->vx = *(u16*)&cfg->field_4->t[0];
            pos->vy                        = *(u16*)&cfg->field_4->t[1];
            pos->vz                        = *(u16*)&cfg->field_4->t[2];
            break;
        case 2:
            *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD - 4;
            walker->field_6F      = func_actor_110600_801327EC(walker, 1);
            walker->field_70      = func_actor_110600_80132958(walker);
            if (walker->field_69 != walker->state || walker->field_70 != walker->field_72 ||
                walker->field_6F != walker->field_71) {
                func_actor_110600_80132A84(walker, 1);
                walker->node = walker->nav->field_4[walker->cursor];
            }
            walker->field_69 = walker->state;
            walker->field_72 = walker->field_70;
            walker->field_71 = walker->field_6F;
            if (func_actor_110600_80132470(walker) != 0) {
                walker->cursor       += (u8)walker->field_73;
                walker->node          = walker->nav->field_4[walker->cursor];
                *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 4;
            }
            break;
        case 3:
            func_actor_110600_80132654(walker, (SVECTOR3*)(head - 0x24));
            break;
    }
    func_actor_110600_80133550(walker, &block->pos);

    cur    = walker->field_5C;
    target = walker->field_5E;
    if (cur != target) {
        diff  = cur - target;
        sdiff = diff;
        if (sdiff > walker->field_60) {
            result = target + walker->field_60;
        } else if (sdiff < -walker->field_60) {
            result = target - walker->field_60;
        } else {
            result = target + diff;
        }
        walker->field_5E = result;
    }

    coord = walker->coord;
    speed = walker->field_5E;
    step  = &walker->moveStep;
    if (D_80072729 == 1) {
        step->vz            = 0;
        step->vy            = 0;
        walker->moveStep.vx = 0;
    } else {
        head2                 = *(u8**)G_SCRATCH_HEAD;
        sv                    = (SVECTOR*)(head2 - 8);
        *(u8**)G_SCRATCH_HEAD = (u8*)sv;
        gsv                   = sv;
        if (speed != 0) {
            Gfx_MatrixCol2(&coord->coord, sv);
            VectorNormalSS(sv, sv);
            gte_lddp(speed);
            gte_ldsv(gsv);
            gte_gpf12_real();
            gte_stsv(gsv);
            coord->coord.t[0] += ((SVECTOR*)(head2 - 8))->vx;
            coord->coord.t[1] += sv->vy;
            coord->coord.t[2] += sv->vz;
            walker->moveStep   = *(SVECTOR*)(head2 - 8);
            coord->flg         = 0;
        }
        *(u8**)G_SCRATCH_HEAD += 8;
    }
    if (walker->field_6C == 0) {
        func_actor_110600_80132D54(walker);
    }
    if (walker->field_6D == 0) {
        func_actor_110600_80132FE0(walker);
    }
}

/// Per-tick walker step: advances the animation the `field_68` byte selects,
/// resolves the patrol node the `field_6E` byte names against `D_80073B08`,
/// and ramp-scales the model matrix between `field_5E` and `field_5C`. The
/// working frame is carved off `G_SCRATCH_HEAD` and handed back once the
/// coordinate has been rebuilt. Same body as the acropolis bridge room's
/// `func_acropolis_bridge_8018532C`.
void func_actor_110600_80133A94(Actor110600Walker* walker)
{
    u8*                     head;
    Actor110600WalkScratch* block;

    head                  = *(u8**)G_SCRATCH_HEAD;
    *(u8**)G_SCRATCH_HEAD = head - 0x28;
    block                 = (Actor110600WalkScratch*)*(u8**)G_SCRATCH_HEAD;
    Actor110600_WalkerStep(walker, head, block);
    walker->coord->flg    = 0;
    *(u8**)G_SCRATCH_HEAD = (u8*)*(u8**)G_SCRATCH_HEAD + 0x28;
}

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);

/// Rebuilds the model's root coordinate around the yaw it already faces and
/// rescales it uniformly: `ratan2` of the rotation's Z basis gives the yaw,
/// `Gfx_RotMatrixY` rebuilds the rotation from it and `ScaleMatrix` applies
/// `scale` on all three axes. The working matrix lives in a frame carved off
/// `G_SCRATCH_HEAD`, which is handed back once the rotation has been copied
/// onto the coordinate. Written as an inline so the four scratch-head accesses
/// stay absolute; see `Actor444000_ShrinkRotation` in `actor_444000_5.c`.
static __inline__ void Actor110600_ScaleRotation(Task* task, s16 scale)
{
    ActorShared80135a60Scratch* blk;
    GsCOORDINATE2*              coord;
    u8*                         head;
    s16                         ang;
    u16                         m22;

    head                                          = *(u8**)G_SCRATCH_HEAD;
    coord                                         = ((TmdObject*)task->extra)->field_8;
    blk                                           = (ActorShared80135a60Scratch*)(head - 0x34);
    *(ActorShared80135a60Scratch**)G_SCRATCH_HEAD = blk;

    ang        = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    blk->angle = ang;
    Gfx_RotMatrixY(&blk->m, ang, 1);
    blk->scale.vz = scale;
    blk->scale.vy = scale;
    blk->scale.vx = scale;
    ScaleMatrix(&blk->m, &blk->scale);

    coord->coord.m[0][0]                    = *(u16*)&((ActorShared80135a60Scratch*)(head - 0x34))->m.m[0][0];
    coord->coord.m[0][1]                    = *(u16*)&blk->m.m[0][1];
    coord->coord.m[0][2]                    = *(u16*)&blk->m.m[0][2];
    coord->coord.m[1][0]                    = *(u16*)&blk->m.m[1][0];
    coord->coord.m[1][1]                    = *(u16*)&blk->m.m[1][1];
    coord->coord.m[1][2]                    = *(u16*)&blk->m.m[1][2];
    coord->coord.m[2][0]                    = *(u16*)&blk->m.m[2][0];
    coord->coord.m[2][1]                    = *(u16*)&blk->m.m[2][1];
    m22                                     = *(u16*)&blk->m.m[2][2];
    coord->flg                              = 0;
    coord->coord.m[2][2]                    = m22;
    ((TmdObject*)task->extra)->field_8->flg = 0;
    *(u8**)G_SCRATCH_HEAD                   = *(u8**)G_SCRATCH_HEAD + 0x34;
}

/// Placement opcode: drops the model's root coordinate onto `placement` (the
/// three longs become its translation, the Euler angles go through
/// `Gfx_RotMatrixX` / `Y` / `Z`), then rebuilds and rescales that coordinate
/// from the actor's own heading and caches the resulting yaw in the work
/// block's `field_8`. Same placement as `ActorsShared80169f74`, with the
/// rescale of `ActorsShared80135a60` folded in behind it.
s32 func_actor_110600_80133E48(Task* task, s32 arg1, ActorShared8013411cPlacement* placement)
{
    Actor110600Work* work;

    work = (Actor110600Work*)task->idMap;

    ((TmdObject*)task->extra)->field_8->coord.t[0] = placement->pos.vx;
    ((TmdObject*)task->extra)->field_8->coord.t[1] = placement->pos.vy;
    ((TmdObject*)task->extra)->field_8->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixX(&((TmdObject*)task->extra)->field_8->coord, placement->rot.vx, 1);
    Gfx_RotMatrixY(&((TmdObject*)task->extra)->field_8->coord, placement->rot.vy, 0);
    Gfx_RotMatrixZ(&((TmdObject*)task->extra)->field_8->coord, placement->rot.vz, 0);
    Actor110600_ScaleRotation(task, (s16)work->field_B7C);
    work->field_8 = ratan2(-((TmdObject*)task->extra)->field_8->coord.m[2][0],
                           ((TmdObject*)task->extra)->field_8->coord.m[2][2]);
    return 1;
}

/// Event handler: saves the event's first three bytes in the work block's
/// `field_BDC`, then dispatches on the event kind. Kind 0x301 with sub-code 1
/// enters state 0x14; kind 0x401 picks a display slot and a `field_892` state
/// per sub-code — 1, 8 and 9 only set the state, and 9 shares its tail with the
/// five sub-codes that repoint a slot — parking the actor in state 0x11 with
/// `field_2` cleared. Written with the share as a `goto` because the sub-codes
/// fall through into it from case 9. `arg1` is unused; it exists because the
/// dispatch passes three arguments.
s32 func_actor_110600_80134040(Actor110600* arg0, s32 arg1, Actor110600Event* arg2)
{
    Actor110600Work* work = arg0->field_1C;

    work->field_BDC[0] = arg2->b[0];
    work->field_BDC[1] = arg2->b[1];
    work->field_BDC[2] = arg2->b[2];
    if (arg2->w[0] == 0x301) {
        if (arg2->w[1] == 1) {
            work->field_0 = 0x14;
            return 1;
        }
        return 0;
    }
    if (arg2->w[0] == 0x401) {
        switch (arg2->w[1]) {
            default:
                return 0;
            case 1:
                work->field_0 = 0x17;
                work->field_2 = -1;
                return 1;
            case 2:
                work->field_892         = 0x23;
                D_actor_110600_80148598 = &D_8015BD7C;
                goto state_11;
            case 3:
                work->field_892         = 0x24;
                D_actor_110600_8014859C = &D_8015BD7C;
                goto state_11;
            case 5:
                work->field_892         = 0x22;
                D_actor_110600_80148594 = &D_8015C950;
                goto state_11;
            case 6:
                work->field_892         = 0x23;
                D_actor_110600_80148598 = &D_8015D2E8;
                goto state_11;
            case 4:
            case 7:
                work->field_892         = 0x25;
                D_actor_110600_801485A0 = &D_8015C064;
                goto state_11;
            case 8:
                work->field_0 = 0xC;
                work->field_2 = -1;
                return 1;
            case 9:
                work->field_892 = 0x11;
            state_11:
                work->field_0 = 0x11;
                work->field_2 = -1;
                return 1;
        }
    }
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_801341A4);

/// Per-tick animation pass: for each clip id 1..0x12, the first ten (`i < 0xB`)
/// write the two clip ids into their slot's `field_9` and tick the primary and
/// blend contexts through `func_800B3448`, then hand both poses to
/// `Gp_AnimWritePoseCopy` with `weight` at 0x8A0 and its complement; the rest
/// only rewrite the primary slot and `Gp_AnimTickIndex` it. Same body as
/// `func_actor_403000_801336B4`, which walks 24 slots instead of 19.
void func_actor_110600_80134438(Actor110600* arg0)
{
    GpAnimPose           pose;
    GpAnimPose           blendPose;
    GpAnimCtx*           anim;
    s16                  weight;
    s16                  i;
    Actor110600AnimWork* work;

    work   = (Actor110600AnimWork*)arg0->field_1C;
    weight = work->field_8A0;
    anim   = &work->anim;
    for (i = 1; i < 0x13; i++) {
        if (i < 0xB) {
            work->blendSlots[i].field_9 = (u8)work->field_89E;
            work->slots[i].field_9      = (u8)(work->field_896 - 3);
            func_800B3448(anim, i, (s32)&pose, 0);
            func_800B3448(&work->blendAnim, i, (s32)&blendPose, 0);
            Gp_AnimWritePoseCopy(anim, i, &pose, &blendPose, weight, 0x1000 - weight);
        } else {
            work->slots[i].field_9 = (u8)(work->field_896 - 3);
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80134564);

/// Reset argument `func_800B4114` is handed for the clip `field_892` of the
/// `field_890` stage: the `0x2D`-byte row of the animation table this overlay's
/// data carries at `D_actor_110600_80147D20`, indexed by the clip id. The row
/// stride is the row's own length, so the load is a signed byte.
extern s8 D_actor_110600_80147D20[];

void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Per-tick animation stage machine driven off the task's work block.
///
/// Stages 1, 2 and 6 arm every slot 1..0x12 and then park the stage at 3 with
/// the frame counter `field_894` and `field_8AC` cleared: stage 1 plays the
/// clip at `field_892` through `func_800B4114`, taking each slot's reset
/// argument out of the `field_890` row of `D_actor_110600_80147D20`; stage 2
/// arms the same clip with `Gp_AnimResetSlot`; stage 6 arms clip 0x10 and then
/// ticks the context 99 times so the pose settles before it is shown. A
/// `field_89A` of 2 is the blend stage instead — it arms the blend context with
/// clip `field_89C` at weight `field_8A0` and parks `field_89A` at 3.
///
/// Every tick then steps all slots, either directly or — while `field_88E` is
/// set — through the blend pass `func_actor_110600_80134438`, which ends the
/// wait once blend slot 1 reports its clamp. `field_8A4` walks towards
/// `field_8A2` in 0x100 steps, and the turn that leaves, clamped to ±0x400, is
/// handed to joints 5 and 3 of the model root (the second a quarter of it).
/// Finally the id `func_actor_110600_80134564` reports is queued through
/// `SndEvt_EnqueueType6` with the model root's pan and depth; the bits 12..15
/// of the enemy's `field_8` are appended to it.
void func_actor_110600_80134728(Actor110600* arg0)
{
    Actor110600AnimWork* work;
    Actor110600AnimWork* seekWork;
    Actor110600AnimWork* resetWork;
    Actor110600AnimWork* warmWork;
    Actor110600AnimWork* blendWork;
    Actor110600AnimWork* tickWork;
    GpEnemy*             enemy;
    u32                  table;
    s32                  index;
    s32                  animation;
    s32                  seekIndex;
    s32                  resetIndex;
    s32                  warmIndex;
    s32                  blendIndex;
    s32                  tickIndex;
    s32                  targetAngle;
    s32                  currentAngle;
    s32                  targetAngleBits;
    s32                  currentAngleBits;
    s32                  turn;
    s16                  turnNow;
    s32                  sound;
    s32                  soundId;
    s32                  pan;
    s16                  state;

    work  = (Actor110600AnimWork*)arg0->field_1C;
    state = work->field_88C;
    enemy = arg0->field_20;
    if (state == 1) {
        seekWork  = work;
        seekIndex = 1;
        table     = (u32)D_actor_110600_80147D20;
        do {
            work->slots[seekIndex].field_9 = (u8)seekWork->field_896;
            animation                      = seekWork->field_892;
            index                          = seekWork->field_890 * 0x2D;
            func_800B4114(&seekWork->anim, seekIndex, animation, 0, (s32) * (s8*)((animation + index) + table));
            seekIndex += 1;
        } while (seekIndex < 0x13);
        seekWork->field_890 = (u16)seekWork->field_892;
        work->field_88C     = 3;
        work->field_894     = 0;
        work->field_8AC     = 0;
    } else if (state == 2) {
        resetWork  = work;
        resetIndex = 1;
        do {
            work->slots[resetIndex].field_9 = (u8)resetWork->field_896;
            Gp_AnimResetSlot(&resetWork->anim, resetIndex, (s32)resetWork->field_892);
            resetIndex += 1;
        } while (resetIndex < 0x13);
        resetWork->field_890 = (u16)resetWork->field_892;
        work->field_88C      = 3;
        work->field_894      = 0;
        work->field_8AC      = 0;
    } else if (state == 6) {
        warmWork  = work;
        warmIndex = 1;
        do {
            work->slots[warmIndex].field_9 = 0x10;
            Gp_AnimResetSlot(&warmWork->anim, warmIndex, (s32)warmWork->field_892);
            warmIndex += 1;
        } while (warmIndex < 0x13);
        warmWork->field_890 = (u16)warmWork->field_892;
        warmIndex           = 1;
        do {
            tickWork  = (Actor110600AnimWork*)arg0->field_1C;
            tickIndex = 1;
            do {
                tickWork->slots[tickIndex].field_9 = (u8)tickWork->field_896;
                Gp_AnimTickIndex(&tickWork->anim, tickIndex);
                tickIndex += 1;
            } while (tickIndex < 0x13);
            warmIndex += 1;
        } while (warmIndex < 0x64);
        work->field_88C = 3;
        work->field_894 = 0;
        work->field_8AC = 0;
    }
    if (work->field_89A == 2) {
        blendWork            = (Actor110600AnimWork*)arg0->field_1C;
        blendIndex           = 1;
        blendWork->field_89E = 0x30;
        blendWork->field_8A0 = 0xB78;
        do {
            blendWork->slots[blendIndex].field_9 = (u8)blendWork->field_89E;
            Gp_AnimResetSlot(&blendWork->blendAnim, blendIndex, (s32)blendWork->field_89C);
            blendIndex += 1;
        } while (blendIndex < 0x13);
        work->field_89A = 3;
    }
    work->field_894 = (u16)(work->field_894 + 1);
    if (work->field_88E == 0) {
        tickWork  = (Actor110600AnimWork*)arg0->field_1C;
        tickIndex = 1;
        do {
            tickWork->slots[tickIndex].field_9 = (u8)tickWork->field_896;
            Gp_AnimTickIndex(&tickWork->anim, tickIndex);
            tickIndex += 1;
        } while (tickIndex < 0x13);
    } else {
        func_actor_110600_80134438(arg0);
        if (work->blendSlots[1].field_10 & 0x1) {
            work->field_88E = 0;
        }
    }
    targetAngle      = work->field_8A2;
    currentAngle     = work->field_8A4;
    targetAngleBits  = (u16)work->field_8A2;
    currentAngleBits = (u16)work->field_8A4;
    if (currentAngle < targetAngle) {
        if ((targetAngle - currentAngle) >= 0x101) {
            work->field_8A4 = (s16)(currentAngleBits + 0x100);
        } else {
            goto block_31;
        }
    } else if ((currentAngle - targetAngle) >= 0x101) {
        work->field_8A4 = (s16)(currentAngleBits - 0x100);
    } else {
    block_31:
        work->field_8A4 = (s16)targetAngleBits;
    }
    turnNow = work->field_8A4;
    turn    = (u16)work->field_8A4;
    if (turnNow != 0) {
        if (turnNow >= 0x401) {
            turn = 0x400;
        }
        if (turnNow < -0x400) {
            turn = -0x400;
        }
        ActorsShared80132808(&arg0->field_2C->field_8[5], (s16)turn);
        ActorsShared80132808(&arg0->field_2C->field_8[3], (s16)((s32)(turn << 0x10) >> 0x12));
    }
    sound = func_actor_110600_80134564((Actor110600Work*)work);
    if (sound != 0) {
        soundId = sound | ((enemy->field_8 >> 12) << 8);
        pan     = (s8)Gp_GetObjPan((GpObj38*)arg0->field_2C->field_8);
        SndEvt_EnqueueType6(soundId, pan, (s32)(s8)Gp_GetObjDepth((GpObj38*)arg0->field_2C->field_8));
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80134AB4);

/// `D_80073B8C` is the camera-target matrix the delta below is measured from.
extern MATRIX* D_80073B8C;

/// Aiming stage: re-arms the aim on a live actor — clear the model object, drop
/// bit 0x8000 of `field_A90.flags` and set 0x4000 of `field_950.flags`, tag the
/// enemy's link node, reload `field_896` from `field_898`, park the stage at 2
/// (`field_88C` / `field_892`) and the walker at state 3 with its turn limit at
/// 0x10. The aim itself is one bearing: the yaw of the camera-target delta from
/// the model's root coordinate, minus that coordinate's own yaw, wrapped into
/// [-0x800, 0x800]. While it is under 0x3E8 and again unconditionally, the XZ
/// delta is measured against the `field_C` / `field_E` hit spheres, and falling
/// inside either moves the actor to state 4. Every tick the walker is stepped
/// first and the model ticked last.
void func_actor_110600_80135194(Actor110600* arg0)
{
    Actor110600Work* work;
    TmdObject*       obj;
    GpEnemy*         enemy;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   facing;
    SVECTOR          delta;
    SVECTOR*         d;
    s16              angle;

    work  = arg0->field_1C;
    obj   = arg0->field_2C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        obj->field_C          = 0;
        work->field_896       = work->field_898;
        work->field_A90.flags = (u16)(work->field_A90.flags & 0x7FFF);
        work->field_950.flags = (u16)(work->field_950.flags | 0x4000);
        enemy->node.field_4   = 8;
        work->field_88C       = 2;
        work->field_892       = 2;
        work->field_B90       = 3;
        work->field_B82       = 0x10;
    }
    work->field_B86 = work->field_8B6;
    func_actor_110600_80133A94((Actor110600Walker*)((u8*)work + 0xB28));
    coord    = arg0->field_2C->field_8;
    d        = &delta;
    delta.vx = (u16)D_80073B8C->t[0] - (u16)coord->coord.t[0];
    d->vy    = (u16)D_80073B8C->t[1] - (u16)coord->coord.t[1];
    d->vz    = (u16)D_80073B8C->t[2] - (u16)coord->coord.t[2];
    facing   = arg0->field_2C->field_8;
    angle    = ratan2(delta.vx, d->vz) - ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
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
    if (abs(angle) < 0x3E8) {
        if (Actor110600_OutsideRadius(&delta, work->field_C) == 0)
            work->field_0 = 4;
    }
    if (Actor110600_OutsideRadius(&delta, work->field_E) == 0)
        work->field_0 = 4;
    func_actor_110600_80134728(arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80135454);

/// Aiming stage: points the model at the camera target. Entering on a live
/// actor re-arms it — clear the model object, drop bit 0x8000 of
/// `field_A90.flags` and set 0x4000 of `field_950.flags`, tag the enemy's link
/// node, then park the stage timer at 0x15 with `field_896` reloaded from
/// `field_898`. The yaw of the delta from the model's root coordinate to
/// `D_80073B8C`'s translation goes through `ratan2`, has the coordinate's own
/// yaw (`ratan2` of `-m[2][0]`, `m[2][2]`) subtracted, and is wrapped into
/// [-0x800, 0x800] before it lands in `field_8A2`; the model is ticked and the
/// actor moves on (state 3) once the `field_5C` bit the walker sets arrives.
/// Both translations are measured in their low 16 bits, so all three delta
/// reads are `u16`.
void func_actor_110600_80135A18(Actor110600* arg0)
{
    Actor110600Work* work;
    TmdObject*       obj;
    GpEnemy*         enemy;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   facing;
    SVECTOR          delta;
    SVECTOR*         d;
    s16              angle;

    work  = arg0->field_1C;
    obj   = arg0->field_2C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        obj->field_C          = 0;
        work->field_A90.flags = (u16)(work->field_A90.flags & 0x7FFF);
        work->field_950.flags = (u16)(work->field_950.flags | 0x4000);
        enemy->node.field_4   = 8;
        work->field_88C       = 1;
        work->field_892       = 0x15;
        work->field_896       = work->field_898;
    }
    coord    = arg0->field_2C->field_8;
    d        = &delta;
    delta.vx = (u16)D_80073B8C->t[0] - (u16)coord->coord.t[0];
    d->vy    = (u16)D_80073B8C->t[1] - (u16)coord->coord.t[1];
    d->vz    = (u16)D_80073B8C->t[2] - (u16)coord->coord.t[2];
    facing   = arg0->field_2C->field_8;
    angle    = ratan2(delta.vx, d->vz) - ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
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
    work->field_8A2 = angle;
    func_actor_110600_80134728(arg0);
    if (work->field_5C & 1) {
        work->field_0 = 3;
    }
}

/// 1 when the first of `recs` carries the kind 0x10000 tag: the walk breaks on
/// an empty slot and reports 0.
static __inline__ s32 Actor110600_HasRec10000(GpRec18* recs)
{
    s16 i;

    for (i = 0; i < 1; i++) {
        if (!recs[i].field_4) {
            break;
        }
        if ((recs[i].field_4 & 0xFFFF0000) == 0x10000) {
            return 1;
        }
    }
    return 0;
}

/// Firing stage. Entering on a live actor re-arms it: clear the model object,
/// take 0x8000 off `field_A90.flags` and put 0x4000 on `field_950.flags`, tag
/// the enemy's link node, and pick one of the two patrol modes off
/// `field_BE6` — a zeroed one packs the model pair with 1 and holds the stage
/// at `field_892` 5 for 0x10 ticks, a set one packs it with 0 and holds mode 4
/// for 0x1A. `field_B90` is raised and the walker block at 0xB28 is re-armed
/// for a fresh patrol (`field_5C` cleared, `field_5E` reloaded from
/// `field_B86`, `field_60` = 8) with `field_8A2` / `field_8A4` cleared and
/// `field_B82` parked at 0x10 to cover the first ten ticks. Every tick after
/// that raises `field_BE0`, which retires `field_B82` once it passes 0xB, and
/// ticks the model; the pose `field_4E` then drives the pair of flag edges the
/// mode owns — 0xF raises and 0x15 drops 0x8000 in mode 4, 0x10 / 0x13 the
/// same in mode 5. The `field_5C` bit 0 the walker sets moves the actor on
/// (state 3). Finally, while the first `GpRec18` record still carries the
/// 0x10000 kind tag, the model root's pan and depth are played as sound
/// 0x401D000D and 0x8000 comes off `field_A90.flags`.
void func_actor_110600_80135B84(Actor110600* arg0)
{
    Actor110600Work*   work;
    Actor110600Walker* walker;
    GpEnemy*           enemy;
    TmdObject*         obj;
    u16                ramp;

    work  = arg0->field_1C;
    obj   = arg0->field_2C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        obj->field_C          = 0;
        work->field_A90.flags = (u16)(work->field_A90.flags & 0x7FFF);
        work->field_950.flags = (u16)(work->field_950.flags | 0x4000);
        enemy->node.field_4   = 8;
        work->field_88C       = 1;
        if (arg0->field_1C->field_BE6 != 0) {
            work->field_A90.field_18 = Gp_PackObjPair((GpObj50*)enemy, 0);
            work->field_892          = 4;
            work->field_896          = 0x1A;
        } else {
            work->field_A90.field_18 = Gp_PackObjPair((GpObj50*)enemy, 1);
            work->field_892          = 5;
            work->field_896          = 0x10;
        }
        work->field_B90  = 1;
        walker           = (Actor110600Walker*)((u8*)work + 0xB28);
        ramp             = work->field_B86;
        walker->field_5C = 0;
        walker->field_60 = 8;
        walker->field_5E = ramp;
        work->field_B82  = 0x10;
        work->field_8A4  = 0;
        work->field_8A2  = 0;
        work->field_BE0  = 0;
    }
    work->field_BE0++;
    if (work->field_BE0 >= 0xB) {
        work->field_B82 = 0;
    }
    func_actor_110600_80134728(arg0);
    if (work->field_892 == 4) {
        switch (work->field_4E & 0x3FF) {
            case 0xF:
                work->field_A90.flags = (u16)(work->field_A90.flags | 0x8000);
                break;
            case 0x15:
                work->field_A90.flags = (u16)(work->field_A90.flags & 0x7FFF);
                break;
        }
    }
    if (work->field_892 == 5) {
        switch (work->field_4E & 0x3FF) {
            case 0x10:
                work->field_A90.flags = (u16)(work->field_A90.flags | 0x8000);
                break;
            case 0x13:
                work->field_A90.flags = (u16)(work->field_A90.flags & 0x7FFF);
                break;
        }
    }
    if (work->field_5C & 1) {
        work->field_0 = 3;
    }
    if (Actor110600_HasRec10000(work->recs)) {
        SndEvt_EnqueueType6(0x401D000D, (s8)Gp_GetObjPan((GpObj38*)arg0->field_2C->field_8),
                            (s8)Gp_GetObjDepth((GpObj38*)arg0->field_2C->field_8));
        work->field_A90.flags = (u16)(work->field_A90.flags & 0x7FFF);
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80135E20);

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80136210);

/// Timer stage that walks between the two long `field_892` values. Entering on
/// a live actor re-arms it: clear the model object, take 0x8000 off
/// `field_A90.flags` and put 0x4000 on `field_950.flags`, tag the enemy's link
/// node, set the stage timer to 0x18 and `field_896` from `field_898`, then
/// re-arm the walker block at 0xB28 for a fresh patrol (`field_5C` cleared,
/// `field_5E` reloaded from `field_B86`, `field_60` = 8) with `field_B90` /
/// `field_B82` / `field_8A4` / `field_8A2` cleared. Every tick after that steps
/// the walker and the model, then retimes: at 0x18 a draw of `Gp_LcgState`
/// whose seventh bit is clear drops it to 0xE, and at 0xE the `field_5C` bit 0
/// the walker sets on arrival — or on hitting something — puts it back to 0x18.
/// Both retimes re-enter state 1 (`field_88C`) and tick once more.
void func_actor_110600_80136888(Actor110600* arg0)
{
    Actor110600Work*   work;
    Actor110600Walker* walker;
    GpEnemy*           enemy;
    TmdObject*         obj;
    u32                rng;
    u16                ramp;

    work  = arg0->field_1C;
    obj   = arg0->field_2C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        obj->field_C          = 0;
        work->field_A90.flags = (u16)(work->field_A90.flags & 0x7FFF);
        work->field_950.flags = (u16)(work->field_950.flags | 0x4000);
        enemy->node.field_4   = 8;
        work->field_88C       = 1;
        work->field_892       = 0x18;
        work->field_896       = work->field_898;
        ramp                  = work->field_B86;
        walker                = (Actor110600Walker*)((u8*)work + 0xB28);
        work->field_B90       = 0;
        walker->field_5C      = 0;
        walker->field_5E      = ramp;
        walker->field_60      = 8;
        work->field_B82       = 0;
        work->field_8A4       = 0;
        work->field_8A2       = 0;
    }
    func_actor_110600_80133A94((Actor110600Walker*)((u8*)work + 0xB28));
    func_actor_110600_80134728(arg0);
    if (work->field_892 == 0x18) {
        if (work->field_5C & 2) {
            rng         = Gp_LcgState * 5 + 0x71357911;
            Gp_LcgState = rng;
            if (!((rng >> 16) & 7)) {
                work->field_892 = 0xE;
                work->field_88C = 1;
                func_actor_110600_80134728(arg0);
            }
        }
    }
    if ((work->field_892 == 0xE) && (work->field_5C & 1)) {
        work->field_892 = 0x18;
        work->field_88C = 1;
        func_actor_110600_80134728(arg0);
    }
}

/// The walker's handoff stage. Entering on a live actor re-arms it: clear the
/// model object, take 0x8000 off `field_A90.flags` and put 0x4000 on
/// `field_950.flags`, tag the enemy's link node, park the stage timer at 0x1D
/// with `field_896` at 0x10, then re-arm the walker block at 0xB28 to run its
/// patrol out (`field_5C` = 0xFFFE, `field_5E` reloaded from `field_B86`,
/// `field_60` = 2) with `field_B90` / `field_B82` / `field_8A4` / `field_8A2`
/// cleared. Every tick after that steps the walker and the model; at 0x1D the
/// `field_5C` bit 0 the walker sets on arrival moves the stage to 0x1E and
/// re-seeds the walker block, and at 0x1E that same bit picks what the actor
/// does next: 0xB while the enemy's `field_40` is still positive, 0xC once it
/// has run out.
void func_actor_110600_801369D8(Actor110600* arg0)
{
    Actor110600Work*   work;
    Actor110600Walker* walker;
    Actor110600Walker* walker2;
    GpEnemy*           enemy;
    u16                ramp;
    u16                ramp2;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        arg0->field_2C->field_C = 0;
        work->field_A90.flags   = (u16)(work->field_A90.flags & 0x7FFF);
        work->field_950.flags   = (u16)(work->field_950.flags | 0x4000);
        enemy->node.field_4     = 8;
        work->field_88C         = 2;
        work->field_892         = 0x1D;
        work->field_896         = 0x10;
        ramp                    = work->field_B86;
        walker                  = (Actor110600Walker*)((u8*)work + 0xB28);
        work->field_B90         = 0;
        walker->field_5C        = 0xFFFE;
        walker->field_5E        = ramp;
        walker->field_60        = 2;
        work->field_B82         = 0;
        work->field_8A4         = 0;
        work->field_8A2         = 0;
    }
    walker2 = (Actor110600Walker*)((u8*)work + 0xB28);
    func_actor_110600_80133A94(walker2);
    func_actor_110600_80134728(arg0);
    if (work->field_892 == 0x1D) {
        if (work->field_5C & 1) {
            ramp2             = work->field_B86;
            work->field_892   = 0x1E;
            work->field_88C   = 2;
            walker2->field_5C = 0;
            walker2->field_60 = 2;
            walker2->field_5E = ramp2;
            return;
        }
    }
    if ((work->field_892 == 0x1E) && (work->field_5C & 1)) {
        if (enemy->field_40 > 0) {
            work->field_0 = 0xB;
        } else {
            work->field_0 = 0xC;
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80136B20);

/// Re-dresses a live actor: take the model object out of draw, drop bit 0x8000
/// of `field_A90.flags` and bit 0x4000 of `field_950.flags`, tag the enemy's
/// link node, clear the `field_B82` / `field_8A4` / `field_8A2` timers and hand
/// the model the 0x80 texture page, then spawn five effects off its part
/// coordinates 6, 8, 10, 11 and 15 (`Gp_SpawnEff` bank 0xA0005, buffer sizes
/// 0x200 / 0x200 / 0x200 / 0x300 / 0x300). Each spawned model object takes its
/// texture page and CLUT from the nested area record the actor's own area key
/// resolves to, and is streamed twice once its aux buffer exists.
void func_actor_110600_80136ECC(Actor110600* arg0)
{
    Actor110600Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;
    GpAreaKey        key;
    u8               areaByte0;
    u32              raw1, index1;
    GpEffWork*       effect1;
    TmdObject*       model1;
    GpCdRec10*       entry1;
    GpAreaKey*       sessionKey1;
    u32              raw2, index2;
    GpEffWork*       effect2;
    TmdObject*       model2;
    GpCdRec10*       entry2;
    GpAreaKey*       sessionKey2;
    u32              raw3, index3;
    GpEffWork*       effect3;
    TmdObject*       model3;
    GpCdRec10*       entry3;
    GpAreaKey*       sessionKey3;
    u32              raw4, index4;
    GpEffWork*       effect4;
    TmdObject*       model4;
    GpCdRec10*       entry4;
    GpAreaKey*       sessionKey4;
    u32              raw5, index5;
    GpEffWork*       effect5;
    TmdObject*       model5;
    GpCdRec10*       entry5;
    GpAreaKey*       sessionKey5;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        enemy                  = arg0->field_20;
        obj                    = arg0->field_2C;
        obj->field_C           = 0;
        work->field_A90.flags &= 0x7FFF;
        work->field_950.flags &= 0xBFFF;
        enemy->node.field_4    = 1;
        work->field_B82        = 0;
        work->field_8A4        = 0;
        work->field_8A2        = 0;
        obj->field_C           = 0x80;

        effect1 = Gp_SpawnEff(0xA0005, &arg0->field_2C->field_8[6], 0x200, NULL);
        if (effect1 != NULL) {
            sessionKey1 = (GpAreaKey*)&Game_Session->field_4;
            raw1        = enemy->field_8;
            model1      = (TmdObject*)effect1->field_0->extra;
            key.field_3 = sessionKey1->field_3;
            key.field_2 = sessionKey1->field_2;
            key.field_1 = sessionKey1->field_1;
            areaByte0   = Game_Session->field_4;
            index1      = raw1 >> 12;
            key.field_0 = areaByte0;
            Gp_SyncAreaKeyIndex(&key);
            entry1           = (GpCdRec10*)((index1 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
            model1->field_24 = entry1->field_D;
            model1->field_25 = entry1->field_E;
            if (model1->field_18 != NULL) {
                Tmd_ProcessStream(model1);
                Tmd_ProcessStream(model1);
            }
        }

        effect2 = Gp_SpawnEff(0xA0005, &arg0->field_2C->field_8[8], 0x200, NULL);
        if (effect2 != NULL) {
            sessionKey2 = (GpAreaKey*)&Game_Session->field_4;
            raw2        = enemy->field_8;
            model2      = (TmdObject*)effect2->field_0->extra;
            key.field_3 = sessionKey2->field_3;
            key.field_2 = sessionKey2->field_2;
            key.field_1 = sessionKey2->field_1;
            areaByte0   = Game_Session->field_4;
            index2      = raw2 >> 12;
            key.field_0 = areaByte0;
            Gp_SyncAreaKeyIndex(&key);
            entry2           = (GpCdRec10*)((index2 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
            model2->field_24 = entry2->field_D;
            model2->field_25 = entry2->field_E;
            if (model2->field_18 != NULL) {
                Tmd_ProcessStream(model2);
                Tmd_ProcessStream(model2);
            }
        }

        effect3 = Gp_SpawnEff(0xA0005, &arg0->field_2C->field_8[10], 0x200, NULL);
        if (effect3 != NULL) {
            sessionKey3 = (GpAreaKey*)&Game_Session->field_4;
            raw3        = enemy->field_8;
            model3      = (TmdObject*)effect3->field_0->extra;
            key.field_3 = sessionKey3->field_3;
            key.field_2 = sessionKey3->field_2;
            key.field_1 = sessionKey3->field_1;
            areaByte0   = Game_Session->field_4;
            index3      = raw3 >> 12;
            key.field_0 = areaByte0;
            Gp_SyncAreaKeyIndex(&key);
            entry3           = (GpCdRec10*)((index3 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
            model3->field_24 = entry3->field_D;
            model3->field_25 = entry3->field_E;
            if (model3->field_18 != NULL) {
                Tmd_ProcessStream(model3);
                Tmd_ProcessStream(model3);
            }
        }

        effect4 = Gp_SpawnEff(0xA0005, &arg0->field_2C->field_8[11], 0x300, NULL);
        if (effect4 != NULL) {
            sessionKey4 = (GpAreaKey*)&Game_Session->field_4;
            raw4        = enemy->field_8;
            model4      = (TmdObject*)effect4->field_0->extra;
            key.field_3 = sessionKey4->field_3;
            key.field_2 = sessionKey4->field_2;
            key.field_1 = sessionKey4->field_1;
            areaByte0   = Game_Session->field_4;
            index4      = raw4 >> 12;
            key.field_0 = areaByte0;
            Gp_SyncAreaKeyIndex(&key);
            entry4           = (GpCdRec10*)((index4 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
            model4->field_24 = entry4->field_D;
            model4->field_25 = entry4->field_E;
            if (model4->field_18 != NULL) {
                Tmd_ProcessStream(model4);
                Tmd_ProcessStream(model4);
            }
        }

        effect5 = Gp_SpawnEff(0xA0005, &arg0->field_2C->field_8[15], 0x300, NULL);
        if (effect5 != NULL) {
            sessionKey5 = (GpAreaKey*)&Game_Session->field_4;
            raw5        = enemy->field_8;
            model5      = (TmdObject*)effect5->field_0->extra;
            key.field_3 = sessionKey5->field_3;
            key.field_2 = sessionKey5->field_2;
            key.field_1 = sessionKey5->field_1;
            areaByte0   = Game_Session->field_4;
            index5      = raw5 >> 12;
            key.field_0 = areaByte0;
            Gp_SyncAreaKeyIndex(&key);
            entry5           = (GpCdRec10*)((index5 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
            model5->field_24 = entry5->field_D;
            model5->field_25 = entry5->field_E;
            if (model5->field_18 != NULL) {
                Tmd_ProcessStream(model5);
                Tmd_ProcessStream(model5);
            }
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_801372CC);

/// The turn-away stage, the pick-up twin of the `field_892` == 0x16 leg of
/// `func_actor_110600_80136888`: entering on a live actor clears the model
/// object, takes 0x8000 off `field_A90.flags` and 0x4000 off
/// `field_950.flags`, tags the enemy's link node with 1 and parks the timer at
/// `field_896` = 0x20 with `field_88C` re-armed, `field_B82` / `field_8A4` /
/// `field_8A2` cleared. Every tick after that steps the shared handler and, at
/// 0x16, rolls `Gp_LcgState` and turns the model's root coordinate by the yaw
/// the roll's low nibble picks — 0x32 while it is under 0xA, -0x78 past it —
/// clearing the coordinate's `flg`. Once `field_894` has run up to 0x1F the
/// stage drops the timer to 0x10, re-arms `field_88C` and steps to 0x21, where
/// the `field_5C` bit 0 the actor sets walks it on to state 0xC.
void func_actor_110600_80137684(Actor110600* arg0)
{
    Actor110600Work* work;
    GpEnemy*         enemy;
    u32              rng;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        enemy                   = arg0->field_20;
        arg0->field_2C->field_C = 0;
        work->field_A90.flags   = (u16)(work->field_A90.flags & 0x7FFF);
        work->field_950.flags   = (u16)(work->field_950.flags & 0xBFFF);
        enemy->node.field_4     = 1;
        work->field_896         = 0x20;
        work->field_88C         = 1;
        work->field_B82         = 0;
        work->field_8A4         = 0;
        work->field_8A2         = 0;
        work->field_892         = 0x16;
    }
    func_actor_110600_80134728(arg0);
    if (work->field_892 == 0x16) {
        rng         = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState = rng;
        if (((rng >> 16) & 0xF) < 0xA) {
            Gfx_RotMatrixY(&arg0->field_2C->field_8->coord, 0x32, 0);
        } else {
            Gfx_RotMatrixY(&arg0->field_2C->field_8->coord, -0x78, 0);
        }
        arg0->field_2C->field_8->flg = 0;
        if ((work->field_892 == 0x16) && (work->field_894 >= 0x1F)) {
            work->field_896 = 0x10;
            work->field_88C = 1;
            work->field_892 = 0x21;
        }
    }
    if ((work->field_892 == 0x21) && (work->field_5C & 1)) {
        work->field_0 = 0xC;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_801377FC);

/// Aiming stage that re-arms the model behaviour on a live actor — `field_892`
/// = 0x15 with `field_88C` = 1, the model object's `field_C` cleared, bit 0x8000
/// off `field_A90.flags` and 0x4000 on `field_950.flags`, the enemy's link node
/// tagged 1 with `field_B82` / `field_8A4` cleared and `field_896` = 0x10 — then
/// wraps the yaw from the model's root coordinate to the camera target
/// `D_80073B8C` against the coordinate's own yaw (`ratan2` of `-m[2][0]`,
/// `m[2][2]`) into `field_8A2`. Ticks the model and moves the actor to state 3
/// once the `field_5C` bit the walker sets arrives. Same wrap as
/// `func_actor_110600_80135A18`.
void func_actor_110600_80137980(Actor110600* arg0)
{
    Actor110600Work* work;
    TmdObject*       obj;
    GpEnemy*         enemy;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   facing;
    SVECTOR          delta;
    SVECTOR*         d;
    s16              angle;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        enemy                 = arg0->field_20;
        obj                   = arg0->field_2C;
        work->field_892       = 0x15;
        work->field_88C       = 1;
        obj->field_C          = 0;
        work->field_A90.flags = (u16)(work->field_A90.flags & 0x7FFF);
        work->field_950.flags = (u16)(work->field_950.flags | 0x4000);
        enemy->node.field_4   = 1;
        work->field_B82       = 0;
        work->field_8A4       = 0;
        work->field_896       = 0x10;
    }
    coord    = arg0->field_2C->field_8;
    d        = &delta;
    delta.vx = (u16)D_80073B8C->t[0] - (u16)coord->coord.t[0];
    d->vy    = (u16)D_80073B8C->t[1] - (u16)coord->coord.t[1];
    d->vz    = (u16)D_80073B8C->t[2] - (u16)coord->coord.t[2];
    facing   = arg0->field_2C->field_8;
    angle    = ratan2(delta.vx, d->vz) - ratan2(-facing->coord.m[2][0], facing->coord.m[2][2]);
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
    work->field_8A2 = angle;
    func_actor_110600_80134728(arg0);
    if (work->field_5C & 1) {
        work->field_0 = 3;
    }
}

/// Stage-driven recoil push, one stage per tick. On a live actor it clears the
/// model's flags, drops bit 0x8000 of `field_A90.flags` and sets 0x4000 of
/// `field_950.flags`, tags the enemy's link node and restarts the stage at 0.
/// The push takes column 0 of the model root coordinate, normalises it out of
/// place and scales it by the stage — 0x320, -0x3E8, 0x190, -0x190, 0xC8,
/// through the GTE's interpolation register. Only the X and Z components are
/// added to the coordinate's translation, and the coordinate is marked dirty so
/// the tree is recomputed. Stage 5 pushes nothing: it moves the actor to state
/// 3 and leaves the counter parked.
void func_actor_110600_80137AF4(Actor110600* arg0)
{
    Actor110600Work* work;
    TmdObject*       obj;
    GpEnemy*         enemy;
    SVECTOR          vec;

    work  = arg0->field_1C;
    obj   = arg0->field_2C;
    enemy = arg0->field_20;
    if (work->field_4 != 0) {
        obj->field_C            = 0;
        work->field_A90.flags   = (u16)(work->field_A90.flags & 0x7FFF);
        work->field_950.flags   = (u16)(work->field_950.flags | 0x4000);
        enemy->node.field_4     = 8;
        D_actor_110600_80148688 = 0;
    }
    Gfx_MatrixCol0(&arg0->field_2C->field_8->coord, &vec);
    VectorNormalSS(&vec, &vec);
    switch (D_actor_110600_80148688) {
        case 0:
            gte_lddp(0x320);
            gte_ldsv(&vec);
            gte_gpf12_real();
            gte_stsv(&vec);
            break;
        case 1:
            gte_lddp(-0x3E8);
            gte_ldsv(&vec);
            gte_gpf12_real();
            gte_stsv(&vec);
            break;
        case 2:
            gte_lddp(0x190);
            gte_ldsv(&vec);
            gte_gpf12_real();
            gte_stsv(&vec);
            break;
        case 3:
            gte_lddp(-0x190);
            gte_ldsv(&vec);
            gte_gpf12_real();
            gte_stsv(&vec);
            break;
        case 4:
            gte_lddp(0xC8);
            gte_ldsv(&vec);
            gte_gpf12_real();
            gte_stsv(&vec);
            break;
        case 5:
            work->field_0 = 3;
            return;
    }
    arg0->field_2C->field_8->coord.t[0] += vec.vx;
    arg0->field_2C->field_8->coord.t[2] += vec.vz;
    D_actor_110600_80148688++;
    arg0->field_2C->field_8->flg = 0;
}

/// Death stage machine, entering on a live actor: take the model out of draw,
/// drop bit 0x8000 of `field_A90.flags` and set 0x4000 of `field_950.flags`,
/// tag the enemy's link node, arm `field_892` / `field_88C` and the `field_896`
/// timer, tick once and clear both `field_BE0` and the `field_BE2` stage. Stage
/// 0 idles on that timer — once the pose `field_4E` reaches 4 it parks
/// `field_896` at -0x10 and steps to stage 1. Stage 1 is the shrink tail:
/// halves `field_896` each tick, parking at -0xC when the halving lands on the
/// stage value and bouncing -1 back to 8, and after 0x35 ticks parks
/// `field_896` / `field_898` at 0x38 and moves the actor to state 3. Every
/// stage-1 tick also adds 0x27 to `field_BE4`.
void func_actor_110600_80137DB0(Actor110600* arg0)
{
    Actor110600Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;
    s16              step;
    s32              state;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        enemy                 = arg0->field_20;
        obj                   = arg0->field_2C;
        obj->field_C          = 0;
        work->field_A90.flags = (u16)(work->field_A90.flags & 0x7FFF);
        work->field_950.flags = (u16)(work->field_950.flags | 0x4000);
        enemy->node.field_4   = 8;
        work->field_892       = 0xC;
        work->field_88C       = 2;
        work->field_B82       = 0;
        work->field_8A4       = 0;
        work->field_896       = 6;
        func_actor_110600_80134728(arg0);
        work->field_BE0 = 0;
        work->field_BE2 = 0;
    }
    state           = work->field_BE2;
    work->field_88E = 0;
    switch (state) {
        case 0:
            func_actor_110600_80134728(arg0);
            if ((work->field_4E & 0x3FF) == 4) {
                work->field_896 = -0x10;
                work->field_BE2 = (s16)((u16)work->field_BE2 + 1);
                return;
            }
            return;
        case 1:
            step            = (s16)work->field_896 / 2;
            work->field_896 = step;
            work->field_BE0++;
            if (work->field_896 == state) {
                work->field_896 = -0xC;
            }
            if (work->field_896 == -1) {
                work->field_896 = 8;
            }
            func_actor_110600_80134728(arg0);
            if (work->field_BE0 >= 0x35) {
                work->field_896 = 0x38;
                work->field_898 = 0x38;
                work->field_0   = 3;
            }
            work->field_BE4 += 0x27;
            break;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_110600/actor_110600", func_actor_110600_80137F2C);

void func_actor_110600_80138394(void)
{
}

INCLUDE_RODATA("actors/nonmatchings/actor_110600/actor_110600", ActorsShared80135df4Table);

/// The `0x7D3` handler of the display-opcode table `D_actor_110600_80148624`:
/// maps the requested state onto the work block's `field_892` (0x22..0x28) and
/// parks the actor in state 0x11 with `field_2` cleared. States 0 and 4 also
/// stamp the enemy's occupancy tag and re-save its pose; state 0 writes its own
/// `field_892` ahead of those calls, so it skips the store the other four share,
/// which is the tail the compiler merged out of the four `break`s.
///
/// The table GCC emits for this switch is what pins the package's
/// `rodata_head`: it lands at 0x18C, 8-aligned only if this unit's `.rodata`
/// starts at 0x4 rather than 0x0 — the package id ahead of it is prepended, not
/// compiled — and behind the id it picks up `.align 3`'s 4-byte pad instead.
s32 func_actor_110600_8013839C(Actor110600* arg0, s32 arg1, Actor110600Msg7D3* arg2)
{
    Actor110600Work* work;
    GpEnemy*         enemy;
    s32              state;

    state = arg2->field_4;
    work  = arg0->field_1C;
    enemy = arg0->field_20;
    switch (state) {
        case 0:
            work->field_892 = 0x22;
            enemy->field_4B = 1;
            Gp_SaveEnemyPose(enemy);
            break;
        case 1:
            work->field_892 = 0x23;
            break;
        case 2:
            work->field_892 = 0x24;
            break;
        case 3:
            work->field_892 = 0x25;
            break;
        case 4:
            enemy->field_4B = 1;
            Gp_SaveEnemyPose(enemy);
            work->field_892 = 0x28;
            break;
    }
    work->field_0 = 0x11;
    work->field_2 = -1;
    return 0;
}

/// Display-object handler, the same shape as `ActorsShared8013d268` one overlay
/// over: `arg2` selects the mode and `GpEnemy.field_4B` -- the occupancy tag
/// `Gp_SaveEnemyPose` writes -- decides whether mode 1 shows the object again.
/// Mode 0 hides it (bit 0x80 of `TmdObject.field_C`) and reinstates its buffers;
/// 1 hides it and restarts the work block's `field_0` while the tag reads 4, and
/// otherwise clears `field_C` and reinstates the buffers; 2 only sets bit 0x4;
/// 3 clears `field_C`, restarts `field_0` and then sets bit 0x4. `arg1` is
/// unused; it exists because the dispatch passes three arguments.
s32 func_actor_110600_80138448(Actor110600* arg0, s32 arg1, s32 arg2)
{
    TmdObject*       obj;
    Actor110600Work* work;
    GpEnemy*         enemy;

    obj   = arg0->field_2C;
    enemy = arg0->field_20;
    work  = arg0->field_1C;
    switch (arg2) {
        case 0:
            obj->field_C = 0x80;
            Tmd_AllocBuffers(obj);
            work->field_0 = 0;
            break;
        case 1:
            if (enemy->field_4B == 0) {
                obj->field_C = 0;
                Tmd_AllocBuffers(obj);
            } else if (enemy->field_4B == 4) {
                obj->field_C  = 0x80;
                work->field_0 = 0;
            } else {
                obj->field_C = 0;
                Tmd_AllocBuffers(obj);
            }
            break;
        case 2:
            obj->field_C |= 4;
            work->field_0 = 0;
            break;
        case 3:
            if (enemy->field_4B == 4) {
                obj->field_C = 0x80;
            } else {
                obj->field_C = 0;
            }
            work->field_0 = 0;
            obj->field_C |= 4;
            break;
    }
    return 0;
}

s32 func_actor_110600_80138538(Task* arg0)
{
    Actor110600Work* work;
    GpEnemy*         enemy;

    enemy = (GpEnemy*)arg0->spawnArg2;
    work  = (Actor110600Work*)arg0->idMap;
    if (enemy->field_40 > 0) {
        return 1;
    }
    work->field_BE4 = 0;
    enemy->field_4C = 0;
    work->field_BE6 = 0;
    return 0;
}
