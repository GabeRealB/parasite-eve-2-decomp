#include "common.h"

#include "actors/actor_123200.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include <psyq/inline_c.h>

/// Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c).
#define SCRATCH_SP (*(u32*)0x1F8003FC)

#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

void func_actor_123200_801332E0(Task* task);

void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

extern u8 D_80072729;

extern u8 D_801153F4;

INCLUDE_ASM("actors/nonmatchings/actor_123200/actor_123200", func_actor_123200_801329F0);

INCLUDE_ASM("actors/nonmatchings/actor_123200/actor_123200", func_actor_123200_80132B94);
