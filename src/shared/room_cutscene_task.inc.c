/* Part of the room cutscene library; see room_cutscene.h. */

/// Only companion family 1 participates in this runner's scripted-control hold.
enum { ROOM_CUTSCENE_COMPANION_FAMILY = 1 };

/// Resumes actors and HUD after a room cutscene and restores any replaced CAP selection.
///
/// Borrows the live cutscene record for this call. The player must be live;
/// companion family 1 also resumes if present. Clears the session event hold
/// and resumes combat actors after the caller has restored model drawing and
/// the view. Nonzero `capFile` restores the loaded bundle's default CAP file,
/// texture page and playback state, requiring that bundle to remain loaded.
static inline void _roomCutsceneReleasePresentation(const RoomCutsceneRec* cutscene)
{
    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType == ROOM_CUTSCENE_COMPANION_FAMILY) {
        companionSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
    }
    gGameSession->hideHud          = 0;
    gGameSession->eventState       = 0;
    gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
    if (cutscene->capFile != 0) {
        capReset();
    }
}

/// Runs a room-owned cutscene, its sound task and the post-scene dialogue.
///
/// spawnArg2 borrows a `RoomCutsceneRec` that must remain unchanged and loaded
/// until this task ends. spawnArg1 is the post-scene CAP command. Runs states
/// 0..14 with frame gaps and an optional story-dialogue loop at 20..23.
/// Holds player/companion control, hides actors and HUD, selects the loaded CAP
/// file and scripted view, and starts the CAP slot with descriptor entry 1's
/// sound task. Confirm/cancel skips that scene; sound-task completion also ends
/// it. Advances story flags and restores the saved view, actors and control.
/// Requires a live descriptor table, CAP resources and non-overlapping use of
/// the room's sound handle and the shared saved-view storage.
static void _roomCutsceneTask(Task* task)
{
    enum {
        ROOM_CUTSCENE_INTERACTION_REARM_UPDATES = 10,
        ROOM_CUTSCENE_STORY_CAP_SLOT            = 1,
        ROOM_CUTSCENE_ITEM_FOLLOW_UP_CHAPTER    = 5,
        ROOM_CUTSCENE_ITEM_FOLLOW_UP_DIALOGUE   = 9,
        ROOM_CUTSCENE_REPEAT_DIALOGUE           = 14,
        ROOM_CUTSCENE_FOLLOW_UP_CLEAR           = 0,
        ROOM_CUTSCENE_FOLLOW_UP_RUNNING         = 1,
        ROOM_CUTSCENE_FOLLOW_UP_COMPLETE        = 2,
        ROOM_CUTSCENE_OPENING_CHAPTER           = 1,
        ROOM_CUTSCENE_ACROPOLIS_PROGRESS_BEFORE = 2,
        ROOM_CUTSCENE_ACROPOLIS_PROGRESS_AFTER  = 3,
        ROOM_CUTSCENE_POST_SCENE_FLAG_VALUE     = 4,
        ROOM_CUTSCENE_SQUARE_OBJECTIVE          = 5,
        ROOM_CUTSCENE_DEFAULT_TPAGE_X           = 960,
        ROOM_CUTSCENE_SCENE_VARIANT             = 99,
        ROOM_CUTSCENE_DIALOGUE_COMMAND_BASE     = 16,
        ROOM_CUTSCENE_LOOP_CHOICE_1             = 11,
        ROOM_CUTSCENE_LOOP_CHOICE_2             = 12,
        ROOM_CUTSCENE_LOOP_COMMAND_1            = 32,
        ROOM_CUTSCENE_LOOP_COMMAND_2            = 33,
    };
    enum {
        ROOM_CUTSCENE_STATE_HOLD             = 0,
        ROOM_CUTSCENE_STATE_HOLD_GAP_1       = 1,
        ROOM_CUTSCENE_STATE_HOLD_GAP_2       = 2,
        ROOM_CUTSCENE_STATE_SELECT_CAP       = 3,
        ROOM_CUTSCENE_STATE_START_SCENE      = 4,
        ROOM_CUTSCENE_STATE_WAIT_SCENE       = 5,
        ROOM_CUTSCENE_STATE_ABORT_CAP        = 6,
        ROOM_CUTSCENE_STATE_START_DIALOGUE   = 7,
        ROOM_CUTSCENE_STATE_WAIT_DIALOGUE    = 8,
        ROOM_CUTSCENE_STATE_WAIT_FOLLOW_UP   = 9,
        ROOM_CUTSCENE_STATE_RESTORE_GAP      = 10,
        ROOM_CUTSCENE_STATE_RESTORE_VIEW     = 11,
        ROOM_CUTSCENE_STATE_VIEW_GAP_1       = 12,
        ROOM_CUTSCENE_STATE_VIEW_GAP_2       = 13,
        ROOM_CUTSCENE_STATE_FINISH           = 14,
        ROOM_CUTSCENE_STATE_UNUSED_15        = 15,
        ROOM_CUTSCENE_STATE_UNUSED_16        = 16,
        ROOM_CUTSCENE_STATE_UNUSED_17        = 17,
        ROOM_CUTSCENE_STATE_UNUSED_18        = 18,
        ROOM_CUTSCENE_STATE_UNUSED_19        = 19,
        ROOM_CUTSCENE_STATE_START_LOOP       = 20,
        ROOM_CUTSCENE_STATE_WAIT_LOOP        = 21,
        ROOM_CUTSCENE_STATE_SELECT_LOOP      = 22,
        ROOM_CUTSCENE_STATE_WAIT_LOOP_CHOICE = 23,
    };
    s32                    soundTaskStatus;
    s32                    tpageX;
    s32                    tpageY;
    s32                    storyChapter;
    const RoomCutsceneRec* cutscene;
    McSaveData*            save;

    cutscene = task->spawnArg2.pointer;
    switch (task->state) {
        // Take over presentation before selecting the scene resource.
        case ROOM_CUTSCENE_STATE_HOLD:
            ROOM_CUTSCENE_SOUND_TASK = NULL;
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            save = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
            if (save->state.companionType == ROOM_CUTSCENE_COMPANION_FAMILY) {
                companionSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            }
            if (cutscene->view > 0) {
                D_80115694                    = save->state.location.loc.view;
                save->state.location.loc.view = cutscene->view;
            } else {
                D_80115694 = -cutscene->view;
            }
            gGameSession->hideHud          = 1;
            gGameSession->eventState       = 1;
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_HIDDEN;
            playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
            companionSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
            if (cutscene->startSound != 0) {
                sndEvtRequestScriptStart(cutscene->startSound, 0, 0);
            }
            task->state++;
            break;
        case ROOM_CUTSCENE_STATE_HOLD_GAP_1:
        case ROOM_CUTSCENE_STATE_HOLD_GAP_2:
            task->state++;
            break;
        case ROOM_CUTSCENE_STATE_SELECT_CAP:
            if (cutscene->capFile != 0) {
                Gp_CapFile = NULL;
                capSelectLoadedFile(cutscene->capFile);
                tpageX = cutscene->capTPageX;
                tpageY = 0;
                if (tpageX == 0) {
                    tpageX = ROOM_CUTSCENE_DEFAULT_TPAGE_X;
                } else {
                    tpageY = cutscene->capTPageY;
                }
                capSetTexturePage(tpageX, tpageY);
            }
            if (cutscene->skipScene != 0) {
                task->state = ROOM_CUTSCENE_STATE_ABORT_CAP;
            } else {
                task->state++;
            }
            break;
        case ROOM_CUTSCENE_STATE_START_SCENE:
            ROOM_CUTSCENE_SOUND_TASK =
                taskSpawnFromTable(gRoomCutsceneTaskDescs, 1, 0, cutscene->sceneSound);
            capStartSequenceSlot(cutscene->capSlot, CAP_PLAYBACK_IN_PLACE, ROOM_CUTSCENE_SCENE_VARIANT);
            task->state++;
            break;
        case ROOM_CUTSCENE_STATE_WAIT_SCENE:
            if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm | Pad_MaskCancel) != 0) {
                sndEvtRequestScriptStop(cutscene->sceneSound, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                taskKill(ROOM_CUTSCENE_SOUND_TASK);
                task->state++;
            } else if (taskPollKill(ROOM_CUTSCENE_SOUND_TASK, &soundTaskStatus) != 0) {
                task->state++;
            }
            break;
        case ROOM_CUTSCENE_STATE_ABORT_CAP:
            capAbortPlayback();
            task->state++;
            break;
        // Settle story progress after either natural completion or a skip.
        case ROOM_CUTSCENE_STATE_START_DIALOGUE:
            if (cutscene->skipScene == 0) {
                sndEvtRequestScriptStart(cutscene->afterSceneSound, 0, 0);
            }
            storyChapter = gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER);
            if (storyChapter > 0) {
                if (storyChapter >= ROOM_CUTSCENE_ITEM_FOLLOW_UP_CHAPTER) {
                    if (storyChapter == ROOM_CUTSCENE_ITEM_FOLLOW_UP_CHAPTER) {
                        if (gameFlagGetNibble(GAME_FLAG_ITEM_125_EXAMINED) != 0) {
                            if (gameFlagGetNibble(GAME_FLAG_ITEM_125_FOLLOWUP_SEEN) == 0) {
                                gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, ROOM_CUTSCENE_FOLLOW_UP_CLEAR);
                                gameFlagSetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX, ROOM_CUTSCENE_ITEM_FOLLOW_UP_DIALOGUE);
                                gameFlagSetNibble(GAME_FLAG_ITEM_125_FOLLOWUP_SEEN, 1);
                            }
                        }
                    }
                }
            }
            if (cutscene->capSlot == ROOM_CUTSCENE_STORY_CAP_SLOT) {
                capRunCommand(gameFlagGetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX) + ROOM_CUTSCENE_DIALOGUE_COMMAND_BASE, CAP_PLAYBACK_IN_PLACE);
            } else {
                capRunCommand(cutscene->capSlot, CAP_PLAYBACK_IN_PLACE);
            }
            if (gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER) == ROOM_CUTSCENE_OPENING_CHAPTER) {
                if (gameFlagGetNibble(GAME_FLAG_ACROPOLIS_PROGRESS) == ROOM_CUTSCENE_ACROPOLIS_PROGRESS_BEFORE) {
                    gameFlagSetNibble(GAME_FLAG_ACROPOLIS_PROGRESS, ROOM_CUTSCENE_ACROPOLIS_PROGRESS_AFTER);
                    gameFlagSetNibble(GAME_FLAG_00E, ROOM_CUTSCENE_POST_SCENE_FLAG_VALUE);
                    if ((GAME_LOCATION_WORD(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_SQUARE, 0, 0)) {
                        areaApplySavedUpdates(D_acropolis_square_80188888);
                        gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, ROOM_CUTSCENE_SQUARE_OBJECTIVE);
                    }
                }
            }
            task->state++;
            break;
        case ROOM_CUTSCENE_STATE_WAIT_DIALOGUE:
            if (capIsBusy() == 0) {
                if ((gameFlagGetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX) == ROOM_CUTSCENE_REPEAT_DIALOGUE) && (gameFlagGetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE) == ROOM_CUTSCENE_FOLLOW_UP_CLEAR)) {
                    gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, ROOM_CUTSCENE_FOLLOW_UP_RUNNING);
                    task->state = ROOM_CUTSCENE_STATE_START_LOOP;
                } else {
                    capRunCommandWithTransition(task->spawnArg1.value);
                    task->state++;
                }
            }
            break;
        case ROOM_CUTSCENE_STATE_WAIT_FOLLOW_UP:
            if (capIsBusy() == 0) {
                task->state++;
            }
            break;
        case ROOM_CUTSCENE_STATE_RESTORE_GAP:
            task->state++;
            break;
        // Restore the view before releasing the actor and HUD hold.
        case ROOM_CUTSCENE_STATE_RESTORE_VIEW:
            playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
            companionSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = (u8)D_80115694;
            task->state++;
            break;
        case ROOM_CUTSCENE_STATE_VIEW_GAP_1:
        case ROOM_CUTSCENE_STATE_VIEW_GAP_2:
            task->state++;
            break;
        case ROOM_CUTSCENE_STATE_FINISH:
            sndEvtRequestScriptStart(cutscene->endSound, 0, 0);
            _roomCutsceneReleasePresentation(cutscene);
            D_80114D08 = ROOM_CUTSCENE_INTERACTION_REARM_UPDATES;
            taskKill(task);
            break;
        case ROOM_CUTSCENE_STATE_UNUSED_15:
        case ROOM_CUTSCENE_STATE_UNUSED_16:
        case ROOM_CUTSCENE_STATE_UNUSED_17:
        case ROOM_CUTSCENE_STATE_UNUSED_18:
        case ROOM_CUTSCENE_STATE_UNUSED_19:
            break;
        case ROOM_CUTSCENE_STATE_START_LOOP:
            capRunCommand(gameFlagGetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX) + ROOM_CUTSCENE_DIALOGUE_COMMAND_BASE, CAP_PLAYBACK_IN_PLACE);
            task->state++;
            break;
        case ROOM_CUTSCENE_STATE_WAIT_LOOP:
            if (capIsBusy() == 0) {
                task->state++;
            }
            break;
        case ROOM_CUTSCENE_STATE_SELECT_LOOP:
            switch (capGetVariantKey()) {
                case ROOM_CUTSCENE_LOOP_CHOICE_1:
                    capRunCommand(ROOM_CUTSCENE_LOOP_COMMAND_1, CAP_PLAYBACK_IN_PLACE);
                    task->state++;
                    break;
                case ROOM_CUTSCENE_LOOP_CHOICE_2:
                    capRunCommand(ROOM_CUTSCENE_LOOP_COMMAND_2, CAP_PLAYBACK_IN_PLACE);
                    task->state++;
                    break;
                default:
                    gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, ROOM_CUTSCENE_FOLLOW_UP_COMPLETE);
                    task->state = ROOM_CUTSCENE_STATE_WAIT_DIALOGUE;
                    break;
            }
            break;
        case ROOM_CUTSCENE_STATE_WAIT_LOOP_CHOICE:
            if (capIsBusy() == 0) {
                task->state = ROOM_CUTSCENE_STATE_START_LOOP;
            }
            break;
    }
}
