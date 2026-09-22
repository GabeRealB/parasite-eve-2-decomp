#include "actors/actors_shared_80134810.h"
#include "common.h"

#include "actors/actor_104600.h"
#include "actors/actors_shared_80134680.h"
#include "actors/actors_shared_80134700.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"

#include <psyq/libgs.h>

void Gp_UpdateCoord(GsCOORDINATE2* arg0);
void func_actor_104600_801321F4(Task* arg0);
void func_actor_104600_80132798(Task* arg0);
void func_actor_104600_80134438(Task* arg0);
void func_actor_104600_80134690(void* arg0, Task* arg1);

extern u8 D_801153F4;

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_104600_80131E24;

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600", func_actor_104600_80131E68);
INCLUDE_RODATA("actors/nonmatchings/actor_104600/actor_104600", D_actor_104600_80131E24);

INCLUDE_RODATA("actors/nonmatchings/actor_104600/actor_104600", D_actor_104600_80131E30);

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600", func_actor_104600_801321F4);

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600", func_actor_104600_801323D0);

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600", func_actor_104600_801325D0);

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600", func_actor_104600_80132798);

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600", func_actor_104600_80132CE8);

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600", func_actor_104600_80132DF8);

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600", func_actor_104600_80132F30);

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600", func_actor_104600_80133424);

INCLUDE_ASM("actors/nonmatchings/actor_104600/actor_104600", func_actor_104600_801335EC);

INCLUDE_RODATA("actors/nonmatchings/actor_104600/actor_104600", D_actor_104600_80131E5C);
