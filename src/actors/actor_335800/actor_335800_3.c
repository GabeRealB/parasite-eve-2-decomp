#include "common.h"

#include "actors/actor_335800.h"

#include "gameplay/3CD8.h"

#include "gameplay/gameplay.h"

#include "main/sound.h"

#include "main/task.h"

#include "main/unknown_syms.h"

extern TaskDesc D_actor_335800_80164DE0;

extern TaskDesc D_80182834;

extern s8 D_8007272D;

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_3", func_actor_335800_80162E8C);

void func_actor_335800_80162F08(void)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_3", func_actor_335800_80162F10);

void func_actor_335800_80162F7C(Task* arg0)
{
    Gp_EnemyTaskExit(arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_3", func_actor_335800_80162F9C);

void func_actor_335800_80162FF4(void)
{
}
