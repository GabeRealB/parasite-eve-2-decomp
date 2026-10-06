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

void func_shelter_b1_south_maintenance_walkway_8017DA34(Task* task);

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

void func_shelter_b1_south_maintenance_walkway_801806F4(Task* arg0);

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

void func_shelter_b1_south_maintenance_walkway_8017FAAC(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B1_SOUTH_MAINTENANCE_WALKWAY_H
