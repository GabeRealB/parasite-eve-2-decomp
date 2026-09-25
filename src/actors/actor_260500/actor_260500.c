#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "actors/actors_shared_8013411c.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3E9C.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

/// Work block the actor hangs off its task's `Task::work` slot. The spawn
/// routine, `func_actor_260500_80149FB0`, allocates it with
/// `memCalloc(0x4B8, 0)` and stores it both there and in
/// `D_actor_260500_80159E4C`, so the size below is the allocation.
///
/// It opens with the light and colour matrices the actor's model is drawn
/// under, then the animation context with nineteen slots and one 0x10-byte
/// pose record per slot. The tail holds the animation state the update and the
/// message handlers share, and the walk and turn counters.
typedef struct Actor260500Work {
    /* 0x000 */ MATRIX     light;
    /* 0x020 */ MATRIX     color;
    /* 0x040 */ GpAnimCtx  anim;
    /* 0x054 */ GpAnimSlot slots[0x13];
    /* 0x34C */ byte       poses[0x13][0x10];
    /* 0x47C */ s16        field_47C; // animation reset mode `func_actor_260500_8014A110` dispatches on (1 reseeds via `func_actor_260500_8014A644`, 2 via `func_actor_260500_8014A5B4`, 3 after)
    /* 0x47E */ s16        field_47E; // copy of `field_480` the plain reseed records as the animation now playing
    /* 0x480 */ s16        field_480; // animation id the reset is seeded with, latched from the preset's `field_4`
    /* 0x482 */ s16        field_482; // cleared before the reset is handed to `func_actor_260500_8014A110`
    /* 0x484 */ byte       pad_484[0x2A];
    /* 0x4AE */ s16        yaw;       // last yaw handed to `Gfx_RotMatrixY`
    /* 0x4B0 */ byte       pad_4B0[0x2];
    /* 0x4B2 */ s16        field_4B2; // steps left in the walk the approach handler sets up
    /* 0x4B4 */ s16        field_4B4; // turn steps left while animation 3 plays; message 0x7DB arms it at 0x14
    /* 0x4B6 */ byte       pad_4B6[0x2];
} Actor260500Work;
STATIC_ASSERT_SIZEOF(Actor260500Work, 0x4B8);

/// The work block, published by the spawn routine and by the task handler
/// `func_actor_260500_8014A460` on every frame, so the message handlers and
/// the animation loops reach it without the task.
extern Actor260500Work* D_actor_260500_80159E4C;

/// The actor's own task, published by the spawn routine: the play-animation
/// handler runs the update on it and the visibility handler reaches its model.
extern Task* D_actor_260500_80159E50;

/// Reset argument the blended reseed forwards: the play-animation handler
/// latches the preset's `field_C` here, and the update sets it to 10 when a
/// walk ends.
extern s16 D_actor_260500_80159D7C;

/// Approach mode the last `func_actor_260500_8014A83C` call selected; the
/// update picks its step length from it.
extern s16 D_actor_260500_80159E54;

/// Animation preset the actor's play-animation message handler applies to the
/// work block: `field_4` is the animation id, `field_8` picks the reset path --
/// non-zero for the blended reseed, zero for the plain one -- and `field_C`
/// becomes the reset argument the blended reseed forwards.
typedef struct Actor260500AnimPreset {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
} Actor260500AnimPreset;
STATIC_ASSERT_SIZEOF(Actor260500AnimPreset, 0x10);

/// Payload of the actor's 0x7DB message; the handler reads only the halfword
/// at 0x2.
typedef struct Actor260500Msg {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} Actor260500Msg;
STATIC_ASSERT_SIZEOF(Actor260500Msg, 0x4);

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */
#define SCRATCH_SP (*(u32*)0x1F8003FC)

/// `func_800B4114` is declared locally with a signed `arg2`; see the note in
/// `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

extern s32        D_actor_260500_8014CB38;
extern s32        D_actor_260500_8014CBF8;
extern s32        D_actor_260500_8014D630;
extern s32        D_actor_260500_8014D7C8;
extern s32        D_actor_260500_8014D948;
extern s32        D_actor_260500_8014DAB0;
extern s32        D_actor_260500_8014DCC0;
extern GpMsgEntry D_actor_260500_80159D80[];
extern u8         D_actor_260500_80159DBC[];

extern u8 D_80072729;

void func_actor_260500_8014A110(Task* task);
void func_actor_260500_8014A4BC(GpEnemy* enemy, Task* task);
void func_actor_260500_8014A540(Task* task);
void func_actor_260500_8014A568(void);
void func_actor_260500_8014A5B4(void);
void func_actor_260500_8014A644(void);
void func_actor_260500_8014A99C(Task* task);

/// Steps the task's model `amount` units along its facing (the coordinate
/// matrix's z column, normalised and scaled on the GTE), using a scratch-pad
/// vector; skipped while `D_80072729` is 1.
static __inline__ void Actor260500_MoveForward(Task* task, s16 amount)
{
    GsCOORDINATE2* coord;
    SVECTOR*       head;
    SVECTOR*       vec;

    coord = ((TmdObject*)task->extra)->coords;
    if (D_80072729 != 1) {
        head                       = *(SVECTOR**)G_SCRATCH_HEAD;
        vec                        = head - 1;
        *(SVECTOR**)G_SCRATCH_HEAD = vec;
        Gfx_MatrixCol2(&coord->coord, vec);
        VectorNormalSS(vec, vec);
        gte_lddp(amount);
        gte_ldsv(vec);
        gte_gpf12();
        gte_stsv(vec);
        coord->coord.t[0]          += head[-1].vx;
        coord->coord.t[1]          += vec->vy;
        coord->coord.t[2]          += vec->vz;
        coord->flg                  = 0;
        *(SVECTOR**)G_SCRATCH_HEAD += 1;
    }
}

/// Loads cap file 2 and starts it (`func_800E6D4C(0x340, 0)`) when `arg0` is
/// non-zero, otherwise resets the cap state.
void func_actor_260500_80149E38(s32 arg0)
{
    if (arg0 != 0) {
        Gp_CapFile = 0;
        Gp_LoadCapFile(2);
        func_800E6D4C(0x340, 0);
        return;
    }
    Gp_ResetCap();
}

/// Sends message 0x7D4 (placement) with the record at
/// `D_actor_260500_8014CB38` to the task in lookup slot 4, when there is one.
void func_actor_260500_80149E80(void)
{
    s32 slot;

    slot = Gp_LookupSlot4(0);
    if (slot != 0) {
        Gp_DispatchMsg((Task*)slot, 0x7D4, (s32)&D_actor_260500_8014CB38, 0);
    }
}

void func_actor_260500_80149EBC(void)
{
    switch (GameFlag_GetNibble(0xE3)) {
        case 0:
            func_800E8634((s32)&D_actor_260500_8014CBF8, 0, (s32)&D_actor_260500_8014D630);
            GameFlag_SetNibble(0xE3, 1);
            break;
        case 1:
            func_800E8614((s32)&D_actor_260500_8014D7C8, 0);
            GameFlag_SetNibble(0xE3, 2);
            break;
        case 2:
            func_800E8614((s32)&D_actor_260500_8014D948, 0);
            GameFlag_SetNibble(0xE3, 3);
            break;
        case 3:
            func_800E8614((s32)&D_actor_260500_8014DAB0, 0);
            GameFlag_SetNibble(0xE3, 4);
            break;
        case 4:
            func_800E8614((s32)&D_actor_260500_8014DCC0, 0);
            break;
    }
}

/// Spawn routine (state 0 of `func_actor_260500_8014A460`): allocates the work
/// block and publishes it in `D_actor_260500_80159E4C` and the task's `work`
/// slot, binds the model to the view and hands it the block's light and colour
/// matrices, publishes the task in `D_actor_260500_80159E50`, relights the
/// model from a point 0x320 above its translation and binds the animation
/// stream. It then installs the message table and runs the first update with
/// the reset mode 2 / id 4 it seeds.
void func_actor_260500_80149FB0(GpEnemy* enemy, Task* task)
{
    VECTOR         vec;
    GsCOORDINATE2* coord;
    TmdObject*     obj;
    void*          work;

    obj                     = task->extra;
    coord                   = obj->coords;
    work                    = memCalloc(sizeof(Actor260500Work), 0);
    D_actor_260500_80159E4C = work;
    task->work              = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback      = func_actor_260500_8014A540;
    coord->sub              = &gGfxViewCoord;
    enemy->field_4          = &coord->coord;
    enemy->field_48         = 0;
    enemy->node.targeted    = 0;
    enemy->node.flags       = 1;
    obj->otOffset           = 1;
    obj->lightMtx           = &D_actor_260500_80159E4C->light;
    obj->colorMtx           = &D_actor_260500_80159E4C->color;
    vec.vx                  = coord->workm.t[0];
    vec.vy                  = coord->workm.t[1] - 0x320;
    D_actor_260500_80159E50 = task;
    vec.vz                  = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&D_actor_260500_80159E4C->anim, D_actor_260500_80159DBC, obj,
                  D_actor_260500_80159E4C->poses, D_actor_260500_80159E4C->slots);
    D_actor_260500_80159E4C->field_480 = 4;
    D_actor_260500_80159E4C->field_47C = 2;
    D_actor_260500_80159E4C->field_4B2 = 0;
    D_actor_260500_80159E4C->field_4B4 = 0;
    task->msgTable                     = D_actor_260500_80159D80;
    func_actor_260500_8014A110(task);
    task->state++;
}

/// Per-frame update: reset modes 1 and 2 run their one-shot reseed (blended or
/// plain) and switch to mode 3. In mode 3 the walking animations 2, 0xE and 0xF
/// step the model forward while `field_4B2` counts down, by a distance the
/// approach mode in `D_actor_260500_80159E54` picks, and blend into animation
/// 0xD with reset argument 10 when the walk ends; animation 3 turns the model
/// while `field_4B4` counts down. Mode 3 then ticks the animation.
void func_actor_260500_8014A110(Task* task)
{
    GsCOORDINATE2*   coord = ((TmdObject*)task->extra)->coords;
    Actor260500Work* work  = (Actor260500Work*)task->work;

    if (D_actor_260500_80159E4C->field_47C == 1) {
        func_actor_260500_8014A644();
        D_actor_260500_80159E4C->field_47C = 3;
    } else if (D_actor_260500_80159E4C->field_47C == 2) {
        func_actor_260500_8014A5B4();
        D_actor_260500_80159E4C->field_47C = 3;
    } else if (D_actor_260500_80159E4C->field_47C == 3) {
        if (work->field_480 == 0xE || work->field_480 == 2 || work->field_480 == 0xF) {
            if (work->field_4B2 != 0) {
                switch (D_actor_260500_80159E54) {
                    case 0:
                        Actor260500_MoveForward(task, 0x3C);
                        break;
                    case 1:
                        Actor260500_MoveForward(task, -0xF);
                        break;
                    case 2:
                        Actor260500_MoveForward(task, 0x19);
                        break;
                }
                if (--work->field_4B2 == 0) {
                    work->field_47C         = 1;
                    D_actor_260500_80159D7C = 10;
                    work->field_480         = 0xD;
                }
            }
        }
        if (work->field_480 == 3 && work->field_4B4 != 0) {
            work->yaw += 0x33;
            Gfx_RotMatrixY(&coord->coord, work->yaw, 1);
            coord->flg = 0;
            work->field_4B4--;
        }
        func_actor_260500_8014A568();
    }
}

/// Two-state task handler: publishes the task's work block in
/// `D_actor_260500_80159E4C` on the way through, then calls the spawn routine
/// or the per-frame state, whichever `Task::state` selects from a table built
/// on the stack.
void func_actor_260500_8014A460(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_260500_80149FB0,
        func_actor_260500_8014A4BC,
    };

    D_actor_260500_80159E4C = task->work;
    fns[task->state](task->spawnArg2, task);
}

/// Per-frame state (state 1 of `func_actor_260500_8014A460`): refreshes the
/// model root's world matrix, relights the model from a point 0x320 above its
/// translation, then runs the update and draws the ground shadow.
void func_actor_260500_8014A4BC(GpEnemy* enemy, Task* task)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    VECTOR         pos;

    obj   = (TmdObject*)task->extra;
    coord = obj->coords;
    Gp_UpdateCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1] - 0x320;
    pos.vz = coord->workm.t[2];
    func_800D7A9C(obj, &pos, 0, 3);
    func_actor_260500_8014A110(task);
    func_actor_260500_8014A99C(task);
}

/// `Task::exitCallback` the spawn routine installs: hands the task's `GpEnemy`
/// back to `Gp_DestroyEnemy`.
void func_actor_260500_8014A540(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}

/// Ticks animation slots 1..0x12 of the work block's animation context.
void func_actor_260500_8014A568(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickIndex(&D_actor_260500_80159E4C->anim, i);
        i++;
    } while (i < 0x13);
}

/// Plain reseed: marks animation slots 1..0x12 of the work block reset-pending
/// and reseeds each of them from the current animation id, then records that id
/// as the one now playing.
void func_actor_260500_8014A5B4(void)
{
    s32 i;

    i = 1;
    do {
        D_actor_260500_80159E4C->slots[i].rate = 1;
        Gp_AnimResetSlot(&D_actor_260500_80159E4C->anim, i, D_actor_260500_80159E4C->field_480);
        i++;
    } while (i < 0x13);
    D_actor_260500_80159E4C->field_47E = D_actor_260500_80159E4C->field_480;
}

/// Blended reseed: reseeds animation slots 1..0x12 of the work block from the
/// current animation id with the latched reset argument
/// `D_actor_260500_80159D7C`, and records that id as the one now playing.
void func_actor_260500_8014A644(void)
{
    s32 i;

    i = 1;
    do {
        func_800B4114(&D_actor_260500_80159E4C->anim, i, D_actor_260500_80159E4C->field_480, 0,
                      D_actor_260500_80159D7C);
        i++;
    } while (i < 0x13);
    D_actor_260500_80159E4C->field_47E = D_actor_260500_80159E4C->field_480;
}

/// Play-animation message handler: adopts the preset's animation id when it is
/// one of the first 0x24, latching the reset mode -- 1 for the blended reseed,
/// 2 for the plain one -- and the reset argument the blended reseed forwards,
/// then runs the update on the actor's task. Ids past the range are rejected
/// with -1 and leave the work block untouched.
s32 func_actor_260500_8014A6C4(Task* task, s32 arg1, Actor260500AnimPreset* preset)
{
    if (preset->field_4 < 0x24) {
        D_actor_260500_80159E4C->field_480 = preset->field_4;
        if (preset->field_8 != 0) {
            D_actor_260500_80159E4C->field_47C = 1;
            D_actor_260500_80159D7C            = preset->field_C;
        } else {
            D_actor_260500_80159E4C->field_47C = 2;
        }
        D_actor_260500_80159E4C->field_482 = 0;
        func_actor_260500_8014A110(D_actor_260500_80159E50);
        return 0;
    }
    return -1;
}

/// Visibility handler: bit 0 of `arg2` shows the actor's model (flags 0) or
/// hides it (0x80), and bit 1 ORs in 0x4.
s32 func_actor_260500_8014A754(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* obj;

    obj = (TmdObject*)D_actor_260500_80159E50->extra;
    if (arg2 & 1) {
        obj->flags = 0;
    } else {
        obj->flags = 0x80;
    }
    if (arg2 & 2) {
        obj->flags |= 4;
    }
    return 0;
}

/// Placement handler: turns the model to the placement's yaw, keeping that yaw
/// in the work block, and moves it to the placement's position. Only the Y
/// rotation is applied.
s32 func_actor_260500_8014A79C(Task* task, s32 arg1, ActorShared8013411cPlacement* placement)
{
    GsCOORDINATE2* coord;
    u16            yaw;

    coord                        = ((TmdObject*)task->extra)->coords;
    D_actor_260500_80159E4C->yaw = yaw = placement->rot.vy;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->flg        = 0;
    return 0;
}

/// Message 0x7DB: a zero payload halfword at 0x2 arms the work block's
/// `field_4B4` at 0x14.
s32 func_actor_260500_8014A818(Task* task, s32 arg1, Actor260500Msg* msg)
{
    if (msg->field_2 == 0) {
        D_actor_260500_80159E4C->field_4B4 = 0x14;
    }
    return 0;
}

/// Approach handler: turns the model to face `target` -- away from it in mode
/// 1, where the update then walks it backwards -- keeps the mode in
/// `D_actor_260500_80159E54`, and stores the number of steps the walk takes:
/// the planar distance over the mode's step length, 60 in mode 0, 15 in mode 1
/// and 25 in mode 2.
s32 func_actor_260500_8014A83C(Task* task, s32 arg1, VECTOR* target, s32 mode)
{
    GsCOORDINATE2*   coord;
    Actor260500Work* work;
    s32              steps;
    s32              dx;
    s32              dz;
    s32              dist;
    s32              angle;

    steps                   = 0;
    coord                   = ((TmdObject*)task->extra)->coords;
    work                    = (Actor260500Work*)task->work;
    D_actor_260500_80159E54 = mode;
    dx                      = target->vx - coord->coord.t[0];
    dz                      = target->vz - coord->coord.t[2];
    angle                   = ratan2(dx, dz);
    work->yaw               = angle;
    if (D_actor_260500_80159E54 == 1) {
        work->yaw = angle + 0x800;
    }
    Gfx_RotMatrixY(&coord->coord, work->yaw, 1);
    dist = SquareRoot0(dx * dx + dz * dz);
    switch (D_actor_260500_80159E54) {
        case 0:
            steps = 0x3C;
            break;
        case 1:
            steps = 0xF;
            break;
        case 2:
            steps = 0x19;
            break;
    }
    work->field_4B2 = dist / steps;
    return 0;
}

/// Draws the actor's ground shadow quad under the model root, unless the model
/// is hidden (`flags & 0x80`) or has no buffer yet. The root's world
/// translation is staged in a scratchpad `VECTOR3` rather than on the stack,
/// and the quad is drawn at a fixed brightness of 0xC0.
void func_actor_260500_8014A99C(Task* task)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    VECTOR3*       vec;

    obj   = (TmdObject*)task->extra;
    coord = obj->coords;
    if (!(obj->flags & 0x80) && obj->buffer != NULL) {
        vec     = (VECTOR3*)(SCRATCH_SP -= 0x18);
        vec->vx = coord->workm.t[0];
        vec->vy = coord->workm.t[1];
        vec->vz = coord->workm.t[2];
        Gp_DrawEffGroundQuad(vec, 0x200, 0xC0);
        SCRATCH_SP += 0x18;
    }
}
