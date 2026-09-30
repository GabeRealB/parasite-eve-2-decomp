#include "kyle/kyle_800102.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "types.h"

#include "kyle_800102_private.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/loading.h"
#include "gameplay/player_actor.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/world_collision.h"

#include "main/coord.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "weapons/weapon.h"
#include "../../shared/grenade_shell.h"

static void func_kyle_800102_80168244(Task* arg0);

#include "../../shared/grenade_shell_spawn.inc.c"

#include "../../shared/grenade_shell_fly.inc.c"

static void func_kyle_800102_80168244(Task* arg0)
{
    WeaponGrenadeWork* work  = (WeaponGrenadeWork*)arg0->work;
    s32                timer = work->field_88.w - 1;

    work->field_88.w = timer;
    if (timer <= 0) {
        arg0->state = 3;
    }
}

#include "../../shared/grenade_shell_exit.inc.c"
void func_kyle_800102_801682B4(Task* task)
{
    TaskFunc states[4] = {
        grenadeShellSpawn,
        grenadeShellFly,
        func_kyle_800102_80168244,
        grenadeShellExit,
    };

    states[task->state](task);
}
