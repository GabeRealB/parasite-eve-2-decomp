#ifndef INCLUDE_ACTORS_ACTOR_136100_H
#define INCLUDE_ACTORS_ACTOR_136100_H

#include "main/task_types.h"

/// Gary Douglas night Main Street spawn recipes: body, unused, head, rifle, fade-in, fade-out.
///
/// All six slots have priority `TASK_PRIORITY_CONTROL` and no terminator; never spawn unused slot 1.
/// Slots 0, 2 and 3 create TMD bodies with manually allocated primitive buffers.
/// Head/rifle argument 2 borrows a live parent coordinate; nonzero rifle argument
/// 1 selects its attached pose. Fade slots 4/5 are bodyless and take an unsigned
/// low-halfword intensity increment per tick in argument 1, normally 9.
/// Keep actor_136100 and the night Main Street scene resources loaded while
/// these tasks run; head and rifle initialization require the live body singleton.
extern TaskDesc gActor136100SceneTaskDescriptors[6];

#endif // INCLUDE_ACTORS_ACTOR_136100_H
