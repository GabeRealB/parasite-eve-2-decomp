#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

INCLUDE_ASM("pe/nonmatchings/energyball/energyball", func_energyball_8012EF48);

INCLUDE_RODATA("pe/nonmatchings/energyball/energyball", D_energyball_8012EF30);

INCLUDE_ASM("pe/nonmatchings/energyball/energyball", func_energyball_8012F180);

INCLUDE_ASM("pe/nonmatchings/energyball/energyball", func_energyball_8012FFD0);

INCLUDE_ASM("pe/nonmatchings/energyball/energyball", func_energyball_8013035C);

INCLUDE_ASM("pe/nonmatchings/energyball/energyball", func_energyball_801307D4);

INCLUDE_ASM("pe/nonmatchings/energyball/energyball", func_energyball_80130B54);

void func_energyball_8013107C(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s16            flag;
    u8             rgb[3];
    s32            scale;
    s32            angle;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->field_E;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    if (flag != 0) {
        return;
    }

    if (arg0->state == 0) {
        Gfx_RotMatrixZ(&coord->coord, arg0->spawnArg1 & 0xFFF, 0);
        coord->flg    = 0;
        mem->field_24 = 0x80;
        mem->field_26 = 0x100;
        arg0->state   = 1;
    }

    Gp_UpdateCoord(coord);
    rgb[0] = (u16)mem->field_24 >> 1;
    rgb[1] = *(u8*)&mem->field_24;
    rgb[2] = (u16)mem->field_24 >> 1;
    Gp_DrawBandEx(coord, mem->field_26, 0x180, rgb);

    angle         = (u16)mem->field_26;
    scale         = (u16)mem->field_24;
    angle        += 0x80;
    scale        -= 8;
    mem->field_24 = scale;
    mem->field_26 = angle;
    if ((s16)scale < 9) {
        Gp_ReleaseState1CMem(mem, arg0);
    }
}
