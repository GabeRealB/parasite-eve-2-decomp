#include "main/random.h"

/* Part of the incinerator blaze library; see incinerator_blaze.h. */

/// Spawn task of the overlay's spawn table (`_blazeFadeTask`'s
/// neighbour entry, started with the encounter): each tick rolls the LCG and
/// aims the overlay's effect record at one part of the player's model, taken
/// from the coordinate array `gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)`'s display object owns.
///
/// State 0 fires unconditionally -- the wide pick, scale 0x100 -- and steps to
/// state 1. State 1 fires only on a frame the `gDisplayState.animFrame` gate lets through,
/// and which pick that is depends on the task's `spawnArg1`: the zero arm
/// takes the same four parts as state 0 at scale 0x10, the non-zero arm the
/// whole table at scale 0x100.
///
/// The three arms each spell the aim-and-fire sequence out. That is what the
/// target's shape is: the two state-1 arms are byte-for-byte equal from the
/// table-base `lui` on, so `jump.c`'s cross-jumping (the `jump_optimize` that
/// runs after reload) merges that suffix into one block and leaves each arm
/// its own copy of the address and scale in front of the jump -- the address
/// and scale cannot merge because the scale differs. Folding the arms into one
/// `goto`-shared block instead compiles them into a single copy with a live
/// scale value, which is a different object (95.02%).
void blazeBodyFireTask(Task* arg0)
{
    Task* slot;
    s32   idx;

    slot            = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    idx             = gRandomLcgState >> 16;

    switch (arg0->state) {
        case 0:
            idx                       &= 3;
            gBlazeFireSpawn.spawnArgLo = 0x100;
            gBlazeFireSpawn.coord      = &slot->extra.tmd->coords[gBlazePlayerParts[idx]];
            func_800FDB18(3, slot->extra.tmd->coords, NULL, &gBlazeFireSpawn);
            arg0->state++;
            return;
        case 1:
            if (arg0->spawnArg1.value == 0) {
                if (gDisplayState.animFrame & 0xF) {
                    return;
                }
                idx                       &= 3;
                gBlazeFireSpawn.spawnArgLo = 0x10;
                gBlazeFireSpawn.coord      = &slot->extra.tmd->coords[gBlazePlayerParts[idx]];
                func_800FDB18(3, slot->extra.tmd->coords, NULL, &gBlazeFireSpawn);
                return;
            }
            if (gDisplayState.animFrame & 7) {
                return;
            }
            idx                       &= 0xF;
            gBlazeFireSpawn.spawnArgLo = 0x100;
            gBlazeFireSpawn.coord      = &slot->extra.tmd->coords[gBlazePlayerParts[idx]];
            func_800FDB18(3, slot->extra.tmd->coords, NULL, &gBlazeFireSpawn);
            return;
    }
}
