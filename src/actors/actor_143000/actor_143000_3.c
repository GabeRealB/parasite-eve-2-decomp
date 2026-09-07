#include "common.h"

#include "actors/actor_143000.h"
#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "psyq/strings.h"
extern TaskDesc D_actor_143000_801350C8;

extern s32              D_80070F6C;
extern s8               D_8007218B;
extern u8               D_801153F4;
extern GpAreaApplyRec   D_80186488;
extern GpAreaApplyRec   D_8018649C;
extern char             D_actor_143000_80131EB0[];
extern char             D_actor_143000_80131EBC[];
extern s32              D_actor_143000_801351B0;
extern s32              D_actor_143000_80135870;
extern s32              D_actor_143000_80135A20;
extern s32              D_actor_143000_80135AE0;
extern Actor143000Spawn D_actor_143000_80135C08;
extern s32              D_actor_143000_80135C14;
extern s32              D_actor_143000_80135C18;
extern s32              D_actor_143000_80135C1C;
extern char             D_actor_143000_80135C20[];
extern u8               D_actor_143000_80135C38[];

void func_actor_143000_80133EE4(Task* arg0)
{
    Actor143000Spawn* spawn;
    s32               i;
    u8*               p;
    u8*               slot;

    spawn = arg0->spawnArg2;
    switch (arg0->state) {
        case 0:
            spawn->field_1 = 1;
            srand(D_80070F6C);
            Gp_MsgPlayerWeapon(0);
            Gp_CapFile = 0;
            Gp_LoadCapFile(1);
            func_800E6D4C(0x340, 0);
            Gp_SetItemSeenBit(0x121, 1);
            Gp_SetItemSeenBit(0x122, 1);
            func_800E8614((s32)&D_actor_143000_80135A20, 1);
            arg0->state++;
            return;
        case 1:
            if (Game_Session->field_1 == 0) {
                Game_Session->field_1 = 1;
                arg0->state           = 2;
            }
            return;
        case 2:
            if (Gp_HasCollectedBit(0x121) != 0) {
                Gp_RunCapCmd(1, 0);
            } else {
                Gp_RunCapCmd(2, 0);
            }
            arg0->state++;
            return;
        case 3:
            if (Gp_CapBusy() == 0) {
                arg0->state++;
            }
            return;
        case 4:
            switch (Gp_GetCapEventKey()) {
                case 0xA:
                    arg0->state = 0x14;
                    return;
                case 0x63:
                    Gp_RunCapCmd(0x20, 0);
                    arg0->state++;
                    return;
                default:
                    Gp_RunCapCmd(3, 0);
                    arg0->state = 6;
                    return;
            }
        case 5:
            if (Gp_CapBusy() == 0) {
                arg0->state = 0xA;
            }
            return;
        case 10:
            func_800E8614((s32)&D_actor_143000_80135AE0, 0);
            arg0->state++;
            return;
        case 11:
            if (Game_Session->field_1 == 0) {
                arg0->state++;
            }
            return;
        case 12:
            Gp_ResetCap();
            Task_Kill(arg0);
            return;
        case 20:
            i = 10;
            p = &D_actor_143000_80135C38[i];
            do {
                *p = 0;
                i--;
                p--;
            } while (i >= 0);
            D_actor_143000_80135C18 = 0;
            D_actor_143000_80135C1C = 0;
            Gp_RunCapCmd(4, 0);
            arg0->state++;
            return;
        case 21:
            if (Gp_CapBusy() == 0) {
                arg0->state++;
            }
            return;
        case 22:
            while (1) {
                D_actor_143000_80135C14 = (rand() * 11) >> 15;
                slot                    = &D_actor_143000_80135C38[D_actor_143000_80135C14];
                if (*slot == 0) {
                    *slot = 1;
                    break;
                }
            }
            Gp_RunCapCmd(D_actor_143000_80135C14 + 5, 0);
            arg0->state++;
            return;
        case 23:
            if (Gp_CapBusy() == 0) {
                arg0->state++;
            }
            return;
        case 24:
            if (Gp_GetCapEventKey() == 0xB) {
                D_actor_143000_80135C1C++;
            }
            D_actor_143000_80135C18++;
            if (D_actor_143000_80135C18 >= 3) {
                if (D_actor_143000_80135C1C >= 3) {
                    arg0->state = 0x28;
                } else {
                    arg0->state = 0x1E;
                }
            } else {
                arg0->state = 0x16;
            }
            return;
        case 30:
            Gp_StartCapSlot(0x21, 0, (s16)D_actor_143000_80135C1C);
            arg0->state++;
            return;
        case 6:
        case 31:
            if (Gp_CapBusy() == 0) {
                arg0->state = 2;
            }
            return;
        case 40:
            Gp_RunCapCmd(0x22, 0);
            arg0->state++;
            return;
        case 41:
            if (Gp_CapBusy() == 0) {
                Gp_ResetCap();
                func_800E3FAC(0xA2, 0x27);
                GameFlag_SetNibble(0xD0, 2);
                GameFlag_SetNibble(0x4B, 0);
                Gp_ApplyAreaRecs(&D_80186488);
                if (GameFlag_GetNibble(0x83) == 0) {
                    Gp_ApplyAreaRecs(&D_8018649C);
                }
                GameFlag_SetNibble(3, 0);
                GameFlag_SetNibble(0x155, 3);
                func_800E8634((s32)&D_actor_143000_801351B0, 0, (s32)&D_actor_143000_80135870);
                Task_Kill(arg0);
            }
            return;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_143000/actor_143000_3", func_actor_143000_801342F8);

void func_actor_143000_801344A8(s32 arg0)
{
    Task_SpawnFromTable(&D_actor_143000_801350C8, 0, 0, arg0);
}

void func_actor_143000_801344D8(void)
{
    Gp_CapFile = 0;
    Gp_LoadCapFile(3);
    func_800E6D4C(0x180, 0x100);
}

void func_actor_143000_8013450C(void)
{
    Gp_ResetCap();
}

void func_actor_143000_8013452C(u8 arg0)
{
    D_801153F4 = arg0;
}

void func_actor_143000_80134538(void)
{
    Gp_SpawnWeaponEff();
}
