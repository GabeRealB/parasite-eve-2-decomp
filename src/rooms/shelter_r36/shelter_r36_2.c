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

/// Entry 0 of `D_shelter_r36_8017E9A4`: spawns that table's entry 1, the
/// stream task `streamedScenePlay`, with an ordering table, passing on
/// this task's `spawnArg1`, sets `gDisplayState.control.flags.flipMode`, spawns the view tasks and ends.
void func_shelter_r36_8017DBC0(Task* arg0)
{
    displaySpawnTaskFromTable(D_shelter_r36_8017E9A4, 1, arg0->spawnArg1.value, 0);
    gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
    Gp_SpawnViewTasks();
    taskKill(arg0);
}
