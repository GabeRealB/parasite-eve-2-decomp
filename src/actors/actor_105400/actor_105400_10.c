#include "common.h"

#include "actors/actor_105400.h"
#include "actors/actors_shared_8013391c.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/gameflag.h"
#include "main/sound.h"
#include "main/task.h"

extern u8 D_801153F4;

void func_8017E524(s32 arg0);
void func_8017FD88(s32 arg0);

/// Teardown handler of the part task (its state 2), ticking only while the
/// gameplay mode `D_801153F4` is 0. The first tick unlinks the enemy's lock-on
/// node and the part's collision object, drops the enemy's `recs`, sends the
/// main task's sound id `field_31C` a type-7 event, and undoes what the spawn
/// did for this sub-state (chosen by `field_46`): the same room call with 0
/// instead of 1, and game flag 0x147 or 0x148 set to 1 where the spawn set it
/// to 0. The enemy is destroyed once the counter `field_42` reaches 0x3D.
void func_actor_105400_80133838(GpEnemy* arg0, Task* arg1)
{
    Actor05400Part* part;
    Actor05400Work* parentWork;
    u16             timer;

    part       = (Actor05400Part*)arg1->work;
    parentWork = (Actor05400Work*)arg1->parent->work;
    if (D_801153F4 == 0) {
        timer          = part->field_42 + 1;
        part->field_42 = timer;
        if ((s16)timer == 1) {
            Gp_UnlinkNode(&arg0->node);
            Gp_UnlinkObj(&part->obj);
            arg0->recs = 0;
            SndEvt_EnqueueType7(parentWork->field_31C, 1);
            if (part->field_46 == 0) {
                func_8017FD88(0);
                GameFlag_SetNibble(0x147, 1);
            } else {
                func_8017E524(0);
                GameFlag_SetNibble(0x148, 1);
            }
        }
        if ((s16)part->field_42 >= 0x3D) {
            Gp_DestroyEnemy(arg0, arg1);
        }
    }
}

/// Message handler in the main task's message table: ORs the bit the payload's
/// selector names into the work block's `field_33A` (1, 2 or both for
/// selector 3; 0 is a no-op). Bit 1 releases the death handler from its wait,
/// bit 2 lets it run its `Gp_ReleaseStateF0Add` call.
s32 func_actor_105400_8013391C(Task* task, s32 msgId, ActorsShared8013391cMsg* msg)
{
    Actor05400Work* work;

    work = (Actor05400Work*)task->work;
    switch (msg->field_2) {
        case 0:
            break;
        case 1:
            work->field_33A |= 1;
            break;
        case 2:
            work->field_33A |= 2;
            break;
        case 3:
            work->field_33A |= 3;
            break;
    }
    return 0;
}
