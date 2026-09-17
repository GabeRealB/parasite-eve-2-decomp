#include "common.h"

#include "actors/actor_213000.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/gameplay.h"

#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

/// The actor's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern TaskFuncTable3 D_actor_213000_80149E30;
extern TaskFuncTable3 D_actor_213000_80149E3C;
extern TaskFuncTable3 D_actor_213000_80149E48;

void func_actor_213000_8014A158(void)
{
}

void func_actor_213000_8014A160(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_213000_80149E30;
    sp.funcs[task->state](task);
}

INCLUDE_ASM("actors/nonmatchings/actor_213000/actor_213000_2", func_actor_213000_8014A1B8);

INCLUDE_ASM("actors/nonmatchings/actor_213000/actor_213000_2", func_actor_213000_8014A2C4);

INCLUDE_ASM("actors/nonmatchings/actor_213000/actor_213000_2", func_actor_213000_8014A35C);

INCLUDE_ASM("actors/nonmatchings/actor_213000/actor_213000_2", func_actor_213000_8014A488);

void func_actor_213000_8014A520(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_213000_80149E3C;
    sp.funcs[task->state](task);
}

void func_actor_213000_8014A578(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_213000_80149E48;
    sp.funcs[task->state](task);
}

/// Per-frame tick: advances the animation context's slots while the work block
/// says it is live, and once the session has finished loading a stage rebuilds
/// model part 1's world matrix and hands its translation to the ground-shadow
/// helper. The work block's countdown then frees the model buffers as it
/// reaches zero.
void func_actor_213000_8014A5D0(Task* task)
{
    Actor213000Work* work;
    TmdObject*       extra;
    GsCOORDINATE2*   coords;
    s32              i;

    extra  = (TmdObject*)task->extra;
    work   = (Actor213000Work*)task->idMap;
    coords = &extra->field_8[1];
    if (work->field_474 != 0) {
        for (i = 1; i < 0x14; i++) {
            Gp_AnimTickIndex((GpAnimCtx*)work, i);
        }
    }
    if (Game_Session->field_4D != 0) {
        coords->flg = 0;
        Gp_UpdateCoord(coords);
        func_800D7A9C(extra, (VECTOR*)coords->workm.t, 0, 3);
    }
    if (work->field_477 >= 0) {
        if (work->field_477 == 0) {
            Tmd_FreeBuffers(extra);
        }
        work->field_477--;
    }
}

/// Republishes the work block's light/color matrices onto the TMD object and
/// rebuilds model part 1's world matrix from it, then hands that part's
/// translation to the ground-shadow helper.
void func_actor_213000_8014A6AC(Task* task)
{
    Actor213000Work* work;
    GsCOORDINATE2*   coords;
    TmdObject*       extra;

    work            = (Actor213000Work*)task->idMap;
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

extern void* D_actor_213000_80157DDC[];

/// Animation-preset handler: while the preset's bank index differs from the
/// one the work block latched, clears the animation id to -1, latches the new
/// index and re-seeds the animation context from that bank
/// (`D_actor_213000_80157DDC`). The preset's `field_4` then replaces the id,
/// and `field_8` picks the slot walk -- the blended `func_800B4114` reseed with
/// a literal count of 6, or the plain `Gp_AnimResetSlot` -- before every slot
/// is ticked once and the `field_474` latch is set. The same body as
/// `func_actor_335800_801632A4` and `func_actor_361100_801634D0`.
s32 func_actor_213000_8014A70C(Task* task, s32 arg1, Actor213000AnimPreset* msg)
{
    Actor213000Work* work;
    TmdObject*       ext;
    s32              i;

    work = (Actor213000Work*)task->idMap;
    ext  = task->extra;
    if (msg->field_0 != work->field_476) {
        work->field_476 = msg->field_0;
        work->field_475 = -1;
        func_800B3F84(&work->anim, D_actor_213000_80157DDC[work->field_476], (GpAnimObj*)ext, work->field_334,
                      work->slots);
    }
    work->field_475 = msg->field_4;
    if (msg->field_8 != 0) {
        for (i = 1; i < 0x14; i++) {
            func_800B4114(&work->anim, i, work->field_475, 0, 6);
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
