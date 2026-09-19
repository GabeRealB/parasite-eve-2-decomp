#include "common.h"

#include <psyq/abs.h>

#include "main/display.h"
#include "main/gfx.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"

/// Placement record the overlay's data table points at, read here only as the
/// target position's x/z pair.
extern VECTOR D_actor_341300_80165330;

INCLUDE_RODATA("actors/nonmatchings/actor_341300/actor_341300", D_actor_341300_80161E20);

/// Draws the two textured quads at fixed positions: each is four fixed
/// model-space corners projected through `Gfx_ViewWorldMtx`, emitted as a
/// POLY_FT4 at the depth `RotTransPers3` returns, and skipped when the
/// projection flags an error.
void func_actor_341300_80161E84(void)
{
    s16     x[8];
    s16     y[8];
    s32     sxy[8];
    s32     otz[2];
    SVECTOR v[8] = {
        { 0x80C, -0xBC6, 0x150 },
        { 0x83C, -0xBC6, 0x150 },
        { 0x80C, -0xBC6, 0x180 },
        { 0x83C, -0xBC6, 0x180 },
        { 0x747, -0xBC6, 0x150 },
        { 0x777, -0xBC6, 0x150 },
        { 0x747, -0xBC6, 0x180 },
        { 0x777, -0xBC6, 0x180 },
    };
    s32       p;
    s32       flag;
    s32       i;
    s32       j;
    s32       k;
    POLY_FT4* prim;

    SetRotMatrix(&Gfx_ViewWorldMtx);
    SetTransMatrix(&Gfx_ViewWorldMtx);
    for (i = 0; i < 2; i++) {
        k = i * 4;
        RotTransPers(&v[k + 3], &sxy[k + 3], &p, &flag);
        otz[i] = RotTransPers3(&v[k], &v[k + 1], &v[k + 2], &sxy[k], &sxy[k + 1], &sxy[k + 2], &p, &flag);
        if (flag >= 0) {
            for (j = k; j < k + 4; j++) {
                x[j] = sxy[j];
                y[j] = sxy[j] >> 16;
            }
            prim           = (POLY_FT4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            setcode(prim, 0x2D);
            prim->x0 = x[k];
            prim->y0 = y[k];
            prim->x1 = x[k + 1];
            prim->y1 = y[k + 1];
            prim->x2 = x[k + 2];
            prim->y2 = y[k + 2];
            prim->x3 = x[k + 3];
            prim->y3 = y[k + 3];
            setUV4(prim, 0x23, 0xD1, 0x2F, 0xD1, 0x23, 0xDD, 0x2F, 0xDD);
            setRGB0(prim, 0x80, 0x80, 0x80);
            prim->clut  = 0x3E00;
            prim->tpage = 0x97;
            addPrim((u_long*)((((u32)(otz[i] << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt), prim);
        }
    }
}

/// Per-frame task that turns the player (`gameGetPtrSlot(3)`, whose
/// `Task::work` is the `GameActor` block) to face the object the area work
/// id resolves to, then kills itself once it is close enough.
///
/// The aim angle is `ratan2` of the target's x/z pair minus the player's own
/// coordinate translation; the delta against `GameActor::field_52` is
/// unwrapped into `-0x800..0x800` and stepped by `0x80` per frame, so the
/// player rotates at a fixed rate. Inside `0x80` of the target the facing
/// snaps to the exact angle and the task ends.
///
/// The task's own argument is only ever the `taskKill` target, reached both
/// when the work lookup or `gGameSession::eventState` fails and on the frame the
/// facing settles.
void func_actor_341300_80162278(Task* task)
{
    Task*          player;
    GameActor*     actor;
    GpWorkObj*     work;
    GsCOORDINATE2* self;
    VECTOR*        target;
    s32            angle;
    s32            delta;
    s32            magnitude;
    s32            step;
    s32            wrapped;

    player = gameGetPtrSlot(3);
    actor  = (GameActor*)player->work;
    work   = Gp_FindWorkById(gGameSession->at4.loc.area | (gGameSession->at4.loc.stage << 8));
    if ((work != NULL) && (gGameSession->eventState != 0)) {
        self      = ((TmdObject*)player->extra)->coords;
        target    = &D_actor_341300_80165330;
        angle     = ratan2(target->vx - self->coord.t[0], target->vz - self->coord.t[2]);
        delta     = angle - actor->field_52;
        magnitude = ABS(delta);
        if (magnitude >= 0x801) {
            wrapped = delta - 0x1000;
            if (delta < 0) {
                wrapped = delta + 0x1000;
            }
            delta = wrapped;
        }
        magnitude = ABS(delta);
        if (magnitude >= 0x81) {
            step = 0x80;
            if (delta < 0) {
                step = -0x80;
            }
            actor->field_52 = (s16)((u16)actor->field_52 + step);
            return;
        }
        actor->field_52 = angle;
    }
    taskKill(task);
}
