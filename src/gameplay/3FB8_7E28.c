#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/session.h"
#include "main/task.h"

#include <psyq/inline_c.h>
#include <psyq/libgs.h>
#include <psyq/libgte.h>

extern s32 D_80070F60;

INCLUDE_ASM("gameplay/nonmatchings/3FB8_7E28", func_800F77F8);

INCLUDE_ASM("gameplay/nonmatchings/3FB8_7E28", func_800F7AD4);

void func_800F7E28(Task* arg0)
{
    GpEffWork*     mem;
    GameActorExt*  extra;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* parent;
    MATRIX*        m;
    MATRIX*        world;
    s16            flag;
    s32            one;

    extra  = arg0->extra;
    mem    = arg0->spawnArg2;
    flag   = D_80115740->field_4;
    coord  = (GsCOORDINATE2*)extra->field_8;
    parent = mem->field_8;
    if (flag >= 2) {
        func_800EC7E4(mem, arg0);
        return;
    }

    func_80098F58(parent);
    coord->workm = parent->workm;
    gte_SetRotMatrix(&parent->workm);
    gte_SetTransMatrix(&parent->workm);
    world = &D_80070F34;
    func_800A8864(world, &coord->workm, &coord->coord);
    coord->flg = 0;
    func_80098F58(coord);

    switch (arg0->spawnArg1) {
        case 0:
            mem->field_24   = 0x280;
            mem->field_2A   = 1;
            mem->field_26   = 0;
            arg0->spawnArg1 = 1;
            if (D_80115740->field_4 != 0) {
                break;
            }
            func_800EA478(0x60042, coord, mem->field_24 + 0x22200 + mem->field_24, 0);
            break;
        case 1:
            func_800F77F8(arg0);
            if (D_80115740->field_4 != 0) {
                break;
            }
            D_80070F60 = D_80070F60 * 5 + 0x71357911;
            if ((((u32)D_80070F60 >> 16) & 3) == 0) {
                func_800EA478(0x60042, coord, mem->field_24 + 0x21000, 0);
            }
            mem->field_22++;
            break;
        case 2:
            if (D_80115740->field_4 == 0) {
                if (mem->field_20 == 0) {
                    func_800EA478(0x60042, coord, mem->field_24 + 0x22200, 0);
                    mem->field_20        = 1;
                    mem->field_22        = 0;
                    mem->field_24      >>= 2;
                    one                  = ONE;
                    *(s32*)&coord->coord = one;
                    m                    = &coord->coord;
                    *(s32*)&m->m[0][2]   = 0;
                    *(s32*)&m->m[1][1]   = one;
                    *(s32*)&m->m[2][0]   = 0;
                    m->m[2][2]           = one;
                }
                mem->field_22 += (u16)Display_State.field_8 & 1;
            }
            if (mem->field_22 < 0x10) {
                func_800FA45C(coord, mem->field_24, mem->field_22 >> 1, mem->field_2A);
            } else {
                arg0->spawnArg1 = 4;
                break;
            }
            goto lcg;
        case 3:
            if (D_80115740->field_4 == 0 && mem->field_20 == 0) {
                func_800EA478(0x60042, coord, mem->field_24 + 0x22200, 0);
                mem->field_20   = 1;
                mem->field_22   = 0;
                mem->field_24 >>= 2;
            }
            mem->field_22++;
            if (mem->field_22 >= 0x10) {
                arg0->spawnArg1 = 4;
                break;
            }
        lcg:
            if (D_80115740->field_4 != 0) {
                break;
            }
            D_80070F60 = D_80070F60 * 5 + 0x71357911;
            if ((u16)(((u32)D_80070F60 >> 16) % 3U) == 0) {
                func_800EA478(0x6003F, coord, mem->field_24, 0);
            }
            break;
        case 4:
            func_800EC7E4(mem, arg0);
            break;
    }
}

static const s32 s_jtbl_pad = 0;
