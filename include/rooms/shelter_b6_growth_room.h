#ifndef INCLUDE_ROOMS_SHELTER_B6_GROWTH_ROOM_H
#define INCLUDE_ROOMS_SHELTER_B6_GROWTH_ROOM_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Number of retained placement-variant slots, including absent slots.
enum { SHELTER_B6_GROWTH_ROOM_AREA_VARIANT_COUNT = 13 };

/// Shelter B6 growth room placement/resource directory indexed by `GameLocationKey.variant`.
///
/// Variant 1 loads scene resources with an empty placement list. Its saved
/// variant and spawn flags share the nursery's state, but its tables are distinct.
/// All other slots, including 0, are absent (NULL, NULL).
/// Placements belong to the Neo Ark map overlay; recipes belong to this room.
/// Keep both and any actors supplying task descriptors loaded through loading,
/// spawning and model-flag updates.
/// Selectors must be in 0..12; there is no terminator or bounds check.
extern AreaVariant gShelterB6GrowthRoomAreaVariants[SHELTER_B6_GROWTH_ROOM_AREA_VARIANT_COUNT];

/// Number of mapped camera and sprite views in the growth room.
enum { SHELTER_B6_GROWTH_ROOM_VIEW_COUNT = 8 };

/// Logical-view map for the growth room's single room slot.
///
/// Room 1 borrows `gViewIdentityMap`: logical views 1..8 select the same
/// 1-based camera, image and sprite indices. The shared map has its own larger
/// capacity; this directory remains valid while the room overlay is loaded.
extern u8* gShelterB6GrowthRoomViewMaps[1];

/// Reverse-search length for room 1's logical-view map, in byte entries.
///
/// The single signed count covers views 1..8 and has no terminator. It limits
/// logical-view searches independently of the shared identity map's capacity.
extern ViewCount gShelterB6GrowthRoomViewCounts[1];

/// The growth room's single warp endpoint.
///
/// Slot 1 places both actors at (4030, 0, -2950), selects view 4 and
/// carries an optional map-marker key, without transition sounds.
/// Index by 1-based warp ID minus one; there is no terminator. Placements
/// use whole world units and yaw uses 4096 units per turn. The stage map
/// borrows this array while the room overlay and its sound bank stay loaded.
extern DirectionWarpEntry gShelterB6GrowthRoomWarpEntries[1];

/// Live growth-room collision mesh with a reserved movable box.
///
/// The 54 faces use 14 normals and 80 vertices in room coordinates, indexed by
/// a 2-by-4 XZ grid with 4000-unit cells. Room setup binds the view coordinate.
/// `shelterB6GrowthRoomResetCollisionBox` restores the leading four faces and
/// normals and eight vertices; later geometry and cell lists remain intact.
/// The descriptor and writable pools must stay loaded while collision uses them.
extern WorldCollisionGrid gShelterB6GrowthRoomCollisionGrid;

/// Growth-room cameras, indexed by the mapped 1-based view minus one.
///
/// Rotation coefficients use `ONE` per unit; translations are negated world
/// camera origins and projection distances are pixels. Keep the room overlay
/// loaded until a queued camera application has copied its selected record.
extern ViewCamera gShelterB6GrowthRoomViewCameras[SHELTER_B6_GROWTH_ROOM_VIEW_COUNT];

/// Background and depth-sorted sprite lists for the growth room's eight views.
///
/// Indexed by mapped view minus one. Views 2 and 3 have six and nineteen source
/// rectangles; the other views retain empty batch lists. Every view keeps the
/// decoded background strips and has no draw-area list. Descriptors and their
/// mutable sources/batches borrow the loaded room overlay's lifetime.
extern SpriteView gShelterB6GrowthRoomSpriteViews[SHELTER_B6_GROWTH_ROOM_VIEW_COUNT];

/// Growth-room model lighting: one white point light shared by every view.
///
/// No directional or cone lights. Room lighting parents and composes the mutable
/// point-light coordinate and updates its attenuation; keep this overlay loaded
/// while its descriptor or light is in use.
extern WorldCoordRoomLights gShelterB6GrowthRoomLights;

/// Twelve directional view boundaries connecting growth-room views 2 through 6.
///
/// Source and destination bytes are 1-based logical views. The final boundary
/// is active and carries `WORLD_COLLISION_TRIGGER_LAST`. Setup links and enables
/// all records; storage must remain loaded until the runtime list is unlinked.
extern WorldCollisionTrigger gShelterB6GrowthRoomViewBoundaryTriggers[12];

/// Growth-room interaction quads for companion scenes, departure and CAP events.
///
/// Twelve active records, ending with `WORLD_COLLISION_TRIGGER_LAST` on the
/// final quad. Room actions request conversation or departure; CAP records
/// request dialogue directly or send the room's companion command. Setup links
/// and enables the mutable records, which must stay loaded until unlinked.
extern WorldCollisionTrigger gShelterB6GrowthRoomActionTriggers[12];

/// Minimum model ambient colours for each growth-room view.
///
/// Entry zero is the eight-view count; entries 1..8 are RGB minima in units of
/// 1/16 of an 8-bit colour level, plus stored luma. View 2 has brighter minima;
/// the remaining views use one colour level. Borrowed while the room is loaded.
extern WorldCoordRoomAmbientEntry gShelterB6GrowthRoomAmbientByView[SHELTER_B6_GROWTH_ROOM_VIEW_COUNT + 1];

/// Collision policies and footstep cues indexed by growth-room surface class 0..7.
///
/// Every class blocks probes, allows weapon impacts and applies pushback.
/// Classes 1 and 2 select distinct sound-script pairs; classes 0 and 3..7 share
/// the silent record. The directory and its records remain room-owned and must
/// stay loaded for collision queries and player/companion footsteps.
extern WorldCollisionSurfaceProperties* gShelterB6GrowthRoomSurfaceProperties[8];

/// Runs growth-room entry setup, message service and teardown.
///
/// Requires the room and its actor_450900 scene resources to remain loaded.
/// State 0 installs room messages and starts the entry scene, state 1 waits
/// for messages, and state 2 tears down the task. No other state is valid.
void shelterB6GrowthRoomTask(Task* task);

/// Restores the growth room's reserved collision box with an optional Y displacement.
///
/// Requires the growth-room overlay and writable live grid pools. Zero
/// useYOffset restores the template's room coordinates; nonzero adds 2000 game
/// units to Y. Replaces the leading four faces/normals and eight vertices,
/// preserving vector fourth components, later geometry and all cell lists.
/// Repeated calls restore before shifting. Actor scene scripts call this while
/// the room's collision grid remains active; no resource is allocated or freed.
void shelterB6GrowthRoomResetCollisionBox(s32 useYOffset);

/// Draws the growth room's glows and emits its mist and drifting puffs.
///
/// Gameplay spawns bank-6 slot 0x15D with a coordinate body, state 0 and
/// spawnArg1 zero. That word grows to 304 every eighth animation frame while
/// room effects run; it sets the bottom-glow height in pixels and mist's
/// vertical spread in coordinate units. State becomes the mist-source count,
/// 1..6, and stays at six once reached. Requires the room overlay, loaded
/// particle textures, current view matrices, initialized scratch stack and
/// frame packet arena/OT.
/// Continues drawing and emitting particles when the ramp is paused.
void shelterB6GrowthRoomAmbientEffectsTask(Task* task);

/// Animates a ten-cell, fading mist sprite with random horizontal drift.
///
/// Requires the growth-room overlay, a coordinate body and the counted
/// `EffectWork` installed in spawnArg2 by the effect spawner. spawnArg1 packs
/// size in bits 0..11, frame ticks in 12..14, and speed in 16..23 (zero selects
/// 64 coordinate units/tick). A zero 12..15 field selects one tick; otherwise
/// bits 12..14 must be 1..7. The room supplies size 1280, six ticks and speed 16.
/// Ramps brightness in four-unit steps, draws before moving, then advances the
/// texture frame; frees the counted work and task after cell 9.
void shelterB6GrowthRoomMistTask(Task* task);

/// Animates a ten-cell puff with a fixed random rotation and downward acceleration.
///
/// Requires the growth-room overlay, a coordinate body and the counted
/// `EffectWork` installed in spawnArg2 by the effect spawner. spawnArg1 has the
/// size, frame-tick and speed encoding of `shelterB6GrowthRoomMistTask`; the room
/// supplies size 640, three ticks and speed 24. Samples a positive-X horizontal
/// direction, draws before moving, and adds two coordinate units/tick to Y
/// velocity after each move. Frees the counted work and task after cell 9.
void shelterB6GrowthRoomDriftPuffTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B6_GROWTH_ROOM_H
