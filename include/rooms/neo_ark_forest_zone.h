#ifndef ROOMS_NEO_ARK_FOREST_ZONE_H
#define ROOMS_NEO_ARK_FOREST_ZONE_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/task.h"

/// Four-byte payload the room hands `Gp_DispatchMsg` with message 0x7DB: two
/// id bytes followed by a halfword parameter.
typedef struct NeoArkForestZoneMsg7DB {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ u16 field_2;
} NeoArkForestZoneMsg7DB;
STATIC_ASSERT_SIZEOF(NeoArkForestZoneMsg7DB, 0x4);

/// The object a slot-4 task holds in `Task::spawnArg2`. Only the pending
/// spawn value at 0x40 (-999 while waiting for one) and the byte at 0x4C,
/// cleared when a value is handed over, are known; the size is not.
///
/// The halfword at 0x40 is compared through an `(s16)` cast of the `u16`
/// member: the reads that compare it are `lh`, the store is `sh`.
typedef struct NeoArkForestZoneObj {
    /* 0x00 */ byte pad_0[0x40];
    /* 0x40 */ u16  field_40;
    /* 0x42 */ byte pad_42[0xA];
    /* 0x4C */ u8   field_4C;
    /* 0x4D */ byte pad_4D[3];
} NeoArkForestZoneObj;

/// A placement for a spawned task: the x and z written into its coordinate
/// translation (y is always zero) and the Y rotation passed to
/// `Gfx_RotMatrixY`. The halfword after `x` is not read.
typedef struct NeoArkForestZoneSpawnPos {
    s16 x;
    s16 pad_2;
    s16 z;
    s16 rotY;
} NeoArkForestZoneSpawnPos;

/// Frame countdown: counted down each frame by `func_...80180D24`, bumped by
/// 0x5A when a spawn is handed out, and tested for zero before a new
/// placement request is accepted.
extern s16 D_neo_ark_forest_zone_80182D62;

/// Placement request, one-based (zero means none). Cleared every frame by
/// `func_...80180D24` after it has been acted on.
extern s16 D_neo_ark_forest_zone_80182D66;

/// Per-session-slot gate, indexed by `gGameSession->at4.loc.place`; zero
/// disables `func_...80180D24` in that slot.
extern u8 D_neo_ark_forest_zone_80182D44[];

/// How many of the pending spawn slots `func_...80180D24` scans.
extern s16 D_neo_ark_forest_zone_80182D64;

/// `Gp_StateF0.field_6` as seen on the previous frame.
extern s16 D_neo_ark_forest_zone_80182DC4;

/// The room's five pending spawn values; a positive entry is handed to the
/// first waiting object and then cleared.
extern u16 D_neo_ark_forest_zone_80182E54[5];

/// Debug format printed with the flag-0x10A count minus the pending slots.
extern char D_neo_ark_forest_zone_8017D5F8[];

extern NeoArkForestZoneMsg7DB D_neo_ark_forest_zone_80182E44;

/// Spawn placements, indexed by the placement request minus one.
extern NeoArkForestZoneSpawnPos D_neo_ark_forest_zone_80182DE8[5];

void func_neo_ark_forest_zone_8017E074(GsCOORDINATE2* arg0, s32 arg1, s16 arg2);
void func_neo_ark_forest_zone_8017E6C4(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb);
void func_neo_ark_forest_zone_8017EAF0(GsCOORDINATE2* arg0, s32 arg1, u8* rgb);
void func_neo_ark_forest_zone_8017F9F4(GsCOORDINATE2* arg0, s16 arg1, u8* arg2);

#endif
