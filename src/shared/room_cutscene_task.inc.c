/* Part of the room cutscene library; see room_cutscene.h. */

/// Task body of the room's cutscene, driven by the
/// `RoomCutsceneRec` in `spawnArg2`. It holds both characters'
/// weapons, hides the HUD, forces the scripted view and selects the loaded CAP resource,
/// then starts the scene's CAP slot together with its sound task (entry 1 of
/// `gRoomCutsceneTaskDescs`); confirm or cancel skips the scene.
/// Afterwards it runs the follow-up CAP command (slot 1 picks it from game
/// flag 0x155), advances the story flags the scene settles, and restores the
/// view, weapons and HUD. While flag 0x155 is 0xE and flag 3 is clear, the end
/// loops through states 20-23 instead, running the command the CAP event key
/// selects until the key is neither 0xB nor 0xC.
void roomCutsceneTask(Task* task)
{
    s32              poll;
    s32              cmd;
    s32              a0;
    s32              a1;
    s32              flag;
    RoomCutsceneRec* script;
    McSaveData*      save;

    script = task->spawnArg2.pointer;
    switch (task->state) {
        case 0:
            ROOM_CUTSCENE_SOUND_TASK = NULL;
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            save = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
            if (save->state.companionType == 1) {
                companionSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            }
            if (script->view > 0) {
                D_80115694                    = save->state.location.loc.view;
                save->state.location.loc.view = script->view;
            } else {
                D_80115694 = -script->view;
            }
            gGameSession->hideHud          = 1;
            gGameSession->eventState       = 1;
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_HIDDEN;
            playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
            companionSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
            if (script->startSound != 0) {
                sndEvtRequestScriptStart(script->startSound, 0, 0);
            }
            task->state++;
            break;
        case 1:
        case 2:
            task->state++;
            break;
        case 3:
            if (script->capFile != 0) {
                Gp_CapFile = 0;
                capSelectLoadedFile(script->capFile);
                a0 = script->capTPageX;
                a1 = 0;
                if (a0 == 0) {
                    a0 = 0x3C0;
                } else {
                    a1 = script->capTPageY;
                }
                capSetTexturePage(a0, a1);
            }
            if (script->skipScene != 0) {
                task->state = 6;
            } else {
                task->state++;
            }
            break;
        case 4:
            ROOM_CUTSCENE_SOUND_TASK =
                taskSpawnFromTable(gRoomCutsceneTaskDescs, 1, 0, script->sceneSound);
            capStartSequenceSlot(script->capSlot, 0, 0x63);
            task->state++;
            break;
        case 5:
            if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm | Pad_MaskCancel) != 0) {
                sndEvtRequestScriptStop(script->sceneSound, SOUND_SCRIPT_STOP_KEEP_RELEASE);
                taskKill(ROOM_CUTSCENE_SOUND_TASK);
                task->state++;
            } else if (taskPollKill(ROOM_CUTSCENE_SOUND_TASK, &poll) != 0) {
                task->state++;
            }
            break;
        case 6:
            Gp_AbortCap();
            task->state++;
            break;
        case 7:
            if (script->skipScene == 0) {
                sndEvtRequestScriptStart(script->afterSceneSound, 0, 0);
            }
            flag = gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER);
            if (flag > 0) {
                if (flag >= 5) {
                    if (flag == 5) {
                        if (gameFlagGetNibble(GAME_FLAG_ITEM_125_EXAMINED) != 0) {
                            if (gameFlagGetNibble(GAME_FLAG_ITEM_125_FOLLOWUP_SEEN) == 0) {
                                gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, 0);
                                gameFlagSetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX, 9);
                                gameFlagSetNibble(GAME_FLAG_ITEM_125_FOLLOWUP_SEEN, 1);
                            }
                        }
                    }
                }
            }
            if (script->capSlot == 1) {
                capRunCommand(gameFlagGetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX) + 0x10, CAP_PLAYBACK_IN_PLACE);
            } else {
                capRunCommand(script->capSlot, CAP_PLAYBACK_IN_PLACE);
            }
            if (gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER) == 1) {
                if (gameFlagGetNibble(0) == 2) {
                    gameFlagSetNibble(0, 3);
                    gameFlagSetNibble(GAME_FLAG_00E, 4);
                    if ((GAME_LOCATION_WORD(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(1, 1, 0, 0)) {
                        areaApplySavedUpdates(D_acropolis_square_80188888);
                        gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, 5);
                    }
                }
            }
            task->state++;
            break;
        case 8:
            if (capIsBusy() == 0) {
                if ((gameFlagGetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX) == 0xE) && (gameFlagGetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE) == 0)) {
                    gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, 1);
                    task->state = 0x14;
                } else {
                    capRunCommandWithTransition(task->spawnArg1.value);
                    task->state++;
                }
            }
            break;
        case 9:
            if (capIsBusy() == 0) {
                task->state++;
            }
            break;
        case 10:
            task->state++;
            break;
        case 11:
            playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
            companionSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = (u8)D_80115694;
            task->state++;
            break;
        case 12:
        case 13:
            task->state++;
            break;
        case 14:
            sndEvtRequestScriptStart(script->endSound, 0, 0);
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType == 1) {
                companionSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            }
            gGameSession->hideHud          = 0;
            gGameSession->eventState       = 0;
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
            if (script->capFile != 0) {
                capReset();
            }
            D_80114D08 = 0xA;
            taskKill(task);
            break;
        case 15:
        case 16:
        case 17:
        case 18:
        case 19:
            break;
        case 20:
            capRunCommand(gameFlagGetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX) + 0x10, CAP_PLAYBACK_IN_PLACE);
            task->state++;
            break;
        case 21:
            if (capIsBusy() == 0) {
                task->state++;
            }
            break;
        case 22:
            switch (capGetVariantKey()) {
                case 11:
                    capRunCommand(0x20, CAP_PLAYBACK_IN_PLACE);
                    task->state++;
                    break;
                case 12:
                    capRunCommand(0x21, CAP_PLAYBACK_IN_PLACE);
                    task->state++;
                    break;
                default:
                    gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, 2);
                    task->state = 8;
                    break;
            }
            break;
        case 23:
            if (capIsBusy() == 0) {
                task->state = 0x14;
            }
            break;
    }
}
