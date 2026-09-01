#include "common.h"

#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/3CD8.h"

#include "actors/actor_403100.h"

/// Overlay-wide work block; `Task::extra` is a `TmdObject` whose `field_8` is
/// this actor's `GsCOORDINATE2`.
extern Actor403100Work* D_actor_403100_80155808;

extern u8 D_801153F4;

/* Resolved through `configs/USA/sym/actors.imports.txt`. */
void func_8017E128(s32 arg0);

/* Still `INCLUDE_ASM` in this unit. */
void func_actor_403100_801327CC(void);
s32  func_actor_403100_80133928(void);
void func_actor_403100_801345E0(Task* arg0, Task* arg1);

void func_actor_403100_8013E6F0(Task* arg0);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_80132064);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_80132320);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_80132528);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_801326DC);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_801327CC);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_801328DC);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_80132C3C);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_801331D4);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100", D_actor_403100_80131E20);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100", D_actor_403100_80131E24);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013335C);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_80133928);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_801339EC);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_80133C94);

void func_actor_403100_80133D88(Task* arg0)
{
    GsCOORDINATE2* coord;
    u16            frame;

    coord                              = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->field_8;
    frame                              = D_actor_403100_80155808->field_5EC + 1;
    D_actor_403100_80155808->field_5EC = frame;
    if ((s16)frame >= 0xA0) {
        coord->coord.t[1] += 0xA;
    }
    if ((s16)D_actor_403100_80155808->field_5EC >= 0x82) {
        func_actor_403100_801345E0(arg0, arg0);
    }
    if ((s16)D_actor_403100_80155808->field_5EC == 0x12C) {
        D_actor_403100_80155808->field_5D8 = 0x1600;
        coord->coord.t[0]                  = -0xA28;
        D_actor_403100_80155808->field_5E2 = 0x10;
        D_actor_403100_80155808->field_5DE = 0xC;
        D_actor_403100_80155808->field_5DA = 2;
        coord->coord.t[1]                  = 0x1390;
        coord->coord.t[2]                  = 0x1130;
        D_actor_403100_80155808->field_82  = -0x6B0;
        D_actor_403100_80155808->field_604 = -0x140;
        D_actor_403100_80155808->field_608 = -0x100;
        D_actor_403100_80155808->field_5EC = 0;
        D_actor_403100_80155808->field_5F8 = D_actor_403100_80155808->field_5F8 + 1;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_80133E88);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_801342B4);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_801345E0);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013480C);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100", D_actor_403100_80131E70);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100", D_actor_403100_80131E7C);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_80134D50);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013506C);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_801351F8);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013539C);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_801354A0);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_801355D4);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_801356F4);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013588C);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_801359DC);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_80135AE0);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_80135C00);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_80135F30);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_80136100);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013631C);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_80136610);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100", D_actor_403100_80131EB0);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100", D_actor_403100_80131EBC);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100", D_actor_403100_80131EC8);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100", D_actor_403100_80131ED8);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100", D_actor_403100_80131EE4);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100", D_actor_403100_80131EF0);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100", D_actor_403100_80131F00);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100", D_actor_403100_80131F10);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100", D_actor_403100_80131F1C);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_80136830);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013712C);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100", D_actor_403100_80131F60);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_80137268);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_80137310);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_801375B8);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_801376D8);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_801379B4);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_80137CA8);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_80137DC4);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_80137F4C);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_80138048);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013842C);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_80138610);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_801386DC);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_80138790);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_80138844);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_80138AB4);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_80138C18);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_80138D08);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_80138DB0);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_80138F88);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013922C);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_801395EC);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_80139818);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_80139E80);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013A064);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013A254);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013A4C8);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013A5AC);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013A81C);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013AA04);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013AC04);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013AE28);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013B128);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013B3C4);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013B5E0);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100", D_actor_403100_80131F84);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100", D_actor_403100_80131F9C);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100", D_actor_403100_80131FB0);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100", D_actor_403100_80131FD4);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100", D_actor_403100_80132000);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100", D_actor_403100_80132014);

INCLUDE_RODATA("actors/nonmatchings/actor_403100/actor_403100", D_actor_403100_80132024);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013BA64);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013BB8C);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013BDE4);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013BEF0);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013C008);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013C214);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013C7B4);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013CBE0);

void func_actor_403100_8013CDC0(void)
{
    s16 next;
    s16 next2;
    u8  state;

    state = D_actor_403100_80155808->field_664.b.field_666;
    switch (state) { /* irregular */
        case 0:
            D_actor_403100_80155808->field_5EA =
                (u16)D_actor_403100_80155808->field_5EA +
                ((s32) - (D_actor_403100_80155808->field_5EA * 0x10) >> 7);
            return;
        case 1:
            D_actor_403100_80155808->field_664.b.field_666 = 2;
            return;
        case 2:
            next = (u16)D_actor_403100_80155808->field_5EA +
                   ((s32)(0x1E00 - (D_actor_403100_80155808->field_5EA * 0x10)) >> 7);
            D_actor_403100_80155808->field_5EA = next;
            if (next >= 0x1C0) {
                D_actor_403100_80155808->field_664.b.field_666 =
                    D_actor_403100_80155808->field_664.b.field_666 + 1;
                return;
            }
            return;
        case 3:
            next2                              = (u16)D_actor_403100_80155808->field_5EA - 0xC;
            D_actor_403100_80155808->field_5EA = next2;
            if ((next2 << 0x10) <= 0) {
                D_actor_403100_80155808->field_664.b.field_666 = 0;
            }
            break;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013CEAC);

void func_actor_403100_8013CF60(SVECTOR* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4)
{
    s16 facing;
    s16 target;
    u16 yaw;
    u16 targetU;
    u16 facingU;

    yaw = (u16)arg0->vy;
    if ((u32)((yaw + 0x27F) & 0xFFFF) < 0x4FFU) {
        target  = D_actor_403100_80155808->field_B2;
        targetU = (u16)D_actor_403100_80155808->field_B2;
        if ((u32)(((s16)yaw - target) + 0x20) >= 0x41U) {
            if (target < (s16)yaw) {
                D_actor_403100_80155808->field_B2 = targetU + arg1;
                D_actor_403100_80155808->field_82 = D_actor_403100_80155808->field_82 + arg2;
                return;
            }
            D_actor_403100_80155808->field_B2 = targetU - arg1;
            D_actor_403100_80155808->field_82 = D_actor_403100_80155808->field_82 - arg2;
            return;
        }
        facing  = (s16)D_actor_403100_80155808->field_82;
        facingU = D_actor_403100_80155808->field_82;
        if (facing < target) {
            D_actor_403100_80155808->field_82 = facingU + arg3;
            return;
        }
        if (target < facing) {
            D_actor_403100_80155808->field_82 = facingU - arg3;
        }
    } else {
        if ((s16)yaw >= 0x281) {
            D_actor_403100_80155808->field_82 = D_actor_403100_80155808->field_82 + arg4;
        }
        if (arg0->vy < -0x280) {
            D_actor_403100_80155808->field_82 = D_actor_403100_80155808->field_82 - arg4;
        }
    }
}

void func_actor_403100_8013D06C(void)
{
    if (D_actor_403100_80155808->field_B4 >= 0x11) {
        D_actor_403100_80155808->field_B4 = (u16)D_actor_403100_80155808->field_B4 - 8;
    }
    if (D_actor_403100_80155808->field_B4 < -0x10) {
        D_actor_403100_80155808->field_B4 = (u16)D_actor_403100_80155808->field_B4 + 8;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013D0B8);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013D11C);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013D1B8);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013D24C);

void func_actor_403100_8013D2A0(s16 arg0)
{
    if (!(D_actor_403100_80155808->field_664.word & 0xFFFF00)) {
        if (arg0 == 1) {
            D_actor_403100_80155808->field_66F = 2;
        }
        D_actor_403100_80155808->field_664.b.field_665 = 1;
        D_actor_403100_80155808->field_664.b.field_666 = 1;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013D2F4);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013D460);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013D564);

void func_actor_403100_8013D5F4(void)
{
    D_actor_403100_80155808->field_65F = 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013D608);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013D6B4);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013D700);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013D74C);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013D770);

void func_actor_403100_8013D88C(Task* arg0)
{
    Gp_UnlinkObj(&D_actor_403100_80155808->field_47C);
    Gp_UnlinkObj(&D_actor_403100_80155808->field_414);
    Gp_UnlinkObj(&D_actor_403100_80155808->field_55C);
    Gp_UnlinkObj(&D_actor_403100_80155808->field_594);
    Gp_DestroyEnemy(arg0->spawnArg2, arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013D8F4);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013D9C4);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013DA6C);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013DAC4);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013DB48);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013DC18);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013DCAC);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013DD78);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013DE0C);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013DEA0);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013DF0C);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013DF64);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013DFBC);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013E02C);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013E04C);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013E0A4);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013E0FC);

void func_actor_403100_8013E16C(void)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013E174);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013E1E4);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013E2BC);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013E33C);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013E450);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013E5FC);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013E624);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013E6A0);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013E6F0);

void func_actor_403100_8013E784(Task* arg0)
{
    u16 temp_v0;

    temp_v0             = arg0->killCountdown + 1;
    arg0->killCountdown = temp_v0;
    if ((s16)temp_v0 >= 0x1E) {
        Task_Kill(arg0);
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013E7C8);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013E88C);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013E920);

void func_actor_403100_8013E964(void)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013E96C);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013E9D8);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013EA60);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013EAD4);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013EB68);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013EBC8);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013EC4C);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013ECD0);

void func_actor_403100_8013ED48(void)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013ED50);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013EDDC);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013EE28);

void func_actor_403100_8013EEB0(void)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013EEB8);

void func_actor_403100_8013EF24(void)
{
}

void func_actor_403100_8013EF2C(void)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013EF34);

void func_actor_403100_8013EF58(void)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013EF60);

void func_actor_403100_8013EFC0(void)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013EFC8);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013F034);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013F0A8);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013F12C);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013F18C);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013F1D8);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013F230);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013F270);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013F2D8);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013F344);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013F3AC);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013F3EC);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013F488);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013F4E0);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013F520);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013F588);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013F610);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013F658);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013F6B0);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013F6F4);

INCLUDE_ASM("actors/nonmatchings/actor_403100/actor_403100", func_actor_403100_8013F76C);

void func_actor_403100_8013F7AC(void)
{
}

void func_actor_403100_8013F7B4(void)
{
}

void func_actor_403100_8013F7BC(void)
{
}
