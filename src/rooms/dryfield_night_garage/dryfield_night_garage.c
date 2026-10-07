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
#include "rooms/shop_tier.h"
#include "../../shared/garage.h"

/// Task descriptor table and cutscene script blobs owned by the main
/// executable.
extern TaskDesc       D_actor_136300_8013B11C;
extern ActorTransform D_actor_136300_8013B570;
extern EvsCommand     D_actor_136300_8013B590[];
extern EvsCommand     D_actor_136300_8013C388[];

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

/// The room's own `TaskMessageEntry[]` - the message table `func_dryfield_night_garage_8017FF2C`
/// publishes in `Task::msgTable`. It terminates with id `TASK_MESSAGE_TABLE_END`.
extern TaskMessageEntry D_dryfield_night_garage_80181C38[];

/// Ally animation descriptor handed to `Gp_AllyAnimId`, then forwarded as the
/// payload of the 0x3E8 message.
extern AnimationPlayRequest D_dryfield_night_garage_80181C68;

/// Script blob passed to `evsStartScript` when game flag 0x8E is already set.
extern EvsCommand D_dryfield_night_garage_80181C7C[];

/// Two layout templates and the live copy the resets restore from them.
extern WorldCollisionGrid D_dryfield_night_garage_80181D7C;
extern WorldCollisionGrid D_dryfield_night_garage_80181E40;

/// The room's action triggers; `flags` bit 0x40 enables collision testing.
/// The room switches the enabled state of entries 3 and 5.

static void func_dryfield_night_garage_80180604(s32 arg0);

#define SHOP_CHARGE_TITLE_BYTES "Charge\0\xF0"
#include "../../shared/shop.h"

s32 func_dryfield_night_garage_801800C8(Task* task, s32 msgId, const void* firstArg, s32);
s32 func_dryfield_night_garage_80180358(Task*, s32, s32, s32);
s32 func_dryfield_night_garage_80180360(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_dryfield_night_garage_801803A4(Task*, s32, s32, s32);

#include "../../shared/shop_data.inc.c"

#include "../../shared/shop_panels.inc.c"

TaskDesc D_dryfield_night_garage_80181C2C = { { { TASK_BODY_NONE, 192 } }, Shop_SessionTask, { .value = 0 } };

TaskMessageEntry D_dryfield_night_garage_80181C38[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_dryfield_night_garage_80180360 },
    { 5105, func_dryfield_night_garage_80180358 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_dryfield_night_garage_801800C8 },
    { ROOM_MESSAGE_COMMAND, func_dryfield_night_garage_801803A4 },
    { ROOM_MESSAGE_SOUND, garageSoundMsg },
    { TASK_MESSAGE_TABLE_END, NULL },
};

AnimationPlayRequest D_dryfield_night_garage_80181C68 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

EvsCommand D_dryfield_night_garage_80181C7C[4] = {
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_136300_8013B570 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_garage_80181C68 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

static SVECTOR _gDryfieldNightGarageCollision047BCNormals[4] = {
#include "assets/dryfield_night_garage_collision_047BC_normals.inc"
};

static SVECTOR _gDryfieldNightGarageCollision047BCVerts[8] = {
#include "assets/dryfield_night_garage_collision_047BC_verts.inc"
};

static WorldCollisionGridFace _gDryfieldNightGarageCollision047BCFaces[4] = {
#include "assets/dryfield_night_garage_collision_047BC_faces.inc"
};

static s16 _gDryfieldNightGarageCollision047BCCells[6] = {
#include "assets/dryfield_night_garage_collision_047BC_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldNightGarageCollision047BCCells[i])
static s16* _gDryfieldNightGarageCollision047BCTable[1] = {
#include "assets/dryfield_night_garage_collision_047BC_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_night_garage_80181D7C = { NULL, _gDryfieldNightGarageCollision047BCNormals, _gDryfieldNightGarageCollision047BCVerts, _gDryfieldNightGarageCollision047BCFaces, _gDryfieldNightGarageCollision047BCTable, 722, 250, 1, 1, 4000, 4 };

static SVECTOR _gDryfieldNightGarageCollision04880Normals[4] = {
#include "assets/dryfield_night_garage_collision_04880_normals.inc"
};

static SVECTOR _gDryfieldNightGarageCollision04880Verts[8] = {
#include "assets/dryfield_night_garage_collision_04880_verts.inc"
};

static WorldCollisionGridFace _gDryfieldNightGarageCollision04880Faces[4] = {
#include "assets/dryfield_night_garage_collision_04880_faces.inc"
};

static s16 _gDryfieldNightGarageCollision04880Cells[6] = {
#include "assets/dryfield_night_garage_collision_04880_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldNightGarageCollision04880Cells[i])
static s16* _gDryfieldNightGarageCollision04880Table[1] = {
#include "assets/dryfield_night_garage_collision_04880_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_night_garage_80181E40 = { NULL, _gDryfieldNightGarageCollision04880Normals, _gDryfieldNightGarageCollision04880Verts, _gDryfieldNightGarageCollision04880Faces, _gDryfieldNightGarageCollision04880Table, -1900, -4890, 1, 1, 4000, 4 };

static AnimationPackedPose _gDryfieldNightGarageAnimation04B80Bank1[6] = {
#include "assets/dryfield_night_garage_animation_04B80_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightGarageAnimation04B80Bank4[46] = {
#include "assets/dryfield_night_garage_animation_04B80_bank4.inc"
};

static AnimationRecord _gDryfieldNightGarageAnimation04B80Records[109] = {
#include "assets/dryfield_night_garage_animation_04B80_records.inc"
};

static u16 _gDryfieldNightGarageAnimation04B80Indices[20] = {
#include "assets/dryfield_night_garage_animation_04B80_indices.inc"
};

AnimationSet gDryfieldNightGarageAnimation04B80 = {
    _gDryfieldNightGarageAnimation04B80Records,
    _gDryfieldNightGarageAnimation04B80Indices,
    { NULL, _gDryfieldNightGarageAnimation04B80Bank1, NULL, NULL, _gDryfieldNightGarageAnimation04B80Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightGarageAnimation04F54Bank1[8] = {
#include "assets/dryfield_night_garage_animation_04F54_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightGarageAnimation04F54Bank4[84] = {
#include "assets/dryfield_night_garage_animation_04F54_bank4.inc"
};

static AnimationRecord _gDryfieldNightGarageAnimation04F54Records[117] = {
#include "assets/dryfield_night_garage_animation_04F54_records.inc"
};

static u16 _gDryfieldNightGarageAnimation04F54Indices[20] = {
#include "assets/dryfield_night_garage_animation_04F54_indices.inc"
};

AnimationSet gDryfieldNightGarageAnimation04F54 = {
    _gDryfieldNightGarageAnimation04F54Records,
    _gDryfieldNightGarageAnimation04F54Indices,
    { NULL, _gDryfieldNightGarageAnimation04F54Bank1, NULL, NULL, _gDryfieldNightGarageAnimation04F54Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightGarageAnimation05344Bank1[7] = {
#include "assets/dryfield_night_garage_animation_05344_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightGarageAnimation05344Bank4[62] = {
#include "assets/dryfield_night_garage_animation_05344_bank4.inc"
};

static AnimationRecord _gDryfieldNightGarageAnimation05344Records[149] = {
#include "assets/dryfield_night_garage_animation_05344_records.inc"
};

static u16 _gDryfieldNightGarageAnimation05344Indices[20] = {
#include "assets/dryfield_night_garage_animation_05344_indices.inc"
};

AnimationSet gDryfieldNightGarageAnimation05344 = {
    _gDryfieldNightGarageAnimation05344Records,
    _gDryfieldNightGarageAnimation05344Indices,
    { NULL, _gDryfieldNightGarageAnimation05344Bank1, NULL, NULL, _gDryfieldNightGarageAnimation05344Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightGarageAnimation056B0Bank1[7] = {
#include "assets/dryfield_night_garage_animation_056B0_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightGarageAnimation056B0Bank4[74] = {
#include "assets/dryfield_night_garage_animation_056B0_bank4.inc"
};

static AnimationRecord _gDryfieldNightGarageAnimation056B0Records[104] = {
#include "assets/dryfield_night_garage_animation_056B0_records.inc"
};

static u16 _gDryfieldNightGarageAnimation056B0Indices[20] = {
#include "assets/dryfield_night_garage_animation_056B0_indices.inc"
};

AnimationSet gDryfieldNightGarageAnimation056B0 = {
    _gDryfieldNightGarageAnimation056B0Records,
    _gDryfieldNightGarageAnimation056B0Indices,
    { NULL, _gDryfieldNightGarageAnimation056B0Bank1, NULL, NULL, _gDryfieldNightGarageAnimation056B0Bank4, NULL, NULL, NULL },
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
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    (D_dryfield_night_garage_80186D7C + 3)->flags &= (0xFF ^ WORLD_COLLISION_TRIGGER_ENABLED);
    player                                         = gameGetTaskSlot(GAME_TASK_SLOT_COMPANION);
    if (gGameSession->location.loc.variant == 3 && player != NULL) {
        TASK_MESSAGE_DISPATCH_POINTER(player, 0x3E9, &D_actor_136300_8013B570, 0);
        Gp_AllyAnimId(&D_dryfield_night_garage_80181C68.source.index);
        TASK_MESSAGE_DISPATCH_POINTER(player, ANIMATION_MESSAGE_PLAY, &D_dryfield_night_garage_80181C68, 0);
        func_dryfield_night_garage_80180604(0);
        Gp_EndPlayerActorTask(player);
        if (gameFlagGetNibble(GAME_FLAG_NIGHT_GARAGE_COMPANION_SCENE_SEEN) == 0) {
            Gp_FillAllyHp();
            gameFlagSetNibble(GAME_FLAG_NIGHT_GARAGE_COMPANION_SCENE_SEEN, 1);
            evsStartScriptWithSkip(D_actor_136300_8013B590, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_actor_136300_8013C388);
        } else {
            evsStartScript(D_dryfield_night_garage_80181C7C, EVENT_SCRIPT_HUD_KEEP);
        }
    }
    if (gGameSession->location.loc.variant == 2 && gameFlagGetNibble(GAME_FLAG_NIGHT_GARAGE_PROGRESS) > 0) {
        if (gameFlagGetNibble(GAME_FLAG_NIGHT_GARAGE_PROGRESS) == 1) {
            gameFlagSetNibble(GAME_FLAG_NIGHT_GARAGE_PROGRESS, 2);
        }
        base         = (D_dryfield_night_garage_80186D7C + 3);
        obj          = base + 2;
        base->flags |= WORLD_COLLISION_TRIGGER_ENABLED;
        obj->flags  &= (0xFF ^ WORLD_COLLISION_TRIGGER_ENABLED);
    }
    task->state = (s32)(task->state + 1);
}

s32 func_dryfield_night_garage_801800C8(Task* task, s32 msgId, const void* firstArg, s32 arg3)
{
    const DirectionActionRequest* msg = firstArg;

    WorldCollisionTrigger* base;
    WorldCollisionTrigger* obj;

    if (msg->actionId == 6) {
        if (gGameSession->location.loc.variant == 2) {
            if (gameFlagGetNibble(GAME_FLAG_NIGHT_GARAGE_PROGRESS) == 0) {
                if (inventoryHasCollectedBit(INVENTORY_COLLECTION_ID_BRONCO_MASTERKEY) == 0) {
                    Gp_MsgPlayerWeapon(0);
                    taskSpawnFromTable(D_dryfield_night_garage_80182C98, 0, 6, 0);
                } else if (inventoryHasCollectedBit(INVENTORY_COLLECTION_ID_JERRY_CAN) == 0 && inventoryHasCollectedBit(INVENTORY_COLLECTION_ID_GASOLINE) == 0) {
                    Gp_MsgPlayerWeapon(0);
                    taskSpawnFromTable(D_dryfield_night_garage_80182C98, 0, 7, 0);
                } else if (inventoryHasCollectedBit(INVENTORY_COLLECTION_ID_GASOLINE) == 0) {
                    Gp_MsgPlayerWeapon(0);
                    taskSpawnFromTable(D_dryfield_night_garage_80182C98, 0, 8, 0);
                } else if (gameFlagGetNibble(GAME_FLAG_NIGHT_GARAGE_PROGRESS) == 0) {
                    base         = (D_dryfield_night_garage_80186D7C + 3);
                    obj          = base + 2;
                    base->flags |= WORLD_COLLISION_TRIGGER_ENABLED;
                    obj->flags  &= (0xFF ^ WORLD_COLLISION_TRIGGER_ENABLED);
                    evsStartScriptWithSkip(D_dryfield_night_garage_80182DF8, EVENT_SCRIPT_HUD_HIDE_RESTORE,
                                           D_dryfield_night_garage_801831B8);
                    gameFlagSetNibble(GAME_FLAG_NIGHT_GARAGE_PROGRESS, 1);
                    func_800E3FAC(0xA2, 0x17);
                    inventoryClearCollectedBit(INVENTORY_COLLECTION_ID_GASOLINE);
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = 5;
                }
            } else {
                Gp_MsgPlayerWeapon(0);
                if (gameFlagGetNibble(GAME_FLAG_NIGHT_GARAGE_PROGRESS) == 1) {
                    taskSpawnFromTable(D_dryfield_night_garage_80182C98, 1, 0xA, 0);
                } else {
                    taskSpawnFromTable(D_dryfield_night_garage_80182C98, 1, 0x15, 0);
                }
            }
        }
    }
    if (msg->actionId == 1) {
        if (gameFlagGetNibble(GAME_FLAG_097) != 0) {
            capStartSequenceSlot(0x14, 1, 0);
        } else {
            Gp_SpawnIfCapIdle(0x36, 0);
        }
    }
    if (msg->actionId == 2 && gGameSession->location.loc.variant == 3 && gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != NULL) {
        taskSpawnFromTable(&D_actor_136300_8013B11C, 1, 0, 0);
    }
    return 0;
}

#include "../../shared/garage_sound_msg.inc.c"

s32 func_dryfield_night_garage_80180358(Task* task, s32 msgId, s32 arg2, s32 arg3)
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

s32 func_dryfield_night_garage_801803A4(Task* task, s32 msgId, s32 arg2, s32 arg3)
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
    switch (arg0->state) {
        case 0:
            capStartSequenceSlot((s16)arg0->spawnArg1.value, 0, 0);
            TASK_MESSAGE_DISPATCH_POINTER(func_dryfield_night_garage_80180A64(0), ACTOR_COMMAND_MESSAGE_APPLY, &D_dryfield_night_garage_80182DE0, 0);
            arg0->state++;
            return;
        case 1:
            if (capIsBusy() == 0) {
                func_800D4D2C(0x20);
                arg0->state++;
            }
            return;
        case 2:
            capStartSequenceSlot((s16)arg0->spawnArg1.value, 0, (s16)(gameFlagGetNibble(GAME_FLAG_NIGHT_GARAGE_SCENE_REPEAT) + 1));
            if (gameFlagGetNibble(GAME_FLAG_NIGHT_GARAGE_SCENE_REPEAT) == 0) {
                gameFlagSetNibble(GAME_FLAG_NIGHT_GARAGE_SCENE_REPEAT, 1);
            }
            arg0->state++;
            return;
        case 3:
            if (capIsBusy() != 0) {
                break;
            }
            Gp_MsgPlayerWeapon(1);
            TASK_MESSAGE_DISPATCH_POINTER(func_dryfield_night_garage_80180A64(0), ACTOR_COMMAND_MESSAGE_APPLY, &D_dryfield_night_garage_80182DE4, 0);
        default:
            taskKill(arg0);
            break;
    }
}

/// Queues the replacement of overlay 0x82.
void func_dryfield_night_garage_80180924(void)
{
    cdCmdStageSceneAudioStart();
}

/// Queues the load of overlay 0x81.
void func_dryfield_night_garage_80180944(void)
{
    cdCmdEnqueueScenePlayback();
}

/// Restores the stream random-number state.
void func_dryfield_night_garage_80180964(void)
{
    streamFinishScene();
}

/// Cancels the queued overlay replacement and restarts the CD queue.
void func_dryfield_night_garage_80180984(void)
{
    cdCmdCancelScene();
}
