#ifndef INCLUDE_ROOMS_MINE_SECRET_PASSAGE_H
#define INCLUDE_ROOMS_MINE_SECRET_PASSAGE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_mine_secret_passage_80183340[12];

// mine_secret_passage
extern WorldCoordRoomLighting D_mine_secret_passage_80180F9C[];

extern WorldCollisionRoomResources D_mine_secret_passage_80180FA4[];

extern u8* D_mine_secret_passage_80180FB4[];

extern ViewCount D_mine_secret_passage_80180FB8[];

extern DirectionWarpEntry D_mine_secret_passage_80180FBC[];

extern ViewCamera D_mine_secret_passage_80181604[];

extern SpriteView D_mine_secret_passage_80182994[];

extern WorldCollisionSurfaceProperties* D_mine_secret_passage_80183420[];

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

void func_mine_secret_passage_80180D58(Task* arg0);

/// Registers the passage's shared effect IDs and draws its visible light glows.
///
/// State 0 installs the mote, halo, orange-burst and spark-emitter IDs once.
/// Every tick draws additive capsule and disc glows for mapped camera views
/// 2..8; view 1 draws nothing. Requires the loaded passage, composed view matrix,
/// initialized scratch stack and current frame's ordering table and packet arena.
/// The coordinate body and spawn arguments are unused; the task remains live.
void mineSecretPassageDrawLightGlowsTask(Task* task);

void func_mine_secret_passage_8017D970(Task* task);

#endif // INCLUDE_ROOMS_MINE_SECRET_PASSAGE_H
