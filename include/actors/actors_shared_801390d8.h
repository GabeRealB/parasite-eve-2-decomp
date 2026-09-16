#ifndef ACTORS_SHARED_801390D8_H
#define ACTORS_SHARED_801390D8_H

#include "common.h"

#include "actors/actors_shared_80138efc.h"
#include "gameplay/1BC.h"
#include "main/task.h"

/// Countdown handler the family's state table lists on its own: the first frame
/// arms the motion id at 0xBA4, `field_BAF` and the 0xB8C countdown, later
/// frames step that countdown and clear `field_BAF` when it lands on 0x28 or
/// 0x3C, and the trigger at 0xBA9 ends the sub-state. The work block and the
/// frame block are the family's, so the types live in
/// `actors_shared_80138efc.h`.
///
/// Five actor overlays carry this body, each in its `_5` unit right after the
/// 0x...8F68 / 0x...0F68 handler: `actor_101100` and `actor_104900` at
/// 0x801390D8 (byte-identical), and `actor_201100`, `actor_204900` and
/// `actor_301100` at 0x801510D8, 0x801510D8 and 0x801690D8.
void ActorsShared801390d8(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, ActorsShared80138efcArg* arg);

#endif // ACTORS_SHARED_801390D8_H
