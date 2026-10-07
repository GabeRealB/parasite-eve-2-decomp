#include "main/sound_ids.h"

/* Part of the Glutton library; see glutton.h. */

/// Drops a spat glob onto the arena floor before its engulf state.
///
/// Requires the glob's live TMD body and projectile work. Each tick covers one
/// fifteenth of its launch-to-player offset and adds the magnitude of its fall
/// step to Y. Crossing Y = 0 starts the next state at height -50 and plays the
/// landing cue. The fight-end flag destroys the enemy instead.
static void _gluttonGlobFall(Enemy* enemy, Task* task)
{
    enum { GLUTTON_GLOB_FALL_TICKS = 15 };
    GluttonProjectileWork* work = task->work;
    GfxCoord*              coord;
    VECTOR                 worldPosition;
    s32                    soundId;
    s32                    audioPan;
    s32                    height;
    s32                    fallDistance;

    if (gGluttonEnded == 1) {
        enemyDestroy(enemy, task);
        return;
    }

    coord  = task->extra.tmd->coords;
    height = coord->coord.t[1];
    if (height > 0) {
        coord->coord.t[1] = -0x32;
        work->stateTicks  = 0;
        soundId           = ((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_GLUTTON_CHUNK_LAND;
        audioPan          = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, audioPan, (s8)(worldCoordGetOriginAudioDepth(task->extra.tmd->coords) / 2));
        task->state++;
        return;
    }

    fallDistance      = ABS(work->fallStep);
    coord->coord.t[1] = height + fallDistance;

    task->extra.tmd->coords->coord.t[0]  += work->aim.travel.vx / GLUTTON_GLOB_FALL_TICKS;
    task->extra.tmd->coords->coord.t[2]  += work->aim.travel.vz / GLUTTON_GLOB_FALL_TICKS;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;

    worldPosition.vx = task->extra.tmd->coords->workm.t[0];
    worldPosition.vy = task->extra.tmd->coords->workm.t[1];
    worldPosition.vz = task->extra.tmd->coords->workm.t[2];
    worldCoordUpdateActorColor(enemy, &worldPosition, 0, 0);

    // Fade the green and blue ambient terms after refreshing room lighting.
    work->colorMtx.t[1] >>= 1;
    work->colorMtx.t[2] >>= 2;

    _actorRenderRescaleYawHalf(task->extra.tmd->coords);
}
