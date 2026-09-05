#include "common.h"

#include "gameplay/3FB8.h"

#include "main/session.h"
#include "main/task.h"

/// Slow left turn-in-place: nudges the player's facing angle by -0x80 each
/// frame for 0x10 frames, wrapping it back into [-0x800, 0x800), then kills
/// itself. Any state other than 0 or 1 kills the task immediately.
void func_acropolis_patio_8017E054(Task* task)
{
    GameActor* actor;
    s16        angle;

    actor = ((GpActorWork*)Game_GetPtrSlot(3))->actor;

    switch (task->state) {
        case 0:
            task->killCountdown = 0x10;
            task->state++;
            /* fallthrough */
        case 1:
            angle = actor->field_52 - 0x80;
            if (angle < -0x800) {
                angle = actor->field_52 + 0xF80;
            }
            actor->field_52 = angle;
            task->killCountdown--;
            if (task->killCountdown > 0) {
                break;
            }
            /* fallthrough */
        default:
            Task_Kill(task);
            break;
    }
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_patio/acropolis_patio_4", func_acropolis_patio_8017E100);

INCLUDE_ASM("rooms/nonmatchings/acropolis_patio/acropolis_patio_4", func_acropolis_patio_8017E324);

INCLUDE_ASM("rooms/nonmatchings/acropolis_patio/acropolis_patio_4", func_acropolis_patio_8017E730);
