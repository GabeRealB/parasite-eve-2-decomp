#ifndef GAMEPLAY_MAP_H
#define GAMEPLAY_MAP_H

#include "common.h"

#include "main/tmd.h"

/// 0xE-byte per-room record in tables pointed to by `Gp_MapRecTables`.
/// Indexed by `GameSession.at4.loc.stage - 1` then `GameSession.at4.loc.area`.
/// field_0/field_2 are signed coords, field_4/field_6 unsigned extents,
/// field_8/field_A signed scales (`Gp_DrawMapCursor`); field_C is the
/// room id stored in `Gp_MapRoomId` (`Gp_GetMapRoomId`).
typedef struct _GpMapRec {
    /* 0x00 */ s16 field_0;
    /* 0x02 */ s16 field_2;
    /* 0x04 */ u16 field_4;
    /* 0x06 */ u16 field_6;
    /* 0x08 */ s16 field_8;
    /* 0x0A */ s16 field_A;
    /* 0x0C */ u16 field_C;
} GpMapRec;
STATIC_ASSERT_SIZEOF(GpMapRec, 0xE);

/// 0x20-byte per-room name string in tables pointed to by `Gp_MapNameTables`.
/// Indexed by `GameSession.at4.loc.stage - 1` then `GameSession.at4.loc.area - 1`.
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
