#ifndef SRC_ROOMS_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_PRIVATE_H
#define SRC_ROOMS_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_PRIVATE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/message.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

extern RoomFadeStorage D_shelter_b1_north_maintenance_walkway_80185B6C;

extern RoomEventMsg D_shelter_b1_north_maintenance_walkway_80185B74;

extern RoomLatchedEvent D_shelter_b1_north_maintenance_walkway_80185B80;

/// Queues a grey gouraud glow spanning the projected points `arg0[0]` and
/// `arg0[1]`, of radius `arg1` turned by the angle `arg2`.
void func_shelter_b1_north_maintenance_walkway_8017DDE0(SVECTOR* arg0, s32 arg1, s32 arg2);

/// Queues a red gouraud disc at the projected point `arg0`, of radius `arg1`.
void func_shelter_b1_north_maintenance_walkway_8017E55C(SVECTOR* arg0, s16 arg1);

#endif // SRC_ROOMS_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_SHELTER_B1_NORTH_MAINTENANCE_WALKWAY_PRIVATE_H
