#include "common.h"

#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/D4.h"

#include "actors/actor_150400.h"
#include "actors/actors_shared_801366fc.h"

extern TaskDesc D_actor_150400_80132CF0;
extern TaskDesc D_80181BBC;
extern Task*    D_actor_150400_8013C924;
extern Task*    D_actor_150400_8013C928;

extern TaskDesc   D_actor_150400_8013C8F4[];
extern u8         D_actor_150400_8013C90C[];
extern GpMsgEntry D_actor_150400_8013C8C4[];

INCLUDE_ASM("actors/nonmatchings/actor_150400/actor_150400", func_actor_150400_80131ECC);

void func_actor_150400_80131F6C(void)
{
    Task_SpawnFromTable(&D_80181BBC, 0, 0, 0);
}

void func_actor_150400_80131F9C(s32 arg0)
{
    D_actor_150400_8013C924->state = arg0;
    D_actor_150400_8013C928->state = arg0;
}

void func_actor_150400_80131FB8(void)
{
    D_actor_150400_8013C924 = Task_SpawnFromTable(&D_actor_150400_80132CF0, 0, 1, 0);
    D_actor_150400_8013C928 = Task_SpawnFromTable(&D_actor_150400_80132CF0, 0, 2, 0);
}

/// Spawn handler shared with the other `ActorsShared80131f9c` overlays: builds
/// the actor's work block, starts the animation and hands the state machine to
/// `func_actor_150400_80132228`.
///
/// Two codegen pins, both load-bearing. `key` lands at `vfp+0x10`, so `&key` is
/// expensive enough that `expand_call` precomputes it; left alone CSE merges
/// the two call-site copies into one pseudo live across the first call and the
/// address costs a callee-saved register. The `SOFT_BARRIER()` keeps each
/// materialization next to its own call and `TOUCH_REG` makes the second a
/// fresh computation, which is the ROM's `addiu $a0,$sp,0x28` twice. The
/// `mem`/`work` pair is the same kind of pin on the `Mem_Calloc` result: the
/// ROM keeps a short-lived copy for the `idMap` store, the NULL test and
/// `field_4BC`, and a longer-lived one for everything after, which one variable
/// cannot express.
void ActorsShared80131e24Sub0(GpEnemy* enemy, Task* task)
{
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

    obj         = task->extra;
    coord       = obj->field_8;
    mem         = (Actor150400Work*)Mem_Calloc(0x4C0, false);
    work        = (Actor150400Work*)mem;
    task->idMap = (TaskIdMap*)mem;
    if (mem == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback  = ActorsShared801366fc;
    coord->sub          = &Gfx_ViewCoord;
    enemy->field_4      = &coord->coord;
    enemy->field_48     = 0;
    enemy->node.field_5 = 0;
    enemy->node.field_4 = 1;
    obj->field_C        = 0;
    obj->field_E        = 1;
    mem->field_4BC      = enemy;
    spawned             = Gp_SpawnEnemyFromTable(D_actor_150400_8013C8F4, 1, 0, enemy);
    model               = (TmdObject*)spawned->task->extra;
    raw                 = enemy->field_8;
    sessionKey          = (GpAreaKey*)&Game_Session->field_4;
    key.field_3         = sessionKey->field_3;
    key.field_2         = sessionKey->field_2;
    key.field_1         = sessionKey->field_1;
    idx                 = raw >> 12;
    areaByte0           = sessionKey->field_0;
    SOFT_BARRIER();
    keyPtr = &key;
    TOUCH_REG(keyPtr);
    key.field_0 = areaByte0;
    Gp_SyncAreaKeyIndex(keyPtr);
    rec = Gp_GetNestedAreaRec(&key);
    /* offset + base, not `&rec->field_0[idx]`: the ROM adds the scaled index
       onto the table (`addu s0, s0, v0`). */
    place           = (GpAreaPlace*)((idx << 4) + (s32)rec->field_0);
    model->field_24 = place->field_D;
    model->field_25 = place->field_E;
    if (model->field_18 != NULL) {
        Tmd_ProcessStream(model);
        Tmd_ProcessStream(model);
    }
    Task_Reparent(task, spawned->task);
    work->field_4B8 = spawned->task;
    obj->field_1C   = &work->light;
    obj->field_20   = &work->color;
    vec.vx          = coord->workm.t[0];
    vec.vy          = coord->workm.t[1] - 0x320;
    vec.vz          = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&work->anim, D_actor_150400_8013C90C, (GpAnimObj*)obj,
                  &work->field_34C, work->slots);
    work->animId   = 1;
    work->state    = 2;
    task->field_24 = D_actor_150400_8013C8C4;
    func_actor_150400_80132228(task);
    task->state++;
}

INCLUDE_ASM("actors/nonmatchings/actor_150400/actor_150400", func_actor_150400_80132228);

INCLUDE_RODATA("actors/nonmatchings/actor_150400/actor_150400", D_actor_150400_80131E20);
