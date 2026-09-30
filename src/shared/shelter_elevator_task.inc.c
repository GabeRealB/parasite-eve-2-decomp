/* Part of the shelter elevator library; see shelter_elevator.h. */

/// Holsters the weapon and waits for the CAP menu. It maps Gp_GetCapEventKey
/// 0xB/0xC/0xD to area 9 warp 3, area 0x1B warp 2 and area 0x2A warp 3
/// (B1/B2/B3); any other key cancels, restoring the weapon and ending the task.
/// It then waits for voice spawnArg1, resolves the room with the Shelter map's
/// resolver, commits warp and room, and spawns room-change task 0x11.
void shelterElevatorTask(Task* task)
{
    RoomEventMsg msg;
    RoomEventMsg msg2;

    switch (task->state) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            Gp_StateF0.field_4 = 1;
            goto next;
        case 1:
            if (Gp_CapBusy() == 0) {
                Gp_StateF0.field_4 = 0;
                goto next;
            }
            break;
        case 2:
            Gp_StateF0.field_4 = 1;
            switch (Gp_GetCapEventKey()) {
                case 0xB:
                    Mc_SaveData[0].state.at4.loc.area = 9;
                    Mc_SaveData[0].state.at4.loc.warp = 3;
                    break;
                case 0xC:
                    Mc_SaveData[0].state.at4.loc.area = 0x1B;
                    Mc_SaveData[0].state.at4.loc.warp = 2;
                    break;
                case 0xD:
                    Mc_SaveData[0].state.at4.loc.area = 0x2A;
                    Mc_SaveData[0].state.at4.loc.warp = 3;
                    break;
                default:
                    Gp_MsgPlayerWeapon(1);
                    Gp_StateF0.field_4 = 0;
                    taskKill(task);
                    break;
            }
            goto next;
        case 3:
            if (SndVoice_HasActiveId(task->spawnArg1.value) != 0) {
                break;
            }
        next:
            task->state++;
            break;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            msg.room      = 1;
            msg.queryOnly = ROOM_EVENT_EXECUTE;
            msg.areaId    = Mc_SaveData[0].state.at4.loc.area;
            msg.warp      = Mc_SaveData[0].state.at4.loc.warp;
            msg2          = msg;
            func_map_shelter_80179A04(&msg, &msg2);
            gDisplayState.spriteVariant       = 1;
            Mc_SaveData[0].state.at4.loc.warp = msg2.warp;
            Mc_SaveData[0].state.at4.loc.room = msg2.room;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}
