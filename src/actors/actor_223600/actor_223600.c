#include "common.h"

#include "actors/actor_223600.h"
#include "actors/actors_shared_80134178.h"
#include "gameplay/3A34.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/tmd.h"

#include <psyq/inline_c.h>

#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

INCLUDE_ASM("actors/nonmatchings/actor_223600/actor_223600", func_actor_223600_8014AA04);
