#include "common.h"
#include "main/task.h"
extern s32 D_80115720;
extern s32 D_80115728;
extern s32 D_8011572C;
extern s32 D_8011573C;
extern s32 D_80115744;
extern s32 D_80115750;
extern s32 D_80115758;

INCLUDE_ASM("rooms/nonmatchings/dryfield_motel_balcony/dryfield_motel_balcony", func_dryfield_motel_balcony_8017D5E8);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_motel_balcony/dryfield_motel_balcony", D_dryfield_motel_balcony_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_motel_balcony/dryfield_motel_balcony", func_dryfield_motel_balcony_8017D74C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_motel_balcony/dryfield_motel_balcony", func_dryfield_motel_balcony_8017D8BC);

INCLUDE_ASM("rooms/nonmatchings/dryfield_motel_balcony/dryfield_motel_balcony", func_dryfield_motel_balcony_8017DB1C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_motel_balcony/dryfield_motel_balcony", func_dryfield_motel_balcony_8017DB6C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_motel_balcony/dryfield_motel_balcony", func_dryfield_motel_balcony_8017DB74);

INCLUDE_ASM("rooms/nonmatchings/dryfield_motel_balcony/dryfield_motel_balcony", func_dryfield_motel_balcony_8017DB7C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_motel_balcony/dryfield_motel_balcony", func_dryfield_motel_balcony_8017DB84);

void func_dryfield_motel_balcony_8017DBC8(void)
{
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_motel_balcony/dryfield_motel_balcony", func_dryfield_motel_balcony_8017DBD0);

void func_dryfield_motel_balcony_8017DC28(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115728  = 0x60282;
        D_80115744  = 0x60283;
        D_8011573C  = 0x60284;
        D_80115720  = 0x60285;
        D_80115758  = 0x60005;
        D_8011572C  = 0x60073;
        D_80115750  = 0x60074;
        arg0->state = 1;
    }
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_motel_balcony/dryfield_motel_balcony", func_dryfield_motel_balcony_8017DCB8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_motel_balcony/dryfield_motel_balcony", func_dryfield_motel_balcony_8017DF84);
