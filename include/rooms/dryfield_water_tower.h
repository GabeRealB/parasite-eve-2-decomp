#ifndef INCLUDE_ROOMS_DRYFIELD_WATER_TOWER_H
#define INCLUDE_ROOMS_DRYFIELD_WATER_TOWER_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Commands broadcast to the room's Desert Chasers with `ACTOR_COMMAND_MESSAGE_APPLY`.
///
/// `ActorCommand.context` names the Dryfield Water Tower stage and area.
/// Commands are stored in a halfword by the sender and a byte by the actors.
enum {
    DRYFIELD_WATER_TOWER_CHASER_COMMAND_START_BATTLE       = 0, // Restore health and release the first placed chaser from its scripted animation
    DRYFIELD_WATER_TOWER_CHASER_COMMAND_START_RUN          = 1, // Restore health and stage the remaining chasers for the timed mechanism run
    DRYFIELD_WATER_TOWER_CHASER_COMMAND_START_FINAL_BATTLE = 2, // Place the remaining chasers in the arena; report a room event when none remain
    DRYFIELD_WATER_TOWER_CHASER_COMMAND_END_RUN            = 3, // Release a held player and send eligible chasers into their retreat state
    DRYFIELD_WATER_TOWER_CHASER_COMMAND_HIDE               = 9, // Hide both chasers while the mechanism's opening script runs
};

extern u16 D_dryfield_water_tower_801876A8;

extern u16 D_dryfield_water_tower_801876AA;

extern AreaVariant D_dryfield_water_tower_8018757C[13];

// dryfield_water_tower
extern WorldCollisionRoomResources D_dryfield_water_tower_801827CC[];

/// Logical-view map directory for the water tower's sole room.
///
/// Room 1 borrows `gViewIdentityMap`, indexed by logical view minus one.
/// Reverse-search counts and camera, image and sprite extents are independent.
/// The directory requires the room overlay loaded; the map requires gameplay.
extern u8* gDryfieldWaterTowerRoomViewMaps[1];

/// Reverse-lookup length for the water tower's sole room, indexed by room minus one.
///
/// Searches 21 logical entries of `gViewIdentityMap`. This count is independent
/// of the 20 camera records and 21 sprite/background slots. The room overlay
/// owns the array; its map-directory import requires this overlay loaded.
extern ViewCount gDryfieldWaterTowerRoomViewCounts[1];

/// Authored model lighting for the water tower's sole room, indexed by room minus one.
///
/// Four directional and five point lights contribute in every view; there are
/// no cone lights. NULL ambient minima select the gameplay defaults. The room
/// overlay owns the descriptor and writable light arrays throughout their use.
extern WorldCoordRoomLighting gDryfieldWaterTowerRoomLighting[1];

extern DirectionWarpEntry D_dryfield_water_tower_801827EC[];

/// Cameras for mapped water-tower views 1..20, indexed by view minus one.
///
/// World-to-view rotations use Q12; translations are negated room-coordinate
/// camera origins. Perspective distances are pixels.
/// The room overlay owns the records. View 21 has a background and sprite slot
/// but no camera record; it must not be selected for camera application.
extern ViewCamera gDryfieldWaterTowerViewCameras[20];

/// Foreground sources, batches and clip lists for mapped water-tower views 1..21.
///
/// Indexed by view minus one, with no terminator. Empty foreground lists retain
/// the decoded background; every batch list ends with SPRITE_BATCH_END.
/// View 19's single mechanism sprite changes visibility with the mechanism state.
/// The room owns this array and its writable lists while the overlay is loaded.
/// The retained view-21 slot does not supply a camera.
extern SpriteView gDryfieldWaterTowerSpriteViews[21];

/// Collision policies and footstep cues indexed by the water tower's surface class (0..7).
///
/// Class 1 passes probes, disables weapon impacts and has no footstep cues.
/// Other classes block probes, allow weapon impacts and borrow the same cues.
/// Every class applies pushback. The directory and records require this room loaded.
extern WorldCollisionSurfaceProperties* gDryfieldWaterTowerSurfacePropertiesByClass[8];

/// Updates the room's ambient-effect gate for the current mapped view each frame.
///
/// Room-effect task 0xD2 ignores its task argument. The water-tower overlay and
/// current view mapping must be live, with a mapped view index in 1..21.
/// The per-view table supplies 2 for enabled effects and 0 for disabled effects.
void dryfieldWaterTowerUpdateViewEffectGateTask(Task* unused);

/// Runs the daytime water-tower room task for one update.
///
/// State 0 registers the message receiver and starts its prop-scene driver,
/// 1 idles and 2 releases the task. The state index is unchecked. Requires a
/// live task and the room overlay loaded throughout its lifetime.
void dryfieldWaterTowerRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_WATER_TOWER_H
