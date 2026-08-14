#include "common.h"

#include "gameplay/gameplay.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

extern u8  D_801153F1;
extern s32 D_8010CA28;

void func_800A1634(s32 arg0, s32 arg1);
void func_800A4A2C(s32 arg0, s32 arg1, s32 arg2, s32 arg3);
s32  func_800A7B20(s32 arg0);
void func_800B065C(u8 arg0);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_80097AC0);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009850C);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_80098F58);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_80098F98);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009902C);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_80099098);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_80099170);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800991DC);

void func_80099214(TmdObject* arg0)
{
    if (arg0->field_18 != NULL) {
        Mem_Free2(arg0->field_18, 1);
        arg0->field_18 = NULL;
    }
    Mem_Free(arg0);
}

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_80099258);

void func_80099290(void* arg0)
{
    Mem_Free(arg0);
}

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800992B0);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_80099338);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009939C);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009988C);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_80099958);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_80099994);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_80099B94);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_80099D40);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_80099FF4);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009A348);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009A57C);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009A804);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009AA5C);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009AC58);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009AF90);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009B2F4);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009B500);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009BD00);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009C024);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009C414);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009CED0);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009D0DC);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009D388);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009D518);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009D718);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009D900);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009DB00);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009DCB8);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009DE48);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009E048);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009E274);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009E4A0);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009E770);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009EA50);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009EAA4);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009EB84);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009EC1C);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009ECC0);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009ED28);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009ED90);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009EE28);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009EECC);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009EF64);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009EFFC);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009F0A0);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009F144);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009F1DC);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009F280);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009F360);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009F3F8);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009F49C);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009F504);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009F56C);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009F670);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009F708);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009F824);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009F8C8);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009F970);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009FA24);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009FB28);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009FC44);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009FC90);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009FCDC);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009FD28);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009FD74);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_8009FEDC);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A0094);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A0504);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A0718);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A087C);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A110C);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A1558);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A1634);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A18BC);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A1CD0);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A1F64);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A2104);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A2BE0);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A2F60);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A3AF0);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A45F0);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A4904);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A4A2C);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A5274);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A5574);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A57B0);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A63B4);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A6480);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A6A9C);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A6F38);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A70A4);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A7320);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A746C);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A74C4);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A7508);

s32 func_800A7550(void)
{
    func_800A1634(1, 0);
    return 0;
}

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A7574);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A7600);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A76A4);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A7744);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A77B4);

void func_800A7824(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg0 == 0) {
        func_800A4A2C(0, arg1, arg2, 5);
    }
}

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A784C);

s32 func_800A78DC(void)
{
    return D_801153F1 == 0;
}

void func_800A78EC(void)
{
    func_800B065C(func_800A7B20(7) + 0x15);
}

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A7918);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A79F8);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A7A64);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A7AE4);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A7B20);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A7BBC);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A7CB0);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A7CF4);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A7D54);

void func_800A7DB8(s32 arg0)
{
    if (!(D_80114C08.field_6 & 1)) {
        D_80114C08.field_E = arg0;
    }
}

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A7DE0);

void func_800A7E4C(void)
{
    D_8010CA28 = 5;
}

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A7E5C);

void func_800A7F24(void)
{
}

s32 func_800A7F2C(s32 arg0)
{
    return arg0 - 0x10;
}

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A7F34);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A7F6C);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A82C0);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A8654);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A8724);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A8864);

s32 func_800A8A1C(s32 arg0)
{
    return Task_Spawn(0, 0xF, 0, arg0) != NULL;
}

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A8A48);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A8B14);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A8B6C);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A8C08);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A8C74);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A8D5C);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A8DC0);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A8E8C);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A9010);

INCLUDE_ASM("gameplay/nonmatchings/gameplay", func_800A91CC);
