#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/session.h"
#include "main/task.h"
#include "weapons/m4a1_hammer.h"
#include "main/tmd.h"

extern u32 Gp_LcgState;

INCLUDE_ASM("weapons/nonmatchings/m4a1_hammer/m4a1_hammer", func_m4a1_hammer_8011D1E0);

INCLUDE_ASM("weapons/nonmatchings/m4a1_hammer/m4a1_hammer", func_m4a1_hammer_8011D904);

void func_m4a1_hammer_8011DD08(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* parent;

    mem   = arg0->spawnArg2;
    coord = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    mem->field_22++;
    switch (arg0->state) {
        case 0:
            Task_Reparent(D_m4a1_hammer_8012D660, arg0);
            if (arg0->spawnArg1 != 0) {
                parent            = mem->field_8;
                coord->coord.t[0] = 0;
                coord->coord.t[1] = 0;
                coord->coord.t[2] = 0;
                coord->flg        = 0;
                coord->sub        = parent;
                Gp_UpdateCoord(coord);
                arg0->state = 1;
            }
            mem->field_24 = 0x80;
            Gp_LcgState   = (Gp_LcgState * 5) + 0x71357911;
            mem->field_26 = (Gp_LcgState >> 16) & 0xFFF;
            /* fallthrough */
        case 1:
            if (mem->field_22 & 1) {
                func_m4a1_hammer_8011DE60(coord, ++mem->field_20, 0x400, mem->field_26);
                if (mem->field_22 < 8) {
                    func_m4a1_hammer_8011E29C(coord, &D_m4a1_hammer_8012D668, mem->field_20, 0x280);
                }
            }
            if (mem->field_22 >= 0x19) {
                Gp_ReleaseState1CMem(mem, arg0);
            }
            break;
    }
}

INCLUDE_ASM("weapons/nonmatchings/m4a1_hammer/m4a1_hammer", func_m4a1_hammer_8011DE60);

INCLUDE_ASM("weapons/nonmatchings/m4a1_hammer/m4a1_hammer", func_m4a1_hammer_8011E29C);

INCLUDE_RODATA("weapons/nonmatchings/m4a1_hammer/m4a1_hammer", D_m4a1_hammer_8011D1C0);

INCLUDE_ASM("weapons/nonmatchings/m4a1_hammer/m4a1_hammer", func_m4a1_hammer_8011E710);
