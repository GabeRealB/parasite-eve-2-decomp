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

/// Places the roof garden's ten light glows and emits its view-dependent red flares.
///
/// Requires a coordinate body and zeroed, counted `EffectWork` from `effectSpawn`.
/// The first tick places glows 0/1 in cell 0 at scale 512, glow 2 in cell 1 at
/// scale 1024 and glows 3..9 in cell 2 at the default scale 640. Every accepting
/// tick also emits a diamond flare in logical views 5/6 or a disc flare in view
/// 7, with pulse step 14 and radius scales 6/3 respectively. Logical views must
/// be 1..7. Control 4 and above suppresses further flares but leaves the emitter
/// live. Spawned work retains borrowed offset and coordinate pointers, while
/// the glow and flare drawers use the copied positions and their own bodies.
void acropolisRoofGardenAmbientEffectsTask(Task* task);

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

/// Dispatches the roof-garden room task's setup, per-frame scene check and teardown.
///
/// `state` must be 0 (register messages and spawn ambience), 1 (arrival-scene
/// check), or 2 (kill). Copies the three callbacks before dispatch; the selected
/// callback may retire the task. The map's Acropolis area 13 descriptor starts
/// it at state zero, with the room overlay loaded throughout its lifetime.
void acropolisRoofGardenRoomTask(Task* task);

/// Selects the roof-garden pickup model's visibility from its saved state and view.
///
/// Requires the placed object's `Enemy` in `spawnArg2.pointer` and a TMD body.
/// The placement key's low byte selects saved object state 0..3. State 2 hides
/// the model, including when a room event suppresses the pickup; otherwise only
/// mapped views 5..7 select the flagged draw pass, with ordering offset zero.
/// Hidden ticks replace all flags with the active-draw skip flag. The task
/// changes neither saved object state nor resource ownership.
void acropolisRoofGardenPickupModelTask(Task* task);

#endif // INCLUDE_ROOMS_ACROPOLIS_ROOF_GARDEN_H
