#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"
#include "pe/lifedrain.h"

void func_lifedrain_801308C0(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s16            flag;
    s16            kind;
    u16            val;
    u8             rgb[3];
    s32            scale;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->field_E;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    if (flag != 0) {
        if (flag >= 4) {
            Gp_ReleaseState1CMem(mem, arg0);
        }
        return;
    }

    if (arg0->state == 0) {
        Gfx_RotMatrixZ(&coord->coord, arg0->spawnArg1 & 0xFFF, 0);
        coord->flg    = 0;
        kind          = (Gp_StateC08.field_0 % 10U) - 1;
        mem->field_20 = kind;
        val           = D_lifedrain_80130AB4[kind].field_2;
        mem->field_26 = 0x80;
        mem->field_24 = val;
        mem->field_28 = D_lifedrain_80130AB4[mem->field_20].field_4;
        arg0->state   = 1;
    }

    Gp_UpdateCoord(coord);
    mem->field_26 = (u16)mem->field_26 + ((s16)D_lifedrain_80130AB4[mem->field_20].field_2 / 3);
    mem->field_28 = (u16)mem->field_28 + ((s16)D_lifedrain_80130AB4[mem->field_20].field_2 >> 1);
    rgb[0]        = (u16)mem->field_24 >> 1;
    rgb[1]        = (u16)mem->field_24 >> 1;
    rgb[2]        = *(u8*)&mem->field_24;
    Gp_DrawBandEx(coord, mem->field_26, mem->field_28, rgb);

    scale         = (u16)mem->field_24;
    scale        -= 8;
    mem->field_24 = scale;
    if ((s16)scale < 9) {
        Gp_ReleaseState1CMem(mem, arg0);
    }
}
