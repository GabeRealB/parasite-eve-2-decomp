#include "common.h"

#include <psyq/libmcrd.h>

#include "main/game.h"
#include "main/unknown_syms.h"

INCLUDE_ASM("main/nonmatchings/21FDC", func_800317DC);

INCLUDE_ASM("main/nonmatchings/21FDC", func_800319E4);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80031B1C);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80031C5C);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80031DA4);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80031F94);

INCLUDE_ASM("main/nonmatchings/21FDC", func_800322B0);

INCLUDE_ASM("main/nonmatchings/21FDC", func_8003245C);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80032578);

INCLUDE_ASM("main/nonmatchings/21FDC", func_800327A4);

INCLUDE_ASM("main/nonmatchings/21FDC", func_800328FC);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80032AB0);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80032D54);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80032F5C);

INCLUDE_ASM("main/nonmatchings/21FDC", func_800330D8);

INCLUDE_ASM("main/nonmatchings/21FDC", func_8003380C);

INCLUDE_ASM("main/nonmatchings/21FDC", func_800338A8);

INCLUDE_ASM("main/nonmatchings/21FDC", func_800338F4);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80033944);

INCLUDE_ASM("main/nonmatchings/21FDC", func_800339C4);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80033A28);

void func_80033A70(void)
{
    GStruct23* p;

    p = &D_80072168;
    p->field_21  = 0;
    p->field_1a8 = 0;
    p->field_1aa = 0;
    p->field_1ab = 0;
    p->field_1a9 = 0;
    p->field_25  = 0;
    func_800260B0(1);
    func_800429C8(0);
}

INCLUDE_ASM("main/nonmatchings/21FDC", func_80033AB8);

// TODO
void func_80033BBC(void)
{
    MemCardInit(0); // 0 = No control routine
    MemCardStart();
    func_800303AC();
}

INCLUDE_ASM("main/nonmatchings/21FDC", func_80033BEC);

void func_80033C38(void)
{
}

INCLUDE_ASM("main/nonmatchings/21FDC", func_80033C40);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80033CC0);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80033D3C);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80033D88);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80033DD4);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80033E58);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80033EB0);

void func_80033F6C(GStruct0* arg0)
{
    GStruct0*   child;
    GStruct37*  obj;
    GStruct37*  flag;

    child = arg0->field_c;
    if (child != NULL) {
        obj  = child->field_20;
        flag = arg0->field_20;
        obj->field_0 = 0;
        func_80048838(obj, obj->field_28);
        flag->field_0 = 1;
    }
}

INCLUDE_ASM("main/nonmatchings/21FDC", func_80033FB8);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80034028);

void func_80034070(GStruct0* arg0, GStruct21* arg1)
{
    arg1->field_0   = 0x10;
    arg1->field_4   = 0;
    arg1->field_18  = 0;
    arg1->field_C   = 0;
    arg1->field_A18 = 0x34;
    arg1->field_A20 = 0;
    arg0->field_30++;
}

INCLUDE_ASM("main/nonmatchings/21FDC", func_800340A4);

INCLUDE_ASM("main/nonmatchings/21FDC", func_8003415C);

INCLUDE_ASM("main/nonmatchings/21FDC", func_8003429C);

INCLUDE_ASM("main/nonmatchings/21FDC", func_800343D0);

INCLUDE_ASM("main/nonmatchings/21FDC", func_800344B4);

INCLUDE_ASM("main/nonmatchings/21FDC", func_800345CC);

INCLUDE_ASM("main/nonmatchings/21FDC", func_800346AC);

INCLUDE_ASM("main/nonmatchings/21FDC", func_8003477C);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80034894);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80034938);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80034A40);

void func_80034B38(GStruct0* arg0)
{
    if (arg0->field_2a != 0) {
        func_8002CCB8(arg0);
    }
}

INCLUDE_ASM("main/nonmatchings/21FDC", func_80034B68);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80034C54);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80034D50);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80034E3C);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80034F2C);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80034FB4);

INCLUDE_ASM("main/nonmatchings/21FDC", func_800350B0);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80035180);

INCLUDE_ASM("main/nonmatchings/21FDC", func_8003527C);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80035358);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80035464);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80035574);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80035684);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80035764);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80035844);

void func_8003591C(GStruct0* arg0, GStruct21* arg1)
{
    arg1->field_8 = 0xE;
    arg1->field_4 = 0;
    if (func_800304AC(arg0, 0xE, 0) != 0) {
        arg0->field_30 = 0x13;
    }
}

void func_80035960(GStruct0* arg0, GStruct21* arg1)
{
    arg1->field_8 = 0xD;
    arg1->field_4 = 0;
    if (func_800304AC(arg0, 0xD, 0) != 0) {
        arg0->field_30 = 0x13;
    }
}

INCLUDE_ASM("main/nonmatchings/21FDC", func_800359A4);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80035A94);

void func_80035AD4(GStruct0* arg0, GStruct21* arg1)
{
    arg1->field_24 = 9;
    arg1->field_28 = -1;
    arg0->field_30 = 7;
}

INCLUDE_ASM("main/nonmatchings/21FDC", func_80035AF0);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80035C2C);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80035D14);

void func_80035E18(GStruct0* arg0)
{
    if (arg0->field_2a != 0) {
        func_8002CCB8(arg0);
    }
}

INCLUDE_ASM("main/nonmatchings/21FDC", func_80035E48);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80035ED4);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80035FD8);

INCLUDE_ASM("main/nonmatchings/21FDC", func_800360C8);

INCLUDE_ASM("main/nonmatchings/21FDC", func_800361C0);

INCLUDE_ASM("main/nonmatchings/21FDC", func_800362A4);

INCLUDE_ASM("main/nonmatchings/21FDC", func_800363AC);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80036488);

INCLUDE_ASM("main/nonmatchings/21FDC", func_800365B0);

INCLUDE_ASM("main/nonmatchings/21FDC", func_800366BC);

INCLUDE_ASM("main/nonmatchings/21FDC", func_800367CC);

INCLUDE_ASM("main/nonmatchings/21FDC", func_800368DC);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80036968);

void func_80036A1C(void)
{
    char pad[0x10];
}

void func_80036A2C(void* arg0, GStruct20* arg1)
{
    func_80048C10(arg0, arg1);
    if (arg1->field_0 == 1) {
        func_80048D58(arg1, arg1->field_1c + 2, 0);
    }
}

INCLUDE_ASM("main/nonmatchings/21FDC", func_80036A70);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80036B2C);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80036C04);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80036CF0);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80036D98);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80036E78);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80036F18);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80036FB8);

INCLUDE_ASM("main/nonmatchings/21FDC", func_80037068);
