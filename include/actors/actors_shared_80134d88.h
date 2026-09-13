#ifndef ACTORS_SHARED_80134D88_H
#define ACTORS_SHARED_80134D88_H

#include "common.h"

#include "main/task.h"

/// Arms the gameplay-side action prompt for a hotspot at the fixed target id
/// 0x80, marks it highlighted and resets the on-screen position, then steps the
/// caller's script on one state.
///
/// Shared verbatim by `actor_143000` and `actor_548100`; the room overlays
/// carry the same body as the shared `Room_Util14`.
void ActorsShared80134d88(Task* task);

#endif
