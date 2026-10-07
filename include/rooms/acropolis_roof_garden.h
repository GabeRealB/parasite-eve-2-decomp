#ifndef INCLUDE_ROOMS_ACROPOLIS_ROOF_GARDEN_H
#define INCLUDE_ROOMS_ACROPOLIS_ROOF_GARDEN_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

extern AreaVariant D_acropolis_roof_garden_80185934[12];

extern TmdSource gAcropolisRoofGardenModel09868;

// acropolis_roof_garden
extern WorldCollisionRoomResources D_acropolis_roof_garden_80184C8C[];

extern u8* D_acropolis_roof_garden_80184C9C[];

extern ViewCount D_acropolis_roof_garden_80184CA0[];

extern WorldCoordRoomLighting D_acropolis_roof_garden_80184CA4[];

extern DirectionWarpEntry D_acropolis_roof_garden_80184CAC[];

extern SpriteView D_acropolis_roof_garden_80186648[];

extern ViewCamera D_acropolis_roof_garden_80186BF4[];

extern WorldCollisionSurfaceProperties* D_acropolis_roof_garden_80186DB0[];

void func_acropolis_roof_garden_8017DCDC(Task* task);

/// Draws a flickering light sprite in the roof garden views that see it.
///
/// Requires a coordinate body and zeroed, counted `EffectWork` from `effectSpawn`.
/// Initially `spawnArg1.value` packs a light index 0..9 in bits 0..3, a texture
/// cell 0..2 in bits 8..9, and a perspective scale in bits 16..27 (zero selects
/// 640). The first visible tick keeps only the light index; `EffectWork::angle`
/// holds the cell and `EffectWork::period` its resting grey level. Logical views
/// are 1..7. Room effect control 2 and above suppresses drawing without release.
/// A permitted view consumes one frame-arena quad even below camera depth Z/4
/// of 17. Requires scratch space and frame-arena capacity through GPU completion.
void acropolisRoofGardenLightGlowTask(Task* task);

/// Draws one frame of a pulsing red or green flare, then retires the effect task.
///
/// Requires a coordinate body and counted, owned `EffectWork` from `effectSpawn`.
/// `spawnArg1.value` packs pulse steps per animation tick in bits 0..7, radius
/// scale in bits 8..15, and green selection in bit 16 (clear selects red).
/// A negative word selects two concentric discs and four rays; otherwise two
/// quads form a diamond, with bit 28 adding two line streaks. Pixel radii divide
/// the scale by camera Z/4: discs multiply it by 1024 and 128, diamonds by 512.
/// Depths below 17 draw nothing but still release the work and task. Requires
/// scratch space and frame-arena capacity for packets and additive draw modes;
/// queued packets remain live until GPU completion.
void acropolisRoofGardenFlareTask(Task* task);

/// Advances a tumbling leaf through its fall, stationary hold and fade.
///
/// Requires a coordinate body and zeroed, counted `EffectWork` from `effectSpawn`.
/// Motion stops after parent-space Y becomes positive. The Acropolis texture
/// uses a square of half-size 32 coordinate units; completed fading releases
/// the work and task. Drawing requires frame-arena and scratch-stack capacity.
void acropolisRoofGardenLeafFallTask(Task* task);

void func_acropolis_roof_garden_8017DC74(Task* task);

void func_acropolis_roof_garden_80180160(Task* task);

#endif // INCLUDE_ROOMS_ACROPOLIS_ROOF_GARDEN_H
