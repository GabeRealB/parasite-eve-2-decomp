#include "common.h"

#include "actors/actor_107000.h"
#include "actors/actors_shared_80134810.h"
#include "main/task.h"
#include "main/mem.h"
#include <psyq/inline_c.h>

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
