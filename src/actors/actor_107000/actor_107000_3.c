#include "actors/actors_shared_80134810.h"
#include "common.h"

#include "actors/actor_107000.h"
#include "actors/actors_shared_80134680.h"
#include "actors/actors_shared_80134700.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/task.h"
#include "main/tmd.h"

#include <psyq/libgs.h>

void ActorsShared80134690(void* arg0, Task* task);
void Gp_UpdateCoord(GsCOORDINATE2* arg0);
void func_actor_107000_80132298(Task* arg0);
void func_actor_107000_8013283C(Task* arg0);
void func_actor_107000_801344DC(Task* arg0);
void func_actor_107000_80132D8C(Task* arg0, s32 arg1);
void func_actor_107000_801334C8(Task* arg0, u8 arg1);

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
    ((TmdObject*)arg1->extra)->flags = 0;
    arg0->field_14                   = 0;
    goto default_body;
case2:
    ((TmdObject*)arg1->extra)->flags = 0x80;
    arg0->field_14                   = one;
    return;
default_body:
    func_actor_107000_80132298(arg1);
    func_actor_107000_801344DC(arg1);
    func_actor_107000_8013283C(arg1);
    ActorsShared80134680(arg1);
    ActorsShared80134810(arg1, &((TmdObject*)arg1->extra)->coords[1]);
    ((TmdObject*)arg1->extra)->coords[0].flg = 0;
    ((TmdObject*)arg1->extra)->coords[1].flg = 0;
    Gp_UpdateCoord(&((TmdObject*)arg1->extra)->coords[1]);
case1:
    ActorsShared80134690(arg0, arg1);
    ActorsShared80134700(arg1);
}

/// Per-frame reaction dispatch, gated on the enemy's flag byte: bit 0x1 runs
/// the death countdown and, once `field_2D4` reaches 5, kills the actor and
/// clears its HP; bit 0x2 arms the reaction sub-state and suppresses the
/// animation rebind; and bits 0x4/0x8 tick the generic flag-4 helper, folding
/// the damage it reports into `func_actor_107000_80132D8C` and clearing the
/// bits once they expire.
void func_actor_107000_801344DC(Task* arg0)
{
    Actor107000Work* work;
    GpEnemy*         enemy;
    s32              tick;
    u8               flags;

    enemy = arg0->spawnArg2;
    flags = enemy->reactionFlags;
    work  = (Actor107000Work*)arg0->work;
    if (flags != 0) {
        if (flags & 1) {
            work->field_2D4 += 1;
            work->field_2AC += 0xC8;
            if ((s16)work->field_2D4 >= 5) {
                func_actor_107000_801334C8(arg0, 0);
                arg0->killCountdown = 5;
                work->field_2B4     = 0;
                arg0->state         = 2;
                enemy->hp           = 0;
            }
        }
        if (enemy->reactionFlags & 2) {
            enemy->reactionFlags &= 0xFD;
            work->field_2B2       = 3;
            work->field_2B6       = 0;
            work->field_2BE       = 0;
            work->field_2D2       = 1;
        }
        if (enemy->reactionFlags & 0xC) {
            tick = Gp_TickObjFlag4((GpObj5C*)enemy);
            if (tick != 0) {
                func_actor_107000_80132D8C(arg0, tick);
            }
            if (Gp_ObjFlag4Expired((GpObj5C*)enemy) != 0) {
                enemy->reactionFlags &= 0xF3;
            }
        }
    }
}
