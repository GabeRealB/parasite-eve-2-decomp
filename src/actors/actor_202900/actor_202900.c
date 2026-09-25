#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include "gameplay/areaplace.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

/// Per-actor work block, allocated at 0x564 bytes by the setup handler
/// `func_actor_202900_80149E24` and reached through the global
/// `D_actor_202900_80156E54`, which the actor's update
/// `func_actor_202900_8014A02C` refreshes from the task every frame.
///
/// It opens with the light and colour matrices the actor's model is drawn
/// under, then the animation context with one slot and one 0x10-byte pose
/// record for each of the nineteen parts `func_actor_202900_8014A208` ticks.
/// Only the fields the functions reach are reconstructed after that.
typedef struct Actor202900Work {
    /* 0x000 */ MATRIX     light;
    /* 0x020 */ MATRIX     color;
    /* 0x040 */ GpAnimCtx  anim;
    /* 0x054 */ GpAnimSlot slots[0x13];
    /* 0x34C */ byte       poses[0x13][0x10];
    /* 0x47C */ s16        field_47C; // actor step: 1 and 2 select the body to run, which then advances it to 3
    /* 0x47E */ u16        field_47E; // animation id currently playing
    /* 0x480 */ u16        animId;    // animation id the slots are seeded with
    /* 0x482 */ s16        field_482; // cleared when a step body is started
    /* 0x484 */ s16        field_484; // frame the second slot last held when it was 0x15, kept for change detection
} Actor202900Work;

/// Argument block of the animation-start message `func_actor_202900_8014A3E0`
/// handles: which animation to start.
typedef struct Actor202900AnimArgs {
    /* 0x0 */ byte pad_0[4];
    /* 0x4 */ s32  animId;
} Actor202900AnimArgs;

extern u8       D_actor_202900_80156E0C[];
extern TaskDesc D_actor_202900_80156E24[];
extern u8       D_actor_202900_80156E3C[];

/// The actor's work block, published so the overlay's functions can reach it
/// without the task in hand.
extern Actor202900Work* D_actor_202900_80156E54;

/// The actor's task, published by the setup handler so the overlay's other
/// functions can reach the actor's model without the task in hand.
extern GpActorWork* D_actor_202900_80156E58;

/// The second task the setup handler starts. Its model is textured from the
/// area record the actor was placed from, shown and hidden together with the
/// actor's, and the task is killed when the actor's exit callback runs.
extern Task* D_actor_202900_80156E5C;

void func_actor_202900_8014A0B4(GpEnemy* enemy, Task* task);
void func_actor_202900_8014A158(Task* arg0);
void func_actor_202900_8014A194(GpActorWork* arg0);
void func_actor_202900_8014A208(void);
void func_actor_202900_8014A260(void);
void func_actor_202900_8014A304(void);
s32  func_actor_202900_8014A394(void);

/// Setup handler, state 0 of the actor's update: allocates and publishes the
/// work block, starts the second task and textures its model from the area
/// record the actor was placed from, then seeds the animation context and runs
/// the first step body.
void func_actor_202900_80149E24(GpEnemy* enemy, Task* task)
{
    VECTOR         vec;
    GpAreaKey      key;
    GsCOORDINATE2* coord;
    TmdObject*     obj;
    TmdObject*     model;
    Task*          spawned;
    GpAreaRec*     rec;
    GpAreaPlace*   place;
    GpAreaKey*     sessionKey;
    GpAreaKey*     keyPtr;
    s32            idx;
    u32            raw;
    u8             areaByte0;

    obj        = task->extra;
    coord      = obj->coords;
    task->work = (TaskIdMap*)(D_actor_202900_80156E54 = memCalloc(0x564, false));
    if (D_actor_202900_80156E54 == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback      = func_actor_202900_8014A158;
    coord->sub              = &gGfxViewCoord;
    enemy->field_4          = &coord->coord;
    enemy->field_48         = 0;
    enemy->node.targeted    = 0;
    enemy->node.flags       = 1;
    obj->otOffset           = 1;
    obj->flags              = 0;
    D_actor_202900_80156E58 = (GpActorWork*)task;
    spawned                 = Task_SpawnFromTable(D_actor_202900_80156E24, 1, 0, 0);
    D_actor_202900_80156E5C = spawned;
    sessionKey              = (GpAreaKey*)&gGameSession->at4.loc;
    raw                     = enemy->placeKey;
    model                   = spawned->extra;
    key.stage               = sessionKey->stage;
    key.area                = sessionKey->area;
    key.room                = sessionKey->room;
    areaByte0               = sessionKey->view;
    idx                     = raw >> 12;
    SOFT_BARRIER();
    keyPtr = &key;
    TOUCH_REG(keyPtr);
    key.view = areaByte0;
    Gp_SyncAreaKeyIndex(keyPtr);
    rec          = Gp_GetNestedAreaRec(&key);
    place        = (GpAreaPlace*)((idx << 4) + (s32)rec->field_0);
    model->tpage = place->tpage;
    model->clut  = place->clut;
    if (model->buffer != NULL) {
        tmdProcessStream(model);
        tmdProcessStream(model);
    }
    obj->lightMtx = &D_actor_202900_80156E54->light;
    obj->colorMtx = &D_actor_202900_80156E54->color;
    vec.vx        = coord->workm.t[0];
    vec.vy        = coord->workm.t[1] - 0x320;
    vec.vz        = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    Gp_AnimInitCtx(&D_actor_202900_80156E54->anim, D_actor_202900_80156E3C, obj,
                   D_actor_202900_80156E54->poses);
    D_actor_202900_80156E54->animId    = 4;
    D_actor_202900_80156E54->field_47C = 2;
    D_actor_202900_80156E54->field_484 = 0;
    task->msgTable                     = D_actor_202900_80156E0C;
    func_actor_202900_8014A194((GpActorWork*)task);
    task->state++;
}

/// Update of the actor's task: publishes the task's work block in
/// `D_actor_202900_80156E54`, so the overlay's other functions can reach it
/// without the task in hand, then dispatches on the task's state to the setup
/// handler `func_actor_202900_80149E24` (state 0) or the per-frame handler
/// `func_actor_202900_8014A0B4` (state 1), passing the task's enemy record
/// along with the task.
void func_actor_202900_8014A02C(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_202900_80149E24,
        func_actor_202900_8014A0B4,
    };

    D_actor_202900_80156E54 = (Actor202900Work*)task->work;
    fns[task->state](task->spawnArg2, task);
}

/// Update of the second task: parents its model to the fifth coordinate of the
/// actor's model, marks the coordinate for recomputation and shows the model.
void func_actor_202900_8014A088(GpActorWork* arg0)
{
    GsCOORDINATE2* parent;
    GsCOORDINATE2* coord;
    TmdObject*     extra;

    extra        = arg0->extra;
    parent       = D_actor_202900_80156E58->extra->coords;
    coord        = extra->coords;
    coord->flg   = 0;
    extra->flags = 0;
    coord->sub   = parent + 4;
}

/// Per-frame handler, state 1 of the actor's update: passes the model and a
/// point 0x320 above its origin to `func_800D7A9C`, runs the step dispatcher,
/// and while animation 1 plays enqueues a sound event each time the second
/// animation slot reaches frame 0x15.
void func_actor_202900_8014A0B4(GpEnemy* enemy, Task* task)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    VECTOR         pos;

    obj    = (TmdObject*)task->extra;
    coord  = obj->coords;
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1] - 0x320;
    pos.vz = coord->workm.t[2];
    func_800D7A9C(obj, &pos, 0, 3);
    func_actor_202900_8014A194((GpActorWork*)task);
    if ((s16)D_actor_202900_80156E54->animId == 1 && (func_actor_202900_8014A394() & 0xFF)) {
        SndEvt_EnqueueType6(0x5104000D, 0, 0);
    }
}

/// Exit callback: kills the second task and destroys the enemy.
void func_actor_202900_8014A158(Task* arg0)
{
    taskKill(D_actor_202900_80156E5C);
    Gp_DestroyEnemy(arg0->spawnArg2, arg0);
}

/// Runs the body the actor's step selects and then leaves it in step 3, the
/// running state. Steps 1 and 2 each return through their own copy of the
/// advance; the two are identical, so jump.c cross-jumps them and only the
/// second survives. `arg0` is handed the actor but the body ignores it: it
/// reaches the work block through the global, like the overlay's other
/// functions.
void func_actor_202900_8014A194(GpActorWork* arg0)
{
    if (D_actor_202900_80156E54->field_47C == 1) {
        func_actor_202900_8014A304();
        D_actor_202900_80156E54->field_47C = 3;
        return;
    }
    if (D_actor_202900_80156E54->field_47C == 2) {
        func_actor_202900_8014A260();
        D_actor_202900_80156E54->field_47C = 3;
        return;
    }
    if (D_actor_202900_80156E54->field_47C == 3) {
        func_actor_202900_8014A208();
    }
}

/// Ticks animation slots 1..0x12 of the actor's animation context.
void func_actor_202900_8014A208(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickSlot(&D_actor_202900_80156E54->anim, &D_actor_202900_80156E54->slots[i]);
        i++;
    } while (i < 0x13);
}

/// Reseeds animation slots 1..0x12 from `animId`, forcing each slot's set
/// index to 1 first, and latches that id into `field_47E` as the one now
/// playing.
///
/// The third argument is the loop counter itself, and the two scaled induction variables are the
/// compiler's own, not a pair of source level pointers.
void func_actor_202900_8014A260(void)
{
    s32 i;

    i = 1;
    do {
        D_actor_202900_80156E54->slots[i].rate = 1;
        Gp_AnimInitSlot(&D_actor_202900_80156E54->anim, &D_actor_202900_80156E54->slots[i], i,
                        (s16)D_actor_202900_80156E54->animId);
        i++;
    } while (i < 0x13);
    D_actor_202900_80156E54->field_47E = D_actor_202900_80156E54->animId;
}

/// Reseeds animation slots 1..0x12 from `animId` and latches that id into
/// `field_47E` as the one now playing.
///
/// The third argument is the loop counter itself. Giving the call its own
/// counter copy (as m2c does) makes the preheader's `a2` initialisation a
/// separate pseudo, and the scheduler then orders the prologue saves around it
/// instead of leaving each `sw` paired with the load that overwrites it.
void func_actor_202900_8014A304(void)
{
    s32 i;

    i = 1;
    do {
        func_800B3AA4(&D_actor_202900_80156E54->anim, &D_actor_202900_80156E54->slots[i], i,
                      (s16)D_actor_202900_80156E54->animId, 0, 8);
        i++;
    } while (i < 0x13);
    D_actor_202900_80156E54->field_47E = D_actor_202900_80156E54->animId;
}

/// Watches the second animation slot for the frame the overlay reacts to:
/// while it holds 0x15, records it in `field_484` and reports whether that is
/// a change.
///
/// The mask is written at each use rather than hoisted into a `u16` local.
/// Hoisting makes the local a copy of the masked word, and combine then folds
/// the compare's zero-extension into a `move`; masking where the value is read
/// keeps the `andi $a1,$a0,0xffff`.
s32 func_actor_202900_8014A394(void)
{
    u16 frame;

    frame = D_actor_202900_80156E54->slots[1].curRec;
    if ((frame & 0x3FF) == 0x15) {
        if (D_actor_202900_80156E54->field_484 != (frame & 0x3FF)) {
            D_actor_202900_80156E54->field_484 = frame & 0x3FF;
            return 1;
        }
        D_actor_202900_80156E54->field_484 = frame & 0x3FF;
    }
    return 0;
}

/// Message 0x7D3 handler, animation start: seeds the work block's `animId` with the requested
/// one, rejecting anything from 5 up, and leaves the actor in step 2 with
/// `field_482` cleared before running the step dispatcher.
///
/// The actor is read into a local between the first two stores on purpose: that
/// is where the original evaluates it, and it is what puts the global's
/// `lui`/`lw` ahead of the `li 2` and leaves the `field_482` clear for the
/// call's delay slot.
s32 func_actor_202900_8014A3E0(Task* task, s32 arg1, Actor202900AnimArgs* args)
{
    GpActorWork* actor;

    if (args->animId < 5) {
        D_actor_202900_80156E54->animId    = args->animId;
        actor                              = D_actor_202900_80156E58;
        D_actor_202900_80156E54->field_47C = 2;
        D_actor_202900_80156E54->field_482 = 0;
        func_actor_202900_8014A194(actor);
        return 0;
    }
    return -1;
}

/// Message 0x7D5 handler: applies the draw-state flags to the actor's model and
/// to the second task's model alike. Bit 0 set shows both, clear hides them
/// (flag 0x80); bit 1 also sets flag 0x4 on both.
///
/// The handler is declared with the message arguments it does not read so the
/// flags arrive in `$a2` as they do for every other handler: with a single
/// parameter the compiler copies the incoming `$a0` into the pseudo global
/// allocation gave `$a2`, one instruction the target does not have.
s32 func_actor_202900_8014A440(Task* task, s32 arg1, s32 flags)
{
    TmdObject* actorModel;
    TmdObject* taskModel;

    actorModel = D_actor_202900_80156E58->extra;
    taskModel  = (TmdObject*)D_actor_202900_80156E5C->extra;
    if (flags & 1) {
        actorModel->flags = 0;
        taskModel->flags  = 0;
    } else {
        actorModel->flags = 0x80;
        taskModel->flags  = 0x80;
    }
    if (flags & 2) {
        actorModel->flags |= 4;
        taskModel->flags  |= 4;
    }
    return 0;
}
