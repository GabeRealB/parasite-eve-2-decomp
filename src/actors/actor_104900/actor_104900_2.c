#include "common.h"

#include "actors/actor_104900.h"
#include "actors/actors_shared_801384ac.h"
#include "actors/actors_shared_801388e8.h"
#include "actors/actors_shared_80138efc.h"
#include "actors/actors_shared_801511c8.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/sound.h"

#include <psyq/inline_c.h>

extern u32       Gp_LcgState;
extern u8        D_actor_104900_80147480[];
extern GpU16Pair D_actor_104900_801392F0[];

/// `mvmva 1, 0, 0, 3, 0`: rotate V0 by the rotation matrix with no translation
/// vector added. The `inline_c.h` macro of that name assembles to a different
/// word, so spell the instruction out.
#define gte_rtv0_real() __asm__ volatile("nop; nop; .word 0x4A486012")
/// `gpf 1`: scale IR1..3 by IR0. Same reason as above for spelling out the word.
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

void func_actor_104900_80137498(GpEnemy*, Task*, ActorsShared80138efcWork*, void*);
s32  func_actor_104900_80132D78(GpEnemy*, Task*, ActorsShared80138efcWork*, void*);
void ActorsShared801357f0(GpEnemy*, Task*, ActorsShared80138efcWork*, ActorsShared80138efcArg*);

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900_2", func_actor_104900_801359CC);
