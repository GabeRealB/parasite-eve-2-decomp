#ifndef GAMEPLAY_PRIVATE_ITEM_USE_H
#define GAMEPLAY_PRIVATE_ITEM_USE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/battle_reward.h"
#include "collision.h"
#include "gameplay/room.h"

#include "main/mc_types.h"
#include "main/task_types.h"

/// Default ambient-light entry returned by `_worldCoordGetRoomAmbientEntry`
/// when no view entry is available, and copied by `_worldCoordCopyDefaultRoomAmbient`.
extern WorldCoordRoomAmbientEntry Gp_RoomBoundDefault;

/// Default `MATRIX` installed at `TmdObject.lightMtx` by `Gp_BindDefaultMtx`.
extern MATRIX Gp_DefaultMtx;

/// Default `MATRIX` installed at `TmdObject.colorMtx` by `Gp_BindDefaultMtx`.
extern MATRIX Gp_DefaultMtx2;

/// Light/color `MATRIX` pair `Gp_DebugPanTask` installs at
/// `TmdObject.lightMtx` / `colorMtx` for the `gPlayerActorTasks[PLAYER_ACTOR_TASK_COMPANION]` actor and
/// its `field_918` / `field_920` child tasks (the second actor uses its own
/// pair instead of `Gp_DefaultMtx` / `Gp_DefaultMtx2`).
extern MATRIX D_80114ED8;

extern MATRIX D_80114EF8;

/// Flag set by `worldCoordSetAmbientColorOverride` when an override SVECTOR is stored at
/// `Gp_OverrideVec`. Cleared when that function is called with NULL, and
/// also by `Gp_BindDefaultMtx`.
extern u8 Gp_OverrideVecFlag;

/// Override SVECTOR copied by `worldCoordSetAmbientColorOverride` from its argument.
extern SVECTOR Gp_OverrideVec;

/// 8.8 fixed-point pair lerped toward projected screen coords by
/// `Gp_DrawTargetCursor`. Reset to `0xFFF00000` by `Gp_ResetLinkState`.
extern s32 D_8010F9EC;

extern s32 D_8010F9F0;

/// Per-stage `InventoryBattleReward` lists selected by
/// `Gp_GrantLocationItems` when
/// `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode` is 0 or 2. Indexed by
/// `GameSession.location.loc.stage`.
extern InventoryBattleReward* D_8010F9F4[];

/// Per-stage `InventoryBattleReward` lists selected by
/// `Gp_GrantLocationItems` when
/// `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode` is not 0 or 2. Indexed by
/// `GameSession.location.loc.stage`.
extern InventoryBattleReward* D_8010FA0C[];

/// Face edge endpoint pairs walked by the grid collision helpers
/// (`Gp_CollideObjGrid` / `Gp_CollideObjGridDir` / `worldCollisionIntersectGridFace` / `worldCollisionTestOccluderSegment`).
extern WorldCollisionFaceEdge Gp_FaceEdgePairs[5];

extern const char D_8009745C[];

extern const char Gp_StrGetLockPosNull[];

void func_800D6334(Task* arg0);

/// `arg1` is passed by `Gp_PlayerNormalState5` (the actor's `field_960`) but the body
/// ignores it.
s32 Gp_FlushPendingRelated(s32 arg0, s32 arg1);

InventoryItemRow* Gp_FindItemById(s32 arg0);

InventoryItemRow* Gp_FindItemInScan(s32 arg0, InventoryItemRange* arg1);

void Gp_DrawWeaponLabel(Task* arg0);

#endif // GAMEPLAY_PRIVATE_ITEM_USE_H
