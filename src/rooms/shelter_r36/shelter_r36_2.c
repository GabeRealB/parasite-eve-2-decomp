#include "shelter_r36_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "gameplay/hud_sprites.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/task_types.h"
#include "../../shared/streamed_scene.h"

#include "../../shared/streamed_scene_play.inc.c"

/// Hands frame presentation and the current view to R36's movie player.
///
/// Forwards the launcher's first task word unchanged; the selected player
/// ignores it. Requires loaded movie, camera and sprite resources until the
/// player restores the game loop. A failed spawn still selects task-only
/// flipping and queues the current view.
static inline void _shelterR36HandoffMovieDisplay(s32 spawnWord)
{
    enum { SHELTER_R36_MOVIE_PLAYER_TASK = 1 };

    displaySpawnTaskFromTable(D_shelter_r36_8017E9A4, SHELTER_R36_MOVIE_PLAYER_TASK, spawnWord, 0);
    gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
    viewQueueCurrentCameraAndPackets();
}

void shelterR36StartMovieTask(Task* task)
{
    _shelterR36HandoffMovieDisplay(task->spawnArg1.value);
    taskKill(task);
}
