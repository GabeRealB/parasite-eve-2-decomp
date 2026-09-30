/* Part of the room events library; see room_events.h. */

/// The event task `gRoomEventTaskDesc` describes, spawned by the gate
/// above once it has latched a request: it runs the request's CAP command,
/// plays and waits out its two sounds (`field_8`, then `field_C`, either
/// skipped when zero), then writes the latched message's destination into the
/// save's location and spawns the room-change task, killing itself.
void roomEventTask(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(ROOM_EVENT_REQ.field_0);
            if (ROOM_EVENT_REQ.field_8 != 0) {
                SndEvt_EnqueueType6(ROOM_EVENT_REQ.field_8, 0, 0);
                task->state++;
            } else {
                task->state = 2;
            }
            break;
        case 1:
            if (SndVoice_HasActiveId(ROOM_EVENT_REQ.field_8) == 0) {
                task->state++;
            }
            break;
        case 2:
            task->state++;
            break;
        case 3:
            if (ROOM_EVENT_REQ.field_C != 0) {
                SndEvt_EnqueueType6(ROOM_EVENT_REQ.field_C, 0, 0);
                task->state++;
            } else {
                task->state = 5;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(ROOM_EVENT_REQ.field_C) == 0) {
                task->state++;
            }
            break;
        case 5:
            SndEvt_EnqueueType7(0x80000000, 0);
            gDisplayState.spriteVariant            = 1;
            Mc_SaveData[0].state.location.loc.area = gRoomEventMsg.areaId;
            Mc_SaveData[0].state.location.loc.warp = gRoomEventMsg.warp;
            Mc_SaveData[0].state.location.loc.room = (u8)gRoomEventMsg.room;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}
