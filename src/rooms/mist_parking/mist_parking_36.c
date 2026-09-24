#include "common.h"

#include "main/task.h"
#include "rooms/mist_parking.h"

/// Spawns entry 1 of `D_mist_parking_80190824` and keeps its handle in
/// `D_mist_parking_8019532C`.
void func_mist_parking_8018459C(void)
{
    D_mist_parking_8019532C = Task_SpawnFromTable(&D_mist_parking_80190824, 1, 0, 0);
}

/// Hands `phase` (0 or 1) to the task in `D_mist_parking_8019532C` as its
/// `spawnArg1`; any other value kills the task and drops the handle.
void func_mist_parking_801845D0(s32 phase)
{
    Task* t = D_mist_parking_8019532C;

    if (t == NULL) {
        return;
    }
    if (phase >= 2) {
        goto kill;
    }
    if (phase < 0) {
        goto kill;
    }
    t->spawnArg1 = phase;
    return;
kill:
    taskKill(D_mist_parking_8019532C);
    D_mist_parking_8019532C = NULL;
}
