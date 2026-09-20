#include "common.h"

#include "actors/actor_102400_fn0208c.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"

#include <psyq/libgte.h>

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_102400_80131E24;

INCLUDE_ASM("actors/nonmatchings/actor_102400/actor_102400", func_actor_102400_80131E84);

INCLUDE_ASM("actors/nonmatchings/actor_102400/actor_102400", func_actor_102400_801323DC);

INCLUDE_RODATA("actors/nonmatchings/actor_102400/actor_102400", D_actor_102400_80131E24);

extern u32 Gp_LcgState;
extern u16 D_actor_102400_801363D8;
extern u16 D_actor_102400_801363E8;
extern s16 D_actor_102400_801363F8[];
extern s16 D_actor_102400_801363FC[];
extern s16 D_actor_102400_8013645C[];
