#ifndef GAMEPLAY_MAP_H
#define GAMEPLAY_MAP_H

#include "common.h"

#include "main/tmd_types.h"

/// `MenuMapArea.page` of an area index the stage's map does not show.
#define MENU_MAP_AREA_PAGE_NONE 0xF000

/// `MenuMapArea.page` of the record closing a stage's table.
#define MENU_MAP_AREA_PAGE_END 0xFFFF

/// Where one area sits on the map screen.
///
/// Each stage has a table of these, indexed by `GameSession.location.loc.area`.
/// A record names the map page the area is drawn on and ties a point in the
/// area's own coordinates to a point on that page, which is what places the
/// player cursor: one map pixel covers `scaleX` units of X and `scaleZ` units
/// of Z, and the page's Y runs against Z.
typedef struct {
    s16 originX; // Area-space X of the reference point
    s16 originZ; // Area-space Z of the reference point
    u16 mapX;    // Map-screen X of the reference point; wraps as 16-bit, so 0xFFFD is -3
    u16 mapY;    // Map-screen Y of the reference point; wraps the same way
    s16 scaleX;  // Area units per map pixel along X
    s16 scaleZ;  // Area units per map pixel along Z
    u16 page;    // Map page showing the area, counted from 1, or a `MENU_MAP_AREA_PAGE_*` marker
} MenuMapArea;
STATIC_ASSERT_SIZEOF(MenuMapArea, 0xE);

/// 0x20-byte per-room name string in tables pointed to by `Gp_MapNameTables`.
/// Indexed by `GameSession.location.loc.stage - 1` then `GameSession.location.loc.area - 1`.
typedef struct _GpMapName {
    /* 0x00 */ u8 text[0x20];
} GpMapName;
STATIC_ASSERT_SIZEOF(GpMapName, 0x20);

/// 8-byte map marker in tables pointed to by `Gp_MapMarkTables`.
/// Indexed by loop `i` in `Gp_DrawMapMarks`. `field_0` is the marker's model,
/// a flat outline in the map picture package loaded for its room; `field_4` is
/// the room id (`Gp_MapRoomId`); `field_5` is an extra bit id (`0xFF` = none).
typedef struct _GpMapMark {
    /* 0x0 */ TmdSource* field_0;
    /* 0x4 */ u8         field_4;
    /* 0x5 */ u8         field_5;
    /* 0x6 */ byte       pad_6[2];
} GpMapMark;
STATIC_ASSERT_SIZEOF(GpMapMark, 8);

/// 8-byte map icon record in the per-stage tables pointed to by `D_8010F0CC`.
/// Walked by `Gp_DrawMapIcons` until `field_0` is 0. `field_0` is the room id
/// (`Gp_MapRoomId`), `field_1` the `Gp_DrawMapMarks` marker index the icon
/// belongs to, `field_2` the icon kind (0 / 1 / 2; kind 2 is the blinking
/// "current objective" icon gated on `func_800E3FCC(0xA2)`), `field_3` a
/// GameFlag nibble id (0 = always shown); `x` / `y` are the map coordinates.
typedef struct _GpMapIcon {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ u8  field_2;
    /* 0x3 */ u8  field_3;
    /* 0x4 */ u16 x;
    /* 0x6 */ u16 y;
} GpMapIcon;
STATIC_ASSERT_SIZEOF(GpMapIcon, 8);

/// Six-byte map icon record walked by `func_800D0C34` until `roomId` is 0.
/// `flagId` gates visibility using the stage flag bank (0 = always, 0xFF = skip).
typedef struct _GpMapFlagIcon {
    /* 0x0 */ u8  roomId;
    /* 0x1 */ u8  flagId;
    /* 0x2 */ u16 x;
    /* 0x4 */ u16 y;
} GpMapFlagIcon;
STATIC_ASSERT_SIZEOF(GpMapFlagIcon, 6);

#endif // GAMEPLAY_MAP_H
