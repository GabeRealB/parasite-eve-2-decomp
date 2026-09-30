/* Part of the roaming enemies library; see roaming_enemies.h. */

/// 0x13EF handler of both pools. When the message's spawn-point byte changes
/// and the cooldown is idle, it becomes the pending spawn request; otherwise
/// the request is cleared. The byte is remembered either way. Both pools carry
/// a copy, so a package includes the fragment twice, the second time under a
/// #define of its own name (as hopping_enemy does with its creep state).
s32 roamerLatchRequest(Task* arg0, s32 arg1, u8* arg2, TaskMessageArg arg3)
{
    s16 counter;

    if (arg2[2] != gRoamerLastRequest) {
        counter = gRoamerCooldown;
        if (counter == 0) {
            gRoamerSpawnRequest = arg2[2];
        } else {
            goto L_clear;
        }
    } else {
    L_clear:
        gRoamerSpawnRequest = 0;
    }
    gRoamerLastRequest = arg2[2];
    return 1;
}
