#ifndef ROOMS_NEO_ARK_SUBSTATION_H
#define ROOMS_NEO_ARK_SUBSTATION_H

#include "common.h"

/// One entry of the room's nine-entry ambience table
/// (`D_neo_ark_substation_8017E2C8`), indexed by `gGameSession->at4.loc.view`. Its two
/// halfwords are the pan and volume bytes `SndEvt_EnqueueType6` /
/// `SndEvt_EnqueueTypeA` take as their second and third arguments, so an entry is
/// the placement and level of the loop the room plays while that area is current.
/// Entries 0 and 1 are all zero - the areas that have no loop of their own.
typedef struct RoomAmbienceEntry {
    /* 0x0 */ s16 pan;
    /* 0x2 */ s16 pad_2;
    /* 0x4 */ s16 vol;
    /* 0x6 */ s16 pad_6;
} RoomAmbienceEntry;
STATIC_ASSERT_SIZEOF(RoomAmbienceEntry, 0x8);

#endif // ROOMS_NEO_ARK_SUBSTATION_H
