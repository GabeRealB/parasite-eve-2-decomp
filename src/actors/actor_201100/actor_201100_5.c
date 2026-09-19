#include "common.h"

#include "actors/actor_101100.h"

#include "gameplay/3CD8.h"
#include "main/task.h"
#include "main/tmd.h"

/// Per-frame state of the actor's two-state controller (state 1). While
/// `D_801153F4` is zero the actor runs its self-destruct countdown: from
/// `killCountdown` 0x15 and above it throws an effect burst (0x60070) at the
/// model's root coordinate on every other frame and reparents the spawned
/// effect onto itself, and a collision hit on the work block's `GpRec18` table
/// -- masked to the 0x10000 slot -- or the countdown reaching 0x14 clears the
/// two 0xC000 bits the spawn state set in the object's flags. The countdown
/// then ticks down and the task calls its exit callback once it reaches zero.
void ActorsShared8013845cSub1(Task* task)
{
    Actor101100Work*  work;
    GsCOORDINATE2*    coord;
    GpObj*            obj;
    struct GpEffWork* eff;
    s16               countdown;

    work  = (Actor101100Work*)task->work;
    coord = ((TmdObject*)task->extra)->coords;

    if (D_801153F4 == 0) {
        if (task->killCountdown >= 0x15) {
            if (((u16)task->killCountdown & 1) == 0) {
                eff = Gp_SpawnEff(0x60070, coord, 0xC0031FFF, NULL);
                if (eff != NULL) {
                    Task_Reparent(task, eff->task);
                }
            }
            if (Gp_CountRec18Hi(&work->rec[0], 0x10000) != 0) {
                obj         = &work->obj;
                obj->flags &= 0x3FFF;
            }
        }
        if (task->killCountdown == 0x14) {
            obj         = &work->obj;
            obj->flags &= 0x3FFF;
        }
        countdown           = (u16)task->killCountdown - 1;
        task->killCountdown = countdown;
        if ((countdown << 0x10) <= 0) {
            Task_CallExit(task);
        }
    }
}
