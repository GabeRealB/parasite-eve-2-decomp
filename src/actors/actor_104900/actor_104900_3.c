#include "common.h"

#include <psyq/inline_c.h>

#include "actors/actors_shared_8013898c.h"
#include "actors/actors_shared_80138efc.h"

void func_actor_104900_80137498(GpEnemy*, Task*, ActorsShared80138efcWork*, void*);

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
void func_actor_104900_8013898C(ActorsShared8013898cCoord* arg0, ActorsShared8013898cVec* arg1, s32 arg2)
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

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900_3", func_actor_104900_80138A2C);

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900_3", func_actor_104900_80138B5C);

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900_3", func_actor_104900_80138C6C);

INCLUDE_ASM("actors/nonmatchings/actor_104900/actor_104900_3", func_actor_104900_80138D58);

/// Arms the 0x15 / 0x16 motion pair on the first frame of the sub-state, then
/// runs the sub-state proper: while bit 1 of the motion flags at 0x9C is set,
/// either keeps the state on the 0x17 motion once `field_B92` has run out and
/// the enemy is not carrying flag 0x2 in `field_4C`, or hands the frame to the
/// shared routine at 0x80137498 on the 0x18 motion.
void func_actor_104900_80138E34(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work, void* scratch)
{
    ActorsShared80138efcMotion* motion = &work->motion;

    if (work->field_BA8 == 0) {
        if (work->field_BAE == 0) {
            work->field_BA4 = 0x15;
        } else {
            work->field_BA4 = 0x16;
        }
        work->field_BA5 = 1;
        work->field_B8C = 0xA;
        work->field_BA8 = (u8)work->field_BA8 + 1;
    }
    if (motion->flags & 2) {
        work->field_B9C = 0;
        if (work->field_B92 > 0) {
            if (!(enemy->field_4C & 2)) {
                work->field_BA6 = 1;
                work->field_BAB = 0x10;
                work->state     = 0x17;
                work->field_BA8 = 0;
                work->field_BAF = 2;
            }
        } else {
            work->state     = 0x18;
            work->field_BA8 = 0;
            func_actor_104900_80137498(enemy, task, work, scratch);
        }
    }
}
