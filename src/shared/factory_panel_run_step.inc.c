/* Part of the factory lift library; see factory_lift.h. */

/// Runs cap step `step` of the room's script, picking the sound, the progress
/// flags and the cap slot for the step.
void factoryPanelRunStep(Task* task, s16 step)
{
    s32 id;
    s32 state;

    if (gameFlagGetNibble(GAME_FLAG_FACTORY_POWER_ON) != 0) {
        switch (step) {
            case 0:
                id = 0x53170000;
                if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
                    id = 0x52170000;
                }
                sndEvtRequestScriptStart(id | 9, 0, 0);
                if (!(gameFlagGetNibble(GAME_FLAG_FACTORY_LIFT_POSITION) & 2)) {
                    gameFlagSetNibble(GAME_FLAG_FACTORY_LIFT_POSITION, gameFlagGetNibble(GAME_FLAG_FACTORY_LIFT_POSITION) | 2);
                    if (gameFlagGetNibble(GAME_FLAG_FACTORY_BARRIER_CLEARED) == 0) {
                        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 0x12;
                    } else {
                        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 0x13;
                    }
                    state = 6;
                } else {
                    capStartSequenceSlot(8, 0, 0);
                    state = 2;
                }
                task->state = state;
                break;
            case 1:
                id = 0x53170000;
                if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
                    id = 0x52170000;
                }
                sndEvtRequestScriptStart(id | 9, 0, 0);
                if (gameFlagGetNibble(GAME_FLAG_FACTORY_LIFT_POSITION) & 2) {
                    gameFlagSetNibble(GAME_FLAG_FACTORY_LIFT_POSITION, gameFlagGetNibble(GAME_FLAG_FACTORY_LIFT_POSITION) & ~2);
                    if (gameFlagGetNibble(GAME_FLAG_FACTORY_BARRIER_CLEARED) == 0) {
                        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 0x12;
                    } else {
                        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 0x13;
                    }
                    state = 6;
                } else {
                    capStartSequenceSlot(9, 0, 0);
                    state = 2;
                }
                task->state = state;
                break;
            case 2:
                id = 0x53170000;
                if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
                    id = 0x52170000;
                }
                sndEvtRequestScriptStart(id | 9, 0, 0);
                gameFlagSetNibble(GAME_FLAG_FACTORY_LIFT_POSITION, gameFlagGetNibble(GAME_FLAG_FACTORY_LIFT_POSITION) ^ 1);
                if (gameFlagGetNibble(GAME_FLAG_FACTORY_BARRIER_CLEARED) == 0) {
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 0x12;
                } else {
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 0x13;
                }
                state       = 6;
                task->state = state;
                break;
            case 3:
                capStartSequenceSlot(6, 0, 1);
                state       = 2;
                task->state = state;
                break;
            case 4:
                capStartSequenceSlot(7, 0, 0);
                state       = 2;
                task->state = state;
                break;
        }
    } else {
        switch (step) {
            case 0:
                id = 0x53170000;
                if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
                    id = 0x52170000;
                }
                sndEvtRequestScriptStart(id | 9, 0, 0);
                capStartSequenceSlot(8, 0, 0);
                break;
            case 1:
                id = 0x53170000;
                if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
                    id = 0x52170000;
                }
                sndEvtRequestScriptStart(id | 9, 0, 0);
                capStartSequenceSlot(9, 0, 0);
                break;
            case 2:
                id = 0x53170000;
                if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
                    id = 0x52170000;
                }
                sndEvtRequestScriptStart(id | 9, 0, 0);
                capStartSequenceSlot(0xA, 0, 0);
                break;
            case 3:
                capStartSequenceSlot(6, 0, 0);
                break;
            case 4:
                capStartSequenceSlot(7, 0, 0);
                break;
        }
        task->state = 2;
    }
}
