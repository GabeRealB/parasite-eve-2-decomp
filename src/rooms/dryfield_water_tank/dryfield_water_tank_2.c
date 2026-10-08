#include "rooms/dryfield_water_tank.h"
#include "../../shared/water_tank.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/rand.h>

#include "common.h"

#include "dryfield_water_tank_private.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"
#include "../../shared/actor_messages.h"
#include "../../shared/screen_fade.h"

/// Requests the room's event script leaves for the event task, each carried
/// out on the task's next update.
///
/// The script issues them through its callback commands, in the order turn,
/// animation, fade, movie. Value 1 is accepted and ignored like
/// `DRYFIELD_WATER_TANK_EVENT_COMMAND_NONE`, and nothing issues it.
enum {
    /// Nothing pending.
    DRYFIELD_WATER_TANK_EVENT_COMMAND_NONE = 0,
    /// Installs the room's animation sets on the player and plays the first,
    /// then blends into the second on the following frame.
    DRYFIELD_WATER_TANK_EVENT_COMMAND_PLAY_PLAYER_ANIMATION = 2,
    /// Starts the full-screen fade-out tile.
    DRYFIELD_WATER_TANK_EVENT_COMMAND_FADE_OUT = 3,
    /// Turns the player to yaw 0x800.
    DRYFIELD_WATER_TANK_EVENT_COMMAND_TURN_PLAYER = 4,
    /// Starts the room's movie task and the view tasks, then three frames
    /// later puts the player at the room placement in the equipped weapon's
    /// stance.
    DRYFIELD_WATER_TANK_EVENT_COMMAND_PLAY_MOVIE = 5,
};

/// Work block of the room's event task, which carries out the requests its
/// event script leaves here.
///
/// The task allocates and zeroes the whole block when the event starts and
/// publishes itself so the script's callbacks can reach it.
typedef struct {
    Task* player;      // Player task registered when the event started, the receiver of the event's animation, turn and placement messages; tested for NULL before the animation requests only
    u16   command;     // Pending `DRYFIELD_WATER_TANK_EVENT_COMMAND_*`, cleared once carried out
    u16   commandStep; // Frames already spent on a command that takes several; zeroed with each new command
    byte  field_8[4];  // Allocated and cleared with the block, never accessed; role and type unproven
} _DryfieldWaterTankEventWork;
STATIC_ASSERT_SIZEOF(_DryfieldWaterTankEventWork, 0xC);

/// Main-executable globals with no module header yet: the cutscene task
/// refuses to start while `Gp_StateC08.mode` is 1 or `gDisplayState.pendingMode` is non-zero.
/// `gPlayerStatus.weapon` is the equipped-weapon index the slot-3 msg 0x3E8 animation
/// record is keyed on, and `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId` picks which of the two weapon-id bases
/// that record uses.

/// Main-executable flag set to 1 before the view tasks are respawned.

/// Spawn table for the task that takes over once the intro stream is done.
extern TaskDesc D_dryfield_water_tank_80180764[];

/// Script record the player is handed with msg 0x3F4.
extern AnimationSet* D_dryfield_water_tank_801804EC[2];

/// The placement the room sends the player task with message 0x3E9.
extern ActorTransform D_dryfield_water_tank_801804F4;

/// The two event scripts `evsStartScriptWithSkip` is handed.
extern EvsCommand D_dryfield_water_tank_8018050C[];
extern EvsCommand D_dryfield_water_tank_8018068C[];

/// The water tank's run: one `SVECTOR` position per frame, `y` fixed at -12000
/// and `z` stepping up the room, 52 entries of movement before the tail clamps.
extern SVECTOR D_dryfield_water_tank_80184530[];

/// The second leg of the tank's run, same `SVECTOR` shape and one entry per
/// frame: the seam repeats the first table's last entry (`x` 2532, `y` -12000,
/// `z` 972), after which `x` steps down while `z` holds. Exactly the 52 entries
/// its walk consumes, so unlike the first table it has no clamp tail.
extern SVECTOR D_dryfield_water_tank_801847C0[];

void func_dryfield_water_tank_8017E9F8(Task*);

static TmdSource _gDryfieldWaterTankModel020D4;
static void      _dryfieldWaterTankFadeOutTileTask(Task* task);
static void      _dryfieldWaterTankMovieTask(Task* task);
void             func_dryfield_water_tank_8017EB80(s16);
static void      _dryfieldWaterTankRestorePlayerAfterSkip(void);

ActorTransform D_dryfield_water_tank_8017F0D0 = { { 820, -0x4010, 884, 0 }, { 0, 2560, 0, 0 } };

ActorTransform D_dryfield_water_tank_8017F0E8 = { { 1868, -0x2EE0, 1740, 0 }, { 0, 512, 0, 0 } };

AnimationPlayRequest D_dryfield_water_tank_8017F100 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

EvsCommand D_dryfield_water_tank_8017F114[11] = {
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_water_tank_8017F100 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 33 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_dryfield_water_tank_8017F0D0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_STAGE_SOUND, { .value = 0x52150001 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_dryfield_water_tank_8017F21C[11] = {
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_dryfield_water_tank_8017F100 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 33 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_dryfield_water_tank_8017F0E8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_STAGE_SOUND, { .value = 0x52150001 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 32 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskMessageEntry D_dryfield_water_tank_8017F324[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, dryfieldWaterTankResolveRoomEvent },
    { ROOM_MESSAGE_USE_KEY_ITEM, dryfieldWaterTankRefuseKeyItem },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_dryfield_water_tank_8017D7EC },
    { ROOM_MESSAGE_COMMAND, func_dryfield_water_tank_8017D910 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_dryfield_water_tank_8017F34C[2] = {
    { { { TASK_BODY_NONE, 32 } }, func_dryfield_water_tank_8017D948, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, func_dryfield_water_tank_8017D618, { .value = 0 } },
};

static TmdBone _gDryfieldWaterTankModel020D4Skeleton[1] = {
#include "assets/dryfield_water_tank_model_020D4_skeleton.inc"
};

static u32 _gDryfieldWaterTankModel020D4PartVerts[1] = {
#include "assets/dryfield_water_tank_model_020D4_partVerts.inc"
};

static SVECTOR _gDryfieldWaterTankModel020D4Verts[97] = {
#include "assets/dryfield_water_tank_model_020D4_verts.inc"
};

static u32 _gDryfieldWaterTankModel020D4Stream[426] = {
#include "assets/dryfield_water_tank_model_020D4_stream.inc"
};

static TmdSource _gDryfieldWaterTankModel020D4 = {
    0,
    3360,
    0,
    1,
    _gDryfieldWaterTankModel020D4PartVerts,
    _gDryfieldWaterTankModel020D4Verts,
    &_gDryfieldWaterTankModel020D4Verts[97],
    _gDryfieldWaterTankModel020D4Skeleton,
    _gDryfieldWaterTankModel020D4Stream,
};

ActorTransform D_dryfield_water_tank_8017FD60[2] = {
    { { 3100, 0, 0, 0 }, { 0, 1024, 0, 0 } },
    { { 3100, 0, 1800, 0 }, { 0, 1024, 0, 0 } },
};

TaskMessageEntry D_dryfield_water_tank_8017FD90[3] = {
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceYawPitchRoll },
    { ACTOR_COMMAND_MESSAGE_APPLY, dryfieldWaterTankRestartPropSlide },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, dryfieldWaterTankSetPropModelDraw },
};

u16 D_dryfield_water_tank_8017FDA8[12] = {
    400,
    1000,
    0xFE70,
    0xFF38,
    0,
    0xFDA8,
    200,
    800,
    0xFC18,
    0xFCE0,
    600,
    0,
};

EvsCommand D_dryfield_water_tank_8017FDC0[11] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 15 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = dryfieldWaterTankPostPropSceneRequest }, { .value = DRYFIELD_WATER_TANK_PROP_SCENE_REQUEST_START_SLIDE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = dryfieldWaterTankPostPropSceneRequest }, { .value = DRYFIELD_WATER_TANK_PROP_SCENE_REQUEST_PLAY_SOUNDS }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = dryfieldWaterTankPostPropSceneRequest }, { .value = DRYFIELD_WATER_TANK_PROP_SCENE_REQUEST_SHOW_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_dryfield_water_tank_8017FEC8[8] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = dryfieldWaterTankSkipPropScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc D_dryfield_water_tank_8017FF88[2] = {
    { { { TASK_BODY_NONE, 192 } }, func_dryfield_water_tank_8017DEA4, { .value = 0 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_dryfield_water_tank_8017DD20, { .model = &_gDryfieldWaterTankModel020D4 } },
};

static AnimationPackedPose _gDryfieldWaterTankAnimation02B70Bank1[2] = {
#include "assets/dryfield_water_tank_animation_02B70_bank1.inc"
};

static AnimationPackedRotation _gDryfieldWaterTankAnimation02B70Bank4[8] = {
#include "assets/dryfield_water_tank_animation_02B70_bank4.inc"
};

static AnimationRecord _gDryfieldWaterTankAnimation02B70Records[76] = {
#include "assets/dryfield_water_tank_animation_02B70_records.inc"
};

static u16 _gDryfieldWaterTankAnimation02B70Indices[20] = {
#include "assets/dryfield_water_tank_animation_02B70_indices.inc"
};

static AnimationSet _gDryfieldWaterTankAnimation02B70 = {
    _gDryfieldWaterTankAnimation02B70Records,
    _gDryfieldWaterTankAnimation02B70Indices,
    { NULL, _gDryfieldWaterTankAnimation02B70Bank1, NULL, NULL, _gDryfieldWaterTankAnimation02B70Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldWaterTankAnimation02F04Bank1[7] = {
#include "assets/dryfield_water_tank_animation_02F04_bank1.inc"
};

static AnimationPackedRotation _gDryfieldWaterTankAnimation02F04Bank4[65] = {
#include "assets/dryfield_water_tank_animation_02F04_bank4.inc"
};

static AnimationRecord _gDryfieldWaterTankAnimation02F04Records[123] = {
#include "assets/dryfield_water_tank_animation_02F04_records.inc"
};

static u16 _gDryfieldWaterTankAnimation02F04Indices[20] = {
#include "assets/dryfield_water_tank_animation_02F04_indices.inc"
};

static AnimationSet _gDryfieldWaterTankAnimation02F04 = {
    _gDryfieldWaterTankAnimation02F04Records,
    _gDryfieldWaterTankAnimation02F04Indices,
    { NULL, _gDryfieldWaterTankAnimation02F04Bank1, NULL, NULL, _gDryfieldWaterTankAnimation02F04Bank4, NULL, NULL, NULL },
};

AnimationSet* D_dryfield_water_tank_801804EC[2] = {
    &_gDryfieldWaterTankAnimation02B70,
    &_gDryfieldWaterTankAnimation02F04,
};

ActorTransform D_dryfield_water_tank_801804F4 = { { 2600, -0x2EE0, -450, 0 }, { 0, 512, 0, 0 } };

EvsCommand D_dryfield_water_tank_8018050C[16] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 3 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_dryfield_water_tank_8017EB80 }, { .value = DRYFIELD_WATER_TANK_EVENT_COMMAND_TURN_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_dryfield_water_tank_8017EB80 }, { .value = DRYFIELD_WATER_TANK_EVENT_COMMAND_PLAY_PLAYER_ANIMATION }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_dryfield_water_tank_8017EB80 }, { .value = DRYFIELD_WATER_TANK_EVENT_COMMAND_FADE_OUT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_dryfield_water_tank_8017EB80 }, { .value = DRYFIELD_WATER_TANK_EVENT_COMMAND_PLAY_MOVIE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_dryfield_water_tank_8018068C[9] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _dryfieldWaterTankRestorePlayerAfterSkip }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc D_dryfield_water_tank_80180764[4] = {
    { { { TASK_BODY_NONE, 192 } }, NULL, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _dryfieldWaterTankMovieTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, screenFadeInTileTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _dryfieldWaterTankFadeOutTileTask, { .value = 0 } },
};

TaskDesc D_dryfield_water_tank_80180794 = { { { TASK_BODY_NONE, 192 } }, func_dryfield_water_tank_8017E9F8, { .value = 0 } };

static AnimationPackedPose _gDryfieldWaterTankAnimation034BCBank1[6] = {
#include "assets/dryfield_water_tank_animation_034BC_bank1.inc"
};

static AnimationPackedRotation _gDryfieldWaterTankAnimation034BCBank4[46] = {
#include "assets/dryfield_water_tank_animation_034BC_bank4.inc"
};

static AnimationRecord _gDryfieldWaterTankAnimation034BCRecords[109] = {
#include "assets/dryfield_water_tank_animation_034BC_records.inc"
};

static u16 _gDryfieldWaterTankAnimation034BCIndices[20] = {
#include "assets/dryfield_water_tank_animation_034BC_indices.inc"
};

AnimationSet gDryfieldWaterTankAnimation034BC = {
    _gDryfieldWaterTankAnimation034BCRecords,
    _gDryfieldWaterTankAnimation034BCIndices,
    { NULL, _gDryfieldWaterTankAnimation034BCBank1, NULL, NULL, _gDryfieldWaterTankAnimation034BCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldWaterTankAnimation0377CBank1[4] = {
#include "assets/dryfield_water_tank_animation_0377C_bank1.inc"
};

static AnimationPackedRotation _gDryfieldWaterTankAnimation0377CBank4[55] = {
#include "assets/dryfield_water_tank_animation_0377C_bank4.inc"
};

static AnimationRecord _gDryfieldWaterTankAnimation0377CRecords[89] = {
#include "assets/dryfield_water_tank_animation_0377C_records.inc"
};

static u16 _gDryfieldWaterTankAnimation0377CIndices[20] = {
#include "assets/dryfield_water_tank_animation_0377C_indices.inc"
};

AnimationSet gDryfieldWaterTankAnimation0377C = {
    _gDryfieldWaterTankAnimation0377CRecords,
    _gDryfieldWaterTankAnimation0377CIndices,
    { NULL, _gDryfieldWaterTankAnimation0377CBank1, NULL, NULL, _gDryfieldWaterTankAnimation0377CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldWaterTankAnimation03A60Bank1[5] = {
#include "assets/dryfield_water_tank_animation_03A60_bank1.inc"
};

static AnimationPackedRotation _gDryfieldWaterTankAnimation03A60Bank4[60] = {
#include "assets/dryfield_water_tank_animation_03A60_bank4.inc"
};

static AnimationRecord _gDryfieldWaterTankAnimation03A60Records[90] = {
#include "assets/dryfield_water_tank_animation_03A60_records.inc"
};

static u16 _gDryfieldWaterTankAnimation03A60Indices[20] = {
#include "assets/dryfield_water_tank_animation_03A60_indices.inc"
};

AnimationSet gDryfieldWaterTankAnimation03A60 = {
    _gDryfieldWaterTankAnimation03A60Records,
    _gDryfieldWaterTankAnimation03A60Indices,
    { NULL, _gDryfieldWaterTankAnimation03A60Bank1, NULL, NULL, _gDryfieldWaterTankAnimation03A60Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldWaterTankAnimation03CB4Bank1[2] = {
#include "assets/dryfield_water_tank_animation_03CB4_bank1.inc"
};

static AnimationPackedRotation _gDryfieldWaterTankAnimation03CB4Bank4[30] = {
#include "assets/dryfield_water_tank_animation_03CB4_bank4.inc"
};

static AnimationRecord _gDryfieldWaterTankAnimation03CB4Records[93] = {
#include "assets/dryfield_water_tank_animation_03CB4_records.inc"
};

static u16 _gDryfieldWaterTankAnimation03CB4Indices[20] = {
#include "assets/dryfield_water_tank_animation_03CB4_indices.inc"
};

AnimationSet gDryfieldWaterTankAnimation03CB4 = {
    _gDryfieldWaterTankAnimation03CB4Records,
    _gDryfieldWaterTankAnimation03CB4Indices,
    { NULL, _gDryfieldWaterTankAnimation03CB4Bank1, NULL, NULL, _gDryfieldWaterTankAnimation03CB4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldWaterTankAnimation04000Bank1[6] = {
#include "assets/dryfield_water_tank_animation_04000_bank1.inc"
};

static AnimationPackedRotation _gDryfieldWaterTankAnimation04000Bank4[60] = {
#include "assets/dryfield_water_tank_animation_04000_bank4.inc"
};

static AnimationRecord _gDryfieldWaterTankAnimation04000Records[113] = {
#include "assets/dryfield_water_tank_animation_04000_records.inc"
};

static u16 _gDryfieldWaterTankAnimation04000Indices[20] = {
#include "assets/dryfield_water_tank_animation_04000_indices.inc"
};

AnimationSet gDryfieldWaterTankAnimation04000 = {
    _gDryfieldWaterTankAnimation04000Records,
    _gDryfieldWaterTankAnimation04000Indices,
    { NULL, _gDryfieldWaterTankAnimation04000Bank1, NULL, NULL, _gDryfieldWaterTankAnimation04000Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldWaterTankAnimation047BCBank1[22] = {
#include "assets/dryfield_water_tank_animation_047BC_bank1.inc"
};

static AnimationPackedRotation _gDryfieldWaterTankAnimation047BCBank4[170] = {
#include "assets/dryfield_water_tank_animation_047BC_bank4.inc"
};

static AnimationRecord _gDryfieldWaterTankAnimation047BCRecords[239] = {
#include "assets/dryfield_water_tank_animation_047BC_records.inc"
};

static u16 _gDryfieldWaterTankAnimation047BCIndices[20] = {
#include "assets/dryfield_water_tank_animation_047BC_indices.inc"
};

AnimationSet gDryfieldWaterTankAnimation047BC = {
    _gDryfieldWaterTankAnimation047BCRecords,
    _gDryfieldWaterTankAnimation047BCIndices,
    { NULL, _gDryfieldWaterTankAnimation047BCBank1, NULL, NULL, _gDryfieldWaterTankAnimation047BCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldWaterTankAnimation04AA0Bank1[5] = {
#include "assets/dryfield_water_tank_animation_04AA0_bank1.inc"
};

static AnimationPackedRotation _gDryfieldWaterTankAnimation04AA0Bank4[60] = {
#include "assets/dryfield_water_tank_animation_04AA0_bank4.inc"
};

static AnimationRecord _gDryfieldWaterTankAnimation04AA0Records[90] = {
#include "assets/dryfield_water_tank_animation_04AA0_records.inc"
};

static u16 _gDryfieldWaterTankAnimation04AA0Indices[20] = {
#include "assets/dryfield_water_tank_animation_04AA0_indices.inc"
};

AnimationSet gDryfieldWaterTankAnimation04AA0 = {
    _gDryfieldWaterTankAnimation04AA0Records,
    _gDryfieldWaterTankAnimation04AA0Indices,
    { NULL, _gDryfieldWaterTankAnimation04AA0Bank1, NULL, NULL, _gDryfieldWaterTankAnimation04AA0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldWaterTankAnimation04C98Bank1[3] = {
#include "assets/dryfield_water_tank_animation_04C98_bank1.inc"
};

static AnimationPackedRotation _gDryfieldWaterTankAnimation04C98Bank4[34] = {
#include "assets/dryfield_water_tank_animation_04C98_bank4.inc"
};

static AnimationRecord _gDryfieldWaterTankAnimation04C98Records[63] = {
#include "assets/dryfield_water_tank_animation_04C98_records.inc"
};

static u16 _gDryfieldWaterTankAnimation04C98Indices[20] = {
#include "assets/dryfield_water_tank_animation_04C98_indices.inc"
};

AnimationSet gDryfieldWaterTankAnimation04C98 = {
    _gDryfieldWaterTankAnimation04C98Records,
    _gDryfieldWaterTankAnimation04C98Indices,
    { NULL, _gDryfieldWaterTankAnimation04C98Bank1, NULL, NULL, _gDryfieldWaterTankAnimation04C98Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldWaterTankAnimation04FECBank1[5] = {
#include "assets/dryfield_water_tank_animation_04FEC_bank1.inc"
};

static AnimationPackedRotation _gDryfieldWaterTankAnimation04FECBank4[58] = {
#include "assets/dryfield_water_tank_animation_04FEC_bank4.inc"
};

static AnimationRecord _gDryfieldWaterTankAnimation04FECRecords[120] = {
#include "assets/dryfield_water_tank_animation_04FEC_records.inc"
};

static u16 _gDryfieldWaterTankAnimation04FECIndices[20] = {
#include "assets/dryfield_water_tank_animation_04FEC_indices.inc"
};

AnimationSet gDryfieldWaterTankAnimation04FEC = {
    _gDryfieldWaterTankAnimation04FECRecords,
    _gDryfieldWaterTankAnimation04FECIndices,
    { NULL, _gDryfieldWaterTankAnimation04FECBank1, NULL, NULL, _gDryfieldWaterTankAnimation04FECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldWaterTankAnimation051E4Bank1[3] = {
#include "assets/dryfield_water_tank_animation_051E4_bank1.inc"
};

static AnimationPackedRotation _gDryfieldWaterTankAnimation051E4Bank4[34] = {
#include "assets/dryfield_water_tank_animation_051E4_bank4.inc"
};

static AnimationRecord _gDryfieldWaterTankAnimation051E4Records[63] = {
#include "assets/dryfield_water_tank_animation_051E4_records.inc"
};

static u16 _gDryfieldWaterTankAnimation051E4Indices[20] = {
#include "assets/dryfield_water_tank_animation_051E4_indices.inc"
};

AnimationSet gDryfieldWaterTankAnimation051E4 = {
    _gDryfieldWaterTankAnimation051E4Records,
    _gDryfieldWaterTankAnimation051E4Indices,
    { NULL, _gDryfieldWaterTankAnimation051E4Bank1, NULL, NULL, _gDryfieldWaterTankAnimation051E4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldWaterTankAnimation05704Bank1[10] = {
#include "assets/dryfield_water_tank_animation_05704_bank1.inc"
};

static AnimationPackedRotation _gDryfieldWaterTankAnimation05704Bank4[100] = {
#include "assets/dryfield_water_tank_animation_05704_bank4.inc"
};

static AnimationRecord _gDryfieldWaterTankAnimation05704Records[178] = {
#include "assets/dryfield_water_tank_animation_05704_records.inc"
};

static u16 _gDryfieldWaterTankAnimation05704Indices[20] = {
#include "assets/dryfield_water_tank_animation_05704_indices.inc"
};

AnimationSet gDryfieldWaterTankAnimation05704 = {
    _gDryfieldWaterTankAnimation05704Records,
    _gDryfieldWaterTankAnimation05704Indices,
    { NULL, _gDryfieldWaterTankAnimation05704Bank1, NULL, NULL, _gDryfieldWaterTankAnimation05704Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldWaterTankAnimation059E4Bank1[5] = {
#include "assets/dryfield_water_tank_animation_059E4_bank1.inc"
};

static AnimationPackedRotation _gDryfieldWaterTankAnimation059E4Bank4[59] = {
#include "assets/dryfield_water_tank_animation_059E4_bank4.inc"
};

static AnimationRecord _gDryfieldWaterTankAnimation059E4Records[90] = {
#include "assets/dryfield_water_tank_animation_059E4_records.inc"
};

static u16 _gDryfieldWaterTankAnimation059E4Indices[20] = {
#include "assets/dryfield_water_tank_animation_059E4_indices.inc"
};

AnimationSet gDryfieldWaterTankAnimation059E4 = {
    _gDryfieldWaterTankAnimation059E4Records,
    _gDryfieldWaterTankAnimation059E4Indices,
    { NULL, _gDryfieldWaterTankAnimation059E4Bank1, NULL, NULL, _gDryfieldWaterTankAnimation059E4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldWaterTankAnimation05DB8Bank1[8] = {
#include "assets/dryfield_water_tank_animation_05DB8_bank1.inc"
};

static AnimationPackedRotation _gDryfieldWaterTankAnimation05DB8Bank4[84] = {
#include "assets/dryfield_water_tank_animation_05DB8_bank4.inc"
};

static AnimationRecord _gDryfieldWaterTankAnimation05DB8Records[117] = {
#include "assets/dryfield_water_tank_animation_05DB8_records.inc"
};

static u16 _gDryfieldWaterTankAnimation05DB8Indices[20] = {
#include "assets/dryfield_water_tank_animation_05DB8_indices.inc"
};

AnimationSet gDryfieldWaterTankAnimation05DB8 = {
    _gDryfieldWaterTankAnimation05DB8Records,
    _gDryfieldWaterTankAnimation05DB8Indices,
    { NULL, _gDryfieldWaterTankAnimation05DB8Bank1, NULL, NULL, _gDryfieldWaterTankAnimation05DB8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldWaterTankAnimation061A8Bank1[7] = {
#include "assets/dryfield_water_tank_animation_061A8_bank1.inc"
};

static AnimationPackedRotation _gDryfieldWaterTankAnimation061A8Bank4[62] = {
#include "assets/dryfield_water_tank_animation_061A8_bank4.inc"
};

static AnimationRecord _gDryfieldWaterTankAnimation061A8Records[149] = {
#include "assets/dryfield_water_tank_animation_061A8_records.inc"
};

static u16 _gDryfieldWaterTankAnimation061A8Indices[20] = {
#include "assets/dryfield_water_tank_animation_061A8_indices.inc"
};

AnimationSet gDryfieldWaterTankAnimation061A8 = {
    _gDryfieldWaterTankAnimation061A8Records,
    _gDryfieldWaterTankAnimation061A8Indices,
    { NULL, _gDryfieldWaterTankAnimation061A8Bank1, NULL, NULL, _gDryfieldWaterTankAnimation061A8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldWaterTankAnimation06514Bank1[7] = {
#include "assets/dryfield_water_tank_animation_06514_bank1.inc"
};

static AnimationPackedRotation _gDryfieldWaterTankAnimation06514Bank4[74] = {
#include "assets/dryfield_water_tank_animation_06514_bank4.inc"
};

static AnimationRecord _gDryfieldWaterTankAnimation06514Records[104] = {
#include "assets/dryfield_water_tank_animation_06514_records.inc"
};

static u16 _gDryfieldWaterTankAnimation06514Indices[20] = {
#include "assets/dryfield_water_tank_animation_06514_indices.inc"
};

AnimationSet gDryfieldWaterTankAnimation06514 = {
    _gDryfieldWaterTankAnimation06514Records,
    _gDryfieldWaterTankAnimation06514Indices,
    { NULL, _gDryfieldWaterTankAnimation06514Bank1, NULL, NULL, _gDryfieldWaterTankAnimation06514Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldWaterTankAnimation06840Bank1[7] = {
#include "assets/dryfield_water_tank_animation_06840_bank1.inc"
};

static AnimationPackedRotation _gDryfieldWaterTankAnimation06840Bank4[56] = {
#include "assets/dryfield_water_tank_animation_06840_bank4.inc"
};

static AnimationRecord _gDryfieldWaterTankAnimation06840Records[106] = {
#include "assets/dryfield_water_tank_animation_06840_records.inc"
};

static u16 _gDryfieldWaterTankAnimation06840Indices[20] = {
#include "assets/dryfield_water_tank_animation_06840_indices.inc"
};

AnimationSet gDryfieldWaterTankAnimation06840 = {
    _gDryfieldWaterTankAnimation06840Records,
    _gDryfieldWaterTankAnimation06840Indices,
    { NULL, _gDryfieldWaterTankAnimation06840Bank1, NULL, NULL, _gDryfieldWaterTankAnimation06840Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldWaterTankAnimation06C68Bank1[5] = {
#include "assets/dryfield_water_tank_animation_06C68_bank1.inc"
};

static AnimationPackedRotation _gDryfieldWaterTankAnimation06C68Bank4[90] = {
#include "assets/dryfield_water_tank_animation_06C68_bank4.inc"
};

static AnimationRecord _gDryfieldWaterTankAnimation06C68Records[141] = {
#include "assets/dryfield_water_tank_animation_06C68_records.inc"
};

static u16 _gDryfieldWaterTankAnimation06C68Indices[20] = {
#include "assets/dryfield_water_tank_animation_06C68_indices.inc"
};

AnimationSet gDryfieldWaterTankAnimation06C68 = {
    _gDryfieldWaterTankAnimation06C68Records,
    _gDryfieldWaterTankAnimation06C68Indices,
    { NULL, _gDryfieldWaterTankAnimation06C68Bank1, NULL, NULL, _gDryfieldWaterTankAnimation06C68Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gDryfieldWaterTankAnimation06F48Bank1[3] = {
#include "assets/dryfield_water_tank_animation_06F48_bank1.inc"
};

static AnimationPackedRotation _gDryfieldWaterTankAnimation06F48Bank4[51] = {
#include "assets/dryfield_water_tank_animation_06F48_bank4.inc"
};

static AnimationRecord _gDryfieldWaterTankAnimation06F48Records[104] = {
#include "assets/dryfield_water_tank_animation_06F48_records.inc"
};

static u16 _gDryfieldWaterTankAnimation06F48Indices[20] = {
#include "assets/dryfield_water_tank_animation_06F48_indices.inc"
};

AnimationSet gDryfieldWaterTankAnimation06F48 = {
    _gDryfieldWaterTankAnimation06F48Records,
    _gDryfieldWaterTankAnimation06F48Indices,
    { NULL, _gDryfieldWaterTankAnimation06F48Bank1, NULL, NULL, _gDryfieldWaterTankAnimation06F48Bank4, NULL, NULL, NULL },
};

SVECTOR D_dryfield_water_tank_80184530[82] = {
#include "assets/dryfield_water_tank_motion_06F70.inc"
};

SVECTOR D_dryfield_water_tank_801847C0[52] = {
#include "assets/dryfield_water_tank_motion_07200.inc"
};

static void func_dryfield_water_tank_8017E78C(Task* task);

/// Resumes room presentation after movie resources have been restored.
static inline void _dryfieldWaterTankResumeAfterMovie(Task* task)
{
    enum {
        DRYFIELD_WATER_TANK_FADE_IN_TASK_INDEX = 2,
        DRYFIELD_WATER_TANK_FADE_IN_RATE       = 8,
    };

    sndEvtRequestScriptStart(SOUND_WATER_TANK_AMBIENCE, 0, 0);
    memFillBytes(Fs_ImgBuffers, 0, sizeof(*Fs_ImgBuffers));
    SetDispMask(1);
    taskKill(task);
    taskSpawnFromTableOnDefaultList(D_dryfield_water_tank_80180764, DRYFIELD_WATER_TANK_FADE_IN_TASK_INDEX, DRYFIELD_WATER_TANK_FADE_IN_RATE, 0);
    displayResumeGameLoop();
}

/// Restarts the equipped weapon's standing clip without enabling grid collision.
static inline void _dryfieldWaterTankRestoreWeaponStance(void)
{
    enum {
        DRYFIELD_WATER_TANK_PRIMARY_CHARACTER_ID       = 1,
        DRYFIELD_WATER_TANK_PRIMARY_WEAPON_BANK_BASE   = 1,
        DRYFIELD_WATER_TANK_ALTERNATE_WEAPON_BANK_BASE = 0x22,
        DRYFIELD_WATER_TANK_WEAPON_STANCE_CLIP         = 1,
    };

    AnimationPlayRequest stance;
    s32                  weaponId;
    s32                  animationBank;

    weaponId                    = gPlayerStatus.weapon;
    animationBank               = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == DRYFIELD_WATER_TANK_PRIMARY_CHARACTER_ID) ? weaponId + DRYFIELD_WATER_TANK_PRIMARY_WEAPON_BANK_BASE : weaponId + DRYFIELD_WATER_TANK_ALTERNATE_WEAPON_BANK_BASE;
    stance.source.index         = animationBank;
    stance.animationId          = DRYFIELD_WATER_TANK_WEAPON_STANCE_CLIP;
    stance.blend                = ANIMATION_BLEND_RESET;
    stance.blendFrames          = 0;
    stance.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &stance, 0);
}

/// Queues a 320x240 subtractive fade tile at the current draw origin.
///
/// Borrows readable ramp channels and uses their low red/green/red bytes without
/// advancing them. The current draw origin, including screen shake, offsets the
/// centred tile. Requires a word-aligned frame cursor with space for a `TILE`
/// and `DR_TPAGE`, and foreground tag -16 in the active ordering table.
/// Packet storage must remain live through GPU consumption. The mode runs first
/// and remains active: subtractive blending, dithering on, displayed-area
/// drawing off and a 4-bit texture page at (0,0), unused by the untextured tile.
static inline void _dryfieldWaterTankDrawFadeOverlay(const ScreenFadeWork* fade)
{
    enum {
        DRYFIELD_WATER_TANK_FADE_WIDTH_PIXELS   = 320,
        DRYFIELD_WATER_TANK_FADE_HEIGHT_PIXELS  = 240,
        DRYFIELD_WATER_TANK_FADE_FOREGROUND_TAG = -16,
        DRYFIELD_WATER_TANK_FADE_TEXTURE_4BIT   = 0,
    };

    u8        red;
    u8        green;
    TILE*     tile;
    DR_TPAGE* drawMode;

    red            = fade->r;
    green          = fade->g;
    tile           = gGpuPrimCursor;
    gGpuPrimCursor = tile + 1;
    setTile(tile);
    setSemiTrans(tile, true);
    tile->r0 = red;
    tile->g0 = green;
    tile->b0 = red;
    tile->x0 = -DRYFIELD_WATER_TANK_FADE_WIDTH_PIXELS / 2;
    tile->y0 = -DRYFIELD_WATER_TANK_FADE_HEIGHT_PIXELS / 2;
    tile->w  = DRYFIELD_WATER_TANK_FADE_WIDTH_PIXELS;
    tile->h  = DRYFIELD_WATER_TANK_FADE_HEIGHT_PIXELS;
    addPrim(gGpuCurrentOt + DRYFIELD_WATER_TANK_FADE_FOREGROUND_TAG, tile);

    // Prepending the mode after the tile makes the GPU execute it first.
    drawMode       = gGpuPrimCursor;
    gGpuPrimCursor = drawMode + 1;
    setDrawTPage(drawMode, false, true, getTPage(DRYFIELD_WATER_TANK_FADE_TEXTURE_4BIT, GPU_BLEND_SUBTRACT, 0, 0));
    addPrim(gGpuCurrentOt + DRYFIELD_WATER_TANK_FADE_FOREGROUND_TAG, drawMode);
}

/// Darkens the screen before the water-tank movie with a rising subtractive tile.
///
/// The room supplies a rate of 8 colour units per frame in `Task::spawnArg1`;
/// only its low 16 bits are used. State 0 allocates owned `ScreenFadeWork` and
/// clears its channels, then draws immediately. State 1 draws before advancing
/// the signed 16-bit ramp and ends at red >= 256. Allocation failure also ends
/// the task; `taskKill` releases the work in either case.
///
/// Requires the frame arena to hold a `TILE` and `DR_TPAGE`, and the active
/// ordering table to provide foreground tag -16. Packets borrow the frame
/// arena until GPU drawing completes. The reverse ramp is `screenFadeInTileTask`.
static void _dryfieldWaterTankFadeOutTileTask(Task* task)
{
    enum {
        DRYFIELD_WATER_TANK_FADE_STATE_INIT  = 0,
        DRYFIELD_WATER_TANK_FADE_STATE_DRAW  = 1,
        DRYFIELD_WATER_TANK_FADE_CHANNEL_END = 256,
    };

    ScreenFadeWork* fade;
    ScreenFadeWork* allocatedFade;

    fade = task->work;
    switch (task->state) {
        case DRYFIELD_WATER_TANK_FADE_STATE_INIT:
            allocatedFade = memMalloc(sizeof(*allocatedFade), false);
            task->work    = allocatedFade;
            if (allocatedFade == NULL) {
                taskKill(task);
                break;
            }
            fade         = allocatedFade;
            fade->b      = 0;
            fade->g      = 0;
            fade->r      = 0;
            task->state += 1;
            /* fallthrough */
        case DRYFIELD_WATER_TANK_FADE_STATE_DRAW:
            _dryfieldWaterTankDrawFadeOverlay(fade);

            // Blue follows the ramp even though the packet uses red for blue.
            fade->r += (u16)task->spawnArg1.value;
            fade->g += (u16)task->spawnArg1.value;
            fade->b += (u16)task->spawnArg1.value;
            if (fade->r >= DRYFIELD_WATER_TANK_FADE_CHANNEL_END) {
                taskKill(task);
            }
            break;
    }
}

/// Plays the room's stream 100, handles a Start skip and restores game presentation.
///
/// Requires the room's movie descriptor and image-memory configuration to be
/// loaded, and a new bodyless task in state 0. Saves displaced VRAM images
/// before playback and waits for CD cancellation/completion before restoring
/// game resources. Completion restarts ambience, clears the resident image
/// workspace, starts a fade-in at eight colour units per tick and kills this task.
static void _dryfieldWaterTankMovieTask(Task* task)
{
    enum {
        DRYFIELD_WATER_TANK_MOVIE_PREPARE       = 0,
        DRYFIELD_WATER_TANK_MOVIE_ENQUEUE       = 1,
        DRYFIELD_WATER_TANK_MOVIE_WAIT_READY    = 2,
        DRYFIELD_WATER_TANK_MOVIE_PLAY          = 3,
        DRYFIELD_WATER_TANK_MOVIE_WAIT_STOP     = 4,
        DRYFIELD_WATER_TANK_MOVIE_RESTORE       = 5,
        DRYFIELD_WATER_TANK_MOVIE_STREAM_ID     = 100,
        DRYFIELD_WATER_TANK_AMBIENCE_FADE_TICKS = 60,
        DRYFIELD_WATER_TANK_SKIP_FADE_TICKS     = 30,
    };

    u8          streamArgs[4];
    GameLoc     movieKey;
    CdCmdQueue* queue;
    s16         movieSlot;

    queue = &gCdCmdQueue;
    switch (task->state) {
        case DRYFIELD_WATER_TANK_MOVIE_PREPARE:
            // Preserve displaced VRAM images while playback owns the workspace.
            SetDispMask(0);
            streamPrepareMovieWorkspace(true);
            sndEvtRequestScriptStop(SOUND_WATER_TANK_AMBIENCE, DRYFIELD_WATER_TANK_AMBIENCE_FADE_TICKS);
            task->state = task->state + 1;
            return;
        case DRYFIELD_WATER_TANK_MOVIE_ENQUEUE:
            movieKey          = gGameSession->location;
            movieKey.loc.view = DRYFIELD_WATER_TANK_MOVIE_STREAM_ID;
            movieSlot         = streamFindMovieSlot(&movieKey.loc, 0, 0);
            // The queue copies four bytes; this opcode consumes only the slot byte.
            streamArgs[0] = movieSlot;
            cdCmdEnqueue(CD_COMMAND_PLAY_STREAM, 0, streamArgs);
            task->state = task->state + 1;
            return;
        case DRYFIELD_WATER_TANK_MOVIE_WAIT_READY:
            if (queue->movieReady == 0) {
                return;
            }
            sndEvtRequestScriptStart(SOUND_WATER_TANK_MOVIE_SFX_A, 0, 0);
            sndEvtRequestScriptStart(SOUND_WATER_TANK_MOVIE_SFX_B, 0, 0);
            SetDispMask(1);
            task->state = task->state + 1;
            return;
        case DRYFIELD_WATER_TANK_MOVIE_PLAY:
            if (cdCmdIsIdle()) {
                SetDispMask(0);
                task->state = task->state + 1;
                return;
            }
            if (padIsStartPressed() == 0) {
                return;
            }
            sndEvtRequestScriptStop(SOUND_WATER_TANK_MOVIE_SFX_A, DRYFIELD_WATER_TANK_SKIP_FADE_TICKS);
            sndEvtRequestScriptStop(SOUND_WATER_TANK_MOVIE_SFX_B, DRYFIELD_WATER_TANK_SKIP_FADE_TICKS);
            SetDispMask(0);
            cdCmdRequestCancel();
            task->state = task->state + 1;
            return;
        case DRYFIELD_WATER_TANK_MOVIE_WAIT_STOP:
            // Cancellation must finish before movie storage becomes game storage.
            if (cdCmdIsIdle() == 0) {
                return;
            }
            streamResetGameRestore();
            task->state = task->state + 1;
            return;
        case DRYFIELD_WATER_TANK_MOVIE_RESTORE:
            if (streamPollGameRestore(false, true) == 0) {
                return;
            }
            _dryfieldWaterTankResumeAfterMovie(task);
            return;
    }
}

/// Carries out the pending `_DryfieldWaterTankEventWork::command`, then clears
/// it; a command that takes several frames advances `commandStep` and returns
/// early until its last one.
static void func_dryfield_water_tank_8017E78C(Task* task)
{
    _DryfieldWaterTankEventWork* work;
    _DryfieldWaterTankEventWork* cur;
    union {
        AnimationPlayRequest rec;
        ActorTransform       warp;
    } msg;
    AnimationPlayRequest  script;
    AnimationPlayRequest* rec;
    u16                   step;
    s32                   weaponId;
    s32                   idx;

    work = task->work;
    switch (work->command) {
        case DRYFIELD_WATER_TANK_EVENT_COMMAND_NONE:
        case 1:
            break;
        case DRYFIELD_WATER_TANK_EVENT_COMMAND_PLAY_PLAYER_ANIMATION:
            step = work->commandStep;
            switch (step) {
                case 0:
                    cur = task->work;
                    if (cur->player != NULL) {
                        msg.rec.source.sets          = D_dryfield_water_tank_801804EC;
                        msg.rec.animationId          = 0;
                        msg.rec.blend                = ANIMATION_BLEND_RESET;
                        msg.rec.blendFrames          = 0;
                        msg.rec.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                        TASK_MESSAGE_DISPATCH_POINTER(cur->player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg.rec, 0);
                    }
                    work->commandStep++;
                    return;
                case 1:
                    cur = task->work;
                    if (cur->player != NULL) {
                        msg.rec.source.sets          = D_dryfield_water_tank_801804EC;
                        msg.rec.animationId          = step;
                        msg.rec.blend                = step;
                        msg.rec.blendFrames          = 0xF;
                        msg.rec.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                        TASK_MESSAGE_DISPATCH_POINTER(cur->player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg.rec, 0);
                    }
                    taskMessageDispatch(work->player, ANIMATION_MESSAGE_SET_RATE, 8, 0);
                    break;
            }
            break;
        case DRYFIELD_WATER_TANK_EVENT_COMMAND_FADE_OUT:
            taskSpawnFromTable(D_dryfield_water_tank_80180764, 3, 8, 0);
            break;
        case DRYFIELD_WATER_TANK_EVENT_COMMAND_TURN_PLAYER:
            msg.warp.rot.vy = 0x800;
            TASK_MESSAGE_DISPATCH_POINTER(work->player, GAME_ACTOR_MESSAGE_TURN_TO_YAW, &msg.warp, 0);
            break;
        case DRYFIELD_WATER_TANK_EVENT_COMMAND_PLAY_MOVIE:
            idx = work->commandStep;
            switch (idx) {
                case 0:
                    displaySpawnTaskFromTable(D_dryfield_water_tank_80180764, 1, 0, 0);
                    gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
                    viewQueueCurrentCameraAndPackets();
                    work->commandStep++;
                    return;
                case 1:
                case 2:
                    work->commandStep = idx + 1;
                    return;
                case 3:
                    TASK_MESSAGE_DISPATCH_POINTER(work->player, GAME_ACTOR_MESSAGE_PLACE, &D_dryfield_water_tank_801804F4, 0);
                    // Taken before the record is filled, the address sits in
                    // $a1 and `animationId` is stored through it.
                    rec                         = &script;
                    weaponId                    = gPlayerStatus.weapon;
                    script.source.index         = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
                    rec->animationId            = 1;
                    script.blend                = ANIMATION_BLEND_RESET;
                    script.blendFrames          = 0;
                    script.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &script, 0);
                    gGameSession->viewDirty = 1;
                    break;
                default:
                    return;
            }
            break;
    }
    work->command = DRYFIELD_WATER_TANK_EVENT_COMMAND_NONE;
}

/// Cutscene task state machine. State 0 refuses to run when the cutscene flag
/// is already up or one is live, otherwise it parks the freshly zeroed
/// `_DryfieldWaterTankEventWork` block in `Task::work`, republishes this task as
/// `D_dryfield_water_tank_80188D50` so the room's script commands can reach
/// that block, and hands slot 3 the 0x3E8 message carrying the animation set of the
/// equipped weapon: `gPlayerStatus.weapon + 1` for the alternate block and
/// `gPlayerStatus.weapon + 0x22` for the base one. A failed `memMalloc` kills the task
/// outright instead of returning, so the message and the state step still run
/// on that path. States 2, 3 and 4 only step; state 1 runs the per-frame
/// driver once the session is up, or steps when it has already torn down;
/// state 5 asks to be killed.
void func_dryfield_water_tank_8017E9F8(Task* task)
{
    _DryfieldWaterTankEventWork* work;
    AnimationPlayRequest         script;
    s32                          weaponId;
    s32                          anim;

    switch (task->state) {
        case 0:
            if ((Gp_StateC08.mode != ATTACHMENT_MODE_WHEEL) && (gDisplayState.pendingMode == DISPLAY_MODE_NONE)) {
                work       = memMalloc(sizeof(*work), false);
                task->work = work;
                if (work == NULL) {
                    taskKill(task);
                } else {
                    memFillBytes(work, 0, sizeof(*work));
                    work->player                   = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                    D_dryfield_water_tank_80188D50 = task;
                }
                weaponId                    = gPlayerStatus.weapon;
                anim                        = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
                script.source.index         = anim;
                script.animationId          = 1;
                script.blend                = ANIMATION_BLEND_INTERPOLATE;
                script.blendFrames          = 0xA;
                script.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &script, 0);
                evsStartScriptWithSkip(D_dryfield_water_tank_8018050C, EVENT_SCRIPT_HUD_HIDE_RESTORE,
                                       D_dryfield_water_tank_8018068C);
                task->state = task->state + 1;
            }
            break;
        case 1:
            if (gGameSession->eventState != 0) {
                func_dryfield_water_tank_8017E78C(task);
                break;
            }
            task->state = task->state + 1;
            break;
        case 2:
        case 3:
        case 4:
            task->state = task->state + 1;
            break;
        case 5:
            taskRequestKill(task, 0);
            break;
    }
}

/// Script command of the room's cutscene: stores `arg0` as the command the
/// cutscene task carries out next (`_DryfieldWaterTankEventWork::command`)
/// and restarts its `commandStep`. The block is reached through the cutscene
/// task parked in `D_dryfield_water_tank_80188D50`.
void func_dryfield_water_tank_8017EB80(s16 arg0)
{
    _DryfieldWaterTankEventWork* work = D_dryfield_water_tank_80188D50->work;

    work->command     = arg0;
    work->commandStep = 0;
}

/// Places the player at the event's exit and restores the weapon stance on skip.
///
/// The event task and its published work, cached player task and registered
/// player task must still be live. Dispatch borrows the placement and animation
/// request synchronously. Grid collision remains disabled until script cleanup;
/// the display is made visible after the requests.
static void _dryfieldWaterTankRestorePlayerAfterSkip(void)
{
    _DryfieldWaterTankEventWork* work;

    work = D_dryfield_water_tank_80188D50->work;
    TASK_MESSAGE_DISPATCH_POINTER(work->player, GAME_ACTOR_MESSAGE_PLACE, &D_dryfield_water_tank_801804F4, 0);
    _dryfieldWaterTankRestoreWeaponStance();
    SetDispMask(1);
}

void func_dryfield_water_tank_8017EC38(u32 arg0)
{
    taskSpawnFromTable(D_dryfield_water_tank_80184DF4, arg0 & 0xFFFF, (s32)(arg0 >> 0x10), 0);
}

void dryfieldWaterTankMovePlayerFirstLegTask(Task* task)
{
    // This leg consumes only the first 52 entries of the longer position table.
    enum { DRYFIELD_WATER_TANK_PLAYER_PATH_LEG_FRAMES = 52 };

    ActorTransform placement;

    if (task->killCountdown >= DRYFIELD_WATER_TANK_PLAYER_PATH_LEG_FRAMES) {
        taskKill(task);
        return;
    }
    placement.pos.vx = D_dryfield_water_tank_80184530[task->killCountdown].vx;
    placement.pos.vy = D_dryfield_water_tank_80184530[task->killCountdown].vy;
    placement.pos.vz = D_dryfield_water_tank_80184530[task->killCountdown].vz;
    placement.rot.vx = 0;
    placement.rot.vy = 1 - ACTOR_TRANSFORM_ANGLE_HALF_TURN;
    placement.rot.vz = 0;
    task->killCountdown++;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_PLACE, &placement, 0);
}

void dryfieldWaterTankMovePlayerSecondLegTask(Task* task)
{
    enum { DRYFIELD_WATER_TANK_SECOND_LEG_FRAMES = ARRAY_SIZE(D_dryfield_water_tank_801847C0) };

    ActorTransform placement;

    if (task->killCountdown >= DRYFIELD_WATER_TANK_SECOND_LEG_FRAMES) {
        taskKill(task);
        return;
    }
    placement.pos.vx = D_dryfield_water_tank_801847C0[task->killCountdown].vx;
    placement.pos.vy = D_dryfield_water_tank_801847C0[task->killCountdown].vy;
    placement.pos.vz = D_dryfield_water_tank_801847C0[task->killCountdown].vz;
    placement.rot.vx = 0;
    placement.rot.vy = ACTOR_TRANSFORM_ANGLE_TURN / 4;
    placement.rot.vz = 0;
    task->killCountdown++;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_PLACE, &placement, 0);
}

#include "../../shared/water_tank_sway_task.inc.c"

void dryfieldWaterTankSetPreOperationSprites(u8 beforeOperation)
{
    enum {
        DRYFIELD_WATER_TANK_POST_OPERATION_VIEW_INDEX  = 2,
        DRYFIELD_WATER_TANK_POST_OPERATION_BATCH_INDEX = 3,
        DRYFIELD_WATER_TANK_PRE_OPERATION_VIEW_INDEX   = 7,
        DRYFIELD_WATER_TANK_PRE_OPERATION_BATCH_INDEX  = 1,
    };

    const GameLocationKey* location;
    SpriteView*            views;
    SpriteBatch*           batches;

    location = &gGameSession->location.loc;
    if (location->stage == GAME_STAGE_DRYFIELD) {
        views = Gp_SprtTables[location->stage - 1]->areaViews[location->area - 1];
        if (beforeOperation == 0) {
            batches                                                        = views[DRYFIELD_WATER_TANK_POST_OPERATION_VIEW_INDEX].batches;
            batches[DRYFIELD_WATER_TANK_POST_OPERATION_BATCH_INDEX].hidden = false;
            batches                                                        = views[DRYFIELD_WATER_TANK_PRE_OPERATION_VIEW_INDEX].batches;
            batches[DRYFIELD_WATER_TANK_PRE_OPERATION_BATCH_INDEX].hidden  = true;
            return;
        }
        batches                                                        = views[DRYFIELD_WATER_TANK_POST_OPERATION_VIEW_INDEX].batches;
        batches[DRYFIELD_WATER_TANK_POST_OPERATION_BATCH_INDEX].hidden = true;
        batches                                                        = views[DRYFIELD_WATER_TANK_PRE_OPERATION_VIEW_INDEX].batches;
        batches[DRYFIELD_WATER_TANK_PRE_OPERATION_BATCH_INDEX].hidden  = false;
    }
}

void dryfieldWaterTankUpdateViewEffectGateTask(Task* task)
{
    s32 mappedViewIndex;

    mappedViewIndex                  = viewGetMappedIndex();
    gRoomEffectState->roomEffectMode = D_dryfield_water_tank_801868CC[(u8)mappedViewIndex - 1];
}
