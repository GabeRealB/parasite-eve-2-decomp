/* Part of the shelter elevator library; see shelter_elevator.h. */

/// Holsters the weapon and waits for the CAP menu. It maps capGetVariantKey
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
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            task->state++;
            break;
        case 1:
            if (capIsBusy() == 0) {
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                task->state++;
                break;
            }
            break;
        case 2:
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            switch (capGetVariantKey()) {
                case 0xB:
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = GAME_AREA_SHELTER_B1_ELEVATOR_HALL;
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = 3;
                    break;
                case 0xC:
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = GAME_AREA_SHELTER_B2_ELEVATOR_HALL;
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = 2;
                    break;
                case 0xD:
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = GAME_AREA_SHELTER_B3_ELEVATOR_HALL;
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = 3;
                    break;
                default:
                    Gp_MsgPlayerWeapon(1);
                    gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                    taskKill(task);
                    break;
            }
            task->state++;
            break;
        case 3:
            if (SndVoice_HasActiveId(task->spawnArg1.value) == 0) {
                task->state++;
            }
            break;
        case 4:
            sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
            msg.room      = 1;
            msg.queryOnly = ROOM_EVENT_EXECUTE;
            msg.areaId    = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area;
            msg.warp      = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp;
            msg2          = msg;
            func_map_shelter_80179A04(&msg, &msg2);
            gDisplayState.spriteVariant                                = 1;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = msg2.warp;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = msg2.room;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}
