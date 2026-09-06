#include "common.h"

#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"

#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017d5f0.h"

/// Part index in the mirrored player model each held-object reflection hangs
/// off, indexed by `Task::spawnArg1` (the held-object slot).
extern u8 RoomsShared8017f128Parts[];

INCLUDE_ASM("rooms/nonmatchings/acropolis_east_elevator_hall/acropolis_east_elevator_hall", RoomsShared8017d7a4);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_east_elevator_hall/acropolis_east_elevator_hall", D_acropolis_east_elevator_hall_8017D5C0);

/// Scale handed to `ScaleMatrix` to flip the reflection across X.
const VECTOR RoomsShared8017f128Scale = { -0x1000, 0x1000, 0x1000, 0 };

INCLUDE_RODATA("rooms/nonmatchings/acropolis_east_elevator_hall/acropolis_east_elevator_hall", RoomsShared8017d878Table);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_east_elevator_hall/acropolis_east_elevator_hall", D_acropolis_east_elevator_hall_8017D5E0);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_east_elevator_hall/acropolis_east_elevator_hall", D_acropolis_east_elevator_hall_8017D5E8);
