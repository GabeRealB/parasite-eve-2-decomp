#include "common.h"
#include "weapons/gunblade.h"

INCLUDE_ASM("weapons/nonmatchings/gunblade/gunblade", func_gunblade_8011D1E4);

INCLUDE_ASM("weapons/nonmatchings/gunblade/gunblade", func_gunblade_8011D70C);

INCLUDE_ASM("weapons/nonmatchings/gunblade/gunblade", func_gunblade_8011DAA4);

void func_gunblade_8011E008(s32 arg0)
{
    GunbladeWork* work = D_gunblade_8012E248;

    if (work != NULL) {
        D_gunblade_8012E244->field_34 = arg0;
        work->field_20++;
    }
}

INCLUDE_RODATA("weapons/nonmatchings/gunblade/gunblade", D_gunblade_8011D1C0);

INCLUDE_ASM("weapons/nonmatchings/gunblade/gunblade", func_gunblade_8011E040);
