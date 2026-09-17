#include "common.h"

#include "actors/actor_311500.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"

INCLUDE_ASM("actors/nonmatchings/actor_311500/actor_311500_2", func_actor_311500_801629D8);

INCLUDE_ASM("actors/nonmatchings/actor_311500/actor_311500_2", func_actor_311500_80162C34);

s16 func_actor_311500_80162DDC(Actor311500* arg0)
{
    Actor311500Work* work = arg0->field_1C;
    GpEnemy*         enemy;
    GpRec18*         recs;
    SVECTOR          pos;
    SVECTOR*         pp;
    s32              v;
    s32              damage;
    s16              i;

    enemy = arg0->field_20;
    pp    = &pos;
    recs  = work->rec18;
    for (i = 0; i < 1; i++) {
        if (recs[i].field_4 == 0) {
            break;
        }
        if ((recs[i].field_4 & 0xFFFF0000) == 0x20000) {
            pp->vx = recs[i].field_8;
            pp->vy = recs[i].field_A;
            pp->vz = recs[i].field_C;
            v      = recs[i].field_4;
            goto done;
        }
    }
    v = 0;
done:
    work->field_4CC = v;
    if (work->field_4CC != 0) {
        work->field_4D0 = v;
        damage          = Gp_ComputeDamage(work->field_4CC, 0, 0, 0x1000);
        if (Gp_RollEnemyChance(enemy, work->field_4CC, 0) != 0) {
            damage *= 5;
            Gp_SpawnEff(0x6009C, arg0->field_2C->field_8, 0, 0);
        }
        enemy->field_40 -= damage;
        Gp_ClearRec18Occupied(work->rec18);
        func_800DA6E8(&enemy->node, damage, 0);
    }
    return work->field_4CC;
}

INCLUDE_ASM("actors/nonmatchings/actor_311500/actor_311500_2", func_actor_311500_80162F28);

INCLUDE_ASM("actors/nonmatchings/actor_311500/actor_311500_2", func_actor_311500_801630A4);

INCLUDE_ASM("actors/nonmatchings/actor_311500/actor_311500_2", func_actor_311500_80163334);

void func_actor_311500_801636A0(Actor311500* arg0, s32 arg1, s32 arg2, u32* arg3)
{
    *arg3 = arg0->field_1C->field_4D4;
}
