#ifndef SRC_ROOMS_SHELTER_R36_SHELTER_R36_PRIVATE_H
#define SRC_ROOMS_SHELTER_R36_SHELTER_R36_PRIVATE_H

#include "main/task_types.h"

/// Zero-body movie task recipes: index 0 launcher, index 1 streamed player.
///
/// Both use priority 192. The room's ending task spawns the launcher; the other
/// room translation unit hands display presentation to the player. Borrow this
/// overlay-private table and keep its callbacks and movie resources loaded
/// through playback and display restoration. There is no terminator.
extern TaskDesc gShelterR36MovieTasks[2];

// Callbacks referenced by the overlay's shared data tables.

/// Gives display presentation to R36's movie player, then releases the launcher.
///
/// Launches descriptor 1, forwarding `spawnArg1.value` unchanged and supplying
/// a zero second word. Selects task-only flipping and queues the current camera
/// and sprite packets. Requires loaded session, camera, sprite and movie resources
/// through playback/restoration. Launch failure is ignored and still releases
/// `task`. Neither this launcher nor the selected player allocates task work.
void shelterR36StartMovieTask(Task* task);

#endif // SRC_ROOMS_SHELTER_R36_SHELTER_R36_PRIVATE_H
