#ifndef ROOMS_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY_H
#define ROOMS_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"
#include "rooms/room_common.h"

/// Set by the walkway's event gate when its last call latched a request and
/// spawned the event task; every call clears it first.
extern u8 D_shelter_b2_north_maintenance_walkway_801863C0;

/// The walkway's event gate. A request whose flag nibble already records the
/// event (a set nibble, or a clear one for a negative `flagId`) answers 1. One
/// whose prerequisite item has not been collected runs the request's CAP
/// command and answers 0. Otherwise the gate answers 2 and - unless the
/// message's `field_5` asks for a dry run - latches the message and the
/// request, writes the flag nibble and spawns the event task.
s32 func_shelter_b2_north_maintenance_walkway_8017D7B4(RoomEventReq* req, RoomEventMsg* msg);

/// Queues a grey gouraud glow spanning the projected points `arg0[0]` and
/// `arg0[1]`: a half-disc at each end, of radius `arg1` scaled by that end's
/// depth and turned by the angle `arg2`, joined by quads. The brightness
/// alternates between 0x20 and 0x28 on successive frames. Nothing is drawn
/// when the second point lies nearer than OTZ 0x11.
void func_shelter_b2_north_maintenance_walkway_8017E0DC(SVECTOR* arg0, s32 arg1, s32 arg2);

/// Queues a red gouraud disc of four quads at the projected point `arg0`, of
/// radius `arg1` scaled by depth, its centre alternating between 0x20 and 0x28
/// on successive frames. Nothing is drawn nearer than OTZ 0x11.
void func_shelter_b2_north_maintenance_walkway_8017E858(SVECTOR* arg0, s16 arg1);

/// Queues a tinted gouraud star at the projected point `arg0`: a disc of
/// radius `arg1` scaled by depth drawn at half and full brightness, plus four
/// spikes, two of them reaching twice the disc's radius. `arg2` packs the tint as four nibbles - a flicker
/// shift, then red, green and blue - and the frame counter's low bit, shifted
/// by the first nibble, is added to every channel. Nothing is drawn when the
/// projection overflows.
void func_shelter_b2_north_maintenance_walkway_8017EBB4(SVECTOR* arg0, s32 arg1, s32 arg2);

#endif // ROOMS_SHELTER_B2_NORTH_MAINTENANCE_WALKWAY_H
