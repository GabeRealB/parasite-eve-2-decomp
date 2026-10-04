/* Part of the room cutscene library; see room_cutscene.h. */

/// Task body of the room's cutscene, driven by the
/// `RoomCutsceneRec` in `spawnArg2`. It holds both characters'
/// weapons, hides the HUD, forces the scripted view and loads the CAP file,
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
            Gp_MsgPlayerWeapon(0);
            save = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
            if (save->state.companionType == 1) {
                Gp_MsgAllyWeapon(0);
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
            Gp_MsgPlayer3F3(0);
            Gp_MsgAlly3F3(0);
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
                Gp_LoadCapFile(script->capFile);
                a0 = script->capTPageX;
                a1 = 0;
                if (a0 == 0) {
                    a0 = 0x3C0;
                } else {
                    a1 = script->capTPageY;
                }
                func_800E6D4C(a0, a1);
            }
            if (script->skipScene != 0) {
                task->state = 6;
            } else {
                task->state++;
            }
            break;
        case 4:
            ROOM_CUTSCENE_SOUND_TASK =
                Task_SpawnFromTable(gRoomCutsceneTaskDescs, 1, 0, script->sceneSound);
            Gp_StartCapSlot(script->capSlot, 0, 0x63);
            task->state++;
            break;
        case 5:
            if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm | Pad_MaskCancel) != 0) {
                SndEvt_EnqueueType7(script->sceneSound, 1);
                taskKill(ROOM_CUTSCENE_SOUND_TASK);
                task->state++;
            } else if (Task_PollKill(ROOM_CUTSCENE_SOUND_TASK, &poll) != 0) {
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
                Gp_RunCapCmd(gameFlagGetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX) + 0x10, 0);
            } else {
                Gp_RunCapCmd(script->capSlot, 0);
            }
            if (gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER) == 1) {
                if (gameFlagGetNibble(0) == 2) {
                    gameFlagSetNibble(0, 3);
                    gameFlagSetNibble(GAME_FLAG_00E, 4);
                    if ((GAME_LOCATION_WORD(gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc) & GAME_LOCATION_STAGE_AREA_MASK) == GAME_LOCATION_KEY(1, 1, 0, 0)) {
                        Gp_ApplyAreaRecs(D_acropolis_square_80188888);
                        func_800E3FAC(0xA2, 5);
                    }
                }
            }
            task->state++;
            break;
        case 8:
            if (Gp_CapBusy() == 0) {
                if ((gameFlagGetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX) == 0xE) && (gameFlagGetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE) == 0)) {
                    gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, 1);
                    task->state = 0x14;
                } else {
                    Gp_RunCapCmd1(task->spawnArg1.value);
                    task->state++;
                }
            }
            break;
        case 9:
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 10:
            task->state++;
            break;
        case 11:
            Gp_MsgPlayer3F3(1);
            Gp_MsgAlly3F3(1);
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = (u8)D_80115694;
            task->state++;
            break;
        case 12:
        case 13:
            task->state++;
            break;
        case 14:
            sndEvtRequestScriptStart(script->endSound, 0, 0);
            Gp_MsgPlayerWeapon(1);
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType == 1) {
                Gp_MsgAllyWeapon(1);
            }
            gGameSession->hideHud          = 0;
            gGameSession->eventState       = 0;
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
            if (script->capFile != 0) {
                Gp_ResetCap();
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
            Gp_RunCapCmd(gameFlagGetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX) + 0x10, 0);
            task->state++;
            break;
        case 21:
            if (Gp_CapBusy() == 0) {
                task->state++;
            }
            break;
        case 22:
            switch (Gp_GetCapEventKey()) {
                case 11:
                    Gp_RunCapCmd(0x20, 0);
                    task->state++;
                    break;
                case 12:
                    Gp_RunCapCmd(0x21, 0);
                    task->state++;
                    break;
                default:
                    gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, 2);
                    task->state = 8;
                    break;
            }
            break;
        case 23:
            if (Gp_CapBusy() == 0) {
                task->state = 0x14;
            }
            break;
    }
}
