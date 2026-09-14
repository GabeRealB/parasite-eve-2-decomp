#include "common.h"

#include "actors/actor_135600.h"

#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

/// The actor's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern TaskFuncTable3 D_actor_135600_80131E30;
extern TaskFuncTable3 D_actor_135600_80131E3C;

void func_actor_135600_80132AB4(void)
{
}

void func_actor_135600_80132ABC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_135600_80131E30;
    sp.funcs[task->state](task);
}

INCLUDE_ASM("actors/nonmatchings/actor_135600/actor_135600_2", func_actor_135600_80132B14);

void func_actor_135600_80132C18(Task* task)
{
    s16 countdown;

    if (Game_Session->field_1 != 0) {
        countdown = task->killCountdown;
        if (countdown > 0 && func_actor_135600_80131E68(((TmdObject*)task->extra)->field_8, countdown) >= 0x1F5) {
            task->killCountdown = 0x800;
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_135600/actor_135600_2", func_actor_135600_80132C80);

void func_actor_135600_80132D64(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_135600_80131E3C;
    sp.funcs[task->state](task);
}

INCLUDE_RODATA("actors/nonmatchings/actor_135600/actor_135600_2", ActorsShared801327f8Table);

INCLUDE_RODATA("actors/nonmatchings/actor_135600/actor_135600_2", ActorsShared80132920Offset);
