#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

#include "rooms/room_common.h"
#include "rooms/shelter_b4_water_supply.h"

/// Task table spawned by `func_shelter_b4_water_supply_8017DA30` once the
/// valve script has run.
extern TaskDesc   D_shelter_b4_water_supply_80182620[];
extern GpMsgEntry D_shelter_b4_water_supply_801825F0[];
extern TaskDesc   D_shelter_b4_water_supply_8018263C[];
void              func_shelter_b4_water_supply_8017DB18(void);
INCLUDE_ASM("rooms/nonmatchings/shelter_b4_water_supply/shelter_b4_water_supply", func_shelter_b4_water_supply_8017D650);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b4_water_supply/shelter_b4_water_supply", RoomsShared8017d878Table);

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_water_supply/shelter_b4_water_supply", func_shelter_b4_water_supply_8017D7C0);

s32 func_shelter_b4_water_supply_8017D970(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_water_supply/shelter_b4_water_supply", func_shelter_b4_water_supply_8017D978);

s32 func_shelter_b4_water_supply_8017DA28(void)
{
    return 0;
}

/// Handler for slot-7 msg `0x13EF` in `D_shelter_b4_water_supply_801825F0`:
/// the directed action on the water-supply valve (`field_2` 0xA / `field_3`
/// 0x20).
s32 func_shelter_b4_water_supply_8017DA30(Task* task, s32 msgId, GpMsg13EF* arg2, s32 arg3)
{
    if (arg2->field_2 == 0xA) {
        if (arg2->field_3 == 0x20) {
            if (GameFlag_GetNibble(0xB8) != 0) {
                if (GameFlag_GetNibble(0x139) != 0) {
                    func_shelter_b4_water_supply_8017DB18();
                } else {
                    GameFlag_SetNibble(0x139, 1);
                    Gp_MsgPlayerWeapon(0);
                    Gp_RunCapCmd1(3);
                    Task_SpawnFromTable(D_shelter_b4_water_supply_80182620, 0, 0, 0);
                }
            } else {
                Gp_RunCapCmd1(1);
                GameFlag_SetNibble(0x1BD, 2);
            }
        }
    }
    return 0;
}

s32 func_shelter_b4_water_supply_8017DAE4(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    if (arg2 == 6) {
        SndEvt_EnqueueType6(0x542E0000 | 6, 0, 0);
    }
    return 0;
}

void func_shelter_b4_water_supply_8017DB18(void)
{
    ShelterB4WaterSupplyEventDesc  work;
    RoomEventMsg                   param;
    ShelterB4WaterSupplyEventDesc* wp;
    s32                            (*resolve)(RoomEventMsg*, RoomEventMsg*) = func_shelter_b4_water_supply_8017DDFC;

    work.field_0 = 3;
    work.field_1 = 0x20;
    work.field_2 = 3;
    work.field_3 = 1;
    work.field_8 = 0x542E0003;
    work.field_4 = 0x400;
    Gp_MsgPlayerWeapon(0);
    wp            = &work;
    param.msgId   = wp->field_1;
    param.field_2 = wp->field_2;
    param.field_3 = wp->field_3;
    param.field_5 = 0;
    resolve(&param, &param);
    wp->field_1                        = param.msgId;
    wp->field_2                        = param.field_2;
    wp->field_3                        = param.field_3;
    D_shelter_b4_water_supply_80184E44 = work;
    Task_SpawnFromTable(&D_shelter_b4_water_supply_801825E4, 0, 0, 0);
    if (gameGetPtrSlot(0xA) != NULL && GameFlag_GetNibble(0xCF) == 0) {
        GameFlag_SetNibble(0x4C, 6);
    }
}

void func_shelter_b4_water_supply_8017DC28(Task* arg0)
{
    ShelterB4WaterSupplyEventDesc work;
    RoomEventMsg                  param;
    s32                           (*resolve)(RoomEventMsg*, RoomEventMsg*);

    if (Gp_CapBusy() == 0) {
        resolve      = func_shelter_b4_water_supply_8017DDFC;
        work.field_0 = 3;
        work.field_1 = 0x20;
        work.field_2 = 3;
        work.field_3 = 1;
        work.field_8 = 0x542E0003;
        work.field_4 = 0x400;
        Gp_MsgPlayerWeapon(0);
        param.msgId   = work.field_1;
        param.field_2 = work.field_2;
        param.field_3 = work.field_3;
        param.field_5 = 0;
        resolve(&param, &param);
        work.field_1                       = param.msgId;
        work.field_2                       = param.field_2;
        work.field_3                       = param.field_3;
        D_shelter_b4_water_supply_80184E44 = work;
        Task_SpawnFromTable(&D_shelter_b4_water_supply_801825E4, 0, 0, 0);
        if (gameGetPtrSlot(0xA) != NULL && GameFlag_GetNibble(0xCF) == 0) {
            GameFlag_SetNibble(0x4C, 6);
        }
        taskKill(arg0);
    }
}

void func_shelter_b4_water_supply_8017DD40(Task* arg0)
{
    arg0->msgTable = D_shelter_b4_water_supply_801825F0;
    Game_SetPtrSlot(arg0, 7);
    Task_SpawnFromTable(D_shelter_b4_water_supply_8018263C, 0, 0, 0);
    arg0->state = (s32)(arg0->state + 1);
}

void func_shelter_b4_water_supply_8017DD9C(void)
{
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_b4_water_supply/shelter_b4_water_supply", jtbl_shelter_b4_water_supply_8017D5FC);
