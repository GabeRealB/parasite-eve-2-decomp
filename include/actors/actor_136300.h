#ifndef ACTOR_136300_H
#define ACTOR_136300_H

#include "common.h"

#include "main/task.h"

/// Spawn-parameter block handed to the `D_actor_136300_80132AC4` entry as
/// `Task_SpawnFromTable`'s fourth argument, so the child task picks it up as
/// `Task::spawnArg2`. The overlay seeds both halves before spawning: `field_0`
/// is the value the child acts on (0x64 or 5, selected by the message this
/// actor received) and `field_2` is 0x100 in every call site.
typedef struct Actor136300Spawn {
    /* 0x0 */ s16 field_0;
    /* 0x2 */ s16 field_2;
} Actor136300Spawn;
STATIC_ASSERT_SIZEOF(Actor136300Spawn, 0x4);

extern Actor136300Spawn D_actor_136300_8013C99C;

/// Latch for a positive message argument, written by
/// `func_actor_136300_80132910` and referenced nowhere else in the overlay.
extern s16 D_actor_136300_8013C9A0;

extern TaskDesc D_actor_136300_80132AC4;

#endif // ACTOR_136300_H
