#include "common.h"

#include "actors/actor_361100.h"

#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"

extern u8 D_801156F9;

extern Task* D_actor_361100_80171BE0;

INCLUDE_ASM("actors/nonmatchings/actor_361100/actor_361100", func_actor_361100_80161E3C);

INCLUDE_ASM("actors/nonmatchings/actor_361100/actor_361100", func_actor_361100_80161FF8);

void func_actor_361100_801627D4(Task* task)
{
    Task*               looker;
    Task*               target;
    Actor361100HeadAim* aim;
    u16                 rate;

    looker = Game_GetPtrSlot(3);
    target = (Task*)Gp_LookupSlot4(2);
    if (D_801156F9 == 0) {
        if ((looker == NULL) || (target == NULL)) {
            task->state = -1;
        }
        switch (task->state) {
            case 0:
                aim = Mem_Calloc(sizeof(Actor361100HeadAim), false);
                if (aim != NULL) {
                    task->idMap     = (TaskIdMap*)aim;
                    aim->yawLimit   = 0x300;
                    aim->pitchLimit = 0x200;
                    task->state++;
                        /* fallthrough */
                    case 1:
                        aim = (Actor361100HeadAim*)task->idMap;
                        if (task->spawnArg1 != 0) {
                            rate      = aim->rate + 0x100;
                            aim->rate = rate;
                            if ((s16)rate >= 0x1001) {
                                aim->rate = 0x1000;
                            }
                        } else {
                            rate      = aim->rate - 0x100;
                            aim->rate = rate;
                            if ((s16)rate < 0) {
                                aim->rate = 0;
                            }
                        }
                        func_800B17D4(looker, target, (GpHeadAim*)aim);
                        return;
                }
                /* fallthrough */
            default:
                Task_Kill(task);
                D_actor_361100_80171BE0 = NULL;
                break;
        }
    }
}

INCLUDE_RODATA("actors/nonmatchings/actor_361100/actor_361100", D_actor_361100_80161E20);
