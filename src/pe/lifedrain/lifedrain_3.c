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
    flag  = Gp_State1C->fadeState;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    if (flag != 0) {
        if (flag >= 4) {
            Gp_ReleaseState1CMem(mem, arg0);
        }
        return;
    }

    if (arg0->state == 0) {
        Gfx_RotMatrixZ(&coord->coord, arg0->spawnArg1 & 0xFFF, 0);
        coord->flg  = 0;
        kind        = (Gp_StateC08.field_0 % 10U) - 1;
        mem->index  = kind;
        val         = D_lifedrain_80130AB4[kind].field_2;
        mem->angle  = 0x80;
        mem->scale  = val;
        mem->period = D_lifedrain_80130AB4[mem->index].field_4;
        arg0->state = 1;
    }

    Gp_UpdateCoord(coord);
    mem->angle  = (u16)mem->angle + ((s16)D_lifedrain_80130AB4[mem->index].field_2 / 3);
    mem->period = (u16)mem->period + ((s16)D_lifedrain_80130AB4[mem->index].field_2 >> 1);
    rgb[0]      = (u16)mem->scale >> 1;
    rgb[1]      = (u16)mem->scale >> 1;
    rgb[2]      = *(u8*)&mem->scale;
    Gp_DrawBandEx(coord, mem->angle, mem->period, rgb);

    scale      = (u16)mem->scale;
    scale     -= 8;
    mem->scale = scale;
    if ((s16)scale < 9) {
        Gp_ReleaseState1CMem(mem, arg0);
    }
}
