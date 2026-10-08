#include "main/random.h"

/* Part of the paced walk library; see paced_walk.h. */

/* ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW is an object-like binding to a
 * declared static void(Task*) drawer. The call evaluates task once. */
#ifndef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW
#error "Bind ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW before including this fragment"
#endif

/// Composes the root and samples three lights from its cached frame at Y minus 800.
///
/// Borrows the model and root for this synchronous query; no frame conversion.
static inline void _actorRenderLightPacedWalkerRoot(TmdObject* model, GfxCoord* rootCoord)
{
    enum { PACED_WALK_LIGHT_SAMPLE_Y_OFFSET = 800,
           PACED_WALK_LIGHT_COUNT           = 3 };
    VECTOR lightingSample;
    actorRenderComposeCoord(rootCoord);
    lightingSample.vx = rootCoord->workm.t[0];
    lightingSample.vy = rootCoord->workm.t[1] - PACED_WALK_LIGHT_SAMPLE_Y_OFFSET;
    lightingSample.vz = rootCoord->workm.t[2];
    worldCoordSetModelLighting(model, &lightingSample, 0, PACED_WALK_LIGHT_COUNT);
}

/// Lights and updates a paced walker, draws its shadow and emits scripted smoke.
///
/// Requires the spawned model/work/rig and live carrier bindings. Chooses one
/// of eleven effect parts with rand() on every call, including hidden frames.
/// Samples the composed root's full-chain XYZ with Y minus 800, preserving
/// that composition frame, before movement/animation. Shadow follows update.
/// A shown, buffered smoking model emits on odd killCountdown values and then
/// increments that counter; each puff consumes two shared LCG draws. All sample
/// storage is borrowed only during the synchronous call; enemy is unused.
static void _pacedWalkFrame(Enemy* unusedEnemy, Task* task)
{
    enum {
        PACED_WALK_EFFECT_PART_COUNT = 11,
        PACED_WALK_SMOKE_LOW_MASK    = 0x10FF,
        PACED_WALK_SMOKE_HIGH_BASE   = 0x800231C0
    };
    TmdObject*     model;
    GfxCoord*      rootCoord;
    GfxCoord*      smokePart;
    PacedWalkWork* work;
    u32            smokeLowBits;
    u32            smokeHighBits;

    model     = task->extra.tmd;
    rootCoord = model->coords;
    smokePart = &task->extra.tmd->coords[gPacedWalkEffectParts[(rand() * PACED_WALK_EFFECT_PART_COUNT) >> 15]];
    work      = task->work;
    // Sample lighting before the update; the shadow uses its resulting root cache.
    _actorRenderLightPacedWalkerRoot(model, rootCoord);
    PACED_WALK_UPDATE(task);
    ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW(task);
    if (work->smoking != 0 && !(model->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) && model->buffer != NULL) {
        if (task->killCountdown & 1) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            smokeLowBits    = (gRandomLcgState >> 16) & PACED_WALK_SMOKE_LOW_MASK;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            smokeHighBits   = (((gRandomLcgState >> 16) & 1) << 30) + PACED_WALK_SMOKE_HIGH_BASE;
            effectSpawn(EFFECT_SMOKE_PUFF, smokePart, smokeLowBits + smokeHighBits, NULL);
        }
        task->killCountdown++;
    }
}
