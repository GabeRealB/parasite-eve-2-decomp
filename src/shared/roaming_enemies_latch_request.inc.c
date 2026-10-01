/* Part of the roaming enemies library; see roaming_enemies.h. */

/// 0x13EF handler of both pools. When the request's action ID changes
/// and the cooldown is idle, it becomes the pending spawn request; otherwise
/// the request is cleared. The byte is remembered either way. Both pools carry
/// a copy, so a package includes the fragment twice, the second time under a
/// #define of its own name (as hopping_enemy does with its creep state).
s32 roamerLatchRequest(Task* arg0, s32 arg1, TaskMessageArg firstArg, TaskMessageArg arg3)
{
    const DirectionActionRequest* request = firstArg.pointer;
    s16                           counter;

    if (request->actionId != gRoamerLastRequest) {
        counter = gRoamerCooldown;
        if (counter == 0) {
            gRoamerSpawnRequest = request->actionId;
        } else {
            goto L_clear;
        }
    } else {
    L_clear:
        gRoamerSpawnRequest = 0;
    }
    gRoamerLastRequest = request->actionId;
    return 1;
}
