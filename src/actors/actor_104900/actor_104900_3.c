#include "common.h"

#include "actors/actors_shared_80138efc.h"

void func_actor_104900_80137498(GpEnemy*, Task*, ActorsShared80138efcWork*, void*);

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900_3", func_actor_104900_8013898C);

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900_3", func_actor_104900_80138A2C);

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900_3", func_actor_104900_80138B5C);

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900_3", func_actor_104900_80138C6C);

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900_3", func_actor_104900_80138D58);

/// Arms the 0x15 / 0x16 motion pair on the first frame of the sub-state, then
/// runs the sub-state proper: while bit 1 of the motion flags at 0x9C is set,
/// either keeps the state on the 0x17 motion once `field_B92` has run out and
/// the enemy is not carrying flag 0x2 in `field_4C`, or hands the frame to the
/// shared routine at 0x80137498 on the 0x18 motion.
void func_actor_104900_80138E34(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, void* scratch)
{
    ActorsShared80138efcMotion* motion = &work->motion;

    if (work->field_BA8 == 0) {
        if (work->field_BAE == 0) {
            work->field_BA4 = 0x15;
        } else {
            work->field_BA4 = 0x16;
        }
        work->field_BA5 = 1;
        work->field_B8C = 0xA;
        work->field_BA8 = (u8)work->field_BA8 + 1;
    }
    if (motion->flags & 2) {
        work->field_B9C = 0;
        if (work->field_B92 > 0) {
            if (!(enemy->field_4C & 2)) {
                work->field_BA6 = 1;
                work->field_BAB = 0x10;
                work->state     = 0x17;
                work->field_BA8 = 0;
                work->field_BAF = 2;
            }
        } else {
            work->state     = 0x18;
            work->field_BA8 = 0;
            func_actor_104900_80137498(enemy, task, work, scratch);
        }
    }
}
