#include "common.h"

#include "actors/actor_105600.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/task.h"

/// Teardown state of the effect child set up by `Actor05600_Fn031B0`: step 0
/// unlinks its three collision bodies and restarts the frame counter, step 1
/// destroys the child once 0x3D frames have passed.
void Actor05600_Fn04C08(GpEnemy* arg0, Task* arg1)
{
    Actor105600FxWork* work;
    u16                temp_v0;

    work = (Actor105600FxWork*)arg1->work;
    switch (work->field_EC) {
        case 0:
            Gp_UnlinkObj(&work->obj40);
            Gp_UnlinkObj(&work->obj78);
            Gp_UnlinkObj(&work->obj98);
            work->field_E8 = 0;
            work->field_EC = 1;
            return;
        case 1:
            temp_v0        = work->field_E8 + 1;
            work->field_E8 = temp_v0;
            if ((s16)temp_v0 >= 0x3D) {
                Gp_DestroyEnemy(arg0, arg1);
            }
            return;
    }
}
