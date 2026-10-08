/* Part of the general store library; see general_store.h. */

/// The store's cutscene task, the one the shop-till arm of
/// `storeDoorMsg` spawns. It runs as a state script:
/// states 0 and 2 arm the cutscene and states 1 / 3 are the idle steps that
/// wait for CAP command 0xF to finish.
///
/// State 0 silences the player's weapon messages and latches the save's stage
/// byte into `gStoreSavedView` before forcing that byte to
/// 0x10, the stage the cutscene belongs to. State 2 queues stage sound
/// 0x5203000D, hands CAP command 0xF the screen and raises `gSceneCombatState.actorControl` /
/// `D_80115690` with it.
///
/// State 4 is the exit test. CAP event key 0xB means the script asked for the
/// helper task 0x31, which it spawns with a `ScreenFade` whose `rampFrames`
/// is 8; any other key cuts the cutscene short instead -
/// captions off, stage sound 0x5203000E, the latched stage byte back into
/// `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view` and the player's weapon messages re-enabled.
///
/// State 5 is the commit: it queues sound event 0x80000000, points the save's
/// location at area 0x26 with the two latched script arguments as its warp
/// point and room, raises `gDisplayState.spriteVariant` and spawns helper task 0x11.
///
/// Every arm that is finished with the task, state 5's and the cut-short arm of
/// state 4's, leaves through the shared `taskKill` below the switch.
void storeCutsceneTask(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
            gStoreSavedView                                            = gMcSaveData[0].state.location.loc.view;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 0x10;
            arg0->state                                               += 1;
            return;
        case 1:
        case 3:
            arg0->state += 1;
            return;
        case 2:
            sndEvtRequestStageScriptStart(SOUND_GENERAL_STORE_UNDERPASS_PROMPT, 0, 0);
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            capRunCommandWithTransition(0xF);
            D_80115690   = 1;
            arg0->state += 1;
            return;
        case 4:
            if (capGetVariantKey() == 0xB) {
                gStoreFade.blend      = SCREEN_FADE_SUBTRACT;
                gStoreFade.phase      = SCREEN_FADE_RUNNING;
                gStoreFade.rampFrames = 8;
                taskSpawn(1, 0x31, 0, &gStoreFade);
                arg0->state += 1;
                return;
            }
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
            sndEvtRequestStageScriptStart(SOUND_GENERAL_STORE_UNDERPASS_CANCEL, 0, 0);
            gMcSaveData[0].state.location.loc.view = gStoreSavedView;
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
            break;
        case 5:
            sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = 0x26;
            gMcSaveData[0].state.location.loc.warp                     = gStoreWarp;
            gMcSaveData[0].state.location.loc.room                     = gStoreRoom;
            gDisplayState.spriteVariant                                = 1;
            taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
            break;
        default:
            return;
    }
    taskKill(arg0);
}
