#ifndef INCLUDE_ROOMS_NEO_ARK_NORTH_PROMENADE_H
#define INCLUDE_ROOMS_NEO_ARK_NORTH_PROMENADE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_neo_ark_north_promenade_8018305C[13];

// neo_ark_north_promenade
extern WorldCollisionRoomResources D_neo_ark_north_promenade_80181DB4[];

extern WorldCoordRoomLighting D_neo_ark_north_promenade_80181DC4[];

extern u8* D_neo_ark_north_promenade_80181DCC[];

extern ViewCount D_neo_ark_north_promenade_80181DD0[];

extern DirectionWarpEntry D_neo_ark_north_promenade_80181DD4[];

extern ViewCamera D_neo_ark_north_promenade_80182410[];

extern SpriteView D_neo_ark_north_promenade_80182CA4[];

extern WorldCollisionSurfaceProperties* D_neo_ark_north_promenade_801832EC[];

/// Runs the room's charging pink flash, peak screen tint and fading star.
///
/// Starts in state zero with a coordinate body and counted `EffectWork` in
/// `spawnArg2.pointer` from `effectSpawn`. `spawnArg1.value` is a positive
/// charge duration in active ticks, consumed as a countdown. Nonzero room
/// effect control pauses it; values at least four cancel it. State three also
/// requests release. Completion frees the effect work and tears down the task.
/// Requires a live `gRoomEffectState` and the room overlay to remain loaded.
void neoArkNorthPromenadeRoomVisualEffectsFlashTask(Task* task);

/// Runs the room's fading beam between two moving endpoint histories.
///
/// Starts in state zero with a coordinate body and counted `EffectWork` in
/// `spawnArg2.pointer` from `effectSpawn`. Borrows its parent coordinate while
/// live and owns two eight-coordinate histories in `Task::work`, freed during
/// teardown. Allocation failure retries with age reset to zero. Initialization
/// counts as the first active tick; later ticks record world-space endpoints
/// and draw seven quads with red:green:blue intensities in the ratio 1:2:3.
/// `spawnArg1.value` is zero for external teardown, or 2..32767 to expire at
/// that active age. Room effect control at two or above holds age and drawing
/// without cancelling. Requires a live `gRoomEffectState` and loaded room overlay.
void neoArkNorthPromenadeRoomVisualEffectsTwinTrailTask(Task* task);

void func_neo_ark_north_promenade_80181120(Task* task);

/// Runs the room's animated mote, rising and brightening or drifting steadily before fading.
///
/// Starts in state zero with a coordinate body and zeroed, counted `EffectWork`
/// in `spawnArg2.pointer` from `effectSpawn`. `spawnArg1.value` packs half-extent
/// in bits 0..11 (world units), palette in bits 12..15 (zero selects the default),
/// speed in bits 16..23 (unsigned local-Y units per active tick), and lifetime
/// in bits 24..31 (signed active ticks). The overlapping low two extent bits
/// select steady motion when either is set; bit one makes it upward. Otherwise
/// it rises at the supplied speed plus a random 0..63. The first tick initializes
/// without drawing; later ticks draw on odd ages and start fading after age
/// exceeds lifetime minus eight. Nonzero room effect control pauses it;
/// values at least four cancel it.
/// Completion releases the work and task. Keep the room overlay loaded while live.
void neoArkNorthPromenadeRoomVisualEffectsMoteTask(Task* task);

/// Runs the room's expanding tinted halo and shrinking ring, then a fading star.
///
/// Starts in state zero with a coordinate body and zeroed, counted `EffectWork`
/// in `spawnArg2.pointer` from `effectSpawn`. Its borrowed parent coordinate must
/// remain live; the copied spawn offset is in that parent's local coordinate units.
/// The signed halves of `spawnArg1` supply a positive expansion duration in
/// active ticks (low) and tint-row index 0..2 (high). Initialization replaces
/// the packed word with a countdown. Nonzero room effect control pauses every
/// phase; values at least four cancel it. State three requests release.
/// Completion releases the work and task. Keep the room overlay loaded while live.
void neoArkNorthPromenadeRoomVisualEffectsHaloTask(Task* task);

/// Runs the room's growing orange disc and layered glow inside a fading ring.
///
/// Starts in state zero with a coordinate body and counted `EffectWork` in
/// `spawnArg2.pointer` from `effectSpawn`; `spawnArg1` is unused. Draws on the
/// first active tick and fades the ring before the central glow. Nonzero room
/// effect control pauses it; values at least four cancel it. Completion releases
/// the work and task. Keep the room overlay loaded while live.
void neoArkNorthPromenadeRoomVisualEffectsHaloOrangeBurstTask(Task* task);

void func_neo_ark_north_promenade_8017FCA0(Task* arg0);

/// Binds the room's seven combat-effect IDs once, then leaves the task idle.
///
/// Starts in state zero; later ticks leave the bindings untouched. The bindings
/// select this loaded overlay's mote, halo, orange burst,
/// spark emitter, flash, twin trail and spark burst tasks. Takes no spawn payload.
void neoArkNorthPromenadeBindRoomEffectsTask(Task* task);

void func_neo_ark_north_promenade_8017D6C8(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_NORTH_PROMENADE_H
