#include "common.h"

#include <psyq/abs.h>

#include "actors/actor_113100.h"

#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/D4.h"

/// The actor's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern TaskFuncTable3 D_actor_113100_80131E24;
extern TaskFuncTable3 D_actor_113100_80131E30;
extern TaskFuncTable3 D_actor_113100_80131E3C;

/// Declared here rather than taken from `gameplay.h`: the overlays call this
/// with the part index and the owning task as extra arguments that the body
/// never reads, so the shared one-argument prototype does not describe this
/// call site.
extern MATRIX* Gp_GetStageView(u8*, s32, void*);

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

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
    GpCdRec10*       entry2;
    GpCdRec10*       entry3;
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
    task->work      = work;
    work->field_475 = -1;
    work->field_476 = -1;
    work->field_53D = -1;
    work->field_510 = 0;
    work->field_514 = 0;
    work->field_518 = 0;
    if (gGameSession->at4.loc.place == 2) {
        work->field_534 = Task_SpawnFromTable(&D_actor_113100_80144308, 1, 8, (s32)task);
    }

    child2 = Task_SpawnFromTable(&D_actor_113100_80144308, 2, 4, (s32)task);
    if (child2 != NULL) {
        sessionKey2 = (GpAreaKey*)&gGameSession->at4.loc;
        raw2        = ((GpEnemy*)task->spawnArg2)->field_8;
        model2      = (TmdObject*)child2->extra;
        key.stage   = sessionKey2->stage;
        key.area    = sessionKey2->area;
        key.room    = sessionKey2->room;
        areaByte0   = gGameSession->at4.loc.view;
        index2      = raw2 >> 12;
        key.view    = areaByte0;
        Gp_SyncAreaKeyIndex(&key);
        entry2        = (GpCdRec10*)((index2 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
        model2->tpage = entry2->field_D;
        model2->clut  = entry2->field_E;
        if (model2->buffer != NULL) {
            Tmd_ProcessStream(model2);
            Tmd_ProcessStream(model2);
        }
    }

    child3 = Task_SpawnFromTable(&D_actor_113100_80144308, 3, 2, (s32)task);
    if (child3 != NULL) {
        sessionKey3 = (GpAreaKey*)&gGameSession->at4.loc;
        raw3        = ((GpEnemy*)task->spawnArg2)->field_8;
        model3      = (TmdObject*)child3->extra;
        key.stage   = sessionKey3->stage;
        key.area    = sessionKey3->area;
        key.room    = sessionKey3->room;
        areaByte0   = gGameSession->at4.loc.view;
        index3      = raw3 >> 12;
        key.view    = areaByte0;
        Gp_SyncAreaKeyIndex(&key);
        entry3        = (GpCdRec10*)((index3 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
        model3->tpage = entry3->field_D;
        model3->clut  = entry3->field_E;
        if (model3->buffer != NULL) {
            Tmd_ProcessStream(model3);
            Tmd_ProcessStream(model3);
        }
    }

    ActorsShared80132f24(task);

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

INCLUDE_ASM("actors/nonmatchings/actor_113100/actor_113100", func_actor_113100_80132104);

/// Per-frame turn handler, one of the four bodies `func_actor_113100_80132FB4`
/// dispatches through `D_actor_113100_80131E48`. It recovers the root
/// coordinate's yaw from its 3x3 (`m[0][2]` over `m[2][2]`) and compares it
/// with the heading the work block latched in `field_53A`: more than 0x41 away
/// it steps `field_53A` 0x40 toward the model and only re-splats the identity
/// 3x3, within 0x41 it turns the root coordinate to `field_53A` and then
/// rotates the local forward offset (0, 0, 0x200000) into `field_500` with
/// `ApplyMatrixLV`, raises the three halves at `field_520` to 0x7FFF and
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
    Actor113100Work*      work;
    GsCOORDINATE2*        coord;
    Actor113100MatWords*  words;
    Actor113100MatWords*  turnWords;
    VECTOR                delta;
    Actor113100AnimPreset preset;
    s32                   angle;
    s32                   angle16;
    u16                   yaw;
    s16                   diff;

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
        turnWords          = (Actor113100MatWords*)&coord->coord;
        turnWords->m00_m01 = ONE;
        turnWords->m02_m10 = 0;
        turnWords->m11_m12 = ONE;
        turnWords->m20_m21 = 0;
        turnWords->m22     = ONE;
        func_8004BFF8((s16)yaw, &coord->coord);
        coord->flg = 0;
    } else {
        words          = (Actor113100MatWords*)&coord->coord;
        words->m00_m01 = ONE;
        words->m02_m10 = 0;
        words->m11_m12 = ONE;
        words->m20_m21 = 0;
        words->m22     = ONE;
        func_8004BFF8((s16)yaw, &coord->coord);
        delta.vx = 0;
        delta.vy = 0;
        delta.vz = 0x200000;
        ApplyMatrixLV(&coord->coord, &delta, &work->field_500);
        work->field_520.vx = 0x7FFF;
        work->field_520.vy = 0x7FFF;
        work->field_520.vz = 0x7FFF;
        preset.field_0     = 0;
        preset.field_4     = 2;
        preset.field_8     = 1;
        preset.field_C     = 4;
        preset.field_10    = 0;
        func_actor_113100_801331E8(task, 0x7D3, &preset, 0);
        work->field_532++;
        coord->flg = 0;
    }
}

/// One of the four main-body handlers `Actor113100Work::field_532` dispatches
/// through `D_actor_113100_80131E48`. It measures how far the work block's
/// `field_4F0` / `field_4F8` have drifted from the root coordinate's
/// translation -- each axis as the 16-bit magnitude of the difference, the
/// signed 32-bit subtraction only picking the direction -- and once both
/// magnitudes reach the thresholds `field_520.vx` / `.vz` it publishes the
/// 0x7D3 preset (`field_4` the animation id, `field_C` 5) and clears the
/// `field_500` vector, bumping `field_532` on to the next handler. Below the
/// thresholds it latches the magnitudes back into `field_520`, so the pair
/// tracks the last distance that was too small. `func_actor_335800_80162B3C`
/// is the same body over its own work block.
void func_actor_113100_8013264C(Task* task)
{
    Actor113100Work*      work;
    GsCOORDINATE2*        coord;
    SVECTOR               d;
    s32                   dx;
    s32                   dz;
    Actor113100AnimPreset preset;

    work  = (Actor113100Work*)task->work;
    coord = ((TmdObject*)task->extra)->coords;
    if (work->field_4F0 - coord->coord.t[0] >= 0) {
        dx = (u16)work->field_4F0 - (u16)coord->coord.t[0];
    } else {
        dx = (u16)coord->coord.t[0] - (u16)work->field_4F0;
    }
    d.vx = dx;
    if (work->field_4F8 - coord->coord.t[2] >= 0) {
        dz = (u16)work->field_4F8 - (u16)coord->coord.t[2];
    } else {
        dz = (u16)coord->coord.t[2] - (u16)work->field_4F8;
    }
    d.vz = dz;
    if (d.vx >= work->field_520.vx && d.vz >= work->field_520.vz) {
        preset.field_0  = 0;
        preset.field_4  = work->field_477;
        preset.field_8  = 1;
        preset.field_C  = 5;
        preset.field_10 = 0;
        func_actor_113100_801331E8(task, 0x7D3, &preset, 0);
        work->field_500.vx = 0;
        work->field_500.vy = 0;
        work->field_500.vz = 0;
        work->field_532++;
        return;
    }
    work->field_520.vx = d.vx < 0 ? -d.vx : d.vx;
    work->field_520.vz = d.vz < 0 ? -d.vz : d.vz;
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
/// actor as placed through `field_530` / `field_532`, then applies the start
/// preset in place -- the body of the 0x7D3 handler
/// `func_actor_113100_801331E8` written out inline against a preset built on
/// this function's own stack, `anim` picking the preset's `field_4`.
s32 func_actor_113100_801328EC(Task* task, s32 msgId, Actor113100Placement* place, Actor113100SpawnAnim* anim)
{
    Actor113100Work*       work;
    Actor113100Work*       w;
    Actor113100AnimPreset  preset;
    Actor113100AnimPreset* msg;
    s32                    i;
    TmdObject*             ext;

    w              = (Actor113100Work*)task->work;
    w->field_530   = 1;
    w->field_532   = 0;
    w->field_4F0   = place->pos.vx;
    w->field_4F4   = place->pos.vy;
    w->field_4F8   = place->pos.vz;
    w->field_528   = place->rot.vx;
    w->field_52A   = place->rot.vy;
    w->field_52C   = place->rot.vz;
    preset.field_0 = 0;
    if (anim != NULL) {
        preset.field_4 = anim->field_0;
        w->field_477   = anim->field_4;
    } else {
        preset.field_4 = 2;
        w->field_477   = 1;
    }
    preset.field_8  = 1;
    preset.field_C  = 5;
    preset.field_10 = 1;

    msg  = &preset;
    work = (Actor113100Work*)task->work;
    ext  = task->extra;
    if (msg->field_0 != work->field_476) {
        work->field_476 = msg->field_0;
        work->field_475 = -1;
        func_800B3F84(&work->anim, D_actor_113100_801442E0[work->field_476], (GpAnimObj*)ext, work->field_334,
                      work->slots);
    }
    if (msg->field_4 != work->field_475) {
        work->field_475 = msg->field_4;
        if (msg->field_8 != 0 && work->field_474 != 0) {
            for (i = 1; i < 0x14; i++) {
                func_800B4114(&work->anim, i, work->field_475, 0, msg->field_C);
            }
        } else {
            for (i = 1; i < 0x14; i++) {
                Gp_AnimResetSlot(&work->anim, i, work->field_475);
            }
        }
        for (i = 1; i < 0x14; i++) {
            Gp_AnimTickIndex(&work->anim, i);
        }
        work->field_474 = 1;
    }
    work->field_53C = D_actor_113100_801442E4[msg->field_4];
    return 0;
}

INCLUDE_RODATA("actors/nonmatchings/actor_113100/actor_113100", D_actor_113100_80131E20);

INCLUDE_RODATA("actors/nonmatchings/actor_113100/actor_113100", D_actor_113100_80131E24);

INCLUDE_RODATA("actors/nonmatchings/actor_113100/actor_113100", D_actor_113100_80131E30);

INCLUDE_RODATA("actors/nonmatchings/actor_113100/actor_113100", D_actor_113100_80131E3C);

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
    view  = Gp_GetStageView(&gGameSession->at4.loc.view, index, task);
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

INCLUDE_ASM("actors/nonmatchings/actor_113100/actor_113100", func_actor_113100_80132CF4);
