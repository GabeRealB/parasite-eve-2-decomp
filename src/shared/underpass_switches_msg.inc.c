/* Part of the underpass switches library; see underpass_switches.h. */

/// Handler for message 0x13F0: for `arg2` 1 or 2, spawns the room's switch
/// task `underpassSwitchTask` from the task table, toggling
/// nibble 0x51 with cap command 1 or nibble 0x52 with cap command 2. Any other
/// value spawns nothing. Always returns 0.
s32 underpassSwitchMsg(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 1:
            taskSpawnFromTable(gUnderpassSwitchTaskDesc, 0, 0x51, 1);
            break;
        case 2:
            taskSpawnFromTable(gUnderpassSwitchTaskDesc, 0, 0x52, 2);
            break;
    }
    return 0;
}
