#include "common.h"

#include <psyq/abs.h>

#include "actors/actor_350700.h"
#include "actors/actors_shared_801327f8.h"
#include "actors/actors_shared_80132f24.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

/// The four `TaskDesc`s `func_actor_350700_80162B30` spawns its child tasks
/// from, and the message table it points the parent's `Task::msgTable` at:
/// ids 0x7D3/0x7D4/0x7D5/0x7DD/0x7DB against the handlers starting
/// `func_actor_350700_801636A8`, terminated by 0x7FFFFFFF.
extern TaskDesc   D_actor_350700_801708DC;
extern GpMsgEntry D_actor_350700_8017090C[];

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700_6", func_actor_350700_80162AF4);

/// The parent's spawn handler, the same body `func_actor_335800_80162640` runs.
/// Allocates the 0x50C `Actor350700MainWork` block, seeds it, and spawns the
/// three children `D_actor_350700_801708DC` holds -- table entries 1, 2 and 3 --
/// parking them at `field_4FC` / `field_500` / `field_504`. The first two are
/// models: each has `TmdObject::tpage` / `field_25` loaded with the texture
/// page and CLUT row of the `GpAreaPlace` that entry selects, reached through
/// the area key `&gGameSession->at4.loc.view` and indexed by the model id the child's
/// own `spawnArg2` carries at `GpEnemy::placeKey >> 12`, and each then has its
/// texture stream processed twice when it has an aux buffer. The body ends by
/// handing the parent to `ActorsShared80132f24`, pointing `msgTable` at the
/// message table and installing `func_actor_350700_801633BC` as its exit
/// callback.
void func_actor_350700_80162B30(Task* arg0)
{
    Actor350700MainWork* work;
    GpAreaKey            key;
    GpAreaKey*           sessionKey;
    u8*                  keyAddr;
    Task*                spawned;

    work = (Actor350700MainWork*)memCalloc(0x50C, false);
    if (work == NULL) {
        Gp_EnemyTaskExit(arg0);
        return;
    }
    arg0->work      = (TaskIdMap*)work;
    work->field_475 = -1;
    work->field_476 = -1;
    work->field_508 = -1;
    work->field_4D8 = 0;
    work->field_4DC = 0;
    work->field_4E0 = 0;
    spawned         = Task_SpawnFromTable(&D_actor_350700_801708DC, 1, 8, (s32)arg0);
    if (spawned != NULL) {
        TmdObject*   model;
        GpAreaRec*   rec;
        GpAreaPlace* place;
        s32          idx;

        work->field_4FC = spawned;
        model           = (TmdObject*)spawned->extra;
        idx             = ((GpEnemy*)arg0->spawnArg2)->placeKey >> 12;
        sessionKey      = (GpAreaKey*)&gGameSession->at4.loc;
        key.stage       = sessionKey->stage;
        key.area        = sessionKey->area;
        key.room        = sessionKey->room;
        key.view        = sessionKey->view;
        Gp_SyncAreaKeyIndex(&key);
        rec          = Gp_GetNestedAreaRec(&key);
        place        = (GpAreaPlace*)((idx << 4) + (s32)rec->field_0);
        model->tpage = place->tpage;
        model->clut  = place->clut;
        if (model->buffer != NULL) {
            tmdProcessStream(model);
            tmdProcessStream(model);
        }
    }
    spawned = Task_SpawnFromTable(&D_actor_350700_801708DC, 2, 0xC, (s32)arg0);
    if (spawned != NULL) {
        TmdObject*   model;
        GpAreaRec*   rec;
        GpAreaPlace* place;
        s32          idx;

        work->field_500 = spawned;
        model           = (TmdObject*)spawned->extra;
        idx             = ((GpEnemy*)arg0->spawnArg2)->placeKey >> 12;
        /* Re-derived address, not the block-1 form: with `sessionKey->field_0`
           for the last byte, global CSE merges this block's area key with the
           one above into a single cross-block pseudo, and the allocation of
           `spawned` and every address temp after it shifts. */
        sessionKey = (GpAreaKey*)(keyAddr = (u8*)&gGameSession->at4.loc.view);
        key.stage  = sessionKey->stage;
        key.area   = sessionKey->area;
        key.room   = ((GpAreaKey*)keyAddr)->room;
        key.view   = ((GpAreaKey*)(&gGameSession->at4.loc.view))->view;
        Gp_SyncAreaKeyIndex(&key);
        rec          = Gp_GetNestedAreaRec(&key);
        place        = (GpAreaPlace*)((idx << 4) + (s32)rec->field_0);
        model->tpage = place->tpage;
        model->clut  = place->clut;
        if (model->buffer != NULL) {
            tmdProcessStream(model);
            tmdProcessStream(model);
        }
    }
    spawned = Task_SpawnFromTable(&D_actor_350700_801708DC, 3, 8, (s32)arg0);
    if (spawned != NULL) {
        work->field_504 = spawned;
    }
    ActorsShared80132f24(arg0);
    arg0->msgTable     = D_actor_350700_8017090C;
    arg0->exitCallback = func_actor_350700_801633BC;
    arg0->state       += 1;
}

/// Per-frame tick of the parent actor, the same body as
/// `func_actor_335800_80162844`: dispatches through the local two-entry table
/// `field_4F8` indexes -- the empty `func_actor_350700_801633F8` or the shared
/// `ActorsShared801327f8` -- then integrates the per-frame deltas at
/// `field_4C8..field_4D0` into the 16.16 accumulators at `field_4D8..field_4E0`,
/// adds their high halves to the root coordinate's translation, clears `flg`
/// and truncates the accumulators back to 16 bits. Ticks the animation slots
/// while `field_474` is set; and, unless the display object's `field_C` carries
/// 0x80, draws the ground-shadow quad from the second part's world matrix.
/// While `gGameSession->viewReady` is set it also clears that part's `flg`,
/// rebuilds its coordinate and rebuilds the actor colour; the colour rebuild
/// runs once more unconditionally. The `field_508` countdown then runs while it
/// is non-negative, freeing the model buffers on the frame it reaches zero; the
/// init's -1 disables it.
void func_actor_350700_80162D5C(Task* arg0)
{
    TmdObject*           ext      = arg0->extra;
    Actor350700MainWork* work     = (Actor350700MainWork*)arg0->work;
    TaskFunc             funcs[2] = { func_actor_350700_801633F8, ActorsShared801327f8 };
    VECTOR3              pos;
    GsCOORDINATE2*       coord;
    s32                  i;

    funcs[work->field_4F8](arg0);
    coord              = ((TmdObject*)arg0->extra)->coords;
    work->field_4D8   += work->field_4C8;
    work->field_4DC   += work->field_4CC;
    work->field_4E0   += work->field_4D0;
    coord->coord.t[0] += (s16)(work->field_4D8 >> 16);
    coord->coord.t[1] += (s16)(work->field_4DC >> 16);
    coord->coord.t[2] += (s16)(work->field_4E0 >> 16);
    coord->flg         = 0;
    work->field_4D8    = (u16)work->field_4D8;
    work->field_4DC    = (u16)work->field_4DC;
    work->field_4E0    = (u16)work->field_4E0;
    if (work->field_474 != 0) {
        for (i = 1; i < 0x14; i++) {
            Gp_AnimTickIndex((GpAnimCtx*)work, i);
        }
    }
    if (!(ext->flags & 0x80)) {
        if (func_800EA1A8((VECTOR3*)((TmdObject*)arg0->extra)->coords[1].workm.t, &pos) != 0) {
            Gp_DrawEffGroundQuad(&pos, 0x300, Gp_State1C->groundShade);
        }
    }
    if (gGameSession->viewReady != 0) {
        ((TmdObject*)arg0->extra)->coords[1].flg = 0;
        Gp_UpdateCoord(&((TmdObject*)arg0->extra)->coords[1]);
        func_800D7A9C(ext, (VECTOR*)((TmdObject*)arg0->extra)->coords[1].workm.t, 0, 3);
    }
    func_800D7A9C(ext, (VECTOR*)((TmdObject*)arg0->extra)->coords[1].workm.t, 0, 3);
    if (work->field_508 >= 0) {
        if (work->field_508 == 0) {
            Tmd_FreeBuffers(ext);
        }
        work->field_508--;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700_6", func_actor_350700_80162F7C);

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700_6", func_actor_350700_801630C0);

INCLUDE_RODATA("actors/nonmatchings/actor_350700/actor_350700_6", ActorsShared801327f8Table);

INCLUDE_RODATA("actors/nonmatchings/actor_350700/actor_350700_6", ActorsShared80132920Offset);
