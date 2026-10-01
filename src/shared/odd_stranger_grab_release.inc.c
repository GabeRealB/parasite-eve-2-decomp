/* Part of the Odd Stranger library; see odd_stranger.h. */

/// State 14: clip 7 with the player sent animation 3; during clip frames 0x10..0x16 it steps field_C0C (halved on contact). At the clip boundary it picks state 6 or 0xA from the targeted flag and sight test and sends the closing 0x3F1 if still latched.
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
    if (work->field_4 != 0) {
        work->field_8A2 = 0x10;
        work->field_89E = 7;
        work->field_898 = 2;
        oddStrangerDrive(arg0);
        msg              = &gOddStrangerPlayerAnim;
        msg->animationId = 3;
        if (cfg->hp > 0) {
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_REPLACE_AND_PLAY, msg, 0);
        }
        work->field_C0C        = -0x78;
        work->field_6          = 0;
        work->field_A10.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
        return;
    }
    if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x3ED, 0, 0) == 0 && cfg->hp > 0 && work->field_C28 == 1) {
        taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x3F1, 0, 0);
        work->field_C28 = 0;
    }
    if ((u32)((work->field_5A & 0x3FF) - 0x10) < 7U) {
        if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x12C, work->field_C0C) != 0) {
            actorMoveForwardNonzero(arg0->extra.tmd->coords, (u16)work->field_C0C);
        }
        if (ActorContact_PushContact(arg0->extra.tmd->coords, work->field_A30, 0xC) == 1) {
            work->field_C0C = (s16)(u16)work->field_C0C / 2;
        }
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    oddStrangerDrive(arg0);
    if (work->flags_68.half & ANIMATION_SLOT_REACHED_BOUNDARY) {
        kind = enemy->node.state.parts.targeted;
        if (kind == 1) {
            if (detectSightBlocked(arg0) == kind) {
                work->field_0 = 6;
            } else {
                work->field_0 = 0xA;
            }
        } else {
            work->field_0 = 6;
        }
        if (cfg->hp > 0 && work->field_C28 == 1) {
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x3F1, 0, 0);
            work->field_C28 = 0;
        }
    }
}
