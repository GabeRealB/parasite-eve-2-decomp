#include "common.h"
#include <psyq/inline_c.h>
#include "actors/actors_shared_80137f1c.h"
#include "main/task.h"
#include "main/tmd.h"

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

void ActorsShared80137f1c(Task* arg0)
{
    SVECTOR                   vec;
    MATRIX*                   m;
    ActorsShared80137f1cWork* work;
    GsCOORDINATE2*            coord;

    work  = (ActorsShared80137f1cWork*)arg0->work;
    coord = ((TmdObject*)arg0->extra)->coords;
    if (work->field_384 != 0) {
        m = &coord[5].coord;
        ACTOR_COPY_MATRIX_COLUMN_TO_SV(m, &vec, 0, 6, 12);
        gte_lddp(work->field_384 + 0x1000);
        gte_ldsv(&vec);
        gte_gpf12_real();
        gte_stsv(&vec);
        ACTOR_COPY_SV_TO_MATRIX_COLUMN(&vec, m, 0, 6, 12);

        ACTOR_COPY_MATRIX_COLUMN_TO_SV(m, &vec, 2, 8, 14);
        gte_lddp((work->field_384 >> 2) + 0x1000);
        gte_ldsv(&vec);
        gte_gpf12_real();
        gte_stsv(&vec);
        ACTOR_COPY_SV_TO_MATRIX_COLUMN(&vec, m, 2, 8, 14);

        ACTOR_COPY_MATRIX_COLUMN_TO_SV(m, &vec, 4, 10, 16);
        gte_lddp((work->field_384 >> 2) + 0x1000);
        gte_ldsv(&vec);
        gte_gpf12_real();
        gte_stsv(&vec);
        ACTOR_COPY_SV_TO_MATRIX_COLUMN(&vec, m, 4, 10, 16);

        coord[5].flg = 0;
    }
}
