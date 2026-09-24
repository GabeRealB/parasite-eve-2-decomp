#include "common.h"

#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"

#include "rooms/room_common.h"
#include "rooms/acropolis_east_elevator_hall.h"

INCLUDE_ASM("rooms/nonmatchings/acropolis_east_elevator_hall/acropolis_east_elevator_hall", func_acropolis_east_elevator_hall_8017D7A4);
/// Scale handed to `ScaleMatrix` to flip the reflection across X.
const VECTOR D_acropolis_east_elevator_hall_8017D5C4 = { -0x1000, 0x1000, 0x1000, 0 };

INCLUDE_RODATA("rooms/nonmatchings/acropolis_east_elevator_hall/acropolis_east_elevator_hall", D_acropolis_east_elevator_hall_8017D5D4);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_east_elevator_hall/acropolis_east_elevator_hall", D_acropolis_east_elevator_hall_8017D5E0);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_east_elevator_hall/acropolis_east_elevator_hall", D_acropolis_east_elevator_hall_8017D5E8);
