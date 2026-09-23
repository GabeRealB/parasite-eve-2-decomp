#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

#include "rooms/shelter_r47.h"

INCLUDE_RODATA("rooms/nonmatchings/shelter_r47/shelter_r47", RoomsShared8017d878Table);

/// Message table the room's controller task answers with.
extern GpMsgEntry D_shelter_r47_80186F2C[];

/// Ally animation descriptor handed to `Gp_AllyAnimId`, then forwarded as the
/// payload of the 0x3E8 message.
extern s32 D_shelter_r47_80186F5C;

void func_shelter_r47_8017FB94(Task* task)
{
    Task* player;

    task->msgTable = D_shelter_r47_80186F2C;
    Game_SetPtrSlot(task, 7);
    player = gameGetPtrSlot(0xA);
    if (player != NULL && GameFlag_GetNibble(0x80) == 0 && GameFlag_GetNibble(0xD1) == 1) {
        Gp_DispatchMsg(player, 0x3F3, 0, 0);
        Gp_AllyAnimId(&D_shelter_r47_80186F5C);
        Gp_DispatchMsg(player, 0x3E8, (s32)&D_shelter_r47_80186F5C, 0);
    }
    D_shelter_r47_8018A690 = NULL;
    func_shelter_r47_80183210();
    Task_SpawnFromTable(&D_shelter_r47_80186F70, 1, 0, 0);
    if (GameFlag_GetNibble(0x83) == 1 || GameFlag_GetNibble(0x80) == 1) {
        D_shelter_r47_80187960[0].field_4A &= 0xBF;
    } else {
        D_shelter_r47_80187C0C.field_4A &= 0xBF;
    }
    task->state++;
}

void func_shelter_r47_8017FCC0(void)
{
    u8 place = gGameSession->at4.loc.place;

    if (place != 1 || D_80114C12 == place) {
        return;
    }
    switch (GameFlag_GetNibble(0x82)) {
        case 1:
            if (gGameSession->eventState == 0) {
                D_shelter_r47_8018A690 = Task_SpawnFromTable(&D_shelter_r47_80187020, 0, 1, 0);
                Gp_MsgPlayer3F3(0);
                Gp_MsgPlayerWeapon(0);
                Gp_MsgSlot4Chain(0, 0);
                GameFlag_SetNibble(0x82, 2);
            }
            break;
        case 2:
            if (gGameSession->cutsceneHold == 0) {
                func_800E8634((s32)&D_80141D9C, 0, (s32)&D_80142A74);
                GameFlag_SetNibble(0x82, 3);
            }
            break;
        case 3:
            if (gGameSession->eventState == 0) {
                D_shelter_r47_8018A690 = Task_SpawnFromTable(&D_shelter_r47_80187618, 0, 2, 0);
                Gp_MsgPlayer3F3(0);
                Gp_MsgPlayerWeapon(0);
                Gp_MsgSlot4Chain(0, 0);
                GameFlag_SetNibble(0x82, 4);
            }
            break;
        case 4:
            if (gGameSession->cutsceneHold == 0) {
                func_800E8634((s32)&D_80142C24, 0, (s32)&D_801432FC);
                GameFlag_SetNibble(0x82, 5);
            }
            break;
    }
}

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

    field9 = gGameSession->at4.loc.place;
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
                    if (gameGetPtrSlot(0xA) != 0) {
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
                    if (gameGetPtrSlot(0xA) != 0) {
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
                    if (gameGetPtrSlot(0xA) != 0) {
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
                    if (gameGetPtrSlot(0xA) != 0) {
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

/// Room request handler. Request 1 plays the room's cutscene through the shared
/// runner once flag 0x13E is set (or runs CAP command 0x2A instead while the
/// 2-bit flag 0x22 reads 1); the first time, it sets that flag and spawns entry
/// 2 of the room's task table. Request 8 spawns entry 0 or 1 of the second task
/// table, depending on which of flags 0x83 and 0x80 is set.
s32 func_shelter_r47_801801DC(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 1) {
        if (GameFlag_GetNibble(0x13E) != 0) {
            if (Gp_GetCurBit2Flag(0x22) == arg2) {
                Gp_RunCapCmd1(0x2A);
                return 0;
            }
            D_shelter_r47_8018A698.field_0  = 0x2C;
            D_shelter_r47_8018A698.field_1  = arg2;
            D_shelter_r47_8018A698.field_3  = 3;
            D_shelter_r47_8018A698.field_2  = 0;
            D_shelter_r47_8018A698.field_4  = 0x542F000C;
            D_shelter_r47_8018A698.field_8  = 0x542F000F;
            D_shelter_r47_8018A698.field_10 = 0x542F000D;
            D_shelter_r47_8018A698.field_C  = 0x542F000E;
            Task_SpawnFromTable(&RoomsShared80181228Desc, 0, 0xA, (s32)&D_shelter_r47_8018A698);
        } else {
            GameFlag_SetNibble(0x13E, 1);
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(&D_shelter_r47_80186F70, 2, 1, 0);
        }
    } else if (arg2 == 8) {
        if (GameFlag_GetNibble(0x83) > 0) {
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(&D_shelter_r47_80186F94, 0, 0, 0);
        } else if (GameFlag_GetNibble(0x80) > 0) {
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(&D_shelter_r47_80186F94, 1, 0, 0);
        }
    }
    return 0;
}

void func_shelter_r47_80180324(Task* task)
{
    switch (task->state) {
        case 0:
            SndEvt_EnqueueType6(0x542F0010, 0, 0);
            task->spawnArg1     = gGameSession->at4.loc.view;
            task->killCountdown = gGameSession->eventState;
            task->state++;
            break;
        case 1:
            if (gGameSession->eventState != task->killCountdown) {
                if (gGameSession->eventState != 0) {
                    SndEvt_EnqueueType7(0x542F0010, 0x3C);
                } else {
                    switch (gGameSession->at4.loc.view) {
                        case 2:
                        case 3:
                            SndEvt_EnqueueType6(0x542F0010, 0, 0);
                            break;
                        case 4:
                            SndEvt_EnqueueType6(0x542F0010, 0xC, 0x58);
                            break;
                    }
                }
            } else {
                if (gGameSession->eventState == 0 && gGameSession->viewReady != 0) {
                    switch (gGameSession->at4.loc.view) {
                        case 2:
                        case 3:
                            SndEvt_EnqueueTypeA(0x542F0010, 0, 0);
                            break;
                        case 4:
                            if (task->spawnArg1 == 3) {
                                SndEvt_EnqueueTypeA(0x542F0010, 0xC, 0x58);
                            } else {
                                SndEvt_EnqueueType6(0x542F0010, 0xC, 0x58);
                            }
                            break;
                        case 5:
                            SndEvt_EnqueueType7(0x542F0010, 0x3C);
                            break;
                    }
                }
                task->spawnArg1 = gGameSession->at4.loc.view;
            }
            task->killCountdown = gGameSession->eventState;
            break;
    }
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_r47/shelter_r47", RoomsShared8017d8d0States);
