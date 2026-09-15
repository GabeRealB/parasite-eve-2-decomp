#ifndef ROOMS_DRYFIELD_MOTEL_ROOM_1_H
#define ROOMS_DRYFIELD_MOTEL_ROOM_1_H

#include "common.h"

#include "main/task.h"

/// Work block hung off `Task::idMap` (0x1C) of the room task parked in
/// `D_dryfield_motel_room_1_8018159C`, which every entry point in this overlay
/// reaches the room state through.
///
/// `field_2C` is an action index the room's script driver consumes and
/// `field_2E` the sub-state counter reset alongside it. The room's other entry
/// points touch the block up to 0x2E; the fields below that are not identified
/// yet, so the struct carries them as padding.
typedef struct Dmr1Work {
    /* 0x00 */ byte pad_0[0x2C];
    /* 0x2C */ s16  field_2C;
    /* 0x2E */ s16  field_2E;
} Dmr1Work;
STATIC_ASSERT_SIZEOF(Dmr1Work, 0x30);

/// The room's script-driver task, whose `idMap` holds a `Dmr1Work`.
extern Task* D_dryfield_motel_room_1_8018159C;

/// Set the room's action index, resetting the sub-state counter that goes with
/// it - the same body as `func_actor_444000_801327E8`.
void func_dryfield_motel_room_1_8017DFB0(s16 arg0);

#endif // ROOMS_DRYFIELD_MOTEL_ROOM_1_H
