#include "common.h"

#include "main/task.h"

#include "gameplay/1BC.h"

/// The enemy's four main-body handlers, dispatched through by state. Two
/// separate state machines in this overlay run the same dispatch shape over
/// their own table.
extern GpEnemyTaskFuncTable4 D_actor_207000_80149E5C;

INCLUDE_ASM("actors/nonmatchings/actor_207000/actor_207000_5", func_actor_207000_8014CC2C);

INCLUDE_ASM("actors/nonmatchings/actor_207000/actor_207000_5", func_actor_207000_8014CF84);

INCLUDE_ASM("actors/nonmatchings/actor_207000/actor_207000_5", func_actor_207000_8014D280);

INCLUDE_ASM("actors/nonmatchings/actor_207000/actor_207000_5", func_actor_207000_8014D60C);

INCLUDE_ASM("actors/nonmatchings/actor_207000/actor_207000_5", func_actor_207000_8014DC28);

INCLUDE_ASM("actors/nonmatchings/actor_207000/actor_207000_5", func_actor_207000_8014E094);

INCLUDE_ASM("actors/nonmatchings/actor_207000/actor_207000_5", func_actor_207000_8014E288);

INCLUDE_ASM("actors/nonmatchings/actor_207000/actor_207000_5", func_actor_207000_8014E4D8);
