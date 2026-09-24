#include "common.h"

#include "actors/actor_107000.h"
#include "actors/actor_107000_anim.h"
#include "actors/actors_shared_80133cd0.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/gameplay.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#define SCRATCH_SP (*(u32*)G_SCRATCH_HEAD)

/// Per-frame handler of the specimen while it drops into place, before it
/// lands (the task state that follows `Actor07000_Fn01870`). `D_801153F4`
/// mode 1 only re-colours the actor and mode 2 hides the model; otherwise,
/// once `field_2E2` has armed the drop, the root part is stepped along its
/// facing and by the fall speed `field_2DE`, the collision response is
/// applied, the animation ticks and the root is recomputed, with the step
/// length `field_2BE` decaying by 2 a frame. When the root reaches the floor
/// (Y at or above 0) the landing sound is cued, the root is pinned at 0 and
/// the specimen moves to the live stage with animation 2 and task state 1;
/// until then the fall speed grows by 10 a frame, or 20 once the collision
/// response has latched `field_2E0`.
void Actor07000_Fn01BA0(GpEnemy* arg0, Task* arg1)
{
    Actor107000Work* work;
    GsCOORDINATE2*   coord;
    s32              soundId;

    work = (Actor107000Work*)arg1->work;
    switch (D_801153F4) {
        case 1:
            Actor107000_UpdateColor(arg0, &((TmdObject*)arg1->extra)->coords[1]);
            break;
        case 2:
            ((TmdObject*)arg1->extra)->flags = 0x80;
            arg0->node.flags                 = 1;
            break;
        case 0:
        default:
            if (work->field_2E2 == 0) {
                return;
            }
            Actor07000_Fn02D78(arg1);
            Actor07000_Fn01EB0(arg1);
            Actor107000_TickAnim(arg1);
            Actor107000_UpdateColor(arg0, &((TmdObject*)arg1->extra)->coords[1]);
            ((TmdObject*)arg1->extra)->coords->flg = 0;
            Gp_UpdateCoord(((TmdObject*)arg1->extra)->coords);
            work->field_2BE -= 2;
            if (work->field_2BE < 0) {
                work->field_2BE = 0;
            }
            coord = ((TmdObject*)arg1->extra)->coords;
            if (coord->coord.t[1] >= 0) {
                soundId = ((((GpEnemy*)arg1->spawnArg2)->placeKey >> 12) << 8) | 0x402C0008;
                SndEvt_EnqueueType6(soundId, (s8)Gp_GetObjPan(coord), (s8)gpGetObjDepth(coord));
                work->field_2B2                               = 1;
                work->field_2C8                               = 1;
                work->field_2BE                               = 0;
                work->field_2DE                               = 0;
                work->field_2B8                               = 2;
                work->field_2BA                               = 0;
                ((TmdObject*)arg1->extra)->coords->coord.t[1] = 0;
                arg1->state                                   = 1;
            } else if (work->field_2E0 == 0) {
                work->field_2DE += 10;
            } else {
                work->field_2DE += 20;
            }
            break;
    }
}

/// Collision response of the dropping specimen: node 2's collision table is
/// run through `func_800E0C10` with a 0x48-byte scratch. Response 1 adds the
/// returned X and Z offsets to the root; only the first one also adds Y,
/// latches `field_2E0`, sets the fall speed `field_2DE` to -0x64 and takes a
/// quarter off the step length `field_2BE`. Response 2 puts the root back at
/// the translation `Actor07000_Fn02D78` saved. The table is released either
/// way.
void Actor07000_Fn01EB0(Task* arg0)
{
    ActorsShared80133cd0Scratch* scratch;
    Actor107000Work*             work;
    GsCOORDINATE2*               coord;
    s32                          movement;

    work     = (Actor107000Work*)arg0->work;
    scratch  = (ActorsShared80133cd0Scratch*)(SCRATCH_SP -= 0x48);
    coord    = ((TmdObject*)arg0->extra)->coords;
    movement = func_800E0C10(&work->field_154[0], &scratch->delta, 4, NULL);
    switch (movement) {
        case 0:
            break;
        case 1:
            if (work->field_2E0 == 0) {
                coord->coord.t[1] += scratch->delta.vy.h.hi;
                work->field_2DE    = -0x64;
                work->field_2BE    = (u16)work->field_2BE - work->field_2BE / 4;
                work->field_2E0    = movement;
            }
            coord->coord.t[0] += scratch->delta.vx.h.hi;
            coord->coord.t[2] += scratch->delta.vz.h.hi;
            break;
        case 2:
            coord->coord.t[0] = work->field_274.vx;
            coord->coord.t[1] = work->field_274.vy;
            coord->coord.t[2] = work->field_274.vz;
            break;
    }
    Gp_ClearRec18Occupied(&work->field_154[0]);
    SCRATCH_SP += 0x48;
}
