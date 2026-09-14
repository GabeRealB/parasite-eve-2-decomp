#include "common.h"

#include "actors/actor_120300.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"

void func_actor_120300_80133E14(s16 arg0)
{
    Actor120300Work* work = D_actor_120300_80141BA8->field_1C;

    work->field_4C0 = arg0;
    work->field_4C2 = 0;
}

void func_actor_120300_80133E34(s16 arg0)
{
    Actor120300Work* work = D_actor_120300_80141BA8->field_1C;

    work->field_4C8 = arg0;
    work->field_4CA = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_120300/actor_120300_3", func_actor_120300_80133E54);

/// Runs the pending player-weapon effect and reports it: the latch at
/// `field_4DE` keeps it one-shot, and the `Gp_MsgPlayerWeapon` argument beside
/// the clear is the same zero.
void func_actor_120300_80133E94(void)
{
    Actor120300Work* work = D_actor_120300_80141BA8->field_1C;

    if (work->field_4DE != 0) {
        Gp_SpawnWeaponEff();
        work->field_4DE = 0;
        Gp_MsgPlayerWeapon(0);
    }
}
