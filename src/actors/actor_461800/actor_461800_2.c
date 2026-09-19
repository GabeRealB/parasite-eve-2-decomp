#include "common.h"

#include <psyq/inline_c.h>

#include "actors/actor_461800.h"
#include "actors/actor_461800_move.h"
#include "actors/actors_shared_801324fc.h"
#include "actors/actors_shared_801326b4.h"
#include "actors/actors_shared_8013411c.h"
#include "actors/actors_shared_801366fc.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

extern u8 D_actor_461800_801437BC[];
extern u8 D_actor_461800_801437F8[];

INCLUDE_ASM("actors/nonmatchings/actor_461800/actor_461800_2", func_actor_461800_80132AD8);

/// State handler of the actor's model task: the spawn tick hangs the task's own
/// coordinate frame off the actor's part `spawnArg1` and steps to state 1, and
/// every later tick hands that part's world translation, dropped by 0x320 in y,
/// to `func_800D7A9C` for the part colour matrix.
void func_actor_461800_80132B74(Task* task)
{
    TmdObject*     extra = task->extra;
    GsCOORDINATE2* coord = extra->coords;
    GsCOORDINATE2* parts = ((TmdObject*)D_actor_461800_80143898->extra)->coords;
    GsCOORDINATE2* part  = parts + task->spawnArg1;
    VECTOR         vec;

    switch (task->state) {
        case 0:
            coord->flg   = 0;
            extra->flags = 0;
            coord->sub   = part;
            task->state++;
            break;
        case 1:
            vec.vx = parts->workm.t[0];
            vec.vy = parts->workm.t[1] - 0x320;
            vec.vz = parts->workm.t[2];
            func_800D7A9C(extra, &vec, 0, 3);
            break;
    }
}

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
        D_actor_461800_80143894->slots[i].rate = 1;
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

/// Applies an animation preset: the id is copied into the work block, the reset
/// mode is picked by the preset's blend flag and the reset argument is either
/// taken from the preset or left at 2, then the whole slot array is re-seeded.
/// Only the six known animation ids are accepted; anything else leaves the work
/// block untouched and reports the failure.
s32 func_actor_461800_80132D84(Task* task, s32 arg1, Actor461800AnimPreset* preset, s32 arg3)
{
    if (preset->field_4 < 6) {
        D_actor_461800_80143894->field_4B8 = preset->field_4;
        if (preset->field_8 != 0) {
            D_actor_461800_80143894->field_4B4 = 1;
            D_actor_461800_80139F58            = preset->field_C;
        } else {
            D_actor_461800_80143894->field_4B4 = 2;
        }
        D_actor_461800_80143894->field_4BA = 0;
        func_actor_461800_80132660(D_actor_461800_80143898);
        return 0;
    }
    return -1;
}

/// Applies a `Tmd_Create` flag word to the three model objects this actor owns:
/// the one on its own task and the two helper tasks' models in the work block.
/// `arg2 & 1` picks the base value -- 0x80 normally, 0 when set -- and
/// `arg2 & 2` ORs bit 0x4 in on top of it.
s32 func_actor_461800_80132E14(Task* arg0, s32 arg1, s32 arg2)
{
    TmdObject* own    = D_actor_461800_80143898->extra;
    TmdObject* first  = D_actor_461800_80143894->field_4F0->extra;
    TmdObject* second = D_actor_461800_80143894->field_4F4->extra;

    if (arg2 & 1) {
        own->flags    = 0;
        first->flags  = 0;
        second->flags = 0;
    } else {
        own->flags    = 0x80;
        first->flags  = 0x80;
        second->flags = 0x80;
    }
    if (arg2 & 2) {
        own->flags    |= 4;
        first->flags  |= 4;
        second->flags |= 4;
    }
    return 0;
}

/// Seeds the task's `TmdObject` coordinate frame from `placement`: only the yaw
/// is used, remembered in the work block and applied with `Gfx_RotMatrixY`,
/// then the three longs become the coordinate's translation.
s32 func_actor_461800_80132EA4(Task* task, s32 arg1, ActorShared8013411cPlacement* placement)
{
    GsCOORDINATE2* coord;
    u16            yaw;

    coord                              = ((TmdObject*)task->extra)->coords;
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

/// Approach mode the last `func_actor_461800_80132F44` call selected.
extern s16 D_actor_461800_8014389C;

/// Turns the model to face `target` -- away from it in mode 1 -- and stores the
/// per-step distance: the planar distance over 60 steps in mode 0, 15 in
/// mode 1 and 25 otherwise.
s32 func_actor_461800_80132F44(Task* task, s32 arg1, VECTOR* target, s32 mode)
{
    GsCOORDINATE2*   coord;
    Actor461800Work* work;
    s32              dx;
    s32              dz;
    s32              steps;
    s32              dist;
    s32              angle;

    coord                   = ((TmdObject*)task->extra)->coords;
    work                    = (Actor461800Work*)task->work;
    D_actor_461800_8014389C = mode;
    dx                      = target->vx - coord->coord.t[0];
    dz                      = target->vz - coord->coord.t[2];
    angle                   = ratan2(dx, dz);
    work->field_4E6         = angle;
    if (D_actor_461800_8014389C == 1) {
        work->field_4E6 = angle + 0x800;
    }
    Gfx_RotMatrixY(&coord->coord, work->field_4E6, 1);
    dist  = SquareRoot0(dx * dx + dz * dz);
    steps = 0x19;
    switch (D_actor_461800_8014389C) {
        case 0:
            steps = 0x3C;
            break;
        case 1:
            steps = 0xF;
            break;
        case 2:
            break;
    }
    work->field_4EA = dist / steps;
    return 0;
}

void func_actor_461800_8013307C(GpEnemy* enemy, Task* task)
{
    VECTOR         vec;
    GsCOORDINATE2* coord;
    TmdObject*     obj;

    obj        = task->extra;
    coord      = obj->coords;
    task->work = (TaskIdMap*)(D_actor_461800_801438A0 = memCalloc(0x4C0, false));
    if (D_actor_461800_801438A0 == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback       = ActorsShared801366fc;
    coord->sub               = &gGfxViewCoord;
    enemy->field_4           = &coord->coord;
    enemy->field_48          = 0;
    enemy->node.targeted     = 0;
    enemy->node.flags        = 1;
    obj->otOffset            = 1;
    obj->lightMtx            = &D_actor_461800_801438A0->light;
    obj->colorMtx            = &D_actor_461800_801438A0->color;
    vec.vx                   = coord->workm.t[0];
    vec.vy                   = coord->workm.t[1] - 0x320;
    ActorsShared801326b4Task = task;
    vec.vz                   = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&D_actor_461800_801438A0->anim, D_actor_461800_801437F8, obj,
                  &D_actor_461800_801438A0->slots[0x13], D_actor_461800_801438A0->slots);
    D_actor_461800_801438A0->field_480 = 1;
    D_actor_461800_801438A0->field_47C = 2;
    D_actor_461800_801438A0->field_4B2 = 0;
    D_actor_461800_801438A0->field_4B4 = 0;
    D_actor_461800_801438A0->field_4B8 = 0;
    D_actor_461800_801438A0->field_4BC = 0;
    task->msgTable                     = D_actor_461800_801437BC;
    func_actor_461800_801331E4(task);
    task->state++;
}

extern s16 D_actor_461800_801437B8;
extern s16 D_actor_461800_801438A8;

/// Per-frame update of the second variant: modes 1 and 2 run their one-shot
/// setup and switch to mode 3 for the next frame; mode 3 walks the model while `field_4B2` counts
/// down (distance picked by `D_actor_461800_801438A8`), turns it while
/// `field_4B4` counts down in animation 3, then ticks the animation.
void func_actor_461800_801331E4(Task* task)
{
    GsCOORDINATE2*    coord = ((TmdObject*)task->extra)->coords;
    Actor461800Work2* work  = (Actor461800Work2*)task->work;

    if (D_actor_461800_801438A0->field_47C == 1) {
        func_actor_461800_8013380C();
        D_actor_461800_801438A0->field_47C = 3;
    } else if (D_actor_461800_801438A0->field_47C == 2) {
        func_actor_461800_80133770();
        D_actor_461800_801438A0->field_47C = 3;
    } else if (D_actor_461800_801438A0->field_47C == 3) {
        if (work->field_480 == 0xE || work->field_480 == 2 || work->field_480 == 0xF) {
            if (work->field_4B2 != 0) {
                switch (D_actor_461800_801438A8) {
                    case 0:
                        Actor461800_MoveForward(task, 0x3C);
                        break;
                    case 1:
                        Actor461800_MoveForward(task, -0xF);
                        break;
                    case 2:
                        Actor461800_MoveForward(task, 0x19);
                        break;
                }
                if (--work->field_4B2 == 0) {
                    work->field_47C         = 1;
                    D_actor_461800_801437B8 = 10;
                    work->field_480         = 0xD;
                }
            }
        }
        if (work->field_480 == 3 && work->field_4B4 != 0) {
            work->field_4AE += 0x33;
            Gfx_RotMatrixY(&coord->coord, work->field_4AE, 1);
            coord->flg = 0;
            work->field_4B4--;
        }
        func_actor_461800_80133724();
        if (work->field_4BC != 0) {
            ActorsShared801324fc(task);
        }
    }
}

/// Two-state dispatcher whose handler table is built on the stack, publishing
/// the task's work block in `D_actor_461800_801438A0` on the way through so the
/// rest of the overlay can reach it without the task.
void func_actor_461800_80133554(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_461800_8013307C,
        func_actor_461800_801335B0,
    };

    D_actor_461800_801438A0 = (Actor461800Work2*)task->work;
    fns[task->state](task->spawnArg2, task);
}

INCLUDE_ASM("actors/nonmatchings/actor_461800/actor_461800_2", func_actor_461800_801335B0);
