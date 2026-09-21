#ifndef ACTORS_SHARED_8013845C_SUB0_H
#define ACTORS_SHARED_8013845C_SUB0_H

#include "common.h"

#include "main/task.h"

/// Overlay-local state 0 of `ActorsShared8013845c` for `actor_143000` and
/// `actor_548100`: resets both action-prompt slots before a script's first
/// cursor scan and steps the caller on one state. Clears each slot's leading
/// words and its two trailing shorts, parks the target id at 0x100 with
/// `field_E` at 0xF, and marks the slot as highlighted (`mode` 1).
///
/// Shared verbatim by those two overlays; the 101100-family slots keep a
/// different overlay-local spawn body under the same name. Room overlays
/// carry this body as the shared `Room_Util04`.
void ActorsShared8013845cSub0(Task* task);

#endif
