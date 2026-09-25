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

/// Work block of the overlay's own actor, allocated zeroed by its spawn
/// routine and kept at `Task::work`; the overlay's enemy uses
/// `Actor150400Work` instead, and the two dispatchers keep the blocks apart.
/// `light` and `color` are the matrices the actor's model is lit with, `rig`
/// and `st` its animation rig and state, and `turnFrames` the frames of
/// turning left while animation 3 plays. `field_4F0` .. `field_4F8` are the
/// helper tasks the spawn routine starts and the exit callback kills.
/// `animArg` is the argument the blended reseed passes on, and `field_4FE`
/// the approach mode the last approach command selected.
typedef struct Actor450800Work {
    MATRIX          light;
    MATRIX          color;
    ActorAnimRig20  rig;
    ActorEnemyState st;
    s16             turnFrames;
    byte            pad_4EE[0x2];
    Task*           field_4F0;
    Task*           field_4F4;
    Task*           field_4F8;
    s16             animArg;
    s16             field_4FE;
    u8              field_500; // 0x7DB mode 1 latches the copied flags here, 2 the 0x84 state
    byte            pad_501[0x3];
} Actor450800Work;
STATIC_ASSERT_SIZEOF(Actor450800Work, 0x504);

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
    task->exitCallback           = func_actor_450800_80132868;
    coord->sub                   = &gGfxViewCoord;
    enemy->field_4               = &coord->coord;
    enemy->field_48              = 0;
    enemy->node.state.b.targeted = 0;
    enemy->node.state.b.flags    = 1;
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
    func_800B3F84(&work->rig.anim, D_actor_450800_8014ACC4, obj, work->rig.poses,
                  work->rig.slots);
    work->st.animId = 1;
    work->st.state  = 2;

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

    work->animArg    = 8;
    work->st.travel  = 0;
    work->turnFrames = 0;
    work->field_500  = 0;
    task->msgTable   = D_actor_450800_8014AC58;
    func_actor_450800_80132448(task);
    task->state++;
}

void func_actor_450800_80132448(Task* task)
{
    GsCOORDINATE2*   coord = ((TmdObject*)task->extra)->coords;
    Actor450800Work* work  = (Actor450800Work*)task->work;

    if (work->st.state == 1) {
        func_actor_450800_80132AE0(task);
        work->st.state = 3;
    } else if (work->st.state == 2) {
        func_actor_450800_80132A68(task);
        work->st.state = 3;
    } else if (work->st.state == 3) {
        if (work->st.animId == 0xE || work->st.animId == 2 || work->st.animId == 0xF) {
            if (work->st.travel != 0) {
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
                if (--work->st.travel == 0) {
                    work->st.state  = 1;
                    work->animArg   = 0xA;
                    work->st.animId = 0xD;
                }
            }
        }
        if (work->st.animId == 3 && work->turnFrames != 0) {
            work->st.yaw += 0x33;
            Gfx_RotMatrixY(&coord->coord, work->st.yaw, 1);
            coord->flg = 0;
            work->turnFrames--;
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
        vec     = (VECTOR3*)SCRATCH_PUSH_BYTES(0x18);
        vec->vx = coord->workm.t[0];
        vec->vy = coord->workm.t[1];
        vec->vz = coord->workm.t[2];
        Gp_DrawEffGroundQuad(vec, 0x200, Gp_State1C->groundShade);
        SCRATCH_POP_BYTES(0x18);
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
        Gp_AnimTickIndex(&work->rig.anim, i);
        i++;
    } while (i < 0x14);
}

/// Resets the actor's animation slots 1..0x13 to clip `st.animId` at rate 1,
/// without a reset argument, and latches the clip into `st.appliedAnimId`.
void func_actor_450800_80132A68(Task* task)
{
    Actor450800Work* work;
    s32              i;

    work = (Actor450800Work*)task->work;
    i    = 1;
    do {
        work->rig.slots[i].rate = 1;
        Gp_AnimResetSlot(&work->rig.anim, i, work->st.animId);
        i++;
    } while (i < 0x14);
    work->st.appliedAnimId = work->st.animId;
}

void func_actor_450800_80132AE0(Task* task)
{
    Actor450800Work* work;
    s32              i;

    work = (Actor450800Work*)task->work;
    i    = 1;
    do {
        func_800B4114(&work->rig.anim, i, work->st.animId, 0, work->animArg);
        i++;
    } while (i < 0x14);
    work->st.appliedAnimId = work->st.animId;
}

/// Script opcode: start animation `args->field_4` on this actor.
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
s32 func_actor_450800_80132B44(Task* task, s32 arg1, GpAnimArg* args)
{
    Actor450800Work* work;

    work = (Actor450800Work*)task->work;
    if (args->field_4 >= 0x1F) {
        return -1;
    }

    work->st.animId = args->field_4;
    if (args->field_8 != 0) {
        SOFT_BARRIER();
        work->st.state = 1;
        work->animArg  = args->field_C;
    } else {
        work->st.state = 2;
    }
    work->st.field_6 = 0;
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
s32 func_actor_450800_80132C68(Task* task, s32 arg1, GpXformArg* placement)
{
    GsCOORDINATE2*   coord;
    Actor450800Work* work;
    u16              yaw;

    coord        = ((TmdObject*)task->extra)->coords;
    work         = (Actor450800Work*)task->work;
    yaw          = placement->rot.vy;
    work->st.yaw = yaw;
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
s32 func_actor_450800_80132CE0(Task* task, s32 arg1, GpCmdArg* msg, s32 arg3)
{
    Actor450800Work* work  = (Actor450800Work*)task->work;
    TmdObject*       obj   = (TmdObject*)work->field_4F8->extra;
    GsCOORDINATE2*   coord = obj->coords;
    TmdObject*       self  = (TmdObject*)task->extra;
    s32              mode  = msg->command;

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
    work->st.yaw    = angle;
    if (work->field_4FE == 1) {
        work->st.yaw = angle + 0x800;
    }
    Gfx_RotMatrixY(&coord->coord, work->st.yaw, 1);
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
    work->st.travel = dist / steps;
    return 0;
}

/// Spawn handler of the enemy this actor's model task carries: state 0 of
/// `func_actor_450800_80133264`'s `fns` table. Builds the enemy's `Actor150400Work` block,
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
/// copy for the `work` store, the NULL test and `enemy`, and a longer-lived
/// one for everything after.
void func_actor_450800_80132E9C(GpEnemy* enemyArg, Task* task)
{
    GpEnemy*         enemy = enemyArg;
    VECTOR           vec;
    GpAreaKey        key;
    Actor150400Work* work;
    Actor150400Work* mem;
    GsCOORDINATE2*   coord;
    TmdObject*       obj;
    GpEnemy*         spawned;
    TmdObject*       model;
    GpAreaKey*       sessionKey;
    GpAreaKey*       keyPtr;
    u8               areaByte0;
    GpAreaRec*       rec;
    GpAreaPlace*     place;
    s32              idx;
    u32              raw;

    obj        = task->extra;
    coord      = obj->coords;
    mem        = (Actor150400Work*)memCalloc(0x4C0, false);
    work       = mem;
    task->work = (TaskIdMap*)mem;
    if (mem == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback           = func_actor_450800_8013333C;
    coord->sub                   = &gGfxViewCoord;
    enemy->field_4               = &coord->coord;
    enemy->field_48              = 0;
    enemy->node.state.b.targeted = 0;
    enemy->node.state.b.flags    = 1;
    obj->otOffset                = 1;
    mem->enemy                   = enemy;
    spawned                      = Gp_SpawnEnemyFromTable(D_actor_450800_801539DC, 1, 0, enemy);
    model                        = (TmdObject*)spawned->task->extra;
    raw                          = enemy->placeKey;
    sessionKey                   = (GpAreaKey*)&gGameSession->at4.loc;
    key.stage                    = sessionKey->stage;
    key.area                     = sessionKey->area;
    key.room                     = sessionKey->room;
    idx                          = raw >> 12;
    areaByte0                    = sessionKey->view;
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
    work->pairTask = spawned->task;
    obj->lightMtx  = &work->light;
    obj->colorMtx  = &work->color;
    vec.vx         = coord->workm.t[0];
    vec.vy         = coord->workm.t[1] - 0x320;
    vec.vz         = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&work->rig.anim, D_actor_450800_801539F4, obj,
                  &work->rig.poses, work->rig.slots);
    work->st.animId = 1;
    work->st.state  = 2;
    task->msgTable  = D_actor_450800_801539AC;
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
    Actor150400Work* work;
    s16              animId;

    work = (Actor150400Work*)task->work;
    if (work->st.state == 1) {
        func_actor_450800_801334C4(task);
        work->st.state = 3;
        return;
    }
    if (work->st.state == 2) {
        func_actor_450800_8013344C(task);
        work->st.state = 3;
        return;
    }
    if (work->st.state == 3) {
        do {
        } while (0);
        animId = work->st.animId;
        if (animId == 4 && work->st.travel != 0) {
            actorMoveModelForward(task, 0xC);
            work->st.travel = (u16)work->st.travel - 1;
            if (work->st.travel == 0) {
                work->animArg   = 0xA;
                work->st.animId = 1;
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
        vec     = (VECTOR3*)SCRATCH_PUSH_BYTES(0x18);
        vec->vx = coord->workm.t[0];
        vec->vy = coord->workm.t[1];
        vec->vz = coord->workm.t[2];
        Gp_DrawEffGroundQuad(vec, 0x200, Gp_State1C->groundShade);
        SCRATCH_POP_BYTES(0x18);
    }
}

/// Ticks the enemy's animation slots 1..0x12.
void func_actor_450800_80133400(Task* task)
{
    Actor150400Work* work;
    s32              i;

    work = (Actor150400Work*)task->work;
    i    = 1;
    do {
        Gp_AnimTickIndex(&work->rig.anim, i);
        i++;
    } while (i < 0x13);
}

/// Resets the enemy's animation slots 1..0x12 to clip `animId` at rate 1,
/// without a reset argument, and latches the clip into `appliedAnimId`.
void func_actor_450800_8013344C(Task* task)
{
    Actor150400Work* work;
    s32              i;

    work = (Actor150400Work*)task->work;
    for (i = 1; i < 0x13; i++) {
        work->rig.slots[i].rate = 1;
        Gp_AnimResetSlot(&work->rig.anim, i, work->st.animId);
    }
    work->st.appliedAnimId = work->st.animId;
}

/// Starts the enemy's animation slots 1..0x12 on clip `animId`, forwarding
/// `animArg` as the reset argument, and latches the clip into `appliedAnimId`.
void func_actor_450800_801334C4(Task* task)
{
    Actor150400Work* work;
    s32              i;

    work = (Actor150400Work*)task->work;
    i    = 1;
    do {
        func_800B4114(&work->rig.anim, i, work->st.animId, 0, work->animArg);
        i++;
    } while (i < 0x13);
    work->st.appliedAnimId = work->st.animId;
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
s32 func_actor_450800_80133528(Task* task, s32 arg1, GpAnimArg* args)
{
    Actor150400Work* work;

    work = (Actor150400Work*)task->work;
    if (args->field_4 >= 6) {
        return -1;
    }

    work->st.animId = args->field_4;
    if (args->field_8 != 0) {
        SOFT_BARRIER();
        work->st.state = 1;
        work->animArg  = args->field_C;
    } else {
        work->st.state = 2;
    }
    work->st.field_6 = 0;
    func_actor_450800_801330AC(task);
    return 0;
}

/// Message handler 0x7D5 of `D_actor_450800_801539AC`: sets `TmdObject::flags`
/// on the enemy's own model and on the sub-model task's in `pairTask` at
/// once. `flags` bit 0 selects 0 rather than 0x80, and bit 1 ORs 4 in.
s32 func_actor_450800_80133594(Task* task, s32 arg1, s32 flags)
{
    TmdObject* self;
    TmdObject* other;

    self  = (TmdObject*)task->extra;
    other = (TmdObject*)((Actor150400Work*)task->work)->pairTask->extra;

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
/// in `Actor150400Work::yaw`, then drops the placement translation into
/// the matrix and marks it dirty.
s32 func_actor_450800_801335F8(Task* task, s32 arg1, GpXformArg* placement)
{
    GsCOORDINATE2*   coord;
    Actor150400Work* work;
    u16              yaw;

    coord        = ((TmdObject*)task->extra)->coords;
    work         = (Actor150400Work*)task->work;
    yaw          = placement->rot.vy;
    work->st.yaw = yaw;
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
/// `Actor150400Work::yaw`, and leaves the horizontal distance to it, in
/// twelfths, in `travel` for the walk state to count down.
s32 func_actor_450800_80133678(Task* task, s32 arg1, VECTOR* target)
{
    GsCOORDINATE2*   coord;
    Actor150400Work* work;
    s32              dx;
    s32              dz;
    u16              yaw;

    coord        = ((TmdObject*)task->extra)->coords;
    work         = (Actor150400Work*)task->work;
    dx           = target->vx - coord->coord.t[0];
    dz           = target->vz - coord->coord.t[2];
    yaw          = ratan2(dx, dz);
    work->st.yaw = yaw;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    work->st.travel = SquareRoot0(dx * dx + dz * dz) / 12;
    return 0;
}

/// Handler of the enemy's sub-model task, the second entry of
/// `D_actor_450800_801539DC`, which the enemy's spawn handler reparents under
/// the enemy's own task. On its first tick it lights the sub-model with the
/// enemy's `Actor150400Work` matrices and hangs its root coordinate off
/// part 7 of the enemy's model; after that it only marks the coordinate dirty
/// each frame so it follows that part.
void func_actor_450800_80133740(Task* task)
{
    char             pad[0x10];
    Task*            parent = task->parent;
    TmdObject*       obj    = task->extra;
    GsCOORDINATE2*   coord  = obj->coords;
    GsCOORDINATE2*   sub    = &((TmdObject*)parent->extra)->coords[7];
    Actor150400Work* work   = (Actor150400Work*)parent->work;

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
