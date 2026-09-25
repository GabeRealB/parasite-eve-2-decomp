#include "common.h"

#include "actors/actor_223600.h"
#include "gameplay/1BC.h"
#include "main/task.h"
#include "main/tmd.h"

/// Idle state of this enemy (entry 0 of `D_actor_223600_80149E4C`). On the
/// frame the state is entered (`field_4` set) it sets the display node's flags
/// to 1 and the model's flags to 0x80; it does nothing on later frames.
void func_actor_223600_8014CF3C(GpEnemy* arg0, Task* arg1)
{
    TmdObject* model;

    if (((Actor223600Work*)arg1->work)->field_4 != 0) {
        model            = (TmdObject*)arg1->extra;
        arg0->node.flags = 1;
        model->flags     = 0x80;
    }
}
