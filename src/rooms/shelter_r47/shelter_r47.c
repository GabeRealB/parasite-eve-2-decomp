#include "common.h"

#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

#include "rooms/shelter_r47.h"

INCLUDE_RODATA("rooms/nonmatchings/shelter_r47/shelter_r47", RoomsShared8017d878Table);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47", func_shelter_r47_8017FB94);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47", func_shelter_r47_8017FCC0);

s32 func_shelter_r47_8017FE84(s32 arg0, s32 arg1, RoomEventMsg* arg2)
{
    Task*             spawned_p;
    Task*             spawned_p6;
    Task*             spawned_a;
    Task*             spawned_a0;
    Task*             spawned_a1;
    s32               flag_a;
    s32               flag_b;
    s32               kind;
    u8                field9;
    ShelterR47Object* p;
    ShelterR47Object* q;

    field9 = Game_Session->field_9;
    if (field9 == 1) {
        switch (arg2->field_2) {
            case 2:
                if ((GameFlag_GetNibble(0x83) == 1) && (GameFlag_GetNibble(0x81) == 0)) {
                    SndEvt_EnqueueType6(0x542F0001, -0xA, 0x40);
                    GameFlag_SetNibble(0x81, 1);
                }
                break;
            case 3:
                if ((GameFlag_GetNibble(0x83) == 1) && (GameFlag_GetNibble(0x82) == 0)) {
                    func_800E3FAC(0xA2, 0x2B);
                    GameFlag_SetNibble(3, 0);
                    GameFlag_SetNibble(0x155, 5);
                    func_800E8634((s32)&D_8014152C, 0, (s32)&D_80141C1C);
                    GameFlag_SetNibble(0x82, 1);
                    if (GameFlag_GetNibble(0x165) == 0) {
                        flag_a = 0x165;
                        flag_b = 1;
                        goto set_and_toggle;
                    }
                    goto toggle_only;
                }
                break;
            case 4:
                if ((GameFlag_GetNibble(0x83) == 1) && (GameFlag_GetNibble(0x82) >= 4)) {
                    func_800E8614((s32)&D_80143494, 0);
                }
                break;
            case 5:
                spawned_p              = Task_SpawnFromTable(&D_shelter_r47_80187618, 0, 0, 0);
                D_shelter_r47_8018A690 = spawned_p;
                if (spawned_p != NULL) {
                    Gp_MsgPlayer3F3(0);
                    Gp_MsgPlayerWeapon(0);
                    Gp_MsgSlot4Chain(0, 0);
                    Task_SpawnFromTable(&D_shelter_r47_80186F70, 0, 0, 0);
                }
                break;
            case 6:
                spawned_p6             = Task_SpawnFromTable(&D_shelter_r47_80187020, 0, 0, 0);
                D_shelter_r47_8018A690 = spawned_p6;
                if (spawned_p6 != NULL) {
                    Gp_MsgPlayer3F3(0);
                    Gp_MsgPlayerWeapon(0);
                    Gp_MsgSlot4Chain(0, 0);
                    Task_SpawnFromTable(&D_shelter_r47_80186F70, 0, 0, 0);
                }
                break;
        }
    } else if (field9 == 2) {
        kind = arg2->field_2;
        if (kind < 6) {
            if (kind < 4) {
                if ((kind == 1) && (GameFlag_GetNibble(0x83) == 0) && (GameFlag_GetNibble(0x80) == 0)) {
                    if (Game_GetPtrSlot(0xA) != 0) {
                        func_800E8634((s32)&D_801350BC, 0, (s32)&D_801359D4);
                    }
                    func_800E3FAC(0xA2, 0x2A);
                    GameFlag_SetNibble(0x80, 1);
                    GameFlag_SetNibble(0x4C, 8);
                    GameFlag_SetNibble(0xD1, 2);
                    Gp_FillAllyHp();
                    GameFlag_SetNibble(3, 0);
                    flag_a = 0x155;
                    flag_b = 4;
                set_and_toggle:
                    GameFlag_SetNibble(flag_a, flag_b);
                toggle_only:
                    p            = D_shelter_r47_80187960;
                    q            = p + 1;
                    p->field_4A &= 0xBF;
                    q->field_4A |= 0x40;
                }
            } else {
                spawned_a0             = Task_SpawnFromTable(&D_shelter_r47_80187618, 0, 0, 0);
                D_shelter_r47_8018A690 = spawned_a0;
                if (spawned_a0 != NULL) {
                    Gp_MsgPlayer3F3(0);
                    Gp_MsgPlayerWeapon(0);
                    if (Game_GetPtrSlot(0xA) != 0) {
                        Gp_MsgAlly3F3(0);
                        Gp_MsgAllyWeapon(0);
                    }
                    Task_SpawnFromTable(&D_shelter_r47_80186F70, 0, 0, 0);
                }
                goto done;
            }
        } else {
            if (kind == 6) {
                goto spawn_six;
            }
        }
    } else {
        switch (arg2->field_2) {
            case 4:
            case 5:
                spawned_a1             = Task_SpawnFromTable(&D_shelter_r47_80187618, 0, 0, 0);
                D_shelter_r47_8018A690 = spawned_a1;
                if (spawned_a1 != NULL) {
                    Gp_MsgPlayer3F3(0);
                    Gp_MsgPlayerWeapon(0);
                    if (Game_GetPtrSlot(0xA) != 0) {
                        Gp_MsgAlly3F3(0);
                        Gp_MsgAllyWeapon(0);
                    }
                    Task_SpawnFromTable(&D_shelter_r47_80186F70, 0, 0, 0);
                }
                goto done;
            case 6:
            spawn_six:
                spawned_a              = Task_SpawnFromTable(&D_shelter_r47_80187020, 0, 0, 0);
                D_shelter_r47_8018A690 = spawned_a;
                if (spawned_a != NULL) {
                    Gp_MsgPlayer3F3(0);
                    Gp_MsgPlayerWeapon(0);
                    if (Game_GetPtrSlot(0xA) != 0) {
                        Gp_MsgAlly3F3(0);
                        Gp_MsgAllyWeapon(0);
                    }
                    Task_SpawnFromTable(&D_shelter_r47_80186F70, 0, 0, 0);
                }
                break;
        }
    }
done:
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47", func_shelter_r47_801801DC);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47", func_shelter_r47_80180324);

INCLUDE_RODATA("rooms/nonmatchings/shelter_r47/shelter_r47", RoomsShared8017d8d0States);
