#include "common.h"

#include "gameplay/3CD8.h"
#include "main/session.h"
#include "main/task.h"

extern s32 D_actor_120300_8014195C;

void func_actor_120300_80133F14(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            func_800E8614((s32)&D_actor_120300_8014195C, 0);
            arg0->state += 1;
            break;
        case 1:
            if (gGameSession->field_1 == 0) {
                Task_Kill(arg0);
            }
            break;
    }
}
