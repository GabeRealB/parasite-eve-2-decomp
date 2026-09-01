#include "common.h"

#include "main/task.h"

#include "gameplay/1BC.h"

#include "gameplay/3CD8.h"

#include "gameplay/3FB8.h"

#include "gameplay/D4.h"

#include "main/session.h"

#include "actors/actor_503500.h"

/// `Gp_DispatchMsg` handler table installed at `Task::field_24` by
/// `func_actor_503500_80132430`; terminator id 0x7FFFFFFF.
extern GpMsgEntry D_actor_503500_80146888[];

void func_actor_503500_801324C4(Task* arg0);

void func_actor_503500_801324EC(Task* arg0);

#include "main/mem.h"

#include "main/sound.h"

#include "main/tmd.h"

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_8013223C);

void func_actor_503500_80132430(Task* arg0)
{
    GameActorExt*        ext;
    Actor503500ColorMtx* work;

    ext  = arg0->extra;
    work = Mem_Calloc(sizeof(Actor503500ColorMtx), false);
    if (work == NULL) {
        Gp_EnemyTaskExit(arg0);
        return;
    }

    arg0->idMap    = (TaskIdMap*)work;
    ext->field_C  |= 0x84;
    work->field_44 = 0;
    func_actor_503500_801324EC(arg0);
    arg0->field_24     = D_actor_503500_80146888;
    arg0->exitCallback = func_actor_503500_801324C4;
    arg0->state       += 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_801324C4);

void func_actor_503500_801324EC(Task* arg0)
{
    GameActorExt*        ext;
    Actor503500ColorMtx* work;

    ext           = arg0->extra;
    work          = (Actor503500ColorMtx*)arg0->idMap;
    ext->field_1C = &work->light;
    ext->field_20 = &work->color;
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_80132508);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_80132584);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_80132664);

INCLUDE_RODATA("actors/nonmatchings/actor_503500/actor_503500", D_actor_503500_80131E20);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_8013270C);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_80132778);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_80132990);

INCLUDE_RODATA("actors/nonmatchings/actor_503500/actor_503500", D_actor_503500_80131E44);
