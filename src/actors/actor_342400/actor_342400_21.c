#include "common.h"

#include "main/sound.h"
#include "main/task.h"

#include "gameplay/1BC.h"

#include "actors/actor_342400.h"

/// Requests animation 0xF and advances the sub-state.
void func_actor_342400_8016B33C(Task* arg0)
{
    Actor342400Work* work = (Actor342400Work*)arg0->work;

    work->field_426 = 4;
    work->field_41C = 0x10;
    work->field_418 = 0xF;
    work->field_414 = 1;
    work->field_422 = work->field_422 + 1;
}
