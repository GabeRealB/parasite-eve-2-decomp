#include "rooms/dryfield_water_hole.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/effects.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gamemain.h"
#include "main/scratch.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

/// Empty presence flag so `water_effects.h` declares the shared
/// `_waterDrawSpinU16` and `_waterDrawTileU16`. This file includes their bodies.
#define WATER_SHARED_U16_DRAWERS
#include "../../shared/water_effects.h"

#include "../../shared/water_drift_task_u16.inc.c"

void func_dryfield_water_hole_8017F118(Task* task)
{
    waterDriftTaskU16(task);
}

#include "../../shared/water_spin_u16.inc.c"

#include "../../shared/water_tile_u16.inc.c"
