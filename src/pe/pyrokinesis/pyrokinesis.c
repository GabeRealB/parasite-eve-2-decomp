#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

void func_pyrokinesis_8012FC34(GsCOORDINATE2* arg0, s16 arg1, s16 arg2);
void func_pyrokinesis_801312B4(GsCOORDINATE2* arg0, s16 arg1, s32 arg2, s16 arg3);

INCLUDE_RODATA("pe/nonmatchings/pyrokinesis/pyrokinesis", D_pyrokinesis_8012EF30);

INCLUDE_ASM("pe/nonmatchings/pyrokinesis/pyrokinesis", func_pyrokinesis_8012EF48);

void func_pyrokinesis_8012FAC8(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s16            scene;
    s16            flag;
    s32            state;

    mem   = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->field_8;
    if (Gp_StateC08.field_3 != -2) {
        scene = Gp_State1C->field_16;
        if (scene == 1) {
            flag = Gp_State1C->field_E;
            if (flag < 4) {
                if (flag != 0) {
                    return;
                }
                mem->field_22 = (u16)mem->field_22 + 1;
                Gp_UpdateCoord(coord);
                state = arg0->state;
                if (state == scene) {
                    goto L_case1;
                }
                if (state < 2) {
                    if (state == 0) {
                        goto L_case0;
                    }
                    return;
                }
                if (state == 2) {
                    goto L_case2;
                }
                if (state == 3) {
                    goto L_release;
                }
                return;
            L_case0:
                Gp_SpawnEff(0x80060010, coord, 0, 0);
                arg0->state = scene;
                return;
            L_case1:
                if (mem->field_22 == 8) {
                    Gp_SpawnEff(0x80060010, coord, 1, 0);
                    arg0->state = 2;
                }
                return;
            L_case2:
                if (mem->field_22 == 0x10) {
                    Gp_SpawnEff(0x80060000 | 0x10, coord, 2, 0);
                    arg0->state = 3;
                }
                return;
            }
        }
    }
L_release:
    Gp_ReleaseState1CMem(mem, arg0);
}

INCLUDE_ASM("pe/nonmatchings/pyrokinesis/pyrokinesis", func_pyrokinesis_8012FC34);

INCLUDE_ASM("pe/nonmatchings/pyrokinesis/pyrokinesis", func_pyrokinesis_80130130);

INCLUDE_ASM("pe/nonmatchings/pyrokinesis/pyrokinesis", func_pyrokinesis_801304C4);

INCLUDE_ASM("pe/nonmatchings/pyrokinesis/pyrokinesis", func_pyrokinesis_80130848);

INCLUDE_ASM("pe/nonmatchings/pyrokinesis/pyrokinesis", func_pyrokinesis_80130C54);

INCLUDE_ASM("pe/nonmatchings/pyrokinesis/pyrokinesis", func_pyrokinesis_80130DC0);

void func_pyrokinesis_801311B8(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s16            flag;
    s32            scale;
    s32            angle;

    mem   = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->field_8;
    if (Gp_StateC08.field_3 != -2) {
        flag = Gp_State1C->field_E;
        if (flag < 4) {
            if (flag != 0) {
                return;
            }
            if (arg0->state == 0) {
                Gfx_RotMatrixZ(&coord->coord, arg0->spawnArg1, 0);
                coord->flg    = 0;
                mem->field_24 = 0x80;
                mem->field_26 = 0x100;
                arg0->state   = 1;
            }
            Gp_UpdateCoord(coord);
            func_pyrokinesis_801312B4(coord, mem->field_26, 0x100, mem->field_24);
            angle         = (u16)mem->field_26;
            scale         = (u16)mem->field_24;
            angle        += 0x80;
            scale        -= 8;
            mem->field_24 = scale;
            mem->field_26 = angle;
            if ((s16)scale >= 9) {
                return;
            }
        }
    }
    Gp_ReleaseState1CMem(mem, arg0);
}

INCLUDE_ASM("pe/nonmatchings/pyrokinesis/pyrokinesis", func_pyrokinesis_801312B4);

INCLUDE_ASM("pe/nonmatchings/pyrokinesis/pyrokinesis", func_pyrokinesis_80131784);

void func_pyrokinesis_80131CE4(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s16            flag;
    s32            scale;
    s32            angle;

    mem   = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->field_8;
    if (Gp_StateC08.field_3 != -2) {
        flag = Gp_State1C->field_E;
        if (flag < 4) {
            if (flag != 0) {
                return;
            }
            mem->field_22 = (u16)mem->field_22 + 1;
            if (arg0->state == 0) {
                mem->field_24 = 0xC0;
                mem->field_26 = 0x100;
                arg0->state   = 1;
            }
            Gp_UpdateCoord(coord);
            func_pyrokinesis_8012FC34(((TmdObject*)arg0->extra)->field_8, mem->field_26, mem->field_24);
            angle         = (u16)mem->field_26;
            scale         = (u16)mem->field_24;
            angle        += 0x40;
            scale        -= 0x10;
            mem->field_24 = scale;
            mem->field_26 = angle;
            if ((s16)scale >= 0x10) {
                return;
            }
        }
    }
    Gp_ReleaseState1CMem(mem, arg0);
}
