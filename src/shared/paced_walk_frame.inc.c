#include "main/random.h"

/* Part of the paced walk library; see paced_walk.h. */

/// The actor's per-frame body (task state 1): refreshes the root coordinate,
/// re-lights the model at the root translation raised by 800, then runs the
/// step body and draws the ground shadow. While `smoking` is set and the
/// model is shown and has a buffer, every other frame spawns a smoke puff on
/// a randomly chosen part, with two `gRandomLcgState` draws packed into the effect
/// argument.
void pacedWalkFrame(Enemy* enemy, Task* task)
{
    TmdObject*     obj;
    GfxCoord*      coord;
    GfxCoord*      part;
    PacedWalkWork* work;
    VECTOR         pos;
    u32            low;
    u32            high;

    obj   = task->extra.tmd;
    coord = obj->coords;
    part  = &task->extra.tmd->coords[gPacedWalkEffectParts[(rand() * 11) >> 15]];
    work  = task->work;
    actorRenderComposeCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1] - 800;
    pos.vz = coord->workm.t[2];
    func_800D7A9C(obj, &pos, 0, 3);
    pacedWalkUpdate(task);
    walkerDrawShadow(task);
    if (work->smoking != 0 && !(obj->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) && obj->buffer != NULL) {
        if (task->killCountdown & 1) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            low             = (gRandomLcgState >> 16) & 0x10FF;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            high            = (((gRandomLcgState >> 16) & 1) << 30) + 0x800231C0;
            Gp_SpawnEff(EFFECT_SMOKE_PUFF, part, low + high, NULL);
        }
        task->killCountdown++;
    }
}
