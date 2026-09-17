#include "common.h"
#include "actors/actor_101500.h"
#include "gameplay/3A34.h"

/// Flag bits 0/1 of `field_4C` knock the actor into pose 13; bits 2/3 tick
/// the damage-over-time effect and apply each hit.
void func_actor_101500_801343E8(Actor101500* actor)
{
    GpEnemy*         enemy;
    Actor101500Work* work;
    s32              damage;
    u8               flags;

    enemy = actor->field_20;
    flags = enemy->field_4C;
    work  = actor->field_1C;
    if (flags & 1) {
        enemy->field_4C = flags & 0xFE;
        if (work->field_358 != 2) {
            work->field_35A = 4;
        }
        work->field_362 = 0;
        work->field_352 = 13;
        work->field_34C = 0;
    }
    if (enemy->field_4C & 2) {
        enemy->field_4C &= 0xFD;
        if (work->field_358 != 2) {
            work->field_35A = 4;
        }
        work->field_362 = 0;
        work->field_352 = 13;
        work->field_34C = 0;
    }
    if (enemy->field_4C & 0xC) {
        damage = Gp_TickObjFlag4((GpObj5C*)enemy);
        if (damage != 0) {
            func_actor_101500_8013291C(actor, damage);
            func_800DA6E8(&enemy->node, damage, 0);
        }
        if (Gp_ObjFlag4Expired((GpObj5C*)enemy) != 0) {
            enemy->field_4C &= 0xF3;
        }
    }
}
