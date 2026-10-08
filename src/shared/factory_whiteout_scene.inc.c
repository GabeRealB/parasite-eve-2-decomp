/* Part of the factory lift library; see factory_lift.h. */

void factoryBarrierTransitionScene(Task* task)
{
    enum {
        FACTORY_BARRIER_TRANSITION_SCENE_START = 0,
        FACTORY_BARRIER_TRANSITION_SCENE_BEFORE_CHOICE,
        FACTORY_BARRIER_TRANSITION_SCENE_CHECK_CHOICE,
        FACTORY_BARRIER_TRANSITION_SCENE_FADE_OUT,
        FACTORY_BARRIER_TRANSITION_SCENE_CLEAR_BARRIER,
        FACTORY_BARRIER_TRANSITION_SCENE_CHANGE_ROOM,
        FACTORY_BARRIER_TRANSITION_SCENE_FADE_IN,
        FACTORY_BARRIER_TRANSITION_SCENE_RESTORE  = -1,
        FACTORY_BARRIER_TRANSITION_CAP_ACCEPTED   = 1,
        FACTORY_BARRIER_TRANSITION_FADE_FRAMES    = 30,
        FACTORY_BARRIER_TRANSITION_FULL_INTENSITY = 255,
        FACTORY_BARRIER_TRANSITION_ROOM           = 2,
    };
    u8 fadeIntensity;
    u8 stageId;

    switch (task->state) {
        case FACTORY_BARRIER_TRANSITION_SCENE_START:
            if (gameFlagGetNibble(GAME_FLAG_FACTORY_BARRIER_CLEARED) != 0) {
                taskKill(task);
                return;
            }
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            companionSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            capRunCommandWithTransition(task->spawnArg1.value);
            task->state = task->state + 1;
            return;
        case FACTORY_BARRIER_TRANSITION_SCENE_CHECK_CHOICE:
            if (capGetVariantKey() == FACTORY_BARRIER_TRANSITION_CAP_ACCEPTED) {
                task->killCountdown = 0;
                if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
                    sndEvtRequestStageScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_FACTORY, 0x0C), 0, 0);
                }
                task->state = task->state + 1;
                return;
            }
            task->state = FACTORY_BARRIER_TRANSITION_SCENE_RESTORE;
            return;
        case FACTORY_BARRIER_TRANSITION_SCENE_FADE_OUT:
            task->killCountdown = task->killCountdown + 1;
            if (task->killCountdown >= FACTORY_BARRIER_TRANSITION_FADE_FRAMES) {
                task->state = task->state + 1;
            }
            fadeIntensity = (task->killCountdown * FACTORY_BARRIER_TRANSITION_FULL_INTENSITY) / FACTORY_BARRIER_TRANSITION_FADE_FRAMES;
            fadeDrawOverlay(fadeIntensity, fadeIntensity, fadeIntensity, GPU_BLEND_SUBTRACT);
            return;
        case FACTORY_BARRIER_TRANSITION_SCENE_CLEAR_BARRIER:
            // Cover the room-variant switch while its barrier geometry is rebuilt.
            gGameSession->viewDirty = 1;
            gameFlagSetNibble(GAME_FLAG_FACTORY_BARRIER_CLEARED, 1);
            stageId = gGameSession->location.loc.stage;
            if (stageId == GAME_STAGE_DRYFIELD) {
                sndEvtRequestStageScriptStart(SOUND_FACTORY_WHITEOUT, 0, 0);
            }
            fadeDrawOverlay(FACTORY_BARRIER_TRANSITION_FULL_INTENSITY, FACTORY_BARRIER_TRANSITION_FULL_INTENSITY, FACTORY_BARRIER_TRANSITION_FULL_INTENSITY, GPU_BLEND_SUBTRACT);
            task->state = task->state + 1;
            return;
        case FACTORY_BARRIER_TRANSITION_SCENE_CHANGE_ROOM:
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = FACTORY_BARRIER_TRANSITION_ROOM;
            gGameSession->location.loc.room                            = FACTORY_BARRIER_TRANSITION_ROOM;
            gGameSession->roomObjsDirty                                = 1;
            fadeDrawOverlay(FACTORY_BARRIER_TRANSITION_FULL_INTENSITY, FACTORY_BARRIER_TRANSITION_FULL_INTENSITY, FACTORY_BARRIER_TRANSITION_FULL_INTENSITY, GPU_BLEND_SUBTRACT);
            task->state = task->state + 1;
            return;
        case FACTORY_BARRIER_TRANSITION_SCENE_BEFORE_CHOICE:
            task->state = task->state + 1;
            return;
        case FACTORY_BARRIER_TRANSITION_SCENE_FADE_IN:
            task->killCountdown = task->killCountdown - 1;
            if (task->killCountdown <= 0) {
                task->state = task->state + 1;
            }
            fadeIntensity = (task->killCountdown * FACTORY_BARRIER_TRANSITION_FULL_INTENSITY) / FACTORY_BARRIER_TRANSITION_FADE_FRAMES;
            fadeDrawOverlay(fadeIntensity, fadeIntensity, fadeIntensity, GPU_BLEND_SUBTRACT);
            return;
        default:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            companionSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            taskKill(task);
            return;
    }
}
