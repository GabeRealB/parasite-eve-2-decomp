#include "rooms/shelter_b1_armory.h"

#include "types.h"

#include "shelter_b1_armory_private.h"

#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/inventory.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/scene_combat.h"
#include "gameplay/world_collision.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/ui_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "mapui/map_shelter.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "rooms/shop_tier.h"
// The flag symbol is four bytes; the gate writes the first.
#define ROOM_EVENT_ACTIVE gRoomEventActive.eventStarted
#include "../../shared/room_events.h"

/// The 0xFFFF-terminated item id lists `func_shelter_b1_armory_8017D768`
/// chooses from, and the one it returns when no case matches.
static u16 Shop_Data_801815F8[];
static u16 Shop_Data_80181600[];
static u16 Shop_Data_80181608[];
static u16 Shop_Data_80181610[];
static u16 Shop_Data_80181620[];
static u16 Shop_Data_80181630[];
static u16 Shop_Data_80181640[];
static u16 Shop_Data_80181648[];
static u16 Shop_Data_80181658[];
static u16 Shop_Data_80181668[];
static u16 Shop_Data_80181678[];
static u16 Shop_Data_80181680[];
static u16 Shop_Data_80181694[];
static u16 Shop_Data_801816AC[];
static u16 Shop_Data_801816C0[];
static u16 Shop_Data_801816C8[];
static u16 Shop_Data_801816D8[];
static u16 Shop_Data_801816F0[];
static u16 Shop_Data_80181704[];
static u16 Shop_Data_8018170C[];
static u16 Shop_Data_80181720[];
static u16 Shop_Data_8018173C[];
static u16 Shop_Data_8018174C[];
static u16 Shop_Data_80181758[];
static u16 Shop_Data_80181770[];
static u16 Shop_Data_8018178C[];
static u16 Shop_Data_801817A0[];
static u16 Shop_Data_801817A8[];
static u16 Shop_Data_801817BC[];
static u16 Shop_Data_801817DC[];
static u16 Shop_Data_801817EC[];
static u16 Shop_Data_801817F8[];
static u16 Shop_Data_80181810[];
static u16 Shop_Data_80181814[];
static u16 Shop_Data_80181818[];
static u16 Shop_Data_80181820[];
static u16 Shop_Data_80181830[];
static u16 Shop_Data_80181838[];
static u16 Shop_Data_80181840[];
static u16 Shop_Data_80181848[];
static u16 Shop_Data_80181854[];
static u16 Shop_Data_8018185C[];
static u16 Shop_Data_80181868[];
static u16 Shop_Data_80181870[];
static u16 Shop_Data_8018187C[];
static u16 Shop_Data_80181888[];
static u16 Shop_Data_80181890[];
static u16 Shop_Data_80181898[];
static u16 Shop_Data_801818A4[];
static u16 Shop_Data_801818B0[];
static u16 Shop_Data_801818B8[];
static u16 Shop_Data_801818C4[];
static u16 Shop_Data_801818D0[];
static u16 Shop_Data_801818DC[];
static u16 Shop_Data_801818E0[];
static u16 Shop_Data_801818EC[];
static u16 Shop_Data_801818F8[];
static u16 Shop_Data_80181904[];
static u16 Shop_Data_8018190C[];
static u16 Shop_Data_80181918[];
static u16 Shop_Data_80181924[];
static u16 Shop_Data_80181930[];
static u16 Shop_Data_80181938[];
static u16 Shop_Data_80181944[];
static u16 Shop_Data_80181AD4[];

/// The shop's tier ladder.
static ShopTier Shop_Data_80181950[SHOP_TIER_COUNT];

/// The item id the shop list's cursor last rested on.
static s32 Shop_Data_801819EC;

/// Messages and labels of the shop's panels.
static u8 Shop_Data_801819F0[];
static u8 Shop_Data_80181A04[];
static u8 Shop_Data_80181A0C[];
static u8 Shop_Data_80181A1C[];
static u8 Shop_Data_80181A20[];
static u8 Shop_Data_80181A5C[];
static u8 Shop_Data_80181A64[];
static u8 Shop_Data_80181A70[];
static u8 Shop_Data_80181A78[];
static u8 Shop_Data_80181A80[];
static u8 Shop_Data_80181A94[];
static u8 Shop_Data_80181AA4[];
static u8 Shop_Data_80181AC4[];
static u8 Shop_Data_80181AD0[];

/// Row handlers, lists and panel descriptors of the shop's panels.
static UiListRowCallback Shop_Data_80181AD8[];
static UiList            Shop_Data_80181AE0;
static UiList            Shop_Data_80181B0C;
static UiObjectDesc      Shop_Data_80181B30;
static UiObjectDesc      Shop_Data_80181B4C;
static UiObjectDesc      Shop_Data_80181B68;
static UiObjectDesc      Shop_Data_80181B84;
static UiObjectDesc      Shop_Data_80181BA0;
static UiObjectDesc      Shop_Data_80181BD8;
static UiObjectDesc      Shop_Data_80181BF4;
static UiObjectDesc      Shop_Data_80181C10;

/// Descriptor of the event task the door gate spawns, and the table the
/// room's own tasks are spawned from.
extern TaskDesc gRoomEventTaskDesc;
extern TaskDesc D_shelter_b1_armory_801824E8[];

/// Message handlers the room's controller task installs in pointer slot 7.
extern TaskMessageEntry D_shelter_b1_armory_80182500[];

/// The view index `func_shelter_b1_armory_8018034C` saves while it runs and
/// restores when it finishes.

static void func_shelter_b1_armory_80180740(Task* task);
static void func_shelter_b1_armory_80180784(Task* task);

#define SHOP_CHARGE_TITLE_BYTES "Charge\0\xD3"
#include "../../shared/shop.h"

s32 func_shelter_b1_armory_80180468(Task*, s32, s32, s32);
s32 func_shelter_b1_armory_801805A8(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_shelter_b1_armory_80180698(Task*, s32, s32, s32);
s32 func_shelter_b1_armory_801806F8(Task* task, s32 msgId, const void* firstArg, s32);

void func_shelter_b1_armory_80180214(Task*);
void func_shelter_b1_armory_8018034C(Task*);

#include "../../shared/shop_data.inc.c"

#include "../../shared/shop_panels.inc.c"

TaskDesc D_shelter_b1_armory_801824D0 = { { { TASK_BODY_NONE, 192 } }, Shop_SessionTask, { .value = 0 } };

TaskDesc gRoomEventTaskDesc = { { { TASK_BODY_NONE, 32 } }, roomEventTask, { .value = 0 } };

TaskDesc D_shelter_b1_armory_801824E8[2] = {
    { { { TASK_BODY_NONE, 192 } }, func_shelter_b1_armory_80180214, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_shelter_b1_armory_8018034C, { .value = 0 } },
};

TaskMessageEntry D_shelter_b1_armory_80182500[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_b1_armory_801805A8 },
    { 5105, func_shelter_b1_armory_80180468 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_shelter_b1_armory_801806F8 },
    { ROOM_MESSAGE_COMMAND, func_shelter_b1_armory_80180698 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

static inline s32 Shop_AddItemCount(s32 item, s32 count);

#include "../../shared/shop.inc.c"

#undef SHOP_CHARGE_TITLE_BYTES

#include "../../shared/room_event_gate.inc.c"

#include "../../shared/room_event_task.inc.c"

/// State handlers of the room's controller task: installing its message
/// table, an idle tick, and the kill.
static const TaskFuncTable3 D_shelter_b1_armory_8017D714 = {
    {
        func_shelter_b1_armory_80180740,
        func_shelter_b1_armory_80180784,
        taskKill,
    },
};

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
            if ((u16)task->spawnArg1.value == 1) {
                sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_ARMORY, 8), 0, 0);
            }
            if ((u16)task->spawnArg1.value == 2) {
                sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_B1_ARMORY, 9), 0, 0);
            }
            Gp_StartCapSlot(task->spawnArg1.value >> 16, 0, 0);
            task->state++;
            break;
        case 1:
        case 2:
            task->state++;
            break;
        case 4:
            if (Gp_CapBusy() == 0) {
                if ((u16)task->spawnArg1.value == 2) {
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
            gGameSession->eventState          = 1;
            gGameSession->hideHud             = 1;
            save                              = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
            view                              = save->state.location.loc.view;
            save->state.location.loc.view     = 0xD;
            D_shelter_b1_armory_8018557C.view = view;
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
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = D_shelter_b1_armory_8018557C.view;
        advance:
            task->state = task->state + 1;
            break;
    }
}

/// Answers 1 and spawns the armory task when a hit room-action trigger with
/// `parameter0` 0xFF exists and `arg2` is 0x105, 0x121 or 0x122. Event nibble
/// 0xF0 selects the task's parameter; on 0x105 a first visit also sets it.
s32 func_shelter_b1_armory_80180468(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    WorldCollisionTrigger* node;
    s32                    found;

    node = Gp_PendingObj4C;
    while (node != NULL) {
        if (node->control == WORLD_COLLISION_TRIGGER_ACTION_ROOM && node->parameter0 == WORLD_COLLISION_TRIGGER_ROOM_EVENT_ID && node->hit != 0) {
            found = 1;
            goto check;
        }
        node = node->next;
    }
    found = 0;
check:
    if (found != 0) {
        if (arg2 == 0x105) {
            gGameSession->eventState = 1;
            if (gameFlagGetNibble(GAME_FLAG_SHELTER_B1_ARMORY_UNLOCKED) != 0) {
                Task_SpawnOnDefaultList(D_shelter_b1_armory_801824E8, 0, 0x170003, 0);
            } else {
                Task_SpawnOnDefaultList(D_shelter_b1_armory_801824E8, 0, 0x180002, 0);
                gameFlagSetNibble(GAME_FLAG_SHELTER_B1_ARMORY_UNLOCKED, 1);
            }
            return 1;
        }
        if (arg2 == 0x121 || arg2 == 0x122) {
            gGameSession->eventState = 1;
            if (gameFlagGetNibble(GAME_FLAG_SHELTER_B1_ARMORY_UNLOCKED) != 0) {
                Task_SpawnOnDefaultList(D_shelter_b1_armory_801824E8, 0, 0x170003, 0);
            } else {
                Task_SpawnOnDefaultList(D_shelter_b1_armory_801824E8, 0, 0x190001, 0);
            }
            return 1;
        }
    }
    return 0;
}

s32 func_shelter_b1_armory_801805A8(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventReq req;

    *out = *in;
    func_map_shelter_80179A04(in, out);
    if (in->areaId == GAME_AREA_SHELTER_B1_STOREROOM) {
        req.capCmd        = 4;
        req.missingCapCmd = 1;
        req.firstSnd      = 0x540D0005;
        req.secondSnd     = 0x540D0001;
        req.flagId        = GAME_FLAG_B1_ARMORY_STOREROOM_DOOR_UNLOCKED;
        req.collectedBit  = 0;
        return roomEventGate(&req, out);
    }
    if (in->areaId != GAME_AREA_SHELTER_B1_ARMORY) {
        return 1;
    }
    if (gameFlagGetNibble(GAME_FLAG_SHELTER_B1_ARMORY_UNLOCKED) != 0) {
        return 1;
    }
    if (in->queryOnly == ROOM_EVENT_EXECUTE) {
        Gp_SetNibbleIf(in->flagId, 2);
        Gp_RunCapCmd1(0xD);
    }
    return 0;
}

s32 func_shelter_b1_armory_80180698(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 12:
            Gp_SpawnIfCapIdle(gameFlagGetNibble(GAME_FLAG_SHELTER_B1_ARMORY_UNLOCKED) == 0 ? 0xC : 0x17, 1);
            break;
        case 10:
            Gp_SpawnIfCapIdle(gameFlagGetNibble(GAME_FLAG_CONTROL_ROOM_RETURN_TAKEN) != 0 ? 0x10 : 0xA, 1);
            break;
    }
    return 0;
}

/// Handler for slot-7 msg `0x13EF`: the directed action (`actionId` 1) that
/// spawns the armory script.
s32 func_shelter_b1_armory_801806F8(Task* task, s32 msgId, const void* firstArg, s32 arg3)
{
    const DirectionActionRequest* request = firstArg;

    if (request->actionId == 1) {
        Gp_MsgPlayerWeapon(0);
        Task_SpawnFromTable(D_shelter_b1_armory_801824E8, 1, 0, 0);
    }
    return 0;
}

/// Installs the room's message table in pointer slot 7 and advances the task.
static void func_shelter_b1_armory_80180740(Task* task)
{
    task->msgTable = D_shelter_b1_armory_80182500;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = (s32)(task->state + 1);
}

/// The controller task's idle state: does nothing.
static void func_shelter_b1_armory_80180784(Task* task)
{
}

/// Runs the task's current state through its three-entry state table, copied
/// onto the stack before the call.
void func_shelter_b1_armory_8018078C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b1_armory_8017D714;
    sp.funcs[task->state](task);
}
