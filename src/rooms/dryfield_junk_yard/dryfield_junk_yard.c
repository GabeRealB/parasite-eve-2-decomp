#include "rooms/dryfield_junk_yard.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/task_tables.h"

#include "gameplay/actor_render.h"
#include "gameplay/actor_render_shadow_types.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_dryfield.h"
#include "../../shared/junk_yard.h"

/// Resident routine at the fixed address `0x80724608`, outside every image
/// the build links. The room hands it the slot-0xA game pointer, two
/// constants and the `"DOG"` name below; what it does with them is unproven.
void func_80724608(void* owner, s32 arg1, s32 arg2, void* name);

/// Main-executable halfword the room task's second state waits on before it
/// calls `func_80724608`.

/// The room's message table, published in `Task::msgTable` for
/// `taskMessageDispatch` to walk.
extern TaskMessageEntry     D_dryfield_junk_yard_8017DD20[];
extern TaskDesc             D_dryfield_junk_yard_8017DD48[];
extern AnimationPlayRequest D_dryfield_junk_yard_8017DD88;
extern AnimationPlayRequest D_dryfield_junk_yard_8017DDD8;
extern AnimationPlayRequest D_dryfield_junk_yard_8017DDEC;
extern ActorTransform       D_dryfield_junk_yard_8017DE00;
extern ActorTransform       D_dryfield_junk_yard_8017DE18;
extern ActorTransform       D_dryfield_junk_yard_8017DE30;
extern EvsCommand           D_dryfield_junk_yard_8017DE48[];
extern EvsCommand           D_dryfield_junk_yard_8017E028[];
extern EvsCommand           D_dryfield_junk_yard_8017E160[];
extern EvsCommand           D_dryfield_junk_yard_8017E2B0[];
extern EvsCommand           D_dryfield_junk_yard_8017E3D0[];
extern EvsCommand           D_dryfield_junk_yard_8017E490[];
extern EvsCommand           D_dryfield_junk_yard_8017E658[];

static void _dryfieldJunkYardDrawModelGroundShadow(Task* task);
static void func_dryfield_junk_yard_8017D708(Task* arg0);
static void _dryfieldJunkYardIdleRoomTask(Task* unusedTask);

/// The room task's states: set up, idle with a conditional companion
/// debug hook, then `taskKill`.
static const TaskFuncTable3 D_dryfield_junk_yard_8017D5C4 = {
    { func_dryfield_junk_yard_8017D708, _dryfieldJunkYardIdleRoomTask, taskKill },
};

/// Name the room task's second state hands to `func_80724608`.
static const char D_dryfield_junk_yard_8017D5D0[] = "DOG";

static void _dryfieldJunkYardCompanionSequenceTask(Task* task);
static s32  _dryfieldJunkYardRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unused);
static s32  _dryfieldJunkYardResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32  _dryfieldJunkYardHandleRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArgument);

enum {
    DRYFIELD_JUNK_YARD_MESSAGE_USE_KEY_ITEM = 0x13F1,
    DRYFIELD_JUNK_YARD_RETURN_SCENE_EVENT   = 5,
};

extern AnimationPlayRequest       D_dryfield_junk_yard_8017DD60;
extern AnimationPlayRequest       D_dryfield_junk_yard_8017DD74;
extern AnimationPlayRequest       D_dryfield_junk_yard_8017DD88;
extern AnimationPlayRequest       D_dryfield_junk_yard_8017DD9C;
extern AnimationPlayRequest       D_dryfield_junk_yard_8017DDB0;
extern AnimationPlayRequest       D_dryfield_junk_yard_8017DDC4;
extern AnimationPlayRequest       D_dryfield_junk_yard_8017DDEC;
extern WorldCollisionGrid         D_dryfield_junk_yard_8017F4C8[1];
extern WorldCollisionOccluder     D_dryfield_junk_yard_80181518[1];
extern WorldCollisionTrigger      D_dryfield_junk_yard_80180C7C[10];
extern WorldCollisionTrigger      D_dryfield_junk_yard_80180F74[19];
extern WorldCoordRoomAmbientEntry D_dryfield_junk_yard_80181BCC[8];
extern WorldCoordRoomLights       D_dryfield_junk_yard_80181BB4[1];
extern ActorTransform             D_dryfield_junk_yard_8017DE00;
extern TaskDesc                   Actor00100_D1BA84;
static void                       _dryfieldJunkYardSetSceneEvent(s8 sceneEvent);

TaskMessageEntry D_dryfield_junk_yard_8017DD20[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _dryfieldJunkYardResolveRoomTransition },
    { DRYFIELD_JUNK_YARD_MESSAGE_USE_KEY_ITEM, _dryfieldJunkYardRejectKeyItemUse },
    { ROOM_MESSAGE_COMMAND, junkYardCapMsg },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldJunkYardHandleRoomAction },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_dryfield_junk_yard_8017DD48[2] = {
    { { { TASK_BODY_NONE, 32 } }, _dryfieldJunkYardCompanionSequenceTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

AnimationPlayRequest D_dryfield_junk_yard_8017DD60 = { { .index = 1 }, 1, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_junk_yard_8017DD74 = { { .index = 6 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_junk_yard_8017DD88 = { { .index = 6 }, 34, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_junk_yard_8017DD9C = { { .index = 6 }, 35, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_junk_yard_8017DDB0 = { { .index = 6 }, 8, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_junk_yard_8017DDC4 = { { .index = 6 }, 10, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_junk_yard_8017DDD8 = { { .index = 6 }, 7, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_junk_yard_8017DDEC = { { .index = 6 }, 9, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_dryfield_junk_yard_8017DE00 = { { 0x288B, 0, 3893, 0 }, { 0, 2700, 0, 0 } };

ActorTransform D_dryfield_junk_yard_8017DE18 = { { 0x530A, 0, 3407, 0 }, { 0, 3074, 0, 0 } };

ActorTransform D_dryfield_junk_yard_8017DE30 = { { 0x54C4, 0, 3800, 0 }, { 0, 2048, 0, 0 } };

EvsCommand D_dryfield_junk_yard_8017DE48[20] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 5 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_junk_yard_8017DD60 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_dryfield_junk_yard_8017DE00 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1000 }, { .message = { .pointer = &D_dryfield_junk_yard_8017DD88 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_junk_yard_8017DD9C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_junk_yard_8017DDB0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_junk_yard_8017DDC4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x40720009 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_VIEW, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_dryfield_junk_yard_8017E028[13] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_junk_yard_8017DD60 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_dryfield_junk_yard_8017DE00 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1000 }, { .message = { .pointer = &D_dryfield_junk_yard_8017DDEC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_VIEW, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_dryfield_junk_yard_8017E160[14] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_junk_yard_8017DD60 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1006 }, { .message = { .pointer = &D_dryfield_junk_yard_8017DE30 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x40720009 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1000 }, { .message = { .pointer = &D_dryfield_junk_yard_8017DDC4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 38 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1000 }, { .message = { .pointer = &D_dryfield_junk_yard_8017DD74 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_dryfield_junk_yard_8017E2B0[12] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_junk_yard_8017DD60 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_dryfield_junk_yard_8017DE30 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1000 }, { .message = { .pointer = &D_dryfield_junk_yard_8017DD74 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_dryfield_junk_yard_8017E3D0[8] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 7 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_junk_yard_8017DD60 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x521A0008 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_dryfield_junk_yard_8017E490[19] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_junk_yard_8017DD60 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_dryfield_junk_yard_8017DE30 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1000 }, { .message = { .pointer = &D_dryfield_junk_yard_8017DDEC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1000 }, { .message = { .pointer = &D_dryfield_junk_yard_8017DDB0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x40720009 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1000 }, { .message = { .pointer = &D_dryfield_junk_yard_8017DDC4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 38 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS8 = _dryfieldJunkYardSetSceneEvent }, { .value = DRYFIELD_JUNK_YARD_RETURN_SCENE_EVENT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_dryfield_junk_yard_8017E658[11] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS8 = _dryfieldJunkYardSetSceneEvent }, { .value = DRYFIELD_JUNK_YARD_RETURN_SCENE_EVENT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

static TmdBone _gDryfieldJunkYardModel01378Skeleton[1] = {
#include "assets/dryfield_junk_yard_model_01378_skeleton.inc"
};

static u32 _gDryfieldJunkYardModel01378PartVerts[1] = {
#include "assets/dryfield_junk_yard_model_01378_partVerts.inc"
};

static SVECTOR _gDryfieldJunkYardModel01378Verts[54] = {
#include "assets/dryfield_junk_yard_model_01378_verts.inc"
};

static u32 _gDryfieldJunkYardModel01378Stream[234] = {
#include "assets/dryfield_junk_yard_model_01378_stream.inc"
};

TmdSource gDryfieldJunkYardModel01378 = {
    0,
    1640,
    0,
    1,
    _gDryfieldJunkYardModel01378PartVerts,
    _gDryfieldJunkYardModel01378Verts,
    &_gDryfieldJunkYardModel01378Verts[54],
    _gDryfieldJunkYardModel01378Skeleton,
    _gDryfieldJunkYardModel01378Stream,
};

WorldCollisionRoomResources D_dryfield_junk_yard_8017ED04[1] = {
    { D_dryfield_junk_yard_8017F4C8, D_dryfield_junk_yard_80180C7C, D_dryfield_junk_yard_80180F74, D_dryfield_junk_yard_80181518 },
};

WorldCoordRoomLighting D_dryfield_junk_yard_8017ED14[1] = {
    { D_dryfield_junk_yard_80181BB4, D_dryfield_junk_yard_80181BCC },
};

u8* D_dryfield_junk_yard_8017ED1C[1] = {
    gViewIdentityMap,
};

ViewCount D_dryfield_junk_yard_8017ED20[1] = { 7 };

DirectionWarpEntry D_dryfield_junk_yard_8017ED24[3] = {
    { { { .word = 0 }, 5030, 0, 390 }, { 0, 0, 0, 0 }, { { .word = 2048 }, 4800, 0, 1000 }, { 0, 0, 0, 0 }, 0x521A0004, 0x521A0003, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, 472 },
    { { { .word = 0 }, 0x55A1, 0, 1639 }, { 0, 0, 0, 0 }, { { .word = 0 }, 0x55A1, 0, 1639 }, { 0, 0, 0, 0 }, 0x521A0002, 0x521A0001, DIRECTION_WARP_SOUND_NONE, 5, DIRECTION_WARP_FLAG_NONE, 471 },
    { { { .word = 0 }, 1859, 0, 672 }, { 0, 0, 0, 0 }, { { .word = 0 }, 1859, 0, 672 }, { 0, 0, 0, 0 }, 0x521A0006, 0x521A0005, DIRECTION_WARP_SOUND_NONE, 3, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gDryfieldJunkYardCollision01F08Normals[33] = {
#include "assets/dryfield_junk_yard_collision_01F08_normals.inc"
};

static SVECTOR _gDryfieldJunkYardCollision01F08Verts[88] = {
#include "assets/dryfield_junk_yard_collision_01F08_verts.inc"
};

static WorldCollisionGridFace _gDryfieldJunkYardCollision01F08Faces[36] = {
#include "assets/dryfield_junk_yard_collision_01F08_faces.inc"
};

static s16 _gDryfieldJunkYardCollision01F08Cells[166] = {
#include "assets/dryfield_junk_yard_collision_01F08_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldJunkYardCollision01F08Cells[i])
static s16* _gDryfieldJunkYardCollision01F08Table[14] = {
#include "assets/dryfield_junk_yard_collision_01F08_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_junk_yard_8017F4C8[1] = {
    { NULL, _gDryfieldJunkYardCollision01F08Normals, _gDryfieldJunkYardCollision01F08Verts, _gDryfieldJunkYardCollision01F08Faces, _gDryfieldJunkYardCollision01F08Table, 2400, 608, 7, 2, 4000, 36 },
};

AreaResource D_dryfield_junk_yard_8017F4EC[1] = {
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_junk_yard_8017F4F8[2] = {
    { 25, 25, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_102500_801379A8 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_junk_yard_8017F510[2] = {
    { 1, 1, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &Actor00100_D1BA84 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_dryfield_junk_yard_8017F528[3] = {
    { 1, 0, 0, 5685, 0, 2272, 4096, 0, 0, 2, 0 },
    { 1, 0, 0, 5485, 0, 2072, 4352, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant D_dryfield_junk_yard_8017F558[13] = {
    { NULL, NULL },
    { D_map_dryfield_8017B9B4, D_dryfield_junk_yard_8017F4EC },
    { D_map_dryfield_8017B9C4, D_dryfield_junk_yard_8017F4F8 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_dryfield_junk_yard_8017F528, D_dryfield_junk_yard_8017F510 },
    { NULL, NULL },
    { NULL, NULL },
};

ViewCamera D_dryfield_junk_yard_8017F5C0[7] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -0x2DE6, 0x61A8, -5750 } }, 257 },
    { { { { 965, 0, -3980 }, { -535, 4058, -129 }, { 3944, 550, 956 } }, { 1022, 2299, -989 } }, 282 },
    { { { { 783, 0, -4020 }, { -3412, 2165, -665 }, { 2125, 3476, 414 } }, { 1120, 4985, -2136 } }, 239 },
    { { { { 1009, 0, -3969 }, { -1187, 3908, -301 }, { 3788, 1224, 962 } }, { -2958, 2969, -1227 } }, 282 },
    { { { { 66, 0, 4095 }, { 1199, 3916, -19 }, { -3915, 1199, 63 } }, { -0x63AC, 2717, -2429 } }, 289 },
    { { { { 1933, 0, -3610 }, { -957, 3949, -512 }, { 3481, 1085, 1864 } }, { -8412, 1167, -2987 } }, 257 },
    { { { { 1162, 0, 3927 }, { 1136, 3920, -336 }, { -3759, 1185, 1113 } }, { -0x4970, 2301, -1670 } }, 269 },
};

SpriteBatch D_dryfield_junk_yard_8017F6BC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_junk_yard_8017F6CC[30] = {
    { 143, 0x3FC0, { .fields = { 32, 128 } }, -40, -104, 3066, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 64 } }, 24, -72, 6500, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 120 } }, 56, -112, 5000, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -72, 64, 1304, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 72 } }, -96, -32, 1248, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 80 } }, -160, -40, 1068, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -88, 48, 1249, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -96, 56, 1146, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 72 } }, -160, 48, 937, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -160, 40, 1038, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -120, 40, 988, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -112, 40, 1281, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -112, 48, 1249, { .fields = { 32, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 64 } }, -112, 56, 1036, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, -104, 56, 1074, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -80, 64, 1281, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -80, 40, 1282, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 40, -24, 4346, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 32, -16, 4080, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, 24, -8, 3816, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 120, 64, 1172, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 112, 64, 1189, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, 120, 72, 1150, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 128, 72, 1160, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 64, -16, 3495, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 72, -40, 3638, { .fields = { 0, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 80, -72, 3625, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, 88, -104, 3625, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 96 } }, 96, 24, 3500, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 144 } }, 96, -120, 3500, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_junk_yard_8017F924[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 0, 0 } },
    { 3, 14, 0, 0, { 3, 0 } },
    { 17, 3, 0, 0, { 2, 0 } },
    { 20, 4, 0, 0, { 4, 0 } },
    { 24, 6, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_junk_yard_8017F95C[60] = {
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -104, 0, 1538, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -56, 0, 1522, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, -64, -56, 1522, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, -104, -56, 1538, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 40 } }, -64, -120, 1372, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -104, -120, 1388, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -40, -80, 1541, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -40, -72, 1541, { .fields = { 24, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 24 } }, -104, -80, 1528, { .fields = { 0, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 104, -64, 1558, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 104, -56, 1577, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 104, -48, 1578, { .fields = { 24, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 56, 984, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 64, 1124, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 64, 1038, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 64, 976, { .fields = { 32, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 72, 955, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 72, 951, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 72, 952, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 72, 949, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 72, 951, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 80, 942, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 80, 937, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 80, 934, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 80, 930, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 80, 931, { .fields = { 24, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 88, 929, { .fields = { 24, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 88, 923, { .fields = { 24, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 88, 920, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 88, 915, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 88, 916, { .fields = { 24, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 88, 918, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 96, 916, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 96, 906, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 96, 903, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 96, 900, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 96, 903, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 96, 904, { .fields = { 24, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 104, 918, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 104, 898, { .fields = { 16, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 104, 888, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 104, 884, { .fields = { 16, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 104, 885, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 104, 890, { .fields = { 16, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 112, 963, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, 112, 938, { .fields = { 16, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 112, 931, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 112, 887, { .fields = { 16, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 112, 878, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 112, 886, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -48, 24, 950, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 64 } }, 56, 56, 1134, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, -136, 24, 1220, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -88, 32, 1270, { .fields = { 24, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -72, 48, 1196, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -56, 56, 1174, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -8, 64, 1158, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 24, 72, 1142, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -104, 48, 1184, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -136, 48, 1200, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_junk_yard_8017FE0C[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 1, 0 } },
    { 9, 3, 0, 0, { 2, 0 } },
    { 12, 48, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_junk_yard_8017FE34[25] = {
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -64, -16, 2084, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -56, 0, 2125, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -80, 16, 2042, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -80, 0, 2069, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, -24, 2101, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -80, -40, 2069, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -80, -120, 2069, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 80, -88, 3836, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 88, -120, 3836, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 24, -80, 6000, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 24, -112, 6000, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 40, -56, 3550, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, 32, -48, 3452, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, 24, -40, 3366, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 24, -32, 2914, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 64, -32, 2838, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 40, -32, 3473, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 40, -24, 2857, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 136 } }, 104, -120, 2500, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 80, -72, 2750, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 80, -32, 2545, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 80 } }, 120, 40, 2500, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, 112, -16, 2500, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, 112, -72, 2500, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -120, 2500, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_junk_yard_80180028[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 11, 0, 0, { 1, 0 } },
    { 11, 7, 0, 0, { 2, 0 } },
    { 18, 7, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_junk_yard_80180050[97] = {
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 16, -112, 8250, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 88 } }, -96, -48, 1825, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -96, -120, 1825, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 32, -80, 4051, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 40, -120, 4051, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 8, -88, 7006, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 0, -40, 2314, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, -40, 2762, { .fields = { 16, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 0, -32, 2398, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, -32, 2533, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, -24, 2312, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, -16, 2122, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, -8, 1963, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, 0, 1825, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, 8, 1720, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 8, 16, 1627, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, -56, -40, 2182, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, -56, -32, 1941, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 8 } }, -64, -24, 1907, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 72, 8 } }, -72, -16, 1862, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 80, 8 } }, -72, -8, 1785, { .fields = { 64, 8 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, -80, 0, 1689, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, -24, 2039, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 88, 24 } }, -80, 8, 1673, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -120, 72, 1250, { .fields = { 16, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, 72, 1256, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -88, -48, 1372, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -88, 0, 1267, { .fields = { 24, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -64, 16, 1379, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 40 } }, -56, 40, 1314, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 40 } }, -56, 0, 1388, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, -120, 1250, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, -80, 1250, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, -40, 1250, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, 0, 1250, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, 40, 1250, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, 80, 1250, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -120, -120, 1250, { .fields = { 16, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -120, -80, 1250, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -120, -40, 1250, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -120, 0, 1250, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -96, -120, 1290, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -96, -80, 1290, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -96, -40, 1290, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -96, 0, 1290, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -120, 104, 569, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -88, 24, 1275, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -120, 40, 1250, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -96, 40, 1253, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, -112, 72, 1131, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -120, 80, 1079, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, -120, 88, 1032, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, -120, 96, 610, { .fields = { 112, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 0, 1521, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, 120, -16, 1266, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 8 } }, 96, -8, 1584, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, 120, -8, 1200, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, 128, 16, 1200, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 136, 24, 1050, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 144, 40, 1075, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 144, 48, 974, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 152, 48, 931, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 136, 40, 1035, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, 128, 24, 1102, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 120, 16, 1203, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 112, 0, 1290, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 104, 8, 1376, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 96, 0, 1476, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 80, 8, 1618, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 112, 0, 1290, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 120, -16, 1237, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 136 } }, 128, -16, 1151, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, 136, -8, 1067, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, 144, -8, 990, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, 152, -8, 959, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 48, -48, 2676, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 56, -48, 2513, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 64, -56, 2167, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 72, -56, 2248, { .fields = { 8, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 16 } }, 72, -48, 2192, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 72, 8 } }, 72, -32, 2202, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 80, 8 } }, 80, -24, 2196, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 72, -24, 2034, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 80, -16, 1882, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 32 } }, 88, -16, 1887, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -152, 96, 575, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -112, 96, 575, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 24 } }, -72, 96, 575, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 24 } }, -32, 96, 575, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 8, 96, 600, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 64, 96, 575, { .fields = { 16, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 112, 96, 500, { .fields = { 16, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, 112, 64, 575, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -64, -24, 2394, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -72, -48, 2252, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -80, -56, 2177, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 64 } }, -80, -72, 2583, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_junk_yard_801807E4[12] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 6, 0 } },
    { 5, 1, 0, 0, { 1, 0 } },
    { 6, 18, 0, 0, { 8, 0 } },
    { 24, 29, 0, 0, { 0, 0 } },
    { 53, 16, 0, 0, { 5, 0 } },
    { 69, 6, 0, 0, { 2, 0 } },
    { 75, 10, 0, 0, { 9, 0 } },
    { 85, 8, 0, 0, { 4, 0 } },
    { 93, 3, 0, 0, { 7, 0 } },
    { 96, 1, 0, 0, { 3, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_junk_yard_80180844[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_junk_yard_80180854[47] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, 56, 719, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, 64, 553, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, -56, -88, 4064, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 16, -120, 2402, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 16, -32, 2312, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 8, -32, 2340, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -16, 8, 1222, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -16, 24, 1098, { .fields = { 0, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -64, 16, 1209, { .fields = { 24, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 16 } }, -120, 16, 1177, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -128, 32, 1056, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 104, 8 } }, -80, 32, 657, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 16 } }, -48, 40, 593, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 16 } }, -136, 40, 597, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 48, 56, 718, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 16 } }, -48, 56, 530, { .fields = { 104, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 96, 16 } }, -144, 56, 540, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 112, 16 } }, -48, 72, 487, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 112, 16 } }, -160, 72, 491, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 120, 16 } }, -40, 88, 447, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 120, 16 } }, -160, 88, 447, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 128, 16 } }, -32, 104, 408, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 128, 16 } }, -160, 104, 411, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 72, -8, 1363, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 80, -8, 1312, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 88, -8, 1246, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 104, 0, 1148, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 88, -16, 1347, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 112, -16, 1388, { .fields = { 16, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, -24, 1012, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 104, -8, 1146, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 120, 0, 1078, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 136, -8, 995, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 136, 32, 1004, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 24, -24, 2186, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 64, 0, 1521, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -120, 16, 1234, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -160, -8, 1283, { .fields = { 16, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -144, 0, 1243, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -160, 0, 1243, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -160, -16, 1314, { .fields = { 32, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -128, -8, 1300, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -160, -64, 1156, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -160, -56, 1031, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -160, -48, 1122, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -160, 8, 1084, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -160, 56, 960, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_junk_yard_80180C00[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 34, 0, 0, { 1, 0 } },
    { 34, 8, 0, 0, { 2, 0 } },
    { 42, 5, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_dryfield_junk_yard_80180C28[7] = {
    { { .empty = D_dryfield_junk_yard_8017F6BC }, D_dryfield_junk_yard_8017F6BC, NULL },
    { { .elements = D_dryfield_junk_yard_8017F6CC }, D_dryfield_junk_yard_8017F924, NULL },
    { { .elements = D_dryfield_junk_yard_8017F95C }, D_dryfield_junk_yard_8017FE0C, NULL },
    { { .elements = D_dryfield_junk_yard_8017FE34 }, D_dryfield_junk_yard_80180028, NULL },
    { { .elements = D_dryfield_junk_yard_80180050 }, D_dryfield_junk_yard_801807E4, NULL },
    { { .empty = D_dryfield_junk_yard_80180844 }, D_dryfield_junk_yard_80180844, NULL },
    { { .elements = D_dryfield_junk_yard_80180854 }, D_dryfield_junk_yard_80180C00, NULL },
};

WorldCollisionTrigger D_dryfield_junk_yard_80180C7C[10] = {
    { NULL, NULL, NULL, { 3568, -1408, 2448, 0 }, { { 467, -2432, -3264, 0 }, { -494, -2432, 3235, 0 }, { 467, 2432, -3264, 0 }, { -494, 2432, 3235, 0 } }, { 4052, 0, 599, 0 }, { 0, 0, 4096, 0 }, 4087, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3680, -1376, 2336, 0 }, { { -517, -2400, 3225, 0 }, { 476, -2400, -3264, 0 }, { -517, 2400, 3225, 0 }, { 476, 2400, -3264, 0 } }, { -4056, 0, -621, 0 }, { 0, 0, 4096, 0 }, 4063, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7324, -1328, 2299, 0 }, { { 335, -2352, -3268, 0 }, { -337, -2352, 3267, 0 }, { 335, 2352, -3268, 0 }, { -337, 2352, 3267, 0 } }, { 4074, 0, 418, 0 }, { 0, 0, 4096, 0 }, 4039, 0, 4, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7483, -1296, 2174, 0 }, { { -337, -2320, 3267, 0 }, { 335, -2320, -3268, 0 }, { -337, 2320, 3267, 0 }, { 335, 2320, -3268, 0 } }, { -4076, 0, -420, 0 }, { 0, 0, 4096, 0 }, 4015, 0, 2, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3FFF, -1296, 3775, 0 }, { { -407, -2320, 1963, 0 }, { 403, -2320, -1967, 0 }, { -407, 2320, 1963, 0 }, { 403, 2320, -1967, 0 } }, { -4033, 0, -832, 0 }, { 0, 0, 4096, 0 }, 3061, 0, 7, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3F40, -1328, 3727, 0 }, { { 400, -2352, -1951, 0 }, { -404, -2352, 1947, 0 }, { 400, 2352, -1951, 0 }, { -404, 2352, 1947, 0 } }, { 4027, 0, 830, 0 }, { 0, 0, 4096, 0 }, 3072, 0, 5, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3400, -1344, 928, 0 }, { { 1262, -2352, -2822, 0 }, { -1262, -2352, 2822, 0 }, { 1262, 2352, -2822, 0 }, { -1262, 2352, 2822, 0 } }, { 3740, 0, 1672, 0 }, { 0, 0, 4096, 0 }, 3882, 0, 7, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3430, -1312, 5328, 0 }, { { -1330, -2352, -1782, 0 }, { 1330, -2352, 1782, 0 }, { -1330, 2352, -1782, 0 }, { 1330, 2352, 1782, 0 } }, { 3290, 0, -2457, 0 }, { 0, 0, 4096, 0 }, 3228, 0, 7, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x3531, -1344, 5393, 0 }, { { 1310, -2352, 1786, 0 }, { -1310, -2352, -1786, 0 }, { 1310, 2352, 1786, 0 }, { -1310, 2352, -1786, 0 } }, { -3316, 0, 2430, 0 }, { 0, 0, 4096, 0 }, 3228, 0, 4, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 0x345F, -1345, 1296, 0 }, { { -1058, -2352, 2442, 0 }, { 1058, -2352, -2442, 0 }, { -1058, 2352, 2442, 0 }, { 1058, 2352, -2442, 0 } }, { -3765, 0, -1632, 0 }, { 0, 0, 4096, 0 }, 3547, 0, 4, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_dryfield_junk_yard_80180F74[19] = {
    { NULL, NULL, NULL, { 5248, -48, 336, 0 }, { { -1024, 0, -464, 0 }, { 1024, 0, -464, 0 }, { -1024, 0, 464, 0 }, { 1024, 0, 464, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, 4096, 0 }, 1123, WORLD_COLLISION_TRIGGER_ACTION_WARP, 24, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x5600, -48, 1568, 0 }, { { -1024, 0, -464, 0 }, { 1024, 0, -464, 0 }, { -1024, 0, 464, 0 }, { 1024, 0, 464, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, 4096, 0 }, 1123, WORLD_COLLISION_TRIGGER_ACTION_WARP, 27, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 432, -64, 4592, 0 }, { { -896, 0, -1056, 0 }, { 896, 0, -1056, 0 }, { -896, 0, 1056, 0 }, { 896, 0, 1056, 0 } }, { 0, 4100, 0, 0 }, { 4096, 0, 0, 0 }, 1384, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6623, -64, 2783, 0 }, { { -935, 0, 3140, 0 }, { -306, 0, -3261, 0 }, { 307, 0, 3262, 0 }, { 936, 0, -3139, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, -4096, 0 }, 3268, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 1, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 0x4A1F, -64, 3487, 0 }, { { -449, 0, 1009, 0 }, { -148, 0, -1017, 0 }, { 149, 0, 1018, 0 }, { 450, 0, -1008, 0 } }, { 0, 4109, 0, 0 }, { 0, 0, 4096, 0 }, 1101, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 2, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 1744, -64, 351, 0 }, { { -1040, 0, -592, 0 }, { 1040, 0, -592, 0 }, { -1040, 0, 849, 0 }, { 1040, 0, 849, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, -4096, 0 }, 1342, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x41A2, -64, 3328, 0 }, { { -2624, 0, -272, 0 }, { 2624, 0, -656, 0 }, { -2624, 0, 656, 0 }, { 2624, 0, 272, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, 4096, 0 }, 2697, WORLD_COLLISION_TRIGGER_ACTION_CAP, 16, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 352, -64, 2336, 0 }, { { -896, 0, -1056, 0 }, { 896, 0, -1056, 0 }, { -896, 0, 1056, 0 }, { 896, 0, 1056, 0 } }, { 0, 4100, 0, 0 }, { 3973, 0, -995, 0 }, 1384, WORLD_COLLISION_TRIGGER_ACTION_CAP, 14, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1376, -64, 4096, 0 }, { { -480, 0, -1344, 0 }, { 448, 0, -1280, 0 }, { -480, 0, 1312, 0 }, { 512, 0, 1312, 0 } }, { 0, 4100, 0, 0 }, { -4077, 0, -402, 0 }, 1425, WORLD_COLLISION_TRIGGER_ACTION_CAP, 15, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x5088, -64, 3056, 0 }, { { -1032, 0, -528, 0 }, { 1112, 0, -528, 0 }, { -1032, 0, 528, 0 }, { 952, 0, 528, 0 } }, { 0, 4106, 0, 0 }, { 200, 0, 4090, 0 }, 1227, WORLD_COLLISION_TRIGGER_ACTION_CAP, 17, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x51B0, -64, 4144, 0 }, { { -1840, 0, -464, 0 }, { 1840, 0, -464, 0 }, { -1840, 0, 464, 0 }, { 1840, 0, 464, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, -4096, 0 }, 1894, WORLD_COLLISION_TRIGGER_ACTION_CAP, 18, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x4140, -64, 4032, 0 }, { { -2384, 0, -464, 0 }, { 2384, 0, -464, 0 }, { -2384, 0, 464, 0 }, { 2384, 0, 464, 0 } }, { 0, 4100, 0, 0 }, { 0, 0, -4096, 0 }, 2428, WORLD_COLLISION_TRIGGER_ACTION_CAP, 24, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2960, -64, 3136, 0 }, { { -1680, 0, -416, 0 }, { 1648, 0, -352, 0 }, { -1680, 0, 384, 0 }, { 1712, 0, 384, 0 } }, { 0, 4098, 0, 0 }, { -201, 0, -4091, 0 }, 1750, WORLD_COLLISION_TRIGGER_ACTION_CAP, 15, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4224, -64, 4160, 0 }, { { -576, 0, -1344, 0 }, { 352, 0, -1280, 0 }, { -576, 0, 1312, 0 }, { 800, 0, 1312, 0 } }, { 0, 4100, 0, 0 }, { 3856, 0, -1380, 0 }, 1536, WORLD_COLLISION_TRIGGER_ACTION_CAP, 15, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 864, -64, 608, 0 }, { { -704, 0, -672, 0 }, { 704, 0, -672, 0 }, { -704, 0, 672, 0 }, { 704, 0, 672, 0 } }, { 0, 4100, 0, 0 }, { 4096, 0, 0, 0 }, 972, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x3660, -64, 4720, 0 }, { { -239, 0, -1216, 0 }, { 848, 0, -1216, 0 }, { -1455, 0, 1216, 0 }, { 848, 0, 1216, 0 } }, { 0, 4106, 0, 0 }, { -4096, 0, 0, 0 }, 1894, WORLD_COLLISION_TRIGGER_ACTION_CAP, 24, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x3587, -64, 2471, 0 }, { { -318, 0, 1507, 0 }, { -895, 0, -1373, 0 }, { 880, 0, 1326, 0 }, { 335, 0, -1458, 0 } }, { 0, 4101, 0, 0 }, { -4091, 0, 201, 0 }, 1634, WORLD_COLLISION_TRIGGER_ACTION_CAP, 16, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0x5320, -64, 2032, 0 }, { { -552, 0, -704, 0 }, { 632, 0, -704, 0 }, { -552, 0, 704, 0 }, { 472, 0, 704, 0 } }, { 0, 4104, 0, 0 }, { 4050, 0, 601, 0 }, 944, WORLD_COLLISION_TRIGGER_ACTION_CAP, 17, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 7520, -64, 4704, 0 }, { { -3216, 0, -624, 0 }, { 3216, 0, -624, 0 }, { -3216, 0, 624, 0 }, { 3216, 0, 624, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, -4096, 0 }, 3268, WORLD_COLLISION_TRIGGER_ACTION_CAP, 11, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionOccluder D_dryfield_junk_yard_80181518[1] = {
    { NULL, NULL, { 0x4980, -544, 1536, 0 }, { { 0, 1024, -1024, 0 }, { 0, -1024, -1024, 0 }, { 0, 1024, 1024, 0 }, { 0, -1024, 1024, 0 } }, { 4096, 0, 0, 0 }, 1448, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

/// The junk yard's point lights, contributing in every room view.
///
/// Positions and falloff radii use integer world units; RGB intensities use
/// 12 fractional bits (`ONE` is full strength). Full strength occurs at each
/// light's position, with squared-distance falloff to its outer radius.
/// The loaded room overlay owns these mutable records: coordinate updates
/// parent them to the view and rebuild transforms; shading queries overwrite
/// attenuation. References must not outlive the loaded overlay.
static WorldCoordPointLight _gDryfieldJunkYardPointLights[] = {
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 505, -2000, 6031 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .attenuation  = 0,
                               .parent       = NULL,
                           } },
            .color     = { 3094, 3094, 3094 },
        },
        .inner = 0,
        .outer = 7200,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1931, -2000, -3662 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .attenuation  = 0,
                               .parent       = NULL,
                           } },
            .color     = { 3094, 3094, 3094 },
        },
        .inner = 0,
        .outer = 7200,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 9505, 161, -2977 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .attenuation  = 0,
                               .parent       = NULL,
                           } },
            .color     = { 3278, 3278, 3278 },
        },
        .inner = 0,
        .outer = 9103,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6211, -1300, 4996 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .attenuation  = 0,
                               .parent       = NULL,
                           } },
            .color     = { 3278, 3278, 3278 },
        },
        .inner = 0,
        .outer = 6008,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x3699, -1637, 7534 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .attenuation  = 0,
                               .parent       = NULL,
                           } },
            .color     = { 3564, 3564, 3564 },
        },
        .inner = 0,
        .outer = 4500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x3CEF, 440, 103 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .attenuation  = 0,
                               .parent       = NULL,
                           } },
            .color     = { 4066, ONE, ONE },
        },
        .inner = 0,
        .outer = 5125,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x59EE, -2000, 1018 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .attenuation  = 0,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 0,
        .outer = 4500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0x4652, -2000, 6020 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .attenuation  = 0,
                               .parent       = NULL,
                           } },
            .color     = { 4066, ONE, ONE },
        },
        .inner = 0,
        .outer = 4000,
    },
};

/// The junk yard's two cone lights, contributing in every room view.
///
/// Positions and falloff radii use integer world units; RGB intensities and
/// axes use 12 fractional bits (`ONE` is full strength or unit length).
/// Each cone's full opening is in 0x1000 units per turn.
/// The loaded room overlay owns these mutable records: coordinate updates
/// parent them to the view and rebuild transforms; shading queries overwrite
/// attenuation. References must not outlive the loaded overlay.
static WorldCoordSpotLight _gDryfieldJunkYardSpotLights[] = {
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { -2140, 0, 3502 }, { 1222, -3850, 746 }, { 3285, 1432, 2002 } }, { -8050, -2992, 2469 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .attenuation  = 0,
                               .parent       = NULL,
                           } },
            .color     = { 2043, 2050, 2050 },
        },
        .axis  = { 3496, 745, 1999, 0 },
        .inner = 0x783C,
        .outer = 0x783D,
        .angle = 671,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { -1983, 0, -3594 }, { -1114, -3902, 614 }, { -3417, 1271, 1884 } }, { 0x7CCE, -3078, 2935 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .attenuation  = 0,
                               .parent       = NULL,
                           } },
            .color     = { 3320, 3320, 3320 },
        },
        .axis  = { -3586, 613, 1881, 0 },
        .inner = 0x4A38,
        .outer = 0x55F0,
        .angle = 671,
    },
};

/// Uninterpreted bytes between the room's cone lights and light descriptor.
///
/// No identified consumer reaches this region. Its original type, internal
/// boundaries and purpose are unproven; preserve the image bytes verbatim.
static u8 _gDryfieldJunkYardUnknownData[648] = {
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x08,
    0x00,
    0x00,
    0x00,
    0x74,
    0xC4,
    0x18,
    0x80,
    0x02,
    0x00,
    0x00,
    0x00,
    0x74,
    0xC7,
    0x00,
    0x00,
    0x49,
    0x00,
    0x00,
    0x10,
    0x4B,
    0x00,
    0x00,
    0x10,
    0x49,
    0x00,
    0x00,
    0x10,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x70,
    0xC8,
    0x18,
    0x80,
    0x70,
    0xC8,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x70,
    0xC8,
    0x18,
    0x80,
    0xC8,
    0x00,
    0x00,
    0x00,
    0xA8,
    0xFF,
    0x60,
    0x00,
    0xFC,
    0x02,
    0x00,
    0x00,
    0x80,
    0x80,
    0x80,
    0x00,
    0x8E,
    0x00,
    0xC0,
    0x3F,
    0xC8,
    0x00,
    0x08,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x21,
    0x03,
    0x38,
    0xE0,
    0x80,
    0x80,
    0x80,
    0x00,
    0x8E,
    0x00,
    0xC0,
    0x3F,
    0xC8,
    0x00,
    0x08,
    0x00,
    0xA8,
    0xFF,
    0x00,
    0x00,
    0x4B,
    0x03,
    0x38,
    0xD8,
    0x80,
    0x80,
    0x80,
    0x00,
    0x8E,
    0x00,
    0xC0,
    0x3F,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x80,
    0x80,
    0x80,
    0x00,
    0x8E,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xA8,
    0xFF,
    0x38,
    0x00,
    0xE9,
    0x03,
    0x00,
    0x00,
    0x80,
    0x80,
    0x80,
    0x00,
    0x8E,
    0x00,
    0x00,
    0x00,
    0xC8,
    0x00,
    0x08,
    0x00,
    0xA8,
    0xFF,
    0x30,
    0x00,
    0x2D,
    0x04,
    0x38,
    0xB8,
    0x00,
    0x00,
    0x00,
    0x00,
    0x8E,
    0x00,
    0xC0,
    0x3F,
    0xC8,
    0x00,
    0x08,
    0x00,
    0xA8,
    0xFF,
    0x28,
    0x00,
    0x7B,
    0x04,
    0x38,
    0xB0,
    0x80,
    0x80,
    0x00,
    0x00,
    0x8E,
    0x00,
    0xC0,
    0x3F,
    0xC8,
    0x00,
    0x08,
    0x00,
    0xA8,
    0xFF,
    0x20,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xC8,
    0x00,
    0x08,
    0x00,
    0xA8,
    0xFF,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x80,
    0x80,
    0x80,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x01,
    0x00,
    0x1B,
    0x00,
    0x0D,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x8F,
    0x00,
    0xC0,
    0x3F,
    0x28,
    0x00,
    0x08,
    0x00,
    0x50,
    0x00,
    0x70,
    0x00,
    0x6F,
    0x02,
    0x00,
    0x00,
    0x80,
    0x80,
    0x80,
    0x00,
    0x8F,
    0x00,
    0xC0,
    0x3F,
    0x28,
    0x00,
    0x08,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x8F,
    0x00,
    0xC0,
    0x3F,
    0x28,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xD9,
    0x02,
    0x00,
    0x38,
    0x80,
    0x80,
    0x00,
    0x00,
    0x8F,
    0x00,
    0xC0,
    0x3F,
    0x28,
    0x00,
    0x00,
    0x00,
    0x50,
    0x00,
    0x50,
    0x00,
    0x05,
    0x03,
    0x00,
    0x58,
    0x80,
    0x80,
    0x80,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x28,
    0x00,
    0x08,
    0x00,
    0x50,
    0x00,
    0x48,
    0x00,
    0x36,
    0x03,
    0x78,
    0x70,
    0x80,
    0x80,
    0x80,
    0x00,
    0x8F,
    0x00,
    0x00,
    0x00,
    0x28,
    0x00,
    0x08,
    0x00,
    0x50,
    0x00,
    0x40,
    0x00,
    0x6E,
    0x03,
    0x00,
    0x30,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x50,
    0x00,
    0x30,
    0x00,
    0xF7,
    0x03,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x8F,
    0x00,
    0xC0,
    0x3F,
    0x28,
    0x00,
    0x00,
    0x00,
    0x50,
    0x00,
    0x28,
    0x00,
    0x4C,
    0x04,
    0x00,
    0x00,
    0x80,
    0x80,
    0x80,
    0x00,
    0x8F,
    0x00,
    0xC0,
    0x3F,
    0x28,
    0x00,
    0x08,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xB1,
    0x04,
    0x08,
    0x10,
    0x80,
    0x80,
    0x80,
    0x00,
    0x8F,
    0x00,
    0xC0,
    0x3F,
    0x28,
    0x00,
    0x08,
    0x00,
    0x50,
    0x00,
    0x00,
    0x00,
    0xF0,
    0xFF,
    0x08,
    0x28,
    0x80,
    0x80,
    0x80,
    0x00,
    0x8F,
    0x00,
    0xC0,
    0x3F,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x80,
    0x80,
    0x80,
    0x00,
    0x8E,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x60,
    0xFF,
    0x60,
    0x00,
    0xBA,
    0x02,
    0x00,
    0x00,
    0x80,
    0x80,
    0x80,
    0x00,
    0x8E,
    0x00,
    0x00,
    0x00,
    0x30,
    0x00,
    0x08,
    0x00,
    0x60,
    0xFF,
    0x58,
    0x00,
    0xC9,
    0x02,
    0x20,
    0x18,
};

WorldCoordRoomLights D_dryfield_junk_yard_80181BB4[1] = {
    { 0, NULL, ARRAY_SIZE(_gDryfieldJunkYardPointLights), _gDryfieldJunkYardPointLights, ARRAY_SIZE(_gDryfieldJunkYardSpotLights), _gDryfieldJunkYardSpotLights },
};

WorldCoordRoomAmbientEntry D_dryfield_junk_yard_80181BCC[8] = {
    { .viewCount = ARRAY_SIZE(D_dryfield_junk_yard_80181BCC) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 400, 400, 400, 400 } },
    { .color = { 350, 350, 350, 350 } },
    { .color = { 300, 300, 300, 300 } },
    { .color = { 400, 400, 400, 400 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 450, 450, 450, 450 } },
};

WorldCollisionFootstepSounds D_dryfield_junk_yard_80181C0C = {
    0x10000049,
    0x1000004B,
    0x10000049,
};

WorldCollisionSurfaceProperties D_dryfield_junk_yard_80181C18[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_junk_yard_80181C20[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_junk_yard_80181C0C },
};

WorldCollisionSurfaceProperties* D_dryfield_junk_yard_80181C28[8] = {
    D_dryfield_junk_yard_80181C18,
    D_dryfield_junk_yard_80181C20,
    D_dryfield_junk_yard_80181C18,
    D_dryfield_junk_yard_80181C18,
    D_dryfield_junk_yard_80181C18,
    D_dryfield_junk_yard_80181C18,
    D_dryfield_junk_yard_80181C18,
    D_dryfield_junk_yard_80181C18,
};

void dryfieldJunkYardWireRopeTask(Task* task)
{
    enum { DRYFIELD_JUNK_YARD_WIRE_ROPE_COLLECTED_STATE = 2 };

    const Enemy* placedObject;
    TmdObject*   model;
    s32          objectState;

    placedObject = task->spawnArg2.pointer;
    model        = task->extra.tmd;
    objectState  = areaGetCurrentObjectState((u8)placedObject->placeKey);
    // Clear flags before the branch; collection hides both model and shadow.
    model->flags = 0;
    if (objectState == DRYFIELD_JUNK_YARD_WIRE_ROPE_COLLECTED_STATE) {
        model->flags = (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
    } else {
        model->otOffset = 0;
    }
    _dryfieldJunkYardDrawModelGroundShadow(task);
}

/// Draws the room model's ground shadow at its composed root position.
///
/// `task` must own a model with a root coordinate. Models that suppress active
/// drawing or have no primitive buffer draw no shadow. The square's half-side
/// is 416 world-coordinate units and its grayscale shade is 192. The temporary
/// centre is borrowed by the shadow drawer and released before returning.
static void _dryfieldJunkYardDrawModelGroundShadow(Task* task)
{
    enum {
        DRYFIELD_JUNK_YARD_GROUND_SHADOW_HALF_SIZE = 0x1A0,
        DRYFIELD_JUNK_YARD_GROUND_SHADOW_SHADE     = 0xC0,
    };
    ActorRenderGroundShadowCentreScratch* shadowScratch;
    GfxCoord*                             rootCoord;
    TmdObject*                            model;

    model     = task->extra.tmd;
    rootCoord = model->coords;
    if ((model->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) == 0 && model->buffer != NULL) {
        shadowScratch = SCRATCH_STACK_RESERVE_BLOCK(ActorRenderGroundShadowCentreScratch);
        // Compose the root before taking its world-space shadow centre.
        actorRenderComposeCoord(rootCoord);
        shadowScratch->centre.vx = rootCoord->workm.t[0];
        shadowScratch->centre.vy = rootCoord->workm.t[1];
        shadowScratch->centre.vz = rootCoord->workm.t[2];
        effectDrawGroundShadow(&shadowScratch->centre, DRYFIELD_JUNK_YARD_GROUND_SHADOW_HALF_SIZE, DRYFIELD_JUNK_YARD_GROUND_SHADOW_SHADE);
        SCRATCH_STACK_RELEASE_BLOCK(ActorRenderGroundShadowCentreScratch);
    }
}

/// State 0 of the room task: publishes the message table and claims game
/// pointer slot 7. With a slot-0xA task present, it sends that task its
/// opening messages while nibble 0x38 is clear, then either latches nibble
/// 0x39 and starts the `evsStartScriptWithSkip` sequence (once nibble 0x28 has reached
/// 2) or, on a visit whose `warp` is 2, sends it message 0x3E9. Advances the
/// state either way.
static void func_dryfield_junk_yard_8017D708(Task* arg0)
{
    arg0->msgTable = D_dryfield_junk_yard_8017DD20;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    if (gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != 0) {
        if (gameFlagGetNibble(GAME_FLAG_JUNK_YARD_PROGRESS) == 0) {
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_COMPANION), 0x3E9, &D_dryfield_junk_yard_8017DE00, 0);
            companionWriteAnimationBankIndex(&D_dryfield_junk_yard_8017DD88.source.index);
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_COMPANION), ANIMATION_MESSAGE_PLAY, &D_dryfield_junk_yard_8017DD88, 0);
        }
        if ((gameFlagGetNibble(GAME_FLAG_JUNK_YARD_RETURN_SCENE_SEEN) == 0) && (gameFlagGetNibble(GAME_FLAG_TRAILER_COACH_PROGRESS) >= 2)) {
            gameFlagSetNibble(GAME_FLAG_JUNK_YARD_RETURN_SCENE_SEEN, 1);
            evsStartScriptWithSkip(D_dryfield_junk_yard_8017E490, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_dryfield_junk_yard_8017E658);
        } else if (gGameSession->location.loc.warp == 2) {
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_COMPANION), 0x3E9, &D_dryfield_junk_yard_8017DE30, 0);
        }
    }
    arg0->state = (s32)(arg0->state + 1);
}

/// Runs the junk-yard companion scene or its turn-and-animation sequence.
///
/// Initial state 0 starts the skippable scene, waits for event completion and
/// releases at state 2. Entry at state 3 instead requests a companion yaw turn,
/// waits for motion, plays clip 7 to completion and starts clip 9 before release.
/// Other states do nothing. The room scripts and companion must remain live
/// through the selected sequence; message payloads are borrowed synchronously.
static void _dryfieldJunkYardCompanionSequenceTask(Task* task)
{
    enum { COMPANION_SCENE_START      = 0,
           COMPANION_SCENE_WAIT       = 1,
           COMPANION_SEQUENCE_RELEASE = 2,
           COMPANION_TURN_START       = 3,
           COMPANION_TURN_WAIT        = 4,
           COMPANION_ANIMATION_START  = 5,
           COMPANION_ANIMATION_WAIT   = 6,
           COMPANION_FINAL_ANIMATION  = 7 };

    switch (task->state) {
        case COMPANION_SCENE_START:
            evsStartScriptWithSkip(D_dryfield_junk_yard_8017DE48, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_dryfield_junk_yard_8017E028);
            task->state = task->state + 1;
            return;
        case COMPANION_SCENE_WAIT:
            if (gGameSession->eventState != 0) {
                return;
            }
            task->state = task->state + 1;
            return;
        case COMPANION_TURN_START:
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_COMPANION), GAME_ACTOR_MESSAGE_TURN_TO_YAW, &D_dryfield_junk_yard_8017DE18, 0);
            task->state = task->state + 1;
            return;
        case COMPANION_TURN_WAIT:
            if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_COMPANION), GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) != 0) {
                return;
            }
            task->state = task->state + 1;
            return;
        case COMPANION_ANIMATION_START:
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_COMPANION), ANIMATION_MESSAGE_PLAY, &D_dryfield_junk_yard_8017DDD8, 0);
            task->state = task->state + 1;
            return;
        case COMPANION_ANIMATION_WAIT:
            if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_COMPANION), ANIMATION_MESSAGE_IS_PLAYING, 0, 0) != 0) {
                return;
            }
            task->state = task->state + 1;
            return;
        case COMPANION_FINAL_ANIMATION:
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_COMPANION), ANIMATION_MESSAGE_PLAY, &D_dryfield_junk_yard_8017DDEC, 0);
            /* fallthrough */
        case COMPANION_SEQUENCE_RELEASE:
            taskKill(task);
            return;
    }
}

#include "../../shared/junk_yard_cap_msg.inc.c"

/// Refuses every key-item-use request in the junk yard without consuming an item.
///
/// The message carries the inventory item ID in `itemId` and an unused second
/// word. Returns 0, selecting the inventory's "no use now" response; all
/// parameters are ignored.
static s32 _dryfieldJunkYardRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unused)
{
    enum { DRYFIELD_JUNK_YARD_KEY_ITEM_USE_REFUSED = 0 };

    return DRYFIELD_JUNK_YARD_KEY_ITEM_USE_REFUSED;
}

/// Resolves garage arrivals and gates departure to the trailer coach on companion progress.
///
/// Borrows a complete request and writable reply, which may alias. Returns 2
/// while the companion's final scene is eligible, otherwise 1. Execution starts
/// that scene or commits the first trailer arrival and companion schedule;
/// queries only copy the request and report eligibility. Requires live flags
/// and the room scripts through deferred playback.
static s32 _dryfieldJunkYardResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { TRANSITION_ALLOWED          = 1,
           COMPANION_SCENE_ELIGIBLE    = 2,
           GARAGE_LATE_CHAPTER         = 4,
           GARAGE_DEFAULT_ROOM         = 1,
           GARAGE_LATE_ROOM            = 2,
           JUNK_YARD_SEQUENCE_ACTIVE   = 1,
           JUNK_YARD_SEQUENCE_FINISHED = 2,
           TRAILER_FIRST_ARRIVAL_WARP  = 2,
           TRAILER_PROGRESS_STARTED    = 1,
           COMPANION_TRAILER_SCHEDULE  = 4 };

    *reply = *request;
    if (request->areaId == GAME_AREA_DRYFIELD_GARAGE && request->queryOnly == ROOM_EVENT_EXECUTE) {
        if (gameFlagGetNibble(GAME_FLAG_STORY_CHAPTER) >= GARAGE_LATE_CHAPTER) {
            reply->room = GARAGE_LATE_ROOM;
        } else {
            reply->room = GARAGE_DEFAULT_ROOM;
        }
    }
    if (request->areaId == GAME_AREA_DRYFIELD_TRAILER_COACH) {
        if (gameFlagGetNibble(GAME_FLAG_JUNK_YARD_PROGRESS) == JUNK_YARD_SEQUENCE_ACTIVE) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                gameFlagSetNibble(GAME_FLAG_JUNK_YARD_PROGRESS, JUNK_YARD_SEQUENCE_FINISHED);
                evsStartScriptWithSkip(D_dryfield_junk_yard_8017E3D0, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_dryfield_junk_yard_8017E2B0);
            }
            return COMPANION_SCENE_ELIGIBLE;
        }
        if (gameFlagGetNibble(GAME_FLAG_TRAILER_COACH_PROGRESS) == 0 && request->queryOnly == ROOM_EVENT_EXECUTE) {
            reply->warp = TRAILER_FIRST_ARRIVAL_WARP;
            gameFlagSetNibble(GAME_FLAG_TRAILER_COACH_PROGRESS, TRAILER_PROGRESS_STARTED);
            gameFlagSetNibble(GAME_FLAG_COMPANION_2_SCHEDULE, COMPANION_TRAILER_SCHEDULE);
        }
    }
    return TRANSITION_ALLOWED;
}

/// Starts and finishes the junk-yard companion sequence from room trigger actions.
///
/// Borrows `request` for synchronous dispatch. Action 1 starts the sequence
/// while progress is 0; action 2 advances progress 1 to 2 once the companion's
/// root X reaches 21001 whole room-coordinate units. Progress is committed
/// before task/script allocation, with no rollback on failure. Returns zero;
/// the receiver, message ID and second payload word are unused. The room and
/// its scripts must remain loaded until the spawned sequence finishes.
static s32 _dryfieldJunkYardHandleRoomAction(Task* task, s32 messageId, const DirectionActionRequest* request, s32 unusedArgument)
{
    enum {
        DRYFIELD_JUNK_YARD_ACTION_BEGIN_SEQUENCE  = 1,
        DRYFIELD_JUNK_YARD_ACTION_FINISH_SEQUENCE = 2,
        DRYFIELD_JUNK_YARD_PROGRESS_INITIAL       = 0,
        DRYFIELD_JUNK_YARD_PROGRESS_SEQUENCE      = 1,
        DRYFIELD_JUNK_YARD_PROGRESS_FINAL_SCENE   = 2,
        DRYFIELD_JUNK_YARD_COMPANION_EXIT_X_MIN   = 21001,
        DRYFIELD_JUNK_YARD_SEQUENCE_TASK          = 0,
    };

    Task* companion;

    if (request->actionId == DRYFIELD_JUNK_YARD_ACTION_BEGIN_SEQUENCE) {
        if (gameFlagGetNibble(GAME_FLAG_JUNK_YARD_PROGRESS) == DRYFIELD_JUNK_YARD_PROGRESS_INITIAL) {
            gameFlagSetNibble(GAME_FLAG_JUNK_YARD_PROGRESS, DRYFIELD_JUNK_YARD_PROGRESS_SEQUENCE);
            taskSpawnFromTable(D_dryfield_junk_yard_8017DD48, DRYFIELD_JUNK_YARD_SEQUENCE_TASK, 0, 0);
        }
    }
    if (request->actionId == DRYFIELD_JUNK_YARD_ACTION_FINISH_SEQUENCE) {
        companion = gameGetTaskSlot(GAME_TASK_SLOT_COMPANION);
        if ((companion != NULL) && (companion->extra.tmd->coords->coord.t[0] >= DRYFIELD_JUNK_YARD_COMPANION_EXIT_X_MIN) &&
            (gameFlagGetNibble(GAME_FLAG_JUNK_YARD_PROGRESS) == DRYFIELD_JUNK_YARD_PROGRESS_SEQUENCE)) {
            gameFlagSetNibble(GAME_FLAG_JUNK_YARD_PROGRESS, DRYFIELD_JUNK_YARD_PROGRESS_FINAL_SCENE);
            evsStartScriptWithSkip(D_dryfield_junk_yard_8017E160, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_dryfield_junk_yard_8017E2B0);
        }
    }
    return 0;
}

/// Sets the live save's scene event for subsequent area-music selection.
///
/// The event-script callback consumes the signed low byte of its argument word.
/// Both the return scene and its skip path supply
/// `DRYFIELD_JUNK_YARD_RETURN_SCENE_EVENT`.
static void _dryfieldJunkYardSetSceneEvent(s8 sceneEvent)
{
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = sceneEvent;
}

/// Keeps the junk-yard room task idle, with an optional companion debug hook.
///
/// With debug mode enabled and a live companion, calls the external routine
/// with that task, -140, 10 and the "DOG" string. The routine's effect and the
/// integer arguments' units are unproven. Leaves the room state unchanged and
/// repeats the hook each update; the callback's own task argument is unused.
static void _dryfieldJunkYardIdleRoomTask(Task* unusedTask)
{
    if ((gDisplayState.debugMode != 0) && (gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != 0)) {
        func_80724608(gameGetTaskSlot(GAME_TASK_SLOT_COMPANION), -0x8C, 0xA, D_dryfield_junk_yard_8017D5D0);
    }
}

void dryfieldJunkYardRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_dryfield_junk_yard_8017D5C4;
    stateHandlers.funcs[task->state](task);
}

void dryfieldJunkYardEnableViewEffectsTask(Task* unusedTask)
{
    gRoomEffectState->roomEffectMode = ROOM_EFFECT_VIEW_ENABLED;
}
