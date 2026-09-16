#ifndef ACTORS_SHARED_80138D58_H
#define ACTORS_SHARED_80138D58_H

#include "common.h"

#include "actors/actors_shared_80138efc.h"
#include "gameplay/1BC.h"
#include "main/task.h"

/// Session word whose bit 0 the frame block's byte at 0x64 follows.
extern s32 D_80070F70;

/// Countdown handler every carrier lists fourth in its 26-entry state table:
/// the first frame arms motion 5 and the 0x64-frame countdown, later frames
/// step it and cue the 0x400B0004 event, and the trigger at 0xBA9 ends the
/// sub-state. The work block and the frame block are the family's, so the types
/// live in `actors_shared_80138efc.h`.
///
/// Five actor overlays carry this body, each in its `_3` unit, between the
/// 0x...8B5C / 0x...8C6C pair and the 0x...8E34 sub-state handler:
/// `actor_101100` and `actor_104900` at 0x80138D58 (byte-identical), and
/// `actor_201100`, `actor_204900` and `actor_301100` at 0x80150D58, 0x80150D58
/// and 0x80168D58.
void ActorsShared80138d58(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg);

#endif // ACTORS_SHARED_80138D58_H
