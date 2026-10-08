#ifndef INCLUDE_MAPUI_MAP_SHELTER_H
#define INCLUDE_MAPUI_MAP_SHELTER_H

#include "types.h"

#include "gameplay/area_flags.h"
#include "gameplay/battle_reward.h"
#include "gameplay/direction.h"
#include "gameplay/map.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/gfx_types.h"
#include "main/stage_types.h"
#include "main/task_types.h"

extern GfxImageSlot D_map_shelter_80179B40[];

extern StageMusicEntry D_map_shelter_8017BE28[];

extern StageMusicEntry D_map_shelter_8017C2D8[];

/// This stage's `Gp_MapFlagIds` entry, indexed by map room id.
extern u8 D_map_shelter_80179CD0[];

/// This stage's `Gp_MapRecTables` entry: where each room sits on the map, by room.
extern MenuMapArea D_map_shelter_80179CD8[];

/// This stage's `Gp_MapMarkTables` entry: the area shapes, indexed by area.
extern MenuMapAreaShape D_map_shelter_80179FA4[];

/// This stage's `D_8010F0E0` entry, ended by a page of 0.
extern MenuMapMarker D_map_shelter_8017A134[];

/// This stage's `D_8010F0CC` entry, ended by a page of 0.
extern MenuMapIcon D_map_shelter_8017A1F0[];

/// This stage's `Gp_MapNameTables` entry: the room names, indexed by room - 1.
extern MenuMapAreaName D_map_shelter_8017A268[];

/// This stage's `Gp_Bit2Banks` room table, ended by `AREA_OBJECT_ROOM_END`.
extern AreaObjectRoom D_map_shelter_8017A998[];

/// This stage's `D_8010FABC` entry: the task each room starts, keyed by
/// `GP_TASK_LOC_KEY` and ended by flags of 0xFFFF.
extern TaskDesc D_map_shelter_8017AB30[];

/// This stage's flag table for `menuMapGetMarkerState`.
extern u16 D_map_shelter_8017AD88[];

/// Mine/Shelter model-lighting arrays indexed by `GameLocationKey.area - 1`.
///
/// Stage 4 has 49 area entries. Each leads to room descriptors indexed by
/// `GameLocationKey.room - 1`; area 39 has two dumping-hole rooms and area 40
/// seven incinerator rooms. Room extents are determined by each area's array.
/// The map overlay must be loaded for the table and its local descriptors;
/// pointers into a room overlay require that specific overlay to remain loaded.
extern WorldCoordRoomLighting* gMapShelterRoomLightingTables[49];

/// This stage's entries in `Gp_WarpTables`,
/// `Gp_ViewCountTables`, `Gp_RoomObjTables`, `Gp_ViewTables`,
/// `gViewIndexTables`, `gSpriteAreaTables` and `Gp_RoomParamTables`: each leads to
/// one pointer per room into that room's package, except that some rooms'
/// object records are this overlay's own.
extern DirectionWarpEntry* D_map_shelter_8017AF88[];

extern ViewCountTable D_map_shelter_8017B110;

extern WorldCollisionStageResources D_map_shelter_8017B3B8;

extern ViewCameraTable D_map_shelter_8017B480;

/// Mine/Shelter's directory of logical-view maps for 49 areas.
///
/// Area, room and logical-view IDs are one-based with separate extents. The map
/// overlay owns the area directory and borrows each loaded room's maps.
extern ViewIndexTable gMapShelterViewIndexTable;

/// Mine/Shelter's directory of mapped-view sprite arrays for 49 areas.
///
/// Contains one directory record; area and mapped-view IDs are one-based with
/// separate extents. Room-owned mutable sprite resources must remain loaded.
extern SpriteAreaTable gMapShelterSpriteAreaTable;

extern WorldCollisionSurfaceProperties** D_map_shelter_8017B614[];

/// This stage's `D_8010F9F4` and `D_8010FA0C` entries: items awarded
/// for battles won at a location, each list ended by a key of
/// `INVENTORY_BATTLE_REWARD_LIST_END`.
extern InventoryBattleReward D_map_shelter_8017BB58[];

extern InventoryBattleReward D_map_shelter_8017BD8C[];

/// Resolves a Mine/Shelter destination's room variant from game progress.
///
/// The `map_shelter` overlay must be loaded. Reads `request->areaId` only when
/// `request->queryOnly` is `ROOM_EVENT_EXECUTE`; a query leaves `reply` untouched.
/// Only `reply->room` can change. Initialize the reply's destination selectors
/// before calling: unhandled areas and unmet progress thresholds keep its room.
/// Request and reply may be the same record; neither pointer is retained.
/// Returns 1 even when no room changes. Progress nibbles used as room indices
/// must select existing rooms; this resolver does not validate their range.
s32 mapShelterRoomVariantResolve(RoomEventMsg* request, RoomEventMsg* reply);

#endif // INCLUDE_MAPUI_MAP_SHELTER_H
