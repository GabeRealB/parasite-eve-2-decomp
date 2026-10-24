#ifndef INCLUDE_STAGES_AKROPOLIS_ROOMS_FOUNTAIN_H
#define INCLUDE_STAGES_AKROPOLIS_ROOMS_FOUNTAIN_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Resource-directory extents exported by the fountain overlay.
enum {
    AKROPOLIS_FOUNTAIN_ROOM_COUNT          = 2,
    AKROPOLIS_FOUNTAIN_SURFACE_CLASS_COUNT = 8,
    AKROPOLIS_FOUNTAIN_WARP_COUNT          = 5,
    AKROPOLIS_FOUNTAIN_SPRITE_VIEW_COUNT   = 22,
    AKROPOLIS_FOUNTAIN_CAMERA_COUNT        = 21
};

/// Actor layout variants of the fountain.
extern AreaVariant gAkropolisFountainAreaVariants[11];

/// Collision resources for fountain rooms 1 and 2, indexed by room - 1.
extern WorldCollisionRoomResources gAkropolisFountainRoomCollisionResources[AKROPOLIS_FOUNTAIN_ROOM_COUNT];

/// Logical-view maps for the fountain's two room indices, selected at room - 1.
extern u8* gAkropolisFountainViewMaps[AKROPOLIS_FOUNTAIN_ROOM_COUNT];

/// Signed logical-view search counts for rooms 1 and 2, indexed by room - 1.
extern ViewCount gAkropolisFountainViewCounts[AKROPOLIS_FOUNTAIN_ROOM_COUNT];

/// Lighting descriptors for both fountain room indices, selected at room - 1.
extern WorldCoordRoomLighting gAkropolisFountainRoomLightingTable[AKROPOLIS_FOUNTAIN_ROOM_COUNT];

/// Five fountain arrival/departure endpoints, indexed by warp ID 1..5 minus one.
extern DirectionWarpEntry gAkropolisFountainWarpEntries[AKROPOLIS_FOUNTAIN_WARP_COUNT];

/// Sprite resources for mapped indices 1..22, indexed by mapped view - 1.
extern SpriteView gAkropolisFountainSpriteViews[AKROPOLIS_FOUNTAIN_SPRITE_VIEW_COUNT];

/// Cameras for mapped indices 1..21, indexed by mapped camera - 1.
extern ViewCamera gAkropolisFountainViewCameras[AKROPOLIS_FOUNTAIN_CAMERA_COUNT];

/// Collision policies and footstep cues indexed by fountain surface class 0..7.
extern WorldCollisionSurfaceProperties* gAkropolisFountainSurfacePropertiesByClass[AKROPOLIS_FOUNTAIN_SURFACE_CLASS_COUNT];

/// Draws the fountain spray as a flickering, semitransparent textured quad.
void akropolisFountainSprayTask(Task* task);

/// Queues the player's fountain climb from the direction-action dispatch.
void akropolisFountainStartClimb(s32 unusedActionId, s32 unusedControl);

/// Spawns the fountain spray and replaces its movie task when the mapped view changes.
void akropolisFountainViewEffectsTask(Task* task);

/// Runs the player's turn, one-step ascent and walk to the room's fixed destination.
void akropolisFountainClimbTask(Task* task);

/// Runs the fountain's room-message receiver and restores its climb interaction.
void akropolisFountainRoomTask(Task* task);

#endif // INCLUDE_STAGES_AKROPOLIS_ROOMS_FOUNTAIN_H
