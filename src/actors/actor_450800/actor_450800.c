#include "common.h"

#include <psyq/libgte.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3E9C.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "actors/actor.h"

/// Work block this overlay hangs off the task's `Task::work` slot (0x1C),
/// which is not a `TaskIdMap` here. `func_actor_450800_80132160` allocates it
/// with `memCalloc(0x504, 0)` and stores it straight into that field, so the
/// size below is the allocation and not a guess. Reach it with
/// `(Actor450800Work*)task->work`.
///
/// The three tasks at +0x4F0 .. +0x4F8 are the helper tasks this actor spawns;
/// the exit callback `func_actor_450800_80132868` kills all three on teardown.
///
/// The leading `light` / `color` pair and the `anim` / `slots` / `pad_374` run
/// are the same prefix `Actor450800SpawnWork` carries, and the spawn handler
/// `func_actor_450800_80132160` fills them the same way: `&light` and `&color`
/// go to the model's `TmdObject::lightMtx` / `colorMtx`, and `func_800B3F84`
/// gets `&anim`, `slots` and `pad_374` as poses.
///
/// `yaw` is the heading last applied to the model's root coordinate: the
/// placement opcode `func_actor_450800_80132C68` caches the placed yaw there,
/// the "face target" handler `func_actor_450800_80132D74` the computed one, and
/// the step state turns it further while clip 3 plays.
///
/// `field_4EA` is the step count the "face target" handler leaves for the walk
/// and `field_4EC` the turn count clip 3 consumes; the spawn handler zeroes
/// both.
///
/// `anim` is the animation context `func_800B4114` walks. `field_4B8` is the
/// current animation id; `func_actor_450800_80132AE0` starts slots 1..0x13 of
/// `anim` from it, forwarding `field_4FC` as the reset argument, and
/// `func_actor_450800_80132A68` resets them to it; both latch the id into
/// `field_4B6` as the applied copy.
///
/// `state` drives `func_actor_450800_80132448`, which dispatches on it: 1 starts
/// the animation through `func_actor_450800_80132AE0` and 2 resets the slots
/// through `func_actor_450800_80132A68`, both then advancing it to 3.
typedef struct Actor450800Work {
    /* 0x000 */ MATRIX     light; // model light matrix (`TmdObject::lightMtx`)
    /* 0x020 */ MATRIX     color; // model colour matrix (`TmdObject::colorMtx`)
    /* 0x040 */ GpAnimCtx  anim;
    /* 0x054 */ GpAnimSlot slots[0x14];
    /* 0x374 */ byte       pad_374[0x140];
    /* 0x4B4 */ s16        state;
    /* 0x4B6 */ s16        field_4B6;
    /* 0x4B8 */ s16        field_4B8;
    /* 0x4BA */ s16        field_4BA;
    /* 0x4BC */ byte       pad_4BC[0x2A];
    /* 0x4E6 */ s16        yaw;
    /* 0x4E8 */ byte       pad_4E8[0x2];
    /* 0x4EA */ s16        field_4EA; // distance to the target over the step count
    /* 0x4EC */ s16        field_4EC;
    /* 0x4EE */ byte       pad_4EE[0x2];
    /* 0x4F0 */ Task*      field_4F0;
    /* 0x4F4 */ Task*      field_4F4;
    /* 0x4F8 */ Task*      field_4F8;
    /* 0x4FC */ s16        field_4FC;
    /* 0x4FE */ s16        field_4FE; // approach mode the last call selected
    /* 0x500 */ u8         field_500; // 0x7DB mode 1 latches the copied flags here, 2 the 0x84 state
    /* 0x501 */ byte       pad_501[0x3];
} Actor450800Work;
STATIC_ASSERT_SIZEOF(Actor450800Work, 0x504);

/// The overlay's *second* work block, hung off the `Task::work` slot exactly
/// as `Actor450800Work` is but never on the same task: this one is the enemy's,
/// allocated by the spawn handler `func_actor_450800_80132E9C` with
/// `memCalloc(0x4C0, 0)` and driven by the state `func_actor_450800_801330AC`
/// reads at 0x47C, while `Actor450800Work` (0x504) belongs to the actor's own
/// handler `func_actor_450800_80132160`. The overlay's two `fns` dispatchers
/// (`func_actor_450800_80133264` and `func_actor_450800_80132790`) are what keep
/// them apart, and both sizes are the allocations, not a guess.
///
/// `state` drives `func_actor_450800_801330AC` the way `Actor450800Work::state`
/// drives `func_actor_450800_80132448` for the actor: 1 starts the animation
/// through `func_actor_450800_801334C4` and 2 resets the slots through
/// `func_actor_450800_8013344C`, both then advancing it to 3. `animId` is the
/// clip now playing - the spawn handler sets it to 1 and the state machine
/// tests it against 4 - and both start paths latch it into `appliedAnimId`.
/// `anim` / `slots` are what `func_800B3F84` fills in.
///
/// `yaw` is the heading last applied to the model's root coordinate, cached by
/// the placement opcode `func_actor_450800_801335F8` and the "walk to" opcode
/// `func_actor_450800_80133678`; the latter also leaves in `travel` the
/// distance to cover, in twelfths. State 3 counts `travel` down a step per
/// frame while the walk clip plays and, when it reaches zero, drops back to
/// clip 1 with 0xA in `animArg`, the reset argument the start path forwards.
///
/// The leading matrices are the ones the enemy renders through - the spawn
/// handler hands `&light` and `&color` to the object's `TmdObject::lightMtx` /
/// `colorMtx`, and the sub-model task `func_actor_450800_80133740` lights its
/// own model with the same pair. `field_4B8` is that sub-model's task and
/// `field_4BC` the `GpEnemy` the spawn handler was given.
typedef struct Actor450800SpawnWork {
    /* 0x000 */ MATRIX     light;
    /* 0x020 */ MATRIX     color;
    /* 0x040 */ GpAnimCtx  anim;
    /* 0x054 */ GpAnimSlot slots[0x13];
    /* 0x34C */ byte       field_34C;
    /* 0x34D */ byte       pad_34D[0x12F];
    /* 0x47C */ s16        state;
    /* 0x47E */ u16        appliedAnimId;
    /* 0x480 */ u16        animId;
    /* 0x482 */ s16        field_482;
    /* 0x484 */ byte       pad_484[0x2A];
    /* 0x4AE */ u16        yaw;
    /* 0x4B0 */ byte       pad_4B0[0x2];
    /* 0x4B2 */ s16        travel;
    /* 0x4B4 */ s16        animArg;
    /* 0x4B6 */ byte       pad_4B6[0x2];
    /* 0x4B8 */ Task*      field_4B8;
    /* 0x4BC */ GpEnemy*   field_4BC;
} Actor450800SpawnWork;
STATIC_ASSERT_SIZEOF(Actor450800SpawnWork, 0x4C0);

/// Message payload the overlay's message handlers take as `Gp_DispatchMsg`'s
/// `arg2`: only the halfword at 0x2 is read.
typedef struct Actor450800Msg {
    /* 0x0 */ byte pad_0[2];
    /* 0x2 */ u16  field_2;
} Actor450800Msg;

/// Script args the "start animation" opcodes receive - the actor's
/// `func_actor_450800_80132B44` and the enemy's `func_actor_450800_80133528`:
/// the clip id, a flag choosing the start path, and the reset argument only
/// that path carries.
typedef struct Actor450800AnimArgs {
    /* 0x0 */ byte pad_0[4];
    /* 0x4 */ s32  animId;
    /* 0x8 */ s32  withArg;
    /* 0xC */ u16  animArg;
} Actor450800AnimArgs;

/// Spawn offset `func_actor_450800_80132108` copies into a local and hands to
/// `Gp_SpawnEff` as the effect's position.
const SVECTOR D_actor_450800_80131E24 = { 0x19C8, -0x578, 0x3C0, 0 };

/// Message table `func_actor_450800_80132160` hangs off `Task::msgTable`, and
/// the `TaskDesc` table its three helper tasks come from.
extern GpMsgEntry D_actor_450800_8014AC58[];
extern TaskDesc   D_actor_450800_8014AC88[];

/// Animation data `func_800B3F84` seeds the work block's slots from.
extern u8 D_actor_450800_8014ACC4[];

/// The enemy's message table, the `TaskDesc` table its model tasks come from,
/// and the animation data its work block's slots are seeded from - the same
/// three roles as the actor's tables above.
extern GpMsgEntry D_actor_450800_801539AC[];
extern TaskDesc   D_actor_450800_801539DC[];
extern u8         D_actor_450800_801539F4[];

extern s32  D_actor_450800_8013930C;
extern s32  D_actor_450800_801397A4;
extern s32  D_actor_450800_801398EC;
extern s32  D_actor_450800_8013A564;
extern s32  D_actor_450800_8013A684;
extern s32  D_actor_450800_8013A774;
extern s32  D_actor_450800_8013A984;
extern s32  D_actor_450800_8013AB7C;
extern s32  D_actor_450800_8013ACFC;
extern s16  D_80071076;
extern void func_80180038(s32);
extern void func_80182D14(s32, s32);

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */
#define SCRATCH_SP (*(u32*)0x1F8003FC)

/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void func_actor_450800_80132448(Task* task);
void func_actor_450800_801327E4(GpEnemy* enemy, Task* task);
void func_actor_450800_80132868(Task* task);
void func_actor_450800_801328BC(Task* task);
void func_actor_450800_80132A1C(Task* task);
void func_actor_450800_80132A68(Task* task);
void func_actor_450800_80132AE0(Task* task);
void func_actor_450800_801330AC(Task* task);
void func_actor_450800_801332B8(GpEnemy* enemy, Task* task);
void func_actor_450800_8013333C(Task* task);
void func_actor_450800_80133364(Task* task);
void func_actor_450800_80133400(Task* task);
void func_actor_450800_8013344C(Task* task);
void func_actor_450800_801334C4(Task* task);

void func_actor_450800_80131E2C(void)
{
    s32 temp_v0;
    s32 n;

    if (gGameSession->at4.loc.view == 4) {
        if (GameFlag_GetNibble(0xC7) == 1) {
            temp_v0                 = D_actor_450800_8013930C + 1;
            D_actor_450800_8013930C = temp_v0;
            if (temp_v0 >= 3) {
                D_actor_450800_8013930C = 3;
                func_800E8614((s32)&D_actor_450800_8013A774, 0);
            } else {
                func_800E8614((s32)&D_actor_450800_8013A684, 0);
            }
        } else {
            n = GameFlag_GetNibble(0xC8) + 1;
            if (n >= 4) {
                n = 3;
            }
            GameFlag_SetNibble(0xC8, n);
            if (n == 1) {
                if (GameFlag_GetNibble(0x83) == n) {
                    func_800E8614((s32)&D_actor_450800_8013A984, 0);
                } else {
                    func_800E8614((s32)&D_actor_450800_8013AB7C, 0);
                }
                func_800E3FAC(0xA2, 0x32);
            } else {
                func_800E8614((s32)&D_actor_450800_8013ACFC, 0);
            }
        }
    }
}

/// Callback the overlay's event scripts name: a non-zero `arg0` clears
/// `Gp_CapFile`, loads capture file 2 and hands 0x340 to `func_800E6D4C`; zero
/// resets the capture state instead.
void func_actor_450800_80131F28(s32 arg0)
{
    if (arg0 != 0) {
        Gp_CapFile = 0;
        Gp_LoadCapFile(2);
        func_800E6D4C(0x340, 0);
        return;
    }
    Gp_ResetCap();
}

void func_actor_450800_80131F70(u32 arg0)
{
    func_80182D14(arg0 >> 16, arg0 & 0xFFFF);
}

/// Two call sites, not one: `Gp_StartCapSlot` is written out in both arms of
/// the outer test. The tail-call cross-jump in `jump.c` merges them only from
/// the `jal` onward, because sched2 hoists the `a1`/`a2` setup away from the
/// call in the first arm before that pass runs - which is why the object sets
/// `$a1`/`$a2` twice and shares one `jal`.
///
/// The global is an `s32` (see `func_actor_450800_80131E2C`, which increments
/// it whole), but this arm only wants its low half, which is the `lhu`.
void func_actor_450800_80131F98(s32 arg0)
{
    s16 var_a0;

    if (arg0 == 1) {
        var_a0 = (u16)D_actor_450800_8013930C + 2;
        Gp_StartCapSlot(var_a0, 0, 0);
    } else {
        if (GameFlag_GetNibble(0xC8) == 2) {
            var_a0 = 8;
        } else {
            var_a0 = 9;
        }
        Gp_StartCapSlot(var_a0, 0, 0);
    }
}

void func_actor_450800_80132000(void)
{
    func_800E8614((s32)&D_actor_450800_8013A564, 0);
}

void func_actor_450800_80132028(void)
{
    Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7D3, (s32)&D_actor_450800_801397A4, 0);
    Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7D4, (s32)&D_actor_450800_801398EC, 0);
}

void func_actor_450800_80132080(void)
{
    if (Mc_SaveData.demoScene != 9) {
        Mc_SaveData.at4.loc.stage = 5;
        Mc_SaveData.at4.loc.area  = 0x17;
        Mc_SaveData.at4.loc.warp  = 1;
        Mc_SaveData.at4.loc.room  = 1;
        D_80071076                = 1;
        Task_Spawn(0, 0x11, 0, 0);
    }
}

void func_actor_450800_801320E8(s32 arg0)
{
    func_80180038(arg0 & 0xFF);
}

void func_actor_450800_80132108(void)
{
    SVECTOR pos;

    pos = D_actor_450800_80131E24;
    Gp_SpawnEff(0x6003B, NULL, 0x200, &pos);
}

/// Spawn handler of the actor's own task, state 0 of the `fns` table
/// `func_actor_450800_80132790` dispatches through. Builds the actor's
/// `Actor450800Work` block, hangs its leading matrices off the model's
/// `lightMtx` / `colorMtx`, and starts the animation.
///
/// The three helper tasks come out of `D_actor_450800_8014AC88`: 1 and 2 are
/// the actor's own model parts, and each is placed by the area key its
/// `Task::spawnArg2` carries. Task 4 is spawned but not placed.
///
/// The `do { } while (0)` around the second `tmdProcessStream` is
/// load-bearing: the loop body is a statement of its own, so the model pointer
/// gains a reference that the bare second call does not. That reference is
/// what lifts the pointer's global-alloc priority (refs 7, not 6) past
/// `work`'s, so it takes `$s1` and pushes `work` into `$s2`, which is the
/// ROM's split. See DECOMPILATION_LEARNINGS.md, "A `do { } while (0)` around
/// one of two identical calls adds its `REF` back".
void func_actor_450800_80132160(GpEnemy* enemyArg, Task* task)
{
    GpEnemy* enemy = enemyArg;

    VECTOR           vec;
    GpAreaKey        key;
    GpAreaKey*       keyp;
    GsCOORDINATE2*   coord;
    TmdObject*       obj;
    Actor450800Work* work;
    u8               areaByte0;
    u8               areaByte1;
    u32              raw1;
    u32              raw2;
    u32              index1;
    u32              index2;
    Task*            spawned;
    TmdObject*       model1;
    GpAreaPlace*     entry1;
    GpAreaKey*       sessionKey1;
    TmdObject*       model2;
    GpAreaPlace*     entry2;
    GpAreaKey*       sessionKey2;

    obj        = task->extra;
    coord      = obj->coords;
    work       = memCalloc(0x504, 0);
    task->work = (TaskIdMap*)work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback   = func_actor_450800_80132868;
    coord->sub           = &gGfxViewCoord;
    enemy->field_4       = &coord->coord;
    enemy->field_48      = 0;
    enemy->node.targeted = 0;
    enemy->node.flags    = 1;
    if ((s16)(task->spawnArg1 >> 16) == 1) {
        obj->flags = 0;
    }
    obj->otOffset = 1;
    obj->lightMtx = &work->light;
    obj->colorMtx = &work->color;
    vec.vx        = coord->workm.t[0];
    vec.vy        = coord->workm.t[1] - 0x320;
    vec.vz        = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&work->anim, D_actor_450800_8014ACC4, obj, work->pad_374,
                  work->slots);
    work->field_4B8 = 1;
    work->state     = 2;

    spawned = Task_SpawnFromTable(D_actor_450800_8014AC88, 1, 8, 0);
    if (spawned != NULL) {
        work->field_4F0 = spawned;
        spawned->parent = task;
        model1          = spawned->extra;
        sessionKey1     = (GpAreaKey*)&gGameSession->at4.loc;
        raw1            = ((GpEnemy*)task->spawnArg2)->placeKey;
        key.stage       = sessionKey1->stage;
        key.area        = sessionKey1->area;
        areaByte1       = sessionKey1->room;
        SOFT_BARRIER();
        keyp = &key;
        TOUCH_REG(keyp);
        key.room  = areaByte1;
        areaByte0 = gGameSession->at4.loc.view;
        index1    = raw1 >> 12;
        key.view  = areaByte0;
        Gp_SyncAreaKeyIndex(keyp);
        entry1        = (GpAreaPlace*)((index1 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
        model1->tpage = entry1->tpage;
        model1->clut  = entry1->clut;
        if (model1->buffer != NULL) {
            tmdProcessStream(model1);
            do {
                tmdProcessStream(model1);
            } while (0);
        }
    }

    spawned = Task_SpawnFromTable(D_actor_450800_8014AC88, 2, 0xC, 0);
    if (spawned != NULL) {
        work->field_4F4 = spawned;
        spawned->parent = task;
        model2          = spawned->extra;
        sessionKey2     = (GpAreaKey*)&gGameSession->at4.loc;
        raw2            = ((GpEnemy*)task->spawnArg2)->placeKey;
        key.stage       = sessionKey2->stage;
        key.area        = sessionKey2->area;
        areaByte1       = sessionKey2->room;
        SOFT_BARRIER();
        keyp = &key;
        TOUCH_REG(keyp);
        key.room  = areaByte1;
        areaByte0 = gGameSession->at4.loc.view;
        index2    = raw2 >> 12;
        key.view  = areaByte0;
        Gp_SyncAreaKeyIndex(keyp);
        entry2        = (GpAreaPlace*)((index2 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
        model2->tpage = entry2->tpage;
        model2->clut  = entry2->clut;
        if (model2->buffer != NULL) {
            tmdProcessStream(model2);
            do {
                tmdProcessStream(model2);
            } while (0);
        }
    }

    spawned = Task_SpawnFromTable(D_actor_450800_8014AC88, 4, 8, 0);
    if (spawned != NULL) {
        spawned->parent = task;
        work->field_4F8 = spawned;
    }

    work->field_4FC = 8;
    work->field_4EA = 0;
    work->field_4EC = 0;
    work->field_500 = 0;
    task->msgTable  = D_actor_450800_8014AC58;
    func_actor_450800_80132448(task);
    task->state++;
}

void func_actor_450800_80132448(Task* task)
{
    GsCOORDINATE2*   coord = ((TmdObject*)task->extra)->coords;
    Actor450800Work* work  = (Actor450800Work*)task->work;

    if (work->state == 1) {
        func_actor_450800_80132AE0(task);
        work->state = 3;
    } else if (work->state == 2) {
        func_actor_450800_80132A68(task);
        work->state = 3;
    } else if (work->state == 3) {
        if (work->field_4B8 == 0xE || work->field_4B8 == 2 || work->field_4B8 == 0xF) {
            if (work->field_4EA != 0) {
                switch (work->field_4FE) {
                    case 0:
                        actorMoveModelForward(task, 0x3C);
                        break;
                    case 1:
                        actorMoveModelForward(task, -0xF);
                        break;
                    case 2:
                        actorMoveModelForward(task, 0x19);
                        break;
                }
                if (--work->field_4EA == 0) {
                    work->state     = 1;
                    work->field_4FC = 0xA;
                    work->field_4B8 = 0xD;
                }
            }
        }
        if (work->field_4B8 == 3 && work->field_4EC != 0) {
            work->yaw += 0x33;
            Gfx_RotMatrixY(&coord->coord, work->yaw, 1);
            coord->flg = 0;
            work->field_4EC--;
        }
        func_actor_450800_80132A1C(task);
    }
}

void func_actor_450800_80132790(Task* task)
{
    GpEnemyTaskFunc fns[2] = { func_actor_450800_80132160, func_actor_450800_801327E4 };

    fns[task->state](task->spawnArg2, task);
}

/// Per-frame handler of the actor's own task, state 1 of the `fns` table
/// `func_actor_450800_80132790` dispatches through: refreshes the model root's
/// world matrix, relights the model from a point 0x320 above its translation,
/// then runs the animation state machine and draws the ground shadow.
void func_actor_450800_801327E4(GpEnemy* enemy, Task* task)
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
    func_actor_450800_80132448(task);
    func_actor_450800_801328BC(task);
}

void func_actor_450800_80132868(Task* task)
{
    Actor450800Work* work = (Actor450800Work*)task->work;

    Gp_DestroyEnemy(task->spawnArg2, task);
    taskKill(work->field_4F0);
    taskKill(work->field_4F4);
    taskKill(work->field_4F8);
}

/// Draws the actor's ground shadow quad under the model root, skipped while
/// the model's `flags` has 0x80 set or it has no buffer yet. The world
/// position is the translation of the root part's `workm`, staged in a
/// scratchpad VECTOR3 rather than on the stack, and the quad's shade is the
/// room's current `Gp_State1C` level.
void func_actor_450800_801328BC(Task* task)
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
        Gp_DrawEffGroundQuad(vec, 0x200, Gp_State1C->groundShade);
        SCRATCH_SP += 0x18;
    }
}

/// State handler of one of the actor's model tasks: the spawn tick hangs this
/// task's own coordinate frame off part `spawnArg1` of the actor's model and
/// every later tick hands that part's world translation, dropped by 0x320 in y,
/// to `func_800D7A9C` for the part colour matrix. The parts come from
/// `task->parent`, the actor task that spawned this one
/// (`func_actor_450800_80132160`, which also tests the same halfword on itself).
///
/// The model flags are cleared only for spawn variant 1: the high half of the
/// parent's `spawnArg1`.
void func_actor_450800_80132958(Task* task)
{
    TmdObject*     extra = task->extra;
    GsCOORDINATE2* coord = extra->coords;
    GsCOORDINATE2* parts = ((TmdObject*)task->parent->extra)->coords;
    GsCOORDINATE2* part  = parts + task->spawnArg1;
    VECTOR         vec;

    switch (task->state) {
        case 0:
            coord->flg = 0;
            if ((s16)(task->parent->spawnArg1 >> 16) == 1) {
                extra->flags = 0;
            }
            coord->sub = part;
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

/// Ticks the actor's animation slots 1..0x13.
void func_actor_450800_80132A1C(Task* task)
{
    Actor450800Work* work;
    s32              i;

    work = (Actor450800Work*)task->work;
    i    = 1;
    do {
        Gp_AnimTickIndex(&work->anim, i);
        i++;
    } while (i < 0x14);
}

/// Resets the actor's animation slots 1..0x13 to clip `field_4B8` at rate 1,
/// without a reset argument, and latches the clip into `field_4B6`.
void func_actor_450800_80132A68(Task* task)
{
    Actor450800Work* work;
    s32              i;

    work = (Actor450800Work*)task->work;
    i    = 1;
    do {
        work->slots[i].rate = 1;
        Gp_AnimResetSlot(&work->anim, i, work->field_4B8);
        i++;
    } while (i < 0x14);
    work->field_4B6 = work->field_4B8;
}

void func_actor_450800_80132AE0(Task* task)
{
    Actor450800Work* work;
    s32              i;

    work = (Actor450800Work*)task->work;
    i    = 1;
    do {
        func_800B4114(&work->anim, i, work->field_4B8, 0, work->field_4FC);
        i++;
    } while (i < 0x14);
    work->field_4B6 = work->field_4B8;
}

/// Script opcode: start animation `args->animId` on this actor.
///
/// `withArg` selects between the two start paths `func_actor_450800_80132448`
/// dispatches on, and only the first carries `animArg`. Returns -1, without
/// touching the work block, when the clip id is out of range.
///
/// The `SOFT_BARRIER()` is a codegen pin, not a semantic one. Without it GCC's
/// delay-slot pass fills the `beqz` from the fall-through arm (`state = 1`);
/// the ROM has the *else* arm's `state = 2` there, which the pass only reaches
/// once an `asm` at the head of the fall-through stops it searching that
/// thread. See DECOMPILATION_LEARNINGS.md, "An empty asm at the head of the
/// then-arm moves the delay slot to the else arm".
s32 func_actor_450800_80132B44(Task* task, s32 arg1, Actor450800AnimArgs* args)
{
    Actor450800Work* work;

    work = (Actor450800Work*)task->work;
    if (args->animId >= 0x1F) {
        return -1;
    }

    work->field_4B8 = args->animId;
    if (args->withArg != 0) {
        SOFT_BARRIER();
        work->state     = 1;
        work->field_4FC = args->animArg;
    } else {
        work->state = 2;
    }
    work->field_4BA = 0;
    func_actor_450800_80132448(task);
    return 0;
}

/// Message handler 0x7D5 of `D_actor_450800_8014AC58`: sets `TmdObject::flags`
/// on this actor's own model and on the three helper tasks' ones at once.
///
/// `arg2` bit 0 selects 0 rather than 0x80, and bit 1 ORs 4 in.
/// `Actor450800Work::field_500` overrides the last of them: while it is 0 the
/// helper at `field_4F8` keeps the 0x84 handler 0x7DB's mode 2 gave it,
/// instead of the flags just computed.
s32 func_actor_450800_80132BB0(Task* task, s32 arg1, s32 arg2)
{
    Actor450800Work* work;
    TmdObject*       self;
    TmdObject*       first;
    TmdObject*       second;
    TmdObject*       third;

    work   = (Actor450800Work*)task->work;
    self   = (TmdObject*)task->extra;
    first  = (TmdObject*)work->field_4F0->extra;
    second = (TmdObject*)work->field_4F4->extra;
    third  = (TmdObject*)work->field_4F8->extra;

    if (arg2 & 1) {
        self->flags   = 0;
        first->flags  = 0;
        second->flags = 0;
        third->flags  = 0;
    } else {
        self->flags   = 0x80;
        first->flags  = 0x80;
        second->flags = 0x80;
        third->flags  = 0x80;
    }
    if (arg2 & 2) {
        self->flags   |= 4;
        first->flags  |= 4;
        second->flags |= 4;
        third->flags  |= 4;
    }
    if (work->field_500 == 0) {
        third->flags = 0x84;
    }
    return 0;
}

/// Message handler 0x7D4 of `D_actor_450800_8014AC58`, the placement opcode:
/// yaws the actor's root coordinate to `placement->rot.vy`, caching that yaw in
/// `Actor450800Work::yaw`, then drops the placement translation into the matrix
/// and marks it dirty.
s32 func_actor_450800_80132C68(Task* task, s32 arg1, GpPlaceArg* placement)
{
    GsCOORDINATE2*   coord;
    Actor450800Work* work;
    u16              yaw;

    coord     = ((TmdObject*)task->extra)->coords;
    work      = (Actor450800Work*)task->work;
    yaw       = placement->rot.vy;
    work->yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->flg        = 0;
    return 0;
}

/// Message handler 0x7DB of `D_actor_450800_8014AC58`: recolour this actor's
/// body (or spawn its 0x6002B burst) according to the message's selector.
///
/// The model is the actor's own -- `task->extra`, the `TmdObject` a spawnType-1
/// task carries -- and the one it is driven through is that of the helper task
/// in `Actor450800Work::field_4F8`. Both pointers, and `field_8` of the helper's
/// model, are resolved before the switch: the ROM reads them there, and a
/// scheduler pass cannot lift the loads into the entry block on its own.
s32 func_actor_450800_80132CE0(Task* task, s32 arg1, Actor450800Msg* msg, s32 arg3)
{
    Actor450800Work* work  = (Actor450800Work*)task->work;
    TmdObject*       obj   = (TmdObject*)work->field_4F8->extra;
    GsCOORDINATE2*   coord = obj->coords;
    TmdObject*       self  = (TmdObject*)task->extra;
    s32              mode  = msg->field_2;

    switch (mode) {
        case 0:
            Gp_SpawnEff(0x6002B, coord, 0x21, 0);
            break;
        case 1:
            work->field_500 = mode;
            obj->flags      = self->flags;
            break;
        case 2:
            work->field_500 = 0;
            obj->flags      = 0x84;
            break;
    }
    return 0;
}

/// Message handler 0x7DD of `D_actor_450800_8014AC58`, the payload's first two
/// words being the target position: turns the actor's model to face it -- away
/// from it in mode 1 -- and latches the per-step distance over the step count
/// the mode selects, 60 in mode 0, 15 in mode 1 and 25 otherwise. The mode and
/// both results are kept on the work block.
///
/// The mode store sits after the two differences on purpose. Its place in the
/// source sets its RTL uid, and the uid is what the scheduler's ready-list
/// tie-break compares once `-O2` has CSE'd the constant 1 into a register and
/// every candidate carries the same priority; from before them the whole entry
/// block comes out in a different order and on different registers.
s32 func_actor_450800_80132D74(Task* task, s32 arg1, VECTOR* target, s32 mode)
{
    Actor450800Work* work;
    GsCOORDINATE2*   coord;
    s32              dx;
    s32              dz;
    s32              steps;
    s32              dist;
    s32              angle;

    coord           = ((TmdObject*)task->extra)->coords;
    work            = (Actor450800Work*)task->work;
    dx              = target->vx - coord->coord.t[0];
    dz              = target->vz - coord->coord.t[2];
    work->field_4FE = mode;
    angle           = ratan2(dx, dz);
    work->yaw       = angle;
    if (work->field_4FE == 1) {
        work->yaw = angle + 0x800;
    }
    Gfx_RotMatrixY(&coord->coord, work->yaw, 1);
    dist  = SquareRoot0(dx * dx + dz * dz);
    steps = 0x19;
    switch (work->field_4FE) {
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

/// Spawn handler of the enemy this actor's model task carries: state 0 of
/// `func_actor_450800_80133264`'s `fns` table. Builds the enemy's `Actor450800SpawnWork` block,
/// spawns its own model task out of the same `D_actor_450800_801539DC` table,
/// faces it at the placed spawn point, starts the animation and hands the state
/// machine to `func_actor_450800_801330AC`.
///
/// Two codegen pins, both load-bearing. `key` lands at `vfp+0x28`, and left
/// alone CSE merges the two call-site copies of `&key` into one pseudo live
/// across the first call, which costs a callee-saved register and grows the
/// frame; `SOFT_BARRIER()` keeps each materialization next to its own call and
/// `TOUCH_REG` makes the second a fresh computation. The `mem` / `work` pair is
/// the same kind of pin on the `memCalloc` result: the ROM keeps a short-lived
/// copy for the `work` store, the NULL test and `field_4BC`, and a longer-lived
/// one for everything after.
void func_actor_450800_80132E9C(GpEnemy* enemyArg, Task* task)
{
    GpEnemy*              enemy = enemyArg;
    VECTOR                vec;
    GpAreaKey             key;
    Actor450800SpawnWork* work;
    Actor450800SpawnWork* mem;
    GsCOORDINATE2*        coord;
    TmdObject*            obj;
    GpEnemy*              spawned;
    TmdObject*            model;
    GpAreaKey*            sessionKey;
    GpAreaKey*            keyPtr;
    u8                    areaByte0;
    GpAreaRec*            rec;
    GpAreaPlace*          place;
    s32                   idx;
    u32                   raw;

    obj        = task->extra;
    coord      = obj->coords;
    mem        = (Actor450800SpawnWork*)memCalloc(0x4C0, false);
    work       = mem;
    task->work = (TaskIdMap*)mem;
    if (mem == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback   = func_actor_450800_8013333C;
    coord->sub           = &gGfxViewCoord;
    enemy->field_4       = &coord->coord;
    enemy->field_48      = 0;
    enemy->node.targeted = 0;
    enemy->node.flags    = 1;
    obj->otOffset        = 1;
    mem->field_4BC       = enemy;
    spawned              = Gp_SpawnEnemyFromTable(D_actor_450800_801539DC, 1, 0, enemy);
    model                = (TmdObject*)spawned->task->extra;
    raw                  = enemy->placeKey;
    sessionKey           = (GpAreaKey*)&gGameSession->at4.loc;
    key.stage            = sessionKey->stage;
    key.area             = sessionKey->area;
    key.room             = sessionKey->room;
    idx                  = raw >> 12;
    areaByte0            = sessionKey->view;
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
    Task_Reparent(task, spawned->task);
    work->field_4B8 = spawned->task;
    obj->lightMtx   = &work->light;
    obj->colorMtx   = &work->color;
    vec.vx          = coord->workm.t[0];
    vec.vy          = coord->workm.t[1] - 0x320;
    vec.vz          = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&work->anim, D_actor_450800_801539F4, obj,
                  &work->field_34C, work->slots);
    work->animId   = 1;
    work->state    = 2;
    task->msgTable = D_actor_450800_801539AC;
    func_actor_450800_801330AC(task);
    task->state++;
}

/// The enemy's animation state machine, run by its spawn and per-frame
/// handlers. States 1 and 2 start the clip in `animId` through
/// `func_actor_450800_801334C4` or `func_actor_450800_8013344C` and advance to
/// state 3. State 3 walks the model 12 units a frame while the walk clip (4)
/// has `travel` left, dropping back to clip 1 with reset argument 0xA when it
/// runs out, then ticks the slots.
void func_actor_450800_801330AC(Task* task)
{
    Actor450800SpawnWork* work;
    s16                   animId;

    work = (Actor450800SpawnWork*)task->work;
    if (work->state == 1) {
        func_actor_450800_801334C4(task);
        work->state = 3;
        return;
    }
    if (work->state == 2) {
        func_actor_450800_8013344C(task);
        work->state = 3;
        return;
    }
    if (work->state == 3) {
        do {
        } while (0);
        animId = work->animId;
        if (animId == 4 && work->travel != 0) {
            actorMoveModelForward(task, 0xC);
            work->travel = (u16)work->travel - 1;
            if (work->travel == 0) {
                work->animArg = 0xA;
                work->animId  = 1;
            }
        }
        func_actor_450800_80133400(task);
        return;
    }
}

void func_actor_450800_80133264(Task* task)
{
    GpEnemyTaskFunc fns[2] = { func_actor_450800_80132E9C, func_actor_450800_801332B8 };

    fns[task->state](task->spawnArg2, task);
}

/// Per-frame handler of the enemy this actor's model task carries: state 1 of
/// `func_actor_450800_80133264`'s `fns` table.
///
/// Refreshes the enemy model root's coordinate, feeds its world translation
/// (lowered by 800 on y, to sit on the ground) to `func_800D7A9C`, then ticks
/// the enemy's animation state through `func_actor_450800_801330AC` and draws its
/// ground shadow.
void func_actor_450800_801332B8(GpEnemy* enemy, Task* task)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    VECTOR         pos;

    obj   = task->extra;
    coord = obj->coords;
    Gp_UpdateCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1] - 800;
    pos.vz = coord->workm.t[2];
    func_800D7A9C(obj, &pos, 0, 3);
    func_actor_450800_801330AC(task);
    func_actor_450800_80133364(task);
}

/// Exit callback of the enemy's task, set by its spawn handler
/// `func_actor_450800_80132E9C`: releases the enemy slot the task was spawned
/// for.
void func_actor_450800_8013333C(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}

/// Draws the enemy's ground shadow quad under its model root, skipped while
/// the model's `flags` has 0x80 set or it has no buffer yet. The world
/// position is the translation of the root part's `workm`, staged in a
/// scratchpad VECTOR3 rather than on the stack, and the quad's shade is the
/// room's current `Gp_State1C` level.
void func_actor_450800_80133364(Task* task)
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
        Gp_DrawEffGroundQuad(vec, 0x200, Gp_State1C->groundShade);
        SCRATCH_SP += 0x18;
    }
}

/// Ticks the enemy's animation slots 1..0x12.
void func_actor_450800_80133400(Task* task)
{
    Actor450800SpawnWork* work;
    s32                   i;

    work = (Actor450800SpawnWork*)task->work;
    i    = 1;
    do {
        Gp_AnimTickIndex(&work->anim, i);
        i++;
    } while (i < 0x13);
}

/// Resets the enemy's animation slots 1..0x12 to clip `animId` at rate 1,
/// without a reset argument, and latches the clip into `appliedAnimId`.
void func_actor_450800_8013344C(Task* task)
{
    Actor450800SpawnWork* work;
    s32                   i;

    work = (Actor450800SpawnWork*)task->work;
    for (i = 1; i < 0x13; i++) {
        work->slots[i].rate = 1;
        Gp_AnimResetSlot(&work->anim, i, (s16)work->animId);
    }
    work->appliedAnimId = work->animId;
}

/// Starts the enemy's animation slots 1..0x12 on clip `animId`, forwarding
/// `animArg` as the reset argument, and latches the clip into `appliedAnimId`.
void func_actor_450800_801334C4(Task* task)
{
    Actor450800SpawnWork* work;
    s32                   i;

    work = (Actor450800SpawnWork*)task->work;
    i    = 1;
    do {
        func_800B4114(&work->anim, i, (s16)work->animId, 0, work->animArg);
        i++;
    } while (i < 0x13);
    work->appliedAnimId = work->animId;
}

/// Message handler 0x7D3 of `D_actor_450800_801539AC`, the enemy's "start
/// animation" opcode: `withArg` selects between the two start paths
/// `func_actor_450800_801330AC` dispatches on, and only the first carries
/// `animArg`. Returns -1, without touching the work block, when the clip id is
/// out of range.
///
/// The `SOFT_BARRIER()` is a codegen pin, not a semantic one. Without it GCC's
/// delay-slot pass fills the `beqz` from the fall-through arm (`state = 1`);
/// the ROM has the *else* arm's `state = 2` there, which the pass only reaches
/// once an `asm` at the head of the fall-through stops it searching that
/// thread. See DECOMPILATION_LEARNINGS.md, "An empty asm at the head of the
/// then-arm moves the delay slot to the else arm".
s32 func_actor_450800_80133528(Task* task, s32 arg1, Actor450800AnimArgs* args)
{
    Actor450800SpawnWork* work;

    work = (Actor450800SpawnWork*)task->work;
    if (args->animId >= 6) {
        return -1;
    }

    work->animId = args->animId;
    if (args->withArg != 0) {
        SOFT_BARRIER();
        work->state   = 1;
        work->animArg = args->animArg;
    } else {
        work->state = 2;
    }
    work->field_482 = 0;
    func_actor_450800_801330AC(task);
    return 0;
}

/// Message handler 0x7D5 of `D_actor_450800_801539AC`: sets `TmdObject::flags`
/// on the enemy's own model and on the sub-model task's in `field_4B8` at
/// once. `flags` bit 0 selects 0 rather than 0x80, and bit 1 ORs 4 in.
s32 func_actor_450800_80133594(Task* task, s32 arg1, s32 flags)
{
    TmdObject* self;
    TmdObject* other;

    self  = (TmdObject*)task->extra;
    other = (TmdObject*)((Actor450800SpawnWork*)task->work)->field_4B8->extra;

    if (flags & 1) {
        self->flags  = 0;
        other->flags = 0;
    } else {
        self->flags  = 0x80;
        other->flags = 0x80;
    }

    if (flags & 2) {
        self->flags  |= 4;
        other->flags |= 4;
    }
    return 0;
}

/// Message handler 0x7D4 of `D_actor_450800_801539AC`, the enemy's placement
/// opcode: yaws its root coordinate to `placement->rot.vy`, caching that yaw
/// in `Actor450800SpawnWork::yaw`, then drops the placement translation into
/// the matrix and marks it dirty.
s32 func_actor_450800_801335F8(Task* task, s32 arg1, GpPlaceArg* placement)
{
    GsCOORDINATE2*        coord;
    Actor450800SpawnWork* work;
    u16                   yaw;

    coord     = ((TmdObject*)task->extra)->coords;
    work      = (Actor450800SpawnWork*)task->work;
    yaw       = placement->rot.vy;
    work->yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->flg        = 0;
    return 0;
}

s32 func_actor_450800_80133670(void)
{
    return 0;
}

/// Message handler 0x7DD of `D_actor_450800_801539AC`, the enemy's "walk to"
/// opcode: turns its root coordinate to face `target`, caching the yaw in
/// `Actor450800SpawnWork::yaw`, and leaves the horizontal distance to it, in
/// twelfths, in `travel` for the walk state to count down.
s32 func_actor_450800_80133678(Task* task, s32 arg1, VECTOR* target)
{
    GsCOORDINATE2*        coord;
    Actor450800SpawnWork* work;
    s32                   dx;
    s32                   dz;
    u16                   yaw;

    coord     = ((TmdObject*)task->extra)->coords;
    work      = (Actor450800SpawnWork*)task->work;
    dx        = target->vx - coord->coord.t[0];
    dz        = target->vz - coord->coord.t[2];
    yaw       = ratan2(dx, dz);
    work->yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    work->travel = SquareRoot0(dx * dx + dz * dz) / 12;
    return 0;
}

/// Handler of the enemy's sub-model task, the second entry of
/// `D_actor_450800_801539DC`, which the enemy's spawn handler reparents under
/// the enemy's own task. On its first tick it lights the sub-model with the
/// enemy's `Actor450800SpawnWork` matrices and hangs its root coordinate off
/// part 7 of the enemy's model; after that it only marks the coordinate dirty
/// each frame so it follows that part.
void func_actor_450800_80133740(Task* task)
{
    char                  pad[0x10];
    Task*                 parent = task->parent;
    TmdObject*            obj    = task->extra;
    GsCOORDINATE2*        coord  = obj->coords;
    GsCOORDINATE2*        sub    = &((TmdObject*)parent->extra)->coords[7];
    Actor450800SpawnWork* work   = (Actor450800SpawnWork*)parent->work;

    switch (task->state) {
        case 0:
            coord->flg    = 0;
            obj->lightMtx = &work->light;
            obj->colorMtx = &work->color;
            coord->sub    = sub;
            task->state++;
            break;
        case 1:
            coord->flg = 0;
            break;
    }
}
