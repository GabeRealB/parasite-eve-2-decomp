#include "common.h"

#include <psyq/inline_c.h>

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/task.h"
#include "main/tmd.h"
#include "pe/antibody.h"

extern s32 Gp_LcgState;

/// `gpf 1`. The `inline_c.h` macro of that name assembles to a different word,
/// so spell the instruction out.
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

void func_antibody_8012FBB0(GsCOORDINATE2* arg0, s16 arg1, s16 arg2, s16 arg3);
void func_antibody_8012FFEC(GsCOORDINATE2* arg0, s16 arg1, s16 arg2, s16 arg3);
void func_antibody_80130428(GsCOORDINATE2* arg0, s16 arg1, s16 arg2);

INCLUDE_ASM("pe/nonmatchings/antibody/antibody", func_antibody_8012EF34);

/// Runs one frame of an antibody mote. State 0 re-bases the effect coordinate
/// on the `GpEffWork.field_8` parent with an identity rotation and the work
/// block's `field_18`..`field_1C` offset, then GPF-scales that offset by 0x100
/// (a sixteenth) into `field_10`..`field_14` as the per-frame step, and seeds
/// the intensity `field_20` from the combo counter, the draw parameter
/// `field_24` from that row's `field_6` and the phase `field_26` from
/// `Gp_LcgState`. State 1 walks the coordinate back down that step every frame
/// and draws with `func_antibody_8012FBB0`; past tick 0x10 it parks a `-0x80`
/// Y drift in `field_12` and moves to state 2, and one frame in sixteen it
/// jumps straight to state 3 instead. State 2 applies that Y drift and keeps
/// drawing; state 3 draws the larger `func_antibody_8012FFEC` /
/// `func_antibody_80130428` pair. All three re-roll `field_24` / `field_26`
/// from the row's `field_8` one frame in eight, and states 2 and 3 release the
/// effect at tick 0x15.
void func_antibody_8012F734(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GpMtxWords*    rot;
    s32            rng0;
    s32            rng1a;
    s32            rng1b;
    s32            rng1c;
    s32            rng1d;
    s32            rng2a;
    s32            rng2b;
    s32            rng2c;
    s32            rng3a;
    s32            rng3b;
    s32            rng3c;
    s16            idx;

    mem           = arg0->spawnArg2;
    coord         = ((TmdObject*)arg0->extra)->field_8;
    mem->field_22 = (u16)mem->field_22 + 1;
    coord->flg    = 0;
    switch (arg0->state) {
        case 0:
            rot        = (GpMtxWords*)&coord->coord;
            coord->sub = mem->field_8;
            rot->w0    = 0x1000;
            rot->w1    = 0;
            rot->w2    = 0x1000;
            rot->w3    = 0;
            rot->h4    = 0x1000;

            coord->coord.t[0] = mem->field_18;
            coord->coord.t[1] = mem->field_1A;
            coord->coord.t[2] = mem->field_1C;

            gte_lddp(0x100);
            gte_ldsv(&mem->field_18);
            gte_gpf12_real();
            gte_stsv(&mem->field_10);

            arg0->state   = 1;
            rng0          = Gp_LcgState * 5 + 0x71357911;
            Gp_LcgState   = rng0;
            idx           = Gp_StateC08.field_0 % 10 - 1;
            mem->field_20 = idx;
            mem->field_24 = D_antibody_80130BD4[idx].field_6;
            mem->field_26 = ((u32)rng0 >> 16) & 0xFFF;
            /* fallthrough */
        case 1:
            rng1a       = Gp_LcgState * 5 + 0x71357911;
            Gp_LcgState = rng1a;
            if ((((u32)rng1a >> 16) & 7) == 0) {
                rng1b       = rng1a * 5 + 0x71357911;
                Gp_LcgState = rng1b;
                mem->field_24 =
                    D_antibody_80130BD4[mem->field_20].field_8 + (((u32)rng1b >> 16) & 0x1FF);
                rng1c         = rng1b * 5 + 0x71357911;
                Gp_LcgState   = rng1c;
                mem->field_26 = ((u32)rng1c >> 16) & 0xFFF;
            }
            coord->coord.t[0] -= mem->field_10;
            coord->coord.t[1] -= mem->field_12;
            coord->coord.t[2] -= mem->field_14;
            coord->flg         = 0;
            Gp_UpdateCoord(coord);
            func_antibody_8012FBB0(coord, mem->field_22, mem->field_24, mem->field_26);
            if (mem->field_22 >= 0x10) {
                mem->field_12 = -0x80;
                arg0->state   = 2;
                return;
            }
            rng1d       = Gp_LcgState * 5 + 0x71357911;
            Gp_LcgState = rng1d;
            if ((((u32)rng1d >> 16) & 0xF) == 0) {
                arg0->state = 3;
            }
            return;
        case 2:
            rng2a       = Gp_LcgState * 5 + 0x71357911;
            Gp_LcgState = rng2a;
            if ((((u32)rng2a >> 16) & 7) == 0) {
                rng2b       = rng2a * 5 + 0x71357911;
                Gp_LcgState = rng2b;
                mem->field_24 =
                    D_antibody_80130BD4[mem->field_20].field_8 + (((u32)rng2b >> 16) & 0x1FF);
                rng2c         = rng2b * 5 + 0x71357911;
                Gp_LcgState   = rng2c;
                mem->field_26 = ((u32)rng2c >> 16) & 0xFFF;
            }
            coord->coord.t[1] += mem->field_12;
            coord->flg         = 0;
            Gp_UpdateCoord(coord);
            func_antibody_8012FBB0(coord, mem->field_22, mem->field_24, mem->field_26);
            goto check;
        case 3:
            rng3a       = Gp_LcgState * 5 + 0x71357911;
            Gp_LcgState = rng3a;
            if ((((u32)rng3a >> 16) & 7) == 0) {
                rng3b         = rng3a * 5 + 0x71357911;
                Gp_LcgState   = rng3b;
                mem->field_24 = (s16)D_antibody_80130BD4[mem->field_20].field_8 * 2 +
                                (((u32)rng3b >> 16) & 0x1FF);
                rng3c         = rng3b * 5 + 0x71357911;
                Gp_LcgState   = rng3c;
                mem->field_26 = ((u32)rng3c >> 16) & 0xFFF;
            }
            Gp_UpdateCoord(coord);
            func_antibody_8012FFEC(coord, mem->field_22, mem->field_24, mem->field_26);
            func_antibody_80130428(coord, mem->field_22, mem->field_24);
        check:
            if (mem->field_22 >= 0x15) {
                Gp_ReleaseState1CMem(mem, arg0);
            }
            break;
    }
}

INCLUDE_ASM("pe/nonmatchings/antibody/antibody", func_antibody_8012FBB0);

INCLUDE_ASM("pe/nonmatchings/antibody/antibody", func_antibody_8012FFEC);

INCLUDE_ASM("pe/nonmatchings/antibody/antibody", func_antibody_80130428);

INCLUDE_RODATA("pe/nonmatchings/antibody/antibody", D_antibody_8012EF30);
