#include "common.h"

#include "gameplay/3CD8.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/dryfield_dilapidated_house.h"

extern RoomMsgHandler D_dryfield_dilapidated_house_80183E8C[];
extern TaskDesc       D_dryfield_dilapidated_house_80183EB4;
extern Task*          D_dryfield_dilapidated_house_80189B78;
extern Task*          D_dryfield_dilapidated_house_80189B7C;
extern s16            D_dryfield_dilapidated_house_80189C98;

void func_dryfield_dilapidated_house_8017EAB4(Task* arg0)
{
    arg0->field_24 = D_dryfield_dilapidated_house_80183E8C;
    Game_SetPtrSlot(arg0, 7);
    if (Gp_LookupSlot4(1) != 0) {
        D_dryfield_dilapidated_house_80189B78 =
            Task_SpawnFromTable(&D_dryfield_dilapidated_house_80183EB4, 0, 0, 0);
    }
    D_dryfield_dilapidated_house_80189C98 = 2;
    D_dryfield_dilapidated_house_80189B7C =
        Task_SpawnFromTable(&D_dryfield_dilapidated_house_80183EB4, 2, 0, 0);
    Game_Session->field_69 = 0x83;
    arg0->state           += 1;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_3", func_dryfield_dilapidated_house_8017EB60);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_3", func_dryfield_dilapidated_house_8017EBB8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_3", func_dryfield_dilapidated_house_8017EE58);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_3", func_dryfield_dilapidated_house_8017F418);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_3", func_dryfield_dilapidated_house_8017F568);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_3", func_dryfield_dilapidated_house_8017FAD4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_3", func_dryfield_dilapidated_house_801803A4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_3", func_dryfield_dilapidated_house_80180738);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_3", func_dryfield_dilapidated_house_80180A0C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_3", func_dryfield_dilapidated_house_80180B84);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_3", func_dryfield_dilapidated_house_80180F04);

void func_dryfield_dilapidated_house_80180F5C(Task* arg0)
{
    DdhCoordWork* work;
    s32           temp_v0;

    work = (DdhCoordWork*)arg0->idMap;
    func_dryfield_dilapidated_house_801810F8((GameActorExt*)arg0->extra,
                                             (GameActorExt*)((Task*)arg0->spawnArg2)->extra);
    func_dryfield_dilapidated_house_80181028(arg0);
    temp_v0       = func_dryfield_dilapidated_house_80180FD8(arg0);
    work->field_0 = temp_v0;
    work->field_8 = temp_v0;
    work->field_4 = temp_v0;
}
