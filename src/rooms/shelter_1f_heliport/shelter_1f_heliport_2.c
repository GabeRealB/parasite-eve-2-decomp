#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include "rooms/room_common.h"

extern void func_80131FBC(void);
extern void func_80132038(void);
extern void func_80132110(void);
extern void func_801322A0(void);
extern void func_80149E38(void);
extern void func_80149EBC(void);

extern TaskDesc D_80136CDC;
extern TaskDesc D_shelter_1f_heliport_801811C8;

extern SVECTOR D_shelter_1f_heliport_80181204;
extern s16     D_shelter_1f_heliport_80181206; // D_shelter_1f_heliport_80181204.vy

void func_shelter_1f_heliport_8018085C(GsCOORDINATE2* coord, SVECTOR* offset);

INCLUDE_ASM("rooms/nonmatchings/shelter_1f_heliport/shelter_1f_heliport_2", func_shelter_1f_heliport_8017FF08);

INCLUDE_ASM("rooms/nonmatchings/shelter_1f_heliport/shelter_1f_heliport_2", func_shelter_1f_heliport_801800A0);

void func_shelter_1f_heliport_801802AC(s32 arg0)
{
    Task* task;
    Task* slotA;

    task  = gameGetPtrSlot(0xA);
    slotA = task;
    if (task == NULL) {
        task = gameGetPtrSlot(3);
    }
    if (slotA != NULL && GameFlag_GetNibble(0xE4) == 1) {
        D_shelter_1f_heliport_80181206 = 0;
    } else {
        D_shelter_1f_heliport_80181206 = 0x2710;
    }
    func_shelter_1f_heliport_8018085C(((TmdObject*)task->extra)->coords, &D_shelter_1f_heliport_80181204);
}

s32 func_shelter_1f_heliport_80180334(s32 arg0, s32 arg1, s32 arg2)
{
    GpObj4C* node;
    s32      found;

    if (arg2 == 0x124 && GameFlag_GetNibble(0xE4) == 1 && gameGetPtrSlot(0xA) != NULL) {
        found = 0;
        node  = Gp_PendingObj4C;
        while (node != NULL) {
            if (node->field_46 == 5 && node->field_48 == 0xFF && node->field_4B != 0) {
                found = 1;
                break;
            }
            node  = node->next;
            found = 0;
        }

        if (found != 0) {
            gGameSession->hideHud    = 1;
            gGameSession->eventState = 1;
            GameFlag_SetNibble(0x4B, 9);
            Task_SpawnOnDefaultList(&D_80136CDC, 0, 0, 0);
            return 1;
        }
    }
    return 0;
}

s32 func_shelter_1f_heliport_8018041C(s32 arg0, s32 arg1, s32 arg2)
{
    s32 need;

    switch (arg2) {
        case 0x21:
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(&D_shelter_1f_heliport_801811C8, 0, 0x21, 0);
            break;
        case 0x22:
            need = 1;
            if (gGameSession->at4.loc.place == 1) {
                need = 2;
            }
            Gp_SpawnIfCapIdle(GameFlag_GetNibble(0x104) >= need ? 0x22 : 0x25, 0);
            break;
    }
    return 0;
}

s32 func_shelter_1f_heliport_801804BC(s32 arg0, s32 arg1, RoomEventMsg* in)
{
    switch (in->field_2) {
        case 1:
            if (gGameSession->at4.loc.place == 1) {
                func_80149EBC();
            }
            if (gGameSession->at4.loc.place == 2) {
                func_80149E38();
            }
            break;
        case 2:
            func_80132038();
            break;
        case 3:
            func_80131FBC();
            break;
        case 4:
            func_80132110();
            break;
        case 5:
            func_801322A0();
            break;
    }
    return 0;
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_1f_heliport/shelter_1f_heliport_2", RoomsShared8017eb5cIdList);
