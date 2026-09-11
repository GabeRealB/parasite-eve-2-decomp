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

void func_actor_341700_8016ABF4(Task* arg0)
{
    Actor341700Work* work;

    work            = (Actor341700Work*)arg0->idMap;
    arg0->state     = 5;
    work->field_420 = 0;
    work->field_422 = 0;
}
