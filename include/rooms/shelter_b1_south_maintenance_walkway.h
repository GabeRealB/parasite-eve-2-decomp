#ifndef INCLUDE_ROOMS_SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_H
#define INCLUDE_ROOMS_SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_shelter_b1_south_maintenance_walkway_80183598[12];

// shelter_b1_south_maintenance_walkway
extern WorldCoordRoomLighting D_shelter_b1_south_maintenance_walkway_801823F4[];

extern WorldCollisionRoomResources D_shelter_b1_south_maintenance_walkway_801823FC[];

extern u8* D_shelter_b1_south_maintenance_walkway_8018240C[];

extern ViewCount D_shelter_b1_south_maintenance_walkway_80182410[];

extern DirectionWarpEntry D_shelter_b1_south_maintenance_walkway_80182414[];

extern ViewCamera D_shelter_b1_south_maintenance_walkway_801827DC[];

extern SpriteView D_shelter_b1_south_maintenance_walkway_80182E18[];

extern WorldCollisionSurfaceProperties* D_shelter_b1_south_maintenance_walkway_80183614[];

/// Runs this room's persistent message receiver.
///
/// The Shelter map spawns a bodyless task in state 0: setup installs its room
/// message table and publishes `GAME_TASK_SLOT_ROOM`, state 1 is idle, and
/// state 2 kills the task. State must stay in 0..2; no bound is checked.
/// Spawn arguments are unused. Keep the room overlay loaded while it runs.
void shelterB1SouthMaintenanceWalkwayRoomTask(Task* task);

/// Charges a pink flash, tints the screen at its peak, then fades it as a star.
///
/// Bank-6 slot 0x1C9 owns the `EffectWork` in `spawnArg2` and uses the task's
/// coordinate body. `spawnArg1.value` starts as a positive charge duration in
/// active ticks and is consumed as a countdown. Room effect control pauses at
/// nonzero and cancels at four or above; state 3 also releases the effect.
void shelterB1SouthMaintenanceWalkwayRoomVisualEffectsFlashTask(Task* task);

/// Records two moving endpoints and draws a fading blue twin trail between their histories.
///
/// Bank-6 slot 0x1E5 owns the `EffectWork` in `spawnArg2` and allocates two
/// eight-coordinate histories in `Task::work`, released by task teardown.
/// The work's parent coordinate must stay live; the endpoint offsets are local
/// to it, while history snapshots retain world positions. `spawnArg1.value`
/// is compared with the signed 16-bit active age after initialization; zero
/// disables that lifetime check. Control values below two advance the trail;
/// two or above leave it untouched until it resumes or is killed externally.
void shelterB1SouthMaintenanceWalkwayRoomVisualEffectsTwinTrailTask(Task* task);

/// Runs an animated spark along a fixed step toward an initially sampled target.
///
/// Bank-6 slot 0x21B owns the `EffectWork` in `spawnArg2`. Its coordinate body
/// and the borrowed target `GfxCoord` in `spawnArg1.pointer` need matrices
/// composed into the same view space on the first active tick. It samples the
/// displacement once, stores a 204/4096 step with signed-16-bit narrowing,
/// and releases the effect at active age 20. Room effect control pauses at
/// nonzero and cancels at four or above.
void shelterB1SouthMaintenanceWalkwayRoomVisualEffectsFlyingSparkTask(Task* task);

/// Runs an attached charge disc with player-joint sparks and a fading release ring.
///
/// Bank-6 slot 0x21C owns a zeroed `EffectWork` in `spawnArg2.pointer` and a
/// coordinate body. `spawnArg1.value` selects tint 0 or 1. The work's parent
/// coordinate and ancestors must remain live; its copied local offset replaces
/// the initial placement. Growth emits adopted flying sparks from player parts
/// 3..18 every fourth active age, requiring the player model and this room's
/// installed flying-spark callback. The owner may request flicker, release or
/// cancel through `ROOM_VISUAL_EFFECTS_GLOW_DISC_*`. Release fades the disc and
/// expands an orange ring; teardown releases work and adopted child tasks.
/// Room effect control pauses at nonzero and cancels at four or above.
void shelterB1SouthMaintenanceWalkwayRoomVisualEffectsGlowDiscTask(Task* task);

/// Expands an orange disc and layered glow, fading its ring before its centre.
///
/// Bank-6 slot 0x21D owns the `EffectWork` in `spawnArg2` and uses the task's
/// coordinate body. Brightness starts at 224; the glow half-extent starts at
/// 128 world units and grows by 16 per active tick. The effect releases its
/// work after fading. Room effect control pauses at nonzero and cancels at four or above.
void shelterB1SouthMaintenanceWalkwayRoomVisualEffectsFlyingOrangeBurstTask(Task* task);

/// Draws this room's view-dependent capsule glows and red disc.
///
/// Bank-6 slot 0x129 first binds this room's six shared effect IDs and enters
/// state 1. Saved views 2..5 select the glows; view 1 draws none. The radius
/// scale is 512, giving a screen radius of 512 * 64 / (camera Z / 4), with
/// the drawers' near-depth clipping and clamp. Requires current view matrices,
/// room effect state, ordering table and packet arena; owns no effect work.
void shelterB1SouthMaintenanceWalkwayDrawGlowsTask(Task* task);

/// Runs an impact flash followed by smoke or orange rings and bouncing sparks.
///
/// Bank-6 slot 0x201 owns a zero-aged `EffectWork` in `spawnArg2.pointer` and
/// a coordinate body. Nonzero `spawnArg1.value` emits smoke at active ages 1..7;
/// zero emits two independent bouncing sparks then draws fading orange rings.
/// Both variants enter release at age 7 and free work on the next active tick.
/// Room effect control pauses at nonzero and cancels at four or above.
void shelterB1SouthMaintenanceWalkwayRoomVisualEffectsSparkBurstTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_H
