#include "main/sound_ids.h"

/* Part of the Glutton library; see glutton.h. */

/// Records the glob's launch-to-player travel and resets its descent counters.
///
/// Requires live projectile work and TMD coordinates for the glob and player
/// in the view frame. Travel uses world units; no pointer is retained.
static __inline__ void _gluttonCaptureGlobTravel(GluttonProjectileWork* work, Task* task, Task* playerTask)
{
    enum { GLUTTON_GLOB_FALL_TICKS = 15 };
    work->fallStep = task->extra.tmd->coords->coord.t[1] / GLUTTON_GLOB_FALL_TICKS;
    work->aim.travel.vx =
        playerTask->extra.tmd->coords->coord.t[0] - task->extra.tmd->coords->coord.t[0];
    work->aim.travel.vy = 0;
    work->aim.travel.vz =
        playerTask->extra.tmd->coords->coord.t[2] - task->extra.tmd->coords->coord.t[2];
    work->stateTicks   = 0;
    work->playerCaught = 0;
}

/// Creates a player-catching glob at escort 1's part-1 world origin.
///
/// Requires a live TMD body, parent host with escort 1 and its part 1, and a
/// player TMD coordinate in the view frame. Allocates zeroed task-owned work,
/// retints both buffer halves when present and lends the model its lighting
/// matrices. Launch-time X/Z travel is spent over fifteen descent ticks;
/// the model retains its yaw at half Q12 scale. Shutdown or allocation failure
/// destroys the enemy; success advances to descent.
static void _gluttonGlobSpawn(Enemy* enemy, Task* task)
{
    enum {
        GLUTTON_GLOB_CLUT_ROW     = 2,
        GLUTTON_GLOB_LAUNCH_SOUND = SOUND_CHARACTER(SOUND_BANK_GLUTTON, 0x1C)
    };
    GluttonProjectileWork* work;
    Enemy*                 hostEnemy;
    GluttonWork*           hostWork;
    Task*                  playerTask;
    SVECTOR                launchPoint;
    s32                    soundId;
    s32                    audioPan;

    hostEnemy  = task->parent->spawnArg2.pointer;
    hostWork   = hostEnemy->task->work;
    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);

    if (gGluttonEnded == 1) {
        enemyDestroy(enemy, task);
        return;
    }

    work       = memCalloc(sizeof(GluttonProjectileWork), false);
    task->work = work;
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }

    task->extra.tmd->coords->parent    = &gGfxViewCoord;
    task->extra.tmd->flags             = 0;
    task->extra.tmd->texturePageOffset = 0;
    task->extra.tmd->clutRowOffset     = GLUTTON_GLOB_CLUT_ROW;

    if (task->extra.tmd->buffer != NULL) {
        tmdBuildBufferHalf(task->extra.tmd);
        tmdBuildBufferHalf(task->extra.tmd);
        soundId  = ((hostEnemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | GLUTTON_GLOB_LAUNCH_SOUND;
        audioPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, audioPan, (s8)(worldCoordGetOriginAudioDepth(task->extra.tmd->coords) / 2));
    }

    task->extra.tmd->lightMtx = &work->lightMtx;
    task->extra.tmd->colorMtx = &work->colorMtx;

    // The spat glob starts on the escort, then travels in the view frame.
    launchPoint.vx = launchPoint.vy = launchPoint.vz = 0;
    _actorRenderTransformLocalPointToWorld(&hostWork->escorts[1]->task->extra.tmd->coords[1], &launchPoint);

    task->extra.tmd->coords->coord.t[0]   = launchPoint.vx;
    task->extra.tmd->coords->coord.t[1]   = launchPoint.vy;
    task->extra.tmd->coords->coord.t[2]   = launchPoint.vz;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;

    _gluttonCaptureGlobTravel(work, task, playerTask);

    _actorRenderRescaleYawHalf(task->extra.tmd->coords);
    task->state++;
}
