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
extern GpEnemyTaskFuncTable3 Actor02400_D00004;

INCLUDE_ASM("actors/nonmatchings/actor_02400/actor_102400", Actor02400_Fn00064);

INCLUDE_ASM("actors/nonmatchings/actor_02400/actor_102400", Actor02400_Fn005BC);

INCLUDE_RODATA("actors/nonmatchings/actor_02400/actor_102400", Actor02400_D00004);

extern u32 Gp_LcgState;
extern u16 Actor02400_D045B8;
extern u16 Actor02400_D045C8;
extern s16 Actor02400_D045D8[];
extern s16 Actor02400_D045DC[];
extern s16 Actor02400_D0463C[];
