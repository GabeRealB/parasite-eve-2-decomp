#include "common.h"

#include "actors/actors_shared_801351d4.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "main/task.h"

#include <psyq/libgte.h>

extern s32 D_80115728;
extern s32 Gp_LcgState;

/// Set nonzero while the game is paused / in a menu; the actor's tick handler
/// is skipped in that state.
extern u8  D_801153F4;
extern s32 D_80115754;

void Gp_UpdateCoord(GsCOORDINATE2* arg0);
void func_actor_102400_80131E84(GsCOORDINATE2* arg0, s32 arg1);
s32  func_800E1B24(s32 arg0);

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_102400_80131E24;

INCLUDE_ASM("actors/nonmatchings/actor_102400/actor_102400_3", func_actor_102400_80134318);

INCLUDE_ASM("actors/nonmatchings/actor_102400/actor_102400_3", func_actor_102400_801345B0);

/// Per-frame tick of the state-1 handler: it walks the enemy along the step
/// held in the work block's `field_A8` / `field_AC`, and once the death timer
/// `field_B0` runs out (or the collision record `field_90` names an occupied
/// room slot) spawns the death effect and moves the task to state 2.
///
/// `arg0` is the enemy the dispatcher passes first; the task itself is `arg1`
/// (`GpEnemyTaskFunc`).
void func_actor_102400_80134910(GpEnemy* arg0, Task* arg1)
{
    ActorsShared801351d4Work* work;
    GsCOORDINATE2*            coord;
    GpRoomParamRec*           param;
    s32                       rec;
    s32                       spawn;
    u16                       timer;

    coord = ((TmdObject*)arg1->extra)->field_8;
    work  = (ActorsShared801351d4Work*)arg1->idMap;
    spawn = 0;
    switch (D_801153F4) {
        case 1:
            func_actor_102400_80131E84(coord, 0x100);
            return;
        case 2:
            return;
        case 0:
        default:
            coord->coord.t[0] += (work->field_A8 * 0x19) >> 9;
            coord->coord.t[2] += (work->field_AC * 0x19) >> 9;
            coord->flg         = 0;
            Gp_UpdateCoord(coord);
            func_actor_102400_80131E84(coord, 0x100);
            rec = work->field_90.field_4;
            if ((rec != 0) &&
                (Gp_RoomParamTables[Game_Session->field_7 - 1][Game_Session->field_6 - 1]
                                   [func_800E1B24(rec)]
                                       ->field_1 == 0)) {
                spawn = 1;
            }
            Gp_ClearRec18Occupied(&work->field_90);
            timer          = work->field_B0 - 1;
            work->field_B0 = timer;
            if (((timer << 0x10) <= 0) || (work->field_40 & 1) || (spawn != 0)) {
                Gp_SpawnEff(D_80115754, coord, 0, NULL);
                arg1->state    = 2;
                work->field_B2 = 0;
            }
            break;
    }
}

void func_actor_102400_80134AC4(GsCOORDINATE2* arg0, s32 arg1)
{
    SVECTOR sp10;
    SVECTOR sp18;
    s32     ang;

    if (Gp_State1C->field_4 == 0) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((((u32)Gp_LcgState >> 16) & 3) == 0) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            ang         = ((u32)Gp_LcgState >> 16) & 0xF80;
            memset(&sp18, 0, sizeof(sp18));
            sp18.vx = (u32)(rcos(ang) * 5) >> 5;
            sp18.vz = (u32)(rsin(ang) * 5) >> 5;
            sp10    = sp18;
            Gp_SpawnEff(D_80115728, arg0, arg1 | 0x20100200, &sp10);
        }
    }
}

void func_actor_102400_80134BD0(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_102400_80131E24;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}
