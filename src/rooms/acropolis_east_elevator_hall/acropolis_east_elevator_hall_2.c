#include "common.h"

#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"

#include "rooms/room_common.h"

extern TaskDesc D_acropolis_east_elevator_hall_8017FC90;

/// Part index in the mirrored player model each held-object reflection hangs
/// off, indexed by `Task::spawnArg1` (the held-object slot).
extern u8 RoomsShared8017f128Parts[];

INCLUDE_ASM("rooms/nonmatchings/acropolis_east_elevator_hall/acropolis_east_elevator_hall_2", func_acropolis_east_elevator_hall_8017F2F8);
