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

/// One-shot task: spawns the stream player, the second entry of the room's
/// descriptor pair, as the display's owning task, sets `gDisplayState.control.flags.flipMode`, spawns
/// the view tasks and kills itself.
void func_shelter_b6_training_room_8017DD98(Task* arg0)
{
    displaySpawnTaskFromTable(D_shelter_b6_training_room_8018431C, 1, 0, 0);
    gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
    viewQueueCurrentCameraAndPackets();
    taskKill(arg0);
}
