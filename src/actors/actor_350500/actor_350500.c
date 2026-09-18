#include "common.h"

#include "actors/actor_350500.h"
#include "actors/actors_shared_801327b4.h"

#include "gameplay/1BC.h"
#include "gameplay/D4.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

/// `Gp_DispatchMsg` handler table installed at `Task::msgTable` by
/// `func_actor_350500_801623CC`; terminator id 0x7FFFFFFF.
extern GpMsgEntry D_actor_350500_80168EB0[];

INCLUDE_ASM("actors/nonmatchings/actor_350500/actor_350500", func_actor_350500_80161E50);

INCLUDE_ASM("actors/nonmatchings/actor_350500/actor_350500", func_actor_350500_80162038);

INCLUDE_ASM("actors/nonmatchings/actor_350500/actor_350500", func_actor_350500_8016217C);

INCLUDE_RODATA("actors/nonmatchings/actor_350500/actor_350500", D_actor_350500_80161E20);

INCLUDE_ASM("actors/nonmatchings/actor_350500/actor_350500", func_actor_350500_80162360);

/// Spawn state of the enemy actor: allocates the 0x4C8-byte work block that
/// every later handler reads through `Task::work`, seeds the three -1 bytes
/// and the three cleared words the work's own init expects, republishes the
/// light and colour matrices onto the display object, then installs the
/// message table and the shared exit handler. An allocation failure ends the
/// task instead of leaving a half-built actor behind.
void func_actor_350500_801623CC(Task* arg0)
{
    Actor350500Work* work;

    work = Mem_Calloc(sizeof(Actor350500Work), false);
    if (work == NULL) {
        Gp_EnemyTaskExit(arg0);
        return;
    }

    arg0->work      = (TaskIdMap*)work;
    work->field_43D = -1;
    work->field_43E = -1;
    work->field_4C5 = -1;
    work->field_4A0 = 0;
    work->field_4A4 = 0;
    work->field_4A8 = 0;

    func_actor_350500_8016247C(arg0);

    arg0->msgTable     = D_actor_350500_80168EB0;
    arg0->exitCallback = ActorsShared801327b4;
    arg0->state        = arg0->state + 1;
}
