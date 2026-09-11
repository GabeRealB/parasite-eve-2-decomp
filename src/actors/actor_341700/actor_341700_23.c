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

void func_actor_341700_8016AA58(Task* arg0)
{
    Actor341700Work* work;
    TmdObject*       model;

    work  = (Actor341700Work*)arg0->idMap;
    model = (TmdObject*)arg0->extra;

    work->field_412++;
    if ((s16)work->field_412 >= 0x18) {
        model->field_C |= 2;
        work->field_412 = 0;
        work->field_451 = 1;
        work->field_420++;
    }
}
