#ifndef INCLUDE_MAPUI_MAP_NEO_ARK_H
#define INCLUDE_MAPUI_MAP_NEO_ARK_H

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

extern GfxImageSlot D_map_neo_ark_80179DB8[];

extern s32 D_map_neo_ark_80179EC8[];

extern StageMusicEntry D_map_neo_ark_8017CB54[];

extern StageMusicEntry D_map_neo_ark_8017CDFC[];

/// This stage's `Gp_MapFlagIds` entry, indexed by map room id.
extern u8 D_map_neo_ark_80179F1C[];

/// This stage's `Gp_MapRecTables` entry: where each room sits on the map, by room.
extern MenuMapArea D_map_neo_ark_80179F24[];

/// This stage's `Gp_MapMarkTables` entry: the area shapes, indexed by area.
extern MenuMapAreaShape D_map_neo_ark_8017A110[];

/// This stage's `D_8010F0E0` entry, ended by a page of 0.
extern MenuMapMarker D_map_neo_ark_8017A230[];

/// This stage's `D_8010F0CC` entry, ended by a page of 0.
extern MenuMapIcon D_map_neo_ark_8017A26C[];

/// This stage's `Gp_MapNameTables` entry: the room names, indexed by room - 1.
extern MenuMapAreaName D_map_neo_ark_8017A28C[];

/// This stage's `Gp_Bit2Banks` room table, ended by `AREA_OBJECT_ROOM_END`.
extern AreaObjectRoom D_map_neo_ark_8017A6EC[];

/// This stage's `D_8010FABC` entry: the task each room starts, keyed by
/// `GP_TASK_LOC_KEY` and ended by flags of 0xFFFF.
extern TaskDesc D_map_neo_ark_8017A804[];

/// This stage's flag table for `Gp_LookupStageFlag`.
extern u16 D_map_neo_ark_8017A9A0[];

/// This stage's entries in `Gp_RoomCoordTables`, `Gp_WarpTables`,
/// `Gp_ViewCountTables`, `Gp_RoomObjTables`, `Gp_ViewTables`,
/// `Gp_ViewIndexTables`, `Gp_SprtTables` and `Gp_RoomParamTables`: each leads to
/// one pointer per room, into that room's package or, for some rooms, at a
/// record this overlay holds itself.
extern WorldCoordRoomLighting* D_map_neo_ark_8017A9FC[];

extern DirectionWarpEntry* D_map_neo_ark_8017AA80[];

extern ViewCountTable D_map_neo_ark_8017AB88;

extern WorldCollisionStageResources D_map_neo_ark_8017ACA0;

extern ViewCameraTable D_map_neo_ark_8017AD28;

extern ViewIndexTable D_map_neo_ark_8017ADB0;

extern SpriteAreaTable D_map_neo_ark_8017AE38;

extern WorldCollisionSurfaceProperties** D_map_neo_ark_8017AE3C[];

/// Area placement lists, each ended by an entry id of 0xFF. The rooms' area
/// records point at them, one list per location; one location's list runs on
/// into the next one's.
extern AreaPlacement D_map_neo_ark_8017AEC0[];

extern AreaPlacement D_map_neo_ark_8017AED0[];

extern AreaPlacement D_map_neo_ark_8017AF00[];

extern AreaPlacement D_map_neo_ark_8017AF30[];

extern AreaPlacement D_map_neo_ark_8017AF80[];

extern AreaPlacement D_map_neo_ark_8017AFD0[];

extern AreaPlacement D_map_neo_ark_8017AFF0[];

extern AreaPlacement D_map_neo_ark_8017B010[];

extern AreaPlacement D_map_neo_ark_8017B030[];

extern AreaPlacement D_map_neo_ark_8017B080[];

extern AreaPlacement D_map_neo_ark_8017B0F0[];

extern AreaPlacement D_map_neo_ark_8017B120[];

extern AreaPlacement D_map_neo_ark_8017B160[];

extern AreaPlacement D_map_neo_ark_8017B190[];

extern AreaPlacement D_map_neo_ark_8017B1C0[];

extern AreaPlacement D_map_neo_ark_8017B1F0[];

extern AreaPlacement D_map_neo_ark_8017B210[];

extern AreaPlacement D_map_neo_ark_8017B2C0[];

extern AreaPlacement D_map_neo_ark_8017B300[];

extern AreaPlacement D_map_neo_ark_8017B330[];

extern AreaPlacement D_map_neo_ark_8017B360[];

extern AreaPlacement D_map_neo_ark_8017B390[];

extern AreaPlacement D_map_neo_ark_8017B3C0[];

extern AreaPlacement D_map_neo_ark_8017B3D0[];

extern AreaPlacement D_map_neo_ark_8017B410[];

extern AreaPlacement D_map_neo_ark_8017B470[];

extern AreaPlacement D_map_neo_ark_8017B4B0[];

extern AreaPlacement D_map_neo_ark_8017B4E0[];

extern AreaPlacement D_map_neo_ark_8017B530[];

extern AreaPlacement D_map_neo_ark_8017B550[];

extern AreaPlacement D_map_neo_ark_8017B590[];

extern AreaPlacement D_map_neo_ark_8017B5C0[];

extern AreaPlacement D_map_neo_ark_8017B600[];

extern AreaPlacement D_map_neo_ark_8017B610[];

extern AreaPlacement D_map_neo_ark_8017B630[];

extern AreaPlacement D_map_neo_ark_8017B6E0[];

extern AreaPlacement D_map_neo_ark_8017B700[];

extern AreaPlacement D_map_neo_ark_8017B780[];

extern AreaPlacement D_map_neo_ark_8017B7D0[];

extern AreaPlacement D_map_neo_ark_8017B850[];

extern AreaPlacement D_map_neo_ark_8017B8B0[];

extern AreaPlacement D_map_neo_ark_8017B8D0[];

extern AreaPlacement D_map_neo_ark_8017B950[];

extern AreaPlacement D_map_neo_ark_8017B9E0[];

extern AreaPlacement D_map_neo_ark_8017BA60[];

extern AreaPlacement D_map_neo_ark_8017BB00[];

extern AreaPlacement D_map_neo_ark_8017BB70[];

extern AreaPlacement D_map_neo_ark_8017BBF0[];

extern AreaPlacement D_map_neo_ark_8017BC70[];

extern AreaPlacement D_map_neo_ark_8017BCE0[];

extern AreaPlacement D_map_neo_ark_8017BD50[];

extern AreaPlacement D_map_neo_ark_8017BD80[];

extern AreaPlacement D_map_neo_ark_8017BDB0[];

extern AreaPlacement D_map_neo_ark_8017BDC0[];

extern AreaPlacement D_map_neo_ark_8017BDD0[];

extern AreaPlacement D_map_neo_ark_8017BE40[];

extern AreaPlacement D_map_neo_ark_8017BE70[];

extern AreaPlacement D_map_neo_ark_8017BEB0[];

extern AreaPlacement D_map_neo_ark_8017BEF0[];

extern AreaPlacement D_map_neo_ark_8017BF40[];

extern AreaPlacement D_map_neo_ark_8017BF90[];

extern AreaPlacement D_map_neo_ark_8017C020[];

extern AreaPlacement D_map_neo_ark_8017C050[];

extern AreaPlacement D_map_neo_ark_8017C070[];

extern AreaPlacement D_map_neo_ark_8017C090[];

extern AreaPlacement D_map_neo_ark_8017C0E0[];

extern AreaPlacement D_map_neo_ark_8017C0F0[];

extern AreaPlacement D_map_neo_ark_8017C140[];

extern AreaPlacement D_map_neo_ark_8017C1D0[];

extern AreaPlacement D_map_neo_ark_8017C210[];

extern AreaPlacement D_map_neo_ark_8017C270[];

extern AreaPlacement D_map_neo_ark_8017C2A0[];

extern AreaPlacement D_map_neo_ark_8017C2E0[];

extern AreaPlacement D_map_neo_ark_8017C320[];

extern AreaPlacement D_map_neo_ark_8017C350[];

extern AreaPlacement D_map_neo_ark_8017C380[];

extern AreaPlacement D_map_neo_ark_8017C3C0[];

extern AreaPlacement D_map_neo_ark_8017C3F0[];

extern AreaPlacement D_map_neo_ark_8017C420[];

extern AreaPlacement D_map_neo_ark_8017C450[];

extern AreaPlacement D_map_neo_ark_8017C4A0[];

extern AreaPlacement D_map_neo_ark_8017C4D0[];

extern AreaPlacement D_map_neo_ark_8017C510[];

extern AreaPlacement D_map_neo_ark_8017C530[];

extern AreaPlacement D_map_neo_ark_8017C550[];

extern AreaPlacement D_map_neo_ark_8017C580[];

extern AreaPlacement D_map_neo_ark_8017C620[];

extern AreaPlacement D_map_neo_ark_8017C690[];

extern AreaPlacement D_map_neo_ark_8017C720[];

extern AreaPlacement D_map_neo_ark_8017C760[];

/// This stage's `D_8010F9F4` and `D_8010FA0C` entries: items awarded
/// for battles won at a location, each list ended by a key of
/// `INVENTORY_BATTLE_REWARD_LIST_END`.
extern InventoryBattleReward D_map_neo_ark_8017C9B0[];

extern InventoryBattleReward D_map_neo_ark_8017CB0C[];

/// Resolves a destination's progress-dependent room variant in the Neo Ark stage.
///
/// `request` borrows a `RoomEventMsg` with a stage-5 `areaId` and `queryOnly` choice.
/// On `ROOM_EVENT_EXECUTE`, updates only `reply->room` for the observatory,
/// pavilion, altar, shrine and pyramid. Queries and other areas leave the reply
/// untouched. Room IDs are 1-based; the caller must initialize the reply's
/// remaining fields (room handlers copy the request first).
///
/// Both arguments may point to the same record. Neither pointer is retained;
/// game flags are only read. Always returns 1, without testing passage gates.
/// The `map_neo_ark` overlay must remain loaded for the call.
s32 mapNeoArkResolveRoomVariant(RoomEventMsg* request, RoomEventMsg* reply);

#endif // INCLUDE_MAPUI_MAP_NEO_ARK_H
