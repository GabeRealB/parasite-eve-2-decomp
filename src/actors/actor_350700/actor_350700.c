#include "common.h"
#include "actors/actor_350700.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"
#include "gameplay/1BC.h"
#include "gameplay/D4.h"

/// `Gp_DispatchMsg` handler table installed at `Task::field_24` by
/// `func_actor_350700_80162404`; terminator id 0x7FFFFFFF.
extern GpMsgEntry D_actor_350700_80169D1C[];

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700", func_actor_350700_80161E88);

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700", func_actor_350700_80162070);

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700", func_actor_350700_801621B4);

INCLUDE_RODATA("actors/nonmatchings/actor_350700/actor_350700", D_actor_350700_80161E20);

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700", func_actor_350700_80162398);

/// Spawn state of the enemy actor: allocates the 0x4C8-byte work block that
/// every later handler reads through `Task::idMap`, seeds the three -1 bytes
/// and three cleared words the work's own init expects, republishes the light
/// and colour matrices onto the display object, then installs the message
/// table and the exit handler. An allocation failure ends the task instead of
/// leaving a half-built actor behind.
void func_actor_350700_80162404(Task* arg0)
{
    Actor350700Work* work;

    work = Mem_Calloc(sizeof(Actor350700Work), false);
    if (work == NULL) {
        Gp_EnemyTaskExit(arg0);
        return;
    }

    arg0->idMap     = (TaskIdMap*)work;
    work->field_43D = -1;
    work->field_43E = -1;
    work->field_4C5 = -1;
    work->field_4A0 = 0;
    work->field_4A4 = 0;
    work->field_4A8 = 0;

    func_actor_350700_801624B4(arg0);

    arg0->field_24     = D_actor_350700_80169D1C;
    arg0->exitCallback = func_actor_350700_80162494;
    arg0->state       += 1;
}

void func_actor_350700_80162494(Task* arg0)
{
    Gp_EnemyTaskExit(arg0);
}

void func_actor_350700_801624B4(Task* arg0)
{
    TmdObject*       ext;
    Actor350700Work* work;

    ext           = arg0->extra;
    work          = (Actor350700Work*)arg0->idMap;
    ext->field_1C = &work->light;
    ext->field_20 = &work->color;
}

void func_actor_350700_801624D0(void)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700", func_actor_350700_801624D8);

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700", func_actor_350700_80162540);

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700", func_actor_350700_8016261C);

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700", func_actor_350700_80162764);

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700", func_actor_350700_80162860);

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700", func_actor_350700_80162998);

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700", func_actor_350700_80162A14);

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700", func_actor_350700_80162AF4);

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700", func_actor_350700_80162B30);

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700", func_actor_350700_80162D5C);

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700", func_actor_350700_80162F7C);

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700", func_actor_350700_801630C0);

INCLUDE_RODATA("actors/nonmatchings/actor_350700/actor_350700", ActorsShared80138404Table);

INCLUDE_RODATA("actors/nonmatchings/actor_350700/actor_350700", D_actor_350700_80161E5C);
