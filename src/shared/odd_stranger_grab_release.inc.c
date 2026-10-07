/* Part of the Odd Stranger library; see odd_stranger.h. */

/// State 14: clip 7 with the player sent animation 3; during clip frames 0x10..0x16 it steps releaseStep (halved on contact). At the clip boundary it picks state 6 or 0xA from the targeted flag and sight test and sends the closing 0x3F1 if still latched.
void oddStrangerGrabRelease(Task* arg0)
{
    OddStrangerWork*      work;
    Enemy*                enemy;
    AnimationPlayRequest* msg;
    PlayerStatus*         cfg;
    u8                    kind;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    cfg   = &gPlayerStatus;
    if (work->stateEntered != 0) {
        work->animRate    = 0x10;
        work->animId      = 7;
        work->animRequest = ODD_STRANGER_ANIM_REQUEST_RESET;
        _oddStrangerDriveAnimation(arg0);
        msg              = &gOddStrangerPlayerAnim;
        msg->animationId = 3;
        if (cfg->hp > 0) {
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, msg, 0);
        }
        work->releaseStep     = -0x78;
        work->stateTimer      = 0;
        work->gridBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        return;
    }
    if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0 && cfg->hp > 0 && work->playerHeld == 1) {
        taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
        work->playerHeld = 0;
    }
    if ((u32)((work->rig.slots[1].currentPose.indices.recordIndex & 0x3FF) - 0x10) < 7U) {
        if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x12C, work->releaseStep) != 0) {
            actorMoveForwardNonzero(arg0->extra.tmd->coords, (u16)work->releaseStep);
        }
        if (_actorContactApplyGridPushback(arg0->extra.tmd->coords, work->gridContacts, ARRAY_SIZE(work->gridContacts)) == 1) {
            work->releaseStep = (s16)(u16)work->releaseStep / 2;
        }
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    _oddStrangerDriveAnimation(arg0);
    if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
        kind = enemy->node.state.parts.targeted;
        if (kind == 1) {
            if (detectSightBlocked(arg0) == kind) {
                work->state = ODD_STRANGER_STATE_ALERT;
            } else {
                work->state = ODD_STRANGER_STATE_SIDESTEP;
            }
        } else {
            work->state = ODD_STRANGER_STATE_ALERT;
        }
        if (cfg->hp > 0 && work->playerHeld == 1) {
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
            work->playerHeld = 0;
        }
    }
}
