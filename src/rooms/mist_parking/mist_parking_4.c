#include "rooms/mist_parking.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "mist_parking_private.h"
#include "mist_parking_head_aim.h"

#include "gameplay/companion_load.h"
#include "gameplay/animation.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/collision.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/scene_runtime.h"

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
#include "main/wipsys.h"
#include "main/wipsys_types.h"

static void       _mistParkingSelectShopDialogueResource(s32 resourceOrdinal);
extern EvsCommand D_mist_parking_80190C74[];
extern EvsCommand D_mist_parking_80190D64[];
extern EvsCommand D_mist_parking_80190E84[];
extern EvsCommand D_mist_parking_80191034[];

extern s8 D_mist_parking_801908C8[];

static void _mistParkingAimPlayerHeadAtShopPartnerTask(Task* task);
static void _mistParkingJodieShopTalkTask(Task* task);
static void _mistParkingShopDepartureMenuTask(Task* task);
static void _mistParkingDepartForAcropolisTask(Task* unusedTask);
static void _mistParkingDelayShopDisplayModeExitTask(Task* task);

extern AnimationPlayRequest D_mist_parking_801908A0;
extern AnimationPlayRequest D_mist_parking_801908B4;
extern AnimationPlayRequest D_mist_parking_80190944;
extern AnimationPlayRequest D_mist_parking_801909F8;
extern ActorCommand         D_mist_parking_80190BA4;
extern ActorCommand         D_mist_parking_80190BA8;

static void _mistParkingRunShopDialogueCommand(s32 commandIndex);
static void _mistParkingStartShopDepartureMovie(s32 movieSelector);
static void _mistParkingReloadShopAcropolisArea(s32 areaId);
static void _mistParkingQueueAcropolisDeparture(void);
void        func_mist_parking_8018459C(void);
static void _mistParkingControlShopPlayerHeadAim(s32 mode);
static void _mistParkingQueueDelayedShopDisplayModeExit(s32 delayTicks);

/// Ordinals of the already-loaded CAP resources used by the variant-1 talks.
enum {
    MIST_PARKING_SHOP_DIALOGUE_DEFAULT = 0,
    MIST_PARKING_SHOP_DIALOGUE_MENU    = 1,
    MIST_PARKING_SHOP_DIALOGUE_PRIZES  = 2
};

/// The five course prizes occupy consecutive saved flags and placed-item states.
enum {
    MIST_PARKING_SHOP_PRIZE_COUNT            = 5,
    MIST_PARKING_SHOP_PRIZE_FLAG_FIRST       = GAME_FLAG_SHOOTING_GALLERY_PRIZE_4_STATE - 4,
    MIST_PARKING_SHOP_PRIZE_WAITING          = 2,
    MIST_PARKING_SHOP_PRIZE_CLOSED           = 3,
    MIST_PARKING_SHOP_PRIZE_OBJECT_FIRST     = 0x20,
    MIST_PARKING_SHOP_PRIZE_OBJECT_AVAILABLE = 1,
    MIST_PARKING_SHOP_PRIZE_CAPTION_SLOT     = 5
};

/// Destination selectors used by the variant-1 departure callbacks.
enum {
    MIST_PARKING_SHOP_RELOAD_WARP           = 1,
    MIST_PARKING_SHOP_RELOAD_ROOM           = 1,
    MIST_PARKING_SHOP_RELOAD_SPRITE_VARIANT = 1
};

TaskDesc D_mist_parking_80190824[5] = {
    { { { TASK_BODY_NONE, 192 } }, _mistParkingDepartForAcropolisTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 97 } }, _mistParkingAimPlayerHeadAtShopPartnerTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _mistParkingDelayShopDisplayModeExitTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _mistParkingJodieShopTalkTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _mistParkingShopDepartureMenuTask, { .value = 0 } },
};

AnimationSet* D_mist_parking_80190860[4] = {
    NULL,
    &gMistParkingAnimation129F8,
    &gMistParkingAnimation12DCC,
    &gMistParkingAnimation1323C,
};

AnimationBankCopyRequest D_mist_parking_80190870 = { { .sets = D_mist_parking_80190860 }, ARRAY_SIZE(D_mist_parking_80190860) };

AnimationPlayRequest D_mist_parking_80190878 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_8019088C = { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_801908A0 = { { .index = 1 }, 49, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_801908B4 = { { .index = 1 }, 50, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

s8 D_mist_parking_801908C8[124] = {
    0,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    20,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    2,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    20,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    3,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    20,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    4,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    20,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    5,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    20,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
};

AnimationPlayRequest D_mist_parking_80190944 = { { .sets = NULL }, 6, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_80190958[8] = {
    { { .index = 0 }, 7, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 8, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 9, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 10, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 11, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 12, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 13, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 14, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
};

AnimationPlayRequest D_mist_parking_801909F8 = { { .index = 0 }, 15, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_80190A0C[20] = {
    { { .index = 0 }, 16, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 17, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 18, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 19, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 20, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 21, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 22, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 23, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 24, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 25, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 26, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 27, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 28, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 29, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 30, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 31, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 32, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 33, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 34, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 35, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
};

GameActorMoveAnim D_mist_parking_80190B9C = { 0, 0x10000 };

ActorCommand D_mist_parking_80190BA4 = { { .loc = { 0, 0 } }, 2 };

ActorCommand D_mist_parking_80190BA8 = { { .loc = { 0, 0 } }, 3 };

AnimationPlayRequest D_mist_parking_80190BAC = { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_80190BC0 = { { .index = 0 }, 1, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_80190BD4[3] = {
    { { .index = 0 }, 2, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 3, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 4, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE },
};

AnimationPlayRequest D_mist_parking_80190C10 = { { .index = 0 }, 5, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_80190C24 = { { .index = 0 }, 6, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_80190C38 = { { .index = 0 }, 7, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_80190C4C = { { .index = 0 }, 8, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_80190C60 = { { .index = 0 }, 9, ANIMATION_BLEND_INTERPOLATE, 15, ANIMATION_WORLD_COLLISION_ENABLE };

EvsCommand D_mist_parking_80190C74[10] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_mist_parking_8018459C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_mist_parking_80190870 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_mist_parking_80190BA4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_801908A0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_80190944 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_mist_parking_80190D64[12] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_mist_parking_80190870 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mistParkingRunShopDialogueCommand }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_801908B4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_801909F8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mistParkingControlShopPlayerHeadAim }, { .value = MIST_PARKING_HEAD_AIM_STOP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_801908B4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_mist_parking_80190BA8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_mist_parking_80190E84[18] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_mist_parking_80190870 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mistParkingRunShopDialogueCommand }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_801909F8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CANCEL_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mistParkingQueueDelayedShopDisplayModeExit }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mistParkingSelectShopDialogueResource }, { .value = MIST_PARKING_SHOP_DIALOGUE_DEFAULT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4004 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mistParkingStartShopDepartureMovie }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _mistParkingQueueAcropolisDeparture }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_mist_parking_80191034[12] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_mist_parking_80190870 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mistParkingRunShopDialogueCommand }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_801909F8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CANCEL_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mistParkingQueueDelayedShopDisplayModeExit }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mistParkingReloadShopAcropolisArea }, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

void mistParkingSetPierceCollisionPatchLowered(s32 lowerPatch)
{
    enum {
        MIST_PARKING_PIERCE_PATCH_FACE_COUNT   = 2,
        MIST_PARKING_PIERCE_PATCH_VERTEX_COUNT = 6,
        MIST_PARKING_PIERCE_PATCH_LOWER_Y      = 2000
    };
    WorldCollisionGrid*       liveGrid;
    const WorldCollisionGrid* sourcePatch;
    SVECTOR                   translation;
    s32                       elementIndex;

    liveGrid    = &D_mist_parking_80192204;
    sourcePatch = &D_mist_parking_8018FCB8;

    // Rebuild the reserved grid prefix without overwriting vector fourth halfwords.
    for (elementIndex = 0; elementIndex < MIST_PARKING_PIERCE_PATCH_FACE_COUNT; elementIndex++) {
        liveGrid->normals[elementIndex].vx = sourcePatch->normals[elementIndex].vx;
        liveGrid->normals[elementIndex].vy = sourcePatch->normals[elementIndex].vy;
        liveGrid->normals[elementIndex].vz = sourcePatch->normals[elementIndex].vz;
        liveGrid->faces[elementIndex]      = sourcePatch->faces[elementIndex];
    }

    for (elementIndex = 0; elementIndex < MIST_PARKING_PIERCE_PATCH_VERTEX_COUNT; elementIndex++) {
        liveGrid->vertices[elementIndex].vx = sourcePatch->vertices[elementIndex].vx;
        liveGrid->vertices[elementIndex].vy = sourcePatch->vertices[elementIndex].vy;
        liveGrid->vertices[elementIndex].vz = sourcePatch->vertices[elementIndex].vz;
    }

    if (lowerPatch == MIST_PARKING_PIERCE_PATCH_RESTORED) {
        translation.vx = 0;
        translation.vy = 0;
    } else {
        translation.vx = 0;
        translation.vy = MIST_PARKING_PIERCE_PATCH_LOWER_Y;
    }
    translation.vz = 0;

    /// Translates the restored six-vertex prefix in world-coordinate units.
    ///
    /// Captures liveGrid and translation and resets/advances elementIndex to six.
    /// Takes no arguments; XYZ additions retain 16 bits and fourth halfwords stay intact.
#define MIST_PARKING_TRANSLATE_PIERCE_PATCH()                                                       \
    for (elementIndex = 0; elementIndex < MIST_PARKING_PIERCE_PATCH_VERTEX_COUNT; elementIndex++) { \
        liveGrid->vertices[elementIndex].vx += translation.vx;                                      \
        liveGrid->vertices[elementIndex].vy += translation.vy;                                      \
        liveGrid->vertices[elementIndex].vz += translation.vz;                                      \
    }
    MIST_PARKING_TRANSLATE_PIERCE_PATCH();
#undef MIST_PARKING_TRANSLATE_PIERCE_PATCH
}

/// Ramps the player's head aim toward the variant-1 shop/departure talk partner.
///
/// State 0 updates; every other state releases the task. Event-script suspension
/// pauses both paths. A nonzero `spawnArg1.value` forces aiming; otherwise slot
/// 1's next animation selects flags at extension indices 1..3. Other indices
/// disable animation aim. `killCountdown` starts at zero and stores a 1/4096
/// blend clamped to 0..4096, stepped by 256 per active callback. Requires live
/// player work, the installed four-word extension bank and the stage/area's
/// index-zero partner, both with the expected head-joint model layout.
static void _mistParkingAimPlayerHeadAtShopPartnerTask(Task* task)
{
    const GameActor* player;
    Enemy*           talkPartner;
    s32              extensionIndex;
    s32              animationRequestsAim;

    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;
    if (D_801156F9 == 0) {
        // Extension index zero has no aim flag; the copied bank bounds the lookup.
        extensionIndex = player->animationSlots[1].nextPose.indices.setIndex - ANIMATION_BANK_BASE_SET_COUNT;
        if ((extensionIndex > 0) && (extensionIndex < D_mist_parking_80190870.wordCount)) {
            animationRequestsAim = D_mist_parking_801908C8[extensionIndex];
        } else {
            animationRequestsAim = 0;
        }
        if (task->state == MIST_PARKING_HEAD_AIM_UPDATE) {
            _mistParkingRampPlayerHeadAimBlend(task, animationRequestsAim);
            // Placement index zero supplies the talk partner for this stage and area.
            talkPartner = sceneFindEnemyByPlaceKey(gGameSession->location.loc.area | (gGameSession->location.loc.stage << 8));
            animationAimHeadAtTask(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), talkPartner->task,
                                   MIST_PARKING_HEAD_AIM_MAX_YAW, MIST_PARKING_HEAD_AIM_MAX_PITCH, task->killCountdown);
        } else {
            taskKill(task);
        }
    }
}

/// Advances one prize's announcement in the variant-1 shop conversation.
///
/// Borrows the live talk and its singleton work. Requires course index 0..4
/// and timer 1..10, initialized to index 0/timer 10 before the first turn.
/// CAP playback pauses the cursor. Each course gets ten idle callback ticks;
/// a waiting prize is captioned halfway, and an unavailable placed prize closes
/// its flag at the end. The fifth completed turn leaves index 5/timer 10 and
/// advances the talk's state; the caller must then stop using this helper.
static inline void _mistParkingAdvanceShopPrizeAnnouncement(Task* task, MistParkingShopTalkState* talk)
{
    s16 courseIndex;

    if (capIsBusy() != 0) {
        return;
    }
    talk->prizeTimer--;
    if (talk->prizeTimer == MIST_PARKING_PRIZE_ANNOUNCEMENT_CAPTION_FRAME) {
        courseIndex = talk->prizeIndex;
        if (gameFlagGetNibble(courseIndex + MIST_PARKING_SHOP_PRIZE_FLAG_FIRST) == MIST_PARKING_SHOP_PRIZE_WAITING) {
            capStartSequenceSlot(MIST_PARKING_SHOP_PRIZE_CAPTION_SLOT, CAP_PLAYBACK_IN_PLACE, courseIndex);
        }
        return;
    }
    if (talk->prizeTimer != 0) {
        return;
    }
    courseIndex = talk->prizeIndex;
    if (areaGetCurrentObjectState(courseIndex + MIST_PARKING_SHOP_PRIZE_OBJECT_FIRST) != MIST_PARKING_SHOP_PRIZE_OBJECT_AVAILABLE) {
        gameFlagSetNibble(courseIndex + MIST_PARKING_SHOP_PRIZE_FLAG_FIRST, MIST_PARKING_SHOP_PRIZE_CLOSED);
    }
    talk->prizeTimer = MIST_PARKING_PRIZE_ANNOUNCEMENT_FRAMES;
    talk->prizeIndex++;
    if (talk->prizeIndex >= MIST_PARKING_SHOP_PRIZE_COUNT) {
        task->state++;
    }
}

/// Runs Jodie's variant-1 prize announcements and shop conversation.
///
/// Requires held player control, live actors and loaded menu/prize/default CAP
/// resources. States 0..10 drive the talk; state 0 clears its shared eight-byte
/// work, so only one instance may run. Prize key 1 announces waiting prizes;
/// other keys skip announcements. Menu keys 6, 7 and 8 open the shop, continue
/// the conversation or leave. `killCountdown` starts at zero and counts callback
/// ticks for menu/reply delays. The menu timer pauses for EVS/CAP; the reply
/// timer advances during playback. `spawnArg1.value` retains the menu key.
/// Completion resumes player control, restores default CAP data and releases
/// the bodyless task. Its second spawn word is unused.
static void _mistParkingJodieShopTalkTask(Task* task)
{
    enum {
        MIST_PARKING_SHOP_TALK_GREET                = 0,
        MIST_PARKING_SHOP_TALK_CHECK_PRIZES         = 1,
        MIST_PARKING_SHOP_TALK_PRIZE_PROMPT         = 2,
        MIST_PARKING_SHOP_TALK_PRIZE_CHOICE         = 3,
        MIST_PARKING_SHOP_TALK_ANNOUNCE_PRIZES      = 4,
        MIST_PARKING_SHOP_TALK_CLOSE_PRIZES         = 5,
        MIST_PARKING_SHOP_TALK_MENU_DELAY           = 6,
        MIST_PARKING_SHOP_TALK_WAIT_MENU            = 7,
        MIST_PARKING_SHOP_TALK_MENU_CHOICE          = 8,
        MIST_PARKING_SHOP_TALK_WAIT_REPLY           = 9,
        MIST_PARKING_SHOP_TALK_FINISH               = 10,
        MIST_PARKING_SHOP_TALK_GREETING_COMMAND     = 6,
        MIST_PARKING_SHOP_TALK_PRIZE_PROMPT_COMMAND = 1,
        MIST_PARKING_SHOP_TALK_ANNOUNCE_KEY         = 1,
        MIST_PARKING_SHOP_TALK_BRIEF_REPLY_COMMAND  = 7,
        MIST_PARKING_SHOP_TALK_ANNOUNCED_COMMAND    = 2,
        MIST_PARKING_SHOP_TALK_SKIPPED_COMMAND      = 3,
        MIST_PARKING_SHOP_TALK_MENU_COMMAND         = 9,
        MIST_PARKING_SHOP_TALK_CONTINUE_COMMAND     = 8,
        MIST_PARKING_SHOP_TALK_NO_BUSINESS_COMMAND  = 10,
        MIST_PARKING_SHOP_TALK_OPEN_SHOP_KEY        = 6,
        MIST_PARKING_SHOP_TALK_CONTINUE_KEY         = 7,
        MIST_PARKING_SHOP_TALK_LEAVE_KEY            = 8,
        MIST_PARKING_SHOP_TALK_REPLY_DELAY_TICKS    = 10,
        MIST_PARKING_SHOP_TALK_BUSINESS_DONE        = 1
    };
    MistParkingShopTalkState* talk = &D_mist_parking_80195334;
    s32                       nextState;
    s32                       courseIndex;
    s32                       choiceKey;

    switch (task->state) {
        case MIST_PARKING_SHOP_TALK_GREET:
            memFillBytes(talk, 0, sizeof(*talk));
            _mistParkingSelectShopDialogueResource(MIST_PARKING_SHOP_DIALOGUE_MENU);
            evsStartScript(D_mist_parking_80191154, EVENT_SCRIPT_HUD_KEEP);
            capRunCommand(MIST_PARKING_SHOP_TALK_GREETING_COMMAND, CAP_PLAYBACK_IN_PLACE);
            task->state++;
            break;
        case MIST_PARKING_SHOP_TALK_CHECK_PRIZES:
            if (gGameSession->eventState != 0) {
                return;
            }
            if (capIsBusy() != 0) {
                return;
            }
            for (courseIndex = 0; courseIndex < MIST_PARKING_SHOP_PRIZE_COUNT; courseIndex++) {
                if (gameFlagGetNibble(courseIndex + MIST_PARKING_SHOP_PRIZE_FLAG_FIRST) == MIST_PARKING_SHOP_PRIZE_WAITING) {
                    task->state = MIST_PARKING_SHOP_TALK_PRIZE_PROMPT;
                    return;
                }
            }
            task->state = MIST_PARKING_SHOP_TALK_MENU_DELAY;
            break;
        case MIST_PARKING_SHOP_TALK_PRIZE_PROMPT:
            _mistParkingSelectShopDialogueResource(MIST_PARKING_SHOP_DIALOGUE_PRIZES);
            evsStartScript(D_mist_parking_80191154, EVENT_SCRIPT_HUD_KEEP);
            capRunCommand(MIST_PARKING_SHOP_TALK_PRIZE_PROMPT_COMMAND, CAP_PLAYBACK_IN_PLACE);
            talk->businessDone = MIST_PARKING_SHOP_TALK_BUSINESS_DONE;
            task->state++;
            break;
        case MIST_PARKING_SHOP_TALK_PRIZE_CHOICE:
            if (gGameSession->eventState != 0) {
                return;
            }
            if (capIsBusy() != 0) {
                return;
            }
            if (capGetVariantKey() == MIST_PARKING_SHOP_TALK_ANNOUNCE_KEY) {
                capRunCommand(MIST_PARKING_SHOP_TALK_BRIEF_REPLY_COMMAND, CAP_PLAYBACK_IN_PLACE);
                talk->prizeTimer          = MIST_PARKING_PRIZE_ANNOUNCEMENT_FRAMES;
                talk->prizeClosingCommand = MIST_PARKING_SHOP_TALK_ANNOUNCED_COMMAND;
                task->state               = MIST_PARKING_SHOP_TALK_ANNOUNCE_PRIZES;
            } else {
                talk->prizeClosingCommand = MIST_PARKING_SHOP_TALK_SKIPPED_COMMAND;
                task->state               = MIST_PARKING_SHOP_TALK_CLOSE_PRIZES;
            }
            break;
        case MIST_PARKING_SHOP_TALK_ANNOUNCE_PRIZES:
            _mistParkingAdvanceShopPrizeAnnouncement(task, talk);
            break;
        case MIST_PARKING_SHOP_TALK_CLOSE_PRIZES:
            evsStartScript(D_mist_parking_80191154, EVENT_SCRIPT_HUD_KEEP);
            capRunCommand(talk->prizeClosingCommand, CAP_PLAYBACK_IN_PLACE);
            task->state++;
            break;
        case MIST_PARKING_SHOP_TALK_MENU_DELAY:
            if (gGameSession->eventState != 0) {
                return;
            }
            if (capIsBusy() != 0) {
                return;
            }
            task->killCountdown++;
            if (task->killCountdown == MIST_PARKING_SHOP_TALK_REPLY_DELAY_TICKS) {
                _mistParkingSelectShopDialogueResource(MIST_PARKING_SHOP_DIALOGUE_MENU);
                capRunCommand(MIST_PARKING_SHOP_TALK_MENU_COMMAND, CAP_PLAYBACK_IN_PLACE);
                task->killCountdown = 0;
                task->state++;
            }
            break;
        case MIST_PARKING_SHOP_TALK_WAIT_MENU:
            if (capIsBusy() == 0) {
                task->state++;
            }
            break;
        case MIST_PARKING_SHOP_TALK_MENU_CHOICE:
            // Keep the menu key while its script changes the CAP resource/selection.
            choiceKey             = capGetVariantKey();
            task->spawnArg1.value = choiceKey;
            if (choiceKey == MIST_PARKING_SHOP_TALK_OPEN_SHOP_KEY) {
                evsStartScript(D_mist_parking_80191214, EVENT_SCRIPT_HUD_KEEP);
                talk->businessDone = MIST_PARKING_SHOP_TALK_BUSINESS_DONE;
            } else if (choiceKey == MIST_PARKING_SHOP_TALK_CONTINUE_KEY) {
                evsStartScript(D_mist_parking_80191304, EVENT_SCRIPT_HUD_KEEP);
                talk->businessDone = MIST_PARKING_SHOP_TALK_BUSINESS_DONE;
            } else {
                evsStartScript(D_mist_parking_801913C4, EVENT_SCRIPT_HUD_KEEP);
            }
            task->state++;
            break;
        case MIST_PARKING_SHOP_TALK_WAIT_REPLY:
            task->killCountdown++;
            if (task->killCountdown == MIST_PARKING_SHOP_TALK_REPLY_DELAY_TICKS) {
                switch (task->spawnArg1.value) {
                    case MIST_PARKING_SHOP_TALK_OPEN_SHOP_KEY:
                        capRunCommand(MIST_PARKING_SHOP_TALK_BRIEF_REPLY_COMMAND, CAP_PLAYBACK_IN_PLACE);
                        break;
                    case MIST_PARKING_SHOP_TALK_CONTINUE_KEY:
                        capRunCommand(MIST_PARKING_SHOP_TALK_CONTINUE_COMMAND, CAP_PLAYBACK_IN_PLACE);
                        break;
                    case MIST_PARKING_SHOP_TALK_LEAVE_KEY:
                        if (talk->businessDone != 0) {
                            capRunCommand(MIST_PARKING_SHOP_TALK_BRIEF_REPLY_COMMAND, CAP_PLAYBACK_IN_PLACE);
                        } else {
                            capRunCommand(MIST_PARKING_SHOP_TALK_NO_BUSINESS_COMMAND, CAP_PLAYBACK_IN_PLACE);
                        }
                        break;
                }
            }
            if (gGameSession->eventState != 0) {
                return;
            }
            if (capIsBusy() != 0) {
                return;
            }
            nextState           = MIST_PARKING_SHOP_TALK_FINISH;
            task->killCountdown = 0;
            if (task->spawnArg1.value == MIST_PARKING_SHOP_TALK_CONTINUE_KEY) {
                nextState = MIST_PARKING_SHOP_TALK_MENU_DELAY;
            }
            task->state = nextState;
            break;
        case MIST_PARKING_SHOP_TALK_FINISH:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            _mistParkingSelectShopDialogueResource(MIST_PARKING_SHOP_DIALOGUE_DEFAULT);
            taskKill(task);
            break;
    }
}

/// Runs the variant-1 departure menu and restores the default CAP selection.
///
/// State 0 starts the prompt; states 1 and 3 wait for EVS, state 2 selects the
/// CAP key and state 4 releases the task. Keys 1, 2 and 3 select staying,
/// Acropolis plaza and the shooting gallery respectively. The chosen key
/// replaces `spawnArg1.value` until finish; only staying resumes player control.
/// Requires loaded dialogue, live scene actors and player control already held.
static void _mistParkingShopDepartureMenuTask(Task* task)
{
    enum {
        MIST_PARKING_SHOP_DEPARTURE_START            = 0,
        MIST_PARKING_SHOP_DEPARTURE_WAIT_PROMPT      = 1,
        MIST_PARKING_SHOP_DEPARTURE_SELECT           = 2,
        MIST_PARKING_SHOP_DEPARTURE_WAIT_SCRIPT      = 3,
        MIST_PARKING_SHOP_DEPARTURE_FINISH           = 4,
        MIST_PARKING_SHOP_DEPARTURE_STAY             = 1,
        MIST_PARKING_SHOP_DEPARTURE_PLAZA            = 2,
        MIST_PARKING_SHOP_DEPARTURE_SHOOTING_GALLERY = 3
    };
    s32 choiceKey;

    switch (task->state) {
        case MIST_PARKING_SHOP_DEPARTURE_START:
            _mistParkingSelectShopDialogueResource(MIST_PARKING_SHOP_DIALOGUE_MENU);
            evsStartScript(D_mist_parking_80190C74, EVENT_SCRIPT_HUD_KEEP);
            task->state++;
            break;
        case MIST_PARKING_SHOP_DEPARTURE_WAIT_PROMPT:
        case MIST_PARKING_SHOP_DEPARTURE_WAIT_SCRIPT:
            if (gGameSession->eventState != 0) {
                return;
            }
            task->state++;
            break;
        case MIST_PARKING_SHOP_DEPARTURE_SELECT:
            // Retain the choice while its script runs and changes CAP selection.
            choiceKey             = capGetVariantKey();
            task->spawnArg1.value = choiceKey;
            switch (choiceKey) {
                case MIST_PARKING_SHOP_DEPARTURE_STAY:
                    evsStartScript(D_mist_parking_80190D64, EVENT_SCRIPT_HUD_KEEP);
                    break;
                case MIST_PARKING_SHOP_DEPARTURE_PLAZA:
                    evsStartScript(D_mist_parking_80190E84, EVENT_SCRIPT_HUD_KEEP);
                    break;
                case MIST_PARKING_SHOP_DEPARTURE_SHOOTING_GALLERY:
                    evsStartScript(D_mist_parking_80191034, EVENT_SCRIPT_HUD_KEEP);
                    break;
            }
            task->state++;
            break;
        case MIST_PARKING_SHOP_DEPARTURE_FINISH:
            if (task->spawnArg1.value == MIST_PARKING_SHOP_DEPARTURE_STAY) {
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            }
            _mistParkingSelectShopDialogueResource(MIST_PARKING_SHOP_DIALOGUE_DEFAULT);
            taskKill(task);
            break;
    }
}

/// Runs the selected variant-1 CAP command in place.
///
/// The current scripts supply commands 2, 3 or 4. The index must select a live
/// command in the already-selected CAP resource, which must remain loaded
/// through playback. No CAP data is selected or loaded by this callback.
static void _mistParkingRunShopDialogueCommand(s32 commandIndex)
{
    capRunCommand(commandIndex, CAP_PLAYBACK_IN_PLACE);
}

/// Queues the variant-1 departure movie and freezes room objects.
///
/// Zero selects movie 100 and every nonzero word movie 101; the departure
/// script supplies 1. The launcher forwards the full selector to its display
/// task. Requires loaded movie/room resources through presentation. Freezes
/// room objects even if allocation fails; this callback does not unfreeze them.
static void _mistParkingStartShopDepartureMovie(s32 movieSelector)
{
    enum { MIST_PARKING_SHOP_MOVIE_LAUNCHER_DESCRIPTOR = 0 };

    taskSpawnFromTable(D_mist_parking_8018FC24, MIST_PARKING_SHOP_MOVIE_LAUNCHER_DESCRIPTOR, movieSelector, 0);
    gGameSession->freezeRoomObjs = 1;
}

/// Installs the variant-1 Acropolis destination and reloads from the current frame.
///
/// Stores the low byte of `areaId`, with stage Acropolis, room/warp/sprite
/// variant 1, then stops non-ambient scripts without fading. The full signed
/// word selects a normal load caption only for the plaza. Its low byte must be
/// a valid Acropolis/MIST area; the script supplies the shooting gallery.
/// Requires live saved state and loaded reload resources.
static void _mistParkingReloadShopAcropolisArea(s32 areaId)
{
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage = GAME_STAGE_ACROPOLIS;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp  = MIST_PARKING_SHOP_RELOAD_WARP;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room  = MIST_PARKING_SHOP_RELOAD_ROOM;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area  = areaId;
    gDisplayState.spriteVariant                                 = MIST_PARKING_SHOP_RELOAD_SPRITE_VARIANT;
    sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
    taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
    if (areaId == GAME_AREA_ACROPOLIS_PLAZA) {
        gameFlowBeginLoadScreen(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc, GAME_FLOW_LOAD_CAPTION_NORMAL);
    }
}

/// Queues the loadout-preparing Acropolis departure after the variant-1 movie.
///
/// Requires the parking overlay and its task table to stay loaded until the
/// queued departure runs. Allocation failure silently skips the request.
static void _mistParkingQueueAcropolisDeparture(void)
{
    enum { MIST_PARKING_ACROPOLIS_DEPARTURE_DESCRIPTOR = 0 };

    taskSpawnFromTable(D_mist_parking_80190824, MIST_PARKING_ACROPOLIS_DEPARTURE_DESCRIPTOR, 0, 0);
}

/// Prepares the player's Acropolis loadout and reloads into the plaza.
///
/// Sets player resource variant 1 and saved stage/area/room/warp, stops
/// non-ambient sounds, queues a reload capturing the frame, then starts its
/// normal load caption. Requires live player/save/display state. Ignores the
/// bodyless task and its spawn words; it neither advances nor releases itself,
/// and the pending reload removes it along with the old room's tasks.
static void _mistParkingDepartForAcropolisTask(Task* unusedTask)
{
    enum { MIST_PARKING_ACROPOLIS_PLAYER_RESOURCE_VARIANT = 1 };

    playerActorPrepareAcropolisLoadout();
    gPlayerStatus.resourceVariant                               = MIST_PARKING_ACROPOLIS_PLAYER_RESOURCE_VARIANT;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area  = GAME_AREA_ACROPOLIS_PLAZA;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage = GAME_STAGE_ACROPOLIS;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp  = MIST_PARKING_SHOP_RELOAD_WARP;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room  = MIST_PARKING_SHOP_RELOAD_ROOM;
    gDisplayState.spriteVariant                                 = MIST_PARKING_SHOP_RELOAD_SPRITE_VARIANT;
    sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
    taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
    gameFlowBeginLoadScreen(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc, GAME_FLOW_LOAD_CAPTION_NORMAL);
}

/// Spawns entry 1 of `D_mist_parking_80190824` and keeps its handle in
/// `D_mist_parking_8019532C.task`.
void func_mist_parking_8018459C(void)
{
    D_mist_parking_8019532C.task = taskSpawnFromTable(D_mist_parking_80190824, 1, 0, 0);
}

/// Controls the existing head-aim task used by the variant-1 conversations.
///
/// FOLLOW_ANIMATION selects animation-driven aiming, FORCE keeps it enabled;
/// every other mode kills the task and clears its handle. A non-NULL handle
/// must refer to a live task. Does nothing when absent and never spawns a task.
static void _mistParkingControlShopPlayerHeadAim(s32 mode)
{
    Task* task = D_mist_parking_8019532C.task;

    if (task == NULL) {
        return;
    }
    switch (mode) {
        case MIST_PARKING_HEAD_AIM_FOLLOW_ANIMATION:
        case MIST_PARKING_HEAD_AIM_FORCE:
            task->spawnArg1.value = mode;
            break;
        default:
            taskKill(D_mist_parking_8019532C.task);
            D_mist_parking_8019532C.task = NULL;
            break;
    }
}

/// Queues the variant-1 talk's display mode with a callback-tick exit delay.
///
/// A nonnegative N exits on dispatch N + 1. Uses the reload presentation
/// policy; an already pending mode request silently rejects the request.
/// Requires this overlay and its descriptor table to remain loaded until exit.
static void _mistParkingQueueDelayedShopDisplayModeExit(s32 delayTicks)
{
    enum { MIST_PARKING_SHOP_DISPLAY_EXIT_DESCRIPTOR_INDEX = 2 };

    displayQueueModeTask(taskGetDescAt(D_mist_parking_80190824, MIST_PARKING_SHOP_DISPLAY_EXIT_DESCRIPTOR_INDEX), delayTicks, 0, STAGE_ENTRY_RELOAD);
}

/// Counts down callback ticks before releasing the variant-1 talk's display mode.
///
/// `spawnArg1.value` is a signed word, decremented before testing. An initial
/// nonnegative N exits on callback N + 1. Requires a live modal task; teardown
/// precedes the mode-exit request, and no work or body is allocated by this callback.
static void _mistParkingDelayShopDisplayModeExitTask(Task* task)
{
    s32 ticksLeft;

    ticksLeft             = task->spawnArg1.value - 1;
    task->spawnArg1.value = ticksLeft;
    if (ticksLeft < 0) {
        taskKill(task);
        stageRequestModeTaskExit();
    }
}

/// Selects already-loaded CAP data and its font page for the variant-1 conversations.
///
/// Resets CAP first: ordinal 1 selects the shop/departure menu, 2 the prizes,
/// and every other value retains the default selected by reset. Playback must
/// have stopped; bundle CAP data and font images must already be loaded and
/// remain live through their use. No files are loaded and no resource is owned.
static void _mistParkingSelectShopDialogueResource(s32 resourceOrdinal)
{
    enum {
        MIST_PARKING_SHOP_MENU_FONT_VRAM_X  = 320,
        MIST_PARKING_SHOP_MENU_FONT_VRAM_Y  = 256,
        MIST_PARKING_SHOP_PRIZE_FONT_VRAM_X = 704,
        MIST_PARKING_SHOP_PRIZE_FONT_VRAM_Y = 0
    };

    capReset();
    switch (resourceOrdinal) {
        case MIST_PARKING_SHOP_DIALOGUE_MENU:
            Gp_CapFile = NULL;
            capSelectLoadedFile(MIST_PARKING_SHOP_DIALOGUE_MENU);
            capSetTexturePage(MIST_PARKING_SHOP_MENU_FONT_VRAM_X, MIST_PARKING_SHOP_MENU_FONT_VRAM_Y);
            break;
        case MIST_PARKING_SHOP_DIALOGUE_PRIZES:
            Gp_CapFile = NULL;
            capSelectLoadedFile(MIST_PARKING_SHOP_DIALOGUE_PRIZES);
            capSetTexturePage(MIST_PARKING_SHOP_PRIZE_FONT_VRAM_X, MIST_PARKING_SHOP_PRIZE_FONT_VRAM_Y);
            break;
    }
}

void mistParkingForgetShopHeadAimTaskHandle(s32 unused)
{
    D_mist_parking_8019532C.task = NULL;
}
