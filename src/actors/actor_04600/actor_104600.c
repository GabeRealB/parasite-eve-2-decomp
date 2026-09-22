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
void Actor04600_Fn003D4(Task* arg0);
void Actor04600_Fn00978(Task* arg0);
void Actor04600_Fn02618(Task* arg0);
void Actor04600_Fn02870(void* arg0, Task* arg1);

extern u8 D_801153F4;

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 Actor04600_D00004;

INCLUDE_ASM("actors/nonmatchings/actor_04600/actor_104600", Actor04600_Fn00048);
INCLUDE_RODATA("actors/nonmatchings/actor_04600/actor_104600", Actor04600_D00004);

INCLUDE_RODATA("actors/nonmatchings/actor_04600/actor_104600", Actor04600_D00010);

INCLUDE_ASM("actors/nonmatchings/actor_04600/actor_104600", Actor04600_Fn003D4);

INCLUDE_ASM("actors/nonmatchings/actor_04600/actor_104600", Actor04600_Fn005B0);

INCLUDE_ASM("actors/nonmatchings/actor_04600/actor_104600", Actor04600_Fn007B0);

INCLUDE_ASM("actors/nonmatchings/actor_04600/actor_104600", Actor04600_Fn00978);

INCLUDE_ASM("actors/nonmatchings/actor_04600/actor_104600", Actor04600_Fn00EC8);

INCLUDE_ASM("actors/nonmatchings/actor_04600/actor_104600", Actor04600_Fn00FD8);

INCLUDE_ASM("actors/nonmatchings/actor_04600/actor_104600", Actor04600_Fn01110);

INCLUDE_ASM("actors/nonmatchings/actor_04600/actor_104600", Actor04600_Fn01604);

INCLUDE_ASM("actors/nonmatchings/actor_04600/actor_104600", Actor04600_Fn017CC);

INCLUDE_RODATA("actors/nonmatchings/actor_04600/actor_104600", Actor04600_D0003C);
