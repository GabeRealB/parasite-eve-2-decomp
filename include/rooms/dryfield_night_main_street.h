#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_MAIN_STREET_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_MAIN_STREET_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Room-space unit normals for the room-2/3 collision mesh (4096 per unit).
///
/// The opening scene replaces the first four entries in place. Keep the room
/// overlay loaded while collision borrows this pool.
extern SVECTOR gDryfieldNightMainStreetRooms2And3CollisionGridNormals[23];

/// Signed room-coordinate vertices for the room-2/3 collision mesh.
///
/// The opening scene replaces the first sixteen entries in place. Keep the room
/// overlay loaded while collision borrows this pool.
extern SVECTOR gDryfieldNightMainStreetRooms2And3CollisionGridVerts[196];

/// Indexed collision faces and room-local surface classes for rooms 2 and 3.
///
/// The opening scene replaces the first four entries in place. Indices refer to
/// this mesh's vertex and normal pools, which remain live with the room overlay.
extern WorldCollisionGridFace gDryfieldNightMainStreetRooms2And3CollisionGridFaces[89];

/// Automatic room-action regions removed by the opening scene.
enum {
    DRYFIELD_NIGHT_MAIN_STREET_ACTION_TRIGGER_AFTER_BURNER  = 8,
    DRYFIELD_NIGHT_MAIN_STREET_ACTION_TRIGGER_BEFORE_BURNER = 9,
};

/// Mutable interaction and warp regions shared by room selectors 2 and 3.
///
/// The final record carries WORLD_COLLISION_TRIGGER_LAST and is included in
/// binding. List links, enabled flags and hit latches change during play;
/// the opening scene unlinks the two named Burner regions. Geometry uses game
/// coordinates and 4096-per-unit normals. Keep this overlay loaded while linked.
extern WorldCollisionTrigger gDryfieldNightMainStreetRooms2And3ActionTriggers[12];

/// Placement layouts indexed by the saved main-street variant in 0..12.
///
/// Slots 1, 2 and 7 supply Desert Chaser/Moth, Gary Douglas scene and Brain
/// Stinger resources respectively; every other slot is absent (both pointers
/// NULL). No terminator. Both this room and the map overlay owning placements
/// must stay loaded during resource loading and actor spawning.
extern AreaVariant gDryfieldNightMainStreetAreaVariants[13];

/// Collision resources indexed by main-street room selector minus one (rooms 1..3).
///
/// All rooms share view boundaries. Room 1 has its own grid/action regions;
/// rooms 2 and 3 share another grid/action array. No room has occluders and
/// there is no terminal record. Activation borrows mutable room-overlay storage
/// until replacement or list clearing; keep the overlay loaded throughout.
extern WorldCollisionRoomResources gDryfieldNightMainStreetRoomCollisionResources[3];

/// Light and ambient resources for main-street room selectors 1..3.
///
/// Indexed by room - 1; all three records borrow the same mutable point-light
/// collection and logical-view ambient table. No terminator. The records and
/// their resources remain valid only while this room overlay is loaded.
extern WorldCoordRoomLighting gDryfieldNightMainStreetRoomLighting[3];

/// Logical-view maps for main-street room selectors 1..3.
///
/// Indexed by room - 1, then logical view - 1 for views 1..24. Room 1 borrows
/// the gameplay identity map; rooms 2 and 3 supply their own byte mappings.
/// Values are 1-based camera/image/sprite indices in 1..24. There is no
/// terminator; keep gameplay and this room overlay loaded while borrowing them.
extern u8* gDryfieldNightMainStreetViewMaps[3];

/// Logical-view reverse-search counts for room selectors 1..3, indexed by room - 1.
///
/// Each signed halfword counts 24 mapping entries, independently of camera or
/// sprite capacities. No terminator; this borrowed array lives with the overlay.
extern ViewCount gDryfieldNightMainStreetViewCounts[3];

/// Seven warp endpoints selected by 1-based arrival/departure slots 1..7.
///
/// Placements use game-coordinate units and 4096 angle units per turn. Entries
/// supply initial logical views, stage-relative sounds and map-marker flags;
/// no spawn flags are set. No terminator. Keep this overlay loaded until the
/// transition has copied its selected entry and finished borrowing its placement.
extern DirectionWarpEntry gDryfieldNightMainStreetWarpEntries[7];

/// Cameras indexed by mapped view minus one for mapped indices 1..24.
///
/// Rotation coefficients use 4096 per unit; translations are negated world
/// origins in game-coordinate units. Projection distances are pixels. There is
/// no terminator; keep this overlay loaded while a camera record is borrowed.
extern ViewCamera gDryfieldNightMainStreetViewCameras[24];

/// Sprite resources indexed by mapped view minus one for mapped indices 1..24.
///
/// Views 13 and 24 share one source/batch pair; empty views retain only decoded
/// backgrounds. All clip-list pointers are NULL. Saved balcony states mutate
/// batch visibility. No view terminator; keep this overlay loaded while its
/// sources, mutable batches or cached sprite packets are in use.
extern SpriteView gDryfieldNightMainStreetSpriteViews[24];

/// Collision and footstep policies indexed by room-local surface class 0..7.
///
/// Classes 0 and 5..7 share a blocking silent policy; class 1 passes probes and
/// ignores weapon impacts without footsteps. Classes 2..4 block probes, allow
/// impacts and select their own footstep pairs. Every class applies pushback.
/// No terminator; the table and borrowed records live with this room overlay.
extern WorldCollisionSurfaceProperties* gDryfieldNightMainStreetSurfacePropertiesByClass[8];

/// Draws view-selected night main street lights and maintains its drifting puffs.
///
/// Requires loaded room anchor/mask tables, graphics state and a mapped view in
/// 1..24. Installs this room's four shared-effect callback IDs on the first tick,
/// updates the room-effect mode and suppresses the broken balcony lamp's shaft.
/// Views 8 and 19 seed 48 puffs on entry, then add one on odd animation frames.
/// `spawnArg1.value` stores the previous mapped view (initialize it differently
/// to force entry seeding). The controller remains live until external teardown;
/// independently counted puffs use the ordinary room effect pause/cancel rules.
void dryfieldNightMainStreetAmbientEffectsTask(Task* task);

/// Animates and drifts one nighttime main street puff (effect 0x601B2).
///
/// Requires the counted effect task and zeroed `EffectWork` created by
/// `effectSpawn`, with a live coordinate body and work in `spawnArg2.pointer`.
/// `spawnArg1.value` packs size factor in bits 0-11 (0..4095), cell period in bits
/// 12-14 (1..7 ticks), and speed in bits 16-23 (coordinate units per tick).
/// A zero period nibble (bits 12-15) selects one tick; a zero speed byte selects
/// 64. A nonzero period nibble must have nonzero bits 12-14; bit 15 alone
/// decodes to zero and is invalid. Bits 24-31 are ignored.
///
/// Initializes a fixed random screen angle (4096 units per turn) and a drift
/// with nonpositive local X, zero Y and signed Z. Draws before moving; cell zero
/// lasts one tick and cells 1-9 each last the selected period. Releases its work
/// and task after 1 + 9 * period ticks. Normal scheduling must compose the dirty
/// coordinate between ticks; callers must discard the work pointer on release.
void dryfieldNightMainStreetPuffTask(Task* task);

/// Runs this room's animated mote with rising or steady vertical motion and a fade.
///
/// Requires the counted task and zeroed `EffectWork` created by `effectSpawn`,
/// with a live coordinate body and owned work in `spawnArg2.pointer`.
/// `spawnArg1` packs half-extent in bits 0..11 (0..4095 world units), palette
/// in bits 12..15 (0 default), unsigned speed in bits 16..23 (coordinate units
/// per active tick), and signed lifetime in bits 24..31 (active ticks).
/// Bits 0..1 overlap the extent: either bit selects steady motion, with bit 1
/// selecting upward motion. Otherwise it rises at speed plus a random 0..63.
/// The first active tick only initializes; later ticks move along local Y and
/// draw on odd ages. Brightness fades by 16 after age exceeds lifetime minus 8;
/// the following tick releases the work and task once brightness is zero.
/// Nonzero room effect control pauses it; control 4 or above cancels it.
/// The controller and coordinate ancestors must stay live until release;
/// callers must discard the work pointer when the effect ends.
void dryfieldNightMainStreetRoomVisualEffectsMoteTask(Task* task);

/// Runs this room's expanding tinted halo, shrinking ring and fading star.
///
/// Requires the counted task and zeroed `EffectWork` created by `effectSpawn`,
/// with a live coordinate body and owned work in `spawnArg2.pointer`.
/// Initialization attaches to the work's borrowed parent at its saved local
/// offset in game coordinate units. `spawnArg1.halves.low` supplies 1..65535
/// expansion ticks; `halves.high` selects tint row 0..2. Initialization replaces
/// the argument word with the remaining ticks. Brightness and world-unit disc
/// radius grow by integer 256 / duration each active tick, then the star fades.
/// State 3 requests release. Nonzero room effect control pauses all phases,
/// including that request; control 4 or above cancels immediately.
/// Completion or cancellation releases the work and task. The controller and
/// borrowed coordinate parent must stay live until release; callers must
/// discard the work pointer when the effect ends.
void dryfieldNightMainStreetRoomVisualEffectsHaloTask(Task* task);

/// Runs this room's growing orange disc and layered glow inside a fading ring.
///
/// Requires the counted task and zeroed `EffectWork` created by `effectSpawn`,
/// with a live coordinate body and owned work in `spawnArg2.pointer`.
/// `spawnArg1` is unused. The first active tick initializes and draws; later
/// ticks expand the burst and fade its ring before fading the centre.
/// Nonzero room effect control pauses it; control 4 or above cancels it.
/// Completion or cancellation releases the work and task. The controller and
/// coordinate ancestors must stay live until release; callers must discard
/// the work pointer when the effect ends.
void dryfieldNightMainStreetRoomVisualEffectsHaloOrangeBurstTask(Task* task);

/// Emits this room's twenty turning, successively higher descending motes.
///
/// Requires a coordinate body and zero-aged counted `EffectWork` from
/// `effectSpawn` in `spawnArg2.pointer`; `spawnArg1` is ignored. Active ages
/// 1..20 emit at a local radial offset near 768 units and Y = -128 * age.
/// Motes descend eight units per active tick and live for 48 ticks independently.
/// This room must have installed its mote callback ID and keep it loaded.
/// Nonzero room effect control below four pauses; four or above cancels.
/// Age 21 or cancellation releases the emitter's work and task.
void dryfieldNightMainStreetRoomVisualEffectsSparkEmitterTask(Task* task);

/// Dispatches the night main street's room-message receiver task.
///
/// Requires a live bodyless task in state 0..2: initialize, idle, release.
/// Initialization restores saved sprite visibility and registers a borrowed
/// room-task slot. Keep the map and room callbacks loaded and stop messaging
/// before teardown, which leaves that slot unchanged. Spawn arguments are unused.
void dryfieldNightMainStreetRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_MAIN_STREET_H
