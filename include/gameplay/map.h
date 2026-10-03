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

/// `MenuMapIcon.kind` of a telephone.
#define MENU_MAP_ICON_KIND_TELEPHONE 1

/// `MenuMapIcon.kind` of the marker for where the current objective is.
#define MENU_MAP_ICON_KIND_OBJECTIVE 2

/// A 16x16 icon the map screen draws over one area of a stage's map.
///
/// Each stage has a table of these, closed by a record whose `page` is 0. An
/// icon is drawn with its area, while its page is the one on screen and its
/// `condition` holds. A telephone and the kind-0 picture are fixed and appear
/// only once the area has been visited; an objective marker pulses, and shows
/// in an area not yet visited too. What the kind-0 picture depicts is unproven:
/// it sits in rooms with a shop, behind the flag that opens the shop, and in
/// other rooms unconditionally.
typedef struct {
    u8  page;      // Map page the icon is on, counted from 1; the page of its area's `MenuMapArea`
    u8  area;      // Area the icon belongs to, as `GameSession.location.loc.area`
    u8  kind;      // Picture (0 fixed picture, 1 telephone, 2 objective marker)
    u8  condition; // Objective marker: the objective number it marks. Other kinds: a game flag that must be set, or 0 for always
    s16 x;         // Map-screen X of the icon's centre
    s16 y;         // Map-screen Y of the icon's centre
} MenuMapIcon;
STATIC_ASSERT_SIZEOF(MenuMapIcon, 8);

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
