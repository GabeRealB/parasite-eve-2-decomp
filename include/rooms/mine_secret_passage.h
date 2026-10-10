#ifndef INCLUDE_ROOMS_MINE_SECRET_PASSAGE_H
#define INCLUDE_ROOMS_MINE_SECRET_PASSAGE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

/// Passage actor layouts indexed directly by placement variant 0..11.
///
/// Variants 1, 2, 3 and 11 contain one Ivory Stalker, two Zebra Stalkers,
/// two Brain Stingers and three Zebra Stalkers respectively; other slots are
/// absent. Lists end with `AREA_PLACEMENT_END`. Gameplay's stage-4 area-8 directory
/// borrows this table; placements stay loaded until their actors are released.
extern AreaVariant gMineSecretPassageAreaVariants[12];

/// Lighting for passage room 1, indexed by room minus one.
///
/// Borrows ten mutable point lights and minimum ambient colours for views 1..8.
/// The Shelter map borrows this directory while the passage overlay is loaded.
extern WorldCoordRoomLighting gMineSecretPassageRoomLighting[1];

/// Collision resources for passage room 1, indexed by room minus one.
///
/// Borrows the grid, ten directed view boundaries, three facing-gated actions
/// and two sight occluders. Setup mutates coordinates, links and enabled bits;
/// the room overlay must stay loaded until those resources are unlinked.
extern WorldCollisionRoomResources gMineSecretPassageRoomCollisionResources[1];

/// Logical-view mapping for passage room 1, indexed by room minus one.
///
/// Borrows `gViewIdentityMap`: logical views 1..8 select camera/image/sprite
/// indices 1..8 at view minus one, without a terminator. The directory requires
/// the passage loaded; the shared byte map requires gameplay loaded.
extern u8* gMineSecretPassageRoomViewMaps[1];

/// Reverse-lookup search count for passage room 1, indexed by room minus one.
///
/// Searches eight logical-view bytes in `gViewIdentityMap`. This signed halfword
/// count is independent of that map's capacity and the nine camera records.
/// The Shelter map borrows this directory while the passage overlay is loaded.
extern ViewCount gMineSecretPassageRoomViewCounts[1];

/// Passage warp endpoints 1 (cavern) and 2 (elevator hall), indexed by warp minus one.
///
/// Supplies player/companion placements in world units and yaw in 4096 per turn,
/// arrival views 2/6, and area-bank arrival/departure sounds. Endpoint 1 marks
/// the cavern map flag; endpoint 2 requests a departure fade. There is no
/// terminator. The Shelter map borrows both records while the passage is loaded.
extern DirectionWarpEntry gMineSecretPassageWarpEntries[2];

/// Nine passage camera records, indexed by mapped camera minus one.
///
/// Normal logical views 1..8 select the first eight; record 9 repeats camera 2.
/// Transforms store Q12 rotations and negated world origins; projection distance
/// is in pixels. The Shelter map borrows this unterminated table while loaded.
extern ViewCamera gMineSecretPassageViewCameras[9];

/// Sprite resources for passage mapped views 1..8, indexed by view minus one.
///
/// Views 1, 2 and 8 retain their empty batch-list addresses as source slots.
/// Views 3..7 borrow mutable sprite sources. Every batch list begins with a
/// zero count, retaining decoded background strips, and ends at `SPRITE_BATCH_END`.
/// No view has clipping commands. The directory and lists require the passage loaded.
extern SpriteView gMineSecretPassageSpriteViews[8];

/// Passage collision policies indexed directly by surface class 0..7.
///
/// Class 1 passes probes and suppresses weapon impacts; other classes block
/// probes and allow impacts. Every class applies pushback. Classes 2 and 3
/// supply distinct footstep cues; classes 0, 1 and 4..7 are silent. The directory,
/// borrowed records and cues require the passage overlay to remain loaded.
extern WorldCollisionSurfaceProperties* gMineSecretPassageSurfacePropertiesByClass[8];

/// Runs the passage's expanding orange disc and glow inside a fading ring.
///
/// Requires the coordinate body and owned `EffectWork` supplied by `effectSpawn`;
/// `spawnArg1` is unused. Nonzero room effect control pauses it; four or above
/// cancels it. Completion or cancellation releases work and task. The passage
/// overlay must stay loaded.
void mineSecretPassageRoomVisualEffectsHaloOrangeBurstTask(Task* task);

/// Runs the passage's expanding tinted halo, shrinking ring and fading star.
///
/// Requires a coordinate body and owned, zero-initialized `EffectWork` in
/// `spawnArg2.pointer`. The signed low half of `spawnArg1` is a positive duration
/// in active ticks; the signed high half selects tint row 0..2. The borrowed
/// parent coordinate and passage overlay must stay live. Nonzero room effect
/// control pauses it; four or above cancels it. Completion or cancellation
/// releases work and task.
void mineSecretPassageRoomVisualEffectsHaloTask(Task* task);

/// Runs the passage's vertically drifting animated mote until it fades.
///
/// Requires the coordinate body and owned, zero-initialized `EffectWork` supplied
/// by `effectSpawn`. `spawnArg1` packs the world-unit half-extent in bits 0..11,
/// palette in bits 12..15, unsigned world-unit speed per active tick in bits
/// 16..23 and signed lifetime in active ticks in bits 24..31.
/// Motion bits 0..1 overlap the half-extent:
/// either selects steady motion, with bit 1 selecting upward motion; neither
/// selects a brightening rise with added random speed. Initialization draws
/// nothing; later ticks draw on odd ages. Nonzero room effect control pauses it;
/// four or above cancels it. Completion or cancellation releases work and task.
/// Borrowed coordinate ancestors and the passage overlay must stay live.
void mineSecretPassageRoomVisualEffectsMoteTask(Task* task);

/// Runs the passage's twenty-emission rotating-offset mote emitter.
///
/// Requires the coordinate body and owned, zero-initialized `EffectWork` supplied
/// by `effectSpawn`; spawnArg1 is unused. Ages 1..20 emit independent downward
/// motes at rotating local offsets; age 21 releases the emitter work and task.
/// Motes descend eight parent-axis units per active tick and can outlive it.
/// Nonzero room effect control pauses emission; four or above cancels it.
/// The installed mote callback, borrowed coordinate ancestors and passage
/// overlay must stay live for the tasks that use them.
void mineSecretPassageRoomVisualEffectsSparkEmitterTask(Task* task);

/// Registers the passage's shared effect IDs and draws its visible light glows.
///
/// State 0 installs the mote, halo, orange-burst and spark-emitter IDs once.
/// Every tick draws additive capsule and disc glows for mapped camera views
/// 2..8; view 1 draws nothing. Requires the loaded passage, composed view matrix,
/// initialized scratch stack and current frame's ordering table and packet arena.
/// The coordinate body and spawn arguments are unused; the task remains live.
void mineSecretPassageDrawLightGlowsTask(Task* task);

/// Runs the passage's room-message controller and one-time CAP event.
///
/// Requires a live task with state 0..3 and the passage overlay loaded. State 0
/// registers the room task and selects countdown-music entry 1; state 1 marks
/// an unseen intro and attempts CAP command 3; state 2 idles; state 3 kills the
/// task. The body and spawn arguments are unused. The message table remains
/// borrowed while registered; senders must stop using the room task before it
/// is killed, since teardown does not clear its resident slot.
void mineSecretPassageRoomTask(Task* task);

#endif // INCLUDE_ROOMS_MINE_SECRET_PASSAGE_H
