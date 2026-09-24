#ifndef ROOMS_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_H
#define ROOMS_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"
#include "rooms/room_common.h"

/// The walkway's event gate. A request whose flag nibble already records the
/// event (a set nibble, or a clear one for a negative `flagId`) answers 1. One
/// whose prerequisite item has not been collected runs the request's CAP
/// command and answers 0. Otherwise the gate answers 2 and - unless the
/// message's `field_5` asks for a dry run - latches the message and the
/// request, writes the flag nibble and spawns the event task.
s32 func_shelter_b2_south_maintenance_walkway_8017D610(RoomEventReq* req, RoomEventMsg* msg);

/// Queues a grey gouraud glow spanning the projected points `arg0[0]` and
/// `arg0[1]`: a half-disc at each end, of radius `arg1` scaled by that end's
/// depth and turned by the angle `arg2`, joined by quads. The brightness
/// alternates between 0x20 and 0x28 on successive frames. Nothing is drawn
/// when the second point lies nearer than OTZ 0x11.
void func_shelter_b2_south_maintenance_walkway_8017DEC4(SVECTOR* arg0, s32 arg1, s32 arg2);

/// Queues a red gouraud disc of four quads at the projected point `arg0`, of
/// radius `arg1` scaled by depth, its centre alternating between 0x20 and 0x28
/// on successive frames. Nothing is drawn nearer than OTZ 0x11.
void func_shelter_b2_south_maintenance_walkway_8017E640(SVECTOR* arg0, s16 arg1);

#endif // ROOMS_SHELTER_B2_SOUTH_MAINTENANCE_WALKWAY_H
