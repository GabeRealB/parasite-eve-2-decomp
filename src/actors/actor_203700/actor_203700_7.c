#include "common.h"

#include "actors/actors_shared_801326f0.h"
#include "gameplay/3A34.h"
#include "main/task.h"

extern u16 D_801153F4[];
extern s8  D_8011540A;

void func_actor_203700_8014D37C(Task* task)
{
    ActorShared801326f0Work* work = (ActorShared801326f0Work*)task->work;
    s32                      state;

    switch (D_8011540A) {
        case 0:
            break;
        case 1:
            D_8011540A = 2;
            return;
        case 2:
            if (Gp_StateF0.field_6 < 0x11) {
                Gp_StateF0.field_1A = 3;
                return;
            }
            break;
        case 3:
            if (Gp_StateF0.field_6 < 0xE) {
                Gp_StateF0.field_1A = 4;
                return;
            }
            break;
        case 4:
            if (Gp_StateF0.field_6 < 0xA) {
                Gp_StateF0.field_1A = 5;
                return;
            }
            break;
        case 5:
            state = task->state;
            if (state == 2 && D_801153F4[1] == 0) {
                work->field_266 = state;
            }
            break;
    }
}
