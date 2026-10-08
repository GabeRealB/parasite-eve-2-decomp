#include "rooms/acropolis_cafeteria.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "acropolis_cafeteria_private.h"

#include "actors/actor_202900.h"

#include "actors/actor_210600.h"

#include "actors/actor_310600.h"

#include "actors/task_tables.h"

#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/area_transitions.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/collision.h"
#include "gameplay/companion_load.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "mapui/map_akropolis.h"
#include "../../shared/room_visual_effects.h"

extern AnimationSet* D_acropolis_cafeteria_80182C60[4];

extern AnimationSet* D_acropolis_cafeteria_80182C40[1];

extern void func_807245E4(void*);
extern void func_80724608(void*, s32, s32, void*);

extern TaskMessageEntry D_acropolis_cafeteria_80182AA8[];

extern ActorTransform D_acropolis_cafeteria_80182D28;
extern s32            D_acropolis_cafeteria_80182DB8;
extern ActorTransform D_acropolis_cafeteria_80182DDC;
extern EvsCommand     D_acropolis_cafeteria_80182E74[];
extern EvsCommand     D_acropolis_cafeteria_801831BC[];
extern EvsCommand     D_acropolis_cafeteria_8018330C[];
extern EvsCommand     D_acropolis_cafeteria_801834D4[];
extern EvsCommand     D_acropolis_cafeteria_8018363C[];
extern EvsCommand     D_acropolis_cafeteria_80183DBC[];
extern EvsCommand     D_acropolis_cafeteria_80183F3C[];
extern s32            D_acropolis_cafeteria_80184164;
extern RECT           D_acropolis_cafeteria_80184168;
extern RECT           D_acropolis_cafeteria_80184170;

static void       _acropolisCafeteriaTickPlayerDebug(Task* task);
static void       _acropolisCafeteriaInitRoom(Task* task);
static inline s32 _acropolisCafeteriaResolvePatioRoom(const RoomEventMsg* request, RoomEventMsg* reply);

enum {
    ACROPOLIS_CAFETERIA_PROGRESS_AFTER_SCENE = 2,
    ACROPOLIS_CAFETERIA_PLACED_STRANGER      = 0,
};

/// State handlers of the room task: set-up, the per-frame tick and `taskKill`.
static const TaskFuncTable3 D_acropolis_cafeteria_8017D5C4 = {
    { _acropolisCafeteriaInitRoom, _acropolisCafeteriaTickPlayerDebug, taskKill },
};

static const char CafeteriaPlayerLabel[12] = "Player";

extern WorldCollisionGrid     D_acropolis_cafeteria_801887A8[1];
extern WorldCollisionOccluder D_acropolis_cafeteria_80189C94[2];
extern WorldCollisionTrigger  D_acropolis_cafeteria_801887CC[16];
extern WorldCollisionTrigger  D_acropolis_cafeteria_80188C8C[18];
extern WorldCollisionTrigger  D_acropolis_cafeteria_801891E4[16];
extern WorldCollisionTrigger  D_acropolis_cafeteria_801896A4[20];

static AnimationSet _gAcropolisCafeteriaAnimation07704;
s32                 func_acropolis_cafeteria_8017D700(Task*, s32, RoomEventMsg*, RoomEventMsg*);
static s32          _acropolisCafeteriaRefuseKeyItemMsg(Task* task, s32 messageId, s32 itemId, s32 unusedArg);
s32                 func_acropolis_cafeteria_8017E0DC(Task*, s32, s32, s32);
s32                 func_acropolis_cafeteria_8017E154(Task* task, s32 msgId, const void* firstArg, s32);
static s32          _acropolisCafeteriaPlaySoundMsg(Task* task, s32 messageId, s32 soundCue, s32 unusedArg);
void                func_acropolis_cafeteria_8017D8F8(Task*);
void                func_acropolis_cafeteria_8017DD1C(Task*);
static void         _acropolisCafeteriaSettleStrangerTask(Task* task);
static void         _acropolisCafeteriaSetPlayerSurface(s32 surfaceClass);
static void         _acropolisCafeteriaCancelPeEffects(void);
static void         _acropolisCafeteriaCopyPostBattleVram(void);
static void         _acropolisCafeteriaPreparePostBattleScene(void);

TaskMessageEntry D_acropolis_cafeteria_80182AA8[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_acropolis_cafeteria_8017D700 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_acropolis_cafeteria_8017E154 },
    { ROOM_MESSAGE_COMMAND, func_acropolis_cafeteria_8017E0DC },
    { ROOM_MESSAGE_USE_KEY_ITEM, _acropolisCafeteriaRefuseKeyItemMsg },
    { ROOM_MESSAGE_SOUND, _acropolisCafeteriaPlaySoundMsg },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_acropolis_cafeteria_80182AD8[4] = {
    { { { TASK_BODY_NONE, 32 } }, func_acropolis_cafeteria_8017D8F8, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, func_acropolis_cafeteria_8017DD1C, { .value = 0 } },

    { { { TASK_BODY_NONE, 32 } }, _acropolisCafeteriaSettleStrangerTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

AnimationPlayRequest D_acropolis_cafeteria_80182B08 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_cafeteria_80182B1C = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_cafeteria_80182B30 = { { .index = 1 }, 8, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_cafeteria_80182B44 = { { .index = 1 }, 9, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_cafeteria_80182B58 = { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_acropolis_cafeteria_80182B6C = { { 0, 128, 0, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_acropolis_cafeteria_80182B84 = { { -3759, -300, 18, 0 }, { 0, 2048, 0, 0 } };

ActorTransform D_acropolis_cafeteria_80182B9C = { { -3748, -300, -885, 0 }, { 0, 2048, 0, 0 } };

ActorTransform D_acropolis_cafeteria_80182BB4 = { { -3689, -300, -2934, 0 }, { 0, 2048, 0, 0 } };

ActorTransform D_acropolis_cafeteria_80182BCC = { { -3892, -300, -1836, 0 }, { 0, 1800, 0, 0 } };

ActorTransform D_acropolis_cafeteria_80182BE4 = { { -3000, -300, 1000, 0 }, { 0, 2048, 0, 0 } };

ActorTransform D_acropolis_cafeteria_80182BFC = { { -3000, -300, -600, 0 }, { 0, 2304, 0, 0 } };

ActorTransform D_acropolis_cafeteria_80182C14 = { { -2800, -300, -892, 0 }, { 0, 2048, 0, 0 } };

AnimationPlayRequest D_acropolis_cafeteria_80182C2C = { { .index = 0 }, 0, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationSet* D_acropolis_cafeteria_80182C40[1] = {
    &gActor202900Animation06098,
};

AnimationBankCopyRequest D_acropolis_cafeteria_80182C44 = { { .sets = D_acropolis_cafeteria_80182C40 }, ARRAY_SIZE(D_acropolis_cafeteria_80182C40) };

AnimationPlayRequest D_acropolis_cafeteria_80182C4C = { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationSet* D_acropolis_cafeteria_80182C60[4] = {
    &gActor310600Animation04ADC,
    &_gAcropolisCafeteriaAnimation07704,
    &gActor210600Animation07EF4,
    &gActor210600Animation0A050,
};

AnimationBankCopyRequest D_acropolis_cafeteria_80182C70 = { { .sets = D_acropolis_cafeteria_80182C60 }, ARRAY_SIZE(D_acropolis_cafeteria_80182C60) };

AnimationPlayRequest D_acropolis_cafeteria_80182C78 = { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_cafeteria_80182C8C = { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_cafeteria_80182CA0 = { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_cafeteria_80182CB4 = { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_acropolis_cafeteria_80182CC8 = { { -3106, -300, -2056, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_acropolis_cafeteria_80182CE0 = { { -3860, -299, -1365, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_acropolis_cafeteria_80182CF8 = { { -3890, -299, -1555, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_acropolis_cafeteria_80182D10 = { { -3490, -299, -4500, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_acropolis_cafeteria_80182D28 = { { -3860, -299, -1253, 0 }, { 0, 200, 0, 0 } };

ActorTransform D_acropolis_cafeteria_80182D40 = { { -3290, -299, -3500, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_acropolis_cafeteria_80182D58 = { { -3650, -299, -961, 0 }, { 0, 200, 0, 0 } };

ActorTransform D_acropolis_cafeteria_80182D70 = { { -3026, -300, -4126, 0 }, { 0, -200, 0, 0 } };

ActorTransform D_acropolis_cafeteria_80182D88 = { { -470, -1400, 701, 0 }, { 0, -800, 0, 0 } };

ActorCommand D_acropolis_cafeteria_80182DA0 = { { .loc = { 1, 4 } }, 0 };

ActorCommand D_acropolis_cafeteria_80182DA4 = { { .loc = { 1, 4 } }, 1 };

ActorCommand D_acropolis_cafeteria_80182DA8 = { { .loc = { 1, 4 } }, 2 };

ActorCommand D_acropolis_cafeteria_80182DAC = { { .loc = { 1, 4 } }, 3 };

ActorCommand D_acropolis_cafeteria_80182DB0 = { { .loc = { 1, 4 } }, 4 };

ActorCommand D_acropolis_cafeteria_80182DB4 = { { .loc = { 1, 4 } }, 5 };

s32 D_acropolis_cafeteria_80182DB8 = 0x70401;

ActorCommand D_acropolis_cafeteria_80182DBC = { { .loc = { 1, 4 } }, 8 };

ActorCommand D_acropolis_cafeteria_80182DC0 = { { .loc = { 1, 4 } }, 9 };

ActorTransform D_acropolis_cafeteria_80182DC4 = { { -3403, -250, 800, 0 }, { 0, 2048, 0, 0 } };

ActorTransform D_acropolis_cafeteria_80182DDC = { { -3850, -299, -900, 0 }, { 0, 2048, 0, 0 } };

ActorTransform D_acropolis_cafeteria_80182DF4 = { { -4400, -299, -1400, 0 }, { 0, 2048, 0, 0 } };

AnimationPlayRequest D_acropolis_cafeteria_80182E0C = { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_cafeteria_80182E20 = { { .index = 0 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_cafeteria_80182E34 = { { .index = 0 }, 3, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_cafeteria_80182E48 = { { .index = 0 }, 4, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

EvsSceneKey D_acropolis_cafeteria_80182E5C = { 1, 4, 11 };

EvsSceneKey D_acropolis_cafeteria_80182E64 = { 1, 6, 11 };

GameActorMoveAnim D_acropolis_cafeteria_80182E6C = { 19, 1 };

EvsCommand D_acropolis_cafeteria_80182E74[35] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _acropolisCafeteriaCancelPeEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_acropolis_cafeteria_80182C44 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_FADE_VOLUME, { .value = 76 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4004 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_cafeteria_80182B58 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_cafeteria_80182C4C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_RUN_MOVEMENT }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_acropolis_cafeteria_80182B6C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _acropolisCafeteriaSetPlayerSurface }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 69 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 54 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 54 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_REQUEST_SCENE_MUSIC, { .value = 6 }, { .value = 1 }, { .value = 1 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_cafeteria_80182B08 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_MUSIC_LOAD, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_EVENT_STATE, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_cafeteria_80182B08 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CANCEL_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_acropolis_cafeteria_80182B84 } }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_acropolis_cafeteria_801831BC[14] = {
    { EVENT_SCRIPT_OPCODE_REQUEST_SCENE_MUSIC, { .value = 6 }, { .value = 1 }, { .value = 1 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_cafeteria_80182B08 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_MUSIC_LOAD, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_EVENT_STATE, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_cafeteria_80182B08 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_acropolis_cafeteria_80182B84 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_acropolis_cafeteria_8018330C[19] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _acropolisCafeteriaPreparePostBattleScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_cafeteria_80182B44 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 11 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _acropolisCafeteriaCopyPostBattleVram }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_acropolis_cafeteria_80182CE0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_acropolis_cafeteria_80182DA8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x5104000C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_FRAMEBUFFER_BLEND, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 320 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_VIEW, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_cafeteria_80182B30 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_acropolis_cafeteria_801834D4[15] = {
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = 0x5104000C }, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_acropolis_cafeteria_80182DB0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _acropolisCafeteriaCopyPostBattleVram }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_acropolis_cafeteria_80182CE0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_VIEW, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_cafeteria_80182B30 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_acropolis_cafeteria_8018363C[80] = {
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_acropolis_cafeteria_80182E64 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 70 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_LIGHT_SCALE, { .value = 128 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_acropolis_cafeteria_80182CF8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_acropolis_cafeteria_80182C70 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_acropolis_cafeteria_80182B9C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_cafeteria_80182C78 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_acropolis_cafeteria_80182DC0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_acropolis_cafeteria_80182DB0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_acropolis_cafeteria_80182BB4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_acropolis_cafeteria_80182D40 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_acropolis_cafeteria_80182DB4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2004 }, { .message = { .pointer = &D_acropolis_cafeteria_80182DC4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_cafeteria_80182E20 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_FRAMEBUFFER_BLEND, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_acropolis_cafeteria_80182D10 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_acropolis_cafeteria_80182DAC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_cafeteria_80182C8C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_FRAMEBUFFER_BLEND, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_acropolis_cafeteria_80182D28 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2004 }, { .message = { .pointer = &D_acropolis_cafeteria_80182DDC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_acropolis_cafeteria_80182DC0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_cafeteria_80182E0C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_acropolis_cafeteria_80182BE4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 76 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 33 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 24 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1010 }, { .message = { .pointer = &D_acropolis_cafeteria_80182BFC } }, { .message = { .pointer = &D_acropolis_cafeteria_80182E6C } } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 57 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1006 }, { .message = { .pointer = &D_acropolis_cafeteria_80182BFC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_cafeteria_80182B08 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_acropolis_cafeteria_80182D58 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_acropolis_cafeteria_80182DBC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2004 }, { .message = { .pointer = &D_acropolis_cafeteria_80182DDC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_cafeteria_80182E34 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_acropolis_cafeteria_80182DDC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_cafeteria_80182CA0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 686 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2004 }, { .message = { .pointer = &D_acropolis_cafeteria_80182D88 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_acropolis_cafeteria_80182DA4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2004 }, { .message = { .pointer = &D_acropolis_cafeteria_80182DF4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_cafeteria_80182E48 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_acropolis_cafeteria_80182DF4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_cafeteria_80182CB4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 252 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_EVENT_STATE, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_LIGHT_SCALE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CANCEL_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_acropolis_cafeteria_80183DBC[10] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_LIGHT_SCALE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_EVENT_STATE, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CANCEL_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_acropolis_cafeteria_80183EAC[6] = {
    { EVENT_SCRIPT_OPCODE_SHAKE_SCREEN, { .value = 4 }, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 13 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_cafeteria_80182B08 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 250 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_EVENT_STATE, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_acropolis_cafeteria_80183F3C[19] = {
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_cafeteria_80182B08 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 12 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_acropolis_cafeteria_80182CC8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_acropolis_cafeteria_80182DA4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_FRAMEBUFFER_BLEND, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = SetDispMask }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = sceneEngageBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_acropolis_cafeteria_80182BCC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_acropolis_cafeteria_80182D70 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_FRAMEBUFFER_BLEND, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2007 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_acropolis_cafeteria_80184104[4] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2004 }, { .message = { .pointer = &D_acropolis_cafeteria_80182D88 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_acropolis_cafeteria_80182DA4 } }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

s32 D_acropolis_cafeteria_80184164 = 0;

RECT D_acropolis_cafeteria_80184168 = { 704, 0, 64, 256 };

RECT D_acropolis_cafeteria_80184170 = { 0, 271, 256, 1 };

TaskDesc D_acropolis_cafeteria_80184178[3] = {
    { { { TASK_BODY_NONE, 192 } }, acropolisCafeteriaStartMovieTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, acropolisCafeteriaBlackoutTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, acropolisCafeteriaPlayMovieTask, { .value = 0 } },
};

static AnimationPackedPose _gAcropolisCafeteriaAnimation07704Bank1[23] = {
#include "assets/acropolis_cafeteria_animation_07704_bank1.inc"
};

static AnimationPackedRotation _gAcropolisCafeteriaAnimation07704Bank4[286] = {
#include "assets/acropolis_cafeteria_animation_07704_bank4.inc"
};

static AnimationRecord _gAcropolisCafeteriaAnimation07704Records[349] = {
#include "assets/acropolis_cafeteria_animation_07704_records.inc"
};

static u16 _gAcropolisCafeteriaAnimation07704Indices[20] = {
#include "assets/acropolis_cafeteria_animation_07704_indices.inc"
};

static AnimationSet _gAcropolisCafeteriaAnimation07704 = {
    _gAcropolisCafeteriaAnimation07704Records,
    _gAcropolisCafeteriaAnimation07704Indices,
    { NULL, _gAcropolisCafeteriaAnimation07704Bank1, NULL, NULL, _gAcropolisCafeteriaAnimation07704Bank4, NULL, NULL, NULL },
};

TaskMessageEntry D_acropolis_cafeteria_80184CEC[2] = {
    { ACROPOLIS_CAFETERIA_MESSAGE_SET_PUFF_ENABLED, acropolisCafeteriaSetPuffEnabled },
    { TASK_MESSAGE_TABLE_END, NULL },
};

s32 D_acropolis_cafeteria_80184CFC = 0;

static TmdBone _gAcropolisCafeteriaModel077D8Skeleton[1] = {
#include "assets/acropolis_cafeteria_model_077D8_skeleton.inc"
};

static u32 _gAcropolisCafeteriaModel077D8PartVerts[1] = {
#include "assets/acropolis_cafeteria_model_077D8_partVerts.inc"
};

static SVECTOR _gAcropolisCafeteriaModel077D8Verts[14] = {
#include "assets/acropolis_cafeteria_model_077D8_verts.inc"
};

static u32 _gAcropolisCafeteriaModel077D8Stream[49] = {
#include "assets/acropolis_cafeteria_model_077D8_stream.inc"
};

TmdSource gAcropolisCafeteriaModel077D8 = {
    0,
    296,
    0,
    1,
    _gAcropolisCafeteriaModel077D8PartVerts,
    _gAcropolisCafeteriaModel077D8Verts,
    &_gAcropolisCafeteriaModel077D8Verts[14],
    _gAcropolisCafeteriaModel077D8Skeleton,
    _gAcropolisCafeteriaModel077D8Stream,
};

#include "../../shared/room_visual_effects_trail_data.inc.c"

static TmdBone _gAcropolisCafeteriaModel07CA8Skeleton[1] = {
#include "assets/acropolis_cafeteria_model_07CA8_skeleton.inc"
};

static u32 _gAcropolisCafeteriaModel07CA8PartVerts[1] = {
#include "assets/acropolis_cafeteria_model_07CA8_partVerts.inc"
};

static SVECTOR _gAcropolisCafeteriaModel07CA8Verts[46] = {
#include "assets/acropolis_cafeteria_model_07CA8_verts.inc"
};

static SVECTOR _gAcropolisCafeteriaModel07CA8Normals[72] = {
#include "assets/acropolis_cafeteria_model_07CA8_normals.inc"
};

static u32 _gAcropolisCafeteriaModel07CA8Stream[407] = {
#include "assets/acropolis_cafeteria_model_07CA8_stream.inc"
};

TmdSource gAcropolisCafeteriaModel07CA8 = {
    0,
    2952,
    0,
    1,
    _gAcropolisCafeteriaModel07CA8PartVerts,
    _gAcropolisCafeteriaModel07CA8Verts,
    _gAcropolisCafeteriaModel07CA8Normals,
    _gAcropolisCafeteriaModel07CA8Skeleton,
    _gAcropolisCafeteriaModel07CA8Stream,
};

static TmdBone _gAcropolisCafeteriaModel08638Skeleton[1] = {
#include "assets/acropolis_cafeteria_model_08638_skeleton.inc"
};

static u32 _gAcropolisCafeteriaModel08638PartVerts[1] = {
#include "assets/acropolis_cafeteria_model_08638_partVerts.inc"
};

static SVECTOR _gAcropolisCafeteriaModel08638Verts[31] = {
#include "assets/acropolis_cafeteria_model_08638_verts.inc"
};

static SVECTOR _gAcropolisCafeteriaModel08638Normals[62] = {
#include "assets/acropolis_cafeteria_model_08638_normals.inc"
};

static u32 _gAcropolisCafeteriaModel08638Stream[409] = {
#include "assets/acropolis_cafeteria_model_08638_stream.inc"
};

TmdSource gAcropolisCafeteriaModel08638 = {
    0,
    2880,
    0,
    1,
    _gAcropolisCafeteriaModel08638PartVerts,
    _gAcropolisCafeteriaModel08638Verts,
    _gAcropolisCafeteriaModel08638Normals,
    _gAcropolisCafeteriaModel08638Skeleton,
    _gAcropolisCafeteriaModel08638Stream,
};

static TmdBone _gAcropolisCafeteriaModel09090Skeleton[1] = {
#include "assets/acropolis_cafeteria_model_09090_skeleton.inc"
};

static u32 _gAcropolisCafeteriaModel09090PartVerts[1] = {
#include "assets/acropolis_cafeteria_model_09090_partVerts.inc"
};

static SVECTOR _gAcropolisCafeteriaModel09090Verts[46] = {
#include "assets/acropolis_cafeteria_model_09090_verts.inc"
};

static SVECTOR _gAcropolisCafeteriaModel09090Normals[71] = {
#include "assets/acropolis_cafeteria_model_09090_normals.inc"
};

static u32 _gAcropolisCafeteriaModel09090Stream[407] = {
#include "assets/acropolis_cafeteria_model_09090_stream.inc"
};

TmdSource gAcropolisCafeteriaModel09090 = {
    0,
    2952,
    0,
    1,
    _gAcropolisCafeteriaModel09090PartVerts,
    _gAcropolisCafeteriaModel09090Verts,
    _gAcropolisCafeteriaModel09090Normals,
    _gAcropolisCafeteriaModel09090Skeleton,
    _gAcropolisCafeteriaModel09090Stream,
};

static TmdBone _gAcropolisCafeteriaModel09A20Skeleton[1] = {
#include "assets/acropolis_cafeteria_model_09A20_skeleton.inc"
};

static u32 _gAcropolisCafeteriaModel09A20PartVerts[1] = {
#include "assets/acropolis_cafeteria_model_09A20_partVerts.inc"
};

static SVECTOR _gAcropolisCafeteriaModel09A20Verts[37] = {
#include "assets/acropolis_cafeteria_model_09A20_verts.inc"
};

static SVECTOR _gAcropolisCafeteriaModel09A20Normals[56] = {
#include "assets/acropolis_cafeteria_model_09A20_normals.inc"
};

static u32 _gAcropolisCafeteriaModel09A20Stream[334] = {
#include "assets/acropolis_cafeteria_model_09A20_stream.inc"
};

TmdSource gAcropolisCafeteriaModel09A20 = {
    0,
    2296,
    0,
    1,
    _gAcropolisCafeteriaModel09A20PartVerts,
    _gAcropolisCafeteriaModel09A20Verts,
    _gAcropolisCafeteriaModel09A20Normals,
    _gAcropolisCafeteriaModel09A20Skeleton,
    _gAcropolisCafeteriaModel09A20Stream,
};

WorldCollisionRoomResources D_acropolis_cafeteria_8018753C[4] = {
    { D_acropolis_cafeteria_801887A8, D_acropolis_cafeteria_801887CC, D_acropolis_cafeteria_801891E4, D_acropolis_cafeteria_80189C94 },
    { D_acropolis_cafeteria_801887A8, D_acropolis_cafeteria_801887CC, D_acropolis_cafeteria_801896A4, D_acropolis_cafeteria_80189C94 },
    { D_acropolis_cafeteria_801887A8, D_acropolis_cafeteria_801887CC, D_acropolis_cafeteria_801896A4, D_acropolis_cafeteria_80189C94 },
    { D_acropolis_cafeteria_801887A8, D_acropolis_cafeteria_80188C8C, D_acropolis_cafeteria_801896A4, D_acropolis_cafeteria_80189C94 },
};

u8 D_acropolis_cafeteria_8018757C[24] = {
    1,
    2,
    21,
    14,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    21,
    22,
    23,
    24,
};

u8 D_acropolis_cafeteria_80187594[24] = {
    1,
    2,
    21,
    22,
    15,
    6,
    7,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    21,
    22,
    23,
    24,
};

u8* D_acropolis_cafeteria_801875AC[4] = {
    gViewIdentityMap,
    D_acropolis_cafeteria_8018757C,
    D_acropolis_cafeteria_8018757C,
    D_acropolis_cafeteria_80187594,
};

ViewCount D_acropolis_cafeteria_801875BC[4] = { 24, 24, 24, 24 };

WorldCoordRoomLighting D_acropolis_cafeteria_801875C4[4] = {
    { D_acropolis_cafeteria_8018AA18, D_acropolis_cafeteria_8018C90C },
    { D_acropolis_cafeteria_8018AA18, D_acropolis_cafeteria_8018C90C },
    { D_acropolis_cafeteria_8018AA18, D_acropolis_cafeteria_8018C90C },
    { D_acropolis_cafeteria_8018AA18, D_acropolis_cafeteria_8018C90C },
};

DirectionWarpEntry D_acropolis_cafeteria_801875E4[3] = {
    { { { .word = 0 }, 1841, -236, -3256 }, { 0, 0, 0, 0 }, { { .word = 0 }, 1841, -236, -3256 }, { 0, 0, 0, 0 }, 0x51040009, 0x51040008, DIRECTION_WARP_SOUND_NONE, 7, DIRECTION_WARP_FLAG_NONE, 499 },
    { { { .word = 2048 }, -4047, -236, 2765 }, { 0, 0, 0, 0 }, { { .word = 2048 }, -4047, -236, 2765 }, { 0, 0, 0, 0 }, 0x51040002, 0x51040001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, 501 },
    { { { .word = 3072 }, -986, -300, -5486 }, { 0, 0, 0, 0 }, { { .word = 3072 }, -986, -300, -5486 }, { 0, 0, 0, 0 }, 0x51040005, 0x51040004, 0x51040003, 5, DIRECTION_WARP_FLAG_NONE, 500 },
};

static SVECTOR _gAcropolisCafeteriaCollision0B1E8Normals[36] = {
#include "assets/acropolis_cafeteria_collision_0B1E8_normals.inc"
};

static SVECTOR _gAcropolisCafeteriaCollision0B1E8Verts[207] = {
#include "assets/acropolis_cafeteria_collision_0B1E8_verts.inc"
};

static WorldCollisionGridFace _gAcropolisCafeteriaCollision0B1E8Faces[120] = {
#include "assets/acropolis_cafeteria_collision_0B1E8_faces.inc"
};

static s16 _gAcropolisCafeteriaCollision0B1E8Cells[480] = {
#include "assets/acropolis_cafeteria_collision_0B1E8_cells.inc"
};

#define GRID_CELL(i) (&_gAcropolisCafeteriaCollision0B1E8Cells[i])
static s16* _gAcropolisCafeteriaCollision0B1E8Table[9] = {
#include "assets/acropolis_cafeteria_collision_0B1E8_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_acropolis_cafeteria_801887A8[1] = {
    { NULL, _gAcropolisCafeteriaCollision0B1E8Normals, _gAcropolisCafeteriaCollision0B1E8Verts, _gAcropolisCafeteriaCollision0B1E8Faces, _gAcropolisCafeteriaCollision0B1E8Table, 5600, 6098, 3, 3, 4000, 120 },
};

WorldCollisionTrigger D_acropolis_cafeteria_801887CC[16] = {
    { NULL, NULL, NULL, { -1458, -1872, 1406, 0 }, { { -1092, 2672, -566, 0 }, { 1084, 2672, 560, 0 }, { -1092, -2672, -566, 0 }, { 1084, -2672, 560, 0 } }, { -1889, 0, 3647, 0 }, { 0, 0, 4096, 0 }, 2930, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1538, 0, 1726, 0 }, { { 1286, 2672, 820, 0 }, { -1293, 2672, -825, 0 }, { 1286, -2672, 820, 0 }, { -1293, -2672, -825, 0 } }, { 2202, 0, -3454, 0 }, { 0, 0, 4096, 0 }, 3072, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4945, 0, -2273, 0 }, { { -1054, 2672, -199, 0 }, { 1047, 2672, 189, 0 }, { -1054, -2672, -199, 0 }, { 1047, -2672, 189, 0 } }, { -747, 0, 4036, 0 }, { 0, 0, 4096, 0 }, 2873, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4866, 0, -1921, 0 }, { { 1073, 2672, 165, 0 }, { -1077, 2672, -172, 0 }, { 1073, -2672, 165, 0 }, { -1077, -2672, -172, 0 } }, { 633, 0, -4048, 0 }, { 0, 0, 4096, 0 }, 2884, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1442, 0, -4738, 0 }, { { 520, 2672, 1242, 0 }, { -521, 2672, -1243, 0 }, { 520, -2672, 1242, 0 }, { -521, -2672, -1243, 0 } }, { 3781, 0, -1586, 0 }, { 0, 0, 4096, 0 }, 2985, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1313, 0, -5121, 0 }, { { -441, 2672, -1044, 0 }, { 434, 2672, 1037, 0 }, { -441, -2672, -1044, 0 }, { 434, -2672, 1037, 0 } }, { -3783, 0, 1588, 0 }, { 0, 0, 4096, 0 }, 2896, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -817, 0, -3731, 0 }, { { 499, 2672, 345, 0 }, { -505, 2672, -353, 0 }, { 499, -2672, 345, 0 }, { -505, -2672, -353, 0 } }, { 2338, 0, -3366, 0 }, { 0, 0, 4096, 0 }, 2733, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1248, 0, 63, 0 }, { { 2033, 2672, -23, 0 }, { -2045, 2672, 15, 0 }, { 2033, -2672, -23, 0 }, { -2045, -2672, 15, 0 } }, { -39, 0, -4096, 0 }, { 0, 0, 4096, 0 }, 3357, 0, 7, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1408, 0, -225, 0 }, { { -2047, 2672, 11, 0 }, { 2030, 2672, -26, 0 }, { -2047, -2672, 11, 0 }, { 2030, -2672, -26, 0 } }, { 37, 0, 4112, 0 }, { 0, 0, 4096, 0 }, 3357, 0, 8, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4320, 0, 1280, 0 }, { { -1410, 2672, 296, 0 }, { 1409, 2672, -297, 0 }, { -1410, -2672, 296, 0 }, { 1409, -2672, -297, 0 } }, { 843, 0, 4010, 0 }, { 0, 0, 4096, 0 }, 3029, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4002, 0, 1566, 0 }, { { 1745, 2672, -397, 0 }, { -1745, 2672, 397, 0 }, { 1745, -2672, -397, 0 }, { -1745, -2672, 397, 0 } }, { -910, 0, -3999, 0 }, { 0, 0, 4096, 0 }, 3207, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -3040, 0, -2128, 0 }, { { -837, 2672, 8, 0 }, { 838, 2672, -8, 0 }, { -837, -2672, 8, 0 }, { 838, -2672, -8, 0 } }, { 37, 0, 4119, 0 }, { 0, 0, 4096, 0 }, 2792, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -3040, 0, -1792, 0 }, { { 837, 2672, -8, 0 }, { -838, 2672, 8, 0 }, { 837, -2672, -8, 0 }, { -838, -2672, 8, 0 } }, { -40, 0, -4122, 0 }, { 0, 0, 4096, 0 }, 2792, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1377, 0, -1697, 0 }, { { 819, 2672, 148, 0 }, { -829, 2672, -162, 0 }, { 819, -2672, 148, 0 }, { -829, -2672, -162, 0 } }, { 756, 0, -4028, 0 }, { 0, 0, 4096, 0 }, 2792, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1313, 0, -1984, 0 }, { { -829, 2672, -168, 0 }, { 814, 2672, 142, 0 }, { -829, -2672, -168, 0 }, { 814, -2672, 142, 0 } }, { -764, 0, 4040, 0 }, { 0, 0, 4096, 0 }, 2792, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -529, 0, -3825, 0 }, { { -408, 2672, -342, 0 }, { 405, 2672, 335, 0 }, { -408, -2672, -342, 0 }, { 405, -2672, 335, 0 } }, { -2627, 0, 3148, 0 }, { 0, 0, 4096, 0 }, 2721, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_acropolis_cafeteria_80188C8C[18] = {
    { NULL, NULL, NULL, { -1458, -1872, 1406, 0 }, { { -1092, 2672, -566, 0 }, { 1084, 2672, 560, 0 }, { -1092, -2672, -566, 0 }, { 1084, -2672, 560, 0 } }, { -1889, 0, 3647, 0 }, { 0, 0, 4096, 0 }, 2930, 0, 2, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1538, 0, 1726, 0 }, { { 1286, 2672, 820, 0 }, { -1293, 2672, -825, 0 }, { 1286, -2672, 820, 0 }, { -1293, -2672, -825, 0 } }, { 2202, 0, -3454, 0 }, { 0, 0, 4096, 0 }, 3072, 0, 6, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4945, 0, -2273, 0 }, { { -1054, 2672, -199, 0 }, { 1047, 2672, 189, 0 }, { -1054, -2672, -199, 0 }, { 1047, -2672, 189, 0 } }, { -747, 0, 4036, 0 }, { 0, 0, 4096, 0 }, 2873, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4866, 0, -1921, 0 }, { { 1073, 2672, 165, 0 }, { -1077, 2672, -172, 0 }, { 1073, -2672, 165, 0 }, { -1077, -2672, -172, 0 } }, { 633, 0, -4048, 0 }, { 0, 0, 4096, 0 }, 2884, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1442, 0, -4738, 0 }, { { 520, 2672, 1242, 0 }, { -521, 2672, -1243, 0 }, { 520, -2672, 1242, 0 }, { -521, -2672, -1243, 0 } }, { 3781, 0, -1586, 0 }, { 0, 0, 4096, 0 }, 2985, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1313, 0, -5121, 0 }, { { -441, 2672, -1044, 0 }, { 434, 2672, 1037, 0 }, { -441, -2672, -1044, 0 }, { 434, -2672, 1037, 0 } }, { -3783, 0, 1588, 0 }, { 0, 0, 4096, 0 }, 2896, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -817, 0, -3731, 0 }, { { 499, 2672, 345, 0 }, { -505, 2672, -353, 0 }, { 499, -2672, 345, 0 }, { -505, -2672, -353, 0 } }, { 2338, 0, -3366, 0 }, { 0, 0, 4096, 0 }, 2733, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1248, 0, 63, 0 }, { { 2033, 2672, -23, 0 }, { -2045, 2672, 15, 0 }, { 2033, -2672, -23, 0 }, { -2045, -2672, 15, 0 } }, { -39, 0, -4096, 0 }, { 0, 0, 4096, 0 }, 3357, 0, 7, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1408, 0, -225, 0 }, { { -2047, 2672, 11, 0 }, { 2030, 2672, -26, 0 }, { -2047, -2672, 11, 0 }, { 2030, -2672, -26, 0 } }, { 37, 0, 4112, 0 }, { 0, 0, 4096, 0 }, 3357, 0, 8, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4320, 0, 1280, 0 }, { { -1410, 2672, 296, 0 }, { 1409, 2672, -297, 0 }, { -1410, -2672, 296, 0 }, { 1409, -2672, -297, 0 } }, { 843, 0, 4010, 0 }, { 0, 0, 4096, 0 }, 3029, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4002, 0, 1566, 0 }, { { 1745, 2672, -397, 0 }, { -1745, 2672, 397, 0 }, { 1745, -2672, -397, 0 }, { -1745, -2672, 397, 0 } }, { -910, 0, -3999, 0 }, { 0, 0, 4096, 0 }, 3207, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -3040, 0, -2128, 0 }, { { -837, 2672, 8, 0 }, { 838, 2672, -8, 0 }, { -837, -2672, 8, 0 }, { 838, -2672, -8, 0 } }, { 37, 0, 4119, 0 }, { 0, 0, 4096, 0 }, 2792, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -3040, 0, -1792, 0 }, { { 837, 2672, -8, 0 }, { -838, 2672, 8, 0 }, { 837, -2672, -8, 0 }, { -838, -2672, 8, 0 } }, { -40, 0, -4122, 0 }, { 0, 0, 4096, 0 }, 2792, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1377, 0, -1697, 0 }, { { 819, 2672, 148, 0 }, { -829, 2672, -162, 0 }, { 819, -2672, 148, 0 }, { -829, -2672, -162, 0 } }, { 756, 0, -4028, 0 }, { 0, 0, 4096, 0 }, 2792, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1313, 0, -1984, 0 }, { { -829, 2672, -168, 0 }, { 814, 2672, 142, 0 }, { -829, -2672, -168, 0 }, { 814, -2672, 142, 0 } }, { -764, 0, 4040, 0 }, { 0, 0, 4096, 0 }, 2792, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -529, 0, -3825, 0 }, { { -408, 2672, -342, 0 }, { 405, 2672, 335, 0 }, { -408, -2672, -342, 0 }, { 405, -2672, 335, 0 } }, { -2627, 0, 3148, 0 }, { 0, 0, 4096, 0 }, 2721, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1316, 0, -611, 0 }, { { -827, 2672, 1287, 0 }, { 818, 2672, -1292, 0 }, { -827, -2672, 1287, 0 }, { 818, -2672, -1292, 0 } }, { 3459, 0, 2206, 0 }, { 0, 0, 4096, 0 }, 3072, 0, 6, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1249, -64, -576, 0 }, { { 817, 2672, -1294, 0 }, { -829, 2672, 1285, 0 }, { 817, -2672, -1294, 0 }, { -829, -2672, 1285, 0 } }, { -3454, 0, -2205, 0 }, { 0, 0, 4096, 0 }, 3072, 0, 3, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_acropolis_cafeteria_801891E4[16] = {
    { NULL, NULL, NULL, { -3984, -368, 2704, 0 }, { { -496, 0, -272, 0 }, { 496, 0, -272, 0 }, { -496, 0, 272, 0 }, { 496, 0, 272, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, -4096, 0 }, 565, WORLD_COLLISION_TRIGGER_ACTION_WARP, 3, 35, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -784, -360, -5440, 0 }, { { 320, 0, -544, 0 }, { 320, 0, 544, 0 }, { -320, 0, -544, 0 }, { -320, 0, 544, 0 } }, { 0, 4111, 0, 0 }, { -4096, 0, 0, 0 }, 630, WORLD_COLLISION_TRIGGER_ACTION_WARP, 7, 52, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1952, -360, -3184, 0 }, { { 768, 0, 272, 0 }, { -768, 0, 272, 0 }, { 768, 0, -272, 0 }, { -768, 0, -272, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, 4096, 0 }, 814, WORLD_COLLISION_TRIGGER_ACTION_WARP, 7, 19, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2296, -401, -2424, 0 }, { { -1592, 0, -1272, 0 }, { 1096, 0, -888, 0 }, { -600, 0, 1576, 0 }, { 1096, 0, 584, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, -4096, 0 }, 2035, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2720, -392, -2464, 0 }, { { -624, 0, -704, 0 }, { 624, 0, -704, 0 }, { -624, 0, 704, 0 }, { 624, 0, 704, 0 } }, { 0, 4103, 0, 0 }, { -4096, 0, 0, 0 }, 940, WORLD_COLLISION_TRIGGER_ACTION_CAP, 8, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1328, -360, 2160, 0 }, { { 464, 0, -608, 0 }, { 464, 0, 608, 0 }, { -464, 0, -608, 0 }, { -464, 0, 608, 0 } }, { 0, 4119, 0, 0 }, { 0, 0, -4096, 0 }, 762, WORLD_COLLISION_TRIGGER_ACTION_CAP, 10, 4, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 40, -384, -1272, 0 }, { { -504, 0, 8, 0 }, { 552, 0, -1272, 0 }, { -408, 0, 1224, 0 }, { 1640, 0, 584, 0 } }, { 0, 4111, 0, 0 }, { 0, 0, -4096, 0 }, 1736, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 2, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3112, -384, 64, 0 }, { { -2744, 0, -464, 0 }, { -2744, 0, -944, 0 }, { 2728, 0, 944, 0 }, { 2760, 0, 464, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, -4096, 0 }, 2896, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 3, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -5200, -384, 2784, 0 }, { { -416, 0, -976, 0 }, { 992, 0, -976, 0 }, { -416, 0, 368, 0 }, { 992, 0, 368, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, -4096, 0 }, 1390, WORLD_COLLISION_TRIGGER_ACTION_CAP, 21, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -992, -384, -2208, 0 }, { { -496, 0, -720, 0 }, { 496, 0, -720, 0 }, { -496, 0, 720, 0 }, { 496, 0, 720, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, -4096, 0 }, 872, WORLD_COLLISION_TRIGGER_ACTION_CAP, 20, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 480, -416, -2432, 0 }, { { -496, 0, -720, 0 }, { 496, 0, -720, 0 }, { -496, 0, 720, 0 }, { 496, 0, 720, 0 } }, { 0, 4099, 0, 0 }, { 4096, 0, 0, 0 }, 872, WORLD_COLLISION_TRIGGER_ACTION_CAP, 22, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2464, -384, -928, 0 }, { { -496, 0, -720, 0 }, { 496, 0, -720, 0 }, { -496, 0, 720, 0 }, { 496, 0, 720, 0 } }, { 0, 4099, 0, 0 }, { -4091, 0, 201, 0 }, 872, WORLD_COLLISION_TRIGGER_ACTION_CAP, 23, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2496, -384, 544, 0 }, { { -496, 0, -720, 0 }, { 496, 0, -720, 0 }, { -496, 0, 720, 0 }, { 496, 0, 720, 0 } }, { 0, 4099, 0, 0 }, { -4052, 0, 601, 0 }, 872, WORLD_COLLISION_TRIGGER_ACTION_CAP, 24, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2496, -384, 1761, 0 }, { { -496, 0, -304, 0 }, { 496, 0, -304, 0 }, { -496, 0, 304, 0 }, { 496, 0, 304, 0 } }, { 0, 4105, 0, 0 }, { 201, 0, -4092, 0 }, 579, WORLD_COLLISION_TRIGGER_ACTION_CAP, 25, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2816, -364, 1872, 0 }, { { -240, 0, -1152, 0 }, { 752, 0, -1152, 0 }, { -752, 0, 1152, 0 }, { 240, 0, 1152, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -4096, 0 }, 1372, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 3, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -3520, -377, -1688, 0 }, { { -1047, 0, -123, 0 }, { 95, 0, -1086, 0 }, { -178, 0, 1212, 0 }, { 1130, 0, -3, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -4096, 0 }, 1221, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 0, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_acropolis_cafeteria_801896A4[20] = {
    { NULL, NULL, NULL, { -3984, -368, 2704, 0 }, { { -496, 0, -272, 0 }, { 496, 0, -272, 0 }, { -496, 0, 272, 0 }, { 496, 0, 272, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, -4096, 0 }, 565, WORLD_COLLISION_TRIGGER_ACTION_WARP, 3, 35, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -784, -360, -5440, 0 }, { { 320, 0, -544, 0 }, { 320, 0, 544, 0 }, { -320, 0, -544, 0 }, { -320, 0, 544, 0 } }, { 0, 4111, 0, 0 }, { -4096, 0, 0, 0 }, 630, WORLD_COLLISION_TRIGGER_ACTION_WARP, 7, 52, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1952, -360, -3184, 0 }, { { 768, 0, 272, 0 }, { -768, 0, 272, 0 }, { 768, 0, -272, 0 }, { -768, 0, -272, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, 4096, 0 }, 814, WORLD_COLLISION_TRIGGER_ACTION_WARP, 7, 19, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2296, -401, -2424, 0 }, { { -1592, 0, -1272, 0 }, { 1096, 0, -888, 0 }, { -600, 0, 1576, 0 }, { 1096, 0, 584, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, -4096, 0 }, 2035, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2720, -392, -2464, 0 }, { { -624, 0, -704, 0 }, { 624, 0, -704, 0 }, { -624, 0, 704, 0 }, { 624, 0, 704, 0 } }, { 0, 4103, 0, 0 }, { -4096, 0, 0, 0 }, 940, WORLD_COLLISION_TRIGGER_ACTION_CAP, 8, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1328, -360, 2160, 0 }, { { 464, 0, -608, 0 }, { 464, 0, 608, 0 }, { -464, 0, -608, 0 }, { -464, 0, 608, 0 } }, { 0, 4119, 0, 0 }, { 0, 0, -4096, 0 }, 762, WORLD_COLLISION_TRIGGER_ACTION_CAP, 10, 4, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 40, -384, -1272, 0 }, { { -504, 0, 8, 0 }, { 552, 0, -1272, 0 }, { -408, 0, 1224, 0 }, { 1640, 0, 584, 0 } }, { 0, 4111, 0, 0 }, { 0, 0, -4096, 0 }, 1736, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 2, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3112, -384, 64, 0 }, { { -2744, 0, -464, 0 }, { -2744, 0, -944, 0 }, { 2728, 0, 944, 0 }, { 2760, 0, 464, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, -4096, 0 }, 2896, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 3, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -5200, -384, 2784, 0 }, { { -416, 0, -976, 0 }, { 992, 0, -976, 0 }, { -416, 0, 368, 0 }, { 992, 0, 368, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, -4096, 0 }, 1390, WORLD_COLLISION_TRIGGER_ACTION_CAP, 21, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -992, -384, -2208, 0 }, { { -496, 0, -720, 0 }, { 496, 0, -720, 0 }, { -496, 0, 720, 0 }, { 496, 0, 720, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, -4096, 0 }, 872, WORLD_COLLISION_TRIGGER_ACTION_CAP, 20, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 480, -416, -2432, 0 }, { { -496, 0, -720, 0 }, { 496, 0, -720, 0 }, { -496, 0, 720, 0 }, { 496, 0, 720, 0 } }, { 0, 4099, 0, 0 }, { 4096, 0, 0, 0 }, 872, WORLD_COLLISION_TRIGGER_ACTION_CAP, 22, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2464, -384, -928, 0 }, { { -496, 0, -720, 0 }, { 496, 0, -720, 0 }, { -496, 0, 720, 0 }, { 496, 0, 720, 0 } }, { 0, 4099, 0, 0 }, { -4091, 0, 201, 0 }, 872, WORLD_COLLISION_TRIGGER_ACTION_CAP, 23, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2496, -384, 544, 0 }, { { -496, 0, -720, 0 }, { 496, 0, -720, 0 }, { -496, 0, 720, 0 }, { 496, 0, 720, 0 } }, { 0, 4099, 0, 0 }, { -4052, 0, 601, 0 }, 872, WORLD_COLLISION_TRIGGER_ACTION_CAP, 24, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2496, -384, 1761, 0 }, { { -496, 0, -304, 0 }, { 496, 0, -304, 0 }, { -496, 0, 304, 0 }, { 496, 0, 304, 0 } }, { 0, 4105, 0, 0 }, { 201, 0, -4092, 0 }, 579, WORLD_COLLISION_TRIGGER_ACTION_CAP, 25, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2816, -364, 1872, 0 }, { { -240, 0, -1152, 0 }, { 752, 0, -1152, 0 }, { -752, 0, 1152, 0 }, { 240, 0, 1152, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -4096, 0 }, 1372, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 3, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -3296, -377, -2296, 0 }, { { -1559, 0, 133, 0 }, { 95, 0, -414, 0 }, { -178, 0, 1756, 0 }, { 1130, 0, 317, 0 } }, { 0, 4104, 0, 0 }, { 0, 0, -4096, 0 }, 1764, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 0, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3480, -384, -1368, 0 }, { { -1272, 0, -792, 0 }, { 1320, 0, -632, 0 }, { -760, 0, 712, 0 }, { 712, 0, 712, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, 4096, 0 }, 1498, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 0, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3496, -384, -2824, 0 }, { { 1272, 0, 792, 0 }, { -1320, 0, 632, 0 }, { 760, 0, -712, 0 }, { -712, 0, -712, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, -4096, 0 }, 1498, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 0, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4034, -384, -2092, 0 }, { { -162, 0, -1428, 0 }, { 1220, 0, 15, 0 }, { -850, 0, -58, 0 }, { -205, 0, 1472, 0 } }, { 0, 4094, 0, 0 }, { -4096, 0, 0, 0 }, 1481, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 0, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3488, -384, -1728, 0 }, { { -240, 0, -720, 0 }, { 240, 0, -720, 0 }, { -240, 0, 720, 0 }, { 240, 0, 720, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, -4096, 0 }, 757, WORLD_COLLISION_TRIGGER_ACTION_ROOM, 0, 0, WORLD_COLLISION_TRIGGER_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionOccluder D_acropolis_cafeteria_80189C94[2] = {
    { NULL, NULL, { -384, -304, -1136, 0 }, { { 0, 752, -4816, 0 }, { 0, 752, 4816, 0 }, { 0, -752, -4816, 0 }, { 0, -752, 4816, 0 } }, { -4106, 0, 0, 0 }, 4857, 1, 0 },
    { NULL, NULL, { -352, -1456, -3920, 0 }, { { 0, 2208, -2400, 0 }, { 0, 2208, 2400, 0 }, { 0, -2208, -2400, 0 }, { 0, -2208, 2400, 0 } }, { -4099, 0, 0, 0 }, 3258, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

AreaResource D_acropolis_cafeteria_80189D0C[4] = {
    { 10, 106, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &D_actor_110600_80148670 },
    { 29, 29, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_202900_80156E24 },
    { 102, 106, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, D_actor_310600_801796A4 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_acropolis_cafeteria_80189D3C[4] = {
    { 10, 106, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &D_actor_110600_80148670 },
    { 19, 106, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &D_actor_210600_8015A4EC },
    { 102, 106, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, D_actor_310600_801796A4 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_acropolis_cafeteria_80189D6C[2] = {
    { 18, 18, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &D_actor_401800_80155AC4 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_acropolis_cafeteria_80189D84[2] = {
    { 18, 18, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &D_actor_401800_80155AC4 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_acropolis_cafeteria_80189D9C[2] = {
    { 12, 12, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_101200_80138E98 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_acropolis_cafeteria_80189DB4[2] = {
    { 57, 57, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor05700GolemPawnRookTasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_acropolis_cafeteria_80189DCC[11] = {
    { NULL, NULL },
    { D_map_akropolis_8017AE9C, D_acropolis_cafeteria_80189D0C },
    { D_map_akropolis_8017AEDC, D_acropolis_cafeteria_80189D3C },
    { D_map_akropolis_8017AF1C, D_acropolis_cafeteria_80189D6C },
    { D_map_akropolis_8017AF3C, D_acropolis_cafeteria_80189D84 },
    { D_map_akropolis_8017AF6C, D_acropolis_cafeteria_80189D9C },
    { NULL, NULL },
    { D_map_akropolis_8017AFCC, D_acropolis_cafeteria_80189DB4 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
};

WorldCoordPointLight gAcropolisCafeteriaPointLights[15] = {
    { .head = { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1495, -3075, -3183 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3400, 3051, 2214 }, { 0, 0 } }, .inner = 1021, .outer = 1963 },
    { .head = { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2998, -2896, -1504 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2578, 2319, 1751 }, { 0, 0 } }, .inner = 3000, .outer = 12102 },
    { .head = { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1010, -2896, -1501 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3380, 3031, 2354 }, { 0, 0 } }, .inner = 1221, .outer = 3424 },
    { .head = { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3140, -1728, -635 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3909, 1478, 540 }, { 0, 0 } }, .inner = 919, .outer = 2199 },
    { .head = { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2429, -1749, 2381 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4004, 4004, 4004 }, { 0, 0 } }, .inner = 299, .outer = 740 },
    { .head = { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1501, -2896, 222 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3540, 2951, 2234 }, { 0, 0 } }, .inner = 1417, .outer = 3524 },
    { .head = { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3223, -1918, 734 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4024, 4024, 4024 }, { 0, 0 } }, .inner = 1280, .outer = 2078 },
    { .head = { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2023, -2503, -3012 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3604, 4008, 4008 }, { 0, 0 } }, .inner = 962, .outer = 2040 },
    { .head = { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -3012, -2896, 1693 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2759, 2230, 1690 }, { 0, 0 } }, .inner = 3584, .outer = 6227 },
    { .head = { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1656, -1336, 2316 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3746, 3353, 3359 }, { 0, 0 } }, .inner = 559, .outer = 1520 },
    { .head = { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -3597, 84, -1634 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2285, 1654, 1243 }, { 0, 0 } }, .inner = 677, .outer = 3269 },
    { .head = { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -4490, -2896, 220 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2641, 2091, 1814 }, { 0, 0 } }, .inner = 360, .outer = 3524 },
    { .head = { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -4997, -2896, -1499 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3365, 2996, 2286 }, { 0, 0 } }, .inner = 1219, .outer = 4243 },
    { .head = { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -4483, -2896, -3170 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3420, 3031, 2232 }, { 0, 0 } }, .inner = 696, .outer = 2693 },
    { .head = { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2998, -2336, -4692 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3400, 2990, 2371 }, { 0, 0 } }, .inner = 698, .outer = 5980 },
};

AcropolisCafeteriaSpotLightStorage gAcropolisCafeteriaSpotLightStorage = {
    .liveLights = {
        {
            .head = {
                .transform = {
                    .lighting = {
                        .composeStamp = GRAPHICS_COORD_DIRTY,
                        .local        = {
                                   .m = { { -1629, -3, 3766 }, { 3759, -23, 1629 }, { 17, 4097, 9 } },
                                   .t = { -388, -2188, -10 },
                        },
                        .composed = {
                            .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } },
                            .t = { 0, 0, 0 },
                        },
                        .viewId      = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                        .unknown_46  = { 0, 0, 0, 0 },
                        .attenuation = 0,
                        .parent      = NULL,
                    },
                },
                .color      = { .r = 4000, .g = 3590, .b = 721 },
                .unknown_56 = { 0, 0 },
            },
            .axis  = { 3759, 1626, 9, 0 },
            .inner = 1040,
            .outer = 3841,
            .angle = 910,
        },
    },
    .inactiveSlots = {
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x98, 0xAE, 0x1D, 0x80, 0x01, 0x00, 0x00, 0x00, 0x38, 0xB4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x48, 0x00, 0x38, 0x00, 0xC5, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8F, 0x00, 0xC0, 0x3F, 0x48, 0x00, 0x00, 0x00, 0x48, 0x00, 0x40, 0x00, 0xA5, 0x02, 0x00, 0x00, 0x80, 0x80, 0x80, 0x00, 0x8F, 0x00, 0xC0, 0x3F, 0x40, 0x00, 0x10, 0x00 },
        { 0x00, 0x00, 0x00, 0x00, 0x73, 0x02, 0x18, 0x40, 0x80, 0x80, 0x80, 0x00, 0x8F, 0x00, 0xC0, 0x3F, 0x20, 0x00, 0x08, 0x00, 0x68, 0x00, 0x00, 0x00, 0x93, 0x02, 0x28, 0xB0, 0x80, 0x80, 0x80, 0x00, 0x8F, 0x00, 0xC0, 0x3F, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x00, 0x8F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x00, 0x38, 0x00, 0x63, 0x03, 0x00, 0x00, 0x80, 0x80, 0x80, 0x00, 0x8F, 0x00, 0x00, 0x00, 0x08, 0x00, 0x18, 0x00, 0x48, 0x00, 0x38, 0x00, 0x63, 0x03, 0x70, 0xD8 },
        { 0x00, 0x00, 0x00, 0x00, 0x8F, 0x00, 0xC0, 0x3F, 0x18, 0x00, 0x08, 0x00, 0x78, 0x00, 0x30, 0x00, 0x02, 0x03, 0x30, 0x60, 0x80, 0x80, 0x00, 0x00, 0x8F, 0x00, 0xC0, 0x3F, 0x18, 0x00, 0x10, 0x00, 0x70, 0x00, 0x38, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x18, 0x00, 0x68, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x00, 0x8F, 0x00, 0x00, 0x00, 0x18, 0x00, 0x18, 0x00, 0x80, 0x00, 0x00, 0x00, 0x60, 0x03, 0x48, 0xA0, 0x80, 0x80, 0x80, 0x00, 0x8F, 0x00, 0xC0, 0x3F },
        { 0x00, 0x00, 0x00, 0x00, 0x38, 0x00, 0x48, 0x00, 0xBC, 0x02, 0x48, 0x70, 0x80, 0x80, 0x80, 0x00, 0x8F, 0x00, 0xC0, 0x3F, 0x10, 0x00, 0x00, 0x00, 0x38, 0x00, 0x50, 0x00, 0xB9, 0x02, 0x40, 0xC0, 0x80, 0x80, 0x80, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xA0, 0x02, 0x38, 0x68, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x08, 0x00, 0x48, 0x00, 0x00, 0x00, 0x89, 0x02, 0x38, 0xA8, 0x80, 0x80, 0x00, 0x00, 0x8F, 0x00, 0xC0, 0x3F, 0x18, 0x00, 0x08, 0x00, 0x30, 0x00, 0x58, 0x00 },
        { 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x00, 0x8F, 0x00, 0xC0, 0x3F, 0x18, 0x00, 0x08, 0x00, 0x30, 0x00, 0x60, 0x00, 0x98, 0x02, 0x00, 0x00, 0x80, 0x80, 0x80, 0x00, 0x8F, 0x00, 0xC0, 0x3F, 0x28, 0x00, 0x10, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8F, 0x00, 0xC0, 0x3F, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x68, 0x02, 0x18, 0x00, 0x80, 0x80, 0x00, 0x00, 0x8F, 0x00, 0xC0, 0x3F, 0x28, 0x00, 0x00, 0x00, 0xF0, 0xFF, 0xF0, 0xFF, 0xD5, 0x03, 0x28, 0x10, 0x80, 0x80, 0x80, 0x00 },
        { 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x30, 0x00, 0xF0, 0xFF, 0x00, 0x00, 0x19, 0x04, 0x50, 0x00, 0x80, 0x80, 0x80, 0x00, 0x8F, 0x00, 0x00, 0x00, 0x18, 0x00, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0xEF, 0x03, 0x48, 0x78, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x50, 0x00, 0xE0, 0xFF, 0xFB, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8F, 0x00, 0xC0, 0x3F, 0x10, 0x00, 0x00, 0x00, 0x58, 0x00, 0xE0, 0xFF, 0xB0, 0x04, 0x00, 0x00, 0x80, 0x80, 0x80, 0x00, 0x8F, 0x00, 0xC0, 0x3F, 0x10, 0x00, 0x40, 0x00 },
        { 0x00, 0x00, 0x00, 0x00, 0x65, 0x04, 0x70, 0x98, 0x80, 0x80, 0x80, 0x00, 0x8F, 0x00, 0xC0, 0x3F, 0x10, 0x00, 0x40, 0x00, 0x78, 0x00, 0x00, 0x00, 0x1A, 0x04, 0x68, 0x00, 0x80, 0x80, 0x80, 0x00, 0x8F, 0x00, 0xC0, 0x3F, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x00, 0x8F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x88, 0x00, 0xE8, 0xFF, 0x9D, 0x03, 0x00, 0x00, 0x80, 0x80, 0x80, 0x00, 0x8F, 0x00, 0x00, 0x00, 0x08, 0x00, 0x50, 0x00, 0x98, 0x00, 0xF8, 0xFF, 0x84, 0x03, 0x78, 0x00 },
        { 0x00, 0x00, 0x00, 0x00, 0x8F, 0x00, 0xC0, 0x3F, 0x10, 0x00, 0x08, 0x00, 0x78, 0x00, 0xE8, 0xFF, 0xE8, 0x03, 0x40, 0xB8, 0x80, 0x80, 0x00, 0x00, 0x8F, 0x00, 0xC0, 0x3F, 0x20, 0x00, 0x08, 0x00, 0x68, 0x00, 0xE0, 0xFF, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x08, 0x00, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x00, 0x8F, 0x00, 0x00, 0x00, 0x08, 0x00, 0x20, 0x00, 0x70, 0x00, 0x00, 0x00, 0xE8, 0x03, 0x78, 0xD8, 0x80, 0x80, 0x80, 0x00, 0x8F, 0x00, 0xC0, 0x3F },
        { 0x00, 0x00, 0x00, 0x00, 0x78, 0x00, 0x88, 0xFF, 0xCF, 0x03, 0x60, 0xB0, 0x80, 0x80, 0x80, 0x00, 0x8F, 0x00, 0xC0, 0x3F, 0x18, 0x00, 0x00, 0x00, 0x88, 0x00, 0x88, 0xFF, 0xB6, 0x03, 0x58, 0x40, 0x80, 0x80, 0x80, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x1B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x28, 0x00, 0x03, 0x00, 0x00, 0x00, 0x02, 0x00 },
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8F, 0x00, 0xC0, 0x3F, 0x18, 0x00, 0x20, 0x00, 0xE0, 0xFF, 0xD8, 0xFF, 0xE6, 0x06, 0x00, 0x00, 0x80, 0x80, 0x80, 0x00, 0x8F, 0x00, 0xC0, 0x3F, 0x20, 0x00, 0x10, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8F, 0x00, 0xC0, 0x3F, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xB4, 0x05, 0x20, 0x00, 0x80, 0x80, 0x00, 0x00, 0x8F, 0x00, 0xC0, 0x3F, 0x10, 0x00, 0x00, 0x00, 0xF8, 0xFF, 0xF0, 0xFF, 0x54, 0x06, 0x30, 0x30, 0x80, 0x80, 0x80, 0x00 },
        { 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x18, 0x00, 0x08, 0x00, 0xF0, 0xFF, 0x41, 0x06, 0x78, 0xE8, 0x80, 0x80, 0x80, 0x00, 0x8F, 0x00, 0x00, 0x00, 0x10, 0x00, 0x20, 0x00, 0xF8, 0xFF, 0xF0, 0xFF, 0x55, 0x05, 0x50, 0xB0, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0xF8, 0xFF, 0x27, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8F, 0x00, 0xC0, 0x3F, 0x18, 0x00, 0x00, 0x00, 0x10, 0x00, 0x08, 0x00, 0x5A, 0x05, 0x00, 0x00, 0x80, 0x80, 0x80, 0x00, 0x8F, 0x00, 0xC0, 0x3F, 0x28, 0x00, 0x20, 0x00 },
        { 0x00, 0x00, 0x00, 0x00, 0x6B, 0x03, 0x30, 0xD0, 0x80, 0x80, 0x80, 0x00, 0x8F, 0x00, 0xC0, 0x3F, 0x08, 0x00, 0x08, 0x00, 0xD0, 0xFF, 0x00, 0x00, 0x0E, 0x04, 0x68, 0xB0, 0x80, 0x80, 0x80, 0x00, 0x8E, 0x00, 0xC0, 0x3F, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x00, 0x8F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xD0, 0xFF, 0x20, 0x00, 0x0B, 0x04, 0x00, 0x00, 0x80, 0x80, 0x80, 0x00, 0x8F, 0x00, 0x00, 0x00, 0x30, 0x00, 0x10, 0x00, 0xA8, 0xFF, 0x28, 0x00, 0x78, 0x03, 0x18, 0x90 },
        { 0x00, 0x00, 0x00, 0x00, 0x8F, 0x00, 0xC0, 0x3F, 0x10, 0x00, 0x10, 0x00, 0xD8, 0xFF, 0x28, 0x00, 0x1C, 0x04, 0x38, 0x70, 0x80, 0x80, 0x00, 0x00, 0x8F, 0x00, 0xC0, 0x3F, 0x28, 0x00, 0x10, 0x00, 0xB0, 0xFF, 0x38, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x08, 0x00, 0xE0, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x00, 0x8F, 0x00, 0x00, 0x00, 0x20, 0x00, 0x10, 0x00, 0xD8, 0xFF, 0x00, 0x00, 0x9A, 0x03, 0x18, 0x70, 0x80, 0x80, 0x80, 0x00, 0x8F, 0x00, 0xC0, 0x3F },
        { 0x00, 0x00, 0x00, 0x00, 0xE0, 0xFF, 0x30, 0x00, 0xE3, 0x03, 0x30, 0x80, 0x80, 0x80, 0x80, 0x00, 0x8F, 0x00, 0xC0, 0x3F, 0x20, 0x00, 0x00, 0x00, 0x40, 0x00, 0x20, 0x00, 0x7E, 0x03, 0x30, 0xB8, 0x80, 0x80, 0x80, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x24, 0x03, 0x08, 0x18, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x28, 0x00, 0x18, 0x00, 0x78, 0x00, 0x00, 0x00, 0xB0, 0x02, 0x20, 0x40, 0x80, 0x80, 0x00, 0x00, 0x8F, 0x00, 0xC0, 0x3F, 0x28, 0x00, 0x10, 0x00, 0x70, 0x00, 0x50, 0x00 },
    },
};

/// Runs the player debug hooks while the display's debug mode is nonzero.
///
/// Requires a live player task; the room task argument is unused. The debug
/// label borrows static storage for the lifetime of the cafeteria overlay.
static void _acropolisCafeteriaTickPlayerDebug(Task* task)
{
    if (gDisplayState.debugMode != 0) {
        func_80724608(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), -0x8C, -0x32, (void*)CafeteriaPlayerLabel);
        func_807245E4(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER));
    }
}

/// Selects the patio room for an executed departure from the cafeteria.
///
/// Borrows a readable request and an initialized writable reply, which may
/// alias. Queries and other destination areas leave the reply unchanged.
/// Before the cafeteria scene, opening progress selects patio room 1 or 2;
/// afterwards it selects room 3. Returns 1 to allow the transition.
static inline s32 _acropolisCafeteriaResolvePatioRoom(const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        ACROPOLIS_CAFETERIA_OPENING_VARIANT_THRESHOLD = 2,
        ACROPOLIS_CAFETERIA_PATIO_ROOM_EARLY          = 1,
        ACROPOLIS_CAFETERIA_PATIO_ROOM_OPENING_PASSED = 2,
    };
    s32 destinationArea = request->areaId;

    if (destinationArea == GAME_AREA_ACROPOLIS_PATIO && request->queryOnly == ROOM_EVENT_EXECUTE) {
        if (gameFlagGetNibble(GAME_FLAG_ACROPOLIS_PROGRESS) < ACROPOLIS_CAFETERIA_PROGRESS_AFTER_SCENE) {
            if (gameFlagGetNibble(GAME_FLAG_ACROPOLIS_OPENING_PROGRESS) < ACROPOLIS_CAFETERIA_OPENING_VARIANT_THRESHOLD) {
                reply->room = ACROPOLIS_CAFETERIA_PATIO_ROOM_EARLY;
            } else {
                reply->room = ACROPOLIS_CAFETERIA_PATIO_ROOM_OPENING_PASSED;
            }
        } else {
            reply->room = destinationArea;
        }
    }
    return 1;
}

/// Copies the room message, selects its response, and starts capture slots 5
/// or 6 when the room's progress permits. queryOnly suppresses side effects.
s32 func_acropolis_cafeteria_8017D700(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    s32 msgId;

    *out = *in;
    if (in->areaId == GAME_AREA_ACROPOLIS_HALLWAY && in->warp == 4) {
        if (gameFlagGetNibble(0) >= 3) {
            return 1;
        }
        if (in->queryOnly == ROOM_EVENT_EXECUTE) {
            gameFlagSetNibbleIfPresent(in->flagId, 2);
            capStartSequenceSlot(5, 1, 0);
        }
        return 0;
    }
    msgId = in->areaId;
    if (msgId == 3) {
        if (gameFlagGetNibble(0) < 2) {
            if (D_acropolis_cafeteria_80184164 == 0) {
                return _acropolisCafeteriaResolvePatioRoom(in, out);
            }
            if (D_acropolis_cafeteria_80184164 == 2) {
                if (in->queryOnly == ROOM_EVENT_EXECUTE) {
                    capStartSequenceSlot(6, 1, 0);
                }
            }
            return 0;
        }
        if (gameFlagGetNibble(GAME_FLAG_00E) == msgId && in->queryOnly == ROOM_EVENT_EXECUTE) {
            gameFlagSetNibble(GAME_FLAG_00E, 2);
        }
        return _acropolisCafeteriaResolvePatioRoom(in, out);
    }
    return 1;
}

/// Scripted-event task for this room, one step per `task->state`. Most states
/// advance by one; state 8 jumps to 14, states 9-13 are never reached that way
/// and idle. States 2-8 and 14 raise `blackout`, which covers the whole frame
/// with a black `TILE` linked into ordering-table slot 10. State 27 ends the
/// sequence by killing the task once the slot-3 object accepts message 0x3ED.
void func_acropolis_cafeteria_8017D8F8(Task* task)
{
    u8    param1[4];
    u8    param2[4];
    TILE* tile;
    u8    blackout;

    blackout = 0;
    switch (task->state) {
        case 0:
            gGameSession->flowFlags = (GAME_SESSION_FLOW_SKIP_ENDING_MUSIC | GAME_SESSION_FLOW_SKIP_AREA_MUSIC);
            evsStartScriptWithSkip(D_acropolis_cafeteria_80182E74, EVENT_SCRIPT_HUD_KEEP, D_acropolis_cafeteria_801831BC);
            task->state += 1;
            break;
        case 1:
            if (gGameSession->eventState != 1) {
                task->state += 1;
            }
            break;
        case 2:
            blackout                        = 1;
            gGameSession->location.loc.room = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = 2;
            gGameSession->roomObjsDirty                                                                  = 1;
            task->state                                                                                 += 1;
            break;
        case 3:
            blackout = 1;
            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_EXIT_PLACED_ACTORS, 0, 0);
            task->state += 1;
            break;
        case 4:
            blackout     = 1;
            task->state += 1;
            break;
        case 5:
            blackout = 1;
            loadingRequestViewGraphicsRestore();
            task->state += 1;
            break;
        case 6:
            blackout  = 1;
            param1[2] = 0x15;
            param1[3] = 0;
            param1[0] = 0;
            param2[0] = 6;
            param2[1] = 0;
            param2[2] = 4;
            param2[3] = 6;
            cdCmdEnqueue(CD_COMMAND_LOAD_FILE, param1, param2);
            task->state += 1;
            break;
        case 7:
            blackout = 1;
            if (cdCmdIsIdle()) {
                areaSetPlacementVariant(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc, 2, AREA_VARIANT_RESET_ALWAYS);
                areaSyncLocationVariant(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc);
                areaSpawnPlacements(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc);
                D_801156A4  &= ~0x40;
                task->state += 1;
            }
            break;
        case 8:
            blackout = 1;
            if (gDisplayState.debugMode != -1) {
                taskSpawnFromTable(D_acropolis_cafeteria_80184178, 0, 0, 0);
            }
            task->state = 14;
            break;
        case 14:
            blackout = 1;
            if (gGameSession->eventState == 0) {
                task->state += 1;
            }
            break;
        case 15:
            evsStartScript(D_acropolis_cafeteria_80183F3C, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            task->state += 1;
            break;
        case 16:
        case 20:
            if (gGameSession->eventState == 0) {
                task->state += 1;
            }
            break;
        case 18:
            if (taskMessageDispatch(sceneFindPlacedActor(0), ACTOR_MESSAGE_IS_PRESENT, 0, 0) == 0 && gPlayerStatus.hp > 0 && Gp_StateC08.mode != ATTACHMENT_MODE_WHEEL &&
                gDisplayState.pendingMode == DISPLAY_MODE_NONE) {
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                task->state += 1;
            }
            break;
        case 19:
            displaySetShakeY(0);
            evsStartScriptWithSkip(D_acropolis_cafeteria_8018330C, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_acropolis_cafeteria_801834D4);
            task->state += 1;
            break;
        case 21:
            sceneReleaseBattleRefWithRewards(sceneFindPlacedActor(0), 0xA);
            gGameSession->flowFlags                       |= GAME_SESSION_FLOW_REEQUIP_WEAPON;
            gSceneCombatState.signals.bytes.endDelayFrames = 3;
            D_acropolis_cafeteria_80184164                 = 2;
            task->state                                   += 1;
            break;
        case 17:
        case 22:
        case 23:
        case 24:
        case 25:
        case 26:
            task->state += 1;
            break;
        case 27:
            if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
                gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, 3);
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                taskKill(task);
            }
            break;
    }
    if (blackout != 0) {
        tile           = gGpuPrimCursor;
        gGpuPrimCursor = tile + 1;
        SetTile(tile);
        tile->x0 = -0xA0;
        tile->y0 = -0x80;
        tile->w  = 0x140;
        tile->h  = 0x100;
        tile->r0 = 0;
        tile->g0 = 0;
        tile->b0 = 0;
        addPrim(&gGpuCurrentOt[10], tile);
    }
}

void func_acropolis_cafeteria_8017DD1C(Task* task)
{
    char pad[8];

    switch (task->state) {
        case 0:
            if (areaGetCurrentObjectState(3) == 1) {
                taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), PLAYER_ACTOR_MESSAGE_ENTER_SCRIPTED_PRESENTATION, 0, 0);
                task->state = task->state + 1;
            } else {
                taskKill(task);
            }
            break;

        case 1:
            if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
                capRunCommandWithTransition(3);
                task->state = task->state + 1;
            }
            break;

        case 2:
            if (capIsBusy() == 0) {
                if (areaGetCurrentObjectState(3) == 1) {
                    taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), PLAYER_ACTOR_MESSAGE_ENTER_SCRIPTED_PRESENTATION, 1, 0);
                    task->state = task->state + 1;
                } else {
                    task->state = 6;
                }
            }
            break;

        case 3:
            if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                taskKill(task);
            }
            break;

        case 6:
            evsStartScriptWithSkip(D_acropolis_cafeteria_8018363C, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_acropolis_cafeteria_80183DBC);
            task->state = task->state + 1;
            break;

        case 7:
            if (gGameSession->eventState != 1) {
                task->state = task->state + 1;
            }
            break;

        case 8:
            gameFlagSetNibble(0, 2);
            gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, 0);
            gameFlagSetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX, 4);
            gameFlagSetNibble(GAME_FLAG_00E, 1);
            areaApplySavedUpdates(D_acropolis_cafeteria_8018C9D4);
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = 4;
            gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, 4);
            func_800ABFF8();
            func_800AC000();
            sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage = GAME_STAGE_ACROPOLIS;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area  = GAME_AREA_ACROPOLIS_PATIO;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp  = 3;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room  = 3;
            gDisplayState.spriteVariant                                 = 1;
            taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
            taskKill(task);
            break;
    }
}

/// Integrates one recoil tick and releases its controller when the root settles.
///
/// Velocities use room units per callback tick; gravity uses units per tick
/// squared. Signed division damps X/Z toward zero, including negative values.
/// Borrows the live model root and bodyless controller; only one controller may
/// use the singleton velocities at a time. Positive Y points down. Clamps to
/// floor Y before damping, then kills only the controller once X/Z are zero
/// and the root is on the floor. Marks the root's composition cache stale.
static inline void _acropolisCafeteriaStepStrangerRecoil(Task* task, GfxCoord* root)
{
    enum {
        ACROPOLIS_CAFETERIA_STRANGER_FLOOR_Y        = -300,
        ACROPOLIS_CAFETERIA_STRANGER_GRAVITY        = 5,
        ACROPOLIS_CAFETERIA_STRANGER_MAX_FALL_SPEED = 20,
    };

    root->coord.t[0] += D_acropolis_cafeteria_8018D6A0;
    root->coord.t[1] += D_acropolis_cafeteria_8018D6A4;
    if (root->coord.t[1] > ACROPOLIS_CAFETERIA_STRANGER_FLOOR_Y) {
        root->coord.t[1] = ACROPOLIS_CAFETERIA_STRANGER_FLOOR_Y;
    }
    root->coord.t[2]               += D_acropolis_cafeteria_8018D6A8;
    root->composeStamp              = GRAPHICS_COORD_DIRTY;
    D_acropolis_cafeteria_8018D6A0  = D_acropolis_cafeteria_8018D6A0 / 2;
    D_acropolis_cafeteria_8018D6A4 += ACROPOLIS_CAFETERIA_STRANGER_GRAVITY;
    if (D_acropolis_cafeteria_8018D6A4 > ACROPOLIS_CAFETERIA_STRANGER_MAX_FALL_SPEED) {
        D_acropolis_cafeteria_8018D6A4 = ACROPOLIS_CAFETERIA_STRANGER_MAX_FALL_SPEED;
    }
    D_acropolis_cafeteria_8018D6A8 = D_acropolis_cafeteria_8018D6A8 / 2;
    if (D_acropolis_cafeteria_8018D6A0 == 0 && D_acropolis_cafeteria_8018D6A8 == 0 &&
        root->coord.t[1] == ACROPOLIS_CAFETERIA_STRANGER_FLOOR_Y) {
        taskKill(task);
    }
}

/// Settles the cafeteria Stranger after the final scripted shot.
///
/// Requires placed actor 0's live TMD root and the cafeteria overlay to remain
/// loaded. State 0 places the actor, selects its recoil animation and initializes
/// singleton X/Y/Z velocities; state 1 steps them until X/Z stop and Y reaches
/// the floor at -300 room units, then kills only this bodyless controller.
/// Only one such controller may use the shared velocities at a time.
static void _acropolisCafeteriaSettleStrangerTask(Task* task)
{
    enum {
        ACROPOLIS_CAFETERIA_SETTLE_INITIALIZE       = 0,
        ACROPOLIS_CAFETERIA_SETTLE_MOVE             = 1,
        ACROPOLIS_CAFETERIA_STRANGER_INITIAL_Y_STEP = -20,
        ACROPOLIS_CAFETERIA_STRANGER_INITIAL_Z_STEP = -20,
    };
    GfxCoord* root;

    root = sceneFindPlacedActor(ACROPOLIS_CAFETERIA_PLACED_STRANGER)->extra.tmd->coords;
    switch (task->state) {
        case ACROPOLIS_CAFETERIA_SETTLE_INITIALIZE:
            // Start the recoil clip before taking over the root's translation.
            TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(ACROPOLIS_CAFETERIA_PLACED_STRANGER), ACTOR_MESSAGE_PLACE, &D_acropolis_cafeteria_80182D28, 0);
            TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(ACROPOLIS_CAFETERIA_PLACED_STRANGER), ACTOR_COMMAND_MESSAGE_APPLY, &D_acropolis_cafeteria_80182DB8, 0);
            D_acropolis_cafeteria_8018D6A0 = 0;
            D_acropolis_cafeteria_8018D6A4 = ACROPOLIS_CAFETERIA_STRANGER_INITIAL_Y_STEP;
            D_acropolis_cafeteria_8018D6A8 = ACROPOLIS_CAFETERIA_STRANGER_INITIAL_Z_STEP;
            task->state                    = task->state + 1;
            break;

        case ACROPOLIS_CAFETERIA_SETTLE_MOVE:
            _acropolisCafeteriaStepStrangerRecoil(task, root);
            break;
    }
}

/// Refuses every key-item use requested at the cafeteria room task.
///
/// Handles `ROOM_MESSAGE_USE_KEY_ITEM`; all arguments are unused. Returns
/// `ROOM_KEY_ITEM_USE_REFUSED`, so the menu presents its refusal notice.
static s32 _acropolisCafeteriaRefuseKeyItemMsg(Task* task, s32 messageId, s32 itemId, s32 unusedArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

s32 func_acropolis_cafeteria_8017E0DC(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    if (arg2 == 7) {
        if (gameFlagGetNibble(0) >= 2 || D_acropolis_cafeteria_80184164 >= 2) {
            if (areaGetCurrentObjectState(4) == 1 || areaGetCurrentObjectState(4) == 0) {
                capStartSequenceSlot(7, 1, 0);
            }
        }
    }
    return 0;
}
/// Handler for slot-7 msg `0x13EF`: the directed action selected by `actionId`.
s32 func_acropolis_cafeteria_8017E154(Task* task, s32 msgId, const void* firstArg, s32 arg3)
{
    const DirectionActionRequest* request = firstArg;

    if (request->actionId == 0) {
        if (D_acropolis_cafeteria_80184164 >= 2 || gameFlagGetNibble(0) >= 2) {
            taskSpawnFromTable(D_acropolis_cafeteria_80182AD8, 1, 0, 0);
            return 0;
        }
    }
    if (request->actionId == 2) {
        capRunCommandWithTransition(9);
    } else if (request->actionId == 3) {
        if (D_acropolis_cafeteria_80184164 == 0 && gameFlagGetNibble(0) == 1) {
            D_acropolis_cafeteria_80184164 = 1;
            taskSpawnFromTable(D_acropolis_cafeteria_80182AD8, 0, 0, 0);
        }
    }
    return 0;
}
/// Maps cafeteria room sound cues 10 and 11 to the same entries in its area bank.
///
/// Handles `ROOM_MESSAGE_SOUND`; other cues do nothing. Task, messageId and
/// unusedArg are ignored. Requests use zero pan offset and attenuation, and
/// return 0 regardless of sound-queue admission.
static s32 _acropolisCafeteriaPlaySoundMsg(Task* task, s32 messageId, s32 soundCue, s32 unusedArg)
{
    enum {
        ACROPOLIS_CAFETERIA_SOUND_CUE_10 = 10,
        ACROPOLIS_CAFETERIA_SOUND_CUE_11 = 11,
    };

    switch (soundCue) {
        case ACROPOLIS_CAFETERIA_SOUND_CUE_10:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_CAFETERIA, ACROPOLIS_CAFETERIA_SOUND_CUE_10), 0, 0);
            break;
        case ACROPOLIS_CAFETERIA_SOUND_CUE_11:
            sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_ACROPOLIS, GAME_AREA_ACROPOLIS_CAFETERIA, ACROPOLIS_CAFETERIA_SOUND_CUE_11), 0, 0);
            break;
    }
    return 0;
}
/// Overrides the live player's room surface index for the scripted footsteps.
///
/// The event callback passes a complete signed word, stored without narrowing.
/// Requires live `GameActor` work and a surface index in 0..7 for the cafeteria's
/// surface table; the opening script supplies 2. Does not retain a pointer.
static void _acropolisCafeteriaSetPlayerSurface(s32 surfaceClass)
{
    GameActor* player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;

    player->surfaceClass = surfaceClass;
}

/// Requests parasite-energy effect cancellation before the opening room scene.
///
/// Requires live room-effect state. Cancellation is deferred until its next
/// update; the event callback ignores its argument register.
static void _acropolisCafeteriaCancelPeEffects(void)
{
    roomEffectRequestCancelPe();
}

/// Copies the two authored VRAM regions used by the post-battle scene.
///
/// Requires initialized GPU transfer state and intact source VRAM. Copies a
/// 64-word by 256-row image and then a 256-word row to their scene destinations;
/// X coordinates and widths count 16-bit VRAM words, Y coordinates count rows.
/// Queues both transfers without waiting for completion. The event callback
/// ignores its argument register.
static void _acropolisCafeteriaCopyPostBattleVram(void)
{
    enum {
        ACROPOLIS_CAFETERIA_POST_BATTLE_IMAGE_X = 384,
        ACROPOLIS_CAFETERIA_POST_BATTLE_IMAGE_Y = 256,
        ACROPOLIS_CAFETERIA_POST_BATTLE_ROW_X   = 0,
        ACROPOLIS_CAFETERIA_POST_BATTLE_ROW_Y   = 247,
    };

    MoveImage(&D_acropolis_cafeteria_80184168, ACROPOLIS_CAFETERIA_POST_BATTLE_IMAGE_X, ACROPOLIS_CAFETERIA_POST_BATTLE_IMAGE_Y);
    MoveImage(&D_acropolis_cafeteria_80184170, ACROPOLIS_CAFETERIA_POST_BATTLE_ROW_X, ACROPOLIS_CAFETERIA_POST_BATTLE_ROW_Y);
}

/// Requests effect cancellation and locks attachment activation for the post-battle scene.
///
/// Requires live room-effect state. Cancellation is deferred to its next
/// update; the attachment event lock is set immediately and left for scene
/// cleanup to clear. The event callback ignores its argument register.
static void _acropolisCafeteriaPreparePostBattleScene(void)
{
    roomEffectRequestCancelAll();
    Gp_StateC08.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
}

/// Registers the cafeteria room task and restores its progress-dependent actors.
///
/// Requires the live scene task and current room resources. At story progress 1,
/// hides the boss and shows the scripted form; at 2, shows the boss and Rupert,
/// suspends the scripted form, disables the CAP 20 trigger and places Rupert.
/// Other progress values leave actors and triggers unchanged. Installs the
/// borrowed message table, publishes the task in the room slot and advances
/// to state 1.
static void _acropolisCafeteriaInitRoom(Task* task)
{
    enum {
        ACROPOLIS_CAFETERIA_PROGRESS_BEFORE_SCENE = 1,
        ACROPOLIS_CAFETERIA_PLACED_SCRIPTED_FORM  = 1,
        ACROPOLIS_CAFETERIA_PLACED_RUPERT         = 2,
        ACROPOLIS_CAFETERIA_DRAW_HIDE             = 0,
        ACROPOLIS_CAFETERIA_DRAW_SHOW             = 1,
        ACROPOLIS_CAFETERIA_DRAW_SUSPEND          = 2,
        ACROPOLIS_CAFETERIA_CAP_20_TRIGGER_INDEX  = 9,
    };
    WorldCollisionTrigger* disabledTrigger;

    task->msgTable = D_acropolis_cafeteria_80182AA8;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (gameFlagGetNibble(GAME_FLAG_ACROPOLIS_PROGRESS) == ACROPOLIS_CAFETERIA_PROGRESS_BEFORE_SCENE) {
        sceneSetPlacedActorDrawMode(ACROPOLIS_CAFETERIA_PLACED_STRANGER, ACROPOLIS_CAFETERIA_DRAW_HIDE);
        sceneSetPlacedActorDrawMode(ACROPOLIS_CAFETERIA_PLACED_SCRIPTED_FORM, ACROPOLIS_CAFETERIA_DRAW_SHOW);
    } else if (gameFlagGetNibble(GAME_FLAG_ACROPOLIS_PROGRESS) == ACROPOLIS_CAFETERIA_PROGRESS_AFTER_SCENE) {
        sceneSetPlacedActorDrawMode(ACROPOLIS_CAFETERIA_PLACED_STRANGER, ACROPOLIS_CAFETERIA_DRAW_SHOW);
        sceneSetPlacedActorDrawMode(ACROPOLIS_CAFETERIA_PLACED_SCRIPTED_FORM, ACROPOLIS_CAFETERIA_DRAW_SUSPEND);
        sceneSetPlacedActorDrawMode(ACROPOLIS_CAFETERIA_PLACED_RUPERT, ACROPOLIS_CAFETERIA_DRAW_SHOW);
        disabledTrigger         = &D_acropolis_cafeteria_801891E4[ACROPOLIS_CAFETERIA_CAP_20_TRIGGER_INDEX];
        disabledTrigger->flags &= (0xFF ^ WORLD_COLLISION_TRIGGER_ENABLED);
        TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(ACROPOLIS_CAFETERIA_PLACED_RUPERT), ACTOR_MESSAGE_PLACE, &D_acropolis_cafeteria_80182DDC, 0);
    }
    task->state = task->state + 1;
}

void acropolisCafeteriaRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_acropolis_cafeteria_8017D5C4;
    stateHandlers.funcs[task->state](task);
}
