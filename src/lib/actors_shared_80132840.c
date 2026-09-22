#include "common.h"

#include "actors/actor_143900.h"
#include "gameplay/3CD8.h"
#include "main/tmd.h"

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */
#define SCRATCH_SP (*(u32*)0x1F8003FC)

void ActorsShared80132840(void)
{
    s32 i;

    i = 1;
    do {
        Gp_AnimTickIndex(&ActorsShared80131f9cWork->anim, i);
        i++;
    } while (i < 0x14);
}
