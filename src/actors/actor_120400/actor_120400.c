#include "common.h"

#include "actors/actor_120400.h"
#include "actors/actors_shared_801327b4.h"
#include "actors/actors_shared_801327f8.h"
#include "actors/actors_shared_80132f24.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

/// The two child tasks the parent actor spawns, and the message table it points
/// its `Task::field_24` at; both live in this overlay's trailing data.
extern TaskDesc   D_actor_120400_8013E748;
extern GpMsgEntry D_actor_120400_8013E76C[];

/// The parent's spawn handler, the same body `func_actor_335800_80162640` and
/// `func_actor_350700_80162B30` run, with two children instead of two or three.
/// Allocates the 0x504 `Actor120400MainWork` block, seeds it, and spawns the
/// two children `D_actor_120400_8013E748` holds -- table entries 1 and 2. Each
/// has `TmdObject::field_24` / `field_25` loaded with the texture page and CLUT
/// row of the `GpAreaPlace` that entry selects, reached through the area key
/// `&gGameSession->loc.view` and indexed by the model id the child's own
/// `spawnArg2` carries at `GpEnemy::field_8 >> 12`, and each then has its
/// texture stream processed twice when it has an aux buffer. The body ends by
/// handing the parent to `ActorsShared80132f24`, pointing `field_24` at the
/// message table and installing `ActorsShared801327b4` as its exit callback.
void func_actor_120400_80131E5C(Task* arg0)
{
    Actor120400MainWork* work;
    GpAreaKey            key;
    GpAreaKey*           sessionKey;
    u8*                  keyAddr;
    Task*                spawned;

    work = (Actor120400MainWork*)Mem_Calloc(0x504, false);
    if (work == NULL) {
        Gp_EnemyTaskExit(arg0);
        return;
    }
    arg0->work      = (TaskIdMap*)work;
    work->field_475 = -1;
    work->field_476 = -1;
    work->field_500 = -1;
    work->field_4D8 = 0;
    work->field_4DC = 0;
    work->field_4E0 = 0;
    spawned         = Task_SpawnFromTable(&D_actor_120400_8013E748, 1, 8, (s32)arg0);
    if (spawned != NULL) {
        TmdObject*   model;
        GpAreaRec*   rec;
        GpAreaPlace* place;
        s32          idx;

        model       = (TmdObject*)spawned->extra;
        idx         = ((GpEnemy*)arg0->spawnArg2)->field_8 >> 12;
        sessionKey  = (GpAreaKey*)&gGameSession->loc;
        key.field_3 = sessionKey->field_3;
        key.field_2 = sessionKey->field_2;
        key.field_1 = sessionKey->field_1;
        key.field_0 = sessionKey->field_0;
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
    spawned = Task_SpawnFromTable(&D_actor_120400_8013E748, 2, 0xC, (s32)arg0);
    if (spawned != NULL) {
        TmdObject*   model;
        GpAreaRec*   rec;
        GpAreaPlace* place;
        s32          idx;

        model = (TmdObject*)spawned->extra;
        idx   = ((GpEnemy*)arg0->spawnArg2)->field_8 >> 12;
        /* Re-derived address, not the block-1 form: with `sessionKey->field_0`
           for the last byte, global CSE merges this block's area key with the
           one above into a single cross-block pseudo, and the allocation of
           `spawned` and every address temp after it shifts. */
        sessionKey  = (GpAreaKey*)(keyAddr = (u8*)&gGameSession->loc.view);
        key.field_3 = sessionKey->field_3;
        key.field_2 = sessionKey->field_2;
        key.field_1 = ((GpAreaKey*)keyAddr)->field_1;
        key.field_0 = ((GpAreaKey*)(&gGameSession->loc.view))->field_0;
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
    ActorsShared80132f24(arg0);
    arg0->field_24     = D_actor_120400_8013E76C;
    arg0->exitCallback = ActorsShared801327b4;
    arg0->state       += 1;
}

/// The parent's per-frame update, the same body `func_actor_350700_80162D5C`
/// and `func_actor_335800_80163568` run, without their duplicated
/// `func_800D7A9C`: the model's own state handler -- entry `field_4F8` of the
/// pair `{func_actor_120400_801327F0, ActorsShared801327f8}` -- runs first, then
/// the three 16.16 step accumulators at 0x4D8..0x4E0 take this frame's `step`,
/// their integer halves are added onto the root coordinate's translation and
/// the fraction is dropped, and `flg` is cleared so the tree rebuilds. With
/// `field_474` set every animation slot is ticked. Unless the model is hidden
/// (bit 0x80 of `TmdObject::field_C`), the second coordinate's work matrix
/// feeds `func_800EA1A8` and a non-zero result draws the ground-effect quad;
/// when `gGameSession->viewReady` is set the same coordinate is flagged stale,
/// updated and re-ranked through `func_800D7A9C`. The body ends decrementing
/// the `field_500` teardown timer, freeing the model's buffers on the frame it
/// reaches zero.
void func_actor_120400_80132050(Task* arg0)
{
    TmdObject*           ext      = arg0->extra;
    Actor120400MainWork* work     = (Actor120400MainWork*)arg0->work;
    TaskFunc             funcs[2] = { (TaskFunc)func_actor_120400_801327F0, ActorsShared801327f8 };
    VECTOR3              pos;
    GsCOORDINATE2*       coord;
    s32                  i;

    funcs[work->field_4F8](arg0);
    coord              = ((TmdObject*)arg0->extra)->field_8;
    work->field_4D8   += work->step.vx;
    work->field_4DC   += work->step.vy;
    work->field_4E0   += work->step.vz;
    coord->coord.t[0] += (s16)(work->field_4D8 >> 16);
    coord->coord.t[1] += (s16)(work->field_4DC >> 16);
    coord->coord.t[2] += (s16)(work->field_4E0 >> 16);
    coord->flg         = 0;
    work->field_4D8    = (u16)work->field_4D8;
    work->field_4DC    = (u16)work->field_4DC;
    work->field_4E0    = (u16)work->field_4E0;
    if (work->field_474 != 0) {
        for (i = 1; i < 0x14; i++) {
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
    if (!(ext->field_C & 0x80)) {
        if (func_800EA1A8((VECTOR3*)((TmdObject*)arg0->extra)->field_8[1].workm.t, &pos) != 0) {
            Gp_DrawEffGroundQuad(&pos, 0x300, Gp_State1C->field_8);
        }
    }
    if (gGameSession->viewReady != 0) {
        ((TmdObject*)arg0->extra)->field_8[1].flg = 0;
        Gp_UpdateCoord(&((TmdObject*)arg0->extra)->field_8[1]);
        func_800D7A9C(ext, (VECTOR*)((TmdObject*)arg0->extra)->field_8[1].workm.t, 0, 3);
    }
    if (work->field_500 >= 0) {
        if (work->field_500 == 0) {
            Tmd_FreeBuffers(ext);
        }
        work->field_500--;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_120400/actor_120400", func_actor_120400_80132254);

/// The parent's placement handler, the same body `func_actor_335800_80162C80`
/// and `func_actor_317000_80162458` run: the spawn position and rotation are
/// copied straight into `Actor120400MainWork` at 0x4B8..0x4C0 / 0x4F0..0x4F4,
/// and a start preset is built on the stack -- bank id 0, the optional start
/// animation's id and companion byte (0x10 and 1 when absent), 1, 5 and 1 --
/// and then applied in-line. A changed bank id latches `field_476` and reseeds
/// the animation through `func_800B3F84` with the bank this overlay's
/// `D_actor_120400_8013E744` selects; `field_475` takes the preset's animation
/// id, and a preset asking for slots while `field_474` says the slots are
/// already ticking is pushed onto `func_800B4114`'s per-slot loop instead of
/// the `Gp_AnimResetSlot` one, followed by a `Gp_AnimTickIndex` pass over the
/// same 0x14 slots and `field_474` raised. Returns 0 either way.
s32 func_actor_120400_80132398(Task* task, s32 arg1, Actor120400Placement* place, Actor120400SpawnAnim* anim)
{
    Actor120400MainWork*   work;
    Actor120400MainWork*   w;
    Actor120400AnimPreset  preset;
    Actor120400AnimPreset* msg;
    s32                    i;
    TmdObject*             ext;

    w              = (Actor120400MainWork*)task->work;
    w->field_4F8   = 1;
    w->field_4FA   = 0;
    w->field_4B8   = place->pos.vx;
    w->field_4BC   = place->pos.vy;
    w->field_4C0   = place->pos.vz;
    w->field_4F0   = place->rot.vx;
    w->field_4F2   = place->rot.vy;
    w->field_4F4   = place->rot.vz;
    preset.field_0 = 0;
    if (anim != NULL) {
        preset.field_4 = anim->field_0;
        w->field_477   = anim->field_4;
    } else {
        preset.field_4 = 0x10;
        w->field_477   = 1;
    }
    preset.field_8  = 1;
    preset.field_C  = 5;
    preset.field_10 = 1;

    msg  = &preset;
    work = (Actor120400MainWork*)task->work;
    ext  = task->extra;
    if (msg->field_0 != work->field_476) {
        work->field_476 = msg->field_0;
        func_800B3F84(&work->anim, D_actor_120400_8013E744[work->field_476], (GpAnimObj*)ext, work->poses,
                      work->slots);
    }
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
    return 0;
}

INCLUDE_RODATA("actors/nonmatchings/actor_120400/actor_120400", D_actor_120400_80131E20);

INCLUDE_RODATA("actors/nonmatchings/actor_120400/actor_120400", ActorsShared80138404Table);

INCLUDE_RODATA("actors/nonmatchings/actor_120400/actor_120400", D_actor_120400_80131E30);
