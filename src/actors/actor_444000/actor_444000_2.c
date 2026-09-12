#include "common.h"

#include "actors/actor_444000.h"
#include "gameplay/D4.h"
#include "main/session.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_444000_80131E90;

extern s16 D_actor_444000_80144A72;

/// The overlay's event/controller task, whose `idMap` holds an
/// `Actor444000EventWork`.
extern Task*        D_actor_444000_80161860;
extern Actor444000* D_actor_444000_80161878;

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_80132778);

/// Set the actor's action index, resetting the sub-state counter that goes
/// with it.
void func_actor_444000_801327E8(s16 action)
{
    Actor444000EventWork* work = (Actor444000EventWork*)D_actor_444000_80161860->idMap;

    work->field_2C = action;
    work->field_2E = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_80132808);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_80132B14);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_80132CB8);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_80133010);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_80133C58);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_80133DE4);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_80133F64);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_80134040);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_801341C4);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_8013441C);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_80134688);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_8013482C);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_80135448);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_801371E8);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_80137594);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_8013799C);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_80137D4C);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_801381B0);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_80138490);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_801389EC);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_80138B94);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_80138FC4);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_8013928C);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_80139594);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_80139AF8);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_80139C80);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_80139EE4);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_8013A1C4);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_8013A3AC);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_8013A77C);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_8013A958);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_8013ACD0);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_8013AFF8);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_8013C060);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_8013C4B0);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_8013CA60);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_8013D128);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_8013D810);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_8013D96C);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_8013E058);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_8013EC84);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_8013FB74);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_801404C0);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_80140BBC);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_80140E28);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_8014105C);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_801411C8);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_80141618);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_80141DFC);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_80142254);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_801423C4);

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_2", func_actor_444000_80142F28);
