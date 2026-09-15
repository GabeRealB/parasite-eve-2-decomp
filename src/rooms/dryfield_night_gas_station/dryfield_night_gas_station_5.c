#include "common.h"

#include <psyq/libgte.h>
#include <psyq/rand.h>

#include "main/task.h"

extern TaskDesc RoomsShared8017e320Desc;
extern Task*    RoomsShared8017e320Task;
extern Task*    D_dryfield_night_gas_station_801907AC;
extern SVECTOR  D_dryfield_night_gas_station_80188580[];

extern void func_dryfield_night_gas_station_8017FD80(s16);
extern void func_dryfield_night_gas_station_801802EC(s16);
extern void func_dryfield_night_gas_station_80180DC8(s16);

/// Retires the room's third tracked task and drops the room's reference to it.
/// The `-1` state is the task's own exit request, so the task frees itself on
/// its next tick.
void func_dryfield_night_gas_station_80180974(void)
{
    if (RoomsShared8017e320Task != NULL) {
        RoomsShared8017e320Task->state = -1;
        RoomsShared8017e320Task        = NULL;
    }
}

/// Runs the room's countdown timer task: for its first 100 ticks it pulses
/// `func_dryfield_night_gas_station_8017FD80` and counts up, then kills itself.
void func_dryfield_night_gas_station_80180998(Task* arg0)
{
    s16 temp_a0;

    if (arg0->state == 0) {
        temp_a0 = arg0->killCountdown;
        if (temp_a0 < 0x64) {
            func_dryfield_night_gas_station_8017FD80(temp_a0);
            arg0->killCountdown = (u16)arg0->killCountdown + 1;
            return;
        }
    }
    Task_Kill(arg0);
}

/// Spawns the room's second tracked task (slot 2 of the shared table) and
/// stores it beside `RoomsShared8017e320Task`.
void func_dryfield_night_gas_station_80180A00(void)
{
    D_dryfield_night_gas_station_801907AC = Task_SpawnFromTable(&RoomsShared8017e320Desc, 2, 0, 0);
}

/// Advances the room's second tracked task by one state and drops the room's
/// reference to it: the task carries on with its own schedule, untracked.
void func_dryfield_night_gas_station_80180A34(void)
{
    if (D_dryfield_night_gas_station_801907AC != NULL) {
        D_dryfield_night_gas_station_801907AC->state++;
        D_dryfield_night_gas_station_801907AC = NULL;
    }
}

/// Steps the room's blinking-light table: each tick it re-derives whether the
/// current entry's `vx` is odd, and when that flag flips it republishes it to
/// `func_dryfield_night_gas_station_80180DC8` (which switches the lamp effect
/// between its on and off appearance, or back to dark for the -1 state). The
/// index runs to 100 and then wraps.
void func_dryfield_night_gas_station_80180A60(Task* arg0)
{
    s16 temp_v0_2;
    s32 temp_v0;

    if (arg0->state == 0) {
        temp_v0 = (D_dryfield_night_gas_station_80188580[arg0->killCountdown].vx & 1) ^ 1;
        if (arg0->spawnArg1 != temp_v0) {
            arg0->spawnArg1 = temp_v0;
            func_dryfield_night_gas_station_80180DC8((s16)arg0->spawnArg1);
        }
        temp_v0_2           = (u16)arg0->killCountdown + 1;
        arg0->killCountdown = temp_v0_2;
        if (temp_v0_2 >= 0x64) {
            arg0->killCountdown = 0;
        }
    } else {
        func_dryfield_night_gas_station_80180DC8(0);
        Task_Kill(arg0);
    }
}

/// Spawns the room's third tracked task (slot 3 of the shared table) and stores
/// it in `RoomsShared8017e320Task`, the slot the room's teardown clears.
void func_dryfield_night_gas_station_80180B04(void)
{
    RoomsShared8017e320Task = Task_SpawnFromTable(&RoomsShared8017e320Desc, 3, 0, 0);
}

/// Second teardown entry point for `RoomsShared8017e320Task`: requests the
/// task's exit and drops the room's reference to it, exactly as
/// `func_dryfield_night_gas_station_80180974` does.
void func_dryfield_night_gas_station_80180B38(void)
{
    if (RoomsShared8017e320Task != NULL) {
        RoomsShared8017e320Task->state = -1;
        RoomsShared8017e320Task        = NULL;
    }
}

/// Runs the room's countdown task: seeds the RNG on its first tick, then for
/// 100 ticks pulses `func_dryfield_night_gas_station_801802EC` and counts up,
/// then kills itself.
void func_dryfield_night_gas_station_80180B5C(Task* arg0)
{
    s16 temp_a0;

    switch (arg0->state) {
        case 0:
            srand(1);
            arg0->state += 1;
            /* fallthrough */
        case 1:
            temp_a0 = arg0->killCountdown;
            if (temp_a0 < 0x64) {
                func_dryfield_night_gas_station_801802EC(temp_a0);
                arg0->killCountdown = (u16)arg0->killCountdown + 1;
                return;
            }
        default:
            Task_Kill(arg0);
            return;
    }
}
