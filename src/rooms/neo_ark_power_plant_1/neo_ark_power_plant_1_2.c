#include "rooms/neo_ark_power_plant_1.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "neo_ark_power_plant_1_private.h"

#include "actors/task_tables.h"

#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_targets.h"
#include "gameplay/scene_combat.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task_types.h"

#include "mapui/map_neo_ark.h"

#include "rooms/room_common.h"
#include "../../shared/glow_draw.h"

extern SVECTOR D_neo_ark_power_plant_1_8017F020[52];
extern SVECTOR D_neo_ark_power_plant_1_8017F1C0;

static WorldCollisionGrid     _gNeoArkPowerPlant1CollisionGrid;
extern WorldCollisionOccluder D_neo_ark_power_plant_1_80181B60[1];
extern WorldCollisionTrigger  D_neo_ark_power_plant_1_8018155C[10];
extern WorldCollisionTrigger  D_neo_ark_power_plant_1_80181854[7];
static WorldCoordRoomLights   _gNeoArkPowerPlant1RoomLights;

extern AnimationPlayRequest D_neo_ark_power_plant_1_8017EB40;
extern AnimationPlayRequest D_neo_ark_power_plant_1_8017EEAC;

static AnimationSet _gNeoArkPowerPlant1Animation01530;

static AnimationPackedPose _gNeoArkPowerPlant1Animation01530Bank1[10] = {
#include "assets/neo_ark_power_plant_1_animation_01530_bank1.inc"
};

static AnimationPackedRotation _gNeoArkPowerPlant1Animation01530Bank4[126] = {
#include "assets/neo_ark_power_plant_1_animation_01530_bank4.inc"
};

static AnimationRecord _gNeoArkPowerPlant1Animation01530Records[171] = {
#include "assets/neo_ark_power_plant_1_animation_01530_records.inc"
};

static u16 _gNeoArkPowerPlant1Animation01530Indices[20] = {
#include "assets/neo_ark_power_plant_1_animation_01530_indices.inc"
};

static AnimationSet _gNeoArkPowerPlant1Animation01530 = {
    _gNeoArkPowerPlant1Animation01530Records,
    _gNeoArkPowerPlant1Animation01530Indices,
    { NULL, _gNeoArkPowerPlant1Animation01530Bank1, NULL, NULL, _gNeoArkPowerPlant1Animation01530Bank4, NULL, NULL, NULL },
};

TaskMessageEntry D_neo_ark_power_plant_1_8017EB18[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, neoArkPowerPlant1ResolveRoomVariant },
    { ROOM_MESSAGE_USE_KEY_ITEM, neoArkPowerPlant1RejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, neoArkPowerPlant1IgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, neoArkPowerPlant1HandleCapCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

AnimationPlayRequest D_neo_ark_power_plant_1_8017EB40 = { { .index = 1 }, 1, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

ActorCommand D_neo_ark_power_plant_1_8017EB54 = { { .loc = { 5, 16 } }, 1 };

ActorCommand D_neo_ark_power_plant_1_8017EB58 = { { .loc = { 5, 16 } }, 2 };

ActorCommand D_neo_ark_power_plant_1_8017EB5C = { { .loc = { 5, 16 } }, 3 };

PadScriptCmd D_neo_ark_power_plant_1_8017EB60[5] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_WAIT, 1), PAD_SCRIPT_COMMAND(PAD_SCRIPT_WAIT, 5) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_LOOP, 40), PAD_SCRIPT_COMMAND(PAD_SCRIPT_LOOP, 15) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_JUMP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_JUMP, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

PadScriptVibrationSegment D_neo_ark_power_plant_1_8017EB74[2] = {
    { 236, 77, 4, 1 },
    { 0, 0, 2, 0 },
};

EvsCommand D_neo_ark_power_plant_1_8017EB7C[24] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = neoArkPowerPlant1PrepareGeneratorClearScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_neo_ark_power_plant_1_8017EB40 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x55110006 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x55110007 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_VIBRATION, { .padCommands = D_neo_ark_power_plant_1_8017EB60 }, { .vibrationSegments = D_neo_ark_power_plant_1_8017EB74 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_neo_ark_power_plant_1_8017EB54 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_VIEW, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_neo_ark_power_plant_1_8017EB58 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_neo_ark_power_plant_1_8017EDBC[10] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = neoArkPowerPlant1StopSkippedSceneVibration }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_VIEW, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_neo_ark_power_plant_1_8017EB5C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

AnimationPlayRequest D_neo_ark_power_plant_1_8017EEAC = { { .index = 1 }, 47, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_ENABLE };

/// Player clip for extended id 47.
///
/// One of the room's event scripts sends the player the copy request and then
/// plays the clip; the room's other play request selects a base-bank clip.
/// `D_neo_ark_power_plant_1_8017EEC4` copies two words starting here into the
/// player's bank, which is one word past the end of this array: the read runs
/// on through the first word of `D_neo_ark_power_plant_1_8017EEC4`, that
/// request's own source pointer. That overrun is the original's and is kept as
/// it is: the request carries a literal count larger than the table, while the
/// table was stored with only its own entries. Id 48, which receives the source
/// pointer, is never played.
AnimationSet* D_neo_ark_power_plant_1_8017EEC0[1] = { &_gNeoArkPowerPlant1Animation01530 };

// Installs the player's clip; the count is two, not the one entry of its source.
AnimationBankCopyRequest D_neo_ark_power_plant_1_8017EEC4 = { { .sets = D_neo_ark_power_plant_1_8017EEC0 }, 2 };

ActorTransform D_neo_ark_power_plant_1_8017EECC = { { 4800, 2, -0x2A94, 0 }, { 0, 0, 0, 0 } };

EvsCommand D_neo_ark_power_plant_1_8017EEE4[13] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_neo_ark_power_plant_1_8017EEC4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_neo_ark_power_plant_1_8017EECC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1021 }, { .value = 8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_neo_ark_power_plant_1_8017EEAC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_neo_ark_power_plant_1_8017EB40 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = sceneEngageBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

s32 D_neo_ark_power_plant_1_8017F01C = 0;

SVECTOR D_neo_ark_power_plant_1_8017F020[52] = {
    { 200, -15, -3375, 0 },
    { 1300, -15, -3375, 0 },
    { 200, -15, -4125, 0 },
    { 1300, -15, -4125, 0 },
    { 200, -15, -4875, 0 },
    { 1300, -15, -4875, 0 },
    { 200, -15, -5625, 0 },
    { 1300, -15, -5625, 0 },
    { 200, -15, -6375, 0 },
    { 1300, -15, -6375, 0 },
    { 200, -15, -7125, 0 },
    { 1300, -15, -7125, 0 },
    { 200, -15, -7875, 0 },
    { 1300, -15, -7875, 0 },
    { 200, -15, -8625, 0 },
    { 1300, -15, -8625, 0 },
    { 200, -15, -9375, 0 },
    { 1300, -15, -9375, 0 },
    { 200, -15, -0x278D, 0 },
    { 1300, -15, -0x278D, 0 },
    { 200, -15, -0x2BF2, 0 },
    { 750, -15, -0x2E18, 0 },
    { 1875, -15, -0x29CC, 0 },
    { 1875, -15, -0x2E18, 0 },
    { 2625, -15, -0x29CC, 0 },
    { 2625, -15, -0x2E18, 0 },
    { 3375, -15, -0x29CC, 0 },
    { 3375, -15, -0x2E18, 0 },
    { 4625, -15, -0x29CC, 0 },
    { 4625, -15, -0x2E18, 0 },
    { 5375, -15, -0x29CC, 0 },
    { 5375, -15, -0x2E18, 0 },
    { 6125, -15, -0x29CC, 0 },
    { 6125, -15, -0x2E18, 0 },
    { 7250, -15, -0x2E18, 0 },
    { 7800, -15, -0x2BF2, 0 },
    { 6700, -15, -0x2783, 0 },
    { 7800, -15, -0x2783, 0 },
    { 6700, -15, -9365, 0 },
    { 7800, -15, -9365, 0 },
    { 6700, -15, -8615, 0 },
    { 7800, -15, -8615, 0 },
    { 750, -4250, -5245, 0 },
    { 3000, -4250, -5245, 0 },
    { 5000, -4250, -5240, 0 },
    { 750, -4250, -7500, 0 },
    { 3000, -4250, -7500, 0 },
    { 5000, -4250, -7500, 0 },
    { 750, -4250, -9740, 0 },
    { 3000, -4250, -9740, 0 },
    { 5000, -4250, -9740, 0 },
    { 7250, -4250, -9740, 0 },
};

SVECTOR D_neo_ark_power_plant_1_8017F1C0 = { 2015, -1080, -4500, 0 };

WorldCollisionRoomResources D_neo_ark_power_plant_1_8017F1C8[1] = {
    { &_gNeoArkPowerPlant1CollisionGrid, D_neo_ark_power_plant_1_8018155C, D_neo_ark_power_plant_1_80181854, D_neo_ark_power_plant_1_80181B60 },
};

WorldCoordRoomLighting gNeoArkPowerPlant1RoomLighting[1] = {
    { &_gNeoArkPowerPlant1RoomLights, NULL },
};

u8* gNeoArkPowerPlant1ViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount D_neo_ark_power_plant_1_8017F1E4[1] = { 9 };

DirectionWarpEntry D_neo_ark_power_plant_1_8017F1E8[1] = {
    { { { .word = 3072 }, 8680, 0, -0x2904 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 8680, 0, -0x2904 }, { 0, 0, 0, 0 }, 0x55110002, 0x55110001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, GAME_FLAG_MAP_MARK_SHRINE },
};

/// Point lights for model shading throughout Power Plant 1, active in every view.
///
/// Positions and falloff radii use room-coordinate units; RGB uses `ONE` for 1.0.
/// The loaded room owns the writable transforms and attenuation scratch.
static WorldCoordPointLight _gNeoArkPowerPlant1PointLights[25] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1845, -1110, -4495 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3481, 3788, ONE }, { 0, 0 } }, 802, 1242 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 7241, -19, -8894 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2866, 3072, 3276 }, { 0, 0 } }, 724, 1646 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4970, -45, -0x2C83 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3072, 3685, ONE }, { 0, 0 } }, 309, 1232 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4375, -45, -0x2F03 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3072, 3685, ONE }, { 0, 0 } }, 947, 1232 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 2446, -45, -0x2DEC } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3072, 3685, ONE }, { 0, 0 } }, 1021, 1692 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 163, -45, -8724 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3072, 3685, ONE }, { 0, 0 } }, 978, 1705 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 805, -4175, -7885 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3072, 3685, ONE }, { 0, 0 } }, 500, 1232 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 104, -45, -6319 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3072, 3685, ONE }, { 0, 0 } }, 1102, 1413 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 44, -45, -4962 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3072, 3685, ONE }, { 0, 0 } }, 1231, 1489 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 7800, -19, -6433 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 3072, 3276 }, { 0, 0 } }, 699, 700 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 750, -4175, -9750 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3072, 3686, ONE }, { 0, 0 } }, 500, 750 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3000, -4175, -9750 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3072, 3685, ONE }, { 0, 0 } }, 2447, 5535 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5000, -4175, -9750 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3072, 3686, ONE }, { 0, 0 } }, 2449, 5472 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 7250, -45, -9750 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3072, 3685, ONE }, { 0, 0 } }, 500, 1232 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5000, -4175, -7500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3072, 3686, ONE }, { 0, 0 } }, 2467, 5362 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3000, -4175, -7500 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3072, 3685, ONE }, { 0, 0 } }, 2503, 5133 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 7800, -19, -0x2BF2 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2866, 3072, 3276 }, { 0, 0 } }, 200, 2241 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6050, -19, -0x2E18 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2867, 3072, 3276 }, { 0, 0 } }, 1164, 1306 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 8895, -1690, -0x2C11 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 3891, 3881 }, { 0, 0 } }, 883, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 9657, -1690, -7758 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { ONE, 3891, 3686 }, { 0, 0 } }, 358, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 44, -45, -3977 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3072, 3685, ONE }, { 0, 0 } }, 1231, 1489 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1845, -1110, -3740 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3481, 3788, ONE }, { 0, 0 } }, 802, 1242 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 569, -2975, -4977 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3072, 3684, ONE }, { 0, 0 } }, 1231, 1489 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 569, -2975, -5962 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3072, 3685, ONE }, { 0, 0 } }, 1231, 1489 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 569, -2975, -3542 } }, { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3072, 3684, ONE }, { 0, 0 } }, 1231, 1489 },
};

/// Power Plant 1's shared light collection, containing only point lights.
static WorldCoordRoomLights _gNeoArkPowerPlant1RoomLights = {
    .pointLightCount = ARRAY_SIZE(_gNeoArkPowerPlant1PointLights),
    .pointLights     = _gNeoArkPowerPlant1PointLights,
};

/// Room-space collision normals, with `ONE` for unit length.
static SVECTOR _gNeoArkPowerPlant1CollisionGridNormals[10] = {
#include "assets/neo_ark_power_plant_1_collision_02AD0_normals.inc"
};

/// Room-space collision vertices in signed game-coordinate units.
static SVECTOR _gNeoArkPowerPlant1CollisionGridVerts[58] = {
#include "assets/neo_ark_power_plant_1_collision_02AD0_verts.inc"
};

/// Collision faces indexing this grid's vertex and normal pools.
static WorldCollisionGridFace _gNeoArkPowerPlant1CollisionGridFaces[30] = {
#include "assets/neo_ark_power_plant_1_collision_02AD0_faces.inc"
};

/// Signed face-index lists terminated by `WORLD_COLLISION_GRID_CELL_END`.
static s16 _gNeoArkPowerPlant1CollisionGridCells[160] = {
#include "assets/neo_ark_power_plant_1_collision_02AD0_cells.inc"
};

enum {
    NEO_ARK_POWER_PLANT_1_COLLISION_CELLS_X = 3,
    NEO_ARK_POWER_PLANT_1_COLLISION_CELLS_Z = 4,
};

#define GRID_CELL(i) (&_gNeoArkPowerPlant1CollisionGridCells[i])
/// X-major cell directory; each pointer selects a list in the face-index pool.
static s16* _gNeoArkPowerPlant1CollisionGridTable[NEO_ARK_POWER_PLANT_1_COLLISION_CELLS_X * NEO_ARK_POWER_PLANT_1_COLLISION_CELLS_Z] = {
#include "assets/neo_ark_power_plant_1_collision_02AD0_table.inc"
};
#undef GRID_CELL

/// Power Plant 1's collision mesh and 3-by-4 room-space cell index.
///
/// Cells are 4000 game units wide, starting at X -100 and Z -12000.
/// Activation binds the view coordinate; the room owns all mesh storage.
static WorldCollisionGrid _gNeoArkPowerPlant1CollisionGrid = {
    .viewCoord   = NULL,
    .normals     = _gNeoArkPowerPlant1CollisionGridNormals,
    .vertices    = _gNeoArkPowerPlant1CollisionGridVerts,
    .faces       = _gNeoArkPowerPlant1CollisionGridFaces,
    .cellFaceIds = _gNeoArkPowerPlant1CollisionGridTable,
    .xBias       = 100,
    .zBias       = 12000,
    .cellCountX  = NEO_ARK_POWER_PLANT_1_COLLISION_CELLS_X,
    .cellCountZ  = NEO_ARK_POWER_PLANT_1_COLLISION_CELLS_Z,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gNeoArkPowerPlant1CollisionGridFaces),
};

ViewCamera gNeoArkPowerPlant1ViewCameras[9] = {
    { { { { ONE, 0, 0 }, { 0, 0, -ONE }, { 0, ONE, 0 } }, { -4500, 0x32C8, 6000 } }, 230 },
    { { { { 4004, 0, 861 }, { 547, 3162, -2544 }, { -665, 2602, 3092 } }, { -8176, 2773, 0x300A } }, 230 },
    { { { { 1658, 0, -3745 }, { 676, 4028, 299 }, { 3683, -739, 1630 } }, { -1166, 583, 0x2CFE } }, 257 },
    { { { { 1339, 0, 3870 }, { -701, 4028, 242 }, { -3806, -742, 1317 } }, { -8156, 723, 0x2CFE } }, 257 },
    { { { { -3889, 0, -1283 }, { 218, 4036, -661 }, { 1264, -696, -3833 } }, { -526, 803, 4408 } }, 257 },
    { { { { 3874, 0, -1327 }, { 65, 4091, 190 }, { 1326, -201, 3870 } }, { -526, 1143, 0x2DB2 } }, 257 },
    { { { { 3954, 0, -1065 }, { -7, 4095, -28 }, { 1065, 29, 3954 } }, { -326, 1163, 7568 } }, 257 },
    { { { { 4095, 0, -4 }, { 1, 3851, 1395 }, { 4, -1395, 3851 } }, { -3996, 561, 0x2E85 } }, 230 },
    { { { { -4095, 0, -5 }, { 0, 4055, -571 }, { 5, -571, -4055 } }, { -3996, 1191, 9279 } }, 680 },
};

/// View 1's empty sprite list, retaining the decoded image-strip background.
static SpriteBatch _gNeoArkPowerPlant1View1SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// VRAM texture pages, palette rows and neutral RGB modulation for view sprites.
///
/// Both pages hold 8-bit textures; page X is measured in VRAM halfwords.
enum {
    NEO_ARK_POWER_PLANT_1_SPRITE_LEFT_PAGE          = getTPage(1, 0, 896, 0),
    NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE         = getTPage(1, 0, 960, 0),
    NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255       = getClut(0, 255),
    NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_256       = getClut(0, 256),
    NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION = 128,
};

/// View 2's depth-sorted scenery rectangles, in pixels relative to the view centre.
static SpriteSource _gNeoArkPowerPlant1View2SpriteSources[24] = {
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 32 } }, -160, -64, 1375, { .fields = { 104, 216 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 32 } }, -144, -72, 1500, { .fields = { 88, 0 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 24 } }, -128, -80, 1600, { .fields = { 88, 104 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 24 } }, -112, -88, 1625, { .fields = { 88, 128 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 24 } }, -96, -96, 1650, { .fields = { 88, 56 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 16 } }, -80, -104, 1700, { .fields = { 88, 152 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 16 } }, -64, -112, 1825, { .fields = { 88, 168 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, -48, -120, 1875, { .fields = { 88, 200 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 56 } }, -160, -120, 1625, { .fields = { 112, 0 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 48 } }, -144, -120, 1700, { .fields = { 104, 56 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 40 } }, -128, -120, 1712, { .fields = { 104, 104 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 32 } }, -112, -120, 1787, { .fields = { 104, 184 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 24 } }, -96, -120, 1800, { .fields = { 88, 80 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 16 } }, -80, -120, 1800, { .fields = { 88, 184 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 8 } }, -64, -120, 1825, { .fields = { 104, 248 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 48 } }, -160, -64, 1000, { .fields = { 120, 168 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 56 } }, -152, -72, 1012, { .fields = { 120, 56 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 56 } }, -144, -72, 1050, { .fields = { 120, 112 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 40 } }, -136, -64, 1040, { .fields = { 120, 216 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, -128, -56, 1032, { .fields = { 96, 32 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, -120, -56, 1025, { .fields = { 104, 32 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 40 } }, -112, -56, 1047, { .fields = { 112, 144 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 32 } }, -104, -48, 1050, { .fields = { 104, 0 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 32 } }, -96, -48, 1081, { .fields = { 104, 144 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
};

/// View 2's ranges over all 24 source sprites, following an image-strip selector.
static SpriteBatch _gNeoArkPowerPlant1View2SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 15, 0, 0, { 1, 0 } },
    { 15, ARRAY_SIZE(_gNeoArkPowerPlant1View2SpriteSources) - 15, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 3's depth-sorted scenery rectangles, in pixels relative to the view centre.
static SpriteSource _gNeoArkPowerPlant1View3SpriteSources[36] = {
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -128, 0, 725, { .fields = { 64, 16 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -96, 0, 725, { .fields = { 72, 192 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -160, 8, 775, { .fields = { 48, 152 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -128, 8, 700, { .fields = { 32, 128 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -80, 8, 825, { .fields = { 32, 144 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -72, 16, 850, { .fields = { 48, 8 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -120, 16, 700, { .fields = { 48, 0 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -160, 16, 725, { .fields = { 64, 176 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 32 } }, -24, 40, 1150, { .fields = { 120, 208 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 16 } }, -16, 56, 1150, { .fields = { 120, 240 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 40 } }, 0, 64, 1150, { .fields = { 120, 0 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 40 } }, -8, 64, 1150, { .fields = { 120, 40 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 32 } }, 8, 72, 1150, { .fields = { 112, 96 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 32 } }, 16, 72, 1150, { .fields = { 112, 176 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 32 } }, 40, 72, 1150, { .fields = { 112, 64 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 32 } }, 32, 72, 1150, { .fields = { 104, 64 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 32 } }, 24, 72, 1150, { .fields = { 104, 32 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, -16, 72, 1150, { .fields = { 104, 176 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, -24, 72, 1150, { .fields = { 96, 0 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -160, 24, 675, { .fields = { 80, 128 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -120, 24, 700, { .fields = { 72, 248 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -72, 24, 850, { .fields = { 72, 240 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -72, 32, 775, { .fields = { 72, 136 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, -120, 32, 675, { .fields = { 64, 200 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -160, 32, 650, { .fields = { 64, 184 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 32 } }, -160, 40, 625, { .fields = { 72, 96 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 48, 32 } }, -120, 40, 700, { .fields = { 72, 208 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 48, 32 } }, -72, 40, 750, { .fields = { 80, 144 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, -56, 72, 1150, { .fields = { 72, 48 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 48, 24 } }, -160, 72, 1150, { .fields = { 56, 24 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 56, 24 } }, -112, 72, 1150, { .fields = { 48, 72 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 32 } }, -160, 24, 636, { .fields = { 104, 0 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 32 } }, -144, 24, 666, { .fields = { 120, 80 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 32 } }, -136, 24, 687, { .fields = { 120, 112 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 32 } }, -128, 24, 699, { .fields = { 112, 32 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 32 } }, -120, 24, 688, { .fields = { 120, 176 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
};

/// View 3's ranges over all 36 source sprites, following an image-strip selector.
static SpriteBatch _gNeoArkPowerPlant1View3SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 31, 0, 0, { 1, 0 } },
    { 31, ARRAY_SIZE(_gNeoArkPowerPlant1View3SpriteSources) - 31, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 4's depth-sorted scenery rectangles using palette rows 255 and 256.
static SpriteSource _gNeoArkPowerPlant1View4SpriteSources[85] = {
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, -40, 72, 1475, { .fields = { 88, 40 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, -32, 72, 1475, { .fields = { 104, 72 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, -24, 72, 1475, { .fields = { 96, 72 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, -16, 72, 1475, { .fields = { 96, 208 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, -8, 72, 1475, { .fields = { 96, 232 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 32 } }, 0, 64, 1475, { .fields = { 96, 176 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 32 } }, 8, 32, 1475, { .fields = { 96, 40 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 32 } }, 8, 64, 1475, { .fields = { 104, 208 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 16, 24, 1125, { .fields = { 16, 248 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 48, 8 } }, 48, 24, 1000, { .fields = { 0, 200 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, 96, 24, 1075, { .fields = { 8, 208 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 136, 24, 1025, { .fields = { 24, 216 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 16, 32, 1100, { .fields = { 16, 240 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, 96, 32, 1075, { .fields = { 8, 224 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 136, 32, 975, { .fields = { 24, 152 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, 120, 40, 975, { .fields = { 8, 80 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, 120, 48, 950, { .fields = { 16, 232 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 48, 40 } }, 16, 56, 1475, { .fields = { 72, 96 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 56, 40 } }, 64, 56, 1475, { .fields = { 64, 0 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 40 } }, 120, 56, 1475, { .fields = { 80, 136 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 64, 48, 900, { .fields = { 16, 160 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 96, 48, 900, { .fields = { 24, 184 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 16, 48, 1000, { .fields = { 24, 192 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 40, 48, 1000, { .fields = { 8, 104 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 16, 40, 1025, { .fields = { 8, 96 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 40, 40, 1025, { .fields = { 8, 88 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, 64, 40, 925, { .fields = { 0, 120 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 96, 40, 925, { .fields = { 8, 112 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 48, 32, 975, { .fields = { 16, 176 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 8 } }, 72, 32, 975, { .fields = { 16, 8 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 16 } }, 136, 72, 766, { .fields = { 104, 240 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 40 } }, 128, 48, 773, { .fields = { 112, 176 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 48 } }, 112, 40, 783, { .fields = { 112, 48 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 48 } }, 104, 40, 1047, { .fields = { 120, 0 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 40 } }, 96, 40, 1034, { .fields = { 112, 216 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 16 } }, 88, 64, 976, { .fields = { 48, 216 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 32 } }, 80, 32, 947, { .fields = { 104, 40 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 32 } }, 88, 32, 906, { .fields = { 104, 176 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 16 } }, 80, 64, 964, { .fields = { 40, 0 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 48 } }, 72, 32, 977, { .fields = { 120, 96 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 48 } }, 64, 32, 954, { .fields = { 120, 192 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 48 } }, 56, 32, 958, { .fields = { 120, 144 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_256, { .fields = { 24, 16 } }, 136, 16, 1675, { .fields = { 32, 88 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_256, { .fields = { 24, 16 } }, 136, 0, 1675, { .fields = { 32, 104 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_256, { .fields = { 24, 16 } }, 136, -16, 1675, { .fields = { 32, 120 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_256, { .fields = { 24, 16 } }, 136, -32, 1675, { .fields = { 24, 16 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_256, { .fields = { 24, 16 } }, 136, -48, 1675, { .fields = { 24, 48 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_256, { .fields = { 24, 16 } }, 136, -64, 1675, { .fields = { 24, 64 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_256, { .fields = { 24, 16 } }, 136, -80, 1675, { .fields = { 24, 136 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_256, { .fields = { 24, 8 } }, 80, -88, 1675, { .fields = { 16, 40 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_256, { .fields = { 32, 16 } }, 104, -96, 1675, { .fields = { 64, 240 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_256, { .fields = { 24, 16 } }, 136, -96, 1675, { .fields = { 72, 224 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_256, { .fields = { 24, 16 } }, 136, -112, 1675, { .fields = { 72, 176 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_256, { .fields = { 24, 8 } }, 136, -120, 1675, { .fields = { 16, 168 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_256, { .fields = { 32, 8 } }, 104, -104, 1675, { .fields = { 8, 32 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_256, { .fields = { 16, 8 } }, 104, -112, 1675, { .fields = { 24, 0 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_256, { .fields = { 8, 16 } }, 96, -112, 1675, { .fields = { 120, 240 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, 72, -80, 1675, { .fields = { 80, 64 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, 88, -80, 1675, { .fields = { 80, 80 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, 72, -64, 1675, { .fields = { 64, 56 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, 88, -64, 1675, { .fields = { 64, 72 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, 72, -48, 1675, { .fields = { 72, 40 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, 88, -48, 1675, { .fields = { 80, 208 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, 72, -32, 1675, { .fields = { 80, 192 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, 88, -32, 1675, { .fields = { 48, 72 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, 72, -16, 1675, { .fields = { 48, 136 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, 88, -16, 1675, { .fields = { 48, 56 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, 72, 0, 1675, { .fields = { 48, 0 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, 88, 0, 1675, { .fields = { 48, 16 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, 72, 16, 1675, { .fields = { 48, 240 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, 88, 16, 1675, { .fields = { 40, 32 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, 104, -80, 1675, { .fields = { 48, 200 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, 120, -80, 1675, { .fields = { 48, 152 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, 104, -64, 1675, { .fields = { 48, 184 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, 120, -64, 1675, { .fields = { 64, 208 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, 104, -48, 1675, { .fields = { 56, 40 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, 120, -48, 1675, { .fields = { 64, 192 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, 104, -32, 1675, { .fields = { 64, 136 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, 120, -32, 1675, { .fields = { 64, 152 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, 104, -16, 1675, { .fields = { 56, 168 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, 120, -16, 1675, { .fields = { 56, 224 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, 104, 0, 1675, { .fields = { 56, 120 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, 120, 0, 1675, { .fields = { 56, 88 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, 104, 16, 1675, { .fields = { 56, 104 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_256, { .fields = { 16, 16 } }, 120, 16, 1675, { .fields = { 40, 168 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
};

/// View 4's ranges over all 85 source sprites, following an image-strip selector.
static SpriteBatch _gNeoArkPowerPlant1View4SpriteBatches[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 30, 0, 0, { 2, 0 } },
    { 30, 12, 0, 0, { 1, 0 } },
    { 42, ARRAY_SIZE(_gNeoArkPowerPlant1View4SpriteSources) - 42, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 5's depth-sorted scenery rectangles spanning the two texture pages.
static SpriteSource _gNeoArkPowerPlant1View5SpriteSources[49] = {
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 32 } }, 24, 72, 1225, { .fields = { 8, 80 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 32 } }, 16, 72, 1225, { .fields = { 16, 120 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 32 } }, 8, 72, 1225, { .fields = { 32, 224 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 32 } }, 0, 72, 1225, { .fields = { 56, 224 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 32 } }, -8, 72, 1225, { .fields = { 64, 224 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 40 } }, -16, 64, 1225, { .fields = { 48, 120 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 48 } }, -24, 56, 1225, { .fields = { 88, 0 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 64 } }, -32, 40, 975, { .fields = { 120, 0 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 40 } }, -48, 24, 900, { .fields = { 56, 144 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 40 } }, -48, 64, 900, { .fields = { 16, 200 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_LEFT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 40 } }, -88, 24, 775, { .fields = { 120, 160 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 40 } }, -88, 64, 775, { .fields = { 32, 0 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 40 } }, -128, 24, 750, { .fields = { 24, 40 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 40 } }, -128, 64, 750, { .fields = { 32, 184 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 40 } }, -160, 24, 725, { .fields = { 72, 216 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 40 } }, -160, 64, 725, { .fields = { 32, 80 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_LEFT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -88, 16, 750, { .fields = { 104, 232 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_LEFT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -128, 16, 725, { .fields = { 104, 224 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_LEFT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -160, 16, 700, { .fields = { 112, 216 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_LEFT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -160, 8, 500, { .fields = { 112, 208 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_LEFT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -128, 8, 600, { .fields = { 104, 200 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_LEFT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -88, 8, 625, { .fields = { 104, 144 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_LEFT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -160, 0, 500, { .fields = { 104, 32 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -120, 0, 525, { .fields = { 0, 248 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_LEFT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 8 } }, -160, -8, 450, { .fields = { 120, 240 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_LEFT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 8 } }, -120, -8, 450, { .fields = { 120, 152 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 48 } }, -40, -120, 700, { .fields = { 72, 0 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 48 } }, -40, -72, 650, { .fields = { 88, 168 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, -40, -24, 650, { .fields = { 112, 232 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 48 } }, -56, -120, 650, { .fields = { 72, 168 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 48 } }, -56, -72, 650, { .fields = { 64, 96 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 32 } }, -56, -24, 575, { .fields = { 0, 0 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 40 } }, -72, -32, 600, { .fields = { 16, 0 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, -72, -56, 500, { .fields = { 120, 232 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 32 } }, -72, -120, 525, { .fields = { 0, 112 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 32 } }, -72, -88, 500, { .fields = { 40, 224 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 48 } }, -88, -120, 425, { .fields = { 64, 48 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 40 } }, -88, -72, 425, { .fields = { 16, 80 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 40 } }, -88, -32, 775, { .fields = { 8, 40 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 56 } }, -112, -120, 575, { .fields = { 104, 176 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 56 } }, -112, -64, 450, { .fields = { 80, 56 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 56 } }, -136, -120, 400, { .fields = { 96, 0 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 56 } }, -136, -64, 625, { .fields = { 80, 112 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 56 } }, -160, -120, 400, { .fields = { 104, 120 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 56 } }, -160, -64, 550, { .fields = { 104, 64 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 24, 40 } }, -32, 40, 1179, { .fields = { 24, 120 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, -40, 40, 924, { .fields = { 104, 232 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, -48, 40, 893, { .fields = { 72, 144 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 24 } }, -64, 40, 794, { .fields = { 40, 160 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
};

/// View 5's ranges over all 49 source sprites, following an image-strip selector.
static SpriteBatch _gNeoArkPowerPlant1View5SpriteBatches[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 45, 0, 0, { 1, 0 } },
    { 45, ARRAY_SIZE(_gNeoArkPowerPlant1View5SpriteSources) - 45, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 6's Life Support rectangle followed by 16 other depth-sorted scenery sprites.
static SpriteSource _gNeoArkPowerPlant1View6SpriteSources[17] = {
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 40 } }, -64, -8, 2125, { .fields = { 72, 144 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 24 } }, 72, 16, 962, { .fields = { 120, 232 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 80 } }, 152, 16, 755, { .fields = { 120, 0 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 80 } }, 144, 16, 775, { .fields = { 120, 80 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 72 } }, 136, 16, 796, { .fields = { 120, 160 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 72 } }, 128, 16, 818, { .fields = { 112, 0 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 64 } }, 120, 16, 816, { .fields = { 112, 72 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 64 } }, 112, 16, 806, { .fields = { 112, 136 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 48 } }, 104, 16, 825, { .fields = { 104, 96 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 48 } }, 96, 16, 818, { .fields = { 112, 200 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 32 } }, 88, 16, 1045, { .fields = { 96, 96 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 32 } }, 80, 16, 1087, { .fields = { 88, 0 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 32 } }, 128, 32, 750, { .fields = { 96, 184 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 32 } }, 112, 32, 737, { .fields = { 96, 216 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 24 } }, 96, 40, 725, { .fields = { 80, 32 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 48 } }, 80, 40, 712, { .fields = { 96, 48 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 48 } }, 64, 40, 700, { .fields = { 96, 0 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
};

/// View 6's image-strip selector and sprite ranges; batch 1 controls Life Support.
///
/// Life Support visibility changes while its generator part is present.
static SpriteBatch _gNeoArkPowerPlant1View6SpriteBatches[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 1, 0 } },
    { 1, 11, 0, 0, { 2, 0 } },
    { 12, ARRAY_SIZE(_gNeoArkPowerPlant1View6SpriteSources) - 12, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 7's seven Life Support strips, sharing a sorting depth of 1125.
static SpriteSource _gNeoArkPowerPlant1View7SpriteSources[7] = {
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 8, 96 } }, 96, -56, 1125, { .fields = { 88, 0 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 96 } }, 80, -56, 1125, { .fields = { 80, 96 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 96 } }, 64, -56, 1125, { .fields = { 72, 0 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 96 } }, 48, -56, 1125, { .fields = { 96, 96 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 96 } }, 32, -56, 1125, { .fields = { 112, 0 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 96 } }, 16, -56, 1125, { .fields = { 112, 96 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 16, 96 } }, 0, -56, 1125, { .fields = { 96, 0 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
};

/// View 7's image-strip selector and Life Support range, whose visibility changes.
static SpriteBatch _gNeoArkPowerPlant1View7SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gNeoArkPowerPlant1View7SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 8's depth-sorted scenery rectangles, in pixels relative to the view centre.
static SpriteSource _gNeoArkPowerPlant1View8SpriteSources[12] = {
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 32 } }, -144, 64, 650, { .fields = { 96, 192 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, -112, 64, 650, { .fields = { 48, 24 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 24 } }, 80, 64, 650, { .fields = { 48, 0 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 32, 32 } }, 112, 64, 650, { .fields = { 96, 224 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, 112, 96, 625, { .fields = { 56, 240 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 16 } }, -152, 96, 637, { .fields = { 40, 48 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 24 } }, -72, 32, 575, { .fields = { 40, 128 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 64, 24 } }, -32, 32, 575, { .fields = { 32, 192 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 40, 24 } }, 32, 32, 575, { .fields = { 56, 216 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 48, 64 } }, -80, 56, 525, { .fields = { 80, 128 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 64, 64 } }, -32, 56, 525, { .fields = { 64, 64 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
    { NEO_ARK_POWER_PLANT_1_SPRITE_RIGHT_PAGE, NEO_ARK_POWER_PLANT_1_SPRITE_CLUT_ROW_255, { .fields = { 48, 64 } }, 32, 56, 525, { .fields = { 80, 0 } }, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, NEO_ARK_POWER_PLANT_1_SPRITE_NEUTRAL_MODULATION, 0 },
};

/// View 8's complete source range, following an image-strip selector.
static SpriteBatch _gNeoArkPowerPlant1View8SpriteBatches[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, ARRAY_SIZE(_gNeoArkPowerPlant1View8SpriteSources), 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

/// View 9's empty sprite list, retaining the decoded image-strip background.
static SpriteBatch _gNeoArkPowerPlant1View9SpriteBatches[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView gNeoArkPowerPlant1SpriteViews[9] = {
    { { .empty = _gNeoArkPowerPlant1View1SpriteBatches }, _gNeoArkPowerPlant1View1SpriteBatches, NULL },
    { { .elements = _gNeoArkPowerPlant1View2SpriteSources }, _gNeoArkPowerPlant1View2SpriteBatches, NULL },
    { { .elements = _gNeoArkPowerPlant1View3SpriteSources }, _gNeoArkPowerPlant1View3SpriteBatches, NULL },
    { { .elements = _gNeoArkPowerPlant1View4SpriteSources }, _gNeoArkPowerPlant1View4SpriteBatches, NULL },
    { { .elements = _gNeoArkPowerPlant1View5SpriteSources }, _gNeoArkPowerPlant1View5SpriteBatches, NULL },
    { { .elements = _gNeoArkPowerPlant1View6SpriteSources }, _gNeoArkPowerPlant1View6SpriteBatches, NULL },
    { { .elements = _gNeoArkPowerPlant1View7SpriteSources }, _gNeoArkPowerPlant1View7SpriteBatches, NULL },
    { { .elements = _gNeoArkPowerPlant1View8SpriteSources }, _gNeoArkPowerPlant1View8SpriteBatches, NULL },
    { { .empty = _gNeoArkPowerPlant1View9SpriteBatches }, _gNeoArkPowerPlant1View9SpriteBatches, NULL },
};

WorldCollisionTrigger D_neo_ark_power_plant_1_8018155C[10] = {
    { NULL, NULL, NULL, { 7775, -1440, -0x2A92, 0 }, { { 2378, -1904, -824, 0 }, { -2377, -1904, 825, 0 }, { 2378, 1904, -824, 0 }, { -2377, 1904, 825, 0 } }, { 1345, 0, 3880, 0 }, { 0, 0, 4096, 0 }, 3145, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 7838, -1440, -0x2A22, 0 }, { { -2377, -1904, 825, 0 }, { 2378, -1904, -824, 0 }, { -2377, 1904, 825, 0 }, { 2378, 1904, -824, 0 } }, { -1347, 0, -3882, 0 }, { 0, 0, 4096, 0 }, 3145, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4416, -1376, -0x2C41, 0 }, { { -24, -1904, 2517, 0 }, { 25, -1904, -2516, 0 }, { -24, 1904, 2517, 0 }, { 25, 1904, -2516, 0 } }, { -4109, 0, -41, 0 }, { 0, 0, 4096, 0 }, 3145, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4255, -1376, -0x2C22, 0 }, { { 25, -1904, -2516, 0 }, { -24, -1904, 2517, 0 }, { 25, 1904, -2516, 0 }, { -24, 1904, 2517, 0 } }, { 4107, 0, 39, 0 }, { 0, 0, 4096, 0 }, 3145, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 638, -1408, -0x2D02, 0 }, { { 1762, -1904, 1797, 0 }, { -1762, -1904, -1796, 0 }, { 1762, 1904, 1797, 0 }, { -1762, 1904, -1796, 0 } }, { -2934, 0, 2876, 0 }, { 0, 0, 4096, 0 }, 3145, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 639, -1440, -0x2C42, 0 }, { { -1762, -1904, -1796, 0 }, { 1762, -1904, 1797, 0 }, { -1762, 1904, -1796, 0 }, { 1762, 1904, 1797, 0 } }, { 2932, 0, -2877, 0 }, { 0, 0, 4096, 0 }, 3145, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 607, -1440, -8418, 0 }, { { 2516, -1904, 25, 0 }, { -2516, -1904, -24, 0 }, { 2516, 1904, 25, 0 }, { -2516, 1904, -24, 0 } }, { -41, 0, 4106, 0 }, { 0, 0, 4096, 0 }, 3145, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 575, -1376, -8290, 0 }, { { -2516, -1904, -24, 0 }, { 2516, -1904, 25, 0 }, { -2516, 1904, -24, 0 }, { 2516, 1904, 25, 0 } }, { 39, 0, -4108, 0 }, { 0, 0, 4096, 0 }, 3145, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 574, -1440, -5571, 0 }, { { -2516, -1904, 11, 0 }, { 2516, -1904, -10, 0 }, { -2516, 1904, 11, 0 }, { 2516, 1904, -10, 0 } }, { -18, 0, -4108, 0 }, { 0, 0, 4096, 0 }, 3145, 0, 6, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 574, -1312, -5761, 0 }, { { 2517, -1904, 17, 0 }, { -2516, -1904, -17, 0 }, { 2517, 1904, 17, 0 }, { -2516, 1904, -17, 0 } }, { -29, 0, 4107, 0 }, { 0, 0, 4096, 0 }, 3145, 0, 7, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_neo_ark_power_plant_1_80181854[7] = {
    { NULL, NULL, NULL, { 8656, -62, -0x2860, 0 }, { { -400, 0, -544, 0 }, { 400, 0, -544, 0 }, { -400, 0, 544, 0 }, { 400, 0, 544, 0 } }, { 0, 4102, 0, 0 }, { -4096, 0, 0, 0 }, 674, WORLD_COLLISION_TRIGGER_ACTION_WARP, 21, 17, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1120, -64, -4096, 0 }, { { -400, 0, -1024, 0 }, { 400, 0, -1024, 0 }, { -400, 0, 1024, 0 }, { 400, 0, 1024, 0 } }, { 0, 4098, 0, 0 }, { -4096, 0, 0, 0 }, 1093, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1024, -64, -8256, 0 }, { { -400, 0, -3008, 0 }, { 400, 0, -3008, 0 }, { -400, 0, 3008, 0 }, { 400, 0, 3008, 0 } }, { 0, 4102, 0, 0 }, { -4096, 0, 0, 0 }, 3029, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3968, -64, -0x2A90, 0 }, { { -3280, 0, -368, 0 }, { 3280, 0, -368, 0 }, { -3280, 0, 368, 0 }, { 3280, 0, 368, 0 } }, { 0, 4112, 0, 0 }, { 0, 0, -4096, 0 }, 3298, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 752, -64, -3408, 0 }, { { -800, 0, -368, 0 }, { 800, 0, -368, 0 }, { -800, 0, 368, 0 }, { 800, 0, 368, 0 } }, { 0, 4106, 0, 0 }, { 0, 0, -4096, 0 }, 879, WORLD_COLLISION_TRIGGER_ACTION_CAP, 12, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 8304, -64, -8576, 0 }, { { -720, 0, -368, 0 }, { 720, 0, -368, 0 }, { -720, 0, 368, 0 }, { 720, 0, 368, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -4096, 0 }, 807, WORLD_COLLISION_TRIGGER_ACTION_CAP, 9, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 6816, -64, -8544, 0 }, { { -720, 0, -368, 0 }, { 720, 0, -368, 0 }, { -720, 0, 368, 0 }, { 720, 0, 368, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -4096, 0 }, 807, WORLD_COLLISION_TRIGGER_ACTION_CAP, 8, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Variant 1's Proto Generator and Watcher files and spawn descriptors.
///
/// Entry IDs 54 and 21 match this variant's placements; the final entry ends the list.
static AreaResource _gNeoArkPowerPlant1Variant1Resources[3] = {
    { .entryId = 54, .fileNumber = 54, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = GENERATOR_BODY_TASK_INDEX, .taskTable = gActor105400GeneratorTasks },
    { .entryId = 21, .fileNumber = 21, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_20, .taskIndex = 0, .taskTable = &gActor202100WatcherTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Variant 2's Black Beetle file and spawn descriptor, keyed by placement entry 38.
static AreaResource _gNeoArkPowerPlant1Variant2Resources[2] = {
    { .entryId = 38, .fileNumber = 38, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .taskTable = &gActor103800BlackBeetleTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

AreaResource D_neo_ark_power_plant_1_80181AA4[2] = {
    { 38, 38, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor103800BlackBeetleTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_neo_ark_power_plant_1_80181ABC[3] = {
    { 25, 25, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, gActor102500Tasks },
    { 38, 38, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &gActor203800BlackBeetleTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_neo_ark_power_plant_1_80181AE0[2] = {
    { 38, 38, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &gActor103800BlackBeetleTaskDesc },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_neo_ark_power_plant_1_80181AF8[13] = {
    { NULL, NULL },
    { D_map_neo_ark_8017B8D0, _gNeoArkPowerPlant1Variant1Resources },
    { D_map_neo_ark_8017B950, _gNeoArkPowerPlant1Variant2Resources },
    { D_map_neo_ark_8017B9E0, D_neo_ark_power_plant_1_80181AA4 },
    { D_map_neo_ark_8017BA60, D_neo_ark_power_plant_1_80181ABC },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_neo_ark_8017BB00, D_neo_ark_power_plant_1_80181AE0 },
    { NULL, NULL },
};

WorldCollisionOccluder D_neo_ark_power_plant_1_80181B60[1] = {
    { NULL, NULL, { 2080, -2432, -5472, 0 }, { { 384, 3456, 2880, 0 }, { -384, 3456, -2880, 0 }, { 384, -3456, 2880, 0 }, { -384, -3456, -2880, 0 } }, { 4075, 0, -544, 0 }, 4492, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

/// Cue-2 bases for the two audible collision surface classes; cue 1 adds one.
enum {
    NEO_ARK_POWER_PLANT_1_SURFACE_4_WALK_SOUND = 0x10000059,
    NEO_ARK_POWER_PLANT_1_SURFACE_4_RUN_SOUND  = 0x1000005B,
    NEO_ARK_POWER_PLANT_1_SURFACE_5_WALK_SOUND = 0x1000005D,
    NEO_ARK_POWER_PLANT_1_SURFACE_5_RUN_SOUND  = 0x1000005F,
};

// Keep these read-only cue tables in the original room-data section.
const WorldCollisionFootstepSounds gNeoArkPowerPlant1Surface4FootstepSounds __attribute__((section(".data"))) = {
    .walk         = NEO_ARK_POWER_PLANT_1_SURFACE_4_WALK_SOUND,
    .run          = NEO_ARK_POWER_PLANT_1_SURFACE_4_RUN_SOUND,
    .scriptedJump = NEO_ARK_POWER_PLANT_1_SURFACE_4_WALK_SOUND,
};

const WorldCollisionFootstepSounds gNeoArkPowerPlant1Surface5FootstepSounds __attribute__((section(".data"))) = {
    .walk         = NEO_ARK_POWER_PLANT_1_SURFACE_5_WALK_SOUND,
    .run          = NEO_ARK_POWER_PLANT_1_SURFACE_5_RUN_SOUND,
    .scriptedJump = NEO_ARK_POWER_PLANT_1_SURFACE_5_WALK_SOUND,
};

/// Draws the intact generator's glow or emits flashes from its destroyed support.
///
/// Draws at the fixed world-space emitter with RGB444 colour 0x334 and radius
/// scale 768. After destruction, samples the shared unsigned LCG once per call
/// only while effects run and the plant remains uncleared; three high-word bits
/// give a one-in-eight flash chance, with base size 1024 and no parent coordinate.
/// Requires live effect state and loaded room emitter/effect resources. A flash
/// retains the emitter address; keep the room loaded until the effect ends.
static inline void _neoArkPowerPlant1DrawGeneratorGlow(void)
{
    enum {
        NEO_ARK_POWER_PLANT_1_GENERATOR_GLOW_RADIUS_SCALE = 768,
        NEO_ARK_POWER_PLANT_1_GENERATOR_GLOW_RGB444       = 0x334,
        NEO_ARK_POWER_PLANT_1_GENERATOR_FLASH_BASE_SIZE   = 1024,
        NEO_ARK_POWER_PLANT_1_GENERATOR_FLASH_CHANCE_MASK = 7,
    };
    if (gameFlagGetNibble(GAME_FLAG_POWER_PLANT_1_GENERATOR_PART_DOWN) != 0) {
        if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING && gameFlagGetNibble(GAME_FLAG_NEO_ARK_POWER_PLANT_1_CLEARED) == 0) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if (((gRandomLcgState >> 16) & NEO_ARK_POWER_PLANT_1_GENERATOR_FLASH_CHANCE_MASK) == 0) {
                effectSpawn(EFFECT_FLASH_BURST, NULL, NEO_ARK_POWER_PLANT_1_GENERATOR_FLASH_BASE_SIZE, &D_neo_ark_power_plant_1_8017F1C0);
            }
        }
    } else {
        glowDrawDisc(&D_neo_ark_power_plant_1_8017F1C0, NEO_ARK_POWER_PLANT_1_GENERATOR_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GENERATOR_GLOW_RGB444);
    }
}

void neoArkPowerPlant1DrawLightGlowsTask(Task* unusedTask)
{
    enum {
        NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE = 512,
        NEO_ARK_POWER_PLANT_1_LARGE_GLOW_RADIUS_SCALE = 768,
        NEO_ARK_POWER_PLANT_1_GLOW_FAINT_CYAN_RGB444  = 0x011,
        NEO_ARK_POWER_PLANT_1_GLOW_DIM_RGB444         = 0x122,
        NEO_ARK_POWER_PLANT_1_GLOW_MEDIUM_RGB444      = 0x233,
        NEO_ARK_POWER_PLANT_1_GLOW_BRIGHT_RGB444      = 0x344,
        NEO_ARK_POWER_PLANT_1_GLOW_LARGE_RGB444       = 0x334,
        NEO_ARK_POWER_PLANT_1_GLOW_LARGE_DIM_RGB444   = 0x223,
    };

    // The low byte selects a fixed, in-bounds set of world-space emitters.
    switch (viewGetMappedIndex() & 0xFF) {
        case 2:
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[30], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_BRIGHT_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[32], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_BRIGHT_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[35], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_BRIGHT_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[36], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_BRIGHT_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[37], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_BRIGHT_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[38], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_BRIGHT_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[39], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_BRIGHT_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[40], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_BRIGHT_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[41], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_BRIGHT_RGB444);
            break;
        case 3:
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[51], NEO_ARK_POWER_PLANT_1_LARGE_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_LARGE_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[26], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_BRIGHT_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[27], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_BRIGHT_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[28], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_BRIGHT_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[29], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_BRIGHT_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[30], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_MEDIUM_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[31], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_MEDIUM_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[32], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_MEDIUM_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[33], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_MEDIUM_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[34], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_DIM_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[35], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_DIM_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[36], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_DIM_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[37], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_DIM_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[39], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_DIM_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[41], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_DIM_RGB444);
            break;
        case 4:
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[42], NEO_ARK_POWER_PLANT_1_LARGE_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_LARGE_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[45], NEO_ARK_POWER_PLANT_1_LARGE_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_LARGE_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[46], NEO_ARK_POWER_PLANT_1_LARGE_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_LARGE_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[48], NEO_ARK_POWER_PLANT_1_LARGE_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_LARGE_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[49], NEO_ARK_POWER_PLANT_1_LARGE_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_LARGE_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[16], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_DIM_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[17], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_DIM_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[18], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_DIM_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[19], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_DIM_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[20], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_DIM_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[21], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_DIM_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[22], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_DIM_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[23], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_DIM_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[24], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_MEDIUM_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[25], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_MEDIUM_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[26], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_MEDIUM_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[27], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_MEDIUM_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[28], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_BRIGHT_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[29], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_BRIGHT_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[30], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_BRIGHT_RGB444);
            break;
        case 5:
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[48], NEO_ARK_POWER_PLANT_1_LARGE_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_LARGE_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[49], NEO_ARK_POWER_PLANT_1_LARGE_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_LARGE_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[12], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_BRIGHT_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[13], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_BRIGHT_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[14], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_BRIGHT_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[15], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_BRIGHT_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[16], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_MEDIUM_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[17], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_MEDIUM_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[18], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_MEDIUM_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[19], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_MEDIUM_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[20], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_DIM_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[21], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_DIM_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[22], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_DIM_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[23], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_DIM_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[24], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_DIM_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[25], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_DIM_RGB444);
            break;
        case 6:
            _neoArkPowerPlant1DrawGeneratorGlow();
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[42], NEO_ARK_POWER_PLANT_1_LARGE_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_LARGE_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[43], NEO_ARK_POWER_PLANT_1_LARGE_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_LARGE_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[0], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_FAINT_CYAN_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[1], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_FAINT_CYAN_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[2], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_FAINT_CYAN_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[3], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_FAINT_CYAN_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[4], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_DIM_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[5], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_DIM_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[6], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_DIM_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[7], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_DIM_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[8], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_MEDIUM_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[9], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_MEDIUM_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[10], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_MEDIUM_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[11], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_MEDIUM_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[12], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_BRIGHT_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[13], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_BRIGHT_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[14], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_BRIGHT_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[15], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_BRIGHT_RGB444);
            break;
        case 7:
            _neoArkPowerPlant1DrawGeneratorGlow();
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[0], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_BRIGHT_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[1], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_BRIGHT_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[2], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_BRIGHT_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[3], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_BRIGHT_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[4], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_BRIGHT_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[5], NEO_ARK_POWER_PLANT_1_SMALL_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_BRIGHT_RGB444);
            break;
        case 8:
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[42], NEO_ARK_POWER_PLANT_1_LARGE_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_LARGE_DIM_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[43], NEO_ARK_POWER_PLANT_1_LARGE_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_LARGE_DIM_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[44], NEO_ARK_POWER_PLANT_1_LARGE_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_LARGE_DIM_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[45], NEO_ARK_POWER_PLANT_1_LARGE_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_LARGE_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[46], NEO_ARK_POWER_PLANT_1_LARGE_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_LARGE_RGB444);
            glowDrawDisc(&D_neo_ark_power_plant_1_8017F020[47], NEO_ARK_POWER_PLANT_1_LARGE_GLOW_RADIUS_SCALE, NEO_ARK_POWER_PLANT_1_GLOW_LARGE_RGB444);
            break;
    }
}

#include "../../shared/glow_draw_disc.inc.c"

void neoArkPowerPlant1SetLifeSupportSpritesHidden(s32 hidden)
{
    GameLocationKey* location;
    SpriteView*      areaViews;
    SpriteBatch*     batches;
    s32              hiddenByte;

    location   = &gGameSession->location.loc;
    areaViews  = gSpriteAreaTables[location->stage - 1]->areaViews[location->area - 1];
    hiddenByte = hidden & 0xFF;

    if (hiddenByte == 0) {
        batches           = areaViews[5].batches;
        batches[1].hidden = 0;
        batches           = areaViews[6].batches;
        batches[1].hidden = 0;
        return;
    }
    if (hiddenByte == 1) {
        batches           = areaViews[5].batches;
        batches[1].hidden = hiddenByte;
        batches           = areaViews[6].batches;
        batches[1].hidden = hiddenByte;
    }
}
