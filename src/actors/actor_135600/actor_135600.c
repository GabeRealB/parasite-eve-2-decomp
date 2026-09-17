#include "common.h"

#include "decomp/common.h"

#include "actors/actor_135600.h"
#include "actors/actors_shared_801327b4.h"
#include "actors/actors_shared_80132f24.h"

#include "gameplay/1BC.h"
#include "gameplay/D4.h"

#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

/// The actor's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern TaskFuncTable3 D_actor_135600_80131E24;

/// Child task table the setup handler spawns from: index 1 is the actor's
/// walking model, index 2 the collision one and index 3 a model-less helper.
extern TaskDesc D_actor_135600_8013B0C4;

/// The actor's message table, stored in `Task::field_24`: 0x7D3, 0x7D4 and
/// 0x7D5 against the handlers below.
extern GpMsgEntry D_actor_135600_8013B0F4[];

/// Declared locally rather than taken from `actors_shared_8013231c.h`: the
/// overlay calls it through its message dispatch, which passes a fourth,
/// unused argument.
s32 ActorsShared8013231c(Task* task, s32 msgId, Actor135600PlaceArgs* args, s32 arg3);

/// The 0x7D3 entry of `D_actor_135600_8013B0F4`, applied to the work block at
/// 0x50C in `actor_135600_4`.
s32 func_actor_135600_801330A8(Task* task, s32 msgId, Actor135600AnimPreset* preset, s32 arg3);

/// The 0x7D5 entry of `D_actor_135600_8013B0F4`: the actor's own visibility,
/// switched on the word `mode` rather than on a pointer.
s32 func_actor_135600_80133240(Task* task, s32 msgId, s32 mode, s32 arg3);

INCLUDE_ASM("actors/nonmatchings/actor_135600/actor_135600", func_actor_135600_80131E68);

void func_actor_135600_80132234(Task* task)
{
    Actor135600Work*      work;
    Task*                 spawned;
    TmdObject*            model1;
    TmdObject*            model2;
    GpAreaKey*            sessionKey1;
    GpAreaKey*            keyp1;
    GpAreaKey*            sessionKey2;
    GpAreaKey*            keyp2;
    GpCdRec10*            entry1;
    GpCdRec10*            entry2;
    u32                   index1;
    u32                   index2;
    u32                   raw1;
    u32                   raw2;
    u8                    areaByte0;
    u8                    areaByte1;
    Actor135600PlaceArgs  args;
    Actor135600AnimPreset preset;
    GpAreaKey             key;

    work = (Actor135600Work*)Mem_Calloc(0x50C, false);
    if (work == NULL) {
        Gp_EnemyTaskExit(task);
        return;
    }
    task->idMap     = (TaskIdMap*)work;
    work->field_475 = -1;
    work->field_476 = -1;
    work->field_508 = -1;
    work->field_4D8 = 0;
    work->field_4DC = 0;
    work->field_4E0 = 0;

    spawned = Task_SpawnFromTable(&D_actor_135600_8013B0C4, 1, 8, (s32)task);
    if (spawned != NULL) {
        work->field_500 = spawned;
        model1          = (TmdObject*)spawned->extra;
        raw1            = ((GpEnemy*)task->spawnArg2)->field_8;
        sessionKey1     = (GpAreaKey*)&Game_Session->field_4;
        key.field_3     = sessionKey1->field_3;
        key.field_2     = sessionKey1->field_2;
        /* Both calls below hand `key` to Gp_SyncAreaKeyIndex and then to
         * Gp_GetNestedAreaRec. Read as one straight-line block, the two
         * `&key` arguments global-CSE into a single address pseudo that then
         * has to survive the first call, which costs `$s4` and shifts `work`,
         * `model` and the index up a register each; the barrier plus the
         * touched pointer pin the block's shape, and each call recomputes the
         * address the way the target does. The same two files'
         * `Actor401300_TintEffect` and `func_actor_450800_80132160` are the
         * worked examples of the idiom. */
        SOFT_BARRIER();
        keyp1       = &key;
        key.field_1 = sessionKey1->field_1;
        TOUCH_REG(keyp1);
        areaByte0   = Game_Session->field_4;
        index1      = raw1 >> 12;
        key.field_0 = areaByte0;
        Gp_SyncAreaKeyIndex(keyp1);
        entry1           = (GpCdRec10*)((index1 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
        model1->field_24 = entry1->field_D;
        model1->field_25 = entry1->field_E;
        if (model1->field_18 != NULL) {
            Tmd_ProcessStream(model1);
            Tmd_ProcessStream(model1);
        }
    }

    spawned = Task_SpawnFromTable(&D_actor_135600_8013B0C4, 2, 0xC, (s32)task);
    if (spawned != NULL) {
        work->field_4FC = spawned;
        model2          = (TmdObject*)spawned->extra;
        raw2            = ((GpEnemy*)task->spawnArg2)->field_8;
        sessionKey2     = (GpAreaKey*)&Game_Session->field_4;
        key.field_3     = sessionKey2->field_3;
        key.field_2     = sessionKey2->field_2;
        SOFT_BARRIER();
        keyp2       = &key;
        key.field_1 = sessionKey2->field_1;
        TOUCH_REG(keyp2);
        areaByte1   = Game_Session->field_4;
        index2      = raw2 >> 12;
        key.field_0 = areaByte1;
        Gp_SyncAreaKeyIndex(keyp2);
        entry2           = (GpCdRec10*)((index2 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
        model2->field_24 = entry2->field_D;
        model2->field_25 = entry2->field_E;
        if (model2->field_18 != NULL) {
            Tmd_ProcessStream(model2);
            Tmd_ProcessStream(model2);
        }
    }

    spawned = Task_SpawnFromTable(&D_actor_135600_8013B0C4, 3, 8, (s32)task);
    if (spawned != NULL) {
        work->field_504 = spawned;
    }

    ActorsShared80132f24(task);

    args.pos.vx = 0xA6E;
    args.pos.vz = 0x5F0;
    args.pos.vy = 0;
    args.rot.vx = 0;
    args.rot.vy = 0x400;
    args.rot.vz = 0;
    ActorsShared8013231c(task, 0x7D4, &args, 0);

    preset.field_0 = 0;
    preset.field_4 = 2;
    preset.field_8 = 0;
    func_actor_135600_801330A8(task, 0x7D3, &preset, 0);

    func_actor_135600_80133240(task, 0x7D5, 1, 0);

    task->field_24     = D_actor_135600_8013B0F4;
    task->exitCallback = ActorsShared801327b4;
    task->state       += 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_135600/actor_135600", func_actor_135600_801324D0);

INCLUDE_ASM("actors/nonmatchings/actor_135600/actor_135600", func_actor_135600_801326E8);

INCLUDE_ASM("actors/nonmatchings/actor_135600/actor_135600", func_actor_135600_8013282C);

INCLUDE_RODATA("actors/nonmatchings/actor_135600/actor_135600", D_actor_135600_80131E20);

INCLUDE_RODATA("actors/nonmatchings/actor_135600/actor_135600", D_actor_135600_80131E24);

void func_actor_135600_801329E0(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_135600_80131E24;
    sp.funcs[task->state](task);
}

INCLUDE_RODATA("actors/nonmatchings/actor_135600/actor_135600", D_actor_135600_80131E30);

INCLUDE_RODATA("actors/nonmatchings/actor_135600/actor_135600", D_actor_135600_80131E3C);
