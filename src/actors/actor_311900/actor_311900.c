#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/animation.h"
#include "gameplay/enemy.h"
#include "gameplay/loading.h"
#include "gameplay/scene_runtime.h"

#include "main/coord.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session_types.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "overlay.h"

/// Work block allocated by the spawn state `func_actor_311900_8016228C`
/// (`memCalloc(0x4CC)`) and parked in that task's `Task::work` slot -- that
/// slot is not a `TaskIdMap` here. `func_actor_311900_8016278C` republishes the
/// two matrices onto `TmdObject::lightMtx` / `colorMtx`, the light / colour pair
/// `Gp_BindDefaultMtx` otherwise points at `Gp_DefaultMtx` / `Gp_DefaultMtx2`.
///
/// The size is the allocation, and the fields below are the ones the spawn
/// state seeds: 2 into the halfword at 0x474, 1 into the one at 0x478, and
/// zero into 0x4C4 / 0x4C6. `func_actor_311900_80162100` turns that pair into
/// the animation request `field_474` and the two ids beside it: `field_478` is
/// the id the slots are seeded with, `field_476` latches it as the one now
/// playing once the slots have been seeded, `field_47C` is the rate byte every
/// seeding writes into `AnimationSlot.rate`, and `field_47A` counts frames
/// while `field_474` is 3 -- the running step, which is where both seeding
/// steps leave it.
typedef struct Actor311900Work {
    /* 0x000 */ ActorAnimRig20 rig;
    /* 0x474 */ s16            field_474;
    /* 0x476 */ s16            field_476;
    /* 0x478 */ u16            field_478;
    /* 0x47A */ u16            field_47A;
    /* 0x47C */ u8             field_47C;
    /* 0x47D */ byte           pad_47D[0x7];
    /* 0x484 */ MATRIX         light;
    /* 0x4A4 */ MATRIX         color;
    /* 0x4C4 */ u16            field_4C4;
    /* 0x4C6 */ u16            field_4C6;
    /* 0x4C8 */ u8             field_4C8; ///< CLUT grey-fade step, func_actor_311900_80161E3C
    /* 0x4C9 */ byte           pad_4C9[0x3];
} Actor311900Work;
STATIC_ASSERT_SIZEOF(Actor311900Work, 0x4CC);

/// The animation data `func_800B3F84` builds the first setup path's clip
/// context from; the spawn hands it over whole, so it is only ever a byte
/// address here.
extern u8 D_actor_311900_8016EBE8[];

/// The animation data the second setup path builds its clip context from.
extern u8 D_actor_311900_8016EBF4[];

/// The palette rows `func_actor_311900_80161E3C` reads back, greys and uploads.
extern u16 D_actor_311900_8016EC18[][0x100];

static void func_actor_311900_8016228C(Enemy* enemy, Task* task);
static void func_actor_311900_801623B0(Enemy* enemy, Task* task);
static void func_actor_311900_801624F8(Enemy* enemy, Task* task);
static void func_actor_311900_801625F0(Enemy* enemy, Task* task);
static s32  func_actor_311900_80162658(GfxCoord* arg0, s16 arg1);
static void func_actor_311900_8016278C(Task* task);
static void func_actor_311900_8016281C(Task* task);

extern TmdSource D_actor_311900_80168784;
extern TmdSource D_actor_311900_8016DF4C;
void             func_actor_311900_8016222C(Task*);
void             func_actor_311900_8016249C(Task*);

TmdBone D_actor_311900_801628B0[20] = {
#include "assets/actor_311900_model_06964_skeleton.inc"
};

u32 D_actor_311900_80162B80[20] = {
#include "assets/actor_311900_model_06964_partVerts.inc"
};

SVECTOR D_actor_311900_80162BD0[386] = {
#include "assets/actor_311900_model_06964_verts.inc"
};

SVECTOR D_actor_311900_801637E0[385] = {
#include "assets/actor_311900_model_06964_normals.inc"
};

u32 D_actor_311900_801643E8[4327] = {
#include "assets/actor_311900_model_06964_stream.inc"
};

TmdSource D_actor_311900_80168784 = {
    0,
    23980,
    6012,
    20,
    D_actor_311900_80162B80,
    D_actor_311900_80162BD0,
    D_actor_311900_801637E0,
    D_actor_311900_801628B0,
    D_actor_311900_801643E8,
};

TmdBone D_actor_311900_801687A8[20] = {
#include "assets/actor_311900_model_0C12C_skeleton.inc"
};

u32 D_actor_311900_80168A78[20] = {
#include "assets/actor_311900_model_0C12C_partVerts.inc"
};

SVECTOR D_actor_311900_80168AC8[360] = {
#include "assets/actor_311900_model_0C12C_verts.inc"
};

SVECTOR D_actor_311900_80169608[358] = {
#include "assets/actor_311900_model_0C12C_normals.inc"
};

u32 D_actor_311900_8016A138[3973] = {
#include "assets/actor_311900_model_0C12C_stream.inc"
};

TmdSource D_actor_311900_8016DF4C = {
    0,
    21176,
    6792,
    20,
    D_actor_311900_80168A78,
    D_actor_311900_80168AC8,
    D_actor_311900_80169608,
    D_actor_311900_801687A8,
    D_actor_311900_8016A138,
};

AnimationPackedPose D_actor_311900_8016DF70[11] = {
#include "assets/actor_311900_animation_0C9A8_bank1.inc"
};

AnimationPackedRotation D_actor_311900_8016DFF4[201] = {
#include "assets/actor_311900_animation_0C9A8_bank4.inc"
};

AnimationRecord D_actor_311900_8016E318[290] = {
#include "assets/actor_311900_animation_0C9A8_records.inc"
};

u16 D_actor_311900_8016E7A0[20] = {
#include "assets/actor_311900_animation_0C9A8_indices.inc"
};

AnimationSet D_actor_311900_8016E7C8 = {
    D_actor_311900_8016E318,
    D_actor_311900_8016E7A0,
    { NULL, D_actor_311900_8016DF70, NULL, NULL, D_actor_311900_8016DFF4, NULL, NULL, NULL },
};

AnimationPackedPose D_actor_311900_8016E7F0[6] = {
#include "assets/actor_311900_animation_0CDA0_bank1.inc"
};

AnimationPackedRotation D_actor_311900_8016E838[75] = {
#include "assets/actor_311900_animation_0CDA0_bank4.inc"
};

AnimationRecord D_actor_311900_8016E964[141] = {
#include "assets/actor_311900_animation_0CDA0_records.inc"
};

u16 D_actor_311900_8016EB98[20] = {
#include "assets/actor_311900_animation_0CDA0_indices.inc"
};

AnimationSet D_actor_311900_8016EBC0 = {
    D_actor_311900_8016E964,
    D_actor_311900_8016EB98,
    { NULL, D_actor_311900_8016E7F0, NULL, NULL, D_actor_311900_8016E838, NULL, NULL, NULL },
};

u8 D_actor_311900_8016EBE8[12] = {
    0,
    0,
    0,
    0,
    200,
    231,
    22,
    128,
    0,
    0,
    0,
    0,
};

u8 D_actor_311900_8016EBF4[12] = {
    0,
    0,
    0,
    0,
    192,
    235,
    22,
    128,
    0,
    0,
    0,
    0,
};

TaskDesc D_actor_311900_8016EC00 = { { { TASK_BODY_TMD, 96 } }, func_actor_311900_8016249C, { .model = &D_actor_311900_8016DF4C } };

TaskDesc D_actor_311900_8016EC0C = { { { TASK_BODY_TMD, 96 } }, func_actor_311900_8016222C, { .model = &D_actor_311900_80168784 } };

u16 D_actor_311900_8016EC18[4][256];

static void func_actor_311900_80161E3C(Task* task, s32 arg1, s16 arg2);
static void func_actor_311900_80162100(Task* task);

/// Fades the two 256-entry CLUT rows `arg2` / `arg2 + 1` of the palette table
/// to grey, one step per call in the work block's `field_4C8`: step 0 reads the
/// VRAM rows `arg1 + 0xF5` / `arg1 + 0xF6` back into the table, step 1 sets each
/// entry's three 5-bit channels to their maximum (keeping the STP bit set), and
/// step 2 uploads the rows again, leaving 3.
static void func_actor_311900_80161E3C(Task* task, s32 arg1, s16 arg2)
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
static void func_actor_311900_80162100(Task* task)
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
            start->rig.slots[i].rate = start->field_47C;
            func_800B4114(&start->rig.anim, i, (s16)start->field_478, 0, 0);
        }
        start->field_476 = start->field_478;
        work->field_474  = 3;
        work->field_47A  = 0;
        return;
    }
    if (work->field_474 == 2) {
        start = (Actor311900Work*)task->work;
        for (j = 1; j < 0x14; j++) {
            start->rig.slots[j].rate = start->field_47C;
            animationResetSlot(&start->rig.anim, j, (s16)start->field_478);
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
            animationTickSlot(&tick->rig.anim, k);
        }
    }
}

/// The actor's first state table - `func_actor_311900_8016228C`'s setup,
/// `func_actor_311900_801623B0`'s tick and teardown - dispatched through by
/// `func_actor_311900_8016222C`.
static const GpEnemyTaskFuncTable3 D_actor_311900_80161E24 = {
    func_actor_311900_8016228C,
    func_actor_311900_801623B0,
    Gp_DestroyEnemy,
};

/// The actor's second state table - `func_actor_311900_801624F8`'s setup,
/// `func_actor_311900_801625F0`'s tick and teardown - dispatched through by
/// `func_actor_311900_8016249C`.
static const GpEnemyTaskFuncTable3 D_actor_311900_80161E30 = {
    func_actor_311900_801624F8,
    func_actor_311900_801625F0,
    Gp_DestroyEnemy,
};

/// Runs the actor's state handler that `Task::state` selects. Copies the
/// table onto the stack first, the same local jump table `Gp_EnemyDispatch`
/// builds for the shared `Gp_EnemyWaitFuncs`, so the call goes through the
/// stack copy rather than the overlay's own `.rodata`.
void func_actor_311900_8016222C(Task* task)
{
    Enemy*                enemy;
    GpEnemyTaskFuncTable3 sp;

    enemy = task->spawnArg2.pointer;
    sp    = D_actor_311900_80161E24;
    sp.funcs[task->state](enemy, task);
}

/// The actor's first setup path, reached through `D_actor_311900_80161E24`. It
/// tears the enemy down instead while game flag 0xA's nibble 2 -- the bit
/// `func_actor_311900_801623B0` raises once the view reaches 0xA -- is already
/// up, or when the 0x4CC-byte work block cannot be allocated into
/// `Task::work` (that slot is not a `TaskIdMap` here).
///
/// Otherwise it splats the light / colour pair `func_actor_311900_8016278C`
/// writes onto the model root's `field_1C` / `field_20` slots, points
/// `Enemy::field_4` at the root coordinate's matrix, re-parents that root to
/// `gGfxViewCoord`, builds the animation context `func_800B3F84` over the
/// block's slot array and packed-pose run, seeds the tick's two work halfwords
/// 0x474 / 0x478 and zeroes the 0x4C4 / 0x4C6 pair it counts in, and publishes
/// the view-dependent light level exactly as the tick does.
static void func_actor_311900_8016228C(Enemy* enemy, Task* task)
{
    Actor311900Work* work;
    GfxCoord*        coord;
    TmdObject*       obj;

    obj   = task->extra.tmd;
    coord = obj->coords;
    if ((GameFlag_GetNibble(0xA) & 2) ||
        (work = memCalloc(0x4CC, 0), task->work = work, work == NULL)) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    func_actor_311900_8016278C(task);
    enemy->field_4  = &coord->coord;
    enemy->field_48 = 0;
    obj->flags      = 0;
    func_800B3F84(&work->rig.anim, D_actor_311900_8016EBE8, obj, work->rig.poses,
                  work->rig.slots);
    coord->parent   = &gGfxViewCoord;
    work->field_474 = 2;
    work->field_478 = 1;
    work->field_4C4 = 0;
    work->field_4C6 = 0;
    if ((Gp_GetViewIndex() & 0xFF) == 0xA) {
        obj->flags = 0;
    } else {
        obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    func_actor_311900_80162100(task);
    task->state += 1;
}

/// The actor's per-frame tick. Publishes the view-dependent light level into
/// `TmdObject::flags` (0 at view 0xA, 0x80 otherwise), and while the work
/// block's `field_4C6` latch is up, counts frames in `field_4C4` and nudges the
/// model along the coordinate part `func_actor_311900_80162658` walks. The
/// counter reaching 0x5A raises game flag 0x102 and advances the state.
static void func_actor_311900_801623B0(Enemy* enemy, Task* task)
{
    Actor311900Work* work;
    GfxCoord*        coord;
    TmdObject*       obj;

    obj   = task->extra.tmd;
    work  = (Actor311900Work*)task->work;
    coord = obj->coords;
    func_actor_311900_80161E3C(task, 2, 0);
    if ((Gp_GetViewIndex() & 0xFF) == 0xA) {
        GameFlag_SetNibble(0xA, GameFlag_GetNibble(0xA) | 2);
        obj->flags      = 0;
        work->field_4C6 = 1;
    } else {
        obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
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

/// Runs the actor's second state table - `func_actor_311900_801624F8`'s setup,
/// `func_actor_311900_801625F0`'s tick and `Gp_DestroyEnemy` - at the handler
/// `Task::state` selects. The table is copied onto the stack before the call,
/// the same shape as `func_actor_311900_8016222C` for the first table.
void func_actor_311900_8016249C(Task* task)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_311900_80161E30;
    sp.funcs[task->state](task->spawnArg2.pointer, task);
}

/// The `D_actor_311900_80161E30` spawn handler -- the actor's second setup
/// path, reached through the three-entry table whose tick is
/// `func_actor_311900_801625F0`. It is the same setup `func_actor_311900_8016228C`
/// performs for the first table, under different conditions: the enemy is torn
/// down instead while game flag 1 has already reached nibble 3, and the work
/// block gets the light / colour pair `func_actor_311900_8016281C` splats
/// (rather than `func_actor_311900_8016278C`'s) from a different animation run
/// (`D_actor_311900_8016EBF4`, not `D_actor_311900_8016EBE8`).
///
/// The 0x4CC-byte block goes into `Task::work` -- that slot is not a
/// `TaskIdMap` here. `Enemy::field_4` takes the model's root coordinate's
/// matrix, the root's `parent` is re-parented to `gGfxViewCoord`, the animation
/// context is built over the block's slot array and packed-pose run, and the
/// two work halfwords 0x474 / 0x478 seed the tick's state. Note this handler,
/// unlike `func_actor_311900_8016228C`, does not touch `field_4C4` / `field_4C6`
/// or the model's `field_C`.
static void func_actor_311900_801624F8(Enemy* enemy, Task* task)
{
    Actor311900Work* work;
    GfxCoord*        coord;
    TmdObject*       obj;

    obj   = task->extra.tmd;
    coord = obj->coords;
    if (GameFlag_GetNibble(1) >= 3 ||
        (work = memCalloc(0x4CC, 0), task->work = work, work == NULL)) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    func_actor_311900_8016281C(task);
    enemy->field_4  = &coord->coord;
    enemy->field_48 = 0;
    obj->flags      = 0;
    func_800B3F84(&work->rig.anim, D_actor_311900_8016EBF4, obj, work->rig.poses,
                  work->rig.slots);
    coord->parent   = &gGfxViewCoord;
    work->field_474 = 2;
    work->field_478 = 1;
    func_actor_311900_80162100(task);
    task->state += 1;
}

static void func_actor_311900_801625F0(Enemy* enemy, Task* task)
{
    TmdObject* obj;

    obj = task->extra.tmd;
    func_actor_311900_80161E3C(task, 4, 2);
    if ((Gp_GetViewIndex() & 0xFF) == 0xB) {
        obj->flags = 0;
    } else {
        obj->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    func_actor_311900_80162100(task);
}

/// Takes `arg1` as a signed 16-bit step, builds a direction vector from
/// `arg0->coord`'s rotation with `Gfx_MatrixCol2`, normalizes it with
/// `VectorNormalSS`, scales it by the step on the GTE, adds it to
/// `arg0->coord.t` and clears `arg0->composeStamp`. Returns the step, or 0 having
/// touched nothing while the game is paused (`gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen == 1`) or when the
/// step is zero. `arg0` is the per-part `GfxCoord` the caller takes from
/// `TmdObject::coords`.
///
/// The scratch-pad vector is carved out under two names: `vec`, which the
/// frame update stores and the calls normalize, and `gte`, which the GTE round
/// trip reads and writes back. The object keeps them apart, and that is what
/// the copy ahead of the `if` is.
static s32 func_actor_311900_80162658(GfxCoord* arg0, s16 arg1)
{
    SVECTOR* head;
    SVECTOR* vec;
    SVECTOR* gte;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen == 1) {
        return 0;
    }
    head                          = SCRATCH_STACK_CURSOR(SVECTOR);
    vec                           = head - 1;
    gte                           = head - 1;
    SCRATCH_STACK_CURSOR(SVECTOR) = vec;
    if (arg1 != 0) {
        Gfx_MatrixCol2(&arg0->coord, vec);
        VectorNormalSS(vec, vec);
        gte_lddp(arg1);
        gte_ldsv(gte);
        gte_gpf12();
        gte_stsv(gte);
        arg0->coord.t[0]  += head[-1].vx;
        arg0->coord.t[1]  += vec->vy;
        arg0->coord.t[2]  += vec->vz;
        arg0->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
    return arg1;
}

/// Splats an identity light / colour matrix pair into the work block the spawn
/// state carved out of `Task::work`, republishes both onto the
/// `TmdObject::lightMtx` / `colorMtx` slots that the renderer otherwise reads
/// from `Gp_DefaultMtx` / `Gp_DefaultMtx2`, and then overwrites each 3x3 with
/// the values the actor lights its model with -- the light matrix flat except
/// for `m[1][0]` and `m[2][2]`, the colour matrix fully pass-through.
static void func_actor_311900_8016278C(Task* task)
{
    OverlayMat*      color;
    OverlayMat*      light;
    TmdObject*       ext;
    Actor311900Work* work;

    work  = (Actor311900Work*)task->work;
    ext   = task->extra.tmd;
    light = (OverlayMat*)&work->light;
    color = (OverlayMat*)&work->color;

    light->ident.m00M01 = ONE;
    light->ident.m02M10 = 0;
    light->ident.m11M12 = ONE;
    light->ident.m20M21 = 0;
    light->ident.m22    = ONE;

    color->ident.m00M01 = ONE;
    color->ident.m02M10 = 0;
    color->ident.m11M12 = ONE;
    color->ident.m20M21 = 0;
    color->ident.m22    = ONE;

    ext->lightMtx = &work->light;

    work->color.m[0][0] = 0x1000;
    work->color.m[0][1] = 0x1000;
    work->color.m[0][2] = 0x1000;
    work->color.m[1][0] = 0x1000;
    work->color.m[1][1] = 0x1000;
    work->color.m[1][2] = 0x1000;
    work->color.m[2][0] = 0x1000;
    work->color.m[2][1] = 0x1000;
    work->color.m[2][2] = 0x1000;

    work->light.m[0][0] = 0x1000;
    work->light.m[0][1] = 0x1000;
    work->light.m[0][2] = 0x1000;
    work->light.m[1][0] = 0;
    work->light.m[1][1] = 0x1000;
    work->light.m[1][2] = 0x1000;
    work->light.m[2][0] = 0x1000;
    work->light.m[2][1] = 0x1000;
    work->light.m[2][2] = 0;

    ext->colorMtx = &work->color;
}

/// Same splat as `func_actor_311900_8016278C`, republishing the light / colour
/// pair onto `TmdObject::lightMtx` / `colorMtx` between the identity seed and
/// the per-actor values: the colour matrix goes fully pass-through, the light
/// matrix flat except for a negated `m[0][0]`.
static void func_actor_311900_8016281C(Task* task)
{
    OverlayMat*      color;
    OverlayMat*      light;
    TmdObject*       ext;
    Actor311900Work* work;

    work  = (Actor311900Work*)task->work;
    ext   = task->extra.tmd;
    light = (OverlayMat*)&work->light;
    color = (OverlayMat*)&work->color;

    light->ident.m00M01 = ONE;
    light->ident.m02M10 = 0;
    light->ident.m11M12 = ONE;
    light->ident.m20M21 = 0;
    light->ident.m22    = ONE;

    color->ident.m00M01 = ONE;
    color->ident.m02M10 = 0;
    color->ident.m11M12 = ONE;
    color->ident.m20M21 = 0;
    color->ident.m22    = ONE;

    ext->lightMtx = &work->light;

    work->color.m[0][0] = 0x1000;
    work->color.m[0][1] = 0x1000;
    work->color.m[0][2] = 0x1000;
    work->color.m[1][0] = 0x1000;
    work->color.m[1][1] = 0x1000;
    work->color.m[1][2] = 0x1000;
    work->color.m[2][0] = 0x1000;
    work->color.m[2][1] = 0x1000;
    work->color.m[2][2] = 0x1000;

    work->light.m[0][0] = -0x1000;
    work->light.m[0][1] = 0x1000;
    work->light.m[0][2] = 0x1000;
    work->light.m[1][0] = 0x1000;
    work->light.m[1][1] = 0x1000;
    work->light.m[1][2] = 0x1000;
    work->light.m[2][0] = 0x1000;
    work->light.m[2][1] = 0x1000;
    work->light.m[2][2] = 0x1000;

    ext->colorMtx = &work->color;
}
