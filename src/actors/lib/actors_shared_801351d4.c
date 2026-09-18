#include "common.h"

#include "actors/actors_shared_801351d4.h"

void ActorsShared801351d4(GpEnemy* arg0, Task* arg1)
{
    ActorsShared801351d4Work* work;
    u16                       temp_v0;

    work = (ActorsShared801351d4Work*)arg1->work;
    switch (work->field_B2) {
        case 0:
            Gp_UnlinkObj(&work->obj_0);
            Gp_UnlinkObj(&work->obj_20);
            Gp_UnlinkObj(&work->obj_58);
            work->field_B2 = 1;
            work->field_B0 = 0x3C;
            return;
        case 1:
            temp_v0        = work->field_B0 - 1;
            work->field_B0 = temp_v0;
            if ((temp_v0 << 0x10) <= 0) {
                Gp_DestroyEnemy(arg0, arg1);
            }
            return;
    }
}
