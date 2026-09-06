#ifndef ROOMS_DRYFIELD_MOTEL_BALCONY_H
#define ROOMS_DRYFIELD_MOTEL_BALCONY_H

#include "common.h"

#include "rooms/room_common.h"

#include <psyq/libgte.h>

/// Same 0x18-byte scratch block as `RoomDraw04Scratch`. This overlay holds two
/// copies of that body (`RoomsShared8017e4f8Halo` and
/// `func_dryfield_motel_balcony_801809AC`), so they cannot share one object.
typedef RoomDraw04Scratch RoomFanScratch;

#endif
