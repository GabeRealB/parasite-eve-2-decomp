#include "common.h"

#include "actors/actor_135400.h"
#include "actors/actors_shared_80132f24.h"
#include "actors/actors_shared_8016a538.h"

#include "main/gameflag.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/gfx.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"

/// The actor's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern TaskFuncTable3 D_actor_135400_80131E24;
extern TaskFuncTable3 D_actor_135400_80131E30;

/// Second state handler of the actor's part-2 table (`D_actor_135400_80131E30`,
/// dispatched by `func_actor_135400_801324D4`): a three-phase machine run off
/// `Task::spawnArg1`, the slot the part's state-0 handler read as the part
/// index and sets to 2 for the second part. Phase 1 bakes the part's
/// parent-relative coordinate into world space with `Gp_ComposeParentWorld` and
/// reparents it to `gGfxViewCoord`. Phases 2 and 3 share a body -- 2 only
/// reaches it while the session's `eventState` is clear -- which resets the
/// coordinate to a `-0x38E` yaw (`func_8004BFF8`, `RotMatrixY`) at the fixed
/// world position (0x12FE, -0x1B3, 0x157C) and drops the phase back to 0.
void func_actor_135400_80131EB4(Task* task)
{
    ActorsShared8016a538Mat  rot;
    ActorsShared8016a538Mat* src;
    SVECTOR                  sv;
    GsCOORDINATE2*           coord;

    switch (task->spawnArg1) {
        case 1:
            coord = ((TmdObject*)task->extra)->coords;
            Gp_ComposeParentWorld(coord, &rot.mat, &sv);
            coord->coord      = rot.mat;
            coord->coord.t[0] = sv.vx;
            coord->coord.t[1] = sv.vy;
            coord->coord.t[2] = sv.vz;
            coord->sub        = &gGfxViewCoord;
            coord->flg        = 0;
            task->spawnArg1  += 1;
            break;
        case 2:
            if (gGameSession->eventState != 0) {
                break;
            }
        case 3:
            coord              = ((TmdObject*)task->extra)->coords;
            src                = &rot;
            src->ident.m00_m01 = 0x1000;
            src->ident.m02_m10 = 0;
            src->ident.m11_m12 = 0x1000;
            src->ident.m20_m21 = 0;
            src->ident.m22     = 0x1000;
            func_8004BFF8(-0x38E, &rot.mat);
            coord->coord      = rot.mat;
            coord->coord.t[0] = 0x12FE;
            coord->coord.t[1] = -0x1B3;
            coord->coord.t[2] = 0x157C;
            coord->sub        = &gGfxViewCoord;
            coord->flg        = 0;
            task->spawnArg1   = 0;
            break;
    }
}

INCLUDE_RODATA("actors/nonmatchings/actor_135400/actor_135400", D_actor_135400_80131E20);

INCLUDE_RODATA("actors/nonmatchings/actor_135400/actor_135400", D_actor_135400_80131E24);

INCLUDE_RODATA("actors/nonmatchings/actor_135400/actor_135400", D_actor_135400_80131E30);

INCLUDE_RODATA("actors/nonmatchings/actor_135400/actor_135400", D_actor_135400_80131E3C);

/// The spawn handler of the actor's main task: carves the 0x4C8-byte work
/// block, seeds its two `-1` latches, starts the two part tasks and copies the
/// area record's texture page / CLUT onto part 1's model. It then installs the
/// handler table, the 0x7D5 model mode and the exit callback, and finally hands
/// the 0x7D4 placement and the 0x7D3 animation the game flag 0x6C selects.
///
/// `keyPtr` / `TOUCH_REG` are load-bearing: the `&key` argument comes out of
/// `expand_call` in a fresh pseudo, and left alone `cse` folds the second call
/// site into it, so the address lives across the first call and costs a
/// callee-saved register. The touch makes the second call materialise it afresh
/// -- the ROM's two `addiu $a0,$sp,0x68`.
void func_actor_135400_80132064(Task* arg0)
{
    Actor135400MainWork* work;
    Actor135400Places    places;
    GpAnimArg            anim[2];
    GpAreaKey            key;
    GpAreaKey*           sessionKey;
    GpAreaKey*           keyPtr;
    u8                   areaByte0;
    GpAreaRec*           rec;
    GpAreaPlace*         place;
    TmdObject*           model;
    Task*                spawned;
    u32                  raw;
    s32                  idx;

    places = D_actor_135400_80131E48;
    memset(anim, 0, sizeof(anim));
    anim[0].field_4 = 1;
    anim[1].field_4 = 4;
    work            = (Actor135400MainWork*)memCalloc(0x4C8, 0);
    if (work == NULL) {
        Gp_EnemyTaskExit(arg0);
        return;
    }
    arg0->work      = (TaskIdMap*)work;
    work->field_475 = -1;
    work->field_476 = -1;
    spawned         = Task_SpawnFromTable(&D_actor_135400_8013A4AC, 1, 4, (s32)arg0);
    if (spawned != NULL) {
        work->field_4B8 = spawned;
        model           = (TmdObject*)spawned->extra;
        sessionKey      = (GpAreaKey*)&gGameSession->at4.loc;
        raw             = ((GpEnemy*)arg0->spawnArg2)->placeKey;
        key.stage       = sessionKey->stage;
        key.area        = sessionKey->area;
        key.room        = sessionKey->room;
        areaByte0       = sessionKey->view;
        keyPtr          = &key;
        TOUCH_REG(keyPtr);
        key.view = areaByte0;
        idx      = raw >> 12;
        Gp_SyncAreaKeyIndex(keyPtr);
        rec          = Gp_GetNestedAreaRec(&key);
        place        = (GpAreaPlace*)((idx << 4) + (s32)rec->field_0);
        model->tpage = place->field_D;
        model->clut  = place->field_E;
        if (model->buffer != NULL) {
            tmdProcessStream(model);
            tmdProcessStream(model);
        }
    }
    spawned = Task_SpawnFromTable(&D_actor_135400_8013A4AC, 2, 8, (s32)arg0);
    if (spawned != NULL) {
        work->field_4BC = spawned;
    }
    ActorsShared80132f24(arg0);
    arg0->msgTable = &D_actor_135400_8013A4D0;
    func_actor_135400_801327E8(arg0, 0x7D5, 1, 0);
    if (GameFlag_GetNibble(0x6C) <= 0) {
        func_actor_135400_8013276C(arg0, 0x7D4, &places.field_0, 0);
        func_actor_135400_80132650(arg0, 0x7D3, &anim[0], 0);
        func_80180414(0);
    } else {
        func_actor_135400_8013276C(arg0, 0x7D4, &places.field_18, 0);
        func_actor_135400_80132650(arg0, 0x7D3, &anim[1], 0);
    }
    arg0->exitCallback = func_actor_135400_80132614;
    arg0->state       += 1;
}

/// The two spawn placements `func_actor_135400_80132064` copies as a whole:
/// the flag-clear branch's is the one at 0x0, the other is 0x18. Defined here,
/// after the four state tables' `INCLUDE_RODATA` lines, so `.rodata` follows
/// source order and closes the unit's `D_actor_135400_80131E48`.
const Actor135400Places D_actor_135400_80131E48 = {
    { { 5700, -150, 5900, 0 }, { 1024, 0, -1024, 0 } },
    { { 4700, 0, 5000, 0 }, { 0, -1024, 0, 0 } },
};

/// The word the split's `.rodata` run ends with, past the two records the copy
/// reads: it is what puts this unit's `.rodata` at 0x5C and so keeps the next
/// unit's at 0x80131E7C, where its switch table opens on an `.align 3` that
/// pads to 0x80131E80. Written out as the word of data it is, the same way
/// `actor_401800` writes its own trailing pad.
const s32 D_actor_135400_80131E78 = 0;

/// Per-frame tick of the actor's main task: ticks the twenty animation slots
/// once `field_474` has latched, and while the model is not flagged killed
/// (bit 0x80 of `TmdObject::flags`) draws its ground shadow from the second
/// part's translation, recomputes that part's world matrix, re-ranks it
/// through `func_800D7A9C`, ramps the head-tracking rate `headRate` and finally
/// turns the head toward the slot-3 skeleton with `func_800B0928`.
void func_actor_135400_801322A8(Task* task)
{
    Actor135400MainWork* work;
    TmdObject*           ext;
    VECTOR3              pos;
    s32                  i;
    s32                  rate;

    work = (Actor135400MainWork*)task->work;
    ext  = task->extra;
    if (work->field_474 != 0) {
        for (i = 1; i < 0x14; i++) {
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
    if (!(ext->flags & 0x80)) {
        if (func_800EA1A8((VECTOR3*)((TmdObject*)task->extra)->coords[1].workm.t, &pos) != 0) {
            Gp_DrawEffGroundQuad(&pos, 0x300, Gp_State1C->field_8);
        }
        Gp_UpdateCoord(&((TmdObject*)task->extra)->coords[1]);
        func_800D7A9C(ext, (VECTOR*)((TmdObject*)task->extra)->coords[1].workm.t, 0, 3);
        if (work->headAim != 0) {
            rate           = work->headRate + 0x100;
            work->headRate = rate;
            if (rate >= 0x1000) {
                work->headRate = 0xFFF;
            }
        } else {
            rate           = work->headRate - 0x80;
            work->headRate = rate;
            if (rate < 0) {
                work->headRate = 0;
            }
        }
        func_800B0928(task, Game_GetPtrSlot(3), 0x200, 0x100, work->headRate);
    }
}

void func_actor_135400_801323F8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_135400_80131E24;
    sp.funcs[task->state](task);
}

void func_actor_135400_80132450(Task* task)
{
    Task*          parent;
    s32            part;
    TmdObject*     extra;
    TmdObject*     parentExtra;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* dest;

    parent          = (Task*)task->spawnArg2;
    part            = task->spawnArg1;
    extra           = (TmdObject*)task->extra;
    parentExtra     = (TmdObject*)parent->extra;
    coord           = extra->coords;
    dest            = &parentExtra->coords[part];
    coord->flg      = 0;
    coord->sub      = dest;
    extra->lightMtx = parentExtra->lightMtx;
    extra->colorMtx = parentExtra->colorMtx;
    Task_Reparent(parent, task);
    task->state += 1;
}

void func_actor_135400_801324CC(void)
{
}

void func_actor_135400_801324D4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_135400_80131E30;
    sp.funcs[task->state](task);
}

INCLUDE_ASM("actors/nonmatchings/actor_135400/actor_135400", func_actor_135400_8013252C);

INCLUDE_ASM("actors/nonmatchings/actor_135400/actor_135400", func_actor_135400_801325A8);

void func_actor_135400_80132614(Task* arg0)
{
    Gp_EnemyTaskExit(arg0);
}
