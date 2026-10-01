#include "rooms/dryfield_night_garage.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "dryfield_night_garage_private.h"

#include "gameplay/animation.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/inventory.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/player_state.h"
#include "gameplay/scene_runtime.h"

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

#include "mapui/map_dryfield_full.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

/// Shop stock uses gameplay's pickup/purchase quantity limits. Item ids
/// 0xA0–0xBF index `Gp_StackLimits[id - 0xA0]`.
typedef GpItemA0 RoomShopStock;

/// Task descriptor table and cutscene script blobs owned by the main
/// executable.
extern TaskDesc D_8013B11C[];
extern s32      D_8013B570;
extern s32      D_8013B590;
extern s32      D_8013C388;

/// The 0xFFFF-terminated item id lists `func_dryfield_night_garage_8017D754`
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

/// The shop's price ladder.
static RoomShopTier Shop_Data_80181950[13];

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
static UiListItemFunc Shop_Data_80181AD8[];
static UiList         Shop_Data_80181AE0;
static UiList         Shop_Data_80181B0C;
static UiObjectDesc   Shop_Data_80181B30;
static UiObjectDesc   Shop_Data_80181B4C;
static UiObjectDesc   Shop_Data_80181B68;
static UiObjectDesc   Shop_Data_80181B84;
static UiObjectDesc   Shop_Data_80181BA0;
static UiObjectDesc   Shop_Data_80181BD8;
static UiObjectDesc   Shop_Data_80181BF4;
static UiObjectDesc   Shop_Data_80181C10;

/// The room's own `GpMsgEntry[]` - the message table `func_dryfield_night_garage_8017FF2C`
/// publishes in `Task::msgTable`. It terminates with id 0x7FFFFFFF.
extern GpMsgEntry D_dryfield_night_garage_80181C38[];

/// Ally animation descriptor handed to `Gp_AllyAnimId`, then forwarded as the
/// payload of the 0x3E8 message.
extern AnimationPlayRequest D_dryfield_night_garage_80181C68;

/// Script blob passed to `func_800E8614` when game flag 0x8E is already set.
extern EvsCommand D_dryfield_night_garage_80181C7C[];

/// Two layout templates and the live copy the resets restore from them.
extern WorldCollisionGrid D_dryfield_night_garage_80181D7C;
extern WorldCollisionGrid D_dryfield_night_garage_80181E40;

/// The room's action triggers; `flags` bit 0x40 enables collision testing.
/// The room switches the enabled state of entries 3 and 5.

static void func_dryfield_night_garage_80180604(s32 arg0);

#define SHOP_CHARGE_TITLE_BYTES "Charge\0\xF0"
#include "../../shared/shop.h"

s32 func_dryfield_night_garage_801800C8(Task*, s32, DirectionActionRequest* msg, s32);
s32 func_dryfield_night_garage_80180300(Task*, s32, s32, TaskMessageArg);
s32 func_dryfield_night_garage_80180358(Task*, s32, TaskMessageArg, TaskMessageArg);
s32 func_dryfield_night_garage_80180360(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_dryfield_night_garage_801803A4(Task*, s32, TaskMessageArg, TaskMessageArg);

#include "../../shared/shop_data.inc.c"

#include "../../shared/shop_panels.inc.c"

TaskDesc D_dryfield_night_garage_80181C2C = { { { TASK_BODY_NONE, 192 } }, Shop_SessionTask, { .value = 0 } };

GpMsgEntry D_dryfield_night_garage_80181C38[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_dryfield_night_garage_80180360 },
    { 5105, func_dryfield_night_garage_80180358 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_dryfield_night_garage_801800C8 },
    { 5104, func_dryfield_night_garage_801803A4 },
    { 5106, func_dryfield_night_garage_80180300 },
    { 0x7FFFFFFF, NULL },
};

AnimationPlayRequest D_dryfield_night_garage_80181C68 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

EvsCommand D_dryfield_night_garage_80181C7C[4] = {
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = 10 }, { .value = 0 }, { .value = 1001 }, { .storage = &D_8013B570 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_garage_80181C68 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_END, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

SVECTOR D_dryfield_night_garage_80181CDC[4] = {
#include "assets/dryfield_night_garage_collision_047BC_normals.inc"
};

SVECTOR D_dryfield_night_garage_80181CFC[8] = {
#include "assets/dryfield_night_garage_collision_047BC_verts.inc"
};

WorldCollisionGridFace D_dryfield_night_garage_80181D3C[4] = {
#include "assets/dryfield_night_garage_collision_047BC_faces.inc"
};

s16 D_dryfield_night_garage_80181D6C[6] = {
#include "assets/dryfield_night_garage_collision_047BC_cells.inc"
};

#define GRID_CELL(i) (&D_dryfield_night_garage_80181D6C[i])
s16* D_dryfield_night_garage_80181D78[1] = {
#include "assets/dryfield_night_garage_collision_047BC_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_night_garage_80181D7C = { NULL, D_dryfield_night_garage_80181CDC, D_dryfield_night_garage_80181CFC, D_dryfield_night_garage_80181D3C, D_dryfield_night_garage_80181D78, 722, 250, 1, 1, 4000, 4 };

SVECTOR D_dryfield_night_garage_80181DA0[4] = {
#include "assets/dryfield_night_garage_collision_04880_normals.inc"
};

SVECTOR D_dryfield_night_garage_80181DC0[8] = {
#include "assets/dryfield_night_garage_collision_04880_verts.inc"
};

WorldCollisionGridFace D_dryfield_night_garage_80181E00[4] = {
#include "assets/dryfield_night_garage_collision_04880_faces.inc"
};

s16 D_dryfield_night_garage_80181E30[6] = {
#include "assets/dryfield_night_garage_collision_04880_cells.inc"
};

#define GRID_CELL(i) (&D_dryfield_night_garage_80181E30[i])
s16* D_dryfield_night_garage_80181E3C[1] = {
#include "assets/dryfield_night_garage_collision_04880_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_night_garage_80181E40 = { NULL, D_dryfield_night_garage_80181DA0, D_dryfield_night_garage_80181DC0, D_dryfield_night_garage_80181E00, D_dryfield_night_garage_80181E3C, -1900, -4890, 1, 1, 4000, 4 };

AnimationPackedPose D_dryfield_night_garage_80181E64[6] = {
#include "assets/dryfield_night_garage_animation_04B80_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_garage_80181EAC[46] = {
#include "assets/dryfield_night_garage_animation_04B80_bank4.inc"
};

AnimationRecord D_dryfield_night_garage_80181F64[109] = {
#include "assets/dryfield_night_garage_animation_04B80_records.inc"
};

u16 D_dryfield_night_garage_80182118[20] = {
#include "assets/dryfield_night_garage_animation_04B80_indices.inc"
};

AnimationSet D_dryfield_night_garage_80182140 = {
    D_dryfield_night_garage_80181F64,
    D_dryfield_night_garage_80182118,
    { NULL, D_dryfield_night_garage_80181E64, NULL, NULL, D_dryfield_night_garage_80181EAC, NULL, NULL, NULL },
};

AnimationPackedPose D_dryfield_night_garage_80182168[8] = {
#include "assets/dryfield_night_garage_animation_04F54_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_garage_801821C8[84] = {
#include "assets/dryfield_night_garage_animation_04F54_bank4.inc"
};

AnimationRecord D_dryfield_night_garage_80182318[117] = {
#include "assets/dryfield_night_garage_animation_04F54_records.inc"
};

u16 D_dryfield_night_garage_801824EC[20] = {
#include "assets/dryfield_night_garage_animation_04F54_indices.inc"
};

AnimationSet D_dryfield_night_garage_80182514 = {
    D_dryfield_night_garage_80182318,
    D_dryfield_night_garage_801824EC,
    { NULL, D_dryfield_night_garage_80182168, NULL, NULL, D_dryfield_night_garage_801821C8, NULL, NULL, NULL },
};

AnimationPackedPose D_dryfield_night_garage_8018253C[7] = {
#include "assets/dryfield_night_garage_animation_05344_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_garage_80182590[62] = {
#include "assets/dryfield_night_garage_animation_05344_bank4.inc"
};

AnimationRecord D_dryfield_night_garage_80182688[149] = {
#include "assets/dryfield_night_garage_animation_05344_records.inc"
};

u16 D_dryfield_night_garage_801828DC[20] = {
#include "assets/dryfield_night_garage_animation_05344_indices.inc"
};

AnimationSet D_dryfield_night_garage_80182904 = {
    D_dryfield_night_garage_80182688,
    D_dryfield_night_garage_801828DC,
    { NULL, D_dryfield_night_garage_8018253C, NULL, NULL, D_dryfield_night_garage_80182590, NULL, NULL, NULL },
};

AnimationPackedPose D_dryfield_night_garage_8018292C[7] = {
#include "assets/dryfield_night_garage_animation_056B0_bank1.inc"
};

AnimationPackedRotation D_dryfield_night_garage_80182980[74] = {
#include "assets/dryfield_night_garage_animation_056B0_bank4.inc"
};

AnimationRecord D_dryfield_night_garage_80182AA8[104] = {
#include "assets/dryfield_night_garage_animation_056B0_records.inc"
};

u16 D_dryfield_night_garage_80182C48[20] = {
#include "assets/dryfield_night_garage_animation_056B0_indices.inc"
};

AnimationSet D_dryfield_night_garage_80182C70 = {
    D_dryfield_night_garage_80182AA8,
    D_dryfield_night_garage_80182C48,
    { NULL, D_dryfield_night_garage_8018292C, NULL, NULL, D_dryfield_night_garage_80182980, NULL, NULL, NULL },
};

static inline s32 Shop_AddItemCount(s32 item, s32 count);
static void       func_dryfield_night_garage_8017FF2C(Task* task);
static void       func_dryfield_night_garage_801803AC(Task* task);

#include "../../shared/shop.inc.c"

#undef SHOP_CHARGE_TITLE_BYTES

/// State 0 of this room's message task, run when the garage scene starts.
/// Publishes the room's message table in `Task::msgTable` and the task itself
/// in pointer slot 7, disables action-trigger entry 3, then
/// hands off to the player actor through messages 0x3E9 / 0x3E8.
static void func_dryfield_night_garage_8017FF2C(Task* task)
{
    WorldCollisionTrigger* base;
    WorldCollisionTrigger* obj;
    Task*                  player;

    task->msgTable = D_dryfield_night_garage_80181C38;
    Game_SetPtrSlot(task, 7);
    (D_dryfield_night_garage_80186D7C + 3)->flags &= (0xFF ^ WORLD_COLLISION_TRIGGER_ENABLED);
    player                                         = gameGetTaskSlot(GAME_TASK_SLOT_COMPANION);
    if (gGameSession->location.loc.variant == 3 && player != NULL) {
        Gp_DispatchMsgPtr(player, 0x3E9, &D_8013B570, 0);
        Gp_AllyAnimId(&D_dryfield_night_garage_80181C68.source.index);
        Gp_DispatchMsgPtr(player, ANIMATION_MESSAGE_PLAY, &D_dryfield_night_garage_80181C68, 0);
        func_dryfield_night_garage_80180604(0);
        Gp_EndPlayerActorTask(player);
        if (GameFlag_GetNibble(0x8E) == 0) {
            Gp_FillAllyHp();
            GameFlag_SetNibble(0x8E, 1);
            func_800E8634(&D_8013B590, 0, &D_8013C388);
        } else {
            func_800E8614(D_dryfield_night_garage_80181C7C, 1);
        }
    }
    if (gGameSession->location.loc.variant == 2 && GameFlag_GetNibble(0x6C) > 0) {
        if (GameFlag_GetNibble(0x6C) == 1) {
            GameFlag_SetNibble(0x6C, 2);
        }
        base         = (D_dryfield_night_garage_80186D7C + 3);
        obj          = base + 2;
        base->flags |= WORLD_COLLISION_TRIGGER_ENABLED;
        obj->flags  &= (0xFF ^ WORLD_COLLISION_TRIGGER_ENABLED);
    }
    task->state = (s32)(task->state + 1);
}

s32 func_dryfield_night_garage_801800C8(Task* task, s32 msgId, DirectionActionRequest* msg, s32 arg3)
{
    WorldCollisionTrigger* base;
    WorldCollisionTrigger* obj;

    if (msg->actionId == 6) {
        if (gGameSession->location.loc.variant == 2) {
            if (GameFlag_GetNibble(0x6C) == 0) {
                if (Gp_HasCollectedBit(0x113) == 0) {
                    Gp_MsgPlayerWeapon(0);
                    Task_SpawnFromTable(D_dryfield_night_garage_80182C98, 0, 6, 0);
                } else if (Gp_HasCollectedBit(0x117) == 0 && Gp_HasCollectedBit(0x118) == 0) {
                    Gp_MsgPlayerWeapon(0);
                    Task_SpawnFromTable(D_dryfield_night_garage_80182C98, 0, 7, 0);
                } else if (Gp_HasCollectedBit(0x118) == 0) {
                    Gp_MsgPlayerWeapon(0);
                    Task_SpawnFromTable(D_dryfield_night_garage_80182C98, 0, 8, 0);
                } else if (GameFlag_GetNibble(0x6C) == 0) {
                    base         = (D_dryfield_night_garage_80186D7C + 3);
                    obj          = base + 2;
                    base->flags |= WORLD_COLLISION_TRIGGER_ENABLED;
                    obj->flags  &= (0xFF ^ WORLD_COLLISION_TRIGGER_ENABLED);
                    func_800E8634(D_dryfield_night_garage_80182DF8, 0,
                                  D_dryfield_night_garage_801831B8);
                    GameFlag_SetNibble(0x6C, 1);
                    func_800E3FAC(0xA2, 0x17);
                    Gp_ClearCollectedBit(0x118);
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = 5;
                }
            } else {
                Gp_MsgPlayerWeapon(0);
                if (GameFlag_GetNibble(0x6C) == 1) {
                    Task_SpawnFromTable(D_dryfield_night_garage_80182C98, 1, 0xA, 0);
                } else {
                    Task_SpawnFromTable(D_dryfield_night_garage_80182C98, 1, 0x15, 0);
                }
            }
        }
    }
    if (msg->actionId == 1) {
        if (GameFlag_GetNibble(0x97) != 0) {
            Gp_StartCapSlot(0x14, 1, 0);
        } else {
            Gp_SpawnIfCapIdle(0x36, 0);
        }
    }
    if (msg->actionId == 2 && gGameSession->location.loc.variant == 3 && gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != NULL) {
        Task_SpawnFromTable(D_8013B11C, 1, 0, 0);
    }
    return 0;
}

/// Room event callback: event 9 plays stage sound 0x52030009 and event 0x6C
/// reads the caption event key. Always returns 0.
s32 func_dryfield_night_garage_80180300(Task* arg0, s32 arg1, s32 arg2, TaskMessageArg arg3)
{
    switch (arg2) {
        case 0x9:
            Gp_EnqueueStageSnd6(0x52030009, 0, 0);
            break;
        case 0x6C:
            Gp_GetCapEventKey();
            break;
    }
    return 0;
}

s32 func_dryfield_night_garage_80180358(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

/// Message handler that copies the incoming record onto the outgoing one and
/// forwards both to `func_map_dryfield_full_80179954`. Always returns 1.
s32 func_dryfield_night_garage_80180360(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_map_dryfield_full_80179954(in, out);
    return 1;
}

s32 func_dryfield_night_garage_801803A4(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

/// The empty per-frame state of `D_dryfield_night_garage_8017D6FC`.
static void func_dryfield_night_garage_801803AC(Task* task)
{
    char pad[0x10];
}

/// State handlers of the room's message task `func_dryfield_night_garage_801803BC`
/// runs: its set-up, an empty per-frame state and the kill.
static const TaskFuncTable3 D_dryfield_night_garage_8017D6FC = {
    {
        func_dryfield_night_garage_8017FF2C,
        func_dryfield_night_garage_801803AC,
        taskKill,
    },
};

/// The room's message task: runs the handler for its state from a stack copy
/// of `D_dryfield_night_garage_8017D6FC`.
void func_dryfield_night_garage_801803BC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_garage_8017D6FC;
    sp.funcs[task->state](task);
}

/// Resets the live layout lists from the other template, the same way as the
/// reset below, then shifts the eight-entry list by (0x126B, -0x84, z) where z
/// is 0x170C when `arg0` is zero and 0x2710 otherwise.
void func_dryfield_night_garage_80180414(s32 arg0)
{
    WorldCollisionGrid* dst;
    WorldCollisionGrid* src;
    SVECTOR             d;
    s32                 i;

    dst = &D_dryfield_night_garage_80183DD4;
    src = &D_dryfield_night_garage_80181D7C;

    for (i = 0; i < 4; i++) {
        dst->normals[i].vx          = src->normals[i].vx;
        dst->normals[i].vy          = src->normals[i].vy;
        dst->normals[i].vz          = src->normals[i].vz;
        dst->vertices[i * 2].vx     = src->vertices[i * 2].vx;
        dst->vertices[i * 2].vy     = src->vertices[i * 2].vy;
        dst->vertices[i * 2].vz     = src->vertices[i * 2].vz;
        dst->vertices[i * 2 + 1].vx = src->vertices[i * 2 + 1].vx;
        dst->vertices[i * 2 + 1].vy = src->vertices[i * 2 + 1].vy;
        dst->vertices[i * 2 + 1].vz = src->vertices[i * 2 + 1].vz;
        dst->faces[i]               = src->faces[i];
    }

    if (arg0 == 0) {
        d.vx = 0x126B;
        d.vy = -0x84;
        d.vz = 0x170C;
    } else {
        d.vx = 0x126B;
        d.vy = -0x84;
        d.vz = 0x2710;
    }

    for (i = 0; i < 8; i++) {
        dst->vertices[i].vx += d.vx;
        dst->vertices[i].vy += d.vy;
        dst->vertices[i].vz += d.vz;
    }
}

/// Resets the live layout lists from the template: the four-entry vector list,
/// the eight-entry list two entries per pass, and the 12-byte records. The
/// eight-entry list is then raised by 0x7D0 on y when `arg0` is nonzero.
static void func_dryfield_night_garage_80180604(s32 arg0)
{
    WorldCollisionGrid* dst;
    WorldCollisionGrid* src;
    SVECTOR             d;
    s32                 i;

    dst = &D_dryfield_night_garage_80183DD4;
    src = &D_dryfield_night_garage_80181E40;

    for (i = 0; i < 4; i++) {
        dst->normals[i].vx          = src->normals[i].vx;
        dst->normals[i].vy          = src->normals[i].vy;
        dst->normals[i].vz          = src->normals[i].vz;
        dst->vertices[i * 2].vx     = src->vertices[i * 2].vx;
        dst->vertices[i * 2].vy     = src->vertices[i * 2].vy;
        dst->vertices[i * 2].vz     = src->vertices[i * 2].vz;
        dst->vertices[i * 2 + 1].vx = src->vertices[i * 2 + 1].vx;
        dst->vertices[i * 2 + 1].vy = src->vertices[i * 2 + 1].vy;
        dst->vertices[i * 2 + 1].vz = src->vertices[i * 2 + 1].vz;
        dst->faces[i]               = src->faces[i];
    }

    if (arg0 == 0) {
        d.vx = 0;
        d.vy = 0;
    } else {
        d.vx = 0;
        d.vy = 0x7D0;
    }
    d.vz = 0;

    for (i = 0; i < 8; i++) {
        dst->vertices[i].vx += d.vx;
        dst->vertices[i].vy += d.vy;
        dst->vertices[i].vz += d.vz;
    }
}

void func_dryfield_night_garage_801807E4(Task* arg0)
{
    s32 temp_v1;

    temp_v1 = arg0->state;
    switch (temp_v1) {
        case 0:
            Gp_StartCapSlot((s16)arg0->spawnArg1.value, 0, 0);
            Gp_DispatchMsgPtr(func_dryfield_night_garage_80180A64(0), 0x7DB, &D_dryfield_night_garage_80182DE0, 0);
            goto block_12;
        case 1:
            if (Gp_CapBusy() == 0) {
                func_800D4D2C(0x20);
                goto block_12;
            }
            return;
        case 2:
            Gp_StartCapSlot((s16)arg0->spawnArg1.value, 0, (s16)(GameFlag_GetNibble(0x107) + 1));
            if (GameFlag_GetNibble(0x107) == 0) {
                GameFlag_SetNibble(0x107, 1);
            }
        block_12:
            arg0->state = arg0->state + 1;
            return;
        case 3:
            if (Gp_CapBusy() != 0) {
                break;
            }
            Gp_MsgPlayerWeapon(1);
            Gp_DispatchMsgPtr(func_dryfield_night_garage_80180A64(0), 0x7DB, &D_dryfield_night_garage_80182DE4, 0);
        default:
            taskKill(arg0);
            break;
    }
}

/// Queues the replacement of overlay 0x82.
void func_dryfield_night_garage_80180924(void)
{
    CdCmd_EnqueueReplaceOverlay82();
}

/// Queues the load of overlay 0x81.
void func_dryfield_night_garage_80180944(void)
{
    CdCmd_EnqueueOverlay81();
}

/// Restores the stream random-number state.
void func_dryfield_night_garage_80180964(void)
{
    Gp_RestoreStreamRng();
}

/// Cancels the queued overlay replacement and restarts the CD queue.
void func_dryfield_night_garage_80180984(void)
{
    CdCmd_CancelReplaceAndActivate();
}
