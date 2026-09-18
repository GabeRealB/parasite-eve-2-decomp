#include "common.h"

#include "actors/actor_113000.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"

#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

/// Message dispatch table the spawn handler parks in `Task::field_24`:
/// message id / handler pairs, terminated by 0x7FFFFFFF and a null word.
extern u8 D_actor_113000_8013ABC0[];

/// Spawn handler: allocates the work block, seeds its head, mirrors the
/// deferred-kill bit into the model, draws the ground shadow under the model's
/// second part, then hands the model's matrices to the light/color rebuilder.
void func_actor_113000_80131F90(Task* task)
{
    Actor113000Work* work;
    TmdObject*       extra;
    VECTOR3          pos;
    u16              flags;

    extra = (TmdObject*)task->extra;
    work  = (Actor113000Work*)Mem_Calloc(0x4CC, 0);
    if (work == NULL) {
        Gp_EnemyTaskExit(task);
        return;
    }
    task->work      = (TaskIdMap*)work;
    work->field_478 = -1;
    work->field_47C = -1;
    work->field_4C6 = 0;
    work->field_4C8 = -1;
    flags           = extra->field_C | 0x80;
    extra->field_C  = flags;
    if (!(flags & 0x80)) {
        if (func_800EA1A8((VECTOR3*)((TmdObject*)task->extra)->field_8[1].workm.t, &pos) != 0) {
            Gp_DrawEffGroundQuad(&pos, 0x200, Gp_State1C->field_8);
        }
    }
    func_actor_113000_801321A8(task);
    task->field_24     = D_actor_113000_8013ABC0;
    task->exitCallback = Gp_EnemyTaskExit;
    task->state++;
}

/// Per-frame tick, run after the model has been published: ticks the animation
/// slots while the preset bank `field_474` marks live, draws the ground shadow
/// under model part 1 while the model is not deferred, rebuilds that part's
/// world matrix while the session's 0x4D is set, runs the texture-upload
/// state, and counts the buffer free at `field_4C8` down to zero.
void func_actor_113000_80132070(Task* task)
{
    Actor113000Work* work;
    TmdObject*       extra;
    GsCOORDINATE2*   coords;
    VECTOR3          pos;
    s32              i;

    work  = (Actor113000Work*)task->work;
    extra = (TmdObject*)task->extra;
    if (work->field_474 != 0) {
        for (i = 1; i < 0x14; i++) {
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
    if (!(extra->field_C & 0x80)) {
        if (func_800EA1A8((VECTOR3*)((TmdObject*)task->extra)->field_8[1].workm.t, &pos) != 0) {
            Gp_DrawEffGroundQuad(&pos, 0x300, Gp_State1C->field_8);
        }
    }
    if (Game_Session->field_4D != 0) {
        coords        = ((TmdObject*)task->extra)->field_8;
        coords[1].flg = 0;
        Gp_UpdateCoord(&coords[1]);
        func_800D7A9C(extra, (VECTOR*)coords[1].workm.t, 0, 3);
    }
    func_actor_113000_80131E30((GpActorWork*)task);
    if (work->field_4C8 >= 0) {
        if (work->field_4C8 == 0) {
            Tmd_FreeBuffers(extra);
        }
        work->field_4C8--;
    }
}

/// Republishes the work block's light/color matrices onto the TMD object and
/// rebuilds model part 1's world matrix from it, then hands that part's
/// translation to the ground-shadow helper.
void func_actor_113000_801321A8(Task* task)
{
    Actor113000Work* work;
    GsCOORDINATE2*   coords;
    TmdObject*       extra;

    work            = (Actor113000Work*)task->work;
    extra           = (TmdObject*)task->extra;
    coords          = extra->field_8;
    extra->field_1C = &work->light;
    extra->field_20 = &work->color;
    coords[1].flg   = 0;
    Gp_UpdateCoord(&coords[1]);
    func_800D7A9C(extra, (VECTOR*)coords[1].workm.t, 0, 3);
}

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

/// Start-preset handler of the `Actor113000Work` block `func_actor_113000_80131F90`
/// parks in `Task::work`, and the twin of `func_actor_323300_80163718`: a preset
/// bank the block is not already on re-seeds it -- the animation id is reset to
/// -1, the bank is stored and the bank's animation source goes to `func_800B3F84`
/// with the block's context, its matrix table and its slots. The preset's
/// animation id is then latched, every slot 1..0x13 restarted -- through
/// `func_800B4114` when the preset asks for it, through `Gp_AnimResetSlot`
/// otherwise -- ticked once, and `field_474` raised.
s32 func_actor_113000_80132208(Task* task, s32 msgId, Actor113000AnimPreset* msg, s32 arg3)
{
    Actor113000Work* work;
    GpAnimObj*       ext;
    s32              i;

    work = (Actor113000Work*)task->work;
    ext  = task->extra;
    if (msg->field_0 != work->field_47C) {
        work->field_47C = msg->field_0;
        work->field_478 = -1;
        func_800B3F84(&work->anim, D_actor_113000_8013ABB0[work->field_47C], ext, work->field_334,
                      work->slots);
    }
    work->field_478 = msg->field_4;
    if (msg->field_8 != 0) {
        for (i = 1; i < 0x14; i++) {
            func_800B4114(&work->anim, i, work->field_478, 0, 6);
        }
    } else {
        for (i = 1; i < 0x14; i++) {
            Gp_AnimResetSlot(&work->anim, i, work->field_478);
        }
    }
    for (i = 1; i < 0x14; i++) {
        Gp_AnimTickIndex(&work->anim, i);
    }
    work->field_474 = 1;
    return 0;
}
