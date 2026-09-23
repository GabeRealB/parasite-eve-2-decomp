#ifndef ROOMS_ACROPOLIS_FIRE_ESCAPE_H
#define ROOMS_ACROPOLIS_FIRE_ESCAPE_H

#include "common.h"

#include "rooms/room_common.h"

/// Spawn-parameter block handed to the `D_acropolis_fire_escape_80181D18`
/// cutscene task as `Task::spawnArg2`. `func_acropolis_fire_escape_8017F9F8`
/// fills it before `Task_SpawnFromTable`: `field_0` is a destination room id,
/// `field_1`..`field_3` are signed selectors and `field_4`..`field_10` are
/// sound-event ids. Same shape as the other rooms' cutscene blocks.
typedef struct AcropolisFireEscapeEvt {
    /* 0x00 */ s8  field_0;
    /* 0x01 */ s8  field_1;
    /* 0x02 */ s8  field_2;
    /* 0x03 */ s8  field_3;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
    /* 0x10 */ s32 field_10;
    /* 0x14 */ s16 field_14;
    /* 0x16 */ s16 field_16;
} AcropolisFireEscapeEvt;
STATIC_ASSERT_SIZEOF(AcropolisFireEscapeEvt, 0x18);

extern AcropolisFireEscapeEvt D_acropolis_fire_escape_80183048;

/// Per-frame scratch the room's flickering glow task reserves off
/// `G_SCRATCH_HEAD`. `vec` is the task coordinate's world translation,
/// projected through `GsWSMATRIX` into `sx` / `sy` with `otz` as its depth
/// (biased toward the camera before use). `radius` is the glow's outer screen
/// radius, scaled by the inverse of that depth; `radius2` is computed the same
/// way with a smaller factor but nothing reads it. Other rooms' billboard tasks
/// reserve a block of the same layout; whether it is one type is unsettled.
typedef struct AcropolisFireEscapeGlowScratch {
    s32     otz;
    s32     radius;
    s32     radius2;
    SVECTOR vec;
    s16     sx;
    s16     sy;
} AcropolisFireEscapeGlowScratch;
STATIC_ASSERT_SIZEOF(AcropolisFireEscapeGlowScratch, 0x18);

/// Work block the "Play Data" weapon / PE list tasks allocate at
/// `Task::work` (`memCalloc(0xC4)`). `ids` holds the item / PE ids the list
/// shows, sorted by use count; `bars` is the per-row bar length and `vals` the
/// scaled use count. `func_acropolis_fire_escape_8017E298` fills it for the
/// weapon list and `func_acropolis_fire_escape_8017E594` for the PE list.
typedef struct AcropolisFireEscapeStatWork {
    /* 0x00 */ s16 ids[0x20];
    /* 0x40 */ s16 bars[0x20];
    /* 0x80 */ s16 vals[0x20];
    /* 0xC0 */ s32 field_C0;
} AcropolisFireEscapeStatWork;
STATIC_ASSERT_SIZEOF(AcropolisFireEscapeStatWork, 0xC4);

#endif // ROOMS_ACROPOLIS_FIRE_ESCAPE_H
