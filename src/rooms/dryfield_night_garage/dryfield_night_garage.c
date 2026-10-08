#include "rooms/dryfield_night_garage.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "dryfield_night_garage_private.h"

#include "gameplay/animation.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/sound.h"
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

/// The room's own `TaskMessageEntry[]` - the message table `_dryfieldNightGarageInitRoomTask`
/// publishes in `Task::msgTable`. It terminates with id `TASK_MESSAGE_TABLE_END`.
extern TaskMessageEntry D_dryfield_night_garage_80181C38[];

/// Ally animation descriptor handed to `companionWriteAnimationBankIndex`, then forwarded as the
/// payload of the 0x3E8 message.
extern AnimationPlayRequest D_dryfield_night_garage_80181C68;

/// Script blob passed to `evsStartScript` when game flag 0x8E is already set.
extern EvsCommand D_dryfield_night_garage_80181C7C[];

/// Two layout templates and the live copy the resets restore from them.
extern WorldCollisionGrid D_dryfield_night_garage_80181D7C;
extern WorldCollisionGrid D_dryfield_night_garage_80181E40;

/// The room's action triggers; `flags` bit 0x40 enables collision testing.
/// The room switches the enabled state of entries 3 and 5.

static void _dryfieldNightGarageResetTallCollisionBox(s32 useYOffset);

#define SHOP_CHARGE_TITLE_BYTES "Charge\0\xF0"
#include "../../shared/shop.h"

static s32 _dryfieldNightGarageHandleRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg);
static s32 _dryfieldNightGarageRefuseKeyItem(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg);
static s32 _roomVariantResolveDryfieldMsg(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32 _dryfieldNightGarageIgnoreCommand(Task* unusedTask, s32 unusedMessageId, s32 unusedCommand, s32 unusedSecondArg);

#include "../../shared/shop_data.inc.c"

#include "../../shared/shop_panels.inc.c"

TaskDesc D_dryfield_night_garage_80181C2C = { { { TASK_BODY_NONE, 192 } }, _shopSessionTask, { .value = 0 } };

TaskMessageEntry D_dryfield_night_garage_80181C38[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _roomVariantResolveDryfieldMsg },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldNightGarageRefuseKeyItem },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldNightGarageHandleRoomAction },
    { ROOM_MESSAGE_COMMAND, _dryfieldNightGarageIgnoreCommand },
    { ROOM_MESSAGE_SOUND, _garageSoundMsg },
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

static void _dryfieldNightGarageInitRoomTask(Task* task);
static void _dryfieldNightGarageIdleRoomTask(Task* unusedTask);

/// Extent of the replaceable box at the start of the larger live collision mesh.
enum {
    DRYFIELD_NIGHT_GARAGE_BOX_FACE_COUNT   = 4,
    DRYFIELD_NIGHT_GARAGE_BOX_VERTEX_COUNT = 8,
};

#include "../../shared/shop.inc.c"

#undef SHOP_CHARGE_TITLE_BYTES

/// Initializes room messages, progress-dependent action triggers and the companion scene.
///
/// State 0 publishes the room task, restores the companion's scene placement
/// in variant 3, and selects the refueling interaction after the event in
/// variant 2. The first companion visit restores HP and enables scene skipping;
/// later visits use a pose-only script. Advances to the idle state.
static void _dryfieldNightGarageInitRoomTask(Task* task)
{
    enum {
        DRYFIELD_NIGHT_GARAGE_VARIANT_REFUELING        = 2,
        DRYFIELD_NIGHT_GARAGE_VARIANT_COMPANION        = 3,
        DRYFIELD_NIGHT_GARAGE_TRIGGER_AFTER_REFUELING  = 3,
        DRYFIELD_NIGHT_GARAGE_TRIGGER_BEFORE_REFUELING = 5,
        DRYFIELD_NIGHT_GARAGE_PROGRESS_SCENE_STARTED   = 1,
        DRYFIELD_NIGHT_GARAGE_PROGRESS_REVISITED       = 2,
    };
    WorldCollisionTrigger* afterRefuelingTrigger;
    WorldCollisionTrigger* beforeRefuelingTrigger;
    Task*                  companion;

    task->msgTable = D_dryfield_night_garage_80181C38;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    (D_dryfield_night_garage_80186D7C + DRYFIELD_NIGHT_GARAGE_TRIGGER_AFTER_REFUELING)->flags &= (0xFF ^ WORLD_COLLISION_TRIGGER_ENABLED);
    companion                                                                                  = gameGetTaskSlot(GAME_TASK_SLOT_COMPANION);
    // Restore the companion before starting either the first-visit scene or its pose-only script.
    if (gGameSession->location.loc.variant == DRYFIELD_NIGHT_GARAGE_VARIANT_COMPANION && companion != NULL) {
        TASK_MESSAGE_DISPATCH_POINTER(companion, GAME_ACTOR_MESSAGE_PLACE, &D_actor_136300_8013B570, 0);
        companionWriteAnimationBankIndex(&D_dryfield_night_garage_80181C68.source.index);
        TASK_MESSAGE_DISPATCH_POINTER(companion, ANIMATION_MESSAGE_PLAY, &D_dryfield_night_garage_80181C68, 0);
        _dryfieldNightGarageResetTallCollisionBox(0);
        companionRemoveEquipment(companion);
        if (gameFlagGetNibble(GAME_FLAG_NIGHT_GARAGE_COMPANION_SCENE_SEEN) == 0) {
            companionRestoreFullHp();
            gameFlagSetNibble(GAME_FLAG_NIGHT_GARAGE_COMPANION_SCENE_SEEN, 1);
            evsStartScriptWithSkip(D_actor_136300_8013B590, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_actor_136300_8013C388);
        } else {
            evsStartScript(D_dryfield_night_garage_80181C7C, EVENT_SCRIPT_HUD_KEEP);
        }
    }
    // Re-entry keeps the post-event interaction and retires the original refueling hotspot.
    if (gGameSession->location.loc.variant == DRYFIELD_NIGHT_GARAGE_VARIANT_REFUELING && gameFlagGetNibble(GAME_FLAG_NIGHT_GARAGE_PROGRESS) > 0) {
        if (gameFlagGetNibble(GAME_FLAG_NIGHT_GARAGE_PROGRESS) == DRYFIELD_NIGHT_GARAGE_PROGRESS_SCENE_STARTED) {
            gameFlagSetNibble(GAME_FLAG_NIGHT_GARAGE_PROGRESS, DRYFIELD_NIGHT_GARAGE_PROGRESS_REVISITED);
        }
        afterRefuelingTrigger          = D_dryfield_night_garage_80186D7C + DRYFIELD_NIGHT_GARAGE_TRIGGER_AFTER_REFUELING;
        beforeRefuelingTrigger         = afterRefuelingTrigger + (DRYFIELD_NIGHT_GARAGE_TRIGGER_BEFORE_REFUELING - DRYFIELD_NIGHT_GARAGE_TRIGGER_AFTER_REFUELING);
        afterRefuelingTrigger->flags  |= WORLD_COLLISION_TRIGGER_ENABLED;
        beforeRefuelingTrigger->flags &= (0xFF ^ WORLD_COLLISION_TRIGGER_ENABLED);
    }
    task->state++;
}

/// Enables the refueled hotspot and disables the preceding refueling hotspot.
///
/// Requires the live room trigger table. Changes only the enabled bit of
/// entries 3 and 5; other trigger flags and story progress are retained.
static inline void _dryfieldNightGarageEnableRefueledTriggers(void)
{
    enum { AFTER_REFUELING_TRIGGER  = 3,
           BEFORE_REFUELING_TRIGGER = 5 };
    WorldCollisionTrigger* afterRefuelingTrigger;
    WorldCollisionTrigger* beforeRefuelingTrigger;

    afterRefuelingTrigger          = D_dryfield_night_garage_80186D7C + AFTER_REFUELING_TRIGGER;
    beforeRefuelingTrigger         = afterRefuelingTrigger + (BEFORE_REFUELING_TRIGGER - AFTER_REFUELING_TRIGGER);
    afterRefuelingTrigger->flags  |= WORLD_COLLISION_TRIGGER_ENABLED;
    beforeRefuelingTrigger->flags &= (0xFF ^ WORLD_COLLISION_TRIGGER_ENABLED);
}

/// Handles the garage's refueling gate, repeat dialogue and companion interaction.
///
/// Borrows `request` synchronously and returns zero. Action 6 in variant 2
/// checks the master key, container and gasoline before committing refueling,
/// consuming gasoline and starting the scene; later visits queue shop dialogue.
/// Action 1 selects CAP playback from its progress flag. Action 2 in variant 3
/// queues the live companion's interaction. Failed spawns leave player control
/// held and committed progress intact. Keep the room and actor overlays loaded
/// through the selected task or script.
static s32 _dryfieldNightGarageHandleRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    enum { ACTION_CAP_DIALOGUE              = 1,
           ACTION_COMPANION_INTERACTION     = 2,
           ACTION_REFUELING                 = 6,
           VARIANT_REFUELING                = 2,
           VARIANT_COMPANION                = 3,
           REFUELING_NOT_STARTED            = 0,
           REFUELING_STARTED                = 1,
           DIALOGUE_TASK_INDEX              = 0,
           SHOP_DIALOGUE_TASK_INDEX         = 1,
           CAP_MISSING_MASTER_KEY           = 6,
           CAP_MISSING_CONTAINER            = 7,
           CAP_MISSING_GASOLINE             = 8,
           CAP_FIRST_SHOP_DIALOGUE          = 10,
           CAP_LATER_SHOP_DIALOGUE          = 21,
           CAP_PROGRESS_SEQUENCE_SLOT       = 20,
           CAP_DEFAULT_DIALOGUE             = 54,
           COMPANION_INTERACTION_TASK_INDEX = 1,
           REFUELED_OBJECTIVE               = 23,
           REFUELED_SCENE_EVENT             = 5 };

    if (request->actionId == ACTION_REFUELING) {
        if (gGameSession->location.loc.variant == VARIANT_REFUELING) {
            if (gameFlagGetNibble(GAME_FLAG_NIGHT_GARAGE_PROGRESS) == REFUELING_NOT_STARTED) {
                if (inventoryHasCollectedBit(INVENTORY_COLLECTION_ID_BRONCO_MASTERKEY) == 0) {
                    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                    taskSpawnFromTable(D_dryfield_night_garage_80182C98, DIALOGUE_TASK_INDEX, CAP_MISSING_MASTER_KEY, 0);
                } else if (inventoryHasCollectedBit(INVENTORY_COLLECTION_ID_JERRY_CAN) == 0 && inventoryHasCollectedBit(INVENTORY_COLLECTION_ID_GASOLINE) == 0) {
                    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                    taskSpawnFromTable(D_dryfield_night_garage_80182C98, DIALOGUE_TASK_INDEX, CAP_MISSING_CONTAINER, 0);
                } else if (inventoryHasCollectedBit(INVENTORY_COLLECTION_ID_GASOLINE) == 0) {
                    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                    taskSpawnFromTable(D_dryfield_night_garage_80182C98, DIALOGUE_TASK_INDEX, CAP_MISSING_GASOLINE, 0);
                } else if (gameFlagGetNibble(GAME_FLAG_NIGHT_GARAGE_PROGRESS) == REFUELING_NOT_STARTED) {
                    // Commit the hotspots and saved progress before playback can fail.
                    _dryfieldNightGarageEnableRefueledTriggers();
                    evsStartScriptWithSkip(D_dryfield_night_garage_80182DF8, EVENT_SCRIPT_HUD_HIDE_RESTORE,
                                           D_dryfield_night_garage_801831B8);
                    gameFlagSetNibble(GAME_FLAG_NIGHT_GARAGE_PROGRESS, REFUELING_STARTED);
                    gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, REFUELED_OBJECTIVE);
                    inventoryClearCollectedBit(INVENTORY_COLLECTION_ID_GASOLINE);
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = REFUELED_SCENE_EVENT;
                }
            } else {
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                if (gameFlagGetNibble(GAME_FLAG_NIGHT_GARAGE_PROGRESS) == REFUELING_STARTED) {
                    taskSpawnFromTable(D_dryfield_night_garage_80182C98, SHOP_DIALOGUE_TASK_INDEX, CAP_FIRST_SHOP_DIALOGUE, 0);
                } else {
                    taskSpawnFromTable(D_dryfield_night_garage_80182C98, SHOP_DIALOGUE_TASK_INDEX, CAP_LATER_SHOP_DIALOGUE, 0);
                }
            }
        }
    }
    if (request->actionId == ACTION_CAP_DIALOGUE) {
        if (gameFlagGetNibble(GAME_FLAG_097) != 0) {
            capStartSequenceSlot(CAP_PROGRESS_SEQUENCE_SLOT, CAP_PLAYBACK_DISPLAY_TRANSITION, 0);
        } else {
            capSpawnEventIfIdle(CAP_DEFAULT_DIALOGUE, CAP_EVENT_NO_FLAGS);
        }
    }
    if (request->actionId == ACTION_COMPANION_INTERACTION && gGameSession->location.loc.variant == VARIANT_COMPANION && gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != NULL) {
        taskSpawnFromTable(&D_actor_136300_8013B11C, COMPANION_INTERACTION_TASK_INDEX, 0, 0);
    }
    return 0;
}

#include "../../shared/garage_sound_msg.inc.c"

/// Refuses every key-item-use request with the item menu's refused reply.
static s32 _dryfieldNightGarageRefuseKeyItem(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Resolves a night Dryfield room transition into an initialized reply.
///
/// Borrows a complete eight-byte request and writable reply, which may alias.
/// Copies the request before resolving the Junk Yard's progress-dependent room;
/// queries preserve it. Always returns 1. Requires the Dryfield map overlay.
static s32 _roomVariantResolveDryfieldMsg(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    *reply = *request;
    mapDryfieldFullResolveRoomVariant(request, reply);
    return 1;
}

/// Ignores room commands and their payloads, returning zero without side effects.
static s32 _dryfieldNightGarageIgnoreCommand(Task* unusedTask, s32 unusedMessageId, s32 unusedCommand, s32 unusedSecondArg)
{
    return 0;
}

/// Keeps the initialized room task alive for synchronous messages without per-frame work.
static void _dryfieldNightGarageIdleRoomTask(Task* unusedTask)
{
    // The binary reserves this unused stack storage; its original purpose is unproven.
    char unusedStackBytes[0x10];
}

/// State handlers of the room's message task `dryfieldNightGarageRoomTask`
/// runs: its set-up, an empty per-frame state and the kill.
static const TaskFuncTable3 D_dryfield_night_garage_8017D6FC = {
    {
        _dryfieldNightGarageInitRoomTask,
        _dryfieldNightGarageIdleRoomTask,
        taskKill,
    },
};

void dryfieldNightGarageRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_dryfield_night_garage_8017D6FC;
    stateHandlers.funcs[task->state](task);
}

/// Copies the replaceable collision box while preserving vector fourth components.
///
/// Both grids must provide four normals, four faces and eight vertices in
/// disjoint pools. Only XYZ and the complete face records are copied; descriptors,
/// cell membership and all later geometry remain with their owners.
static inline void _dryfieldNightGarageCopyCollisionBox(WorldCollisionGrid* liveGrid, const WorldCollisionGrid* boxTemplate)
{
    s32 faceIndex;

    // Replace only the reserved box; retain the room mesh and its cell membership.
    for (faceIndex = 0; faceIndex < DRYFIELD_NIGHT_GARAGE_BOX_FACE_COUNT; faceIndex++) {
        liveGrid->normals[faceIndex].vx          = boxTemplate->normals[faceIndex].vx;
        liveGrid->normals[faceIndex].vy          = boxTemplate->normals[faceIndex].vy;
        liveGrid->normals[faceIndex].vz          = boxTemplate->normals[faceIndex].vz;
        liveGrid->vertices[faceIndex * 2].vx     = boxTemplate->vertices[faceIndex * 2].vx;
        liveGrid->vertices[faceIndex * 2].vy     = boxTemplate->vertices[faceIndex * 2].vy;
        liveGrid->vertices[faceIndex * 2].vz     = boxTemplate->vertices[faceIndex * 2].vz;
        liveGrid->vertices[faceIndex * 2 + 1].vx = boxTemplate->vertices[faceIndex * 2 + 1].vx;
        liveGrid->vertices[faceIndex * 2 + 1].vy = boxTemplate->vertices[faceIndex * 2 + 1].vy;
        liveGrid->vertices[faceIndex * 2 + 1].vz = boxTemplate->vertices[faceIndex * 2 + 1].vz;
        liveGrid->faces[faceIndex]               = boxTemplate->faces[faceIndex];
    }
}

void dryfieldNightGaragePlaceLowCollisionBox(s32 useFarPosition)
{
    WorldCollisionGrid*       liveGrid;
    const WorldCollisionGrid* boxTemplate;
    SVECTOR                   offset;
    s32                       vertexIndex;

    liveGrid    = &D_dryfield_night_garage_80183DD4;
    boxTemplate = &D_dryfield_night_garage_80181D7C;

    _dryfieldNightGarageCopyCollisionBox(liveGrid, boxTemplate);

    if (useFarPosition == 0) {
        offset.vx = 4715;
        offset.vy = -132;
        offset.vz = 5900;
    } else {
        offset.vx = 4715;
        offset.vy = -132;
        offset.vz = 10000;
    }

    // Translate XYZ in room coordinates, narrowing each result back to a signed halfword.
    for (vertexIndex = 0; vertexIndex < DRYFIELD_NIGHT_GARAGE_BOX_VERTEX_COUNT; vertexIndex++) {
        liveGrid->vertices[vertexIndex].vx += offset.vx;
        liveGrid->vertices[vertexIndex].vy += offset.vy;
        liveGrid->vertices[vertexIndex].vz += offset.vz;
    }
}

/// Restores the tall collision box in the live mesh's reserved leading storage.
///
/// Replaces four faces, four normals and eight vertices from the room-space
/// template. Zero restores their original coordinates; nonzero adds 2000 game
/// units to Y. Does not replace the remaining mesh or cell lists. Requires the
/// room overlay and writable live pools; vector fourth components are preserved.
static void _dryfieldNightGarageResetTallCollisionBox(s32 useYOffset)
{
    WorldCollisionGrid*       liveGrid;
    const WorldCollisionGrid* boxTemplate;
    SVECTOR                   offset;
    s32                       vertexIndex;

    liveGrid    = &D_dryfield_night_garage_80183DD4;
    boxTemplate = &D_dryfield_night_garage_80181E40;

    _dryfieldNightGarageCopyCollisionBox(liveGrid, boxTemplate);

    if (useYOffset == 0) {
        offset.vx = 0;
        offset.vy = 0;
    } else {
        offset.vx = 0;
        offset.vy = 2000;
    }
    offset.vz = 0;

    // Translate XYZ in room coordinates, narrowing each result back to a signed halfword.
    for (vertexIndex = 0; vertexIndex < DRYFIELD_NIGHT_GARAGE_BOX_VERTEX_COUNT; vertexIndex++) {
        liveGrid->vertices[vertexIndex].vx += offset.vx;
        liveGrid->vertices[vertexIndex].vy += offset.vy;
        liveGrid->vertices[vertexIndex].vz += offset.vz;
    }
}

void dryfieldNightGarageShopDialogueTask(Task* task)
{
    enum { SHOP_DIALOGUE_START         = 0,
           SHOP_DIALOGUE_WAIT_AND_OPEN = 1,
           SHOP_DIALOGUE_REPLY         = 2,
           SHOP_DIALOGUE_WAIT_REPLY    = 3,
           CAP_FIRST_RESPONSE          = 1,
           SHOP_DIALOGUE_REPEAT_SEEN   = 1 };

    switch (task->state) {
        case SHOP_DIALOGUE_START:
            capStartSequenceSlot((s16)task->spawnArg1.value, CAP_PLAYBACK_IN_PLACE, 0);
            TASK_MESSAGE_DISPATCH_POINTER(dryfieldNightGarageFindPlacedActor(0), ACTOR_COMMAND_MESSAGE_APPLY, &D_dryfield_night_garage_80182DE0, 0);
            task->state++;
            return;
        case SHOP_DIALOGUE_WAIT_AND_OPEN:
            if (capIsBusy() == 0) {
                shopOpenSession(SHOP_STOCK_DRYFIELD);
                task->state++;
            }
            return;
        case SHOP_DIALOGUE_REPLY:
            // The queued shop session resumes this task at its follow-up dialogue.
            capStartSequenceSlot((s16)task->spawnArg1.value, CAP_PLAYBACK_IN_PLACE, (s16)(gameFlagGetNibble(GAME_FLAG_NIGHT_GARAGE_SCENE_REPEAT) + CAP_FIRST_RESPONSE));
            if (gameFlagGetNibble(GAME_FLAG_NIGHT_GARAGE_SCENE_REPEAT) == 0) {
                gameFlagSetNibble(GAME_FLAG_NIGHT_GARAGE_SCENE_REPEAT, SHOP_DIALOGUE_REPEAT_SEEN);
            }
            task->state++;
            return;
        case SHOP_DIALOGUE_WAIT_REPLY:
            if (capIsBusy() != 0) {
                break;
            }
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            TASK_MESSAGE_DISPATCH_POINTER(dryfieldNightGarageFindPlacedActor(0), ACTOR_COMMAND_MESSAGE_APPLY, &D_dryfield_night_garage_80182DE4, 0);
        default:
            taskKill(task);
            break;
    }
}

void dryfieldNightGarageStageSceneAudioStart(void)
{
    cdCmdStageSceneAudioStart();
}

void dryfieldNightGarageEnqueueScenePlayback(void)
{
    cdCmdEnqueueScenePlayback();
}

void dryfieldNightGarageFinishScene(void)
{
    streamFinishScene();
}

void dryfieldNightGarageCancelScene(void)
{
    cdCmdCancelScene();
}
