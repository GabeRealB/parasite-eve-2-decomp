/* Part of the room events library; see room_events.h. */

/// The room's departure task, run on the descriptor the message handler
/// staged in `ROOM_DEPARTURE`. State 0 sends the descriptor's `facing` to the slot-3 game pointer as message 0x3EE, skipping
/// to state 2 when it is -1; state 1 waits until that pointer answers 0x3F0
/// with 0. States 2 and 3 play the sound event `sndEvent`, if any, and wait for
/// its voice to go quiet. State 4 queues type-7 sound event 0x80000000, commits
/// the save location in the descriptor's first four bytes (stage, area, warp,
/// room), re-spawns the player task as type 0x11 and kills itself.
void roomDepartureTask(Task* arg0)
{
    ActorTransform msg;
    void*          slot;

    slot = gameGetPtrSlot(3);
    switch (arg0->state) {
        case 0:
            msg.rot.vy = ROOM_DEPARTURE.facing;
            if (msg.rot.vy == -1) {
                arg0->state = 2;
                break;
            }
            Gp_DispatchMsgPtr(slot, 0x3EE, &msg, 0);
            arg0->state = (s32)(arg0->state + 1);
            break;
        case 1:
            if (Gp_DispatchMsg(slot, 0x3F0, 0, 0) == 0) {
                arg0->state = (s32)(arg0->state + 1);
            }
            break;
        case 2:
            if (ROOM_DEPARTURE.sndEvent == 0) {
                arg0->state = 4;
                break;
            }
            SndEvt_EnqueueType6(ROOM_DEPARTURE.sndEvent, 0, 0);
            arg0->state = (s32)(arg0->state + 1);
            break;
        case 3:
            if (SndVoice_HasActiveId(ROOM_DEPARTURE.sndEvent) == 0) {
                arg0->state = (s32)(arg0->state + 1);
            }
            break;
        case 4:
            SndEvt_EnqueueType7((s32)0x80000000, 0);
            gDisplayState.spriteVariant             = 1;
            Mc_SaveData[0].state.location.loc.stage = ROOM_DEPARTURE.stage;
            Mc_SaveData[0].state.location.loc.area  = ROOM_DEPARTURE.area;
            Mc_SaveData[0].state.location.loc.warp  = ROOM_DEPARTURE.warp;
            Mc_SaveData[0].state.location.loc.room  = ROOM_DEPARTURE.room;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
        default:
            break;
    }
}
