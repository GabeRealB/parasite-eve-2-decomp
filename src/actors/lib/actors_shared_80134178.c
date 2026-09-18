#include "common.h"

#include "actors/actors_shared_80134178.h"

void ActorsShared80134178(GpEnemy* arg0, Task* arg1)
{
    TmdObject* model;

    if (((ActorShared80134178Work*)arg1->work)->field_4 != 0) {
        model              = (TmdObject*)arg1->extra;
        arg0->node.field_4 = 1;
        model->field_C     = 0x80;
    }
}
