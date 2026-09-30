#ifndef GAMEPLAY_ROOM_H
#define GAMEPLAY_ROOM_H

#include "common.h"

#include "gameplay/light.h"

struct _GpGridParams;
struct _GpObj3A;
struct _GpObj4C;

/// Entry in a room's table of minimum ambient light levels.
///
/// Entry zero holds `viewCount`; entries 1..viewCount hold each view's colour.
/// RGB levels use 16 units per 8-bit colour level, matching the GTE back colour.
/// Lighting clamps the model colour matrix's ambient term to these minima.
/// The room overlay owns the table, referenced by `GpRoomCoordRec.field_4`;
/// a missing table or view beyond its count uses `Gp_RoomBoundDefault`.
typedef union {
    struct {
        s16 r;     // Minimum red ambient level.
        s16 g;     // Minimum green ambient level.
        s16 b;     // Minimum blue ambient level.
        s16 luma;  // Stored weighted brightness (3*r + 4*g + b)/8; unused by lighting.
    } color;
    s16 viewCount; // Header only: number of view entries following entry zero.
} WorldCoordRoomAmbientEntry;
STATIC_ASSERT_SIZEOF(WorldCoordRoomAmbientEntry, 8);

/// A room view's own lights, returned by `Gp_GetRoomCoordSet`
/// (`GpRoomCoordRec.field_0`): its directional, point and spot lights.
/// `Gp_UpdateRoomCoords` parents each light to `gGfxViewCoord` on first run,
/// builds each spot light's orientation from its `dir`, then updates them every
/// frame via `Gp_UpdateCoordEx`.
typedef struct _GpRoomCoordSet {
    /* 0x00 */ s32           n58;
    /* 0x04 */ GpLight*      arr58; // directional lights
    /* 0x08 */ s32           n60;
    /* 0x0C */ GpPointLight* arr60; // point lights
    /* 0x10 */ s32           n6C;
    /* 0x14 */ GpSpotLight*  arr6C; // spot lights
} GpRoomCoordSet;
STATIC_ASSERT_SIZEOF(GpRoomCoordSet, 0x18);

/// 8-byte record in tables pointed to by `Gp_RoomCoordTables`. Indexed 1-based
/// by `GameLocationKey.room`. `Gp_GetRoomCoordRec` returns the record (or NULL).
/// `Gp_GetRoomCoordSet` returns `field_0`, the room view's lights (or NULL).
/// `Gp_GetRoomBound` walks `field_4` as a `WorldCoordRoomAmbientEntry` table, falling
/// back to `Gp_RoomBoundDefault`.
typedef struct _GpRoomCoordRec {
    /* 0x0 */ GpRoomCoordSet*             field_0;
    /* 0x4 */ WorldCoordRoomAmbientEntry* field_4;
} GpRoomCoordRec;
STATIC_ASSERT_SIZEOF(GpRoomCoordRec, 8);

/// Record in the 8-entry arrays pointed to by `Gp_RoomParamTables`.
/// `Gp_LoadRoomParams` copies `field_3` into `Gp_RoomParams[]`. Nearby helpers
/// also load `field_1` (`func_800DDDF8`, `func_800DE7CC`) and `field_2`
/// (`Gp_PickNearestRec18` keeps a slot only when this is nonzero). `field_4`
/// points to three base sound ids used by `func_80105ED4`, or is NULL.
typedef struct _GpRoomParamRec {
    /* 0x0 */ u8   field_0;
    /* 0x1 */ u8   field_1;
    /* 0x2 */ u8   field_2;
    /* 0x3 */ u8   field_3;
    /* 0x4 */ s32* field_4;
} GpRoomParamRec;
STATIC_ASSERT_SIZEOF(GpRoomParamRec, 8);

/// 0x10-byte per-room record in tables pointed to by `Gp_RoomObjTables`.
/// Indexed 1-based by `GameSession.location.loc.room` / `GameLocationKey.room`.
/// `Gp_LinkRoomObjects` / `Gp_LinkRoomObjectsSpawn` parent `field_0` to `&gGfxViewCoord` and
/// link the `field_4` / `field_8` (`GpObj4A`) and `field_C` (`GpObj3A`) arrays.
typedef struct _GpRoomObjRec {
    /* 0x0 */ struct _GpGridParams* field_0;
    /* 0x4 */ struct _GpObj4C*      field_4;
    /* 0x8 */ struct _GpObj4C*      field_8;
    /* 0xC */ struct _GpObj3A*      field_C;
} GpRoomObjRec;
STATIC_ASSERT_SIZEOF(GpRoomObjRec, 0x10);

/// Per-stage wrapper. `field_0` is an array of `GpRoomObjRec*`, indexed
/// 1-based by `GameSession.location.loc.area` / `GameLocationKey.area`.
typedef struct _GpRoomObjTbl {
    /* 0x0 */ GpRoomObjRec** field_0;
} GpRoomObjTbl;

#endif // GAMEPLAY_ROOM_H
