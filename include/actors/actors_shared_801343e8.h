#ifndef ACTORS_SHARED_801343E8_H
#define ACTORS_SHARED_801343E8_H

#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

/// Work block reached through `Task::idMap`: the flag word `field_1C2`, the
/// target position `field_23C`, the mode `field_24E`, the phase `field_250`
/// and its countdown `field_256`.
typedef struct ActorShared801343e8Work {
    /* 0x000 */ byte    pad_0[0x1C2];
    /* 0x1C2 */ u16     field_1C2;
    /* 0x1C4 */ byte    pad_1C4[0x78];
    /* 0x23C */ SVECTOR field_23C;
    /* 0x244 */ byte    pad_244[0xA];
    /* 0x24E */ s16     field_24E;
    /* 0x250 */ s16     field_250;
    /* 0x252 */ byte    pad_252[0x4];
    /* 0x256 */ s16     field_256;
} ActorShared801343e8Work;

typedef struct ActorShared801343e8Kind {
    /* 0x0 */ byte pad_0[0x2];
    /* 0x2 */ u16  field_2;
} ActorShared801343e8Kind;

/// Spawn argument reached through `Task::spawnArg2`.
typedef struct ActorShared801343e8Spawn {
    /* 0x00 */ byte                     pad_0[0x14];
    /* 0x14 */ u8                       field_14;
    /* 0x15 */ byte                     pad_15[0x27];
    /* 0x3C */ ActorShared801343e8Kind* field_3C;
} ActorShared801343e8Spawn;

void ActorsShared801343e8(Task* task);

#endif
