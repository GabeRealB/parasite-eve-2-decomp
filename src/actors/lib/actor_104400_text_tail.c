#include "common.h"
#include "actors/actor_104400.h"
#include "actors/actors_shared_801639a8.h"
#include "actors/actors_shared_8016945c.h"
#include "actors/actors_shared_801692e8.h"
#include "actors/actors_shared_801673f8.h"
#include "main/fs.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "psyq/inline_c.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

void Actor04400_Fn006A8(Task* arg0);
/* Reads the caller's Task* from $a0; the call passes no argument. */
void Actor04400_Fn02B8C();
s16  Actor04400_Fn06328(Task* arg0);
void Actor04400_Fn06374(Task* arg0, s32 arg1);
s16  Actor04400_Fn065F4(Task* arg0, s32 arg1);
s16  Actor04400_Fn06618(Task* arg0);
void Actor04400_Fn067A0(Task* arg0, s32 step);
void Actor04400_Fn06BC4(Task* arg0);
void Actor04400_Fn06BF8(Task* arg0);
void Actor04400_Fn03390(Task* arg0);
void Actor04400_Fn07968(Task* arg0);
void Actor04400_Fn07984(Task* arg0);
void Actor04400_Fn07A38(Task* arg0);
void Actor04400_Fn07B4C(Task* arg0);
void Actor04400_Fn07C60(Task* arg0);
void Actor04400_Fn08208(Task* arg0);
void Actor04400_Fn0823C(Task* arg0);
void Actor04400_Fn08718(Task* arg0);
void Actor04400_Fn087E0(Task* arg0);
s32  Actor04400_Fn08DBC(Task* arg0);
void Actor04400_Fn00220(Task* arg0, s16 arg1, s16 arg2, s16 arg3, s32 arg4, u8 arg5);
void Actor04400_Fn022A8(Task* arg0, s32 arg1);

extern TaskFuncTable10 Actor04400_D00184;
extern TaskFuncTable4  Actor04400_D000B0;
extern TaskFuncTable6  Actor04400_D00004;
extern TaskFuncTable10 Actor04400_D0001C;
extern TaskFuncTable5  Actor04400_D001C4;
extern TaskFuncTable7  Actor04400_D001D8;
extern u32             Gp_LcgState;

/// Same body as `ActorsShared80166b20`. This overlay's whole `.text` is already
/// one shared span, so it cannot join that unit.
///
/// Counts `field_412` up against the `field_446` hold and enters state 2 once
/// it runs out; over its last 0x30 frames, eases the yaw `field_424` back to
/// zero. Before that, while `field_43A` (the distance to the nearer player
/// actor) is under 0xDAC and the heading `field_444` is outside 0x3C0..0xC40,
/// eases the yaw toward it and enters state 3 (arming `Gp_ArmStateF0`) after
/// 16 such frames; otherwise swings it toward +-0x380 on bit 6 of
/// `field_442`.
void Actor04400_Fn039EC(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;
    Actor104400Work* state;
    Actor104400Work* state2;
    s32              angle;
    s32              cur;
    s32              aim;

    if ((s16)++work->field_412 > work->field_446) {
        state            = (Actor104400Work*)arg0->work;
        state->field_420 = 2;
        state->field_422 = 0;
        return;
    }
    if (work->field_446 - 0x30 < (s16)work->field_412) {
        cur             = (u16)work->field_424;
        work->field_424 = cur + ((s16)(-(cur * 16)) >> 9);
        return;
    }
    if (work->field_43A < 0xDAC && (aim = (u16)work->field_444, (aim < 0x3C0 || aim > 0xC40))) {
        angle           = (u16)work->field_424;
        work->field_424 = angle + ((s16)((aim - angle) * 16) >> 6);
        if (++work->field_42C >= 0x10) {
            Gp_ArmStateF0(1);
            state2            = (Actor104400Work*)arg0->work;
            state2->field_420 = 3;
            state2->field_422 = 0;
        }
    } else {
        // Both arms are spelled out: the cross-jumped tail leaves each its own
        // load of `field_424`, which a single update after an if/else lacks.
        if (!(((u16)work->field_442 >> 6) & 1)) {
            work->field_424 = (u16)work->field_424 + ((s16)(0x3800 - (u16)work->field_424 * 16) >> 9);
        } else {
            work->field_424 = (u16)work->field_424 + ((s16)(-0x3800 - (u16)work->field_424 * 16) >> 9);
        }
    }
}

/// Counts the frame in `field_412` and, on frames 0x1D..0x29, pushes the model
/// root along the heading `field_7A` turned a quarter circle, by `field_41C`
/// scaled 30/16. Once `flags_EC` reports a hit (bit 0 as a halfword, or 0x102
/// as a word), clears `field_438` and puts the task in state 3 with its work
/// block at state 3.
///
/// Byte-for-byte `ActorsShared80166c68` of `actor_341700` / `actor_342400`,
/// which is this same source written on that pair's work type. The two are
/// separate shared units of this family (this one is `actor_104400`'s and
/// `actor_342200`'s), so neither absorbs the other.
void Actor04400_Fn03B34(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    s32              cond;
    s16              angle;
    s16              speed;
    s32              scale;

    work = (Actor104400Work*)arg0->work;
    if ((u16)(work->field_412++ - 0x1D) < 0xD) {
        scale                                           = 0x1E;
        angle                                           = work->field_7A + 0x400;
        speed                                           = (((Actor104400Work*)arg0->work)->field_41C * scale) << 0xC >> 0x10;
        ((TmdObject*)arg0->extra)->field_8->coord.t[0] += ((rsin(angle) << 4) * speed) >> 0x10;
        ((TmdObject*)arg0->extra)->field_8->coord.t[2] += ((rcos(angle) << 4) * speed) >> 0x10;
        ((TmdObject*)arg0->extra)->field_8->flg         = 0;
    }
    work2 = (Actor104400Work*)arg0->work;
    if ((work2->flags_EC.half & 1) || (work2->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_438 = 0;
        ActorsShared_SetTaskState(arg0, 3);
        ActorsShared_SetWorkState(arg0, 3);
    }
}

/// Counts the frame in `field_412` and, on frames 0x1D..0x29, pushes the model
/// root along the heading `field_7A` turned a quarter circle, by `field_41C`
/// scaled 30/16. Once `flags_EC` reports a hit (bit 0 as a halfword, or 0x102 as
/// a word), clears `field_438`, requests animation 3 at speed 0x10 and rewinds
/// the frame counter so the next state starts fresh.
///
/// Byte-for-byte `ActorsShared80166dd4` of `actor_341700` / `actor_342400`,
/// which is this same source written on that pair's work type. The two are
/// separate shared units of this family (this one is `actor_104400`'s), so
/// neither absorbs the other.
void Actor04400_Fn03CA0(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    s32              cond;
    s16              angle;
    s16              speed;
    s32              scale;

    work = (Actor104400Work*)arg0->work;
    if ((u16)(work->field_412++ - 0x1D) < 0xD) {
        scale                                           = 0x1E;
        angle                                           = work->field_7A + 0x400;
        speed                                           = (((Actor104400Work*)arg0->work)->field_41C * scale) << 0xC >> 0x10;
        ((TmdObject*)arg0->extra)->field_8->coord.t[0] += ((rsin(angle) << 4) * speed) >> 0x10;
        ((TmdObject*)arg0->extra)->field_8->coord.t[2] += ((rcos(angle) << 4) * speed) >> 0x10;
        ((TmdObject*)arg0->extra)->field_8->flg         = 0;
    }
    work2 = (Actor104400Work*)arg0->work;
    if ((work2->flags_EC.half & 1) || (work2->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_438  = 0;
        work2            = (Actor104400Work*)arg0->work;
        work2->field_426 = 8;
        work2->field_41C = 0x10;
        work2->field_418 = 3;
        work2->field_414 = 1;
        work->field_412  = 0;
        work->field_422++;
    }
}

/// Unless `Actor04400_Fn06328` claims the frame: count the frame in
/// `field_412` and, on frames 0x17..0x23, push the model root along the
/// heading `field_7A` turned a quarter circle, by `field_41C` scaled -30/16.
/// Once `flags_EC` reports a hit (bit 0 as a halfword, or 0x102 as a word),
/// `field_438` is cleared and the state machine rewinds to state 0.
/// Same body as `ActorsShared80166f54` up to the callee, which is this
/// overlay's own copy of the "frame claimed" test.
void Actor04400_Fn03E20(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    Actor104400Work* next;
    s32              cond;
    s16              angle;
    s16              speed;
    s32              scale;

    work = (Actor104400Work*)arg0->work;
    if ((Actor04400_Fn06328(arg0) << 0x10) == 0) {
        if ((u16)(work->field_412++ - 0x17) < 0xD) {
            scale                                           = -0x1E;
            angle                                           = work->field_7A + 0x400;
            speed                                           = (((Actor104400Work*)arg0->work)->field_41C * scale) << 0xC >> 0x10;
            ((TmdObject*)arg0->extra)->field_8->coord.t[0] += ((rsin(angle) << 4) * speed) >> 0x10;
            ((TmdObject*)arg0->extra)->field_8->coord.t[2] += ((rcos(angle) << 4) * speed) >> 0x10;
            ((TmdObject*)arg0->extra)->field_8->flg         = 0;
        }
        work2 = (Actor104400Work*)arg0->work;
        if ((work2->flags_EC.half & 1) || (work2->flags_EC.word & 0x102)) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            work->field_438 = 0;
            next            = (Actor104400Work*)arg0->work;
            next->field_420 = 0;
            next->field_422 = 0;
        }
    }
}

/// `ActorsShared8013a2c0`'s body, inlined: push the model's second coordinate's
/// world position onto `G_SCRATCH_HEAD` and hand it to `Gp_UpdateActorColor`.
/// This overlay's copy lives in `actor_104400_text.c`; the tail unit needs its
/// own because the two are separate translation units.
static __inline__ void Actor04400_UpdateColor(void* enemy, GsCOORDINATE2* coord)
{
    VECTOR* block = (VECTOR*)(*(u8**)G_SCRATCH_HEAD - 0x10);

    block->vx                 = coord->workm.t[0];
    block->vy                 = coord->workm.t[1];
    *(VECTOR**)G_SCRATCH_HEAD = block;
    block->vz                 = coord->workm.t[2];
    Gp_UpdateActorColor(enemy, block, 0, 0);
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x10;
}

/// Coalesces the `0x2C00` hit message: returns 1 when the low nibble of
/// `field_44C` is 3, which consumes it into state 7 with a fresh state machine
/// so the caller skips this frame's handler.
static __inline__ s16 Actor04400_TakeHit(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;
    s16              hit  = 0;
    Actor104400Work* w2;

    if ((work->field_44C & 0xF) == 3) {
        hit             = 1;
        work->field_44C = 0;
        arg0->state     = 7;
        w2              = (Actor104400Work*)arg0->work;
        w2->field_420   = 0;
        w2->field_422   = 0;
    }
    return hit;
}

/// Wraps the pitch / heading / roll at 0x78..0x7C to 12 bits and rebuilds the
/// model root's rotation from them (Z, then X, then the heading) in a matrix
/// taken off `G_SCRATCH_HEAD`, copying the 3x3 into the root coordinate.
static __inline__ void Actor04400_UpdateRotation(Task* arg0)
{
    Actor104400Work* work  = (Actor104400Work*)arg0->work;
    MATRIX*          m     = (MATRIX*)(*(u8**)G_SCRATCH_HEAD - 0x20);
    GsCOORDINATE2*   coord = ((TmdObject*)arg0->extra)->field_8;
    MATRIX*          dst;

    work->field_78           &= 0xFFF;
    work->field_7A           &= 0xFFF;
    work->field_7C           &= 0xFFF;
    *(s32*)&m->m[0][0]        = 0x1000;
    *(s32*)&m->m[0][2]        = 0;
    *(s32*)&m->m[1][1]        = 0x1000;
    *(s32*)&m->m[2][0]        = 0;
    m->m[2][2]                = 0x1000;
    *(MATRIX**)G_SCRATCH_HEAD = m;
    RotMatrixZ(work->field_7C, m);
    RotMatrixX(work->field_78, m);
    func_8004BFF8(work->field_7A, m);
    dst                   = &coord->coord;
    dst->m[0][0]          = m->m[0][0];
    dst->m[0][1]          = m->m[0][1];
    dst->m[0][2]          = m->m[0][2];
    dst->m[1][0]          = m->m[1][0];
    dst->m[1][1]          = m->m[1][1];
    dst->m[1][2]          = m->m[1][2];
    dst->m[2][0]          = m->m[2][0];
    dst->m[2][1]          = m->m[2][1];
    *(u8**)G_SCRATCH_HEAD = *(u8**)G_SCRATCH_HEAD + 0x20;
    dst->m[2][2]          = m->m[2][2];
}

/// Per-frame callback of the enemy this overlay drives, and the ten-state
/// counterpart of `Actor04400_Fn05DE0`: its handlers come from the
/// `Actor04400_D00184` table copied onto the stack, and in mode 0 a pending hit
/// (`Actor04400_TakeHit`) replaces this frame's handler. `Actor04400_Fn02B8C`
/// advances the animation, the root rotation is rebuilt from 0x78..0x7C, and
/// `Actor04400_Fn022A8` applies the frame's motion before the root coordinate
/// is marked dirty. Mode 1 re-pushes the model's second coordinate for
/// `Gp_UpdateActorColor` and rebuilds the part-pair colour quads while
/// `field_451` is clear. `D_801153F4` short-circuits both: 1 runs mode 1 only,
/// 2 hides the model instead.
///
/// The shape is `func_actor_342400_801670C0`'s on this overlay's work block,
/// with this overlay's own animation and motion helpers in place of that
/// actor's.
void Actor04400_Fn03F8C(Task* arg0)
{
    TmdObject*       obj   = arg0->extra;
    Actor104400Work* work  = (Actor104400Work*)arg0->work;
    GsCOORDINATE2*   coord = obj->field_8;
    TaskFuncTable10  sp    = Actor04400_D00184;

    switch (D_801153F4) {
        case 2:
            obj->field_C |= 0x80;
            return;
        case 0:
            work->field_442++;
            if (Actor04400_TakeHit(arg0) == 0) {
                sp.funcs[(s16)work->field_420](arg0);
            }
            Actor04400_Fn02B8C(arg0);
            Actor04400_UpdateRotation(arg0);
            Actor04400_Fn022A8(arg0, 0);
            coord->flg = 0;
        case 1:
            Actor04400_UpdateColor(arg0->spawnArg2, &((TmdObject*)arg0->extra)->field_8[1]);
            if (work->field_451 == 0) {
                Actor04400_Fn00220(arg0, 2, 6, 0xC8, 0, 0xFF);
                Actor04400_Fn00220(arg0, 1, 7, 0x80, 0, 0xFF);
                Actor04400_Fn00220(arg0, 7, 8, 0x80, 0, 0xFF);
            }
            return;
    }
}

/// Moves the task's state machine to `state`, sub-state 0. Each call site
/// reloads `work`, and cross-jumping merges the identical stores, which is
/// what leaves one `lw` per arm in front of a shared tail.
static __inline__ void Actor04400_SetState(Task* arg0, s16 state)
{
    Actor104400Work* w = (Actor104400Work*)arg0->work;

    w->field_420 = state;
    w->field_422 = 0;
}

/// Message 0x2C00 with low nibble 1 (see `field_44C`): reveals the model,
/// places its root at the spawn point bits 8..11 select from the current map's
/// table (playing the appearance sound on map 0x427), and starts state 1, 4
/// or 7 by bits 4..7.
///
/// The same body as `ActorsShared801673f8`, which `actor_341700` and
/// `actor_342400` link from `src/actors/lib/`; this overlay puts its own copy
/// at its own address, so the two cannot share one object.
void Actor04400_Fn042C4(Task* arg0)
{
    Actor104400Work* work  = (Actor104400Work*)arg0->work;
    TmdObject*       obj   = arg0->extra;
    GpEnemy*         enemy = arg0->spawnArg2;
    GsCOORDINATE2*   coord = obj->field_8;
    Actor104400Work* w2;
    s32              id;
    s32              pan;
    u32              map;

    if ((work->field_44C & 0xF) == 1) {
        work->field_451      = 1;
        work->obj_2AC.flags |= 0x8000;
        work->obj_2CC.flags &= 0xBFFF;
        obj->field_C        &= 0xFF7F;
        if ((arg0->spawnArg1 & 0xF) != 2) {
            Tmd_AllocBuffers(obj);
            obj->field_C &= 0xFFFB;
        }
        enemy->node.field_4 = 0;
        map                 = *(u32*)&gGameSession->loc & 0xFFFF0000;
        if (map == 0x4270000) {
            work->field_78    = 0;
            work->field_7A    = (D_8018B74C[(work->field_44C >> 8) & 0xF].heading + 0x800) & 0xFFF;
            work->field_7C    = 0;
            coord->coord.t[0] = D_8018B74C[(work->field_44C >> 8) & 0xF].x;
            coord->coord.t[1] = D_8018B74C[(work->field_44C >> 8) & 0xF].y;
            coord->coord.t[2] = D_8018B74C[(work->field_44C >> 8) & 0xF].z;
            id                = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 12) << 8) | 0x54270006;
            pan               = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
            SndEvt_EnqueueType6(id, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
        } else if (map == 0x4280000) {
            work->field_78    = 0;
            work->field_7A    = (D_801874C4[(work->field_44C >> 8) & 0xF].heading + 0x800) & 0xFFF;
            work->field_7C    = 0;
            coord->coord.t[0] = D_801874C4[(work->field_44C >> 8) & 0xF].x;
            coord->coord.t[1] = D_801874C4[(work->field_44C >> 8) & 0xF].y;
            coord->coord.t[2] = D_801874C4[(work->field_44C >> 8) & 0xF].z;
        }
        work->field_428 = 0;
        work->field_42A = 100;
        w2              = (Actor104400Work*)arg0->work;
        w2->field_41C   = 0x10;
        w2->field_418   = 7;
        w2->field_414   = 2;
        switch ((work->field_44C >> 4) & 0xF) {
            case 0:
                Actor04400_SetState(arg0, 1);
                break;
            case 1:
                Actor04400_SetState(arg0, 4);
                break;
            default:
                Actor04400_SetState(arg0, 7);
                break;
        }
        coord->flg = 0;
        Gp_UpdateCoord(coord);
        work->field_44C = 0;
    }
}

/// Same body as `Actor04400_Fn04A3C` with a slower push and a wider pitch
/// step: the root moves 0x8C back along the heading `field_7A`, `field_78`
/// eases an eighth of the way to 0x800 rather than a thirty-second of the way
/// to 0x200, and the heading turns half a circle before animation 0x11 is
/// requested at speed 0x10.
void Actor04400_Fn045A0(Task* arg0)
{
    Actor104400Work* work;
    s16              angle;
    GsCOORDINATE2*   coord;
    Actor104400Work* anim;
    s32              speed;
    s32              dx;

    work                                            = (Actor104400Work*)arg0->work;
    angle                                           = work->field_7A;
    coord                                           = ((TmdObject*)arg0->extra)->field_8;
    dx                                              = rsin(angle) << 4;
    speed                                           = -0x8C;
    ((TmdObject*)arg0->extra)->field_8->coord.t[0] += (dx * speed) >> 16;
    ((TmdObject*)arg0->extra)->field_8->coord.t[2] += ((rcos(angle) << 4) * speed) >> 16;
    ((TmdObject*)arg0->extra)->field_8->flg         = 0;
    work->field_78                                 += (0x800 - work->field_78) >> 3;
    coord->coord.t[1]                              += work->field_42A;
    work->field_428                                += 2;
    work->field_42A                                += work->field_428;
    if (coord->coord.t[1] > 0) {
        work->field_451   = 0;
        work->field_412   = 0;
        coord->coord.t[1] = -0x3C;
        work->field_78    = 0;
        work->field_7C    = 0;
        work->field_7A   += 0x800;
        anim              = (Actor104400Work*)arg0->work;
        anim->field_41C   = 0x10;
        anim->field_418   = 0x11;
        anim->field_414   = 2;
        work->field_428   = 0;
        work->field_42A   = -0x6E;
        work->field_420++;
    }
}

/// Same body as `ActorsShared8016784c` at this overlay's own address, so the
/// two cannot share one object: the package needs both addresses.
///
/// Counts `field_412` up and on the first frame plays sound 0x402C0009 (bank
/// from the enemy's `field_8` high nibble) panned and attenuated from the model
/// root. Every frame, pushes the root 0x14 forward along the heading
/// `field_7A`, then walks the root's y by `field_42A` while `field_428` ramps
/// it by 4 and feeds that back into `field_42A`. Once the root y passes zero it
/// snaps back to -0x3C and starts the cycle again, clearing the frame counter
/// and stepping the state `field_420`.
void Actor04400_Fn04718(Task* arg0)
{
    Actor104400Work* work;
    GsCOORDINATE2*   coord;
    s32              soundId;
    s32              pan;
    s16              angle;
    s16              speed;

    work  = (Actor104400Work*)arg0->work;
    coord = ((TmdObject*)arg0->extra)->field_8;
    if ((s16)++work->field_412 == 1) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x402C0009;
        pan     = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
    }
    speed                                           = 0x50;
    angle                                           = work->field_7A;
    ((TmdObject*)arg0->extra)->field_8->coord.t[0] += ((rsin(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->field_8->coord.t[2] += ((rcos(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->field_8->flg         = 0;
    coord->coord.t[1]                              += work->field_42A;
    work->field_428                                += 4;
    work->field_42A                                += work->field_428;
    if (coord->coord.t[1] > 0) {
        coord->coord.t[1] = -0x3C;
        work->field_412   = 0;
        work->field_420++;
    }
}

/// Same body as `Actor04400_Fn05260` at this overlay's own address: counts
/// `field_412` up and on the first frame plays sound 0x402C0009 (bank from the
/// enemy's `field_8` high nibble) panned and attenuated from the model root.
/// Every frame, pushes the root 0x14 forward along the heading `field_7A`.
/// Once status bit 0 or bits 0x102 of `flags_EC` are set, flags `obj_2CC` with
/// 0x4000 and switches the task to state 3 with the work block's state 5,
/// sub-state 0.
void Actor04400_Fn048A0(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    Actor104400Work* next;
    Actor104400Work* next2;
    s32              soundId;
    s32              pan;
    s32              cond;
    s16              angle;
    s16              speed;

    work = (Actor104400Work*)arg0->work;
    if ((s16)++work->field_412 == 1) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x402C0009;
        pan     = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
    }
    speed                                           = 0x14;
    angle                                           = work->field_7A;
    ((TmdObject*)arg0->extra)->field_8->coord.t[0] += ((rsin(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->field_8->coord.t[2] += ((rcos(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->field_8->flg         = 0;
    work2                                           = (Actor104400Work*)arg0->work;
    if ((work2->flags_EC.half & 1) || (work2->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->obj_2CC.flags |= 0x4000;
        next                 = (Actor104400Work*)arg0->work;
        arg0->state          = 3;
        next->field_420      = 0;
        next->field_422      = 0;
        next2                = (Actor104400Work*)arg0->work;
        next2->field_420     = 5;
        next2->field_422     = 0;
    }
}

/// Same body as `ActorsShared80167b70`. This overlay's whole `.text` is already
/// one shared span, so it cannot join that unit.
///
/// Pushes the model root 0x8C back against the heading `field_7A`, eases
/// `field_78` a 32nd of the way to 0x200, and falls under an accelerating drop
/// (`field_428` the acceleration, `field_42A` the speed). Once the root passes
/// above y = 0 it is pinned at -0x3C, `field_78` / `field_7C` clear, the fall
/// rearms at speed -0x6E, animation 12 (kind 2, speed 0x10) is requested and the
/// state advances.
void Actor04400_Fn04A3C(Task* arg0)
{
    Actor104400Work* work;
    s16              angle;
    GsCOORDINATE2*   coord;
    Actor104400Work* anim;
    s32              speed;
    s32              dx;

    work                                            = (Actor104400Work*)arg0->work;
    angle                                           = work->field_7A;
    coord                                           = ((TmdObject*)arg0->extra)->field_8;
    dx                                              = rsin(angle) << 4;
    speed                                           = -0x8C;
    ((TmdObject*)arg0->extra)->field_8->coord.t[0] += (dx * speed) >> 16;
    ((TmdObject*)arg0->extra)->field_8->coord.t[2] += ((rcos(angle) << 4) * speed) >> 16;
    ((TmdObject*)arg0->extra)->field_8->flg         = 0;
    work->field_78                                 += (0x200 - work->field_78) >> 5;
    coord->coord.t[1]                              += work->field_42A;
    work->field_428                                += 2;
    work->field_42A                                += work->field_428;
    if (coord->coord.t[1] > 0) {
        work->field_451   = 0;
        work->field_412   = 0;
        coord->coord.t[1] = -0x3C;
        work->field_78    = 0;
        work->field_7C    = 0;
        anim              = (Actor104400Work*)arg0->work;
        anim->field_41C   = 0x10;
        anim->field_418   = 0xC;
        anim->field_414   = 2;
        work->field_428   = 0;
        work->field_42A   = -0x6E;
        work->field_420++;
    }
}

/// Same body as `ActorsShared80167cdc`, which this overlay cannot join: its
/// whole `.text` is already one shared span.
///
/// Counts the frame and decays `field_78` by a thirty-second towards 0. On the
/// first frame plays sound 0x402C0009 (bank from the enemy's `field_8` high
/// nibble) panned and attenuated from the model root. Every frame, pushes the
/// root 0x50 back against the heading `field_7A`. Once the root passes above
/// y = 0 it is pinned at -0x3C, the frame counter clears and the state
/// advances.
void Actor04400_Fn04BA8(Task* arg0)
{
    Actor104400Work* work;
    GsCOORDINATE2*   coord;
    s32              soundId;
    s32              pan;
    s16              angle;
    s16              speed;

    work  = (Actor104400Work*)arg0->work;
    coord = ((TmdObject*)arg0->extra)->field_8;
    work->field_412++;
    work->field_78 += -work->field_78 >> 5;
    if ((s16)work->field_412 == 1) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x402C0009;
        pan     = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
    }
    speed                                           = -0x50;
    angle                                           = work->field_7A;
    ((TmdObject*)arg0->extra)->field_8->coord.t[0] += ((rsin(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->field_8->coord.t[2] += ((rcos(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->field_8->flg         = 0;
    coord->coord.t[1]                              += work->field_42A;
    work->field_428                                += 4;
    work->field_42A                                += work->field_428;
    if (coord->coord.t[1] > 0) {
        coord->coord.t[1] = -0x3C;
        work->field_412   = 0;
        work->field_420++;
    }
}

/// Same body as `ActorsShared80167e78` at this overlay's own address. This
/// overlay's whole `.text` is already one shared span, so it cannot join that
/// unit.
///
/// Counts `field_412` up and on the first frame plays sound 0x402C0009 (bank
/// from the enemy's `field_8` high nibble) panned and attenuated from the model
/// root. Every frame, pushes the root 0x14 back against the heading `field_7A`.
/// Once status bit 0 or bits 0x102 of `flags_EC` are set, flags `obj_2CC` with
/// 0x4000 and switches the task to state 3 with the work block's state 3,
/// sub-state 0.
void Actor04400_Fn04D44(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    Actor104400Work* next;
    Actor104400Work* next2;
    s32              soundId;
    s32              pan;
    s32              cond;
    s16              angle;
    s16              speed;

    work = (Actor104400Work*)arg0->work;
    if ((s16)++work->field_412 == 1) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x402C0009;
        pan     = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
    }
    speed                                           = -0x14;
    angle                                           = work->field_7A;
    ((TmdObject*)arg0->extra)->field_8->coord.t[0] += ((rsin(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->field_8->coord.t[2] += ((rcos(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->field_8->flg         = 0;
    work2                                           = (Actor104400Work*)arg0->work;
    if ((work2->flags_EC.half & 1) || (work2->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->obj_2CC.flags |= 0x4000;
        next                 = (Actor104400Work*)arg0->work;
        arg0->state          = 3;
        next->field_420      = 0;
        next->field_422      = 0;
        next2                = (Actor104400Work*)arg0->work;
        next2->field_420     = 3;
        next2->field_422     = 0;
    }
}

/// Same body as `ActorsShared80168010`. This overlay's whole `.text` is already
/// one shared span, so it cannot join that unit.
///
/// Pushes the model root 0x8C back against the heading `field_7A`, eases
/// `field_78` a 32nd of the way to 0x200, and falls under an accelerating drop
/// (`field_428` the acceleration, `field_42A` the speed). Once the root passes
/// above y = 0 it is pinned at -0x3C, the fall rearms at speed -0x12C,
/// animation 12 (kind 2, speed 0x10) is requested and the state advances.
void Actor04400_Fn04EDC(Task* arg0)
{
    Actor104400Work* work;
    s16              angle;
    GsCOORDINATE2*   coord;
    Actor104400Work* anim;
    s32              speed;
    s32              dx;

    work                                            = (Actor104400Work*)arg0->work;
    angle                                           = work->field_7A;
    coord                                           = ((TmdObject*)arg0->extra)->field_8;
    dx                                              = rsin(angle) << 4;
    speed                                           = -0x8C;
    ((TmdObject*)arg0->extra)->field_8->coord.t[0] += (dx * speed) >> 16;
    ((TmdObject*)arg0->extra)->field_8->coord.t[2] += ((rcos(angle) << 4) * speed) >> 16;
    ((TmdObject*)arg0->extra)->field_8->flg         = 0;
    work->field_78                                 += (0x200 - work->field_78) >> 5;
    coord->coord.t[1]                              += work->field_42A;
    work->field_428                                += 2;
    work->field_42A                                += work->field_428;
    if (coord->coord.t[1] > 0) {
        work->field_451   = 0;
        work->field_412   = 0;
        coord->coord.t[1] = -0x3C;
        anim              = (Actor104400Work*)arg0->work;
        anim->field_41C   = 0x10;
        anim->field_418   = 0xC;
        anim->field_414   = 2;
        work->field_428   = 0;
        work->field_42A   = -0x12C;
        work->field_420++;
    }
}

/// Same body as `ActorsShared80168174`. This overlay's whole `.text` is already
/// one shared span, so it cannot join that unit.
///
/// Counts `field_412` up and eases `field_78` an eighth of the way to 0x800. On
/// the first frame plays sounds 0x402C0009 and 0x402C0003 (bank from the
/// enemy's `field_8` high nibble) panned and attenuated from the model root.
/// Every frame, pushes the root 0x5A back against the heading `field_7A` and
/// falls as `Actor04400_Fn04BA8` does. Once the root passes above y = 0 it is
/// pinned at -0x3C, `field_78` / `field_7C` clear, the heading turns half a
/// circle, animation 0x11 is requested at speed 0x10 and the state advances.
void Actor04400_Fn05040(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* anim;
    GsCOORDINATE2*   coord;
    s32              soundId;
    s32              pan;
    s32              soundId2;
    s32              pan2;
    s16              angle;
    s16              speed;

    work  = (Actor104400Work*)arg0->work;
    coord = ((TmdObject*)arg0->extra)->field_8;
    work->field_412++;
    work->field_78 += (0x800 - work->field_78) >> 3;
    if ((s16)work->field_412 == 1) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x402C0009;
        pan     = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
        soundId2 = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x402C0003;
        pan2     = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(soundId2, pan2, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
    }
    speed                                           = -0x5A;
    angle                                           = work->field_7A;
    ((TmdObject*)arg0->extra)->field_8->coord.t[0] += ((rsin(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->field_8->coord.t[2] += ((rcos(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->field_8->flg         = 0;
    coord->coord.t[1]                              += work->field_42A;
    work->field_428                                += 4;
    work->field_42A                                += work->field_428;
    if (coord->coord.t[1] > 0) {
        coord->coord.t[1] = -0x3C;
        work->field_78    = 0;
        work->field_7C    = 0;
        work->field_7A   += 0x800;
        anim              = (Actor104400Work*)arg0->work;
        anim->field_41C   = 0x10;
        anim->field_418   = 0x11;
        anim->field_414   = 2;
        work->field_412   = 0;
        work->field_420++;
    }
}

/// Same body as `Actor04400_Fn04D44` with the opposite step: the push is 0x14
/// forward along the heading and the state it lands on is 5 rather than 3.
///
/// Counts `field_412` up and on the first frame plays sound 0x402C0009 (bank
/// from the enemy's `field_8` high nibble) panned and attenuated from the model
/// root. Every frame, pushes the root 0x14 forward along the heading `field_7A`.
/// Once status bit 0 or bits 0x102 of `flags_EC` are set, flags `obj_2CC` with
/// 0x4000 and switches the task to state 3 with the work block's state 5,
/// sub-state 0.
void Actor04400_Fn05260(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    Actor104400Work* next;
    Actor104400Work* next2;
    s32              soundId;
    s32              pan;
    s32              cond;
    s16              angle;
    s16              speed;

    work = (Actor104400Work*)arg0->work;
    if ((s16)++work->field_412 == 1) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x402C0009;
        pan     = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
    }
    speed                                           = 0x14;
    angle                                           = work->field_7A;
    ((TmdObject*)arg0->extra)->field_8->coord.t[0] += ((rsin(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->field_8->coord.t[2] += ((rcos(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->field_8->flg         = 0;
    work2                                           = (Actor104400Work*)arg0->work;
    if ((work2->flags_EC.half & 1) || (work2->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->obj_2CC.flags |= 0x4000;
        next                 = (Actor104400Work*)arg0->work;
        arg0->state          = 3;
        next->field_420      = 0;
        next->field_422      = 0;
        next2                = (Actor104400Work*)arg0->work;
        next2->field_420     = 5;
        next2->field_422     = 0;
    }
}

/// Steering counterpart of `Actor04400_Fn048A0`, the same body as
/// `func_actor_342400_80168530` on this overlay's own work block.
///
/// While the enemy lives it ramps `field_41C` up to 0x40 along `field_44F`,
/// turns `field_7A` toward `field_70` and pushes the root back along it by
/// `field_41C` scaled -0x10, then slides toward `field_70` at the accelerating
/// `field_42A` scaled 1/64. Past 120 frames it eases the root y toward
/// `field_70`'s (a quarter within 3000 units, a thirty-second beyond) and marks
/// `field_438`; while alive a hit flag plays sound 0x402C0001 at the enemy's
/// pan and depth. Within 800 units it advances `field_422`, otherwise a dead
/// enemy queues its follow-up animation.
void Actor04400_Fn053FC(Task* arg0)
{
    TmdObject*       obj;
    Actor104400Work* work;
    GpEnemy*         enemy;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   c;
    VECTOR           d;
    SVECTOR          dir;
    VECTOR           sq;
    VECTOR*          out;
    s16              angle;
    s32              dist;
    s32              cond;
    s32              soundId;
    s32              pan;

    obj   = arg0->extra;
    work  = (Actor104400Work*)arg0->work;
    coord = obj->field_8;
    enemy = (GpEnemy*)arg0->spawnArg2;
    work->field_412++;
    if (enemy->field_40 > 0) {
        Actor104400Work* w;
        s32              diff;
        s32              k;
        s32              step;

        if ((u32)(work->field_44F >> 1) < 0x40) {
            work->field_41C = work->field_44F >> 2;
            work->field_44F++;
        } else {
            work->field_41C = 0x40;
        }
        w      = (Actor104400Work*)arg0->work;
        c      = ((TmdObject*)arg0->extra)->field_8;
        dir.vx = work->field_70.vx - c->coord.t[0];
        dir.vy = 0;
        dir.vz = work->field_70.vz - c->coord.t[2];
        VectorNormalSS(&dir, &dir);
        diff = (((u16)w->field_7A - ratan2(dir.vx, dir.vz)) << 20) >> 20;
        if (diff > 0x100) {
            w->field_7A -= 0x18;
        } else if (diff < -0x100) {
            w->field_7A += 0x18;
        }
        angle                                           = work->field_7A;
        k                                               = -0x10;
        step                                            = ((((Actor104400Work*)arg0->work)->field_41C * k) << 12) >> 16;
        ((TmdObject*)arg0->extra)->field_8->coord.t[0] += ((rsin(angle) << 4) * step) >> 16;
        ((TmdObject*)arg0->extra)->field_8->coord.t[2] += ((rcos(angle) << 4) * step) >> 16;
        ((TmdObject*)arg0->extra)->field_8->flg         = 0;
    }
    work->field_428++;
    work->field_42A += work->field_428;
    {
        s32 step = work->field_42A >> 6;

        c      = ((TmdObject*)arg0->extra)->field_8;
        dir.vx = work->field_70.vx - c->coord.t[0];
        dir.vy = 0;
        dir.vz = work->field_70.vz - c->coord.t[2];
        VectorNormalSS(&dir, &dir);
        angle                                           = ratan2(dir.vx, dir.vz);
        ((TmdObject*)arg0->extra)->field_8->coord.t[0] += ((rsin(angle) << 4) * step) >> 16;
        ((TmdObject*)arg0->extra)->field_8->coord.t[2] += ((rcos(angle) << 4) * step) >> 16;
        ((TmdObject*)arg0->extra)->field_8->flg         = 0;
    }
    d.vx = coord->coord.t[0] - work->field_70.vx;
    d.vy = coord->coord.t[1] - work->field_70.vy;
    d.vz = coord->coord.t[2] - work->field_70.vz;
    out  = &sq;
    gte_ldlvl(&d);
    __asm__ volatile("nop; nop; .word 0x4AA00428"); // sqr 0
    gte_stlvnl(out);
    dist = SquareRoot0(sq.vx + sq.vy + sq.vz);
    if ((s16)work->field_412 > 120) {
        if (dist <= 3000) {
            work->field_438    = 1;
            coord->coord.t[1] += (work->field_70.vy - coord->coord.t[1]) >> 4;
        } else {
            work->field_438    = 1;
            coord->coord.t[1] += (work->field_70.vy - coord->coord.t[1]) >> 5;
        }
    } else if (enemy->field_40 > 0) {
        Actor104400Work* w2 = (Actor104400Work*)arg0->work;

        if ((w2->flags_EC.half & 1) || (w2->flags_EC.word & 0x102)) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x402C0001;
            pan     = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
            SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
        }
    } else {
        work->field_438    = 1;
        coord->coord.t[1] += (work->field_70.vy - coord->coord.t[1]) >> 5;
    }
    if (dist < 800) {
        work->field_422++;
        return;
    }
    if (enemy->field_40 <= 0) {
        if (work->field_448 != 4) {
            work->field_438 = 1;
            if (work->field_418 == 8) {
                if (work->field_440 == 0) {
                    Actor104400Work* w = (Actor104400Work*)arg0->work;

                    w->field_426 = 4;
                    w->field_41C = 0x10;
                    w->field_418 = 5;
                    w->field_414 = 1;
                } else {
                    Actor104400Work* w = (Actor104400Work*)arg0->work;

                    w->field_426 = 4;
                    w->field_41C = 0x10;
                    w->field_418 = 6;
                    w->field_414 = 1;
                }
            } else {
                Actor104400Work* w;
                s16              next;

                next         = Actor04400_D10828[work->field_418 - 1];
                w            = (Actor104400Work*)arg0->work;
                w->field_426 = 4;
                w->field_41C = 0x10;
                w->field_418 = next;
                w->field_414 = 1;
            }
        } else {
            work->field_438 = 0;
        }
    }
}

/// Same body as `ActorsShared80168a28`. This overlay's whole `.text` is already
/// one shared span, so it cannot join that unit.
void Actor04400_Fn058F4(Task* arg0)
{
    Actor104400Work* objs;
    GpEnemy*         enemy;
    TmdObject*       tmd;
    Actor104400Work* work;
    s32              soundId;
    s32              pan;

    work            = (Actor104400Work*)arg0->work;
    enemy           = (GpEnemy*)arg0->spawnArg2;
    tmd             = (TmdObject*)arg0->extra;
    work->field_438 = 1;
    if (enemy->field_40 >= 0) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x402C0003;
        pan     = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
    }
    if ((Gp_StateF0.field_1F & 0xF) == (((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC)) {
        Gp_StateF0.field_1F = 0;
    }
    Gp_UnlinkNode(&enemy->node);
    Gp_ReleaseStateF0Add((GpObj20E*)arg0, 0);
    enemy->field_54 = 0;
    objs            = (Actor104400Work*)arg0->work;
    Gp_UnlinkObj(&objs->obj_2AC);
    Gp_UnlinkObj(&objs->obj_2CC);
    Gp_UnlinkObj(&objs->obj_3AC);
    ActorsShared_SetTaskState(arg0, 5);
    Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x13F4, 0, 0);
    tmd->field_C |= 0x80;
}

/// Sub-state handler: slides the model's root toward `field_70` in x/z,
/// accelerating with `field_42A`; after 90 frames it also eases y in and marks
/// `field_438`. Within 800 units it advances `field_422`; if the enemy's HP is
/// gone instead, it queues the follow-up animation (or clears `field_438` when
/// state 4 is pending).
///
/// The same body as `func_actor_342400_80168B74` and
/// `func_actor_341700_80167890`, except that the animation to follow id 8 with
/// comes from this overlay's `Actor04400_D10828`. The three name different
/// tables, so the bodies are not interchangeable and each is matched on its own.
void Actor04400_Fn05A40(Task* arg0)
{
    TmdObject*       obj;
    Actor104400Work* work;
    GpEnemy*         enemy;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   c;
    VECTOR           d;
    SVECTOR          dir;
    VECTOR           sq;
    VECTOR*          out;
    s16              angle;
    s16              next;

    obj   = arg0->extra;
    work  = (Actor104400Work*)arg0->work;
    enemy = (GpEnemy*)arg0->spawnArg2;
    coord = obj->field_8;
    work->field_412++;
    work->field_428++;
    work->field_42A += work->field_428;
    if ((s16)work->field_412 < 0x5A) {
        s32 step = work->field_42A >> 6;

        c      = ((TmdObject*)arg0->extra)->field_8;
        dir.vx = work->field_70.vx - c->coord.t[0];
        dir.vy = 0;
        dir.vz = work->field_70.vz - c->coord.t[2];
        VectorNormalSS(&dir, &dir);
        angle                                           = ratan2(dir.vx, dir.vz);
        ((TmdObject*)arg0->extra)->field_8->coord.t[0] += ((rsin(angle) << 4) * step) >> 16;
        ((TmdObject*)arg0->extra)->field_8->coord.t[2] += ((rcos(angle) << 4) * step) >> 16;
        ((TmdObject*)arg0->extra)->field_8->flg         = 0;
    } else {
        s32 step;

        work->field_438 = 1;
        step            = work->field_42A >> 5;
        c               = ((TmdObject*)arg0->extra)->field_8;
        dir.vx          = work->field_70.vx - c->coord.t[0];
        dir.vy          = 0;
        dir.vz          = work->field_70.vz - c->coord.t[2];
        VectorNormalSS(&dir, &dir);
        angle                                           = ratan2(dir.vx, dir.vz);
        ((TmdObject*)arg0->extra)->field_8->coord.t[0] += ((rsin(angle) << 4) * step) >> 16;
        ((TmdObject*)arg0->extra)->field_8->coord.t[2] += ((rcos(angle) << 4) * step) >> 16;
        ((TmdObject*)arg0->extra)->field_8->flg         = 0;
        coord->coord.t[1]                              += (work->field_70.vy - coord->coord.t[1]) >> 4;
    }
    d.vx = coord->coord.t[0] - work->field_70.vx;
    d.vy = coord->coord.t[1] - work->field_70.vy;
    d.vz = coord->coord.t[2] - work->field_70.vz;
    out  = &sq;
    gte_ldlvl(&d);
    __asm__ volatile("nop; nop; .word 0x4AA00428"); // sqr 0
    gte_stlvnl(out);
    if (SquareRoot0(sq.vx + sq.vy + sq.vz) < 800) {
        work->field_422++;
        return;
    }
    if (enemy->field_40 <= 0) {
        SndEvt_EnqueueType7(0x402C0002, 1);
        if (work->field_448 != 4) {
            work->field_438 = 1;
            if (work->field_418 == 8) {
                if (work->field_440 == 0) {
                    Actor104400Work* w = (Actor104400Work*)arg0->work;

                    w->field_426 = 4;
                    w->field_41C = 0x10;
                    w->field_418 = 5;
                    w->field_414 = 1;
                } else {
                    Actor104400Work* w = (Actor104400Work*)arg0->work;

                    w->field_426 = 4;
                    w->field_41C = 0x10;
                    w->field_418 = 6;
                    w->field_414 = 1;
                }
            } else {
                Actor104400Work* w;

                next         = Actor04400_D10828[work->field_418 - 1];
                w            = (Actor104400Work*)arg0->work;
                w->field_426 = 4;
                w->field_41C = 0x10;
                w->field_418 = next;
                w->field_414 = 1;
            }
        } else {
            work->field_438 = 0;
        }
    }
}

/// Same body as `func_actor_342400_80168F14`. This overlay's whole `.text` is
/// already one shared span, so it cannot join that unit.
///
/// The per-frame callback the actor's AI states are dispatched from: state 0
/// counts `field_442` up, runs the handler `field_420` selects from
/// `Actor04400_D001C4` and spawns effect 3 on the model's second coordinate
/// part every 32 frames, then falls into state 1, which re-pushes that
/// coordinate's world position for `Gp_UpdateActorColor` and rebuilds the
/// part-pair colour quads while `field_451` is clear. `D_801153F4` short-
/// circuits both: nonzero runs state 1 only, 2 hides the model instead.
void Actor04400_Fn05DE0(Task* arg0)
{
    TmdObject*       obj   = arg0->extra;
    Actor104400Work* work  = (Actor104400Work*)arg0->work;
    GsCOORDINATE2*   coord = obj->field_8;
    TaskFuncTable5   sp    = Actor04400_D001C4;

    switch (D_801153F4) {
        case 2:
            obj->field_C |= 0x80;
            return;
        case 0:
            work->field_442++;
            sp.funcs[(s16)work->field_420](arg0);
            if (!(work->field_442 & 0x1F)) {
                func_800FDB18(3, &((TmdObject*)arg0->extra)->field_8[1], NULL, &work->eff_3FC);
            }
            coord->flg = 0;
        case 1:
            Actor04400_UpdateColor(arg0->spawnArg2, &((TmdObject*)arg0->extra)->field_8[1]);
            if (work->field_451 == 0) {
                Actor04400_Fn00220(arg0, 2, 6, 0xC8, 0, 0xFF);
                Actor04400_Fn00220(arg0, 1, 7, 0x80, 0, 0xFF);
                Actor04400_Fn00220(arg0, 7, 8, 0x80, 0, 0xFF);
            }
            obj->field_C &= ~0x80;
            return;
    }
}

/// Same body as `func_actor_342400_801690FC`. This overlay's whole `.text` is
/// already one shared span, so it cannot join that unit.
///
/// `Actor04400_Fn05DE0`'s seven-state counterpart, and the only difference is
/// the exit: mode 1 ends without clearing bit 0x80 of the model flags, which
/// leaves `obj` live only as far as mode 2 and lets it stay in `$a0` instead of
/// a saved register.
void Actor04400_Fn05FC8(Task* arg0)
{
    TmdObject*       obj   = arg0->extra;
    Actor104400Work* work  = (Actor104400Work*)arg0->work;
    GsCOORDINATE2*   coord = obj->field_8;
    TaskFuncTable7   sp    = Actor04400_D001D8;

    switch (D_801153F4) {
        case 2:
            obj->field_C |= 0x80;
            return;
        case 0:
            work->field_442++;
            sp.funcs[(s16)work->field_420](arg0);
            if (!(work->field_442 & 0x1F)) {
                func_800FDB18(3, &((TmdObject*)arg0->extra)->field_8[1], NULL, &work->eff_3FC);
            }
            coord->flg = 0;
        case 1:
            Actor04400_UpdateColor(arg0->spawnArg2, &((TmdObject*)arg0->extra)->field_8[1]);
            if (work->field_451 == 0) {
                Actor04400_Fn00220(arg0, 2, 6, 0xC8, 0, 0xFF);
                Actor04400_Fn00220(arg0, 1, 7, 0x80, 0, 0xFF);
                Actor04400_Fn00220(arg0, 7, 8, 0x80, 0, 0xFF);
            }
            return;
    }
}

/// Same body as `ActorsShared801692e8`. This overlay's whole `.text` is already
/// one shared span, so it cannot join that unit.
///
/// Queues CD command 0x21 once, guarded by `D_80115415`: the first parameter
/// block selects 2 or 3 when session `field_7` is 4, `field_6` is 0x27 or 0x28
/// and `field_9` is 1 or 2 respectively, and 1 otherwise.
void Actor04400_Fn061B4(void)
{
    u8 param1[8];
    u8 param2[8];

    if (D_80115415 == 0) {
        /* Each branch makes its own call; jump2's cross-jumping merges the
         * identical tails after sched2, which is why the argument setup is
         * duplicated per branch in the target. */
        if (gGameSession->loc.stage == 4 && (u32)(gGameSession->loc.area - 0x27) < 2 && gGameSession->loc.place == 1) {
            param1[2] = 0xA;
            param1[0] = 2;
            param1[3] = 0;
            param2[0] = 0x2C;
            param2[3] = 0;
            param2[2] = 0;
            param2[1] = 0;
            CdCmd_Enqueue(0x21, param1, param2);
        } else if (gGameSession->loc.stage == 4 && (u32)(gGameSession->loc.area - 0x27) < 2 && gGameSession->loc.place == 2) {
            param1[2] = 0xA;
            param1[0] = 3;
            param1[3] = 0;
            param2[0] = 0x2C;
            param2[3] = 0;
            param2[2] = 0;
            param2[1] = 0;
            CdCmd_Enqueue(0x21, param1, param2);
        } else {
            param1[2] = 0xA;
            param1[0] = 1;
            param1[3] = 0;
            param2[0] = 0x2C;
            param2[3] = 0;
            param2[2] = 0;
            param2[1] = 0;
            CdCmd_Enqueue(0x21, param1, param2);
        }
        D_80115415 = 1;
    }
}

/// Walks the death sequence's two-state handler table on the work block's
/// state index, the same shape as `func_actor_341700_80168124`.
void Actor04400_Fn062D4(Task* arg0)
{
    Actor104400Work* work                = (Actor104400Work*)arg0->work;
    void             (*states[2])(Task*) = {
        Actor04400_Fn08718,
        Actor04400_Fn087E0,
    };

    states[(s16)work->field_420](arg0);
}

/// Same body as `ActorsShared8016945c`. This overlay's whole `.text` is already
/// one shared span, so it cannot join that unit.
s16 Actor04400_Fn06328(Task* arg0)
{
    if ((s8)Gp_StateF0.field_1F & 0x80) {
        ActorsShared_SetTaskState(arg0, 3);
        ActorsShared_SetWorkState(arg0, 5);
        return 1;
    }
    return 0;
}

/// Claims or releases this actor's spawn place in `Gp_StateF0.field_1F`:
/// `arg1` non-zero sets bit 7 from the place id in bits 12+ of the spawn
/// descriptor (unless the place is already claimed), and `arg1` zero clears the
/// byte when its low nibble still matches that place. Spawn paths pass 1,
/// despawn paths pass 0; `Actor04400_Fn06328` reads bit 7 back.
///
/// Same body as `func_actor_341700_801681C4` in `actor_341700` and
/// `func_actor_342400_801694A8` in `actor_342400` — all three are byte-identical
/// — but those are separate packages whose copies sit at their own link
/// offsets, so this one cannot join them.
void Actor04400_Fn06374(Task* arg0, s32 arg1)
{
    if ((arg1 << 0x10) != 0) {
        if (!((s8)Gp_StateF0.field_1F & 0x80)) {
            Gp_StateF0.field_1F = (((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) | 0x80;
        }
    } else if ((Gp_StateF0.field_1F & 0xF) == (((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC)) {
        Gp_StateF0.field_1F = 0;
    }
}

/// While `field_41E` is 1, consumes the pending request in `field_448`:
/// requests 1..5 jump the state machine to states 6, 7, 8, 7 and 9 at
/// sub-state 0, anything else is just cleared. Returns 1 when `field_41E` is 1
/// and 0 otherwise. Each case reloads the work block through its own local;
/// one shared local lands in `$a0` instead of `$v1`.
s32 Actor04400_Fn063E4(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;

    if (work->field_41E == 1) {
        switch ((s16)(work->field_448 - 1)) {
            case 0: {
                Actor104400Work* w = (Actor104400Work*)arg0->work;
                w->field_420       = 6;
                w->field_422       = 0;
                break;
            }
            case 1: {
                Actor104400Work* w = (Actor104400Work*)arg0->work;
                w->field_420       = 7;
                w->field_422       = 0;
                break;
            }
            case 2: {
                Actor104400Work* w = (Actor104400Work*)arg0->work;
                w->field_420       = 8;
                w->field_422       = 0;
                break;
            }
            case 3: {
                Actor104400Work* w = (Actor104400Work*)arg0->work;
                w->field_420       = 7;
                w->field_422       = 0;
                break;
            }
            case 4: {
                Actor104400Work* w = (Actor104400Work*)arg0->work;
                w->field_420       = 9;
                w->field_422       = 0;
                break;
            }
        }
        work->field_448 = 0;
        return 1;
    }
    return 0;
}

/// Message handler: on message 0x2C00 whose low nibble is 1..5, store the
/// message halfword in `field_44C`. The five identical case bodies are
/// cross-jumped into one, but only separate bodies keep the jump table; a
/// single `case 1 ... 5` becomes a range test. `arg1` is the dispatch's
/// handler index and is unused here.
void Actor04400_Fn0648C(Task* arg0, s32 arg1, Actor104400Msg* arg2)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;

    if (arg2->field_0 == 0x2C00) {
        switch (arg2->field_2 & 0xF) {
            case 1:
                work->field_44C = arg2->field_2;
                break;
            case 2:
                work->field_44C = arg2->field_2;
                break;
            case 3:
                work->field_44C = arg2->field_2;
                break;
            case 4:
                work->field_44C = arg2->field_2;
                break;
            case 5:
                work->field_44C = arg2->field_2;
                break;
        }
    }
}

void Actor04400_Fn064EC(Task* task, s16 part, VECTOR3* pos)
{
    GsCOORDINATE2* coord;

    coord             = ((TmdObject*)task->extra)->field_8;
    coord->coord.t[0] = pos->vx;
    coord->coord.t[1] = pos->vy;
    coord->coord.t[2] = pos->vz;
    coord->flg        = 0;
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text_tail", Actor04400_Fn06520);

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text_tail", Actor04400_Fn065F4);

/// Same body as `ActorsShared8016974c`. This overlay's whole `.text` is already
/// one shared span, so it cannot join that unit.
s16 Actor04400_Fn06618(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;

    if ((work->flags_EC.half & 1) || (work->flags_EC.word & 0x102)) {
        return 1;
    }
    return 0;
}

/// Dispatches this overlay's `Actor04400_D0001C` dispatcher table by `Task::state`.
/// The body is `func_actor_342400_8016978C`'s, byte for byte.
void Actor04400_Fn06658(Task* arg0)
{
    TaskFuncTable10 sp;

    sp = Actor04400_D0001C;
    sp.funcs[arg0->state](arg0);
}

/// Dispatches this overlay's `Actor04400_D00004` dispatcher table by `Task::state`.
/// The body is `func_actor_341700_8016852C`'s, byte for byte.
void Actor04400_Fn066DC(Task* arg0)
{
    TaskFuncTable6 sp;

    sp = Actor04400_D00004;
    sp.funcs[arg0->state](arg0);
}

/// Runs the intro's two-state handler table on the work block's state index,
/// the same shape as `Actor04400_Fn062D4`.
void Actor04400_Fn0674C(Task* arg0)
{
    Actor104400Work* work                = (Actor104400Work*)arg0->work;
    void             (*states[2])(Task*) = {
        Actor04400_Fn07968,
        Actor04400_Fn07984,
    };

    states[(s16)work->field_420](arg0);
}

/// Same body as `ActorsShared801698d4`. This overlay's whole `.text` is already
/// one shared span, so it cannot join that unit.
void Actor04400_Fn067A0(Task* arg0, s32 step)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;
    SVECTOR          vec;
    s32              diff;
    u16              angle;
    s32              yaw;

    vec.vx = work->field_88;
    vec.vy = 0;
    vec.vz = work->field_8C;
    VectorNormalSS(&vec, &vec);
    yaw   = ratan2(-vec.vx, -vec.vz);
    angle = work->field_7A;
    diff  = ((angle - yaw) << 20) >> 20;
    if (diff > 0x100) {
        work->field_7A = angle - step;
    } else if (diff < -0x100) {
        work->field_7A = angle + step;
    }
}

void Actor04400_Fn06834(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;

    work->field_420 = 5;
    work->field_422 = 0;
}

void Actor04400_Fn06848(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;

    work->field_420 = 5;
    work->field_422 = 0;
}

void Actor04400_Fn0685C(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;

    work->field_420 = 5;
    work->field_422 = 0;
}

/// Copies this overlay's `Actor04400_D0007C` dispatcher table onto the stack and
/// lets the pending-request handler `Actor04400_Fn063E4` consume the request
/// first: the table entry `field_422` selects runs only when nothing was
/// consumed.
void Actor04400_Fn06870(Task* arg0)
{
    Actor104400Work* work;
    TaskFuncTable3   sp;

    work = (Actor104400Work*)arg0->work;
    sp   = Actor04400_D0007C;
    if ((s16)Actor04400_Fn063E4(arg0) == 0) {
        sp.funcs[(s16)work->field_422](arg0);
    }
}

/// Copies this overlay's five-entry `Actor04400_D00088` dispatcher table onto the
/// stack and calls the entry `field_422` selects, the same shape as
/// `Actor04400_Fn06964` with the other table.
void Actor04400_Fn068F8(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;
    TaskFuncTable5   sp;

    sp = Actor04400_D00088;
    sp.funcs[(s16)work->field_422](arg0);
}

/// Copies this overlay's five-entry `Actor04400_D0009C` dispatcher table onto the
/// stack and calls the entry `field_422` selects, the same shape as
/// `Actor04400_Fn06870` without the pending-request handler in front of it.
void Actor04400_Fn06964(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;
    TaskFuncTable5   sp;

    sp = Actor04400_D0009C;
    sp.funcs[(s16)work->field_422](arg0);
}

/// Dispatches through a two-entry table built on the stack: entry 0 applies the
/// encounter's animation (`Actor04400_Fn07A38`, which then advances `field_422`
/// itself), entry 1 runs the handler that answers a pending request or a hit
/// (`Actor04400_Fn03390`), chosen by the sub-state index `field_422`.
void Actor04400_Fn069D0(Task* arg0)
{
    Actor104400Work* work                = (Actor104400Work*)arg0->work;
    void             (*states[2])(Task*) = {
        Actor04400_Fn07A38,
        Actor04400_Fn03390,
    };

    states[(s16)work->field_422](arg0);
}

/// Dispatches through a two-entry table built on the stack: entry 0 applies the
/// animation the encounter asked for (`Actor04400_Fn07B4C`), entry 1 finishes
/// the encounter (`Actor04400_Fn07C60`), chosen by the sub-state index
/// `field_422`.
void Actor04400_Fn06A24(Task* arg0)
{
    Actor104400Work* work                = (Actor104400Work*)arg0->work;
    void             (*states[2])(Task*) = {
        Actor04400_Fn07B4C,
        Actor04400_Fn07C60,
    };

    states[(s16)work->field_422](arg0);
}

/// Dispatches through a two-entry table built on the stack: entry 0 advances the
/// animation sub-state (`Actor04400_Fn06BC4`), entry 1 runs the pending-request
/// handler (`Actor04400_Fn06BF8`), chosen by the sub-state index `field_422`.
void Actor04400_Fn06A78(Task* arg0)
{
    Actor104400Work* work                = (Actor104400Work*)arg0->work;
    void             (*states[2])(Task*) = {
        Actor04400_Fn06BC4,
        Actor04400_Fn06BF8,
    };

    states[(s16)work->field_422](arg0);
}

void Actor04400_Fn06ACC(Task* arg0)
{
    Actor104400Work* work;
    TaskFuncTable3   sp;

    work = (Actor104400Work*)arg0->work;
    sp   = Actor04400_D00070;
    sp.funcs[(s16)work->field_422](arg0);
    if (work->field_44F == 1) {
        Actor04400_Fn08DBC(arg0);
    }
}

/// Copies this overlay's six-entry `Actor04400_D001AC` dispatcher table onto the
/// stack and calls the entry `field_422` selects, the same shape as
/// `func_actor_342400_80169C84` with the other table.
void Actor04400_Fn06B50(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;
    TaskFuncTable6   sp;

    sp = Actor04400_D001AC;
    sp.funcs[(s16)work->field_422](arg0);
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text_tail", Actor04400_Fn06BC4);

void Actor04400_Fn06BF8(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;

    if ((Actor04400_Fn06618(arg0) << 0x10) != 0) {
        work            = (Actor104400Work*)arg0->work;
        work->field_426 = 4;
        work->field_41C = 0x10;
        work->field_418 = 0xB;
        work->field_414 = 1;
    }
    if (Gp_TickObjFlag2((GpObj5D*)arg0->spawnArg2) != 0) {
        work2            = (Actor104400Work*)arg0->work;
        work2->field_420 = 3;
        work2->field_422 = 0;
    }
}

void Actor04400_Fn06C70(Task* arg0)
{
    Actor104400Work* work;

    work            = (Actor104400Work*)arg0->work;
    work->field_44F = Actor04400_D10814[work->field_418 - 1];
    if (work->field_44F == 1) {
        Actor104400Work* w = (Actor104400Work*)arg0->work;

        w->field_426 = 6;
        w->field_41C = 0x10;
        w->field_418 = 6;
        w->field_414 = 1;
    } else {
        Actor104400Work* w = (Actor104400Work*)arg0->work;

        w->field_426 = 6;
        w->field_41C = 0x10;
        w->field_418 = 5;
        w->field_414 = 1;
    }
    work->field_422++;
}

void Actor04400_Fn06CF0(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* slow;
    Actor104400Work* fast;

    work = (Actor104400Work*)arg0->work;
    if ((Actor04400_Fn06618(arg0) << 0x10) != 0) {
        if (work->field_44F == 1) {
            fast            = (Actor104400Work*)arg0->work;
            fast->field_426 = 0x32;
            fast->field_41C = 0x10;
            fast->field_418 = 7;
            fast->field_414 = 1;
        } else {
            slow            = (Actor104400Work*)arg0->work;
            slow->field_426 = 0x1E;
            slow->field_41C = 0x10;
            slow->field_418 = 1;
            slow->field_414 = 1;
        }
        work->field_422 = work->field_422 + 1;
    }
}

void Actor04400_Fn06D90(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;

    if (Actor04400_Fn06618(arg0)) {
        if (work->field_44F == 1) {
            Actor104400Work* w = (Actor104400Work*)arg0->work;

            w->field_420 = 3;
            w->field_422 = 0;
        } else {
            Actor104400Work* w = (Actor104400Work*)arg0->work;

            w->field_420 = 5;
            w->field_422 = 0;
        }
    }
}

void Actor04400_Fn06DFC(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;
    s16              angle;
    s16              speed;

    Actor04400_Fn067A0(arg0, 0x10);
    speed                                           = Actor04400_Fn065F4(arg0, -0x10);
    angle                                           = work->field_7A;
    ((TmdObject*)arg0->extra)->field_8->coord.t[0] += ((rsin(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->field_8->coord.t[2] += ((rcos(angle) << 4) * speed) >> 0x10;
    ((TmdObject*)arg0->extra)->field_8->flg         = 0;
    if ((Actor04400_Fn06618(arg0) << 0x10) != 0) {
        Actor104400Work* next = (Actor104400Work*)arg0->work;

        next->field_420 = 4;
        next->field_422 = 0;
    }
}

/// Same body as `ActorsShared80168d3c`: seeds the model's root coord.t[1] from
/// the animation's, then resets the whole animation/state block. The `tmp`
/// variable is deliberate - reusing one halfword for 8 and then 1 is what the
/// original did, and it is what puts both constants in the same register.
void Actor04400_Fn06EEC(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    s16              tmp;

    work             = (Actor104400Work*)arg0->work;
    work->field_92   = *(u16*)&((TmdObject*)arg0->extra)->field_8->coord.t[1];
    work2            = (Actor104400Work*)arg0->work;
    tmp              = 8;
    work2->field_426 = tmp;
    work2->field_418 = tmp;
    work2->field_41C = 0x10;
    tmp              = 1;
    work2->field_414 = tmp;
    work->field_412  = 0;
    work->field_428  = 0;
    work->field_42A  = -0x12C;
    work->field_440  = tmp;
    work->field_438  = 0;
    work->field_432  = 0;
    work->field_422  = work->field_422 + 1;
}

/// Counts `field_412` up, clearing the death flag `field_438` on the way. On
/// frame 1 it plays the enemy's hit sound at the model's pan and depth, with
/// the id's high half taken from `GpEnemy::field_8`. Then, when
/// `Actor04400_Fn06618` accepts the frame, draws `field_44A` as 0x5A..0xD9 from
/// `Gp_LcgState` and enters state 3.
void Actor04400_Fn06F50(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    s32              soundId;
    s32              pan;
    u32              rand;

    work            = (Actor104400Work*)arg0->work;
    work->field_438 = 0;
    if ((s16)++work->field_412 == 1) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x402C0004;
        pan     = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
    }
    if ((Actor04400_Fn06618(arg0) << 0x10) != 0) {
        rand             = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState      = rand;
        work->field_44A  = ((rand >> 16) & 0x7F) + 0x5A;
        work2            = (Actor104400Work*)arg0->work;
        work2->field_420 = 3;
        work2->field_422 = 0;
    }
}

/// Same body as `ActorsShared8016a184`. This overlay's whole `.text` is already
/// one shared span, so it cannot join that unit.
void Actor04400_Fn07050(Task* arg0)
{
    Actor104400Work* work;
    GpEnemy*         enemy;
    s32              soundId;
    s32              pan;

    work            = (Actor104400Work*)arg0->work;
    enemy           = (GpEnemy*)arg0->spawnArg2;
    work->field_426 = 4;
    work->field_41C = 0x10;
    work->field_418 = 9;
    work->field_414 = 1;
    work->field_412 = 0;
    work->field_422++;
    if (enemy->field_40 > 0) {
        soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x402C0002;
        pan     = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
    }
}

void Actor04400_Fn0710C(Task* arg0)
{
    u16              ticks;
    Actor104400Work* work;

    work            = (Actor104400Work*)arg0->work;
    ticks           = work->field_412;
    work->field_412 = ticks + 1;
    if ((s16)ticks >= 0x51) {
        work->field_422 = work->field_422 + 1;
    }
}

void Actor04400_Fn0714C(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;

    work = (Actor104400Work*)arg0->work;
    if ((Actor04400_Fn06618(arg0) << 0x10) != 0) {
        Actor04400_Fn06374(arg0, 0);
        work2            = (Actor104400Work*)arg0->work;
        work2->field_426 = 8;
        work2->field_41C = 0x10;
        work2->field_418 = 0xF;
        work2->field_414 = 1;
        work->field_422  = work->field_422 + 1;
    }
}

void Actor04400_Fn071C8(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;

    work = (Actor104400Work*)arg0->work;
    if ((Actor04400_Fn06618(arg0) << 0x10) != 0) {
        work->field_438  = 1;
        work->field_412  = 0;
        work2            = (Actor104400Work*)arg0->work;
        work2->field_426 = 4;
        work2->field_41C = 0x10;
        work2->field_418 = 4;
        work2->field_414 = 1;
        work->field_422  = work->field_422 + 1;
    }
}

/// Frames 0x1D..0x29 of the state: each frame `Actor04400_Fn065F4` gives the
/// step speed, which pushes the model's root coordinate along `field_7A` +
/// 0x400 with `rsin`/`rcos` and clears the root flag. On the frame
/// `Actor04400_Fn06618` accepts, clears the death flag `field_438` and enters
/// state 3.
void Actor04400_Fn0723C(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    s16              angle;
    s16              speed;
    u16              ticks;

    work            = (Actor104400Work*)arg0->work;
    ticks           = work->field_412;
    work->field_412 = ticks + 1;
    if ((u32)((ticks - 0x1D) & 0xFFFF) < 0xDU) {
        speed                                           = Actor04400_Fn065F4(arg0, 0x1E);
        angle                                           = work->field_7A + 0x400;
        ((TmdObject*)arg0->extra)->field_8->coord.t[0] += ((rsin(angle) << 4) * speed) >> 0x10;
        ((TmdObject*)arg0->extra)->field_8->coord.t[2] += ((rcos(angle) << 4) * speed) >> 0x10;
        ((TmdObject*)arg0->extra)->field_8->flg         = 0;
    }
    if ((Actor04400_Fn06618(arg0) << 0x10) != 0) {
        work->field_438  = 0;
        work2            = (Actor104400Work*)arg0->work;
        work2->field_420 = 3;
        work2->field_422 = 0;
    }
}

/// Dispatches this overlay's `Actor04400_D000B0` dispatcher table by the
/// sub-state index `field_422`. Entry 2 is the fall-to-floor handler
/// `Actor04400_Fn02008` and entry 3 the landing it triggers
/// (`Actor04400_Fn0216C`).
void Actor04400_Fn07360(Task* arg0)
{
    Actor104400Work* work;
    TaskFuncTable4   sp;

    work = (Actor104400Work*)arg0->work;
    sp   = Actor04400_D000B0;
    sp.funcs[(s16)work->field_422](arg0);
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text_tail", Actor04400_Fn073C8);

/// Rebuild the model root's rotation: pitch about X by a sine sway driven by
/// `field_442`, then turn by the heading `field_7A`, and copy the 3x3 into the
/// root coordinate. When `field_41E` is 1, latch that pitch into `field_434`,
/// clear the flag and three motion halfwords, and advance `field_422`.
/// The same body as `ActorsShared8016a538`, on this overlay's work block; the
/// `field_442` read is signed even though the field is a `u16`, because the
/// sway phase turns negative.
void Actor04400_Fn07404(Task* arg0)
{
    Actor104400Work* work;
    GsCOORDINATE2*   coord;
    Actor104400Mat   rot;
    Actor104400Mat*  src;
    MATRIX*          dst;
    s16              pitch;

    work               = (Actor104400Work*)arg0->work;
    coord              = ((TmdObject*)arg0->extra)->field_8;
    src                = &rot;
    src->ident.m00_m01 = 0x1000;
    src->ident.m02_m10 = 0;
    src->ident.m11_m12 = 0x1000;
    src->ident.m20_m21 = 0;
    src->ident.m22     = 0x1000;
    pitch              = ((rsin((s16)work->field_442 << 6) * 0x10) >> 7) - 0x400;
    RotMatrixX(pitch, &src->mat);
    func_8004BFF8(work->field_7A, &src->mat);
    dst          = &coord->coord;
    dst->m[0][0] = src->mat.m[0][0];
    dst->m[0][1] = src->mat.m[0][1];
    dst->m[0][2] = src->mat.m[0][2];
    dst->m[1][0] = src->mat.m[1][0];
    dst->m[1][1] = src->mat.m[1][1];
    dst->m[1][2] = src->mat.m[1][2];
    dst->m[2][0] = src->mat.m[2][0];
    dst->m[2][1] = src->mat.m[2][1];
    dst->m[2][2] = src->mat.m[2][2];
    if (work->field_41E == 1) {
        work->field_41E = 0;
        work->field_432 = 0;
        work->field_428 = 0;
        work->field_42A = 0;
        work->field_434 = pitch;
        work->field_422++;
    }
}

void Actor04400_Fn07530(Task* arg0)
{
    GpEnemy*         enemy;
    Actor104400Work* work;
    TmdObject*       model;
    Actor104400Work* work2;

    enemy = (GpEnemy*)arg0->spawnArg2;
    model = (TmdObject*)arg0->extra;
    work  = (Actor104400Work*)arg0->work;
    SndEvt_EnqueueType7(((enemy->field_8 >> 0xC) << 8) | 0x402C0002, 0xF);
    Actor04400_Fn06374(arg0, 0);
    Gp_UnlinkNode(&enemy->node);
    if (work->field_448 == 4) {
        work->field_412  = 0;
        model->field_C   = model->field_C | 0x80;
        work2            = (Actor104400Work*)arg0->work;
        work2->field_420 = 7;
        work2->field_422 = 0;
        return;
    }
    work->field_420 = work->field_420 + 1;
}

/// Same body as `func_actor_342400_8016A724`.
void Actor04400_Fn075F0(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    Actor104400Work* work3;
    Actor104400Work* work4;
    s16              anim;
    s16              next;

    work = (Actor104400Work*)arg0->work;
    Gp_ReleaseStateF0Add((GpObj20E*)arg0, 0);
    anim = work->field_418;
    if (anim == 8) {
        if (work->field_440 == 0) {
            work2            = (Actor104400Work*)arg0->work;
            work2->field_426 = 4;
            work2->field_41C = 0x10;
            work2->field_418 = 5;
            work2->field_414 = 1;
        } else {
            work3            = (Actor104400Work*)arg0->work;
            work3->field_426 = 4;
            work3->field_41C = 0x10;
            work3->field_418 = 6;
            work3->field_414 = 1;
        }
    } else {
        next             = Actor04400_D10828[anim - 1];
        work4            = (Actor104400Work*)arg0->work;
        work4->field_426 = 4;
        work4->field_41C = 0x10;
        work4->field_418 = next;
        work4->field_414 = 1;
    }
    Actor04400_Fn02B8C(arg0);
    work->field_420++;
}

void Actor04400_Fn076D0(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    s32              cond;

    work = (Actor104400Work*)arg0->work;
    Actor04400_Fn02B8C();
    work2 = (Actor104400Work*)arg0->work;
    if ((work2->flags_EC.half & 1) || (work2->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_420 = work->field_420 + 1;
    }
}

void Actor04400_Fn07750(Task* arg0)
{
    GsCOORDINATE2*   coord = ((TmdObject*)arg0->extra)->field_8;
    GpEnemy*         enemy = (GpEnemy*)arg0->spawnArg2;
    Actor104400Work* work  = (Actor104400Work*)arg0->work;
    Actor104400Work* objWork;

    enemy->field_54 = 0;

    objWork = (Actor104400Work*)arg0->work;
    Gp_UnlinkObj(&objWork->obj_2AC);
    Gp_UnlinkObj(&objWork->obj_2CC);
    Gp_UnlinkObj(&objWork->obj_3AC);

    work->field_430 = 0x1000;
    work->matrix_0  = coord->coord;

    Gp_SetLightMode((GpObj4C*)arg0->spawnArg2, 1);

    work->field_412 = 0;
    work->field_420++;
}

/// Waits 0x18 frames on `field_412`, then hides the model by setting bit 1 of
/// `TmdObject.field_C` and returns the actor to the state that follows this one.
void Actor04400_Fn0781C(Task* arg0)
{
    Actor104400Work* work;
    TmdObject*       model;
    u16              ticks;

    work            = (Actor104400Work*)arg0->work;
    model           = (TmdObject*)arg0->extra;
    ticks           = work->field_412 + 1;
    work->field_412 = ticks;
    if ((s16)ticks >= 0x18) {
        model->field_C |= 2;
        work->field_412 = 0;
        work->field_451 = 1;
        work->field_420 = work->field_420 + 1;
    }
}

void Actor04400_Fn07878(Task* arg0)
{
    Actor104400Work* work;

    work            = (Actor104400Work*)arg0->work;
    arg0->state     = 5;
    work->field_420 = 0;
    work->field_422 = 0;
}

void Actor04400_Fn07890(Task* arg0)
{
    Actor104400Work* work;
    u16              ticks;

    work            = (Actor104400Work*)arg0->work;
    ticks           = work->field_412 + 1;
    work->field_412 = ticks;
    if ((s16)ticks >= 2) {
        work->field_420 = work->field_420 + 1;
    }
}

void Actor04400_Fn078D4(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    TmdObject*       model;
    GpEnemy*         enemy;

    model = (TmdObject*)arg0->extra;
    enemy = (GpEnemy*)arg0->spawnArg2;
    Tmd_FreeBuffers(model);
    model->field_C |= 4;
    Actor04400_Fn006A8(arg0);
    Gp_ReleaseStateF0Add((GpObj20E*)arg0, 0);
    enemy->field_54 = 0;
    work            = (Actor104400Work*)arg0->work;
    Gp_UnlinkObj(&work->obj_2AC);
    Gp_UnlinkObj(&work->obj_2CC);
    Gp_UnlinkObj(&work->obj_3AC);
    work2            = (Actor104400Work*)arg0->work;
    arg0->state      = 5;
    work2->field_420 = 0;
    work2->field_422 = 0;
}

void Actor04400_Fn07968(Task* arg0)
{
    Actor104400Work* work;

    work            = (Actor104400Work*)arg0->work;
    work->field_412 = 0;
    work->field_420 = work->field_420 + 1;
}

void Actor04400_Fn07984(Task* arg0)
{
    Actor104400Work* work;
    u16              ticks;

    work            = (Actor104400Work*)arg0->work;
    ticks           = work->field_412 + 1;
    work->field_412 = ticks;
    if ((s16)ticks >= 0x24) {
        if ((gGameSession->loc.stage == 4) && ((u32)(gGameSession->loc.area - 0x27) < 2U) && (gGameSession->loc.place == 1)) {
            Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x13F4, 1, 0);
        }
        Gp_DestroyEnemy(arg0->spawnArg2, arg0);
    }
}

/// Same body as `Actor04400_Fn07B4C` up to the two animation requests it
/// writes: this one asks for animation 0xB at speed 8 when the value overlapped
/// onto `field_44F` is 1, and animation 0x11 otherwise. Plays the same
/// encounter sound at the enemy's pan and depth and counts the sub-state up.
void Actor04400_Fn07A38(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    Actor104400Work* work3;
    s32              soundId;
    s32              pan;
    u8               kind;

    work            = (Actor104400Work*)arg0->work;
    kind            = Actor04400_D10814[work->field_418 - 1];
    work->field_44F = kind;
    if (kind == 1) {
        work2            = (Actor104400Work*)arg0->work;
        work2->field_426 = 8;
        work2->field_41C = 0x10;
        work2->field_418 = 0xB;
        work2->field_414 = 1;
        SndEvt_EnqueueType7(0x402C0002, 1);
    } else {
        work3            = (Actor104400Work*)arg0->work;
        work3->field_426 = 8;
        work3->field_41C = 0x10;
        work3->field_418 = 0x11;
        work3->field_414 = 1;
    }
    soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x402C0003;
    pan     = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
    SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
    work->field_422 = (u16)(work->field_422 + 1);
}

/// Applies the animation `field_418` asks for: overlaps the values at
/// `Actor04400_D10814[field_418 - 1]` onto `field_44F`, then requests either
/// animation 0xC (kind 1, with sound 0x402C0002) or animation 0x11 for the
/// enemy encountered, and plays the encounter sound at its pan and depth.
/// Counts the sub-state up.
void Actor04400_Fn07B4C(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    Actor104400Work* work3;
    s32              soundId;
    s32              pan;
    u8               kind;

    work            = (Actor104400Work*)arg0->work;
    kind            = Actor04400_D10814[work->field_418 - 1];
    work->field_44F = kind;
    if (kind == 1) {
        work2            = (Actor104400Work*)arg0->work;
        work2->field_426 = 2;
        work2->field_41C = 0x10;
        work2->field_418 = 0xC;
        work2->field_414 = 1;
        SndEvt_EnqueueType7(0x402C0002, 1);
    } else {
        work3            = (Actor104400Work*)arg0->work;
        work3->field_426 = 8;
        work3->field_41C = 0x10;
        work3->field_418 = 0x11;
        work3->field_414 = 1;
    }
    soundId = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x402C0003;
    pan     = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
    SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
    work->field_422 = (u16)(work->field_422 + 1);
}

void Actor04400_Fn07C60(Task* arg0)
{
    Actor104400Work* work;
    s32              cond;

    work = (Actor104400Work*)arg0->work;
    if ((work->flags_EC.half & 1) || (work->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        if (work->field_44F == 1) {
            work            = (Actor104400Work*)arg0->work;
            work->field_420 = 3;
            work->field_422 = 0;
        } else {
            Actor04400_Fn06374(arg0, 1);
            work            = (Actor104400Work*)arg0->work;
            work->field_420 = 5;
            work->field_422 = 0;
        }
    }
}

void Actor04400_Fn07CF0(Task* arg0)
{
    Actor104400Work* work;
    TaskFuncTable3   sp;

    work = (Actor104400Work*)arg0->work;
    sp   = Actor04400_D00150;
    if ((Actor04400_Fn06328(arg0) << 0x10) == 0) {
        sp.funcs[(s16)work->field_422](arg0);
    }
}

void Actor04400_Fn07D78(Task* arg0)
{
    Actor104400Work* work;
    TaskFuncTable3   sp;

    work = (Actor104400Work*)arg0->work;
    sp   = Actor04400_D0015C;
    if ((Actor04400_Fn06328(arg0) << 0x10) == 0) {
        sp.funcs[(s16)work->field_422](arg0);
    }
}

void Actor04400_Fn07E00(Task* arg0)
{
    Actor104400Work* work                = (Actor104400Work*)arg0->work;
    void             (*states[2])(Task*) = {
        Actor04400_Fn08208,
        Actor04400_Fn0823C,
    };

    if ((Actor04400_Fn06328(arg0) << 0x10) == 0) {
        states[(s16)work->field_422](arg0);
    }
}

extern TaskFuncTable3 Actor04400_D00168;

void Actor04400_Fn07E74(Task* arg0)
{
    Actor104400Work* work;
    TaskFuncTable3   sp;

    work = (Actor104400Work*)arg0->work;
    sp   = Actor04400_D00168;
    if ((Actor04400_Fn06328(arg0) << 0x10) != 0) {
        work->field_438 = 0;
        return;
    }
    sp.funcs[(s16)work->field_422](arg0);
}

void Actor04400_Fn07F04(Task* arg0)
{
    Actor104400Work* work;
    TaskFuncTable4   sp;

    work = (Actor104400Work*)arg0->work;
    sp   = Actor04400_D00174;
    sp.funcs[(s16)work->field_422](arg0);
}

void Actor04400_Fn07F6C(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;

    work->field_426 = 4;
    work->field_41C = 0x10;
    work->field_418 = 1;
    work->field_414 = 1;
    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
    work->field_446 = (((u32)Gp_LcgState >> 16) & 0x3F) + 0x60;
    work->field_412 = 0;
    work->field_422 = work->field_422 + 1;
}

/// Same body as `ActorsShared8016b104`. This overlay's whole `.text` is already
/// one shared span, so it cannot join that unit.
void Actor04400_Fn07FD0(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;
    s16              dist;

    if (work->field_446 < (s16)work->field_412++) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((Gp_LcgState >> 16) & 1) {
            Actor104400Work* w = (Actor104400Work*)arg0->work;

            w->field_420 = 4;
            w->field_422 = 0;
        } else {
            Actor104400Work* w = (Actor104400Work*)arg0->work;

            w->field_420 = 1;
            w->field_422 = 0;
        }
        return;
    }
    dist = work->field_43A;
    if (dist < 0xDAC) {
        Actor104400Work* next = (Actor104400Work*)arg0->work;

        next->field_420 = 3;
        next->field_422 = 0;
        return;
    }
    if (dist < 0x1388) {
        work->field_422++;
    }
}

void Actor04400_Fn08094(Task* arg0)
{
    Actor104400Work* work;
    s32              cond;

    work = (Actor104400Work*)arg0->work;
    if ((work->flags_EC.half & 1) || (work->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work            = (Actor104400Work*)arg0->work;
        work->field_420 = 1;
        work->field_422 = 0;
    }
}

void Actor04400_Fn080E8(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    s32              cond;

    work = (Actor104400Work*)arg0->work;
    if ((work->flags_EC.half & 1) || (work->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work2            = (Actor104400Work*)arg0->work;
        work2->field_426 = 8;
        work2->field_41C = 0x10;
        work2->field_418 = 0xD;
        work2->field_414 = 1;
        work->field_422++;
    }
}

void Actor04400_Fn08160(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    s32              cond;

    work = (Actor104400Work*)arg0->work;
    if ((work->flags_EC.half & 1) || (work->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work2            = (Actor104400Work*)arg0->work;
        work2->field_426 = 4;
        work2->field_41C = 0x10;
        work2->field_418 = 0xE;
        work2->field_414 = 1;
        Gp_LcgState      = Gp_LcgState * 5 + 0x71357911;
        work->field_412  = 0;
        work->field_42C  = 0;
        work->field_446  = (s16)((((u32)Gp_LcgState >> 16) & 0x3F) + 0xB0);
        work->field_422++;
    }
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text_tail", Actor04400_Fn08208);

void Actor04400_Fn0823C(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    s32              cond;

    work = (Actor104400Work*)arg0->work;
    if ((work->flags_EC.half & 1) || (work->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work2            = (Actor104400Work*)arg0->work;
        work2->field_420 = 0;
        work2->field_422 = 0;
    }
}

INCLUDE_ASM("actors/nonmatchings/lib/actor_104400_text_tail", Actor04400_Fn08290);

void Actor04400_Fn082E0(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    s32              cond;

    work = (Actor104400Work*)arg0->work;
    if ((work->flags_EC.half & 1) || (work->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_412  = 0;
        work->field_438  = 1;
        work2            = (Actor104400Work*)arg0->work;
        work2->field_426 = 4;
        work2->field_41C = 0x10;
        work2->field_418 = 4;
        work2->field_414 = 1;
        work->field_422++;
    }
}

void Actor04400_Fn08358(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;

    work = (Actor104400Work*)arg0->work;
    if ((Actor04400_Fn06328(arg0) << 0x10) == 0) {
        work2            = (Actor104400Work*)arg0->work;
        work2->field_426 = 8;
        work2->field_41C = 0x10;
        work2->field_418 = 0xF;
        work2->field_414 = 1;
        work->field_422  = work->field_422 + 1;
    }
}

/// Same body as `ActorsShared8016b500`, which tests the same `flags_EC` pair
/// through `ActorsShared8016945c`; this overlay's copy calls `Actor04400_Fn06328`.
/// Re-requests animation 4 whenever the enemy still carries status flag 1 or
/// 0x102, and counts `field_422` up.
void Actor04400_Fn083CC(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    Actor104400Work* work3;
    s32              cond;

    work = (Actor104400Work*)arg0->work;
    if ((Actor04400_Fn06328(arg0) << 0x10) == 0) {
        work2 = (Actor104400Work*)arg0->work;
        if ((work2->flags_EC.half & 1) || (work2->flags_EC.word & 0x102)) {
            cond = 1;
        } else {
            cond = 0;
        }
        if (cond) {
            work->field_412  = 0;
            work->field_438  = 1;
            work3            = (Actor104400Work*)arg0->work;
            work3->field_426 = 4;
            work3->field_41C = 0x10;
            work3->field_418 = 4;
            work3->field_414 = 1;
            work->field_422  = work->field_422 + 1;
        }
    }
}

/// Same body as `func_actor_342400_8016B5B0`, instruction for instruction.
///
/// Resets the actor's slide state, then re-derives `field_70` as the view-space
/// position of slot 4 entry 0's `coords[3]`: zeroes it and walks up the `sub`
/// chain from that joint towards `&Gfx_ViewCoord`, transforming the point
/// through each coord's rotation and translation with the GTE and writing the
/// result into `field_70` on arrival. Snapshots the root coord's translation
/// into `field_90`..`field_94`, clears `field_412` and the `field_428` /
/// `field_42A` slide accumulators, plays the encounter sound 0x402C0002 and
/// counts the sub-state up.
void Actor04400_Fn0847C(Task* arg0)
{
    Actor104400Work* work;
    GsCOORDINATE2*   coords;
    GsCOORDINATE2*   current;
    SVECTOR*         pos;
    SVECTOR          local;
    VECTOR           result;
    s32              flag;

    work   = (Actor104400Work*)arg0->work;
    coords = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    SndEvt_EnqueueType7(0x402C0002, 1);
    work->field_90  = coords->coord.t[0];
    work->field_92  = coords->coord.t[1];
    work->field_94  = coords->coord.t[2];
    work->field_412 = 0;
    work->field_428 = 0;
    work->field_42A = 0;
    work->field_422++;
    pos     = &work->field_70;
    pos->vx = pos->vy = pos->vz = 0;
    current                     = &((GsCOORDINATE2*)((TmdObject*)((Task*)Gp_LookupSlot4(0))->extra)->field_8)[3];
    local.vx                    = pos->vx;
    local.vy                    = pos->vy;
    local.vz                    = pos->vz;
    while (1) {
        if (current->sub == NULL) {
            return;
        }
        if (current == &Gfx_ViewCoord) {
            pos->vx = local.vx;
            pos->vy = local.vy;
            pos->vz = local.vz;
            return;
        }
        gte_SetTransMatrix(&current->coord);
        gte_SetRotMatrix(&current->coord);
        gte_ldv0(&local);
        __asm__ volatile("nop; nop; .word 0x4A480012");
        gte_stlvnl(&result);
        gte_stflg(&flag);
        local.vx = result.vx;
        local.vy = result.vy;
        local.vz = result.vz;
        current  = current->sub;
    }
}

/// Same body as `func_actor_342400_8016B744`, up to the animation ids it
/// requests. When `Actor04400_D10814[field_418 - 1]` is 0, requests animation
/// 9 with `field_426` 4 and plays the encounter sound 0x402C0002 at the
/// enemy's pan and depth, then sets the sub-state to 4. Otherwise requests
/// animation 7 with `field_426` 8, folds `field_41C * 4` onto `field_44F` and
/// counts the sub-state up.
void Actor04400_Fn08610(Task* arg0)
{
    Actor104400Work* work;
    s32              soundId;
    s32              pan;

    work = (Actor104400Work*)arg0->work;
    if (Actor04400_D10814[work->field_418 - 1] == 0) {
        work->field_426 = 4;
        work->field_41C = 0x10;
        work->field_418 = 9;
        work->field_414 = 1;
        soundId         = ((((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC) << 8) | 0x402C0002;
        pan             = (s8)Gp_GetObjPan((GpObj38*)((TmdObject*)arg0->extra)->field_8);
        SndEvt_EnqueueType6(soundId, pan, (s8)Gp_GetObjDepth((GpObj38*)((TmdObject*)arg0->extra)->field_8));
        work->field_422 = 4;
        return;
    }
    work->field_426 = 8;
    work->field_41C = 0x10;
    work->field_418 = 7;
    work->field_414 = 1;
    work->field_44F = (u8)work->field_41C * 4;
    work->field_422++;
}

void Actor04400_Fn08718(Task* arg0)
{
    Actor104400Work* work2;
    Actor104400Work* work;
    GpEnemy*         enemy;
    TmdObject*       model;

    work            = (Actor104400Work*)arg0->work;
    enemy           = (GpEnemy*)arg0->spawnArg2;
    model           = (TmdObject*)arg0->extra;
    work->field_412 = 0;
    SndEvt_EnqueueType7(0x402C0002, 1);
    if ((Gp_StateF0.field_1F & 0xF) == (((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC)) {
        Gp_StateF0.field_1F = 0;
    }
    Gp_UnlinkNode(&enemy->node);
    enemy->field_54 = 0;
    work2           = (Actor104400Work*)arg0->work;
    Gp_UnlinkObj(&work2->obj_2AC);
    Gp_UnlinkObj(&work2->obj_2CC);
    Gp_UnlinkObj(&work2->obj_3AC);
    model->field_C  = model->field_C | 0x80;
    work->field_420 = work->field_420 + 1;
}

void Actor04400_Fn087E0(Task* arg0)
{
    Actor104400Work* work;
    TmdObject*       model;
    u16              ticks;

    work            = (Actor104400Work*)arg0->work;
    model           = (TmdObject*)arg0->extra;
    ticks           = work->field_412 + 1;
    work->field_412 = ticks;
    if ((s16)ticks == 3) {
        Tmd_FreeBuffers(model);
        model->field_C |= 4;
    }
    if ((s16)work->field_412 >= 0x24) {
        Gp_DestroyEnemy(arg0->spawnArg2, arg0);
    }
}

/// Same body as `func_actor_341700_8016A6C0`.
void Actor04400_Fn08870(Task* arg0)
{
    Actor104400Work* work;
    GpEnemy*         enemy;

    enemy = (GpEnemy*)arg0->spawnArg2;
    work  = (Actor104400Work*)arg0->work;
    SndEvt_EnqueueType7(((enemy->field_8 >> 0xC) << 8) | 0x402C0002, 0xF);
    if ((Gp_StateF0.field_1F & 0xF) == (((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC)) {
        Gp_StateF0.field_1F = 0;
    }
    Gp_UnlinkNode(&enemy->node);
    work->field_420 = work->field_420 + 1;
}

/// Same body as `func_actor_342400_8016BA3C`.
void Actor04400_Fn08908(Task* arg0)
{
    Actor104400Work* work;
    s16              anim;
    s16              next;

    work = (Actor104400Work*)arg0->work;
    anim = work->field_418;
    if (anim == 8) {
        if (work->field_440 == 0) {
            work->field_426 = 4;
            work->field_41C = 0x10;
            work->field_418 = 5;
            work->field_414 = 1;
        } else {
            work->field_426 = 4;
            work->field_41C = 0x10;
            work->field_418 = 6;
            work->field_414 = 1;
        }
    } else {
        next            = Actor04400_D10828[anim - 1];
        work->field_426 = 4;
        work->field_41C = 0x10;
        work->field_418 = next;
        work->field_414 = 1;
    }
    Actor04400_Fn02B8C();
    work->field_420++;
}

void Actor04400_Fn089C0(Task* arg0)
{
    Actor104400Work* work;
    Actor104400Work* work2;
    s32              cond;

    work = (Actor104400Work*)arg0->work;
    Actor04400_Fn02B8C();
    work2 = (Actor104400Work*)arg0->work;
    if ((work2->flags_EC.half & 1) || (work2->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_420 = work->field_420 + 1;
    }
}

void Actor04400_Fn08A40(Task* arg0)
{
    Actor104400Work* work2;
    Actor104400Work* work;

    work                                  = (Actor104400Work*)arg0->work;
    ((GpEnemy*)arg0->spawnArg2)->field_54 = 0;
    work2                                 = (Actor104400Work*)arg0->work;
    Gp_UnlinkObj(&work2->obj_2AC);
    Gp_UnlinkObj(&work2->obj_2CC);
    Gp_UnlinkObj(&work2->obj_3AC);
    work->field_412 = 0;
    work->field_420 = work->field_420 + 1;
}

void Actor04400_Fn08A9C(void)
{
}

/// Same body as `Actor04400_Fn08870`.
void Actor04400_Fn08AA4(Task* arg0)
{
    Actor104400Work* work;
    GpEnemy*         enemy;

    enemy = (GpEnemy*)arg0->spawnArg2;
    work  = (Actor104400Work*)arg0->work;
    SndEvt_EnqueueType7(((enemy->field_8 >> 0xC) << 8) | 0x402C0002, 0xF);
    if ((Gp_StateF0.field_1F & 0xF) == (((GpEnemy*)arg0->spawnArg2)->field_8 >> 0xC)) {
        Gp_StateF0.field_1F = 0;
    }
    Gp_UnlinkNode(&enemy->node);
    work->field_420 = work->field_420 + 1;
}

/// Same body as `ActorsShared8016a98c`, which serves `actor_341700` and
/// `actor_342400`; this unit serves `actor_104400` and `actor_342200`.
///
/// `work` is declared before `coord` on purpose: sched1 promotes all four
/// loads to `LAUNCH_PRIORITY` and breaks that tie by descending `INSN_LUID`,
/// so the order the initialisers are emitted in is the order the loads land.
void Actor04400_Fn08B3C(Task* arg0)
{
    GpEnemy*         enemy = (GpEnemy*)arg0->spawnArg2;
    Actor104400Work* work  = (Actor104400Work*)arg0->work;
    GsCOORDINATE2*   coord = ((TmdObject*)arg0->extra)->field_8;
    Actor104400Work* objWork;

    enemy->field_54 = 0;

    objWork = (Actor104400Work*)arg0->work;
    Gp_UnlinkObj(&objWork->obj_2AC);
    Gp_UnlinkObj(&objWork->obj_2CC);
    Gp_UnlinkObj(&objWork->obj_3AC);

    work->field_430 = 0x1000;
    work->matrix_0  = coord->coord;

    Gp_SetLightMode((GpObj4C*)arg0->spawnArg2, 1);

    work->field_412 = 0;
    work->field_420++;
}

/// Waits 0x18 frames on `field_412`, then hides the model by setting bit 1 of
/// `TmdObject.field_C`. Body is identical to `Actor04400_Fn0781C`'s.
void Actor04400_Fn08C08(Task* arg0)
{
    Actor104400Work* work;
    TmdObject*       model;
    u16              ticks;

    work            = (Actor104400Work*)arg0->work;
    model           = (TmdObject*)arg0->extra;
    ticks           = work->field_412 + 1;
    work->field_412 = ticks;
    if ((s16)ticks >= 0x18) {
        model->field_C |= 2;
        work->field_412 = 0;
        work->field_451 = 1;
        work->field_420 = work->field_420 + 1;
    }
}

/// Squashes the model vertically by the shrinking `field_430`: the root
/// coordinate takes `matrix_0` scaled by (1, field_430, 1) through a local
/// identity rotation. Frame 0x10 rotates the light mode to 2; from frame 0x21
/// the model is flagged 0x80 and the state advances.
///
/// Same body as `ActorsShared8016bd98`, which does this for the `actor_341700`
/// and `actor_342400` work blocks.
void Actor04400_Fn08C64(Task* arg0)
{
    Actor104400Work*     work;
    TmdObject*           obj;
    GsCOORDINATE2*       coord;
    VECTOR               scale;
    Actor104400Mat       m;
    Actor104400MatWords* ident;

    work             = (Actor104400Work*)arg0->work;
    ident            = &m.ident;
    obj              = (TmdObject*)arg0->extra;
    coord            = obj->field_8;
    work->field_430 -= 0x40;
    scale.vx         = 0x1000;
    scale.vy         = (s16)work->field_430;
    scale.vz         = 0x1000;
    coord->coord     = work->matrix_0;
    m.ident.m00_m01  = 0x1000;
    m.ident.m02_m10  = 0;
    ident->m11_m12   = 0x1000;
    m.ident.m20_m21  = 0;
    ident->m22       = 0x1000;
    ScaleMatrix(&m.mat, &scale);
    MulMatrix(&coord->coord, &m.mat);
    if ((s16)++work->field_412 == 0x10) {
        Gp_SetLightMode(arg0->spawnArg2, 2);
    }
    if ((s16)work->field_412 >= 0x21) {
        obj->field_C    = obj->field_C | 0x80;
        work->field_412 = 0;
        work->field_420 = work->field_420 + 1;
    }
}

void Actor04400_Fn08DA4(Task* arg0)
{
    Actor104400Work* work;

    work            = (Actor104400Work*)arg0->work;
    arg0->state     = 5;
    work->field_420 = 0;
    work->field_422 = 0;
}

s32 Actor04400_Fn08DBC(Task* arg0)
{
    Actor104400Work* work = (Actor104400Work*)arg0->work;

    if (work->field_41E == 1) {
        switch (work->field_448) {
            case 3:
                work->field_420 = 8;
                work->field_422 = 0;
                break;
            case 5:
                work->field_420 = 9;
                work->field_422 = 0;
                break;
        }
        work->field_448 = 0;
        return 1;
    }
    return 0;
}
