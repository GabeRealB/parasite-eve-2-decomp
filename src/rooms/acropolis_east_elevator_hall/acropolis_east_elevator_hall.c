#include "rooms/acropolis_east_elevator_hall.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/gtemac.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/model_objects.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/gfxgte.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "mapui/map_akropolis.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

extern void func_807245E4(void*);
extern void func_80724608(void*, s32, s32, void*);

/// Part of the mirrored player model each held-object reflection hangs off,
/// indexed by `Task::spawnArg1`.
static u8 Reflection_Data_8017FC8C[];

extern AnimationPlayRequest D_acropolis_east_elevator_hall_80185C8C;
extern EvsCommand           D_acropolis_east_elevator_hall_80185D54[];
extern EvsCommand           D_acropolis_east_elevator_hall_801860B4[];
extern EvsCommand           D_acropolis_east_elevator_hall_8018621C[];
extern TaskMessageEntry     D_acropolis_east_elevator_hall_801862F4[];
extern s32                  D_acropolis_east_elevator_hall_8018631C;

/// Name word handed to `func_80724608`: `"Player"`, followed by one
/// retained nonzero byte, preserved in the C initializer.
static const char D_acropolis_east_elevator_hall_8017D5E0[];

/// The mirror task's descriptors: entry 0 spawns the mirror itself, entry 1
/// one reflection of a held object.
static TaskDesc D_acropolis_east_elevator_hall_8017FC90[];

/// Keeps the reflection scale at this room's earlier rodata position.
///
/// 0 requires `planar_reflection_rodata.inc.c` before the shared implementation.
#define PLANAR_REFLECTION_DEFINE_SCALE_WITH_IMPLEMENTATION 0
#include "../../shared/planar_reflection.h"
/// Binds the shared beacon body to this room's public `void (Task*)` callback.
///
/// Required before `red_beacon.h` and its task fragment; gameplay imports this instance.
#define RED_BEACON_TASK acropolisEastElevatorHallRedBeaconTask
#include "../../shared/red_beacon.h"

static void func_acropolis_east_elevator_hall_8017F478(Task* task);
static void func_acropolis_east_elevator_hall_8017F4E8(Task* task);

/// Scale handed to `ScaleMatrix` to flip the reflection across X.
#include "../../shared/planar_reflection_rodata.inc.c"

/// State handlers of the room task: set-up, the per-frame tick and `taskKill`.
static const TaskFuncTable3 D_acropolis_east_elevator_hall_8017D5D4 = {
    { func_acropolis_east_elevator_hall_8017F478, func_acropolis_east_elevator_hall_8017F4E8, taskKill },
};

static AnimationSet _gAcropolisEastElevatorHallAnimation02EB8;
static AnimationSet _gAcropolisEastElevatorHallAnimation07824;
static AnimationSet _gAcropolisEastElevatorHallAnimation08600;

extern AnimationPlayRequest  D_acropolis_east_elevator_hall_80185C00;
extern AnimationPlayRequest  D_acropolis_east_elevator_hall_80185C14;
extern AnimationPlayRequest  D_acropolis_east_elevator_hall_80185C28;
extern AnimationPlayRequest  D_acropolis_east_elevator_hall_80185C3C;
extern AnimationPlayRequest  D_acropolis_east_elevator_hall_80185C50;
extern AnimationPlayRequest  D_acropolis_east_elevator_hall_80185C64;
extern AnimationPlayRequest  D_acropolis_east_elevator_hall_80185C78;
extern AnimationPlayRequest  D_acropolis_east_elevator_hall_80185C8C;
extern AnimationPlayRequest  D_acropolis_east_elevator_hall_80185CA0;
extern WorldCollisionGrid    D_acropolis_east_elevator_hall_80186838[1];
extern WorldCollisionTrigger D_acropolis_east_elevator_hall_8018685C[6];
extern WorldCollisionTrigger D_acropolis_east_elevator_hall_80186A24[7];
extern EvsSceneKey           D_acropolis_east_elevator_hall_80185CB4;
extern WorldCoordRoomLights  D_acropolis_east_elevator_hall_80187A44[1];
static s32                   _acropolisEastElevatorHallResolveTransitionMessage(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32                   _acropolisEastElevatorHallRejectKeyItemMessage(Task* task, s32 messageId, s32 itemId, s32 unusedArg);
s32                          func_acropolis_east_elevator_hall_8017F378(Task* task, s32 msgId, const void* firstArg, s32);
s32                          func_acropolis_east_elevator_hall_8017F420(Task*, s32, s32, s32);
void                         func_acropolis_east_elevator_hall_8017F450(void);

/// Key-item use request sent to the room task by the inventory menu.
enum { ACROPOLIS_EAST_ELEVATOR_HALL_MESSAGE_USE_KEY_ITEM = 0x13F1 };

#include "../../shared/planar_reflection_data.inc.c"

static TaskDesc D_acropolis_east_elevator_hall_8017FC90[2] = {
    { { { TASK_BODY_NONE, 112 } }, acropolisEastElevatorHallPlayerReflectionTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 112 } }, _planarReflectionAttachmentTask, { .value = 0 } },
};

/// Borrows this overlay's two reflection task descriptors.
///
/// Slot 0 spawns the player reflection; slot 1 spawns an attachment or equipment
/// reflection. There is no terminator. The table and its callbacks remain valid
/// while the overlay is loaded; the caller neither owns nor copies the table.
static inline TaskDesc* _planarReflectionGetTaskTable(void)
{
    return D_acropolis_east_elevator_hall_8017FC90;
}

static AnimationPackedPose _gAcropolisEastElevatorHallAnimation02EB8Bank1[4] = {
#include "assets/acropolis_east_elevator_hall_animation_02EB8_bank1.inc"
};

static AnimationPackedRotation _gAcropolisEastElevatorHallAnimation02EB8Bank4[216] = {
#include "assets/acropolis_east_elevator_hall_animation_02EB8_bank4.inc"
};

static AnimationRecord _gAcropolisEastElevatorHallAnimation02EB8Records[262] = {
#include "assets/acropolis_east_elevator_hall_animation_02EB8_records.inc"
};

static u16 _gAcropolisEastElevatorHallAnimation02EB8Indices[20] = {
#include "assets/acropolis_east_elevator_hall_animation_02EB8_indices.inc"
};

static AnimationSet _gAcropolisEastElevatorHallAnimation02EB8 = {
    _gAcropolisEastElevatorHallAnimation02EB8Records,
    _gAcropolisEastElevatorHallAnimation02EB8Indices,
    { NULL, _gAcropolisEastElevatorHallAnimation02EB8Bank1, NULL, NULL, _gAcropolisEastElevatorHallAnimation02EB8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisEastElevatorHallAnimation07824Bank1[156] = {
#include "assets/acropolis_east_elevator_hall_animation_07824_bank1.inc"
};

static AnimationPackedRotation _gAcropolisEastElevatorHallAnimation07824Bank4[1724] = {
#include "assets/acropolis_east_elevator_hall_animation_07824_bank4.inc"
};

static AnimationRecord _gAcropolisEastElevatorHallAnimation07824Records[2487] = {
#include "assets/acropolis_east_elevator_hall_animation_07824_records.inc"
};

static u16 _gAcropolisEastElevatorHallAnimation07824Indices[20] = {
#include "assets/acropolis_east_elevator_hall_animation_07824_indices.inc"
};

static AnimationSet _gAcropolisEastElevatorHallAnimation07824 = {
    _gAcropolisEastElevatorHallAnimation07824Records,
    _gAcropolisEastElevatorHallAnimation07824Indices,
    { NULL, _gAcropolisEastElevatorHallAnimation07824Bank1, NULL, NULL, _gAcropolisEastElevatorHallAnimation07824Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gAcropolisEastElevatorHallAnimation08600Bank1[21] = {
#include "assets/acropolis_east_elevator_hall_animation_08600_bank1.inc"
};

static AnimationPackedRotation _gAcropolisEastElevatorHallAnimation08600Bank4[370] = {
#include "assets/acropolis_east_elevator_hall_animation_08600_bank4.inc"
};

static AnimationRecord _gAcropolisEastElevatorHallAnimation08600Records[434] = {
#include "assets/acropolis_east_elevator_hall_animation_08600_records.inc"
};

static u16 _gAcropolisEastElevatorHallAnimation08600Indices[20] = {
#include "assets/acropolis_east_elevator_hall_animation_08600_indices.inc"
};

static AnimationSet _gAcropolisEastElevatorHallAnimation08600 = {
    _gAcropolisEastElevatorHallAnimation08600Records,
    _gAcropolisEastElevatorHallAnimation08600Indices,
    { NULL, _gAcropolisEastElevatorHallAnimation08600Bank1, NULL, NULL, _gAcropolisEastElevatorHallAnimation08600Bank4, NULL, NULL, NULL },
};

AnimationSet* D_acropolis_east_elevator_hall_80185BE8[2] = {
    NULL,
    &_gAcropolisEastElevatorHallAnimation07824,
};

AnimationSet* D_acropolis_east_elevator_hall_80185BF0[2] = {
    NULL,
    &_gAcropolisEastElevatorHallAnimation02EB8,
};

AnimationSet* D_acropolis_east_elevator_hall_80185BF8[2] = {
    NULL,
    &_gAcropolisEastElevatorHallAnimation08600,
};

AnimationPlayRequest D_acropolis_east_elevator_hall_80185C00 = { { .sets = D_acropolis_east_elevator_hall_80185BE8 }, 1, ANIMATION_BLEND_RESET, 1, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_east_elevator_hall_80185C14 = { { .sets = D_acropolis_east_elevator_hall_80185BF0 }, 1, ANIMATION_BLEND_RESET, 1, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_east_elevator_hall_80185C28 = { { .sets = D_acropolis_east_elevator_hall_80185BF8 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_east_elevator_hall_80185C3C = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_east_elevator_hall_80185C50 = { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_east_elevator_hall_80185C64 = { { .index = 0 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_east_elevator_hall_80185C78 = { { .index = 0 }, 3, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_east_elevator_hall_80185C8C = { { .index = 0 }, 4, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_east_elevator_hall_80185CA0 = { { .index = 0 }, 5, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

EvsSceneKey D_acropolis_east_elevator_hall_80185CB4 = { 1, 3, 11 };

EvsSceneKey D_acropolis_east_elevator_hall_80185CBC = { 1, 3, 21 };

ActorTransform D_acropolis_east_elevator_hall_80185CC4 = { { 1, 0, 0, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_acropolis_east_elevator_hall_80185CDC = { { 5051, 0, -1280, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_acropolis_east_elevator_hall_80185CF4 = { 0 };

ActorTransform D_acropolis_east_elevator_hall_80185D0C = { { 1937, 0, -492, 0 }, { 0, 1224, 0, 0 } };

ActorTransform D_acropolis_east_elevator_hall_80185D24 = { { 0, 0, 0, 0 }, { 0, 1640, 0, 0 } };

ActorTransform D_acropolis_east_elevator_hall_80185D3C = { { 4440, 0, -672, 0 }, { 0, 1640, 0, 0 } };

EvsCommand D_acropolis_east_elevator_hall_80185D54[36] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_acropolis_east_elevator_hall_80185CB4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_acropolis_east_elevator_hall_80185CF4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_east_elevator_hall_80185C50 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1010 }, { .message = { .pointer = &D_acropolis_east_elevator_hall_80185CDC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_east_elevator_hall_80185C3C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x51020003 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 67 }, { .value = 67 }, { .value = 78 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_acropolis_east_elevator_hall_80185D0C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1012 }, { .message = { .pointer = &D_acropolis_east_elevator_hall_80185C14 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 72 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_acropolis_east_elevator_hall_80185CF4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1012 }, { .message = { .pointer = &D_acropolis_east_elevator_hall_80185C00 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_east_elevator_hall_80185C64 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 944 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1012 }, { .message = { .pointer = &D_acropolis_east_elevator_hall_80185C28 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_east_elevator_hall_80185C78 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 80 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_east_elevator_hall_80185C8C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_east_elevator_hall_80185C3C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_acropolis_east_elevator_hall_80185D3C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 240 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_acropolis_east_elevator_hall_801860B4[15] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_acropolis_east_elevator_hall_80185D3C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_east_elevator_hall_80185C3C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_east_elevator_hall_80185C8C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_acropolis_east_elevator_hall_8017F450 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 240 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_acropolis_east_elevator_hall_8018621C[9] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_east_elevator_hall_80185C3C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_east_elevator_hall_80185CA0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_east_elevator_hall_80185C8C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskMessageEntry D_acropolis_east_elevator_hall_801862F4[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _acropolisEastElevatorHallResolveTransitionMessage },
    { ROOM_MESSAGE_COMMAND, func_acropolis_east_elevator_hall_8017F420 },
    { ACROPOLIS_EAST_ELEVATOR_HALL_MESSAGE_USE_KEY_ITEM, _acropolisEastElevatorHallRejectKeyItemMessage },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_acropolis_east_elevator_hall_8017F378 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

s32 D_acropolis_east_elevator_hall_8018631C = 0;

WorldCollisionRoomResources D_acropolis_east_elevator_hall_80186320[1] = {
    { D_acropolis_east_elevator_hall_80186838, D_acropolis_east_elevator_hall_8018685C, D_acropolis_east_elevator_hall_80186A24, NULL },
};

u8* D_acropolis_east_elevator_hall_80186330[1] = {
    gViewIdentityMap,
};

ViewCount D_acropolis_east_elevator_hall_80186334[1] = { 7 };

WorldCoordRoomLighting D_acropolis_east_elevator_hall_80186338[1] = {
    { D_acropolis_east_elevator_hall_80187A44, NULL },
};

DirectionWarpEntry D_acropolis_east_elevator_hall_80186340[1] = {
    { { { .word = 1024 }, -5248, -31, 1068 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -5248, -31, 1068 }, { 0, 0, 0, 0 }, 0x51020002, 0x51020001, DIRECTION_WARP_SOUND_NONE, 4, DIRECTION_WARP_FLAG_NONE, 502 },
};

static SVECTOR _gAcropolisEastElevatorHallCollision09278Normals[10] = {
#include "assets/acropolis_east_elevator_hall_collision_09278_normals.inc"
};

static SVECTOR _gAcropolisEastElevatorHallCollision09278Verts[66] = {
#include "assets/acropolis_east_elevator_hall_collision_09278_verts.inc"
};

static WorldCollisionGridFace _gAcropolisEastElevatorHallCollision09278Faces[33] = {
#include "assets/acropolis_east_elevator_hall_collision_09278_faces.inc"
};

static s16 _gAcropolisEastElevatorHallCollision09278Cells[90] = {
#include "assets/acropolis_east_elevator_hall_collision_09278_cells.inc"
};

#define GRID_CELL(i) (&_gAcropolisEastElevatorHallCollision09278Cells[i])
static s16* _gAcropolisEastElevatorHallCollision09278Table[8] = {
#include "assets/acropolis_east_elevator_hall_collision_09278_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_acropolis_east_elevator_hall_80186838[1] = {
    { NULL, _gAcropolisEastElevatorHallCollision09278Normals, _gAcropolisEastElevatorHallCollision09278Verts, _gAcropolisEastElevatorHallCollision09278Faces, _gAcropolisEastElevatorHallCollision09278Table, 6390, 3090, 4, 2, 4000, 33 },
};

WorldCollisionTrigger D_acropolis_east_elevator_hall_8018685C[6] = {
    { NULL, NULL, NULL, { -2977, -1024, 783, 0 }, { { -889, -1664, -2485, 0 }, { -889, 1664, -2485, 0 }, { 890, -1664, 2486, 0 }, { 890, 1664, 2486, 0 } }, { -3862, 0, 1381, 0 }, { 0, 0, 0, 0 }, 3114, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -3649, -992, 799, 0 }, { { -889, 1664, -2485, 0 }, { -889, -1664, -2485, 0 }, { 890, 1664, 2486, 0 }, { 890, -1664, 2486, 0 } }, { 3860, 0, -1383, 0 }, { 0, 0, 0, 0 }, 3114, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1311, -960, 480, 0 }, { { 0, -1664, -2640, 0 }, { 0, 1664, -2640, 0 }, { 0, -1664, 2640, 0 }, { 0, 1664, 2640, 0 } }, { -4102, 0, 0, 0 }, { 0, 0, 0, 0 }, 3114, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 704, -1056, 512, 0 }, { { 0, 1664, -2640, 0 }, { 0, -1664, -2640, 0 }, { 0, 1664, 2640, 0 }, { 0, -1664, 2640, 0 } }, { 4101, 0, 0, 0 }, { 0, 0, 0, 0 }, 3114, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4351, -1056, 127, 0 }, { { 1245, 1664, 2329, 0 }, { 1245, -1664, 2329, 0 }, { -1244, 1664, -2328, 0 }, { -1244, -1664, -2328, 0 } }, { -3618, 0, 1933, 0 }, { 0, 0, 0, 0 }, 3114, 0, 2, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3838, -1088, 222, 0 }, { { -1244, 1664, -2328, 0 }, { -1244, -1664, -2328, 0 }, { 1245, 1664, 2329, 0 }, { 1245, -1664, 2329, 0 } }, { 3616, 0, -1935, 0 }, { 0, 0, 0, 0 }, 3114, 0, 5, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_acropolis_east_elevator_hall_80186A24[7] = {
    { NULL, NULL, NULL, { -5232, -32, 992, 0 }, { { -400, 0, -1024, 0 }, { 400, 0, -1024, 0 }, { -400, 0, 1024, 0 }, { 400, 0, 1024, 0 } }, { 0, 4098, 0, 0 }, { 4096, 0, 0, 0 }, 1093, WORLD_COLLISION_TRIGGER_ACTION_WARP, 1, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2996, -128, 2774, 0 }, { { 1024, 0, -400, 0 }, { 1024, 0, 400, 0 }, { -1024, 0, -400, 0 }, { -1024, 0, 400, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, -4096, 0 }, 1093, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1216, -160, 1888, 0 }, { { 800, 0, -400, 0 }, { 800, 0, 400, 0 }, { -800, 0, -400, 0 }, { -800, 0, 400, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, -4096, 0 }, 893, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3696, -128, 1888, 0 }, { { 656, 0, -400, 0 }, { 656, 0, 400, 0 }, { -656, 0, -400, 0 }, { -656, 0, 400, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, -4096, 0 }, 768, WORLD_COLLISION_TRIGGER_ACTION_CAP, 5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3920, -96, -1472, 0 }, { { 832, 0, -624, 0 }, { 448, 0, 624, 0 }, { -896, 0, -624, 0 }, { -384, 0, 624, 0 } }, { 0, 4112, 0, 0 }, { 0, 0, 4096, 0 }, 1086, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1664, -96, 312, 0 }, { { -1840, 0, -2392, 0 }, { 1776, 0, -2488, 0 }, { -1744, 0, 2440, 0 }, { 1808, 0, 2440, 0 } }, { 0, 4109, 0, 0 }, { 4096, 0, 0, 0 }, 3050, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 0, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 4911, 0, -689, 0 }, { { -569, 0, -56, 0 }, { -209, 0, -1293, 0 }, { -174, 0, 942, 0 }, { 954, 0, 409, 0 } }, { 0, 4099, 0, 0 }, { -2276, 0, 3406, 0 }, 1305, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_acropolis_east_elevator_hall_80186C38[2] = {
    { 110, 103, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor110300ViewFigureTasks },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_acropolis_east_elevator_hall_80186C50[3] = {
    { NULL, NULL },
    { D_map_akropolis_8017ACDC, D_acropolis_east_elevator_hall_80186C38 },
    { NULL, NULL },
};

SpriteBatch D_acropolis_east_elevator_hall_80186C68[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_east_elevator_hall_80186C78[39] = {
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -88, 8, 1104, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -88, 32, 1151, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -72, 0, 1246, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -72, 24, 1175, { .fields = { 88, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -72, 48, 1175, { .fields = { 72, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -56, 0, 1306, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -56, 16, 1325, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -56, 32, 1325, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 104, -48, 1209, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 104, -16, 1243, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 104, 16, 1279, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 120, -48, 1184, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 120, -32, 1208, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 120, -8, 1241, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 88, -16, 1475, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 88, 8, 1452, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 88, 32, 1330, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 96, -16, 1253, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 96, 8, 1280, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 96, 32, 1301, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 88, -48, 1475, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 96, -48, 1300, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -16, -88, 1803, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -16, -56, 1851, { .fields = { 104, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -16, -32, 1800, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 80, -88, 1639, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 80, -64, 1667, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, -48, 1718, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 72, -32, 1727, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -32, -16, 1832, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -32, 0, 1936, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -16, -16, 1933, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -16, 0, 1937, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 0, -8, 1889, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 24, -8, 1842, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 48, -8, 1775, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 72, -8, 1734, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 64, 16, 1791, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 80, 16, 1765, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_east_elevator_hall_80186F84[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 22, 0, 0, { 1, 0 } },
    { 22, 17, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_east_elevator_hall_80186FA4[41] = {
    { 143, 0x3FC0, { .fields = { 16, 104 } }, 64, -48, 1650, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, 80, -56, 1250, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, 96, -56, 1225, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, -160, 88, 651, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, -152, 88, 679, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, -144, 80, 731, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, -136, 72, 792, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, -128, 64, 865, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -120, 64, 942, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -112, 48, 1067, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -104, 40, 1209, { .fields = { 72, 104 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -96, 32, 1398, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -88, 24, 1659, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, -80, 16, 2050, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 48 } }, -152, 40, 733, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -160, 48, 647, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 40 } }, -144, 40, 769, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 40 } }, -136, 32, 777, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, -128, 32, 919, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 40 } }, -120, 24, 971, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -112, 24, 1132, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -104, 16, 1285, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -96, 8, 1487, { .fields = { 64, 104 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -88, 0, 1722, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, -80, 0, 2066, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -112, 0, 1137, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -112, -24, 1141, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -112, -48, 1088, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, -104, 0, 1151, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, -88, -24, 2073, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -88, -40, 2085, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 24 } }, -96, -64, 1660, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -112, -64, 1203, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -120, -64, 1021, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, -96, -88, 1488, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 32 } }, -112, -96, 1075, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 32 } }, -128, -96, 858, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 32 } }, -144, -104, 740, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 32 } }, -160, -112, 655, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, -120, -120, 975, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -136, -120, 857, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_east_elevator_hall_801872D8[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 1, 0 } },
    { 3, 38, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_east_elevator_hall_801872F8[38] = {
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -16, 32, 1604, { .fields = { 24, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -40, 32, 1576, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -88, 40, 1474, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -120, 40, 1389, { .fields = { 24, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -120, 8, 1348, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -128, -16, 1312, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -128, -40, 1297, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -136, -88, 1207, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -96, -88, 1304, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -56, -88, 1363, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -16, -88, 1420, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 16, -88, 1507, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -40, 16, 1548, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -40, 0, 1514, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -40, -16, 1483, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -40, -40, 1442, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -24, -40, 1398, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, -40, 1522, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, -24, 1552, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, -8, 1584, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, 8, 1617, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -16, 24, 1603, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 16, 8, 1559, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 16, -16, 1532, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 16, -40, 1569, { .fields = { 72, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 32, -40, 1495, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 32, -16, 1583, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 32, 8, 1622, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -64, -40, 1350, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -80, -40, 1370, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -80, -24, 1393, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -80, 8, 1360, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -64, 32, 1522, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -80, 24, 1465, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -96, 16, 1370, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -96, -8, 1377, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -96, -40, 1397, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -80, -8, 1328, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_east_elevator_hall_801875F0[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 38, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_east_elevator_hall_80187608[28] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, 32, 1056, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, 48, 1002, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 48, -80, 750, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 48, -56, 775, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 88, 56, 875, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 72, -88, 650, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 72, -48, 800, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, 72, -8, 850, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 72 } }, 96, -80, 800, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 64 } }, 96, -8, 800, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, 16, 1000, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 56, 32, 950, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, 32, 1000, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 56, 48, 935, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, 56, 900, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, 72, 912, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 56, 64, 948, { .fields = { 48, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 40, -32, 800, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 40, -16, 875, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 40, 0, 941, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 48, -32, 800, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 48, -16, 850, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 48, 0, 888, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 56, -32, 760, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 56, -16, 812, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 56, 0, 820, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 48, 16, 927, { .fields = { 72, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 56, 16, 880, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_east_elevator_hall_80187838[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 28, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_east_elevator_hall_80187850[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_east_elevator_hall_80187860[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_acropolis_east_elevator_hall_80187870[7] = {
    { { .empty = D_acropolis_east_elevator_hall_80186C68 }, D_acropolis_east_elevator_hall_80186C68, NULL },
    { { .elements = D_acropolis_east_elevator_hall_80186C78 }, D_acropolis_east_elevator_hall_80186F84, NULL },
    { { .elements = D_acropolis_east_elevator_hall_80186FA4 }, D_acropolis_east_elevator_hall_801872D8, NULL },
    { { .elements = D_acropolis_east_elevator_hall_801872F8 }, D_acropolis_east_elevator_hall_801875F0, NULL },
    { { .elements = D_acropolis_east_elevator_hall_80187608 }, D_acropolis_east_elevator_hall_80187838, NULL },
    { { .empty = D_acropolis_east_elevator_hall_80187850 }, D_acropolis_east_elevator_hall_80187850, NULL },
    { { .empty = D_acropolis_east_elevator_hall_80187860 }, D_acropolis_east_elevator_hall_80187860, NULL },
};

/// Four authored point lights for model shading in every east elevator hall view.
///
/// Positions and falloff radii use world units; RGB intensities have 12 fractional
/// bits (`ONE` is full strength). The room overlay owns this writable storage:
/// coordinate updates set its view parent and cached transforms, and lighting
/// queries overwrite attenuation. Borrowed pointers expire when the room unloads.
static WorldCoordPointLight _gAcropolisEastElevatorHallPointLights[] = {
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -3250, -2350, 810 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 5324, 4915, 4505 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 4000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 110, -2350, 270 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 5324, 4915, 4505 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 3332,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3400, -2350, 180 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { 5324, 4915, 4505 },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 4000,
    },
    {
        .head = {
            .transform  = { .lighting = {
                                .composeStamp = GRAPHICS_COORD_DIRTY,
                                .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4070, -1700, -1420 } },
                                .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                                .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                                .unknown_46   = { 0, 0, 0, 0 },
                                .attenuation  = 0,
                                .parent       = NULL,
                           } },
            .color      = { ONE, ONE, ONE },
            .unknown_56 = { 0, 0 },
        },
        .inner = 500,
        .outer = 2000,
    },
};

WorldCoordRoomLights D_acropolis_east_elevator_hall_80187A44[1] = {
    { 0, NULL, ARRAY_SIZE(_gAcropolisEastElevatorHallPointLights), _gAcropolisEastElevatorHallPointLights, 0, NULL },
};

ViewCamera D_acropolis_east_elevator_hall_80187A5C[7] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 100, 0x7530, -500 } }, 617 },
    { { { { 952, 0, -3983 }, { -880, 3994, -210 }, { 3885, 905, 928 } }, { 1724, 2100, 547 } }, 230 },
    { { { { 1225, 0, 3908 }, { 221, 4089, -69 }, { -3901, 232, 1223 } }, { -3673, 1410, 858 } }, 230 },
    { { { { 1157, 0, 3928 }, { 1144, 3918, -337 }, { -3758, 1192, 1107 } }, { 274, 2690, -202 } }, 230 },
    { { { { -2087, 0, -3524 }, { -2521, 2861, 1493 }, { 2462, 2930, -1458 } }, { -2676, 3435, -318 } }, 257 },
    { { { { 950, 0, 3984 }, { 685, 4034, -163 }, { -3924, 705, 936 } }, { -5446, 1425, 1431 } }, 230 },
    { { { { -869, 0, 4002 }, { 3549, 1892, 771 }, { -1849, 3632, -401 } }, { -5320, 2510, 990 } }, 230 },
};

WorldCollisionFootstepSounds D_acropolis_east_elevator_hall_80187B58 = {
    0x10000011,
    0x10000013,
    0x10000011,
};

WorldCollisionSurfaceProperties D_acropolis_east_elevator_hall_80187B64[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_acropolis_east_elevator_hall_80187B58 },
};

WorldCollisionSurfaceProperties D_acropolis_east_elevator_hall_80187B6C[1] = { 0 };

WorldCollisionSurfaceProperties* D_acropolis_east_elevator_hall_80187B74[8] = {
    D_acropolis_east_elevator_hall_80187B64,
    D_acropolis_east_elevator_hall_80187B6C,
    D_acropolis_east_elevator_hall_80187B64,
    D_acropolis_east_elevator_hall_80187B64,
    D_acropolis_east_elevator_hall_80187B64,
    D_acropolis_east_elevator_hall_80187B64,
    D_acropolis_east_elevator_hall_80187B64,
    D_acropolis_east_elevator_hall_80187B64,
};

#include "../../shared/planar_reflection.inc.c"

void acropolisEastElevatorHallPlayerReflectionTask(Task* reflectionTask)
{
    _planarReflectionPlayerTask(reflectionTask);
}

#undef PLANAR_REFLECTION_DEFINE_SCALE_WITH_IMPLEMENTATION

/// Allows a room transition with the requested destination unchanged.
///
/// The resolve message borrows a readable eight-byte request and a writable
/// reply for this call; they may be the same record. Copies the entire record
/// for both queries and execution requests and returns 1 (transition allowed).
/// The receiving task and message ID are unused; no payload storage is retained.
static s32 _acropolisEastElevatorHallResolveTransitionMessage(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { ROOM_EVENT_RESULT_ALLOWED = 1 };

    *reply = *request;
    return ROOM_EVENT_RESULT_ALLOWED;
}

/// Refuses every key-item use request in this room.
///
/// `itemId` is the collected inventory item's ID and `unusedArg` is the unused
/// second message word. All arguments are ignored. Returns 0, which tells the
/// inventory menu that the item cannot be used here; no item is consumed.
static s32 _acropolisEastElevatorHallRejectKeyItemMessage(Task* task, s32 messageId, s32 itemId, s32 unusedArg)
{
    enum { ROOM_KEY_ITEM_RESULT_REFUSED = 0 };

    return ROOM_KEY_ITEM_RESULT_REFUSED;
}

s32 func_acropolis_east_elevator_hall_8017F378(Task* task, s32 msgId, const void* firstArg, s32 arg3)
{
    const DirectionActionRequest* request = firstArg;

    if (request->actionId == 0 && gameFlagGetNibble(0) == 0 && D_acropolis_east_elevator_hall_8018631C == 0) {
        evsStartScriptWithSkip(D_acropolis_east_elevator_hall_80185D54, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_acropolis_east_elevator_hall_801860B4);
        D_acropolis_east_elevator_hall_8018631C = 1;
        gameFlagSetNibble(0, 1);
        gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, 0);
        gameFlagSetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX, 3);
        gameFlagSetNibble(GAME_FLAG_PATIO_CAFETERIA_DOOR_STATE, 2);
        func_800E3FAC(0xA2, 2);
    }
    return 0;
}

s32 func_acropolis_east_elevator_hall_8017F420(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 2) {
        evsStartScript(D_acropolis_east_elevator_hall_8018621C, EVENT_SCRIPT_HUD_HIDE_RESTORE);
    }
    return 0;
}

void func_acropolis_east_elevator_hall_8017F450(void)
{
    Gp_StartCapSlot(0x10, 1, 0);
}

static void func_acropolis_east_elevator_hall_8017F478(Task* task)
{
    task->msgTable = D_acropolis_east_elevator_hall_801862F4;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    Gp_MsgSlot4Chain(0, 1);
    TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(0), 0x7D3, &D_acropolis_east_elevator_hall_80185C8C, 0);
    task->state++;
}

/// "Player", followed by the non-zero padding the original toolchain left.
static const char D_acropolis_east_elevator_hall_8017D5E0[8] = "Player\0\x0F";

static void func_acropolis_east_elevator_hall_8017F4E8(Task* task)
{
    if (gDisplayState.debugMode != 0) {
        func_807245E4(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER));
        if (gDisplayState.debugMode != 0) {
            func_80724608(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), -0x8C, -0x32, &D_acropolis_east_elevator_hall_8017D5E0);
        }
    }
}

/// Runs the room task's current state through a stack copy of the room's
/// three-entry state table.
void func_acropolis_east_elevator_hall_8017F55C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_east_elevator_hall_8017D5D4;
    sp.funcs[task->state](task);
}

/// Position of the first of the six effects
/// `func_acropolis_east_elevator_hall_8017F5B4` spawns in view 2; the other
/// five positions are written into the local copy in turn.
static const SVECTOR D_acropolis_east_elevator_hall_8017D5E8 = { 0x1600, -0x964, 0x540, 0 };

void func_acropolis_east_elevator_hall_8017F5B4(Task* task)
{
    GfxCoord* coord;

    coord = task->extra.coordBody->coord;
    switch (task->state) {
        case 0:
            taskSpawn(1, 0x25, 0, 0);
            taskSpawn(1, 0x25, 1, 0);
            task->state++;
            /* fallthrough */
        case 1:
            if (gGameSession->location.loc.view == 2) {
                SVECTOR vec = D_acropolis_east_elevator_hall_8017D5E8;

                effectSpawn(EFFECT_ACROPOLIS_EAST_ELEVATOR_HALL_RED_BEACON, coord, RED_BEACON_ARG(3, 0xC), &vec);
                vec.vx = 0x1600;
                vec.vy = -0x985;
                vec.vz = 0x55;
                effectSpawn(EFFECT_ACROPOLIS_EAST_ELEVATOR_HALL_RED_BEACON, coord, RED_BEACON_ARG(3, 0xC), &vec);
                vec.vx = 0x1600;
                vec.vy = -0xA81;
                vec.vz = -0x1CA;
                effectSpawn(EFFECT_ACROPOLIS_EAST_ELEVATOR_HALL_RED_BEACON, coord, RED_BEACON_ARG(4, 0x12), &vec);
                vec.vx = 0x1600;
                vec.vy = -0xA93;
                vec.vz = -0x61C;
                effectSpawn(EFFECT_ACROPOLIS_EAST_ELEVATOR_HALL_RED_BEACON, coord, RED_BEACON_ARG(4, 0x12), &vec);
                vec.vx = 0x1600;
                vec.vy = -0x460;
                vec.vz = -0x1A1;
                effectSpawn(EFFECT_ACROPOLIS_EAST_ELEVATOR_HALL_RED_BEACON, coord, RED_BEACON_ARG(4, 0x12), &vec);
                vec.vx = 0x1600;
                vec.vy = -0x449;
                vec.vz = -0x635;
                effectSpawn(EFFECT_ACROPOLIS_EAST_ELEVATOR_HALL_RED_BEACON, coord, RED_BEACON_ARG(4, 0x12), &vec);
            }
            break;
    }
}

#include "../../shared/red_beacon_task.inc.c"
#undef RED_BEACON_TASK

/// Projects a view-space translation and queues a single additive gray pixel.
///
/// `composedCoord` must have a current view-space composition. Only its cached
/// x/y/z translation is read, in signed 32-bit game-coordinate units, and each
/// component narrows to signed 16 bits before projection through `GsWSMATRIX`.
/// The GTE projection geometry must already be configured; FLAG is not tested.
/// `tileScratch` borrows one complete, word-aligned `EffectPointTileScratch`
/// block for this call. Neither input nor scratch storage is retained.
///
/// The word-aligned frame arena needs one `TILE_1` even at rejected depths and
/// one additional `DR_TPAGE` when SZ3 / 4 is at least `EFFECT_POINT_TILE_MIN_DEPTH`.
/// The current ordering table must provide 1024 depth tags; scaled depths wrap
/// into those tags. Queued packets remain live until GPU drawing completes.
/// Changes GTE state and leaves additive GPU draw mode active after the
/// accepted pixel.
static inline void _acropolisEastElevatorHallDrawPointTile(const GfxCoord* composedCoord, EffectPointTileScratch* tileScratch)
{
    enum {
        ACROPOLIS_EAST_ELEVATOR_HALL_POINT_TILE_COLOR                   = 0x80,
        ACROPOLIS_EAST_ELEVATOR_HALL_POINT_TILE_DEPTH_TO_OT_BYTES_SHIFT = 2,
    };

    TILE_1* tile;

    // Narrow the composed view position before perspective projection.
    tileScratch->viewPoint.vx = composedCoord->workm.t[0];
    tileScratch->viewPoint.vy = composedCoord->workm.t[1];
    tileScratch->viewPoint.vz = composedCoord->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&tileScratch->viewPoint);
    gte_rtps();
    // Reserve the tile even when its depth will reject it.
    tile           = gGpuPrimCursor;
    gGpuPrimCursor = tile + 1;
    setTile1(tile);
    gte_stsxy(&tile->x0);
    gte_stszotz(&tileScratch->depth);
    if (tileScratch->depth >= EFFECT_POINT_TILE_MIN_DEPTH) {
        setRGB0(tile, ACROPOLIS_EAST_ELEVATOR_HALL_POINT_TILE_COLOR, ACROPOLIS_EAST_ELEVATOR_HALL_POINT_TILE_COLOR, ACROPOLIS_EAST_ELEVATOR_HALL_POINT_TILE_COLOR);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(
                    (((u32)tileScratch->depth << gDisplayState.otDepthShift) >> ACROPOLIS_EAST_ELEVATOR_HALL_POINT_TILE_DEPTH_TO_OT_BYTES_SHIFT) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK),
                tile);
        // Prepend the draw-mode command at the same depth so blending is set before the tile.
        gpuSetPrimitiveBlendMode(tile, GPU_BLEND_ADD, tileScratch->depth);
    }
}

void acropolisEastElevatorHallPointTileTask(Task* task)
{
    EffectPointTileScratch* tileScratch;
    GfxCoord*               effectCoord;
    void*                   effectWork;

    effectCoord = task->extra.coordBody->coord;
    effectWork  = task->spawnArg2.pointer;
    actorRenderComposeCoord(effectCoord);
    tileScratch = SCRATCH_STACK_RESERVE_BLOCK(EffectPointTileScratch);
    _acropolisEastElevatorHallDrawPointTile(effectCoord, tileScratch);
    SCRATCH_STACK_RELEASE_BLOCK(EffectPointTileScratch);
    effectKillTask(effectWork, task);
}
