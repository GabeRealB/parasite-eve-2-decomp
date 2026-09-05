#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

void func_pyrokinesis_801312B4(GsCOORDINATE2* arg0, s16 arg1, s32 arg2, s16 arg3);

INCLUDE_RODATA("pe/nonmatchings/pyrokinesis/pyrokinesis", D_pyrokinesis_8012EF30);

INCLUDE_ASM("pe/nonmatchings/pyrokinesis/pyrokinesis", func_pyrokinesis_8012EF48);

INCLUDE_ASM("pe/nonmatchings/pyrokinesis/pyrokinesis", func_pyrokinesis_8012FAC8);

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

INCLUDE_ASM("pe/nonmatchings/pyrokinesis/pyrokinesis", func_pyrokinesis_80131CE4);
