#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "main/mem.h"
#include "main/task.h"

#include "gameplay/gameplay.h"

#include "actors/actor_402200.h"

/// Main-executable global with no module header yet: the remaining-enemy count.
extern s16 D_80073BA0;

void func_actor_402200_80136D9C(s32 otz);

/// Raises the actor's phase `field_6F4` to 1 while enemies remain.
s32 func_actor_402200_801381E0(Task* task)
{
    if (D_80073BA0 > 0) {
        ((Actor402200Work*)task->work)->field_6F4 = 1;
    }
    return 0;
}

/// Projects the origin of `arg0` to find its ordering-table depth, adds
/// `arg1`, and queues the frame-buffer pass `func_actor_402200_80136D9C` there
/// (at depth `arg1` when the projection fails).
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
    gte_rtps();
    gte_stsxy(&block->sxy);
    gte_stdp(&block->dp);
    gte_stflg(&block->flag);
    gte_stszotz(&block->otz);
    if (block->flag < 0) {
        block->otz = 0;
    }
    block->otz = (block->otz >> 4) + arg1;
    func_actor_402200_80136D9C(block->otz);
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x18;
}
