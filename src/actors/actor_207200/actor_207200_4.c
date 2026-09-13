#include "common.h"

#include "actors/actor_207200.h"
#include "actors/actors_shared_80135b58.h"

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

/// Rebuilds the first coordinate node of the actor's model from the transform
/// stored in `work->field_464`, scaled along Y by `work->field_49C` (its own
/// angle field, decaying by 0x50 a frame while it sits above 0x200). The 0x30
/// bytes that hold the scaling matrix and its `VECTOR` are borrowed from the
/// scratchpad and released again; the node's `flg` is cleared so the next
/// `Gp_UpdateCoord` recomputes it.
void func_actor_207200_8014D7E8(Actor207200* arg0)
{
    GsCOORDINATE2*              coord;
    MATRIX*                     head;
    ActorShared80135b58Scratch* scratch;
    Actor207200Work*            work;

    head                = *(MATRIX**)0x1F8003FC;
    work                = arg0->field_1C;
    scratch             = (ActorShared80135b58Scratch*)((u8*)head - 0x30);
    *(void**)0x1F8003FC = scratch;
    coord               = (*(TmdObject**)&arg0->field_2C)->field_8;
    if (work->field_49C >= 0x201) {
        work->field_49C = (u16)work->field_49C - 0x50;
    }
    scratch->scale.vx          = 0x1000;
    scratch->scale.vy          = (s32)work->field_49C;
    scratch->scale.vz          = 0x1000;
    coord->coord               = work->field_464;
    scratch->mat.ident.m00_m01 = 0x1000;
    scratch->mat.ident.m02_m10 = 0;
    scratch->mat.ident.m11_m12 = 0x1000;
    scratch->mat.ident.m20_m21 = 0;
    scratch->mat.ident.m22     = 0x1000;
    ScaleMatrix(&scratch->mat.mat, &scratch->scale);
    MulMatrix(&coord->coord, &scratch->mat.mat);
    coord->flg         = 0;
    *(u8**)0x1F8003FC += 0x30;
}

/// Re-picks the model coordinate the context is driven from. While the actor
/// is idle (`work->field_4A6 == 0`) the angle to the player is measured and a
/// large deviation falls back to the second model part, otherwise the fourth
/// part is used.
void func_actor_207200_8014D8DC(Actor207200* arg0)
{
    Actor207200Work* work;
    Actor207200Ctx*  ctx;
    GsCOORDINATE2*   coord;
    s32              dist;
    s32              angle;

    work = arg0->field_1C;
    ctx  = arg0->field_20;
    if (work->field_4A6 != 0) {
        coord = (*(TmdObject**)&arg0->field_2C)->field_8 + 1;
    } else {
        angle = func_actor_207200_8014CE20((*(TmdObject**)&arg0->field_2C)->field_8, &dist);
        if (angle < 0) {
            angle = -angle;
        }
        if (angle < 0x400) {
            coord = (*(TmdObject**)&arg0->field_2C)->field_8 + 3;
        } else {
            coord = (*(TmdObject**)&arg0->field_2C)->field_8 + 1;
        }
    }
    ctx->field_18 = coord;
    Gp_LinkNode(&ctx->node);
}

INCLUDE_ASM("actors/nonmatchings/actor_207200/actor_207200_4", func_actor_207200_8014D97C);
