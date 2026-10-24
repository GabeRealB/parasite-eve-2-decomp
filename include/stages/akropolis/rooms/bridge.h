#ifndef INCLUDE_STAGES_AKROPOLIS_ROOMS_BRIDGE_H
#define INCLUDE_STAGES_AKROPOLIS_ROOMS_BRIDGE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Extents of the bridge's room directory, mapped views and warp endpoints.
enum {
    AKROPOLIS_BRIDGE_ROOM_COUNT = 2,
    AKROPOLIS_BRIDGE_VIEW_COUNT = 10,
    AKROPOLIS_BRIDGE_WARP_COUNT = 2
};

/// Per-step surface classes for the bridge's indexed five-step stairs.
extern u8 gAkropolisBridgeStairSurfaceRows[2][12];

/// Placement/resource pairs indexed by the bridge's saved placement variant.
extern AreaVariant gAkropolisBridgePlacementVariants[12];

/// Collision resources for the bridge's two rooms, indexed by room - 1.
extern WorldCollisionRoomResources gAkropolisBridgeRoomCollisionResources[AKROPOLIS_BRIDGE_ROOM_COUNT];

/// Logical-to-mapped view indices for the bridge's two rooms.
extern u8* gAkropolisBridgeViewMaps[2];

/// Logical-view counts for the bridge's two rooms, indexed by room - 1.
extern ViewCount gAkropolisBridgeViewCounts[AKROPOLIS_BRIDGE_ROOM_COUNT];

/// Lighting descriptors for the bridge's two rooms, indexed by room - 1.
extern WorldCoordRoomLighting gAkropolisBridgeRoomLightingTable[2];

/// Arrival/departure endpoints for the promenade and fire escape.
extern DirectionWarpEntry gAkropolisBridgeWarpEntries[AKROPOLIS_BRIDGE_WARP_COUNT];

/// Sprite resources for the bridge's ten mapped views, indexed by mapped view - 1.
extern SpriteView gAkropolisBridgeSpriteViews[AKROPOLIS_BRIDGE_VIEW_COUNT];

/// Camera transforms and projection distances for the bridge's ten mapped views.
extern ViewCamera gAkropolisBridgeViewCameras[AKROPOLIS_BRIDGE_VIEW_COUNT];

/// Collision and footstep policies for the bridge's surface classes 0..7.
extern WorldCollisionSurfaceProperties* gAkropolisBridgeSurfacePropertiesByClass[8];

/// Emits the bridge's view-dependent glows, player-joint water effects and framebuffer streaks.
void akropolisBridgeAmbientEffectsTask(Task* task);

/// Draws one frame of an animated lamp core and a rotating, flickering flare.
void akropolisBridgeGlowStarTask(Task* task);

/// Draws the bridge's one-frame ground glow and releases its effect work.
void akropolisBridgeGroundGlowTask(Task* task);

/// Draws one frame of a flickering additive lamp billboard and releases the effect.
void akropolisBridgeGlowLampTask(Task* task);

/// Draws a falling framebuffer strip starting on a random row 96..239.
void akropolisBridgeFallingStreakTask(Task* task);

/// Draws a falling framebuffer strip starting on rows 72..239, with a near squared-depth ramp.
void akropolisBridgeMidDustStreakTask(Task* task);

/// Draws a falling framebuffer strip starting on rows 104..239, with a low linear-depth ramp.
void akropolisBridgeLowDustStreakTask(Task* task);

/// Draws a falling framebuffer strip starting on rows 72..239, with a farther squared-depth ramp.
void akropolisBridgeTallDustStreakTask(Task* task);

/// Draws a falling framebuffer strip starting on a random row 104..231.
void akropolisBridgeParticleStreakTask(Task* task);

/// Expands and fades a water-surface ripple using the task's composed coordinate.
void akropolisBridgeWaterRippleTask(Task* task);

/// Animates one eight-cell debris sprite used for the bridge's water spray.
void akropolisBridgeEffectSpriteDebrisTask(Task* task);

/// Moves and draws one blue-green 1x1 dust mote at the task coordinate.
void akropolisBridgeDustMoteTask(Task* task);

/// Runs the bridge keypad's entry, Examine prompt, code check and result display.
void akropolisBridgeKeypadTask(Task* task);

/// Runs the bridge room's setup, message-waiting idle state or teardown.
void akropolisBridgeRoomTask(Task* task);

#endif // INCLUDE_STAGES_AKROPOLIS_ROOMS_BRIDGE_H
