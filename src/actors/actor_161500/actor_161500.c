#include "actors/actor_161500.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "rooms/shelter_1f_heliport.h"
// The paced walk helpers this package carries run on the stride walker's block.
#define PACED_WALK_WORK_T StrideWalkWork
#include "../../shared/paced_walk.h"
#include "../../shared/walker.h"
#include "../../shared/stride_walk.h"

/// CAP file selectors, post-shop command indices and each variant's dialogue extent.
enum {
    ACTOR_161500_ALTERNATE_VARIANT           = 1,
    ACTOR_161500_TALKS_PER_VARIANT           = 4,
    ACTOR_161500_LAST_TALK                   = ACTOR_161500_TALKS_PER_VARIANT - 1,
    ACTOR_161500_CAPTION_RESET               = 0,
    ACTOR_161500_CAPTION_BY_VARIANT          = -1,
    ACTOR_161500_CAPTION_FILE_STANDARD       = 1,
    ACTOR_161500_CAPTION_FILE_ALTERNATE      = 2,
    ACTOR_161500_CAPTION_AFTER_PARKING_SHOP  = 29,
    ACTOR_161500_CAPTION_AFTER_HELIPORT_SHOP = 52,
};

/// Saved stages of the companion's item request.
enum {
    ACTOR_161500_REQUEST_UNSEEN   = 0,
    ACTOR_161500_REQUEST_PENDING  = 1,
    ACTOR_161500_REQUEST_COMPLETE = 2,
};

/// Stock selectors passed to the room's shop, with the initial category zero.
enum {
    ACTOR_161500_SHOP_STOCK_STANDARD        = 0x30,
    ACTOR_161500_SHOP_STOCK_ALTERNATE       = 0x31,
    ACTOR_161500_SHOP_STOCK_EVENT           = 0x32,
    ACTOR_161500_SHOP_STOCK_ALTERNATE_EVENT = 0x33,
};

/// Pending parking remarks and the visitor progress consumed by the departure remark.
enum {
    ACTOR_161500_REMARK_DEFAULT                      = 0,
    ACTOR_161500_REMARK_STERILIZATION_WITH_COMPANION = 1,
    ACTOR_161500_REMARK_STERILIZATION_ALONE          = 2,
    ACTOR_161500_REMARK_STERILIZATION_FOLLOWUP       = 3,
    ACTOR_161500_REMARK_VISITOR_DEPARTURE            = 4,
    ACTOR_161500_VISITOR_DEPARTED                    = 4,
    ACTOR_161500_VISITOR_REMARK_SEEN                 = 5,
};

extern TaskDesc      gStrideWalkTasks[];
extern AnimationSet* gStrideWalkAnimParams[12];
// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry gStrideWalkMessages[6];

extern EvsCommand*    D_actor_161500_80134920[8];
extern EvsCommand*    D_actor_161500_80135288[8];
extern EvsCommand     D_actor_161500_801352A8[];
extern EvsCommand     D_actor_161500_801354B8[];
extern EvsCommand     D_actor_161500_80135668[];
extern EvsCommand     D_actor_161500_801357E8[];
extern EvsCommand     D_actor_161500_80135968[];
extern EvsCommand     D_actor_161500_80135AE8[];
extern EvsCommand     D_actor_161500_80135C68[];
extern EvsCommand     D_actor_161500_80136E88[];
extern EvsCommand     D_actor_161500_80137080[];
extern EvsCommand     D_actor_161500_80137650[];
extern ActorTransform D_actor_161500_801376E0;
extern EvsCommand     D_actor_161500_801376F8[];
extern EvsCommand     D_actor_161500_801378D8[];
extern EvsCommand     D_actor_161500_80137AB8[];

extern AnimationPlayRequest     D_actor_161500_80133F7C;
extern AnimationPlayRequest     D_actor_161500_80134020;
extern AnimationPlayRequest     D_actor_161500_80134034;
extern ActorCommand             D_actor_161500_80133F74;
extern ActorCommand             D_actor_161500_80133F78;
extern AnimationBankCopyRequest D_actor_161500_80134048;
static void                     _actor161500SelectCaptionFile(s32 captionFile);
static void                     _actor161500OpenHeliportShop(void);
static void                     _actor161500RunCaptionCommand(s32 commandId);
static void                     _actor161500OpenParkingShop(void);

static void _actor161500CompleteCompanionRequestTask(Task* task);

static AnimationSet _gActor161500Animation04304;
static AnimationSet _gActor161500Animation04518;
static AnimationSet _gActor161500Animation047F8;
static AnimationSet _gActor161500Animation04A88;
static AnimationSet _gActor161500Animation04C60;
static AnimationSet _gActor161500Animation04E94;

extern ActorTransform D_actor_161500_80136CE8;
extern ActorTransform D_actor_161500_80136D00;
extern ActorTransform D_actor_161500_80136D18;
extern ActorTransform D_actor_161500_80136D30;
extern ActorTransform D_actor_161500_80136D48;

extern AnimationPlayRequest     D_actor_161500_80136D90;
extern AnimationPlayRequest     D_actor_161500_80136DA4;
extern AnimationPlayRequest     D_actor_161500_80136DB8;
extern AnimationPlayRequest     D_actor_161500_80136E38;
extern AnimationPlayRequest     D_actor_161500_80136E60;
extern AnimationPlayRequest     D_actor_161500_80136E74;
extern AnimationBankCopyRequest D_actor_161500_80136E08;
static s32                      _strideWalkSetTurnMode(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArgument);
static void                     _actor161500PreparePlayerFacingCompanion(void);
static void                     _actor161500SetPlayerUpdateHold(u8 holdPlayerUpdate);
static void                     _actor161500StrideWalkerTask(Task* task);

static AnimationPackedPose _gActor161500Animation010D0Bank1[3] = {
#include "assets/actor_161500_animation_010D0_bank1.inc"
};

static AnimationPackedRotation _gActor161500Animation010D0Bank4[36] = {
#include "assets/actor_161500_animation_010D0_bank4.inc"
};

static AnimationRecord _gActor161500Animation010D0Records[80] = {
#include "assets/actor_161500_animation_010D0_records.inc"
};

static u16 _gActor161500Animation010D0Indices[20] = {
#include "assets/actor_161500_animation_010D0_indices.inc"
};

static AnimationSet _gActor161500Animation010D0 = {
    _gActor161500Animation010D0Records,
    _gActor161500Animation010D0Indices,
    { NULL, _gActor161500Animation010D0Bank1, NULL, NULL, _gActor161500Animation010D0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor161500Animation0129CBank1[3] = {
#include "assets/actor_161500_animation_0129C_bank1.inc"
};

static AnimationPackedRotation _gActor161500Animation0129CBank4[29] = {
#include "assets/actor_161500_animation_0129C_bank4.inc"
};

static AnimationRecord _gActor161500Animation0129CRecords[57] = {
#include "assets/actor_161500_animation_0129C_records.inc"
};

static u16 _gActor161500Animation0129CIndices[20] = {
#include "assets/actor_161500_animation_0129C_indices.inc"
};

static AnimationSet _gActor161500Animation0129C = {
    _gActor161500Animation0129CRecords,
    _gActor161500Animation0129CIndices,
    { NULL, _gActor161500Animation0129CBank1, NULL, NULL, _gActor161500Animation0129CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor161500Animation016C4Bank1[5] = {
#include "assets/actor_161500_animation_016C4_bank1.inc"
};

static AnimationPackedRotation _gActor161500Animation016C4Bank4[90] = {
#include "assets/actor_161500_animation_016C4_bank4.inc"
};

static AnimationRecord _gActor161500Animation016C4Records[141] = {
#include "assets/actor_161500_animation_016C4_records.inc"
};

static u16 _gActor161500Animation016C4Indices[20] = {
#include "assets/actor_161500_animation_016C4_indices.inc"
};

static AnimationSet _gActor161500Animation016C4 = {
    _gActor161500Animation016C4Records,
    _gActor161500Animation016C4Indices,
    { NULL, _gActor161500Animation016C4Bank1, NULL, NULL, _gActor161500Animation016C4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor161500Animation01A48Bank1[6] = {
#include "assets/actor_161500_animation_01A48_bank1.inc"
};

static AnimationPackedRotation _gActor161500Animation01A48Bank4[69] = {
#include "assets/actor_161500_animation_01A48_bank4.inc"
};

static AnimationRecord _gActor161500Animation01A48Records[118] = {
#include "assets/actor_161500_animation_01A48_records.inc"
};

static u16 _gActor161500Animation01A48Indices[20] = {
#include "assets/actor_161500_animation_01A48_indices.inc"
};

static AnimationSet _gActor161500Animation01A48 = {
    _gActor161500Animation01A48Records,
    _gActor161500Animation01A48Indices,
    { NULL, _gActor161500Animation01A48Bank1, NULL, NULL, _gActor161500Animation01A48Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor161500Animation01D74Bank1[7] = {
#include "assets/actor_161500_animation_01D74_bank1.inc"
};

static AnimationPackedRotation _gActor161500Animation01D74Bank4[56] = {
#include "assets/actor_161500_animation_01D74_bank4.inc"
};

static AnimationRecord _gActor161500Animation01D74Records[106] = {
#include "assets/actor_161500_animation_01D74_records.inc"
};

static u16 _gActor161500Animation01D74Indices[20] = {
#include "assets/actor_161500_animation_01D74_indices.inc"
};

static AnimationSet _gActor161500Animation01D74 = {
    _gActor161500Animation01D74Records,
    _gActor161500Animation01D74Indices,
    { NULL, _gActor161500Animation01D74Bank1, NULL, NULL, _gActor161500Animation01D74Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor161500Animation0212CBank1[3] = {
#include "assets/actor_161500_animation_0212C_bank1.inc"
};

static AnimationPackedRotation _gActor161500Animation0212CBank4[81] = {
#include "assets/actor_161500_animation_0212C_bank4.inc"
};

static AnimationRecord _gActor161500Animation0212CRecords[128] = {
#include "assets/actor_161500_animation_0212C_records.inc"
};

static u16 _gActor161500Animation0212CIndices[20] = {
#include "assets/actor_161500_animation_0212C_indices.inc"
};

static AnimationSet _gActor161500Animation0212C = {
    _gActor161500Animation0212CRecords,
    _gActor161500Animation0212CIndices,
    { NULL, _gActor161500Animation0212CBank1, NULL, NULL, _gActor161500Animation0212CBank4, NULL, NULL, NULL },
};

ActorCommand D_actor_161500_80133F74 = { { .loc = { 5, 4 } }, 1 };

ActorCommand D_actor_161500_80133F78 = { { .loc = { 5, 4 } }, 0 };

AnimationPlayRequest D_actor_161500_80133F7C = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Player clips for extended ids 47-52, added by the soldier's talk scenes.
///
/// Every event script the soldier's talk and remark handlers start sends the
/// player its copy request before it plays any of these clips.
/// `D_actor_161500_80134048` copies `ANIMATION_BANK_EXTENSION_CAPACITY` (32)
/// words starting here into the player's bank, which is 26 words past the end
/// of this array: the read runs on through `D_actor_161500_80133FA8`,
/// `D_actor_161500_80133FBC`, `D_actor_161500_80133FD0`,
/// `D_actor_161500_80133FE4`, `D_actor_161500_80133FF8` and the first word of
/// `D_actor_161500_8013400C`. That overrun is the original's and is kept as it
/// is: the request carries the bank's fixed capacity, while the table was
/// stored with only its own entries. The requests select ids 47-52 only, so
/// none of the words installed after the six clips is played as one.
AnimationSet* D_actor_161500_80133F90[6] = { &_gActor161500Animation010D0, &_gActor161500Animation0129C, &_gActor161500Animation016C4, &_gActor161500Animation01A48, &_gActor161500Animation01D74, &_gActor161500Animation0212C };

// One request per clip, in id order, starts here; the talk scripts play them on the player.
AnimationPlayRequest D_actor_161500_80133FA8 = { { .index = 1 }, 47, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_161500_80133FBC = { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_161500_80133FD0 = { { .index = 1 }, 49, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_161500_80133FE4 = { { .index = 1 }, 50, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_161500_80133FF8 = { { .index = 1 }, 51, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_161500_8013400C = { { .index = 1 }, 52, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_161500_80134020 = { { .index = 0 }, 3, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_161500_80134034 = { { .index = 0 }, 11, ANIMATION_BLEND_INTERPOLATE, 4, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationBankCopyRequest D_actor_161500_80134048 = { { .sets = D_actor_161500_80133F90 }, ANIMATION_BANK_EXTENSION_CAPACITY };

EvsCommand D_actor_161500_80134050[13] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_FILE_STANDARD }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 10 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_161500_80134048 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133FD0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_RESET }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_161500_80134188[11] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_FILE_STANDARD }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 11 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_161500_80134048 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_RESET }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_161500_80134290[11] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_FILE_STANDARD }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 12 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_161500_80134048 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_RESET }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_161500_80134398[11] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_FILE_STANDARD }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 13 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_161500_80134048 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_RESET }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_161500_801344A0[15] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_FILE_ALTERNATE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 34 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_161500_80134048 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133FD0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_8013400C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_RESET }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_161500_80134608[11] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_FILE_ALTERNATE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 35 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_161500_80134048 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_RESET }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_161500_80134710[11] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_FILE_ALTERNATE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 36 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_161500_80134048 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_RESET }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_161500_80134818[11] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_FILE_ALTERNATE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 37 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_161500_80134048 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_RESET }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand* D_actor_161500_80134920[8] = {
    D_actor_161500_80134050,
    D_actor_161500_80134188,
    D_actor_161500_80134290,
    D_actor_161500_80134398,
    D_actor_161500_801344A0,
    D_actor_161500_80134608,
    D_actor_161500_80134710,
    D_actor_161500_80134818,
};

EvsCommand D_actor_161500_80134940[15] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_FILE_STANDARD }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 14 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_161500_80134048 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133FE4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133FA8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133FBC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_RESET }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_161500_80134AA8[12] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_FILE_STANDARD }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 15 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_161500_80134048 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133FE4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_RESET }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_161500_80134BC8[13] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_FILE_STANDARD }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 16 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_161500_80134048 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133FE4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_RESET }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_161500_80134D00[11] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_FILE_STANDARD }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 17 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_161500_80134048 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_RESET }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_161500_80134E08[12] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_FILE_ALTERNATE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 38 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_161500_80134048 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133FE4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_RESET }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_161500_80134F28[12] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_FILE_ALTERNATE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 39 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_161500_80134048 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133FE4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_RESET }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_161500_80135048[13] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_FILE_ALTERNATE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 40 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_161500_80134048 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133FF8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_RESET }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_161500_80135180[11] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_FILE_ALTERNATE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 41 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_161500_80134048 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_RESET }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand* D_actor_161500_80135288[8] = {
    D_actor_161500_80134940,
    D_actor_161500_80134AA8,
    D_actor_161500_80134BC8,
    D_actor_161500_80134D00,
    D_actor_161500_80134E08,
    D_actor_161500_80134F28,
    D_actor_161500_80135048,
    D_actor_161500_80135180,
};

EvsCommand D_actor_161500_801352A8[22] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_BY_VARIANT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 50 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_161500_80134048 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 3 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 3 }, { .value = 2003 }, { .message = { .pointer = &D_actor_161500_80134020 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133FE4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 3 }, { .value = 2003 }, { .message = { .pointer = &D_actor_161500_80134020 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor161500OpenHeliportShop }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500RunCaptionCommand }, { .value = ACTOR_161500_CAPTION_AFTER_HELIPORT_SHOP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 3 }, { .value = 2003 }, { .message = { .pointer = &D_actor_161500_80134020 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 3 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_RESET }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_161500_801354B8[18] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_BY_VARIANT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 51 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_161500_80134048 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 3 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 3 }, { .value = 2003 }, { .message = { .pointer = &D_actor_161500_80134020 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor161500OpenHeliportShop }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500RunCaptionCommand }, { .value = ACTOR_161500_CAPTION_AFTER_HELIPORT_SHOP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 3 }, { .value = 2003 }, { .message = { .pointer = &D_actor_161500_80134020 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 3 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500SelectCaptionFile }, { .value = ACTOR_161500_CAPTION_RESET }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_161500_80135668[16] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 24 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_161500_80134048 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_161500_80134020 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor161500OpenParkingShop }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500RunCaptionCommand }, { .value = ACTOR_161500_CAPTION_AFTER_PARKING_SHOP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_161500_80134020 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_161500_801357E8[16] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 25 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_161500_80134048 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_161500_80134020 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_161500_80134034 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor161500OpenParkingShop }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500RunCaptionCommand }, { .value = ACTOR_161500_CAPTION_AFTER_PARKING_SHOP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_161500_80134020 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_161500_80135968[16] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 26 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_161500_80134048 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133FE4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_161500_80134034 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor161500OpenParkingShop }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500RunCaptionCommand }, { .value = ACTOR_161500_CAPTION_AFTER_PARKING_SHOP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_161500_80134020 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_161500_80135AE8[16] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 27 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_161500_80134048 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F74 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_161500_80134020 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor161500OpenParkingShop }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500RunCaptionCommand }, { .value = ACTOR_161500_CAPTION_AFTER_PARKING_SHOP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_161500_80134020 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_161500_80133F78 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_161500_80135C68[16] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 28 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_161500_80134048 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133F7C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_161500_80134034 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80133FE4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor161500OpenParkingShop }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor161500RunCaptionCommand }, { .value = ACTOR_161500_CAPTION_AFTER_PARKING_SHOP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_161500_80134020 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

static AnimationPackedPose _gActor161500Animation04304Bank1[6] = {
#include "assets/actor_161500_animation_04304_bank1.inc"
};

static AnimationPackedRotation _gActor161500Animation04304Bank4[75] = {
#include "assets/actor_161500_animation_04304_bank4.inc"
};

static AnimationRecord _gActor161500Animation04304Records[104] = {
#include "assets/actor_161500_animation_04304_records.inc"
};

static u16 _gActor161500Animation04304Indices[20] = {
#include "assets/actor_161500_animation_04304_indices.inc"
};

static AnimationSet _gActor161500Animation04304 = {
    _gActor161500Animation04304Records,
    _gActor161500Animation04304Indices,
    { NULL, _gActor161500Animation04304Bank1, NULL, NULL, _gActor161500Animation04304Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor161500Animation04518Bank1[2] = {
#include "assets/actor_161500_animation_04518_bank1.inc"
};

static AnimationPackedRotation _gActor161500Animation04518Bank4[24] = {
#include "assets/actor_161500_animation_04518_bank4.inc"
};

static AnimationRecord _gActor161500Animation04518Records[83] = {
#include "assets/actor_161500_animation_04518_records.inc"
};

static u16 _gActor161500Animation04518Indices[20] = {
#include "assets/actor_161500_animation_04518_indices.inc"
};

static AnimationSet _gActor161500Animation04518 = {
    _gActor161500Animation04518Records,
    _gActor161500Animation04518Indices,
    { NULL, _gActor161500Animation04518Bank1, NULL, NULL, _gActor161500Animation04518Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor161500Animation047F8Bank1[5] = {
#include "assets/actor_161500_animation_047F8_bank1.inc"
};

static AnimationPackedRotation _gActor161500Animation047F8Bank4[60] = {
#include "assets/actor_161500_animation_047F8_bank4.inc"
};

static AnimationRecord _gActor161500Animation047F8Records[89] = {
#include "assets/actor_161500_animation_047F8_records.inc"
};

static u16 _gActor161500Animation047F8Indices[20] = {
#include "assets/actor_161500_animation_047F8_indices.inc"
};

static AnimationSet _gActor161500Animation047F8 = {
    _gActor161500Animation047F8Records,
    _gActor161500Animation047F8Indices,
    { NULL, _gActor161500Animation047F8Bank1, NULL, NULL, _gActor161500Animation047F8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor161500Animation04A88Bank1[3] = {
#include "assets/actor_161500_animation_04A88_bank1.inc"
};

static AnimationPackedRotation _gActor161500Animation04A88Bank4[36] = {
#include "assets/actor_161500_animation_04A88_bank4.inc"
};

static AnimationRecord _gActor161500Animation04A88Records[99] = {
#include "assets/actor_161500_animation_04A88_records.inc"
};

static u16 _gActor161500Animation04A88Indices[20] = {
#include "assets/actor_161500_animation_04A88_indices.inc"
};

static AnimationSet _gActor161500Animation04A88 = {
    _gActor161500Animation04A88Records,
    _gActor161500Animation04A88Indices,
    { NULL, _gActor161500Animation04A88Bank1, NULL, NULL, _gActor161500Animation04A88Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor161500Animation04C60Bank1[3] = {
#include "assets/actor_161500_animation_04C60_bank1.inc"
};

static AnimationPackedRotation _gActor161500Animation04C60Bank4[32] = {
#include "assets/actor_161500_animation_04C60_bank4.inc"
};

static AnimationRecord _gActor161500Animation04C60Records[57] = {
#include "assets/actor_161500_animation_04C60_records.inc"
};

static u16 _gActor161500Animation04C60Indices[20] = {
#include "assets/actor_161500_animation_04C60_indices.inc"
};

static AnimationSet _gActor161500Animation04C60 = {
    _gActor161500Animation04C60Records,
    _gActor161500Animation04C60Indices,
    { NULL, _gActor161500Animation04C60Bank1, NULL, NULL, _gActor161500Animation04C60Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor161500Animation04E94Bank1[3] = {
#include "assets/actor_161500_animation_04E94_bank1.inc"
};

static AnimationPackedRotation _gActor161500Animation04E94Bank4[29] = {
#include "assets/actor_161500_animation_04E94_bank4.inc"
};

static AnimationRecord _gActor161500Animation04E94Records[83] = {
#include "assets/actor_161500_animation_04E94_records.inc"
};

static u16 _gActor161500Animation04E94Indices[20] = {
#include "assets/actor_161500_animation_04E94_indices.inc"
};

static AnimationSet _gActor161500Animation04E94 = {
    _gActor161500Animation04E94Records,
    _gActor161500Animation04E94Indices,
    { NULL, _gActor161500Animation04E94Bank1, NULL, NULL, _gActor161500Animation04E94Bank4, NULL, NULL, NULL },
};

TaskDesc D_actor_161500_80136CDC = { { { TASK_BODY_NONE, 32 } }, _actor161500CompleteCompanionRequestTask, { .value = 0 } };

ActorTransform D_actor_161500_80136CE8 = { { 850, 0, 4400, 0 }, { 0, 568, 0, 0 } };

ActorTransform D_actor_161500_80136D00 = { { 850, 0, 4400, 0 }, { 0, 568, 0, 0 } };

ActorTransform D_actor_161500_80136D18 = { { 2510, 0, 5950, 0 }, { 0, 2616, 0, 0 } };

ActorTransform D_actor_161500_80136D30 = { { 1330, 0, 4780, 0 }, { 0, 2616, 0, 0 } };

ActorTransform D_actor_161500_80136D48 = { { 4224, 0, 5209, 0 }, { 0, 2048, 0, 0 } };

/// Player clips for extended ids 47-52, added by the soldier's request scenes,
/// then the NULL that closes the table (id 53), which nothing requests.
///
/// The request scenes are the ones the soldier plays while the companion is
/// present: the first exchange, the two that repeat while the request is
/// pending, and the one that hands the item over. Each event script that plays
/// one of these clips first sends the player its copy request.
/// `D_actor_161500_80136E08` copies ten words starting here into the player's
/// bank, which is three words past the end of this array: the read runs on
/// through the first three words of `D_actor_161500_80136D7C`. That overrun is
/// the original's and is kept as it is: the request carries a literal count
/// larger than the table, while the table was stored with only its own entries.
/// The scenes select ids 47-52 only, so none of the words installed after the
/// table is played as a clip.
AnimationSet* D_actor_161500_80136D60[7] = { &_gActor161500Animation04304, &_gActor161500Animation04518, &_gActor161500Animation047F8, &_gActor161500Animation04A88, &_gActor161500Animation04C60, &_gActor161500Animation04E94, NULL };

// Request for extended id 47. Nothing references it; it repeats the first of the requests that follow.
AnimationPlayRequest D_actor_161500_80136D7C = { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_161500_80136D90 = { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_161500_80136DA4 = { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_161500_80136DB8 = { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_161500_80136DCC = { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_161500_80136DE0 = { { .index = 1 }, 51, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_161500_80136DF4 = { { .index = 1 }, 52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationBankCopyRequest D_actor_161500_80136E08 = { { .sets = D_actor_161500_80136D60 }, 10 };

AnimationPlayRequest D_actor_161500_80136E10 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_161500_80136E24 = { { .index = 6 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_161500_80136E38 = { { .index = 6 }, 38, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_161500_80136E4C = { { .index = 6 }, 7, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_161500_80136E60 = { { .index = 6 }, 8, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_161500_80136E74 = { { .index = 6 }, 9, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

EvsCommand D_actor_161500_80136E88[21] = {
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = 0x55040003 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_161500_80136D00 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_161500_80136D48 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136E10 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136E74 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 23 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = shelter1fHeliportUpdateCompanionObstacle }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_161500_80137080[62] = {
    { EVENT_SCRIPT_OPCODE_SET_SKIP_KEEP_SOUND, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 22 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_161500_80136E08 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136E10 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136E24 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_161500_80136CE8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_161500_80136D18 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x55040003 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1010 }, { .message = { .pointer = &D_actor_161500_80136D30 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136D90 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136DA4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1006 }, { .message = { .pointer = &D_actor_161500_80136D30 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136E24 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136E4C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136E38 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136E74 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136DCC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136DE0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136DA4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136DF4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 46 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136DA4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136E38 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x55040004 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_161500_80136D48 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136E74 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = shelter1fHeliportUpdateCompanionObstacle }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_161500_80137650[6] = {
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136E74 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_161500_80136D48 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = shelter1fHeliportUpdateCompanionObstacle }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

ActorTransform D_actor_161500_801376E0 = { { 0, 0, 0, 0 }, { 0, -1024, 0, 0 } };

EvsCommand D_actor_161500_801376F8[20] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_161500_80136E08 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 27 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor161500PreparePlayerFacingCompanion }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1006 }, { .message = { .pointer = &D_actor_161500_801376E0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136D90 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136DA4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x55040004 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136E38 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136E74 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136DB8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_161500_801378D8[20] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_161500_80136E08 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 29 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor161500PreparePlayerFacingCompanion }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1006 }, { .message = { .pointer = &D_actor_161500_801376E0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136D90 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136DA4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x55040004 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136E38 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136E74 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136DB8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_161500_80137AB8[29] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = shelter1fHeliportUpdateCompanionObstacle }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_161500_80136E08 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 28 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = _actor161500SetPlayerUpdateHold }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor161500PreparePlayerFacingCompanion }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1006 }, { .message = { .pointer = &D_actor_161500_801376E0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136D90 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136DA4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x55040004 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136E38 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x55040004 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136E38 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136E60 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_161500_80136DB8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

static TmdBone _gActor161500SoldierBRifleSkeleton[1] = {
#include "assets/soldier_b_rifle_skeleton.inc"
};

static u32 _gActor161500SoldierBRiflePartVerts[1] = {
#include "assets/soldier_b_rifle_partVerts.inc"
};

static SVECTOR _gActor161500SoldierBRifleVerts[58] = {
#include "assets/soldier_b_rifle_verts.inc"
};

static SVECTOR _gActor161500SoldierBRifleNormals[58] = {
#include "assets/soldier_b_rifle_normals.inc"
};

static u32 _gActor161500SoldierBRifleStream[406] = {
#include "assets/soldier_b_rifle_stream.inc"
};

static TmdSource _gActor161500SoldierBRifle = {
    0,
    2940,
    0,
    1,
    _gActor161500SoldierBRiflePartVerts,
    _gActor161500SoldierBRifleVerts,
    _gActor161500SoldierBRifleNormals,
    _gActor161500SoldierBRifleSkeleton,
    _gActor161500SoldierBRifleStream,
};

static TmdBone _gActor161500SoldierBBodySkeleton[20] = {
#include "assets/soldier_b_body_skeleton.inc"
};

static u32 _gActor161500SoldierBBodyPartVerts[20] = {
#include "assets/soldier_b_body_partVerts.inc"
};

static SVECTOR _gActor161500SoldierBBodyVerts[366] = {
#include "assets/soldier_b_body_verts.inc"
};

static SVECTOR _gActor161500SoldierBBodyNormals[363] = {
#include "assets/soldier_b_body_normals.inc"
};

static u32 _gActor161500SoldierBBodyStream[3913] = {
#include "assets/soldier_b_body_stream.inc"
};

static TmdSource _gActor161500SoldierBBody = {
    0,
    21132,
    6448,
    20,
    _gActor161500SoldierBBodyPartVerts,
    _gActor161500SoldierBBodyVerts,
    _gActor161500SoldierBBodyNormals,
    _gActor161500SoldierBBodySkeleton,
    _gActor161500SoldierBBodyStream,
};

static AnimationPackedPose _gActor161500Animation0C318Bank1[2] = {
#include "assets/actor_161500_animation_0C318_bank1.inc"
};

static AnimationPackedRotation _gActor161500Animation0C318Bank4[32] = {
#include "assets/actor_161500_animation_0C318_bank4.inc"
};

static AnimationRecord _gActor161500Animation0C318Records[101] = {
#include "assets/actor_161500_animation_0C318_records.inc"
};

static u16 _gActor161500Animation0C318Indices[20] = {
#include "assets/actor_161500_animation_0C318_indices.inc"
};

static AnimationSet _gActor161500Animation0C318 = {
    _gActor161500Animation0C318Records,
    _gActor161500Animation0C318Indices,
    { NULL, _gActor161500Animation0C318Bank1, NULL, NULL, _gActor161500Animation0C318Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor161500Animation0C88CBank1[2] = {
#include "assets/actor_161500_animation_0C88C_bank1.inc"
};

static AnimationPackedRotation _gActor161500Animation0C88CBank4[135] = {
#include "assets/actor_161500_animation_0C88C_bank4.inc"
};

static AnimationRecord _gActor161500Animation0C88CRecords[188] = {
#include "assets/actor_161500_animation_0C88C_records.inc"
};

static u16 _gActor161500Animation0C88CIndices[20] = {
#include "assets/actor_161500_animation_0C88C_indices.inc"
};

static AnimationSet _gActor161500Animation0C88C = {
    _gActor161500Animation0C88CRecords,
    _gActor161500Animation0C88CIndices,
    { NULL, _gActor161500Animation0C88CBank1, NULL, NULL, _gActor161500Animation0C88CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor161500Animation0CB1CBank1[3] = {
#include "assets/actor_161500_animation_0CB1C_bank1.inc"
};

static AnimationPackedRotation _gActor161500Animation0CB1CBank4[27] = {
#include "assets/actor_161500_animation_0CB1C_bank4.inc"
};

static AnimationRecord _gActor161500Animation0CB1CRecords[108] = {
#include "assets/actor_161500_animation_0CB1C_records.inc"
};

static u16 _gActor161500Animation0CB1CIndices[20] = {
#include "assets/actor_161500_animation_0CB1C_indices.inc"
};

static AnimationSet _gActor161500Animation0CB1C = {
    _gActor161500Animation0CB1CRecords,
    _gActor161500Animation0CB1CIndices,
    { NULL, _gActor161500Animation0CB1CBank1, NULL, NULL, _gActor161500Animation0CB1CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor161500Animation0D2B0Bank1[21] = {
#include "assets/actor_161500_animation_0D2B0_bank1.inc"
};

static AnimationPackedRotation _gActor161500Animation0D2B0Bank4[156] = {
#include "assets/actor_161500_animation_0D2B0_bank4.inc"
};

static AnimationRecord _gActor161500Animation0D2B0Records[246] = {
#include "assets/actor_161500_animation_0D2B0_records.inc"
};

static u16 _gActor161500Animation0D2B0Indices[20] = {
#include "assets/actor_161500_animation_0D2B0_indices.inc"
};

static AnimationSet _gActor161500Animation0D2B0 = {
    _gActor161500Animation0D2B0Records,
    _gActor161500Animation0D2B0Indices,
    { NULL, _gActor161500Animation0D2B0Bank1, NULL, NULL, _gActor161500Animation0D2B0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor161500Animation0D544Bank1[5] = {
#include "assets/actor_161500_animation_0D544_bank1.inc"
};

static AnimationPackedRotation _gActor161500Animation0D544Bank4[36] = {
#include "assets/actor_161500_animation_0D544_bank4.inc"
};

static AnimationRecord _gActor161500Animation0D544Records[94] = {
#include "assets/actor_161500_animation_0D544_records.inc"
};

static u16 _gActor161500Animation0D544Indices[20] = {
#include "assets/actor_161500_animation_0D544_indices.inc"
};

static AnimationSet _gActor161500Animation0D544 = {
    _gActor161500Animation0D544Records,
    _gActor161500Animation0D544Indices,
    { NULL, _gActor161500Animation0D544Bank1, NULL, NULL, _gActor161500Animation0D544Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor161500Animation0D834Bank1[2] = {
#include "assets/actor_161500_animation_0D834_bank1.inc"
};

static AnimationPackedRotation _gActor161500Animation0D834Bank4[55] = {
#include "assets/actor_161500_animation_0D834_bank4.inc"
};

static AnimationRecord _gActor161500Animation0D834Records[107] = {
#include "assets/actor_161500_animation_0D834_records.inc"
};

static u16 _gActor161500Animation0D834Indices[20] = {
#include "assets/actor_161500_animation_0D834_indices.inc"
};

static AnimationSet _gActor161500Animation0D834 = {
    _gActor161500Animation0D834Records,
    _gActor161500Animation0D834Indices,
    { NULL, _gActor161500Animation0D834Bank1, NULL, NULL, _gActor161500Animation0D834Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor161500Animation0D9E8Bank1[2] = {
#include "assets/actor_161500_animation_0D9E8_bank1.inc"
};

static AnimationPackedRotation _gActor161500Animation0D9E8Bank4[19] = {
#include "assets/actor_161500_animation_0D9E8_bank4.inc"
};

static AnimationRecord _gActor161500Animation0D9E8Records[64] = {
#include "assets/actor_161500_animation_0D9E8_records.inc"
};

static u16 _gActor161500Animation0D9E8Indices[20] = {
#include "assets/actor_161500_animation_0D9E8_indices.inc"
};

static AnimationSet _gActor161500Animation0D9E8 = {
    _gActor161500Animation0D9E8Records,
    _gActor161500Animation0D9E8Indices,
    { NULL, _gActor161500Animation0D9E8Bank1, NULL, NULL, _gActor161500Animation0D9E8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor161500Animation0DB9CBank1[2] = {
#include "assets/actor_161500_animation_0DB9C_bank1.inc"
};

static AnimationPackedRotation _gActor161500Animation0DB9CBank4[19] = {
#include "assets/actor_161500_animation_0DB9C_bank4.inc"
};

static AnimationRecord _gActor161500Animation0DB9CRecords[64] = {
#include "assets/actor_161500_animation_0DB9C_records.inc"
};

static u16 _gActor161500Animation0DB9CIndices[20] = {
#include "assets/actor_161500_animation_0DB9C_indices.inc"
};

static AnimationSet _gActor161500Animation0DB9C = {
    _gActor161500Animation0DB9CRecords,
    _gActor161500Animation0DB9CIndices,
    { NULL, _gActor161500Animation0DB9CBank1, NULL, NULL, _gActor161500Animation0DB9CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor161500Animation0DE54Bank1[3] = {
#include "assets/actor_161500_animation_0DE54_bank1.inc"
};

static AnimationPackedRotation _gActor161500Animation0DE54Bank4[29] = {
#include "assets/actor_161500_animation_0DE54_bank4.inc"
};

static AnimationRecord _gActor161500Animation0DE54Records[116] = {
#include "assets/actor_161500_animation_0DE54_records.inc"
};

static u16 _gActor161500Animation0DE54Indices[20] = {
#include "assets/actor_161500_animation_0DE54_indices.inc"
};

static AnimationSet _gActor161500Animation0DE54 = {
    _gActor161500Animation0DE54Records,
    _gActor161500Animation0DE54Indices,
    { NULL, _gActor161500Animation0DE54Bank1, NULL, NULL, _gActor161500Animation0DE54Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor161500Animation0E148Bank1[2] = {
#include "assets/actor_161500_animation_0E148_bank1.inc"
};

static AnimationPackedRotation _gActor161500Animation0E148Bank4[43] = {
#include "assets/actor_161500_animation_0E148_bank4.inc"
};

static AnimationRecord _gActor161500Animation0E148Records[120] = {
#include "assets/actor_161500_animation_0E148_records.inc"
};

static u16 _gActor161500Animation0E148Indices[20] = {
#include "assets/actor_161500_animation_0E148_indices.inc"
};

static AnimationSet _gActor161500Animation0E148 = {
    _gActor161500Animation0E148Records,
    _gActor161500Animation0E148Indices,
    { NULL, _gActor161500Animation0E148Bank1, NULL, NULL, _gActor161500Animation0E148Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor161500Animation0E338Bank1[2] = {
#include "assets/actor_161500_animation_0E338_bank1.inc"
};

static AnimationPackedRotation _gActor161500Animation0E338Bank4[30] = {
#include "assets/actor_161500_animation_0E338_bank4.inc"
};

static AnimationRecord _gActor161500Animation0E338Records[68] = {
#include "assets/actor_161500_animation_0E338_records.inc"
};

static u16 _gActor161500Animation0E338Indices[20] = {
#include "assets/actor_161500_animation_0E338_indices.inc"
};

static AnimationSet _gActor161500Animation0E338 = {
    _gActor161500Animation0E338Records,
    _gActor161500Animation0E338Indices,
    { NULL, _gActor161500Animation0E338Bank1, NULL, NULL, _gActor161500Animation0E338Bank4, NULL, NULL, NULL },
};

TaskMessageEntry gStrideWalkMessages[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _strideWalkPlayAnimation },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _strideWalkSetModelDraw },
    { ACTOR_MESSAGE_PLACE, _pacedWalkPlace },
    { ACTOR_COMMAND_MESSAGE_APPLY, _strideWalkSetTurnMode },
    { ACTOR_MESSAGE_WALK_TO, _strideWalkSetWalkTarget },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc gStrideWalkTasks[2] = {
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, _actor161500StrideWalkerTask, { .model = &_gActor161500SoldierBBody } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, _strideWalkSubModelTask, { .model = &_gActor161500SoldierBRifle } },
};

AnimationSet* gStrideWalkAnimParams[12] = {
    NULL,
    &_gActor161500Animation0C318,
    &_gActor161500Animation0CB1C,
    &_gActor161500Animation0C88C,
    &_gActor161500Animation0D2B0,
    &_gActor161500Animation0D834,
    &_gActor161500Animation0D9E8,
    &_gActor161500Animation0DB9C,
    &_gActor161500Animation0DE54,
    &_gActor161500Animation0E148,
    &_gActor161500Animation0E338,
    &_gActor161500Animation0D544,
};

/// Queues soldier B's visitor-departure remark after pending sterilization outcomes.
///
/// The two initial sterilization remarks take priority. When meeting progress
/// reaches departed, records its remark as handled before selecting the line;
/// subsequent calls leave it alone. A sterilization follow-up may be replaced.
static inline void _actor161500PrepareVisitorDepartureRemark(void)
{
    if ((gameFlagGetNibble(GAME_FLAG_SOLDIER_B_REMARK_STATE) != ACTOR_161500_REMARK_STERILIZATION_WITH_COMPANION) &&
        (gameFlagGetNibble(GAME_FLAG_SOLDIER_B_REMARK_STATE) != ACTOR_161500_REMARK_STERILIZATION_ALONE) &&
        (gameFlagGetNibble(GAME_FLAG_ACTOR_160700_MEETING_PROGRESS) == ACTOR_161500_VISITOR_DEPARTED)) {
        gameFlagSetNibble(GAME_FLAG_ACTOR_160700_MEETING_PROGRESS, ACTOR_161500_VISITOR_REMARK_SEEN);
        gameFlagSetNibble(GAME_FLAG_SOLDIER_B_REMARK_STATE, ACTOR_161500_REMARK_VISITOR_DEPARTURE);
    }
}

void actor161500StartSoldierBRemark(void)
{
    _actor161500PrepareVisitorDepartureRemark();

    switch (gameFlagGetNibble(GAME_FLAG_SOLDIER_B_REMARK_STATE)) {
        case ACTOR_161500_REMARK_DEFAULT:
            evsStartScript(D_actor_161500_80135668, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            break;
        case ACTOR_161500_REMARK_STERILIZATION_WITH_COMPANION:
            evsStartScript(D_actor_161500_801357E8, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            gameFlagSetNibble(GAME_FLAG_SOLDIER_B_REMARK_STATE, ACTOR_161500_REMARK_STERILIZATION_FOLLOWUP);
            break;
        case ACTOR_161500_REMARK_STERILIZATION_ALONE:
            evsStartScript(D_actor_161500_80135968, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            gameFlagSetNibble(GAME_FLAG_SOLDIER_B_REMARK_STATE, ACTOR_161500_REMARK_STERILIZATION_FOLLOWUP);
            break;
        case ACTOR_161500_REMARK_STERILIZATION_FOLLOWUP:
            evsStartScript(D_actor_161500_80135AE8, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            gameFlagSetNibble(GAME_FLAG_SOLDIER_B_REMARK_STATE, ACTOR_161500_REMARK_DEFAULT);
            break;
        case ACTOR_161500_REMARK_VISITOR_DEPARTURE:
            evsStartScript(D_actor_161500_80135C68, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            gameFlagSetNibble(GAME_FLAG_SOLDIER_B_REMARK_STATE, ACTOR_161500_REMARK_DEFAULT);
            break;
    }
}

/// Selects the soldier's loaded CAP data and texture page, or restores the default.
///
/// Zero resets CAP; positive values are loaded data-resource ordinals. Any
/// negative value selects ordinal 2 in variant 1, ordinal 1 otherwise. The
/// selected writable CAP resource and texture page must remain loaded for playback.
static void _actor161500SelectCaptionFile(s32 captionFile)
{
    enum { ACTOR_161500_CAPTION_VRAM_X = 832,
           ACTOR_161500_CAPTION_VRAM_Y = 0 };
    s8 variantCaptionFile;

    if (captionFile != ACTOR_161500_CAPTION_RESET) {
        Gp_CapFile = NULL;
        if (captionFile <= 0) {
            variantCaptionFile = ACTOR_161500_CAPTION_FILE_STANDARD;
            if (gGameSession->location.loc.variant == ACTOR_161500_ALTERNATE_VARIANT) {
                variantCaptionFile = ACTOR_161500_CAPTION_FILE_ALTERNATE;
            }
            captionFile = variantCaptionFile;
        }
        capSelectLoadedFile(captionFile);
        capSetTexturePage(ACTOR_161500_CAPTION_VRAM_X, ACTOR_161500_CAPTION_VRAM_Y);
        return;
    }
    capReset();
}

void actor161500StartSoldierBTalkA(void)
{
    s32 variantScriptOffset;
    s32 talkIndex;

    variantScriptOffset = (gGameSession->location.loc.variant == ACTOR_161500_ALTERNATE_VARIANT) * ACTOR_161500_TALKS_PER_VARIANT;
    talkIndex           = gameFlagGetNibble(GAME_FLAG_SOLDIER_B_TALK_COUNT_A);
    evsStartScript(D_actor_161500_80134920[talkIndex + variantScriptOffset], EVENT_SCRIPT_HUD_HIDE_RESTORE);
    if (talkIndex < ACTOR_161500_LAST_TALK) {
        gameFlagSetNibble(GAME_FLAG_SOLDIER_B_TALK_COUNT_A, talkIndex + 1);
    }
}

void actor161500StartSoldierBTalkB(void)
{
    s32 variantScriptOffset;
    s32 talkIndex;

    variantScriptOffset = (gGameSession->location.loc.variant == ACTOR_161500_ALTERNATE_VARIANT) * ACTOR_161500_TALKS_PER_VARIANT;
    talkIndex           = gameFlagGetNibble(GAME_FLAG_SOLDIER_B_TALK_COUNT_B);
    evsStartScript(D_actor_161500_80135288[talkIndex + variantScriptOffset], EVENT_SCRIPT_HUD_HIDE_RESTORE);
    if (talkIndex < ACTOR_161500_LAST_TALK) {
        gameFlagSetNibble(GAME_FLAG_SOLDIER_B_TALK_COUNT_B, talkIndex + 1);
    }
}

/// Opens the heliport shop's standard or variant-1 alternate stock at category zero.
static void _actor161500OpenHeliportShop(void)
{
    GameSession* session;

    session = gGameSession;
    switch (session->location.loc.variant) {
        case ACTOR_161500_ALTERNATE_VARIANT:
            shopOpenSession(ACTOR_161500_SHOP_STOCK_ALTERNATE);
            break;
        default:
            shopOpenSession(ACTOR_161500_SHOP_STOCK_STANDARD);
            break;
    }
}

/// Resumes the selected soldier CAP file with a command after the shop closes.
///
/// `commandId` is a CAP command-table index. Uses the current view in place;
/// the selected CAP file and its texture page must still be loaded.
static void _actor161500RunCaptionCommand(s32 commandId)
{
    capRunCommand(commandId, CAP_PLAYBACK_IN_PLACE);
}

void actor161500StartSoldierBShopConversation(void)
{
    if (gameFlagGetNibble(GAME_FLAG_105) == 0) {
        evsStartScript(D_actor_161500_801352A8, EVENT_SCRIPT_HUD_HIDE_RESTORE);
    } else {
        evsStartScript(D_actor_161500_801354B8, EVENT_SCRIPT_HUD_HIDE_RESTORE);
    }
}

/// Opens parking-shop stock selected by the memo follow-up and sterilization outcome.
static void _actor161500OpenParkingShop(void)
{
    enum { ACTOR_161500_STERILIZATION_WITH_COMPANION = 2 };
    if (gameFlagGetNibble(GAME_FLAG_ITEM_125_FOLLOWUP_SEEN) != 0) {
        shopOpenSession((gameFlagGetNibble(GAME_FLAG_STERILIZATION_ROOM_EVENT_STATE) != ACTOR_161500_STERILIZATION_WITH_COMPANION) ? ACTOR_161500_SHOP_STOCK_ALTERNATE : ACTOR_161500_SHOP_STOCK_ALTERNATE_EVENT);
    } else {
        shopOpenSession((gameFlagGetNibble(GAME_FLAG_STERILIZATION_ROOM_EVENT_STATE) == ACTOR_161500_STERILIZATION_WITH_COMPANION) ? ACTOR_161500_SHOP_STOCK_EVENT : ACTOR_161500_SHOP_STOCK_STANDARD);
    }
}

/// Completes the accepted item request and starts its handover scene, then kills this task.
///
/// The room has checked item use and companion presence before spawning this
/// one-shot task. Holds the player state tick until the script releases it.
static void _actor161500CompleteCompanionRequestTask(Task* task)
{
    enum { ACTOR_161500_REQUEST_ITEM = 0x124 };
    D_80115768 = true;
    itemSetIdentified(ACTOR_161500_REQUEST_ITEM, true);
    gameFlagSetNibble(GAME_FLAG_HELIPORT_SOLDIER_REQUEST_STATE, ACTOR_161500_REQUEST_COMPLETE);
    evsStartScript(D_actor_161500_80137AB8, EVENT_SCRIPT_HUD_HIDE_RESTORE);
    taskKill(task);
}

/// Sets the request scene's player-facing yaw toward the live companion.
///
/// Both task slots must contain TMD models whose roots share a parent frame.
/// Stores a 0..4095 yaw, with +Z as zero and +X as the positive quarter turn.
static void _actor161500PreparePlayerFacingCompanion(void)
{
    GfxCoord* companionCoord;
    GfxCoord* playerCoord;

    companionCoord = gameGetTaskSlot(GAME_TASK_SLOT_COMPANION)->extra.tmd->coords;
    playerCoord    = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
    actorRenderComposeCoord(companionCoord);
    actorRenderComposeCoord(playerCoord);
    D_actor_161500_801376E0.rot.vy =
        ratan2(companionCoord->coord.t[0] - playerCoord->coord.t[0], companionCoord->coord.t[2] - playerCoord->coord.t[2]) & ACTOR_TRANSFORM_ANGLE_MASK;
}

/// Sets the player state-tick hold byte (zero runs the tick, nonzero holds it).
///
/// Input capture and collision preparation continue while held. The request
/// handover script passes zero to release the hold; all eight bits are retained.
static void _actor161500SetPlayerUpdateHold(u8 holdPlayerUpdate)
{
    D_80115768 = holdPlayerUpdate;
}

void actor161500StartCompanionRequestReminder(void)
{
    enum { ACTOR_161500_REQUEST_PLACEMENT = 3 };
    s32 requestState;

    if (gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != NULL) {
        requestState = gameFlagGetNibble(GAME_FLAG_HELIPORT_SOLDIER_REQUEST_STATE);
        if (requestState == ACTOR_161500_REQUEST_PENDING) {
            if (areaGetCurrentObjectState(ACTOR_161500_REQUEST_PLACEMENT) == requestState) {
                evsStartScript(D_actor_161500_801378D8, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            } else {
                evsStartScript(D_actor_161500_801376F8, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            }
        }
    }
}

void actor161500RestoreCompanionRequestScene(void)
{
    s32 requestState;

    if (gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != NULL) {
        requestState = gameFlagGetNibble(GAME_FLAG_HELIPORT_SOLDIER_REQUEST_STATE);
        switch (requestState) {
            case ACTOR_161500_REQUEST_UNSEEN:
                // The skip path establishes the same pending placement as the full scene.
                evsStartScriptWithSkip(D_actor_161500_80137080, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_actor_161500_80136E88);
                gameFlagSetNibble(GAME_FLAG_HELIPORT_SOLDIER_REQUEST_STATE, ACTOR_161500_REQUEST_PENDING);
                break;
            case ACTOR_161500_REQUEST_PENDING:
                evsStartScript(D_actor_161500_80137650, EVENT_SCRIPT_HUD_KEEP);
                break;
            case ACTOR_161500_REQUEST_COMPLETE:
                break;
        }
    }
}

#include "../../shared/stride_walk_spawn.inc.c"

#include "../../shared/stride_walk_update.inc.c"

/// Dispatches the soldier's stride walker initialization or frame update.
///
/// Descriptor 0 requires a live Enemy in `spawnArg2.pointer`, a twenty-part
/// TMD body and state 0..1. State 0 allocates the rig and stride-walk work;
/// state 1 composes, lights, moves and animates the model and draws its shadow.
/// State 1 also requires a live player model for the soldier's head aim.
/// Initialization may destroy the enemy and task on allocation failure.
/// The handler array has no bounds check; keep this overlay and clips loaded.
static void _actor161500StrideWalkerTask(Task* task)
{
    EnemyTaskFunc stateHandlers[] = {
        _strideWalkSpawn,
        _strideWalkFrame,
    };

    stateHandlers[task->state](task->spawnArg2.pointer, task);
}

#define ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW _actorRenderDrawFixedWalkerGroundShadow
#include "../../shared/stride_walk_frame.inc.c"
#undef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW

#include "../../shared/stride_walk_exit.inc.c"

#define ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW _actorRenderDrawFixedWalkerGroundShadow
#include "../../shared/walker_shadow.inc.c"
#undef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW

#include "../../shared/paced_walk_tick_anim.inc.c"

#include "../../shared/paced_walk_reset_anim.inc.c"

#include "../../shared/paced_walk_blend_anim.inc.c"

#include "../../shared/stride_walk_play.inc.c"

#include "../../shared/stride_walk_visibility.inc.c"

#include "../../shared/paced_walk_place.inc.c"

/// Applies a borrowed head-turn command to a live stride walker's work block.
///
/// Retains the command halfword in the signed turn mode: `STRIDE_WALK_TURN_PLAYER`
/// aims toward the player; every other value releases toward the animated pose.
/// The command is read synchronously and never retained. Other message arguments
/// are ignored, and zero acknowledges the command.
static s32 _strideWalkSetTurnMode(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArgument)
{
    StrideWalkWork* work = task->work;

    work->turnMode = command->command;
    return 0;
}

#include "../../shared/stride_walk_to.inc.c"

#include "../../shared/stride_walk_sub_model.inc.c"
