#include "shelter_b6_training_room_private.h"

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

#include "../../shared/streamed_scene_play_then_hold.inc.c"

/// Hands frame presentation and the current view to the room's movie player.
static inline void _shelterB6TrainingRoomHandoffMovieDisplay(void)
{
    enum { SHELTER_B6_TRAINING_ROOM_MOVIE_PLAYER_TASK = 1 };

    displaySpawnTaskFromTable(D_shelter_b6_training_room_8018431C, SHELTER_B6_TRAINING_ROOM_MOVIE_PLAYER_TASK, 0, 0);
    gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
    viewQueueCurrentCameraAndPackets();
}

void shelterB6TrainingRoomStartMovieTask(Task* task)
{
    _shelterB6TrainingRoomHandoffMovieDisplay();
    taskKill(task);
}
