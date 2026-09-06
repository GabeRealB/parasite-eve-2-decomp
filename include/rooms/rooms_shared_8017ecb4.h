#ifndef ROOMS_SHARED_8017ECB4_H
#define ROOMS_SHARED_8017ECB4_H

#include "common.h"

#include "rooms/room_common.h"

/// Hit-tests the action cursor at (`x`, `y`) against the 0xFFFF-terminated
/// hotspot table `table`, raising `hit` on every entry whose rectangle contains
/// the point and clearing it on every other one. Returns non-zero if any entry
/// was hit, so the caller can tell "cursor is over something" from "cursor is
/// over nothing" without rescanning the table. Eight rooms carry this body.
s32 RoomsShared8017ecb4(RoomHotspot* table, s16 x, s16 y);

#endif // ROOMS_SHARED_8017ECB4_H
