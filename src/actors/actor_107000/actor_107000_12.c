#include "common.h"

#include "actors/actor_107000.h"
#include "gameplay/1BC.h"
#include "main/task.h"

void func_actor_107000_80136094(Task* arg0, s32 arg1);

INCLUDE_ASM("actors/nonmatchings/actor_107000/actor_107000_12", func_actor_107000_801381B0);

/// Per-frame reaction handler: folds the generic hit flags into the enemy's
/// flag byte, applies a pending hit, and drops the work to its death pose when
/// the hit lands.
void func_actor_107000_8013844C(Task* arg0)
{
    Actor107000Work* work;
    GpEnemy*         enemy;
    s32              tick;
    u8               flags;

    enemy = arg0->spawnArg2;
    flags = enemy->field_4C;
    work  = (Actor107000Work*)arg0->work;
    if (flags != 0) {
        if (flags & 1) {
            enemy->field_4C = flags & 0xFE;
        }
        if (enemy->field_4C & 2) {
            enemy->field_4C &= 0xFD;
            work->field_36A  = 3;
            work->field_36E  = 0;
        }
        if (enemy->field_4C & 0xC) {
            tick = Gp_TickObjFlag4((GpObj5C*)enemy);
            if (tick != 0) {
                func_actor_107000_80136094(arg0, tick);
                work->field_36A = 0;
                work->field_370 = 5;
            }
            if (Gp_ObjFlag4Expired((GpObj5C*)enemy) != 0) {
                enemy->field_4C &= 0xF3;
            }
        }
    }
}
