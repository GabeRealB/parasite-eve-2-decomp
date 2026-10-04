/* Part of the room events library; see room_events.h. */

/// The event task `gRoomEventTaskDesc` describes, spawned by the gate
/// above once it has latched a request: it runs the request's CAP command,
/// plays and waits out its two sounds (`firstSnd`, then `secondSnd`, either
/// skipped when zero), then writes the latched message's destination into the
/// save's location and spawns the room-change task, killing itself.
void roomEventTask(Task* task)
{
    switch (task->state) {
        case 0:
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(ROOM_EVENT_REQ.capCmd);
            if (ROOM_EVENT_REQ.firstSnd != 0) {
                sndEvtRequestScriptStart(ROOM_EVENT_REQ.firstSnd, 0, 0);
                task->state++;
            } else {
                task->state = 2;
            }
            break;
        case 1:
            if (SndVoice_HasActiveId(ROOM_EVENT_REQ.firstSnd) == 0) {
                task->state++;
            }
            break;
        case 2:
            task->state++;
            break;
        case 3:
            if (ROOM_EVENT_REQ.secondSnd != 0) {
                sndEvtRequestScriptStart(ROOM_EVENT_REQ.secondSnd, 0, 0);
                task->state++;
            } else {
                task->state = 5;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(ROOM_EVENT_REQ.secondSnd) == 0) {
                task->state++;
            }
            break;
        case 5:
            SndEvt_EnqueueType7(SOUND_BANK_TYPE_ALL_NON_AMBIENT, 0);
            gDisplayState.spriteVariant                                = 1;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = gRoomEventMsg.areaId;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = gRoomEventMsg.warp;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = (u8)gRoomEventMsg.room;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}
