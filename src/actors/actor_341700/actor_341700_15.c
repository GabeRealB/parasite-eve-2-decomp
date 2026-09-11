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

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_15", func_actor_341700_80169F38);

INCLUDE_ASM("actors/nonmatchings/actor_341700/actor_341700_15", func_actor_341700_80169FB0);

void func_actor_341700_8016A058(Task* arg0)
{
    Actor341700Work* work = (Actor341700Work*)arg0->idMap;

    work->field_426 = 4;
    work->field_41C = 0x10;
    work->field_418 = 0xF;
    work->field_414 = 1;
    work->field_422 = work->field_422 + 1;
}
