#ifndef INCLUDE_ROOMS_SHELTER_B2_OPERATING_ROOM_H
#define INCLUDE_ROOMS_SHELTER_B2_OPERATING_ROOM_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_shelter_b2_operating_room_80184124[12];

// shelter_b2_operating_room
extern u8* D_shelter_b2_operating_room_80180BC8[];

extern ViewCount D_shelter_b2_operating_room_80180BCC[];

extern DirectionWarpEntry D_shelter_b2_operating_room_80180BD0[];

extern WorldCollisionGrid D_shelter_b2_operating_room_80181364;

extern ViewCamera D_shelter_b2_operating_room_80181388[];

extern SpriteView D_shelter_b2_operating_room_80183184[];

extern WorldCoordRoomLights D_shelter_b2_operating_room_80183718;

extern WorldCollisionTrigger D_shelter_b2_operating_room_80183730[];

extern WorldCollisionOccluder D_shelter_b2_operating_room_80183A28[];

extern WorldCollisionTrigger D_shelter_b2_operating_room_80183ADC[];

extern WorldCoordRoomAmbientEntry D_shelter_b2_operating_room_80184184[];

extern WorldCollisionSurfaceProperties* D_shelter_b2_operating_room_801841F4[];

/// Dispatches the operating room's persistent task and message service.
///
/// `task->state` must be 0 (install handlers and publish the room task),
/// 1 (idle, retaining the message service), or 2 (release the task).
/// The room overlay must remain loaded while this callback can be dispatched.
void shelterB2OperatingRoomTask(Task* task);

/// Runs this room's animated spark toward its initial target position.
///
/// Requires live room-effect state, a counted coordinate-body task starting at
/// state zero, and owned `EffectWork` in `spawnArg2.pointer` with zero age and
/// frame index. `spawnArg1.pointer` borrows a composed target `GfxCoord` until
/// the first running tick; the task's own composed coordinate uses the same
/// view space. That tick fixes a step at 204/4096 of the initial separation,
/// narrowing to signed 16-bit components before scaling. Later running ticks
/// move by that step and draw on odd ages; age 20 releases the work and task.
/// Room-effect control 1..3 pauses; 4 or above cancels and releases both.
/// Keep the room overlay loaded through dispatch and do not retain freed work.
void shelterB2OperatingRoomRoomVisualEffectsFlyingSparkTask(Task* task);

/// Draws the operating room's fixed glows for the current mapped view.
///
/// State zero selects this room's glow-disc, flying-spark and orange-burst
/// effect IDs, then becomes state one. Every tick draws capsules and discs in
/// views 2..7; other views draw nothing. Requires the room overlay and view
/// mapping loaded, the current view matrix composed, and the frame's scratch
/// stack and GPU arena ready. Uses no task work or spawn payload; its owner
/// removes the task when leaving the room.
void shelterB2OperatingRoomDrawGlowsTask(Task* task);

/// Runs an attached two-tint charge disc with player-joint sparks and a release ring.
///
/// Start at state 0 with a coordinate body and counted, zeroed `EffectWork`
/// owned through `spawnArg2.pointer`; `spawnArg1.value` selects tint 0 or 1.
/// The work's parent coordinate stays borrowed until teardown; its stored
/// offset replaces the initial placement on attachment. Growing emits adopted
/// flying sparks from player parts 3..18 every fourth active age. The live
/// player rig and this room's installed flying-spark callback are required.
/// Its owner selects flicker, fading release or cancel through the
/// `ROOM_VISUAL_EFFECTS_GLOW_DISC_*` states. Room-effect control 1..3 pauses;
/// 4 or above cancels. Release/cancel frees work and adopted children. Keep the
/// room, ancestors, effect resources, scratch and GPU arena live until teardown.
void shelterB2OperatingRoomRoomVisualEffectsGlowDiscTask(Task* task);

/// Runs this room's orange burst with an expanding glow and fading ring.
///
/// Requires live room-effect state, a counted coordinate-body task starting at
/// state zero, and owned `EffectWork` in `spawnArg2.pointer`; `spawnArg1` is
/// ignored. Running ticks compose the coordinate, grow the disc and layered
/// glow, and fade the ring before fading the disc. Ground projection may add
/// a ground quad; the glow refreshes an orange point light even off screen.
/// Room-effect control 1..3 pauses; 4 or above cancels. Cancellation or finished
/// fading releases the work and task. Keep the room overlay loaded through
/// dispatch and do not retain freed work.
void shelterB2OperatingRoomRoomVisualEffectsFlyingOrangeBurstTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B2_OPERATING_ROOM_H
