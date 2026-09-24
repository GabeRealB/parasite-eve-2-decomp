#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/268.h"
#include "gameplay/3688.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"

#include "main/display.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017d638.h"

extern s32 func_80179A04(RoomEventMsg* in, RoomEventMsg* out);

extern TaskDesc D_shelter_b1_armory_801824E8[];

extern u8 D_shelter_b1_armory_8018557C;

/// `Mc_SaveData.at4.loc.view`, spelled by address because the store below
/// relocates against this name.
extern u8 D_8007216C;

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_armory/shelter_b1_armory_2", RoomsShared8017d878Table);

void func_shelter_b1_armory_80180214(Task* task)
{
    switch (task->state) {
        case 0:
            Display_AcquireRef();
            D_80115768 = 1;
            task->state++;
            break;
        case 3:
            Display_ReleaseRef();
            D_80115768 = 0;
            Gp_MsgPlayerWeapon(0);
            if ((u16)task->spawnArg1 == 1) {
                SndEvt_EnqueueType6(0x540D0008, 0, 0);
            }
            if ((u16)task->spawnArg1 == 2) {
                SndEvt_EnqueueType6(0x540D0009, 0, 0);
            }
            Gp_StartCapSlot(task->spawnArg1 >> 16, 0, 0);
            task->state++;
            break;
        case 1:
        case 2:
            task->state++;
            break;
        case 4:
            if (Gp_CapBusy() == 0) {
                if ((u16)task->spawnArg1 == 2) {
                    Gp_SetItemSeenBit(0x105, 1);
                }
                Gp_MsgPlayerWeapon(1);
                gGameSession->eventState = 0;
                taskKill(task);
            }
            break;
    }
}

void func_shelter_b1_armory_8018034C(Task* task)
{
    McSaveData* save;
    u8          view;

    switch (task->state) {
        case 0:
            gGameSession->eventState     = 1;
            gGameSession->hideHud        = 1;
            save                         = &Mc_SaveData;
            view                         = save->at4.loc.view;
            save->at4.loc.view           = 0xD;
            D_shelter_b1_armory_8018557C = view;
            Gp_MsgPlayer3F3(0);
            Gp_RunCapCmd(0x16, 0);
            goto advance;
        case 1:
            if (Gp_CapBusy() != 0) {
                break;
            }
            func_800D4D2C(0x40);
            goto advance;
        case 2:
            task->state = 3;
        case 3:
            gGameSession->eventState = 0;
            gGameSession->hideHud    = 0;
            Gp_MsgPlayer3F3(1);
            Gp_MsgPlayerWeapon(1);
            D_8007216C = D_shelter_b1_armory_8018557C;
        advance:
            task->state = task->state + 1;
            break;
    }
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_armory/shelter_b1_armory_2", func_shelter_b1_armory_80180468);

s32 func_shelter_b1_armory_801805A8(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventReq req;

    *out = *in;
    func_80179A04(in, out);
    if (in->msgId == 0xB) {
        req.field_0 = 4;
        req.field_4 = 1;
        req.field_8 = 0x540D0005;
        req.field_C = 0x540D0001;
        req.flagId  = 0xA6;
        req.itemId  = 0;
        return RoomsShared8017d638(&req, out);
    }
    if (in->msgId != 0xD) {
        return 1;
    }
    if (GameFlag_GetNibble(0xF0) != 0) {
        return 1;
    }
    if (in->field_5 == 0) {
        Gp_SetNibbleIf(in->field_6, 2);
        Gp_RunCapCmd1(0xD);
    }
    return 0;
}

s32 func_shelter_b1_armory_80180698(s32 arg0, s32 arg1, s32 arg2)
{
    switch (arg2) {
        case 12:
            Gp_SpawnIfCapIdle(GameFlag_GetNibble(0xF0) == 0 ? 0xC : 0x17, 1);
            break;
        case 10:
            Gp_SpawnIfCapIdle(GameFlag_GetNibble(0xF7) != 0 ? 0x10 : 0xA, 1);
            break;
    }
    return 0;
}

/// Handler for slot-7 msg `0x13EF`: the directed action (`field_2` 1) that
/// spawns the armory script.
s32 func_shelter_b1_armory_801806F8(Task* task, s32 msgId, GpMsg13EF* arg2, s32 arg3)
{
    if (arg2->field_2 == 1) {
        Gp_MsgPlayerWeapon(0);
        Task_SpawnFromTable(D_shelter_b1_armory_801824E8, 1, 0, 0);
    }
    return 0;
}
