#include "rooms/acropolis_helicopter_landing_pad.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "acropolis_helicopter_landing_pad_private.h"

#include "actors/actor_511000.h"

#include "gameplay/companion_load.h"
#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"
#include "gameplay/scene_combat.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "overlay.h"

#include "rooms/room_common.h"
#include "../../shared/actor_contacts.h"

/// Returns this carrier's persistent last contact-push correction.
///
/// Components are signed 16.16 corrections shifted right by 16 and narrowed
/// to halfwords. Fractional X/Z add a further unit in the correction's sign;
/// X/Z record the root correction, while Y is only recorded. No grid hit
/// leaves the old value intact. The borrowed vector lives for the overlay's
/// lifetime; `pad` is unused.
static inline SVECTOR* _actorContactGetLastPushStep(void)
{
    return &ActorContact_ScratchPosition;
}

/// Converts one local spark endpoint to signed 16-bit view coordinates in place.
///
/// Borrows an EffectLineScratch block, index 0 or 1, and a composed GfxCoord.
/// Arguments are evaluated repeatedly and must be side-effect-free; captures
/// no locals. Changes GTE state and narrows XYZ sums to s16. Expands to several
/// statements and is used only at standalone call sites in the two drawers.
#define ACROPOLIS_HELICOPTER_LANDING_PAD_TRANSFORM_SPARK_ENDPOINT(line, endpointIndex, coord) \
    gte_SetRotMatrix(&(coord)->workm);                                                        \
    gte_ldv0(&(line)->endpoints[endpointIndex]);                                              \
    gte_rtv0();                                                                               \
    gte_stsv(&(line)->endpoints[endpointIndex]);                                              \
    (line)->endpoints[endpointIndex].vx += (coord)->workm.t[0];                               \
    (line)->endpoints[endpointIndex].vy += (coord)->workm.t[1];                               \
    (line)->endpoints[endpointIndex].vz += (coord)->workm.t[2]

extern AnimationSet* D_acropolis_helicopter_landing_pad_801838F4[3];

extern SVECTOR D_acropolis_helicopter_landing_pad_80184E80[12];
extern s32     D_acropolis_helicopter_landing_pad_80184EE0[12];

// The two sprite tasks use signed Q12 trig results and the same texture page.
enum {
    ACROPOLIS_HELICOPTER_LANDING_PAD_SPRITE_TEXTURE_PAGE       = 0x2B,
    ACROPOLIS_HELICOPTER_LANDING_PAD_SPRITE_TRIG_FRACTION_BITS = 12,
    ACROPOLIS_HELICOPTER_LANDING_PAD_SPRITE_RAW_TEXTURE_CODE   = 0x2D,
};

// Transient-light RGB uses twelve fractional bits; blue varies from 9/16 to 1.
enum {
    ACROPOLIS_HELICOPTER_LANDING_PAD_LIGHT_HALF_INTENSITY   = ONE / 2,
    ACROPOLIS_HELICOPTER_LANDING_PAD_LIGHT_BLUE_RANDOM_MASK = 7 * ONE / 16,
    ACROPOLIS_HELICOPTER_LANDING_PAD_LIGHT_BLUE_BASE        = 9 * ONE / 16,
};

static void _acropolisHelicopterLandingPadResolveExit(Task* task);
static void _acropolisHelicopterLandingPadTurnPlayerToExit(Task* task);
static void _acropolisHelicopterLandingPadDescendExitStairs(Task* task);
static void _acropolisHelicopterLandingPadWaitForPlayerTurn(Task* task);
static void _acropolisHelicopterLandingPadDrawPerimeterLight(const SVECTOR* worldPoint, s16 lightIndex, s32 brightness);
static void _acropolisHelicopterLandingPadDrawUpperSparkLine(GfxCoord* coord);

static void _acropolisHelicopterLandingPadMovieTask(Task* task);
static void _acropolisHelicopterLandingPadStartMovieTask(Task* task);

extern WorldCollisionGrid D_acropolis_helicopter_landing_pad_80185998[1];

extern WorldCollisionTrigger D_acropolis_helicopter_landing_pad_801859BC[16];

TaskMessageEntry D_acropolis_helicopter_landing_pad_80183710[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, acropolisHelicopterLandingPadResolveDeparture },
    { ROOM_MESSAGE_USE_KEY_ITEM, acropolisHelicopterLandingPadRefuseKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, acropolisHelicopterLandingPadHandleRoomAction },
    { ROOM_MESSAGE_COMMAND, acropolisHelicopterLandingPadHandleCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

PadScriptCmd D_acropolis_helicopter_landing_pad_80183738[4] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 1), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_LOOP, 3), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 4), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

PadScriptVibrationSegment D_acropolis_helicopter_landing_pad_80183748[2] = {
    { 0, 0, 1, 0 },
    { 255, 255, 4, 1 },
};

ActorTransform D_acropolis_helicopter_landing_pad_80183750 = { { 0, 0, 0, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_acropolis_helicopter_landing_pad_80183768 = { { 3000, 0, -6692, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_acropolis_helicopter_landing_pad_80183780 = { { 3000, 0, -6692, 0 }, { 0, -1204, 0, 0 } };

ActorTransform D_acropolis_helicopter_landing_pad_80183798 = { { 3000, 0, -6692, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_acropolis_helicopter_landing_pad_801837B0 = { { 0, 0, 0, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_acropolis_helicopter_landing_pad_801837C8 = { { -3975, -2871, -1454, 0 }, { 0, 808, 0, 0 } };

s32 D_acropolis_helicopter_landing_pad_801837E0[18] = {
    -3496,
    -2871,
    -1283,
    0,
    0x3280000,
    0,
    -360,
    -120,
    480,
    0,
    0,
    0,
    -3188,
    -2871,
    -694,
    0,
    0x9C40000,
    0,
};

ActorTransform D_acropolis_helicopter_landing_pad_80183828 = { { -2176, 0, -6845, 0 }, { 0, 1024, 0, 0 } };

AnimationPlayRequest D_acropolis_helicopter_landing_pad_80183840 = { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 1, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_helicopter_landing_pad_80183854 = { { .index = 0 }, 2, ANIMATION_BLEND_RESET, 1, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_helicopter_landing_pad_80183868 = { { .index = 0 }, 3, ANIMATION_BLEND_RESET, 1, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_helicopter_landing_pad_8018387C = { { .index = 0 }, 4, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_helicopter_landing_pad_80183890 = { { .index = 0 }, 5, ANIMATION_BLEND_RESET, 1, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_helicopter_landing_pad_801838A4 = { { .index = 0 }, 6, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_helicopter_landing_pad_801838B8 = { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_helicopter_landing_pad_801838CC = { { .index = 0 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_helicopter_landing_pad_801838E0 = { { .index = 0 }, 3, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationSet* D_acropolis_helicopter_landing_pad_801838F4[3] = {
    NULL,
    &gActor511000Animation04CC8,
    &gActor511000Animation07ADC,
};

AnimationBankCopyRequest D_acropolis_helicopter_landing_pad_80183900 = { { .sets = D_acropolis_helicopter_landing_pad_801838F4 }, ARRAY_SIZE(D_acropolis_helicopter_landing_pad_801838F4) };

AnimationPlayRequest D_acropolis_helicopter_landing_pad_80183908 = { { .index = 1 }, 47, ANIMATION_BLEND_INTERPOLATE, 5, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_helicopter_landing_pad_8018391C = { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_helicopter_landing_pad_80183930 = { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_helicopter_landing_pad_80183944 = { { .index = 1 }, 1, ANIMATION_BLEND_INTERPOLATE, 7, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_helicopter_landing_pad_80183958 = { { .index = 1 }, 9, ANIMATION_BLEND_INTERPOLATE, 6, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_helicopter_landing_pad_8018396C = { { .index = 1 }, 9, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

EvsSceneKey D_acropolis_helicopter_landing_pad_80183980 = { 1, 9, 11 };

EvsSceneKey D_acropolis_helicopter_landing_pad_80183988 = { 1, 9, 21 };

EvsSceneKey D_acropolis_helicopter_landing_pad_80183990 = { 1, 10, 11 };

AnimationPlayRequest D_acropolis_helicopter_landing_pad_80183998 = { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 1, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_helicopter_landing_pad_801839AC = { { .index = 0 }, 2, ANIMATION_BLEND_RESET, 1, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_helicopter_landing_pad_801839C0 = { { .index = 0 }, 3, ANIMATION_BLEND_RESET, 1, ANIMATION_WORLD_COLLISION_DISABLE };

ActorCommand D_acropolis_helicopter_landing_pad_801839D4 = { { .loc = { 1, 16 } }, 0 };

ActorCommand D_acropolis_helicopter_landing_pad_801839D8 = { { .loc = { 1, 16 } }, 1 };

AnimationPlayRequest D_acropolis_helicopter_landing_pad_801839DC = { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_acropolis_helicopter_landing_pad_801839F0 = { { .index = 0 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

EvsCommand D_acropolis_helicopter_landing_pad_80183A04[2] = {
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x51100005 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_acropolis_helicopter_landing_pad_80183A34[58] = {
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = 0x51100005 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_FRAMEBUFFER_BLEND, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2007 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_acropolis_helicopter_landing_pad_80183980 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_acropolis_helicopter_landing_pad_80183750 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_helicopter_landing_pad_80183840 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_VIBRATION, { .padCommands = D_acropolis_helicopter_landing_pad_80183738 }, { .vibrationSegments = D_acropolis_helicopter_landing_pad_80183748 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x51100006 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_helicopter_landing_pad_80183944 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_FRAMEBUFFER_BLEND, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_helicopter_landing_pad_80183958 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = acropolisHelicopterLandingPadStartPlayerYawTurn }, { .value = 3072 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 43 }, { .value = 43 }, { .value = 45 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_helicopter_landing_pad_80183854 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_acropolis_helicopter_landing_pad_80183780 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_helicopter_landing_pad_80183958 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_helicopter_landing_pad_80183868 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 360 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_helicopter_landing_pad_8018387C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_helicopter_landing_pad_80183890 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_acropolis_helicopter_landing_pad_80183768 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = sceneEngageBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_FINISH_SCENE_STREAM, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2007 }, { .value = 1 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_acropolis_helicopter_landing_pad_80183FA4[16] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_acropolis_helicopter_landing_pad_80183768 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_helicopter_landing_pad_80183958 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = sceneEngageBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2007 }, { .value = 1 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_acropolis_helicopter_landing_pad_80184124[38] = {
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_helicopter_landing_pad_80183944 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SAVE_VIEW, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_acropolis_helicopter_landing_pad_80183988 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_acropolis_helicopter_landing_pad_80183828 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = acropolisHelicopterLandingPadLockAttachmentsForEncounter }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_acropolis_helicopter_landing_pad_80183750 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_helicopter_landing_pad_801838A4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = acropolisHelicopterLandingPadPlacePlayerAfterEncounter }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = 40 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_helicopter_landing_pad_801839DC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_FINISH_SCENE_STREAM, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_REQUEST_SCENE_MUSIC, { .value = 6 }, { .value = 1 }, { .value = 1 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x51100003 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_MUSIC_LOAD, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_VIEW, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2007 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = acropolisHelicopterLandingPadReleaseEncounterBattle }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_acropolis_helicopter_landing_pad_801844B4[19] = {
    { EVENT_SCRIPT_OPCODE_REQUEST_SCENE_MUSIC, { .value = 6 }, { .value = 1 }, { .value = 1 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_MUSIC_LOAD, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_helicopter_landing_pad_80183944 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = 40 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_helicopter_landing_pad_801839F0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = acropolisHelicopterLandingPadPlacePlayerAfterEncounter }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_VIEW, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2007 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = acropolisHelicopterLandingPadReleaseEncounterBattle }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_acropolis_helicopter_landing_pad_8018467C[69] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_acropolis_helicopter_landing_pad_80183900 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 3 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_acropolis_helicopter_landing_pad_80183990 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_helicopter_landing_pad_80183958 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1013 }, { .message = { .pointer = &gGfxViewCoord } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_acropolis_helicopter_landing_pad_801837C8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = acropolisHelicopterLandingPadStartPlayerMoveToSceneMark }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_acropolis_helicopter_landing_pad_80183750 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_helicopter_landing_pad_80183998 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_FADE_VOLUME, { .value = 32 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = acropolisHelicopterLandingPadSetPlayerPitchPulseState }, { .value = ACROPOLIS_HELICOPTER_LANDING_PAD_PITCH_RISE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = acropolisHelicopterLandingPadSetPlayerPitchPulseState }, { .value = ACROPOLIS_HELICOPTER_LANDING_PAD_PITCH_RISE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_FADE_VOLUME, { .value = 64 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_FRAMEBUFFER_BLEND, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_helicopter_landing_pad_801839AC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_helicopter_landing_pad_801839C0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_FRAMEBUFFER_BLEND, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_FRAMEBUFFER_BLEND, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2004 }, { .message = { .pointer = &D_acropolis_helicopter_landing_pad_80183750 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 1 }, { .value = 1001 }, { .message = { .pointer = &D_acropolis_helicopter_landing_pad_80183750 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_helicopter_landing_pad_8018391C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_acropolis_helicopter_landing_pad_801839D8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2004 }, { .message = { .pointer = &D_acropolis_helicopter_landing_pad_80183750 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_helicopter_landing_pad_801838CC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 300 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_acropolis_helicopter_landing_pad_80183930 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_acropolis_helicopter_landing_pad_801838E0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 350 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = acropolisHelicopterLandingPadStartDepartureShakeTimeline }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x313A0003 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = 0x313A0003 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x313A0004 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 1 }, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = 0x313A0004 }, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = acropolisHelicopterLandingPadStartReturnToMist }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CANCEL_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_acropolis_helicopter_landing_pad_80184CF4[7] = {
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = acropolisHelicopterLandingPadStartReturnToMist }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

s32 D_acropolis_helicopter_landing_pad_80184D9C = 0;

TaskDesc D_acropolis_helicopter_landing_pad_80184DA0[9] = {
    { { { TASK_BODY_NONE, 32 } }, acropolisHelicopterLandingPadPrepareDepartureTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, acropolisHelicopterLandingPadMovePlayerToSceneMarkTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, acropolisHelicopterLandingPadScreenShakeTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, acropolisHelicopterLandingPadDepartureShakeTimelineTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, acropolisHelicopterLandingPadConfirmDepartureTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, acropolisHelicopterLandingPadReturnToMistTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, acropolisHelicopterLandingPadTurnPlayerYawTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 97 } }, acropolisHelicopterLandingPadPlayerPitchPulseTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

s32 D_acropolis_helicopter_landing_pad_80184E0C = 0;

s32 D_acropolis_helicopter_landing_pad_80184E10[6] = {
    0,
    1,
    0,
    0,
    0,
    0,
};

AnimationPlayRequest D_acropolis_helicopter_landing_pad_80184E28 = { 0 };

AnimationPlayRequest D_acropolis_helicopter_landing_pad_80184E3C = { { .index = 1 }, 0, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_acropolis_helicopter_landing_pad_80184E50 = { { -6801, 0, -1998, 0 }, { 0, 1024, 0, 0 } };

TaskDesc D_acropolis_helicopter_landing_pad_80184E68[2] = {
    { { { TASK_BODY_NONE, 192 } }, _acropolisHelicopterLandingPadStartMovieTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _acropolisHelicopterLandingPadMovieTask, { .value = 0 } },
};

SVECTOR D_acropolis_helicopter_landing_pad_80184E80[12] = {
    { -7050, -2800, -7030, 0 },
    { -2800, -3000, -7760, 0 },
    { 2800, -3000, -7760, 0 },
    { 7050, -2800, -7030, 0 },
    { 7760, -3000, -2800, 0 },
    { 7760, -3000, 2800, 0 },
    { 7050, -2800, 7030, 0 },
    { 2800, -3000, 7760, 0 },
    { -2800, -3000, 7760, 0 },
    { -7050, -2800, 7030, 0 },
    { -7760, -3000, 2800, 0 },
    { -7760, -3000, -2900, 0 },
};

s32 D_acropolis_helicopter_landing_pad_80184EE0[12] = {
    0x4014404,
    0x4014000,
    8,
    0x8818,
    2064,
    0xC2830,
    0xC2910,
    0x2C2980,
    0x200580,
    0x200700,
    0x200600,
    0x20400,
};

WorldCollisionRoomResources D_acropolis_helicopter_landing_pad_80184F10[1] = {
    { D_acropolis_helicopter_landing_pad_80185998, D_acropolis_helicopter_landing_pad_801859BC, D_acropolis_helicopter_landing_pad_80185E7C, D_acropolis_helicopter_landing_pad_80186128 },
};

u8 D_acropolis_helicopter_landing_pad_80184F20[28] = {
    1,
    2,
    3,
    4,
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
    25,
    26,
    27,
    0,
};

u8* D_acropolis_helicopter_landing_pad_80184F3C[1] = {
    D_acropolis_helicopter_landing_pad_80184F20,
};

ViewCount D_acropolis_helicopter_landing_pad_80184F40[1] = { 27 };

WorldCoordRoomLighting D_acropolis_helicopter_landing_pad_80184F44[1] = {
    { D_acropolis_helicopter_landing_pad_80186AE8, NULL },
};

DirectionWarpEntry D_acropolis_helicopter_landing_pad_80184F4C[3] = {
    { { { .word = 2048 }, -6387, 1, -6306 }, { 0, 0, 0, 0 }, { { .word = 2048 }, -6387, 1, -6306 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 1024 }, -5340, -3001, -1921 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -5340, -3001, -1921 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 11, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 2048 }, -6400, 600, -5280 }, { 0, 0, 0, 0 }, { { .word = 2048 }, -6387, 451, -5400 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_SCRIPTED_PLAYER, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gAcropolisHelicopterLandingPadCollision083D8Normals[15] = {
#include "assets/acropolis_helicopter_landing_pad_collision_083D8_normals.inc"
};

static SVECTOR _gAcropolisHelicopterLandingPadCollision083D8Verts[92] = {
#include "assets/acropolis_helicopter_landing_pad_collision_083D8_verts.inc"
};

static WorldCollisionGridFace _gAcropolisHelicopterLandingPadCollision083D8Faces[37] = {
#include "assets/acropolis_helicopter_landing_pad_collision_083D8_faces.inc"
};

static s16 _gAcropolisHelicopterLandingPadCollision083D8Cells[512] = {
#include "assets/acropolis_helicopter_landing_pad_collision_083D8_cells.inc"
};

#define GRID_CELL(i) (&_gAcropolisHelicopterLandingPadCollision083D8Cells[i])
static s16* _gAcropolisHelicopterLandingPadCollision083D8Table[36] = {
#include "assets/acropolis_helicopter_landing_pad_collision_083D8_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_acropolis_helicopter_landing_pad_80185998[1] = {
    { NULL, _gAcropolisHelicopterLandingPadCollision083D8Normals, _gAcropolisHelicopterLandingPadCollision083D8Verts, _gAcropolisHelicopterLandingPadCollision083D8Faces, _gAcropolisHelicopterLandingPadCollision083D8Table, 0x2710, 0x2710, 6, 6, 4000, 37 },
};

WorldCollisionTrigger D_acropolis_helicopter_landing_pad_801859BC[16] = {
    { NULL, NULL, NULL, { -5505, 0, -5857, 0 }, { { -473, 6080, -1561, 0 }, { 474, 6080, 1562, 0 }, { -473, -6080, -1561, 0 }, { 474, -6080, 1562, 0 } }, { -3931, 0, 1191, 0 }, { 0, 0, 4096, 0 }, 6291, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6016, 0, -5888, 0 }, { { 395, 6080, 1582, 0 }, { -397, 6080, -1584, 0 }, { 395, -6080, 1582, 0 }, { -397, -6080, -1584, 0 } }, { 3984, 0, -998, 0 }, { 0, 0, 4096, 0 }, 6291, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1985, 0, -5921, 0 }, { { 155, 6080, 1619, 0 }, { -165, 6080, -1627, 0 }, { 155, -6080, 1619, 0 }, { -165, -6080, -1627, 0 } }, { 4085, 0, -403, 0 }, { 0, 0, 4096, 0 }, 6291, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1536, 0, -6112, 0 }, { { -161, 6080, -1626, 0 }, { 157, 6080, 1621, 0 }, { -161, -6080, -1626, 0 }, { 157, -6080, 1621, 0 } }, { -4087, 0, 400, 0 }, { 0, 0, 4096, 0 }, 6291, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3550, 0, -5857, 0 }, { { 1224, 6080, -1401, 0 }, { -1223, 6080, 1401, 0 }, { 1224, -6080, -1401, 0 }, { -1223, -6080, 1401, 0 } }, { -3090, 0, -2698, 0 }, { 0, 0, 4096, 0 }, 6353, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3007, 0, -6113, 0 }, { { -1515, 6080, 1692, 0 }, { 1515, 6080, -1691, 0 }, { -1515, -6080, 1692, 0 }, { 1515, -6080, -1691, 0 } }, { 3053, 0, 2735, 0 }, { 0, 0, 4096, 0 }, 6476, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6048, 0, -1152, 0 }, { { 1860, 6080, -35, 0 }, { -1860, 6080, 34, 0 }, { 1860, -6080, -35, 0 }, { -1860, -6080, 34, 0 } }, { -77, 0, -4102, 0 }, { 0, 0, 4096, 0 }, 6353, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 6144, 0, -1729, 0 }, { { -1860, 6080, 34, 0 }, { 1860, 6080, -35, 0 }, { -1860, -6080, 34, 0 }, { 1860, -6080, -35, 0 } }, { 75, 0, 4100, 0 }, { 0, 0, 4096, 0 }, 6353, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5760, 0, 3359, 0 }, { { -1624, 6080, 907, 0 }, { 1624, 6080, -907, 0 }, { -1624, -6080, 907, 0 }, { 1624, -6080, -907, 0 } }, { 1999, 0, 3580, 0 }, { 0, 0, 4096, 0 }, 6353, 0, 7, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 5952, 0, 3840, 0 }, { { 1666, 6080, -827, 0 }, { -1667, 6080, 826, 0 }, { 1666, -6080, -827, 0 }, { -1667, -6080, 826, 0 } }, { -1823, 0, -3675, 0 }, { 0, 0, 4096, 0 }, 6353, 0, 6, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2655, 0, 6143, 0 }, { { 304, 6080, 1833, 0 }, { -310, 6080, -1836, 0 }, { 304, -6080, 1833, 0 }, { -310, -6080, -1836, 0 } }, { 4044, 0, -677, 0 }, { 0, 0, 4096, 0 }, 6353, 0, 7, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3231, 0, 6047, 0 }, { { -310, 6080, -1836, 0 }, { 304, 6080, 1832, 0 }, { -310, -6080, -1836, 0 }, { 304, -6080, 1832, 0 } }, { -4044, 0, 676, 0 }, { 0, 0, 4096, 0 }, 6353, 0, 8, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -4193, 0, 6111, 0 }, { { 124, 6080, 1855, 0 }, { -126, 6080, -1856, 0 }, { 124, -6080, 1855, 0 }, { -126, -6080, -1856, 0 } }, { 4091, 0, -276, 0 }, { 0, 0, 4096, 0 }, 6353, 0, 8, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -3939, 0, 6079, 0 }, { { -216, 6080, -1848, 0 }, { 215, 6080, 1846, 0 }, { -216, -6080, -1848, 0 }, { 215, -6080, 1846, 0 } }, { -4073, 0, 474, 0 }, { 0, 0, 4096, 0 }, 6353, 0, 9, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6098, 0, 3342, 0 }, { { -1437, 6080, 1741, 0 }, { 1437, 6080, -1740, 0 }, { -1437, -6080, 1741, 0 }, { 1437, -6080, -1740, 0 } }, { 3159, 0, 2608, 0 }, { 0, 0, 4096, 0 }, 6476, 0, 9, 10, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5856, 0, 4064, 0 }, { { 1437, 6080, -1741, 0 }, { -1437, 6080, 1740, 0 }, { 1437, -6080, -1741, 0 }, { -1437, -6080, 1740, 0 } }, { -3161, 0, -2610, 0 }, { 0, 0, 4096, 0 }, 6476, 0, 10, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

/// Plays the room's view-100 movie, permits Start cancellation and restores gameplay.
///
/// Runs as the display-owned task with state 0..5. Requires the current location's
/// movie lookup to yield slot 0..14, serialized CD access and valid image-memory
/// storage. Preserves displaced VRAM images, waits for playback/cancellation to
/// end, restores model resources and clears the complete resident image workspace.
/// The room overlay remains loaded until the task ends and resumes the game loop.
static void _acropolisHelicopterLandingPadMovieTask(Task* task)
{
    enum {
        ACROPOLIS_HELICOPTER_LANDING_PAD_MOVIE_PREPARE    = 0,
        ACROPOLIS_HELICOPTER_LANDING_PAD_MOVIE_QUEUE      = 1,
        ACROPOLIS_HELICOPTER_LANDING_PAD_MOVIE_WAIT_READY = 2,
        ACROPOLIS_HELICOPTER_LANDING_PAD_MOVIE_PLAY       = 3,
        ACROPOLIS_HELICOPTER_LANDING_PAD_MOVIE_WAIT_IDLE  = 4,
        ACROPOLIS_HELICOPTER_LANDING_PAD_MOVIE_RESTORE    = 5,
        ACROPOLIS_HELICOPTER_LANDING_PAD_MOVIE_VIEW       = 100,
    };
    u8          movieArgs[sizeof(gCdCmdQueue.entries[0].args.bytes)];
    GameLoc     movieLocation;
    s16         movieSlot;
    CdCmdQueue* cdQueue;

    cdQueue = &gCdCmdQueue;
    switch (task->state) {
        case ACROPOLIS_HELICOPTER_LANDING_PAD_MOVIE_PREPARE:
            SetDispMask(0);
            streamPrepareMovieWorkspace(1);
            task->state++;
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_MOVIE_QUEUE:
            movieLocation          = gGameSession->location;
            movieLocation.loc.view = ACROPOLIS_HELICOPTER_LANDING_PAD_MOVIE_VIEW;
            movieSlot              = streamFindMovieSlot(&movieLocation.loc, 0, 0);
            // The queue copies four bytes; playback uses only the slot byte.
            movieArgs[0] = movieSlot;
            cdCmdEnqueue(CD_COMMAND_PLAY_STREAM, NULL, movieArgs);
            task->state++;
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_MOVIE_WAIT_READY:
            if (cdQueue->movieReady != 0) {
                SetDispMask(1);
                task->state++;
                break;
            }
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_MOVIE_PLAY:
            if (cdCmdIsIdle()) {
                SetDispMask(0);
                task->state++;
                break;
            }
            if (padIsStartPressed() != 0) {
                SetDispMask(0);
                cdCmdRequestCancel();
                task->state++;
                break;
            }
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_MOVIE_WAIT_IDLE:
            if (cdCmdIsIdle()) {
                streamResetGameRestore();
                task->state++;
                break;
            }
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_MOVIE_RESTORE:
            if (streamPollGameRestore(0, 0)) {
                memFillBytes(Fs_ImgBuffers, 0, sizeof(*Fs_ImgBuffers));
                SetDispMask(1);
                taskKill(task);
                displayResumeGameLoop();
            }
            break;
    }
}

/// Hands display presentation to the movie task and retires the requesting room task.
///
/// Queues the current camera and packets after selecting task-only flips.
/// Requires the room's movie descriptors to remain loaded through playback.
static void _acropolisHelicopterLandingPadStartMovieTask(Task* task)
{
    enum { ACROPOLIS_HELICOPTER_LANDING_PAD_MOVIE_TASK_ENTRY = 1 };

    displaySpawnTaskFromTable(D_acropolis_helicopter_landing_pad_80184E68, ACROPOLIS_HELICOPTER_LANDING_PAD_MOVIE_TASK_ENTRY, 0, 0);
    gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
    viewQueueCurrentCameraAndPackets();
    taskKill(task);
}

/// Resolves the fire-escape exit before the player turns and descends the stairs.
///
/// Requests area 15, room 1, warp 3 in the current stage. The room task resolves
/// the shared request/reply synchronously. A nonzero result advances this exit
/// sequence; zero kills it. The request remains live for the later transition.
static void _acropolisHelicopterLandingPadResolveExit(Task* task)
{
    enum {
        ACROPOLIS_HELICOPTER_LANDING_PAD_EXIT_ROOM = 1,
        ACROPOLIS_HELICOPTER_LANDING_PAD_EXIT_WARP = 3,
    };
    Task* roomTask = gameGetTaskSlot(GAME_TASK_SLOT_ROOM);

    D_acropolis_helicopter_landing_pad_80187F90.field_4   = 1;
    D_acropolis_helicopter_landing_pad_80187F90.room      = ACROPOLIS_HELICOPTER_LANDING_PAD_EXIT_ROOM;
    D_acropolis_helicopter_landing_pad_80187F90.areaId    = GAME_AREA_ACROPOLIS_FIRE_ESCAPE;
    D_acropolis_helicopter_landing_pad_80187F90.warp      = ACROPOLIS_HELICOPTER_LANDING_PAD_EXIT_WARP;
    D_acropolis_helicopter_landing_pad_80187F90.queryOnly = ROOM_EVENT_EXECUTE;
    if (TASK_MESSAGE_DISPATCH_POINTERS(roomTask, ROOM_EVENT_MESSAGE_RESOLVE, &D_acropolis_helicopter_landing_pad_80187F90,
                                       &D_acropolis_helicopter_landing_pad_80187F90) != 0) {
        task->state += 1;
    } else {
        taskKill(task);
    }
}

/// Starts the player's scripted turn toward the exit stairs, then advances the task.
///
/// Yaw zero faces the flight. Synchronous dispatch copies the target yaw;
/// the payload's position is unused and the turn finishes in a later frame.
static void _acropolisHelicopterLandingPadTurnPlayerToExit(Task* task)
{
    enum { ACROPOLIS_HELICOPTER_LANDING_PAD_EXIT_YAW = 0 };

    ActorTransform targetTransform;
    Task*          playerTask;

    playerTask             = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    targetTransform.rot.vx = 0;
    targetTransform.rot.vy = ACROPOLIS_HELICOPTER_LANDING_PAD_EXIT_YAW;
    targetTransform.rot.vz = 0;
    TASK_MESSAGE_DISPATCH_POINTER(playerTask, GAME_ACTOR_MESSAGE_TURN_TO_YAW, &targetTransform, 0);
    task->state = task->state + 1;
}

/// Holds the exit sequence until the player's scripted turn has finished.
///
/// Requires the live player task that received the turn request; advances once
/// its scripted-motion query returns zero.
static void _acropolisHelicopterLandingPadWaitForPlayerTurn(Task* task)
{
    if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
        task->state = task->state + 1;
    }
}

/// Starts the player's three-step descent from the landing pad, then advances.
///
/// Requires the preceding turn to yaw zero to have finished. The player copies
/// the stair request during dispatch and completes the descent asynchronously.
static void _acropolisHelicopterLandingPadDescendExitStairs(Task* task)
{
    enum { ACROPOLIS_HELICOPTER_LANDING_PAD_EXIT_STAIR_STEPS = 3 };

    GameActorStairClimb climb;
    Task*               playerTask;

    playerTask      = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    climb.descend   = true;
    climb.stepCount = ACROPOLIS_HELICOPTER_LANDING_PAD_EXIT_STAIR_STEPS;
    TASK_MESSAGE_DISPATCH_POINTER(playerTask, GAME_ACTOR_MESSAGE_CLIMB_STAIRS, &climb, 0);
    task->state = task->state + 1;
}

/// Commits the resolved fire-escape destination once the player's descent finishes.
///
/// Requires the live player and the room's resolved destination record. Copies
/// its area, warp and room into the live save, queues a captured-frame reload,
/// then kills this task. A failed reload allocation still leaves the save changed.
static void _acropolisHelicopterLandingPadCommitExitTask(Task* task)
{
    if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = (u8)D_acropolis_helicopter_landing_pad_80187F90.areaId;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = D_acropolis_helicopter_landing_pad_80187F90.warp;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = D_acropolis_helicopter_landing_pad_80187F90.room;
        taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
        taskKill(task);
    }
}

void acropolisHelicopterLandingPadStartExit(s32 unusedArgument, s32 unusedActionId)
{
    enum { ACROPOLIS_HELICOPTER_LANDING_PAD_EXIT_TASK_BANK = 2,
           ACROPOLIS_HELICOPTER_LANDING_PAD_EXIT_TASK_SLOT = 15 };

    taskSpawn(ACROPOLIS_HELICOPTER_LANDING_PAD_EXIT_TASK_BANK, ACROPOLIS_HELICOPTER_LANDING_PAD_EXIT_TASK_SLOT, 0, 0);
}

void acropolisHelicopterLandingPadExitTask(Task* task)
{
    enum { ACROPOLIS_HELICOPTER_LANDING_PAD_EXIT_STAIR_SURFACE = 2 };

    GameActor* playerActor     = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;
    TaskFunc   stateHandlers[] = {
        _acropolisHelicopterLandingPadResolveExit,
        _acropolisHelicopterLandingPadTurnPlayerToExit,
        _acropolisHelicopterLandingPadWaitForPlayerTurn,
        _acropolisHelicopterLandingPadDescendExitStairs,
        _acropolisHelicopterLandingPadCommitExitTask,
    };

    // Scripted stairs bypass collision-based surface selection.
    playerActor->surfaceClass = ACROPOLIS_HELICOPTER_LANDING_PAD_EXIT_STAIR_SURFACE;
    stateHandlers[task->state](task);
}

/// Sets a Gouraud glow wedge's packet header and red-to-black vertex colors.
///
/// Borrows writable storage for one `POLY_G4`. `centerRed` is the byte intensity
/// at vertex 2; vertices 0, 1 and 3 are black. The caller supplies coordinates
/// and ordering links and enables additive blending after initialization.
static inline void _acropolisHelicopterLandingPadInitGlowWedge(POLY_G4* wedge, u8 centerRed)
{
    setPolyG4(wedge);
    setRGB0(wedge, 0, 0, 0);
    setRGB1(wedge, 0, 0, 0);
    setRGB2(wedge, centerRed, 0, 0);
    setRGB3(wedge, 0, 0, 0);
}

/// Draws a pulsing red perimeter glow and refreshes its shared transient light.
///
/// Borrows one aligned world point; `lightIndex` is 0..11 and `brightness` is 0..254.
/// The current mapped view is 1..27; its bit in the light's visibility mask must
/// be set. Slots 6/7 alternate by index and later visible lights overwrite them.
/// A successful projection refreshes the slot for two frames with Q12 red
/// `brightness` * 16, full strength to 1600 world units and no light at 12800.
///
/// Queues sixteen Gouraud wedges and four blades with additive blending, at the
/// unbiased `SZ3` / 4 depth. Their screen radii are 0xC000/depth and 0x1800/depth;
/// the current GTE projection must yield a nonzero depth on success. Reserves
/// and releases 20 scratch bytes and consumes twenty `POLY_G4` packets. Paused
/// controls skip drawing; cancellation disables the slot. Retains no pointer.
static void _acropolisHelicopterLandingPadDrawPerimeterLight(const SVECTOR* worldPoint, s16 lightIndex, s32 brightness)
{
    enum {
        ACROPOLIS_HELICOPTER_LANDING_PAD_GLOW_FIRST_LIGHT_SLOT     = 6,
        ACROPOLIS_HELICOPTER_LANDING_PAD_GLOW_LIGHT_FRAMES         = 2,
        ACROPOLIS_HELICOPTER_LANDING_PAD_GLOW_LIGHT_INNER_DISTANCE = 1600,
        ACROPOLIS_HELICOPTER_LANDING_PAD_GLOW_LIGHT_OUTER_DISTANCE = 12800,
        ACROPOLIS_HELICOPTER_LANDING_PAD_GLOW_LIGHT_COLOR_SCALE    = ONE / 256,
        ACROPOLIS_HELICOPTER_LANDING_PAD_GLOW_OUTER_RADIUS_SCALE   = 0xC000,
        ACROPOLIS_HELICOPTER_LANDING_PAD_GLOW_INNER_RADIUS_SCALE   = 0x1800,
        ACROPOLIS_HELICOPTER_LANDING_PAD_GLOW_TRIG_FRACTION_BITS   = 12,
        ACROPOLIS_HELICOPTER_LANDING_PAD_GLOW_FULL_TURN            = 0x1000,
        ACROPOLIS_HELICOPTER_LANDING_PAD_GLOW_WEDGE_ANGLE          = 0x200,
        ACROPOLIS_HELICOPTER_LANDING_PAD_GLOW_QUARTER_TURN         = 0x400,
        ACROPOLIS_HELICOPTER_LANDING_PAD_GLOW_HALF_TURN            = 0x800,
    };

    WorldCoordTransientPointLight* lightSlot;
    WorldCoordPointLight*          pointLight;
    GlowCentreRadiiScratch*        block;
    POLY_G4*                       wedge;
    s32                            angle;
    s32                            halfStepAngle;
    s32                            nextAngle;
    s32                            armEdgeAngle;
    s16                            wedgeBrightness;
    s32                            halfBrightness;
    s32                            visibleViewBit;

    wedgeBrightness = brightness;
    lightSlot       = &gWorldCoordTransientPointLights[ACROPOLIS_HELICOPTER_LANDING_PAD_GLOW_FIRST_LIGHT_SLOT + (lightIndex & 1)];
    pointLight      = &lightSlot->light;
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            lightSlot->framesLeft = WORLD_COORDINATE_TRANSIENT_LIGHT_INACTIVE;
        }
    } else {
        visibleViewBit = D_acropolis_helicopter_landing_pad_80184EE0[lightIndex] & (1 << ((viewGetMappedIndex() & 0xFF) - 1));
        if (visibleViewBit == 0) {
            return;
        }
        // Project the authored world position with the current view.
        block = SCRATCH_STACK_RESERVE_BLOCK(GlowCentreRadiiScratch);
        gte_SetTransMatrix(&gGfxViewCoord.workm);
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_ldv0(worldPoint);
        gte_rtps();
        gte_stsxy(&block->sx);
        gte_stflg(&block->flag);
        if (block->flag >= 0) {
            gte_stszotz(&block->otz);
            lightSlot->framesLeft                              = ACROPOLIS_HELICOPTER_LANDING_PAD_GLOW_LIGHT_FRAMES;
            pointLight->inner                                  = ACROPOLIS_HELICOPTER_LANDING_PAD_GLOW_LIGHT_INNER_DISTANCE;
            pointLight->outer                                  = ACROPOLIS_HELICOPTER_LANDING_PAD_GLOW_LIGHT_OUTER_DISTANCE;
            pointLight->head.color.r                           = brightness * ACROPOLIS_HELICOPTER_LANDING_PAD_GLOW_LIGHT_COLOR_SCALE;
            pointLight->head.color.g                           = 0;
            pointLight->head.color.b                           = 0;
            pointLight->head.transform.lighting.local.t[0]     = worldPoint->vx;
            pointLight->head.transform.lighting.local.t[1]     = worldPoint->vy;
            pointLight->head.transform.lighting.local.t[2]     = worldPoint->vz;
            lightSlot->light.head.transform.coord.composeStamp = GRAPHICS_COORD_DIRTY;
            block->outerRadius                                 = ACROPOLIS_HELICOPTER_LANDING_PAD_GLOW_OUTER_RADIUS_SCALE / block->otz;
            block->innerRadius                                 = ACROPOLIS_HELICOPTER_LANDING_PAD_GLOW_INNER_RADIUS_SCALE / block->otz;

            // Layer eight dim outer wedges with eight brighter half-radius wedges.
            for (angle = 0; angle < ACROPOLIS_HELICOPTER_LANDING_PAD_GLOW_FULL_TURN; angle += ACROPOLIS_HELICOPTER_LANDING_PAD_GLOW_WEDGE_ANGLE) {
                wedge          = gGpuPrimCursor;
                gGpuPrimCursor = wedge + 1;
                setPolyG4(wedge);
                setRGB0(wedge, 0, 0, 0);
                setRGB1(wedge, 0, 0, 0);
                halfBrightness = wedgeBrightness >> 1;
                setRGB2(wedge, halfBrightness, 0, 0);
                setRGB3(wedge, 0, 0, 0);
                wedge->x0     = block->sx + ((block->outerRadius * rsin(angle)) >> ACROPOLIS_HELICOPTER_LANDING_PAD_GLOW_TRIG_FRACTION_BITS);
                wedge->y0     = block->sy + ((block->outerRadius * rcos(angle)) >> ACROPOLIS_HELICOPTER_LANDING_PAD_GLOW_TRIG_FRACTION_BITS);
                halfStepAngle = angle + (ACROPOLIS_HELICOPTER_LANDING_PAD_GLOW_WEDGE_ANGLE / 2);
                wedge->x1     = block->sx + ((block->outerRadius * rsin(halfStepAngle)) >> ACROPOLIS_HELICOPTER_LANDING_PAD_GLOW_TRIG_FRACTION_BITS);
                wedge->y1     = block->sy + ((block->outerRadius * rcos(halfStepAngle)) >> ACROPOLIS_HELICOPTER_LANDING_PAD_GLOW_TRIG_FRACTION_BITS);
                wedge->x2     = block->sx;
                wedge->y2     = block->sy;
                nextAngle     = angle + ACROPOLIS_HELICOPTER_LANDING_PAD_GLOW_WEDGE_ANGLE;
                wedge->x3     = block->sx + ((block->outerRadius * rsin(nextAngle)) >> ACROPOLIS_HELICOPTER_LANDING_PAD_GLOW_TRIG_FRACTION_BITS);
                wedge->y3     = block->sy + ((block->outerRadius * rcos(nextAngle)) >> ACROPOLIS_HELICOPTER_LANDING_PAD_GLOW_TRIG_FRACTION_BITS);
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        wedge);
                gpuSetPrimitiveBlendMode(wedge, GPU_BLEND_ADD, block->otz);

                wedge          = gGpuPrimCursor;
                gGpuPrimCursor = wedge + 1;
                _acropolisHelicopterLandingPadInitGlowWedge(wedge, wedgeBrightness);
                wedge->x0 = block->sx + ((block->outerRadius * rsin(angle)) >> (ACROPOLIS_HELICOPTER_LANDING_PAD_GLOW_TRIG_FRACTION_BITS + 1));
                wedge->y0 = block->sy + ((block->outerRadius * rcos(angle)) >> (ACROPOLIS_HELICOPTER_LANDING_PAD_GLOW_TRIG_FRACTION_BITS + 1));
                wedge->x1 = block->sx + ((block->outerRadius * rsin(halfStepAngle)) >> (ACROPOLIS_HELICOPTER_LANDING_PAD_GLOW_TRIG_FRACTION_BITS + 1));
                wedge->y1 = block->sy + ((block->outerRadius * rcos(halfStepAngle)) >> (ACROPOLIS_HELICOPTER_LANDING_PAD_GLOW_TRIG_FRACTION_BITS + 1));
                wedge->x2 = block->sx;
                wedge->y2 = block->sy;
                wedge->x3 = block->sx + ((block->outerRadius * rsin(nextAngle)) >> (ACROPOLIS_HELICOPTER_LANDING_PAD_GLOW_TRIG_FRACTION_BITS + 1));
                wedge->y3 = block->sy + ((block->outerRadius * rcos(nextAngle)) >> (ACROPOLIS_HELICOPTER_LANDING_PAD_GLOW_TRIG_FRACTION_BITS + 1));
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        wedge);
                gpuSetPrimitiveBlendMode(wedge, GPU_BLEND_ADD, block->otz);
            }

            // Overlay four red blades; alternate tips reach twice the disc radius.
            wedgeBrightness = halfBrightness;
            for (angle = ACROPOLIS_HELICOPTER_LANDING_PAD_GLOW_WEDGE_ANGLE; angle < ACROPOLIS_HELICOPTER_LANDING_PAD_GLOW_FULL_TURN; angle += ACROPOLIS_HELICOPTER_LANDING_PAD_GLOW_HALF_TURN) {
                armEdgeAngle   = angle - ACROPOLIS_HELICOPTER_LANDING_PAD_GLOW_QUARTER_TURN;
                wedge          = gGpuPrimCursor;
                gGpuPrimCursor = wedge + 1;
                _acropolisHelicopterLandingPadInitGlowWedge(wedge, wedgeBrightness);
                wedge->x0    = block->sx + ((block->innerRadius * rsin(armEdgeAngle)) >> (ACROPOLIS_HELICOPTER_LANDING_PAD_GLOW_TRIG_FRACTION_BITS + 1));
                wedge->y0    = block->sy + ((block->innerRadius * rcos(armEdgeAngle)) >> (ACROPOLIS_HELICOPTER_LANDING_PAD_GLOW_TRIG_FRACTION_BITS + 1));
                wedge->x1    = block->sx + ((block->outerRadius * rsin(angle)) >> ACROPOLIS_HELICOPTER_LANDING_PAD_GLOW_TRIG_FRACTION_BITS);
                wedge->y1    = block->sy + ((block->outerRadius * rcos(angle)) >> ACROPOLIS_HELICOPTER_LANDING_PAD_GLOW_TRIG_FRACTION_BITS);
                wedge->x2    = block->sx;
                wedge->y2    = block->sy;
                armEdgeAngle = angle + ACROPOLIS_HELICOPTER_LANDING_PAD_GLOW_QUARTER_TURN;
                wedge->x3    = block->sx + ((block->innerRadius * rsin(armEdgeAngle)) >> (ACROPOLIS_HELICOPTER_LANDING_PAD_GLOW_TRIG_FRACTION_BITS + 1));
                wedge->y3    = block->sy + ((block->innerRadius * rcos(armEdgeAngle)) >> (ACROPOLIS_HELICOPTER_LANDING_PAD_GLOW_TRIG_FRACTION_BITS + 1));
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        wedge);
                gpuSetPrimitiveBlendMode(wedge, GPU_BLEND_ADD, block->otz);

                wedge          = gGpuPrimCursor;
                gGpuPrimCursor = wedge + 1;
                _acropolisHelicopterLandingPadInitGlowWedge(wedge, wedgeBrightness);
                wedge->x0    = block->sx + ((block->innerRadius * rsin(angle)) >> ACROPOLIS_HELICOPTER_LANDING_PAD_GLOW_TRIG_FRACTION_BITS);
                wedge->y0    = block->sy + ((block->innerRadius * rcos(angle)) >> ACROPOLIS_HELICOPTER_LANDING_PAD_GLOW_TRIG_FRACTION_BITS);
                wedge->x1    = block->sx + ((block->outerRadius * rsin(armEdgeAngle)) >> (ACROPOLIS_HELICOPTER_LANDING_PAD_GLOW_TRIG_FRACTION_BITS - 1));
                wedge->y1    = block->sy + ((block->outerRadius * rcos(armEdgeAngle)) >> (ACROPOLIS_HELICOPTER_LANDING_PAD_GLOW_TRIG_FRACTION_BITS - 1));
                wedge->x2    = block->sx;
                wedge->y2    = block->sy;
                armEdgeAngle = angle + ACROPOLIS_HELICOPTER_LANDING_PAD_GLOW_HALF_TURN;
                wedge->x3    = block->sx + ((block->innerRadius * rsin(armEdgeAngle)) >> ACROPOLIS_HELICOPTER_LANDING_PAD_GLOW_TRIG_FRACTION_BITS);
                wedge->y3    = block->sy + ((block->innerRadius * rcos(armEdgeAngle)) >> ACROPOLIS_HELICOPTER_LANDING_PAD_GLOW_TRIG_FRACTION_BITS);
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        wedge);
                gpuSetPrimitiveBlendMode(wedge, GPU_BLEND_ADD, block->otz);
            }
        }
        SCRATCH_STACK_RELEASE_BLOCK(GlowCentreRadiiScratch);
    }
}

/// Projects a sprite's cached origin through `GsWSMATRIX`.
///
/// Borrows a coordinate with a current composition cache and one writable,
/// word-aligned `EffectShapeScratch`. Its cached translation must be in the
/// input space of `GsWSMATRIX`; XYZ narrow to s16 before projection. Stores raw
/// screen XY and GTE flags, leaving depth in the GTE for the caller without
/// writing the scratch depth or extent. Overwrites GTE matrix/projection state.
static inline void _acropolisHelicopterLandingPadProjectSpriteCentre(EffectShapeScratch* projection, const GfxCoord* coord)
{
    projection->worldPoint.vx = coord->workm.t[0];
    projection->worldPoint.vy = coord->workm.t[1];
    projection->worldPoint.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&projection->worldPoint);
    gte_rtps();
    gte_stsxy(&projection->screenX);
    gte_stflg(&projection->projectionFlags);
}

void acropolisHelicopterLandingPadEmberTask(Task* task)
{
    enum {
        ACROPOLIS_HELICOPTER_LANDING_PAD_EMBER_INITIALIZE       = 0,
        ACROPOLIS_HELICOPTER_LANDING_PAD_EMBER_FLICKER_VARIANT  = 1,
        ACROPOLIS_HELICOPTER_LANDING_PAD_EMBER_CLUT             = 0x4383,
        ACROPOLIS_HELICOPTER_LANDING_PAD_EMBER_CELL_PIXELS      = 32,
        ACROPOLIS_HELICOPTER_LANDING_PAD_EMBER_TEXTURE_ROW      = 0x28,
        ACROPOLIS_HELICOPTER_LANDING_PAD_EMBER_FRAMES_PER_CELL  = 6,
        ACROPOLIS_HELICOPTER_LANDING_PAD_EMBER_BRIGHT_RISE_STEP = 24,
    };

    EffectWork*         work;
    GfxCoord*           coord;
    EffectShapeScratch* projection;
    POLY_FT4*           sprite;
    u32                 colorSample;
    s16                 nextAge;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_HIDDEN) {
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(work, task);
        }
        return;
    }
    // Render the current pose before advancing its local drift and atlas frame.
    actorRenderComposeCoord(coord);
    if (task->state == ACROPOLIS_HELICOPTER_LANDING_PAD_EMBER_INITIALIZE) {
        if (task->spawnArg1.value != 0) {
            work->move.vx   = 0;
            work->move.vy   = -ACROPOLIS_HELICOPTER_LANDING_PAD_EMBER_BRIGHT_RISE_STEP;
            work->move.vz   = 0;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->scale     = ((gRandomLcgState >> 16) & 0xFF) + 0x300;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->angle     = (gRandomLcgState >> 16) & ACTOR_TRANSFORM_ANGLE_MASK;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->period    = (gRandomLcgState >> 16) & 0x3F;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->step      = ((gRandomLcgState >> 16) & 3) + 2;
        } else {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->scale     = ((gRandomLcgState >> 16) & 0xFF) + 0x100;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->angle     = (gRandomLcgState >> 16) & ACTOR_TRANSFORM_ANGLE_MASK;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->period    = ((gRandomLcgState >> 16) & 3) + 1;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->step      = ((gRandomLcgState >> 16) & 3) + 1;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vx   = ((gRandomLcgState >> 16) & 7) - 4;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vy   = ~((gRandomLcgState >> 16) & 0xF);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vz   = ((gRandomLcgState >> 16) & 7) - 4;
        }
        task->state++;
    }
    projection = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
    _acropolisHelicopterLandingPadProjectSpriteCentre(projection, coord);
    if (projection->projectionFlags >= 0) {
        gte_stszotz(&projection->depth);
        sprite         = gGpuPrimCursor;
        gGpuPrimCursor = sprite + 1;
        setPolyFT4(sprite);
        if (task->spawnArg1.value == ACROPOLIS_HELICOPTER_LANDING_PAD_EMBER_FLICKER_VARIANT) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if (((gRandomLcgState >> 16) & 3) == 0) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                colorSample     = (gRandomLcgState >> 16) & 0xFF;
                setRGB0(sprite, colorSample >> 1, colorSample, 0xFF);
            } else {
                setShadeTex(sprite, true);
            }
            if (work->age < work->step * ACROPOLIS_HELICOPTER_LANDING_PAD_EMBER_FRAMES_PER_CELL - 2) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                if (((gRandomLcgState >> 16) & 0xF) == 0 && gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                    effectSpawn(EFFECT_FLASH_BURST, coord, 0x100, NULL);
                }
            }
        } else {
            sprite->code = ACROPOLIS_HELICOPTER_LANDING_PAD_SPRITE_RAW_TEXTURE_CODE;
        }
        sprite->tpage = ACROPOLIS_HELICOPTER_LANDING_PAD_SPRITE_TEXTURE_PAGE;
        sprite->clut  = ACROPOLIS_HELICOPTER_LANDING_PAD_EMBER_CLUT;
        setSemiTrans(sprite, true);
        sprite->u0                  = (work->age / work->step + 1) * ACROPOLIS_HELICOPTER_LANDING_PAD_EMBER_CELL_PIXELS;
        sprite->v0                  = ACROPOLIS_HELICOPTER_LANDING_PAD_EMBER_TEXTURE_ROW;
        sprite->u1                  = (work->age / work->step + 1) * ACROPOLIS_HELICOPTER_LANDING_PAD_EMBER_CELL_PIXELS + (ACROPOLIS_HELICOPTER_LANDING_PAD_EMBER_CELL_PIXELS - 1);
        sprite->v1                  = ACROPOLIS_HELICOPTER_LANDING_PAD_EMBER_TEXTURE_ROW;
        sprite->u2                  = (work->age / work->step + 1) * ACROPOLIS_HELICOPTER_LANDING_PAD_EMBER_CELL_PIXELS;
        sprite->v2                  = ACROPOLIS_HELICOPTER_LANDING_PAD_EMBER_TEXTURE_ROW + ACROPOLIS_HELICOPTER_LANDING_PAD_EMBER_CELL_PIXELS - 1;
        sprite->u3                  = (work->age / work->step + 1) * ACROPOLIS_HELICOPTER_LANDING_PAD_EMBER_CELL_PIXELS + (ACROPOLIS_HELICOPTER_LANDING_PAD_EMBER_CELL_PIXELS - 1);
        sprite->v3                  = ACROPOLIS_HELICOPTER_LANDING_PAD_EMBER_TEXTURE_ROW + ACROPOLIS_HELICOPTER_LANDING_PAD_EMBER_CELL_PIXELS - 1;
        projection->extent.corner.x = ((work->scale * (ACROPOLIS_HELICOPTER_LANDING_PAD_EMBER_CELL_PIXELS - 1) / projection->depth) * rsin(work->angle)) >> ACROPOLIS_HELICOPTER_LANDING_PAD_SPRITE_TRIG_FRACTION_BITS;
        projection->extent.corner.y = ((work->scale * (ACROPOLIS_HELICOPTER_LANDING_PAD_EMBER_CELL_PIXELS - 1) / projection->depth) * rcos(work->angle)) >> ACROPOLIS_HELICOPTER_LANDING_PAD_SPRITE_TRIG_FRACTION_BITS;
        sprite->x0                  = projection->screenX + (u16)projection->extent.corner.x;
        sprite->x3                  = projection->screenX - (u16)projection->extent.corner.x;
        sprite->y0                  = projection->screenY - (u16)projection->extent.corner.y;
        sprite->y3                  = projection->screenY + (u16)projection->extent.corner.y;
        projection->extent.corner.x = ((work->scale * (ACROPOLIS_HELICOPTER_LANDING_PAD_EMBER_CELL_PIXELS - 1) / projection->depth) * rsin(work->angle + ACTOR_TRANSFORM_ANGLE_TURN / 4)) >> ACROPOLIS_HELICOPTER_LANDING_PAD_SPRITE_TRIG_FRACTION_BITS;
        projection->extent.corner.y = ((work->scale * (ACROPOLIS_HELICOPTER_LANDING_PAD_EMBER_CELL_PIXELS - 1) / projection->depth) * rcos(work->angle + ACTOR_TRANSFORM_ANGLE_TURN / 4)) >> ACROPOLIS_HELICOPTER_LANDING_PAD_SPRITE_TRIG_FRACTION_BITS;
        sprite->x1                  = projection->screenX + (u16)projection->extent.corner.x;
        sprite->x2                  = projection->screenX - (u16)projection->extent.corner.x;
        sprite->y1                  = projection->screenY - (u16)projection->extent.corner.y;
        sprite->y2                  = projection->screenY + (u16)projection->extent.corner.y;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)projection->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), sprite);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
    if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
        coord->coord.t[0]  += work->move.vx;
        coord->coord.t[1]  += work->move.vy;
        coord->coord.t[2]  += work->move.vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        work->scale        += work->period;
        nextAge             = work->age + 1;
        work->age           = nextAge;
        if (nextAge > work->step * ACROPOLIS_HELICOPTER_LANDING_PAD_EMBER_FRAMES_PER_CELL - 1) {
            effectKillTask(work, task);
        }
    }
}

void acropolisHelicopterLandingPadDamagedLightSparksTask(Task* task)
{
    enum {
        ACROPOLIS_HELICOPTER_LANDING_PAD_SPARKS_LINES_AND_BURSTS     = 0,
        ACROPOLIS_HELICOPTER_LANDING_PAD_SPARKS_BURSTS_ONLY          = 1,
        ACROPOLIS_HELICOPTER_LANDING_PAD_SPARKS_RELEASE              = 2,
        ACROPOLIS_HELICOPTER_LANDING_PAD_SPARKS_LINE_LIGHT_SLOT      = 4,
        ACROPOLIS_HELICOPTER_LANDING_PAD_SPARKS_BURST_LIGHT_SLOT     = 5,
        ACROPOLIS_HELICOPTER_LANDING_PAD_SPARKS_LIGHT_FRAMES         = 4,
        ACROPOLIS_HELICOPTER_LANDING_PAD_SPARKS_CHILD_COUNT          = 6,
        ACROPOLIS_HELICOPTER_LANDING_PAD_SPARKS_LINE_INNER_DISTANCE  = 5600,
        ACROPOLIS_HELICOPTER_LANDING_PAD_SPARKS_LINE_OUTER_DISTANCE  = 6400,
        ACROPOLIS_HELICOPTER_LANDING_PAD_SPARKS_BURST_INNER_DISTANCE = 4000,
        ACROPOLIS_HELICOPTER_LANDING_PAD_SPARKS_BURST_OUTER_DISTANCE = 4800,
    };

    EffectWork*                    work;
    GfxCoord*                      coord;
    WorldCoordTransientPointLight* lightSlot;
    WorldCoordPointLight*          light;
    EffectWork*                    childWork;
    s32                            sparkIndex;
    s32                            upperSparkCount;
    s32                            audioPan;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (task->state == ACROPOLIS_HELICOPTER_LANDING_PAD_SPARKS_RELEASE) {
            effectKillTask(work, task);
        }
        return;
    }
    // Follow the damaged model part in its local frame for the effect lifetime.
    if (work->index == 0) {
        coord->parent       = work->parent;
        coord->coord.t[0]   = work->pos.vx;
        coord->coord.t[1]   = work->pos.vy;
        coord->coord.t[2]   = work->pos.vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(coord);
        work->scale = 1;
        work->index++;
    }
    work->age++;
    switch (task->state) {
        case ACROPOLIS_HELICOPTER_LANDING_PAD_SPARKS_LINES_AND_BURSTS:
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            upperSparkCount = (gRandomLcgState >> 16) & 3;
            for (sparkIndex = 0; sparkIndex < upperSparkCount; sparkIndex++) {
                _acropolisHelicopterLandingPadDrawUpperSparkLine(coord);
            }
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if (((gRandomLcgState >> 16) & 3) == 0) {
                if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                    acropolisHelicopterLandingPadDrawLowerSparkLine(coord);
                }
            }
            lightSlot             = &gWorldCoordTransientPointLights[ACROPOLIS_HELICOPTER_LANDING_PAD_SPARKS_LINE_LIGHT_SLOT];
            light                 = &lightSlot->light;
            lightSlot->framesLeft = ACROPOLIS_HELICOPTER_LANDING_PAD_SPARKS_LIGHT_FRAMES;
            light->inner          = ACROPOLIS_HELICOPTER_LANDING_PAD_SPARKS_LINE_INNER_DISTANCE;
            light->outer          = ACROPOLIS_HELICOPTER_LANDING_PAD_SPARKS_LINE_OUTER_DISTANCE;
            light->head.color.r   = ACROPOLIS_HELICOPTER_LANDING_PAD_LIGHT_HALF_INTENSITY;
            light->head.color.g   = ACROPOLIS_HELICOPTER_LANDING_PAD_LIGHT_HALF_INTENSITY;
            gRandomLcgState       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            light->head.color.b   = ((gRandomLcgState >> 16) & ACROPOLIS_HELICOPTER_LANDING_PAD_LIGHT_BLUE_RANDOM_MASK) + ACROPOLIS_HELICOPTER_LANDING_PAD_LIGHT_BLUE_BASE;
            gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coord->workm, &lightSlot->light.head.transform.coord.coord);
            lightSlot->light.head.transform.coord.composeStamp = GRAPHICS_COORD_DIRTY;
            /* fallthrough */
        case ACROPOLIS_HELICOPTER_LANDING_PAD_SPARKS_BURSTS_ONLY:
            if ((gDisplayState.animFrame & 7) == 0) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                if (((gRandomLcgState >> 16) & 3) == 0) {
                    work->scale = 1;
                }
            }
            // Link emitted tasks under this controller so parent teardown reaches them.
            if (work->scale != 0) {
                audioPan = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(SOUND_HELICOPTER_LANDING_PAD_LIGHT_SPARK, audioPan, (s8)worldCoordGetOriginAudioDepth(coord));
                work->scale = 0;
                childWork   = effectSpawn(EFFECT_IMPACT_SPARK, coord, 0x200, NULL);
                if (childWork != NULL) {
                    taskReparent(task, childWork->task);
                }
                for (sparkIndex = 0; sparkIndex < ACROPOLIS_HELICOPTER_LANDING_PAD_SPARKS_CHILD_COUNT; sparkIndex++) {
                    childWork = effectSpawn(EFFECT_PIXEL_SPARK, coord, 1, NULL);
                    if (childWork != NULL) {
                        taskReparent(task, childWork->task);
                    }
                }
                lightSlot             = &gWorldCoordTransientPointLights[ACROPOLIS_HELICOPTER_LANDING_PAD_SPARKS_BURST_LIGHT_SLOT];
                light                 = &lightSlot->light;
                lightSlot->framesLeft = ACROPOLIS_HELICOPTER_LANDING_PAD_SPARKS_LIGHT_FRAMES;
                light->inner          = ACROPOLIS_HELICOPTER_LANDING_PAD_SPARKS_BURST_INNER_DISTANCE;
                light->outer          = ACROPOLIS_HELICOPTER_LANDING_PAD_SPARKS_BURST_OUTER_DISTANCE;
                light->head.color.r   = ONE * 3 / 4;
                light->head.color.g   = ONE * 3 / 4;
                light->head.color.b   = ONE * 3 / 8;
                gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coord->workm, &lightSlot->light.head.transform.coord.coord);
                lightSlot->light.head.transform.coord.composeStamp = GRAPHICS_COORD_DIRTY;
            }
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_SPARKS_RELEASE:
            effectKillTask(work, task);
            break;
    }
}

/// Draws one blue spark from above the coordinate toward its origin.
///
/// Borrows a live coordinate chain and composes it. Local endpoint 0 has x/z
/// in [-32,31] and y in [-192,-65]; endpoint 1 has x/z in [-64,63] and y in
/// [-128,126]. Negative local Y is the upper side of an upright light.
/// The transformed endpoints narrow to signed 16-bit view coordinates before
/// projection. Queues one opaque `LINE_F2`, with a random green byte and half
/// that value in red, unless the second projection has negative GTE flags.
/// Uses that endpoint's `SZ3` / 4 for ordering, consumes seven LCG samples and
/// 32 scratch bytes, and retains no pointers after returning.
static void _acropolisHelicopterLandingPadDrawUpperSparkLine(GfxCoord* coord)
{
    EffectLineScratch* line;
    LINE_F2*           spark;
    u32                colorSample;
    u16                greenIntensity;

    actorRenderComposeCoord(coord);
    line                  = SCRATCH_STACK_RESERVE_BLOCK(EffectLineScratch);
    gRandomLcgState       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    line->endpoints[0].vx = ((gRandomLcgState >> 16) & 0x3F) - 0x20;
    gRandomLcgState       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    line->endpoints[0].vy = ((gRandomLcgState >> 16) & 0x7F) - 0xC0;
    gRandomLcgState       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    line->endpoints[0].vz = ((gRandomLcgState >> 16) & 0x3F) - 0x20;
    // Convert the local endpoint to view space, narrowing each component to s16.
    ACROPOLIS_HELICOPTER_LANDING_PAD_TRANSFORM_SPARK_ENDPOINT(line, 0, coord);
    gRandomLcgState       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    line->endpoints[1].vx = ((gRandomLcgState >> 16) & 0x7F) - 0x40;
    gRandomLcgState       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    line->endpoints[1].vy = ((gRandomLcgState >> 16) % 0xFF) - 0x80;
    gRandomLcgState       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    line->endpoints[1].vz = ((gRandomLcgState >> 16) & 0x7F) - 0x40;
    ACROPOLIS_HELICOPTER_LANDING_PAD_TRANSFORM_SPARK_ENDPOINT(line, 1, coord);
    // Project both ends; only the second endpoint supplies rejection flags and depth.
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&line->endpoints[0]);
    gte_rtps();
    gte_stsxy(&line->screenEndpoints[0]);
    gte_ldv0(&line->endpoints[1]);
    gte_rtps();
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    colorSample     = (gRandomLcgState >> 16) & 0xFF;
    greenIntensity  = colorSample;
    gte_stsxy(&line->screenEndpoints[1]);
    gte_stflg(&line->projectionFlags);
    if (line->projectionFlags >= 0) {
        gte_stszotz(&line->depth);
        spark          = gGpuPrimCursor;
        gGpuPrimCursor = spark + 1;
        setLineF2(spark);
        setRGB0(spark, colorSample >> 1, greenIntensity, 0xFF);
        spark->x0 = line->screenEndpoints[0].vx;
        spark->y0 = line->screenEndpoints[0].vy;
        spark->x1 = line->screenEndpoints[1].vx;
        spark->y1 = line->screenEndpoints[1].vy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)line->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), spark);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectLineScratch);
}

void acropolisHelicopterLandingPadDrawLowerSparkLine(GfxCoord* coord)
{
    EffectLineScratch* line;
    LINE_F2*           spark;
    u32                colorSample;
    u16                greenIntensity;

    actorRenderComposeCoord(coord);
    line                  = SCRATCH_STACK_RESERVE_BLOCK(EffectLineScratch);
    gRandomLcgState       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    line->endpoints[0].vx = ((gRandomLcgState >> 16) & 0x3F) - 0x20;
    gRandomLcgState       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    line->endpoints[0].vy = (gRandomLcgState >> 16) & 0x7F;
    gRandomLcgState       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    line->endpoints[0].vz = ((gRandomLcgState >> 16) & 0x3F) - 0x20;
    // Convert the local endpoint to view space, narrowing each component to s16.
    ACROPOLIS_HELICOPTER_LANDING_PAD_TRANSFORM_SPARK_ENDPOINT(line, 0, coord);
    gRandomLcgState       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    line->endpoints[1].vx = ((gRandomLcgState >> 16) & 0x3F) - 0x20;
    gRandomLcgState       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    line->endpoints[1].vy = (gRandomLcgState >> 16) & 0xFF;
    gRandomLcgState       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    line->endpoints[1].vz = ((gRandomLcgState >> 16) & 0x3F) - 0x20;
    ACROPOLIS_HELICOPTER_LANDING_PAD_TRANSFORM_SPARK_ENDPOINT(line, 1, coord);
    // Project both ends; only the second endpoint supplies rejection flags and depth.
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&line->endpoints[0]);
    gte_rtps();
    gte_stsxy(&line->screenEndpoints[0]);
    gte_ldv0(&line->endpoints[1]);
    gte_rtps();
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    colorSample     = (gRandomLcgState >> 16) & 0xFF;
    greenIntensity  = colorSample;
    gte_stsxy(&line->screenEndpoints[1]);
    gte_stflg(&line->projectionFlags);
    if (line->projectionFlags >= 0) {
        gte_stszotz(&line->depth);
        spark          = gGpuPrimCursor;
        gGpuPrimCursor = spark + 1;
        setLineF2(spark);
        setRGB0(spark, colorSample >> 1, greenIntensity, 0xFF);
        spark->x0 = line->screenEndpoints[0].vx;
        spark->y0 = line->screenEndpoints[0].vy;
        spark->x1 = line->screenEndpoints[1].vx;
        spark->y1 = line->screenEndpoints[1].vy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)line->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), spark);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectLineScratch);
}

#undef ACROPOLIS_HELICOPTER_LANDING_PAD_TRANSFORM_SPARK_ENDPOINT

void acropolisHelicopterLandingPadFlareEmitterTask(Task* task)
{
    enum {
        ACROPOLIS_HELICOPTER_LANDING_PAD_FLARE_EMITTER_FLASH               = 0,
        ACROPOLIS_HELICOPTER_LANDING_PAD_FLARE_EMITTER_RAW                 = 1,
        ACROPOLIS_HELICOPTER_LANDING_PAD_FLARE_EMITTER_EMBERS              = 2,
        ACROPOLIS_HELICOPTER_LANDING_PAD_FLARE_EMITTER_RELEASE             = 3,
        ACROPOLIS_HELICOPTER_LANDING_PAD_FLARE_EMITTER_LIGHT_SLOT          = 4,
        ACROPOLIS_HELICOPTER_LANDING_PAD_FLARE_EMITTER_LIGHT_FRAMES        = 4,
        ACROPOLIS_HELICOPTER_LANDING_PAD_FLARE_EMITTER_INNER_DISTANCE      = 6400,
        ACROPOLIS_HELICOPTER_LANDING_PAD_FLARE_EMITTER_OUTER_DISTANCE      = 7200,
        ACROPOLIS_HELICOPTER_LANDING_PAD_FLARE_EMITTER_FRAME_HALF_BIT      = 0x40,
        ACROPOLIS_HELICOPTER_LANDING_PAD_FLARE_EMITTER_LARGE_EMBER_VARIANT = 2,
    };

    EffectWork*                    work;
    GfxCoord*                      coord;
    WorldCoordTransientPointLight* lightSlot;
    WorldCoordPointLight*          light;

    lightSlot = &gWorldCoordTransientPointLights[ACROPOLIS_HELICOPTER_LANDING_PAD_FLARE_EMITTER_LIGHT_SLOT];
    light     = &lightSlot->light;
    work      = task->spawnArg2.pointer;
    coord     = task->extra.coordBody->coord;
    if (task->state == ACROPOLIS_HELICOPTER_LANDING_PAD_FLARE_EMITTER_RELEASE) {
        effectKillTask(work, task);
        return;
    }
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING && task->state < ACROPOLIS_HELICOPTER_LANDING_PAD_FLARE_EMITTER_RELEASE) {
        return;
    }
    actorRenderComposeCoord(coord);
    switch (task->state) {
        case ACROPOLIS_HELICOPTER_LANDING_PAD_FLARE_EMITTER_FLASH:
            effectSpawn(EFFECT_ACROPOLIS_HELIPAD_LENS_FLARE, coord, 1, NULL);
            effectSpawn(EFFECT_ACROPOLIS_HELIPAD_LENS_FLARE, coord, 1, NULL);
            lightSlot->framesLeft = ACROPOLIS_HELICOPTER_LANDING_PAD_FLARE_EMITTER_LIGHT_FRAMES;
            light->inner          = ACROPOLIS_HELICOPTER_LANDING_PAD_FLARE_EMITTER_INNER_DISTANCE;
            light->outer          = ACROPOLIS_HELICOPTER_LANDING_PAD_FLARE_EMITTER_OUTER_DISTANCE;
            light->head.color.r   = ACROPOLIS_HELICOPTER_LANDING_PAD_LIGHT_HALF_INTENSITY;
            light->head.color.g   = ACROPOLIS_HELICOPTER_LANDING_PAD_LIGHT_HALF_INTENSITY;
            gRandomLcgState       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            light->head.color.b   = ((gRandomLcgState >> 16) & ACROPOLIS_HELICOPTER_LANDING_PAD_LIGHT_BLUE_RANDOM_MASK) + ACROPOLIS_HELICOPTER_LANDING_PAD_LIGHT_BLUE_BASE;
            // Spawn places the emitter under the view; copy that parent-frame translation.
            light->head.transform.coord.coord.t[0]             = coord->coord.t[0];
            light->head.transform.coord.coord.t[1]             = coord->coord.t[1];
            light->head.transform.coord.coord.t[2]             = coord->coord.t[2];
            lightSlot->light.head.transform.coord.composeStamp = GRAPHICS_COORD_DIRTY;
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_FLARE_EMITTER_RAW:
            effectSpawn(EFFECT_ACROPOLIS_HELIPAD_LENS_FLARE, coord, 0, NULL);
            effectSpawn(EFFECT_ACROPOLIS_HELIPAD_LENS_FLARE, coord, 0, NULL);
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_FLARE_EMITTER_EMBERS:
            if (gDisplayState.animFrame & ACROPOLIS_HELICOPTER_LANDING_PAD_FLARE_EMITTER_FRAME_HALF_BIT) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                if (((gRandomLcgState >> 16) & 0xF) == 0) {
                    effectSpawn(EFFECT_ACROPOLIS_HELIPAD_EMBER, coord, ACROPOLIS_HELICOPTER_LANDING_PAD_FLARE_EMITTER_LARGE_EMBER_VARIANT, NULL);
                }
            }
            break;
        case ACROPOLIS_HELICOPTER_LANDING_PAD_FLARE_EMITTER_RELEASE:
            effectKillTask(work, task);
            break;
    }
}

void acropolisHelicopterLandingPadLensFlareTask(Task* task)
{
    enum {
        ACROPOLIS_HELICOPTER_LANDING_PAD_FLARE_INITIALIZE      = 0,
        ACROPOLIS_HELICOPTER_LANDING_PAD_FLARE_CLUT            = 0x4384,
        ACROPOLIS_HELICOPTER_LANDING_PAD_FLARE_CELL_PIXELS     = 40,
        ACROPOLIS_HELICOPTER_LANDING_PAD_FLARE_TEXTURE_ROW     = 0x48,
        ACROPOLIS_HELICOPTER_LANDING_PAD_FLARE_FRAMES_PER_CELL = 6,
        ACROPOLIS_HELICOPTER_LANDING_PAD_FLARE_FADE_FRAMES     = 8,
    };

    EffectWork*         work;
    GfxCoord*           coord;
    EffectShapeScratch* projection;
    POLY_FT4*           sprite;
    s32                 lifetimeFrames;
    s32                 age;
    s32                 fadeIntensity;
    u8                  greenIntensity;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_HIDDEN) {
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(work, task);
        }
        return;
    }
    actorRenderComposeCoord(coord);
    if (task->state == ACROPOLIS_HELICOPTER_LANDING_PAD_FLARE_INITIALIZE) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->scale     = ((gRandomLcgState >> 16) & 0x1FF) + 0x200;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->angle     = (gRandomLcgState >> 16) & ACTOR_TRANSFORM_ANGLE_MASK;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->step      = ((gRandomLcgState >> 16) & 3) + 1;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->move.vx   = -((gRandomLcgState >> 16) & 0x1F) - 0x40;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->move.vy   = ((gRandomLcgState >> 16) & 0xF) - 8;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->move.vz   = ((gRandomLcgState >> 16) & 0xF) - 8;
        task->state++;
    }
    projection = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
    _acropolisHelicopterLandingPadProjectSpriteCentre(projection, coord);
    if (projection->projectionFlags >= 0) {
        gte_stszotz(&projection->depth);
        sprite         = gGpuPrimCursor;
        gGpuPrimCursor = sprite + 1;
        setPolyFT4(sprite);
        // Retain the strict fade threshold: the final seven drawn ages fade.
        lifetimeFrames = work->step * ACROPOLIS_HELICOPTER_LANDING_PAD_FLARE_FRAMES_PER_CELL;
        age            = work->age;
        if (lifetimeFrames - ACROPOLIS_HELICOPTER_LANDING_PAD_FLARE_FADE_FRAMES < age) {
            fadeIntensity = (lifetimeFrames - age + 1) * 16;
            setRGB0(sprite, fadeIntensity, fadeIntensity, fadeIntensity);
        } else {
            if (task->spawnArg1.value != 0) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                if (((gRandomLcgState >> 16) & 3) == 0) {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    greenIntensity  = gRandomLcgState >> 16;
                    setRGB0(sprite, greenIntensity >> 1, greenIntensity, 0xFF);
                } else {
                    setShadeTex(sprite, true);
                }
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                if (((gRandomLcgState >> 16) & 0xF) == 0 && gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                    effectSpawn(EFFECT_FLASH_BURST, coord, 0x100, NULL);
                }
            } else {
                sprite->code = ACROPOLIS_HELICOPTER_LANDING_PAD_SPRITE_RAW_TEXTURE_CODE;
            }
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if (((gRandomLcgState >> 16) & 0xF) == 0 && gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                effectSpawn(EFFECT_ACROPOLIS_HELIPAD_EMBER, coord, 2 - task->spawnArg1.value, NULL);
            }
        }
        sprite->tpage = ACROPOLIS_HELICOPTER_LANDING_PAD_SPRITE_TEXTURE_PAGE;
        setSemiTrans(sprite, true);
        sprite->clut                = ACROPOLIS_HELICOPTER_LANDING_PAD_FLARE_CLUT;
        sprite->u0                  = (work->age / work->step) * ACROPOLIS_HELICOPTER_LANDING_PAD_FLARE_CELL_PIXELS;
        sprite->v0                  = ACROPOLIS_HELICOPTER_LANDING_PAD_FLARE_TEXTURE_ROW;
        sprite->u1                  = (work->age / work->step) * ACROPOLIS_HELICOPTER_LANDING_PAD_FLARE_CELL_PIXELS + (ACROPOLIS_HELICOPTER_LANDING_PAD_FLARE_CELL_PIXELS - 1);
        sprite->v1                  = ACROPOLIS_HELICOPTER_LANDING_PAD_FLARE_TEXTURE_ROW;
        sprite->u2                  = (work->age / work->step) * ACROPOLIS_HELICOPTER_LANDING_PAD_FLARE_CELL_PIXELS;
        sprite->v2                  = ACROPOLIS_HELICOPTER_LANDING_PAD_FLARE_TEXTURE_ROW + ACROPOLIS_HELICOPTER_LANDING_PAD_FLARE_CELL_PIXELS - 1;
        sprite->u3                  = (work->age / work->step) * ACROPOLIS_HELICOPTER_LANDING_PAD_FLARE_CELL_PIXELS + (ACROPOLIS_HELICOPTER_LANDING_PAD_FLARE_CELL_PIXELS - 1);
        sprite->v3                  = ACROPOLIS_HELICOPTER_LANDING_PAD_FLARE_TEXTURE_ROW + ACROPOLIS_HELICOPTER_LANDING_PAD_FLARE_CELL_PIXELS - 1;
        projection->extent.corner.x = ((work->scale * (ACROPOLIS_HELICOPTER_LANDING_PAD_FLARE_CELL_PIXELS - 1) / projection->depth) * rsin(work->angle)) >> ACROPOLIS_HELICOPTER_LANDING_PAD_SPRITE_TRIG_FRACTION_BITS;
        projection->extent.corner.y = ((work->scale * (ACROPOLIS_HELICOPTER_LANDING_PAD_FLARE_CELL_PIXELS - 1) / projection->depth) * rcos(work->angle)) >> ACROPOLIS_HELICOPTER_LANDING_PAD_SPRITE_TRIG_FRACTION_BITS;
        sprite->x0                  = projection->screenX + (u16)projection->extent.corner.x;
        sprite->x3                  = projection->screenX - (u16)projection->extent.corner.x;
        sprite->y0                  = projection->screenY - (u16)projection->extent.corner.y;
        sprite->y3                  = projection->screenY + (u16)projection->extent.corner.y;
        projection->extent.corner.x = ((work->scale * (ACROPOLIS_HELICOPTER_LANDING_PAD_FLARE_CELL_PIXELS - 1) / projection->depth) * rsin(work->angle + ACTOR_TRANSFORM_ANGLE_TURN / 4)) >> ACROPOLIS_HELICOPTER_LANDING_PAD_SPRITE_TRIG_FRACTION_BITS;
        projection->extent.corner.y = ((work->scale * (ACROPOLIS_HELICOPTER_LANDING_PAD_FLARE_CELL_PIXELS - 1) / projection->depth) * rcos(work->angle + ACTOR_TRANSFORM_ANGLE_TURN / 4)) >> ACROPOLIS_HELICOPTER_LANDING_PAD_SPRITE_TRIG_FRACTION_BITS;
        sprite->x1                  = projection->screenX + (u16)projection->extent.corner.x;
        sprite->x2                  = projection->screenX - (u16)projection->extent.corner.x;
        sprite->y1                  = projection->screenY - (u16)projection->extent.corner.y;
        sprite->y2                  = projection->screenY + (u16)projection->extent.corner.y;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)projection->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), sprite);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
    if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
        coord->coord.t[0]  += work->move.vx;
        coord->coord.t[1]  += work->move.vy;
        coord->coord.t[2]  += work->move.vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        work->age++;
        if (work->age > work->step * ACROPOLIS_HELICOPTER_LANDING_PAD_FLARE_FRAMES_PER_CELL - 1) {
            effectKillTask(work, task);
        }
    }
}

void acropolisHelicopterLandingPadPerimeterLightsTask(Task* task)
{
    enum {
        ACROPOLIS_HELICOPTER_LANDING_PAD_GROUND_SHADOW_DISABLED_VIEW = 18,
        ACROPOLIS_HELICOPTER_LANDING_PAD_PULSE_FRAME_SHIFT           = 2,
        ACROPOLIS_HELICOPTER_LANDING_PAD_PULSE_FALLING_BIT           = 0x80,
        ACROPOLIS_HELICOPTER_LANDING_PAD_PULSE_HALF_MASK             = 0x7F,
        ACROPOLIS_HELICOPTER_LANDING_PAD_PULSE_RISING_MASK           = 0x7C,
    };

    EffectWork*    work = task->spawnArg2.pointer;
    const SVECTOR* worldPoint;
    s32            lightIndex;
    s32            phase;
    s32            halfBrightness;

    if ((viewGetMappedIndex() & 0xFF) == ACROPOLIS_HELICOPTER_LANDING_PAD_GROUND_SHADOW_DISABLED_VIEW) {
        gRoomEffectState->groundShadowShade = ROOM_EFFECT_GROUND_SHADOW_DISABLED;
    } else {
        gRoomEffectState->groundShadowShade = ROOM_EFFECT_GROUND_SHADOW_UNMODULATED;
    }

    // Keep the full word for the wave; the work field deliberately narrows to s16.
    phase       = gDisplayState.animFrame << ACROPOLIS_HELICOPTER_LANDING_PAD_PULSE_FRAME_SHIFT;
    work->scale = phase;
    if (phase & ACROPOLIS_HELICOPTER_LANDING_PAD_PULSE_FALLING_BIT) {
        halfBrightness = ACROPOLIS_HELICOPTER_LANDING_PAD_PULSE_HALF_MASK - (phase & ACROPOLIS_HELICOPTER_LANDING_PAD_PULSE_HALF_MASK);
    } else {
        halfBrightness = phase & ACROPOLIS_HELICOPTER_LANDING_PAD_PULSE_RISING_MASK;
    }
    work->scale = halfBrightness * 2;

    lightIndex = 0;
    worldPoint = D_acropolis_helicopter_landing_pad_80184E80;
    for (; lightIndex < ARRAY_SIZE(D_acropolis_helicopter_landing_pad_80184E80); lightIndex++) {
        _acropolisHelicopterLandingPadDrawPerimeterLight(worldPoint++, lightIndex, work->scale);
    }
}

#include "../../shared/actor_contacts_push_contact.inc.c"

#include "../../shared/actor_contacts_push.inc.c"

void acropolisHelicopterLandingPadPickupModelTask(Task* task)
{
    enum {
        ACROPOLIS_HELICOPTER_LANDING_PAD_PICKUP_COLLECTED = 2,
    };

    Enemy*     placement;
    TmdObject* model;
    s32        placementState;

    placement      = task->spawnArg2.pointer;
    model          = task->extra.tmd;
    placementState = areaGetCurrentObjectState((u8)placement->placeKey);
    viewGetMappedIndex();
    if (placementState == ACROPOLIS_HELICOPTER_LANDING_PAD_PICKUP_COLLECTED) {
        model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    } else {
        model->flags    = TMD_OBJECT_FLAGGED_PASS;
        model->otOffset = 0;
        tmdAllocPrimitiveBuffer(model);
    }
}
