#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/task.h"
#include "main/tmd.h"

extern s32 Gp_LcgState;

INCLUDE_ASM("pe/nonmatchings/energyshot/energyshot", func_energyshot_8012EF34);

INCLUDE_ASM("pe/nonmatchings/energyshot/energyshot", func_energyshot_8012F750);

INCLUDE_ASM("pe/nonmatchings/energyshot/energyshot", func_energyshot_8012FA50);

void func_energyshot_8012FFB8(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s32            y;

    mem           = arg0->spawnArg2;
    coord         = ((TmdObject*)arg0->extra)->field_8;
    mem->field_22 = (u16)mem->field_22 + 1;
    if (arg0->state == 0) {
        mem->field_10 = 0;
        mem->field_14 = 0;
        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
        mem->field_12 = 0xFFF0 - (((u32)Gp_LcgState >> 16) & 0x3F);
        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
        mem->field_24 = ((u32)Gp_LcgState >> 16) & 0xFFF;
        arg0->state   = 1;
    }

    y                 = coord->coord.t[1] + mem->field_12;
    coord->flg        = 0;
    coord->coord.t[1] = y;
    Gp_UpdateCoord(coord);
    if ((mem->field_22 & 3) == 0) {
        mem->field_20 = (u16)mem->field_20 + 1;
    }
    if (mem->field_20 < 8) {
        Gp_DrawFxQuad(coord, (u16)mem->field_20, 0x400, (u16)mem->field_24);
        return;
    }
    Gp_ReleaseState1CMem(mem, arg0);
}

INCLUDE_RODATA("pe/nonmatchings/energyshot/energyshot", D_energyshot_8012EF30);
