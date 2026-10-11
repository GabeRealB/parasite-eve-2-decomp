#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_JUNK_YARD_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_JUNK_YARD_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Placement/resource directory for night Junk Yard variants 0..21.
///
/// Indexed directly by `GameLocationKey.variant`; slots 1, 2, 4, 7 and 21 are
/// populated and every other slot has two NULL pointers. Gameplay imports the
/// directory from this room; placements belong to the Dryfield full-map overlay
/// and resource recipes to this room. Keep both overlays and the selected actor
/// descriptors loaded during loading, spawning and model-flag updates; actors
/// retain their map-owned placement records throughout their lifetimes.
extern AreaVariant gDryfieldNightJunkYardAreaVariants[22];

/// Lighting for nighttime junk-yard rooms 1 and 2, indexed by room ID minus one.
///
/// Both borrow the same seven-point-light collection and use the default
/// ambient minima. The map overlay exports these descriptors to gameplay;
/// keep the junk-yard overlay loaded while its lighting is in use.
extern WorldCoordRoomLighting gDryfieldNightJunkYardRoomLighting[2];

/// Collision resources for night Junk Yard rooms 1 and 2, indexed by room minus one.
///
/// Both entries borrow the same grid, ten view boundaries, twenty action triggers
/// and one sight occluder. The final record of each list carries LAST and is
/// included in setup. Resources remain mutable and must stay loaded until the
/// grid is inactive and the trigger/occluder records have been unlinked.
extern WorldCollisionRoomResources gDryfieldNightJunkYardRoomCollisionResources[2];

/// Logical-view maps for nighttime junk-yard rooms 1 and 2, indexed by room ID minus one.
///
/// Room 1 borrows gameplay's identity map; room 2 remaps logical views 1..10
/// to camera/image/sprite indices 1..10. Index each map at logical view minus
/// one. The maps and their directory must remain loaded while in use.
extern u8* gDryfieldNightJunkYardViewMaps[2];

/// Logical-view search lengths for rooms 1 and 2, indexed by room minus one.
///
/// Both signed-halfword counts are ten. They bound logical slots 1..10 in the
/// corresponding view map, excluding its retained trailing bytes. The count
/// array stays borrowed while the room overlay is loaded.
extern ViewCount gDryfieldNightJunkYardViewCounts[2];

/// Warp endpoints 1..3 for the night Junk Yard, indexed by warp ID minus one.
///
/// Slots 1/2 connect the garage/trailer coach; slot 3's live use is unproven.
/// Player and companion share each slot's XYZ in world units and
/// yaw zero, with initial views 2, 5 and 3. Slot 2 has a blocked-departure cue;
/// slots 1/2 update map flags 472/471 on arrival. There is no end marker.
/// Slot 3 retains sound-bank entries 6/5, which are empty in the night bank.
/// The map overlay borrows the array; keep this room loaded through each lookup.
extern DirectionWarpEntry gDryfieldNightJunkYardWarpEntries[3];

/// Stored cameras for the nighttime junk yard's two room slots.
///
/// Both current maps select 1-based indices 1..10 at index minus one; the
/// eleventh stored camera's use is unproven and its bytes remain present.
/// Rotation uses ONE per unit, translation is the negated world-space origin
/// and perspective distance is pixels. The camera directory borrows this
/// mutable array while the room overlay is loaded.
extern ViewCamera gDryfieldNightJunkYardViewCameras[11];

/// Dryfield night junk yard's sprite resources, indexed by mapped view minus one.
///
/// Views 1 and 6 have no sources; the other slots borrow sprite pools.
/// Both logical rooms select this directory through their separate maps.
/// Every slot retains decoded image strips and has no clipping list. Empty
/// source slots retain batch addresses without supplying SpriteSource elements.
/// Descriptors and mutable source/batch lists stay borrowed while the owning
/// room overlay is loaded; selectors must be nonzero and within this array.
extern SpriteView gDryfieldNightJunkYardSpriteViews[10];

/// Collision and footstep policies for junk-yard surface classes 0..7.
///
/// Class 1 supplies walk/run/jump cues and blocks probes; class 2 passes probes
/// and ignores weapon impacts. Classes 0 and 3..7 block probes without footstep
/// cues. All apply pushback. The map overlay borrows the table and its records
/// while the junk-yard overlay remains loaded.
extern WorldCollisionSurfaceProperties* gDryfieldNightJunkYardSurfacePropertiesByClass[8];

/// Shows or hides the junk-yard encounter's sprite batch in mapped view 7.
///
/// Zero shows the nineteen sprites in batch 5; any nonzero byte hides them.
/// Requires the active location to select the loaded night junk-yard sprite
/// directory and its mutable batch list. Does not change the saved event flag.
void dryfieldNightJunkYardSetEventSpriteBatchHidden(u8 hidden);

/// Runs the junk yard's charging pink flash, peak screen tint and fading star.
///
/// Starts in state 0 with a coordinate body and counted `EffectWork` in
/// `spawnArg2.pointer` from `effectSpawn`. `spawnArg1.value` is a positive
/// charge duration in active ticks, consumed as a countdown. Nonzero room
/// effect control pauses it; values at least four cancel it. State 3 also
/// releases the work and task. Requires live effect state, view transforms,
/// scratch space and frame packet storage; the room overlay must stay loaded.
void dryfieldNightJunkYardRoomVisualEffectsFlashTask(Task* task);

/// Runs the junk yard's fading sword beam between two moving endpoint histories.
///
/// Starts in state 0 with a coordinate body and counted `EffectWork` in
/// `spawnArg2.pointer` from `effectSpawn`; its parent coordinate must stay live.
/// Owns two eight-coordinate histories in `Task::work`, freed by task teardown.
/// Allocation failure retries with age zero. Initialization is the first active
/// tick; subsequent ticks record endpoints and draw seven quads tinted 1:2:3
/// in R:G:B. `spawnArg1.value` is 0 for external teardown or 2..32767 for an
/// active-age limit. Room effect control at two or above suspends recording and
/// drawing without cancelling. Requires live effect state, current view
/// transforms, scratch space, frame packet storage and the loaded room overlay.
void dryfieldNightJunkYardRoomVisualEffectsTwinTrailTask(Task* task);

/// Runs the junk yard's impact flash with smoke or orange rings and bouncing sparks.
///
/// Starts in state 0 with a coordinate body and zero-aged counted `EffectWork`
/// in `spawnArg2.pointer`. Nonzero `spawnArg1.value` selects smoke; zero selects
/// rings and sparks. Active age seven enters release; the next active tick
/// frees the work and task. Nonzero room effect control below four pauses it,
/// and four or above cancels it. Child effects live independently. Requires
/// live effect state, view transforms, scratch and packet storage, and the
/// room overlay loaded until teardown.
void dryfieldNightJunkYardRoomVisualEffectsSparkBurstTask(Task* task);

/// Selects the junk yard's enemy effects and draws the current view's lamp glows.
///
/// State 0 installs the room's flash, sword-trail and spark-burst IDs on every
/// tick until another caller changes the state. Every tick enables the room's
/// view-effect gate. Views 2/8 draw a capsule and four flares; 4/9 draw three
/// flares; 5/10 draw the capsule and three flares; 7 draws it with two flares.
/// The capsule caps start at a quarter-turn angle in views 2/8 and zero in
/// 5/7/10. Other views draw none. Requires live session and effect state,
/// current view transforms, scratch space and frame packet storage. The room
/// overlay must stay loaded while its effects can run.
void dryfieldNightJunkYardDrawGlowsTask(Task* task);

/// Dispatches initialization, idle message reception and teardown for the room.
///
/// Starts in state 0; state must remain in 0..2. Publishes the room task slot
/// and message table on initialization. The room overlay and sprite directory
/// must stay loaded while this task or its message handlers can run.
void dryfieldNightJunkYardRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_JUNK_YARD_H
