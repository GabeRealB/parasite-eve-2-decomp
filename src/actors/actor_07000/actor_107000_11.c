#include "common.h"

#include "actors/actor_107000.h"
#include "gameplay/1BC.h"
#include "main/task.h"

void Actor07000_Fn04274(Task* arg0, s32 arg1);

/// Per-frame reaction handler: folds the generic hit flags into the enemy's
/// flag byte, applies a pending hit, and drops the work to its death pose when
/// the hit lands.
void Actor07000_Fn0662C(Task* arg0)
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
            enemy->reactionFlags = flags & 0xFE;
        }
        if (enemy->reactionFlags & 2) {
            enemy->reactionFlags &= 0xFD;
            work->field_36A       = 3;
            work->field_36E       = 0;
        }
        if (enemy->reactionFlags & 0xC) {
            tick = Gp_TickObjFlag4((GpObj5C*)enemy);
            if (tick != 0) {
                Actor07000_Fn04274(arg0, tick);
                work->field_36A = 0;
                work->field_370 = 5;
            }
            if (Gp_ObjFlag4Expired((GpObj5C*)enemy) != 0) {
                enemy->reactionFlags &= 0xF3;
            }
        }
    }
}
