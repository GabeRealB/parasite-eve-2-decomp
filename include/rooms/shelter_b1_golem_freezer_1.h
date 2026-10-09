#ifndef INCLUDE_ROOMS_SHELTER_B1_GOLEM_FREEZER_1_H
#define INCLUDE_ROOMS_SHELTER_B1_GOLEM_FREEZER_1_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Actor layouts indexed directly by the saved placement variant (0..22).
///
/// Variant 1 has empty lists; variant 21 places Pierce Carradine for the meeting.
/// All other slots are absent. Borrowed tables require this room overlay loaded.
extern AreaVariant gShelterB1GolemFreezer1AreaVariants[23];

/// Logical-view map for room 1, borrowing gameplay's identity map for views 1..7.
///
/// Indexed by room - 1, then view - 1; the directory has no terminator.
/// Both gameplay's map bytes and this room's directory must remain loaded.
extern u8* gShelterB1GolemFreezer1ViewMaps[1];

/// Reverse-lookup count for room 1's seven logical views, followed by a retained zero.
///
/// The map publishes only room 1; the second halfword is not a list terminator.
extern ViewCount gShelterB1GolemFreezer1ViewCounts[2];

extern DirectionWarpEntry D_shelter_b1_golem_freezer_1_8017E798[];

/// Mutable room collision mesh on a two-by-one XZ grid of 4000-unit cells.
///
/// Owns nine normals, 28 vertices and twelve faces through private room pools.
/// The first face, normal and four vertices are rebuilt for the meeting actor;
/// other initially empty leading entries and the cell lists retain their authored values.
/// Setup borrows the current view coordinate; keep this overlay loaded while active.
extern WorldCollisionGrid gShelterB1GolemFreezer1RoomCollision;

/// Cameras indexed by mapped view - 1 for the room's seven identity-mapped views.
///
/// Rotation uses Q12, negated camera origins use game units, and projection
/// distances use pixels. Queued camera application borrows this loaded overlay.
extern ViewCamera gShelterB1GolemFreezer1ViewCameras[7];

/// Background sprite resources indexed by mapped view - 1 (views 1..7).
///
/// View 2 adds thirty depth-sorted sprites; every view also draws image strips.
/// Other views retain empty batch lists. No view supplies clipping areas.
/// Descriptors and their mutable lists remain borrowed from the loaded room.
extern SpriteView gShelterB1GolemFreezer1SpriteViews[7];

/// The room's one point light, contributing in every view, with no directional or cone lights.
///
/// Coordinates and attenuation in its private array remain writable while active;
/// the collection is borrowed only while this room overlay is loaded.
extern WorldCoordRoomLights gShelterB1GolemFreezer1RoomLights;

/// Directed camera boundaries between views 2/3 and 2/5, with LAST on the final record.
///
/// Room setup binds, links and enables all four mutable quads. Keep this overlay
/// loaded until they are unlinked; source and destination views are 1-based.
extern WorldCollisionTrigger gShelterB1GolemFreezer1ViewBoundaryTriggers[4];

/// Five interaction quads: an access-tunnel warp, three CAP commands and Pierce's talk action.
///
/// CAP indices are 2, 1 and 7; room action 1 starts the meeting in variant 21.
/// The warp uses departure slot 1 and arrival slot 2; LAST includes the final record.
/// Room setup binds and links the records; keep this overlay loaded until unlinked.
extern WorldCollisionTrigger gShelterB1GolemFreezer1ActionTriggers[5];

/// Minimum ambient RGB for logical views 1..7, with the view count at entry zero.
///
/// Colour levels use 16 units per 8-bit intensity; luma is retained authored data.
/// Lighting borrows this table from the loaded room overlay.
extern WorldCoordRoomAmbientEntry gShelterB1GolemFreezer1AmbientByView[8];

/// Collision policy indexed by room-local surface class (0..7), with footsteps on class 1.
///
/// All classes block probes, permit weapon impacts and apply pushback.
/// Only the class-1 floor faces supply footstep cues; the remaining classes share
/// one record without cues. Records remain borrowed from the loaded room overlay.
extern WorldCollisionSurfaceProperties* gShelterB1GolemFreezer1SurfacePropertiesByClass[8];

/// Runs the freezer room task's setup, message-waiting or teardown state.
///
/// Requires loaded room resources and a valid state (0 setup, 1 idle, 2 teardown).
/// Setup publishes the room receiver and prepares the meeting actor's obstacle.
/// The map overlay spawns it without a body or work block.
void shelterB1GolemFreezer1RoomTask(Task* task);

/// Animates and drifts one ten-frame floor-mist puff, then releases its counted effect.
///
/// Requires a coordinate body, task state zero, and an owned, counted
/// `EffectWork` with frame index zero in `spawnArg2.pointer`, as supplied by
/// `effectSpawn`. `spawnArg1.value` packs
/// size in bits 0..11, ticks per animation cell in bits 12..14 (1 if bits
/// 12..15 are all zero), and drift speed in bits 16..23 (64 if zero), in local
/// coordinate units per task tick. Bit 15 alone would decode a zero period;
/// callers must supply a nonzero period whenever any of bits 12..15 is set.
///
/// Chooses a fixed random screen rotation and local-XZ drift on the first tick.
/// Draws before moving; cell zero advances on that tick, and later cells last
/// the requested period. The composed coordinate must be refreshed between
/// ticks. The work allocation and task retire after cell nine; cancellation
/// uses the effect spawner's exit callback. Keep this room overlay loaded for
/// the effect's lifetime.
void shelterB1GolemFreezer1FloorMistTask(Task* task);

/// Emits floor mist every fourth animation frame and draws the current view's glows.
///
/// Requires the freezer room resources, view and effect controller to be live.
/// Emits one puff at each of nine floor origins, with radius 64..1024 in steps
/// of 64 and Y in -255..0 game units. Each puff snapshots its position during
/// spawning; its size is 1024..1279, cell period five or six ticks and drift
/// speed eight units per tick. Glows are drawn in mapped views 2..5.
/// The task and spawn arguments are unused; room teardown owns its lifetime.
void shelterB1GolemFreezer1AmbientEffectsTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B1_GOLEM_FREEZER_1_H
