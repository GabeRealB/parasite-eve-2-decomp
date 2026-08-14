#include "common.h"

#include "main/fs.h"
#include "main/session.h"
#include "main/task.h"
#include "main/ui.h"

extern s32  D_8010E8F8;
extern s32  D_80114D88;
extern u32  D_80114DCC;
extern u8*  D_80114DD4;
extern s32  D_80114E88;
extern char D_8010F8D0[];

void  func_8017F41C(Task* task);
void  func_8017F2F8(Task* task);
void  func_8017F304(Task* task);
void  func_80181184(Task* task);
void  func_801811A0(Task* task);
void  func_800C05CC(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
void  func_800CB33C(UiObject* arg0, Task* arg1, s32 arg2);
void  func_800CC15C(UiObject* arg0, Task* arg1, s32 arg2);
void  func_800D02A4(Task* arg0);
void  func_800D0C34(Task* arg0);
void  func_800D0614(Task* arg0);
void  func_800D08D4(Task* arg0);
void  func_800D15D0(Task* arg0);
void  func_8003F9F4(void);
void  func_8003F6F8(void);
char* func_800B8EB0(s32 arg0, s32 arg1, s32 arg2);
void  func_80049D34(char* arg0, s32 arg1, s32 arg2);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800BF9FC);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C010C);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C02A0);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C05CC);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C0B98);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C0CA0);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C0E20);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C1148);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C16B4);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C1960);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C1D18);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C2140);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C22D8);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C2538);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C26B8);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C2B70);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C2CE8);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C32A8);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C3418);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C388C);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C3CE0);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C41A4);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C46B4);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C5188);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C5328);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C58B8);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C5A5C);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C5C2C);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C5F70);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C70F0);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C7444);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C7590);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C7844);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C7AE8);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C7DA8);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C8368);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C8700);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C8B40);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C8E10);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C9010);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C942C);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C9654);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C9A10);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C9BE8);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800C9E94);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CA25C);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CA634);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CA838);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CAB40);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CAC88);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CADFC);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CB188);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CB33C);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CB6FC);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CC15C);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CC41C);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CC4F4);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CC6C4);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CCA48);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CCC28);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CCDC8);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CCEEC);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CD160);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CD39C);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CD508);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CD704);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CD78C);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CD814);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CD89C);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CD924);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CDA64);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CDBEC);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CDCAC);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CDDA0);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CDE80);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CDEF4);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CDF18);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CDFA8);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CE094);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CE188);

void func_800CE1E0(Task* arg0)
{
    arg0->killCountdown--;
    if (arg0->killCountdown <= 0) {
        Task_Kill(arg0);
        func_8003F9F4();
        func_8003F6F8();
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CE22C);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CE294);

void func_800CE398(s32 arg0)
{
    D_80114D88 = arg0;
}

s32 func_800CE3A4(void)
{
    return D_80114D88;
}

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CE3B4);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CE498);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CE4F4);

void func_800CE5D0(UiObject* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    func_800C05CC(arg0, arg1, arg2, arg3, 0);
}

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CE5F0);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CE738);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CE894);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CE980);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CEA00);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CEA88);

void func_800CEB40(s32 arg0)
{
    if (arg0 == 0) {
        func_80049D34(D_8010F8D0, 0, 0);
    } else {
        func_80049D34(func_800B8EB0(arg0, 1, 0), 0, 0);
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CEB84);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CEC5C);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CECC0);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CEDA0);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CEE5C);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CEF68);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CF090);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CF148);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CF204);

s32 func_800CF27C(void)
{
    return D_8010E8F8;
}

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CF28C);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CF330);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CF374);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CF448);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CF4EC);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CF658);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CF6E8);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CF7C4);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CF88C);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CF940);

void func_800CFA34(UiObject* arg0, Task* arg1)
{
    func_800CB33C(arg0, arg1, *D_80114DD4);
}

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CFA60);

void func_800CFAA8(UiObject* arg0, Task* arg1)
{
    func_800CC15C(arg0, arg1, arg1->spawnArg1);
}

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CFACC);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CFB64);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CFBFC);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CFCD4);

void func_800CFD78(Task* arg0)
{
    if (arg0->state == 0) {
        D_80114DCC = *(u32*)&Game_Session->field_4 & 0xFFFF0000;
    }
    switch (D_80114DCC) {
        case 0x1010000:
            func_8017F41C(arg0);
            break;
        case 0x1020000:
            func_8017F2F8(arg0);
            break;
        case 0x1110000:
            func_8017F304(arg0);
            break;
        case 0x21E0000:
            func_80181184(arg0);
            break;
        case 0x31E0000:
            func_801811A0(arg0);
            break;
        default:
            Task_Kill(arg0);
            break;
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CFE68);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800CFF04);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D02A4);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D0614);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D08D4);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D0C34);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D0F3C);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D131C);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D1434);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D15D0);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D1A20);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D1BAC);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D1CF8);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D1D5C);

void func_800D1E28(Task* arg0)
{
    UiObject* obj;

    obj = arg0->spawnArg2;
    if (CdCmd_IsIdle() & 0xFFFF) {
        obj->timer = 1;
        func_800D02A4(arg0);
        func_800D0C34(arg0);
        func_800D0614(arg0);
        func_800D08D4(arg0);
        func_800D15D0(arg0);
        arg0->state = arg0->state + 1;
    } else {
        obj->timer = (u16)obj->timer + 1;
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D1EB8);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D1F90);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D1FD4);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D2020);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D20B8);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D2224);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D2384);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D2538);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D27E8);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D29B0);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D2E04);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D2F68);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D30CC);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D3660);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D3D98);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D3FF0);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D4140);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D4270);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D4D2C);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D4E40);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D4E78);

s32 func_800D4EC0(void)
{
    return D_80114E88;
}

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D4ED0);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D4FD0);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D50D4);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D5178);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D5234);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D5338);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D540C);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D5520);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D5648);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D573C);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D5770);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D587C);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D5968);

INCLUDE_ASM("gameplay/nonmatchings/3688", func_800D5A48);
