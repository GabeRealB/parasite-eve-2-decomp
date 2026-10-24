#ifndef INCLUDE_STAGES_AKROPOLIS_ROOMS_PROMENADE_H
#define INCLUDE_STAGES_AKROPOLIS_ROOMS_PROMENADE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Promenade placement/resource pairs indexed directly by placement variant.
extern AreaVariant gAkropolisPromenadeAreaVariants[17];

/// Collision resources for promenade rooms 1 and 2, indexed by room minus one.
extern WorldCollisionRoomResources gAkropolisPromenadeRoomCollisionResources[2];

/// Logical-view maps for promenade rooms 1 and 2, indexed by room minus one.
extern u8* gAkropolisPromenadeViewMaps[2];

/// Logical-view counts for promenade rooms 1 and 2, indexed by room minus one.
extern ViewCount gAkropolisPromenadeViewCounts[2];

/// Lighting descriptors for promenade rooms 1 and 2, indexed by room minus one.
extern WorldCoordRoomLighting gAkropolisPromenadeRoomLightingTable[2];

/// Five promenade arrival/departure endpoints, indexed by warp ID minus one.
extern DirectionWarpEntry gAkropolisPromenadeWarpEntries[5];

/// Sprite resources for mapped promenade views 1..13, indexed by view minus one.
extern SpriteView gAkropolisPromenadeSpriteViews[13];

/// Cameras for mapped promenade views 1..13, indexed by camera index minus one.
extern ViewCamera gAkropolisPromenadeViewCameras[13];

/// Promenade collision and footstep policies, indexed by surface class 0..7.
extern WorldCollisionSurfaceProperties* gAkropolisPromenadeSurfacePropertiesByClass[8];

/// Emits the promenade's view-dependent lamps, ground glow and screen drips.
void akropolisPromenadeRoomEffectTask(Task* task);

/// Draws one frame of an animated lamp core and a rotating, flickering flare.
void akropolisPromenadeGlowStarTask(Task* task);

/// Smears a one-pixel framebuffer row down each frame to draw one screen drip.
void akropolisPromenadeScreenDripTask(Task* task);

/// Draws a flickering additive square in the coordinate body's local XZ plane.
void akropolisPromenadeGroundGlowTask(Task* task);

/// Draws one frame of a flickering additive lamp billboard and releases the effect.
void akropolisPromenadeGlowLampTask(Task* task);

/// Runs the promenade's room messages, arrival scene and battle follow-up.
void akropolisPromenadeRoomTask(Task* task);

#endif // INCLUDE_STAGES_AKROPOLIS_ROOMS_PROMENADE_H
