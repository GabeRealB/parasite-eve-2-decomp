#include "common.h"

#include "actors/actor_461800.h"
#include "actors/actors_shared_8013411c.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

INCLUDE_ASM("actors/nonmatchings/actor_461800/actor_461800_2", func_actor_461800_80132AD8);

INCLUDE_ASM("actors/nonmatchings/actor_461800/actor_461800_2", func_actor_461800_80132B74);

/// Ticks animation slots 1..0x13 of the actor's animation context.
void func_actor_461800_80132C28(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickIndex(&D_actor_461800_80143894->anim, i);
        i++;
    } while (i < 0x14);
}

/// Marks animation slots 1..0x13 reset-pending and reseeds each of them from
/// the current animation id, then records that id as the one now playing.
void func_actor_461800_80132C74(void)
{
    s32 i;

    i = 1;
    do {
        D_actor_461800_80143894->slots[i].field_9 = 1;
        Gp_AnimResetSlot(&D_actor_461800_80143894->anim, i, D_actor_461800_80143894->field_4B8);
        i++;
    } while (i < 0x14);
    D_actor_461800_80143894->field_4B6 = D_actor_461800_80143894->field_4B8;
}

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s16 arg2, s32 arg3, s32 arg4);

/// Reset argument this overlay forwards to every reseeded slot.
extern s16 D_actor_461800_80139F58;

/// Reseeds animation slots 1..0x13 from the current animation id and records
/// that id as the one now playing.
void func_actor_461800_80132D04(void)
{
    s32 i;

    i = 1;
    do {
        func_800B4114(&D_actor_461800_80143894->anim, i, D_actor_461800_80143894->field_4B8, 0,
                      D_actor_461800_80139F58);
        i++;
    } while (i < 0x14);
    D_actor_461800_80143894->field_4B6 = D_actor_461800_80143894->field_4B8;
}

INCLUDE_ASM("actors/nonmatchings/actor_461800/actor_461800_2", func_actor_461800_80132D84);

INCLUDE_ASM("actors/nonmatchings/actor_461800/actor_461800_2", func_actor_461800_80132E14);

/// Seeds the task's `TmdObject` coordinate frame from `placement`: only the yaw
/// is used, remembered in the work block and applied with `Gfx_RotMatrixY`,
/// then the three longs become the coordinate's translation.
s32 func_actor_461800_80132EA4(Task* task, s32 arg1, ActorShared8013411cPlacement* placement)
{
    GsCOORDINATE2* coord;
    u16            yaw;

    coord                              = ((TmdObject*)task->extra)->field_8;
    D_actor_461800_80143894->field_4E6 = yaw = placement->rot.vy;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->flg        = 0;
    return 0;
}

s32 func_actor_461800_80132F20(Task* arg0, s32 arg1, Actor461800Msg* arg2, s32 arg3)
{
    if (arg2->field_2 == 0) {
        D_actor_461800_80143894->field_4EC = 0x14;
    }
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_461800/actor_461800_2", func_actor_461800_80132F44);

INCLUDE_ASM("actors/nonmatchings/actor_461800/actor_461800_2", func_actor_461800_8013307C);

INCLUDE_ASM("actors/nonmatchings/actor_461800/actor_461800_2", func_actor_461800_801331E4);

/// Two-state dispatcher whose handler table is built on the stack, publishing
/// the task's work block in `D_actor_461800_801438A0` on the way through so the
/// rest of the overlay can reach it without the task.
void func_actor_461800_80133554(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_461800_8013307C,
        func_actor_461800_801335B0,
    };

    D_actor_461800_801438A0 = (Actor461800Work2*)task->idMap;
    fns[task->state](task->spawnArg2, task);
}

INCLUDE_ASM("actors/nonmatchings/actor_461800/actor_461800_2", func_actor_461800_801335B0);
