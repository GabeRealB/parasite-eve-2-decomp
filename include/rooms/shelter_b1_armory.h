#ifndef INCLUDE_ROOMS_SHELTER_B1_ARMORY_H
#define INCLUDE_ROOMS_SHELTER_B1_ARMORY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern TaskDesc D_shelter_b1_armory_801824D0;

extern AreaVariant D_shelter_b1_armory_801854E0[11];

/// Logical-view maps for the armory's single room, indexed by room ID minus one.
///
/// Room 1 borrows `gViewIdentityMap`: logical views 1..13 select the same
/// camera, image and sprite indices. The directory lives with this room overlay;
/// its shared byte map lives with gameplay. Neither level has a terminator.
extern u8* gShelterB1ArmoryRoomViewMaps[1];

extern ViewCount D_shelter_b1_armory_80182584[];

extern DirectionWarpEntry D_shelter_b1_armory_80182588[];

/// The armory's room-space collision mesh and 4-by-3 XZ cell index.
///
/// Cells have 4000-unit sides and an origin at (-1400, -4400). The mesh has
/// 55 quad faces, 111 vertices and 11 unit normals scaled by `ONE`.
/// Faces use room-local surface classes 0 and 1. Activation binds the mutable
/// descriptor to the current view; it and its pools stay live with this overlay.
extern WorldCollisionGrid gShelterB1ArmoryCollisionGrid;

extern ViewCamera D_shelter_b1_armory_80182EF4[];

/// Shelter b1 armory's sprite resources, indexed by mapped view minus one.
///
/// Views 1 and 9..13 have no sources; views 2..8 borrow sprite pools.
/// The room uses an identity logical-view map.
/// Every slot retains decoded image strips and has no clipping list. Empty
/// source slots retain batch addresses without supplying SpriteSource elements.
/// Descriptors and mutable source/batch lists stay borrowed while the owning
/// room overlay is loaded; selectors must be nonzero and within this array.
extern SpriteView gShelterB1ArmorySpriteViews[13];

/// The armory's 22 point lights for model shading and room-light queries.
///
/// All lights are enabled in every view; there are no directional or cone
/// lights. Gameplay updates their coordinate caches and attenuation in place.
/// The descriptor and its borrowed array remain valid while this overlay is loaded.
extern WorldCoordRoomLights gShelterB1ArmoryRoomLights;

extern WorldCollisionTrigger D_shelter_b1_armory_80184B14[];

extern WorldCollisionTrigger D_shelter_b1_armory_80184E0C[];

/// The armory's collision and footstep policies, indexed by surface class 0..7.
///
/// All classes block probes, allow weapon impacts and apply pushback. Class 1
/// supplies paired walking/running footsteps and uses the walking base for
/// scripted jumps; classes 0 and 2..7 share a record without footstep sounds.
/// The directory and its borrowed records remain live with this room overlay.
extern WorldCollisionSurfaceProperties* gShelterB1ArmorySurfacePropertiesByClass[8];

/// Dispatches the armory controller's initialization, idle tick and teardown.
///
/// The Mine/Shelter area's room-task table starts it in state 0. Requires state
/// 0..2: initialization publishes the room task and its message table, state 1
/// waits for messages, and state 2 kills the task. The index is unchecked.
/// Keep this overlay loaded while its controller or spawned events are live.
void shelterB1ArmoryRoomTask(Task* task);

/// Draws the armory's view-dependent beams and glows for the current frame.
///
/// The lock indicator is green when the armory is unlocked and red otherwise.
/// The task argument is unused; no task state or spawn arguments are read.
/// Requires this room overlay to be loaded, a valid current view mapping and
/// composed view matrices, plus an initialized scratch stack and enough space
/// in the current frame's GPU packet arena and depth ordering table.
void shelterB1ArmoryDrawGlowsTask(Task* unusedTask);

#endif // INCLUDE_ROOMS_SHELTER_B1_ARMORY_H
