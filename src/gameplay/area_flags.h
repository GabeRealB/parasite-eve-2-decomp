#ifndef GAMEPLAY_PRIVATE_AREA_FLAGS_H
#define GAMEPLAY_PRIVATE_AREA_FLAGS_H

#include "common.h"

#include "gameplay/area_flags.h"

/// One stage's placed objects: where they stand and what state each is in.
///
/// The stage table holds one of these per `GAME_STAGE_*` value. `rooms` is the
/// stage map's `AreaObjectRoom` table. Spawning reads the row at
/// `GameLocationKey.area`; flag seeding and a search for one flag index walk
/// it from the first row. `objectStates` addresses the object-state words of
/// the stage's saved bank (`GameFlagStageHeader.objectStates`), where an
/// `AreaObjectPlace.flagIndex` selects one two-bit state. Two stages can
/// share one set of words. Both members are NULL for `GAME_STAGE_NONE`;
/// callers test `rooms` for NULL but read `objectStates` unchecked.
typedef struct {
    AreaObjectRoom* rooms;        // Stage's room table, indexed by area id and ended by AREA_OBJECT_ROOM_END. NULL when the stage has none
    u32*            objectStates; // Saved two-bit object states, sixteen per word, low pair first. Word is flagIndex >> 4, pair is flagIndex & 0xF
} AreaObjectStage;
STATIC_ASSERT_SIZEOF(AreaObjectStage, 0x8);

#endif // GAMEPLAY_PRIVATE_AREA_FLAGS_H
