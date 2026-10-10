#ifndef INCLUDE_ROOMS_MINE_CAVERN_H
#define INCLUDE_ROOMS_MINE_CAVERN_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_mine_cavern_8018E238[22];

// mine_cavern
extern WorldCollisionRoomResources D_mine_cavern_80188FE0[];

/// Lighting for cavern rooms 1..3, indexed by room minus one.
///
/// All rooms borrow the same eight point lights and use the default minimum
/// ambient colour. The directory and mutable lights remain live only while
/// the cavern overlay is loaded.
extern WorldCoordRoomLighting gMineCavernRoomLighting[3];

/// Logical-view maps for cavern rooms 1..3, indexed by room minus one.
///
/// Each map accepts logical views 1..25 at index view minus one and returns a
/// 1-based camera/image/sprite index in 1..25. Room 1 borrows the gameplay
/// identity map; rooms 2 and 3 borrow cavern-owned byte maps. These are counted
/// maps with no terminator. Cavern-owned storage requires this overlay loaded.
extern u8* gMineCavernRoomViewMaps[3];

/// Reverse-search limits for cavern rooms 1..3, indexed by room minus one.
///
/// Each signed-halfword count searches 25 logical-view entries. The counts
/// describe search lengths independently of map storage and resource capacity;
/// there is no terminator. Borrowed by the Shelter map while the cavern is loaded.
extern ViewCount gMineCavernRoomViewCounts[3];

extern DirectionWarpEntry D_mine_cavern_80189074[];

/// Cameras for cavern mapped view indices 1..25, indexed by mapped index minus one.
///
/// Rotation has 12 fractional bits, translation is the negated world-space
/// camera origin, and projection distance is in pixels. There is no terminator.
/// The Shelter map borrows this array; queued camera use requires it to stay loaded.
extern ViewCamera gMineCavernViewCameras[25];

/// Sprite resources for cavern mapped view indices 1..25, indexed by index minus one.
///
/// Views 5 and 25 share sources and batches. Views 1 and 11..19 retain their
/// empty batch-list address in the source slot without supplying source elements.
/// All batch lists begin with zero sprites, selecting decoded background strips,
/// and end at `SPRITE_BATCH_END`; clipping lists are NULL. Progress can change
/// batch visibility. The directory and borrowed arrays require the cavern loaded.
extern SpriteView gMineCavernSpriteViews[25];

/// Cavern collision policies indexed directly by surface class 0..7.
///
/// Class 1 blocks probes and supplies footstep cues; class 2 passes probes and
/// supplies the same cues. Classes 0 and 3..7 share a blocking, silent policy.
/// Every class allows weapon impacts and applies pushback. Borrowed records
/// and cues require the cavern overlay to remain loaded.
extern WorldCollisionSurfaceProperties* gMineCavernSurfacePropertiesByClass[8];

/// Runs the cavern's expanding orange disc, layered glow and fading ring effect.
///
/// Requires a coordinate-body task with owned `EffectWork` in
/// `spawnArg2.pointer`, as supplied by `effectSpawn`. Room effect control 0
/// runs it, 1..3 pause it, and 4 or above cancels it. Completion or cancellation
/// releases the effect work and task; its coordinate ancestors must stay live.
void mineCavernRoomVisualEffectsHaloOrangeBurstTask(Task* task);

/// Registers the cavern's room effects and draws its view-dependent light glows.
///
/// A live state-0 task registers four effect IDs and enables room-view effects;
/// later ticks draw flares/capsules and, while progress flag 0xC4 is 1, may emit
/// a localized flash. Requires the loaded cavern's point tables,
/// view, scratch stack and frame packet arena. This task retains no effect
/// work and does not free its coordinate body or task itself.
void mineCavernDrawRoomLightGlowsTask(Task* task);

/// Runs the cavern's vertically drifting animated mote effect.
///
/// Requires a coordinate-body task and zeroed owned `EffectWork` in
/// `spawnArg2.pointer`. `spawnArg1` packs world-unit half-extent in bits 0..11
/// (bits 0..1 also select steady motion, bit 1 upward), palette in bits 12..15
/// (0 default), unsigned speed in bits 16..23 and signed lifetime in bits
/// 24..31; speed is local-Y units per active tick, lifetime is active ticks.
/// Room effect control 1..3 pauses it; 4 or above cancels it. Completion or
/// cancellation releases the work and task; coordinate ancestors must stay live.
void mineCavernRoomVisualEffectsMoteTask(Task* task);

/// Runs the cavern's expanding tinted halo and shrinking ring, then a fading star.
///
/// Requires a coordinate-body task and zeroed owned `EffectWork` in
/// `spawnArg2.pointer`, with a borrowed parent and local world-unit offset.
/// `spawnArg1`'s signed low half is a positive expansion duration in active
/// ticks; its signed high half selects tint row 0..2. Initialization replaces
/// the argument with the remaining duration. Room effect control 1..3 pauses
/// it; 4 or above cancels it. Completion or cancellation releases work and task;
/// the parent and its ancestors must stay live until then.
void mineCavernRoomVisualEffectsHaloTask(Task* task);

/// Emits twenty motes from rotating offsets along the cavern effect's parent.
///
/// Requires a coordinate-body task and zeroed owned `EffectWork` in
/// `spawnArg2.pointer`, with a live borrowed parent. Each active tick emits a
/// mote that fades to zero at active age 48 and retires at 49, moving downward
/// eight local units per motion tick.
/// Its emission offset decreases local Y by 128 world units each active tick.
/// Control 1..3 pauses emission; 4 or above cancels it. At active age 21 or on
/// cancellation the emitter releases its work and task. Spawned motes copy
/// their placement, so they can finish after the emitter retires.
void mineCavernRoomVisualEffectsSparkEmitterTask(Task* task);

/// Runs the cavern's room controller: initialization, event polling, then teardown.
///
/// The live task's state must be 0..2, initially 0; dispatch copies the three
/// handlers by value and does not check bounds. Requires the cavern's scripts
/// and room resources to remain loaded, together with the live session and
/// gameplay APIs. State 2 releases the task.
void mineCavernRoomTask(Task* task);

#endif // INCLUDE_ROOMS_MINE_CAVERN_H
