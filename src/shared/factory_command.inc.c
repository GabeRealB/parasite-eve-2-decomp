/* Part of the factory lift library; see factory_lift.h. */

/// Command handler for the night factory room, reached from the room's command
/// table (`gFactoryMsgTable`, id 0x13F0) with the command in
/// `$a2`.
///
/// Cases 1/2/3/5/12 spawn an actor out of whichever spawn table the session
/// selected (`D_..._A7E4`, written by `factoryRoomInit`)
/// at index 2/3/1/0/6, handing the command on as `Task_SpawnFromTable`'s third
/// argument. Case 6 silences both characters' weapons and spawns the factory's
/// own table `D_..._80186E4C` at index 0 instead -- that table's task is the
/// `factoryPanelSpawn` poller. Case 12 only acts while
/// progress flag 0x49 is 1, and silences the player's and the ally's weapon
/// before spawning. Every other command does nothing.
///
/// The `goto`s are the target's shape: every path shares the single `return 0`
/// at `end`, so the exit block is the only place `$v0` is zeroed.
s32 factoryCommand(Task* arg0, s32 arg1, s32 cmd, s32 arg3)
{
    TaskDesc* table;
    s32       idx;

    switch (cmd) {
        case 1:
            table = gFactorySpawnTable;
            idx   = 2;
            break;
        case 2:
            table = gFactorySpawnTable;
            idx   = 3;
            break;
        case 3:
            table = gFactorySpawnTable;
            idx   = 1;
            break;
        case 5:
            table = gFactorySpawnTable;
            idx   = 0;
            break;
        case 6:
            Gp_MsgPlayerWeapon(0);
            Gp_MsgPlayer3F3(0);
            Gp_MsgAllyWeapon(0);
            Gp_MsgAlly3F3(0);
            Task_SpawnFromTable(gFactoryPanelSessionDesc, 0, 0, 0);
            goto end;
        case 12:
            if (gameFlagGetNibble(GAME_FLAG_FACTORY_LIFT_POSITION) == 1) {
                Gp_MsgPlayerWeapon(0);
                Gp_MsgAllyWeapon(0);
                table = gFactorySpawnTable;
                idx   = 6;
                break;
            }
            goto end;
        default:
            goto end;
    }
    Task_SpawnFromTable(table, idx, cmd, 0);
end:
    return 0;
}
