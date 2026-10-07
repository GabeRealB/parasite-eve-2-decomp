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

/// Default `MATRIX` installed at `TmdObject.lightMtx` by `_worldCoordInitPlayerLighting`.
extern MATRIX Gp_DefaultMtx;

/// Default `MATRIX` installed at `TmdObject.colorMtx` by `_worldCoordInitPlayerLighting`.
extern MATRIX Gp_DefaultMtx2;

/// Light/color `MATRIX` pair `_worldCoordUpdatePlayerLighting` installs at
/// `TmdObject.lightMtx` / `colorMtx` for the `gPlayerActorTasks[PLAYER_ACTOR_TASK_COMPANION]` actor and
/// its `GameActor::attachmentTasks` / `equipmentTasks` child models (the second actor uses its own
/// pair instead of `Gp_DefaultMtx` / `Gp_DefaultMtx2`).
extern MATRIX D_80114ED8;

extern MATRIX D_80114EF8;

/// Flag set by `worldCoordSetAmbientColorOverride` when an override SVECTOR is stored at
/// `Gp_OverrideVec`. Cleared when that function is called with NULL, and
/// also by `_worldCoordInitPlayerLighting`.
extern u8 Gp_OverrideVecFlag;

/// Override SVECTOR copied by `worldCoordSetAmbientColorOverride` from its argument.
extern SVECTOR Gp_OverrideVec;

/// 8.8 fixed-point pair lerped toward projected screen coords by
/// `worldTargetDrawOverlay`. Reset to `0xFFF00000` by `worldTargetResetAreaTracking`.
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
/// (`worldCollisionCollideSphereGrid` / `worldCollisionCollideMotionSphereGrid` / `worldCollisionIntersectGridFace` / `worldCollisionTestOccluderSegment`).
extern WorldCollisionFaceEdge Gp_FaceEdgePairs[5];

extern const char D_8009745C[];

extern const char Gp_StrGetLockPosNull[];

void func_800D6334(Task* arg0);

/// `arg1` is passed by `Gp_PlayerNormalState5` (the actor's `field_960`) but the body
/// ignores it.
s32 Gp_FlushPendingRelated(s32 arg0, s32 arg1);

/// Borrows the last carried inventory row with this exact item id.
///
/// Returns `NULL` if no row matches, including an empty carried range.
/// Item ids are compared without narrowing; 0 selects the last free row.
/// Quantity and attachment state are ignored. The live carried range must fit
/// its selected table. Sorting, transfers or replacing the live save can
/// change the item at the returned writable address.
InventoryItemRow* inventoryFindLastCarriedItemRow(s32 itemId);

/// Borrows the last inventory row with this exact item id in `range`.
///
/// Returns `NULL` if no row matches, including a zero-row range. Item ids are
/// compared without narrowing; 0 selects the last free row. Quantity and
/// attachment state are ignored. `range` must be readable and its first row
/// and row count must fit the selected table, which must remain available.
/// The writable result borrows that table; sorting and transfers can change
/// the item at the same address. The descriptor and rows are left intact.
InventoryItemRow* inventoryFindLastItemRowInRange(s32 itemId, const InventoryItemRange* range);

void Gp_DrawWeaponLabel(Task* arg0);

#endif // GAMEPLAY_PRIVATE_ITEM_USE_H
