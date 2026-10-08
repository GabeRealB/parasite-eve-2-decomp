/* Included General Store underpass transition; each carrier declares its static instance. */

/// Restores the presentation and live view held by the underpass prompt.
static inline void _generalStoreCancelUnderpassTransition(void)
{
    gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
    sndEvtRequestStageScriptStart(SOUND_GENERAL_STORE_UNDERPASS_CANCEL, 0, 0);
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = gStoreSavedView;
    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
    playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO);
}

/// Prompts for the General Store's underpass departure and commits or cancels it.
///
/// Start in state 0 with the destination warp and room latched in `gStoreWarp`
/// and `gStoreRoom`. Holds player control, hides its model and saves the live
/// view before selecting prompt view 16. CAP key 11 requests a subtractive
/// eight-frame fade and a reload from the latched underpass destination;
/// another key restores the view, player presentation and actor control.
/// The intervening states each consume one callback tick; the task does not
/// poll CAP completion or fade completion. Spawn arguments are unused.
/// Keep the room loaded through the task and its borrowed `gStoreFade` record
/// live for the fade task. Spawn failures do not roll back the transition.
static void _generalStoreUnderpassTransitionTask(Task* task)
{
    enum {
        GENERAL_STORE_TRANSITION_HOLD_PLAYER    = 0,
        GENERAL_STORE_TRANSITION_BEFORE_PROMPT  = 1,
        GENERAL_STORE_TRANSITION_START_PROMPT   = 2,
        GENERAL_STORE_TRANSITION_AFTER_PROMPT   = 3,
        GENERAL_STORE_TRANSITION_CHECK_CHOICE   = 4,
        GENERAL_STORE_TRANSITION_COMMIT         = 5,
        GENERAL_STORE_TRANSITION_PROMPT_VIEW    = 16,
        GENERAL_STORE_TRANSITION_CAP_COMMAND    = 15,
        GENERAL_STORE_TRANSITION_ACCEPT_KEY     = 11,
        GENERAL_STORE_TRANSITION_FADE_FRAMES    = 8,
        GENERAL_STORE_TRANSITION_FADE_TASK_BANK = 1,
        GENERAL_STORE_TRANSITION_FADE_TASK_TYPE = 0x31,
        GENERAL_STORE_CAP_KEEP_ACTORS_PAUSED    = 1,
        GENERAL_STORE_RELOAD_SPRITE_VARIANT     = 1,
    };
    switch (task->state) {
        case GENERAL_STORE_TRANSITION_HOLD_PLAYER:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
            gStoreSavedView                                            = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = GENERAL_STORE_TRANSITION_PROMPT_VIEW;
            task->state                                               += 1;
            return;
        case GENERAL_STORE_TRANSITION_BEFORE_PROMPT:
        case GENERAL_STORE_TRANSITION_AFTER_PROMPT:
            task->state += 1;
            return;
        case GENERAL_STORE_TRANSITION_START_PROMPT:
            sndEvtRequestStageScriptStart(SOUND_GENERAL_STORE_UNDERPASS_PROMPT, 0, 0);
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            capRunCommandWithTransition(GENERAL_STORE_TRANSITION_CAP_COMMAND);
            D_80115690   = GENERAL_STORE_CAP_KEEP_ACTORS_PAUSED;
            task->state += 1;
            return;
        case GENERAL_STORE_TRANSITION_CHECK_CHOICE:
            if (capGetVariantKey() == GENERAL_STORE_TRANSITION_ACCEPT_KEY) {
                gStoreFade.blend      = SCREEN_FADE_SUBTRACT;
                gStoreFade.phase      = SCREEN_FADE_RUNNING;
                gStoreFade.rampFrames = GENERAL_STORE_TRANSITION_FADE_FRAMES;
                taskSpawn(GENERAL_STORE_TRANSITION_FADE_TASK_BANK, GENERAL_STORE_TRANSITION_FADE_TASK_TYPE, 0, &gStoreFade);
                task->state += 1;
                return;
            }
            _generalStoreCancelUnderpassTransition();
            break;
        case GENERAL_STORE_TRANSITION_COMMIT:
            // Reload from the live save after committing the latched destination.
            sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = GAME_AREA_DRYFIELD_UNDERPASS;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = gStoreWarp;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = gStoreRoom;
            gDisplayState.spriteVariant                                = GENERAL_STORE_RELOAD_SPRITE_VARIANT;
            taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
            break;
        default:
            return;
    }
    taskKill(task);
}
