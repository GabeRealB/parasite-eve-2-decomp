#ifndef SRC_ROOMS_SHELTER_R36_SHELTER_R36_PRIVATE_H
#define SRC_ROOMS_SHELTER_R36_SHELTER_R36_PRIVATE_H

#include "main/task_types.h"

extern TaskDesc D_shelter_r36_8017E9A4[2];

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
