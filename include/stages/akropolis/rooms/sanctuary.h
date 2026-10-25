#ifndef INCLUDE_STAGES_AKROPOLIS_ROOMS_SANCTUARY_H
#define INCLUDE_STAGES_AKROPOLIS_ROOMS_SANCTUARY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

/// Sanctuary layouts indexed by the saved placement variant (0..11).
extern AreaVariant gAkropolisSanctuaryAreaVariants[12];

extern TmdSource gAkropolisSanctuaryItemBoxModel;

extern TmdSource gAkropolisSanctuaryRedKeyPickupModel;

/// Collision resources for the sanctuary's single room.
extern WorldCollisionRoomResources gAkropolisSanctuaryRoomCollisionResources[1];

/// Logical-view maps for the sanctuary's single room.
extern u8* gAkropolisSanctuaryViewMaps[1];

/// Logical-view counts for the sanctuary's single room.
extern ViewCount gAkropolisSanctuaryViewCounts[1];

/// Lighting for the sanctuary's single room, with default ambient minima.
extern WorldCoordRoomLighting gAkropolisSanctuaryRoomLightingTable[1];

/// Sanctuary warp endpoints, indexed by the 1-based warp ID minus one.
extern DirectionWarpEntry gAkropolisSanctuaryWarpEntries[3];

/// Sprite resources for sanctuary views 1..16, indexed by mapped view minus one.
extern SpriteView gAkropolisSanctuarySpriteViews[16];

/// Cameras for sanctuary views 1..16, indexed by mapped view minus one.
extern ViewCamera gAkropolisSanctuaryViewCameras[16];

/// Sanctuary collision policies indexed by surface class 0..7.
extern WorldCollisionSurfaceProperties* gAkropolisSanctuarySurfacePropertiesByClass[8];

/// Creates the sanctuary's twelve flames and maintains the mosaic cleanup latch.
void akropolisSanctuaryRoomEffectTask(Task* task);

/// Spawns the mosaic's 72 tiles and a second copy of 16 selected tiles.
void akropolisSanctuaryMosaicTask(Task* task);

/// Draws and advances one intact mosaic tile, spawning shards when it breaks.
void akropolisSanctuaryMosaicTileTask(Task* task);

/// Draws and advances a scaled mosaic shard that can bounce or split again.
void akropolisSanctuaryMosaicShardTask(Task* task);

/// Draws the sanctuary's view-gated, flickering additive flame billboard.
void akropolisSanctuaryFlameTask(Task* task);

/// Dispatches the sanctuary room's setup, per-frame update and exit states.
void akropolisSanctuaryRoomTask(Task* task);

/// Updates the sanctuary placed item's model visibility from its saved state and view.
void akropolisSanctuaryItemVisibilityTask(Task* task);

#endif // INCLUDE_STAGES_AKROPOLIS_ROOMS_SANCTUARY_H
