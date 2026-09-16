#include "common.h"

#include "actors/actor_107000.h"
#include "actors/actors_shared_80134700.h"
#include "main/task.h"
#include "main/tmd.h"

#include <psyq/libgs.h>

void ActorsShared80134690(void* arg0, Task* task);
void Gp_UpdateCoord(GsCOORDINATE2* arg0);
void func_actor_107000_80132298(Task* arg0);
void func_actor_107000_8013283C(Task* arg0);
void func_actor_107000_801344DC(Task* arg0);
void func_actor_107000_80134680(Task* arg0);
void func_actor_107000_80134810(Task* arg0, GsCOORDINATE2* arg1);

extern u8 D_801153F4;

/// Per-frame mode handler of the actor, shared with the other enemy actors that
/// gate on `D_801153F4`: mode 1 runs only the tail, mode 2 puts the model in
/// its hidden pose and returns, mode 0 clears both flags before falling into
/// the update, and any other mode updates directly. The update drives the
/// actor's four handlers, clears the display flags of the model's first two
/// coordinate parts and recomputes the second one's world matrix; the tail then
/// colours the actor from that second part and draws its ground shadow.
void func_actor_107000_801343C4(Actor107000Ctx* arg0, Task* arg1)
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
    ((TmdObject*)arg1->extra)->field_C = 0;
    arg0->field_14                     = 0;
    goto default_body;
case2:
    ((TmdObject*)arg1->extra)->field_C = 0x80;
    arg0->field_14                     = one;
    return;
default_body:
    func_actor_107000_80132298(arg1);
    func_actor_107000_801344DC(arg1);
    func_actor_107000_8013283C(arg1);
    func_actor_107000_80134680(arg1);
    func_actor_107000_80134810(arg1, &((TmdObject*)arg1->extra)->field_8[1]);
    ((TmdObject*)arg1->extra)->field_8[0].flg = 0;
    ((TmdObject*)arg1->extra)->field_8[1].flg = 0;
    Gp_UpdateCoord(&((TmdObject*)arg1->extra)->field_8[1]);
case1:
    ActorsShared80134690(arg0, arg1);
    ActorsShared80134700(arg1);
}

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000_2", func_actor_107000_801344DC);
