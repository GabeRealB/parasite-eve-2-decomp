#include "common.h"

#include "actors/actor_521100.h"
#include "actors/actors_shared_801326b4.h"
#include "actors/actors_shared_801366fc.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

extern u8 D_actor_521100_8016A358;
extern u8 D_actor_521100_8016A3A0;

extern GpEffArg      D_actor_521100_8016A3CC;
extern u16           D_actor_521100_8016A3D4;
extern Task*         D_actor_521100_8016A3E0;
extern Task*         D_actor_521100_8016A3E4;
extern GsCOORDINATE2 D_actor_521100_8016A3E8;

void func_800D7A9C(TmdObject* arg0, VECTOR* arg1, s32 arg2, s32 arg3);
void func_actor_521100_80135DDC(void* spawnArg2, Task* task);
void func_actor_521100_801360C4(void* spawnArg2, Task* task);
void func_actor_521100_80136290(void* arg0, Task* task);
void func_actor_521100_801368B0(Actor521100* arg0);
void func_actor_521100_80136680(void* arg0, Task* task);

s32 func_actor_521100_80135D10(Actor521100* arg0, s32 arg1, s32 arg2)
{
    Actor521100Obj2C* obj;
    Actor521100Work*  work;

    obj  = arg0->field_2C;
    work = arg0->field_1C;
    if (!(arg2 & 1)) {
        obj->field_C = 0x80;
    } else {
        obj->field_C = 0;
    }
    if (arg2 & 2) {
        obj->field_C |= 4;
    }
    work->field_692 = arg2;
    return 0;
}

s32 func_actor_521100_80135D58(Actor521100* arg0, s32 arg1, Actor521100Msg* arg2)
{
    Actor521100Work* work;

    work = arg0->field_1C;
    switch (arg2->field_2) {
        case 0:
            work->field_68C = 1;
            work->field_68E = 0;
            work->field_690 = 0;
            break;
        case 1:
            work->field_694 = arg2->field_2;
            break;
    }
    return 0;
}

s32 func_actor_521100_80135D9C(Actor521100* arg0)
{
    arg0->field_1C->field_6B0 = 1;
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    return 0;
}

s16 func_actor_521100_80135DC8(Actor521100* arg0)
{
    return arg0->field_1C->field_6B2;
}

void func_actor_521100_80135DDC(void* spawnArg2, Task* task)
{
    VECTOR           vec;
    Actor521100Work* mem;
    GpEnemy*         enemy;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;

    enemy                   = (GpEnemy*)spawnArg2;
    obj                     = task->extra;
    coord                   = obj->coords;
    mem                     = Mem_Calloc(0x4B4, 0);
    D_actor_521100_8016A3D8 = mem;
    task->work              = (TaskIdMap*)mem;
    if (mem == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback       = ActorsShared801366fc;
    coord->sub               = &Gfx_ViewCoord;
    enemy->field_4           = &coord->coord;
    enemy->field_48          = 0;
    enemy->node.field_5      = 0;
    enemy->node.field_4      = 1;
    obj->otOffset            = 1;
    obj->lightMtx            = (MATRIX*)D_actor_521100_8016A3D8;
    obj->colorMtx            = (MATRIX*)D_actor_521100_8016A3D8 + 1;
    vec.vx                   = coord->workm.t[0];
    vec.vy                   = coord->workm.t[1] - 0x320;
    ActorsShared801326b4Task = task;
    vec.vz                   = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    Gp_AnimInitCtx(&D_actor_521100_8016A3D8->anim, &D_actor_521100_8016A3A0, (GpAnimObj*)obj, D_actor_521100_8016A3D8->pad_34C);
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
/// `Actor521100_MoveForward`. The pause check the helper makes is why the step
/// is skipped while the game is frozen - `travel` still ticks down, so a
/// paused actor finishes its walk.
void func_actor_521100_80135F2C(Task* task)
{
    Actor521100Work* work;
    s16              animId;

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
        if (animId == 1 && work->travel != 0) {
            Actor521100_MoveForward(((TmdObject*)task->extra)->coords, 0x14);
            D_actor_521100_8016A3D8->travel = (u16)D_actor_521100_8016A3D8->travel - 1;
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
void func_actor_521100_801360C4(void* spawnArg2, Task* task)
{
    GsCOORDINATE2    sp10;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    Actor521100Work* work;
    s32              i;

    obj   = task->extra;
    coord = obj->coords;
    work  = (Actor521100Work*)task->work;
    sp10  = *coord;

    switch (work->field_484) {
        case 0:
            work->field_486 = 0;
            work->field_488 = 0x1000;
            Gfx_RotMatrixY(&coord->coord, (s16)work->yaw, 1);
            work->field_48C = coord->coord;
            work->field_484 = 1;
            break;

        case 1:
            func_actor_521100_801368B0((Actor521100*)task);
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
void func_actor_521100_80136290(void* arg0, Task* task)
{
    Actor521100Work* work;
    GsCOORDINATE2*   coord;
    void**           scratch;
    u8*              head;
    VECTOR*          block;

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
    ScaleMatrixL((MATRIX*)&work->pad_0[0x20], block);
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
    Actor521100* ctx;

    ctx = task->spawnArg2;
    if (!(task->state & 7)) {
        if (task->spawnArg1 == 0) {
            D_actor_521100_8016A3E8             = ctx->field_2C->field_8[1];
            D_actor_521100_8016A3E8.coord.t[2] += 0x32;
            Gp_LcgState                         = Gp_LcgState * 5 + 0x71357911;
            D_actor_521100_8016A3E8.coord.t[1] -= 0xFA + (s32)(((u32)Gp_LcgState >> 16) - 0x8000) * 0xC8 / 0x10000;
            D_actor_521100_8016A3E8.coord.t[0] -= 0x32;
        } else {
            D_actor_521100_8016A3E8             = ((Actor521100*)Game_GetPtrSlot(3))->field_2C->field_8[0];
            D_actor_521100_8016A3E8.coord.t[1] -= 0x384;
            D_actor_521100_8016A3E8.coord.t[0] += 0x2BC;
        }
        D_actor_521100_8016A3E8.flg = 0;
        Gp_UpdateCoord(&D_actor_521100_8016A3E8);
        D_actor_521100_8016A3CC.field_0 = &D_actor_521100_8016A3E8;
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
/// splat migrates this table into `func_actor_521100_80136604`'s own `.s`, so
/// there is no standalone rodata file to `INCLUDE_RODATA`; it is defined here.
/// The word of 0 after the three handlers is the `.align 3` pad before
/// `jtbl_actor_521100_80131E78` in the next unit; splat grouped it into this
/// symbol.
const Actor521100StateFuncTable3 D_actor_521100_80131E68 = { {
    func_actor_521100_80135DDC,
    func_actor_521100_80136680,
    func_actor_521100_801360C4,
} };
const s32                        D_actor_521100_80131E74 = 0;

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
    D_actor_521100_8016A3D8 = (Actor521100Work*)arg0->work;
    sp.table.funcs[arg0->state](arg0->spawnArg2, arg0);
}

void func_actor_521100_80136680(void* arg0, Task* task)
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
