#include "common.h"

#include "actors/actor_443500.h"
#include "actors/actors_shared_801327b4.h"
#include "actors/actors_shared_80132f24.h"

#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/D4.h"

extern s8 D_8007272D;

/// Default animation arguments, 0x14 bytes: `{ NULL, 0x1C, 1, 4, 0 }`.
extern GpAnimArg D_actor_443500_80158728;

/// The actor's two-entry `TaskDesc` table; the spawn handler starts entry 1.
extern TaskDesc D_actor_443500_8015873C;

/// The actor's animation table: `(anim id, handler)` pairs for 0x7D3 / 0x7D4 /
/// 0x7D5, ended by `0x7FFFFFFF`. The spawn handler parks its address in
/// `Task::msgTable` (0x24).
extern s32 D_actor_443500_80158754;

void func_actor_443500_8013206C(s8 arg0)
{
    D_8007272D = arg0;
}

/// Spawn handler: allocates the work block, seeds its head from the parent
/// model, starts the actor's child task and copies the location it spawns over
/// from the session key onto that child's model, then installs the animation
/// table, the exit callback and the tick handler.
void func_actor_443500_80132078(Task* task)
{
    Actor443500Work* work;
    GpAreaKey        key;
    GpAreaKey*       sessionKey;
    u8               areaByte0;
    GpAreaRec*       rec;
    GpCdRec10*       entry;
    TmdObject*       model;
    Task*            spawned;
    s32              idx;
    u32              raw;

    work = memCalloc(0x4C4, 0);
    if (work == NULL) {
        Gp_EnemyTaskExit(task);
        return;
    }
    task->work      = (TaskIdMap*)work;
    work->field_475 = -1;
    work->field_476 = -1;
    work->field_4BC = -1;
    work->field_4C0 = ((TmdObject*)task->extra)->flags;
    spawned         = Task_SpawnFromTable(&D_actor_443500_8015873C, 1, 4, (s32)task);
    if (spawned != NULL) {
        sessionKey = (GpAreaKey*)&gGameSession->at4.loc;
        raw        = ((Actor443500Ctx*)task->spawnArg2)->field_8;
        model      = (TmdObject*)spawned->extra;
        key.stage  = sessionKey->stage;
        key.area   = sessionKey->area;
        key.room   = sessionKey->room;
        areaByte0  = sessionKey->view;
        idx        = raw >> 12;
        key.view   = areaByte0;
        Gp_SyncAreaKeyIndex(&key);
        rec = Gp_GetNestedAreaRec(&key);
        /* offset + base, not `&rec->field_0[idx]`: the ROM adds the scaled
           index onto the table (`addu s0, s0, v0`). */
        entry        = (GpCdRec10*)((idx << 4) + (s32)rec->field_0);
        model->tpage = entry->field_D;
        model->clut  = entry->field_E;
        if (model->buffer != NULL) {
            tmdProcessStream(model);
            tmdProcessStream(model);
        }
    }
    func_actor_443500_8013297C(task, 0x7D5, 0, 0);
    func_actor_443500_801327E0(task, 0x7D3, &D_actor_443500_80158728, 0);
    ActorsShared80132f24(task);
    task->msgTable     = &D_actor_443500_80158754;
    task->exitCallback = ActorsShared801327b4;
    task->state++;
}

INCLUDE_ASM("actors/nonmatchings/actor_443500/actor_443500_2", func_actor_443500_801321F0);
