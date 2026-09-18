#include "common.h"
#include "main/task.h"
#include "main/tmd.h"

#include "actors/actor_450800.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/session.h"

/// Message table `func_actor_450800_80132160` hangs off `Task::field_24`, and
/// the `TaskDesc` table its three helper tasks come from - the same two roles
/// `D_actor_461800_80139F5C` / `D_actor_461800_80139F8C` play for that overlay.
extern GpMsgEntry D_actor_450800_8014AC58[];
extern TaskDesc   D_actor_450800_8014AC88[];

/// Animation data `func_800B3F84` seeds the work block's slots from.
extern u8 D_actor_450800_8014ACC4[];

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

void func_actor_450800_80132448(Task* task);
void func_actor_450800_80132868(Task* task);

void func_actor_450800_80131E34(void)
{
    GameSession* session;
    s32          temp_v0;
    s32          n;

    if ((u8)session->field_4 == 4) {
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

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800", func_actor_450800_80131F28);

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
/// The global is an `s32` (see `func_actor_450800_80131E34`, which increments
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
    if (Mc_SaveData.field_23 != 9) {
        Mc_SaveData.field_7 = 5;
        Mc_SaveData.field_6 = 0x17;
        Mc_SaveData.field_8 = 1;
        Mc_SaveData.field_5 = 1;
        D_80071076          = 1;
        Task_Spawn(0, 0x11, 0, 0);
    }
}

void func_actor_450800_801320E8(s32 arg0)
{
    func_80180038(arg0 & 0xFF);
}

INCLUDE_RODATA("actors/nonmatchings/actor_450800/actor_450800", D_actor_450800_80131E20);

INCLUDE_RODATA("actors/nonmatchings/actor_450800/actor_450800", D_actor_450800_80131E24);

void func_actor_450800_80132108(void)
{
    SVECTOR pos;

    pos = D_actor_450800_80131E24;
    Gp_SpawnEff(0x6003B, NULL, 0x200, &pos);
}

/// Spawn handler of the actor's own task, state 0 of the `fns` table
/// `func_actor_450800_80132790` dispatches through. Builds the actor's
/// `Actor450800Work` block, hangs its leading matrices off the model's
/// `field_1C` / `field_20`, and starts the animation.
///
/// The three helper tasks come out of `D_actor_450800_8014AC88`: 1 and 2 are
/// the actor's own model parts, and each is placed by the area key its
/// `Task::spawnArg2` carries. Task 4 is spawned but not placed.
///
/// The `do { } while (0)` around the second `Tmd_ProcessStream` is
/// load-bearing: the loop body is a statement of its own, so the model pointer
/// gains a reference that the bare second call does not. That reference is
/// what lifts the pointer's global-alloc priority (refs 7, not 6) past
/// `work`'s, so it takes `$s1` and pushes `work` into `$s2`, which is the
/// ROM's split. See DECOMPILATION_LEARNINGS.md, "A `do { } while (0)` around
/// one of two identical calls adds its `REF` back".
void func_actor_450800_80132160(void* enemyArg, Task* task)
{
    GpEnemy* enemy = (GpEnemy*)enemyArg;

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
    GpCdRec10*       entry1;
    GpAreaKey*       sessionKey1;
    TmdObject*       model2;
    GpCdRec10*       entry2;
    GpAreaKey*       sessionKey2;

    obj        = task->extra;
    coord      = obj->field_8;
    work       = Mem_Calloc(0x504, 0);
    task->work = (TaskIdMap*)work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback  = func_actor_450800_80132868;
    coord->sub          = &Gfx_ViewCoord;
    enemy->field_4      = &coord->coord;
    enemy->field_48     = 0;
    enemy->node.field_5 = 0;
    enemy->node.field_4 = 1;
    if ((s16)(task->spawnArg1 >> 16) == 1) {
        obj->field_C = 0;
    }
    obj->field_E  = 1;
    obj->field_1C = &work->light;
    obj->field_20 = &work->color;
    vec.vx        = coord->workm.t[0];
    vec.vy        = coord->workm.t[1] - 0x320;
    vec.vz        = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&work->anim, D_actor_450800_8014ACC4, (GpAnimObj*)obj, work->pad_374,
                  work->slots);
    work->field_4B8 = 1;
    work->state     = 2;

    spawned = Task_SpawnFromTable(D_actor_450800_8014AC88, 1, 8, 0);
    if (spawned != NULL) {
        work->field_4F0 = spawned;
        spawned->parent = task;
        model1          = spawned->extra;
        sessionKey1     = (GpAreaKey*)&Game_Session->field_4;
        raw1            = ((GpEnemy*)task->spawnArg2)->field_8;
        key.field_3     = sessionKey1->field_3;
        key.field_2     = sessionKey1->field_2;
        areaByte1       = sessionKey1->field_1;
        SOFT_BARRIER();
        keyp = &key;
        TOUCH_REG(keyp);
        key.field_1 = areaByte1;
        areaByte0   = Game_Session->field_4;
        index1      = raw1 >> 12;
        key.field_0 = areaByte0;
        Gp_SyncAreaKeyIndex(keyp);
        entry1           = (GpCdRec10*)((index1 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
        model1->field_24 = entry1->field_D;
        model1->field_25 = entry1->field_E;
        if (model1->field_18 != NULL) {
            Tmd_ProcessStream(model1);
            do {
                Tmd_ProcessStream(model1);
            } while (0);
        }
    }

    spawned = Task_SpawnFromTable(D_actor_450800_8014AC88, 2, 0xC, 0);
    if (spawned != NULL) {
        work->field_4F4 = spawned;
        spawned->parent = task;
        model2          = spawned->extra;
        sessionKey2     = (GpAreaKey*)&Game_Session->field_4;
        raw2            = ((GpEnemy*)task->spawnArg2)->field_8;
        key.field_3     = sessionKey2->field_3;
        key.field_2     = sessionKey2->field_2;
        areaByte1       = sessionKey2->field_1;
        SOFT_BARRIER();
        keyp = &key;
        TOUCH_REG(keyp);
        key.field_1 = areaByte1;
        areaByte0   = Game_Session->field_4;
        index2      = raw2 >> 12;
        key.field_0 = areaByte0;
        Gp_SyncAreaKeyIndex(keyp);
        entry2           = (GpCdRec10*)((index2 * 0x10) + (s32)Gp_GetNestedAreaRec(&key)->field_0);
        model2->field_24 = entry2->field_D;
        model2->field_25 = entry2->field_E;
        if (model2->field_18 != NULL) {
            Tmd_ProcessStream(model2);
            do {
                Tmd_ProcessStream(model2);
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
    task->field_24  = D_actor_450800_8014AC58;
    func_actor_450800_80132448(task);
    task->state++;
}

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800", func_actor_450800_80132448);

void func_actor_450800_801327E4(void* enemy, Task* task);

void func_actor_450800_80132790(Task* task)
{
    void (*fns[2])(void*, Task*) = { func_actor_450800_80132160, func_actor_450800_801327E4 };

    fns[task->state](task->spawnArg2, task);
}

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800", func_actor_450800_801327E4);

void func_actor_450800_80132868(Task* task)
{
    Actor450800Work* work = (Actor450800Work*)task->work;

    Gp_DestroyEnemy(task->spawnArg2, task);
    Task_Kill(work->field_4F0);
    Task_Kill(work->field_4F4);
    Task_Kill(work->field_4F8);
}

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800", func_actor_450800_801328BC);

/// State handler of one of the actor's model tasks: the spawn tick hangs this
/// task's own coordinate frame off part `spawnArg1` of the actor's model and
/// every later tick hands that part's world translation, dropped by 0x320 in y,
/// to `func_800D7A9C` for the part colour matrix -- the same handler as
/// `func_actor_461800_80132B74`, which reaches the parts through the global
/// task `D_actor_461800_80143898`. Here they come from `task->parent`, the
/// actor task that spawned this one and reparented it
/// (`func_actor_450800_80132160`, which also tests the same halfword on itself).
///
/// The model flags are cleared only for spawn variant 1: the high half of the
/// parent's `spawnArg1`, the halfword `actor_107600` reads the same way.
void func_actor_450800_80132958(Task* task)
{
    TmdObject*     extra = task->extra;
    GsCOORDINATE2* coord = extra->field_8;
    GsCOORDINATE2* parts = ((TmdObject*)task->parent->extra)->field_8;
    GsCOORDINATE2* part  = parts + task->spawnArg1;
    VECTOR         vec;

    switch (task->state) {
        case 0:
            coord->flg = 0;
            if ((s16)(task->parent->spawnArg1 >> 16) == 1) {
                extra->field_C = 0;
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
