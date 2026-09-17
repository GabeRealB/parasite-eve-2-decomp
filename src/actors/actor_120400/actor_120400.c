#include "common.h"

#include "actors/actor_120400.h"
#include "actors/actors_shared_801327b4.h"
#include "actors/actors_shared_80132f24.h"
#include "gameplay/1BC.h"
#include "gameplay/D4.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

/// The two child tasks the parent actor spawns, and the message table it points
/// its `Task::field_24` at; both live in this overlay's trailing data.
extern TaskDesc   D_actor_120400_8013E748;
extern GpMsgEntry D_actor_120400_8013E76C[];

/// The parent's spawn handler, the same body `func_actor_335800_80162640` and
/// `func_actor_350700_80162B30` run, with two children instead of two or three.
/// Allocates the 0x504 `Actor120400MainWork` block, seeds it, and spawns the
/// two children `D_actor_120400_8013E748` holds -- table entries 1 and 2. Each
/// has `TmdObject::field_24` / `field_25` loaded with the texture page and CLUT
/// row of the `GpAreaPlace` that entry selects, reached through the area key
/// `&Game_Session->field_4` and indexed by the model id the child's own
/// `spawnArg2` carries at `GpEnemy::field_8 >> 12`, and each then has its
/// texture stream processed twice when it has an aux buffer. The body ends by
/// handing the parent to `ActorsShared80132f24`, pointing `field_24` at the
/// message table and installing `ActorsShared801327b4` as its exit callback.
void func_actor_120400_80131E5C(Task* arg0)
{
    Actor120400MainWork* work;
    GpAreaKey            key;
    GpAreaKey*           sessionKey;
    u8*                  keyAddr;
    Task*                spawned;

    work = (Actor120400MainWork*)Mem_Calloc(0x504, false);
    if (work == NULL) {
        Gp_EnemyTaskExit(arg0);
        return;
    }
    arg0->idMap     = (TaskIdMap*)work;
    work->field_475 = -1;
    work->field_476 = -1;
    work->field_500 = -1;
    work->field_4D8 = 0;
    work->field_4DC = 0;
    work->field_4E0 = 0;
    spawned         = Task_SpawnFromTable(&D_actor_120400_8013E748, 1, 8, (s32)arg0);
    if (spawned != NULL) {
        TmdObject*   model;
        GpAreaRec*   rec;
        GpAreaPlace* place;
        s32          idx;

        model       = (TmdObject*)spawned->extra;
        idx         = ((GpEnemy*)arg0->spawnArg2)->field_8 >> 12;
        sessionKey  = (GpAreaKey*)&Game_Session->field_4;
        key.field_3 = sessionKey->field_3;
        key.field_2 = sessionKey->field_2;
        key.field_1 = sessionKey->field_1;
        key.field_0 = sessionKey->field_0;
        Gp_SyncAreaKeyIndex(&key);
        rec             = Gp_GetNestedAreaRec(&key);
        place           = (GpAreaPlace*)((idx << 4) + (s32)rec->field_0);
        model->field_24 = place->field_D;
        model->field_25 = place->field_E;
        if (model->field_18 != NULL) {
            Tmd_ProcessStream(model);
            Tmd_ProcessStream(model);
        }
    }
    spawned = Task_SpawnFromTable(&D_actor_120400_8013E748, 2, 0xC, (s32)arg0);
    if (spawned != NULL) {
        TmdObject*   model;
        GpAreaRec*   rec;
        GpAreaPlace* place;
        s32          idx;

        model = (TmdObject*)spawned->extra;
        idx   = ((GpEnemy*)arg0->spawnArg2)->field_8 >> 12;
        /* Re-derived address, not the block-1 form: with `sessionKey->field_0`
           for the last byte, global CSE merges this block's area key with the
           one above into a single cross-block pseudo, and the allocation of
           `spawned` and every address temp after it shifts. */
        sessionKey  = (GpAreaKey*)(keyAddr = (u8*)&Game_Session->field_4);
        key.field_3 = sessionKey->field_3;
        key.field_2 = sessionKey->field_2;
        key.field_1 = ((GpAreaKey*)keyAddr)->field_1;
        key.field_0 = ((GpAreaKey*)(&Game_Session->field_4))->field_0;
        Gp_SyncAreaKeyIndex(&key);
        rec             = Gp_GetNestedAreaRec(&key);
        place           = (GpAreaPlace*)((idx << 4) + (s32)rec->field_0);
        model->field_24 = place->field_D;
        model->field_25 = place->field_E;
        if (model->field_18 != NULL) {
            Tmd_ProcessStream(model);
            Tmd_ProcessStream(model);
        }
    }
    ActorsShared80132f24(arg0);
    arg0->field_24     = D_actor_120400_8013E76C;
    arg0->exitCallback = ActorsShared801327b4;
    arg0->state       += 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_120400/actor_120400", func_actor_120400_80132050);

INCLUDE_ASM("actors/nonmatchings/actor_120400/actor_120400", func_actor_120400_80132254);

INCLUDE_ASM("actors/nonmatchings/actor_120400/actor_120400", func_actor_120400_80132398);

INCLUDE_RODATA("actors/nonmatchings/actor_120400/actor_120400", D_actor_120400_80131E20);

INCLUDE_RODATA("actors/nonmatchings/actor_120400/actor_120400", ActorsShared80138404Table);

INCLUDE_RODATA("actors/nonmatchings/actor_120400/actor_120400", D_actor_120400_80131E30);
