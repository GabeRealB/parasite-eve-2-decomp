#ifndef INCLUDE_ROOMS_ACROPOLIS_WEST_ELEVATOR_HALL_H
#define INCLUDE_ROOMS_ACROPOLIS_WEST_ELEVATOR_HALL_H

#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

// acropolis_west_elevator_hall
extern WorldCollisionRoomResources D_acropolis_west_elevator_hall_80185024[];

extern u8* D_acropolis_west_elevator_hall_80185034[];

extern ViewCount D_acropolis_west_elevator_hall_80185038[];

extern WorldCoordRoomLighting D_acropolis_west_elevator_hall_8018503C[];

extern DirectionWarpEntry D_acropolis_west_elevator_hall_80185044[];

extern SpriteView D_acropolis_west_elevator_hall_80186408[];

extern ViewCamera D_acropolis_west_elevator_hall_801869FC[];

extern WorldCollisionSurfaceProperties* D_acropolis_west_elevator_hall_80186AC4[];

/// Draws one frame of a pulsing additive red diamond, then ends the counted effect.
///
/// Bank-6 slot 0x1F requires a live coordinate body and owned `EffectWork` in
/// `spawnArg2.pointer`, distinct from `Task::work`. The low two bytes of
/// `spawnArg1` are unsigned pulse rate and size (0..255); its high half is ignored.
/// The frame counter times the rate wraps to an 8-bit phase folded into red
/// levels 0..128. Half extent is `size * 512 / depth` screen pixels.
///
/// Projects the composed translation through `GsWSMATRIX`, narrowing each
/// component to signed 16-bit game units. Depth is SZ3 / 4 (0..16383); values
/// below 17 queue nothing, and GTE FLAG is not tested. Requires initialized GTE
/// projection settings, one aligned `RoomGlowSpriteScratch` block and arena
/// room for two `POLY_G4`/`DR_TPAGE` pairs at accepted depth. The scaled depth
/// wraps into the current 1024-tag ordering table; packets live through GPU completion.
/// Scratch and effect work are released on every call. Keep this overlay loaded
/// through the callback; the room respawns its three beacons while view 2 is active.
void acropolisWestElevatorHallRedBeaconTask(Task* task);

/// Distorts an 82-row, 120-pixel strip of the drawing framebuffer for one frame.
///
/// Copies rows at (80, 80) in the selected buffer, whose VRAM Y stride is 272.
/// A cosine selects each source X; alternate 128-frame intervals vary its
/// divisor randomly per row. Reserves 82 DR_MOVE packets, all in ordering-table
/// element 0x72. Requires a current table and enough packet-arena space.
/// Releases the counted EffectWork in `spawnArg2.pointer` and kills the task
/// after queuing the copies; packets remain live through GPU completion.
void acropolisWestElevatorHallScanlineDistortionTask(Task* task);

/// Draws one frame of the hall's additive light billboard, then retires its effect.
///
/// Requires a live coordinate body, a counted EffectWork in `spawnArg2.pointer`,
/// the current view matrix, and room for one POLY_FT4 and RoomGlowSpriteScratch.
/// The composed position narrows to signed 16-bit view coordinates before projection.
/// Depths below 17 (camera Z / 4) reserve a packet but do not queue it; other
/// depths give a half-extent of 0x6700 / depth pixels. The scratch block is
/// released before effect teardown. Queued storage lives through GPU completion.
void acropolisWestElevatorHallLightGlowTask(Task* task);

/// Lights the elevator bay's palette over two updates and holds it in view 5.
///
/// `spawnArg2.pointer` owns a counted EffectWork initialized with zero scale
/// and angle. Here scale is the Q12 lit-palette weight (0..4096), and angle is
/// the latch (0 ramping, 1 holding). Each ramp step adds 2048 and uploads all
/// 256 colours. Leaving view 5 restores the unlit palette before freeing the
/// work and killing the task, including when a ramp upload happened that tick.
void acropolisWestElevatorHallBayLightingTask(Task* task);

/// Runs this room's player-model floor or mirror-plane reflection.
///
/// Starts bodyless in state 0 with a live player TMD; `spawnArg1.value` selects
/// floor (0) or room-plane (1). Initialization owns a cloned model and mirror
/// work, parents the task to the player, spawns attachment reflections and
/// performs the first update. State 1 follows the player's pose, lighting and
/// reflected frame. Requires this room overlay to remain loaded; dispatch
/// accepts only states 0 and 1 and does not check bounds.
void acropolisWestElevatorHallPlayerReflectionTask(Task* reflectionTask);

/// Registers the hall's room effects and emits lights for the current view.
///
/// Starts with a coordinate body in state 0, registers the room-effect message
/// table and spawns two reflection tasks. State 1 emits three red beacons each
/// frame in view 2 or a light glow in view 5. Positions use the parent coordinate
/// frame and are copied by the spawner. Keep the hall overlay, coordinate and
/// resources live while this task and its effects run; only states 0/1 act.
void acropolisWestElevatorHallViewEffectsTask(Task* task);

/// Runs the west elevator hall's room messages, door leaves and arrival event.
///
/// Starts bodyless in state 0: installs the room message table, registers the
/// room task and spawns two model-backed door leaves. State 1 maintains the
/// one-shot warp-1 arrival event; state 2 kills the room task. The state index
/// is unchecked and must be 0..2. Keep this overlay and its resources loaded
/// while the room task and separately spawned leaves remain live.
void acropolisWestElevatorHallRoomTask(Task* task);

#endif // INCLUDE_ROOMS_ACROPOLIS_WEST_ELEVATOR_HALL_H
