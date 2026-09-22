#include "common.h"
#include "actors/actor_107000.h"
#include "actors/actors_shared_80134810.h"
#include "main/task.h"
#include "main/mem.h"
#include <psyq/inline_c.h>
#include "actors/actors_shared_80135b58.h"
#include "main/tmd.h"
#include "actors/actors_shared_8014ca28.h"
#include "gameplay/1BC.h"

#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

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

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */
#define SCRATCH_SP (*(u32*)0x1F8003FC)

MATRIX* ScaleMatrix(MATRIX* m, VECTOR* v);
MATRIX* MulMatrix(MATRIX* m0, MATRIX* m1);

void ActorsShared80134810(Task* arg0, GsCOORDINATE2* arg1)
{
    ActorScaleScratchHead* scratch;
    SVECTOR*               vec;
    MATRIX*                matrix;
    Actor107000Work*       work;

    scratch = (ActorScaleScratchHead*)G_SCRATCH_HEAD;
    vec     = scratch->head;
    work    = arg0->work;
    vec--;
    scratch->head = vec;
    if ((u32)work->field_2AC >= 0x13E8U) {
        work->field_2AC = 0x13E8;
    }
    if ((u32)work->field_2AC < 0x1001U) {
        work->field_2AC = 0x1000;
    }
    matrix = &arg1->coord;

    ACTOR_COPY_MATRIX_COLUMN_TO_SV(matrix, vec, 0, 6, 12);
    gte_lddp(work->field_2AC);
    gte_ldsv(vec);
    gte_gpf12_real();
    gte_stsv(vec);
    ACTOR_COPY_SV_TO_MATRIX_COLUMN(vec, matrix, 0, 6, 12);

    ACTOR_COPY_MATRIX_COLUMN_TO_SV(matrix, vec, 2, 8, 14);
    gte_lddp(work->field_2AC);
    gte_ldsv(vec);
    gte_gpf12_real();
    gte_stsv(vec);
    ACTOR_COPY_SV_TO_MATRIX_COLUMN(vec, matrix, 2, 8, 14);

    ACTOR_COPY_MATRIX_COLUMN_TO_SV(matrix, vec, 4, 10, 16);
    gte_lddp(work->field_2AC);
    gte_ldsv(vec);
    gte_gpf12_real();
    gte_stsv(vec);
    ACTOR_COPY_SV_TO_MATRIX_COLUMN(vec, matrix, 4, 10, 16);

    scratch->head = (u8*)scratch->head + 8;
}

/// Rebuilds the first coordinate node of the actor's model from the transform
/// stored in `work->field_28C`, scaled along Y by `work->field_2CA` (its own
/// angle field, decaying by 0x50 a frame while it sits above 0x200). The 0x30
/// bytes that hold the scaling matrix and its `VECTOR` are borrowed from the
/// scratchpad and released again; the node's `flg` is cleared so the next
/// `Gp_UpdateCoord` recomputes it.
///
/// Carried by four enemy slots - `actor_104600`, `actor_107000`, `actor_204600`
/// and `actor_207000` - which is why it takes the `Task` rather than either
/// overlay's own context type; the shared span is in `configs/USA/overlays.toml`.
void ActorsShared801349d8(Task* arg0)
{
    GsCOORDINATE2*              coord;
    MATRIX*                     head;
    ActorShared80135b58Scratch* scratch;
    Actor107000Work*            work;

    head                = *(MATRIX**)0x1F8003FC;
    work                = arg0->work;
    scratch             = (ActorShared80135b58Scratch*)((u8*)head - 0x30);
    *(void**)0x1F8003FC = scratch;
    coord               = (*(TmdObject**)&arg0->extra)->coords;
    if (work->field_2CA >= 0x201) {
        work->field_2CA = (u16)work->field_2CA - 0x50;
    }
    scratch->scale.vx          = 0x1000;
    scratch->scale.vy          = (s32)work->field_2CA;
    scratch->scale.vz          = 0x1000;
    coord->coord               = work->field_28C;
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

void ActorsShared8014ca28(Task* task)
{
    ActorShared8014ca28Work* work;
    GpEnemy*                 enemy;

    enemy = task->spawnArg2;
    work  = (ActorShared8014ca28Work*)task->work;

    enemy->recs = 0;
    Gp_UnlinkNode(&enemy->node);
    Gp_UnlinkObj(&work->field_FC);
    Gp_UnlinkObj(&work->field_134);
    Gp_UnlinkObj(&work->field_1B4);
    Gp_UnlinkObj(&work->field_1EC);
    Gp_EnemyTaskExit(task);
}
