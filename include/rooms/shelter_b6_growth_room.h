#ifndef ROOMS_SHELTER_B6_GROWTH_ROOM_H
#define ROOMS_SHELTER_B6_GROWTH_ROOM_H

#include "common.h"

#include <psyq/libgte.h>

/// One entry of the room's vector lists: three coordinates plus padding, eight
/// bytes apart. Only the three coordinates are ever read or written.
typedef struct ShelterB6GrowthRoomVec {
    s16 x;
    s16 y;
    s16 z;
    s16 pad;
} ShelterB6GrowthRoomVec;

/// A 12-byte record copied whole, never read field by field.
typedef struct ShelterB6GrowthRoomBlob {
    s8 b[12];
} ShelterB6GrowthRoomBlob;

/// A table of pointers into layout data: a four-entry vector list, an
/// eight-entry vector list and four 12-byte records. The room keeps a template
/// and a live copy, and resets the live lists from the template. Nothing pins
/// where the table ends.
typedef struct ShelterB6GrowthRoomLayout {
    s32                      field_0;
    ShelterB6GrowthRoomVec*  field_4;
    ShelterB6GrowthRoomVec*  field_8;
    ShelterB6GrowthRoomBlob* field_C;
} ShelterB6GrowthRoomLayout;

void func_shelter_b6_growth_room_8017E0A8(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_shelter_b6_growth_room_8017E448(s16 arg0, s16 arg1);

#endif
