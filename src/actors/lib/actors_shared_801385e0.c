#include "common.h"

#include <psyq/inline_c.h>

#include "main/mem.h"

#include "actors/actors_shared_801385e0.h"

/// `gpf 12`; the `inline_c.h` macro of that name assembles to a different word.
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

/// Reads the column `r0` holds at offsets `o0` / `o1` / `o2` into the `SVECTOR`
/// at `r1`. The offsets are 6 bytes apart, so the caller passes one column of a
/// matrix's 3x3 part - `0` / `6` / `12` for column 0. `$12` / `$13` / `$14` are
/// the temporaries the target reads the column through, before it stores it
/// back. `src/actors/lib/actors_shared_8013898c.c` defines the same macro.
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

/// The way back: the three halves the GTE left in the scratch `SVECTOR` at `r0`
/// are stored into the matrix column at `o0` / `o1` / `o2` of `r1`.
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

void ActorsShared801385e0(MATRIX* arg0, ActorsShared801385e0Scale* arg1)
{
    void**   scratch;
    void*    head;
    SVECTOR* vec;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    vec      = (SVECTOR*)((u8*)head - 8);
    *scratch = vec;

    ACTOR_COPY_MATRIX_COLUMN_TO_SV(arg0, vec, 0, 6, 12);
    gte_lddp(arg1->vx);
    gte_ldsv(vec);
    gte_gpf12_real();
    gte_stsv(vec);
    ACTOR_COPY_SV_TO_MATRIX_COLUMN(vec, arg0, 0, 6, 12);

    ACTOR_COPY_MATRIX_COLUMN_TO_SV(arg0, vec, 2, 8, 14);
    gte_lddp(arg1->vy);
    gte_ldsv(vec);
    gte_gpf12_real();
    gte_stsv(vec);
    ACTOR_COPY_SV_TO_MATRIX_COLUMN(vec, arg0, 2, 8, 14);

    ACTOR_COPY_MATRIX_COLUMN_TO_SV(arg0, vec, 4, 10, 16);
    gte_lddp(arg1->vz);
    gte_ldsv(vec);
    gte_gpf12_real();
    gte_stsv(vec);
    ACTOR_COPY_SV_TO_MATRIX_COLUMN(vec, arg0, 4, 10, 16);

    head     = *scratch;
    *scratch = (u8*)head + 8;
}
