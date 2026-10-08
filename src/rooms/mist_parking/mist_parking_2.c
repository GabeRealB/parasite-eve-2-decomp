#include "mist_parking_private.h"

#include "types.h"

#include "rooms/mist_parking.h"

#include "gameplay/animation.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/inventory.h"
#include "gameplay/item_menu.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
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

extern EvsCommand D_mist_parking_8018F374[];
extern EvsCommand D_mist_parking_8018F4AC[];
extern EvsCommand D_mist_parking_8018F5E4[];
extern EvsCommand D_mist_parking_8018F824[];
extern EvsCommand D_mist_parking_8018F9A4[];
extern EvsCommand D_mist_parking_8018FA4C[];
extern EvsCommand D_mist_parking_8018FB3C[];
extern s32        D_mist_parking_8018FBFC[];
extern s32        D_mist_parking_8018FC10[];

static void _modelPlacementAttachPartTask(Task* childTask);
static void _mistParkingWaitOptionDialogAnswer(Task* task);

static void _mistParkingIdleAttachedModelState(Task* unusedTask);
static void _mistParkingOpenOptionDialog(Task* task);
static void _mistParkingExitOptionDialogTask(Task* task);

/// State handlers of the same shape for a task that attaches a model to a
/// parent's part and then idles; nothing in the room reads this table.
static const TaskFuncTable3 D_mist_parking_8017D7E8 = {
    {
        _modelPlacementAttachPartTask,
        _mistParkingIdleAttachedModelState,
        taskKill,
    },
};
/// State handlers of the two-option choice task `_mistParkingOptionDialogTask` runs.
static const TaskFuncTable3 D_mist_parking_8017D7F4 = {
    {
        _mistParkingOpenOptionDialog,
        _mistParkingWaitOptionDialogAnswer,
        _mistParkingExitOptionDialogTask,
    },
};

extern AnimationPlayRequest D_mist_parking_8018D848;
extern AnimationPlayRequest D_mist_parking_8018DE38;
extern AnimationPlayRequest D_mist_parking_8018DE88;
extern AnimationPlayRequest D_mist_parking_8018DEB0;
extern AnimationPlayRequest D_mist_parking_8018DEC4;
extern AnimationPlayRequest D_mist_parking_8018DED8;

void        func_mist_parking_80182A44(Task*);
static void _mistParkingDepartureMenuTask(Task* task);
void        func_mist_parking_80183100(s32);
void        func_mist_parking_8018312C(s32);
void        func_mist_parking_8018316C(s32);
void        func_mist_parking_801831F0(s32);
static void _mistParkingReleaseCutsceneModel(s32 descriptorIndex);

/// Descriptor selected by the arrival conversation's model-release callback.
enum { MIST_PARKING_CUTSCENE_MODEL_DESCRIPTOR_INDEX = 0 };
static void _mistParkingOptionDialogTask(Task* task);
static void _mistParkingContinueShopChoiceTask(Task* task);
void        func_mist_parking_8018354C(void);
void        func_mist_parking_8018357C(Task*);
void        func_mist_parking_80183600(void);

TaskDesc D_mist_parking_8018D75C[9] = {
    { { { TASK_BODY_TMD, 192 } }, mistParkingCutsceneModelPitchTask, { .model = &gMistParkingModel09B9C } },
    { { { TASK_BODY_NONE, 192 } }, _mistParkingOptionDialogTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, mistParkingContinueDepartureChoiceTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_mist_parking_8018357C, { .value = 0 } },
    { { { TASK_BODY_NONE, 97 } }, mistParkingAimPlayerHeadAtTalkPartnerTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, mistParkingDelayDisplayModeExitTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _mistParkingContinueShopChoiceTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_mist_parking_80182A44, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _mistParkingDepartureMenuTask, { .value = 0 } },
};

AnimationSet* D_mist_parking_8018D7C8[25] = {
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

AnimationBankCopyRequest D_mist_parking_8018D82C = { { .sets = D_mist_parking_8018D7C8 }, ARRAY_SIZE(D_mist_parking_8018D7C8) };

AnimationPlayRequest D_mist_parking_8018D834 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_8018D848 = { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_8018D85C = { { .index = 1 }, 49, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_8018D870 = { { .index = 1 }, 50, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_8018D884 = { { .index = 1 }, 51, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_8018D898 = { { .index = 1 }, 52, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_8018D8AC = { { .index = 1 }, 53, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_8018D8C0 = { { .index = 1 }, 54, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_8018D8D4 = { { .index = 1 }, 55, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_8018D8E8 = { { .index = 1 }, 56, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_8018D8FC = { { .index = 1 }, 57, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_8018D910 = { { .index = 1 }, 58, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_8018D924 = { { .index = 1 }, 59, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_8018D938 = { { .index = 1 }, 60, ANIMATION_BLEND_INTERPOLATE, 30, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_8018D94C = { { .index = 1 }, 61, ANIMATION_BLEND_INTERPOLATE, 30, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_8018D960 = { { .index = 1 }, 62, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_8018D974 = { { .index = 1 }, 63, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_8018D988 = { { .index = 1 }, 64, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_8018D99C = { { .index = 1 }, 65, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_8018D9B0 = { { .index = 1 }, 66, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_8018D9C4 = { { .index = 1 }, 67, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_8018D9D8 = { { .index = 1 }, 68, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_8018D9EC = { { .index = 1 }, 69, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_8018DA00 = { { .index = 1 }, 70, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_8018DA14 = { { .index = 1 }, 71, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

s8 D_mist_parking_8018DA28[28] = {
    0,
    1,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    1,
    0,
    0,
    0,
    0,
    1,
    1,
    1,
    0,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
};

ActorTransform D_mist_parking_8018DA44 = { { 3310, 0, -3550, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_mist_parking_8018DA5C = { { 4494, 0, -3867, 0 }, { 0, 1365, 0, 0 } };

ActorTransform D_mist_parking_8018DA74 = { { 3270, 0, -3550, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_mist_parking_8018DA8C = { { 4370, 0, -4000, 0 }, { 0, 1365, 0, 0 } };

ActorTransform D_mist_parking_8018DAA4 = { { 4560, 0, -4916, 0 }, { 0, 1630, 0, 0 } };

AnimationPlayRequest D_mist_parking_8018DABC = { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_8018DAD0 = { { .index = 0 }, 1, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_8018DAE4[3] = {
    { { .index = 0 }, 2, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 3, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 4, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
};

AnimationPlayRequest D_mist_parking_8018DB20 = { { .index = 0 }, 5, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_8018DB34 = { { .index = 0 }, 6, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_8018DB48 = { { .index = 0 }, 7, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_8018DB5C = { { .index = 0 }, 8, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_8018DB70[6] = {
    { { .index = 0 }, 9, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 10, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 11, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 12, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 13, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 14, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
};

AnimationPlayRequest D_mist_parking_8018DBE8 = { { .index = 0 }, 15, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_8018DBFC = { { .index = 0 }, 16, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_8018DC10 = { { .index = 0 }, 17, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_8018DC24 = { { .index = 0 }, 18, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_8018DC38 = { { .index = 0 }, 19, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_8018DC4C = { { .index = 0 }, 20, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_8018DC60[6] = {
    { { .index = 0 }, 21, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 22, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 23, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 24, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 25, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 26, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
};

AnimationPlayRequest D_mist_parking_8018DCD8 = { { .index = 0 }, 27, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_8018DCEC = { { .index = 0 }, 28, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_8018DD00 = { { .index = 0 }, 29, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_8018DD14 = { { .index = 0 }, 30, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_8018DD28 = { { .index = 0 }, 31, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_8018DD3C[2] = {
    { { .index = 0 }, 32, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 33, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
};

AnimationPlayRequest D_mist_parking_8018DD64 = { { .index = 0 }, 34, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_8018DD78 = { { .index = 0 }, 35, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

ActorTransform D_mist_parking_8018DD8C = { { 6400, 0, -4800, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_mist_parking_8018DDA4 = { { 5150, 0, -4450, 0 }, { 0, -515, 0, 0 } };

ActorTransform D_mist_parking_8018DDBC = { { 5030, 0, -5680, 0 }, { 0, -296, 0, 0 } };

ActorTransform D_mist_parking_8018DDD4 = { { -1960, 0, -4650, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_mist_parking_8018DDEC = { { 5150, 0, -4450, 0 }, { 0, -715, 0, 0 } };

ActorMotionWalkAnim D_mist_parking_8018DE04 = { .animationId = 2, .nextAnimId = 26 };

ActorMotionWalkAnim D_mist_parking_8018DE0C = { .animationId = 2, .nextAnimId = 33 };

ActorCommand D_mist_parking_8018DE14 = { { .loc = { 0, 0 } }, 0 };

ActorCommand D_mist_parking_8018DE18 = { { .loc = { 0, 0 } }, 1 };

ActorCommand D_mist_parking_8018DE1C = { { .loc = { 0, 0 } }, 2 };

ActorCommand D_mist_parking_8018DE20 = { { .loc = { 0, 0 } }, 3 };

AnimationPlayRequest D_mist_parking_8018DE24 = { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_mist_parking_8018DE38 = { { .index = 0 }, 1, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_8018DE4C[3] = {
    { { .index = 0 }, 2, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 3, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 0 }, 4, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE },
};

AnimationPlayRequest D_mist_parking_8018DE88 = { { .index = 0 }, 5, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_8018DE9C = { { .index = 0 }, 6, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_8018DEB0 = { { .index = 0 }, 7, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_8018DEC4 = { { .index = 0 }, 8, ANIMATION_BLEND_INTERPOLATE, 5, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_mist_parking_8018DED8 = { { .index = 0 }, 9, ANIMATION_BLEND_INTERPOLATE, 15, ANIMATION_WORLD_COLLISION_ENABLE };

ActorTransform D_mist_parking_8018DEEC = { { 8460, 0, 92, 0 }, { 0, 2047, 0, 0 } };

u8 D_mist_parking_8018DF04[8] = {
    130,
    220,
    130,
    190,
    130,
    230,
    0,
    0,
};

u8 D_mist_parking_8018DF0C[8] = {
    130,
    166,
    130,
    166,
    0,
    0,
    0,
    0,
};

u8 D_mist_parking_8018DF14[8] = {
    130,
    205,
    130,
    162,
    0,
    0,
    0,
    0,
};

u8 D_mist_parking_8018DF1C[8] = {
    130,
    162,
    130,
    162,
    130,
    166,
    0,
    0,
};

u8* D_mist_parking_8018DF24[4] = {
    D_mist_parking_8018DF04,
    D_mist_parking_8018DF0C,
    D_mist_parking_8018DF14,
    D_mist_parking_8018DF1C,
};

EvsCommand D_mist_parking_8018DF34[155] = {
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_mist_parking_80183600 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_mist_parking_8018D82C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_mist_parking_8018DA44 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_mist_parking_8018DD8C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2004 }, { .message = { .pointer = &D_mist_parking_8018DEEC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_mist_parking_8018DE14 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_8018DE88 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 14 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_mist_parking_801831F0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D884 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x51130005 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mistParkingReleaseCutsceneModel }, { .value = MIST_PARKING_CUTSCENE_MODEL_DESCRIPTOR_INDEX }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_mist_parking_8018DA74 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D898 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2013 }, { .message = { .pointer = &D_mist_parking_8018DDA4 } }, { .message = { .pointer = &D_mist_parking_8018DE0C } } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 31 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x51130006 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D8AC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D8C0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_8018DCEC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_mist_parking_8018DDA4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x5113000C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = 0x5113000C }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D8D4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x5113000D }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_8018DCD8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_8018DD64 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D8E8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x51130011 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_mist_parking_8018DA5C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D924 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_8018DC10 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 12 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x51130008 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_mist_parking_8018DA5C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_8018DC24 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D8FC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_mist_parking_8018DE1C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 28 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x5113000E }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_mist_parking_8018DE18 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D988 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_8018DC4C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D924 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_8018DAD0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D9B0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_8018DCD8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D870 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D9C4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_8018DD00 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D988 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_8018DD28 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D9B0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_8018DCD8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D938 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_8018DAD0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D94C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_8018DD64 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D9C4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_mist_parking_8018DA8C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D988 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_8018DC38 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D924 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D9C4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_mist_parking_8018DDEC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_8018DAD0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_8018DB34 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_8018DAD0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_mist_parking_8018DA5C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018DA00 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_mist_parking_8018DDA4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018DA14 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D848 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_8018DB34 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D848 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D9C4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_8018DAD0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D974 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = mistParkingControlPlayerHeadAim }, { .value = MIST_PARKING_HEAD_AIM_FORCE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 90 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = mistParkingControlPlayerHeadAim }, { .value = MIST_PARKING_HEAD_AIM_STOP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = mistParkingSetPierceCollisionPatchLowered }, { .value = MIST_PARKING_PIERCE_PATCH_RESTORED }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2013 }, { .message = { .pointer = &D_mist_parking_8018DDBC } }, { .message = { .pointer = &D_mist_parking_8018DE04 } } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_mist_parking_8018DE20 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 120 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = mistParkingSetConversationProgress }, { .value = MIST_PARKING_CONVERSATION_INTRO_COMPLETE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_mist_parking_8018EDBC[23] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_DIRTY_VIEW, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _mistParkingReleaseCutsceneModel }, { .value = MIST_PARKING_CUTSCENE_MODEL_DESCRIPTOR_INDEX }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = mistParkingControlPlayerHeadAim }, { .value = MIST_PARKING_HEAD_AIM_STOP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = mistParkingSetPierceCollisionPatchLowered }, { .value = MIST_PARKING_PIERCE_PATCH_RESTORED }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_mist_parking_8018DA5C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_mist_parking_8018DDA4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_mist_parking_8018DE18 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2013 }, { .message = { .pointer = &D_mist_parking_8018DDBC } }, { .message = { .pointer = &D_mist_parking_8018DE04 } } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_mist_parking_8018DE20 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 100 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = mistParkingSetConversationProgress }, { .value = MIST_PARKING_CONVERSATION_INTRO_COMPLETE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_mist_parking_8018EFE4[8] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_mist_parking_8018DDA4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_mist_parking_8018DE18 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2013 }, { .message = { .pointer = &D_mist_parking_8018DDBC } }, { .message = { .pointer = &D_mist_parking_8018DE04 } } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2004 }, { .message = { .pointer = &D_mist_parking_8018DEEC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_8018DE88 } }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_mist_parking_8018F0A4[10] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_mist_parking_80183600 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_mist_parking_8018D82C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_mist_parking_80183100 }, { .value = 0xF0000 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D988 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_8018DB5C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = mistParkingControlPlayerHeadAim }, { .value = MIST_PARKING_HEAD_AIM_STOP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_8018DAD0 } }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_mist_parking_8018F194[10] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_mist_parking_80183600 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_mist_parking_8018D82C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_mist_parking_80183100 }, { .value = 0xF0001 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D988 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_8018DB5C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = mistParkingControlPlayerHeadAim }, { .value = MIST_PARKING_HEAD_AIM_STOP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_8018DAD0 } }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_mist_parking_8018F284[10] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_mist_parking_80183600 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_mist_parking_8018D82C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_mist_parking_80183100 }, { .value = 0xF0002 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D988 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_8018DB5C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = mistParkingControlPlayerHeadAim }, { .value = MIST_PARKING_HEAD_AIM_STOP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_8018DAD0 } }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_mist_parking_8018F374[13] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_mist_parking_80183600 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_mist_parking_8018D82C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_mist_parking_8018DE1C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = mistParkingSelectDialogueResource }, { .value = MIST_PARKING_DIALOGUE_DEPARTURE_MENU }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 5 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_mist_parking_8018DAA4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D988 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_8018DB20 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_mist_parking_8018F4AC[13] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_mist_parking_8018D82C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_mist_parking_80183100 }, { .value = 0x50001 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D9D8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_8018DBE8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = mistParkingControlPlayerHeadAim }, { .value = MIST_PARKING_HEAD_AIM_STOP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D9D8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_mist_parking_8018DE20 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = mistParkingSelectDialogueResource }, { .value = MIST_PARKING_DIALOGUE_DEFAULT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_mist_parking_8018F5E4[24] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_mist_parking_8018D82C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_mist_parking_80183100 }, { .value = 0x50002 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018DA00 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_8018DBE8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = mistParkingControlPlayerHeadAim }, { .value = MIST_PARKING_HEAD_AIM_STOP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1010 }, { .message = { .pointer = &D_mist_parking_8018DA44 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2013 }, { .message = { .pointer = &D_mist_parking_8018DDD4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_mist_parking_8018DE20 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CANCEL_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = mistParkingQueueDelayedDisplayModeExit }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = mistParkingSelectDialogueResource }, { .value = MIST_PARKING_DIALOGUE_DEFAULT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4004 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_mist_parking_8018312C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_mist_parking_8018354C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_mist_parking_8018F824[16] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_mist_parking_8018D82C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_mist_parking_80183100 }, { .value = 0x50003 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D9D8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_8018DAD0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_8018DBE8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = mistParkingControlPlayerHeadAim }, { .value = MIST_PARKING_HEAD_AIM_STOP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CANCEL_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = mistParkingQueueDelayedDisplayModeExit }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_mist_parking_8018316C }, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = mistParkingSelectDialogueResource }, { .value = MIST_PARKING_DIALOGUE_DEFAULT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_mist_parking_8018F9A4[7] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_mist_parking_8018D82C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D848 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_8018DE38 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_8018DEB0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_mist_parking_8018FA4C[10] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_mist_parking_8018D82C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackResult = shopOpenSession }, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D848 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_8018DEC4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_8018DED8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_8018DE88 } }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_mist_parking_8018FB3C[8] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_mist_parking_8018D82C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_mist_parking_8018D848 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_8018DEC4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_8018DED8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_mist_parking_8018DE88 } }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

s32 D_mist_parking_8018FBFC[5] = {
    161,
    61,
    63,
    11,
    108,
};

s32 D_mist_parking_8018FC10[5] = {
    50,
    1,
    1,
    1,
    1,
};

/// The labels of the two options `_mistParkingOpenOptionDialog` offers, and
/// the alternative pair it uses when the task's `spawnArg1` is 1.
extern u8* D_mist_parking_8018DF24[4];

void func_mist_parking_80182A44(Task* task)
{
    s32                                i;
    s32                                flag;
    s16                                prize;
    MistParkingPrizeAnnouncementState* announcement = &D_mist_parking_80195328;

    switch (task->state) {
        case 0:
            memFillBytes(announcement, 0, sizeof(*announcement));
            evsStartScript(D_mist_parking_8018F9A4, EVENT_SCRIPT_HUD_KEEP);
            capRunCommand(2, CAP_PLAYBACK_IN_PLACE);
            task->state++;
            break;
        case 1:
            if (gGameSession->eventState != 0) {
                return;
            }
            if (capIsBusy() != 0) {
                return;
            }
            for (i = 0; i < 5; i++) {
                if (gameFlagGetNibble(i + 0x125) == 2) {
                    task->state = 2;
                    return;
                }
            }
            task->state = 6;
            break;
        case 2:
            mistParkingSelectDialogueResource(MIST_PARKING_DIALOGUE_PRIZES);
            evsStartScript(D_mist_parking_8018F9A4, EVENT_SCRIPT_HUD_KEEP);
            capRunCommand(6, CAP_PLAYBACK_IN_PLACE);
            task->state++;
            break;
        case 3:
            if (gGameSession->eventState != 0) {
                return;
            }
            if (capIsBusy() != 0) {
                return;
            }
            switch (capGetVariantKey()) {
                case 1:
                    announcement->prizeTimer = MIST_PARKING_PRIZE_ANNOUNCEMENT_FRAMES;
                    capRunCommand(7, CAP_PLAYBACK_IN_PLACE);
                    task->state = 4;
                    break;
                case 4:
                    for (i = 0; i < 5; i++) {
                        flag = i + 0x125;
                        if (gameFlagGetNibble(flag) == 2) {
                            gameFlagSetNibble(flag, 3);
                        }
                    }
                    evsStartScript(D_mist_parking_8018F9A4, EVENT_SCRIPT_HUD_KEEP);
                    capRunCommand(8, CAP_PLAYBACK_IN_PLACE);
                    task->state = 6;
                    break;
                case 3:
                    for (i = 0; i < 4; i++) {
                        flag = i + 0x125;
                        if (gameFlagGetNibble(flag) == 2 && inventoryGiveItem(Gp_ScanPtrs[3], D_mist_parking_8018FBFC[i], D_mist_parking_8018FC10[i]) != 0) {
                            gameFlagSetNibble(flag, 3);
                            areaSetCurrentObjectState(i + 0x20, 2);
                        }
                    }
                    if (gameFlagGetNibble(GAME_FLAG_SHOOTING_GALLERY_PRIZE_4_STATE) == 2 && inventoryIsItemLimitReached(0x6C) == 0) {
                        if (inventoryGiveItem(Gp_ScanPtrs[3], 0x6C, 1) != 0) {
                            gameFlagSetNibble(GAME_FLAG_SHOOTING_GALLERY_PRIZE_4_STATE, 3);
                            areaSetCurrentObjectState(0x24, 2);
                        }
                    }
                    evsStartScript(D_mist_parking_8018F9A4, EVENT_SCRIPT_HUD_KEEP);
                    capRunCommand(4, CAP_PLAYBACK_IN_PLACE);
                    task->state = 6;
                    break;
            }
            break;
        case 4:
            if (capIsBusy() != 0) {
                return;
            }
            // Give each prize its turn: the caption of one still waiting here
            // starts partway through, and its state flag moves on when the
            // turn ends.
            announcement->prizeTimer--;
            if (announcement->prizeTimer == MIST_PARKING_PRIZE_ANNOUNCEMENT_CAPTION_FRAME) {
                prize = announcement->prizeIndex;
                if (gameFlagGetNibble(prize + 0x125) == 2) {
                    capStartSequenceSlot(5, 0, prize);
                }
                return;
            }
            if (announcement->prizeTimer != 0) {
                return;
            }
            prize = announcement->prizeIndex;
            if (areaGetCurrentObjectState(prize + 0x20) != 1) {
                gameFlagSetNibble(prize + 0x125, 3);
            }
            announcement->prizeTimer = MIST_PARKING_PRIZE_ANNOUNCEMENT_FRAMES;
            announcement->prizeIndex++;
            if (announcement->prizeIndex >= 5) {
                task->state++;
            }
            break;
        case 5:
            if (gGameSession->eventState != 0) {
                return;
            }
            if (capIsBusy() == 0) {
                evsStartScript(D_mist_parking_8018F9A4, EVENT_SCRIPT_HUD_KEEP);
                capRunCommand(2, CAP_PLAYBACK_IN_PLACE);
                task->state++;
            }
            /* fallthrough */
        case 6:
            if (gGameSession->eventState != 0) {
                return;
            }
            if (capIsBusy() != 0) {
                return;
            }
            task->state++;
            break;
        case 7:
            task->killCountdown++;
            if (task->killCountdown >= 0xB) {
                mistParkingSelectDialogueResource(MIST_PARKING_DIALOGUE_DEFAULT);
                capRunCommand(4, CAP_PLAYBACK_IN_PLACE);
                task->killCountdown = 0;
                task->state++;
            }
            break;
        case 8:
            if (capIsBusy() != 0) {
                return;
            }
            if (capGetVariantKey() == 1) {
                evsStartScript(D_mist_parking_8018FA4C, EVENT_SCRIPT_HUD_KEEP);
            } else {
                evsStartScript(D_mist_parking_8018FB3C, EVENT_SCRIPT_HUD_KEEP);
            }
            task->state++;
            break;
        case 9:
            task->killCountdown++;
            if (task->killCountdown == 0xA) {
                capRunCommand(3, CAP_PLAYBACK_IN_PLACE);
            }
            if (gGameSession->eventState != 0) {
                return;
            }
            task->state++;
            break;
        case 10:
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
            evsStartScript(D_mist_parking_8018F374, EVENT_SCRIPT_HUD_KEEP);
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
                    evsStartScript(D_mist_parking_8018F4AC, EVENT_SCRIPT_HUD_KEEP);
                    break;
                case MIST_PARKING_DEPARTURE_DRYFIELD:
                    evsStartScript(D_mist_parking_8018F5E4, EVENT_SCRIPT_HUD_KEEP);
                    break;
                case MIST_PARKING_DEPARTURE_SHOOTING_GALLERY:
                    evsStartScript(D_mist_parking_8018F824, EVENT_SCRIPT_HUD_KEEP);
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

#include "../../shared/model_placement_attach_part.inc.c"

/// Keeps an attached model task idle between attachment and teardown.
///
/// Ignores the task and makes no state change. Its three-state table has no
/// observed dispatcher in this room; the retained callback performs no drawing.
static void _mistParkingIdleAttachedModelState(Task* unusedTask)
{
}

void func_mist_parking_80183100(s32 arg0)
{
    capStartSequenceSlot(arg0 >> 16, 0, arg0);
}

void func_mist_parking_8018312C(s32 arg0)
{
    taskSpawnFromTable(D_mist_parking_8018FC24, 0, arg0, 0);
    gGameSession->freezeRoomObjs = 1;
}

void func_mist_parking_8018316C(s32 arg0)
{
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage = GAME_STAGE_ACROPOLIS;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp  = 1;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room  = 1;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area  = arg0;
    gDisplayState.spriteVariant                                 = 1;
    sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
    taskSpawn(0, 0x11, 0, 0);
    if (arg0 == 5) {
        gameFlowBeginLoadScreen(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc, GAME_FLOW_LOAD_CAPTION_NORMAL);
    }
}

void func_mist_parking_801831F0(s32 arg0)
{
    Task**     slot;
    Task*      task;
    TmdObject* obj;

    if (arg0 == 0) {
        slot = &D_mist_parking_80195320;
    } else {
        slot = NULL;
    }

    if ((slot != NULL) && (*slot == NULL)) {
        task  = taskSpawnFromTable(D_mist_parking_8018D75C, arg0, 0, 0);
        *slot = task;
        if (task != NULL) {
            obj         = task->extra.tmd;
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
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
        if (D_mist_parking_80195320 != NULL) {
            taskKill(D_mist_parking_80195320);
        }
        D_mist_parking_80195320 = NULL;
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

    stateHandlers = D_mist_parking_8017D7F4;
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
    labelCursor           = D_mist_parking_8018DF24;
    task->work            = dialog;
    task->exitCallback    = _mistParkingExitOptionDialogTask;

    /// Labels and links both rows before the UI borrows the dialog.
    ///
    /// Captures task, dialog, option, labelCursor, alternatePairSelector and
    /// optionIndex. Advances both cursors past the two rows, leaves optionIndex
    /// at two and terminates the last link. Takes no arguments.
#define MIST_PARKING_LINK_OPTION_LABELS()                                                          \
    {                                                                                              \
        for (; optionIndex < ARRAY_SIZE(dialog->options); optionIndex++) {                         \
            if (task->spawnArg1.value == alternatePairSelector) {                                  \
                option->text = D_mist_parking_8018DF24[optionIndex + ARRAY_SIZE(dialog->options)]; \
            } else {                                                                               \
                option->text = *labelCursor;                                                       \
            }                                                                                      \
            option->next = option + 1;                                                             \
            option++;                                                                              \
            labelCursor++;                                                                         \
        }                                                                                          \
        option[-1].next = NULL;                                                                    \
    }
    MIST_PARKING_LINK_OPTION_LABELS();
#undef MIST_PARKING_LINK_OPTION_LABELS

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
            evsStartScript(D_mist_parking_8018F5E4, EVENT_SCRIPT_HUD_KEEP);
        } else {
            evsStartScript(D_mist_parking_8018F4AC, EVENT_SCRIPT_HUD_KEEP);
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
            evsStartScript(D_mist_parking_8018FB3C, EVENT_SCRIPT_HUD_KEEP);
        } else {
            evsStartScript(D_mist_parking_8018FA4C, EVENT_SCRIPT_HUD_KEEP);
        }
        taskKill(task);
    }
}

/// Spawns entry 3 of `D_mist_parking_8018D75C`.
void func_mist_parking_8018354C(void)
{
    taskSpawnFromTable(D_mist_parking_8018D75C, 3, 0, 0);
}

void func_mist_parking_8018357C(Task* arg0)
{
    playerActorPrepareDryfieldLoadout();
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage = GAME_STAGE_DRYFIELD;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area  = GAME_AREA_DRYFIELD_GAS_STATION;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp  = 1;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room  = 1;
    gDisplayState.spriteVariant                                 = 1;
    gameFlowBeginLoadScreen(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc, GAME_FLOW_LOAD_CAPTION_ALTERNATE);
    sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
    taskSpawn(0, 0x11, 0, 0);
    taskKill(arg0);
}

/// Spawns entry 4 of `D_mist_parking_8018D75C` and keeps its handle in
/// `D_mist_parking_80195324`.
void func_mist_parking_80183600(void)
{
    D_mist_parking_80195324 = taskSpawnFromTable(D_mist_parking_8018D75C, 4, 0, 0);
}
