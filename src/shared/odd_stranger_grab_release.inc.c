/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Ends the player's hold, backs away during the release clip and resumes combat.
///
/// Handles `ODD_STRANGER_STATE_GRAB_RELEASE` on a live Odd Stranger task with
/// bound rigs and a live player task/model in the same root-parent space.
/// Entry sends player clip 3 only while HP is positive. If still held, a living
/// player is released once that clip stops or at the enemy clip boundary.
/// Enemy records 16..22 allow a -120-unit step; grid pushback halves that
/// signed step with truncation toward zero. At the boundary a targeted enemy
/// with clear sight selects `SIDESTEP`, otherwise `ALERT`. The animation
/// request is lent from overlay-static storage only during synchronous dispatch.
static void _oddStrangerGrabRelease(Task* task)
{
    enum {
        ODD_STRANGER_PLAYER_ANIM_GRAB_RELEASE  = 3,
        ODD_STRANGER_GRAB_RELEASE_STEP         = -120,
        ODD_STRANGER_GRAB_RELEASE_FIRST_RECORD = 16,
        ODD_STRANGER_GRAB_RELEASE_RECORD_COUNT = 7
    };
    OddStrangerWork*      work;
    Enemy*                enemy;
    AnimationPlayRequest* playerAnimation;
    PlayerStatus*         playerStatus;
    u8                    targeted;

    work         = task->work;
    enemy        = task->spawnArg2.pointer;
    playerStatus = &gPlayerStatus;
    if (work->stateEntered != 0) {
        work->animRate    = ANIMATION_RATE_ONE;
        work->animId      = ODD_STRANGER_ANIM_GRAB_RELEASE;
        work->animRequest = ODD_STRANGER_ANIM_REQUEST_RESET;
        _oddStrangerDriveAnimation(task);
        playerAnimation              = &gOddStrangerPlayerAnim;
        playerAnimation->animationId = ODD_STRANGER_PLAYER_ANIM_GRAB_RELEASE;
        if (playerStatus->hp > 0) {
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, playerAnimation, 0);
        }
        work->releaseStep     = ODD_STRANGER_GRAB_RELEASE_STEP;
        work->stateTimer      = 0;
        work->gridBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        return;
    }
    if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0 && playerStatus->hp > 0 && work->playerHeld == 1) {
        taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
        work->playerHeld = 0;
    }
    // Only the release clip's retreat records move the enemy away from the player.
    if ((u32)((work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK) - ODD_STRANGER_GRAB_RELEASE_FIRST_RECORD) < (u32)ODD_STRANGER_GRAB_RELEASE_RECORD_COUNT) {
        if ((s16)_playerDetectionOutOfReach(task->extra.tmd->coords, ODD_STRANGER_MOVE_STOP_DISTANCE, work->releaseStep) != 0) {
            _actorMovementTranslateForwardNonzero(task->extra.tmd->coords, (u16)work->releaseStep);
        }
        if (_actorContactApplyGridPushback(task->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts)) == 1) {
            work->releaseStep = work->releaseStep / 2;
        }
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    _oddStrangerDriveAnimation(task);
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        targeted = enemy->node.state.parts.targeted;
        if (targeted == 1) {
            if (_playerDetectionSightBlocked(task) == targeted) {
                work->state = ODD_STRANGER_STATE_ALERT;
            } else {
                work->state = ODD_STRANGER_STATE_SIDESTEP;
            }
        } else {
            work->state = ODD_STRANGER_STATE_ALERT;
        }
        if (playerStatus->hp > 0 && work->playerHeld == 1) {
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
            work->playerHeld = 0;
        }
    }
}
