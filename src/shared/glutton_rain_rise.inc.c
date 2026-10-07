#include "main/random.h"

/* Part of the Glutton library; see glutton.h. */

/// Raises a rain blob out of view, then positions it over its landing point.
///
/// Moves its task coordinate upward by 500 plus `speedJitter` world units per
/// tick. Crossing y = -20000 clamps there, selects the landing x/z, clears the
/// state timer, rerolls drop jitter to 0..31 and enables the attack body before
/// advancing to descent. The body's independent coordinate follows the blob.
/// Requires a live coordinate body and initialized `GluttonProjectileWork`;
/// encounter shutdown unlinks the attack body and destroys the enemy.
static void _gluttonRainRise(Enemy* enemy, Task* task)
{
    enum { GLUTTON_RAIN_CLIMB_SPEED      = 500,
           GLUTTON_RAIN_TOP_Y            = -20000,
           GLUTTON_RAIN_DROP_JITTER_MASK = 31 };
    GluttonProjectileWork* work;
    s32                    nextY;

    work = task->work;
    if (gGluttonEnded == 1) {
        worldCollisionUnlinkBody(&work->attackBody);
        enemyDestroy(enemy, task);
        return;
    }

    nextY                                    = task->extra.coordBody->coord->coord.t[1] - GLUTTON_RAIN_CLIMB_SPEED;
    task->extra.coordBody->coord->coord.t[1] = nextY - work->speedJitter;
    // Start collision tests only after moving over the chosen floor point.
    if (task->extra.coordBody->coord->coord.t[1] < GLUTTON_RAIN_TOP_Y) {
        task->state++;
        task->extra.coordBody->coord->coord.t[0] = work->aim.landing.vx;
        task->extra.coordBody->coord->coord.t[2] = work->aim.landing.vz;
        gRandomLcgState                          = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        task->extra.coordBody->coord->coord.t[1] = GLUTTON_RAIN_TOP_Y;
        work->stateTicks                         = 0;
        work->speedJitter                        = (gRandomLcgState >> 16) & GLUTTON_RAIN_DROP_JITTER_MASK;
        work->attackBody.flags                  |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    }

    task->extra.coordBody->coord->composeStamp = GRAPHICS_COORD_DIRTY;
    work->bodyCoord.node.coord.t[0]            = task->extra.coordBody->coord->coord.t[0];
    work->bodyCoord.node.coord.t[1]            = task->extra.coordBody->coord->coord.t[1];
    work->bodyCoord.node.coord.t[2]            = task->extra.coordBody->coord->coord.t[2];
    work->bodyCoord.node.composeStamp          = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&work->bodyCoord.node);
}
