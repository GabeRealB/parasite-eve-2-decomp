#ifndef INCLUDE_ROOMS_DRYFIELD_BREEZEWAY_H
#define INCLUDE_ROOMS_DRYFIELD_BREEZEWAY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_breezeway_801842F8[13];

// dryfield_breezeway
extern WorldCollisionRoomResources D_dryfield_breezeway_8018316C[];

extern u8* D_dryfield_breezeway_8018317C[];

extern WorldCoordRoomLighting D_dryfield_breezeway_80183180[];

extern ViewCount D_dryfield_breezeway_80183188[];

extern DirectionWarpEntry D_dryfield_breezeway_8018318C[];

extern ViewCamera D_dryfield_breezeway_8018364C[];

extern SpriteView D_dryfield_breezeway_80183D9C[];

extern WorldCollisionSurfaceProperties* D_dryfield_breezeway_8018437C[];

/// Animates a spinning particle with gravity, damped grid bounces and a sixty-tick lifetime.
///
/// Gameplay effect slot 0x3C requires a coordinate body and spawner-owned
/// EffectWork in spawnArg2. The low twelve bits of spawnArg1 supply its size;
/// a zero move vector selects a random launch direction. Speed starts at 80
/// coordinate units per tick; bounce response halves speed and spin. Frames
/// 30..59 fade the texture, and frame 60 releases the task/work. Control pauses
/// motion and age after initialization; hidden control suspends all work and
/// cancellation releases it. The spawn parent must stay live through initialization.
void dryfieldBreezewayBouncingParticleTask(Task* task);

/// Draws the breezeway glow and emits the first encounter's bouncing particles and sound.
///
/// Gameplay effect slot 0xD4 requires its coordinate body, EffectWork and live
/// room-effect/player state. Glows draw in views 3..5 even when emission is
/// paused. Running control enables view effects; before the first encounter,
/// views 2/3 emit particles, and afterward a stored sixteen-tick tail emits
/// pairs. View indices must permit a signed-word bit shift; this room uses 0..5.
/// Borrows the spawn frame/work, and keeps its sound latch across ticks.
void dryfieldBreezewayAmbientEffectsTask(Task* task);

/// Runs the breezeway's room-message task: register, wait for messages, then retire.
///
/// A bodyless room task starts at state 0. State 1 idles indefinitely; state 2
/// kills the task. The caller must keep the state within 0..2. Registration
/// publishes the room message table and, before the first encounter is seen,
/// initializes its placed-actor bindings.
void dryfieldBreezewayMessageTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_BREEZEWAY_H
