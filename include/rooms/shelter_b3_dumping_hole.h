#ifndef INCLUDE_ROOMS_SHELTER_B3_DUMPING_HOLE_H
#define INCLUDE_ROOMS_SHELTER_B3_DUMPING_HOLE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"
#include "overlay.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

extern OverlayEncounterSpot D_shelter_b3_dumping_hole_8018B74C[12];

extern TmdSource gShelterB3DumpingHoleAcropolisSanctuaryModel090F0;

extern TaskDesc D_shelter_b3_dumping_hole_8018B83C[4];

extern TaskDesc D_shelter_b3_dumping_hole_8018B57C[1];

extern AreaVariant D_shelter_b3_dumping_hole_8018EC3C[13];

// shelter_b3_dumping_hole
extern WorldCollisionRoomResources D_shelter_b3_dumping_hole_8018B678[];

extern u8* D_shelter_b3_dumping_hole_8018B698[];

extern ViewCount D_shelter_b3_dumping_hole_8018B6A0[];

extern DirectionWarpEntry D_shelter_b3_dumping_hole_8018B6A4[];

extern ViewCamera D_shelter_b3_dumping_hole_8018C410[];

extern SpriteView D_shelter_b3_dumping_hole_8018E050[];

extern WorldCoordRoomLights D_shelter_b3_dumping_hole_8018E3DC;

extern WorldCoordRoomLights D_shelter_b3_dumping_hole_8018E874;

extern WorldCoordRoomAmbientEntry D_shelter_b3_dumping_hole_8018F1FC[];

extern WorldCoordRoomAmbientEntry D_shelter_b3_dumping_hole_8018F32C[];

extern WorldCollisionSurfaceProperties* D_shelter_b3_dumping_hole_8018F480[];

void func_shelter_b3_dumping_hole_8017D9A8(Task* task);

/// Spawns a dark rising sprite at a coordinate's world position plus `worldOffset`.
///
/// The offset is copied in world units during this call; the coordinate is
/// lent through the task's first update. The sprite then moves independently.
/// Requires the debris event director to remain live; its actor-sprite stop
/// flag cancels the task. Allocation failure removes the spawned task.
void shelterB3DumpingHoleSpawnActorSprite(GfxCoord* sourceCoord, const SVECTOR* worldOffset);

/// Draws the dumping hole's world-space discs and capsules for the mapped camera view.
///
/// Exported to gameplay's room-effect table. Requires composed view matrices
/// and the frame packet arena and scratch stack used by the glow drawers.
/// On its first tick clears the room's conditional-glow flag; view 14 draws
/// its two gated points only while that flag is nonzero. Retains no pointer.
void shelterB3DumpingHoleDrawViewGlowsTask(Task* task);

void shelterB3DumpingHoleEffectSpriteDriftTaskAimed(Task* task);

/// Animates an eight-frame chip or billboard particle shed by the Glutton rain effect.
///
/// Spawn argument 2 lends the effect spawner's counted `EffectWork`.
/// Argument 1 packs size in bits 0-11, frame period in 12-15 (zero means one tick), speed in 16-23
/// (zero means 64), and velocity kind in 24-27: 0 still, 1 random upward,
/// 2 random in all axes, 3 narrow upward, 5 direction from the work position.
/// A nonzero top nibble selects billboard drawing; zero selects a spinning
/// chip. An existing nonzero velocity bypasses direction generation.
/// Movement adds six world units per tick to Y velocity, then animation
/// advances at the selected period and releases after frame 7. Suspension
/// redraws without advancing; cancellation releases through `effectKillTask`.
void shelterB3DumpingHoleGluttonRainParticleTask(Task* task);

void func_shelter_b3_dumping_hole_80186D4C(Task* arg0);

#endif // INCLUDE_ROOMS_SHELTER_B3_DUMPING_HOLE_H
