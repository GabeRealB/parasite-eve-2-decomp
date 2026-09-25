#include "common.h"

#include <psyq/abs.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "actors/actor.h"
#include "actors/actors_shared_8013231c.h"

#include "main/gameflag.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3E9C.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

/// Work block of the overlay's walker, allocated zeroed by its spawn routine
/// and kept at `Task::work`: a twenty-part rig and the model state, the
/// display node `obj` the spawn routine links with the contact record that
/// follows it, and the walk. The exit callback hands `obj` back to
/// `Gp_UnlinkObj`. `field_53E` latches the `GameFlag_GetNibble(0xED)` result
/// the flag check uses, so the setup it triggers runs only on the edge where
/// the flag turns positive and the latch is still clear. `field_534` is the
/// task that setup spawns, and `field_53C` the mode byte the 0x7DB handler
/// writes and the per-frame body switches on. `field_53D` is the frames until
/// the model buffers are freed, -1 disabling the countdown, which the
/// visibility handler re-arms to 2 in its hide-and-free mode.
typedef struct Actor113100Work {
    ActorAnimRig20  rig;
    ActorModelState model;
    GpObj           obj;
    GpRec18         field_4D8;
    ActorWalkState  walk;
    Task*           field_534;
    s16             field_538;
    s16             field_53A;
    u8              field_53C;
    s8              field_53D;
    s8              field_53E;
    byte            pad_53F[1];
} Actor113100Work;
STATIC_ASSERT_SIZEOF(Actor113100Work, 0x540);

/// Optional start animation for the 0x7DD handler: the preset's `field_4` and
/// the `model.nextAnimId` id byte. Absent, the defaults are anim 2 and id 1.
typedef struct Actor113100SpawnAnim {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ u8  field_4;
} Actor113100SpawnAnim;

/// Child task table the setup handler `func_actor_113100_80131E58` spawns
/// from, four `TaskDesc` entries. Index 1 is spawned only when
/// `gGameSession->at4.loc.place == 2` and its task lands in
/// `Actor113100Work::field_534`; indices 2 and 3 are the two modelled parts the
/// handler re-dresses from the area record.
extern TaskDesc D_actor_113100_80144308;

/// The actor's message table, stored in `Task::msgTable`: 0x7D3
/// (`func_actor_113100_801331E8`), 0x7D4 (`func_actor_113100_8013333C`), 0x7D5
/// (`func_actor_113100_80132790`), 0x7DD (`func_actor_113100_801328EC`) and
/// 0x7DB (`func_actor_113100_801333B8`), terminated by 0x7FFFFFFF.
extern GpMsgEntry D_actor_113100_80144338[];

/// Animation bank table the 0x7D3 handler `func_actor_113100_801331E8` indexes
/// by the animation id it has latched into `Actor113100Work::model.bank`; the
/// entry is the `void*` its `func_800B3F84` call passes on.
extern void* D_actor_113100_801442E0[];

/// Per-animation byte the same handler copies into
/// `Actor113100Work::field_53C` from `GpAnimArg::field_4`.
extern u8 D_actor_113100_801442E4[];

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Main-executable routine the turn handler `func_actor_113100_801324DC` calls
/// with a yaw angle and the root coordinate's matrix, after resetting its 3x3
/// to the identity.
void func_8004BFF8(s16 angle, MATRIX* matrix);

/// Gameplay import, called with 1 by the setup handler and with 0 by
/// `func_actor_113100_80132F40`.
void func_80183BAC(s32 arg0);

void func_actor_113100_80131E58(Task* task);
void func_actor_113100_80132104(Task* task);
void func_actor_113100_801324DC(Task* task);
void func_actor_113100_8013264C(Task* task);
s32  func_actor_113100_80132790(Task* task, s32 msgId, s32 mode, s32 arg3);
void func_actor_113100_80132B30(Task* task);
void func_actor_113100_80132BDC(Task* task);
void func_actor_113100_80132CF4(Task* task);
void func_actor_113100_80132E00(Task* task);
void func_actor_113100_80132EF0(Task* task);
void func_actor_113100_80132F24(Task* task);
void func_actor_113100_80132F40(Task* task);
void func_actor_113100_80132FB4(Task* task);
void func_actor_113100_8013301C(Task* task);
void func_actor_113100_801330E8(Task* task);
s32  func_actor_113100_801331E8(Task* task, s32 msgId, GpAnimArg* preset, s32 arg3);

/// States of a child task posed on one of the parent's parts: attach to the
/// parent, rebuild its display matrix every frame, then `taskKill`. Dispatched
/// by `func_actor_113100_80132AD8`.
const TaskFuncTable3 D_actor_113100_80131E24 = { {
    func_actor_113100_80132B30,
    func_actor_113100_80132BDC,
    taskKill,
} };

/// States of the child task that follows the parent's model flags: attach to
/// the parent's part, mirror its flags every frame, then `taskKill`.
/// Dispatched by `func_actor_113100_80132C9C`.
const TaskFuncTable3 D_actor_113100_80131E30 = { {
    func_actor_113100_80132CF4,
    func_actor_113100_80132E00,
    taskKill,
} };

/// The actor's own three states - setup, per-frame tick and exit -
/// dispatched by `func_actor_113100_80132E98`.
const TaskFuncTable3 D_actor_113100_80131E3C = { {
    func_actor_113100_80131E58,
    func_actor_113100_80132104,
    func_actor_113100_80132EF0,
} };

/// The four main-body handlers, dispatched by `func_actor_113100_80132FB4`
/// through `Actor113100Work::walk.motionStep`.
const TaskFuncTable4 D_actor_113100_80131E48 = { {
    func_actor_113100_8013301C,
    func_actor_113100_801324DC,
    func_actor_113100_8013264C,
    func_actor_113100_801330E8,
} };

/// Setup handler (state 0): allocates the 0x540-byte work block, clears the
/// three "no id yet" sentinels and spawns the actor's children from
/// `D_actor_113100_80144308` -- index 1 only in arena mode
/// (`gGameSession->at4.loc.place == 2`), then indices 2 and 3, whose models get the
/// texture page and CLUT of the area record the actor's own location key
/// resolves to. It then builds the work block's display node: `field_C` points
/// at the `GpRec18` table that follows it, the position triple is zeroed, the
/// node is linked and its flags raised to 0x8000 with `field_1C` set to 0x100,
/// and `field_8` is attached to model part 1. Finally it publishes the message
/// table, installs the exit callback and steps to the next state.
void func_actor_113100_80131E58(Task* task)
{
    Actor113100Work* work;
    Task*            child2;
    Task*            child3;
    GpAreaKey        key;
    GpAreaKey*       sessionKey2;
    GpAreaKey*       sessionKey3;
    TmdObject*       model2;
    TmdObject*       model3;
    GpAreaPlace*     entry2;
    GpAreaPlace*     entry3;
    GpObj*           obj;
    u8               areaByte0;
    u32              raw2;
    u32              raw3;
    u32              index2;
    u32              index3;

    work = memCalloc(0x540, 0);
    if (work == NULL) {
        Gp_EnemyTaskExit(task);
        return;
    }
    task->work          = work;
    work->model.animId  = -1;
    work->model.bank    = -1;
    work->field_53D     = -1;
    work->walk.acc[0].w = 0;
    work->walk.acc[1].w = 0;
    work->walk.acc[2].w = 0;
    if (gGameSession->at4.loc.place == 2) {
        work->field_534 = Task_SpawnFromTable(&D_actor_113100_80144308, 1, 8, (s32)task);
    }

    child2 = Task_SpawnFromTable(&D_actor_113100_80144308, 2, 4, (s32)task);
    if (child2 != NULL) {
        sessionKey2 = (GpAreaKey*)&gGameSession->at4.loc;
        raw2        = ((GpEnemy*)task->spawnArg2)->placeKey;
        model2      = (TmdObject*)child2->extra;
        key.stage   = sessionKey2->stage;
        key.area    = sessionKey2->area;
        key.room    = sessionKey2->room;
        areaByte0   = gGameSession->at4.loc.view;
        index2      = raw2 >> 12;
        key.view    = areaByte0;
        Gp_SyncAreaKeyIndex(&key);
        entry2        = (GpAreaPlace*)((index2 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
        model2->tpage = entry2->tpage;
        model2->clut  = entry2->clut;
        if (model2->buffer != NULL) {
            tmdProcessStream(model2);
            tmdProcessStream(model2);
        }
    }

    child3 = Task_SpawnFromTable(&D_actor_113100_80144308, 3, 2, (s32)task);
    if (child3 != NULL) {
        sessionKey3 = (GpAreaKey*)&gGameSession->at4.loc;
        raw3        = ((GpEnemy*)task->spawnArg2)->placeKey;
        model3      = (TmdObject*)child3->extra;
        key.stage   = sessionKey3->stage;
        key.area    = sessionKey3->area;
        key.room    = sessionKey3->room;
        areaByte0   = gGameSession->at4.loc.view;
        index3      = raw3 >> 12;
        key.view    = areaByte0;
        Gp_SyncAreaKeyIndex(&key);
        entry3        = (GpAreaPlace*)((index3 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
        model3->tpage = entry3->tpage;
        model3->clut  = entry3->clut;
        if (model3->buffer != NULL) {
            tmdProcessStream(model3);
            tmdProcessStream(model3);
        }
    }

    func_actor_113100_80132F24(task);

    obj           = &work->obj;
    obj->coord    = &((TmdObject*)task->extra)->coords[1];
    obj->ctx.recs = &work->field_4D8;
    obj->key      = 0x30000;
    obj->radius   = 0x100;
    obj->pos.vx   = 0;
    obj->pos.vy   = 0;
    obj->pos.vz   = 0;
    obj->flags    = 1;
    Gp_LinkObj(2, obj);
    obj->flags |= 0x8000;
    Gp_InitRec18Table(obj->ctx.recs, 1, 0);

    task->msgTable = &D_actor_113100_80144338;
    func_80183BAC(1);
    func_actor_113100_80132790(task, 0, 0, 0);
    task->exitCallback = func_actor_113100_80132EF0;
    task->state       += 1;
}

/// Per-frame tick of the actor's live state. While the model is not deferred
/// (bit 0x80 of `TmdObject::flags`) it rebuilds part 1's world matrix and
/// draws the ground shadow under that part. `Gp_StateF0.field_4` gates the rest: a
/// nonzero value skips it. The live path dispatches `func_actor_113100_80132F40`
/// or `func_actor_113100_80132FB4` from a two-entry stack table indexed by
/// `walk.motion`, integrates the 16.16 step at `walk.step` into `walk.acc[0].w` /
/// `walk.acc[1].w` / `walk.acc[2].w` and the root translation, ticks slots 1..0x13
/// once `model.ticking` has latched, and plays ids 0x5113000F / 0x51130013 /
/// 0x51130010 from the slot-1 cue flags. While the model is visible it clears
/// the occupancy table, ramps `field_538` toward 0 or 0x1000 according to
/// `field_53C`, and turns the head toward slot 3. `viewReady` rebuilds part 1's
/// lighting, and `field_53D` counts the buffer free down to zero.
void func_actor_113100_80132104(Task* task)
{
    TmdObject*       extra    = (TmdObject*)task->extra;
    Actor113100Work* work     = (Actor113100Work*)task->work;
    TaskFunc         funcs[2] = { func_actor_113100_80132F40, func_actor_113100_80132FB4 };
    VECTOR3          pos;
    GsCOORDINATE2*   coord;
    GpAnimRec*       rec;
    s32              i;
    s32              snd;
    s8               mode;
    u16              rate;

    if (!(extra->flags & 0x80)) {
        ((TmdObject*)task->extra)->coords[1].flg = 0;
        Gp_UpdateCoord(&((TmdObject*)task->extra)->coords[1]);
        if (func_800EA1A8((VECTOR3*)((TmdObject*)task->extra)->coords[1].workm.t, &pos) != 0) {
            Gp_DrawEffGroundQuad(&pos, 0x200, Gp_State1C->groundShade);
        }
    }
    if (Gp_StateF0.field_4 == 0) {
        funcs[work->walk.motion](task);
        coord                = ((TmdObject*)task->extra)->coords;
        work->walk.acc[0].w += work->walk.step.vx;
        work->walk.acc[1].w += work->walk.step.vy;
        work->walk.acc[2].w += work->walk.step.vz;
        coord->coord.t[0]   += (s16)(work->walk.acc[0].w >> 16);
        coord->coord.t[1]   += (s16)(work->walk.acc[1].w >> 16);
        coord->coord.t[2]   += (s16)(work->walk.acc[2].w >> 16);
        coord->flg           = 0;
        work->walk.acc[0].w  = (u16)work->walk.acc[0].w;
        work->walk.acc[1].w  = (u16)work->walk.acc[1].w;
        work->walk.acc[2].w  = (u16)work->walk.acc[2].w;
        if (work->model.ticking != 0) {
            for (i = 1; i < 0x14; i++) {
                Gp_AnimTickIndex(&work->rig.anim, i);
            }
            rec = Gp_AnimGetRec(&work->rig.anim, &work->rig.slots[1]);
            if (rec != NULL) {
                if (rec->flags & 0x20) {
                    snd = 0x5113000F;
                    if (gGameSession->at4.loc.view == 0x10) {
                        snd = 0x51130013;
                    }
                    SndEvt_EnqueueType6(snd, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                }
                if ((rec->flags & 0x10) && (gGameSession->at4.loc.view != 0x10)) {
                    SndEvt_EnqueueType6(0x51130010, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                }
            }
        }
        if (!(extra->flags & 0x80)) {
            Gp_ClearRec18Occupied(&work->field_4D8);
            mode = work->field_53C;
            switch (mode) {
                case 0:
                    rate            = work->field_538 - 0x100;
                    work->field_538 = rate;
                    if ((s16)rate < 0) {
                        work->field_538 = 0;
                    }
                    break;
                case 1:
                    rate            = work->field_538 + 0x100;
                    work->field_538 = rate;
                    if ((s16)rate >= 0x1001) {
                        work->field_538 = 0x1000;
                    }
                    break;
            }
            func_800B0928(task, gameGetPtrSlot(3), 0x200, 0x100, (s16)work->field_538);
        }
        if (gGameSession->viewReady != 0) {
            ((TmdObject*)task->extra)->coords[1].flg = 0;
            Gp_UpdateCoord(&((TmdObject*)task->extra)->coords[1]);
            func_800D7A9C(extra, (VECTOR*)((TmdObject*)task->extra)->coords[1].workm.t, 0, 3);
        }
        if (work->field_53D >= 0) {
            if (work->field_53D == 0) {
                Tmd_FreeBuffers(extra);
            }
            work->field_53D--;
        }
    }
}

/// Per-frame turn handler, one of the four bodies `func_actor_113100_80132FB4`
/// dispatches through `D_actor_113100_80131E48`. It recovers the root
/// coordinate's yaw from its 3x3 (`m[0][2]` over `m[2][2]`) and compares it
/// with the heading the work block latched in `field_53A`: more than 0x41 away
/// it steps `field_53A` 0x40 toward the model and only re-splats the identity
/// 3x3, within 0x41 it turns the root coordinate to `field_53A` and then
/// rotates the local forward offset (0, 0, 0x200000) into `walk.step` with
/// `ApplyMatrixLV`, raises the three halves at `walk.limit` to 0x7FFF and
/// publishes preset 0x7D3. Both arms clear `GsCOORDINATE2::flg` -- the node's
/// recompute bit -- and end at the same epilogue.
///
/// `yaw` carries two different values on purpose: it holds the work block's
/// `field_53A` for the comparison, and the snapped heading on the turn arm.
/// One variable for both is what puts the snapped value in `$a0` -- the
/// pseudo then spans the whole body, so `$v0` (written by both `ratan2` and
/// the identity constant) is denied it and the `(s16)angle` temporary takes
/// `$v0` instead. `words` and `turnWords` are likewise two pointers rather
/// than one: a single `words` would make the turn arm and the normal arm share
/// a pseudo, which lengthens its life across the branch and adds a copy.
void func_actor_113100_801324DC(Task* task)
{
    Actor113100Work* work;
    GsCOORDINATE2*   coord;
    GpMtxWords*      words;
    GpMtxWords*      turnWords;
    VECTOR           delta;
    GpAnimArg        preset;
    s32              angle;
    s32              angle16;
    u16              yaw;
    s16              diff;

    coord = ((TmdObject*)task->extra)->coords;
    work  = (Actor113100Work*)task->work;
    angle = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]);
    yaw   = (u16)work->field_53A;
    diff  = yaw - angle;
    if (ABS(diff) >= 0x41) {
        angle16 = (s16)angle;
        if (diff < 0) {
            yaw = angle16 - 0x40;
        } else {
            yaw = angle16 + 0x40;
        }
        turnWords          = (GpMtxWords*)&coord->coord;
        turnWords->m00_m01 = ONE;
        turnWords->m02_m10 = 0;
        turnWords->m11_m12 = ONE;
        turnWords->m20_m21 = 0;
        turnWords->m22     = ONE;
        func_8004BFF8((s16)yaw, &coord->coord);
        coord->flg = 0;
    } else {
        words          = (GpMtxWords*)&coord->coord;
        words->m00_m01 = ONE;
        words->m02_m10 = 0;
        words->m11_m12 = ONE;
        words->m20_m21 = 0;
        words->m22     = ONE;
        func_8004BFF8((s16)yaw, &coord->coord);
        delta.vx = 0;
        delta.vy = 0;
        delta.vz = 0x200000;
        ApplyMatrixLV(&coord->coord, &delta, (VECTOR*)&work->walk.step);
        work->walk.limit.vx    = 0x7FFF;
        work->walk.limit.vy    = 0x7FFF;
        work->walk.limit.vz    = 0x7FFF;
        preset.animBlock.index = 0;
        preset.field_4         = 2;
        preset.field_8         = 1;
        preset.field_C         = 4;
        preset.field_10        = 0;
        func_actor_113100_801331E8(task, 0x7D3, &preset, 0);
        work->walk.motionStep++;
        coord->flg = 0;
    }
}

/// One of the four main-body handlers `Actor113100Work::walk.motionStep` dispatches
/// through `D_actor_113100_80131E48`. It measures how far the work block's
/// `walk.target.vx` / `walk.target.vz` have drifted from the root coordinate's
/// translation -- each axis as the 16-bit magnitude of the difference, the
/// signed 32-bit subtraction only picking the direction -- and once both
/// magnitudes reach the thresholds `walk.limit.vx` / `.vz` it publishes the
/// 0x7D3 preset (`field_4` the animation id, `field_C` 5) and clears the
/// `walk.step` vector, bumping `walk.motionStep` on to the next handler. Below the
/// thresholds it latches the magnitudes back into `walk.limit`, so the pair
/// tracks the last distance that was too small.
void func_actor_113100_8013264C(Task* task)
{
    Actor113100Work* work;
    GsCOORDINATE2*   coord;
    SVECTOR          d;
    s32              dx;
    s32              dz;
    GpAnimArg        preset;

    work  = (Actor113100Work*)task->work;
    coord = ((TmdObject*)task->extra)->coords;
    if (work->walk.target.vx - coord->coord.t[0] >= 0) {
        dx = (u16)work->walk.target.vx - (u16)coord->coord.t[0];
    } else {
        dx = (u16)coord->coord.t[0] - (u16)work->walk.target.vx;
    }
    d.vx = dx;
    if (work->walk.target.vz - coord->coord.t[2] >= 0) {
        dz = (u16)work->walk.target.vz - (u16)coord->coord.t[2];
    } else {
        dz = (u16)coord->coord.t[2] - (u16)work->walk.target.vz;
    }
    d.vz = dz;
    if (d.vx >= work->walk.limit.vx && d.vz >= work->walk.limit.vz) {
        preset.animBlock.index = 0;
        preset.field_4         = work->model.nextAnimId;
        preset.field_8         = 1;
        preset.field_C         = 5;
        preset.field_10        = 0;
        func_actor_113100_801331E8(task, 0x7D3, &preset, 0);
        work->walk.step.vx = 0;
        work->walk.step.vy = 0;
        work->walk.step.vz = 0;
        work->walk.motionStep++;
        return;
    }
    work->walk.limit.vx = d.vx < 0 ? -d.vx : d.vx;
    work->walk.limit.vz = d.vz < 0 ? -d.vz : d.vz;
}

/// The 0x7D5 entry of `D_actor_113100_80144338`: the visibility control the
/// setup handler and `func_actor_113100_80132F40` drive. This one takes its
/// payload as a mode word rather than a pointer -- both call sites pass a
/// literal, and a mode outside 0..3 is answered with 1, the "not handled"
/// return the dispatch expects. All four modes lift the 0x8000 bit the setup
/// handler raised on the work block's display node and then rewrite the
/// actor's own `TmdObject::flags`, whose bit 0x80 is `taskKill`'s type-1
/// deferred kill and whose 0x4 is the flag `Tmd_Create` seeds from `flags & 1`:
/// mode 0 shows the model and clears 0x4; mode 1 hides it, hands the object to
/// `Tmd_AllocBuffers` and clears 0x4; mode 2 hides it, latches 2 into
/// `field_53D` -- the countdown `func_actor_113100_80132104` walks down to
/// `Tmd_FreeBuffers` -- and raises 0x4; mode 3 shows it and raises 0x4.
///
/// `work` and `work2` are the same `Task::work` read twice. The second read
/// becomes a register copy at the entry, which is what leaves the block in
/// `$v1` for the node base mode 3 folds out of `work` while the hoisted `head`
/// and the `field_53D` latch run off the copy in `$a1`; one read and one local
/// for the node instead collapses all four arms onto a single register
/// (98.517%).
s32 func_actor_113100_80132790(Task* task, s32 msgId, s32 mode, s32 arg3)
{
    Actor113100Work* work;
    Actor113100Work* work2;
    GpObj*           head;
    GpObj*           node;
    TmdObject*       obj;
    s32              i;
    s32              ret;

    work  = (Actor113100Work*)task->work;
    obj   = task->extra;
    work2 = (Actor113100Work*)task->work;
    head  = &work2->obj;
    ret   = 0;

    switch (mode) {
        case 0:
            obj->flags |= 0x80;
            node        = head;
            for (i = 0; i <= 0; i++) {
                node->flags &= 0x7FFF;
                node++;
            }
            obj->flags &= ~4;
            break;
        case 1:
            obj->flags &= ~0x80;
            node        = head;
            for (i = 0; i <= 0; i++) {
                node->flags &= 0x7FFF;
                node++;
            }
            Tmd_AllocBuffers(obj);
            obj->flags &= ~4;
            break;
        case 2:
            obj->flags |= 0x80;
            node        = head;
            for (i = 0; i <= 0; i++) {
                node->flags &= 0x7FFF;
                node++;
            }
            work2->field_53D = 2;
            obj->flags      |= 4;
            break;
        case 3:
            obj->flags &= ~0x80;
            node        = &work->obj;
            for (i = 0; i <= 0; i++) {
                node->flags &= 0x7FFF;
                node++;
            }
            obj->flags |= 4;
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}

/// The 0x7DD entry of `D_actor_113100_80144338`: the placement command. It
/// latches its payload's position and rotation into the work block, flags the
/// actor as placed through `walk.motion` / `walk.motionStep`, then applies the start
/// preset in place -- the body of the 0x7D3 handler
/// `func_actor_113100_801331E8` written out inline against a preset built on
/// this function's own stack, `anim` picking the preset's `field_4`.
s32 func_actor_113100_801328EC(Task* task, s32 msgId, GpXformArg* place, Actor113100SpawnAnim* anim)
{
    Actor113100Work* work;
    Actor113100Work* w;
    GpAnimArg        preset;
    GpAnimArg*       msg;
    s32              i;
    TmdObject*       ext;

    w                      = (Actor113100Work*)task->work;
    w->walk.motion         = 1;
    w->walk.motionStep     = 0;
    w->walk.target.vx      = place->pos.vx;
    w->walk.target.vy      = place->pos.vy;
    w->walk.target.vz      = place->pos.vz;
    w->walk.rotX           = place->rot.vx;
    w->walk.rotY           = place->rot.vy;
    w->walk.rotZ           = place->rot.vz;
    preset.animBlock.index = 0;
    if (anim != NULL) {
        preset.field_4      = anim->field_0;
        w->model.nextAnimId = anim->field_4;
    } else {
        preset.field_4      = 2;
        w->model.nextAnimId = 1;
    }
    preset.field_8  = 1;
    preset.field_C  = 5;
    preset.field_10 = 1;

    msg  = &preset;
    work = (Actor113100Work*)task->work;
    ext  = task->extra;
    if (msg->animBlock.index != work->model.bank) {
        work->model.bank   = msg->animBlock.index;
        work->model.animId = -1;
        func_800B3F84(&work->rig.anim, D_actor_113100_801442E0[work->model.bank], ext, work->rig.poses,
                      work->rig.slots);
    }
    if (msg->field_4 != work->model.animId) {
        work->model.animId = msg->field_4;
        if (msg->field_8 != 0 && work->model.ticking != 0) {
            for (i = 1; i < 0x14; i++) {
                func_800B4114(&work->rig.anim, i, work->model.animId, 0, msg->field_C);
            }
        } else {
            for (i = 1; i < 0x14; i++) {
                Gp_AnimResetSlot(&work->rig.anim, i, work->model.animId);
            }
        }
        for (i = 1; i < 0x14; i++) {
            Gp_AnimTickIndex(&work->rig.anim, i);
        }
        work->model.ticking = 1;
    }
    work->field_53C = D_actor_113100_801442E4[msg->field_4];
    return 0;
}

void func_actor_113100_80132AD8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_113100_80131E24;
    sp.funcs[task->state](task);
}

void func_actor_113100_80132B30(Task* task)
{
    TmdObject*     model;
    Task*          parent;
    s32            index;
    GsCOORDINATE2* node;
    GsCOORDINATE2* part;

    model = task->extra;
    SOFT_BARRIER();
    parent = (Task*)task->spawnArg2;
    index  = task->spawnArg1;
    node   = model->coords;
    part   = ((TmdObject*)parent->extra)->coords;

    node->coord.t[1] = 0x64;
    node->coord.t[0] = 0;
    node->coord.t[2] = 0;
    node->flg        = 0;
    node->sub        = &part[index];

    Task_Reparent(parent, task);
    if (GameFlag_GetNibble(0xF1) == 0) {
        model->flags &= 0xFF7F;
    } else {
        model->flags |= 0x80;
    }
    task->state += 1;
}

/// Builds the display matrix of the modelled part this actor is posed on.
/// `spawnArg1` indexes the part in the model task's coordinate array: the part's
/// `workm` is transposed into the actor coordinate, the stage view is multiplied
/// in, and the part's X euler angle is applied, after which the coordinate's
/// update flag is cleared so the GTE sees the new matrix.
void func_actor_113100_80132BDC(Task* task)
{
    MATRIX         sp10;
    SVECTOR        sp30;
    MATRIX*        view;
    MATRIX*        coord;
    GsCOORDINATE2* part;
    GsCOORDINATE2* node;
    s32            index;

    index = task->spawnArg1;
    node  = (GsCOORDINATE2*)((TmdObject*)task->extra)->coords;
    part  = &((TmdObject*)((Task*)task->spawnArg2)->extra)->coords[index];
    view  = &Gp_GetStageView(&gGameSession->at4.loc)->mtx;
    coord = &node->coord;
    TransposeMatrix(&part->workm, coord);
    TransposeMatrix(view, &sp10);
    MulMatrix0(coord, &sp10, coord);
    Gp_ExtractEuler(&sp30, &part->coord);
    RotMatrixX(sp30.vy, coord);
    node->flg = 0;
}

void func_actor_113100_80132C9C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_113100_80131E30;
    sp.funcs[task->state](task);
}

/// Setup state of the child task whose state table is
/// `D_actor_113100_80131E30`: hides the child's model, then mirrors the parent's
/// (`spawnArg2`) model flag bits 0x80 and 0x4 as the tick state does. It draws
/// the model at order-table offset -2, hangs the child's root coordinate off the
/// parent's part `spawnArg1`, shares the parent's light and colour matrices,
/// reparents the task under the parent and steps to the next state.
void func_actor_113100_80132CF4(Task* task)
{
    Task*          parent;
    TmdObject*     obj;
    TmdObject*     parentObj;
    GsCOORDINATE2* coords;
    GsCOORDINATE2* root;

    parent      = task->spawnArg2;
    obj         = task->extra;
    parentObj   = parent->extra;
    coords      = parentObj->coords;
    obj->flags |= 0x80;
    root        = obj->coords;
    if (!(parentObj->flags & 0x80)) {
        obj->flags &= 0xFF7F;
    }
    if (!(parentObj->flags & 4)) {
        obj->flags &= 0xFFFB;
        Tmd_AllocBuffers(obj);
    } else {
        obj->flags |= 4;
    }
    obj->otOffset = -2;
    coords       += task->spawnArg1;
    root->flg     = 0;
    root->sub     = coords;
    obj->lightMtx = parentObj->lightMtx;
    obj->colorMtx = parentObj->colorMtx;
    Task_Reparent(parent, task);
    task->state++;
}

/// Tick state of the child task whose state table is
/// `D_actor_113100_80131E30`: copies the parent's (`spawnArg2`) model flag bits
/// 0x80 and 0x4 onto the child's own model. Bit 0x80 is the deferred-kill flag
/// that hides the model; when the parent's bit 0x4 is clear the child's is
/// cleared too and `Tmd_AllocBuffers` is called on the child's model.
void func_actor_113100_80132E00(Task* task)
{
    TmdObject* parentObject;
    TmdObject* object;

    parentObject = (TmdObject*)((Task*)task->spawnArg2)->extra;
    object       = (TmdObject*)task->extra;

    if (!(parentObject->flags & 0x80)) {
        object->flags &= 0xFF7F;
    } else {
        object->flags |= 0x80;
    }
    if (!(parentObject->flags & 4)) {
        object->flags &= 0xFFFB;
        Tmd_AllocBuffers(object);
        return;
    }
    object->flags |= 4;
}

void func_actor_113100_80132E98(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_113100_80131E3C;
    sp.funcs[task->state](task);
}

/// The task's exit callback: it unlinks the work block's display node and
/// destroys the task.
void func_actor_113100_80132EF0(Task* arg0)
{
    Gp_UnlinkObj(&((Actor113100Work*)arg0->work)->obj);
    Gp_EnemyTaskExit(arg0);
}

/// Points the model's light and colour matrices at the work block's own
/// `light` / `color` pair; the setup handler calls it once.
void func_actor_113100_80132F24(Task* task)
{
    TmdObject*       ext;
    Actor113100Work* work;

    ext           = task->extra;
    work          = (Actor113100Work*)task->work;
    ext->lightMtx = &work->model.light;
    ext->colorMtx = &work->model.color;
}

void func_actor_113100_80132F40(Task* arg0)
{
    Actor113100Work* work;
    s32              flag;

    work = (Actor113100Work*)arg0->work;
    flag = GameFlag_GetNibble(0xED);
    if (flag > 0 && work->field_53E == 0) {
        func_actor_113100_80132790(arg0, 0, 1, 0);
        func_80183BAC(0);
    }
    work->field_53E = flag;
}

/// Dispatches the actor's four main-body handlers by the animation slot index
/// `walk.motionStep` counts up in `func_actor_113100_8013301C`.
void func_actor_113100_80132FB4(Task* arg0)
{
    Actor113100Work* work;
    TaskFuncTable4   sp;

    work = (Actor113100Work*)arg0->work;
    sp   = D_actor_113100_80131E48;
    sp.funcs[(s16)work->walk.motionStep](arg0);
}

/// Builds the offset from the actor's own translation (work + 0x4F0) to the
/// root part's coordinate translation and stores its yaw into the work block,
/// then dispatches animation preset 0x7D3 through `func_actor_113100_801331E8`
/// and counts the frame. The preset is built on this function's stack: it
/// carries the slot index, the animation id and the two per-slot arguments.
///
/// `preset` is declared before `delta` / `dir` on purpose -- the stack slots
/// land at 0x10, 0x28 and 0x38 only in that order (GCC assigns the frame in
/// declaration order, and the 16-byte `VECTOR` is 8-byte aligned).
void func_actor_113100_8013301C(Task* arg0)
{
    Actor113100Work* work;
    GsCOORDINATE2*   coord;
    GpAnimArg        preset;
    VECTOR           delta;
    SVECTOR          dir;

    work  = (Actor113100Work*)arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;

    delta.vx = work->walk.target.vx - coord->coord.t[0];
    delta.vy = work->walk.target.vy - coord->coord.t[1];
    delta.vz = work->walk.target.vz - coord->coord.t[2];
    VectorNormalS(&delta, &dir);
    work->field_53A = ratan2(dir.vx, dir.vz);

    preset.animBlock.index = 0;
    preset.field_4         = 0x16;
    preset.field_8         = 1;
    preset.field_C         = 4;
    preset.field_10        = 0;
    func_actor_113100_801331E8(arg0, 0x7D3, &preset, 0);
    work->walk.motionStep++;
}

void func_actor_113100_801330E8(Task* arg0)
{
    Actor113100Work* work;
    GpMtxWords*      words;
    GsCOORDINATE2*   coord;
    SVECTOR          vec;
    GpAnimArg        preset;
    s32              vy;
    s16              diff;

    coord = ((TmdObject*)arg0->extra)->coords;
    work  = (Actor113100Work*)arg0->work;

    Gp_ExtractEuler(&vec, &coord->coord);
    diff = (u16)work->walk.rotY - (u16)vec.vy;
    if (ABS(diff) >= 0x41) {
        vy = vec.vy;
        if (diff < 0) {
            vec.vy = vy - 0x40;
        } else {
            vec.vy = vy + 0x40;
        }
    } else {
        vec.vy                 = work->walk.rotY;
        preset.animBlock.index = 0;
        preset.field_4         = work->model.nextAnimId;
        preset.field_8         = 1;
        preset.field_C         = 5;
        preset.field_10        = 0;
        func_actor_113100_801331E8(arg0, 0x7D3, &preset, 0);
        work->walk.motion     = 0;
        work->walk.motionStep = 0;
    }

    words          = (GpMtxWords*)&coord->coord;
    words->m00_m01 = ONE;
    words->m02_m10 = 0;
    words->m11_m12 = ONE;
    words->m20_m21 = 0;
    words->m22     = ONE;
    RotMatrix(&vec, &coord->coord);
    coord->flg = 0;
}

/// The 0x7D3 animation-preset handler: the preset `func_actor_113100_8013301C`
/// / `func_actor_113100_801330E8` / `func_actor_113100_801324DC` build on their
/// own stacks. `field_0` is the animation id and `field_4` the slot id, each
/// latched into its work-block field with the pair's -1 sentinel left behind
/// it. When the animation id changes the whole bank is re-seeded through
/// `func_800B3F84`,
/// and when the slot id changes every slot is started either through
/// `func_800B4114` -- the path `field_8` selects, and the only one that reads
/// `field_C` -- or cleared through `Gp_AnimResetSlot`; either way all of them
/// are advanced once by `Gp_AnimTickIndex` and the `model.ticking` latch is set.
/// The trailing store is the mode byte `func_actor_113100_801333B8` reads.
s32 func_actor_113100_801331E8(Task* task, s32 msgId, GpAnimArg* preset, s32 arg3)
{
    Actor113100Work* work;
    TmdObject*       ext;
    s32              i;

    work = (Actor113100Work*)task->work;
    ext  = task->extra;
    if (preset->animBlock.index != work->model.bank) {
        work->model.bank   = preset->animBlock.index;
        work->model.animId = -1;
        func_800B3F84(&work->rig.anim, D_actor_113100_801442E0[work->model.bank], ext, work->rig.poses,
                      work->rig.slots);
    }
    if (preset->field_4 != work->model.animId) {
        work->model.animId = preset->field_4;
        if (preset->field_8 != 0 && work->model.ticking != 0) {
            for (i = 1; i < 0x14; i++) {
                func_800B4114(&work->rig.anim, i, work->model.animId, 0, preset->field_C);
            }
        } else {
            for (i = 1; i < 0x14; i++) {
                Gp_AnimResetSlot(&work->rig.anim, i, work->model.animId);
            }
        }
        for (i = 1; i < 0x14; i++) {
            Gp_AnimTickIndex(&work->rig.anim, i);
        }
        work->model.ticking = 1;
    }
    work->field_53C = D_actor_113100_801442E4[preset->field_4];
    return 0;
}

/// The 0x7D4 entry of `D_actor_113100_80144338`: places the actor at `args`.
/// The translation goes straight into the root part's local matrix, the Euler
/// angles into the coordinate's `rot` slot, from which `RotMatrix` rebuilds
/// the rotation; clearing `flg` makes the world matrix be recomputed.
s32 func_actor_113100_8013333C(Task* task, s32 msgId, GpXformArg* args)
{
    GpCoordExt* coord;

    coord               = (GpCoordExt*)((TmdObject*)task->extra)->coords;
    coord->coord.t[0]   = args->pos.vx;
    coord->coord.t[1]   = args->pos.vy;
    coord->coord.t[2]   = args->pos.vz;
    coord->param.rot.vx = args->rot.vx;
    coord->param.rot.vy = args->rot.vy;
    coord->param.rot.vz = args->rot.vz;
    RotMatrix(&coord->param.rot, &coord->coord);
    coord->flg = 0;
    return 0;
}

/// Message 0x7DB handler, listed in `D_actor_113100_80144338` after the 0x7D3 /
/// 0x7D5 / 0x7DD ones. The payload halfword selects one of four actions: 0 and
/// 1 clear and raise bit 0x80 of the child task's `TmdObject::flags`, the
/// deferred-kill flag that decides whether the model is drawn; 2 and 3 set the
/// work block's `field_53C` mode byte to 1 and 0. Nothing reads the opcode
/// itself, hence `msgId`.
s32 func_actor_113100_801333B8(Task* task, s32 msgId, GpCmdArg* msg)
{
    Actor113100Work* work;
    TmdObject*       model;

    work = (Actor113100Work*)task->work;

    switch (msg->command) {
        case 0:
            if (work->field_534 != NULL) {
                model         = (TmdObject*)work->field_534->extra;
                model->flags &= 0xFF7F;
            }
            break;

        case 1:
            if (work->field_534 != NULL) {
                model         = (TmdObject*)work->field_534->extra;
                model->flags |= 0x80;
            }
            break;

        case 2:
            work->field_53C = 1;
            break;

        case 3:
            work->field_53C = 0;
            break;

        default:
            return 0;
    }
    return 0;
}
