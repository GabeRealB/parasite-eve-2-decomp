#include "common.h"

#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include "actors/actor_213100.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"

void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

/// The actor's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern TaskFuncTable3 D_actor_213100_80149E24;

/// Per-area flag byte table; the tick below consults the entry the session's
/// `field_4` selects and drives both models' `field_C` bit 0x80 from it, the
/// same way `Actor213100Work::field_43D` gates the animation tick.
extern s8 D_actor_213100_801521E0[];

/// Per-frame tick: ticks the work block's animation slots while `field_43C`
/// says it is live; while the model is not flagged hidden, samples the child
/// part's translation through `func_800EA1A8` and draws the ground shadow if
/// it hits. Once the session has finished loading a stage, rebuilds that
/// part's world matrix, hands its translation to `func_800D7A9C`, and sets
/// both this model's and the child's `field_C` bit 0x80 from the per-area
/// table. The work block's countdown then frees the model buffers as it
/// reaches zero.
void func_actor_213100_80149E3C(Task* task)
{
    Actor213100Work* work;
    TmdObject*       extra;
    TmdObject*       child;
    VECTOR3          pos;
    s32              i;

    work  = (Actor213100Work*)task->work;
    extra = (TmdObject*)task->extra;
    if (work->field_43C != 0) {
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex((GpAnimCtx*)work, i);
        }
    }
    if (!(extra->flags & 0x80)) {
        if (func_800EA1A8((VECTOR3*)((TmdObject*)task->extra)->coords[1].workm.t, &pos) != 0) {
            Gp_DrawEffGroundQuad(&pos, 0x300, Gp_State1C->groundShade);
        }
    }
    if (gGameSession->viewReady != 0) {
        ((TmdObject*)task->extra)->coords[1].flg = 0;
        Gp_UpdateCoord(&((TmdObject*)task->extra)->coords[1]);
        func_800D7A9C(extra, (VECTOR*)((TmdObject*)task->extra)->coords[1].workm.t, 0, 3);
        child = (TmdObject*)work->field_480->extra;
        if (D_actor_213100_801521E0[gGameSession->at4.loc.view] != 0) {
            extra->flags &= ~0x80;
            child->flags &= ~0x80;
        } else {
            extra->flags |= 0x80;
            child->flags |= 0x80;
        }
    }
    if (work->field_484 >= 0) {
        if (work->field_484 == 0) {
            Tmd_FreeBuffers(extra);
        }
        work->field_484--;
    }
}
INCLUDE_RODATA("actors/nonmatchings/actor_213100/actor_213100", D_actor_213100_80149E24);

void func_actor_213100_80149FE4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_213100_80149E24;
    sp.funcs[task->state](task);
}

INCLUDE_RODATA("actors/nonmatchings/actor_213100/actor_213100", D_actor_213100_80149E30);
