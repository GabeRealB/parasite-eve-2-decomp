#include "common.h"

#include "gameplay/3A34.h"
#include "main/session.h"
#include "main/task.h"

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D5B14);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D6170);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D6334);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D68C4);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D6910);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D6994);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D6A24);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D6AA4);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D6B20);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D6E5C);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D70E4);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D72D0);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D759C);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D78A4);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D7A9C);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D8684);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D8C0C);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D8EA0);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D9138);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D930C);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D9340);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D937C);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D94B8);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D9504);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D9550);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D957C);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D9618);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D9654);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D96C8);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D9718);

s32 func_800D9788(GpObj38* arg0)
{
    return arg0->field_38;
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D9794);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D98C4);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D9A30);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D9B9C);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D9C3C);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D9C64);

void func_800D9CC8(Task* arg0)
{
    Task_CallExit(arg0);
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D9CE8);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D9D18);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800D9DFC);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DA2A0);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DA6E8);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DA7B8);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DAB38);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DABEC);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DAC54);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DACAC);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DACF8);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DAD54);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DAD78);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DADE4);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DAE50);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DAF98);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DAFD0);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DB004);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DB0D8);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DB128);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DB28C);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DB31C);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DB3FC);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DB4E0);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DB500);

void func_800DB530(s32 arg0)
{
    D_801153F3 = arg0;
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DB53C);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DB558);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DB630);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DB6B4);

void func_800DB72C(void)
{
    if (Game_GetPtrSlot(3) != NULL) {
        func_8010154C();
        func_800E0540(D_80115570);
        func_800E0540(D_80115574);
        func_800E0540(D_80115578);
        func_800E0540(D_8011557C);
        func_800E0540(D_80115580);
        func_800E0540(D_8011558C);
        func_800E0540(D_80115590);
        func_800E0414(D_80115570, D_80115578);
        func_800E0414(D_80115570, D_8011557C);
        func_800E0414(D_80115570, D_80115580);
        func_800E0414(D_80115570, D_80115590);
        func_800DB900(D_80115570);
        func_800E0414(D_80115574, D_80115578);
        func_800E0414(D_80115574, D_80115580);
        func_800E0414(D_80115574, D_80115588);
        func_800E0414(D_80115578, D_80115580);
        func_800E0414(D_80115578, D_80115590);
        func_800DB900(D_80115578);
        func_800E0414(D_8011557C, D_80115580);
        func_800E0414(D_80115580, D_80115590);
        if (D_80115424 != 0) {
            func_800E0B08();
        }
        func_800E0608(D_80115570, 0x9007, 0x9004);
        if (Game_Session->field_12C == 0) {
            func_800E06AC(D_80115570, 0xA007, 0xA004);
        }
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DB900);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DBA20);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DBCAC);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DBE7C);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DC528);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DCB80);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DD324);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DD940);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DDC2C);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DDDF8);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DE150);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DE2C0);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DE7CC);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DEAFC);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DEC80);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DEF80);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DF6AC);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800DFCCC);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E0294);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E0308);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E0414);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E0540);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E0608);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E06AC);

s32 func_800E076C(void)
{
    return 0;
}

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E0774);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E08CC);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E0994);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E0B08);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E0B48);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E0C10);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E0FEC);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E1380);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E15AC);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E1638);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E1688);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E1708);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E1758);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E17B4);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E1834);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E1884);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E18E0);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E192C);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E19B8);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E1A1C);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E1A6C);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E1ACC);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E1B24);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E1B80);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E1BF0);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E1C58);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E1CD4);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E1FEC);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E2438);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E25F8);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E2A24);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E2BF8);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E2C40);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E2C78);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E2CD4);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E2D3C);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E2D90);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E2DE4);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E2EC4);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E2F7C);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E3008);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E301C);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E3084);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E3194);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E31E8);

INCLUDE_ASM("gameplay/nonmatchings/3A34", func_800E337C);
