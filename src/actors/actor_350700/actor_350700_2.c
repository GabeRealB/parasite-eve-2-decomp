#include "common.h"

#include <psyq/abs.h>

#include "actors/actor_350700.h"
#include "actors/actors_shared_801327f8.h"
#include "actors/actors_shared_80132f24.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

/// The four `TaskDesc`s `func_actor_350700_80162B30` spawns its child tasks
/// from, and the message table it points the parent's `Task::field_24` at:
/// ids 0x7D3/0x7D4/0x7D5/0x7DD/0x7DB against the handlers starting
/// `func_actor_350700_801636A8`, terminated by 0x7FFFFFFF.
extern TaskDesc   D_actor_350700_801708DC;
extern GpMsgEntry D_actor_350700_8017090C[];

/// State handler at index 1 of `D_actor_350700_80161E30`, the move body that
/// mirrors `ActorsShared80132920`: rotates the constant local-space offset
/// `D_actor_350700_80161E40` through the root part's matrix into `work->step`,
/// opens the per-axis stop threshold to 0x7FFF, which disables it for the
/// update loop, and advances `field_4C2` so the dispatcher runs the next
/// handler. Where the shared body rotates the offset unchanged, this one
/// shrinks it to -0.4 of its length whenever `field_4C4` is clear.
void func_actor_350700_8016261C(Task* arg0)
{
    Actor350700Work* work;
    GsCOORDINATE2*   coord;
    VECTOR           vec;

    coord = ((TmdObject*)arg0->extra)->field_8;
    work  = (Actor350700Work*)arg0->work;

    vec = D_actor_350700_80161E40;
    if (work->field_4C4 == 0) {
        vec.vx = vec.vx * -0.4;
        vec.vy = vec.vy * -0.4;
        vec.vz = vec.vz * -0.4;
    }
    ApplyMatrixLV(&coord->coord, &vec, (VECTOR*)&work->step);
    work->limit.vx = 0x7FFF;
    work->limit.vy = 0x7FFF;
    work->limit.vz = 0x7FFF;
    work->field_4C2++;
}

/// State handler at index 3 of `D_actor_350700_80161E30`, the turn-to-face body
/// that mirrors `ActorsShared80162540`. Euler-extracts the root coordinate into
/// `vec`, and while the yaw gap to the target `work->field_4BA` is at least
/// 0x61 it steps `vec.vy` toward it by 0x60 -- the step is taken on an `s32`
/// widening of the extracted yaw -- and otherwise snaps the yaw to the target
/// and plays anim 0x7D3, clearing the two body counters. Either way the root
/// coordinate is rebuilt as the identity matrix rotated by `vec`, which
/// `Gp_UpdateCoordTree` picks up once `flg` is cleared.
void func_actor_350700_80162764(Task* arg0)
{
    Actor350700Work*      work;
    Actor350700MatWords*  words;
    GsCOORDINATE2*        coord;
    SVECTOR               vec;
    Actor350700AnimPreset preset;
    s32                   vy;
    s16                   diff;

    coord = ((TmdObject*)arg0->extra)->field_8;
    work  = (Actor350700Work*)arg0->work;

    Gp_ExtractEuler(&vec, &coord->coord);
    diff = (u16)work->field_4BA - (u16)vec.vy;
    if (ABS(diff) >= 0x61) {
        vy = vec.vy;
        if (diff < 0) {
            vec.vy = vy - 0x60;
        } else {
            vec.vy = vy + 0x60;
        }
    } else {
        vec.vy          = work->field_4BA;
        preset.field_0  = 0;
        preset.field_4  = 1;
        preset.field_8  = 1;
        preset.field_C  = 4;
        preset.field_10 = 0;
        func_actor_350700_80162860(arg0, 0x7D3, &preset, 0);
        work->field_4C0 = 0;
        work->field_4C2 = 0;
    }

    words          = (Actor350700MatWords*)&coord->coord;
    words->m00_m01 = ONE;
    words->m02_m10 = 0;
    words->m11_m12 = ONE;
    words->m20_m21 = 0;
    words->m22     = ONE;
    RotMatrix(&vec, &coord->coord);
    coord->flg = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700_2", func_actor_350700_80162860);

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700_2", func_actor_350700_80162998);

/// `Gp_DispatchMsg` handler, the four-way visibility/mode switch of
/// `func_actor_141000_80133E8C` run against the `TmdObject` parked in
/// `Task::extra`. Mode 0 shows the model and clears the 4 flag, 1 hides it,
/// frees the aux buffers and clears the flag, 2 does both plus latching the
/// mode into the work block's `field_4C5`, and 3 hides it while setting the
/// flag. Anything else returns 1 and leaves the object alone; the handled
/// modes return 0.
s32 func_actor_350700_80162A14(Task* task, s32 arg1, s32 mode)
{
    TmdObject* obj;
    s32        ret;

    obj = task->extra;
    ret = 0;
    switch (mode) {
        case 0:
            obj->field_C |= 0x80;
            obj->field_C &= ~4;
            break;
        case 1:
            obj->field_C &= ~0x80;
            Tmd_AllocBuffers(obj);
            obj->field_C &= ~4;
            break;
        case 2:
            obj->field_C                             |= 0x80;
            ((Actor350700Work*)task->work)->field_4C5 = mode;
            obj->field_C                             |= 4;
            break;
        case 3:
            obj->field_C &= ~0x80;
            obj->field_C |= 4;
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700_2", func_actor_350700_80162AF4);

/// The parent's spawn handler, the same body `func_actor_335800_80162640` runs.
/// Allocates the 0x50C `Actor350700MainWork` block, seeds it, and spawns the
/// three children `D_actor_350700_801708DC` holds -- table entries 1, 2 and 3 --
/// parking them at `field_4FC` / `field_500` / `field_504`. The first two are
/// models: each has `TmdObject::field_24` / `field_25` loaded with the texture
/// page and CLUT row of the `GpAreaPlace` that entry selects, reached through
/// the area key `&gGameSession->field_4` and indexed by the model id the child's
/// own `spawnArg2` carries at `GpEnemy::field_8 >> 12`, and each then has its
/// texture stream processed twice when it has an aux buffer. The body ends by
/// handing the parent to `ActorsShared80132f24`, pointing `field_24` at the
/// message table and installing `func_actor_350700_801633BC` as its exit
/// callback.
void func_actor_350700_80162B30(Task* arg0)
{
    Actor350700MainWork* work;
    GpAreaKey            key;
    GpAreaKey*           sessionKey;
    u8*                  keyAddr;
    Task*                spawned;

    work = (Actor350700MainWork*)Mem_Calloc(0x50C, false);
    if (work == NULL) {
        Gp_EnemyTaskExit(arg0);
        return;
    }
    arg0->work      = (TaskIdMap*)work;
    work->field_475 = -1;
    work->field_476 = -1;
    work->field_508 = -1;
    work->field_4D8 = 0;
    work->field_4DC = 0;
    work->field_4E0 = 0;
    spawned         = Task_SpawnFromTable(&D_actor_350700_801708DC, 1, 8, (s32)arg0);
    if (spawned != NULL) {
        TmdObject*   model;
        GpAreaRec*   rec;
        GpAreaPlace* place;
        s32          idx;

        work->field_4FC = spawned;
        model           = (TmdObject*)spawned->extra;
        idx             = ((GpEnemy*)arg0->spawnArg2)->field_8 >> 12;
        sessionKey      = (GpAreaKey*)&gGameSession->field_4;
        key.field_3     = sessionKey->field_3;
        key.field_2     = sessionKey->field_2;
        key.field_1     = sessionKey->field_1;
        key.field_0     = sessionKey->field_0;
        Gp_SyncAreaKeyIndex(&key);
        rec             = Gp_GetNestedAreaRec(&key);
        place           = (GpAreaPlace*)((idx << 4) + (s32)rec->field_0);
        model->field_24 = place->field_D;
        model->field_25 = place->field_E;
        if (model->field_18 != NULL) {
            Tmd_ProcessStream(model);
            Tmd_ProcessStream(model);
        }
    }
    spawned = Task_SpawnFromTable(&D_actor_350700_801708DC, 2, 0xC, (s32)arg0);
    if (spawned != NULL) {
        TmdObject*   model;
        GpAreaRec*   rec;
        GpAreaPlace* place;
        s32          idx;

        work->field_500 = spawned;
        model           = (TmdObject*)spawned->extra;
        idx             = ((GpEnemy*)arg0->spawnArg2)->field_8 >> 12;
        /* Re-derived address, not the block-1 form: with `sessionKey->field_0`
           for the last byte, global CSE merges this block's area key with the
           one above into a single cross-block pseudo, and the allocation of
           `spawned` and every address temp after it shifts. */
        sessionKey  = (GpAreaKey*)(keyAddr = (u8*)&gGameSession->field_4);
        key.field_3 = sessionKey->field_3;
        key.field_2 = sessionKey->field_2;
        key.field_1 = ((GpAreaKey*)keyAddr)->field_1;
        key.field_0 = ((GpAreaKey*)(&gGameSession->field_4))->field_0;
        Gp_SyncAreaKeyIndex(&key);
        rec             = Gp_GetNestedAreaRec(&key);
        place           = (GpAreaPlace*)((idx << 4) + (s32)rec->field_0);
        model->field_24 = place->field_D;
        model->field_25 = place->field_E;
        if (model->field_18 != NULL) {
            Tmd_ProcessStream(model);
            Tmd_ProcessStream(model);
        }
    }
    spawned = Task_SpawnFromTable(&D_actor_350700_801708DC, 3, 8, (s32)arg0);
    if (spawned != NULL) {
        work->field_504 = spawned;
    }
    ActorsShared80132f24(arg0);
    arg0->field_24     = D_actor_350700_8017090C;
    arg0->exitCallback = func_actor_350700_801633BC;
    arg0->state       += 1;
}

/// Per-frame tick of the parent actor, the same body as
/// `func_actor_335800_80162844`: dispatches through the local two-entry table
/// `field_4F8` indexes -- the empty `func_actor_350700_801633F8` or the shared
/// `ActorsShared801327f8` -- then integrates the per-frame deltas at
/// `field_4C8..field_4D0` into the 16.16 accumulators at `field_4D8..field_4E0`,
/// adds their high halves to the root coordinate's translation, clears `flg`
/// and truncates the accumulators back to 16 bits. Ticks the animation slots
/// while `field_474` is set; and, unless the display object's `field_C` carries
/// 0x80, draws the ground-shadow quad from the second part's world matrix.
/// While `gGameSession->field_4D` is set it also clears that part's `flg`,
/// rebuilds its coordinate and rebuilds the actor colour; the colour rebuild
/// runs once more unconditionally. The `field_508` countdown then runs while it
/// is non-negative, freeing the model buffers on the frame it reaches zero; the
/// init's -1 disables it.
void func_actor_350700_80162D5C(Task* arg0)
{
    TmdObject*           ext      = arg0->extra;
    Actor350700MainWork* work     = (Actor350700MainWork*)arg0->work;
    TaskFunc             funcs[2] = { (TaskFunc)func_actor_350700_801633F8, ActorsShared801327f8 };
    VECTOR3              pos;
    GsCOORDINATE2*       coord;
    s32                  i;

    funcs[work->field_4F8](arg0);
    coord              = ((TmdObject*)arg0->extra)->field_8;
    work->field_4D8   += work->field_4C8;
    work->field_4DC   += work->field_4CC;
    work->field_4E0   += work->field_4D0;
    coord->coord.t[0] += (s16)(work->field_4D8 >> 16);
    coord->coord.t[1] += (s16)(work->field_4DC >> 16);
    coord->coord.t[2] += (s16)(work->field_4E0 >> 16);
    coord->flg         = 0;
    work->field_4D8    = (u16)work->field_4D8;
    work->field_4DC    = (u16)work->field_4DC;
    work->field_4E0    = (u16)work->field_4E0;
    if (work->field_474 != 0) {
        for (i = 1; i < 0x14; i++) {
            Gp_AnimTickIndex((GpAnimCtx*)work, i);
        }
    }
    if (!(ext->field_C & 0x80)) {
        if (func_800EA1A8((VECTOR3*)((TmdObject*)arg0->extra)->field_8[1].workm.t, &pos) != 0) {
            Gp_DrawEffGroundQuad(&pos, 0x300, Gp_State1C->field_8);
        }
    }
    if (gGameSession->field_4D != 0) {
        ((TmdObject*)arg0->extra)->field_8[1].flg = 0;
        Gp_UpdateCoord(&((TmdObject*)arg0->extra)->field_8[1]);
        func_800D7A9C(ext, (VECTOR*)((TmdObject*)arg0->extra)->field_8[1].workm.t, 0, 3);
    }
    func_800D7A9C(ext, (VECTOR*)((TmdObject*)arg0->extra)->field_8[1].workm.t, 0, 3);
    if (work->field_508 >= 0) {
        if (work->field_508 == 0) {
            Tmd_FreeBuffers(ext);
        }
        work->field_508--;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700_2", func_actor_350700_80162F7C);

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700_2", func_actor_350700_801630C0);

INCLUDE_RODATA("actors/nonmatchings/actor_350700/actor_350700_2", ActorsShared801327f8Table);

INCLUDE_RODATA("actors/nonmatchings/actor_350700/actor_350700_2", ActorsShared80132920Offset);
