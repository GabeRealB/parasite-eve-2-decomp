#include "main/random.h"

/* Part of the Glutton library; see glutton.h. */

/// Ascent state that precedes the descent above: lift the model by 0x1F4 plus
/// `speedJitter` a step until it passes -0x4E20, then clamp it there, move it
/// over `work->aim.landing`, restart `stateTicks`, reroll `speedJitter` to
/// 0..0x1F for the drop, enable pair tests on `attackBody` and step the task
/// on. Either way `bodyCoord` is left tracking the model. Bails to
/// `enemyDestroy` when the overlay is shutting down.
void gluttonRainRise(Enemy* enemy, Task* task)
{
    GluttonProjectileWork* work;
    s32                    y;

    work = task->work;
    if (gGluttonEnded == 1) {
        Gp_UnlinkObj(&work->attackBody);
        enemyDestroy(enemy, task);
        return;
    }

    y                                   = task->extra.tmd->coords->coord.t[1] - 0x1F4;
    task->extra.tmd->coords->coord.t[1] = y - work->speedJitter;
    if (task->extra.tmd->coords->coord.t[1] < -0x4E20) {
        task->state++;
        task->extra.tmd->coords->coord.t[0] = work->aim.landing.vx;
        task->extra.tmd->coords->coord.t[2] = work->aim.landing.vz;
        gRandomLcgState                     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        task->extra.tmd->coords->coord.t[1] = -0x4E20;
        work->stateTicks                    = 0;
        work->speedJitter                   = (gRandomLcgState >> 16) & 0x1F;
        work->attackBody.flags             |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    }

    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    work->bodyCoord.node.coord.t[0]       = task->extra.tmd->coords->coord.t[0];
    work->bodyCoord.node.coord.t[1]       = task->extra.tmd->coords->coord.t[1];
    work->bodyCoord.node.coord.t[2]       = task->extra.tmd->coords->coord.t[2];
    work->bodyCoord.node.composeStamp     = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&work->bodyCoord.node);
}
