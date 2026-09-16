#include "common.h"

#include "actors/actor_201100.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/task.h"

extern u8 D_801153F4;

/// State 1 of `ActorsShared8013845c`: counts `Task::killCountdown` down to the
/// task's exit. Above 20 it spawns a spark on every even frame (reparented to
/// this task) and clears the model's top flag bits once `Gp_CountRec18Hi`
/// reports a hit; at exactly 20 it clears them unconditionally. Frozen while
/// `D_801153F4` is set.
void ActorsShared8013845cSub1(Task* task)
{
    Actor201100Work*  work  = (Actor201100Work*)task->idMap;
    GsCOORDINATE2*    coord = ((Actor201100Extra*)task->extra)->coord;
    GpEffWork*        eff;
    Actor201100Model* model;

    if (D_801153F4 != 0) {
        return;
    }
    if (task->killCountdown > 0x14) {
        if (!(task->killCountdown & 1)) {
            eff = Gp_SpawnEff(0x60070, coord, 0xC0031FFF, NULL);
            if (eff != NULL) {
                Task_Reparent(task, eff->field_0);
            }
        }
        if (Gp_CountRec18Hi(&work->rec, 0x10000) != 0) {
            model         = &work->model;
            model->flags &= 0x3FFF;
        }
    }
    if (task->killCountdown == 0x14) {
        model         = &work->model;
        model->flags &= 0x3FFF;
    }
    if (--task->killCountdown <= 0) {
        Task_CallExit(task);
    }
}
