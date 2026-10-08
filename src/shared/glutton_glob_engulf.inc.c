/* Part of the Glutton library; see glutton.h. */

/// Requests a forty-press player hold and installs the glob's caught animation on acceptance.
///
/// Borrows live projectile work, player and animation sets. The request is
/// consumed synchronously; acceptance is reply zero and latches playerCaught.
static inline void _gluttonGlobRequestPlayerHold(GluttonProjectileWork* work, Task* player)
{
    enum {
        GLUTTON_GLOB_HOLD_BUTTON_PRESSES = 40,
        GLUTTON_GLOB_CAUGHT_ANIMATION    = 1,
        GLUTTON_GLOB_CAUGHT_BLEND_FRAMES = 3
    };
    gGluttonGrabQuery.hold.pressCount = GLUTTON_GLOB_HOLD_BUTTON_PRESSES;
    if (TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_AWAIT_BUTTON_PRESSES, &gGluttonGrabQuery.hold, 0) == 0) {
        gGluttonGrabActive           = 1;
        work->playerAnim.source.sets = gGluttonCaughtAnimSets;
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

    if (gGluttonEnded == 1) {
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
    } else if (work->playerCaught == 1 && gGluttonGrabActive == 0) {
        task->state++;
    } else if (work->stateTicks >= GLUTTON_GLOB_HOLD_TIMEOUT_TICK) {
        gGluttonGrabActive = 0;
        task->state++;
    }

    worldPosition.vx = task->extra.tmd->coords->workm.t[0];
    worldPosition.vy = task->extra.tmd->coords->workm.t[1];
    worldPosition.vz = task->extra.tmd->coords->workm.t[2];
    worldCoordUpdateActorColor(enemy, &worldPosition, 0, 0);

    work->colorMtx.t[1] >>= 1;
    work->colorMtx.t[2] >>= 2;
}
