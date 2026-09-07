#include "common.h"

#include "gameplay/1BC.h"

#include "actors/actors_shared_80136a28.h"

void ActorsShared80136a28(GpEnemy* arg0, Task* arg1)
{
    ActorsShared80136a28Work* work;
    u16                       temp_v0;

    work = (ActorsShared80136a28Work*)arg1->idMap;
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
