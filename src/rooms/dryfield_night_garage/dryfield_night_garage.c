#include "rooms/dryfield_night_garage.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"
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

#include "actors/actor_136300.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "rooms/shop_tier.h"
#include "../../shared/garage.h"

static const u16 _gShopStandardDefaultWeaponsStock[];
static const u16 _gShopStandardDefaultAmmunitionStock[];
static const u16 _gShopStandardDefaultArmorStock[];
static const u16 _gShopStandardDefaultItemsStock[];

static s32 _gShopPreviewRowId;

/// The room's action triggers; `flags` bit 0x40 enables collision testing.
/// The room switches the enabled state of entries 3 and 5.

static void _dryfieldNightGarageResetTallCollisionBox(s32 useYOffset);

/// Binds the shop's animated refill quantity to this room's writable s32 storage.
///
/// Defined in dryfield_night_garage_2.c and declared in the private header.
/// Uses eight fractional bits; keep the binding through shop.inc.c and the
/// storage alive through panel closure. Only one refill panel may use it.
#define SHOP_REFILL_DISPLAY_QUANTITY gShopRefillDisplayQuantityFixed

/// Binds the refill panel's borrowed catalog selection to this room's pointer storage.
///
/// Expands to the writable `const EquipmentWeaponSupply*` defined in
/// `dryfield_night_garage_2.c` and declared in the private header. Keep this binding
/// through `shop.inc.c`; `shop.h` specifies selection, lifetime and use by one
/// panel at a time.
#define SHOP_REFILL_SUPPLY gShopRefillSupply
#include "../../shared/room/shop.h"

static s32 _dryfieldNightGarageHandleRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedSecondArg);
static s32 _dryfieldNightGarageRefuseKeyItem(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg);
static s32 _roomVariantResolveDryfieldMsg(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32 _dryfieldNightGarageIgnoreCommand(Task* unusedTask, s32 unusedMessageId, s32 unusedCommand, s32 unusedSecondArg);

#include "../../shared/room/shop_data.inc.c"

enum {
    DRYFIELD_NIGHT_GARAGE_SHOP_TASK_PRIORITY  = 192,
    DRYFIELD_NIGHT_GARAGE_COMPANION_POSE_CLIP = 1,
};

TaskDesc gDryfieldNightGarageShopSessionTaskDesc = {
    .header   = { .fields = { TASK_BODY_NONE, DRYFIELD_NIGHT_GARAGE_SHOP_TASK_PRIORITY } },
    .callback = _shopSessionTask,
    .data     = { .value = 0 },
};

/// Synchronous messages accepted by the persistent night-garage room task.
///
/// Resolves departures, refuses key items, handles action triggers, ignores
/// commands and routes sound cues. The final marker makes other IDs return
/// zero. The task borrows this table and its callbacks while the room is loaded.
static TaskMessageEntry _gDryfieldNightGarageRoomMessageTable[] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _roomVariantResolveDryfieldMsg },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldNightGarageRefuseKeyItem },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldNightGarageHandleRoomAction },
    { ROOM_MESSAGE_COMMAND, _dryfieldNightGarageIgnoreCommand },
    { ROOM_MESSAGE_SOUND, _garageSoundMsg },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Resets the companion to garage pose clip 1 with world collision disabled.
///
/// Room setup overwrites the bank index from the live companion selection;
/// event playback resolves that selection again in a copy. Dispatch consumes
/// the request synchronously, while the selected animation bank stays borrowed.
static AnimationPlayRequest _gDryfieldNightGarageCompanionPoseRequest = {
    .source               = { .index = 1 },
    .animationId          = DRYFIELD_NIGHT_GARAGE_COMPANION_POSE_CLIP,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restores the companion's garage placement and pose on a later scene visit.
///
/// Four commands including END. The zero-frame wait still yields one task
/// update before placement and playback. Starts with HUD unchanged and borrows
/// the loaded garage, companion bank and actor resources owning the placement.
static EvsCommand _gDryfieldNightGarageCompanionRevisitScript[] = {
    { .opcode = EVENT_SCRIPT_OPCODE_WAIT_FRAMES, .operand0 = { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_SEND_MESSAGE, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = GAME_ACTOR_MESSAGE_PLACE }, .operand3 = { .message = { .transform = &gActor136300CompanionGaragePlacement } } },
    { .opcode = EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, .operand0 = { .value = GAME_TASK_SLOT_COMPANION }, .operand2 = { .value = ANIMATION_MESSAGE_PLAY }, .operand3 = { .animation = &_gDryfieldNightGarageCompanionPoseRequest } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

static SVECTOR _gDryfieldNightGarageLowCollisionBoxTemplateNormals[4] = {
#include "assets/dryfield_night_garage_collision_047BC_normals.inc"
};

static SVECTOR _gDryfieldNightGarageLowCollisionBoxTemplateVerts[8] = {
#include "assets/dryfield_night_garage_collision_047BC_verts.inc"
};

static WorldCollisionGridFace _gDryfieldNightGarageLowCollisionBoxTemplateFaces[4] = {
#include "assets/dryfield_night_garage_collision_047BC_faces.inc"
};

static s16 _gDryfieldNightGarageLowCollisionBoxTemplateCells[6] = {
#include "assets/dryfield_night_garage_collision_047BC_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// Captures `_gDryfieldNightGarageLowCollisionBoxTemplateCells`.
/// `halfwordOffset` is a constant s16-element offset in 0..5,
/// evaluated once and selecting a list's first entry. Lists end at
/// `WORLD_COLLISION_GRID_CELL_END`. Returns a borrowed `s16*`; no pool writes.
/// `cell_face_list_macro` selects this binding for the table include.
/// The binding is undefined immediately after that include.
#define DRYFIELD_NIGHT_GARAGE_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldNightGarageLowCollisionBoxTemplateCells[(halfwordOffset)])
static s16* _gDryfieldNightGarageLowCollisionBoxTemplateTable[1] = {
#include "assets/dryfield_night_garage_collision_047BC_table.inc"
};
#undef DRYFIELD_NIGHT_GARAGE_CELL_FACE_LIST_AT_OFFSET

/// Source geometry for the movable low box in the garage's live collision mesh.
///
/// Four vertical walls use four normals scaled by 4096 and eight vertices,
/// spanning X -722..1002 and Y/Z -250..250 in game units. Placement copies the faces and
/// vector XYZ into the live grid, then translates the box for refueling progress.
/// The retained one-cell index is unused by that copy; this grid is never activated.
static WorldCollisionGrid _gDryfieldNightGarageLowCollisionBoxTemplate = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldNightGarageLowCollisionBoxTemplateNormals,
    .vertices    = _gDryfieldNightGarageLowCollisionBoxTemplateVerts,
    .faces       = _gDryfieldNightGarageLowCollisionBoxTemplateFaces,
    .cellFaceIds = _gDryfieldNightGarageLowCollisionBoxTemplateTable,
    .xBias       = 722,
    .zBias       = 250,
    .cellCountX  = 1,
    .cellCountZ  = 1,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gDryfieldNightGarageLowCollisionBoxTemplateFaces),
};

static SVECTOR _gDryfieldNightGarageTallCollisionBoxTemplateNormals[4] = {
#include "assets/dryfield_night_garage_collision_04880_normals.inc"
};

static SVECTOR _gDryfieldNightGarageTallCollisionBoxTemplateVerts[8] = {
#include "assets/dryfield_night_garage_collision_04880_verts.inc"
};

static WorldCollisionGridFace _gDryfieldNightGarageTallCollisionBoxTemplateFaces[4] = {
#include "assets/dryfield_night_garage_collision_04880_faces.inc"
};

static s16 _gDryfieldNightGarageTallCollisionBoxTemplateCells[6] = {
#include "assets/dryfield_night_garage_collision_04880_cells.inc"
};

/// Supplies this grid's cell face-list addresses to its generated table.
///
/// Captures `_gDryfieldNightGarageTallCollisionBoxTemplateCells`.
/// `halfwordOffset` is a constant s16-element offset in 0..5,
/// evaluated once and selecting a list's first entry. Lists end at
/// `WORLD_COLLISION_GRID_CELL_END`. Returns a borrowed `s16*`; no pool writes.
/// `cell_face_list_macro` selects this binding for the table include.
/// The binding is undefined immediately after that include.
#define DRYFIELD_NIGHT_GARAGE_CELL_FACE_LIST_AT_OFFSET(halfwordOffset) (&_gDryfieldNightGarageTallCollisionBoxTemplateCells[(halfwordOffset)])
static s16* _gDryfieldNightGarageTallCollisionBoxTemplateTable[1] = {
#include "assets/dryfield_night_garage_collision_04880_table.inc"
};
#undef DRYFIELD_NIGHT_GARAGE_CELL_FACE_LIST_AT_OFFSET

/// Source geometry for the tall box around the companion's garage placement.
///
/// Four vertical walls use four normals scaled by 4096 and eight room-space vertices,
/// spanning X 1900..2560, Y -1726..33 and Z 4890..5590 in game units. Reset
/// copies the faces and vector XYZ into the live grid and optionally adds 2000
/// to Y. The retained one-cell index is unused; this grid is never activated.
static WorldCollisionGrid _gDryfieldNightGarageTallCollisionBoxTemplate = {
    .viewCoord   = NULL,
    .normals     = _gDryfieldNightGarageTallCollisionBoxTemplateNormals,
    .vertices    = _gDryfieldNightGarageTallCollisionBoxTemplateVerts,
    .faces       = _gDryfieldNightGarageTallCollisionBoxTemplateFaces,
    .cellFaceIds = _gDryfieldNightGarageTallCollisionBoxTemplateTable,
    .xBias       = -1900,
    .zBias       = -4890,
    .cellCountX  = 1,
    .cellCountZ  = 1,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gDryfieldNightGarageTallCollisionBoxTemplateFaces),
};

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

/// Retained byte following this room's weapon-supply refill title terminator.
///
/// One-byte string-literal binding for `shop.inc.c`; label drawing does not read it.
#define SHOP_WEAPON_SUPPLY_REFILL_TITLE_TRAILING_BYTES "\xF0"
#include "../../shared/room/shop.inc.c"
#undef SHOP_WEAPON_SUPPLY_REFILL_TITLE_TRAILING_BYTES
#undef SHOP_REFILL_DISPLAY_QUANTITY
#undef SHOP_REFILL_SUPPLY

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

    task->msgTable = _gDryfieldNightGarageRoomMessageTable;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    (gDryfieldNightGarageEarlyRoomActionTriggers + DRYFIELD_NIGHT_GARAGE_TRIGGER_AFTER_REFUELING)->flags &= (0xFF ^ WORLD_COLLISION_TRIGGER_ENABLED);
    companion                                                                                             = gameGetTaskSlot(GAME_TASK_SLOT_COMPANION);
    // Restore the companion before starting either the first-visit scene or its pose-only script.
    if (gGameSession->location.variant == DRYFIELD_NIGHT_GARAGE_VARIANT_COMPANION && companion != NULL) {
        TASK_MESSAGE_DISPATCH_POINTER(companion, GAME_ACTOR_MESSAGE_PLACE, &gActor136300CompanionGaragePlacement, 0);
        companionWriteAnimationBankIndex(&_gDryfieldNightGarageCompanionPoseRequest.source.index);
        TASK_MESSAGE_DISPATCH_POINTER(companion, ANIMATION_MESSAGE_PLAY, &_gDryfieldNightGarageCompanionPoseRequest, 0);
        _dryfieldNightGarageResetTallCollisionBox(0);
        companionRemoveEquipment(companion);
        if (gameFlagGetNibble(GAME_FLAG_NIGHT_GARAGE_COMPANION_SCENE_SEEN) == 0) {
            companionRestoreFullHp();
            gameFlagSetNibble(GAME_FLAG_NIGHT_GARAGE_COMPANION_SCENE_SEEN, 1);
            evsStartScriptWithSkip(gActor136300GarageCompanionSceneScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, gActor136300GarageCompanionSceneSkipScript);
        } else {
            evsStartScript(_gDryfieldNightGarageCompanionRevisitScript, EVENT_SCRIPT_HUD_KEEP);
        }
    }
    // Re-entry keeps the post-event interaction and retires the original refueling hotspot.
    if (gGameSession->location.variant == DRYFIELD_NIGHT_GARAGE_VARIANT_REFUELING && gameFlagGetNibble(GAME_FLAG_NIGHT_GARAGE_PROGRESS) > 0) {
        if (gameFlagGetNibble(GAME_FLAG_NIGHT_GARAGE_PROGRESS) == DRYFIELD_NIGHT_GARAGE_PROGRESS_SCENE_STARTED) {
            gameFlagSetNibble(GAME_FLAG_NIGHT_GARAGE_PROGRESS, DRYFIELD_NIGHT_GARAGE_PROGRESS_REVISITED);
        }
        afterRefuelingTrigger          = gDryfieldNightGarageEarlyRoomActionTriggers + DRYFIELD_NIGHT_GARAGE_TRIGGER_AFTER_REFUELING;
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

    afterRefuelingTrigger          = gDryfieldNightGarageEarlyRoomActionTriggers + AFTER_REFUELING_TRIGGER;
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
    enum { ACTION_CAP_DIALOGUE          = 1,
           ACTION_COMPANION_INTERACTION = 2,
           ACTION_REFUELING             = 6,
           VARIANT_REFUELING            = 2,
           VARIANT_COMPANION            = 3,
           REFUELING_NOT_STARTED        = 0,
           REFUELING_STARTED            = 1,
           DIALOGUE_TASK_INDEX          = 0,
           SHOP_DIALOGUE_TASK_INDEX     = 1,
           CAP_MISSING_MASTER_KEY       = 6,
           CAP_MISSING_CONTAINER        = 7,
           CAP_MISSING_GASOLINE         = 8,
           CAP_FIRST_SHOP_DIALOGUE      = 10,
           CAP_LATER_SHOP_DIALOGUE      = 21,
           CAP_PROGRESS_SEQUENCE_SLOT   = 20,
           CAP_DEFAULT_DIALOGUE         = 54,
           REFUELED_OBJECTIVE           = 23,
           REFUELED_SCENE_EVENT         = 5 };

    if (request->actionId == ACTION_REFUELING) {
        if (gGameSession->location.variant == VARIANT_REFUELING) {
            if (gameFlagGetNibble(GAME_FLAG_NIGHT_GARAGE_PROGRESS) == REFUELING_NOT_STARTED) {
                if (inventoryHasCollectedBit(INVENTORY_COLLECTION_ID_BRONCO_MASTERKEY) == 0) {
                    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                    taskSpawnFromTable(gDryfieldNightGarageDialogueTasks, DIALOGUE_TASK_INDEX, CAP_MISSING_MASTER_KEY, 0);
                } else if (inventoryHasCollectedBit(INVENTORY_COLLECTION_ID_JERRY_CAN) == 0 && inventoryHasCollectedBit(INVENTORY_COLLECTION_ID_GASOLINE) == 0) {
                    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                    taskSpawnFromTable(gDryfieldNightGarageDialogueTasks, DIALOGUE_TASK_INDEX, CAP_MISSING_CONTAINER, 0);
                } else if (inventoryHasCollectedBit(INVENTORY_COLLECTION_ID_GASOLINE) == 0) {
                    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                    taskSpawnFromTable(gDryfieldNightGarageDialogueTasks, DIALOGUE_TASK_INDEX, CAP_MISSING_GASOLINE, 0);
                } else if (gameFlagGetNibble(GAME_FLAG_NIGHT_GARAGE_PROGRESS) == REFUELING_NOT_STARTED) {
                    // Commit the hotspots and saved progress before playback can fail.
                    _dryfieldNightGarageEnableRefueledTriggers();
                    evsStartScriptWithSkip(gDryfieldNightGarageRefuelingScript, EVENT_SCRIPT_HUD_HIDE_RESTORE,
                                           gDryfieldNightGarageSkipRefuelingScript);
                    gameFlagSetNibble(GAME_FLAG_NIGHT_GARAGE_PROGRESS, REFUELING_STARTED);
                    gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, REFUELED_OBJECTIVE);
                    inventoryClearCollectedBit(INVENTORY_COLLECTION_ID_GASOLINE);
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = REFUELED_SCENE_EVENT;
                }
            } else {
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                if (gameFlagGetNibble(GAME_FLAG_NIGHT_GARAGE_PROGRESS) == REFUELING_STARTED) {
                    taskSpawnFromTable(gDryfieldNightGarageDialogueTasks, SHOP_DIALOGUE_TASK_INDEX, CAP_FIRST_SHOP_DIALOGUE, 0);
                } else {
                    taskSpawnFromTable(gDryfieldNightGarageDialogueTasks, SHOP_DIALOGUE_TASK_INDEX, CAP_LATER_SHOP_DIALOGUE, 0);
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
    if (request->actionId == ACTION_COMPANION_INTERACTION && gGameSession->location.variant == VARIANT_COMPANION && gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != NULL) {
        taskSpawnFromTable(gActor136300TaskTable, ACTOR_136300_TASK_GARAGE_DEPARTURE, 0, 0);
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

/// Initialize, idle and teardown states of the persistent garage room task.
///
/// Indexed by `Task::state` in 0..2 without a terminator or bounds check.
/// Dispatch copies all three callbacks by value; the overlay must stay loaded.
static const TaskFuncTable3 _gDryfieldNightGarageRoomTaskStates = {
    .funcs = {
        _dryfieldNightGarageInitRoomTask,
        _dryfieldNightGarageIdleRoomTask,
        taskKill,
    },
};

void dryfieldNightGarageRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gDryfieldNightGarageRoomTaskStates;
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

    liveGrid    = &gDryfieldNightGarageEarlyRoomCollisionGrid;
    boxTemplate = &_gDryfieldNightGarageLowCollisionBoxTemplate;

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

    liveGrid    = &gDryfieldNightGarageEarlyRoomCollisionGrid;
    boxTemplate = &_gDryfieldNightGarageTallCollisionBoxTemplate;

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
            TASK_MESSAGE_DISPATCH_POINTER(dryfieldNightGarageFindPlacedActor(0), ACTOR_COMMAND_MESSAGE_APPLY, &gDryfieldNightGarageGaryHeadTurnStartCommand, 0);
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
            TASK_MESSAGE_DISPATCH_POINTER(dryfieldNightGarageFindPlacedActor(0), ACTOR_COMMAND_MESSAGE_APPLY, &gDryfieldNightGarageGaryHeadTurnReleaseCommand, 0);
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
