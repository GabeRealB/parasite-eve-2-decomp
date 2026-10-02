#ifndef SRC_ACTORS_ACTOR_143000_ACTOR_143000_PRIVATE_H
#define SRC_ACTORS_ACTOR_143000_ACTOR_143000_PRIVATE_H

#include "common.h"

#include "actor_143000_capture_private.h"

#include "gameplay/animation.h"

#include "main/task_types.h"

/// Spawn argument the actor hands to the task it starts once the code is
/// entered (`D_actor_143000_80135C08`); that task sets `field_1` when it starts.
typedef struct Actor143000Spawn {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ s16 field_2;
} Actor143000Spawn;
STATIC_ASSERT_SIZEOF(Actor143000Spawn, 4);

extern Actor143000Spawn D_actor_143000_80135C08;

extern AnimationSet gActor143000Animation02A20;

extern AnimationSet gActor143000Animation02CCC;

extern AnimationSet gActor143000Animation02EE8;

extern AnimationSet gActor143000Animation03090;

extern AnimationSet gActor143000Animation03248;

extern TaskDesc D_actor_143000_801350B0[2];

extern s32 D_actor_143000_80135C00;

extern s32 D_actor_143000_80135C04;

extern char D_actor_143000_80135C20[24];

extern Actor143000CaptureArgs D_actor_143000_80135090;

extern Actor143000CaptureArgs D_actor_143000_801350A0;

// Callbacks referenced by the overlay's shared data tables.
void func_actor_143000_80133CF0(Task*);

#endif // SRC_ACTORS_ACTOR_143000_ACTOR_143000_PRIVATE_H
