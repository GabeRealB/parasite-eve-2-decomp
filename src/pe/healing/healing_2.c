#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/task.h"
#include "main/tmd.h"

/// Per-level scale row. `field_2` is the starting radius; after frame 0x10 it
/// decays by that value >> 4. Indexed by `(Gp_StateC08.field_0 % 10) - 1`.
typedef struct HealingScale {
    /* 0x0 */ s16 unk0;
    /* 0x2 */ u16 field_2;
    /* 0x4 */ s16 unk4;
    /* 0x6 */ s16 unk6;
} HealingScale;
STATIC_ASSERT_SIZEOF(HealingScale, 8);

extern HealingScale D_healing_8012FC1C[];

void func_healing_8012F7FC(GsCOORDINATE2* arg0, s16 arg1, s16 arg2, s16 arg3);

void func_healing_8012F5E4(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s32            y;
    s16            step;
    s16            kind;
    GpEffWork*     spawned;

    mem           = arg0->spawnArg2;
    coord         = ((TmdObject*)arg0->extra)->field_8;
    mem->field_22 = (u16)mem->field_22 + 1;
    if (arg0->state == 0) {
        coord->sub        = mem->field_8;
        coord->coord.t[0] = mem->field_18;
        coord->coord.t[1] = mem->field_1A;
        coord->coord.t[2] = mem->field_1C;
        coord->flg        = 0;
        Gp_UpdateCoord(coord);
        mem->field_12 = 4;
        mem->field_10 = 0;
        mem->field_14 = 0;
        arg0->state   = 1;
        kind          = (Gp_StateC08.field_0 % 10U) - 1;
        mem->field_2A = kind;
        mem->field_24 = D_healing_8012FC1C[kind].field_2;
        mem->field_26 = (u16)arg0->spawnArg1 & 0xFFF;
    }
    step              = mem->field_12;
    y                 = coord->coord.t[1] + step;
    coord->flg        = 0;
    coord->coord.t[1] = y;
    Gp_UpdateCoord(coord);
    if (mem->field_22 < 0x1E) {
        if ((u16)mem->field_22 & 1) {
            mem->field_20 = (u16)mem->field_20 + 1;
            if (mem->field_22 >= 0x10) {
                mem->field_24 = (u16)mem->field_24 - ((s16)D_healing_8012FC1C[mem->field_2A].field_2 >> 4);
            }
            if (mem->field_2A < 2) {
                func_800EB6E8(coord, (u16)mem->field_20, (u16)mem->field_26,
                              (u16)mem->field_24);
            } else {
                func_healing_8012F7FC(coord, mem->field_20, mem->field_26, mem->field_24);
            }
            if (((u16)mem->field_22 & 7) == 1) {
                spawned = Gp_SpawnEff(0x60016, coord, mem->field_26, 0);
                if (spawned != NULL) {
                    Task_Reparent(arg0, spawned->field_0);
                }
            }
        }
    } else {
        Gp_ReleaseState1CMem(mem, arg0);
    }
}

INCLUDE_ASM("pe/nonmatchings/healing/healing_2", func_healing_8012F7FC);
