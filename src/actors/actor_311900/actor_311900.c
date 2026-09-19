#include "common.h"

#include "actors/actor_311900.h"
#include "gameplay/1BC.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

/// The animation data `func_800B3F84` builds the work block's clip context
/// from; the spawn hands it over whole, so it is only ever a byte address here.
extern u8 D_actor_311900_8016EBE8[];

void func_actor_311900_8016228C(GpEnemy* enemy, Task* task);
void func_actor_311900_801623B0(GpEnemy* enemy, Task* task);

void func_actor_311900_80161E3C(Task* task, s32 arg1, s16 arg2);
void func_actor_311900_80162100(Task* task);

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

extern u16 D_actor_311900_8016EC18[][0x100];

/// Fades the two 256-entry CLUT rows `arg2` / `arg2 + 1` of the palette table
/// to grey, one step per call in the work block's `field_4C8`: step 0 reads the
/// VRAM rows `arg1 + 0xF5` / `arg1 + 0xF6` back into the table, step 1 sets each
/// entry's three 5-bit channels to their maximum (keeping the STP bit set), and
/// step 2 uploads the rows again, leaving 3.
void func_actor_311900_80161E3C(Task* task, s32 arg1, s16 arg2)
{
    RECT             rect;
    Actor311900Work* work;
    s32              i;
    u16              r;
    u16              g;
    u16              b;

    work = (Actor311900Work*)task->work;
    if (work->field_4C8 == 0) {
        rect.x = 0;
        rect.y = arg1 + 0xF5;
        rect.w = 0x100;
        rect.h = 1;
        StoreImage2(&rect, (u_long*)D_actor_311900_8016EC18[arg2]);
        rect.x = 0;
        rect.y = arg1 + 0xF6;
        rect.w = 0x100;
        rect.h = 1;
        StoreImage2(&rect, (u_long*)D_actor_311900_8016EC18[arg2 + 1]);
        work->field_4C8 = 1;
    } else if (work->field_4C8 == 1) {
        for (i = 0; i < 0x100; i++) {
            r = D_actor_311900_8016EC18[arg2][i] & 0x1F;
            g = (D_actor_311900_8016EC18[arg2][i] >> 5) & 0x1F;
            b = (D_actor_311900_8016EC18[arg2][i] >> 10) & 0x1F;
            if (r < g) {
                r = g;
            } else {
                g = r;
            }
            if (g < b) {
                g = b;
            } else {
                b = g;
            }
            if (b < r) {
                b = r;
            } else {
                r = b;
            }
            D_actor_311900_8016EC18[arg2][i] = r | (g << 5) | (b << 10) | 0x8000;
        }
        for (i = 0; i < 0x100; i++) {
            r = D_actor_311900_8016EC18[arg2 + 1][i] & 0x1F;
            g = (D_actor_311900_8016EC18[arg2 + 1][i] >> 5) & 0x1F;
            b = (D_actor_311900_8016EC18[arg2 + 1][i] >> 10) & 0x1F;
            if (r < g) {
                r = g;
            } else {
                g = r;
            }
            if (g < b) {
                g = b;
            } else {
                b = g;
            }
            if (b < r) {
                b = r;
            } else {
                r = b;
            }
            D_actor_311900_8016EC18[arg2 + 1][i] = r | (g << 5) | (b << 10) | 0x8000;
        }
        work->field_4C8 = 2;
    } else if (work->field_4C8 == 2) {
        rect.x = 0;
        rect.y = arg1 + 0xF5;
        rect.w = 0x100;
        rect.h = 1;
        LoadImage2(&rect, (u_long*)D_actor_311900_8016EC18[arg2]);
        rect.x = 0;
        rect.y = arg1 + 0xF6;
        rect.w = 0x100;
        rect.h = 1;
        LoadImage2(&rect, (u_long*)D_actor_311900_8016EC18[arg2 + 1]);
        work->field_4C8 = 3;
    }
}

/// Applies the animation request in the work block's `field_474` to slots 1..19
/// of its context, which is where the block itself begins. Step 1 seeks every
/// slot to the id in `field_478` through `func_800B4114`, step 2 resets them to
/// it; each first marks the slot reset-pending with the rate byte at `field_47C`,
/// and both then latch that id as the one now playing in `field_476`, settle on
/// step 3 and clear the frame counter at `field_47A`. Step 3 only ticks the slots
/// and counts frames.
///
/// The two advances are one block in the ROM: jump.c cross-jumps them because
/// both branches name the same local. Step 3 reads the block again into an alias
/// of its own -- keeping `start` dead before `work` there is what leaves the
/// slot walk on the `work` register cse2 picks for it.
void func_actor_311900_80162100(Task* task)
{
    Actor311900Work* work;
    Actor311900Work* start;
    Actor311900Work* tick;
    s32              i;
    s32              j;
    s32              k;

    work = (Actor311900Work*)task->work;
    if (work->field_474 == 1) {
        start = (Actor311900Work*)task->work;
        for (i = 1; i < 0x14; i++) {
            start->anim.slots[i].field_9 = start->field_47C;
            func_800B4114(&start->anim.context, i, (s16)start->field_478, 0, 0);
        }
        start->field_476 = start->field_478;
        work->field_474  = 3;
        work->field_47A  = 0;
        return;
    }
    if (work->field_474 == 2) {
        start = (Actor311900Work*)task->work;
        for (j = 1; j < 0x14; j++) {
            start->anim.slots[j].field_9 = start->field_47C;
            Gp_AnimResetSlot(&start->anim.context, j, (s16)start->field_478);
        }
        start->field_476 = start->field_478;
        work->field_474  = 3;
        work->field_47A  = 0;
        return;
    }
    if (work->field_474 == 3) {
        work->field_47A++;
        tick = (Actor311900Work*)task->work;
        for (k = 1; k < 0x14; k++) {
            Gp_AnimTickIndex(&tick->anim.context, k);
        }
    }
}

INCLUDE_RODATA("actors/nonmatchings/actor_311900/actor_311900", D_actor_311900_80161E20);

/// The actor's three state handlers - spawn, per-frame tick and teardown -
/// dispatched through by state. splat migrates the table into the `.s` of the
/// function that reads it, so it is written out here to keep the block in the
/// unit's `.rodata` now that `func_actor_311900_8016222C` is decompiled.
const GpEnemyTaskFuncTable3 D_actor_311900_80161E24 = {
    func_actor_311900_8016228C,
    func_actor_311900_801623B0,
    Gp_DestroyEnemy,
};

/// Runs the actor's state handler that `Task::state` selects. Copies the
/// table onto the stack first, the same local jump table `Gp_EnemyDispatch`
/// builds for the shared `Gp_EnemyWaitFuncs`, so the call goes through the
/// stack copy rather than the overlay's own `.rodata`.
void func_actor_311900_8016222C(Task* task)
{
    GpEnemy*              enemy;
    GpEnemyTaskFuncTable3 sp;

    enemy = task->spawnArg2;
    sp    = D_actor_311900_80161E24;
    sp.funcs[task->state](enemy, task);
}

INCLUDE_RODATA("actors/nonmatchings/actor_311900/actor_311900", ActorsShared80135df4Table);

/// The actor's first setup path, reached through `D_actor_311900_80161E24`. It
/// tears the enemy down instead while game flag 0xA's nibble 2 -- the bit
/// `func_actor_311900_801623B0` raises once the view reaches 0xA -- is already
/// up, or when the 0x4CC-byte work block cannot be allocated into
/// `Task::work` (that slot is not a `TaskIdMap` here).
///
/// Otherwise it splats the light / colour pair `func_actor_311900_8016278C`
/// writes onto the model root's `field_1C` / `field_20` slots, points
/// `GpEnemy::field_4` at the root coordinate's matrix, re-parents that root to
/// `gGfxViewCoord`, builds the animation context `func_800B3F84` over the
/// block's slot array and packed-pose run, seeds the tick's two work halfwords
/// 0x474 / 0x478 and zeroes the 0x4C4 / 0x4C6 pair it counts in, and publishes
/// the view-dependent light level exactly as the tick does.
void func_actor_311900_8016228C(GpEnemy* enemy, Task* task)
{
    Actor311900Work* work;
    GsCOORDINATE2*   coord;
    TmdObject*       obj;

    obj   = (TmdObject*)task->extra;
    coord = obj->coords;
    if ((GameFlag_GetNibble(0xA) & 2) ||
        (work = Mem_Calloc(0x4CC, 0), task->work = (TaskIdMap*)work, work == NULL)) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    func_actor_311900_8016278C(task);
    enemy->field_4  = &coord->coord;
    enemy->field_48 = 0;
    obj->flags      = 0;
    func_800B3F84((GpAnimCtx*)work, D_actor_311900_8016EBE8, (GpAnimObj*)obj, work->anim.poses,
                  work->anim.slots);
    coord->sub      = &gGfxViewCoord;
    work->field_474 = 2;
    work->field_478 = 1;
    work->field_4C4 = 0;
    work->field_4C6 = 0;
    if ((Gp_GetViewIndex() & 0xFF) == 0xA) {
        obj->flags = 0;
    } else {
        obj->flags = 0x80;
    }
    func_actor_311900_80162100(task);
    task->state += 1;
}

/// The actor's per-frame tick. Publishes the view-dependent light level into
/// `TmdObject::flags` (0 at view 0xA, 0x80 otherwise), and while the work
/// block's `field_4C6` latch is up, counts frames in `field_4C4` and nudges the
/// model along the coordinate part `func_actor_311900_80162658` walks. The
/// counter reaching 0x5A raises game flag 0x102 and advances the state.
void func_actor_311900_801623B0(GpEnemy* enemy, Task* task)
{
    Actor311900Work* work;
    GsCOORDINATE2*   coord;
    TmdObject*       obj;

    obj   = task->extra;
    work  = (Actor311900Work*)task->work;
    coord = obj->coords;
    func_actor_311900_80161E3C(task, 2, 0);
    if ((Gp_GetViewIndex() & 0xFF) == 0xA) {
        GameFlag_SetNibble(0xA, GameFlag_GetNibble(0xA) | 2);
        obj->flags      = 0;
        work->field_4C6 = 1;
    } else {
        obj->flags = 0x80;
    }
    if ((s16)work->field_4C6 == 1) {
        work->field_4C4++;
        func_actor_311900_80162658(coord, 0x24);
    }
    func_actor_311900_80162100(task);
    if ((s16)work->field_4C4 >= 0x5A) {
        GameFlag_SetNibble(0x102, 1);
        task->state++;
    }
}
