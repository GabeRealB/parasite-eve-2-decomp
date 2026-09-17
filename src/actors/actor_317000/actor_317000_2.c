#include "common.h"

#include "actors/actor_317000.h"
#include "actors/actors_shared_801327b4.h"

#include "gameplay/1BC.h"
#include "gameplay/D4.h"

#include "main/mem.h"
#include "main/task.h"

/// `Gp_DispatchMsg` handler table installed at `Task::field_24` by
/// `func_actor_317000_8016267C`; terminator id 0x7FFFFFFF.
extern GpMsgEntry D_actor_317000_8016CF50[];

/// Spawn state of the enemy actor: allocates the 0x4CC-byte work block every
/// later handler reads through `Task::idMap`, seeds the three -1 fields and the
/// three cleared words the work's own init expects, republishes the light and
/// colour matrices onto the display object, raises display flag 0x80 through
/// the mode-0 call, then installs the message table and the shared exit
/// handler. An allocation failure ends the task instead of leaving a
/// half-built actor behind.
void func_actor_317000_8016267C(Task* arg0)
{
    Actor317000Work* work;

    work = Mem_Calloc(sizeof(Actor317000Work), false);
    if (work == NULL) {
        Gp_EnemyTaskExit(arg0);
        return;
    }

    arg0->idMap     = (TaskIdMap*)work;
    work->field_43D = -1;
    work->field_43E = -1;
    work->field_4C8 = -1;
    work->field_4A0 = 0;
    work->field_4A4 = 0;
    work->field_4A8 = 0;

    func_actor_317000_80162744(arg0);
    func_actor_317000_80162BC4(arg0, 0x7D5, 0, 0);

    arg0->field_24     = D_actor_317000_8016CF50;
    arg0->exitCallback = ActorsShared801327b4;
    arg0->state++;
}

INCLUDE_RODATA("actors/nonmatchings/actor_317000/actor_317000_2", D_actor_317000_80161E30);

INCLUDE_RODATA("actors/nonmatchings/actor_317000/actor_317000_2", D_actor_317000_80161E40);
