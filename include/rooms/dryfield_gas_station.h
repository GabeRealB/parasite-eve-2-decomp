#ifndef INCLUDE_ROOMS_DRYFIELD_GAS_STATION_H
#define INCLUDE_ROOMS_DRYFIELD_GAS_STATION_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_gas_station_80184A38[12];

// dryfield_gas_station
extern WorldCollisionRoomResources D_dryfield_gas_station_8018314C[];

/// The gas station's one-room directory of logical-to-resource view maps.
///
/// Room 1 borrows `gViewIdentityMap`: logical views 1..14 select the same
/// camera, image and sprite indices. Index the directory by room minus one
/// and the map by view minus one. Keep this room overlay loaded while using
/// the directory; the shared map lives with gameplay and has no terminator.
extern u8* gDryfieldGasStationRoomViewMaps[1];

/// The gas station's one-room lighting directory, indexed by room minus one.
///
/// Room 1 supplies two directional lights and uses the default ambient
/// minima, with no point or cone lights. The descriptor and mutable light
/// coordinates stay valid only while this room overlay is loaded.
extern WorldCoordRoomLighting gDryfieldGasStationRoomLighting[1];

extern ViewCount D_dryfield_gas_station_80183168[];

extern DirectionWarpEntry D_dryfield_gas_station_8018316C[];

extern ViewCamera D_dryfield_gas_station_80183EC8[];

extern SpriteView D_dryfield_gas_station_801842A8[];

/// The gas station's eight collision and footstep policies, indexed by surface class.
///
/// Classes 0 and 5..7 block probes, allow weapon impacts and have no footstep
/// cues. Class 1 passes probes, ignores weapon impacts and has no cues;
/// classes 2..4 block probes, allow impacts and select distinct footstep cues.
/// All classes apply pushback. Classes 0 and 5..7 share one policy record.
/// Keep this overlay loaded while borrowing the directory or its records.
extern WorldCollisionSurfaceProperties* gDryfieldGasStationSurfacePropertiesByClass[8];

/// Draws the gas station's pulsing cyan glow and permits footstep dust.
///
/// Bank-6 effect 0xC2 requires a live `TASK_BODY_COORD` task and the gas-station
/// overlay. The current logical view must be 1..14: views 4, 6, 11 and 12 use
/// a diamond with crossing rays; the others use a disc with four rays.
/// Both shapes pulse at 96 angle units per animation frame (4096 per turn),
/// with a nominal radius of 4096 / (camera Z / 4) pixels. Depth below 17 emits
/// no glow, but still permits dust.
///
/// Requires live `gRoomEffectState`, initialized view matrices and scratch
/// stack, and a current frame arena/ordering table with room for up to twenty
/// additive quads and their blend commands. Borrows the task's coordinate for
/// the call; the drawers update its composed transform. Allocates no task work.
void dryfieldGasStationCyanGlowTask(Task* task);

/// Updates the gas-station telephone's save menu and optional statistics panels.
///
/// `spawnArg2.pointer` borrows the UI object owned by the menu task. Its save,
/// notice and statistics children must stay linked until their answers are
/// consumed. Requires this overlay and its telephone resources throughout use.
void dryfieldGasStationTelephoneMenuTask(Task* task);

/// Runs the gas station's room receiver through entry setup, idle and teardown.
///
/// Requires a live bodyless task with state 0 (entry), 1 (idle) or 2 (kill).
/// Entry registers `GAME_TASK_SLOT_ROOM`, starts the arrival scene or area music,
/// and permits post-CAP sound messages. Keep the gas-station overlay loaded
/// for the task's lifetime.
void dryfieldGasStationRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_GAS_STATION_H
