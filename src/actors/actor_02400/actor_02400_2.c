#include "common.h"

#include "actors/actors_shared_801351d4.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/gfx.h"

#include <psyq/libgte.h>
#include <psyq/inline_c.h>

extern s32 D_80115728;
extern s32 Gp_LcgState;

/// Set nonzero while the game is paused / in a menu; the actor's tick handler
/// is skipped in that state.
extern u8  D_801153F4;
extern s32 D_80115754;

void Gp_UpdateCoord(GsCOORDINATE2* arg0);
void Actor02400_Fn00064(GsCOORDINATE2* arg0, s32 arg1);
s32  func_800E1B24(s32 arg0);

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 Actor02400_D00004;
extern GpU16Pair             Actor02400_BodyPairs;

void Actor02400_Fn02DB0(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor02400_D00004;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}
