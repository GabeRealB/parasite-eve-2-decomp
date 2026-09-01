#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "main/session.h"
#include "main/task.h"
#include "weapons/m4a1_javelin.h"

extern u32 Gp_LcgState;

void func_m4a1_javelin_8011F0AC(M4a1JavelinVecLo* arg0, s16 arg1, s16 arg2, s16 arg3);

INCLUDE_ASM("weapons/nonmatchings/m4a1_javelin/m4a1_javelin", func_m4a1_javelin_8011D1E4);

INCLUDE_ASM("weapons/nonmatchings/m4a1_javelin/m4a1_javelin", func_m4a1_javelin_8011DAB0);

INCLUDE_ASM("weapons/nonmatchings/m4a1_javelin/m4a1_javelin", func_m4a1_javelin_8011E4A8);

INCLUDE_ASM("weapons/nonmatchings/m4a1_javelin/m4a1_javelin", func_m4a1_javelin_8011EE78);

INCLUDE_ASM("weapons/nonmatchings/m4a1_javelin/m4a1_javelin", func_m4a1_javelin_8011F0AC);

void func_m4a1_javelin_8011F4A4(M4a1JavelinVecLo* arg0)
{
    if (arg0 == NULL) {
        D_m4a1_javelin_8012EB70 = 0;
        return;
    }
    D_m4a1_javelin_8012EB68.vx = arg0->vx;
    D_m4a1_javelin_8012EB68.vy = arg0->vy;
    D_m4a1_javelin_8012EB70    = 1;
    D_m4a1_javelin_8012EB68.vz = arg0->vz;
}

void func_m4a1_javelin_8011F4E8(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    s16            flag;

    mem   = arg0->spawnArg2;
    flag  = Gp_State1C->field_4;
    coord = (GsCOORDINATE2*)((GameActorExt*)arg0->extra)->field_8;
    if (flag != 0) {
        if (flag < 4) {
            return;
        }
        Gp_ReleaseState1CMem(mem, arg0);
        return;
    }

    mem->field_22++;
    if (arg0->state == 0) {
        mem->field_24 = 0x200;
        Gp_LcgState   = (Gp_LcgState * 5) + 0x71357911;
        mem->field_26 = (Gp_LcgState >> 16) & 0xFFF;
        arg0->state   = 1;
    }
    func_m4a1_javelin_8011F0AC((M4a1JavelinVecLo*)&coord->workm.t, mem->field_22 - 1, mem->field_24, mem->field_26);
    if (mem->field_22 == 8) {
        Gp_ReleaseState1CMem(mem, arg0);
    }
}

INCLUDE_RODATA("weapons/nonmatchings/m4a1_javelin/m4a1_javelin", D_m4a1_javelin_8011D1C0);

INCLUDE_ASM("weapons/nonmatchings/m4a1_javelin/m4a1_javelin", func_m4a1_javelin_8011F5D4);
