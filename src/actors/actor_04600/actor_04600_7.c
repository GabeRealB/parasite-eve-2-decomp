#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"

#include "actors/actors_shared_80133cd0.h"
#include "actors/actor_104600.h"

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */
#define SCRATCH_SP (*(u32*)0x1F8003FC)

/// Per-frame handler of the first enemy while it drops into place. Mode 1 of
/// `D_801153F4` only re-colours it and mode 2 hides the model. Otherwise, once
/// `field_2E2` has armed the drop, the root steps along its facing and by the
/// fall speed `field_2DE`, the collision response is applied, the animation
/// ticks and the root is recomputed, with the step length decaying by 2 a
/// frame. Reaching the floor (Y at or above 0) plays the landing sound, pins
/// the root at 0 and moves the enemy to the live stage with animation 2 and
/// task state 1; until then the fall speed grows by 10 a frame, or by 20 once
/// the drop has hit something.
void Actor04600_Fn01AFC(GpEnemy* arg0, Task* arg1)
{
    Actor104600Work* work;
    GsCOORDINATE2*   coord;
    s32              soundId;

    work = (Actor104600Work*)arg1->work;
    switch (D_801153F4) {
        case 1:
            Actor04600_UpdateColor(arg0, &((TmdObject*)arg1->extra)->coords[1]);
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
            Actor04600_Fn02CD4(arg1);
            Actor04600_Fn01E0C(arg1);
            Actor04600_TickAnim(arg1);
            Actor04600_UpdateColor(arg0, &((TmdObject*)arg1->extra)->coords[1]);
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

/// Collision response of the dropping first enemy: the contact table at
/// `rec154` is run through `func_800E0C10` with a 0x48-byte scratch. Response 1
/// adds the returned X and Z offsets to the root; only the first one also adds
/// Y, latches `field_2E0`, sets the fall speed to -0x64 and takes a quarter off
/// the step length. Response 2 puts the root back where the last step started.
/// The table is released either way.
void Actor04600_Fn01E0C(Task* arg0)
{
    ActorsShared80133cd0Scratch* scratch;
    Actor104600Work*             work;
    GsCOORDINATE2*               coord;
    s32                          movement;

    work     = (Actor104600Work*)arg0->work;
    scratch  = (ActorsShared80133cd0Scratch*)(SCRATCH_SP -= 0x48);
    coord    = ((TmdObject*)arg0->extra)->coords;
    movement = func_800E0C10(&work->rec154[0], &scratch->delta, 4, NULL);
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
    Gp_ClearRec18Occupied(&work->rec154[0]);
    SCRATCH_SP += 0x48;
}
