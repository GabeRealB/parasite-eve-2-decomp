#include "common.h"

#include "actors/actor_511000.h"

#include "gameplay/1BC.h"

#include "gameplay/3A34.h"

#include "gameplay/3CD8.h"

#include "gameplay/gameplay.h"

#include "main/session.h"

#include "main/sound.h"

#include "main/task.h"

#include "main/tmd.h"

/// The actor's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern TaskFuncTable3 D_actor_511000_80131E24;

/// The three texture records the tick state's upload steps post, one per
/// `field_4D0` value -- the same trio `func_actor_511000_80132904` selects
/// from by mode. Each is a lone `GpImgRec` whose 0x18x0x10 source rect
/// repeats the size the state's scratch `RECT` carries.
extern GpImgRec D_actor_511000_80146C74;
extern GpImgRec D_actor_511000_80146F94;
extern GpImgRec D_actor_511000_801472B4;

/// Offset `Gp_SpawnEff` places the tick state's effect at.
extern SVECTOR D_actor_511000_8014733C;

/// Tick state: while `field_474` is set, steps animation slots 1..19; in
/// mode 1 counts `field_4D2` up and, on frame 0x10, plays the sound and spawns
/// the effect at the first child's model. Then draws the ground shadow under
/// model part 1, refreshes that part's coordinate and colour when the session
/// asks, runs the texture-upload state, and ticks the `field_480` countdown
/// that frees the model's buffers when it reaches zero.
void func_actor_511000_80131E78(GpActorWork* arg0)
{
    Actor511000Work2* work;
    TmdObject*        extra;
    GsCOORDINATE2*    coord;
    GsCOORDINATE2*    obj;
    VECTOR            pos;
    s32               i;
    s32               pan;

    extra = arg0->extra;
    work  = (Actor511000Work2*)arg0->actor;
    coord = &extra->coords[1];
    if (work->field_474 != 0) {
        for (i = 1; i < 0x14; i++) {
            Gp_AnimTickIndex((GpAnimCtx*)work, i);
        }
        if (work->field_478 == 1) {
            if (++work->field_4D2 == 0x10) {
                obj = ((TmdObject*)work->field_4C4->extra)->coords;
                pan = (s8)Gp_GetObjPan(obj);
                SndEvt_EnqueueType6(0x313A0003, pan, (s8)Gp_GetObjDepth(obj));
                Gp_SpawnEff(0x6006A, obj, 0, &D_actor_511000_8014733C);
            }
        }
    }
    if (!(extra->flags & 0x80)) {
        if (func_800EA1A8((VECTOR3*)arg0->extra->coords[1].workm.t, (VECTOR3*)&pos) != 0) {
            Gp_DrawEffGroundQuad((VECTOR3*)&pos, 0x300, Gp_State1C->field_8);
        }
    }
    if (gGameSession->viewReady != 0) {
        coord->flg = 0;
        Gp_UpdateCoord(coord);
        func_800D7A9C(extra, (VECTOR*)coord->workm.t, 0, 3);
    }
    func_actor_511000_80132048(arg0);
    if (work->field_480.word >= 0) {
        if (work->field_480.word == 0) {
            Tmd_FreeBuffers(extra);
        }
        work->field_480.word--;
    }
}

/// Texture-upload state: runs the countdown at `field_4CE` down one a frame
/// while `field_4D0` names the upload in progress, and on the frame it
/// underflows posts that step's image over the 0x18x0x10 rect at y 0x28 --
/// reloading the countdown from `field_4CC` and advancing `field_4D0` for
/// steps 1 and 2, or clearing it and starting over for step 3. Steps 1 and 2
/// share their whole tail, which is what makes the compiler emit one copy of
/// it that step 1 jumps into; step 3 only differs in clearing the step
/// instead of advancing it.
void func_actor_511000_80132048(GpActorWork* arg0)
{
    Actor511000Work2* work;
    RECT              rect;

    work   = (Actor511000Work2*)arg0->actor;
    rect.x = 0;
    rect.y = 0x28;
    rect.w = 0x18;
    rect.h = 0x10;

    switch (work->field_4D0) {
        case 1:
            work->field_4CE = work->field_4CE - 1;
            if ((s16)work->field_4CE < 0) {
                Gp_LoadActorImage(arg0, &D_actor_511000_801472B4, &rect);
                work->field_4CE = work->field_4CC;
                work->field_4D0 = work->field_4D0 + 1;
            }
            break;
        case 2:
            work->field_4CE = work->field_4CE - 1;
            if ((s16)work->field_4CE < 0) {
                Gp_LoadActorImage(arg0, &D_actor_511000_80146F94, &rect);
                work->field_4CE = work->field_4CC;
                work->field_4D0 = work->field_4D0 + 1;
            }
            break;
        case 3:
            work->field_4CE = work->field_4CE - 1;
            if ((s16)work->field_4CE < 0) {
                Gp_LoadActorImage(arg0, &D_actor_511000_80146C74, &rect);
                work->field_4D0 = 0;
            }
            break;
    }
}

INCLUDE_RODATA("actors/nonmatchings/actor_511000/actor_511000", D_actor_511000_80131E20);

INCLUDE_RODATA("actors/nonmatchings/actor_511000/actor_511000", D_actor_511000_80131E24);

void func_actor_511000_80132150(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_511000_80131E24;
    sp.funcs[task->state](task);
}

INCLUDE_RODATA("actors/nonmatchings/actor_511000/actor_511000", D_actor_511000_80131E30);

INCLUDE_RODATA("actors/nonmatchings/actor_511000/actor_511000", D_actor_511000_80131E3C);
