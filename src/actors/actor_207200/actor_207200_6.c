#include "common.h"

#include <psyq/inline_c.h>

#include "actors/actors_shared_80136614.h"

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
    coord               = (*(TmdObject**)&arg0->field_2C)->coords;
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
        coord = (*(TmdObject**)&arg0->field_2C)->coords + 1;
    } else {
        angle = ActorsShared80136614((*(TmdObject**)&arg0->field_2C)->coords, &dist);
        if (angle < 0) {
            angle = -angle;
        }
        if (angle < 0x400) {
            coord = (*(TmdObject**)&arg0->field_2C)->coords + 3;
        } else {
            coord = (*(TmdObject**)&arg0->field_2C)->coords + 1;
        }
    }
    ctx->field_18 = coord;
    Gp_LinkNode(&ctx->node);
}

/// `gpf 12`; the `inline_c.h` macro of that name assembles to a different word.
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

/// Reads the matrix column at offsets `o0` / `o1` / `o2` of `r0` into the
/// `SVECTOR` at `r1`. Same macro as `src/actors/lib/actors_shared_801385e0.c`.
#define ACTOR_COPY_MATRIX_COLUMN_TO_SV(r0, r1, o0, o1, o2) \
    __asm__ volatile(                                      \
        "lhu $12, %2(%0);"                                 \
        "lhu $13, %3(%0);"                                 \
        "lhu $14, %4(%0);"                                 \
        "sh $12, 0(%1);"                                   \
        "sh $13, 2(%1);"                                   \
        "sh $14, 4(%1)"                                    \
        :                                                  \
        : "r"(r0), "r"(r1), "i"(o0), "i"(o1), "i"(o2)      \
        : "$12", "$13", "$14", "memory")

/// The way back: the `SVECTOR` at `r0` into the column of `r1`.
#define ACTOR_COPY_SV_TO_MATRIX_COLUMN(r0, r1, o0, o1, o2) \
    __asm__ volatile(                                      \
        "lhu $12, 0(%0);"                                  \
        "lhu $13, 2(%0);"                                  \
        "lhu $14, 4(%0);"                                  \
        "sh $12, %2(%1);"                                  \
        "sh $13, %3(%1);"                                  \
        "sh $14, %4(%1)"                                   \
        :                                                  \
        : "r"(r0), "r"(r1), "i"(o0), "i"(o1), "i"(o2)      \
        : "$12", "$13", "$14", "memory")

/// While `work->field_4A6` is set, runs each column of the node's rotation
/// matrix through GTE `gpf 12` with a zero interpolation factor, zeroing the
/// 3x3 part, and clears `flg` so the node is recomputed.
void func_actor_207200_8014D97C(Actor207200* arg0, GsCOORDINATE2* arg1)
{
    SVECTOR vec;
    MATRIX* m;

    if (arg0->field_1C->field_4A6 != 0) {
        m = &arg1->coord;
        ACTOR_COPY_MATRIX_COLUMN_TO_SV(m, &vec, 0, 6, 12);
        gte_lddp(0);
        gte_ldsv(&vec);
        gte_gpf12_real();
        gte_stsv(&vec);
        ACTOR_COPY_SV_TO_MATRIX_COLUMN(&vec, m, 0, 6, 12);

        ACTOR_COPY_MATRIX_COLUMN_TO_SV(m, &vec, 2, 8, 14);
        gte_lddp(0);
        gte_ldsv(&vec);
        gte_gpf12_real();
        gte_stsv(&vec);
        ACTOR_COPY_SV_TO_MATRIX_COLUMN(&vec, m, 2, 8, 14);

        ACTOR_COPY_MATRIX_COLUMN_TO_SV(m, &vec, 4, 10, 16);
        gte_lddp(0);
        gte_ldsv(&vec);
        gte_gpf12_real();
        gte_stsv(&vec);
        ACTOR_COPY_SV_TO_MATRIX_COLUMN(&vec, m, 4, 10, 16);

        arg1->flg = 0;
    }
}
