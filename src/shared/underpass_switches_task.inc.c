/* Part of the underpass switches library; see underpass_switches.h. */

/// Switch task the room's 0x13F0 handler spawns: plays cap command `spawnArg2`,
/// waits for it to finish, and once its event key reaches 0xA toggles game
/// nibble `spawnArg1`. When that nibble is 0x51 it also picks the room variant
/// to load next from nibbles 0xC9, 0x53 and 0x51 and writes it to both the
/// session and the save data. The last state flags the view dirty when the
/// chosen room is 5 or above, then kills the task.
void underpassSwitchTask(Task* task)
{
    RoomEventMsg  src;
    RoomEventMsg  dst;
    RoomEventMsg* s;
    RoomEventMsg* d;
    GameSession*  session;
    s32           flag;
    s32           state;
    s32           arg;
    u8            room;

    flag  = task->spawnArg1.value;
    state = task->state;
    arg   = task->spawnArg2.value;
    switch (state) {
        case 0:
            Gp_RunCapCmd1(arg);
            task->state = task->state + 1;
            return;
        case 1:
            if (Gp_CapBusy() != 0) {
                return;
            }
            task->state = task->state + 1;
            return;
        case 2:
            if (Gp_GetCapEventKey() >= 0xA) {
                gameFlagSetNibble(flag, gameFlagGetNibble(flag) == 0);
                if (flag == 0x51) {
                    d             = &dst;
                    s             = &src;
                    src.areaId    = 0x26;
                    src.queryOnly = ROOM_EVENT_EXECUTE;
                    if (s->queryOnly == ROOM_EVENT_EXECUTE) {
                        if (gameFlagGetNibble(GAME_FLAG_UNDERPASS_EVENT_SEEN) != 0) {
                            if (gameFlagGetNibble(GAME_FLAG_053) != 0) {
                                d->room = 2;
                            } else {
                                d->room = 1;
                            }
                            if (gameFlagGetNibble(GAME_FLAG_UNDERPASS_SWITCH_1) == 0) {
                                dst.room = dst.room + 2;
                            }
                        } else {
                            if (gameFlagGetNibble(GAME_FLAG_UNDERPASS_SWITCH_1) != 0) {
                                d->room = 5;
                            } else {
                                d->room = 6;
                            }
                        }
                    }
                    session                                                    = gGameSession;
                    room                                                       = dst.room;
                    session->location.loc.room                                 = room;
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = room;
                }
            }
            task->state = task->state + 1;
            return;
        case 3:
            if (gGameSession->location.loc.room >= 5) {
                gGameSession->viewDirty = 1;
            }
            taskKill(task);
            return;
    }
}
