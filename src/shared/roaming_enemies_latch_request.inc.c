/* Part of the roaming enemies library; see roaming_enemies.h. */

/// 0x13EF handler of both pools. When the request's action ID changes
/// and the cooldown is idle, it becomes the pending spawn request; otherwise
/// the request is cleared. The byte is remembered either way. Both pools carry
/// a copy, so a package includes the fragment twice, the second time under a
/// #define of its own name (as the Mad Chaser library does with its creep state).
s32 roamerLatchRequest(Task* arg0, s32 arg1, const void* firstArg, s32 arg3)
{
    const DirectionActionRequest* request = firstArg;

    if (request->actionId != gRoamerLastRequest && gRoamerCooldown == 0) {
        gRoamerSpawnRequest = request->actionId;
    } else {
        gRoamerSpawnRequest = 0;
    }
    gRoamerLastRequest = request->actionId;
    return 1;
}
