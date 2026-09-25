#include "common.h"
#include "psyq/libgte.h"
#include "psyq/libgpu.h"
#include "psyq/libgs.h"
#include "psyq/inline_c.h"
#include "gte.h"

#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/D4.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "actors/actors_shared_80132074.h"
#include "actors/actors_shared_8013231c.h"
#include "actors/actors_shared_801334c4.h"
#include "actors/actors_shared_80132604.h"

/// Work block of the enemy task, reached by its model-attach children through
/// the parent task's `Task::work`. The spawn handler
/// `func_actor_511000_80133958` allocates it (`memCalloc(0x488, 0)`), hands
/// `anim` / `slots` / `field_30C` to `func_800B3F84`, and points its own model
/// at the two matrices; the three children it spawns do the same.
typedef struct Actor511000ParentWork {
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[1];
    /* 0x03C */ byte       pad_3C[0x2D0];
    /* 0x30C */ byte       field_30C[0x130];
    /* 0x43C */ MATRIX     field_43C; ///< colour matrix, handed to TmdObject::colorMtx
    /* 0x45C */ MATRIX     field_45C; ///< light matrix, handed to TmdObject::lightMtx
    /* 0x47C */ s32        field_47C; ///< cleared by the spawn handler
    /* 0x480 */ s16        field_480; ///< frame counter; fades both matrices every third tick in state 3+
    /* 0x482 */ byte       pad_482[6];
} Actor511000ParentWork;
STATIC_ASSERT_SIZEOF(Actor511000ParentWork, 0x488);

/// Work block the task running `D_actor_511000_80131E48` parks in
/// `Task::work`; its spawn state allocates it with `memCalloc(0x70, 0)`.
/// `light` / `color` are the matrices the model's `lightMtx` / `colorMtx`
/// point at. `field_8` is the `Tmd_FreeBuffers` countdown (-1 disables it);
/// `field_C` is the 16-colour CLUT published through
/// `D_actor_511000_80147EB0`, written byte by byte as little-endian 15-bit
/// colours by the palette fade `func_actor_511000_80132E6C`, which steps
/// `field_2C` and holds on `field_2E`. `field_2F` latches once the message-1
/// children have been spawned.
typedef struct Actor511000Work {
    /* 0x00 */ byte   pad_0[8];
    /* 0x08 */ s32    field_8;
    /* 0x0C */ u8     field_C[0x20];
    /* 0x2C */ s16    field_2C;
    /* 0x2E */ s8     field_2E;
    /* 0x2F */ s8     field_2F;
    /* 0x30 */ MATRIX light;
    /* 0x50 */ MATRIX color;
} Actor511000Work;
STATIC_ASSERT_SIZEOF(Actor511000Work, 0x70);

/// `GsCOORDINATE2` at `TmdObject::coords` as the placement code uses it: the
/// libgs `param` slot at 0x44 holds the Euler angles written there and then
/// handed straight to `RotMatrix`.
typedef struct Actor511000Coord {
    /* 0x00 */ s32     flg;
    /* 0x04 */ MATRIX  coord;
    /* 0x24 */ MATRIX  workm;
    /* 0x44 */ SVECTOR rot;
} Actor511000Coord;
STATIC_ASSERT_SIZEOF(Actor511000Coord, 0x4C);

/// Work block `func_actor_511000_80132480` allocates (`memCalloc(0x4D4, 0)`)
/// and parks in that task's `Task::work`. Its front is the animation state the
/// animation message handler drives: the context, 20 slots and the pose
/// buffer handed to `func_800B3F84`. Its light/color pair is republished onto
/// model part 1, not the root coordinate.
typedef struct Actor511000Work2 {
    /* 0x000 */ GpAnimCtx  anim;
    /* 0x014 */ GpAnimSlot slots[0x14];
    /* 0x334 */ byte       field_334[0x140];
    /* 0x474 */ s32        field_474; ///< nonzero while the tick state steps animation slots 1..19
    /* 0x478 */ s32        field_478; ///< animation id the slots were last restarted on; -1 out of the spawn handler
    /* 0x47C */ s32        field_47C; ///< animation source last loaded; also the id `func_actor_511000_80133DEC` resets slots to
    /* 0x480 */ union {
        s32 word;                     ///< seeded to -1 whole by the spawn handler
        s16 half;                     ///< the halfword `func_actor_511000_80133DEC` clears after the slot reseed
    } field_480;
    /* 0x484 */ MATRIX light;
    /* 0x4A4 */ MATRIX color;
    /* 0x4C4 */ Task*  field_4C4; ///< task spawned from the table's index 1
    /* 0x4C8 */ Task*  field_4C8; ///< task spawned from the table's index 2
    /* 0x4CC */ s16    field_4CC; ///< set to 1 alongside `field_4D0` by the message-0x7E0 handler's mode 3
    /* 0x4CE */ u16    field_4CE; ///< upload countdown the texture-upload state runs down, reloaded from `field_4CC` on underflow
    /* 0x4D0 */ s16    field_4D0; ///< texture-upload step in progress, 0 when idle
    /* 0x4D2 */ s16    field_4D2; ///< frame counter of the tick state's mode-1 effect; cleared by the spawn handler
} Actor511000Work2;
STATIC_ASSERT_SIZEOF(Actor511000Work2, 0x4D4);

/// Animation preset `func_actor_511000_80133DEC` takes as `arg2`. `field_4`
/// is the animation id copied into `Actor511000Work2::field_47C`.
typedef struct Actor511000AnimPreset {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ s32 field_4;
} Actor511000AnimPreset;

/// Payload of message 0x7DB; the handler `func_actor_511000_8013287C` reads
/// the halfword at 0x2 as its mode.
typedef struct Actor511000Msg {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} Actor511000Msg;
STATIC_ASSERT_SIZEOF(Actor511000Msg, 0x4);

void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void func_actor_511000_80131E78(GpActorWork* arg0);
void func_actor_511000_80132048(GpActorWork* arg0);
void func_actor_511000_801321A8(Task* task);
void func_actor_511000_80132224(Task* task);
void func_actor_511000_80132284(Task* task);
void func_actor_511000_80132390(Task* task);
void func_actor_511000_80132480(Task* task);
void func_actor_511000_801325A4(Task* task);
void func_actor_511000_801329C4(Task* task);
void func_actor_511000_80132B14(Task* task, CVECTOR* col, s8* rgb);
void func_actor_511000_80133034(Task* task);
void func_actor_511000_801330F0(Task* task);
void func_actor_511000_80133220(Task* task);
void func_actor_511000_80133240(Task* task);
void func_actor_511000_801332E4(Task* task);
void func_actor_511000_801333A4(Task* task);
void func_actor_511000_801333C4(Task* task);
void func_actor_511000_80133498(Task* task);
void func_actor_511000_801336E0(Task* task, SVECTOR* rots, SVECTOR* trans, s32 index);
void func_actor_511000_80133760(Task* task);
void func_actor_511000_801337F0(Task* task);
void func_actor_511000_80133958(GpEnemy* enemy, Task* task);
void func_actor_511000_80133B80(GpEnemy* enemy, Task* task);
void func_actor_511000_80133F48(void* enemy, Task* task);
void func_actor_511000_80133F88(void* enemy, Task* task);
void func_actor_511000_8013401C(void* enemy, Task* task);
void func_actor_511000_8013405C(void* enemy, Task* task);
void func_actor_511000_801340F0(void* enemy, Task* task);
void func_actor_511000_80134130(void* enemy, Task* task);

/// State table of a child chained under a part of its spawner's model: the
/// attach state, an empty tick and the kill.
const TaskFuncTable3 D_actor_511000_80131E24 = {
    func_actor_511000_801321A8,
    func_actor_511000_80132224,
    taskKill,
};

/// State table of a child chained under a part of its spawner's model that
/// also follows the spawner's visibility: the attach state, the flag-mirroring
/// tick and the kill.
const TaskFuncTable3 D_actor_511000_80131E30 = {
    func_actor_511000_80132284,
    func_actor_511000_80132390,
    taskKill,
};

/// State table of the task that owns the `Actor511000Work2` block: its spawn
/// state, the per-frame tick and the enemy task exit.
const TaskFuncTable3 D_actor_511000_80131E3C = {
    func_actor_511000_80132480,
    (TaskFunc)func_actor_511000_80131E78,
    Gp_EnemyTaskExit,
};

/// State table of the task that owns the `Actor511000Work` block: its spawn
/// state, the per-frame tick and the kill.
const TaskFuncTable3 D_actor_511000_80131E48 = {
    func_actor_511000_80133034,
    func_actor_511000_801330F0,
    func_actor_511000_80133220,
};

/// State table of a child placed at a translation preset under its spawner:
/// the attach state, the spinning tick and the kill.
const TaskFuncTable3 D_actor_511000_80131E54 = {
    func_actor_511000_80133240,
    func_actor_511000_801332E4,
    func_actor_511000_801333A4,
};

/// State table of a child posed from the kill-countdown rotations: the attach
/// state, the tick that follows the countdown and the kill.
const TaskFuncTable3 D_actor_511000_80131E60 = {
    func_actor_511000_801333C4,
    func_actor_511000_801329C4,
    func_actor_511000_80133498,
};

/// The enemy's three state handlers - spawn, per-frame tick and teardown.
const GpEnemyTaskFuncTable3 D_actor_511000_80131E6C = {
    func_actor_511000_80133958,
    func_actor_511000_80133B80,
    Gp_DestroyEnemy,
};

/// Camera path `func_actor_511000_801330F0` walks once the session reaches
/// mode 0x18, one 0x24-byte `GpViewRec` per step of the kill countdown: the
/// rotation and projection plane repeat down the table while the translation
/// descends, so the spawn of a view task per index pans the camera as the
/// actor goes down. Handed straight to `Gp_TrySpawnViewTask`, exactly as
/// `Gp_SpawnViewTasks` hands its own stage record.
extern GpViewRec D_actor_511000_80147EE4[];

/// The three texture records the tick state's upload steps and the
/// message-0x7E0 handler post. Each is a lone `GpImgRec` whose 0x18x0x10
/// source rect repeats the size the users' scratch `RECT` carries and whose
/// `data` points at its pixel blob.
extern GpImgRec D_actor_511000_80146C74;
extern GpImgRec D_actor_511000_80146F94;
extern GpImgRec D_actor_511000_801472B4;

/// Animation sources the animation message handler selects by index.
extern void* D_actor_511000_801472E4[];

/// Spawn table `func_actor_511000_80132480` starts its two child tasks from,
/// and the message table it parks in `Task::msgTable`.
extern TaskDesc D_actor_511000_801472E8;
extern u8       D_actor_511000_8014730C[];

/// Offset `Gp_SpawnEff` places the tick state's effect at.
extern SVECTOR D_actor_511000_8014733C;

extern SVECTOR    D_actor_511000_80147344[];
extern SVECTOR    D_actor_511000_80147704[];
extern SVECTOR    D_actor_511000_80147AC4[];
extern u8         D_actor_511000_80147E84[];
extern GpImgRec   D_actor_511000_80147EA4;
extern u16*       D_actor_511000_80147EB0;
extern u8         D_actor_511000_80147EC4[];
extern GpMsgEntry D_actor_511000_80148FC4[];

/// Translation presets `func_actor_511000_80133760` copies onto the root
/// coordinate; `Task::spawnArg1` selects the entry.
extern SVECTOR D_actor_511000_80148FE4[];

extern CVECTOR D_actor_511000_80149004[];
extern DVECTOR D_actor_511000_80149014[];

/// Spawn table and per-child args for the children spawned on message 1.
extern TaskDesc D_actor_511000_80139924;
extern s32      D_actor_511000_80149054[];

/// Spawn table for the three children `func_actor_511000_80133958` creates.
extern TaskDesc D_actor_511000_80155070[];
/// Message table and animation data `func_actor_511000_80133958` installs.
extern void* D_actor_511000_801550A0;
extern void* D_actor_511000_801550C0;

/// Tick state: while `field_474` is set, steps animation slots 1..19; in
/// mode 1 counts `field_4D2` up and, on frame 0x10, plays the sound and spawns
/// the effect at the first child's model. Then draws the ground shadow under
/// model part 1, refreshes that part's coordinate and colour when the session
/// asks, runs the texture-upload state, and ticks the `field_480` countdown
/// that frees the model's buffers when it reaches zero.
void func_actor_511000_80131E78(GpActorWork* arg0)
{
    Actor511000Work2* work;
    TmdObject*        extra;
    GsCOORDINATE2*    coord;
    GsCOORDINATE2*    obj;
    VECTOR            pos;
    s32               i;
    s32               pan;

    extra = arg0->extra;
    work  = (Actor511000Work2*)arg0->actor;
    coord = &extra->coords[1];
    if (work->field_474 != 0) {
        for (i = 1; i < 0x14; i++) {
            Gp_AnimTickIndex((GpAnimCtx*)work, i);
        }
        if (work->field_478 == 1) {
            if (++work->field_4D2 == 0x10) {
                obj = ((TmdObject*)work->field_4C4->extra)->coords;
                pan = (s8)Gp_GetObjPan(obj);
                SndEvt_EnqueueType6(0x313A0003, pan, (s8)gpGetObjDepth(obj));
                Gp_SpawnEff(0x6006A, obj, 0, &D_actor_511000_8014733C);
            }
        }
    }
    if (!(extra->flags & 0x80)) {
        if (func_800EA1A8((VECTOR3*)arg0->extra->coords[1].workm.t, (VECTOR3*)&pos) != 0) {
            Gp_DrawEffGroundQuad((VECTOR3*)&pos, 0x300, Gp_State1C->groundShade);
        }
    }
    if (gGameSession->viewReady != 0) {
        coord->flg = 0;
        Gp_UpdateCoord(coord);
        func_800D7A9C(extra, (VECTOR*)coord->workm.t, 0, 3);
    }
    func_actor_511000_80132048(arg0);
    if (work->field_480.word >= 0) {
        if (work->field_480.word == 0) {
            Tmd_FreeBuffers(extra);
        }
        work->field_480.word--;
    }
}

/// Texture-upload state: runs the countdown at `field_4CE` down one a frame
/// while `field_4D0` names the upload in progress, and on the frame it
/// underflows posts that step's image over the 0x18x0x10 rect at y 0x28 --
/// reloading the countdown from `field_4CC` and advancing `field_4D0` for
/// steps 1 and 2, or clearing it and starting over for step 3. Steps 1 and 2
/// share their whole tail, which is what makes the compiler emit one copy of
/// it that step 1 jumps into; step 3 only differs in clearing the step
/// instead of advancing it.
void func_actor_511000_80132048(GpActorWork* arg0)
{
    Actor511000Work2* work;
    RECT              rect;

    work   = (Actor511000Work2*)arg0->actor;
    rect.x = 0;
    rect.y = 0x28;
    rect.w = 0x18;
    rect.h = 0x10;

    switch (work->field_4D0) {
        case 1:
            work->field_4CE = work->field_4CE - 1;
            if ((s16)work->field_4CE < 0) {
                Gp_LoadActorImage(arg0, &D_actor_511000_801472B4, &rect);
                work->field_4CE = work->field_4CC;
                work->field_4D0 = work->field_4D0 + 1;
            }
            break;
        case 2:
            work->field_4CE = work->field_4CE - 1;
            if ((s16)work->field_4CE < 0) {
                Gp_LoadActorImage(arg0, &D_actor_511000_80146F94, &rect);
                work->field_4CE = work->field_4CC;
                work->field_4D0 = work->field_4D0 + 1;
            }
            break;
        case 3:
            work->field_4CE = work->field_4CE - 1;
            if ((s16)work->field_4CE < 0) {
                Gp_LoadActorImage(arg0, &D_actor_511000_80146C74, &rect);
                work->field_4D0 = 0;
            }
            break;
    }
}

void func_actor_511000_80132150(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_511000_80131E24;
    sp.funcs[task->state](task);
}

/// Spawn state of the child in the first state table: chains this task's root
/// coordinate under the parent's part named by `spawnArg1`, takes the parent
/// model's light and colour matrices, reparents the task under the spawner
/// named by `spawnArg2` and advances to the next state.
void func_actor_511000_801321A8(Task* task)
{
    Task*          parent;
    s32            part;
    TmdObject*     extra;
    TmdObject*     parentExtra;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* dest;

    parent          = (Task*)task->spawnArg2;
    part            = task->spawnArg1;
    extra           = (TmdObject*)task->extra;
    parentExtra     = (TmdObject*)parent->extra;
    coord           = extra->coords;
    dest            = &parentExtra->coords[part];
    coord->flg      = 0;
    coord->sub      = dest;
    extra->lightMtx = parentExtra->lightMtx;
    extra->colorMtx = parentExtra->colorMtx;
    Task_Reparent(parent, task);
    task->state += 1;
}

/// Tick state of the first state table's child: nothing to do, the chained
/// coordinate follows the spawner by itself.
void func_actor_511000_80132224(Task* task)
{
}

void func_actor_511000_8013222C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_511000_80131E30;
    sp.funcs[task->state](task);
}

void func_actor_511000_80132284(Task* task)
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

/// Tick state of the child in the second state table: copies the spawner's
/// model flag bits 0x80 (hidden) and 0x4 (draw buffers allocated) onto this
/// task's model, rebuilding the buffers through `Tmd_AllocBuffers` when the
/// spawner's are gone.
void func_actor_511000_80132390(Task* task)
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

void func_actor_511000_80132428(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_511000_80131E3C;
    sp.funcs[task->state](task);
}

/// Spawn handler: allocates the work block, seeds its head, mirrors the
/// deferred-kill bit into the model, draws the ground shadow under the model's
/// second part, starts the actor's two child tasks and hands the model's
/// matrices to the light/color rebuilder, then advances to the tick handler.
void func_actor_511000_80132480(Task* task)
{
    Actor511000Work2* work;
    TmdObject*        extra;
    VECTOR3           pos;
    u16               flags;

    extra = (TmdObject*)task->extra;
    work  = (Actor511000Work2*)memCalloc(0x4D4, 0);
    if (work == NULL) {
        Gp_EnemyTaskExit(task);
        return;
    }
    task->work           = (TaskIdMap*)work;
    work->field_478      = -1;
    work->field_47C      = -1;
    work->field_4D2      = 0;
    work->field_480.word = -1;
    flags                = extra->flags | 0x80;
    extra->flags         = flags;
    if (!(flags & 0x80)) {
        if (func_800EA1A8((VECTOR3*)((TmdObject*)task->extra)->coords[1].workm.t, &pos) != 0) {
            Gp_DrawEffGroundQuad(&pos, 0x200, Gp_State1C->groundShade);
        }
    }
    work->field_4C4 = Task_SpawnFromTable(&D_actor_511000_801472E8, 1, 8, (s32)task);
    work->field_4C8 = Task_SpawnFromTable(&D_actor_511000_801472E8, 2, 0xC, (s32)task);
    func_actor_511000_801325A4(task);
    task->msgTable     = D_actor_511000_8014730C;
    task->exitCallback = Gp_EnemyTaskExit;
    task->state++;
}

/// Republishes the work block's light/color matrices onto the TMD object and
/// rebuilds model part 1's world matrix from it, then hands that part's
/// translation to the ground-shadow helper.
void func_actor_511000_801325A4(Task* task)
{
    Actor511000Work2* work;
    GsCOORDINATE2*    coords;
    TmdObject*        extra;

    work            = (Actor511000Work2*)task->work;
    extra           = (TmdObject*)task->extra;
    coords          = extra->coords;
    extra->lightMtx = &work->light;
    extra->colorMtx = &work->color;
    coords[1].flg   = 0;
    Gp_UpdateCoord(&coords[1]);
    func_800D7A9C(extra, (VECTOR*)coords[1].workm.t, 0, 3);
}

/// Animation message handler: when the source index in the payload changes,
/// reseeds the animation context from that entry of the source table; when the
/// animation id changes, restarts slots 1..19 on it, blended when the
/// payload's third word is set, steps them once and turns on the tick state's
/// per-frame stepping.
s32 func_actor_511000_80132604(Task* task, s32 arg1, ActorsShared80132604Args* msg, s32 arg3)
{
    Actor511000Work2* work;
    s32               i;
    register s32      id asm("v1");
    TmdObject*        ext;

    work = (Actor511000Work2*)task->work;
    id   = msg->field_0;
    ext  = (TmdObject*)task->extra;
    if (id != work->field_47C) {
        work->field_478 = -1;
        work->field_47C = id;
        func_800B3F84(&work->anim, D_actor_511000_801472E4[id], ext, work->field_334, work->slots);
    }
    if (msg->field_4 != work->field_478) {
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
    }
    return 0;
}

/// Placement message handler: writes the payload's translation into the root
/// coordinate, keeps its Euler angles in the coordinate's `rot` slot and
/// rebuilds the rotation from them, then clears `flg` so the world matrix is
/// recomputed.
s32 func_actor_511000_80132724(Task* task, s32 arg1, ActorsShared8013231cArgs* args)
{
    Actor511000Coord* coord;

    coord             = (Actor511000Coord*)((TmdObject*)task->extra)->coords;
    coord->coord.t[0] = args->pos.vx;
    coord->coord.t[1] = args->pos.vy;
    coord->coord.t[2] = args->pos.vz;
    coord->rot.vx     = args->rot.vx;
    coord->rot.vy     = args->rot.vy;
    coord->rot.vz     = args->rot.vz;
    RotMatrix(&coord->rot, &coord->coord);
    coord->flg = 0;
    return 0;
}

/// Message-0x7D5 handler: the four-way visibility/mode switch on the message's
/// mode word, run against the `TmdObject` parked in `Task::extra`. Mode 0 shows
/// the model (`field_C` bit 0x80) and clears the 4 flag, 1 hides it, frees the
/// aux buffers and clears the flag, 2 does both plus latching the mode into the
/// work block's `field_480`, and 3 hides it while setting the flag. Anything
/// else returns 1 and leaves the object alone; the handled modes return 0.
/// The handler reads `work` before the switch even though mode 2 is its only
/// use, so retail's `lw $v1,0x1C($a0)` sits in the entry block. The same body
/// shape as `func_actor_141000_80133E8C` / `func_actor_503500_80132584`.
s32 func_actor_511000_801327A0(GpActorWork* arg0, s32 arg1, s32 mode)
{
    TmdObject*        obj;
    Actor511000Work2* work;
    s32               ret;

    obj  = arg0->extra;
    work = (Actor511000Work2*)arg0->actor;
    ret  = 0;

    switch (mode) {
        case 0:
            obj->flags |= 0x80;
            obj->flags &= ~4;
            break;
        case 1:
            obj->flags &= ~0x80;
            Tmd_AllocBuffers(obj);
            obj->flags &= ~4;
            break;
        case 2:
            obj->flags          |= 0x80;
            work->field_480.word = mode;
            obj->flags          |= 4;
            break;
        case 3:
            obj->flags &= ~0x80;
            obj->flags |= 4;
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}

/// Message-0x7DB handler: un-hides the model its first child task carries in
/// `Task::extra` (`field_C` bit 0x80) for mode 1 and hides it for mode 0, then
/// hides the second child as well on the mode-1 path -- the same two tasks
/// `func_actor_511000_80132480` parked at `field_4C4` / `field_4C8`. Any other
/// mode leaves both alone.
/// The `default:` arm jumps straight to the shared `return 0` instead of
/// falling through the hide block: retail's single epilogue is only reached
/// that way, the hide block and the shared return merging into one block whose
/// first label sits on the value store.
s32 func_actor_511000_8013287C(GpActorWork* arg0, s32 arg1, Actor511000Msg* msg)
{
    Actor511000Work2* work;
    Task*             child;
    u16               mode;

    mode = msg->field_2;
    work = (Actor511000Work2*)arg0->actor;

    switch (mode) {
        case 0:
            child = work->field_4C4;
            break;
        case 1:
            child = work->field_4C4;
            if (child != NULL) {
                ((TmdObject*)child->extra)->flags |= 0x80;
            }
            child = work->field_4C8;
            break;
        default:
            goto out;
    }

    if (child != NULL) {
        ((TmdObject*)child->extra)->flags &= 0xFF7F;
    }
out:
    return 0;
}

/// Message-0x7E0 handler: uploads one of the actor's three texture records
/// over the 0x18x0x10 rect at y 0x28 -- `D_actor_511000_801472B4` for mode 1,
/// `D_actor_511000_80146C74` for modes 0 and 2, and `D_actor_511000_80146F94`
/// for mode 3, which sets the work block's `field_4D0` / `field_4CC` to 1
/// first. Any other mode leaves the image NULL and returns 0.
/// The mode-1 case is written first because the compiler lays the case bodies
/// out in source order and that is the order the retail image has them in.
s32 func_actor_511000_80132904(GpActorWork* arg0, s32 arg1, s32 mode)
{
    RECT      rect;
    GpImgRec* img;
    s32       ret;

    ret    = 0;
    rect.x = 0;
    rect.y = 0x28;
    rect.w = 0x18;
    rect.h = 0x10;

    switch (mode) {
        case 1:
            img = &D_actor_511000_801472B4;
            break;
        case 0:
        case 2:
            img = &D_actor_511000_80146C74;
            break;
        case 3:
            ((Actor511000Work2*)arg0->actor)->field_4D0 = 1;
            ((Actor511000Work2*)arg0->actor)->field_4CC = 1;
            img                                         = &D_actor_511000_80146F94;
            break;
        default:
            img = NULL;
            break;
    }

    if (img != NULL) {
        ret = Gp_LoadActorImage(arg0, img, &rect);
    }
    return ret;
}

/// Per-frame tick for a child of the spawner: mirrors the parent model's
/// visibility bit (`field_C` 0x80) onto its own model, and once the session
/// reaches mode 0x18 poses its root coordinate from the parent's
/// `killCountdown` entry in `D_actor_511000_80147AC4`. Within three steps of
/// countdown 0x59 it also picks that distance's colour from
/// `D_actor_511000_80149004`, darkened by 0x1E per channel, and hands both to
/// `func_actor_511000_80132B14`.
/// The table is loaded into its own local before indexing: `&table[d]` on the
/// symbol directly shifts `d` ahead of the `lui`/`addiu` pair.
void func_actor_511000_801329C4(Task* task)
{
    Task*             parent;
    TmdObject*        extra;
    Actor511000Coord* coord;
    CVECTOR*          col;
    CVECTOR*          tbl;
    s32               d;
    s8                rgb[3];

    parent = (Task*)task->spawnArg2;
    extra  = (TmdObject*)task->extra;
    coord  = (Actor511000Coord*)extra->coords;

    if (!(((TmdObject*)parent->extra)->flags & 0x80)) {
        extra->flags &= 0xFF7F;
    } else {
        extra->flags |= 0x80;
    }

    if (gGameSession->at4.loc.view == 0x18) {
        coord->rot.vx = D_actor_511000_80147AC4[parent->killCountdown].vx;
        coord->rot.vy = D_actor_511000_80147AC4[parent->killCountdown].vy;
        coord->rot.vz = D_actor_511000_80147AC4[parent->killCountdown].vz;
        RotMatrix(&coord->rot, &coord->coord);
        coord->flg = 0;

        d = parent->killCountdown - 0x59;
        if (d < 0) {
            d = 0x59 - parent->killCountdown;
        }
        if (d < 4) {
            tbl    = D_actor_511000_80149004;
            col    = &tbl[d];
            rgb[0] = col->r - 0x1E;
            rgb[1] = col->g - 0x1E;
            rgb[2] = col->b - 0x1E;
            func_actor_511000_80132B14(task, col, rgb);
        }
    }
}

/// Draws a semi-transparent gradient disc at the model's root: projects the
/// parent-composed origin, scales the 16 unit offsets in
/// `D_actor_511000_80149014` by 0x12C/0x1000 around it, and fans 16 `POLY_G3`
/// from the centre (`col`) to the rim (`rgb`) into one OT slot, followed by an
/// additive draw-mode `DR_TPAGE`. Both `pts` and the offset table walk by
/// pointer and `scale` is a variable, which is what keeps the `mult` and the
/// retail induction-variable order.
void func_actor_511000_80132B14(Task* task, CVECTOR* col, s8* rgb)
{
    SVECTOR   pos;
    DVECTOR   pts[16];
    MATRIX    mtx;
    s32       sxy;
    s32       p;
    s32       flag;
    s32       otz;
    POLY_G3*  prim;
    DR_TPAGE* dr;
    u16       x;
    u16       y;
    s32       scale;
    u32*      ot;
    s32       i;
    DVECTOR*  pt;
    DVECTOR*  src;

    Gp_ComposeParentWorld(((TmdObject*)task->extra)->coords, &mtx, &pos);
    SetRotMatrix(&Gfx_ViewWorldMtx);
    SetTransMatrix(&Gfx_ViewWorldMtx);
    otz   = RotTransPers(&pos, &sxy, (long*)&p, (long*)&flag);
    x     = sxy;
    y     = sxy >> 16;
    src   = D_actor_511000_80149014;
    pt    = pts;
    scale = 0x12C;
    for (i = 0; i < 16; i++) {
        pt->vx = x + scale * src->vx / 0x1000;
        pt->vy = y + scale * src->vy / 0x1000;
        pt++;
        src++;
    }
    ot = (u32*)((u32)gGpuCurrentOt + (((u32)(otz << gDisplayState.otDepthShift) >> 2) & 0xFFC)) - 30;
    pt = pts;
    for (i = 0; i < 15; i++, pt++) {
        prim           = (POLY_G3*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyG3(prim);
        setSemiTrans(prim, 1);
        prim->x0 = x;
        prim->y0 = y;
        prim->x1 = pt->vx;
        prim->y1 = pt->vy;
        prim->x2 = pt[1].vx;
        prim->y2 = pt[1].vy;
        prim->r0 = col->r;
        prim->g0 = col->g;
        prim->b0 = col->b;
        prim->r1 = rgb[0];
        prim->g1 = rgb[1];
        prim->b1 = rgb[2];
        prim->r2 = rgb[0];
        prim->g2 = rgb[1];
        prim->b2 = rgb[2];
        addPrim(ot, prim);
    }
    prim           = (POLY_G3*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setPolyG3(prim);
    setSemiTrans(prim, 1);
    prim->x0 = x;
    prim->y0 = y;
    prim->x1 = pt->vx;
    prim->y1 = pt->vy;
    prim->x2 = pts[0].vx;
    prim->y2 = pts[0].vy;
    prim->r0 = col->r;
    prim->g0 = col->g;
    prim->b0 = col->b;
    prim->r1 = rgb[0];
    prim->g1 = rgb[1];
    prim->b1 = rgb[2];
    prim->r2 = rgb[0];
    prim->g2 = rgb[1];
    prim->b2 = rgb[2];
    addPrim(ot, prim);
    dr             = gGpuPrimCursor;
    gGpuPrimCursor = dr + 1;
    setDrawTPage(dr, 1, 0, 0x2A);
    addPrim(ot, dr);
}

/// Palette fade: steps `field_2C` up by 0x555 per frame while the
/// `field_2E` hold counter is live (counting it down once the blend saturates
/// at 0x1000), otherwise snaps it back to 0 and re-arms the hold at 0x1E. Each
/// of the 16 little-endian 15-bit colours is then blended between
/// `D_actor_511000_80147E84` and `D_actor_511000_80147EC4` by that weight into
/// the `field_C` CLUT, which `D_actor_511000_80147EA4` uploads.
/// The destination is formed as `work + i` before the field offset so the
/// `addu` keeps the index first and CSE cannot fold the 0xC into a store.
void func_actor_511000_80132E6C(Actor511000Work* work)
{
    CVECTOR col[3];
    s32     i;
    s32     c;
    s32     inv;
    s32     fade;
    u8*     src0;
    u8*     src1;
    u8*     dst;

    if (work->field_2E != 0) {
        work->field_2C += 0x555;
        i               = 0;
        if (work->field_2C >= 0x1000) {
            work->field_2C = 0x1000;
            if (--work->field_2E < 0) {
                work->field_2E = 0;
            }
        }
    } else {
        work->field_2C -= 0x1000;
        i               = 0;
        if (work->field_2C <= 0) {
            work->field_2C = 0;
            work->field_2E = 0x1E;
        }
    }
    inv  = 0x1000 - work->field_2C;
    fade = work->field_2C;
    do {
        dst      = (u8*)(i + (s32)work);
        dst      = ((Actor511000Work*)dst)->field_C;
        src0     = &D_actor_511000_80147E84[i];
        src1     = &D_actor_511000_80147EC4[i];
        c        = src0[0] | (src0[1] << 8);
        col[0].r = ((u16)c >> 10) & 0x1F;
        col[0].g = ((u16)c >> 5) & 0x1F;
        col[0].b = c & 0x1F;
        c        = src1[0] | (src1[1] << 8);
        col[1].r = ((u16)c >> 10) & 0x1F;
        col[1].g = ((u16)c >> 5) & 0x1F;
        col[1].b = c & 0x1F;
        LoadAverageCol(&col[0], &col[1], inv, fade, &col[2]);
        c      = col[2].b + ((col[2].r << 10) + (col[2].g << 5));
        dst[1] = (u32)c >> 8;
        i     += 2;
        dst[0] = c;
    } while (i < 0x20);
    Gp_LoadImages(&D_actor_511000_80147EA4);
}

/// Spawn/setup state: allocates the 0x70 work block, parks it in `work`,
/// arms the buffer-free countdown at -1, un-hides the model (`field_C` bit
/// 0x80), places it at rot/trans index 0, binds light/color, installs the
/// message table, and publishes `work->field_C` through
/// `D_actor_511000_80147EB0` before advancing to the per-frame state.
void func_actor_511000_80133034(Task* task)
{
    Actor511000Work* work;
    TmdObject*       extra;

    extra = (TmdObject*)task->extra;
    work  = (Actor511000Work*)memCalloc(0x70, 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work    = (TaskIdMap*)work;
    work->field_8 = -1;
    extra->flags |= 0x80;
    func_actor_511000_801336E0(task, D_actor_511000_80147344, D_actor_511000_80147704, 0);
    func_actor_511000_801337F0(task);
    do {
        task->msgTable          = D_actor_511000_80148FC4;
        D_actor_511000_80147EB0 = (u16*)work->field_C;
    } while (0);
    task->state += 1;
}

/// Per-frame state: while the model is hidden (`field_C` bit 0x80 clear) it
/// refreshes the root coordinate, rebuilds the colour matrix from that
/// coordinate's own translation, and runs the work block's follow-up. Once the
/// session reaches mode 0x18 it walks `killCountdown` up to 0x77, spawning a
/// view task for the camera record at each index and re-posing the model from
/// the matching rotations, and finally runs the `Tmd_FreeBuffers` countdown the
/// spawn state armed at -1, freeing the buffers and latching the field back to
/// -1 on the frame the countdown reaches zero.
void func_actor_511000_801330F0(Task* task)
{
    Actor511000Work* work;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    s32              countdown;
    s16              frame;

    obj   = (TmdObject*)task->extra;
    work  = (Actor511000Work*)task->work;
    coord = obj->coords;

    if (!(obj->flags & 0x80)) {
        Gp_UpdateCoord(coord);
        func_800D7A9C(obj, (VECTOR*)coord->workm.t, 0, 3);
        func_actor_511000_80132E6C((Actor511000Work*)task->work);
    }
    if (gGameSession->at4.loc.view == 0x18) {
        frame               = task->killCountdown + 1;
        task->killCountdown = frame;
        if (frame >= 0x78) {
            task->killCountdown = 0x77;
        }
        Gp_TrySpawnViewTask((s32)&D_actor_511000_80147EE4[task->killCountdown]);
        func_actor_511000_801336E0(task, D_actor_511000_80147344, D_actor_511000_80147704, task->killCountdown);
        coord->flg = 0;
    }
    countdown = work->field_8;
    if (countdown >= 0) {
        if (countdown == 0) {
            Tmd_FreeBuffers(obj);
            countdown = work->field_8;
        }
        work->field_8 = countdown - 1;
    }
}

void func_actor_511000_80133220(Task* task)
{
    taskKill(task);
}

/// Inherits the parent model's light/color and visibility bit, chains this
/// actor's root coordinate under the parent's, places it at the spawnArg1
/// translation, and reparents the task.
void func_actor_511000_80133240(Task* task)
{
    Task*          parent;
    TmdObject*     extra;
    TmdObject*     parentExtra;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* dest;

    parent          = (Task*)task->spawnArg2;
    parentExtra     = (TmdObject*)parent->extra;
    extra           = (TmdObject*)task->extra;
    dest            = parentExtra->coords;
    extra->lightMtx = parentExtra->lightMtx;
    extra->colorMtx = parentExtra->colorMtx;
    extra->flags    = 0x80;
    coord           = extra->coords;
    if (!(parentExtra->flags & 0x80)) {
        extra->flags = 0;
    }
    func_actor_511000_80133760(task);
    coord->sub = dest;
    Task_Reparent(parent, task);
    task->state += 1;
}

/// Tracks the parent model's visibility bit every frame: while the parent model
/// is hidden (`field_C` bit 0x80 clear) this clears its own bit and, for
/// spawnArg1 1 or 2, spins the root coordinate's yaw (0x46) by 0x294 or its
/// pitch (0x44) by 0x3E8, wrapping each to 0x1000. The rotation matrix is then
/// rebuilt from the angles and the coordinate's `flg` cleared. With the parent
/// visible the rotation is left alone and the visibility bit is set instead.
void func_actor_511000_801332E4(Task* task)
{
    TmdObject*        extra;
    TmdObject*        parentExtra;
    Actor511000Coord* coord;

    extra       = (TmdObject*)task->extra;
    coord       = (Actor511000Coord*)extra->coords;
    parentExtra = (TmdObject*)((Task*)task->spawnArg2)->extra;

    if (!(parentExtra->flags & 0x80)) {
        extra->flags &= 0xFF7F;

        switch (task->spawnArg1) {
            case 1:
                coord->rot.vy = ((u16)coord->rot.vy + 0x294) & 0xFFF;
                break;
            case 2:
                coord->rot.vx = ((u16)coord->rot.vx + 0x3E8) & 0xFFF;
                break;
        }

        RotMatrix(&coord->rot, &coord->coord);
        coord->flg = 0;
        return;
    }
    extra->flags |= 0x80;
}

void func_actor_511000_801333A4(Task* task)
{
    taskKill(task);
}

/// Inherits the parent model's light/color and visibility bit, chains this
/// actor's root coordinate under the parent's, places it at the spawnArg1
/// translation, copies `D_actor_511000_80147AC4` onto the Euler angles,
/// rebuilds the rotation matrix, and reparents the task.
void func_actor_511000_801333C4(Task* task)
{
    Task*             parent;
    TmdObject*        extra;
    TmdObject*        parentExtra;
    Actor511000Coord* coord;
    GsCOORDINATE2*    dest;

    parent          = (Task*)task->spawnArg2;
    parentExtra     = (TmdObject*)parent->extra;
    extra           = (TmdObject*)task->extra;
    dest            = parentExtra->coords;
    extra->lightMtx = parentExtra->lightMtx;
    extra->colorMtx = parentExtra->colorMtx;
    extra->flags    = 0x80;
    coord           = (Actor511000Coord*)extra->coords;
    if (!(parentExtra->flags & 0x80)) {
        extra->flags = 0;
    }
    func_actor_511000_80133760(task);
    ((GsCOORDINATE2*)coord)->sub = dest;
    Task_Reparent(parent, task);
    coord->rot.vx = D_actor_511000_80147AC4[0].vx;
    coord->rot.vy = D_actor_511000_80147AC4[0].vy;
    coord->rot.vz = D_actor_511000_80147AC4[0].vz;
    RotMatrix(&coord->rot, &coord->coord);
    coord->flg   = 0;
    task->state += 1;
}

void func_actor_511000_80133498(Task* task)
{
    taskKill(task);
}

s32 func_actor_511000_801334B8(Task* arg0)
{
    arg0->killCountdown = 0;
    return 0;
}

/// Placement message handler that also shows the model: writes the payload's
/// translation and Euler angles into the root coordinate, rebuilds its
/// rotation, clears `flg` so the world matrix is recomputed and clears the
/// model's hidden bit 0x80.
s32 func_actor_511000_801334C4(Task* task, s32 arg1, ActorsShared801334c4Args* args, s32 arg3)
{
    Actor511000Coord* coord;
    TmdObject*        extra;

    extra             = (TmdObject*)task->extra;
    coord             = (Actor511000Coord*)extra->coords;
    coord->coord.t[0] = args->pos.vx;
    coord->coord.t[1] = args->pos.vy;
    coord->coord.t[2] = args->pos.vz;
    coord->rot.vx     = args->rot.vx;
    coord->rot.vy     = args->rot.vy;
    coord->rot.vz     = args->rot.vz;
    RotMatrix(&coord->rot, &coord->coord);
    coord->flg    = 0;
    extra->flags &= 0xFF7F;
    return 0;
}

s32 func_actor_511000_80133554(Task* task, s32 arg1, s32 msg)
{
    TmdObject*       obj;
    Actor511000Work* work;
    Task*            child;
    s32              ret;
    s32              i;

    obj  = (TmdObject*)task->extra;
    work = (Actor511000Work*)task->work;
    ret  = 0;
    switch (msg) {
        case 0:
            obj->flags |= 0x80;
            obj->flags &= ~4;
            break;
        case 1:
            obj->flags &= ~0x80;
            Tmd_AllocBuffers(obj);
            obj->flags &= ~4;
            break;
        case 2:
            obj->flags   |= 0x80;
            work->field_8 = msg;
            obj->flags   |= 4;
            break;
        case 3:
            obj->flags &= ~0x80;
            obj->flags |= 4;
            break;
        default:
            ret = 1;
            break;
    }
    if (msg == 1 && work->field_2F == 0) {
        for (i = 1; i < 4; i++) {
            child = Task_SpawnFromTable(&D_actor_511000_80139924, D_actor_511000_80149054[i - 1], i, (s32)task);
            if (child != NULL) {
                ((TmdObject*)child->extra)->flags &= ~0x84;
            }
        }
        work->field_2F = 1;
    }
    return ret;
}

/// Places the task's model at the indexed rotation and translation: copies
/// `rots[index]` onto the root coordinate's Euler angles, `trans[index]` into
/// its local translation, rebuilds the rotation matrix and marks the
/// coordinate dirty.
void func_actor_511000_801336E0(Task* task, SVECTOR* rots, SVECTOR* trans, s32 index)
{
    Actor511000Coord* coord;
    SVECTOR*          rot;
    SVECTOR*          pos;
    s32               off;

    off               = (index << 16) >> 13;
    rot               = (SVECTOR*)(off + (s32)rots);
    coord             = (Actor511000Coord*)((TmdObject*)task->extra)->coords;
    coord->rot.vx     = rot->vx;
    coord->rot.vy     = rot->vy;
    pos               = (SVECTOR*)(off + (s32)trans);
    coord->rot.vz     = rot->vz;
    coord->coord.t[0] = pos->vx;
    coord->coord.t[1] = pos->vy;
    coord->coord.t[2] = pos->vz;
    RotMatrix(&coord->rot, &coord->coord);
    coord->flg = 0;
}

/// Places the task's model at the indexed translation: copies
/// `D_actor_511000_80148FE4[spawnArg1]` into the root coordinate's local
/// translation, zeros the Euler angles, rebuilds the rotation matrix and
/// marks the coordinate dirty.
void func_actor_511000_80133760(Task* task)
{
    Actor511000Coord* coord;

    coord             = (Actor511000Coord*)((TmdObject*)task->extra)->coords;
    coord->coord.t[0] = D_actor_511000_80148FE4[task->spawnArg1].vx;
    coord->coord.t[1] = D_actor_511000_80148FE4[task->spawnArg1].vy;
    coord->coord.t[2] = D_actor_511000_80148FE4[task->spawnArg1].vz;
    coord->rot.vx     = 0;
    coord->rot.vy     = 0;
    coord->rot.vz     = 0;
    RotMatrix(&coord->rot, &coord->coord);
    coord->flg = 0;
}

/// Binds the task's TMD object to the work-block light/color matrices, clears
/// the root coordinate flag, and rebuilds lighting from the world translation.
void func_actor_511000_801337F0(Task* task)
{
    GsCOORDINATE2*   coord;
    Actor511000Work* work;
    TmdObject*       extra;

    work            = (Actor511000Work*)task->work;
    extra           = (TmdObject*)task->extra;
    coord           = extra->coords;
    extra->lightMtx = &work->light;
    extra->colorMtx = &work->color;
    coord->flg      = 0;
    Gp_UpdateCoord(coord);
    func_800D7A9C(extra, (VECTOR*)coord->workm.t, 0, 3);
}

void func_actor_511000_80133850(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_511000_80131E48;
    sp.funcs[task->state](task);
}

void func_actor_511000_801338A8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_511000_80131E54;
    sp.funcs[task->state](task);
}

void func_actor_511000_80133900(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_511000_80131E60;
    sp.funcs[task->state](task);
}

/// Spawn handler: allocates the 0x488-byte work block and parks it in
/// `Task::work`, binds the task's model to the block's matrices and
/// animation state, installs the message table, then spawns table entries 1
/// and 2 - tinting each child's model from the current area's record - and
/// entry 3, and advances to state 1. An allocation failure destroys the enemy.
void func_actor_511000_80133958(GpEnemy* enemy, Task* task)
{
    GpAreaKey              key;
    GpAreaKey*             sessionKey;
    u8                     areaByte0;
    u8                     areaByte3;
    GpAreaRec*             rec;
    TmdObject*             model;
    GsCOORDINATE2*         coord;
    Actor511000ParentWork* work;
    TaskDesc*              table;
    u32                    idx;
    GpEnemy*               spawned;
    GameSession*           session;

    model = task->extra;
    coord = model->coords;
    work  = memCalloc(0x488, 0);
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->work      = (TaskIdMap*)work;
    model->flags    = 0x80;
    model->lightMtx = &work->field_45C;
    model->colorMtx = &work->field_43C;
    func_800B3F84(&work->anim, &D_actor_511000_801550C0, model, work->field_30C, work->slots);
    work->field_47C = 0;
    task->msgTable  = &D_actor_511000_801550A0;
    coord->flg      = 0;

    table   = D_actor_511000_80155070;
    spawned = Gp_SpawnEnemyFromTable(table, 1, 0, enemy);
    session = gGameSession;
    /* Reusing `spawned` for the task, rather than a new local, keeps the
       spawn result's v0 preference off the byte copied into a0. */
    spawned    = (GpEnemy*)spawned->task;
    sessionKey = (GpAreaKey*)&session->at4.loc;
    idx        = enemy->placeKey;
    areaByte3  = sessionKey->stage;
    model      = ((Task*)spawned)->extra;
    key.stage  = areaByte3;
    key.area   = sessionKey->area;
    key.room   = sessionKey->room;
    areaByte0  = session->at4.loc.view;
    idx        = idx >> 12;
    key.view   = areaByte0;
    Gp_SyncAreaKeyIndex(&key);
    rec          = Gp_GetNestedAreaRec(&key);
    idx        <<= 4;
    idx         += (s32)rec->field_0;
    model->tpage = ((GpAreaPlace*)idx)->tpage;
    model->clut  = ((GpAreaPlace*)idx)->clut;
    if (model->buffer != NULL) {
        tmdProcessStream(model);
        tmdProcessStream(model);
    }
    spawned    = Gp_SpawnEnemyFromTable(table, 2, 0, enemy);
    session    = gGameSession;
    spawned    = (GpEnemy*)spawned->task;
    sessionKey = (GpAreaKey*)&session->at4.loc;
    idx        = enemy->placeKey;
    areaByte3  = sessionKey->stage;
    model      = ((Task*)spawned)->extra;
    key.stage  = areaByte3;
    key.area   = sessionKey->area;
    key.room   = sessionKey->room;
    areaByte0  = session->at4.loc.view;
    idx        = idx >> 12;
    key.view   = areaByte0;
    Gp_SyncAreaKeyIndex(&key);
    rec          = Gp_GetNestedAreaRec(&key);
    idx        <<= 4;
    idx         += (s32)rec->field_0;
    model->tpage = ((GpAreaPlace*)idx)->tpage;
    model->clut  = ((GpAreaPlace*)idx)->clut;
    if (model->buffer != NULL) {
        tmdProcessStream(model);
        tmdProcessStream(model);
    }
    Gp_SpawnEnemyFromTable(table, 3, 0, enemy);
    task->state = 1;
}

void func_actor_511000_80133B80(GpEnemy* enemy, Task* task)
{
    TmdObject*             extra;
    VECTOR*                pos;
    VECTOR*                out;
    Actor511000ParentWork* work;
    GsCOORDINATE2*         coords;
    GsCOORDINATE2*         coord;
    s32                    i;
    s32                    j;
    s32                    flag;

    extra                  = *(TmdObject**)&task->extra;
    *(u32*)G_SCRATCH_HEAD -= 0x20;
    work                   = (Actor511000ParentWork*)task->work;
    coords                 = extra->coords;
    coord                  = &coords[1];
    flag                   = work->field_47C;
    pos                    = (VECTOR*)*(u32*)G_SCRATCH_HEAD;
    if (flag != 0) {
        for (i = 1; i < 19; i++) {
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
    if (work->field_47C < 3) {
        coord->flg = 0;
        Gp_UpdateCoord(coord);
        pos->vx = coord->workm.t[0];
        pos->vy = coord->workm.t[1];
        pos->vz = coord->workm.t[2];
        Gp_UpdateActorColor(enemy, pos, 0, 0);
        pos->vx = coord->workm.t[0];
        pos->vy = coord->workm.t[1];
        pos->vz = coord->workm.t[2];
        out     = pos + 1;
        if (func_800EA1A8((VECTOR3*)pos, (VECTOR3*)out) != 0) {
            Gp_DrawEffGroundQuad((VECTOR3*)out, 0x400, Gp_State1C->groundShade);
        }
    } else {
        if ((s16)(work->field_480 % 3) == 0) {
            for (i = 0; i < 3; i++) {
                for (j = 0; j < 3; j++) {
                    work->field_43C.m[i][j] = (work->field_43C.m[i][j] * 15) >> 4;
                    work->field_45C.m[i][j] = (work->field_45C.m[i][j] * 15) >> 4;
                }
            }
        }
        if (work->field_480 > 96) {
            task->state = 2;
        }
    }
    work->field_480++;
    coords->flg             = 0;
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x20;
}

/// Runs the enemy's current state handler, copying the table onto the stack
/// before the call.
void func_actor_511000_80133D90(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_511000_80131E6C;
    sp.funcs[task->state](task->spawnArg2, task);
}

/// Copies the animation id from `preset` into the work block parked in
/// `task->work`, reseeds slots 1..0x12 through `Gp_AnimResetSlot`, and
/// clears `field_480`'s halfword.
s32 func_actor_511000_80133DEC(Task* task, s32 arg1, Actor511000AnimPreset* preset)
{
    Actor511000Work2* work;
    s32               i;

    work            = (Actor511000Work2*)task->work;
    work->field_47C = preset->field_4;
    i               = 1;
    do {
        Gp_AnimResetSlot((GpAnimCtx*)work, i, work->field_47C);
        i++;
    } while (i < 0x13);
    work->field_480.half = 0;
    return 0;
}

/// Placement message handler: builds the root coordinate's matrix from the
/// payload's Euler angles, drops its translation in and clears `flg` so the
/// world matrix is recomputed.
s32 func_actor_511000_80133E48(Task* task, s32 arg1, ActorsShared80132074Args* args)
{
    TmdObject*     ext   = task->extra;
    GsCOORDINATE2* coord = ext->coords;

    RotMatrix(&args->rot, &coord->coord);
    coord->coord.t[0] = args->pos.vx;
    coord->coord.t[1] = args->pos.vy;
    coord->coord.t[2] = args->pos.vz;
    coord->flg        = 0;
    return 0;
}

/// Visibility message handler: bit 0 of `arg2` shows the model (flags 0)
/// instead of hiding it (0x80); bit 1 also sets flag 0x4.
s32 func_actor_511000_80133EAC(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* obj;

    obj = (TmdObject*)task->extra;
    if (!(arg2 & 1)) {
        obj->flags = 0x80;
    } else {
        obj->flags = 0;
    }
    if (arg2 & 2) {
        obj         = (TmdObject*)task->extra;
        obj->flags |= 4;
    }
    return 0;
}

void func_actor_511000_80133EF4(Task* task)
{
    void (*fns[2])(void*, Task*) = { func_actor_511000_80133F48, func_actor_511000_80133F88 };

    fns[task->state](task->spawnArg2, task);
}

/// Spawn state of the model child attached to the spawner's part 8: chains
/// the root coordinate under that part, takes the spawner work block's light
/// and colour matrices, shows the model and advances to the tick state.
void func_actor_511000_80133F48(void* enemy, Task* task)
{
    Task*                  parent;
    TmdObject*             obj;
    Actor511000ParentWork* work;
    GsCOORDINATE2*         coord;
    GsCOORDINATE2*         parentCoords;

    parent       = task->parent;
    obj          = (TmdObject*)task->extra;
    parentCoords = ((TmdObject*)parent->extra)->coords;
    coord        = obj->coords;
    work         = (Actor511000ParentWork*)parent->work;

    coord->sub    = &parentCoords[8];
    obj->lightMtx = &work->field_45C;
    obj->flags    = 0;
    obj->colorMtx = &work->field_43C;
    task->state   = 1;
}

void func_actor_511000_80133F88(void* arg0, Task* arg1)
{
    ((TmdObject*)arg1->extra)->coords->flg = 0;
    Gp_UpdateCoord(((TmdObject*)arg1->extra)->coords);
}

void func_actor_511000_80133FC8(Task* task)
{
    void (*fns[2])(void*, Task*) = { func_actor_511000_8013401C, func_actor_511000_8013405C };

    fns[task->state](task->spawnArg2, task);
}

void func_actor_511000_8013401C(void* enemy, Task* task)
{
    Task*                  parent;
    TmdObject*             obj;
    Actor511000ParentWork* work;
    GsCOORDINATE2*         coord;
    GsCOORDINATE2*         parentCoords;

    parent       = task->parent;
    obj          = (TmdObject*)task->extra;
    parentCoords = ((TmdObject*)parent->extra)->coords;
    coord        = obj->coords;
    work         = (Actor511000ParentWork*)parent->work;

    coord->sub    = &parentCoords[3];
    obj->lightMtx = &work->field_45C;
    obj->flags    = 0;
    obj->colorMtx = &work->field_43C;
    task->state   = 1;
}

void func_actor_511000_8013405C(void* arg0, Task* arg1)
{
    ((TmdObject*)arg1->extra)->coords->flg = 0;
    Gp_UpdateCoord(((TmdObject*)arg1->extra)->coords);
}

void func_actor_511000_8013409C(Task* task)
{
    void (*fns[2])(void*, Task*) = { func_actor_511000_801340F0, func_actor_511000_80134130 };

    fns[task->state](task->spawnArg2, task);
}

void func_actor_511000_801340F0(void* enemy, Task* task)
{
    Task*                  parent;
    TmdObject*             obj;
    Actor511000ParentWork* work;
    GsCOORDINATE2*         coord;
    GsCOORDINATE2*         parentCoords;

    parent       = task->parent;
    obj          = (TmdObject*)task->extra;
    parentCoords = ((TmdObject*)parent->extra)->coords;
    coord        = obj->coords;
    work         = (Actor511000ParentWork*)parent->work;

    coord->sub    = &parentCoords[12];
    obj->lightMtx = &work->field_45C;
    obj->flags    = 0;
    obj->colorMtx = &work->field_43C;
    task->state   = 1;
}

void func_actor_511000_80134130(void* arg0, Task* arg1)
{
    ((TmdObject*)arg1->extra)->coords->flg = 0;
    Gp_UpdateCoord(((TmdObject*)arg1->extra)->coords);
}
