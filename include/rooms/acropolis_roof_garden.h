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

/// Actor layout variants for the Acropolis roof garden.
///
/// Indexed directly by `GameLocationKey.variant` (0..11); no terminator.
/// Slots 1, 2, 7 and 8 publish placement/resource pairs; all other slots are NULL pairs.
/// The room owns recipes and the stage map owns placements. Keep the map
/// loaded while actors borrow placements.
/// Keep the room loaded during loading, spawning and model-policy updates,
/// and each selected actor overlay loaded throughout its tasks' lifetime.
extern AreaVariant gAcropolisRoofGardenAreaVariants[12];

extern TmdSource gAkropolisRoofGardenAcropolisRoofGardenModel09868;

/// Collision resources for roof-garden room 1, indexed by room minus one.
///
/// Borrows the room's mesh, six directed view boundaries, seven action triggers
/// and two sight occluders. Setup mutates their transforms, links and flags;
/// all storage stays live while the room overlay is loaded and resources are bound.
extern WorldCollisionRoomResources gAcropolisRoofGardenCollisionRoomResources[1];

/// Number of logical views in the roof garden's single room.
enum { ACROPOLIS_ROOF_GARDEN_VIEW_COUNT = 7 };

/// Logical-to-mapped view directory for roof-garden room 1.
///
/// Borrows `gViewIdentityMap`: logical views 1..7 select the same camera,
/// image and sprite indices. The directory lives with the loaded room overlay.
extern u8* gAcropolisRoofGardenViewMaps[1];

/// Reverse-lookup view count for roof-garden room 1, in logical-view elements.
///
/// Seven entries of the borrowed identity map are searched; this count does
/// not describe the camera array's capacity. Storage lives with the overlay.
extern ViewCount gAcropolisRoofGardenViewCounts[1];

/// Model-shading lights and ambient policy for roof-garden room 1.
///
/// Borrows fourteen mutable point lights and uses the default ambient minima.
/// The descriptor and its light collection stay live while the overlay is loaded.
extern WorldCoordRoomLighting gAcropolisRoofGardenRoomLightingTable[1];

/// Two roof-garden warp endpoints, indexed by the 1-based warp ID minus one.
///
/// Both place player and companion at (-7458, 0, -9712), yaw zero, in world
/// units. Endpoint 1 starts view 2 and uses room sound entries 2/1 for arrival/
/// departure; endpoint 2 starts view 3 and is silent. Neither has flags or a
/// map marker. The map directory borrows the array while the room is loaded.
extern DirectionWarpEntry gAcropolisRoofGardenWarpEntries[2];

/// Sprite overlays for mapped roof-garden views 1..7, indexed by view minus one.
///
/// Views 2..5 borrow source rectangles and range lists; views 1, 6 and 7 have
/// empty lists. Every view keeps decoded image strips and has no clip list.
/// Descriptors and their mutable arrays live with the loaded room overlay.
extern SpriteView gAcropolisRoofGardenSpriteViews[ACROPOLIS_ROOF_GARDEN_VIEW_COUNT];

/// Camera transforms and pixel projection distances for the roof garden.
///
/// Mapped views 1..7 select entries 0..6. Entry 7 is retained authored storage;
/// its use is unproven. Rotation has 12 fractional bits and translation is
/// the negated world-space camera origin. Storage lives with the room overlay.
extern ViewCamera gAcropolisRoofGardenViewCameras[8];

/// Collision and footstep policies indexed by roof-garden surface class 0..7.
///
/// Every class blocks probes, allows weapon impacts and applies pushback.
/// Classes 1 and 2 share footstep cues; the other classes are silent.
/// The directory and borrowed records live with the loaded room overlay.
extern WorldCollisionSurfaceProperties* gAcropolisRoofGardenSurfacePropertiesByClass[8];

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
