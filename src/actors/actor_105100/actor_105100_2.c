#include "common.h"
#include "gameplay/1BC.h"

#include "actors/actor_105100.h"

#include "main/mem.h"
#include "main/task.h"

/// Each enemy task's three state handlers - spawn/setup, per-frame tick
/// and teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_105100_80131E90;
extern GpEnemyTaskFuncTable3 D_actor_105100_80131EB0;

INCLUDE_RODATA("actors/nonmatchings/actor_105100/actor_105100_2", D_actor_105100_80131EB0);

void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

void func_actor_105100_80136524(Actor105100* arg0)
{
    GsCOORDINATE2* coord;
    VECTOR3        vec;

    coord  = arg0->field_2C->field_8;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_DrawEffGroundQuad(&vec, 0x9C4, 0x80);
}

/// Multiplies `arg1` into the coordinate `arg0->field_2C` points at, scaled by
/// `arg2`: through a 0x30-byte block borrowed from the scratchpad and released
/// again, an identity rotation is splatted word-wise and `ScaleMatrix` scales
/// it. `arg3` selects the axis layout - non-zero scales all three axes by
/// `arg2`, zero leaves X and Z at 0x1000 and scales only Y.
///
/// The scratchpad head is deliberately written twice, from two separate
/// computations of `head - 0x30`. CSE cannot substitute a value that holds no
/// register, so the store keeps the block-local `$v1` while `blk` - which
/// crosses both calls - is copied into `$s0` by `reload_cse_regs`. Folding the
/// two into one variable allocates `blk`'s register for the store as well and
/// loses the copy, the delay-slot fill and the frame layout.
void func_actor_105100_80136574(Actor105100* arg0, MATRIX* arg1, s16 arg2, s32 arg3)
{
    void*               head;
    Actor105100Scratch* blk;
    GsCOORDINATE2*      coord;

    head                    = *(void**)G_SCRATCH_HEAD;
    *(void**)G_SCRATCH_HEAD = (u8*)head - 0x30;
    blk                     = (Actor105100Scratch*)((u8*)head - 0x30);
    coord                   = arg0->field_2C->field_8;

    if (arg3 == 0) {
        blk->scale.vx = 0x1000;
        blk->scale.vy = arg2;
        blk->scale.vz = 0x1000;
    } else {
        blk->scale.vx = arg2;
        blk->scale.vy = arg2;
        blk->scale.vz = arg2;
    }

    coord->coord = *arg1;

    blk->mat.ident.m00_m01 = 0x1000;
    blk->mat.ident.m02_m10 = 0;
    blk->mat.ident.m11_m12 = 0x1000;
    blk->mat.ident.m20_m21 = 0;
    blk->mat.ident.m22     = 0x1000;

    ScaleMatrix(&blk->mat.mat, &blk->scale);
    MulMatrix(&coord->coord, &blk->mat.mat);
    coord->flg              = 0;
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x30;
}

void func_actor_105100_8013667C(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_105100_80131E90;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

void func_actor_105100_801366D8(GpEnemy* arg0, Task* arg1)
{
    Actor105100Work* work;

    work = (Actor105100Work*)arg1->idMap;
    Gp_UnlinkObj(&work->obj0);
    Gp_UnlinkObj(&work->obj38);
    Gp_DestroyEnemy(arg0, arg1);
}

void func_actor_105100_8013672C(Task* arg0)
{
    GpEnemyTaskFuncTable3 sp;

    sp = D_actor_105100_80131EB0;
    sp.funcs[arg0->state](arg0->spawnArg2, arg0);
}

void func_actor_105100_80136788(GpEnemy* arg0, Task* arg1)
{
    Gp_UnlinkObj(arg1->idMap);
    Gp_DestroyEnemy(arg0, arg1);
}
