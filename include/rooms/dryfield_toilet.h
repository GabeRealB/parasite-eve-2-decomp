#ifndef INCLUDE_ROOMS_DRYFIELD_TOILET_H
#define INCLUDE_ROOMS_DRYFIELD_TOILET_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"
#include "overlay.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern ModelMorph D_dryfield_toilet_801865D0;

extern AreaVariant D_dryfield_toilet_80182918[13];

// dryfield_toilet
extern WorldCollisionRoomResources D_dryfield_toilet_8018112C[];

extern u8* D_dryfield_toilet_8018113C[];

extern ViewCount D_dryfield_toilet_80181140[];

extern WorldCoordRoomLighting D_dryfield_toilet_80181144[];

extern DirectionWarpEntry D_dryfield_toilet_8018114C[];

extern ViewCamera D_dryfield_toilet_80181428[];

extern SpriteView D_dryfield_toilet_801821F8[];

extern WorldCollisionSurfaceProperties* D_dryfield_toilet_8018660C[];

/// Draws a randomly rotated six-frame puff, then advances its optional falling motion.
///
/// `spawnArg2.pointer` owns an `EffectWork`; the extra body supplies its coordinate.
/// `spawnArg1.value` packs world-size in bits 0..11 and ticks per texture frame
/// in bits 12..15 (0 means 1; valid nonzero periods are 1..7). Bit 20 replaces
/// `move` with random components in [-16, 15]; bit 24 scales `move` by size/1024.
/// Rotation and motion are initialized only at projected depth SZ3/4 >= 17.
/// Drawing continues while actors are paused; motion and age pause. Cancellation
/// or six completed texture frames release the work and task. An initially
/// hidden puff retains its zero period and expires on its first unpaused tick.
/// Requires an initialized scratch stack and one `POLY_FT4` in the frame arena;
/// even a clipped puff consumes its packet, which lives until GPU completion.
void dryfieldToiletJetPuffTask(Task* task);

/// Registers this room's glow-disc, flying-spark and orange-burst effect IDs once.
///
/// Bank-6 slot 0xCE changes state 0 to 1; later ticks leave the registrations
/// alone. The callback leaves the task alive after registration.
void dryfieldToiletConfigureEffectsTask(Task* task);

void func_dryfield_toilet_8017E69C(Task* arg0);

/// Runs the room's spark toward a target's initial position for twenty active ticks.
///
/// `spawnArg1.pointer` borrows a composed `GfxCoord` for the first active tick;
/// `spawnArg2.pointer` owns an `EffectWork`. The task's composed coordinate fixes
/// a step of 204/4096 of the initial separation, with 16-bit intermediate
/// components; later ticks use that step without resampling the target. The
/// spark draws on odd ages after initialization. Room effect control pauses
/// at nonzero and cancels at four or above; expiry and cancellation release it.
void dryfieldToiletFlyingSparkTask(Task* task);

/// Runs the room's expanding orange disc and glow behind a fading outer ring.
///
/// `spawnArg2.pointer` owns an `EffectWork`; the extra body supplies its coordinate.
/// Each active tick grows the glow by 16 world units. The ring fades first,
/// then the central brightness falls until the work and task are released.
/// Room effect control pauses at nonzero and cancels at four or above.
void dryfieldToiletFlyingOrangeBurstTask(Task* task);

void func_dryfield_toilet_8017DCF0(Task* arg0);

/// Runs the Dryfield toilet room task's entry, idle or teardown state.
///
/// The stage map spawns this task for area 16. Requires state 0..2: entry
/// installs the room message table and claims `GAME_TASK_SLOT_ROOM`, idle
/// keeps receiving messages, and state 2 releases the task. Keep this room
/// overlay loaded throughout dispatch and while the task can receive messages.
void dryfieldToiletRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_TOILET_H
