/* Part of the factory lift library; see factory_lift.h. */

/// Plays the operator-panel button cue from the active day/night factory bank.
///
/// Uses centred pan/depth. Requires initialized session and sound state.
static inline void _factoryPanelPlayButtonSound(void)
{
    enum {
        FACTORY_PANEL_BUTTON_SOUND     = 9,
        FACTORY_PANEL_DAY_SOUND_AREA   = SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_FACTORY, 0),
        FACTORY_PANEL_NIGHT_SOUND_AREA = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_FACTORY, 0)
    };
    s32 soundArea;

    soundArea = FACTORY_PANEL_NIGHT_SOUND_AREA;
    if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
        soundArea = FACTORY_PANEL_DAY_SOUND_AREA;
    }
    sndEvtRequestScriptStart(soundArea | FACTORY_PANEL_BUTTON_SOUND, 0, 0);
}

/// Applies the confirmed operator-panel choice and selects the next panel state.
///
/// Choices 0/1/2 request raise/lower/turn only when power is on; an unavailable
/// move starts its caption instead. Choices 3/4 are caption-only. A real move
/// selects saved view 18 or 19 and waits for the lift; other paths return to idle.
/// Unsupported powered choices leave the state alone; unsupported unpowered
/// choices still return to idle. Requires a live panel task and initialized
/// session, save, sound and CAP state. Does not retain any new resource.
static void _factoryPanelApplyChoice(Task* task, s16 choice)
{
    enum {
        FACTORY_PANEL_CHOICE_RAISE              = 0,
        FACTORY_PANEL_CHOICE_LOWER              = 1,
        FACTORY_PANEL_CHOICE_TURN               = 2,
        FACTORY_PANEL_CHOICE_CAPTION6           = 3,
        FACTORY_PANEL_CHOICE_CAPTION7           = 4,
        FACTORY_PANEL_CAPTION6                  = 6,
        FACTORY_PANEL_CAPTION7                  = 7,
        FACTORY_PANEL_CAPTION_CANNOT_RAISE      = 8,
        FACTORY_PANEL_CAPTION_CANNOT_LOWER      = 9,
        FACTORY_PANEL_CAPTION_CANNOT_TURN       = 10,
        FACTORY_PANEL_MOVE_VIEW_BARRIER_PRESENT = 18,
        FACTORY_PANEL_MOVE_VIEW_BARRIER_CLEARED = 19,
    };
    s32 nextState;

    // Power gates lift movement; captions remain available without it.
    if (gameFlagGetNibble(GAME_FLAG_FACTORY_POWER_ON) != 0) {
        switch (choice) {
            case FACTORY_PANEL_CHOICE_RAISE:
                _factoryPanelPlayButtonSound();
                if (!(gameFlagGetNibble(GAME_FLAG_FACTORY_LIFT_POSITION) & FACTORY_LIFT_POSITION_RAISED)) {
                    gameFlagSetNibble(GAME_FLAG_FACTORY_LIFT_POSITION, gameFlagGetNibble(GAME_FLAG_FACTORY_LIFT_POSITION) | FACTORY_LIFT_POSITION_RAISED);
                    if (gameFlagGetNibble(GAME_FLAG_FACTORY_BARRIER_CLEARED) == 0) {
                        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = FACTORY_PANEL_MOVE_VIEW_BARRIER_PRESENT;
                    } else {
                        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = FACTORY_PANEL_MOVE_VIEW_BARRIER_CLEARED;
                    }
                    nextState = FACTORY_PANEL_STATE_WAIT_MOVE;
                } else {
                    capStartSequenceSlot(FACTORY_PANEL_CAPTION_CANNOT_RAISE, 0, 0);
                    nextState = FACTORY_PANEL_STATE_IDLE;
                }
                task->state = nextState;
                break;
            case FACTORY_PANEL_CHOICE_LOWER:
                _factoryPanelPlayButtonSound();
                if (gameFlagGetNibble(GAME_FLAG_FACTORY_LIFT_POSITION) & FACTORY_LIFT_POSITION_RAISED) {
                    gameFlagSetNibble(GAME_FLAG_FACTORY_LIFT_POSITION, gameFlagGetNibble(GAME_FLAG_FACTORY_LIFT_POSITION) & ~FACTORY_LIFT_POSITION_RAISED);
                    if (gameFlagGetNibble(GAME_FLAG_FACTORY_BARRIER_CLEARED) == 0) {
                        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = FACTORY_PANEL_MOVE_VIEW_BARRIER_PRESENT;
                    } else {
                        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = FACTORY_PANEL_MOVE_VIEW_BARRIER_CLEARED;
                    }
                    nextState = FACTORY_PANEL_STATE_WAIT_MOVE;
                } else {
                    capStartSequenceSlot(FACTORY_PANEL_CAPTION_CANNOT_LOWER, 0, 0);
                    nextState = FACTORY_PANEL_STATE_IDLE;
                }
                task->state = nextState;
                break;
            case FACTORY_PANEL_CHOICE_TURN:
                _factoryPanelPlayButtonSound();
                gameFlagSetNibble(GAME_FLAG_FACTORY_LIFT_POSITION, gameFlagGetNibble(GAME_FLAG_FACTORY_LIFT_POSITION) ^ FACTORY_LIFT_POSITION_TURNED);
                if (gameFlagGetNibble(GAME_FLAG_FACTORY_BARRIER_CLEARED) == 0) {
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = FACTORY_PANEL_MOVE_VIEW_BARRIER_PRESENT;
                } else {
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = FACTORY_PANEL_MOVE_VIEW_BARRIER_CLEARED;
                }
                nextState   = FACTORY_PANEL_STATE_WAIT_MOVE;
                task->state = nextState;
                break;
            case FACTORY_PANEL_CHOICE_CAPTION6:
                capStartSequenceSlot(FACTORY_PANEL_CAPTION6, 0, 1);
                nextState   = FACTORY_PANEL_STATE_IDLE;
                task->state = nextState;
                break;
            case FACTORY_PANEL_CHOICE_CAPTION7:
                capStartSequenceSlot(FACTORY_PANEL_CAPTION7, 0, 0);
                nextState   = FACTORY_PANEL_STATE_IDLE;
                task->state = nextState;
                break;
        }
    } else {
        switch (choice) {
            case FACTORY_PANEL_CHOICE_RAISE:
                _factoryPanelPlayButtonSound();
                capStartSequenceSlot(FACTORY_PANEL_CAPTION_CANNOT_RAISE, 0, 0);
                break;
            case FACTORY_PANEL_CHOICE_LOWER:
                _factoryPanelPlayButtonSound();
                capStartSequenceSlot(FACTORY_PANEL_CAPTION_CANNOT_LOWER, 0, 0);
                break;
            case FACTORY_PANEL_CHOICE_TURN:
                _factoryPanelPlayButtonSound();
                capStartSequenceSlot(FACTORY_PANEL_CAPTION_CANNOT_TURN, 0, 0);
                break;
            case FACTORY_PANEL_CHOICE_CAPTION6:
                capStartSequenceSlot(FACTORY_PANEL_CAPTION6, 0, 0);
                break;
            case FACTORY_PANEL_CHOICE_CAPTION7:
                capStartSequenceSlot(FACTORY_PANEL_CAPTION7, 0, 0);
                break;
        }
        task->state = FACTORY_PANEL_STATE_IDLE;
    }
}
