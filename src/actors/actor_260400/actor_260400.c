#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "actors/actors_shared_8013411c.h"
#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3E9C.h"
#include "gameplay/D4.h"
#include "gameplay/areaplace.h"
#include "gameplay/gameplay.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

/// Work block the actor hangs off its task's `Task::work` slot. The spawn
/// routine, `func_actor_260400_80149FE0`, allocates it with
/// `memCalloc(0x4F8, 0)` and stores it both there and in
/// `D_actor_260400_80154C70`, so the size below is the allocation.
///
/// It opens with the light and colour matrices the actor's model is drawn
/// under, then the animation context with twenty slots and one 0x10-byte pose
/// record per slot. The tail holds the animation state the update and the
/// message handlers share, the walk and turn counters, and the helper task
/// the spawn routine starts.
typedef struct Actor260400Work {
    /* 0x000 */ MATRIX     light;
    /* 0x020 */ MATRIX     color;
    /* 0x040 */ GpAnimCtx  anim;
    /* 0x054 */ GpAnimSlot slots[0x14];
    /* 0x374 */ byte       poses[0x14][0x10];
    /* 0x4B4 */ s16        field_4B4; // reset mode: 1 runs the blended reseed, 2 the plain one, 3 the per-frame update
    /* 0x4B6 */ s16        field_4B6; // copy of `field_4B8` the reseeds record as the animation now playing
    /* 0x4B8 */ s16        field_4B8; // animation id the slots are seeded with
    /* 0x4BA */ s16        field_4BA; // cleared by the play-animation handler before the reseed
    /* 0x4BC */ byte       pad_4BC[0x2A];
    /* 0x4E6 */ u16        yaw;       // last yaw handed to `Gfx_RotMatrixY`
    /* 0x4E8 */ byte       pad_4E8[0x2];
    /* 0x4EA */ s16        field_4EA; // steps left in the walk the approach handler sets up
    /* 0x4EC */ s16        field_4EC; // turn steps left while animation 3 plays
    /* 0x4EE */ byte       pad_4EE[0x2];
    /* 0x4F0 */ Task*      field_4F0; // helper task the spawn routine starts; the exit callback kills it
    /* 0x4F4 */ s8         field_4F4; // non-zero once the helper's model may be shown
    /* 0x4F5 */ byte       pad_4F5[0x3];
} Actor260400Work;
STATIC_ASSERT_SIZEOF(Actor260400Work, 0x4F8);

/// Payload of the actor's 0x7DB message; the handler reads only the halfword
/// at 0x2, which selects the action.
typedef struct Actor260400Msg {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} Actor260400Msg;
STATIC_ASSERT_SIZEOF(Actor260400Msg, 0x4);

/// Animation preset the actor's play-animation message handler applies to the
/// work block: `field_4` is the animation id, `field_8` picks the reset path --
/// non-zero for the blended reseed, zero for a plain one -- and `field_C`
/// becomes the reset argument the blended reseed forwards.
typedef struct Actor260400AnimPreset {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
} Actor260400AnimPreset;
STATIC_ASSERT_SIZEOF(Actor260400AnimPreset, 0x10);

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */
#define SCRATCH_SP (*(u32*)0x1F8003FC)

/// `func_800B4114` is declared locally with a signed `arg2`; see the note in
/// `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

extern s32      D_actor_260400_8014C6C0;
extern s32      D_actor_260400_8014C788;
extern s32      D_actor_260400_8014CF38;
extern s32      D_actor_260400_8014D118;
extern s32      D_actor_260400_8014D208;
extern s32      D_actor_260400_8014D340;
extern s32      D_actor_260400_8014D4A8;
extern s32      D_actor_260400_8014D610;
extern u8       D_actor_260400_80154BE8[];
extern TaskDesc D_actor_260400_80154C18[];
extern u8       D_actor_260400_80154C30[];

/// Reset argument the blended reseed forwards: the play-animation handler
/// latches the preset's `field_C` here, and the update sets it to 10 when a
/// walk ends.
extern s16 D_actor_260400_80154BE4;

/// The work block, published by the spawn routine and by the task handler
/// `func_actor_260400_8014A550` on every frame, so the message handlers and
/// the animation loops reach it without the task.
extern Actor260400Work* D_actor_260400_80154C70;

/// The actor's own task, published by the spawn routine: the helper task
/// hangs its model off this task's model parts, the play-animation handler runs
/// the update on it, and the visibility handler reaches its model.
extern Task* D_actor_260400_80154C74;

/// Approach mode the last `func_actor_260400_8014AB50` call selected; the
/// update picks its step length from it. It is the image's trailing halfword,
/// which the split covers as padding, so it has no symbol-file declaration.
extern s16 D_actor_260400_80154C78;

extern u8 D_80072729;

void func_actor_260400_8014A200(Task* task);
void func_actor_260400_8014A5AC(GpEnemy* enemy, Task* task);
void func_actor_260400_8014A630(Task* task);
void func_actor_260400_8014A66C(Task* task);
void func_actor_260400_8014A7AC(void);
void func_actor_260400_8014A7F8(void);
void func_actor_260400_8014A888(void);

/// Steps the task's model `amount` units along its facing (the coordinate
/// matrix's z column, normalised and scaled on the GTE), using a scratch-pad
/// vector; skipped while `D_80072729` is 1.
static __inline__ void Actor260400_MoveForward(Task* task, s16 amount)
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

void func_actor_260400_80149E38(void)
{
    switch (GameFlag_GetNibble(0xE3)) {
        case 0:
            func_800E8634((s32)&D_actor_260400_8014C788, 0, (s32)&D_actor_260400_8014CF38);
            GameFlag_SetNibble(0xE3, 1);
            break;
        case 1:
            if ((Gp_GetCurBit2Flag(4) == 1) || (Gp_GetCurBit2Flag(5) == 1)) {
                func_800E8614((s32)&D_actor_260400_8014D118, 0);
            } else {
                func_800E8614((s32)&D_actor_260400_8014D208, 0);
                GameFlag_SetNibble(0xE3, 2);
            }
            break;
        case 2:
            func_800E8614((s32)&D_actor_260400_8014D340, 0);
            GameFlag_SetNibble(0xE3, 3);
            break;
        case 3:
            func_800E8614((s32)&D_actor_260400_8014D4A8, 0);
            GameFlag_SetNibble(0xE3, 4);
            break;
        case 4:
            func_800E8614((s32)&D_actor_260400_8014D610, 0);
            break;
    }
}

void func_actor_260400_80149F5C(s32 arg0)
{
    if (arg0 != 0) {
        Gp_CapFile = 0;
        Gp_LoadCapFile(1);
        func_800E6D4C(0x340, 0);
        return;
    }
    Gp_ResetCap();
}

void func_actor_260400_80149FA4(void)
{
    s32 slot;

    slot = Gp_LookupSlot4(0);
    if (slot != 0) {
        Gp_DispatchMsg((Task*)slot, 0x7D4, (s32)&D_actor_260400_8014C6C0, 0);
    }
}

/// Spawn routine (state 0 of `func_actor_260400_8014A550`): allocates the work
/// block and publishes it in `D_actor_260400_80154C70` and the task's `work`
/// slot, binds the model to the view and hands it the block's light and colour
/// matrices, publishes the task in `D_actor_260400_80154C74`, relights the
/// model from a point 0x320 above its translation and binds the animation
/// stream. It then starts the helper task and textures the helper's model from
/// the area placement record the spawning enemy names, before running the
/// first update with the reset mode 2 / id 1 it seeds.
void func_actor_260400_80149FE0(GpEnemy* enemy, Task* task)
{
    VECTOR         vec;
    GpAreaKey      key;
    GsCOORDINATE2* coord;
    TmdObject*     obj;
    TmdObject*     model;
    Task*          spawned;
    GpAreaPlace*   place;
    s32            idx;
    u32            raw;
    GpAreaKey*     sessionKey;
    GpAreaKey*     keyPtr;
    void*          work;

    obj                     = task->extra;
    coord                   = obj->coords;
    work                    = memCalloc(0x4F8, 0);
    D_actor_260400_80154C70 = work;
    task->work              = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback      = func_actor_260400_8014A630;
    coord->sub              = &gGfxViewCoord;
    enemy->field_4          = &coord->coord;
    enemy->field_48         = 0;
    enemy->node.targeted    = 0;
    enemy->node.flags       = 1;
    obj->otOffset           = 1;
    obj->flags              = 0;
    obj->lightMtx           = &D_actor_260400_80154C70->light;
    obj->colorMtx           = &D_actor_260400_80154C70->color;
    vec.vx                  = coord->workm.t[0];
    vec.vy                  = coord->workm.t[1] - 0x320;
    D_actor_260400_80154C74 = task;
    vec.vz                  = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&D_actor_260400_80154C70->anim, D_actor_260400_80154C30, obj,
                  D_actor_260400_80154C70->poses, D_actor_260400_80154C70->slots);
    D_actor_260400_80154C70->field_4B8 = 1;
    D_actor_260400_80154C70->field_4B4 = 2;
    spawned                            = Task_SpawnFromTable(D_actor_260400_80154C18, 1, 8, 0);
    if (spawned != NULL) {
        D_actor_260400_80154C70->field_4F0 = spawned;
        sessionKey                         = (GpAreaKey*)&gGameSession->at4.loc;
        model                              = spawned->extra;
        raw                                = ((GpEnemy*)task->spawnArg2)->placeKey;
        key.stage                          = sessionKey->stage;
        key.area                           = sessionKey->area;
        key.room                           = sessionKey->room;
        keyPtr                             = &key;
        TOUCH_REG(keyPtr);
        key.view = sessionKey->view;
        idx      = raw >> 12;
        Gp_SyncAreaKeyIndex(keyPtr);
        place        = (GpAreaPlace*)((idx << 4) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
        model->tpage = place->tpage;
        model->clut  = place->clut;
        if (model->buffer != NULL) {
            tmdProcessStream(model);
            tmdProcessStream(model);
        }
    }
    D_actor_260400_80154C70->field_4EA = 0;
    D_actor_260400_80154C70->field_4EC = 0;
    D_actor_260400_80154C70->field_4F4 = 0;
    task->msgTable                     = D_actor_260400_80154BE8;
    func_actor_260400_8014A200(task);
    task->state++;
}

/// Per-frame update: reset modes 1 and 2 run their one-shot reseed (blended or
/// plain) and switch to mode 3. In mode 3 the walking animations 2, 0xE and 0xF
/// step the model forward while `field_4EA` counts down, by a distance the
/// approach mode in `D_actor_260400_80154C78` picks, and blend into animation
/// 0xD with reset argument 10 when the walk ends; animation 3 turns the model
/// while `field_4EC` counts down. Mode 3 then ticks the animation.
void func_actor_260400_8014A200(Task* task)
{
    GsCOORDINATE2*   coord = ((TmdObject*)task->extra)->coords;
    Actor260400Work* work  = (Actor260400Work*)task->work;

    if (D_actor_260400_80154C70->field_4B4 == 1) {
        func_actor_260400_8014A888();
        D_actor_260400_80154C70->field_4B4 = 3;
    } else if (D_actor_260400_80154C70->field_4B4 == 2) {
        func_actor_260400_8014A7F8();
        D_actor_260400_80154C70->field_4B4 = 3;
    } else if (D_actor_260400_80154C70->field_4B4 == 3) {
        if (work->field_4B8 == 0xE || work->field_4B8 == 2 || work->field_4B8 == 0xF) {
            if (work->field_4EA != 0) {
                switch (D_actor_260400_80154C78) {
                    case 0:
                        Actor260400_MoveForward(task, 0x3C);
                        break;
                    case 1:
                        Actor260400_MoveForward(task, -0xF);
                        break;
                    case 2:
                        Actor260400_MoveForward(task, 0x19);
                        break;
                }
                if (--work->field_4EA == 0) {
                    work->field_4B4         = 1;
                    D_actor_260400_80154BE4 = 10;
                    work->field_4B8         = 0xD;
                }
            }
        }
        if (work->field_4B8 == 3 && work->field_4EC != 0) {
            work->yaw += 0x33;
            Gfx_RotMatrixY(&coord->coord, (s16)work->yaw, 1);
            coord->flg = 0;
            work->field_4EC--;
        }
        func_actor_260400_8014A7AC();
    }
}

/// Two-state task handler: publishes the task's work block in
/// `D_actor_260400_80154C70` on the way through, then calls the spawn routine
/// or the per-frame state, whichever `Task::state` selects from a table built
/// on the stack.
void func_actor_260400_8014A550(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_260400_80149FE0,
        func_actor_260400_8014A5AC,
    };

    D_actor_260400_80154C70 = (Actor260400Work*)task->work;
    fns[task->state](task->spawnArg2, task);
}

/// Per-frame state (state 1 of `func_actor_260400_8014A550`): refreshes the
/// model root's world matrix, relights the model from a point 0x320 above its
/// translation, then runs the update and draws the ground shadow.
void func_actor_260400_8014A5AC(GpEnemy* enemy, Task* task)
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
    func_actor_260400_8014A200(task);
    func_actor_260400_8014A66C(task);
}

/// `Task::exitCallback` the spawn routine installs: hands the task's `GpEnemy`
/// back to `Gp_DestroyEnemy` and kills the helper task.
void func_actor_260400_8014A630(Task* task)
{
    Actor260400Work* work = (Actor260400Work*)task->work;

    Gp_DestroyEnemy(task->spawnArg2, task);
    taskKill(work->field_4F0);
}

/// Draws the actor's ground shadow quad under the model root, unless the model
/// is hidden (`flags & 0x80`) or has no buffer yet. The root's world
/// translation is staged in a scratchpad `VECTOR3` rather than on the stack,
/// and the quad is drawn at a fixed brightness of 0xC0.
void func_actor_260400_8014A66C(Task* task)
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

/// Task handler of the helper the spawn routine starts: the first tick hangs
/// the task's coordinate frame off the actor's model part `spawnArg1`, shows
/// its model and steps to state 1; every later tick relights the model from a
/// point 0x320 above the actor's root translation.
void func_actor_260400_8014A6F8(Task* task)
{
    TmdObject*     extra = task->extra;
    GsCOORDINATE2* coord = extra->coords;
    GsCOORDINATE2* parts = ((TmdObject*)D_actor_260400_80154C74->extra)->coords;
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

/// Ticks animation slots 1..0x13 of the work block's animation context.
void func_actor_260400_8014A7AC(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickIndex(&D_actor_260400_80154C70->anim, i);
        i++;
    } while (i < 0x14);
}

/// Plain reseed: marks animation slots 1..0x13 of the work block reset-pending
/// and reseeds each of them from the current animation id, then records that id
/// as the one now playing.
void func_actor_260400_8014A7F8(void)
{
    s32 i;

    i = 1;
    do {
        D_actor_260400_80154C70->slots[i].rate = 1;
        Gp_AnimResetSlot(&D_actor_260400_80154C70->anim, i, D_actor_260400_80154C70->field_4B8);
        i++;
    } while (i < 0x14);
    D_actor_260400_80154C70->field_4B6 = D_actor_260400_80154C70->field_4B8;
}

/// Blended reseed: reseeds animation slots 1..0x13 of the work block from the
/// current animation id with the latched reset argument
/// `D_actor_260400_80154BE4`, and records that id as the one now playing.
void func_actor_260400_8014A888(void)
{
    s32 i;

    i = 1;
    do {
        func_800B4114(&D_actor_260400_80154C70->anim, i, D_actor_260400_80154C70->field_4B8, 0,
                      D_actor_260400_80154BE4);
        i++;
    } while (i < 0x14);
    D_actor_260400_80154C70->field_4B6 = D_actor_260400_80154C70->field_4B8;
}

/// Message 0x7D3 (play animation): adopts the preset's animation id when it is
/// one of the first 0x10, latching the reset mode -- 1 for the blended reseed,
/// 2 for the plain one -- and the reset argument the blended reseed forwards,
/// then runs the update on the actor's task. Ids past the range are rejected
/// with -1 and leave the work block untouched.
s32 func_actor_260400_8014A908(Task* task, s32 arg1, Actor260400AnimPreset* preset)
{
    if (preset->field_4 < 0x10) {
        D_actor_260400_80154C70->field_4B8 = preset->field_4;
        if (preset->field_8 != 0) {
            D_actor_260400_80154C70->field_4B4 = 1;
            D_actor_260400_80154BE4            = preset->field_C;
        } else {
            D_actor_260400_80154C70->field_4B4 = 2;
        }
        D_actor_260400_80154C70->field_4BA = 0;
        func_actor_260400_8014A200(D_actor_260400_80154C74);
        return 0;
    }
    return -1;
}

/// Message 0x7D5 (visibility): bit 0 of `arg2` shows both the actor's and the
/// helper's model (flags 0) or hides them (0x80), and bit 1 ORs in 0x4. Until
/// message 0x7DB has enabled the helper (`field_4F4`), its model is kept
/// hidden at 0x84 whatever the mask says.
s32 func_actor_260400_8014A998(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* obj;
    TmdObject* helperObj;

    obj       = (TmdObject*)D_actor_260400_80154C74->extra;
    helperObj = (TmdObject*)D_actor_260400_80154C70->field_4F0->extra;

    if (arg2 & 1) {
        obj->flags       = 0;
        helperObj->flags = 0;
    } else {
        obj->flags       = 0x80;
        helperObj->flags = 0x80;
    }
    if (arg2 & 2) {
        obj->flags       |= 4;
        helperObj->flags |= 4;
    }
    if ((u8)D_actor_260400_80154C70->field_4F4 == 0) {
        helperObj->flags = 0x84;
    }
    return 0;
}

/// Message 0x7D4 (placement): turns the model to the placement's yaw, keeping
/// that yaw in the work block, and moves it to the placement's position. Only
/// the Y rotation is applied.
s32 func_actor_260400_8014AA28(Task* task, s32 arg1, ActorShared8013411cPlacement* placement)
{
    GsCOORDINATE2* coord;
    u16            yaw;

    coord                        = ((TmdObject*)task->extra)->coords;
    D_actor_260400_80154C70->yaw = yaw = placement->rot.vy;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->flg        = 0;
    return 0;
}

/// Message 0x7DB: the payload's halfword at 0x2 selects the action. Case 0
/// starts a turn of 0x14 steps; case 1 enables and shows the helper's model,
/// but only while `func_800B7420(0x88)` returns 0; case 2 disables it and
/// hides the model again (flags 0x84).
s32 func_actor_260400_8014AAA4(Task* task, s32 arg1, Actor260400Msg* msg)
{
    TmdObject* obj;
    s32        mode;

    obj  = (TmdObject*)D_actor_260400_80154C70->field_4F0->extra;
    mode = msg->field_2;

    switch (mode) {
        case 0:
            D_actor_260400_80154C70->field_4EC = 0x14;
            break;
        case 1:
            if (func_800B7420(0x88) == 0) {
                D_actor_260400_80154C70->field_4F4 = mode;
                obj->flags                         = 0;
            }
            break;
        case 2:
            D_actor_260400_80154C70->field_4F4 = 0;
            obj->flags                         = 0x84;
            break;
    }
    return 0;
}

/// Message 0x7DD (approach): turns the model to face `target` -- away from it
/// in mode 1, where the update then walks it backwards -- keeps the mode in
/// `D_actor_260400_80154C78`, and stores the number of steps the walk takes:
/// the planar distance over the mode's step length, 60 in mode 0, 15 in mode 1
/// and 25 otherwise.
s32 func_actor_260400_8014AB50(Task* task, s32 arg1, VECTOR* target, s32 mode)
{
    GsCOORDINATE2*   coord;
    Actor260400Work* work;
    s32              dx;
    s32              dz;
    s32              steps;
    s32              dist;
    s32              angle;

    coord                   = ((TmdObject*)task->extra)->coords;
    work                    = (Actor260400Work*)task->work;
    D_actor_260400_80154C78 = mode;
    dx                      = target->vx - coord->coord.t[0];
    dz                      = target->vz - coord->coord.t[2];
    angle                   = ratan2(dx, dz);
    work->yaw               = angle;
    if (D_actor_260400_80154C78 == 1) {
        work->yaw = angle + 0x800;
    }
    Gfx_RotMatrixY(&coord->coord, (s16)work->yaw, 1);
    dist  = SquareRoot0(dx * dx + dz * dz);
    steps = 0x19;
    switch (D_actor_260400_80154C78) {
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
