#include "rooms/dryfield_night_driveway.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "actors/task_tables.h"

#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/area_transitions.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_dryfield_full.h"

#include "overlay.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/glow_draw.h"
#include "../../shared/room_events.h"
#include "../../shared/dryfield_driveway.h"

/// Set once the driveway event has been spawned.
extern u8 gDrivewayEventSpawned;

/// The clips two of the room's scene scripts add to the player's animation
/// bank, with the two play requests stored after them.
///
/// Each of those scripts sends the player a copy request for the first ten
/// words of this storage before it plays an extended clip. That is more than
/// the clip table holds: the three set pointers and the NULL after them occupy
/// extended ids 47-50, and the five words of `unusedPlay` and the bank selector
/// of `firstClipPlay` are written into the bank after them. The scripts play
/// only ids 47-49 and one of the bank's own clips on the player, so none of
/// those following words is played as a clip.
///
/// The two requests are part of this object only because the copied span runs
/// through the first and one word into the second; the requests for ids 48 and
/// 49 follow as separate objects. The storage is only read.
typedef union {
    struct {
        AnimationSet*        sets[4];       // Player clips for extended ids 47-49, then the NULL that ends the table
        AnimationPlayRequest unusedPlay;    // Same request as `firstClipPlay`; nothing refers to it
        AnimationPlayRequest firstClipPlay; // Starts extended id 47 without a blend; the first extended clip each script plays
    } data;                                 // The records by name
    s32 words[14];                          // The same storage as the copy reads it; the last four words lie beyond the copied span
} _DryfieldNightDrivewayAnimationBankExtensionStorage;
STATIC_ASSERT_SIZEOF(_DryfieldNightDrivewayAnimationBankExtensionStorage, 56);

extern _DryfieldNightDrivewayAnimationBankExtensionStorage D_dryfield_night_driveway_8017F8C4;

/// Descriptor of the room's event task, which the event gate spawns.
extern TaskDesc gRoomEventStagedTaskDesc;

/// Descriptor table of two tasks (`drivewayBlackoutTask`,
/// `drivewayCutsceneTask`), ended by a 0xFFFF entry; the
/// event gate spawns entry 1.
extern TaskDesc gDrivewayCutsceneTasks[];

/// Blocks the room's tasks hand to `func_800E8614` / `func_800E8634`.
extern EvsCommand gDrivewayCutsceneScript[];
extern EvsCommand gDrivewayBlackoutScript[];
extern EvsCommand gDrivewayBlackoutTail[];
extern EvsCommand D_dryfield_night_driveway_8017F998[];
extern EvsCommand D_dryfield_night_driveway_8017FB00[];

/// Message table the room task installs at `Task::msgTable`.
extern TaskMessageEntry D_dryfield_night_driveway_8017F7A4[];

/// Three pairs of beam end points, back to back: the first pair at `B0[0]`,
/// the second at `B0[2]` and the third at `D0`. `D0` is its own symbol because
/// the code reaches the third pair by name while it indexes the array for the
/// second.

/// Spawn argument of the helper task 0x31 the event task starts.
extern RoomFadeStorage gRoomEventFade;

/// The message and the event the event gate latched for the event task, and
/// the flag it sets when it latches one.
extern RoomEventMsg     gRoomEventStagedMsg;
extern RoomLatchedEvent gRoomEventLatched;

static void func_dryfield_night_driveway_8017DCFC(Task* arg0);
static void func_dryfield_night_driveway_8017DD7C(Task* task);

extern WorldCollisionGrid         D_dryfield_night_driveway_80180C0C[1];
extern WorldCollisionOccluder     D_dryfield_night_driveway_80181FFC[2];
extern WorldCollisionTrigger      D_dryfield_night_driveway_801818E8[6];
extern WorldCollisionTrigger      D_dryfield_night_driveway_80181DC8[4];
extern WorldCoordRoomAmbientEntry D_dryfield_night_driveway_80182074[11];
extern WorldCoordRoomLights       D_dryfield_night_driveway_80181DB0[1];

extern AnimationPlayRequest     D_dryfield_night_driveway_8017F380;
extern AnimationPlayRequest     D_dryfield_night_driveway_8017F3A8;
static AnimationSet             _gDryfieldNightDrivewayAnimation01870;
static AnimationSet             _gDryfieldNightDrivewayAnimation01A84;
static AnimationSet             _gDryfieldNightDrivewayAnimation01D64;
extern AnimationBankCopyRequest D_dryfield_night_driveway_8017F378;
s32                             func_dryfield_night_driveway_8017DCE4(Task*, s32, s32, s32);
s32                             func_dryfield_night_driveway_8017DCEC(Task*, s32, s32, s32);
s32                             func_dryfield_night_driveway_8017DCF4(Task*, s32, s32, s32);
void                            func_dryfield_night_driveway_8017DC6C(s32);
void                            func_dryfield_night_driveway_8017DC88(u8);

TaskDesc gRoomEventStagedTaskDesc = { { { TASK_BODY_NONE, 32 } }, roomEventStagedTask, { .value = 0 } };

static AnimationPackedPose _gDryfieldNightDrivewayAnimation0150CBank1[10] = {
#include "assets/dryfield_night_driveway_animation_0150C_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightDrivewayAnimation0150CBank4[95] = {
#include "assets/dryfield_night_driveway_animation_0150C_bank4.inc"
};

static AnimationRecord _gDryfieldNightDrivewayAnimation0150CRecords[139] = {
#include "assets/dryfield_night_driveway_animation_0150C_records.inc"
};

static u16 _gDryfieldNightDrivewayAnimation0150CIndices[20] = {
#include "assets/dryfield_night_driveway_animation_0150C_indices.inc"
};

static AnimationSet _gDryfieldNightDrivewayAnimation0150C = {
    _gDryfieldNightDrivewayAnimation0150CRecords,
    _gDryfieldNightDrivewayAnimation0150CIndices,
    { NULL, _gDryfieldNightDrivewayAnimation0150CBank1, NULL, NULL, _gDryfieldNightDrivewayAnimation0150CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightDrivewayAnimation01870Bank1[6] = {
#include "assets/dryfield_night_driveway_animation_01870_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightDrivewayAnimation01870Bank4[75] = {
#include "assets/dryfield_night_driveway_animation_01870_bank4.inc"
};

static AnimationRecord _gDryfieldNightDrivewayAnimation01870Records[104] = {
#include "assets/dryfield_night_driveway_animation_01870_records.inc"
};

static u16 _gDryfieldNightDrivewayAnimation01870Indices[20] = {
#include "assets/dryfield_night_driveway_animation_01870_indices.inc"
};

static AnimationSet _gDryfieldNightDrivewayAnimation01870 = {
    _gDryfieldNightDrivewayAnimation01870Records,
    _gDryfieldNightDrivewayAnimation01870Indices,
    { NULL, _gDryfieldNightDrivewayAnimation01870Bank1, NULL, NULL, _gDryfieldNightDrivewayAnimation01870Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightDrivewayAnimation01A84Bank1[2] = {
#include "assets/dryfield_night_driveway_animation_01A84_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightDrivewayAnimation01A84Bank4[24] = {
#include "assets/dryfield_night_driveway_animation_01A84_bank4.inc"
};

static AnimationRecord _gDryfieldNightDrivewayAnimation01A84Records[83] = {
#include "assets/dryfield_night_driveway_animation_01A84_records.inc"
};

static u16 _gDryfieldNightDrivewayAnimation01A84Indices[20] = {
#include "assets/dryfield_night_driveway_animation_01A84_indices.inc"
};

static AnimationSet _gDryfieldNightDrivewayAnimation01A84 = {
    _gDryfieldNightDrivewayAnimation01A84Records,
    _gDryfieldNightDrivewayAnimation01A84Indices,
    { NULL, _gDryfieldNightDrivewayAnimation01A84Bank1, NULL, NULL, _gDryfieldNightDrivewayAnimation01A84Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldNightDrivewayAnimation01D64Bank1[5] = {
#include "assets/dryfield_night_driveway_animation_01D64_bank1.inc"
};

static AnimationPackedRotation _gDryfieldNightDrivewayAnimation01D64Bank4[60] = {
#include "assets/dryfield_night_driveway_animation_01D64_bank4.inc"
};

static AnimationRecord _gDryfieldNightDrivewayAnimation01D64Records[89] = {
#include "assets/dryfield_night_driveway_animation_01D64_records.inc"
};

static u16 _gDryfieldNightDrivewayAnimation01D64Indices[20] = {
#include "assets/dryfield_night_driveway_animation_01D64_indices.inc"
};

static AnimationSet _gDryfieldNightDrivewayAnimation01D64 = {
    _gDryfieldNightDrivewayAnimation01D64Records,
    _gDryfieldNightDrivewayAnimation01D64Indices,
    { NULL, _gDryfieldNightDrivewayAnimation01D64Bank1, NULL, NULL, _gDryfieldNightDrivewayAnimation01D64Bank4, NULL, NULL, NULL },
};

TaskDesc gDrivewayCutsceneTasks[3] = {
    { { { TASK_BODY_NONE, 32 } }, drivewayBlackoutTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, drivewayCutsceneTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

AnimationSet* D_dryfield_night_driveway_8017F370[2] = {
    &_gDryfieldNightDrivewayAnimation0150C,
    NULL,
};

AnimationBankCopyRequest D_dryfield_night_driveway_8017F378 = { { .sets = D_dryfield_night_driveway_8017F370 }, ARRAY_SIZE(D_dryfield_night_driveway_8017F370) };

AnimationPlayRequest D_dryfield_night_driveway_8017F380 = { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_night_driveway_8017F394 = { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 7, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_night_driveway_8017F3A8 = { { .index = 1 }, 32, ANIMATION_BLEND_INTERPOLATE, 5, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_dryfield_night_driveway_8017F3BC = { { -1067, 0, 1407, 0 }, { 0, 0, 0, 0 } };

EvsCommand gDrivewayCutsceneScript[14] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_dryfield_night_driveway_8017F378 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_driveway_8017F3A8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x5219000C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_dryfield_night_driveway_8017DC6C }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_driveway_8017F380 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = Gp_ArmStateF0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

AnimationPlayRequest D_dryfield_night_driveway_8017F524 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_driveway_8017F538 = { { .index = 1 }, 24, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

EvsCommand gDrivewayBlackoutScript[16] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 3 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = func_dryfield_night_driveway_8017DC88 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = SetDispMask }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_driveway_8017F524 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x52190009 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = drivewaySetViewDirty }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_driveway_8017F538 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gDrivewayBlackoutTail[9] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = drivewaySetViewDirty }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskMessageEntry D_dryfield_night_driveway_8017F7A4[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, drivewayResolveEvent },
    { 5105, func_dryfield_night_driveway_8017DCE4 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_dryfield_night_driveway_8017DCF4 },
    { ROOM_MESSAGE_COMMAND, func_dryfield_night_driveway_8017DCEC },
    { ROOM_MESSAGE_SOUND, drivewayScriptSound },
    { TASK_MESSAGE_TABLE_END, NULL },
};

ActorTransform D_dryfield_night_driveway_8017F7D4 = { { -700, 0, 1320, 0 }, { 0, 1365, 0, 0 } };

ActorTransform D_dryfield_night_driveway_8017F7EC = { { -700, 0, 1320, 0 }, { 0, 3072, 0, 0 } };

ActorTransform D_dryfield_night_driveway_8017F804 = { { 3300, 0, 790, 0 }, { 0, 3072, 0, 0 } };

ActorTransform D_dryfield_night_driveway_8017F81C = { { 100, 0, 790, 0 }, { 0, 3413, 0, 0 } };

ActorTransform D_dryfield_night_driveway_8017F834 = { { -3000, 0, 790, 0 }, { 0, 3413, 0, 0 } };

ActorTransform D_dryfield_night_driveway_8017F84C = { { -9527, 0, -1500, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_dryfield_night_driveway_8017F864 = { { -9527, 0, -1500, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_dryfield_night_driveway_8017F87C = { { -3800, 0, -1500, 0 }, { 0, 3072, 0, 0 } };

ActorTransform D_dryfield_night_driveway_8017F894 = { { -8800, 0, -1500, 0 }, { 0, 3072, 0, 0 } };

ActorTransform D_dryfield_night_driveway_8017F8AC = { { -3800, 0, -1500, 0 }, { 0, 3413, 0, 0 } };

_DryfieldNightDrivewayAnimationBankExtensionStorage D_dryfield_night_driveway_8017F8C4 = { .data = { { &_gDryfieldNightDrivewayAnimation01870, &_gDryfieldNightDrivewayAnimation01A84, &_gDryfieldNightDrivewayAnimation01D64, NULL }, { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE } } };

AnimationPlayRequest D_dryfield_night_driveway_8017F8FC = { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_dryfield_night_driveway_8017F910 = { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationBankCopyRequest D_dryfield_night_driveway_8017F924 = { { .words = D_dryfield_night_driveway_8017F8C4.words }, 10 };

AnimationPlayRequest D_dryfield_night_driveway_8017F92C = { { .index = 6 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_driveway_8017F940 = { { .index = 6 }, 38, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_driveway_8017F954 = { { .index = 6 }, 7, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_driveway_8017F968 = { { .index = 6 }, 8, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_night_driveway_8017F97C = { { .index = 6 }, 9, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

GameActorMoveAnim D_dryfield_night_driveway_8017F990 = { 4, 9 };

EvsCommand D_dryfield_night_driveway_8017F998[15] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_dryfield_night_driveway_8017F7EC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_dryfield_night_driveway_8017F834 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_driveway_8017F524 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_dryfield_night_driveway_8017FB00[51] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 5 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_driveway_8017F524 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_driveway_8017F92C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_dryfield_night_driveway_8017F924 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_dryfield_night_driveway_8017F7D4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_dryfield_night_driveway_8017F804 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x5319000D }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1019 }, { .message = { .pointer = &D_dryfield_night_driveway_8017F81C } }, { .message = { .pointer = &D_dryfield_night_driveway_8017F990 } } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_driveway_8017F8C4.data.firstClipPlay }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_driveway_8017F8FC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1006 }, { .message = { .pointer = &D_dryfield_night_driveway_8017F81C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_driveway_8017F954 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_driveway_8017F97C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_driveway_8017F940 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_driveway_8017F968 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1019 }, { .message = { .pointer = &D_dryfield_night_driveway_8017F834 } }, { .message = { .pointer = &D_dryfield_night_driveway_8017F990 } } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_driveway_8017F910 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1006 }, { .message = { .pointer = &D_dryfield_night_driveway_8017F7EC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_dryfield_night_driveway_8017FFC8[14] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_dryfield_night_driveway_8017F864 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_dryfield_night_driveway_8017F8AC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_driveway_8017F524 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_dryfield_night_driveway_80180118[49] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 5 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_driveway_8017F524 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_driveway_8017F92C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_dryfield_night_driveway_8017F924 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_dryfield_night_driveway_8017F84C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_dryfield_night_driveway_8017F87C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1019 }, { .message = { .pointer = &D_dryfield_night_driveway_8017F894 } }, { .message = { .pointer = &D_dryfield_night_driveway_8017F990 } } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1006 }, { .message = { .pointer = &D_dryfield_night_driveway_8017F894 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_driveway_8017F954 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_driveway_8017F97C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_driveway_8017F8C4.data.firstClipPlay }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_driveway_8017F8FC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_driveway_8017F940 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_driveway_8017F968 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1019 }, { .message = { .pointer = &D_dryfield_night_driveway_8017F8AC } }, { .message = { .pointer = &D_dryfield_night_driveway_8017F990 } } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_night_driveway_8017F910 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1006 }, { .message = { .pointer = &D_dryfield_night_driveway_8017F864 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

SVECTOR D_dryfield_night_driveway_801805B0[6] = {
    { -0x27B0, -2800, -1210, 0 },
    { -0x27B0, -3100, -1640, 0 },
    { -3960, -2070, 890, 0 },
    { -3960, -2070, 140, 0 },
    { 5740, -2160, 440, 0 },
    { 5740, -2430, 900, 0 },
};

WorldCoordRoomLighting D_dryfield_night_driveway_801805E0[2] = {
    { D_dryfield_night_driveway_80181DB0, D_dryfield_night_driveway_80182074 },
    { D_dryfield_night_driveway_80181DB0, D_dryfield_night_driveway_80182074 },
};

WorldCollisionRoomResources D_dryfield_night_driveway_801805F0[2] = {
    { D_dryfield_night_driveway_80180C0C, D_dryfield_night_driveway_801818E8, D_dryfield_night_driveway_80181DC8, D_dryfield_night_driveway_80181FFC },
    { D_dryfield_night_driveway_80180C0C, D_dryfield_night_driveway_801818E8, D_dryfield_night_driveway_80181DC8, D_dryfield_night_driveway_80181FFC },
};

u8 D_dryfield_night_driveway_80180610[12] = {
    1,
    9,
    10,
    4,
    5,
    6,
    4,
    8,
    2,
    3,
    0,
    0,
};

u8* D_dryfield_night_driveway_8018061C[2] = {
    gViewIdentityMap,
    D_dryfield_night_driveway_80180610,
};

ViewCount D_dryfield_night_driveway_80180624[2] = { 10, 10 };

DirectionWarpEntry D_dryfield_night_driveway_80180628[4] = {
    { { { .word = 1024 }, -3400, 0, 600 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -3400, 0, 600 }, { 0, 0, 0, 0 }, 0x53190004, 0x53190003, DIRECTION_WARP_SOUND_NONE, 4, DIRECTION_WARP_FLAG_NONE, 474 },
    { { { .word = ACTOR_SPAWN_YAW_FACE_TRANSITION_POINT_ALT }, -1300, 0, 1700 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -3400, 0, 600 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 4, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 1024 }, -9500, 4, -1418 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -9500, 4, -1418 }, { 0, 0, 0, 0 }, 0x53190002, 0x53190001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, 482 },
    { { { .word = ACTOR_SPAWN_YAW_FACE_TRANSITION_POINT }, -1300, 0, 1700 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -3400, 0, 600 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 4, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gDryfieldNightDrivewayCollision0364CNormals[13] = {
#include "assets/dryfield_night_driveway_collision_0364C_normals.inc"
};

static SVECTOR _gDryfieldNightDrivewayCollision0364CVerts[64] = {
#include "assets/dryfield_night_driveway_collision_0364C_verts.inc"
};

static WorldCollisionGridFace _gDryfieldNightDrivewayCollision0364CFaces[23] = {
#include "assets/dryfield_night_driveway_collision_0364C_faces.inc"
};

static s16 _gDryfieldNightDrivewayCollision0364CCells[154] = {
#include "assets/dryfield_night_driveway_collision_0364C_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldNightDrivewayCollision0364CCells[i])
static s16* _gDryfieldNightDrivewayCollision0364CTable[21] = {
#include "assets/dryfield_night_driveway_collision_0364C_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_night_driveway_80180C0C[1] = {
    { NULL, _gDryfieldNightDrivewayCollision0364CNormals, _gDryfieldNightDrivewayCollision0364CVerts, _gDryfieldNightDrivewayCollision0364CFaces, _gDryfieldNightDrivewayCollision0364CTable, 0x2B16, 5210, 7, 3, 4000, 23 },
};

ViewCamera D_dryfield_night_driveway_80180C30[10] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 1950, 0x7530, -2670 } }, 380 },
    { { { { -651, 0, 4043 }, { -292, 4085, -47 }, { -4033, -296, -649 } }, { 4393, 820, 1267 } }, 230 },
    { { { { -852, 0, 4006 }, { 449, 4070, 95 }, { -3980, 459, -846 } }, { 425, 1344, 1172 } }, 230 },
    { { { { -1441, 0, 3833 }, { 939, 3971, 353 }, { -3717, 1003, -1397 } }, { -2544, 1724, -2548 } }, 230 },
    { { { { -781, 0, -4020 }, { -1703, 3710, 331 }, { 3642, 1735, -707 } }, { 1820, 2220, -2312 } }, 230 },
    { { { { 287, 0, 4085 }, { 2747, 3031, -193 }, { -3024, 2754, 213 } }, { 29, 1719, -2342 } }, 230 },
    { { { { -1441, 0, 3833 }, { 939, 3971, 353 }, { -3717, 1003, -1397 } }, { -2544, 1724, -2548 } }, 230 },
    { { { { 1822, 0, -3668 }, { -2650, 2831, -1317 }, { 2535, 2959, 1260 } }, { 1959, 2418, -462 } }, 289 },
    { { { { -651, 0, 4043 }, { -292, 4085, -47 }, { -4033, -296, -649 } }, { 4393, 820, 1267 } }, 230 },
    { { { { -852, 0, 4006 }, { 449, 4070, 95 }, { -3980, 459, -846 } }, { 425, 1344, 1172 } }, 230 },
};

SpriteBatch D_dryfield_night_driveway_80180D98[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_driveway_80180DA8[9] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -8, 112, 375, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 32, 40, 978, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -32, 88, 375, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -48, 80, 375, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -56, 88, 375, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 152, 56, 54, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 72 } }, 128, 48, 62, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, 104, 40, 81, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 88 } }, 64, 32, 131, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_driveway_80180E5C[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 1, 0 } },
    { 5, 4, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_driveway_80180E7C[17] = {
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -160, 88, 240, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 40 } }, -120, 80, 240, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 32 } }, -56, 88, 240, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 0, 96, 240, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 16 } }, 24, 104, 240, { .fields = { 0, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 96, 112, 206, { .fields = { 8, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -56, 72, 500, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 8, 104, 500, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 24, 104, 500, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -40, 64, 500, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -24, 56, 500, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 0, 72, 500, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 40, 0, 900, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 40, 48, 903, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, 48, 0, 910, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 144 } }, 56, -120, 918, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 48, -48, 931, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_driveway_80180FD0[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 6, 0, 0, { 0, 0 } },
    { 6, 6, 0, 0, { 3, 0 } },
    { 12, 1, 0, 0, { 2, 0 } },
    { 13, 2, 0, 0, { 4, 0 } },
    { 15, 2, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_dryfield_night_driveway_80181008[2] = {
    { { 0, 0, 222, 239 }, 925 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteSource D_dryfield_night_driveway_8018101C[54] = {
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -64, -16, 1125, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -80, -16, 1075, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -96, -24, 1025, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 48 } }, -160, -120, 835, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 72 } }, -160, -72, 860, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 64 } }, -160, 0, 910, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 128 } }, -40, -120, 1800, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 128 } }, -8, -120, 1825, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 128 } }, 24, -120, 1825, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 56, -80, 1000, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 48, -40, 1000, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 104, -80, 1000, { .fields = { 16, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 104, -16, 1000, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 96, 0, 1000, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 56, -96, 1000, { .fields = { 32, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 96, -96, 1000, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 40, -88, 825, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 40, -16, 875, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 112, -88, 800, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 112, -64, 825, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 104, -16, 825, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 104, 0, 850, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 96, 48, 850, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, 40, -104, 750, { .fields = { 24, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, 88, -104, 750, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 48, -104, 762, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 80, -104, 762, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 112, -104, 762, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, -80, 851, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, -56, 837, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 72, -40, 845, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 72, -64, 845, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 72, -56, 845, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, -16, 1000, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 56, -24, 845, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 64, -40, 845, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 88, -56, 837, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 80, -32, 837, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, 80, -16, 837, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 80, -96, 845, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 80, 112, 507, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 120, 112, 507, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, 88, 48, 507, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 120, 56, 507, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 40, 8, 900, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 56, 16, 875, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 72, 16, 875, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 88, 16, 875, { .fields = { 0, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 104, 8, 850, { .fields = { 0, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 88, 0, 1064, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 72, 0, 1064, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 104, 8, 1064, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 56, 0, 1064, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 40, 0, 1064, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_driveway_80181454[13] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 1, 0 } },
    { 3, 0, 0, 0, { 6, 0 } },
    { 3, 3, 0, 0, { 2, 0 } },
    { 6, 3, 0, 0, { 7, 0 } },
    { 9, 7, 0, 0, { 5, 0 } },
    { 16, 9, 0, 0, { 8, 0 } },
    { 25, 3, 0, 0, { 4, 0 } },
    { 28, 12, 0, 0, { 9, 0 } },
    { 40, 4, 0, 0, { 0, 0 } },
    { 44, 5, 0, 0, { 10, 0 } },
    { 49, 5, 0, 0, { 3, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_driveway_801814BC[12] = {
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 0, 969, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -88, 8, 948, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -88, 16, 888, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -88, 24, 895, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -88, 32, 890, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -88, 40, 885, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -88, 48, 880, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -88, 56, 880, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -96, 32, 650, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -64, 80, 650, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -88, 32, 650, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -88, 72, 681, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_driveway_801815AC[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 1, 0 } },
    { 8, 4, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_driveway_801815CC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_driveway_801815DC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_driveway_801815EC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_driveway_801815FC[9] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -8, 112, 375, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 32, 40, 978, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -32, 88, 375, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -48, 80, 375, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -56, 88, 375, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 152, 56, 54, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 72 } }, 128, 48, 62, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, 104, 40, 81, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 88 } }, 64, 32, 131, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_driveway_801816B0[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 1, 0 } },
    { 5, 4, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_driveway_801816D0[17] = {
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -160, 88, 240, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 40 } }, -120, 80, 240, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 32 } }, -56, 88, 240, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 0, 96, 240, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 16 } }, 24, 104, 240, { .fields = { 0, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 96, 112, 206, { .fields = { 8, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -56, 72, 500, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 8, 104, 500, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 24, 104, 500, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -40, 64, 500, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -24, 56, 500, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 0, 72, 500, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 40, 0, 900, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 40, 48, 903, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, 48, 0, 910, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 144 } }, 56, -120, 918, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 48, -48, 931, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_driveway_80181824[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 6, 0, 0, { 0, 0 } },
    { 6, 6, 0, 0, { 3, 0 } },
    { 12, 1, 0, 0, { 2, 0 } },
    { 13, 2, 0, 0, { 4, 0 } },
    { 15, 2, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_dryfield_night_driveway_8018185C[2] = {
    { { 0, 0, 222, 239 }, 925 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteView D_dryfield_night_driveway_80181870[10] = {
    { { .empty = D_dryfield_night_driveway_80180D98 }, D_dryfield_night_driveway_80180D98, NULL },
    { { .elements = D_dryfield_night_driveway_80180DA8 }, D_dryfield_night_driveway_80180E5C, NULL },
    { { .elements = D_dryfield_night_driveway_80180E7C }, D_dryfield_night_driveway_80180FD0, D_dryfield_night_driveway_80181008 },
    { { .elements = D_dryfield_night_driveway_8018101C }, D_dryfield_night_driveway_80181454, NULL },
    { { .elements = D_dryfield_night_driveway_801814BC }, D_dryfield_night_driveway_801815AC, NULL },
    { { .empty = D_dryfield_night_driveway_801815CC }, D_dryfield_night_driveway_801815CC, NULL },
    { { .empty = D_dryfield_night_driveway_801815DC }, D_dryfield_night_driveway_801815DC, NULL },
    { { .empty = D_dryfield_night_driveway_801815EC }, D_dryfield_night_driveway_801815EC, NULL },
    { { .elements = D_dryfield_night_driveway_801815FC }, D_dryfield_night_driveway_801816B0, NULL },
    { { .elements = D_dryfield_night_driveway_801816D0 }, D_dryfield_night_driveway_80181824, D_dryfield_night_driveway_8018185C },
};

WorldCollisionTrigger D_dryfield_night_driveway_801818E8[6] = {
    { NULL, NULL, NULL, { -6369, -1536, -1504, 0 }, { { 0, -2560, 1024, 0 }, { 0, -2560, -1024, 0 }, { 0, 2560, 1024, 0 }, { 0, 2560, -1024, 0 } }, { -4095, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2757, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6816, -1472, -1568, 0 }, { { 0, -2496, -1024, 0 }, { 0, -2496, 1024, 0 }, { 0, 2496, -1024, 0 }, { 0, 2496, 1024, 0 } }, { 4096, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2697, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2770, -1600, -856, 0 }, { { -1521, -2624, 0, 0 }, { 1522, -2624, 1, 0 }, { -1521, 2624, 0, 0 }, { 1522, 2624, 1, 0 } }, { 1, 0, -4103, 0 }, { 0, 0, 4096, 0 }, 3029, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2675, -1376, -1017, 0 }, { { 1554, -2400, 1, 0 }, { -1553, -2400, 0, 0 }, { 1554, 2400, 1, 0 }, { -1553, 2400, 0, 0 } }, { -3, 0, 4097, 0 }, { 0, 0, 4096, 0 }, 2850, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 294, -1520, 1474, 0 }, { { -435, -2544, 2215, 0 }, { 435, -2544, -2214, 0 }, { -435, 2544, 2215, 0 }, { 435, 2544, -2214, 0 } }, { -4034, 0, -793, 0 }, { 0, 0, 4096, 0 }, 3396, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -26, -1488, 1301, 0 }, { { 435, -2512, -2213, 0 }, { -435, -2512, 2214, 0 }, { 435, 2512, -2213, 0 }, { -435, 2512, 2214, 0 } }, { 4031, 0, 792, 0 }, { 0, 0, 4096, 0 }, 3367, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCoordPointLight D_dryfield_night_driveway_80181AB0[8] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -0x2950, -2500, -1400 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3686, 3276 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -7437, -2500, -1400 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3686, 3276 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -4803, -2500, -1400 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3686, 3276 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1618, -1633, -537 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3686, 3276 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -3352, -2500, 705 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3686, 4096 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1510, -2500, 2719 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1670, -2500, 2899 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4860, -2500, 705 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 3686, 4096 }, { 0, 0 } }, 1000, 3000 },
};

WorldCoordRoomLights D_dryfield_night_driveway_80181DB0[1] = {
    { 0, NULL, ARRAY_SIZE(D_dryfield_night_driveway_80181AB0), D_dryfield_night_driveway_80181AB0, 0, NULL },
};

WorldCollisionTrigger D_dryfield_night_driveway_80181DC8[4] = {
    { NULL, NULL, NULL, { -3680, -48, 624, 0 }, { { -352, 0, -592, 0 }, { 352, 0, -592, 0 }, { -352, 0, 592, 0 }, { 352, 0, 592, 0 } }, { 0, 4107, 0, 0 }, { 4096, 0, 0, 0 }, 686, WORLD_COLLISION_TRIGGER_ACTION_WARP, 23, 17, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1488, -48, 2496, 0 }, { { -1200, 0, -1216, 0 }, { 1200, 0, -1216, 0 }, { -1200, 0, 1024, 0 }, { 1200, 0, 1024, 0 } }, { 0, 4096, 0, 0 }, { 4096, 0, 0, 0 }, 1707, WORLD_COLLISION_TRIGGER_ACTION_WARP, 32, 33, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -9664, -48, -1440, 0 }, { { -352, 0, -352, 0 }, { 352, 0, -352, 0 }, { -352, 0, 352, 0 }, { 352, 0, 352, 0 } }, { 0, 4102, 0, 0 }, { 4096, 0, 0, 0 }, 497, WORLD_COLLISION_TRIGGER_ACTION_WARP, 2, 55, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5984, -64, 1344, 0 }, { { -352, 0, -1024, 0 }, { 352, 0, -1024, 0 }, { -352, 0, 1024, 0 }, { 352, 0, 1024, 0 } }, { 0, 4094, 0, 0 }, { -4096, 0, 0, 0 }, 1078, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_dryfield_night_driveway_80181EF8[2] = {
    { 37, 37, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_103700_80139DAC },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_night_driveway_80181F10[2] = {
    { 37, 37, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_103700_80139DAC },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_night_driveway_80181F28[3] = {
    { 15, 15, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &Actor01500_D0A008 },
    { 25, 25, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, Actor02500_D05B88 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_dryfield_night_driveway_80181F4C[22] = {
    { NULL, NULL },
    { D_map_dryfield_full_8017C5B8, D_dryfield_night_driveway_80181EF8 },
    { NULL, NULL },
    { D_map_dryfield_full_8017C698, D_dryfield_night_driveway_80181F10 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_dryfield_full_8017C768, D_dryfield_night_driveway_80181F28 },
};

WorldCollisionOccluder D_dryfield_night_driveway_80181FFC[2] = {
    { NULL, NULL, { -5648, -2128, 1792, 0 }, { { -1648, 3152, 2624, 0 }, { 1648, 3152, -2624, 0 }, { -1648, -3152, 2624, 0 }, { 1648, -3152, -2624, 0 } }, { 3483, 0, 2187, 0 }, 4404, 1, 0 },
    { NULL, NULL, { 1824, -2160, -2656, 0 }, { { -1648, 3184, 2624, 0 }, { 1648, 3184, -2624, 0 }, { -1648, -3184, 2624, 0 }, { 1648, -3184, -2624, 0 } }, { 3478, 0, 2184, 0 }, 4434, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

WorldCoordRoomAmbientEntry D_dryfield_night_driveway_80182074[11] = {
    { .viewCount = ARRAY_SIZE(D_dryfield_night_driveway_80182074) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 618, 618, 616, 617 } },
    { .color = { 618, 618, 618, 618 } },
    { .color = { 615, 617, 616, 616 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
};

WorldCollisionFootstepSounds D_dryfield_night_driveway_801820CC = {
    0x1000001D,
    0x1000001F,
    0x1000001D,
};

WorldCollisionSurfaceProperties D_dryfield_night_driveway_801820D8[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_night_driveway_801820E0[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_night_driveway_801820E8[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_night_driveway_801820CC },
};

WorldCollisionSurfaceProperties* D_dryfield_night_driveway_801820F0[8] = {
    D_dryfield_night_driveway_801820D8,
    D_dryfield_night_driveway_801820E0,
    D_dryfield_night_driveway_801820E8,
    D_dryfield_night_driveway_801820D8,
    D_dryfield_night_driveway_801820D8,
    D_dryfield_night_driveway_801820D8,
    D_dryfield_night_driveway_801820D8,
    D_dryfield_night_driveway_801820D8,
};

RoomFadeStorage gRoomEventFade = { 0 };

RoomEventMsg gRoomEventStagedMsg = { 0 };

u8 gDrivewayEventSpawned = 0;

/// Three bytes stored after the flag; nothing references them.
u8 D_dryfield_night_driveway_80182121 = 2;

u8 D_dryfield_night_driveway_80182122 = 0;

u8 D_dryfield_night_driveway_80182123 = 0;

RoomLatchedEvent gRoomEventLatched = { 0 };

#include "../../shared/room_event_staged_task.inc.c"

/// The room task's three states: set up, idle, kill.
static const TaskFuncTable3 D_dryfield_night_driveway_8017D5D8 = {
    { func_dryfield_night_driveway_8017DCFC, func_dryfield_night_driveway_8017DD7C, taskKill },
};

#include "../../shared/dryfield_driveway_resolve.inc.c"

#include "../../shared/dryfield_driveway_blackout.inc.c"

#include "../../shared/dryfield_driveway_cutscene.inc.c"

/// Script callback: stores its argument into `gSceneCombatState.actor03700Wave`.
void func_dryfield_night_driveway_8017DC6C(s32 arg0)
{
    gSceneCombatState.actor03700Wave = arg0;
}

#include "../../shared/dryfield_driveway_set_view_dirty.inc.c"

/// Script callback: stores its argument into `D_80115768`.
void func_dryfield_night_driveway_8017DC88(u8 arg0)
{
    D_80115768 = arg0;
}

#include "../../shared/dryfield_driveway_script_sound.inc.c"

/// Message handlers that answer 0 (messages 0x13F1, 0x13F0 and 0x13EF of the
/// room's message table).
s32 func_dryfield_night_driveway_8017DCE4(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_dryfield_night_driveway_8017DCEC(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_dryfield_night_driveway_8017DCF4(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// First state of the room task: installs the room's message table and takes
/// pointer slot 7; when slot 0xA is set and the room was entered by warp 4, it
/// also hands `D_dryfield_night_driveway_8017FB00` and
/// `D_dryfield_night_driveway_8017F998` to `func_800E8634`. Then advances the
/// state.
static void func_dryfield_night_driveway_8017DCFC(Task* arg0)
{
    arg0->msgTable = D_dryfield_night_driveway_8017F7A4;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    if ((gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != 0) && (gGameSession->location.loc.warp == 4)) {
        func_800E8634(D_dryfield_night_driveway_8017FB00, 0, D_dryfield_night_driveway_8017F998);
    }
    arg0->state = (s32)(arg0->state + 1);
}

/// Empty task state: the middle entry of the room task's state table. Its only
/// trace is a 0x10-byte stack frame.
static void func_dryfield_night_driveway_8017DD7C(Task* task)
{
    char pad[0x10];
}

/// Room task: copies the state table onto the stack and runs the entry for the
/// task's current state.
void func_dryfield_night_driveway_8017DD8C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_driveway_8017D5D8;
    sp.funcs[task->state](task);
}

#include "../../shared/glow_draw_shaft.inc.c"

/// Room draw hook: sets the effect mode to 2, then draws the beams the current
/// view (`gGameSession->location.loc.view`) shows - views 2 and 9 the first pair, 4
/// and 7 the second, 5 the third, and 3 and 10 both the first and second.
void func_dryfield_night_driveway_8017E5CC(Task* unused)
{
    gRoomEffectState->roomEffectMode = ROOM_EFFECT_VIEW_ENABLED;
    switch (gGameSession->location.loc.view) {
        case 2:
        case 9:
            glowDrawShaft(&D_dryfield_night_driveway_801805B0[0], 0x180);
            break;
        case 4:
        case 7:
            glowDrawShaft(&D_dryfield_night_driveway_801805B0[2], 0x180);
            break;
        case 5:
            glowDrawShaft(&D_dryfield_night_driveway_801805B0[4], 0x180);
            break;
        case 3:
        case 10:
            glowDrawShaft(&D_dryfield_night_driveway_801805B0[0], 0x180);
            glowDrawShaft(&D_dryfield_night_driveway_801805B0[2], 0x180);
            break;
    }
}
