#ifndef INCLUDE_MAPUI_MAP_AKROPOLIS_H
#define INCLUDE_MAPUI_MAP_AKROPOLIS_H

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
#include "main/session_types.h"
#include "main/stage_types.h"
#include "main/task_types.h"

/// Places movie decode buffers for the current Acropolis area.
///
/// Borrows `location` only for its area ID. Main must first cache movie pixel
/// dimensions, reserve the movie workspace and actor-buffer-0 ring/table region,
/// and reset staging flags. The 32-sector ring occupies 0x10000 bytes; all VLC
/// and decoded-frame extents must fit the reserved regions and be word-aligned.
/// Layouts reuse actor buffers, clear their session reuse fields and stage
/// decoded columns in VRAM at (704,0) or (384,256), in words/rows. Other areas
/// retain the preceding buffer pointers. No allocation or bounds check occurs.
/// Storage remains borrowed by the decoder through playback.
void mapAkropolisSetupMovieBuffers(const GameLocationKey* location);

/// Queues the MIST briefing's four-item menu as a display-mode task.
///
/// `unused` is ignored. With zero `retainSelection`, marks all four briefing
/// items collected and clears the Dryfield-map choice; nonzero preserves it.
/// Always returns 1, including when queueing fails. The singleton list and
/// choice storage require this overlay to remain loaded throughout the menu.
s32 mapAkropolisOpenKeyItemMenu(s32 unused, s32 retainSelection);

/// Returns 1 once the briefing menu has opened the Dryfield map, otherwise 0.
///
/// The latch survives menu closure and reopening with nonzero retainSelection.
/// A fresh menu request clears it; opening another item's detail does not.
s32 mapAkropolisWasDryfieldMapSelected(void);

/// Image slots of each map, indexed by `Gfx_ImageSlotTables`.
extern GfxImageSlot D_map_akropolis_8017A048[];

/// Per-map tables `CdCmd_MapHeapSizes` indexes (maps without one have NULL there).
extern s32 D_map_akropolis_8017A0F8[];

/// Per-map task id tables `Stage_MusicTables` and `Stage_CountdownMusicTables` index.
extern StageMusicEntry D_map_akropolis_8017C1B4[];

extern StageMusicEntry D_map_akropolis_8017C304[];

/// This stage's `Gp_MapFlagIds` entry, indexed by map room id.
extern u8 D_map_akropolis_8017A14C[];

/// This stage's `Gp_MapRecTables` entry: where each room sits on the map, by room.
extern MenuMapArea D_map_akropolis_8017A154[];

/// This stage's `Gp_MapMarkTables` entry: the area shapes, indexed by area.
extern MenuMapAreaShape D_map_akropolis_8017A288[];

/// This stage's `D_8010F0E0` entry, ended by a page of 0.
extern MenuMapMarker D_map_akropolis_8017A330[];

/// This stage's `D_8010F0CC` entry, ended by a page of 0.
extern MenuMapIcon D_map_akropolis_8017A38C[];

/// This stage's `Gp_MapNameTables` entry: the room names, indexed by room - 1.
extern MenuMapAreaName D_map_akropolis_8017A3BC[];

/// This stage's `Gp_Bit2Banks` room table, ended by `AREA_OBJECT_ROOM_END`.
extern AreaObjectRoom D_map_akropolis_8017A7FC[];

/// This stage's `D_8010FABC` entry: the task each room starts, keyed by
/// `GP_TASK_LOC_KEY` and ended by flags of 0xFFFF.
extern TaskDesc D_map_akropolis_8017A8AC[];

/// This stage's flag table for `menuMapGetMarkerState`.
extern u16 D_map_akropolis_8017AA0C[];

/// This stage's entries in `gWorldCoordRoomLightingTables`, `Gp_RoomObjTables`,
/// `gSpriteAreaTables`, `Gp_WarpTables`, `Gp_ViewCountTables`, `Gp_ViewTables`,
/// `gViewIndexTables` and `Gp_RoomParamTables`: each leads to one pointer per
/// room into that room's package.
extern WorldCoordRoomLighting* D_map_akropolis_8017AA28[];

extern WorldCollisionStageResources D_map_akropolis_8017AAC8;

extern SpriteAreaTable D_map_akropolis_8017AB1C;

extern DirectionWarpEntry* D_map_akropolis_8017AB20[];

extern ViewCountTable D_map_akropolis_8017ABC0;

extern ViewCameraTable D_map_akropolis_8017AC14;

extern ViewIndexTable D_map_akropolis_8017AC68;

extern WorldCollisionSurfaceProperties** D_map_akropolis_8017AC6C[];

/// Area placement lists, each ended by an entry id of 0xFF. The rooms' area
/// records point at them, one list per location.
extern AreaPlacement D_map_akropolis_8017ACBC[];

extern AreaPlacement D_map_akropolis_8017ACDC[];

extern AreaPlacement D_map_akropolis_8017ACFC[];

extern AreaPlacement D_map_akropolis_8017AD2C[];

extern AreaPlacement D_map_akropolis_8017AD7C[];

extern AreaPlacement D_map_akropolis_8017ADEC[];

extern AreaPlacement D_map_akropolis_8017AE3C[];

extern AreaPlacement D_map_akropolis_8017AE6C[];

extern AreaPlacement D_map_akropolis_8017AE9C[];

extern AreaPlacement D_map_akropolis_8017AEDC[];

extern AreaPlacement D_map_akropolis_8017AF1C[];

extern AreaPlacement D_map_akropolis_8017AF3C[];

extern AreaPlacement D_map_akropolis_8017AF6C[];

extern AreaPlacement D_map_akropolis_8017AFCC[];

extern AreaPlacement D_map_akropolis_8017AFEC[];

extern AreaPlacement D_map_akropolis_8017B01C[];

extern AreaPlacement D_map_akropolis_8017B04C[];

extern AreaPlacement D_map_akropolis_8017B0DC[];

extern AreaPlacement D_map_akropolis_8017B0FC[];

extern AreaPlacement D_map_akropolis_8017B11C[];

extern AreaPlacement D_map_akropolis_8017B17C[];

extern AreaPlacement D_map_akropolis_8017B1AC[];

extern AreaPlacement D_map_akropolis_8017B1DC[];

extern AreaPlacement D_map_akropolis_8017B1FC[];

extern AreaPlacement D_map_akropolis_8017B24C[];

extern AreaPlacement D_map_akropolis_8017B29C[];

extern AreaPlacement D_map_akropolis_8017B2EC[];

extern AreaPlacement D_map_akropolis_8017B33C[];

extern AreaPlacement D_map_akropolis_8017B35C[];

extern AreaPlacement D_map_akropolis_8017B41C[];

extern AreaPlacement D_map_akropolis_8017B48C[];

extern AreaPlacement D_map_akropolis_8017B4FC[];

extern AreaPlacement D_map_akropolis_8017B54C[];

extern AreaPlacement D_map_akropolis_8017B5BC[];

extern AreaPlacement D_map_akropolis_8017B5EC[];

extern AreaPlacement D_map_akropolis_8017B65C[];

extern AreaPlacement D_map_akropolis_8017B67C[];

extern AreaPlacement D_map_akropolis_8017B6AC[];

extern AreaPlacement D_map_akropolis_8017B6DC[];

extern AreaPlacement D_map_akropolis_8017B70C[];

extern AreaPlacement D_map_akropolis_8017B73C[];

extern AreaPlacement D_map_akropolis_8017B7DC[];

extern AreaPlacement D_map_akropolis_8017B80C[];

extern AreaPlacement D_map_akropolis_8017B82C[];

extern AreaPlacement D_map_akropolis_8017B85C[];

extern AreaPlacement D_map_akropolis_8017B8DC[];

extern AreaPlacement D_map_akropolis_8017B98C[];

extern AreaPlacement D_map_akropolis_8017B9AC[];

extern AreaPlacement D_map_akropolis_8017BA5C[];

extern AreaPlacement D_map_akropolis_8017BA8C[];

extern AreaPlacement D_map_akropolis_8017BABC[];

extern AreaPlacement D_map_akropolis_8017BB1C[];

extern AreaPlacement D_map_akropolis_8017BB9C[];

extern AreaPlacement D_map_akropolis_8017BBFC[];

extern AreaPlacement D_map_akropolis_8017BC7C[];

extern AreaPlacement D_map_akropolis_8017BCEC[];

extern AreaPlacement D_map_akropolis_8017BCFC[];

extern AreaPlacement D_map_akropolis_8017BD0C[];

extern AreaPlacement D_map_akropolis_8017BD2C[];

extern AreaPlacement D_map_akropolis_8017BD4C[];

extern AreaPlacement D_map_akropolis_8017BD8C[];

extern AreaPlacement D_map_akropolis_8017BDBC[];

extern AreaPlacement D_map_akropolis_8017BDEC[];

/// This stage's `D_8010F9F4` and `D_8010FA0C` entries: items awarded
/// for battles won at a location, each list ended by a key of
/// `INVENTORY_BATTLE_REWARD_LIST_END`.
extern InventoryBattleReward D_map_akropolis_8017C0DC[];

extern InventoryBattleReward D_map_akropolis_8017C16C[];

#endif // INCLUDE_MAPUI_MAP_AKROPOLIS_H
