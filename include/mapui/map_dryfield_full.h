#ifndef INCLUDE_MAPUI_MAP_DRYFIELD_FULL_H
#define INCLUDE_MAPUI_MAP_DRYFIELD_FULL_H

#include "types.h"

#include "gameplay/area_flags.h"
#include "gameplay/areaplace.h"
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

extern GfxImageSlot D_map_dryfield_full_801799A4[];

extern StageMusicEntry D_map_dryfield_full_8017D238[];

extern StageMusicEntry D_map_dryfield_full_8017D594[];

/// This stage's `Gp_MapFlagIds` entry, indexed by map room id.
extern u8 D_map_dryfield_full_80179ADC[];

/// This stage's `Gp_MapRecTables` entry: where each room sits on the map, by room.
extern MenuMapArea D_map_dryfield_full_80179AE0[];

/// This stage's `Gp_MapMarkTables` entry: the area shapes, indexed by area.
extern MenuMapAreaShape D_map_dryfield_full_80179D10[];

/// This stage's `D_8010F0E0` entry, ended by a page of 0.
extern MenuMapMarker D_map_dryfield_full_80179E48[];

/// This stage's `D_8010F0CC` entry, ended by a page of 0.
extern MenuMapIcon D_map_dryfield_full_80179F04[];

/// This stage's `Gp_MapNameTables` entry: the room names, indexed by room - 1.
extern MenuMapAreaName D_map_dryfield_full_80179F4C[];

/// This stage's `Gp_Bit2Banks` room table, ended by `AREA_OBJECT_ROOM_END`.
extern AreaObjectRoom D_map_dryfield_full_8017A46C[];

/// This stage's `D_8010FABC` entry: the task each room starts, keyed by
/// `GP_TASK_LOC_KEY` and ended by flags of 0xFFFF.
extern TaskDesc D_map_dryfield_full_8017A5AC[];

/// This stage's flag table for `Gp_LookupStageFlag`.
extern u16 D_map_dryfield_full_8017A738[];

/// This stage's entries in `gWorldCoordRoomLightingTables`, `Gp_WarpTables`,
/// `Gp_ViewCountTables`, `Gp_RoomObjTables`, `Gp_ViewTables`,
/// `Gp_ViewIndexTables`, `Gp_SprtTables` and `Gp_RoomParamTables`: each leads to
/// one pointer per room into that room's package.
extern WorldCoordRoomLighting* D_map_dryfield_full_8017A774[];

extern DirectionWarpEntry* D_map_dryfield_full_8017A80C[];

extern ViewCountTable D_map_dryfield_full_8017A93C;

extern WorldCollisionStageResources D_map_dryfield_full_8017A9D8;

extern ViewCameraTable D_map_dryfield_full_8017AA74;

extern ViewIndexTable D_map_dryfield_full_8017AB10;

extern SpriteAreaTable D_map_dryfield_full_8017ABAC;

extern WorldCollisionSurfaceProperties** D_map_dryfield_full_8017ABB0[];

/// Area placement lists, each ended by an entry id of 0xFF. The rooms' area
/// records point at them, one list per location.
extern AreaPlacement D_map_dryfield_full_8017AC48[];

extern AreaPlacement D_map_dryfield_full_8017AD18[];

extern AreaPlacement D_map_dryfield_full_8017AD48[];

extern AreaPlacement D_map_dryfield_full_8017AD78[];

extern AreaPlacement D_map_dryfield_full_8017ADA8[];

extern AreaPlacement D_map_dryfield_full_8017ADF8[];

extern AreaPlacement D_map_dryfield_full_8017AE78[];

extern AreaPlacement D_map_dryfield_full_8017AE98[];

extern AreaPlacement D_map_dryfield_full_8017AEC8[];

extern AreaPlacement D_map_dryfield_full_8017AF18[];

extern AreaPlacement D_map_dryfield_full_8017AF58[];

extern AreaPlacement D_map_dryfield_full_8017AFE8[];

extern AreaPlacement D_map_dryfield_full_8017B048[];

extern AreaPlacement D_map_dryfield_full_8017B0B8[];

extern AreaPlacement D_map_dryfield_full_8017B128[];

extern AreaPlacement D_map_dryfield_full_8017B1A8[];

extern AreaPlacement D_map_dryfield_full_8017B1C8[];

extern AreaPlacement D_map_dryfield_full_8017B2B8[];

extern AreaPlacement D_map_dryfield_full_8017B308[];

extern AreaPlacement D_map_dryfield_full_8017B358[];

extern AreaPlacement D_map_dryfield_full_8017B408[];

extern AreaPlacement D_map_dryfield_full_8017B438[];

extern AreaPlacement D_map_dryfield_full_8017B4A8[];

extern AreaPlacement D_map_dryfield_full_8017B508[];

extern AreaPlacement D_map_dryfield_full_8017B558[];

extern AreaPlacement D_map_dryfield_full_8017B5D8[];

extern AreaPlacement D_map_dryfield_full_8017B648[];

extern AreaPlacement D_map_dryfield_full_8017B6A8[];

extern AreaPlacement D_map_dryfield_full_8017B708[];

extern AreaPlacement D_map_dryfield_full_8017B798[];

extern AreaPlacement D_map_dryfield_full_8017B7D8[];

extern AreaPlacement D_map_dryfield_full_8017B858[];

extern AreaPlacement D_map_dryfield_full_8017B8C8[];

extern AreaPlacement D_map_dryfield_full_8017B938[];

extern AreaPlacement D_map_dryfield_full_8017B998[];

extern AreaPlacement D_map_dryfield_full_8017BA08[];

extern AreaPlacement D_map_dryfield_full_8017BA38[];

extern AreaPlacement D_map_dryfield_full_8017BAB8[];

extern AreaPlacement D_map_dryfield_full_8017BB28[];

extern AreaPlacement D_map_dryfield_full_8017BB78[];

extern AreaPlacement D_map_dryfield_full_8017BBE8[];

extern AreaPlacement D_map_dryfield_full_8017BC08[];

extern AreaPlacement D_map_dryfield_full_8017BCC8[];

extern AreaPlacement D_map_dryfield_full_8017BD38[];

extern AreaPlacement D_map_dryfield_full_8017BDF8[];

extern AreaPlacement D_map_dryfield_full_8017BE58[];

extern AreaPlacement D_map_dryfield_full_8017BE78[];

extern AreaPlacement D_map_dryfield_full_8017BEA8[];

extern AreaPlacement D_map_dryfield_full_8017BF08[];

extern AreaPlacement D_map_dryfield_full_8017BF88[];

extern AreaPlacement D_map_dryfield_full_8017C038[];

extern AreaPlacement D_map_dryfield_full_8017C0B8[];

extern AreaPlacement D_map_dryfield_full_8017C158[];

extern AreaPlacement D_map_dryfield_full_8017C198[];

extern AreaPlacement D_map_dryfield_full_8017C1B8[];

extern AreaPlacement D_map_dryfield_full_8017C228[];

extern AreaPlacement D_map_dryfield_full_8017C238[];

extern AreaPlacement D_map_dryfield_full_8017C288[];

extern AreaPlacement D_map_dryfield_full_8017C2A8[];

extern AreaPlacement D_map_dryfield_full_8017C378[];

extern AreaPlacement D_map_dryfield_full_8017C448[];

extern AreaPlacement D_map_dryfield_full_8017C4E8[];

extern AreaPlacement D_map_dryfield_full_8017C538[];

extern AreaPlacement D_map_dryfield_full_8017C568[];

extern AreaPlacement D_map_dryfield_full_8017C578[];

extern AreaPlacement D_map_dryfield_full_8017C5B8[];

extern AreaPlacement D_map_dryfield_full_8017C698[];

extern AreaPlacement D_map_dryfield_full_8017C768[];

extern AreaPlacement D_map_dryfield_full_8017C7F8[];

extern AreaPlacement D_map_dryfield_full_8017C828[];

extern AreaPlacement D_map_dryfield_full_8017C878[];

extern AreaPlacement D_map_dryfield_full_8017C8B8[];

extern AreaPlacement D_map_dryfield_full_8017C8E8[];

extern AreaPlacement D_map_dryfield_full_8017C948[];

extern AreaPlacement D_map_dryfield_full_8017C968[];

extern AreaPlacement D_map_dryfield_full_8017C9C8[];

extern AreaPlacement D_map_dryfield_full_8017CA28[];

extern AreaPlacement D_map_dryfield_full_8017CA88[];

extern AreaPlacement D_map_dryfield_full_8017CAC8[];

extern AreaPlacement D_map_dryfield_full_8017CAF8[];

extern AreaPlacement D_map_dryfield_full_8017CB58[];

extern AreaPlacement D_map_dryfield_full_8017CB98[];

extern AreaPlacement D_map_dryfield_full_8017CBF8[];

extern AreaPlacement D_map_dryfield_full_8017CC78[];

extern AreaPlacement D_map_dryfield_full_8017CCE8[];

extern AreaPlacement D_map_dryfield_full_8017CD08[];

extern AreaPlacement D_map_dryfield_full_8017CD38[];

extern AreaPlacement D_map_dryfield_full_8017CD48[];

extern AreaPlacement D_map_dryfield_full_8017CD88[];

extern AreaPlacement D_map_dryfield_full_8017CDE8[];

extern AreaPlacement D_map_dryfield_full_8017CEA8[];

extern AreaPlacement D_map_dryfield_full_8017CF58[];

extern AreaPlacement D_map_dryfield_full_8017CF78[];

extern AreaPlacement D_map_dryfield_full_8017CFD8[];

extern AreaPlacement D_map_dryfield_full_8017D018[];

extern AreaPlacement D_map_dryfield_full_8017D038[];

/// This stage's `D_8010F9F4` and `D_8010FA0C` entries: items awarded
/// for battles won at a location, each list ended by a key of
/// `INVENTORY_BATTLE_REWARD_LIST_END`.
extern InventoryBattleReward D_map_dryfield_full_8017D0B8[];

extern InventoryBattleReward D_map_dryfield_full_8017D1CC[];

/// Updates the outgoing room marker state for this stage.
s32 func_map_dryfield_full_80179954(RoomEventMsg* in, RoomEventMsg* out);

#endif // INCLUDE_MAPUI_MAP_DRYFIELD_FULL_H
