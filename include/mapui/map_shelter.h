#ifndef INCLUDE_MAPUI_MAP_SHELTER_H
#define INCLUDE_MAPUI_MAP_SHELTER_H

#include "types.h"

#include "gameplay/area_flags.h"
#include "gameplay/direction.h"
#include "gameplay/item_pickup.h"
#include "gameplay/map.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/gfx_types.h"
#include "main/task_types.h"

extern GfxImageSlot D_map_shelter_80179B40[];

extern TaskIdPair D_map_shelter_8017BE28[];

extern TaskIdPair D_map_shelter_8017C2D8[];

/// This stage's `Gp_MapFlagIds` entry, indexed by map room id.
extern u8 D_map_shelter_80179CD0[];

/// This stage's `Gp_MapRecTables` entry: where each room sits on the map, by room.
extern GpMapRec D_map_shelter_80179CD8[];

/// This stage's `Gp_MapMarkTables` entry: the room markers, each a model in the
/// map picture its map room id selects.
extern GpMapMark D_map_shelter_80179FA4[];

/// This stage's `D_8010F0E0` entry, ended by a room id of 0.
extern GpMapFlagIcon D_map_shelter_8017A134[];

/// This stage's `D_8010F0CC` entry, ended by a room id of 0.
extern GpMapIcon D_map_shelter_8017A1F0[];

/// This stage's `Gp_MapNameTables` entry: the room names, indexed by room - 1.
extern GpMapName D_map_shelter_8017A268[];

/// This stage's `Gp_Bit2Banks` list table, by room, ended by a -1 list.
extern GpBit2List D_map_shelter_8017A998[];

/// This stage's `D_8010FABC` entry: the task each room starts, keyed by
/// `GP_TASK_LOC_KEY` and ended by flags of 0xFFFF.
extern TaskDesc D_map_shelter_8017AB30[];

/// This stage's flag table for `Gp_LookupStageFlag`.
extern u16 D_map_shelter_8017AD88[];

/// This stage's entries in `Gp_RoomCoordTables`, `Gp_WarpTables`,
/// `Gp_ViewCountTables`, `Gp_RoomObjTables`, `Gp_ViewTables`,
/// `Gp_ViewIndexTables`, `Gp_SprtTables` and `Gp_RoomParamTables`: each leads to
/// one pointer per room into that room's package, except that some rooms'
/// coordinate and object records are this overlay's own.
extern WorldCoordRoomLighting* D_map_shelter_8017AEC4[];

extern DirectionWarpEntry* D_map_shelter_8017AF88[];

extern GpViewCountTbl D_map_shelter_8017B110;

extern GpRoomObjTbl D_map_shelter_8017B3B8;

extern GpViewTbl D_map_shelter_8017B480;

extern ViewIndexTable D_map_shelter_8017B548;

extern SpriteAreaTable D_map_shelter_8017B610;

extern WorldCollisionSurfaceProperties** D_map_shelter_8017B614[];

/// This stage's `D_8010F9F4` and `D_8010FA0C` entries: items granted by
/// location, each ended by a key of -1.
extern GpGiveRec D_map_shelter_8017BB58[];

extern GpGiveRec D_map_shelter_8017BD8C[];

/// Updates the outgoing room marker state for this stage.
s32 func_map_shelter_80179A04(RoomEventMsg* in, RoomEventMsg* out);

#endif // INCLUDE_MAPUI_MAP_SHELTER_H
