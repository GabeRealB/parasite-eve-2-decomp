#include "main/sound_ids.h"

/* Part of the Glutton library; see glutton.h. */

/// Records the glob's horizontal launch offset and initializes its descent.
///
/// Both TMD roots must use the view coordinate as their parent. Offsets use
/// signed world-coordinate units; the fall step narrows to a signed halfword as
/// launch Y / 15, truncated toward zero. Descent spends X/Z in fifteenths and
/// adds the fall step's magnitude to Y. Clears elapsed ticks and caught state;
/// all arguments are borrowed for this call.
static inline void _gluttonCaptureGlobTravel(GluttonProjectileWork* work, Task* task, Task* playerTask)
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

    if (_gGluttonEncounterEnded == GLUTTON_ENCOUNTER_ENDED) {
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

    if (_gGluttonEncounterEnded == GLUTTON_ENCOUNTER_ENDED) {
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

/// Requests a forty-press player hold and installs the glob's caught animation on acceptance.
///
/// player must be the current player task returned by the player slot. The
/// hold copies its forty direction-pad/face-button presses during dispatch;
/// recovery rejects it without changing grab state. Acceptance is reply zero.
/// Acceptance sets grab-active, dispatches caught clip 1 with a three-frame
/// reset blend, then latches playerCaught. The animation table must remain live
/// through playback. Requires live projectile work and animation context.
static inline void _gluttonGlobRequestPlayerHold(GluttonProjectileWork* work, Task* player)
{
    enum {
        GLUTTON_GLOB_HOLD_BUTTON_PRESSES = 40,
        GLUTTON_GLOB_CAUGHT_ANIMATION    = 1,
        GLUTTON_GLOB_CAUGHT_BLEND_FRAMES = 3
    };
    _gGluttonGlobEngulfHold.hold.pressCount = GLUTTON_GLOB_HOLD_BUTTON_PRESSES;
    if (TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &_gGluttonGlobEngulfHold.hold, 0) == 0) {
        _gGluttonGlobHoldActive      = GLUTTON_GLOB_HOLD_ACTIVE;
        work->playerAnim.source.sets = _gGluttonGlobPlayerAnimSets;
        work->playerAnim.animationId = GLUTTON_GLOB_CAUGHT_ANIMATION;
        work->playerAnim.blend       = ANIMATION_BLEND_RESET;
        work->playerAnim.blendFrames = GLUTTON_GLOB_CAUGHT_BLEND_FRAMES;
        TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnim, 0);
        work->playerCaught = 1;
    }
}

/// Widens and flattens a Glutton glob, attempting to hold the player on tick seven.
///
/// Requires live projectile work, model lighting matrices and a player in the
/// same root-coordinate frame. XZ reach is strictly below 1000 game units after
/// signed-halfword narrowing. A living, unscripted player must accept a
/// forty-button-press hold before its animation is replaced; the borrowed
/// animation request and loaded sets remain live through the hold.
/// Advances after tick ten if uncaught, when the hold ends, or on tick 201.
/// Shutdown releases a caught player before destroying the glob. Each running
/// tick refreshes lighting, then halves green and quarters blue ambient terms.
static void _gluttonGlobEngulf(Enemy* enemy, Task* task)
{
    enum {
        GLUTTON_GLOB_GRAB_RADIUS           = 1000,
        GLUTTON_GLOB_GRAB_TICK             = 7,
        GLUTTON_GLOB_RESHAPE_END_TICK      = 10,
        GLUTTON_GLOB_UNCAUGHT_ADVANCE_TICK = 11,
        GLUTTON_GLOB_HOLD_TIMEOUT_TICK     = 201,
        GLUTTON_GLOB_HORIZONTAL_STEP_Q12   = 400,
        GLUTTON_GLOB_HALF_SCALE_Q12        = ONE / 2,
        GLUTTON_GLOB_GRAB_WIDTH_Q12        = 6048,
    };

    GluttonProjectileWork* work;
    Task*                  player;
    GameActor*             actor;
    PlayerStatus*          playerStatus;
    SVECTOR                playerOffset;
    VECTOR                 worldPosition;
    s16                    stateTick;
    s16                    horizontalScale;
    s32                    verticalScale;

    work         = task->work;
    player       = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    actor        = player->work;
    playerStatus = &gPlayerStatus;

    if (_gGluttonEncounterEnded == GLUTTON_ENCOUNTER_ENDED) {
        if (work->playerCaught == 1) {
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, PLAYER_ACTOR_END_SCRIPTED_KEEP_ROOT_OFFSET, 0);
            work->playerCaught = 0;
        }
        enemyDestroy(enemy, task);
        return;
    }

    // Q12 width grows while height shrinks; tick seven uses a fixed half height.
    stateTick = ++work->stateTicks;
    if (stateTick < GLUTTON_GLOB_RESHAPE_END_TICK) {
        horizontalScale = stateTick * GLUTTON_GLOB_HORIZONTAL_STEP_Q12 + GLUTTON_GLOB_HALF_SCALE_Q12;
        verticalScale   = GLUTTON_GLOB_HALF_SCALE_Q12 / stateTick;
        _actorRenderRescaleYawXZ(task->extra.tmd->coords, horizontalScale, verticalScale);
    }

    if (work->stateTicks == GLUTTON_GLOB_GRAB_TICK) {
        _actorRenderRescaleYawXZ(task->extra.tmd->coords, GLUTTON_GLOB_GRAB_WIDTH_Q12, GLUTTON_GLOB_HALF_SCALE_Q12);

        // The reach test takes signed-halfword offsets in the roots' shared world frame.
        playerOffset.vx = task->extra.tmd->coords->coord.t[0] -
                          player->extra.tmd->coords->coord.t[0];
        playerOffset.vy = 0;
        playerOffset.vz = task->extra.tmd->coords->coord.t[2] -
                          player->extra.tmd->coords->coord.t[2];

        if (_actorRangeOutsideRadiusXZ(&playerOffset, GLUTTON_GLOB_GRAB_RADIUS) == 0 && actor->mode != GAME_ACTOR_MODE_SCRIPTED &&
            playerStatus->hp > 0) {
            _gluttonGlobRequestPlayerHold(work, player);
        }
    }

    if (work->stateTicks >= GLUTTON_GLOB_UNCAUGHT_ADVANCE_TICK && work->playerCaught == 0) {
        task->state++;
    } else if (work->playerCaught == 1 && _gGluttonGlobHoldActive == GLUTTON_GLOB_HOLD_RELEASED) {
        task->state++;
    } else if (work->stateTicks >= GLUTTON_GLOB_HOLD_TIMEOUT_TICK) {
        _gGluttonGlobHoldActive = GLUTTON_GLOB_HOLD_RELEASED;
        task->state++;
    }

    worldPosition.vx = task->extra.tmd->coords->workm.t[0];
    worldPosition.vy = task->extra.tmd->coords->workm.t[1];
    worldPosition.vz = task->extra.tmd->coords->workm.t[2];
    worldCoordUpdateActorColor(enemy, &worldPosition, 0, 0);

    work->colorMtx.t[1] >>= 1;
    work->colorMtx.t[2] >>= 2;
}

/// Plays the glob's release clip on its caught player, then returns control.
///
/// Requires live projectile work and the player task. On entry, an uncaught glob
/// skips this state; a caught one installs clip 2, using set 9 of the equipped
/// weapon bank, and hides its own model. After nine ticks it ends scripted control
/// without adding the player's clip translation to the root. Fight teardown also
/// releases a player caught by this glob before destroying it. The animation
/// request and set table remain live while the player borrows them.
static void _gluttonGlobHold(Enemy* enemy, Task* task)
{
    enum { GLUTTON_GLOB_RELEASE_CLIP       = 2,
           GLUTTON_GLOB_WEAPON_RELEASE_SET = 9,
           GLUTTON_GLOB_HOLD_TICKS         = 9,
           GLUTTON_GLOB_RELEASE_IN_PLACE   = 2 };
    GluttonProjectileWork* work;
    Task*                  player;
    s32                    playerCaught;

    work   = task->work;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (_gGluttonEncounterEnded == GLUTTON_ENCOUNTER_ENDED) {
        if (work->playerCaught == 1) {
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, GLUTTON_GLOB_RELEASE_IN_PLACE, 0);
            work->playerCaught = 0;
        }
        enemyDestroy(enemy, task);
        return;
    }

    if (work->stateChanged != 0) {
        playerCaught     = work->playerCaught;
        work->stateTicks = 0;
        if (playerCaught != 1) {
            task->state++;
            return;
        }
        // Restore a weapon-bank clip before handing scripted control back.
        _gGluttonGlobPlayerAnimSets[GLUTTON_GLOB_RELEASE_CLIP] =
            (gPlayerActorWeaponAnimationBanks[gPlayerActorWeaponAnimationBankBases[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId - 1] + gPlayerStatus.weapon])[GLUTTON_GLOB_WEAPON_RELEASE_SET];
        work->playerAnim.source.sets = _gGluttonGlobPlayerAnimSets;
        work->playerAnim.animationId = GLUTTON_GLOB_RELEASE_CLIP;
        work->playerAnim.blend       = ANIMATION_BLEND_INTERPOLATE;
        work->playerAnim.blendFrames = GLUTTON_GLOB_HOLD_TICKS;
        TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_REPLACE_AND_PLAY, &work->playerAnim, 0);
        task->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }

    if (work->stateTicks >= GLUTTON_GLOB_HOLD_TICKS) {
        taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, GLUTTON_GLOB_RELEASE_IN_PLACE, 0);
        work->playerCaught = 0;
        task->state++;
    }
    work->stateTicks++;
}
