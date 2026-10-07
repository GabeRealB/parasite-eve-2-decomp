/* Part of the room events library; see room_events.h. */

/// The room's event task, spawned by its message handler for a latched event.
/// State 0 runs the event's CAP command; state 1 waits for it and, when the
/// event asks for it, starts helper task 0x31; states 2 and 3 play the event's
/// stage sound and wait for it; state 4 writes the latched message's
/// destination into the save data and hands over to task type 0x11.
void roomEventStagedTask(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd(ROOM_EVENT_LATCHED.capCmd, 0);
            D_80115690 = 1;
            arg0->state++;
            break;
        case 1:
            if (capIsBusy() == 0) {
                if (ROOM_EVENT_LATCHED.fade != 0) {
                    ROOM_EVENT_FADE.blend      = SCREEN_FADE_SUBTRACT;
                    ROOM_EVENT_FADE.phase      = SCREEN_FADE_RUNNING;
                    ROOM_EVENT_FADE.rampFrames = 0x1E;
                    taskSpawn(1, 0x31, 0, &ROOM_EVENT_FADE);
                }
                arg0->state++;
            }
            break;
        case 2:
            if (ROOM_EVENT_LATCHED.stageSnd != 0) {
                Gp_EnqueueStageSnd6(ROOM_EVENT_LATCHED.stageSnd, 0, 0);
                arg0->state++;
            } else {
                arg0->state = 4;
            }
            break;
        case 3:
            if (SndVoice_HasActiveId(Gp_PackStageSndId(ROOM_EVENT_LATCHED.stageSnd)) == 0) {
                arg0->state++;
            }
            break;
        case 4:
            sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
            gDisplayState.spriteVariant                                = 1;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = gRoomEventStagedMsg.areaId;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = gRoomEventStagedMsg.warp;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = gRoomEventStagedMsg.room;
            taskSpawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
    }
}
