#include "common.h"
#include "actors/actor_104600.h"
#include "actors/actors_shared_80134680.h"
#include "actors/actors_shared_80134700.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"
#include "gameplay/1BC.h"
#include <psyq/libgs.h>
void                         Gp_UpdateCoord(GsCOORDINATE2* arg0);
void                         func_actor_104600_801321F4(Task* arg0);
void                         func_actor_104600_80132798(Task* arg0);
void                         func_actor_104600_80134438(Task* arg0);
void                         func_actor_104600_80134690(void* arg0, Task* arg1);
void                         func_actor_104600_8013476C(Task* arg0, GsCOORDINATE2* arg1);
extern u8                    D_801153F4;
extern GpEnemyTaskFuncTable3 D_actor_104600_80131E24;
INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600_2", func_actor_104600_80133D74);

void func_actor_104600_801342C4(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_104600_80131E24;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

/// Per-frame mode handler of the actor, shared with the other enemy actors
/// that gate on `D_801153F4`: mode 1 runs only the tail, mode 2 puts the model
/// in its hidden pose and returns, mode 0 clears both flags before falling
/// into the update, and any other mode updates directly. The update drives the
/// actor's four handlers, clears the display flags of the model's first two
/// coordinate parts and recomputes the second one's world matrix; the tail
/// then colours the actor from that second part and draws its ground shadow.
void func_actor_104600_80134320(Actor104600Ctx* arg0, Task* arg1)
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
    ((TmdObject*)arg1->extra)->flags = 0;
    arg0->field_14                   = 0;
    goto default_body;
case2:
    ((TmdObject*)arg1->extra)->flags = 0x80;
    arg0->field_14                   = one;
    return;
default_body:
    func_actor_104600_801321F4(arg1);
    func_actor_104600_80134438(arg1);
    func_actor_104600_80132798(arg1);
    ActorsShared80134680(arg1);
    func_actor_104600_8013476C(arg1, &((TmdObject*)arg1->extra)->coords[1]);
    ((TmdObject*)arg1->extra)->coords[0].flg = 0;
    ((TmdObject*)arg1->extra)->coords[1].flg = 0;
    Gp_UpdateCoord(&((TmdObject*)arg1->extra)->coords[1]);
case1:
    func_actor_104600_80134690(arg0, arg1);
    ActorsShared80134700(arg1);
}

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600_2", func_actor_104600_80134438);
