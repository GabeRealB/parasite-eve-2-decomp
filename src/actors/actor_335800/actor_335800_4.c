#include "common.h"

#include "actors/actor_335800.h"

#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

/// `Gp_DispatchMsg` handler table installed at `Task::field_24` by
/// `func_actor_335800_80163AA0`; terminator id 0x7FFFFFFF.
extern GpMsgEntry D_actor_335800_80172EA8[];

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_4", func_actor_335800_801631A4);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_4", func_actor_335800_801632A4);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_4", func_actor_335800_801633C0);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_4", func_actor_335800_8016343C);

s32 func_actor_335800_8016354C(Task* arg0, s32 arg1, Actor335800Msg* arg2, s32 arg3)
{
    Actor335800MainWork* work;

    work = (Actor335800MainWork*)arg0->idMap;
    if (arg2->field_2 == 0) {
        work->field_504 = 0;
    }
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_4", func_actor_335800_80163568);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_4", func_actor_335800_8016373C);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_4", func_actor_335800_80163880);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_4", func_actor_335800_80163A34);

/// Spawn state of the enemy actor: allocates the 0x4C8-byte work block that
/// every later handler reads through `Task::idMap`, seeds the two -1 bytes,
/// the -1 halfword and three cleared words the work's own init expects,
/// republishes the light and colour matrices onto the display object, then
/// installs the message table and the exit handler. An allocation failure
/// ends the task instead of leaving a half-built actor behind.
void func_actor_335800_80163AA0(Task* arg0)
{
    Actor335800Work* work;

    work = Mem_Calloc(sizeof(Actor335800Work), false);
    if (work == NULL) {
        Gp_EnemyTaskExit(arg0);
        return;
    }

    arg0->idMap     = (TaskIdMap*)work;
    work->field_43D = -1;
    work->field_43E = -1;
    work->field_4C4 = -1;
    work->field_4A0 = 0;
    work->field_4A4 = 0;
    work->field_4A8 = 0;

    func_actor_335800_80163B54(arg0);

    arg0->field_24     = D_actor_335800_80172EA8;
    arg0->exitCallback = func_actor_335800_80163B34;
    arg0->state       += 1;
}

void func_actor_335800_80163B34(Task* arg0)
{
    Gp_EnemyTaskExit(arg0);
}

void func_actor_335800_80163B54(Task* arg0)
{
    TmdObject*       ext;
    Actor335800Work* work;

    ext           = arg0->extra;
    work          = (Actor335800Work*)arg0->idMap;
    ext->field_1C = &work->light;
    ext->field_20 = &work->color;
}

void func_actor_335800_80163B70(void)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_4", func_actor_335800_80163B78);
