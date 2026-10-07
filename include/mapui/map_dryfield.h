#ifndef INCLUDE_MAPUI_MAP_DRYFIELD_H
#define INCLUDE_MAPUI_MAP_DRYFIELD_H

#include "types.h"

#include "gameplay/area_flags.h"
#include "gameplay/areaplace.h"
#include "gameplay/battle_reward.h"
#include "gameplay/direction.h"
#include "gameplay/map.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/gfx_types.h"
#include "main/stage_types.h"
#include "main/task_types.h"

extern GfxImageSlot D_map_dryfield_80179A14[];

extern s32 D_map_dryfield_80179B4C[];

extern StageMusicEntry D_map_dryfield_8017BDE0[];

extern StageMusicEntry D_map_dryfield_8017C004[];

/// This stage's `Gp_MapFlagIds` entry, indexed by map room id.
extern u8 D_map_dryfield_80179BCC[];

/// This stage's `Gp_MapRecTables` entry: where each room sits on the map, by room.
extern MenuMapArea D_map_dryfield_80179BD0[];

/// This stage's `Gp_MapMarkTables` entry: the area shapes, indexed by area.
extern MenuMapAreaShape D_map_dryfield_80179E00[];

/// This stage's `D_8010F0E0` entry, ended by a page of 0.
extern MenuMapMarker D_map_dryfield_80179F38[];

/// This stage's `D_8010F0CC` entry, ended by a page of 0.
extern MenuMapIcon D_map_dryfield_80179FEC[];

/// This stage's `Gp_MapNameTables` entry: the room names, indexed by room - 1.
extern MenuMapAreaName D_map_dryfield_8017A044[];

/// This stage's `Gp_Bit2Banks` room table, ended by `AREA_OBJECT_ROOM_END`.
extern AreaObjectRoom D_map_dryfield_8017A564[];

/// This stage's `D_8010FABC` entry: the task each room starts, keyed by
/// `GP_TASK_LOC_KEY` and ended by flags of 0xFFFF.
extern TaskDesc D_map_dryfield_8017A6A4[];

/// This stage's flag table for `Gp_LookupStageFlag`.
extern u16 D_map_dryfield_8017A824[];

/// This stage's entries in `gWorldCoordRoomLightingTables`, `Gp_WarpTables`,
/// `Gp_ViewCountTables`, `Gp_RoomObjTables`, `Gp_ViewTables`,
/// `Gp_ViewIndexTables`, `Gp_SprtTables` and `Gp_RoomParamTables`: each leads to
/// one pointer per room into that room's package.
extern WorldCoordRoomLighting* D_map_dryfield_8017A860[];

extern DirectionWarpEntry* D_map_dryfield_8017A8F8[];

extern ViewCountTable D_map_dryfield_8017AA28;

extern WorldCollisionStageResources D_map_dryfield_8017AAC4;

extern ViewCameraTable D_map_dryfield_8017AB60;

extern ViewIndexTable D_map_dryfield_8017ABFC;

extern SpriteAreaTable D_map_dryfield_8017AC98;

extern WorldCollisionSurfaceProperties** D_map_dryfield_8017AC9C[];

/// Area placement lists, each ended by an entry id of 0xFF. The rooms' area
/// records point at them, one list per location.
extern AreaPlacement D_map_dryfield_8017AD34[];

extern AreaPlacement D_map_dryfield_8017AD44[];

extern AreaPlacement D_map_dryfield_8017AD74[];

extern AreaPlacement D_map_dryfield_8017ADE4[];

extern AreaPlacement D_map_dryfield_8017AE14[];

extern AreaPlacement D_map_dryfield_8017AE44[];

extern AreaPlacement D_map_dryfield_8017AEA4[];

extern AreaPlacement D_map_dryfield_8017AED4[];

extern AreaPlacement D_map_dryfield_8017AF04[];

extern AreaPlacement D_map_dryfield_8017AF44[];

extern AreaPlacement D_map_dryfield_8017AF64[];

extern AreaPlacement D_map_dryfield_8017AF94[];

extern AreaPlacement D_map_dryfield_8017AFA4[];

extern AreaPlacement D_map_dryfield_8017B004[];

extern AreaPlacement D_map_dryfield_8017B054[];

extern AreaPlacement D_map_dryfield_8017B0B4[];

extern AreaPlacement D_map_dryfield_8017B0E4[];

extern AreaPlacement D_map_dryfield_8017B154[];

extern AreaPlacement D_map_dryfield_8017B1C4[];

extern AreaPlacement D_map_dryfield_8017B1D4[];

extern AreaPlacement D_map_dryfield_8017B204[];

extern AreaPlacement D_map_dryfield_8017B284[];

extern AreaPlacement D_map_dryfield_8017B2A4[];

extern AreaPlacement D_map_dryfield_8017B2D4[];

extern AreaPlacement D_map_dryfield_8017B354[];

extern AreaPlacement D_map_dryfield_8017B394[];

extern AreaPlacement D_map_dryfield_8017B3F4[];

extern AreaPlacement D_map_dryfield_8017B474[];

extern AreaPlacement D_map_dryfield_8017B4B4[];

extern AreaPlacement D_map_dryfield_8017B534[];

extern AreaPlacement D_map_dryfield_8017B564[];

extern AreaPlacement D_map_dryfield_8017B5E4[];

extern AreaPlacement D_map_dryfield_8017B644[];

extern AreaPlacement D_map_dryfield_8017B664[];

extern AreaPlacement D_map_dryfield_8017B674[];

extern AreaPlacement D_map_dryfield_8017B6A4[];

extern AreaPlacement D_map_dryfield_8017B724[];

extern AreaPlacement D_map_dryfield_8017B744[];

extern AreaPlacement D_map_dryfield_8017B7A4[];

extern AreaPlacement D_map_dryfield_8017B7E4[];

extern AreaPlacement D_map_dryfield_8017B984[];

extern AreaPlacement D_map_dryfield_8017B994[];

extern AreaPlacement D_map_dryfield_8017B9B4[];

extern AreaPlacement D_map_dryfield_8017B9C4[];

extern AreaPlacement D_map_dryfield_8017BA14[];

extern AreaPlacement D_map_dryfield_8017BA34[];

extern AreaPlacement D_map_dryfield_8017BA44[];

extern AreaPlacement D_map_dryfield_8017BA84[];

extern AreaPlacement D_map_dryfield_8017BAF4[];

extern AreaPlacement D_map_dryfield_8017BB24[];

extern AreaPlacement D_map_dryfield_8017BB54[];

extern AreaPlacement D_map_dryfield_8017BB74[];

extern AreaPlacement D_map_dryfield_8017BB84[];

extern AreaPlacement D_map_dryfield_8017BC14[];

extern AreaPlacement D_map_dryfield_8017BCC4[];

/// This stage's `D_8010F9F4` and `D_8010FA0C` entries: items awarded
/// for battles won at a location, each list ended by a key of
/// `INVENTORY_BATTLE_REWARD_LIST_END`.
extern InventoryBattleReward D_map_dryfield_8017BCE4[];

extern InventoryBattleReward D_map_dryfield_8017BD80[];

/// The Dryfield map's part of main's MDEC buffer setup: `Mdec_SetupBuffers`
/// calls this map-slot address (as `func_80179954`) for one of its layouts.
void func_map_dryfield_80179954(u8* entry);

#endif // INCLUDE_MAPUI_MAP_DRYFIELD_H
