#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/session.h"
#include "main/task.h"

#include <psyq/libgs.h>
#include <psyq/libgte.h>

extern s32 Gp_LcgState;

void Gp_EffSprTask46(Task* arg0)
{
    GpEffWork*     mem;
    TmdObject*     extra;
    GsCOORDINATE2* coord;
    s16            flag;
    s32            param;

    mem   = arg0->spawnArg2;
    extra = arg0->extra;
    flag  = Gp_State1C->eventState;
    coord = (GsCOORDINATE2*)extra->coords;
    if (flag != 0) {
        if (flag >= 4) {
            Gp_ReleaseState1CMem(mem, arg0);
        }
        Gp_DrawEffSprite46(coord, mem->scale, mem->period, mem->step);
        return;
    }

    Gp_UpdateCoord(coord);
    switch (arg0->state) {
        case 0:
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            Gfx_RotMatrixY(&coord->coord, ((u32)Gp_LcgState >> 16) & 0xFFF, 1);
            coord->flg = 0;
            mem->angle = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_0 & 0xFFF;
            param      = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_2;
            mem->step  = param & 0xF;
            if (arg0->spawnArg1 & 0x20000000) {
                mem->period = 0x80;
                mem->scale  = mem->angle;
                arg0->state = 4;
            } else if (arg0->spawnArg1 & 0x10000000) {
                mem->period = 0x40;
                mem->scale  = mem->angle;
                arg0->state = 2;
            } else {
                mem->period = 0x80;
                mem->scale  = 0;
                arg0->state = 1;
            }
            break;
        case 1:
            Gp_DrawEffSprite46(coord, mem->scale, mem->period, mem->step);
            if (mem->scale < mem->angle) {
                mem->scale += 6;
            } else {
                mem->scale  = mem->angle;
                arg0->state = 2;
            }
            break;
        case 2:
            Gp_DrawEffSprite46(coord, mem->scale, mem->period, mem->step);
            if (mem->period >= 0x41) {
                mem->period--;
            }
            break;
        case 3:
            Gp_DrawEffSprite46(coord, mem->scale, mem->period, mem->step);
            mem->period -= 4;
            if (mem->period < 4) {
                Gp_ReleaseState1CMem(mem, arg0);
            }
            break;
        case 4:
            Gp_DrawEffSprite46(coord, mem->scale, mem->period, mem->step);
            break;
    }
}

static const s32 s_jtbl_pad = 0;
