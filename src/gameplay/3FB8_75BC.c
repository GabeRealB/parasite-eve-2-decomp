#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/session.h"
#include "main/task.h"

#include <psyq/libgs.h>
#include <psyq/libgte.h>

extern s32 D_80070F60;

void func_800F75BC(Task* arg0)
{
    GpEffWork*     mem;
    GameActorExt*  extra;
    GsCOORDINATE2* coord;
    s16            flag;
    s32            param;

    mem   = arg0->spawnArg2;
    extra = arg0->extra;
    flag  = D_80115740->field_4;
    coord = (GsCOORDINATE2*)extra->field_8;
    if (flag != 0) {
        if (flag >= 4) {
            func_800EC7E4(mem, arg0);
        }
        func_800F7AD4(coord, mem->field_24, mem->field_28, mem->field_2A);
        return;
    }

    func_80098F58(coord);
    switch (arg0->state) {
        case 0:
            D_80070F60 = D_80070F60 * 5 + 0x71357911;
            Gfx_RotMatrixY(&coord->coord, ((u32)D_80070F60 >> 16) & 0xFFF, 1);
            coord->flg    = 0;
            mem->field_26 = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_0 & 0xFFF;
            param         = ((GpEffSpawnArg*)&arg0->spawnArg1)->field_2;
            mem->field_2A = param & 0xF;
            if (arg0->spawnArg1 & 0x20000000) {
                mem->field_28 = 0x80;
                mem->field_24 = mem->field_26;
                arg0->state   = 4;
            } else if (arg0->spawnArg1 & 0x10000000) {
                mem->field_28 = 0x40;
                mem->field_24 = mem->field_26;
                arg0->state   = 2;
            } else {
                mem->field_28 = 0x80;
                mem->field_24 = 0;
                arg0->state   = 1;
            }
            break;
        case 1:
            func_800F7AD4(coord, mem->field_24, mem->field_28, mem->field_2A);
            if (mem->field_24 < mem->field_26) {
                mem->field_24 += 6;
            } else {
                mem->field_24 = mem->field_26;
                arg0->state   = 2;
            }
            break;
        case 2:
            func_800F7AD4(coord, mem->field_24, mem->field_28, mem->field_2A);
            if (mem->field_28 >= 0x41) {
                mem->field_28--;
            }
            break;
        case 3:
            func_800F7AD4(coord, mem->field_24, mem->field_28, mem->field_2A);
            mem->field_28 -= 4;
            if (mem->field_28 < 4) {
                func_800EC7E4(mem, arg0);
            }
            break;
        case 4:
            func_800F7AD4(coord, mem->field_24, mem->field_28, mem->field_2A);
            break;
    }
}

static const s32 s_jtbl_pad = 0;
