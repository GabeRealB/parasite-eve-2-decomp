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

/// Depth-sorts the actor against the camera: the origin is zeroed into a
/// 0x18-byte `G_SCRATCH_HEAD` block, `Gp_UpdateCoord` folds the attach
/// coordinate's matrix chain into `arg0`, that world matrix is loaded into the
/// GTE, and `rtps` projects the origin through it. The block's `otz` is the
/// projection's average screen z, which `arg1`'s screen depth biases after the
/// perspective divide (`>> 4`); the ordering-table body gets the result. A
/// negative `FLAG` means the point fell behind the eye, and its depth is
/// dropped to zero.
void func_actor_402200_80138208(GsCOORDINATE2* arg0, s32 arg1)
{
    u8*                        head;
    Actor402200ProjectScratch* block;
    SVECTOR*                   vec;

    head                                         = *(u8**)G_SCRATCH_HEAD;
    block                                        = (Actor402200ProjectScratch*)(head - sizeof(Actor402200ProjectScratch));
    *(Actor402200ProjectScratch**)G_SCRATCH_HEAD = block;
    block->vec.vx                                = 0;
    block->vec.vy                                = 0;
    block->vec.vz                                = 0;
    Gp_UpdateCoord(arg0);
    vec = &block->vec;
    SOFT_TOUCH_REG(vec);
    gte_SetRotMatrix(&arg0->workm);
    gte_SetTransMatrix(&arg0->workm);
    gte_ldv0(vec);
    gte_rtps_real();
    gte_stsxy(&block->sxy);
    gte_stdp(&block->dp);
    gte_stflg(&block->flag);
    gte_stszotz(&block->otz);
    if (block->flag < 0) {
        block->otz = 0;
    }
    block->otz = (block->otz >> 4) + arg1;
    ActorsShared80131fc8(block->otz);
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x18;
}
