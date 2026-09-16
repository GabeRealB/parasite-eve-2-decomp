#include "common.h"

#include <psyq/inline_c.h>

#include "actors/actors_shared_8013898c.h"

/// `gpf 12`; the `inline_c.h` macro of that name assembles to a different word.
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

/// Reads the column `r0` holds at offsets `o0` / `o1` / `o2` into the `SVECTOR`
/// at `r1`. The offsets are 6 bytes apart, so the caller passes one column of a
/// matrix's 3x3 part - `4` / `10` / `16` for column 2. `$12` / `$13` / `$14` are
/// the temporaries the target reads the column through, before it stores it
/// back. The same pair of macros `src/actors/actor_403600/actor_403600_2.c`
/// defines for its own transpose.
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

/// Steps the actor along its own forward axis: column 2 of the coordinate's
/// rotation is copied into the caller's vector, scaled by GPF with `arg2` as
/// the distance, and added back into the coordinate's X and Z translation. Y is
/// left alone, so the step stays in the ground plane, and clearing `flg` asks
/// the next coordinate update to rebuild the world matrix. Does nothing while
/// the movement freeze flag is set.
///
/// Byte-identical in the five actor slots 101100 / 104900 / 201100 / 204900 /
/// 301100, none of which references it: no carrier holds a `jal` to it, so the
/// argument types are the ones the body itself needs and nothing here is
/// confirmed by a call site.
void ActorsShared8013898c(ActorsShared8013898cCoord* arg0, ActorsShared8013898cVec* arg1, s32 arg2)
{
    if (D_80072729 == 0) {
        ACTOR_COPY_MATRIX_COLUMN_TO_SV(&arg0->coord, &arg1->vec, 4, 10, 16);
        gte_lddp(arg2);
        gte_ldsv(&arg1->vec);
        gte_gpf12_real();
        gte_stsv(&arg1->vec);
        arg0->coord.t[0] += arg1->vec.vx;
        arg0->coord.t[2] += arg1->vec.vz;
        arg0->flg         = 0;
    }
}
