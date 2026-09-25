#include "common.h"

#include <psyq/libgte.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "actors/actor.h"
#include "actors/actor_521100.h"
#include "actors/actors_shared_80132074.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

extern u8 D_80072729;

/// The attach coordinate's rotation as the scale-in step snapshots it, and the
/// cache the "walk to" placement opcode writes beside it: the heading it
/// applied to the root coordinate and the remaining distance, scaled by 20.
typedef struct Actor521100AttachRot {
    /* 0x00 */ MATRIX mat;
    /* 0x20 */ byte   pad_20[2];
    /* 0x22 */ u16    yaw;
    /* 0x24 */ byte   pad_24[2];
    /* 0x26 */ s16    travel;
} Actor521100AttachRot;
STATIC_ASSERT_SIZEOF(Actor521100AttachRot, 0x28);

/// Work block of the task `func_actor_521100_80136604` dispatches:
/// `memCalloc(0x4B4, 0)` in its spawn state `func_actor_521100_80135DDC`, kept
/// both in `Task::work` and in `D_actor_521100_8016A3D8`. It carries the
/// model's `light` / `color` matrices, its animation context and slots, the
/// step that selects the slot reseed, and the scale-in state the task's last
/// state runs. The nineteen slots are the ones `func_actor_521100_80136724`
/// ticks.
typedef struct Actor521100Work4B4 {
    /* 0x000 */ MATRIX               light;
    /* 0x020 */ MATRIX               color;
    /* 0x040 */ GpAnimCtx            anim;
    /* 0x054 */ GpAnimSlot           slots[0x13];
    /* 0x34C */ byte                 poses[0x130];
    /* 0x47C */ s16                  field_47C; // actor step: 1 and 2 select the body to run, which then advances it to 3
    /* 0x47E */ u16                  field_47E; // animation id currently playing
    /* 0x480 */ u16                  animId;    // animation id the slots are seeded with
    /* 0x482 */ s16                  field_482; // cleared when a step body is started
    /* 0x484 */ s16                  field_484; // scale-in step func_actor_521100_801360C4 switches on: 0 seeds, 1 shrinks, 2 is done
    /* 0x486 */ u16                  field_486; // frames the shrink has run, counted to 0xA and 0xF by the step-1 body
    /* 0x488 */ u16                  field_488; // scale the shrink applies, stepped down by 0x10 per frame from 0x1000
    /* 0x48A */ byte                 pad_48A[2];
    /* 0x48C */ Actor521100AttachRot field_48C;
} Actor521100Work4B4;
STATIC_ASSERT_SIZEOF(Actor521100Work4B4, 0x4B4);

extern Actor521100Work4B4* D_actor_521100_8016A3D8;

/// Stack copy `func_actor_521100_80136604` makes before the indirect call.
/// The copy itself moves only the 3 words of `D_actor_521100_80131E68`, but
/// the dispatcher's frame is 0x30 with `$ra` at 0x28, which needs 17-24 bytes
/// of locals. The trailing `u8`/`u8`/`u16` at 0x10 are written to 2, 9, 1;
/// `field_C` is unread. Same 20-byte table-plus-context shape as
/// `Actor210600DispatchCtx`.
typedef struct Actor521100DispatchCtx {
    /* 0x00 */ GpEnemyTaskFuncTable3 table;
    /* 0x0C */ s32                   field_C;
    /* 0x10 */ u8                    field_10;
    /* 0x11 */ u8                    field_11;
    /* 0x12 */ u16                   field_12;
} Actor521100DispatchCtx;
STATIC_ASSERT_SIZEOF(Actor521100DispatchCtx, 0x14);

extern u8       D_actor_521100_8016A358;
extern TaskDesc D_actor_521100_8016A388;
extern u8       D_actor_521100_8016A3A0;

extern GpEffArg D_actor_521100_8016A3CC;
extern u16      D_actor_521100_8016A3D4;

/// The task `func_actor_521100_80136604` runs, stored by its create state
/// `func_actor_521100_80135DDC`.
extern Task*         D_actor_521100_8016A3DC;
extern Task*         D_actor_521100_8016A3E0;
extern Task*         D_actor_521100_8016A3E4;
extern GsCOORDINATE2 D_actor_521100_8016A3E8;

void func_actor_521100_80135DDC(GpEnemy* spawnArg2, Task* task);
void func_actor_521100_80135F2C(Task* task);
void func_actor_521100_801360C4(GpEnemy* spawnArg2, Task* task);
void func_actor_521100_80136290(GpEnemy* arg0, Task* task);
void func_actor_521100_80136680(GpEnemy* arg0, Task* task);
void func_actor_521100_801366FC(Task* task);
void func_actor_521100_80136724(void);
void func_actor_521100_8013677C(void);
void func_actor_521100_80136820(void);
void func_actor_521100_801368B0(Task* task);

s32 func_actor_521100_80135D10(Task* arg0, s32 arg1, s32 arg2)
{
    TmdObject*       obj;
    Actor521100Work* work;

    obj  = arg0->extra;
    work = arg0->work;
    if (!(arg2 & 1)) {
        obj->flags = 0x80;
    } else {
        obj->flags = 0;
    }
    if (arg2 & 2) {
        obj->flags |= 4;
    }
    work->field_692 = arg2;
    return 0;
}

s32 func_actor_521100_80135D58(Task* arg0, s32 arg1, GpCmdArg* arg2)
{
    Actor521100Work* work;

    work = arg0->work;
    switch (arg2->command) {
        case 0:
            work->field_68C = 1;
            work->field_68E = 0;
            work->field_690 = 0;
            break;
        case 1:
            work->field_694 = arg2->command;
            break;
    }
    return 0;
}

s32 func_actor_521100_80135D9C(Task* arg0)
{
    ((Actor521100Work*)arg0->work)->field_6B0 = 1;
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    return 0;
}

s16 func_actor_521100_80135DC8(Task* arg0)
{
    return ((Actor521100Work*)arg0->work)->field_6B2;
}

void func_actor_521100_80135DDC(GpEnemy* spawnArg2, Task* task)
{
    VECTOR              vec;
    Actor521100Work4B4* mem;
    GpEnemy*            enemy;
    TmdObject*          obj;
    GsCOORDINATE2*      coord;

    enemy                   = spawnArg2;
    obj                     = task->extra;
    coord                   = obj->coords;
    mem                     = memCalloc(0x4B4, 0);
    D_actor_521100_8016A3D8 = mem;
    task->work              = (TaskIdMap*)mem;
    if (mem == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback      = func_actor_521100_801366FC;
    coord->sub              = &gGfxViewCoord;
    enemy->field_4          = &coord->coord;
    enemy->field_48         = 0;
    enemy->node.targeted    = 0;
    enemy->node.flags       = 1;
    obj->otOffset           = 1;
    obj->lightMtx           = &D_actor_521100_8016A3D8->light;
    obj->colorMtx           = &D_actor_521100_8016A3D8->color;
    vec.vx                  = coord->workm.t[0];
    vec.vy                  = coord->workm.t[1] - 0x320;
    D_actor_521100_8016A3DC = task;
    vec.vz                  = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    Gp_AnimInitCtx(&D_actor_521100_8016A3D8->anim, &D_actor_521100_8016A3A0, obj, D_actor_521100_8016A3D8->poses);
    D_actor_521100_8016A3D8->animId    = 1;
    D_actor_521100_8016A3D8->field_47C = 2;
    task->msgTable                     = &D_actor_521100_8016A358;
    func_actor_521100_80135F2C(task);
    task->state += 1;
}

/// The actor's step body, run every frame while `field_47C` is 3. The two
/// pending-animation states run their reseed body first and advance the step to
/// 3, which is why they share the tail that stores it.
///
/// Step 3 while the walk-to cache is armed (`animId` is the walk clip and
/// `travel` still has distance left) advances the attach coordinate one step:
/// 20 units along its local Z axis, the scale `travel` is counted in, through
/// `actorMoveForward`. The pause check the helper makes is why the step
/// is skipped while the game is frozen - `travel` still ticks down, so a
/// paused actor finishes its walk.
void func_actor_521100_80135F2C(Task* task)
{
    Actor521100Work4B4* work;
    s16                 animId;

    work = D_actor_521100_8016A3D8;
    if (work->field_47C == 1) {
        func_actor_521100_80136820();
        D_actor_521100_8016A3D8->field_47C = 3;
        return;
    }
    if (work->field_47C == 2) {
        func_actor_521100_8013677C();
        D_actor_521100_8016A3D8->field_47C = 3;
        return;
    }
    if (work->field_47C == 3) {
        animId = work->animId;
        if (animId == 1 && work->field_48C.travel != 0) {
            actorMoveForward(((TmdObject*)task->extra)->coords, 0x14);
            D_actor_521100_8016A3D8->field_48C.travel = (u16)D_actor_521100_8016A3D8->field_48C.travel - 1;
        }
        func_actor_521100_80136724();
        return;
    }
}
/// State-2 body, the actor's last: it snapshots the attach coordinate onto a
/// stack `GsCOORDINATE2` - the copy the shrink's effect is placed off - and
/// runs the scale-in step `field_484`. Step 0 seeds the shrink (the step-1
/// body `func_actor_521100_801368B0` scales by `field_488`, so the seed stores
/// 0x1000 there and snapshots the coordinate's rotation into `field_48C`),
/// step 1 runs that body and drops the 0x600A5 effect once the counter reaches
/// 0xF, and step 2 returns without animating. Every other step falls through
/// to the slot tick and the colour step.
void func_actor_521100_801360C4(GpEnemy* spawnArg2, Task* task)
{
    GsCOORDINATE2       sp10;
    TmdObject*          obj;
    GsCOORDINATE2*      coord;
    Actor521100Work4B4* work;
    s32                 i;

    obj   = task->extra;
    coord = obj->coords;
    work  = (Actor521100Work4B4*)task->work;
    sp10  = *coord;

    switch (work->field_484) {
        case 0:
            work->field_486 = 0;
            work->field_488 = 0x1000;
            Gfx_RotMatrixY(&coord->coord, (s16)work->field_48C.yaw, 1);
            work->field_48C.mat = coord->coord;
            work->field_484     = 1;
            break;

        case 1:
            func_actor_521100_801368B0(task);
            work->field_486++;
            if ((s16)work->field_486 == 0xA) {
                obj->flags = 2;
            }
            if ((s16)work->field_486 == 0xF) {
                sp10.coord.t[0] -= 0x1F4;
                sp10.coord.t[2] -= 0x64;
                Gp_SpawnEff(0x600A5, &sp10, 5, NULL);
            }
            break;

        case 2:
            return;
    }

    i = 1;
    do {
        Gp_AnimTickSlot(&D_actor_521100_8016A3D8->anim, &D_actor_521100_8016A3D8->slots[i]);
        i++;
    } while (i < 0x13);

    func_actor_521100_80136290(spawnArg2, task);
}
/// The scale-in's colour step: takes a 0x10-byte `VECTOR` off `G_SCRATCH_HEAD`,
/// fills it with the world position of the model's *second* attach coordinate
/// (the one the shrink is scaling) and hands it to `Gp_UpdateActorColor` as the
/// colour target. The same draw then overwrites the three components with
/// `field_488` scaled by the top half of three successive `Gp_LcgState` draws,
/// and `ScaleMatrixL` multiplies the work block's second matrix by it.
///
/// Each draw reads `Gp_LcgState` back from the global: the initialiser's store
/// is what the next draw's shift sees, and it is why one `lw` feeds all three
/// and each draw's value gets its own register.
void func_actor_521100_80136290(GpEnemy* arg0, Task* task)
{
    Actor521100Work4B4* work;
    GsCOORDINATE2*      coord;
    void**              scratch;
    u8*                 head;
    VECTOR*             block;

    coord    = &((TmdObject*)task->extra)->coords[1];
    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    block    = (VECTOR*)(head - 0x10);
    *scratch = block;
    Gp_UpdateCoord(coord);
    block->vx = coord->workm.t[0];
    block->vy = coord->workm.t[1];
    block->vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0, block, 0, 0);
    work        = D_actor_521100_8016A3D8;
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    block->vx   = (s16)work->field_488 * (s32)(((u32)Gp_LcgState >> 16) + 0x8000) / 0x10000;
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    block->vy   = (s16)work->field_488 * (s32)(((u32)Gp_LcgState >> 16) + 0x8000) / 0x10000;
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    block->vz   = (s16)work->field_488 * (s32)(((u32)Gp_LcgState >> 16) + 0x8000) / 0x10000;
    ScaleMatrixL(&work->color, block);
    *scratch = (u8*)*scratch + 0x10;
}
/// Companion task body: the two tasks the `0x7DB` handler
/// `func_actor_521100_80136AE0` spawns out of the `D_actor_521100_8016A388`
/// table, which differ only in `Task::spawnArg1` and in the slot they are kept
/// in (`D_actor_521100_8016A3E0` for 0, `D_actor_521100_8016A3E4` for 1).
///
/// Every 8th frame of `Task::state` it re-anchors the global effect coordinate
/// on one of the two models' attach coordinates: with `spawnArg1` 0 the actor's
/// own second coordinate, raised 0x32, pushed back 0x32 and given a random
/// vertical jitter of `(LCG top half - 0x8000) * 200 / 0x10000` (so within
/// +/-100); with 1 the player's (slot 3) first coordinate, moved by a fixed
/// (0x2BC, -0x384). The anchor is then cleared, updated and handed to the
/// effect spawner `func_800FDB18` through the `GpEffArg` record beside it.
///
/// `Task::state` is the frame counter as well as the run gate - it advances
/// every frame and the body stops re-anchoring once it reaches 0x83, killing
/// the task and clearing whichever slot holds it.
///
/// The 0x32 pair is adjusted before the `Gp_LcgState` draw, not after: that is
/// the source order that lets the draw's store sink below both halfword-field
/// loads in `sched2`, which is what puts them on $a3 rather than $a0.
void func_actor_521100_80136404(Task* task)
{
    Task* ctx;

    ctx = task->spawnArg2;
    if (!(task->state & 7)) {
        if (task->spawnArg1 == 0) {
            D_actor_521100_8016A3E8             = ((TmdObject*)ctx->extra)->coords[1];
            D_actor_521100_8016A3E8.coord.t[2] += 0x32;
            Gp_LcgState                         = Gp_LcgState * 5 + 0x71357911;
            D_actor_521100_8016A3E8.coord.t[1] -= 0xFA + (s32)(((u32)Gp_LcgState >> 16) - 0x8000) * 0xC8 / 0x10000;
            D_actor_521100_8016A3E8.coord.t[0] -= 0x32;
        } else {
            D_actor_521100_8016A3E8             = ((TmdObject*)gameGetPtrSlot(3)->extra)->coords[0];
            D_actor_521100_8016A3E8.coord.t[1] -= 0x384;
            D_actor_521100_8016A3E8.coord.t[0] += 0x2BC;
        }
        D_actor_521100_8016A3E8.flg = 0;
        Gp_UpdateCoord(&D_actor_521100_8016A3E8);
        D_actor_521100_8016A3CC.coord = &D_actor_521100_8016A3E8;
        func_800FDB18(D_actor_521100_8016A3D4, &D_actor_521100_8016A3E8, NULL, &D_actor_521100_8016A3CC);
    }
    if (task->state >= 0x83) {
        taskKill(task);
        if (task->spawnArg1 == 0) {
            D_actor_521100_8016A3E0 = NULL;
        } else {
            D_actor_521100_8016A3E4 = NULL;
        }
    }
    task->state++;
}
/// State table the overlay dispatches through, indexed by `Task::state`:
/// create, update and teardown.
const GpEnemyTaskFuncTable3 D_actor_521100_80131E68 = { {
    func_actor_521100_80135DDC,
    func_actor_521100_80136680,
    func_actor_521100_801360C4,
} };

/// State dispatcher: copies the overlay's 3-entry state table onto a 20-byte
/// stack record, fills the trailing context bytes, caches the work pointer,
/// and calls the entry `Task::state` selects.
void func_actor_521100_80136604(Task* arg0)
{
    Actor521100DispatchCtx sp;

    sp.table                = D_actor_521100_80131E68;
    sp.field_10             = 2;
    sp.field_11             = 9;
    sp.field_12             = 1;
    D_actor_521100_8016A3D8 = (Actor521100Work4B4*)arg0->work;
    sp.table.funcs[arg0->state](arg0->spawnArg2, arg0);
}

void func_actor_521100_80136680(GpEnemy* arg0, Task* task)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    VECTOR         vec;

    obj   = task->extra;
    coord = obj->coords;
    Gp_UpdateCoord(coord);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1] - 0x320;
    vec.vz = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_actor_521100_80135F2C(task);
}

/// `Task::exitCallback` the create state `func_actor_521100_80135DDC`
/// installs: hands the task's `GpEnemy` back to `Gp_DestroyEnemy`.
void func_actor_521100_801366FC(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}

/// Ticks animation slots 1..0x12 of the actor's animation context.
void func_actor_521100_80136724(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickSlot(&D_actor_521100_8016A3D8->anim, &D_actor_521100_8016A3D8->slots[i]);
        i++;
    } while (i < 0x13);
}

/// Re-inits animation slots 1..0x12 from `animId`, forcing each slot's set
/// index to 1 first, and latches that id into `field_47E` as the one now
/// playing.
void func_actor_521100_8013677C(void)
{
    s32 i;

    i = 1;
    do {
        D_actor_521100_8016A3D8->slots[i].rate = 1;
        Gp_AnimInitSlot(&D_actor_521100_8016A3D8->anim, &D_actor_521100_8016A3D8->slots[i], i,
                        (s16)D_actor_521100_8016A3D8->animId);
        i++;
    } while (i < 0x13);
    D_actor_521100_8016A3D8->field_47E = D_actor_521100_8016A3D8->animId;
}

/// Reseeds animation slots 1..0x12 from `animId` and latches that id into
/// `field_47E` as the one now playing.
void func_actor_521100_80136820(void)
{
    s32 i;

    i = 1;
    do {
        func_800B3AA4(&D_actor_521100_8016A3D8->anim, &D_actor_521100_8016A3D8->slots[i], i,
                      (s16)D_actor_521100_8016A3D8->animId, 0, 8);
        i++;
    } while (i < 0x13);
    D_actor_521100_8016A3D8->field_47E = D_actor_521100_8016A3D8->animId;
}

/// Scale-in step body, run while `field_484` is 1: takes a 0x30-byte scratch
/// from `G_SCRATCH_HEAD`, splats an identity rotation into it and hands it to
/// `ScaleMatrix` with a `(0x1000, field_488, 0x1000)` vector, then multiplies
/// the product into the attach coordinate whose rotation step 0 snapshotted
/// into `field_48C`. The scale drops 0x10 a frame; under 0x101 the step
/// advances to 2 and this body stops running.
///
/// The scratch pointer is taken with a chained assignment on purpose: the
/// store and the callee-saved copy are what put the extra `move $s0, $v0`
/// between the `addiu` and the `sw` (and the `nop` in the load's delay slot).
void func_actor_521100_801368B0(Task* task)
{
    MATRIX*             head;
    ActorScaleScratch*  scratch;
    Actor521100Work4B4* work;
    GsCOORDINATE2*      coord;

    head    = *(MATRIX**)0x1F8003FC;
    work    = task->work;
    scratch = (*(void**)0x1F8003FC = (ActorScaleScratch*)((u8*)head - 0x30));
    coord   = ((TmdObject*)task->extra)->coords;
    if ((s16)work->field_488 >= 0x101) {
        work->field_488 = (u16)work->field_488 - 0x10;
    } else {
        work->field_484 = 2;
    }
    scratch->scale.vx          = 0x1000;
    scratch->scale.vy          = (s32)(s16)work->field_488;
    scratch->scale.vz          = 0x1000;
    coord->coord               = work->field_48C.mat;
    scratch->mat.ident.m00_m01 = 0x1000;
    scratch->mat.ident.m02_m10 = 0;
    scratch->mat.ident.m11_m12 = 0x1000;
    scratch->mat.ident.m20_m21 = 0;
    scratch->mat.ident.m22     = 0x1000;
    ScaleMatrix(&scratch->mat.mat, &scratch->scale);
    MulMatrix(&coord->coord, &scratch->mat.mat);
    coord->flg         = 0;
    *(u8**)0x1F8003FC += 0x30;
}
/// Animation-start handler: seeds the work block's `animId` with
/// `args->field_4 + 1`, rejecting anything whose incremented id is 0xB or up,
/// and leaves the actor in step 2 with `field_482` cleared before running the
/// step dispatcher.
///
/// The published task is read into a local between the first two stores on
/// purpose: that is where the original evaluates it, and it is what puts the
/// global's `lui`/`lw` ahead of the `li 2` and leaves the `field_482` clear
/// for the call's delay slot.
s32 func_actor_521100_801369B8(Task* task, s32 arg1, GpAnimArg* args)
{
    Task* dispatcher;

    if (args->field_4 + 1 < 0xB) {
        D_actor_521100_8016A3D8->animId    = (u16)args->field_4 + 1;
        dispatcher                         = D_actor_521100_8016A3DC;
        D_actor_521100_8016A3D8->field_47C = 2;
        D_actor_521100_8016A3D8->field_482 = 0;
        func_actor_521100_80135F2C(dispatcher);
        return 0;
    }
    return -1;
}

/// Message 0x7D5 handler in `D_actor_521100_8016A358`: shows or hides the model of `D_actor_521100_8016A3DC`.
/// Bit 0 of `arg2` selects `TmdObject::flags` 0 (shown) or 0x80 (hidden), and
/// bit 1 ORs in 0x4.
s32 func_actor_521100_80136A1C(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* obj;

    obj = (TmdObject*)D_actor_521100_8016A3DC->extra;
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

/// Message 0x7D4 handler in `D_actor_521100_8016A358`, placing the actor: only
/// the yaw of the argument block's angles is used, cached in the work block's
/// `field_48C.yaw` and applied with `Gfx_RotMatrixY`, then the position becomes
/// the root coordinate's translation and `flg` is cleared.
s32 func_actor_521100_80136A64(Task* task, s32 arg1, GpXformArg* placement)
{
    GsCOORDINATE2* coord;
    u16            yaw;

    coord                                  = ((TmdObject*)task->extra)->coords;
    D_actor_521100_8016A3D8->field_48C.yaw = yaw = placement->rot.vy;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->flg        = 0;
    return 0;
}

/// Message 0x7DB handler, listed in `D_actor_521100_8016A358` -- the
/// `{id, handler}` table the create body `func_actor_521100_80135DDC` installs
/// at `Task::msgTable`. `msg->field_2` picks the sub-command: 0 puts the task
/// back on its update state; 1 and 4 spawn one of the two companion tasks out
/// of the `D_actor_521100_8016A388` desc table into `D_actor_521100_8016A3E0` /
/// `D_actor_521100_8016A3E4`, leaving the state alone; 2 clears the scale-in
/// step (`Actor521100Work4B4::field_484`) and sends the task to state 2, the
/// teardown entry `func_actor_521100_801360C4`; 3 kills both companions and
/// then falls into 0, sharing its state store.
s32 func_actor_521100_80136AE0(Task* task, s32 arg1, GpCmdArg* msg)
{
    switch (msg->command) {
        case 1:
            D_actor_521100_8016A3E0 = Task_SpawnFromTable(&D_actor_521100_8016A388, 1, 0, (s32)task);
            break;

        case 2:
            D_actor_521100_8016A3D8->field_484 = 0;
            task->state                        = 2;
            break;

        case 3:
            if (D_actor_521100_8016A3E0 != NULL) {
                taskKill(D_actor_521100_8016A3E0);
                D_actor_521100_8016A3E0 = NULL;
            }
            if (D_actor_521100_8016A3E4 != NULL) {
                taskKill(D_actor_521100_8016A3E4);
                D_actor_521100_8016A3E4 = NULL;
            }
            /* fall through -- the jump table's index 0 lands on the same store */

        case 0:
            task->state = 1;
            break;

        case 4:
            D_actor_521100_8016A3E4 = Task_SpawnFromTable(&D_actor_521100_8016A388, 1, 1, (s32)task);
            break;
    }
    return 0;
}
s32 func_actor_521100_80136BE8(Task* task, s32 arg1, GpXformArg* target)
{
    GsCOORDINATE2* coord;
    s32            dx;
    s32            dz;
    u16            yaw;

    coord                                  = ((TmdObject*)task->extra)->coords;
    dx                                     = target->pos.vx - coord->coord.t[0];
    dz                                     = target->pos.vz - coord->coord.t[2];
    yaw                                    = ratan2(dx, dz);
    D_actor_521100_8016A3D8->field_48C.yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    D_actor_521100_8016A3D8->field_48C.travel = SquareRoot0(dx * dx + dz * dz) / 20;
    return 0;
}
