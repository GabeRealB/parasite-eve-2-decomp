#include "common.h"

#include "main/gfx.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "actors/actor_341700.h"

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_15", func_actor_341700_8016D130);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_15", func_actor_341700_8016D2B8);

void func_actor_341700_8016D2E8(GpEnemy* arg0, Task* arg1)
{
    TmdObject* model;

    if (((Actor341700SubWork*)arg1->idMap)->field_4 != 0) {
        model              = (TmdObject*)arg1->extra;
        arg0->node.field_4 = 1;
        model->field_C     = 0;
        Tmd_AllocBuffers(model);
    }
}
