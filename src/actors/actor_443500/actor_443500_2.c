#include "common.h"

#include "actors/actor_443500.h"
#include "actors/actors_shared_801327b4.h"
#include "actors/actors_shared_80132f24.h"

#include "main/gameflag.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

extern s8 D_8007272D;

void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

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
    GpAreaPlace*     entry;
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
        entry        = (GpAreaPlace*)((idx << 4) + (s32)rec->field_0);
        model->tpage = entry->tpage;
        model->clut  = entry->clut;
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

/// Per-frame tick: while the view is live and idle, views 0..3 hide the model
/// (saving `TmdObject::flags` into `field_4C0`) and views 4..5 restore that
/// saved word, showing the model through message 0x7D5 when flag 0x83 is set.
/// Ticks animation slots 1..0x13 once `field_474` is latched, restarting 0x7D3
/// when slot 1 reports the clip ended. The `field_475 == 0x1C` path is the
/// default clip's sound: `field_4BA` counts to 0xF for a Type6 (views 4/5) or
/// Type7 (view 3) cue, TypeA otherwise while the view is ready, and resets on
/// slot 1's control-entry bit. A visible model gets a ground shadow and a
/// rebuilt child-part matrix; `field_4BC` then counts down to free the buffers.
void func_actor_443500_801321F0(Task* task)
{
    Actor443500Work* work;
    TmdObject*       extra;
    VECTOR3          pos;
    s32              i;
    u8               view;

    extra = (TmdObject*)task->extra;
    work  = (Actor443500Work*)task->work;
    if (gGameSession->viewReady != 0 && gGameSession->eventState == 0 &&
        gGameSession->cutsceneHold == 0) {
        view = gGameSession->at4.loc.view;
        if (view < 4) {
            work->field_4C0 = extra->flags;
            extra->flags    = extra->flags | 0x80;
        } else if (view < 6) {
            if (GameFlag_GetNibble(0x83) > 0) {
                func_actor_443500_80132A68(0);
                func_actor_443500_8013297C(task, 0x7D5, 1, 0);
            }
            extra->flags = work->field_4C0;
        }
    }
    if (work->field_474 != 0) {
        for (i = 1; i < 0x14; i++) {
            Gp_AnimTickIndex(&work->anim, i);
        }
        if (gGameSession->eventState == 0 && (work->slots[1].flags & 1)) {
            func_actor_443500_801327E0(task, 0x7D3, &D_actor_443500_80158728, 0);
        }
    }
    if (work->field_475 == 0x1C) {
        work->field_4BA++;
        if (work->field_4BA == 0xF) {
            switch (gGameSession->at4.loc.view) {
                case 5:
                    SndEvt_EnqueueType6(0x542F0001, 9, 0);
                    break;
                case 4:
                    SndEvt_EnqueueType6(0x542F0001, -0xA, 0x40);
                    break;
                case 3:
                    SndEvt_EnqueueType7(0x542F0001, 0x1E);
                    break;
            }
        } else if (gGameSession->viewReady != 0) {
            switch (gGameSession->at4.loc.view) {
                case 5:
                    SndEvt_EnqueueTypeA(0x542F0001, 9, 0);
                    break;
                case 4:
                    SndEvt_EnqueueTypeA(0x542F0001, -0xA, 0x40);
                    break;
                case 3:
                    SndEvt_EnqueueType7(0x542F0001, 0x1E);
                    break;
            }
        }
        if (work->slots[1].flags & 2) {
            work->field_4BA = 0;
        }
    }
    if (!(extra->flags & 0x80)) {
        if (func_800EA1A8((VECTOR3*)((TmdObject*)task->extra)->coords[1].workm.t, &pos) != 0) {
            Gp_DrawEffGroundQuad(&pos, 0x300, Gp_State1C->groundShade);
        }
        if (gGameSession->viewReady != 0) {
            Gp_UpdateCoord(&((TmdObject*)task->extra)->coords[1]);
            func_800D7A9C(extra, (VECTOR*)((TmdObject*)task->extra)->coords[1].workm.t, 0, 3);
        }
    }
    if (work->field_4BC >= 0) {
        if (work->field_4BC == 0) {
            Tmd_FreeBuffers(extra);
        }
        work->field_4BC--;
    }
}
