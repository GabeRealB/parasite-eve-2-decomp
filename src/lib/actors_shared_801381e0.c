#include "common.h"
#include "actors/actors_shared_801381e0.h"
#include "actors/actors_shared_80131fc8.h"
#include "main/mem.h"
#include "gameplay/gameplay.h"
#include "actors/actor_402200.h"
#include "psyq/inline_c.h"

/// Main-executable global with no module header yet: the remaining-enemy count.
extern s16 D_80073BA0;

/// `rtps`: project V0 through the loaded rotation and translation matrices.
/// The `inline_c.h` macro of that name assembles to a different word, so the
/// opcode is written out with its two delay slots kept explicit.
#define gte_rtps_real() __asm__ volatile("nop; nop; .word 0x4A180001")

s32 ActorsShared801381e0(Task* task)
{
    if (D_80073BA0 > 0) {
        ((ActorsShared801381e0Work*)task->work)->field_6F4 = 1;
    }
    return 0;
}

void ActorsShared8013820c(GsCOORDINATE2* arg0, s32 arg1)
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
