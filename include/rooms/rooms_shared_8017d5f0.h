#ifndef ROOMS_SHARED_8017D5F0_H
#define ROOMS_SHARED_8017D5F0_H

#include "common.h"

#include "main/task.h"

/// The descriptor the reflection tasks are spawned from, one per carrying room.
extern TaskDesc RoomsShared8017d5f0Desc;

/// The mirror's per-frame update, still one copy per carrying room.
void RoomsShared8017d7a4(Task* task);

/// Sets up a room's mirror: re-attaches the player's own TMD source to this
/// task so the reflection draws the same model, allocates the `RoomMirrorWork`
/// block the reflection's coordinate frame and matrices live in, and hangs the
/// task off the player task so it dies with it. `spawnArg1` selects which of
/// the room's two mirrors this is; mirror 0 also raises `GameSession::field_4E`.
/// The two child tasks mirror the player's held-object tasks. Six rooms carry
/// this body.
void RoomsShared8017d5f0(Task* task);

#endif // ROOMS_SHARED_8017D5F0_H
