#include "common.h"

#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/sound.h"
#include "main/wipsys.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"

#include "actors/actors_shared_80135d50.h"
#include "actors/actors_shared_8014ae08.h"
#include "actors/actors_shared_8014af2c.h"
#include "actors/actors_shared_8014df20.h"
#include "actors/actor_104600.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 Actor04600_D0003C;

extern u8 D_801153F4;

void Actor04600_Fn03EC0(void* arg0, Task* arg1);
void Gp_UpdateCoord(GsCOORDINATE2* arg0);

extern SVECTOR Actor04600_D064B4;
extern SVECTOR Actor04600_D064BC;

void Actor04600_Fn03B80(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = Actor04600_D0003C;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

/// Per-frame mode handler under the shared `D_801153F4` mode byte: mode 1 runs
/// only the tail, mode 2 sets the model flags and the pose flag and returns,
/// mode 0 clears the pose flag before falling into the update, and any other
/// mode updates directly. The update adds 0x80 to the root coordinate's Y
/// translation, runs the per-frame helpers including the hit handler, clears
/// the first two coordinates' flags and recomputes the second one's matrix
/// before the tail.
void Actor04600_Fn03BDC(GpEnemy* arg0, Task* arg1)
{
    s32 state;
    s32 one;

    state = D_801153F4;
    one   = 1;
    if (state == one) {
        goto case1;
    }
    if (state >= 2) {
        goto ge2;
    }
    if (state == 0) {
        goto case0;
    }
    goto default_body;
ge2:
    if (state == 2) {
        goto case2;
    }
    goto default_body;
case0:
    arg0->node.flags = 0;
    goto default_body;
case2:
    ((TmdObject*)arg1->extra)->flags = 0x80;
    arg0->node.flags                 = one;
    return;
default_body:
    ((TmdObject*)arg1->extra)->coords[0].coord.t[1] += 0x80;
    Actor04600_Fn03D54(arg1);
    ActorsShared80135d50(arg1);
    ActorsShared8014ae08(arg1);
    Actor04600_Fn0346C(arg1);
    ActorsShared8014af2c(arg1);
    ((TmdObject*)arg1->extra)->coords[0].flg = 0;
    ((TmdObject*)arg1->extra)->coords[1].flg = 0;
    Gp_UpdateCoord(&((TmdObject*)arg1->extra)->coords[1]);
case1:
    Actor04600_Fn03EC0(arg0, arg1);
}
