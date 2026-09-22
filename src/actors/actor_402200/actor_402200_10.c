#include "common.h"

#include "actors/actors_shared_80131fc8.h"

#include "main/mem.h"

#include "gameplay/gameplay.h"

#include "actors/actor_402200.h"

#include "psyq/inline_c.h"

/// `rtps`: project V0 through the loaded rotation and translation matrices.
/// The `inline_c.h` macro of that name assembles to a different word, so the
/// opcode is written out with its two delay slots kept explicit.
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")
