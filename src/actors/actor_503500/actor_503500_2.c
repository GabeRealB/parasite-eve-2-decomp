#include "common.h"

#include "actors/actor_503500.h"

s32  func_actor_503500_80133684(Actor503500* arg0);
void func_actor_503500_801338E8(Actor503500* arg0);
void func_actor_503500_80134408(Actor503500* arg0);
void func_actor_503500_801345F4(Actor503500* arg0);
void func_actor_503500_80134A24(Actor503500* arg0);
void func_actor_503500_80134C68(Actor503500* arg0);
void func_actor_503500_80135FB4(Actor503500* arg0, s32 arg1, s32 arg2);
void func_actor_503500_80136450(Actor503500* arg0);
void func_actor_503500_801369E4(Actor503500* arg0);
void func_actor_503500_80136A80(Actor503500* arg0);
void func_actor_503500_80136EFC(Actor503500* arg0, s32 arg1);
void func_actor_503500_801374BC(Actor503500* arg0);
void func_actor_503500_80137678(Actor503500* arg0);
void func_actor_503500_80138454(Actor503500* arg0);

void func_actor_503500_80136304(Actor503500* arg0)
{
    Actor503500Work* work = arg0->field_1C;
    u16              timer;

    switch (work->field_7B0) {
        case 0:
            if (func_actor_503500_80133684(arg0) == 0) {
                func_actor_503500_80136450(arg0);
            }
            break;
        case 1:
            func_actor_503500_801338E8(arg0);
            break;
        case 2:
            func_actor_503500_801369E4(arg0);
            break;
        case 3:
            timer           = work->field_7B2 - 1;
            work->field_7B2 = timer;
            if ((s16)timer < 0) {
                func_actor_503500_80135FB4(arg0, 6, 0x10);
                work->field_7B2 = 3;
            }
            if (Gp_TickObjFlag2(arg0->field_20) != 0) {
                func_actor_503500_80136EFC(arg0, 0);
                work->field_7CA = 0x3C;
            }
            break;
        case 4:
            func_actor_503500_80134408(arg0);
            break;
        case 5:
            func_actor_503500_80136A80(arg0);
            break;
        case 6:
            func_actor_503500_801345F4(arg0);
            break;
        case 7:
            func_actor_503500_80134A24(arg0);
            break;
    }
    if (work->field_7E0 != 0) {
        func_actor_503500_80134C68(arg0);
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80136450);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_801364D0);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013656C);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013667C);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80136770);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013680C);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80136948);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_801369E4);

void func_actor_503500_80136A80(Actor503500* arg0)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80136A88);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80136AEC);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80136B64);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80136D30);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80136DDC);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80136EFC);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80136F40);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80136FA8);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80136FDC);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80137048);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80137074);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80137088);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80137158);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80137238);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80137290);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_801372AC);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_801372C8);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_801374BC);

INCLUDE_RODATA("actors/nonmatchings/actor_503500/actor_503500_2", D_actor_503500_80131F4C);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80137678);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80137C90);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013815C);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80138288);

void func_actor_503500_801382F4(void)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_801382FC);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80138378);

void func_actor_503500_801383D0(Actor503500* arg0)
{
    switch (arg0->field_1C->field_15C) {
        case 0:
            func_actor_503500_80138454(arg0);
            break;
        case 1:
            func_actor_503500_801374BC(arg0);
            break;
        case 2:
            func_actor_503500_80137678(arg0);
            break;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80138454);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80138490);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_801384D4);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013852C);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80138898);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80138A30);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80138C08);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80139014);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_801395BC);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_801398D0);

INCLUDE_RODATA("actors/nonmatchings/actor_503500/actor_503500_2", D_actor_503500_80131F9C);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80139A20);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80139EFC);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013A0D0);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013A470);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013A7B0);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013A900);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013A96C);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013AA44);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013AAC0);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013AB38);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013AC6C);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013ACC4);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013AD0C);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013AD64);

INCLUDE_RODATA("actors/nonmatchings/actor_503500/actor_503500_2", D_actor_503500_80131FF0);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013AF60);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013B460);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013B60C);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013B8D0);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013BBCC);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013BC54);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013BCB4);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013BD0C);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013BD88);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013BE0C);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013BE48);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013BE8C);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013BEE4);

INCLUDE_RODATA("actors/nonmatchings/actor_503500/actor_503500_2", D_actor_503500_80132028);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013C088);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013C558);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013C878);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013C900);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013C960);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013C9DC);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013CA34);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013CA74);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013CA8C);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013CAE4);

INCLUDE_RODATA("actors/nonmatchings/actor_503500/actor_503500_2", D_actor_503500_80132060);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013CCBC);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013D1CC);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013D558);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013D7D4);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013D85C);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013D8BC);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013D914);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013D990);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013DA2C);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013DBA8);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013DBF4);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013DC4C);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013DD10);

INCLUDE_RODATA("actors/nonmatchings/actor_503500/actor_503500_2", D_actor_503500_80132098);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013DEB4);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013E384);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013E740);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013E9A4);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013EA2C);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013EA8C);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013EAE4);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013EB60);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013EBE4);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013EC20);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013EC64);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013ECBC);

INCLUDE_RODATA("actors/nonmatchings/actor_503500/actor_503500_2", D_actor_503500_801320D0);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013EE5C);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013F328);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013F4A4);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013F6F0);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013F778);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013F7D8);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013F830);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013F8AC);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013F948);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013F984);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013F9D4);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013FA1C);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013FA74);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8013FF0C);

INCLUDE_RODATA("actors/nonmatchings/actor_503500/actor_503500_2", D_actor_503500_80132108);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_801400A4);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80140654);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80140BE8);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80140D38);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80141248);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80141448);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8014176C);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80141A44);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80141B94);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80141D04);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80141D7C);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80141E64);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80141F48);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80141FC8);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_801420C4);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8014215C);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_801421A8);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80142220);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_801422B8);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80142310);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80142370);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_801423C8);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8014271C);

INCLUDE_RODATA("actors/nonmatchings/actor_503500/actor_503500_2", D_actor_503500_80132178);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80142980);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_801431EC);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_801437D0);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80143AC0);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80143EB4);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80143F78);

void func_actor_503500_80143FFC(void)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80144004);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80144098);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_801440F0);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8014418C);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_801441E8);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80144238);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_801442A8);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80144300);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80144520);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_801446E4);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8014473C);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80144778);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80144890);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_801448E8);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80144B40);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80144D50);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80144DA8);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80144E10);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80144E34);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80144E8C);

INCLUDE_RODATA("actors/nonmatchings/actor_503500/actor_503500_2", D_actor_503500_801321F4);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_801450A0);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80145428);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80145480);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_801454E0);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8014554C);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_801455A4);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80145754);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_801458F8);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80145950);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_801459B0);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_801459D4);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80145A2C);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80145C50);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80145E1C);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80145E98);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80145F18);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80145F84);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80145FDC);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8014618C);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_801463C0);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8014642C);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_801464E8);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80146508);

void func_actor_503500_80146524(void)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_8014652C);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_80146664);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_801466E0);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_2", func_actor_503500_801467C0);
