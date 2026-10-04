/* Part of the room events library; see room_events.h. */

/// The room's departure task, run on the descriptor the room staged in
/// `ROOM_DEPARTURE`. State 0 sends the descriptor's `facing` to the slot-3
/// game pointer as message 0x3EE, skipping to state 2 when it is
/// `ROOM_DEPARTURE_SKIP_FACING`; state 1 waits until that pointer answers
/// 0x3F0 with 0. States 2 and 3 play the sound event `sndEvent`, if any, and
/// wait for its voice to go quiet. State 4 queues type-7 sound event
/// 0x80000000, copies the descriptor's stage, area, warp and room into the
/// save location, starts room-change task 0x11 and kills itself.
void roomDepartureTask(Task* arg0)
{
    ActorTransform msg;
    Task*          playerTask;

    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    switch (arg0->state) {
        case 0:
            msg.rot.vy = ROOM_DEPARTURE.facing;
            if (msg.rot.vy == ROOM_DEPARTURE_SKIP_FACING) {
                arg0->state = 2;
                break;
            }
            TASK_MESSAGE_DISPATCH_POINTER(playerTask, 0x3EE, &msg, 0);
            arg0->state = (s32)(arg0->state + 1);
            break;
        case 1:
            if (taskMessageDispatch(playerTask, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
                arg0->state = (s32)(arg0->state + 1);
            }
            break;
        case 2:
            if (ROOM_DEPARTURE.sndEvent == 0) {
                arg0->state = 4;
                break;
            }
            sndEvtRequestScriptStart(ROOM_DEPARTURE.sndEvent, 0, 0);
            arg0->state = (s32)(arg0->state + 1);
            break;
        case 3:
            if (SndVoice_HasActiveId(ROOM_DEPARTURE.sndEvent) == 0) {
                arg0->state = (s32)(arg0->state + 1);
            }
            break;
        case 4:
            SndEvt_EnqueueType7((s32)SOUND_BANK_TYPE_ALL_NON_AMBIENT, 0);
            gDisplayState.spriteVariant                                 = 1;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage = ROOM_DEPARTURE.stage;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area  = ROOM_DEPARTURE.area;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp  = ROOM_DEPARTURE.warp;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room  = ROOM_DEPARTURE.room;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
        default:
            break;
    }
}
