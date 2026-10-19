#include "mist_parking_private.h"

#include "types.h"

#include "rooms/mist_parking.h"

#include "gameplay/companion_load.h"
#include "gameplay/animation.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/inventory.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/starter_inventory.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/ui.h"
#include "main/ui_types.h"

#include "rooms/room_common.h"

static void _modelPlacementAttachPartTask(Task* childTask);
static void _mistParkingWaitOptionDialogAnswer(Task* task);

static void _mistParkingIdleAttachedModelState(Task* unusedTask);
static void _mistParkingOpenOptionDialog(Task* task);
static void _mistParkingExitOptionDialogTask(Task* task);

/// Unreferenced three-state recipe for attaching a child model to a parent part.
///
/// States 0..2 attach/reparent, idle, then kill the task; there is no terminator.
/// Setup expects a live TMD child, spawnArg1's part index and spawnArg2's parent.
/// The parent's model coordinates and lighting must outlive the child.
static const TaskFuncTable3 _gMistParkingAttachedModelStates = {
    {
        _modelPlacementAttachPartTask,
        _mistParkingIdleAttachedModelState,
        taskKill,
    },
};
/// Open, wait-for-answer and exit callbacks indexed by the option task's state (0..2).
static const TaskFuncTable3 _gMistParkingOptionDialogStates = {
    {
        _mistParkingOpenOptionDialog,
        _mistParkingWaitOptionDialogAnswer,
        _mistParkingExitOptionDialogTask,
    },
};

static void _mistParkingJodiePrizeAndShopTalkTask(Task* task);
static void _mistParkingDepartureMenuTask(Task* task);
static void _mistParkingStartPackedDialogue(s32 packedCommandAndVariant);
static void _mistParkingStartDepartureMovie(s32 movieSelector);
static void _mistParkingReloadAcropolisArea(s32 areaId);
static void _mistParkingSpawnCutsceneModel(s32 descriptorIndex);
static void _mistParkingReleaseCutsceneModel(s32 descriptorIndex);

/// Descriptor selected by the arrival conversation's model-release callback.
enum { MIST_PARKING_CUTSCENE_MODEL_DESCRIPTOR_INDEX = 0 };
static void _mistParkingOptionDialogTask(Task* task);
static void _mistParkingContinueShopChoiceTask(Task* task);
static void _mistParkingQueueDryfieldDeparture(void);
static void _mistParkingDepartForDryfieldTask(Task* task);
static void _mistParkingStartPlayerHeadAim(void);

/// CAP slots and variant keys packed into the room's dialogue callback words.
enum {
    MIST_PARKING_CAP_COMMAND_SHIFT              = 16,
    MIST_PARKING_CAP_FIRST_FOLLOWUP             = (15 << MIST_PARKING_CAP_COMMAND_SHIFT) | 0,
    MIST_PARKING_CAP_SECOND_FOLLOWUP            = (15 << MIST_PARKING_CAP_COMMAND_SHIFT) | 1,
    MIST_PARKING_CAP_FOLLOWUP_VARIANT_2         = (15 << MIST_PARKING_CAP_COMMAND_SHIFT) | 2,
    MIST_PARKING_CAP_DEPARTURE_STAY             = (5 << MIST_PARKING_CAP_COMMAND_SHIFT) | 1,
    MIST_PARKING_CAP_DEPARTURE_DRYFIELD         = (5 << MIST_PARKING_CAP_COMMAND_SHIFT) | 2,
    MIST_PARKING_CAP_DEPARTURE_SHOOTING_GALLERY = (5 << MIST_PARKING_CAP_COMMAND_SHIFT) | 3
};

/// Resource and entrance selectors installed for a departure reload.
enum {
    MIST_PARKING_RELOAD_WARP           = 1,
    MIST_PARKING_RELOAD_ROOM           = 1,
    MIST_PARKING_RELOAD_SPRITE_VARIANT = 1
};

/// The five course prizes share consecutive saved flags and placed-item states.
enum {
    MIST_PARKING_PRIZE_FLAG_FIRST       = GAME_FLAG_SHOOTING_GALLERY_PRIZE_4_STATE - 4,
    MIST_PARKING_PRIZE_WAITING          = 2,
    MIST_PARKING_PRIZE_CLOSED           = 3,
    MIST_PARKING_PRIZE_OBJECT_FIRST     = 0x20,
    MIST_PARKING_PRIZE_OBJECT_AVAILABLE = 1,
    MIST_PARKING_PRIZE_OBJECT_COLLECTED = 2,
    MIST_PARKING_PRIZE_CAPTION_SLOT     = 5
};

/// Clip indices and actor actions used by the parking conversation requests.
enum {
    MIST_PARKING_CONVERSATION_TASK_PRIORITY        = 192,
    MIST_PARKING_PLAYER_HEAD_AIM_TASK_PRIORITY     = 97,
    MIST_PARKING_PLAYER_ANIM_RESET                 = ANIMATION_BANK_BASE_SET_COUNT + 1,
    MIST_PARKING_PLAYER_ANIM_ARRIVAL_CUE           = ANIMATION_BANK_BASE_SET_COUNT + 3,
    MIST_PARKING_PLAYER_ANIM_MODEL_CUE             = ANIMATION_BANK_BASE_SET_COUNT + 4,
    MIST_PARKING_PLAYER_ANIM_TALK                  = ANIMATION_BANK_BASE_SET_COUNT + 17,
    MIST_PARKING_PLAYER_ANIM_STAY_OR_GALLERY_REPLY = ANIMATION_BANK_BASE_SET_COUNT + 21,
    MIST_PARKING_PLAYER_ANIM_DEPARTURE_REPLY       = ANIMATION_BANK_BASE_SET_COUNT + 23,
    MIST_PARKING_ACTOR_BODY_ANIMATION_BANK         = 0,
    MIST_PARKING_PIERCE_ANIM_REST                  = 1,
    MIST_PARKING_PIERCE_ANIM_WALK                  = 2,
    MIST_PARKING_PIERCE_ANIM_FOLLOWUP_TALK         = 8,
    MIST_PARKING_PIERCE_ANIM_DEPARTURE_REPLY       = 15,
    MIST_PARKING_PIERCE_ANIM_POST_ARRIVAL          = 26,
    MIST_PARKING_JODIE_ANIM_REST                   = 5,
    MIST_PARKING_JODIE_ANIM_SHOP_REPLY             = 8,
    MIST_PARKING_JODIE_ANIM_SHOP_REPLY_CLOSING     = 9,
    MIST_PARKING_PIERCE_COMMAND_HIDE_BILLBOARD     = 1,
    MIST_PARKING_PIERCE_COMMAND_AIM_HEAD           = 2,
    MIST_PARKING_PIERCE_COMMAND_RELEASE_HEAD       = 3,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_OFF     = 0,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_ON      = 1,
};

/// Clip IDs and whole-frame blend durations for the private dialogue requests.
enum {
    MIST_PARKING_PLAYER_DIALOGUE_BLEND_FRAMES  = 6,
    MIST_PARKING_PLAYER_SLOW_BLEND_FRAMES      = 30,
    MIST_PARKING_ACTOR_DIALOGUE_BLEND_FRAMES   = 20,
    MIST_PARKING_PLAYER_ANIM_ARRIVAL_CLIP_52   = ANIMATION_BANK_BASE_SET_COUNT + 5,
    MIST_PARKING_PLAYER_ANIM_ARRIVAL_CLIP_53   = ANIMATION_BANK_BASE_SET_COUNT + 6,
    MIST_PARKING_PLAYER_ANIM_ARRIVAL_CLIP_54   = ANIMATION_BANK_BASE_SET_COUNT + 7,
    MIST_PARKING_PLAYER_ANIM_ARRIVAL_CLIP_55   = ANIMATION_BANK_BASE_SET_COUNT + 8,
    MIST_PARKING_PLAYER_ANIM_ARRIVAL_CLIP_56   = ANIMATION_BANK_BASE_SET_COUNT + 9,
    MIST_PARKING_PLAYER_ANIM_ARRIVAL_CLIP_57   = ANIMATION_BANK_BASE_SET_COUNT + 10,
    MIST_PARKING_PLAYER_ANIM_ARRIVAL_CLIP_59   = ANIMATION_BANK_BASE_SET_COUNT + 12,
    MIST_PARKING_PLAYER_ANIM_ARRIVAL_CLIP_60   = ANIMATION_BANK_BASE_SET_COUNT + 13,
    MIST_PARKING_PLAYER_ANIM_ARRIVAL_CLIP_61   = ANIMATION_BANK_BASE_SET_COUNT + 14,
    MIST_PARKING_PLAYER_ANIM_ARRIVAL_CLIP_63   = ANIMATION_BANK_BASE_SET_COUNT + 16,
    MIST_PARKING_PLAYER_ANIM_ARRIVAL_CLIP_66   = ANIMATION_BANK_BASE_SET_COUNT + 19,
    MIST_PARKING_PLAYER_ANIM_ARRIVAL_CLIP_67   = ANIMATION_BANK_BASE_SET_COUNT + 20,
    MIST_PARKING_PLAYER_ANIM_ARRIVAL_CLIP_71   = ANIMATION_BANK_BASE_SET_COUNT + 24,
    MIST_PARKING_PIERCE_ANIM_DEPARTURE_PROMPT  = 5,
    MIST_PARKING_PIERCE_ANIM_ARRIVAL_CLOSING   = 6,
    MIST_PARKING_PIERCE_ANIM_ARRIVAL_CLIP_17   = 17,
    MIST_PARKING_PIERCE_ANIM_ARRIVAL_CLIP_18   = 18,
    MIST_PARKING_PIERCE_ANIM_ARRIVAL_CLIP_19   = 19,
    MIST_PARKING_PIERCE_ANIM_ARRIVAL_CLIP_20   = 20,
    MIST_PARKING_PIERCE_ANIM_ARRIVAL_CLIP_27   = 27,
    MIST_PARKING_PIERCE_ANIM_ARRIVAL_CLIP_28   = 28,
    MIST_PARKING_PIERCE_ANIM_ARRIVAL_CLIP_29   = 29,
    MIST_PARKING_PIERCE_ANIM_ARRIVAL_CLIP_31   = 31,
    MIST_PARKING_PIERCE_ANIM_ARRIVAL_CLIP_34   = 34,
    MIST_PARKING_JODIE_ANIM_TALK_OPENING       = 1,
    MIST_PARKING_PIERCE_ANIM_ARRIVAL_TALK      = 33,
    MIST_PARKING_PIERCE_COMMAND_SHOW_BILLBOARD = 0,
};

/// Stored bank seed and clip selectors of the unreferenced conversation requests.
enum {
    MIST_PARKING_PLAYER_REQUEST_BANK_SEED   = 1,
    MIST_PARKING_PLAYER_ANIM_BASE_IDLE      = 1,
    MIST_PARKING_PLAYER_ANIM_UNUSED_CLIP_49 = ANIMATION_BANK_BASE_SET_COUNT + 2,
    MIST_PARKING_PLAYER_ANIM_UNUSED_CLIP_58 = ANIMATION_BANK_BASE_SET_COUNT + 11,
    MIST_PARKING_PLAYER_ANIM_UNUSED_CLIP_62 = ANIMATION_BANK_BASE_SET_COUNT + 15,
    MIST_PARKING_PLAYER_ANIM_UNUSED_CLIP_65 = ANIMATION_BANK_BASE_SET_COUNT + 18,
    MIST_PARKING_PLAYER_ANIM_UNUSED_CLIP_69 = ANIMATION_BANK_BASE_SET_COUNT + 22,
    MIST_PARKING_PIERCE_ANIM_UNUSED_CLIP_3  = 3,
    MIST_PARKING_PIERCE_ANIM_UNUSED_CLIP_4  = 4,
    MIST_PARKING_PIERCE_ANIM_UNUSED_CLIP_7  = 7,
    MIST_PARKING_PIERCE_ANIM_UNUSED_CLIP_9  = 9,
    MIST_PARKING_PIERCE_ANIM_UNUSED_CLIP_10 = 10,
    MIST_PARKING_PIERCE_ANIM_UNUSED_CLIP_11 = 11,
    MIST_PARKING_PIERCE_ANIM_UNUSED_CLIP_12 = 12,
    MIST_PARKING_PIERCE_ANIM_UNUSED_CLIP_13 = 13,
    MIST_PARKING_PIERCE_ANIM_UNUSED_CLIP_14 = 14,
    MIST_PARKING_PIERCE_ANIM_UNUSED_CLIP_16 = 16,
    MIST_PARKING_PIERCE_ANIM_UNUSED_CLIP_21 = 21,
    MIST_PARKING_PIERCE_ANIM_UNUSED_CLIP_22 = 22,
    MIST_PARKING_PIERCE_ANIM_UNUSED_CLIP_23 = 23,
    MIST_PARKING_PIERCE_ANIM_UNUSED_CLIP_24 = 24,
    MIST_PARKING_PIERCE_ANIM_UNUSED_CLIP_25 = 25,
    MIST_PARKING_PIERCE_ANIM_UNUSED_CLIP_30 = 30,
    MIST_PARKING_PIERCE_ANIM_UNUSED_CLIP_32 = 32,
    MIST_PARKING_PIERCE_ANIM_UNUSED_CLIP_35 = 35,
};

/// Jodie greeting clip and its whole-frame interpolation duration.
enum {
    MIST_PARKING_JODIE_ANIM_GREETING         = 7,
    MIST_PARKING_JODIE_GREETING_BLEND_FRAMES = 10
};

/// Script targets, CAP slots, view selectors and the room's MIST stock profile.
///
/// WAIT_FRAMES delays below count interpreter updates; zero still yields once.
enum {
    MIST_PARKING_PIERCE_SCENE_PLACEMENT  = 0,
    MIST_PARKING_JODIE_SCENE_PLACEMENT   = 1,
    MIST_PARKING_SCENE_MODEL_HIDE        = 0,
    MIST_PARKING_SCENE_MODEL_SHOW        = 1,
    MIST_PARKING_CAP_ARRIVAL             = 14,
    MIST_PARKING_CAP_DEPARTURE_MENU      = 5,
    MIST_PARKING_VIEW_ARRIVAL            = 10,
    MIST_PARKING_VIEW_POST_ARRIVAL       = 5,
    MIST_PARKING_SHOP_STOCK_PROFILE      = 0x10,
    MIST_PARKING_SHOP_POSE_DELAY_UPDATES = 20,
    MIST_PARKING_SHOP_REPLY_END_UPDATES  = 30,
    MIST_PARKING_DEPARTURE_MOVIE_NORMAL  = 0,
    MIST_PARKING_FADE_BLEND_AVERAGE      = 0
};

/// Arrival sound ids: bank 0x5113, instance tag 0, and the named script entry.
enum {
    MIST_PARKING_ARRIVAL_SOUND_ENTRY_05  = 0x51130005,
    MIST_PARKING_ARRIVAL_SOUND_ENTRY_06  = 0x51130006,
    MIST_PARKING_ARRIVAL_SOUND_ENTRY_08  = 0x51130008,
    MIST_PARKING_ARRIVAL_SOUND_ENTRY_0C  = 0x5113000C,
    MIST_PARKING_ARRIVAL_SOUND_ENTRY_0D  = 0x5113000D,
    MIST_PARKING_ARRIVAL_SOUND_ENTRY_0E  = 0x5113000E,
    MIST_PARKING_ARRIVAL_SOUND_ENTRY_11  = 0x51130011,
    MIST_PARKING_ARRIVAL_AMBIENT_CHANNEL = 100
};

/// Catalog ids and quantities awarded for the five shooting-gallery courses.
enum {
    MIST_PARKING_COURSE_PRIZE_9MM_HYDRA        = 0xA1,
    MIST_PARKING_COURSE_PRIZE_RINGERS_SOLUTION = 0x3D,
    MIST_PARKING_COURSE_PRIZE_HUNTER_GOGGLES   = 0x3F,
    MIST_PARKING_COURSE_PRIZE_LIPSTICK         = 0x0B,
    MIST_PARKING_COURSE_PRIZE_SHOULDER_HOLSTER = 0x6C,
    MIST_PARKING_COURSE_PRIZE_AMMO_ROUNDS      = 50,
    MIST_PARKING_COURSE_PRIZE_ONE_ITEM         = 1
};

TaskDesc gMistParkingConversationTaskDescs[9] = {
    { { { TASK_BODY_TMD, MIST_PARKING_CONVERSATION_TASK_PRIORITY } }, mistParkingCutsceneModelPitchTask, { .model = &gMistParkingModel09B9C } },
    { { { TASK_BODY_NONE, MIST_PARKING_CONVERSATION_TASK_PRIORITY } }, _mistParkingOptionDialogTask, { .value = 0 } },
    { { { TASK_BODY_NONE, MIST_PARKING_CONVERSATION_TASK_PRIORITY } }, mistParkingContinueDepartureChoiceTask, { .value = 0 } },
    { { { TASK_BODY_NONE, MIST_PARKING_CONVERSATION_TASK_PRIORITY } }, _mistParkingDepartForDryfieldTask, { .value = 0 } },
    { { { TASK_BODY_NONE, MIST_PARKING_PLAYER_HEAD_AIM_TASK_PRIORITY } }, mistParkingAimPlayerHeadAtTalkPartnerTask, { .value = 0 } },
    { { { TASK_BODY_NONE, MIST_PARKING_CONVERSATION_TASK_PRIORITY } }, mistParkingDelayDisplayModeExitTask, { .value = 0 } },
    { { { TASK_BODY_NONE, MIST_PARKING_CONVERSATION_TASK_PRIORITY } }, _mistParkingContinueShopChoiceTask, { .value = 0 } },
    { { { TASK_BODY_NONE, MIST_PARKING_CONVERSATION_TASK_PRIORITY } }, _mistParkingJodiePrizeAndShopTalkTask, { .value = 0 } },
    { { { TASK_BODY_NONE, MIST_PARKING_CONVERSATION_TASK_PRIORITY } }, _mistParkingDepartureMenuTask, { .value = 0 } },
};

/// Player-bank extension for the arrival, follow-up, departure and Jodie shop scripts.
///
/// Copied to bank indices 47..71: entry 0 is NULL, entries 1..24 are playable
/// sets. These descriptors and their clip data are borrowed for playback.
static AnimationSet* _gMistParkingPlayerConversationAnimationSets[25] = {
    NULL,
    &gMistParkingAnimation09FD4,
    &gMistParkingAnimation0A774,
    &gMistParkingAnimation0AC5C,
    &gMistParkingAnimation0B138,
    &gMistParkingAnimation0B700,
    &gMistParkingAnimation0BAB4,
    &gMistParkingAnimation0C1B0,
    &gMistParkingAnimation0C688,
    &gMistParkingAnimation0CD80,
    &gMistParkingAnimation0D060,
    &gMistParkingAnimation0D6A4,
    &gMistParkingAnimation0DA94,
    &gMistParkingAnimation0DE94,
    &gMistParkingAnimation0E1D0,
    &gMistParkingAnimation0E6E0,
    &gMistParkingAnimation0EA0C,
    &gMistParkingAnimation0EDE0,
    &gMistParkingAnimation0F14C,
    &gMistParkingAnimation0F574,
    &gMistParkingAnimation0F7F0,
    &gMistParkingAnimation0FC60,
    &gMistParkingAnimation0FE5C,
    &gMistParkingAnimation10174,
    &gMistParkingAnimation0D3E4,
};

AnimationBankCopyRequest gMistParkingPlayerConversationBankCopy = { _gMistParkingPlayerConversationAnimationSets, ARRAY_SIZE(_gMistParkingPlayerConversationAnimationSets) };

/// Unreferenced player conversation request for clip 1.
///
/// Retained in the room image; no script or dispatcher selects this storage.
/// Stores source index 1; a weapon-animation opcode would select a bank in a copy.
/// Blend durations are whole animation frames.
static const AnimationPlayRequest _gMistParkingUnusedPlayerClip1Request SECTION(".data") = {
    .source               = { .index = MIST_PARKING_PLAYER_REQUEST_BANK_SEED },
    .animationId          = MIST_PARKING_PLAYER_ANIM_BASE_IDLE,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Resets the player to extension clip 1 between arrival or shop dialogue phases.
///
/// EVS replaces source.index with the equipped weapon's bank before dispatch.
/// The copied parking extension and its clips must remain loaded for playback.
static AnimationPlayRequest _gMistParkingPlayerConversationResetAnimation = {
    .source               = { .index = 1 },
    .animationId          = MIST_PARKING_PLAYER_ANIM_RESET,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Unreferenced player conversation request for clip 49.
///
/// Retained in the room image; no script or dispatcher selects this storage.
/// Stores source index 1; a weapon-animation opcode would select a bank in a copy.
/// Blend durations are whole animation frames.
static const AnimationPlayRequest _gMistParkingUnusedPlayerClip49Request SECTION(".data") = {
    .source               = { .index = MIST_PARKING_PLAYER_REQUEST_BANK_SEED },
    .animationId          = MIST_PARKING_PLAYER_ANIM_UNUSED_CLIP_49,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = MIST_PARKING_PLAYER_DIALOGUE_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays player extension clip 3 at its CAP cue during the arrival conversation.
///
/// EVS replaces source.index with the equipped weapon's bank before dispatch.
/// The copied parking extension and its clips must remain loaded for playback.
static AnimationPlayRequest _gMistParkingPlayerArrivalCueAnimation50 = {
    .source               = { .index = 1 },
    .animationId          = MIST_PARKING_PLAYER_ANIM_ARRIVAL_CUE,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 6,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays player extension clip 4 while the arrival cutscene model is present.
///
/// EVS replaces source.index with the equipped weapon's bank before dispatch.
/// The copied parking extension and its clips must remain loaded for playback.
static AnimationPlayRequest _gMistParkingPlayerCutsceneModelCueAnimation = {
    .source               = { .index = 1 },
    .animationId          = MIST_PARKING_PLAYER_ANIM_MODEL_CUE,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 6,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays player extension clip 5 at its arrival-conversation cue.
///
/// EVS selects the equipped weapon bank in a copy; the stored bank seed is unused.
/// The copied parking extension and its clips must stay loaded during playback.
static AnimationPlayRequest _gMistParkingPlayerArrivalClip52Request = {
    .source               = { .index = 1 },
    .animationId          = MIST_PARKING_PLAYER_ANIM_ARRIVAL_CLIP_52,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = MIST_PARKING_PLAYER_DIALOGUE_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays player extension clip 6 at its arrival-conversation cue.
///
/// EVS selects the equipped weapon bank in a copy; the stored bank seed is unused.
/// The copied parking extension and its clips must stay loaded during playback.
static AnimationPlayRequest _gMistParkingPlayerArrivalClip53Request = {
    .source               = { .index = 1 },
    .animationId          = MIST_PARKING_PLAYER_ANIM_ARRIVAL_CLIP_53,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = MIST_PARKING_PLAYER_DIALOGUE_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays player extension clip 7 at its arrival-conversation cue.
///
/// EVS selects the equipped weapon bank in a copy; the stored bank seed is unused.
/// The copied parking extension and its clips must stay loaded during playback.
static AnimationPlayRequest _gMistParkingPlayerArrivalClip54Request = {
    .source               = { .index = 1 },
    .animationId          = MIST_PARKING_PLAYER_ANIM_ARRIVAL_CLIP_54,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = MIST_PARKING_PLAYER_DIALOGUE_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays player extension clip 8 at its arrival-conversation cue.
///
/// EVS selects the equipped weapon bank in a copy; the stored bank seed is unused.
/// The copied parking extension and its clips must stay loaded during playback.
static AnimationPlayRequest _gMistParkingPlayerArrivalClip55Request = {
    .source               = { .index = 1 },
    .animationId          = MIST_PARKING_PLAYER_ANIM_ARRIVAL_CLIP_55,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = MIST_PARKING_PLAYER_DIALOGUE_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays player extension clip 9 at its arrival-conversation cue.
///
/// EVS selects the equipped weapon bank in a copy; the stored bank seed is unused.
/// The copied parking extension and its clips must stay loaded during playback.
static AnimationPlayRequest _gMistParkingPlayerArrivalClip56Request = {
    .source               = { .index = 1 },
    .animationId          = MIST_PARKING_PLAYER_ANIM_ARRIVAL_CLIP_56,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = MIST_PARKING_PLAYER_DIALOGUE_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays player extension clip 10 at its arrival-conversation cue.
///
/// EVS selects the equipped weapon bank in a copy; the stored bank seed is unused.
/// The copied parking extension and its clips must stay loaded during playback.
static AnimationPlayRequest _gMistParkingPlayerArrivalClip57Request = {
    .source               = { .index = 1 },
    .animationId          = MIST_PARKING_PLAYER_ANIM_ARRIVAL_CLIP_57,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = MIST_PARKING_PLAYER_DIALOGUE_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Unreferenced player conversation request for clip 58.
///
/// Retained in the room image; no script or dispatcher selects this storage.
/// Stores source index 1; a weapon-animation opcode would select a bank in a copy.
/// Blend durations are whole animation frames.
static const AnimationPlayRequest _gMistParkingUnusedPlayerClip58Request SECTION(".data") = {
    .source               = { .index = MIST_PARKING_PLAYER_REQUEST_BANK_SEED },
    .animationId          = MIST_PARKING_PLAYER_ANIM_UNUSED_CLIP_58,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts player extension clip 12 between arrival-conversation exchanges.
///
/// EVS selects the equipped weapon bank in a copy; the stored bank seed is unused.
/// The copied parking extension and its clips must stay loaded during playback.
static AnimationPlayRequest _gMistParkingPlayerArrivalTalkResetAnimation = {
    .source               = { .index = 1 },
    .animationId          = MIST_PARKING_PLAYER_ANIM_ARRIVAL_CLIP_59,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays player extension clip 13 at its arrival-conversation cue.
///
/// EVS selects the equipped weapon bank in a copy; the stored bank seed is unused.
/// The copied parking extension and its clips must stay loaded during playback.
static AnimationPlayRequest _gMistParkingPlayerArrivalClip60Request = {
    .source               = { .index = 1 },
    .animationId          = MIST_PARKING_PLAYER_ANIM_ARRIVAL_CLIP_60,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = MIST_PARKING_PLAYER_SLOW_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays player extension clip 14 at its arrival-conversation cue.
///
/// EVS selects the equipped weapon bank in a copy; the stored bank seed is unused.
/// The copied parking extension and its clips must stay loaded during playback.
static AnimationPlayRequest _gMistParkingPlayerArrivalClip61Request = {
    .source               = { .index = 1 },
    .animationId          = MIST_PARKING_PLAYER_ANIM_ARRIVAL_CLIP_61,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = MIST_PARKING_PLAYER_SLOW_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Unreferenced player conversation request for clip 62.
///
/// Retained in the room image; no script or dispatcher selects this storage.
/// Stores source index 1; a weapon-animation opcode would select a bank in a copy.
/// Blend durations are whole animation frames.
static const AnimationPlayRequest _gMistParkingUnusedPlayerClip62Request SECTION(".data") = {
    .source               = { .index = MIST_PARKING_PLAYER_REQUEST_BANK_SEED },
    .animationId          = MIST_PARKING_PLAYER_ANIM_UNUSED_CLIP_62,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = MIST_PARKING_PLAYER_DIALOGUE_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays player extension clip 16 at its arrival-conversation cue.
///
/// EVS selects the equipped weapon bank in a copy; the stored bank seed is unused.
/// The copied parking extension and its clips must stay loaded during playback.
static AnimationPlayRequest _gMistParkingPlayerArrivalClip63Request = {
    .source               = { .index = 1 },
    .animationId          = MIST_PARKING_PLAYER_ANIM_ARRIVAL_CLIP_63,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = MIST_PARKING_PLAYER_DIALOGUE_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays player extension clip 17 during arrival exchanges, follow-ups and the departure prompt.
///
/// EVS replaces source.index with the equipped weapon's bank before dispatch.
/// The copied parking extension and its clips must remain loaded for playback.
static AnimationPlayRequest _gMistParkingPlayerTalkAnimation = {
    .source               = { .index = 1 },
    .animationId          = MIST_PARKING_PLAYER_ANIM_TALK,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 6,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Unreferenced player conversation request for clip 65.
///
/// Retained in the room image; no script or dispatcher selects this storage.
/// Stores source index 1; a weapon-animation opcode would select a bank in a copy.
/// Blend durations are whole animation frames.
static const AnimationPlayRequest _gMistParkingUnusedPlayerClip65Request SECTION(".data") = {
    .source               = { .index = MIST_PARKING_PLAYER_REQUEST_BANK_SEED },
    .animationId          = MIST_PARKING_PLAYER_ANIM_UNUSED_CLIP_65,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = MIST_PARKING_PLAYER_DIALOGUE_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays player extension clip 19 at its arrival-conversation cue.
///
/// EVS selects the equipped weapon bank in a copy; the stored bank seed is unused.
/// The copied parking extension and its clips must stay loaded during playback.
static AnimationPlayRequest _gMistParkingPlayerArrivalClip66Request = {
    .source               = { .index = 1 },
    .animationId          = MIST_PARKING_PLAYER_ANIM_ARRIVAL_CLIP_66,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = MIST_PARKING_PLAYER_DIALOGUE_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays player extension clip 20 at its arrival-conversation cue.
///
/// EVS selects the equipped weapon bank in a copy; the stored bank seed is unused.
/// The copied parking extension and its clips must stay loaded during playback.
static AnimationPlayRequest _gMistParkingPlayerArrivalClip67Request = {
    .source               = { .index = 1 },
    .animationId          = MIST_PARKING_PLAYER_ANIM_ARRIVAL_CLIP_67,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = MIST_PARKING_PLAYER_DIALOGUE_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays player extension clip 21 for staying here or going to the shooting gallery.
///
/// EVS replaces source.index with the equipped weapon's bank before dispatch.
/// The copied parking extension and its clips must remain loaded for playback.
static AnimationPlayRequest _gMistParkingPlayerStayOrGalleryReplyAnimation = {
    .source               = { .index = 1 },
    .animationId          = MIST_PARKING_PLAYER_ANIM_STAY_OR_GALLERY_REPLY,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 6,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Unreferenced player conversation request for clip 69.
///
/// Retained in the room image; no script or dispatcher selects this storage.
/// Stores source index 1; a weapon-animation opcode would select a bank in a copy.
/// Blend durations are whole animation frames.
static const AnimationPlayRequest _gMistParkingUnusedPlayerClip69Request SECTION(".data") = {
    .source               = { .index = MIST_PARKING_PLAYER_REQUEST_BANK_SEED },
    .animationId          = MIST_PARKING_PLAYER_ANIM_UNUSED_CLIP_69,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays player extension clip 23 near the arrival closing and the Dryfield departure reply.
///
/// EVS replaces source.index with the equipped weapon's bank before dispatch.
/// The copied parking extension and its clips must remain loaded for playback.
static AnimationPlayRequest _gMistParkingPlayerDepartureReplyAnimation = {
    .source               = { .index = 1 },
    .animationId          = MIST_PARKING_PLAYER_ANIM_DEPARTURE_REPLY,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 6,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays player extension clip 24 at its arrival-conversation cue.
///
/// EVS selects the equipped weapon bank in a copy; the stored bank seed is unused.
/// The copied parking extension and its clips must stay loaded during playback.
static AnimationPlayRequest _gMistParkingPlayerArrivalClip71Request = {
    .source               = { .index = 1 },
    .animationId          = MIST_PARKING_PLAYER_ANIM_ARRIVAL_CLIP_71,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = MIST_PARKING_PLAYER_DIALOGUE_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

s8 gMistParkingPlayerAnimationHeadAimFlags[28] = {
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_OFF,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_ON,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_ON,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_ON,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_OFF,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_OFF,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_OFF,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_OFF,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_OFF,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_OFF,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_ON,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_ON,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_ON,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_OFF,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_OFF,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_OFF,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_OFF,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_ON,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_ON,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_ON,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_OFF,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_OFF,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_OFF,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_OFF,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_ON,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_OFF,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_OFF,
    MIST_PARKING_PLAYER_ANIMATION_HEAD_AIM_OFF,
};

/// Player placement at arrival start and walking destination for Dryfield departure.
///
/// Position uses whole room-coordinate units; Euler angles use 4096 units per turn.
/// Borrowed through synchronous placement or walk dispatch; not retained by the receiver.
static ActorTransform _gMistParkingPlayerArrivalAndDepartureTransform = { .pos = { 3310, 0, -3550, 0 }, .rot = { 0, -1024, 0, 0 } };

/// Player placement during the arrival exchanges and after skipping the arrival talk.
///
/// Position uses whole room-coordinate units; Euler angles use 4096 units per turn.
/// Borrowed through synchronous placement or walk dispatch; not retained by the receiver.
static ActorTransform _gMistParkingPlayerArrivalTalkTransform = { .pos = { 4494, 0, -3867, 0 }, .rot = { 0, 1365, 0, 0 } };

/// Player placement after the arrival model cue, before Pierce approaches.
///
/// Position uses whole room-coordinate units; Euler angles use 4096 units per turn.
/// Placement consumes both vectors synchronously and retains no pointer.
static ActorTransform _gMistParkingPlayerArrivalApproachTransform = { .pos = { 3270, 0, -3550, 0 }, .rot = { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } };

/// Player placement for the closing arrival-conversation exchanges.
///
/// Position uses whole room-coordinate units; Euler angles use 4096 units per turn.
/// Placement consumes both vectors synchronously and retains no pointer.
static ActorTransform _gMistParkingPlayerArrivalClosingTransform = { .pos = { 4370, 0, -4000, 0 }, .rot = { 0, 1365, 0, 0 } };

/// Player placement during the departure-menu prompt.
///
/// Position uses whole room-coordinate units; Euler angles use 4096 units per turn.
/// Placement consumes both vectors synchronously and retains no pointer.
static ActorTransform _gMistParkingPlayerDeparturePromptTransform = { .pos = { 4560, 0, -4916, 0 }, .rot = { 0, 1630, 0, 0 } };

/// Unreferenced pierce conversation request for clip 1.
///
/// Retained in the room image; no script or dispatcher selects this storage.
/// Stores body-bank index 0; no visual gesture is inferred from the clip id.
/// Blend durations are whole animation frames.
static const AnimationPlayRequest _gMistParkingUnusedPierceResetRequest SECTION(".data") = {
    .source               = { .index = MIST_PARKING_ACTOR_BODY_ANIMATION_BANK },
    .animationId          = MIST_PARKING_PIERCE_ANIM_REST,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Returns Pierce to body-bank clip 1 between or after conversation exchanges.
///
/// Scene placement 0 receives this borrowed request synchronously; its bank must be loaded.
/// Pierce ignores the world-collision field.
static AnimationPlayRequest _gMistParkingPierceConversationRestAnimation = {
    .source               = { .index = MIST_PARKING_ACTOR_BODY_ANIMATION_BANK },
    .animationId          = MIST_PARKING_PIERCE_ANIM_REST,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 20,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Unreferenced pierce conversation requests for clips 2, 3, 4.
///
/// Retained in the room image; no script or dispatcher selects this storage.
/// Stores body-bank index 0; no visual gesture is inferred from the clip id.
/// Blend durations are whole animation frames.
static const AnimationPlayRequest _gMistParkingUnusedPierceClip2To4Requests[3] SECTION(".data") = {
    { { .index = MIST_PARKING_ACTOR_BODY_ANIMATION_BANK }, MIST_PARKING_PIERCE_ANIM_WALK, ANIMATION_BLEND_INTERPOLATE, MIST_PARKING_ACTOR_DIALOGUE_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = MIST_PARKING_ACTOR_BODY_ANIMATION_BANK }, MIST_PARKING_PIERCE_ANIM_UNUSED_CLIP_3, ANIMATION_BLEND_INTERPOLATE, MIST_PARKING_ACTOR_DIALOGUE_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = MIST_PARKING_ACTOR_BODY_ANIMATION_BANK }, MIST_PARKING_PIERCE_ANIM_UNUSED_CLIP_4, ANIMATION_BLEND_INTERPOLATE, MIST_PARKING_ACTOR_DIALOGUE_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_ENABLE },
};

/// Plays Pierce body-bank clip 5 during the departure-menu prompt.
///
/// Scene placement 0 borrows this request through synchronous dispatch.
/// Body bank 0 and its clips must stay loaded; the world-collision choice is ignored.
static AnimationPlayRequest _gMistParkingPierceDeparturePromptAnimation = {
    .source               = { .index = MIST_PARKING_ACTOR_BODY_ANIMATION_BANK },
    .animationId          = MIST_PARKING_PIERCE_ANIM_DEPARTURE_PROMPT,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = MIST_PARKING_ACTOR_DIALOGUE_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Plays Pierce body-bank clip 6 in the two closing arrival exchanges.
///
/// Scene placement 0 borrows this request through synchronous dispatch.
/// Body bank 0 and its clips must stay loaded; the world-collision choice is ignored.
static AnimationPlayRequest _gMistParkingPierceArrivalClosingAnimation = {
    .source               = { .index = MIST_PARKING_ACTOR_BODY_ANIMATION_BANK },
    .animationId          = MIST_PARKING_PIERCE_ANIM_ARRIVAL_CLOSING,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = MIST_PARKING_ACTOR_DIALOGUE_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Unreferenced pierce conversation request for clip 7.
///
/// Retained in the room image; no script or dispatcher selects this storage.
/// Stores body-bank index 0; no visual gesture is inferred from the clip id.
/// Blend durations are whole animation frames.
static const AnimationPlayRequest _gMistParkingUnusedPierceClip7Request SECTION(".data") = {
    .source               = { .index = MIST_PARKING_ACTOR_BODY_ANIMATION_BANK },
    .animationId          = MIST_PARKING_PIERCE_ANIM_UNUSED_CLIP_7,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = MIST_PARKING_ACTOR_DIALOGUE_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Plays Pierce's body-bank clip 8 for each follow-up conversation.
///
/// Scene placement 0 receives this borrowed request synchronously; its bank must be loaded.
/// Pierce ignores the world-collision field.
static AnimationPlayRequest _gMistParkingPierceFollowupTalkAnimation = {
    .source               = { .index = MIST_PARKING_ACTOR_BODY_ANIMATION_BANK },
    .animationId          = MIST_PARKING_PIERCE_ANIM_FOLLOWUP_TALK,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 20,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Unreferenced pierce conversation requests for clips 9, 10, 11, 12, 13, 14.
///
/// Retained in the room image; no script or dispatcher selects this storage.
/// Stores body-bank index 0; no visual gesture is inferred from the clip id.
/// Blend durations are whole animation frames.
static const AnimationPlayRequest _gMistParkingUnusedPierceClip9To14Requests[6] SECTION(".data") = {
    { { .index = MIST_PARKING_ACTOR_BODY_ANIMATION_BANK }, MIST_PARKING_PIERCE_ANIM_UNUSED_CLIP_9, ANIMATION_BLEND_INTERPOLATE, MIST_PARKING_ACTOR_DIALOGUE_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = MIST_PARKING_ACTOR_BODY_ANIMATION_BANK }, MIST_PARKING_PIERCE_ANIM_UNUSED_CLIP_10, ANIMATION_BLEND_INTERPOLATE, MIST_PARKING_ACTOR_DIALOGUE_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = MIST_PARKING_ACTOR_BODY_ANIMATION_BANK }, MIST_PARKING_PIERCE_ANIM_UNUSED_CLIP_11, ANIMATION_BLEND_INTERPOLATE, MIST_PARKING_ACTOR_DIALOGUE_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = MIST_PARKING_ACTOR_BODY_ANIMATION_BANK }, MIST_PARKING_PIERCE_ANIM_UNUSED_CLIP_12, ANIMATION_BLEND_INTERPOLATE, MIST_PARKING_ACTOR_DIALOGUE_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = MIST_PARKING_ACTOR_BODY_ANIMATION_BANK }, MIST_PARKING_PIERCE_ANIM_UNUSED_CLIP_13, ANIMATION_BLEND_INTERPOLATE, MIST_PARKING_ACTOR_DIALOGUE_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = MIST_PARKING_ACTOR_BODY_ANIMATION_BANK }, MIST_PARKING_PIERCE_ANIM_UNUSED_CLIP_14, ANIMATION_BLEND_INTERPOLATE, MIST_PARKING_ACTOR_DIALOGUE_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_ENABLE },
};

/// Plays Pierce's body-bank clip 15 in each departure-menu reply.
///
/// Scene placement 0 receives this borrowed request synchronously; its bank must be loaded.
/// Pierce ignores the world-collision field.
static AnimationPlayRequest _gMistParkingPierceDepartureReplyAnimation = {
    .source               = { .index = MIST_PARKING_ACTOR_BODY_ANIMATION_BANK },
    .animationId          = MIST_PARKING_PIERCE_ANIM_DEPARTURE_REPLY,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 20,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Unreferenced pierce conversation request for clip 16.
///
/// Retained in the room image; no script or dispatcher selects this storage.
/// Stores body-bank index 0; no visual gesture is inferred from the clip id.
/// Blend durations are whole animation frames.
static const AnimationPlayRequest _gMistParkingUnusedPierceClip16Request SECTION(".data") = {
    .source               = { .index = MIST_PARKING_ACTOR_BODY_ANIMATION_BANK },
    .animationId          = MIST_PARKING_PIERCE_ANIM_UNUSED_CLIP_16,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = MIST_PARKING_ACTOR_DIALOGUE_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Plays Pierce body-bank clip 17 at its arrival-conversation cue.
///
/// Scene placement 0 borrows this request through synchronous dispatch.
/// Body bank 0 and its clips must stay loaded; the world-collision choice is ignored.
static AnimationPlayRequest _gMistParkingPierceArrivalClip17Request = {
    .source               = { .index = MIST_PARKING_ACTOR_BODY_ANIMATION_BANK },
    .animationId          = MIST_PARKING_PIERCE_ANIM_ARRIVAL_CLIP_17,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = MIST_PARKING_ACTOR_DIALOGUE_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Plays Pierce body-bank clip 18 at its arrival-conversation cue.
///
/// Scene placement 0 borrows this request through synchronous dispatch.
/// Body bank 0 and its clips must stay loaded; the world-collision choice is ignored.
static AnimationPlayRequest _gMistParkingPierceArrivalClip18Request = {
    .source               = { .index = MIST_PARKING_ACTOR_BODY_ANIMATION_BANK },
    .animationId          = MIST_PARKING_PIERCE_ANIM_ARRIVAL_CLIP_18,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = MIST_PARKING_ACTOR_DIALOGUE_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Plays Pierce body-bank clip 19 at its arrival-conversation cue.
///
/// Scene placement 0 borrows this request through synchronous dispatch.
/// Body bank 0 and its clips must stay loaded; the world-collision choice is ignored.
static AnimationPlayRequest _gMistParkingPierceArrivalClip19Request = {
    .source               = { .index = MIST_PARKING_ACTOR_BODY_ANIMATION_BANK },
    .animationId          = MIST_PARKING_PIERCE_ANIM_ARRIVAL_CLIP_19,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = MIST_PARKING_ACTOR_DIALOGUE_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Plays Pierce body-bank clip 20 at its arrival-conversation cue.
///
/// Scene placement 0 borrows this request through synchronous dispatch.
/// Body bank 0 and its clips must stay loaded; the world-collision choice is ignored.
static AnimationPlayRequest _gMistParkingPierceArrivalClip20Request = {
    .source               = { .index = MIST_PARKING_ACTOR_BODY_ANIMATION_BANK },
    .animationId          = MIST_PARKING_PIERCE_ANIM_ARRIVAL_CLIP_20,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = MIST_PARKING_ACTOR_DIALOGUE_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Unreferenced pierce conversation requests for clips 21, 22, 23, 24, 25, 26.
///
/// Retained in the room image; no script or dispatcher selects this storage.
/// Stores body-bank index 0; no visual gesture is inferred from the clip id.
/// Blend durations are whole animation frames.
static const AnimationPlayRequest _gMistParkingUnusedPierceClip21To26Requests[6] SECTION(".data") = {
    { { .index = MIST_PARKING_ACTOR_BODY_ANIMATION_BANK }, MIST_PARKING_PIERCE_ANIM_UNUSED_CLIP_21, ANIMATION_BLEND_INTERPOLATE, MIST_PARKING_ACTOR_DIALOGUE_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = MIST_PARKING_ACTOR_BODY_ANIMATION_BANK }, MIST_PARKING_PIERCE_ANIM_UNUSED_CLIP_22, ANIMATION_BLEND_INTERPOLATE, MIST_PARKING_ACTOR_DIALOGUE_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = MIST_PARKING_ACTOR_BODY_ANIMATION_BANK }, MIST_PARKING_PIERCE_ANIM_UNUSED_CLIP_23, ANIMATION_BLEND_INTERPOLATE, MIST_PARKING_ACTOR_DIALOGUE_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = MIST_PARKING_ACTOR_BODY_ANIMATION_BANK }, MIST_PARKING_PIERCE_ANIM_UNUSED_CLIP_24, ANIMATION_BLEND_INTERPOLATE, MIST_PARKING_ACTOR_DIALOGUE_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = MIST_PARKING_ACTOR_BODY_ANIMATION_BANK }, MIST_PARKING_PIERCE_ANIM_UNUSED_CLIP_25, ANIMATION_BLEND_INTERPOLATE, MIST_PARKING_ACTOR_DIALOGUE_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = MIST_PARKING_ACTOR_BODY_ANIMATION_BANK }, MIST_PARKING_PIERCE_ANIM_POST_ARRIVAL, ANIMATION_BLEND_INTERPOLATE, MIST_PARKING_ACTOR_DIALOGUE_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_ENABLE },
};

/// Plays Pierce body-bank clip 27 at its arrival-conversation cue.
///
/// Scene placement 0 borrows this request through synchronous dispatch.
/// Body bank 0 and its clips must stay loaded; the world-collision choice is ignored.
static AnimationPlayRequest _gMistParkingPierceArrivalClip27Request = {
    .source               = { .index = MIST_PARKING_ACTOR_BODY_ANIMATION_BANK },
    .animationId          = MIST_PARKING_PIERCE_ANIM_ARRIVAL_CLIP_27,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = MIST_PARKING_ACTOR_DIALOGUE_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Plays Pierce body-bank clip 28 at its arrival-conversation cue.
///
/// Scene placement 0 borrows this request through synchronous dispatch.
/// Body bank 0 and its clips must stay loaded; the world-collision choice is ignored.
static AnimationPlayRequest _gMistParkingPierceArrivalClip28Request = {
    .source               = { .index = MIST_PARKING_ACTOR_BODY_ANIMATION_BANK },
    .animationId          = MIST_PARKING_PIERCE_ANIM_ARRIVAL_CLIP_28,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = MIST_PARKING_ACTOR_DIALOGUE_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Plays Pierce body-bank clip 29 at its arrival-conversation cue.
///
/// Scene placement 0 borrows this request through synchronous dispatch.
/// Body bank 0 and its clips must stay loaded; the world-collision choice is ignored.
static AnimationPlayRequest _gMistParkingPierceArrivalClip29Request = {
    .source               = { .index = MIST_PARKING_ACTOR_BODY_ANIMATION_BANK },
    .animationId          = MIST_PARKING_PIERCE_ANIM_ARRIVAL_CLIP_29,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = MIST_PARKING_ACTOR_DIALOGUE_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Unreferenced pierce conversation request for clip 30.
///
/// Retained in the room image; no script or dispatcher selects this storage.
/// Stores body-bank index 0; no visual gesture is inferred from the clip id.
/// Blend durations are whole animation frames.
static const AnimationPlayRequest _gMistParkingUnusedPierceClip30Request SECTION(".data") = {
    .source               = { .index = MIST_PARKING_ACTOR_BODY_ANIMATION_BANK },
    .animationId          = MIST_PARKING_PIERCE_ANIM_UNUSED_CLIP_30,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = MIST_PARKING_ACTOR_DIALOGUE_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Plays Pierce body-bank clip 31 at its arrival-conversation cue.
///
/// Scene placement 0 borrows this request through synchronous dispatch.
/// Body bank 0 and its clips must stay loaded; the world-collision choice is ignored.
static AnimationPlayRequest _gMistParkingPierceArrivalClip31Request = {
    .source               = { .index = MIST_PARKING_ACTOR_BODY_ANIMATION_BANK },
    .animationId          = MIST_PARKING_PIERCE_ANIM_ARRIVAL_CLIP_31,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = MIST_PARKING_ACTOR_DIALOGUE_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Unreferenced pierce conversation requests for clips 32, 33.
///
/// Retained in the room image; no script or dispatcher selects this storage.
/// Stores body-bank index 0; no visual gesture is inferred from the clip id.
/// Blend durations are whole animation frames.
static const AnimationPlayRequest _gMistParkingUnusedPierceClip32To33Requests[2] SECTION(".data") = {
    { { .index = MIST_PARKING_ACTOR_BODY_ANIMATION_BANK }, MIST_PARKING_PIERCE_ANIM_UNUSED_CLIP_32, ANIMATION_BLEND_INTERPOLATE, MIST_PARKING_ACTOR_DIALOGUE_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = MIST_PARKING_ACTOR_BODY_ANIMATION_BANK }, MIST_PARKING_PIERCE_ANIM_ARRIVAL_TALK, ANIMATION_BLEND_INTERPOLATE, MIST_PARKING_ACTOR_DIALOGUE_BLEND_FRAMES, ANIMATION_WORLD_COLLISION_ENABLE },
};

/// Plays Pierce body-bank clip 34 at its arrival-conversation cue.
///
/// Scene placement 0 borrows this request through synchronous dispatch.
/// Body bank 0 and its clips must stay loaded; the world-collision choice is ignored.
static AnimationPlayRequest _gMistParkingPierceArrivalClip34Request = {
    .source               = { .index = MIST_PARKING_ACTOR_BODY_ANIMATION_BANK },
    .animationId          = MIST_PARKING_PIERCE_ANIM_ARRIVAL_CLIP_34,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = MIST_PARKING_ACTOR_DIALOGUE_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Unreferenced pierce conversation request for clip 35.
///
/// Retained in the room image; no script or dispatcher selects this storage.
/// Stores body-bank index 0; no visual gesture is inferred from the clip id.
/// Blend durations are whole animation frames.
static const AnimationPlayRequest _gMistParkingUnusedPierceClip35Request SECTION(".data") = {
    .source               = { .index = MIST_PARKING_ACTOR_BODY_ANIMATION_BANK },
    .animationId          = MIST_PARKING_PIERCE_ANIM_UNUSED_CLIP_35,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = MIST_PARKING_ACTOR_DIALOGUE_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce placement at arrival setup, before his body is shown.
///
/// Position uses whole room-coordinate units; Euler angles use 4096 units per turn.
/// Placement consumes both vectors synchronously and retains no pointer.
static ActorTransform _gMistParkingPierceArrivalStartTransform = { .pos = { 6400, 0, -4800, 0 }, .rot = { 0, -ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } };

/// Pierce's arrival-talk placement and first scripted-walk destination.
///
/// Position uses whole room-coordinate units; Euler angles use 4096 units per turn.
/// Borrowed through synchronous placement or walk dispatch; not retained by the receiver.
static ActorTransform _gMistParkingPierceArrivalTalkTransform = { .pos = { 5150, 0, -4450, 0 }, .rot = { 0, -515, 0, 0 } };

/// Pierce's final placement after the arrival conversation, skip or short room entry.
///
/// Position uses whole room-coordinate units; Euler angles use 4096 units per turn.
/// Borrowed through synchronous placement or walk dispatch; not retained by the receiver.
static ActorTransform _gMistParkingPiercePostArrivalTransform = { .pos = { 5030, 0, -5680, 0 }, .rot = { 0, -296, 0, 0 } };

/// Pierce's walking destination when departing for Dryfield; yaw is the final facing.
///
/// Position uses whole room-coordinate units; Euler angles use 4096 units per turn.
/// Borrowed through synchronous placement or walk dispatch; not retained by the receiver.
static ActorTransform _gMistParkingPierceDryfieldDepartureTarget = { .pos = { -1960, 0, -4650, 0 }, .rot = { 0, -1024, 0, 0 } };

/// Pierce placement and facing for the closing arrival exchanges.
///
/// Position uses whole room-coordinate units; Euler angles use 4096 units per turn.
/// Placement consumes both vectors synchronously and retains no pointer.
static ActorTransform _gMistParkingPierceArrivalClosingTransform = { .pos = { 5150, 0, -4450, 0 }, .rot = { 0, -715, 0, 0 } };

/// Pierce's bank-0 walk and arrival clips for his post-conversation placement.
///
/// Clip 2 starts the walk; clip 26 plays at arrival and after the final yaw turn.
/// The walk handler copies both choices during dispatch.
static ActorMotionWalkAnim _gMistParkingPiercePostArrivalWalkClips = {
    .animationId = MIST_PARKING_PIERCE_ANIM_WALK,
    .nextAnimId  = MIST_PARKING_PIERCE_ANIM_POST_ARRIVAL,
};

/// Pierce bank-0 walk and arrival clips for approaching the arrival conversation.
///
/// Bank-0 clip 2 starts the walk; clip 33 plays at arrival and after the final yaw turn.
/// The walk handler copies both choices during synchronous dispatch.
static ActorMotionWalkAnim _gMistParkingPierceArrivalTalkWalkClips = {
    .animationId = MIST_PARKING_PIERCE_ANIM_WALK,
    .nextAnimId  = MIST_PARKING_PIERCE_ANIM_ARRIVAL_TALK,
};

/// Shows Pierce's optional billboard during arrival setup.
///
/// Scene placement 0 borrows the command synchronously; Pierce ignores its context.
/// It has no effect when the placement variant has no billboard task.
static ActorCommand _gMistParkingPierceShowBillboardCommand = {
    .context = { .loc = { 0, 0 } },
    .command = MIST_PARKING_PIERCE_COMMAND_SHOW_BILLBOARD,
};

/// Hides Pierce's optional billboard during arrival setup or skip.
///
/// Scene placement 0 receives the command synchronously; Pierce ignores its context.
static ActorCommand _gMistParkingPierceHideBillboardCommand = {
    .context = { .loc = { 0, 0 } },
    .command = MIST_PARKING_PIERCE_COMMAND_HIDE_BILLBOARD,
};

/// Requests Pierce's head turn toward the player for the conversation.
///
/// Scene placement 0 receives the command synchronously; Pierce ignores its context.
static ActorCommand _gMistParkingPierceAimHeadCommand = {
    .context = { .loc = { 0, 0 } },
    .command = MIST_PARKING_PIERCE_COMMAND_AIM_HEAD,
};

/// Releases Pierce's head turn after arrival or a departure-menu reply.
///
/// Scene placement 0 receives the command synchronously; Pierce ignores its context.
static ActorCommand _gMistParkingPierceReleaseHeadCommand = {
    .context = { .loc = { 0, 0 } },
    .command = MIST_PARKING_PIERCE_COMMAND_RELEASE_HEAD,
};

/// Unreferenced jodie conversation request for clip 1.
///
/// Retained in the room image; no script or dispatcher selects this storage.
/// Stores body-bank index 0; no visual gesture is inferred from the clip id.
/// Blend durations are whole animation frames.
static const AnimationPlayRequest _gMistParkingUnusedJodieResetRequest SECTION(".data") = {
    .source               = { .index = MIST_PARKING_ACTOR_BODY_ANIMATION_BANK },
    .animationId          = MIST_PARKING_JODIE_ANIM_TALK_OPENING,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays Jodie body-bank clip 1 before greeting and prize-choice dialogue.
///
/// Scene placement 1 borrows this request through synchronous dispatch.
/// Body bank 0 and its clips must stay loaded; the world-collision choice is ignored.
static AnimationPlayRequest _gMistParkingJodieTalkOpeningAnimation = {
    .source               = { .index = MIST_PARKING_ACTOR_BODY_ANIMATION_BANK },
    .animationId          = MIST_PARKING_JODIE_ANIM_TALK_OPENING,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = MIST_PARKING_ACTOR_DIALOGUE_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

enum {
    MIST_PARKING_JODIE_UNUSED_CLIP_2 = 2,
    MIST_PARKING_JODIE_UNUSED_CLIP_3 = 3,
    MIST_PARKING_JODIE_UNUSED_CLIP_4 = 4,
};

/// Retained, unused Jodie body-bank requests for clips 2, 3 and 4.
///
/// Clips 2 and 3 store twenty-frame blends; clip 4 stores ten frames.
/// All enable collision, which Jodie ignores. Bank 0 loads all three clips,
/// but no script selects these records; their physical gestures are unproven.
static const AnimationPlayRequest _gMistParkingJodieUnusedAnimations2To4[3] SECTION(".data") = {
    {
        .source               = { .index = MIST_PARKING_ACTOR_BODY_ANIMATION_BANK },
        .animationId          = MIST_PARKING_JODIE_UNUSED_CLIP_2,
        .blend                = ANIMATION_BLEND_INTERPOLATE,
        .blendFrames          = MIST_PARKING_ACTOR_DIALOGUE_BLEND_FRAMES,
        .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
    },
    {
        .source               = { .index = MIST_PARKING_ACTOR_BODY_ANIMATION_BANK },
        .animationId          = MIST_PARKING_JODIE_UNUSED_CLIP_3,
        .blend                = ANIMATION_BLEND_INTERPOLATE,
        .blendFrames          = MIST_PARKING_ACTOR_DIALOGUE_BLEND_FRAMES,
        .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
    },
    {
        .source               = { .index = MIST_PARKING_ACTOR_BODY_ANIMATION_BANK },
        .animationId          = MIST_PARKING_JODIE_UNUSED_CLIP_4,
        .blend                = ANIMATION_BLEND_INTERPOLATE,
        .blendFrames          = MIST_PARKING_JODIE_GREETING_BLEND_FRAMES,
        .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
    },
};

/// Returns Jodie to body-bank clip 5 at room entry and after shop replies.
///
/// Scene placement 1 receives this borrowed request synchronously; its bank must be loaded.
/// Jodie ignores the world-collision field.
static AnimationPlayRequest _gMistParkingJodieConversationRestAnimation = {
    .source               = { .index = MIST_PARKING_ACTOR_BODY_ANIMATION_BANK },
    .animationId          = MIST_PARKING_JODIE_ANIM_REST,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 20,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

enum {
    MIST_PARKING_JODIE_UNUSED_CLIP_6 = 6,
};

/// Retained, unused Jodie body-bank reset request for clip 6.
///
/// Stores zero blend frames and enabled collision, which Jodie ignores.
/// Bank 0 loads clip 6, but no script selects this record; its physical
/// gesture and intended scene use are unproven.
static const AnimationPlayRequest _gMistParkingJodieUnusedAnimation6 SECTION(".data") = {
    .source               = { .index = MIST_PARKING_ACTOR_BODY_ANIMATION_BANK },
    .animationId          = MIST_PARKING_JODIE_UNUSED_CLIP_6,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Jodie's bank-0 clip-7 greeting pose after the talk's opening delay.
///
/// Scene placement 1 borrows the request synchronously. Slots blend for ten
/// whole frames when already ticking; selecting the same clip preserves it.
/// The receiver ignores the collision word. Its bank and clips must stay loaded.
static const AnimationPlayRequest _gMistParkingJodieTalkGreetingAnimation SECTION(".data") = {
    .source               = { .index = MIST_PARKING_ACTOR_BODY_ANIMATION_BANK },
    .animationId          = MIST_PARKING_JODIE_ANIM_GREETING,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = MIST_PARKING_JODIE_GREETING_BLEND_FRAMES,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Plays Jodie's body-bank clip 8 at the start of both shop-choice reply scripts.
///
/// Scene placement 1 receives this borrowed request synchronously; its bank must be loaded.
/// Jodie ignores the world-collision field.
static AnimationPlayRequest _gMistParkingJodieShopReplyAnimation = {
    .source               = { .index = MIST_PARKING_ACTOR_BODY_ANIMATION_BANK },
    .animationId          = MIST_PARKING_JODIE_ANIM_SHOP_REPLY,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 5,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Plays Jodie's body-bank clip 9 at the closing cue of both shop-choice replies.
///
/// Scene placement 1 receives this borrowed request synchronously; its bank must be loaded.
/// Jodie ignores the world-collision field.
static AnimationPlayRequest _gMistParkingJodieShopReplyClosingAnimation = {
    .source               = { .index = MIST_PARKING_ACTOR_BODY_ANIMATION_BANK },
    .animationId          = MIST_PARKING_JODIE_ANIM_SHOP_REPLY_CLOSING,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 15,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Jodie's shop placement installed during arrival setup and short room entry.
///
/// Position uses whole room-coordinate units; Euler angles use 4096 units per turn.
/// Borrowed through synchronous placement or walk dispatch; not retained by the receiver.
static ActorTransform _gMistParkingJodieShopTransform = { .pos = { 8460, 0, 92, 0 }, .rot = { 0, 2047, 0, 0 } };

/// Null-terminated Shift-JIS option label "まだよ (not yet)"; eight bytes of storage.
static u8 _gMistParkingOptionNotYetLabel[8] = "\x82\xdc\x82\xbe\x82\xe6";

/// Null-terminated Shift-JIS option label "ええ (yes)"; eight bytes of storage.
static u8 _gMistParkingOptionEeLabel[8] = "\x82\xa6\x82\xa6";

/// Null-terminated Shift-JIS option label "はい (yes)"; eight bytes of storage.
static u8 _gMistParkingOptionHaiLabel[8] = "\x82\xcd\x82\xa2";

/// Null-terminated Shift-JIS option label "いいえ (no)"; eight bytes of storage.
static u8 _gMistParkingOptionNoLabel[8] = "\x82\xa2\x82\xa2\x82\xa6";

/// Two ordered label pairs borrowed by the modal option dialog.
///
/// Pair 0 is not-yet/yes; pair 1 is yes/no. Only selector 1 chooses pair 1;
/// every other selector uses pair 0. Each row supplies the two dialog options.
/// The strings remain in the loaded parking overlay through dialog closure.
static u8* _gMistParkingOptionLabelPairs[2][2] = {
    { _gMistParkingOptionNotYetLabel, _gMistParkingOptionEeLabel },
    { _gMistParkingOptionHaiLabel, _gMistParkingOptionNoLabel },
};

EvsCommand gMistParkingArrivalConversationScript[155] = {
    // Establish the player bank and hidden/shown actor placements for arrival.
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = MIST_PARKING_ARRIVAL_AMBIENT_CHANNEL }, { .value = MIST_PARKING_ARRIVAL_AMBIENT_CHANNEL }, { .value = MIST_PARKING_ARRIVAL_AMBIENT_CHANNEL }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mistParkingStartPlayerHeadAim }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &gMistParkingPlayerConversationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gMistParkingPlayerArrivalAndDepartureTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = MIST_PARKING_SCENE_MODEL_HIDE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gMistParkingPierceArrivalStartTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_JODIE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = MIST_PARKING_SCENE_MODEL_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_JODIE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gMistParkingJodieShopTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gMistParkingPierceShowBillboardCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_JODIE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistParkingJodieConversationRestAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = MIST_PARKING_VIEW_ARRIVAL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = MIST_PARKING_CAP_ARRIVAL }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // CAP cues bracket the temporary model and its sound.
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mistParkingSpawnCutsceneModel }, { .value = MIST_PARKING_CUTSCENE_MODEL_DESCRIPTOR_INDEX }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMistParkingPlayerCutsceneModelCueAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = MIST_PARKING_ARRIVAL_SOUND_ENTRY_05 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mistParkingReleaseCutsceneModel }, { .value = MIST_PARKING_CUTSCENE_MODEL_DESCRIPTOR_INDEX }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gMistParkingPlayerArrivalApproachTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMistParkingPlayerArrivalClip52Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = MIST_PARKING_SCENE_MODEL_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_WALK_TO }, { .message = { .transform = &_gMistParkingPierceArrivalTalkTransform } }, { .message = { .pointer = &_gMistParkingPierceArrivalTalkWalkClips } } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 31 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = MIST_PARKING_ARRIVAL_SOUND_ENTRY_06 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMistParkingPlayerArrivalClip53Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMistParkingPlayerArrivalClip54Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistParkingPierceArrivalClip28Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gMistParkingPierceArrivalTalkTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = MIST_PARKING_ARRIVAL_SOUND_ENTRY_0C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = MIST_PARKING_ARRIVAL_SOUND_ENTRY_0C }, { .value = SOUND_SCRIPT_STOP_KEEP_RELEASE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMistParkingPlayerArrivalClip55Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = MIST_PARKING_ARRIVAL_SOUND_ENTRY_0D }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistParkingPierceArrivalClip27Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistParkingPierceArrivalClip34Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMistParkingPlayerArrivalClip56Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = MIST_PARKING_ARRIVAL_SOUND_ENTRY_11 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gMistParkingPlayerArrivalTalkTransform } }, { .value = 0 } },
    // Stage the main exchanges; CAP cues advance the conversation poses.
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMistParkingPlayerArrivalTalkResetAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistParkingPierceArrivalClip17Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 12 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = MIST_PARKING_ARRIVAL_SOUND_ENTRY_08 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gMistParkingPlayerArrivalTalkTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistParkingPierceArrivalClip18Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMistParkingPlayerArrivalClip57Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gMistParkingPierceAimHeadCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 28 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = MIST_PARKING_ARRIVAL_SOUND_ENTRY_0E }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = MIST_PARKING_SCENE_MODEL_HIDE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gMistParkingPierceHideBillboardCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = MIST_PARKING_SCENE_MODEL_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMistParkingPlayerTalkAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistParkingPierceArrivalClip20Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMistParkingPlayerArrivalTalkResetAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistParkingPierceConversationRestAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMistParkingPlayerArrivalClip66Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistParkingPierceArrivalClip27Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMistParkingPlayerArrivalCueAnimation50 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMistParkingPlayerArrivalClip67Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistParkingPierceArrivalClip29Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMistParkingPlayerTalkAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistParkingPierceArrivalClip31Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMistParkingPlayerArrivalClip66Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistParkingPierceArrivalClip27Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMistParkingPlayerArrivalClip60Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistParkingPierceConversationRestAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMistParkingPlayerArrivalClip61Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistParkingPierceArrivalClip34Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMistParkingPlayerArrivalClip67Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gMistParkingPlayerArrivalClosingTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMistParkingPlayerTalkAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistParkingPierceArrivalClip19Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMistParkingPlayerArrivalTalkResetAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMistParkingPlayerArrivalClip67Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gMistParkingPierceArrivalClosingTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistParkingPierceConversationRestAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistParkingPierceArrivalClosingAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistParkingPierceConversationRestAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gMistParkingPlayerArrivalTalkTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMistParkingPlayerDepartureReplyAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gMistParkingPierceArrivalTalkTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMistParkingPlayerArrivalClip71Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMistParkingPlayerConversationResetAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistParkingPierceArrivalClosingAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMistParkingPlayerConversationResetAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMistParkingPlayerArrivalClip67Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistParkingPierceConversationRestAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMistParkingPlayerArrivalClip63Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = mistParkingControlPlayerHeadAim }, { .value = MIST_PARKING_HEAD_AIM_FORCE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = MIST_PARKING_FADE_BLEND_AVERAGE }, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 90 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Restore ordinary presentation before marking the intro complete.
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = mistParkingControlPlayerHeadAim }, { .value = MIST_PARKING_HEAD_AIM_STOP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = mistParkingSetPierceCollisionPatchLowered }, { .value = MIST_PARKING_PIERCE_PATCH_RESTORED }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_WALK_TO }, { .message = { .transform = &_gMistParkingPiercePostArrivalTransform } }, { .message = { .pointer = &_gMistParkingPiercePostArrivalWalkClips } } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gMistParkingPierceReleaseHeadCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = MIST_PARKING_VIEW_POST_ARRIVAL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 120 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = mistParkingSetConversationProgress }, { .value = MIST_PARKING_CONVERSATION_INTRO_COMPLETE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gMistParkingArrivalSkipScript[23] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = MIST_PARKING_FADE_BLEND_AVERAGE }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_DIRTY_VIEW, { .value = MIST_PARKING_VIEW_POST_ARRIVAL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mistParkingReleaseCutsceneModel }, { .value = MIST_PARKING_CUTSCENE_MODEL_DESCRIPTOR_INDEX }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = mistParkingControlPlayerHeadAim }, { .value = MIST_PARKING_HEAD_AIM_STOP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = mistParkingSetPierceCollisionPatchLowered }, { .value = MIST_PARKING_PIERCE_PATCH_RESTORED }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gMistParkingPlayerArrivalTalkTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = MIST_PARKING_SCENE_MODEL_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gMistParkingPierceArrivalTalkTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gMistParkingPierceHideBillboardCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_WALK_TO }, { .message = { .transform = &_gMistParkingPiercePostArrivalTransform } }, { .message = { .pointer = &_gMistParkingPiercePostArrivalWalkClips } } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gMistParkingPierceReleaseHeadCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 100 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = mistParkingSetConversationProgress }, { .value = MIST_PARKING_CONVERSATION_INTRO_COMPLETE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gMistParkingArrivalRoomEntryScript[8] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = MIST_PARKING_SCENE_MODEL_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gMistParkingPierceArrivalTalkTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gMistParkingPierceHideBillboardCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_WALK_TO }, { .message = { .transform = &_gMistParkingPiercePostArrivalTransform } }, { .message = { .pointer = &_gMistParkingPiercePostArrivalWalkClips } } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_JODIE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = MIST_PARKING_SCENE_MODEL_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_JODIE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gMistParkingJodieShopTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_JODIE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistParkingJodieConversationRestAnimation } }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gMistParkingFirstFollowupScript[10] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mistParkingStartPlayerHeadAim }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &gMistParkingPlayerConversationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mistParkingStartPackedDialogue }, { .value = MIST_PARKING_CAP_FIRST_FOLLOWUP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMistParkingPlayerTalkAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistParkingPierceFollowupTalkAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = mistParkingControlPlayerHeadAim }, { .value = MIST_PARKING_HEAD_AIM_STOP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistParkingPierceConversationRestAnimation } }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gMistParkingSecondFollowupScript[10] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mistParkingStartPlayerHeadAim }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &gMistParkingPlayerConversationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mistParkingStartPackedDialogue }, { .value = MIST_PARKING_CAP_SECOND_FOLLOWUP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMistParkingPlayerTalkAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistParkingPierceFollowupTalkAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = mistParkingControlPlayerHeadAim }, { .value = MIST_PARKING_HEAD_AIM_STOP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistParkingPierceConversationRestAnimation } }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Retained, unused third Pierce follow-up conversation script.
///
/// Selects CAP command 15, variant 2; installs player talk clips, starts head
/// aim and poses Pierce, then waits for a CAP cue before restoring them.
/// The ten commands end at END and borrow this overlay's payloads/callbacks
/// if executed. No dispatcher selects this variant.
static EvsCommand _gMistParkingUnusedThirdFollowupScript[10] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mistParkingStartPlayerHeadAim }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &gMistParkingPlayerConversationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mistParkingStartPackedDialogue }, { .value = MIST_PARKING_CAP_FOLLOWUP_VARIANT_2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMistParkingPlayerTalkAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistParkingPierceFollowupTalkAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = mistParkingControlPlayerHeadAim }, { .value = MIST_PARKING_HEAD_AIM_STOP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistParkingPierceConversationRestAnimation } }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Presents Pierce's departure menu and its player/actor poses.
///
/// Installs the player extension, starts CAP slot 5 and yields at its cues.
/// Leaves head aim and scripted control held for the selected reply.
/// END terminates the fixed command array. Commands, payloads, callbacks and
/// animation/CAP resources remain borrowed while their script or playback runs.
static EvsCommand _gMistParkingDeparturePromptScript[13] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mistParkingStartPlayerHeadAim }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &gMistParkingPlayerConversationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gMistParkingPierceAimHeadCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = mistParkingSelectDialogueResource }, { .value = MIST_PARKING_DIALOGUE_DEPARTURE_MENU }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = MIST_PARKING_CAP_DEPARTURE_MENU }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gMistParkingPlayerDeparturePromptTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMistParkingPlayerTalkAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistParkingPierceDeparturePromptAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Plays the stay-here reply and restores the normal conversation view.
///
/// CAP command 5/key 1 accompanies player clip 68 and Pierce clip 15.
/// Releases head aim and restores the default CAP resource; the caller resumes control.
/// END terminates the fixed command array. Commands, payloads, callbacks and
/// animation/CAP resources remain borrowed while their script or playback runs.
static EvsCommand _gMistParkingStayReplyScript[13] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &gMistParkingPlayerConversationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mistParkingStartPackedDialogue }, { .value = MIST_PARKING_CAP_DEPARTURE_STAY }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMistParkingPlayerStayOrGalleryReplyAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistParkingPierceDepartureReplyAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = mistParkingControlPlayerHeadAim }, { .value = MIST_PARKING_HEAD_AIM_STOP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMistParkingPlayerStayOrGalleryReplyAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gMistParkingPierceReleaseHeadCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = MIST_PARKING_VIEW_POST_ARRIVAL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = mistParkingSelectDialogueResource }, { .value = MIST_PARKING_DIALOGUE_DEFAULT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Plays the Dryfield reply, walks both actors away and queues departure.
///
/// CAP command 5/key 2 accompanies the reply, followed by a fade, normal
/// departure movie and the task that prepares the Dryfield loadout.
/// The room and movie resources must survive the queued transition tasks.
/// END terminates the fixed command array. Commands, payloads, callbacks and
/// animation/CAP resources remain borrowed while their script or playback runs.
static EvsCommand _gMistParkingDryfieldDepartureScript[24] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &gMistParkingPlayerConversationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mistParkingStartPackedDialogue }, { .value = MIST_PARKING_CAP_DEPARTURE_DRYFIELD }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMistParkingPlayerDepartureReplyAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistParkingPierceDepartureReplyAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = mistParkingControlPlayerHeadAim }, { .value = MIST_PARKING_HEAD_AIM_STOP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_MOVE_TO }, { .message = { .transform = &_gMistParkingPlayerArrivalAndDepartureTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_WALK_TO }, { .message = { .transform = &_gMistParkingPierceDryfieldDepartureTarget } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &_gMistParkingPierceReleaseHeadCommand } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = MIST_PARKING_FADE_BLEND_AVERAGE }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CANCEL_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = mistParkingQueueDelayedDisplayModeExit }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_RELEASE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = MIST_PARKING_SCENE_MODEL_HIDE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = mistParkingSelectDialogueResource }, { .value = MIST_PARKING_DIALOGUE_DEFAULT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_HIDE_HUD }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mistParkingStartDepartureMovie }, { .value = MIST_PARKING_DEPARTURE_MOVIE_NORMAL }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mistParkingQueueDryfieldDeparture }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Plays the shooting-gallery reply and queues the Acropolis area reload.
///
/// CAP command 5/key 3 accompanies the reply; after fading it releases head
/// aim, queues the shooting-gallery entrance and restores the default CAP resource.
/// END terminates the fixed command array. Commands, payloads, callbacks and
/// animation/CAP resources remain borrowed while their script or playback runs.
static EvsCommand _gMistParkingShootingGalleryDepartureScript[16] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &gMistParkingPlayerConversationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mistParkingStartPackedDialogue }, { .value = MIST_PARKING_CAP_DEPARTURE_SHOOTING_GALLERY }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMistParkingPlayerStayOrGalleryReplyAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistParkingPierceConversationRestAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_PIERCE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistParkingPierceDepartureReplyAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = MIST_PARKING_FADE_BLEND_AVERAGE }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = mistParkingControlPlayerHeadAim }, { .value = MIST_PARKING_HEAD_AIM_STOP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CANCEL_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = mistParkingQueueDelayedDisplayModeExit }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mistParkingReloadAcropolisArea }, { .value = GAME_AREA_MIST_SHOOTING_GALLERY }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = mistParkingSelectDialogueResource }, { .value = MIST_PARKING_DIALOGUE_DEFAULT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Stages Jodie's greeting and prize-dialogue poses outside variant 1.
///
/// Installs the player extension and resets its clip 48; Jodie at placement 1
/// plays clip 1, then clip 7 after twenty interpreter-countdown updates.
/// The caller starts the selected CAP command; this script yields at its cue.
/// END terminates the fixed command array. Commands, payloads, callbacks and
/// animation/CAP resources remain borrowed while their script or playback runs.
static EvsCommand _gMistParkingJodieTalkPoseScript[7] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &gMistParkingPlayerConversationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMistParkingPlayerConversationResetAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_JODIE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistParkingJodieTalkOpeningAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = MIST_PARKING_SHOP_POSE_DELAY_UPDATES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_JODIE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistParkingJodieTalkGreetingAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Queues the MIST shop session and stages Jodie's reply and closing poses.
///
/// Stock profile 0x10 is requested before the player reset and Jodie clips 8/9.
/// After the CAP cue and closing delay, Jodie returns to clip 5.
/// Shop task completion belongs to the queued display session.
/// END terminates the fixed command array. Commands, payloads, callbacks and
/// animation/CAP resources remain borrowed while their script or playback runs.
static EvsCommand _gMistParkingJodieShopSessionScript[10] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &gMistParkingPlayerConversationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackResult = shopOpenSession }, { .value = MIST_PARKING_SHOP_STOCK_PROFILE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = MIST_PARKING_SHOP_POSE_DELAY_UPDATES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMistParkingPlayerConversationResetAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_JODIE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistParkingJodieShopReplyAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_JODIE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistParkingJodieShopReplyClosingAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = MIST_PARKING_SHOP_REPLY_END_UPDATES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_JODIE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistParkingJodieConversationRestAnimation } }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Stages Jodie's reply and closing poses when the player declines the shop.
///
/// Resets the player extension clip 48, plays Jodie clips 8/9 around the CAP
/// cue and returns her to clip 5 after the closing delay.
/// END terminates the fixed command array. Commands, payloads, callbacks and
/// animation/CAP resources remain borrowed while their script or playback runs.
static EvsCommand _gMistParkingJodieShopDeclineScript[8] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &gMistParkingPlayerConversationBankCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gMistParkingPlayerConversationResetAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_JODIE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistParkingJodieShopReplyAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_JODIE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistParkingJodieShopReplyClosingAnimation } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = MIST_PARKING_SHOP_REPLY_END_UPDATES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = MIST_PARKING_JODIE_SCENE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gMistParkingJodieConversationRestAnimation } }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Item ids for shooting-gallery courses 0..4, paired with their quantities.
///
/// Courses 0..3 use these words for storage grants. Course 4's Shoulder Holster
/// is retained here but granted separately after checking its ownership limit.
/// Signed-word catalog ids stay in the authored data section.
static const s32 _gMistParkingCoursePrizeItemIds[5] SECTION(".data") = {
    MIST_PARKING_COURSE_PRIZE_9MM_HYDRA,
    MIST_PARKING_COURSE_PRIZE_RINGERS_SOLUTION,
    MIST_PARKING_COURSE_PRIZE_HUNTER_GOGGLES,
    MIST_PARKING_COURSE_PRIZE_LIPSTICK,
    MIST_PARKING_COURSE_PRIZE_SHOULDER_HOLSTER,
};

/// Item units per course prize: fifty Hydra rounds, then one of each item.
///
/// Same course order and extent as `_gMistParkingCoursePrizeItemIds`; only
/// courses 0..3 read this array. The last quantity is retained metadata.
/// Signed words match `inventoryGiveItem`'s quantity contract; storage stays in .data.
static const s32 _gMistParkingCoursePrizeQuantities[ARRAY_SIZE(_gMistParkingCoursePrizeItemIds)] SECTION(".data") = {
    MIST_PARKING_COURSE_PRIZE_AMMO_ROUNDS,
    MIST_PARKING_COURSE_PRIZE_ONE_ITEM,
    MIST_PARKING_COURSE_PRIZE_ONE_ITEM,
    MIST_PARKING_COURSE_PRIZE_ONE_ITEM,
    MIST_PARKING_COURSE_PRIZE_ONE_ITEM,
};

/// Advances one course prize's announcement while CAP is idle.
///
/// Borrows the singleton cursor and a live talk in its announcement state.
/// Requires course index 0..4 and timer 1..10, initialized to index 0/timer 10
/// before the first turn. Waiting prizes start their caption at five ticks left.
/// At zero, an unavailable placed prize closes its saved prize flag and the
/// cursor resets to ten ticks. Finishing course 4 leaves index 5 and advances
/// the talk, which must stop calling this helper. CAP playback pauses the timer.
static inline void _mistParkingAdvancePrizeAnnouncement(Task* task, MistParkingPrizeAnnouncementState* announcement)
{
    s16 announcementCourse;

    if (capIsBusy() != 0) {
        return;
    }
    announcement->prizeTimer--;
    if (announcement->prizeTimer == MIST_PARKING_PRIZE_ANNOUNCEMENT_CAPTION_FRAME) {
        announcementCourse = announcement->prizeIndex;
        if (gameFlagGetNibble(announcementCourse + MIST_PARKING_PRIZE_FLAG_FIRST) == MIST_PARKING_PRIZE_WAITING) {
            capStartSequenceSlot(MIST_PARKING_PRIZE_CAPTION_SLOT, CAP_PLAYBACK_IN_PLACE, announcementCourse);
        }
        return;
    }
    if (announcement->prizeTimer != 0) {
        return;
    }
    announcementCourse = announcement->prizeIndex;
    if (areaGetCurrentObjectState(announcementCourse + MIST_PARKING_PRIZE_OBJECT_FIRST) != MIST_PARKING_PRIZE_OBJECT_AVAILABLE) {
        gameFlagSetNibble(announcementCourse + MIST_PARKING_PRIZE_FLAG_FIRST, MIST_PARKING_PRIZE_CLOSED);
    }
    announcement->prizeTimer = MIST_PARKING_PRIZE_ANNOUNCEMENT_FRAMES;
    announcement->prizeIndex++;
    if (announcement->prizeIndex >= (s32)ARRAY_SIZE(_gMistParkingCoursePrizeItemIds)) {
        task->state++;
    }
}

/// Runs Jodie's prize collection and shop conversation outside area variant 1.
///
/// Requires player control already held, live scene actors and the loaded default
/// and prize CAP resources. States 0..10 drive the exchange. State 0 clears the
/// shared announcement cursor; only one talk may run at a time.
/// Keys 1, 3 and 4 announce, store or close the waiting
/// prizes; the final menu opens the shop on key 1. The signed `killCountdown`
/// counts callback ticks before the menu and farewell. Completion resumes player
/// control and releases this bodyless task; spawn arguments and work are unused.
static void _mistParkingJodiePrizeAndShopTalkTask(Task* task)
{
    enum {
        MIST_PARKING_JODIE_TALK_GREET               = 0,
        MIST_PARKING_JODIE_TALK_CHECK_PRIZES        = 1,
        MIST_PARKING_JODIE_TALK_START_PRIZE_PROMPT  = 2,
        MIST_PARKING_JODIE_TALK_PRIZE_CHOICE        = 3,
        MIST_PARKING_JODIE_TALK_ANNOUNCE_PRIZES     = 4,
        MIST_PARKING_JODIE_TALK_CLOSE_ANNOUNCEMENTS = 5,
        MIST_PARKING_JODIE_TALK_WAIT_PRIZE_CLOSING  = 6,
        MIST_PARKING_JODIE_TALK_DELAY_MENU          = 7,
        MIST_PARKING_JODIE_TALK_SHOP_CHOICE         = 8,
        MIST_PARKING_JODIE_TALK_WAIT_SHOP_EXIT      = 9,
        MIST_PARKING_JODIE_TALK_FINISH              = 10,
        MIST_PARKING_JODIE_CHOICE_ANNOUNCE_PRIZES   = 1,
        MIST_PARKING_JODIE_CHOICE_STORE_PRIZES      = 3,
        MIST_PARKING_JODIE_CHOICE_CLOSE_PRIZES      = 4,
        MIST_PARKING_JODIE_CHOICE_OPEN_SHOP         = 1,
        MIST_PARKING_JODIE_CAP_GREETING             = 2,
        MIST_PARKING_JODIE_CAP_MENU                 = 4,
        MIST_PARKING_JODIE_CAP_FAREWELL             = 3,
        MIST_PARKING_PRIZE_CAP_PROMPT               = 6,
        MIST_PARKING_PRIZE_CAP_BEGIN_ANNOUNCEMENTS  = 7,
        MIST_PARKING_PRIZE_CAP_CLOSE_WITHOUT_ITEMS  = 8,
        MIST_PARKING_PRIZE_CAP_ITEMS_STORED         = 4,
        MIST_PARKING_PRIZE_CAP_END_ANNOUNCEMENTS    = 2,
        MIST_PARKING_PRIZE_COUNT                    = ARRAY_SIZE(_gMistParkingCoursePrizeItemIds),
        MIST_PARKING_PRIZE_STORAGE_RANGE            = 3,
        MIST_PARKING_PRIZE_SHOULDER_HOLSTER_ITEM_ID = 0x6C,
        MIST_PARKING_PRIZE_ARMOR_QUANTITY           = 1,
        MIST_PARKING_JODIE_MENU_DELAY_TICKS         = 11,
        MIST_PARKING_JODIE_FAREWELL_TICK            = 10
    };
    s32                                courseIndex;
    s32                                prizeFlag;
    MistParkingPrizeAnnouncementState* announcement = &gMistParkingPrizeAnnouncement;

    switch (task->state) {
        case MIST_PARKING_JODIE_TALK_GREET:
            memFillBytes(announcement, 0, sizeof(*announcement));
            evsStartScript(_gMistParkingJodieTalkPoseScript, EVENT_SCRIPT_HUD_KEEP);
            capRunCommand(MIST_PARKING_JODIE_CAP_GREETING, CAP_PLAYBACK_IN_PLACE);
            task->state++;
            break;
        case MIST_PARKING_JODIE_TALK_CHECK_PRIZES:
            if (gGameSession->eventState != 0) {
                return;
            }
            if (capIsBusy() != 0) {
                return;
            }
            for (courseIndex = 0; courseIndex < MIST_PARKING_PRIZE_COUNT; courseIndex++) {
                if (gameFlagGetNibble(courseIndex + MIST_PARKING_PRIZE_FLAG_FIRST) == MIST_PARKING_PRIZE_WAITING) {
                    task->state = MIST_PARKING_JODIE_TALK_START_PRIZE_PROMPT;
                    return;
                }
            }
            task->state = MIST_PARKING_JODIE_TALK_WAIT_PRIZE_CLOSING;
            break;
        case MIST_PARKING_JODIE_TALK_START_PRIZE_PROMPT:
            mistParkingSelectDialogueResource(MIST_PARKING_DIALOGUE_PRIZES);
            evsStartScript(_gMistParkingJodieTalkPoseScript, EVENT_SCRIPT_HUD_KEEP);
            capRunCommand(MIST_PARKING_PRIZE_CAP_PROMPT, CAP_PLAYBACK_IN_PLACE);
            task->state++;
            break;
        case MIST_PARKING_JODIE_TALK_PRIZE_CHOICE:
            if (gGameSession->eventState != 0) {
                return;
            }
            if (capIsBusy() != 0) {
                return;
            }
            switch (capGetVariantKey()) {
                case MIST_PARKING_JODIE_CHOICE_ANNOUNCE_PRIZES:
                    announcement->prizeTimer = MIST_PARKING_PRIZE_ANNOUNCEMENT_FRAMES;
                    capRunCommand(MIST_PARKING_PRIZE_CAP_BEGIN_ANNOUNCEMENTS, CAP_PLAYBACK_IN_PLACE);
                    task->state = MIST_PARKING_JODIE_TALK_ANNOUNCE_PRIZES;
                    break;
                case MIST_PARKING_JODIE_CHOICE_CLOSE_PRIZES:
                    for (courseIndex = 0; courseIndex < MIST_PARKING_PRIZE_COUNT; courseIndex++) {
                        prizeFlag = courseIndex + MIST_PARKING_PRIZE_FLAG_FIRST;
                        if (gameFlagGetNibble(prizeFlag) == MIST_PARKING_PRIZE_WAITING) {
                            gameFlagSetNibble(prizeFlag, MIST_PARKING_PRIZE_CLOSED);
                        }
                    }
                    evsStartScript(_gMistParkingJodieTalkPoseScript, EVENT_SCRIPT_HUD_KEEP);
                    capRunCommand(MIST_PARKING_PRIZE_CAP_CLOSE_WITHOUT_ITEMS, CAP_PLAYBACK_IN_PLACE);
                    task->state = MIST_PARKING_JODIE_TALK_WAIT_PRIZE_CLOSING;
                    break;
                case MIST_PARKING_JODIE_CHOICE_STORE_PRIZES:
                    // The final armor prize also checks its global ownership limit.
                    for (courseIndex = 0; courseIndex < MIST_PARKING_PRIZE_COUNT - 1; courseIndex++) {
                        prizeFlag = courseIndex + MIST_PARKING_PRIZE_FLAG_FIRST;
                        if (gameFlagGetNibble(prizeFlag) == MIST_PARKING_PRIZE_WAITING && inventoryGiveItem(gInventoryContainerRanges[MIST_PARKING_PRIZE_STORAGE_RANGE], _gMistParkingCoursePrizeItemIds[courseIndex], _gMistParkingCoursePrizeQuantities[courseIndex]) != 0) {
                            gameFlagSetNibble(prizeFlag, MIST_PARKING_PRIZE_CLOSED);
                            areaSetCurrentObjectState(courseIndex + MIST_PARKING_PRIZE_OBJECT_FIRST, MIST_PARKING_PRIZE_OBJECT_COLLECTED);
                        }
                    }
                    if (gameFlagGetNibble(GAME_FLAG_SHOOTING_GALLERY_PRIZE_4_STATE) == MIST_PARKING_PRIZE_WAITING && inventoryIsItemLimitReached(MIST_PARKING_PRIZE_SHOULDER_HOLSTER_ITEM_ID) == 0) {
                        if (inventoryGiveItem(gInventoryContainerRanges[MIST_PARKING_PRIZE_STORAGE_RANGE], MIST_PARKING_PRIZE_SHOULDER_HOLSTER_ITEM_ID, MIST_PARKING_PRIZE_ARMOR_QUANTITY) != 0) {
                            gameFlagSetNibble(GAME_FLAG_SHOOTING_GALLERY_PRIZE_4_STATE, MIST_PARKING_PRIZE_CLOSED);
                            areaSetCurrentObjectState(MIST_PARKING_PRIZE_OBJECT_FIRST + MIST_PARKING_PRIZE_COUNT - 1, MIST_PARKING_PRIZE_OBJECT_COLLECTED);
                        }
                    }
                    evsStartScript(_gMistParkingJodieTalkPoseScript, EVENT_SCRIPT_HUD_KEEP);
                    capRunCommand(MIST_PARKING_PRIZE_CAP_ITEMS_STORED, CAP_PLAYBACK_IN_PLACE);
                    task->state = MIST_PARKING_JODIE_TALK_WAIT_PRIZE_CLOSING;
                    break;
            }
            break;
        case MIST_PARKING_JODIE_TALK_ANNOUNCE_PRIZES:
            _mistParkingAdvancePrizeAnnouncement(task, announcement);
            break;
        case MIST_PARKING_JODIE_TALK_CLOSE_ANNOUNCEMENTS:
            if (gGameSession->eventState != 0) {
                return;
            }
            if (capIsBusy() == 0) {
                evsStartScript(_gMistParkingJodieTalkPoseScript, EVENT_SCRIPT_HUD_KEEP);
                capRunCommand(MIST_PARKING_PRIZE_CAP_END_ANNOUNCEMENTS, CAP_PLAYBACK_IN_PLACE);
                task->state++;
            }
            // Keep polling the closing script on the same callback tick.
            /* fallthrough */
        case MIST_PARKING_JODIE_TALK_WAIT_PRIZE_CLOSING:
            if (gGameSession->eventState != 0) {
                return;
            }
            if (capIsBusy() != 0) {
                return;
            }
            task->state++;
            break;
        case MIST_PARKING_JODIE_TALK_DELAY_MENU:
            task->killCountdown++;
            if (task->killCountdown >= MIST_PARKING_JODIE_MENU_DELAY_TICKS) {
                mistParkingSelectDialogueResource(MIST_PARKING_DIALOGUE_DEFAULT);
                capRunCommand(MIST_PARKING_JODIE_CAP_MENU, CAP_PLAYBACK_IN_PLACE);
                task->killCountdown = 0;
                task->state++;
            }
            break;
        case MIST_PARKING_JODIE_TALK_SHOP_CHOICE:
            if (capIsBusy() != 0) {
                return;
            }
            if (capGetVariantKey() == MIST_PARKING_JODIE_CHOICE_OPEN_SHOP) {
                evsStartScript(_gMistParkingJodieShopSessionScript, EVENT_SCRIPT_HUD_KEEP);
            } else {
                evsStartScript(_gMistParkingJodieShopDeclineScript, EVENT_SCRIPT_HUD_KEEP);
            }
            task->state++;
            break;
        case MIST_PARKING_JODIE_TALK_WAIT_SHOP_EXIT:
            task->killCountdown++;
            if (task->killCountdown == MIST_PARKING_JODIE_FAREWELL_TICK) {
                capRunCommand(MIST_PARKING_JODIE_CAP_FAREWELL, CAP_PLAYBACK_IN_PLACE);
            }
            if (gGameSession->eventState != 0) {
                return;
            }
            task->state++;
            break;
        case MIST_PARKING_JODIE_TALK_FINISH:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            taskKill(task);
            break;
    }
}

/// Runs the departure menu used after the variant-2 conversation follow-ups.
///
/// State 0 starts the prompt; states 1 and 3 wait for EVS, state 2 selects the
/// CAP key and state 4 releases the task. Keys 4, 5 and 6 select staying,
/// Dryfield departure and the shooting gallery respectively. The selected key
/// replaces `spawnArg1.value` until finish; only staying resumes player control.
/// Requires loaded dialogue, live scene actors and player control already held.
static void _mistParkingDepartureMenuTask(Task* task)
{
    enum {
        MIST_PARKING_DEPARTURE_START            = 0,
        MIST_PARKING_DEPARTURE_WAIT_PROMPT      = 1,
        MIST_PARKING_DEPARTURE_SELECT           = 2,
        MIST_PARKING_DEPARTURE_WAIT_SCRIPT      = 3,
        MIST_PARKING_DEPARTURE_FINISH           = 4,
        MIST_PARKING_DEPARTURE_STAY             = 4,
        MIST_PARKING_DEPARTURE_DRYFIELD         = 5,
        MIST_PARKING_DEPARTURE_SHOOTING_GALLERY = 6
    };
    s32 choiceKey;

    switch (task->state) {
        case MIST_PARKING_DEPARTURE_START:
            evsStartScript(_gMistParkingDeparturePromptScript, EVENT_SCRIPT_HUD_KEEP);
            task->state++;
            break;
        case MIST_PARKING_DEPARTURE_WAIT_PROMPT:
        case MIST_PARKING_DEPARTURE_WAIT_SCRIPT:
            if (gGameSession->eventState != 0) {
                return;
            }
            task->state++;
            break;
        case MIST_PARKING_DEPARTURE_SELECT:
            // Retain the choice while its script runs and changes CAP selection.
            choiceKey             = capGetVariantKey();
            task->spawnArg1.value = choiceKey;
            switch (choiceKey) {
                case MIST_PARKING_DEPARTURE_STAY:
                    evsStartScript(_gMistParkingStayReplyScript, EVENT_SCRIPT_HUD_KEEP);
                    break;
                case MIST_PARKING_DEPARTURE_DRYFIELD:
                    evsStartScript(_gMistParkingDryfieldDepartureScript, EVENT_SCRIPT_HUD_KEEP);
                    break;
                case MIST_PARKING_DEPARTURE_SHOOTING_GALLERY:
                    evsStartScript(_gMistParkingShootingGalleryDepartureScript, EVENT_SCRIPT_HUD_KEEP);
                    break;
            }
            task->state++;
            break;
        case MIST_PARKING_DEPARTURE_FINISH:
            if (task->spawnArg1.value == MIST_PARKING_DEPARTURE_STAY) {
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            }
            taskKill(task);
            break;
    }
}

#include "../../shared/actor/model_placement_attach_part.inc.c"

/// Keeps an attached model task idle between attachment and teardown.
///
/// Ignores the task and makes no state change. Its three-state table has no
/// observed dispatcher in this room; the retained callback performs no drawing.
static void _mistParkingIdleAttachedModelState(Task* unusedTask)
{
}

/// Starts in-place CAP playback from a packed command slot and variant key.
///
/// Bits 16..31 and 0..15 become signed halfword slot and key arguments.
/// The slot must satisfy `capStartSequenceSlot`'s bounds, and its borrowed CAP
/// resources must remain loaded through playback. Busy playback is skipped;
/// the return value is discarded. The full signed word is the EVS argument.
static void _mistParkingStartPackedDialogue(s32 packedCommandAndVariant)
{
    capStartSequenceSlot(packedCommandAndVariant >> MIST_PARKING_CAP_COMMAND_SHIFT, CAP_PLAYBACK_IN_PLACE, packedCommandAndVariant);
}

/// Queues the departure movie launcher and freezes room objects for the transition.
///
/// Zero selects the normal movie, every nonzero `movieSelector` the alternate.
/// The launcher transfers this word to a display task on its next update.
/// The room and movie resources must stay loaded until presentation is restored.
/// Room objects are frozen even if allocation fails; this call does not unfreeze them.
static void _mistParkingStartDepartureMovie(s32 movieSelector)
{
    enum { MIST_PARKING_DEPARTURE_MOVIE_LAUNCHER_DESCRIPTOR = 0 };

    taskSpawnFromTable(gMistParkingDepartureMovieTaskDescs, MIST_PARKING_DEPARTURE_MOVIE_LAUNCHER_DESCRIPTOR, movieSelector, 0);
    gGameSession->freezeRoomObjs = 1;
}

/// Installs an Acropolis destination and queues a reload capturing the current frame.
///
/// Stores the low byte of `areaId` in the live save, with entrance, room and sprite
/// variant 1. Stops all non-ambient sound scripts immediately. The full signed
/// argument selects a normal load caption only for the plaza. The low byte must
/// name a valid Acropolis/MIST area; the current caller selects the shooting gallery.
/// Requires live save and session state.
static void _mistParkingReloadAcropolisArea(s32 areaId)
{
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.stage     = GAME_STAGE_AKROPOLIS;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp      = MIST_PARKING_RELOAD_WARP;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = MIST_PARKING_RELOAD_ROOM;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area      = areaId;
    gDisplayState.spriteVariant                                 = MIST_PARKING_RELOAD_SPRITE_VARIANT;
    sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
    taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
    if (areaId == GAME_AREA_AKROPOLIS_PLAZA) {
        gameFlowBeginLoadScreen(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location, GAME_FLOW_LOAD_CAPTION_NORMAL);
    }
}

/// Shows the arrival conversation's model task if the room holds none.
///
/// Only descriptor 0 selects the model; every other signed index is ignored.
/// The room holds the spawned task until the release callback or room reset;
/// an existing handle must remain live. Allocation failure leaves it NULL.
/// Successful spawn owns its TMD body and borrows the loaded model source.
static void _mistParkingSpawnCutsceneModel(s32 descriptorIndex)
{
    Task**     modelTaskHandle = descriptorIndex == MIST_PARKING_CUTSCENE_MODEL_DESCRIPTOR_INDEX ? &gMistParkingCutsceneModelTask : NULL;
    Task*      modelTask;
    TmdObject* model;

    if (modelTaskHandle != NULL && *modelTaskHandle == NULL) {
        modelTask        = taskSpawnFromTable(gMistParkingConversationTaskDescs, descriptorIndex, 0, 0);
        *modelTaskHandle = modelTask;
        if (modelTask != NULL) {
            model         = modelTask->extra.tmd;
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
        }
    }
}

/// Releases the arrival conversation's model task and forgets its handle.
///
/// Descriptor 0 selects the model; every other index is ignored. A non-NULL
/// handle must identify a live task. Model-body release follows normal task teardown.
static void _mistParkingReleaseCutsceneModel(s32 descriptorIndex)
{
    if (descriptorIndex == MIST_PARKING_CUTSCENE_MODEL_DESCRIPTOR_INDEX) {
        if (gMistParkingCutsceneModelTask != NULL) {
            taskKill(gMistParkingCutsceneModelTask);
        }
        gMistParkingCutsceneModelTask = NULL;
    }
}

/// Runs a modal two-option choice and forwards its one-based answer.
///
/// State 0 opens, 1 waits and 2 exits, with no bounds check. `spawnArg1.value`
/// selects the second label pair only when 1; `spawnArg2.pointer` borrows a
/// writable aligned s32 for the answer (1 or 2). The task owns its dialog work
/// until teardown. Its descriptor's launcher has not been identified.
static void _mistParkingOptionDialogTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gMistParkingOptionDialogStates;
    stateHandlers.funcs[task->state](task);
}

/// Allocates and opens the room's two-option dialog with cancellation disabled.
///
/// Labels are "not yet"/"yes", or "yes"/"no" when `spawnArg1.value` is 1.
/// The task owns the request and linked options; the UI borrows them and the
/// loaded label strings until closing. Allocation failure kills the task;
/// success installs the exit callback and advances to waiting for the answer.
static void _mistParkingOpenOptionDialog(Task* task)
{
    enum { MIST_PARKING_OPTION_ALTERNATE_LABEL_PAIR = 1 };
    RoomOptionDialog* dialog;
    UiDialogOption*   option;
    u8**              labelCursor;
    s32               alternatePairSelector;
    s32               optionIndex;

    dialog = memCalloc(sizeof(RoomOptionDialog), 0);
    if (dialog == NULL) {
        taskKill(task);
        return;
    }

    option                = dialog->options;
    optionIndex           = 0;
    alternatePairSelector = MIST_PARKING_OPTION_ALTERNATE_LABEL_PAIR;
    labelCursor           = _gMistParkingOptionLabelPairs[0];
    task->work            = dialog;
    task->exitCallback    = _mistParkingExitOptionDialogTask;

    // Link both labelled options before the UI borrows the dialog.
    for (; optionIndex < ARRAY_SIZE(dialog->options); optionIndex++) {
        if (task->spawnArg1.value == alternatePairSelector) {
            option->text = _gMistParkingOptionLabelPairs[alternatePairSelector][optionIndex];
        } else {
            option->text = *labelCursor;
        }
        option->next = option + 1;
        option++;
        labelCursor++;
    }
    option[-1].next = NULL;

    dialog->request.optionCount = ARRAY_SIZE(dialog->options);
    dialog->request.options     = dialog->options;
    dialog->request.title       = NULL;
    dialog->request.flags       = 0;
    uiSpawnOptionDialog(&dialog->request, 0, 0, 0);
    task->state++;
}

/// Publishes the dialog's one-based option number and advances to exit.
///
/// `work` owns a live `RoomOptionDialog`; `spawnArg2.pointer` borrows a writable,
/// word-aligned `s32` through this wait. Zero leaves both destination and state
/// untouched. A nonzero signed-halfword result is widened before storing; normal
/// answers are 1 or 2 because cancellation is disabled. The destination is borrowed.
static void _mistParkingWaitOptionDialogAnswer(Task* task)
{
    const RoomOptionDialog* dialog;
    s16                     choiceResult;

    dialog       = task->work;
    choiceResult = dialog->request.result;
    if (choiceResult != USER_INTERFACE_RESULT_NONE) {
        s32* choiceDestination = task->spawnArg2.pointer;

        *choiceDestination = choiceResult;
        task->state        = task->state + 1;
    }
}

/// Releases the room's option-dialog request task and requests modal exit.
///
/// Used as both the final state and exit callback. The task must be live;
/// task teardown frees its dialog work before the mode-exit request is posted.
static void _mistParkingExitOptionDialogTask(Task* task)
{
    taskKill(task);
    stageRequestModeTaskExit();
}

void mistParkingContinueDepartureChoiceTask(Task* task)
{
    enum { MIST_PARKING_DEPARTURE_CHOICE_LEAVE = 2 };

    if (gGameSession->eventState == 0 && capIsBusy() == 0) {
        if (D_mist_parking_8019531C == MIST_PARKING_DEPARTURE_CHOICE_LEAVE) {
            evsStartScript(_gMistParkingDryfieldDepartureScript, EVENT_SCRIPT_HUD_KEEP);
        } else {
            evsStartScript(_gMistParkingStayReplyScript, EVENT_SCRIPT_HUD_KEEP);
        }
        taskKill(task);
    }
}

/// Starts or skips the shop once the room's current EVS and CAP have finished.
///
/// The room answer word selects skipping when it is 2, opening otherwise.
/// The producer of that word is unproven. Starts one script, then kills the
/// continuation; work, body and spawn arguments are unused.
static void _mistParkingContinueShopChoiceTask(Task* task)
{
    enum { MIST_PARKING_SHOP_CHOICE_SKIP = 2 };

    if (gGameSession->eventState == 0 && capIsBusy() == 0) {
        if (D_mist_parking_8019531C == MIST_PARKING_SHOP_CHOICE_SKIP) {
            evsStartScript(_gMistParkingJodieShopDeclineScript, EVENT_SCRIPT_HUD_KEEP);
        } else {
            evsStartScript(_gMistParkingJodieShopSessionScript, EVENT_SCRIPT_HUD_KEEP);
        }
        taskKill(task);
    }
}

/// Queues the task that prepares the field loadout and departs for Dryfield.
///
/// The departure script calls this after starting the movie launcher. The
/// bodyless task needs no work or spawn arguments. Allocation failure silently
/// leaves the departure unqueued; the parking overlay must remain loaded until dispatch.
static void _mistParkingQueueDryfieldDeparture(void)
{
    enum { MIST_PARKING_DRYFIELD_DEPARTURE_DESCRIPTOR = 3 };

    taskSpawnFromTable(gMistParkingConversationTaskDescs, MIST_PARKING_DRYFIELD_DEPARTURE_DESCRIPTOR, 0, 0);
}

/// Prepares the field loadout and queues departure to Dryfield's gas station.
///
/// Runs once with live save and player state, selecting entrance, room and sprite
/// variant 1 and the alternate load caption. Stops non-ambient sound scripts,
/// queues a reload capturing the frame, then releases this bodyless task.
/// Spawn arguments and work are unused; the destination is saved, not immediately applied.
static void _mistParkingDepartForDryfieldTask(Task* task)
{
    playerActorPrepareDryfieldLoadout();
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.stage     = GAME_STAGE_DRYFIELD;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area      = GAME_AREA_DRYFIELD_GAS_STATION;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp      = MIST_PARKING_RELOAD_WARP;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = MIST_PARKING_RELOAD_ROOM;
    gDisplayState.spriteVariant                                 = MIST_PARKING_RELOAD_SPRITE_VARIANT;
    gameFlowBeginLoadScreen(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location, GAME_FLOW_LOAD_CAPTION_ALTERNATE);
    sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
    taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
    taskKill(task);
}

/// Starts animation-controlled player head aiming toward the room's talk partner.
///
/// Replaces the room's task handle without releasing an earlier task; call while
/// the room holds none. Allocation failure stores NULL. The task borrows the live
/// player and index-zero partner models and uses the loaded parking animation bank.
/// `mistParkingControlPlayerHeadAim` stops it and clears the handle after the talk.
static void _mistParkingStartPlayerHeadAim(void)
{
    enum { MIST_PARKING_PLAYER_HEAD_AIM_DESCRIPTOR = 4 };

    gMistParkingPlayerHeadAimTask = taskSpawnFromTable(gMistParkingConversationTaskDescs, MIST_PARKING_PLAYER_HEAD_AIM_DESCRIPTOR, MIST_PARKING_HEAD_AIM_FOLLOW_ANIMATION, 0);
}
