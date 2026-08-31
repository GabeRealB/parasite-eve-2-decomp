#include "common.h"

#include "weapons/m4a1_javelin.h"

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

INCLUDE_ASM("weapons/nonmatchings/m4a1_javelin/m4a1_javelin", func_m4a1_javelin_8011F4E8);

INCLUDE_RODATA("weapons/nonmatchings/m4a1_javelin/m4a1_javelin", D_m4a1_javelin_8011D1C0);

INCLUDE_ASM("weapons/nonmatchings/m4a1_javelin/m4a1_javelin", func_m4a1_javelin_8011F5D4);
