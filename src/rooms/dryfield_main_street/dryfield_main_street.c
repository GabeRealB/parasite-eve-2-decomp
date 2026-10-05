#include "rooms/dryfield_main_street.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actors/task_tables.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/object_task.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_dryfield.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
#include "../../shared/room_visual_effects.h"
// The flag symbol is four bytes; the gate writes the first.
#define ROOM_EVENT_ACTIVE gRoomEventActive.eventStarted
// The latched-event symbol carries four unproven bytes after the event.
#define ROOM_EVENT_LATCHED gRoomEventLatched.event
#include "../../shared/room_events.h"
// Exported instance: another image refers to this package's copy by name.
#define mainStreetPuffTask dryfieldMainStreetPuffTask
#include "../../shared/main_street.h"

#define DRYFIELD_MAIN_STREET_RAND() ((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16)

/// Advances the gameplay LCG and yields the high half of the new state.

extern RoomEventActiveBytes gRoomEventActive;

/// The clips the Dryfield main street adds to the player's animation bank.
///
/// The room's first-visit cutscene script sends `data.copy` to the player.
/// The copy takes four words from the start of this storage: the three set
/// pointers and the request's own source pointer. Those words occupy extended
/// ids 47-50. The script then plays id 48 and, twenty frames later, id 49;
/// its remaining plays, and the one in its skip script, are base-bank clips.
/// Id 47 stays NULL, id 50 holds the source pointer, and the stored word count
/// sits past the copied span.
typedef union {
    struct {
        AnimationSet*            sets[3]; // Player clips for extended ids 47-49; NULL at the id nothing plays
        AnimationBankCopyRequest copy;    // Copies the first four words of this storage
    } data;                               // The records by name
    s32 words[5];                         // The same storage as the copy reads it; the last word lies beyond the copied span
} _DryfieldMainStreetAnimationBankExtensionStorage;
STATIC_ASSERT_SIZEOF(_DryfieldMainStreetAnimationBankExtensionStorage, 20);

extern _DryfieldMainStreetAnimationBankExtensionStorage D_dryfield_main_street_80181584;

/// Descriptor of the room's own event task, which the message handler spawns.
extern TaskDesc gMainStreetEventTaskDesc;

/// Descriptor of the event task the event gate spawns.
extern TaskDesc gRoomEventTaskDesc;

/// Descriptor of the task `mainStreetTalkMsg` spawns.
extern TaskDesc gMainStreetPlayTimeTaskDesc;

/// Message table the room entry task installs at `Task::msgTable`.
extern TaskMessageEntry D_dryfield_main_street_80180EA0[];

/// Payload of message 0x7DA the room entry task sends to pointer slot 4.
extern s32 D_dryfield_main_street_80180ED0;

/// The two arguments `func_dryfield_main_street_8017E05C` hands to
/// `func_800E8634`.
extern EvsCommand D_dryfield_main_street_80181624[];
extern EvsCommand D_dryfield_main_street_80181A14[];

/// Descriptor of the task `func_dryfield_main_street_8017E320` spawns.
extern TaskDesc D_dryfield_main_street_8018156C[];

/// The room effect mode of each view, indexed by view - 1.
extern u16 D_dryfield_main_street_80181B94[];

/// Spawn position scratch of the room task's effects.
extern SVECTOR D_dryfield_main_street_80181BA4;

/// The two anchors of the trail task's coordinate trails; the second is also
/// reached by its own name.

/// Spawn argument of the helper task 0x31 the room's event task starts.
extern RoomFadeStorage gRoomEventFade;

/// The message and event the message handler latched for the room's event
/// task.
extern RoomEventMsg            gRoomEventStagedMsg;
extern RoomLatchedEventStorage gRoomEventLatched;

/// Set by the message handler when its last 0xB/0xC message latched an event
/// and spawned the room's event task; every such message clears it first.
extern RoomEventStartStorage gMainStreetEventSpawned;

/// The message and request the event gate latched for its event task.
extern RoomEventMsg gRoomEventMsg;
extern RoomEventReq gRoomEventReq;

/// Set by the event gate when its last call latched a request and spawned the
/// event task; every call clears it first.

/// The task `func_dryfield_main_street_8017E320` spawned, until it is killed
/// or forgotten.
extern Task* D_dryfield_main_street_80185630;

static void func_dryfield_main_street_8017E0D8(Task* task);
static void func_dryfield_main_street_8017E158(Task* task);
static void func_dryfield_main_street_8017E4A4(s32 arg0);

extern WorldCollisionGrid    D_dryfield_main_street_80182C9C[1];
extern WorldCollisionTrigger D_dryfield_main_street_801843A4[26];
extern WorldCollisionTrigger D_dryfield_main_street_80184B5C[10];
extern WorldCoordRoomLights  D_dryfield_main_street_80185588[1];

extern TaskDesc Actor00100_D1BA84;

extern AnimationPlayRequest D_dryfield_main_street_801815AC;
extern AnimationPlayRequest D_dryfield_main_street_801815C0;
extern AnimationPlayRequest D_dryfield_main_street_801815D4;
extern ActorTransform       D_dryfield_main_street_801815E8;
void                        func_dryfield_main_street_8017E2F4(s32);
void                        func_dryfield_main_street_8017E320(void);
void                        func_dryfield_main_street_8017E354(s32);

s32  func_dryfield_main_street_8017E054(Task*, s32, s32, s32);
s32  func_dryfield_main_street_8017E05C(Task* task, s32 msgId, const void* firstArg, s32);
void func_dryfield_main_street_8017E1C0(Task*);
void func_dryfield_main_street_8017E3A8(Task*);

TaskDesc gMainStreetEventTaskDesc = { { { TASK_BODY_NONE, 32 } }, roomEventStagedTask, { .value = 0 } };

TaskDesc gRoomEventTaskDesc = { { { TASK_BODY_NONE, 32 } }, roomEventTask, { .value = 0 } };

TaskDesc gMainStreetPlayTimeTaskDesc = { { { TASK_BODY_NONE, 32 } }, mainStreetPlayTimeTask, { .value = 0 } };

TaskMessageEntry D_dryfield_main_street_80180EA0[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, mainStreetResolveMsg },
    { 5105, func_dryfield_main_street_8017E054 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_dryfield_main_street_8017E05C },
    { ROOM_MESSAGE_COMMAND, mainStreetTalkMsg },
    { ROOM_MESSAGE_SOUND, mainStreetCapSoundCue },
    { TASK_MESSAGE_TABLE_END, NULL },
};

s32 D_dryfield_main_street_80180ED0 = 514;

static AnimationPackedPose _gDryfieldMainStreetAnimation03BF0Bank1[6] = {
#include "assets/dryfield_main_street_animation_03BF0_bank1.inc"
};

static AnimationPackedRotation _gDryfieldMainStreetAnimation03BF0Bank4[46] = {
#include "assets/dryfield_main_street_animation_03BF0_bank4.inc"
};

static AnimationRecord _gDryfieldMainStreetAnimation03BF0Records[109] = {
#include "assets/dryfield_main_street_animation_03BF0_records.inc"
};

static u16 _gDryfieldMainStreetAnimation03BF0Indices[20] = {
#include "assets/dryfield_main_street_animation_03BF0_indices.inc"
};

static AnimationSet _gDryfieldMainStreetAnimation03BF0 = {
    _gDryfieldMainStreetAnimation03BF0Records,
    _gDryfieldMainStreetAnimation03BF0Indices,
    { NULL, _gDryfieldMainStreetAnimation03BF0Bank1, NULL, NULL, _gDryfieldMainStreetAnimation03BF0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldMainStreetAnimation03F84Bank1[7] = {
#include "assets/dryfield_main_street_animation_03F84_bank1.inc"
};

static AnimationPackedRotation _gDryfieldMainStreetAnimation03F84Bank4[65] = {
#include "assets/dryfield_main_street_animation_03F84_bank4.inc"
};

static AnimationRecord _gDryfieldMainStreetAnimation03F84Records[123] = {
#include "assets/dryfield_main_street_animation_03F84_records.inc"
};

static u16 _gDryfieldMainStreetAnimation03F84Indices[20] = {
#include "assets/dryfield_main_street_animation_03F84_indices.inc"
};

static AnimationSet _gDryfieldMainStreetAnimation03F84 = {
    _gDryfieldMainStreetAnimation03F84Records,
    _gDryfieldMainStreetAnimation03F84Indices,
    { NULL, _gDryfieldMainStreetAnimation03F84Bank1, NULL, NULL, _gDryfieldMainStreetAnimation03F84Bank4, NULL, NULL, NULL },
};

TaskDesc D_dryfield_main_street_8018156C[2] = {
    { { { TASK_BODY_NONE, 192 } }, func_dryfield_main_street_8017E1C0, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_dryfield_main_street_8017E3A8, { .value = 0 } },
};

_DryfieldMainStreetAnimationBankExtensionStorage D_dryfield_main_street_80181584 = { .data = { { NULL, &_gDryfieldMainStreetAnimation03BF0, &_gDryfieldMainStreetAnimation03F84 }, { { .words = D_dryfield_main_street_80181584.words }, 4 } } };

AnimationPlayRequest D_dryfield_main_street_80181598 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_main_street_801815AC = { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_main_street_801815C0 = { { .index = 1 }, 49, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_dryfield_main_street_801815D4 = { { .index = 1 }, 9, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_dryfield_main_street_801815E8 = { { -2237, 0, -2100, 0 }, { 0, 0, 0, 0 } };

// Retained parameter record; layout follows the adjacent script arguments.
ActorTransform D_dryfield_main_street_80181600 = { { -3000, 0, 6000, 0 }, { 0, 0, 0, 0 } };

ActorCommand D_dryfield_main_street_80181618 = { { .loc = { 2, 2 } }, 1 };

ActorCommand D_dryfield_main_street_8018161C = { { .loc = { 2, 2 } }, 2 };

ActorCommand D_dryfield_main_street_80181620 = { { .loc = { 2, 2 } }, 3 };

EvsCommand D_dryfield_main_street_80181624[42] = {
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 13 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x5202000F }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_dryfield_main_street_80181584.data.copy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_main_street_801815AC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_main_street_801815C0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1021 }, { .value = 48 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_dryfield_main_street_801815E8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = EVENT_SCRIPT_MESSAGE_SELECT_SCENE_MANAGER }, { .value = SCENE_MESSAGE_BROADCAST_TO_ACTORS }, { .message = { .command = &D_dryfield_main_street_80181618 } }, { .value = ACTOR_COMMAND_MESSAGE_APPLY } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_dryfield_main_street_8017E320 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_dryfield_main_street_8017E354 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_main_street_801815D4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = EVENT_SCRIPT_MESSAGE_SELECT_SCENE_MANAGER }, { .value = SCENE_MESSAGE_BROADCAST_TO_ACTORS }, { .message = { .command = &D_dryfield_main_street_80181620 } }, { .value = ACTOR_COMMAND_MESSAGE_APPLY } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 12 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_FRAMEBUFFER_BLEND, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_main_street_801815D4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_FRAMEBUFFER_BLEND, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 11 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x5202000E }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_FRAMEBUFFER_BLEND, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 48 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_FRAMEBUFFER_BLEND, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_dryfield_main_street_8017E2F4 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_dryfield_main_street_8017E354 }, { .value = -1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = EVENT_SCRIPT_MESSAGE_SELECT_SCENE_MANAGER }, { .value = SCENE_MESSAGE_BROADCAST_TO_ACTORS }, { .message = { .command = &D_dryfield_main_street_8018161C } }, { .value = ACTOR_COMMAND_MESSAGE_APPLY } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_dryfield_main_street_80181A14[16] = {
    { EVENT_SCRIPT_OPCODE_SET_FRAMEBUFFER_BLEND, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_dryfield_main_street_801815E8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_main_street_801815D4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = EVENT_SCRIPT_MESSAGE_SELECT_SCENE_MANAGER }, { .value = SCENE_MESSAGE_BROADCAST_TO_ACTORS }, { .message = { .command = &D_dryfield_main_street_8018161C } }, { .value = ACTOR_COMMAND_MESSAGE_APPLY } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_dryfield_main_street_8017E354 }, { .value = -1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_DIRTY_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 13 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_dryfield_main_street_8017E2F4 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

u16 D_dryfield_main_street_80181B94[8] = {
    0,
    2,
    2,
    2,
    0,
    0,
    0,
    0,
};

SVECTOR D_dryfield_main_street_80181BA4 = { -850, -1320, 9770, 0 };

#include "../../shared/room_visual_effects_trail_data.inc.c"

WorldCollisionRoomResources D_dryfield_main_street_80181BBC[1] = {
    { D_dryfield_main_street_80182C9C, D_dryfield_main_street_801843A4, D_dryfield_main_street_80184B5C, NULL },
};

u8* D_dryfield_main_street_80181BCC[1] = {
    gViewIdentityMap,
};

ViewCount D_dryfield_main_street_80181BD0[1] = { 13 };

WorldCoordRoomLighting D_dryfield_main_street_80181BD4[1] = {
    { D_dryfield_main_street_80185588, NULL },
};

DirectionWarpEntry D_dryfield_main_street_80181BDC[7] = {
    { { { .word = 3072 }, -450, 0, -3435 }, { 0, 0, 0, 0 }, { { .word = 3072 }, -450, 0, -3435 }, { 0, 0, 0, 0 }, 0x52020002, 0x52020001, DIRECTION_WARP_SOUND_NONE, 13, DIRECTION_WARP_FLAG_NONE, 488 },
    { { { .word = 1024 }, -6588, 0, -2500 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -6588, 0, -2500 }, { 0, 0, 0, 0 }, 0x52020006, 0x52020005, 0x52020009, 7, DIRECTION_WARP_FLAG_NONE, 487 },
    { { { .word = 1024 }, -6673, 0, 2155 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -6673, 0, 2155 }, { 0, 0, 0, 0 }, 0x52020004, 0x52020003, 0x52020007, 6, DIRECTION_WARP_FLAG_NONE, 486 },
    { { { .word = 1024 }, -6588, 0, 5900 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -6588, 0, 5900 }, { 0, 0, 0, 0 }, 0x52020006, 0x52020005, 0x52020009, 5, DIRECTION_WARP_FLAG_NONE, 485 },
    { { { .word = 1024 }, -6652, 0, 0x2956 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -6652, 0, 0x2956 }, { 0, 0, 0, 0 }, 0x52020006, 0x52020005, 0x52020009, 5, DIRECTION_WARP_FLAG_NONE, 484 },
    { { { .word = 2048 }, -3478, 0, 0x29B5 }, { 0, 0, 0, 0 }, { { .word = 2048 }, -3478, 0, 0x29B5 }, { 0, 0, 0, 0 }, 0x52020006, 0x52020005, 0x52020009, 4, DIRECTION_WARP_FLAG_NONE, 483 },
    { { { .word = 3072 }, -488, 0, 8495 }, { 0, 0, 0, 0 }, { { .word = 3072 }, -488, 0, 8495 }, { 0, 0, 0, 0 }, 0x52020002, 0x52020001, DIRECTION_WARP_SOUND_NONE, 9, DIRECTION_WARP_FLAG_NONE, 482 },
};

static SVECTOR _gDryfieldMainStreetCollision056DCNormals[20] = {
#include "assets/dryfield_main_street_collision_056DC_normals.inc"
};

static SVECTOR _gDryfieldMainStreetCollision056DCVerts[180] = {
#include "assets/dryfield_main_street_collision_056DC_verts.inc"
};

static WorldCollisionGridFace _gDryfieldMainStreetCollision056DCFaces[85] = {
#include "assets/dryfield_main_street_collision_056DC_faces.inc"
};

static s16 _gDryfieldMainStreetCollision056DCCells[554] = {
#include "assets/dryfield_main_street_collision_056DC_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldMainStreetCollision056DCCells[i])
static s16* _gDryfieldMainStreetCollision056DCTable[42] = {
#include "assets/dryfield_main_street_collision_056DC_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_main_street_80182C9C[1] = {
    { NULL, _gDryfieldMainStreetCollision056DCNormals, _gDryfieldMainStreetCollision056DCVerts, _gDryfieldMainStreetCollision056DCFaces, _gDryfieldMainStreetCollision056DCTable, 0x2EE6, 0x2C4C, 6, 7, 4000, 85 },
};

ViewCamera D_dryfield_main_street_80182CC0[13] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 4112, 0x6D60, -2748 } }, 289 },
    { { { { -3906, 0, 1231 }, { 39, 4093, 125 }, { -1231, 131, -3904 } }, { 1214, 1299, -3483 } }, 230 },
    { { { { 3809, 0, 1504 }, { 102, 4086, -259 }, { -1500, 279, 3800 } }, { 1205, 1259, 2882 } }, 230 },
    { { { { 3962, 0, 1037 }, { 65, 4087, -250 }, { -1035, 258, 3954 } }, { 1414, 1228, -1924 } }, 230 },
    { { { { 3997, 0, -893 }, { -382, 3702, -1708 }, { 807, 1750, 3613 } }, { 6425, 2237, -3705 } }, 230 },
    { { { { 3980, 0, -964 }, { 9, 4095, 40 }, { 964, -42, 3980 } }, { 6489, 989, 2493 } }, 230 },
    { { { { -3989, 0, -928 }, { 63, 4086, -271 }, { 926, -278, -3980 } }, { 6440, 837, -1624 } }, 230 },
    { { { { 1028, 0, -3964 }, { -2496, 3181, -647 }, { 3079, 2579, 798 } }, { 2261, 1919, -9779 } }, 230 },
    { { { { 3973, 0, -993 }, { -593, 3285, -2373 }, { 796, 2446, 3187 } }, { 1213, 2874, -5701 } }, 230 },
    { { { { 3637, 0, -1883 }, { 328, 4033, 634 }, { 1854, -714, 3581 } }, { 6170, 490, 4710 } }, 230 },
    { { { { 3848, 0, -1401 }, { -215, 4047, -592 }, { 1385, 631, 3802 } }, { 2050, 1920, -3970 } }, 680 },
    { { { { -4092, 0, -170 }, { -1, 4095, 40 }, { 170, 40, -4092 } }, { 1306, 433, -8467 } }, 557 },
    { { { { -4080, 0, 356 }, { 15, 4092, 177 }, { -355, 177, -4076 } }, { 1060, 1050, 30 } }, 257 },
};

SpriteBatch D_dryfield_main_street_80182E94[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_main_street_80182EA4[51] = {
    { 143, 0x3FC0, { .fields = { 24, 104 } }, -160, -120, 1250, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, -160, -16, 1250, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 152 } }, -72, -120, 2350, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, -112, -120, 2250, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 80 } }, -160, -24, 1250, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 80 } }, -128, -24, 1750, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -112, -24, 1875, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 64 } }, -104, -24, 2350, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -96, 16, 2375, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 136, -24, 1131, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -144, -16, 2375, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 16, -16, 2500, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 136, -16, 1133, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 136, -8, 1136, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, 0, 2375, { .fields = { 0, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 136, 0, 1138, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -152, 8, 2375, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 8, 1091, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 16, 1112, { .fields = { 8, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 64 } }, -160, -32, 2375, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -136, -24, 2250, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -128, 0, 2375, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -128, 16, 2375, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -112, -24, 2375, { .fields = { 24, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 64 } }, -96, -32, 2375, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 32 } }, -88, 0, 2375, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -80, -16, 2375, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 64 } }, -48, -32, 2375, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -40, 8, 2375, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -16, -24, 2500, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 8, 8, 2500, { .fields = { 0, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 32, -24, 2500, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, 0, -24, 2500, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 32, -80, 2250, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 104 } }, 16, -72, 2275, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 40, -88, 2250, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 152 } }, 56, -120, 2344, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 40, -8, 2425, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 152 } }, 96, -120, 2300, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 96, -96, 1294, { .fields = { 16, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, 104, -112, 1237, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 168 } }, 112, -112, 1190, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, 96, 0, 1339, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, 104, 0, 1311, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 176 } }, 120, -112, 1166, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 176 } }, 128, -112, 1128, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 184 } }, 136, -120, 1130, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, 144, -120, 1094, { .fields = { 24, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, 152, -120, 1006, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, 144, 8, 1039, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, 88, 32, 1250, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_main_street_801832A0[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 2, 0, 0, { 3, 0 } },
    { 2, 7, 0, 0, { 0, 0 } },
    { 9, 24, 0, 0, { 5, 0 } },
    { 33, 6, 0, 0, { 1, 0 } },
    { 39, 11, 0, 0, { 4, 0 } },
    { 50, 1, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_main_street_801832E0[26] = {
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, -120, 920, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -144, -120, 1006, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -128, -120, 1060, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -160, -80, 931, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -152, -80, 936, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -144, -80, 991, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -160, -8, 938, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -152, -8, 967, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -144, -8, 1000, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -136, -8, 1088, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -128, -8, 1117, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -120, -8, 1132, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -120, 16, 1142, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -32, -80, 2087, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -24, -72, 2128, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -16, -72, 2165, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -40, -8, 2002, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -24, -8, 2138, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -16, -8, 2178, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 8, -56, 3120, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 16, -56, 3144, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 24, -56, 3099, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 24, -16, 3102, { .fields = { 72, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 104, -64, 2647, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 96, -16, 2685, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 144, 48, 696, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_main_street_801834E8[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 26, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_main_street_80183500[17] = {
    { 143, 0x3FC0, { .fields = { 8, 112 } }, -160, -120, 960, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -152, -120, 998, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -144, -120, 1037, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -160, -8, 968, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -152, -8, 1071, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -144, -8, 1089, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -136, 32, 1107, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -64, -88, 1943, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -56, -80, 1928, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -64, -8, 1842, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -56, -8, 1952, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -48, -80, 1950, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -40, -8, 1970, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -40, -80, 1907, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 80, -88, 1620, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 96, -56, 1623, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 40 } }, 72, -8, 1706, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_main_street_80183654[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 17, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_main_street_8018366C[27] = {
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -8, -120, 1375, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 0, -8, 1475, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 16, -56, 1489, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 24, -56, 1482, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 120, -120, 329, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 112, -72, 366, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 136, -72, 322, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 72, 24, 612, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 96, 24, 600, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 128, 24, 500, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 72, 48, 625, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 96, 48, 600, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 128, 48, 500, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 64, 64, 650, { .fields = { 40, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 88, 64, 600, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 128, 64, 500, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, 80, 690, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 64, 80, 675, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 72, 80, 675, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 80, 80, 650, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 40 } }, 112, 80, 650, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -8, -72, 1412, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -8, -40, 1475, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 96, 0, 600, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 128, 0, 550, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 104, -40, 550, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 136, -40, 500, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_main_street_80183888[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 27, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_main_street_801838A0[26] = {
    { 143, 0x3FC0, { .fields = { 24, 104 } }, -24, -64, 1801, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -24, -72, 1617, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -16, -80, 1450, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -16, -88, 1314, { .fields = { 8, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -8, -96, 1201, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -8, -104, 1106, { .fields = { 8, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 0, -112, 1026, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 0, -120, 956, { .fields = { 0, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -40, -48, 2867, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -24, 0, 2867, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 40, -120, 750, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 224 } }, 48, -120, 775, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 88, 112, 550, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 120 } }, 104, 0, 512, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 128, 0, 512, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 128, 88, 512, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 32, 0, 797, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 40, 0, 800, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 32, 24, 875, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 32, 48, 873, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 224 } }, 64, -120, 700, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 232 } }, 80, -120, 600, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 232 } }, 88, -120, 525, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 96, 568, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -8, 16, 1791, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 120, 56, 623, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_main_street_80183AA8[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 3, 0 } },
    { 8, 15, 0, 0, { 0, 0 } },
    { 23, 3, 0, 0, { 2, 0 } },
    { 26, 0, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_main_street_80183AD8[31] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 16, 0, 2025, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 64 } }, -80, -24, 2025, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -40, 8, 2041, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 72 } }, -24, -24, 2025, { .fields = { 16, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 72 } }, 24, -24, 2025, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -136, 32, 2000, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -136, 88, 2000, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -160, -80, 1875, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -152, -120, 2004, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 200 } }, -128, -104, 2000, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 136 } }, -160, -16, 358, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 128 } }, -136, -8, 402, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 240 } }, -128, -120, 468, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 240 } }, -88, -120, 565, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 104 } }, -72, -120, 574, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -72, -16, 581, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 112 } }, -72, 0, 582, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 104 } }, -64, 0, 642, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 96 } }, -56, 0, 669, { .fields = { 64, 104 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 128, 16 } }, -48, -120, 1053, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 120, 16 } }, -40, -104, 1206, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 104, 16 } }, -24, -88, 1221, { .fields = { 72, 16 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 96, 8 } }, -16, -72, 1400, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 88, 8 } }, -8, -64, 1678, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -8, -56, 1653, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 96 } }, 0, -48, 1750, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 64 } }, 16, -16, 1750, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 40 } }, 24, 8, 1750, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 40, 48, 1750, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 120 } }, 72, -56, 1750, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 184 } }, 80, -120, 1750, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_main_street_80183D44[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 1, 0 } },
    { 5, 5, 0, 0, { 2, 0 } },
    { 10, 21, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_main_street_80183D6C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_main_street_80183D7C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_main_street_80183D8C[12] = {
    { 141, 0x3FC0, { .fields = { 8, 96 } }, 80, -40, 1977, { .fields = { 88, 8 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 96 } }, 72, -40, 2058, { .fields = { 80, 8 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 88 } }, 64, -32, 2142, { .fields = { 72, 16 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 88 } }, 56, -32, 2238, { .fields = { 64, 16 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 88 } }, 48, -32, 2343, { .fields = { 56, 16 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 80 } }, 40, -24, 2453, { .fields = { 48, 24 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 80 } }, 32, -24, 2579, { .fields = { 40, 24 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 72 } }, 24, -16, 2713, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 72 } }, 16, -16, 2869, { .fields = { 24, 32 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 72 } }, 8, -16, 3043, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 64 } }, 0, -8, 3231, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 64 } }, -8, -8, 3454, { .fields = { 0, 40 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_main_street_80183E7C[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 12, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_main_street_80183E94[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_main_street_80183EA4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_main_street_80183EB4[53] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -152, -48, 1675, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, -24, 1675, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, 0, 1675, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, 8, 1675, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -152, 32, 1675, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -64, 32, 1675, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, -144, -64, 1675, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -128, -48, 1675, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -128, -32, 1675, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -128, -8, 1675, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 16 } }, -128, 8, 1675, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 96 } }, -56, -56, 1675, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -48, 24, 1675, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -80, -32, 1675, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -160, -32, 1675, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -160, -8, 1675, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -40, -48, 1675, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 16 } }, -40, -32, 1675, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -40, -8, 1675, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 16 } }, -40, 8, 1675, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 96 } }, 24, -56, 1675, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 56, -48, 1675, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 56, -32, 1675, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 56, -8, 1675, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 56, 8, 1675, { .fields = { 64, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 96 } }, 104, -56, 1675, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 128, -48, 1675, { .fields = { 80, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 128, -24, 1675, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 136, -8, 1675, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 136, 8, 1675, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 208 } }, -160, -88, 701, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 184 } }, -152, -64, 753, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 184 } }, -136, -64, 803, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 168 } }, -120, -56, 857, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 136 } }, -112, -40, 899, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, -104, -40, 937, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, -96, -40, 1005, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 120 } }, -88, -40, 1078, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, -80, -40, 1155, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 96 } }, -72, -40, 1179, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 168 } }, -72, -120, 1625, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 160 } }, -16, -120, 1641, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, 24, 1675, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -88, 24, 1675, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -72, 24, 1675, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -40, 16, 1682, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -24, 16, 1675, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -16, 8, 1674, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 0, 16, 1682, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 16, 8, 1675, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 40, 16, 1675, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 128, 16, 1682, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 144, 16, 1675, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_main_street_801842D8[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 30, 0, 0, { 3, 0 } },
    { 30, 10, 0, 0, { 0, 0 } },
    { 40, 2, 0, 0, { 2, 0 } },
    { 42, 11, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_dryfield_main_street_80184308[13] = {
    { { .empty = D_dryfield_main_street_80182E94 }, D_dryfield_main_street_80182E94, NULL },
    { { .elements = D_dryfield_main_street_80182EA4 }, D_dryfield_main_street_801832A0, NULL },
    { { .elements = D_dryfield_main_street_801832E0 }, D_dryfield_main_street_801834E8, NULL },
    { { .elements = D_dryfield_main_street_80183500 }, D_dryfield_main_street_80183654, NULL },
    { { .elements = D_dryfield_main_street_8018366C }, D_dryfield_main_street_80183888, NULL },
    { { .elements = D_dryfield_main_street_801838A0 }, D_dryfield_main_street_80183AA8, NULL },
    { { .elements = D_dryfield_main_street_80183AD8 }, D_dryfield_main_street_80183D44, NULL },
    { { .empty = D_dryfield_main_street_80183D6C }, D_dryfield_main_street_80183D6C, NULL },
    { { .empty = D_dryfield_main_street_80183D7C }, D_dryfield_main_street_80183D7C, NULL },
    { { .elements = D_dryfield_main_street_80183D8C }, D_dryfield_main_street_80183E7C, NULL },
    { { .empty = D_dryfield_main_street_80183E94 }, D_dryfield_main_street_80183E94, NULL },
    { { .empty = D_dryfield_main_street_80183EA4 }, D_dryfield_main_street_80183EA4, NULL },
    { { .elements = D_dryfield_main_street_80183EB4 }, D_dryfield_main_street_801842D8, NULL },
};

WorldCollisionTrigger D_dryfield_main_street_801843A4[26] = {
    { NULL, NULL, NULL, { -2385, -3392, 351, 0 }, { { -2863, -4416, -165, 0 }, { 2863, -4416, 165, 0 }, { -2863, 4416, -165, 0 }, { 2863, 4416, 165, 0 } }, { 236, 0, -4102, 0 }, { 0, 0, 4096, 0 }, 5246, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2337, -3376, 239, 0 }, { { 2868, -4400, 137, 0 }, { -2867, -4400, -136, 0 }, { 2868, 4400, 137, 0 }, { -2867, 4400, -136, 0 } }, { -195, 0, 4092, 0 }, { 0, 0, 4096, 0 }, 5246, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -3665, -3232, 5071, 0 }, { { -1383, -4256, 519, 0 }, { 1381, -4256, -521, 0 }, { -1383, 4256, 519, 0 }, { 1381, 4256, -521, 0 } }, { -1447, 0, -3843, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -3764, -3216, 4924, 0 }, { { 1447, -4240, -514, 0 }, { -1449, -4240, 512, 0 }, { 1447, 4240, -514, 0 }, { -1449, 4240, 512, 0 } }, { 1368, 0, 3863, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6177, -3200, 5327, 0 }, { { 980, -4224, 41, 0 }, { -979, -4224, -40, 0 }, { 980, 4224, 41, 0 }, { -979, 4224, -40, 0 } }, { -171, 0, 4095, 0 }, { 0, 0, 4096, 0 }, 4314, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6210, -3280, 5598, 0 }, { { -979, -4304, 7, 0 }, { 980, -4304, -7, 0 }, { -979, 4304, 7, 0 }, { 980, 4304, -7, 0 } }, { -30, 0, -4117, 0 }, { 0, 0, 4096, 0 }, 4404, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5988, -3552, -292, 0 }, { { -959, -4576, 198, 0 }, { 960, -4576, -198, 0 }, { -959, 4576, 198, 0 }, { 960, 4576, -198, 0 } }, { -829, 0, -4015, 0 }, { 0, 0, 4096, 0 }, 4664, 0, 7, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6051, -3536, -452, 0 }, { { 949, -4560, -244, 0 }, { -949, -4560, 245, 0 }, { 949, 4560, -244, 0 }, { -949, 4560, 245, 0 } }, { 1025, 0, 3983, 0 }, { 0, 0, 4096, 0 }, 4636, 0, 6, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5216, -3488, -2833, 0 }, { { -8, -4224, -2308, 0 }, { 8, -4224, 2308, 0 }, { -8, 4224, -2308, 0 }, { 8, 4224, 2308, 0 } }, { 4105, 0, -15, 0 }, { 0, 0, 4096, 0 }, 4802, 0, 2, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5072, -3488, -2960, 0 }, { { 8, -4224, 2292, 0 }, { -8, -4224, -2292, 0 }, { 8, 4224, 2292, 0 }, { -8, 4224, -2292, 0 } }, { -4102, 0, 14, 0 }, { 0, 0, 4096, 0 }, 4802, 0, 7, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5312, -2848, 0x28E0, 0 }, { { 235, -4224, -1330, 0 }, { -240, -4224, 1325, 0 }, { 235, 4224, -1330, 0 }, { -240, 4224, 1325, 0 } }, { 4031, 0, 720, 0 }, { 0, 0, 4096, 0 }, 4434, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5216, -3008, 0x2940, 0 }, { { -239, -4224, 1326, 0 }, { 236, -4224, -1329, 0 }, { -239, 4224, 1326, 0 }, { 236, 4224, -1329, 0 } }, { -4033, 0, -722, 0 }, { 0, 0, 4096, 0 }, 4434, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1314, -3296, 4926, 0 }, { { 1314, -4256, 675, 0 }, { -1313, -4256, -675, 0 }, { 1314, 4256, 675, 0 }, { -1313, 4256, -675, 0 } }, { -1878, 0, 3651, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1076, -3264, 5229, 0 }, { { -1235, -4256, -651, 0 }, { 1236, -4256, 651, 0 }, { -1235, 4256, -651, 0 }, { 1236, 4256, 651, 0 } }, { 1912, 0, -3632, 0 }, { 0, 0, 4096, 0 }, 4463, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5056, -3232, 2256, 0 }, { { 24, -4224, 2036, 0 }, { -24, -4224, -2036, 0 }, { 24, 4224, 2036, 0 }, { -24, 4224, -2036, 0 } }, { -4105, 0, 48, 0 }, { 0, 0, 4096, 0 }, 4664, 0, 6, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5216, -2944, 2272, 0 }, { { -24, -4224, -2100, 0 }, { 24, -4224, 2100, 0 }, { -24, 4224, -2100, 0 }, { 24, 4224, 2100, 0 } }, { 4111, 0, -47, 0 }, { 0, 0, 4096, 0 }, 4692, 0, 3, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5248, -2912, 6816, 0 }, { { -24, -4224, -1844, 0 }, { 24, -4224, 1844, 0 }, { -24, 4224, -1844, 0 }, { 24, 4224, 1844, 0 } }, { 4101, 0, -54, 0 }, { 0, 0, 4096, 0 }, 4608, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5040, -3008, 6912, 0 }, { { -24, -4224, 1844, 0 }, { 24, -4224, -1844, 0 }, { -24, 4224, 1844, 0 }, { 24, 4224, -1844, 0 } }, { -4103, 0, -54, 0 }, { 0, 0, 4096, 0 }, 4608, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -384, -2624, 7537, 0 }, { { -1800, -4416, 722, 0 }, { 1793, -4416, -730, 0 }, { -1800, 4416, 722, 0 }, { 1793, 4416, -730, 0 } }, { -1542, 0, -3815, 0 }, { 0, 0, 4096, 0 }, 4802, 0, 4, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -448, -2816, 7328, 0 }, { { 1841, -4416, -751, 0 }, { -1852, -4416, 741, 0 }, { 1841, 4416, -751, 0 }, { -1852, 4416, 741, 0 } }, { 1537, 0, 3806, 0 }, { 0, 0, 4096, 0 }, 4830, 0, 9, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2465, -2944, 9983, 0 }, { { 211, -4416, -1982, 0 }, { -215, -4416, 1978, 0 }, { 211, 4416, -1982, 0 }, { -215, 4416, 1978, 0 } }, { 4081, 0, 438, 0 }, { 0, 0, 4096, 0 }, 4830, 0, 9, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2370, -2976, 0x27BE, 0 }, { { -217, -4416, 1976, 0 }, { 209, -4416, -1985, 0 }, { -217, 4416, 1976, 0 }, { 209, 4416, -1985, 0 } }, { -4083, 0, -440, 0 }, { 0, 0, 4096, 0 }, 4830, 0, 4, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 62, -3264, -1955, 0 }, { { -2049, -4416, -351, 0 }, { 2050, -4416, 351, 0 }, { -2049, 4416, -351, 0 }, { 2050, 4416, 351, 0 } }, { 694, 0, -4055, 0 }, { 0, 0, 4096, 0 }, 4857, 0, 13, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -192, -3264, -2146, 0 }, { { 1607, -4416, 267, 0 }, { -1607, -4416, -266, 0 }, { 1607, 4416, 267, 0 }, { -1607, 4416, -266, 0 } }, { -671, 0, 4041, 0 }, { 0, 0, 4096, 0 }, 4692, 0, 2, 13, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1602, -3296, -4498, 0 }, { { -133, -4416, 2152, 0 }, { 134, -4416, -2152, 0 }, { -133, 4416, 2152, 0 }, { 134, 4416, -2152, 0 } }, { -4099, 0, -255, 0 }, { 0, 0, 4096, 0 }, 4910, 0, 2, 13, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1793, -3360, -4513, 0 }, { { 139, -4416, -2237, 0 }, { -139, -4416, 2238, 0 }, { 139, 4416, -2237, 0 }, { -139, 4416, 2238, 0 } }, { 4090, 0, 253, 0 }, { 0, 0, 4096, 0 }, 4937, 0, 13, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_dryfield_main_street_80184B5C[10] = {
    { NULL, NULL, NULL, { -384, 0, -3392, 0 }, { { -416, 0, -1024, 0 }, { 416, 0, -1024, 0 }, { -416, 0, 1024, 0 }, { 416, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 1101, WORLD_COLLISION_TRIGGER_ACTION_WARP, 1, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6912, -48, -2672, 0 }, { { -416, 0, -848, 0 }, { 416, 0, -848, 0 }, { -416, 0, 848, 0 }, { 416, 0, 848, 0 } }, { 0, 4105, 0, 0 }, { 4096, 0, 0, 0 }, 942, WORLD_COLLISION_TRIGGER_ACTION_WARP, 11, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6848, -48, 2016, 0 }, { { -416, 0, -1024, 0 }, { 416, 0, -1024, 0 }, { -416, 0, 1024, 0 }, { 416, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 1101, WORLD_COLLISION_TRIGGER_ACTION_WARP, 15, 49, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6848, -48, 5824, 0 }, { { -416, 0, -1024, 0 }, { 416, 0, -1024, 0 }, { -416, 0, 1024, 0 }, { 416, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 1101, WORLD_COLLISION_TRIGGER_ACTION_WARP, 12, 65, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6816, -48, 0x2870, 0 }, { { -416, 0, -848, 0 }, { 416, 0, -848, 0 }, { -416, 0, 848, 0 }, { 416, 0, 848, 0 } }, { 0, 4105, 0, 0 }, { 4096, 0, 0, 0 }, 942, WORLD_COLLISION_TRIGGER_ACTION_WARP, 13, 81, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3536, -48, 0x2A00, 0 }, { { 816, 0, -416, 0 }, { 816, 0, 416, 0 }, { -816, 0, -416, 0 }, { -816, 0, 416, 0 } }, { 0, 4114, 0, 0 }, { 0, 0, -4096, 0 }, 914, WORLD_COLLISION_TRIGGER_ACTION_WARP, 14, 97, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -800, -64, 0x2880, 0 }, { { 1024, 0, -960, 0 }, { 1024, 0, 960, 0 }, { -1024, 0, -960, 0 }, { -1024, 0, 960, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 1402, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -257, -48, 8703, 0 }, { { -415, 0, -511, 0 }, { 417, 0, -511, 0 }, { -416, 0, 512, 0 }, { 416, 0, 512, 0 } }, { 0, 4099, 0, 0 }, { -4096, 0, 0, 0 }, 658, WORLD_COLLISION_TRIGGER_ACTION_WARP, 25, 115, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 320, -64, -1520, 0 }, { { -2880, 0, -880, 0 }, { 2880, 0, -624, 0 }, { -2880, 0, 624, 0 }, { 2880, 0, 880, 0 } }, { 0, 4102, 0, 0 }, { -4096, 0, 0, 0 }, 3007, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 1, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { -2144, -64, -3952, 0 }, { { -640, 0, -3136, 0 }, { 544, 0, -3104, 0 }, { -544, 0, 3104, 0 }, { 640, 0, 3136, 0 } }, { 0, 4099, 0, 0 }, { -4096, 0, 0, 0 }, 3197, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 1, 0, WORLD_COLLISION_TRIGGER_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_dryfield_main_street_80184E54[3] = {
    { 101, 230, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, &D_actor_323000_80173A08 },
    { 1, 1, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &Actor00100_D1BA84 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_main_street_80184E78[2] = {
    { 25, 25, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_102500_801379A8 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_main_street_80184E90[2] = {
    { 1, 1, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &Actor00100_D1BA84 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_main_street_80184EA8[3] = {
    { 20, 20, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, Actor02000_D15FD0 },
    { 23, 23, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_202300_8015FAB8 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_main_street_80184ECC[3] = {
    { 101, 230, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, &D_actor_323000_80173A08 },
    { 1, 1, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &Actor00100_D1BA84 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_dryfield_main_street_80184EF0[3] = {
    { 101, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0 },
    { 1, 0, 0, -1638, 0, 4280, -1012, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant D_dryfield_main_street_80184F20[13] = {
    { NULL, NULL },
    { D_map_dryfield_8017AD44, D_dryfield_main_street_80184E54 },
    { D_map_dryfield_8017AD74, D_dryfield_main_street_80184E78 },
    { D_map_dryfield_8017ADE4, D_dryfield_main_street_80184E90 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_dryfield_8017AE14, D_dryfield_main_street_80184EA8 },
    { NULL, NULL },
    { NULL, NULL },
    { D_dryfield_main_street_80184EF0, D_dryfield_main_street_80184ECC },
    { NULL, NULL },
    { NULL, NULL },
};

WorldCoordPointLight D_dryfield_main_street_80184F88[16] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -3910, -4583, -2531 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4642, 3808, 3733 }, { 0, 0 } }, 2050, 0x312B },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -5600, -2355, -4127 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4097, 3734, 3714 }, { 0, 0 } }, 1303, 3024 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5140, -2738, -5045 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3502, 3033, 2983 }, { 0, 0 } }, 1421, 3621 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4637, -2215, -5198 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3359, 2922, 2821 }, { 0, 0 } }, 701, 1211 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -4405, -2155, -5380 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3274, 2784, 2741 }, { 0, 0 } }, 1063, 2153 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -5104, -900, -5740 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3163, 2621, 2539 }, { 0, 0 } }, 100, 1000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5037, -900, -5783 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3003, 2621, 2539 }, { 0, 0 } }, 100, 1000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2794, -4433, 5 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4193, 3948, 3953 }, { 0, 0 } }, 1792, 7511 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5464, -4865, -456 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4304, 3842, 3575 }, { 0, 0 } }, 1755, 6785 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1367, -4865, 872 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3848, 2949, 2931 }, { 0, 0 } }, 9220, 0x450B },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1162, -3422, -5474 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3969, 3722, 3700 }, { 0, 0 } }, 283, 4826 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3332, -3624, -6033 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4112, 3810, 3716 }, { 0, 0 } }, 1463, 4128 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -4925, -2543, -2722 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3254, 2855, 2829 }, { 0, 0 } }, 705, 2774 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4029, -3246, -2729 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2792, 2483, 2445 }, { 0, 0 } }, 1132, 2911 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3765, -3608, -7108 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3324, 2700, 2611 }, { 0, 0 } }, 480, 1328 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -3265, -2641, -2536 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3170, 2745, 2730 }, { 0, 0 } }, 822, 2975 },
};

WorldCoordRoomLights D_dryfield_main_street_80185588[1] = {
    { 0, NULL, ARRAY_SIZE(D_dryfield_main_street_80184F88), D_dryfield_main_street_80184F88, 0, NULL },
};

WorldCollisionFootstepSounds D_dryfield_main_street_801855A0 = {
    0x10000049,
    0x1000004B,
    0x10000049,
};

WorldCollisionFootstepSounds D_dryfield_main_street_801855AC = {
    0x1000003D,
    0x1000003F,
    0x1000003D,
};

WorldCollisionFootstepSounds D_dryfield_main_street_801855B8 = {
    0x1000002D,
    0x1000002F,
    0x1000002D,
};

WorldCollisionSurfaceProperties D_dryfield_main_street_801855C4[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_main_street_801855CC[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_main_street_801855A0 },
};

WorldCollisionSurfaceProperties D_dryfield_main_street_801855D4[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_main_street_801855AC },
};

WorldCollisionSurfaceProperties D_dryfield_main_street_801855DC[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_main_street_801855B8 },
};

WorldCollisionSurfaceProperties D_dryfield_main_street_801855E4[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties* D_dryfield_main_street_801855EC[8] = {
    D_dryfield_main_street_801855C4,
    D_dryfield_main_street_801855E4,
    D_dryfield_main_street_801855D4,
    D_dryfield_main_street_801855DC,
    D_dryfield_main_street_801855CC,
    D_dryfield_main_street_801855C4,
    D_dryfield_main_street_801855C4,
    D_dryfield_main_street_801855C4,
};

RoomFadeStorage gRoomEventFade = { 0 };

RoomEventMsg gRoomEventStagedMsg = { 0 };

RoomEventStartStorage gMainStreetEventSpawned = { 0 };

RoomEventMsg gRoomEventMsg = { 0 };

RoomEventActiveBytes gRoomEventActive = { 0, { 3, 190, 28 } };

Task* D_dryfield_main_street_80185630 = NULL;

RoomLatchedEventStorage gRoomEventLatched = { { 0 }, { 0 } };

RoomEventReq gRoomEventReq = { 0, 0, 0, 0, 0, 0 };

#include "../../shared/room_event_staged_task.inc.c"

#include "../../shared/room_event_gate.inc.c"

#include "../../shared/room_event_task.inc.c"

/// The room entry task's three states: set the room up, idle, end.
static const TaskFuncTable3 D_dryfield_main_street_8017D5F4 = {
    { func_dryfield_main_street_8017E0D8, func_dryfield_main_street_8017E158, taskKill },
};

#include "../../shared/main_street_resolve_msg.inc.c"

#include "../../shared/main_street_play_time_task.inc.c"

#include "../../shared/main_street_cap_sound_cue.inc.c"

#include "../../shared/main_street_talk_msg.inc.c"

/// Does nothing and answers 0.
s32 func_dryfield_main_street_8017E054(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// On a message whose `actionId` is 1, the first time only (nibble 0x5F still
/// clear): forgets the task `func_dryfield_main_street_8017E320` spawned, calls
/// `func_800E8634` with the room's two data blocks, and sets nibbles 0x5F and
/// 0x155 and clears nibble 3. Always answers 0.
s32 func_dryfield_main_street_8017E05C(Task* task, s32 msgId, const void* firstArg, s32 arg3)
{
    const DirectionActionRequest* msg = firstArg;

    if ((msg->actionId == 1) && (gameFlagGetNibble(GAME_FLAG_MAIN_STREET_CUTSCENE_SEEN) == 0)) {
        func_dryfield_main_street_8017E4A4(0);
        func_800E8634(D_dryfield_main_street_80181624, 0, D_dryfield_main_street_80181A14);
        gameFlagSetNibble(GAME_FLAG_MAIN_STREET_CUTSCENE_SEEN, 1);
        gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, 0);
        gameFlagSetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX, 1);
    }
    return 0;
}

/// The room entry task's first state: installs the room's message table,
/// hands the task to pointer slot 7, raises `D_80115598` and, until nibble
/// 0x5F is set, sends message 0x7DA to the task in pointer slot 4.
static void func_dryfield_main_street_8017E0D8(Task* task)
{
    task->msgTable = D_dryfield_main_street_80180EA0;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    D_80115598 = 1;
    if (gameFlagGetNibble(GAME_FLAG_MAIN_STREET_CUTSCENE_SEEN) == 0) {
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &D_dryfield_main_street_80180ED0, ACTOR_COMMAND_MESSAGE_APPLY);
    }
    task->state++;
}

/// The room entry task's idle state; it only opens and closes a stack frame.
static void func_dryfield_main_street_8017E158(Task* task)
{
    char pad[0x10];
}

/// Runs the room entry task's current state from its three-entry table, which
/// it copies onto the stack before the call.
void func_dryfield_main_street_8017E168(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_main_street_8017D5F4;
    sp.funcs[task->state](task);
}

/// Per-frame task that turns the player (`gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)`, whose
/// `Task::work` is the `GameActor` block) to face the object the area work
/// id resolves to, then kills itself once it is close enough.
///
/// The aim angle is `ratan2` of the translation of the *second*
/// `GfxCoord` node of the target's model (`field_8[1]`) minus the
/// player's own (`field_8[0]`); the delta against `GameActor::rotation.vy` is
/// unwrapped into `-0x800..0x800` and stepped by `0x80` per frame, so the
/// player rotates at a fixed rate. Inside `0x80` of the target the facing
/// snaps to the exact angle and the task ends.
///
/// The task's own argument is only ever the `taskKill` target, reached both
/// when the work lookup or `gGameSession::eventState` fails and on the frame the
/// facing settles.
void func_dryfield_main_street_8017E1C0(Task* task)
{
    Task*      player;
    GameActor* actor;
    Enemy*     enemy;
    GfxCoord*  self;
    GfxCoord*  target;
    s32        angle;
    s32        delta;
    s32        magnitude;
    s32        step;
    s32        wrapped;

    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    actor  = (GameActor*)player->work;
    enemy  = Gp_FindWorkById(gGameSession->location.loc.area | (gGameSession->location.loc.stage << 8));
    if ((enemy != NULL) && (gGameSession->eventState != 0)) {
        self      = player->extra.tmd->coords;
        target    = &enemy->task->extra.tmd->coords[1];
        angle     = ratan2(target->coord.t[0] - self->coord.t[0], target->coord.t[2] - self->coord.t[2]);
        delta     = angle - actor->rotation.vy;
        magnitude = ABS(delta);
        if (magnitude >= 0x801) {
            wrapped = delta - 0x1000;
            if (delta < 0) {
                wrapped = delta + 0x1000;
            }
            delta = wrapped;
        }
        magnitude = ABS(delta);
        if (magnitude >= 0x81) {
            step = 0x80;
            if (delta < 0) {
                step = -0x80;
            }
            actor->rotation.vy = (s16)((u16)actor->rotation.vy + step);
            return;
        }
        actor->rotation.vy = angle;
    }
    taskKill(task);
}

/// Passes `arg0` to `Gp_ArmStateF0` and marks item 0x10A as seen.
void func_dryfield_main_street_8017E2F4(s32 arg0)
{
    Gp_ArmStateF0(arg0);
    Gp_SetItemSeenBit(0x10A, 1);
}

/// Spawns the ramp task described at `D_dryfield_main_street_8018156C` and
/// keeps it in `D_dryfield_main_street_80185630`.
void func_dryfield_main_street_8017E320(void)
{
    D_dryfield_main_street_80185630 = taskSpawnFromTable(D_dryfield_main_street_8018156C, 1, 0, 0);
}

/// Steers the task `func_dryfield_main_street_8017E320` spawned, if there is
/// one: 0 or 1 becomes its `spawnArg1`, any other value kills and forgets it.
void func_dryfield_main_street_8017E354(s32 arg0)
{
    Task* t = D_dryfield_main_street_80185630;

    if (t == NULL) {
        return;
    }
    if (arg0 >= 2) {
        goto kill;
    }
    if (arg0 < 0) {
        goto kill;
    }
    t->spawnArg1.value = arg0;
    return;
kill:
    taskKill(D_dryfield_main_street_80185630);
    D_dryfield_main_street_80185630 = NULL;
}

/// Per-frame ramp task. While `D_801156F9` is clear, state 0 ramps
/// `killCountdown` by 0x100 a frame towards 0x1000 while `spawnArg1` is set,
/// or towards 0 while it is clear, and passes it to `func_800B0928` with the
/// player and the current area's work object. Any other state ends the task.
void func_dryfield_main_street_8017E3A8(Task* task)
{
    Enemy* enemy;
    u16    tick;

    if (D_801156F9 == 0) {
        if (task->state == 0) {
            if (task->spawnArg1.value != 0) {
                tick                = task->killCountdown + 0x100;
                task->killCountdown = tick;
                if ((s16)tick >= 0x1001) {
                    task->killCountdown = 0x1000;
                }
            } else {
                tick                = task->killCountdown - 0x100;
                task->killCountdown = tick;
                if ((s16)tick < 0) {
                    task->killCountdown = 0;
                }
            }
            enemy = Gp_FindWorkById(gGameSession->location.loc.area | (gGameSession->location.loc.stage << 8));
            func_800B0928(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), enemy->task, 0x300, 0x200, task->killCountdown);
        } else {
            taskKill(task);
        }
    }
}

/// Forgets the task `func_dryfield_main_street_8017E320` spawned, without
/// killing it. The argument is unused; its caller passes 0.
static void func_dryfield_main_street_8017E4A4(s32 arg0)
{
    D_dryfield_main_street_80185630 = 0;
}

/// Per-frame room task. On its first run it stores the ids 0x60293-0x60295 in
/// three gameplay globals. Each run it publishes the current view's
/// `roomEffectMode`. In view 8 it spawns 0x30 randomly placed 0x601B1 effects
/// on entering the view, and one more on each run with bit 0 of `gDisplayState.animFrame`
/// set while it stays. `spawnArg1` holds the view seen on the previous run.
void func_dryfield_main_street_8017E4B0(Task* task)
{
    s32 i;

    if (task->state == 0) {
        gRoomEffectFlashId      = EFFECT_DRYFIELD_MAIN_STREET_FLASH;
        gRoomEffectTwinTrailId  = EFFECT_DRYFIELD_MAIN_STREET_TWIN_TRAIL;
        gRoomEffectSparkBurstId = EFFECT_DRYFIELD_MAIN_STREET_SPARK_BURST;
        task->state             = 1;
    }
    gRoomEffectState->roomEffectMode = D_dryfield_main_street_80181B94[(viewGetMappedIndex() & 0xFF) - 1];
    if ((viewGetMappedIndex() & 0xFF) == 8) {
        if (task->spawnArg1.value != (viewGetMappedIndex() & 0xFF)) {
            for (i = 0; i < 0x30; i++) {
                D_dryfield_main_street_80181BA4.vx = DRYFIELD_MAIN_STREET_RAND() % 300 - 0x4A1;
                D_dryfield_main_street_80181BA4.vy = DRYFIELD_MAIN_STREET_RAND() % 600 - 0x4E7;
                D_dryfield_main_street_80181BA4.vz = 0x2927 - DRYFIELD_MAIN_STREET_RAND() % 700;
                Gp_SpawnEff(EFFECT_DRYFIELD_MAIN_STREET_SMOKE_PUFF, NULL, (DRYFIELD_MAIN_STREET_RAND() & 0x10FF) + 0x103100,
                            &D_dryfield_main_street_80181BA4);
            }
        } else if (gDisplayState.animFrame & 1) {
            D_dryfield_main_street_80181BA4.vx = DRYFIELD_MAIN_STREET_RAND() % 300 - 0x4A1;
            D_dryfield_main_street_80181BA4.vy = DRYFIELD_MAIN_STREET_RAND() % 600 - 0x4E7;
            D_dryfield_main_street_80181BA4.vz = 0x2927 - DRYFIELD_MAIN_STREET_RAND() % 700;
            Gp_SpawnEff(EFFECT_DRYFIELD_MAIN_STREET_SMOKE_PUFF, NULL, (DRYFIELD_MAIN_STREET_RAND() & 0x10FF) | 0x82100,
                        &D_dryfield_main_street_80181BA4);
        }
    }
    task->spawnArg1.value = viewGetMappedIndex() & 0xFF;
}

#include "../../shared/main_street_puff_task.inc.c"

#include "../../shared/main_street_draw_puff.inc.c"

#include "../../shared/room_visual_effects.inc.c"

#include "../../shared/room_visual_effects_flash_task.inc.c"

void func_dryfield_main_street_8017EEE8(Task* arg0)
{
    _roomVisualEffectsFlashTask(arg0);
}

#include "../../shared/room_visual_effects_trails.inc.c"

void func_dryfield_main_street_8017F94C(Task* task)
{
#include "../../shared/room_visual_effects_trail_task.inc.c"
}

#include "../../shared/room_visual_effects_sparks.inc.c"

void func_dryfield_main_street_80180234(Task* task)
{
    RoomFx_SparkBurstTask(task);
}

#include "../../shared/room_visual_effects_glow.inc.c"
