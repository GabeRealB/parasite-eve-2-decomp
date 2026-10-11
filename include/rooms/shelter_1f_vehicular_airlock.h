#ifndef INCLUDE_ROOMS_SHELTER_1F_VEHICULAR_AIRLOCK_H
#define INCLUDE_ROOMS_SHELTER_1F_VEHICULAR_AIRLOCK_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

/// Models the Neo Ark map UI overlay's enemy descriptors attach.
extern TmdSource gShelter1fVehicularAirlockModel03A58;

/// Number of retained placement-variant slots, including absent slots.
enum { SHELTER_1F_VEHICULAR_AIRLOCK_AREA_VARIANT_COUNT = 12 };

/// Shelter 1F vehicular airlock placement/resource directory indexed by `GameLocationKey.variant`.
///
/// Variant 1 places a Beam Sword Pawn and a grenade-launcher Rook.
/// All other slots, including 0, are absent (NULL, NULL).
/// Placements belong to the Neo Ark map overlay; recipes belong to this room.
/// Keep both and any actors supplying task descriptors loaded through loading,
/// spawning and model-flag updates.
/// Selectors must be in 0..11; there is no terminator or bounds check.
extern AreaVariant gShelter1fVehicularAirlockAreaVariants[SHELTER_1F_VEHICULAR_AIRLOCK_AREA_VARIANT_COUNT];

/// Room 1's collision grid, two view boundaries and seven action regions; no occluders.
///
/// The Neo Ark area directory borrows this record and its mutable resources
/// while the vehicular airlock overlay remains loaded.
extern WorldCollisionRoomResources gShelter1fVehicularAirlockCollisionResources[1];

/// Room 1's model-shading light collection and per-view minimum ambient colours.
///
/// Borrows five mutable point lights and a count-prefixed ambient table from
/// this overlay; keep them loaded while coordinate and lighting queries use them.
extern WorldCoordRoomLighting gShelter1fVehicularAirlockRoomLighting[1];

/// Room 1's logical-view map, borrowing gameplay's persistent identity mapping.
///
/// Logical views 1..3 map to camera/image/sprite indices 1..3. The room's
/// three-view search limit is independent of the shared map's larger capacity.
extern u8* gShelter1fVehicularAirlockViewMaps[1];

/// Room 1's three-view reverse-search limit, stored as a signed 16-bit count.
extern ViewCount gShelter1fVehicularAirlockViewCounts[1];

/// Three arrival/departure records for the Airlock, Guardroom and Bulwark doors.
///
/// Warp IDs 1..3 select entries at ID minus one. Positions use room-coordinate
/// units and yaw uses 4096 per turn; initial mapped views are 2, 3 and 3.
/// Transition handling borrows the records while this overlay is loaded.
extern DirectionWarpEntry gShelter1fVehicularAirlockWarpEntries[3];

/// Cameras for mapped views 1..3, with Q12 rotations and pixel projection distances.
///
/// Translations store negated camera origins in world units. The Neo Ark
/// camera directory borrows this array while the room overlay is loaded.
extern ViewCamera gShelter1fVehicularAirlockViewCameras[3];

/// Mapped views 1..3's empty sprite lists, selecting decoded image strips without clipping.
///
/// Each source slot retains its own batch-list address and supplies no sprites.
/// The view descriptors and mutable batch lists remain borrowed from this overlay.
extern SpriteView gShelter1fVehicularAirlockSpriteViews[3];

/// Collision surface classes 0..7, all blocking probes and enabling impacts and pushback.
///
/// Class 1 supplies floor footsteps; the other classes have no footstep cues.
/// Records and cue bases belong to this loaded overlay; selectors have no terminator.
extern WorldCollisionSurfaceProperties* gShelter1fVehicularAirlockSurfaceProperties[8];

/// Hides the placed airlock model while its saved two-bit object state is 2.
///
/// Requires a live TMD body and the owning `Enemy` in `spawnArg2.pointer`, as
/// supplied by area object placement. The low byte of its place key selects
/// object-state flag 0..63 (the map places this model at flag 6). State 2 sets
/// `TMD_OBJECT_SKIP_ACTIVE_DRAW`; every other state clears it. Other flags and
/// task state are preserved. Both the body and session flag bank must remain live.
void shelter1fVehicularAirlockUpdatePlacedModelVisibilityTask(Task* task);

/// Runs the vehicular airlock's room-message receiver.
///
/// Start the bodyless task in state 0 to install the table and register
/// `GAME_TASK_SLOT_ROOM`; states 1 and 2 idle and release it. The state must
/// be 0..2: dispatch uses an unchecked stack copy of the three-entry table.
/// Keep this overlay and the borrowed registered task live through dispatch.
void shelter1fVehicularAirlockRoomTask(Task* task);

/// Runs the vehicular airlock's charging pink flash, peak screen tint and fading star.
///
/// Requires a coordinate body and counted, owned `EffectWork` in
/// `spawnArg2.pointer`, as installed by `effectSpawn`. `spawnArg1.value` must
/// start as a positive charge duration in active ticks; charging consumes it.
/// Nonzero room effect control pauses the flash; control 4 or above, state 3,
/// or completion releases the effect work and task.
void shelter1fVehicularAirlockRoomVisualEffectsFlashTask(Task* task);

/// Draws fading twin trails from two fixed offsets on the effect's parent.
///
/// Requires a coordinate body and counted, owned `EffectWork` in
/// `spawnArg2.pointer`; its parent coordinate must stay live until teardown.
/// `Task::work` starts null and owns two allocated eight-coordinate histories.
/// `spawnArg1.value` is the nonzero signed-16-bit age at which to finish;
/// zero leaves lifetime to external teardown. Age includes initialization and
/// advances only with room effect control below 2. Allocation failure resets
/// age for retry. Teardown releases both the histories and the effect work.
void shelter1fVehicularAirlockRoomVisualEffectsTwinTrailTask(Task* task);

/// Runs an impact flash followed by smoke puffs or orange rings and bouncing sparks.
///
/// Requires a coordinate body and owned, zero-aged `EffectWork` in
/// `spawnArg2.pointer`, supplied by `effectSpawn`. Nonzero `spawnArg1.value`
/// selects smoke; zero selects rings. Active age seven enters release and the
/// next active tick frees the work and task. Room effect control 1..3 pauses
/// the task and 4 or above cancels it. Children have independent lifetimes.
void shelter1fVehicularAirlockRoomVisualEffectsSparkBurstTask(Task* task);

/// Draws the vehicular airlock's fixed light glows in mapped views 2 and 3.
///
/// Bank-6 task 0x164 installs this room's flash, twin-trail and spark-burst IDs
/// on its first tick. View 3 uses pulsing stars for three lamps while the
/// bulwark-unlocked flag is 1, and flickering discs otherwise. Other views emit
/// no geometry. Requires this overlay loaded, current view matrices, an
/// initialized scratch stack, and frame packet/ordering-table space. Queued
/// packets borrow the frame arena until GPU completion; the task owns no work.
void shelter1fVehicularAirlockDrawLightsTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_1F_VEHICULAR_AIRLOCK_H
