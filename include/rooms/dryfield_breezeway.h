#ifndef INCLUDE_ROOMS_DRYFIELD_BREEZEWAY_H
#define INCLUDE_ROOMS_DRYFIELD_BREEZEWAY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Number of logical and mapped camera/sprite views in the breezeway.
enum { DRYFIELD_BREEZEWAY_VIEW_COUNT = 6 };

extern AreaVariant D_dryfield_breezeway_801842F8[13];

// dryfield_breezeway
extern WorldCollisionRoomResources D_dryfield_breezeway_8018316C[];

/// Room 1's logical-view map, borrowing gameplay's identity entries for views 1..6.
///
/// The map directory has one room slot; mapped cameras, images and sprites
/// use the same 1-based index. The room and gameplay must remain loaded.
extern u8* gDryfieldBreezewayViewMaps[1];

/// Room 1's authored light collection, using the default ambient minima.
///
/// Light transforms and attenuation remain writable while this overlay is loaded.
extern WorldCoordRoomLighting gDryfieldBreezewayRoomLighting[1];

/// Logical-view count storage: six views for room 1, then an unused zero halfword.
///
/// The trailing halfword's role is unproven; it is not a list terminator.
extern ViewCount gDryfieldBreezewayViewCounts[2];

extern DirectionWarpEntry D_dryfield_breezeway_8018318C[];

/// Cameras for mapped views 1..6, indexed by mapped view minus one.
///
/// Matrices use 4096 per unit rotation and world-unit translations; projection
/// distances are pixels. Borrow these records only while the room is loaded.
extern ViewCamera gDryfieldBreezewayViewCameras[DRYFIELD_BREEZEWAY_VIEW_COUNT];

/// Sprite descriptors for mapped views 1..6, indexed by mapped view minus one.
///
/// Views 2, 3 and 6 have source sprites; views 1, 4 and 5 have empty batch lists.
/// All six keep the decoded background and have no clipping list. The mutable
/// descriptors and their arrays remain borrowed only while this room is loaded.
extern SpriteView gDryfieldBreezewaySpriteViews[DRYFIELD_BREEZEWAY_VIEW_COUNT];

/// Collision properties for surface classes 0..7 while the breezeway is loaded.
///
/// Every class blocks probes, allows weapon impacts and applies pushback.
/// Class 1 supplies footstep cues; the other classes share a silent record.
extern WorldCollisionSurfaceProperties* gDryfieldBreezewaySurfacePropertiesByClass[8];

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
