#include "common.h"

#include "actors/actor_103700.h"
#include "gameplay/3A34.h"

extern u16 D_801153F4[];
extern s8  D_8011540A;

void func_actor_103700_8013537C(Task* task)
{
    Actor103700Work* work = (Actor103700Work*)task->idMap;
    s32              state;

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
