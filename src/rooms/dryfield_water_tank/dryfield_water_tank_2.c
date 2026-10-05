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

/// The two event scripts `func_800E8634` is handed.
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
void             func_dryfield_water_tank_8017E3C4(Task*);
void             func_dryfield_water_tank_8017E568(Task*);
void             func_dryfield_water_tank_8017EB80(s16);
void             func_dryfield_water_tank_8017EBA0(void);

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
    { ROOM_EVENT_MESSAGE_RESOLVE, func_dryfield_water_tank_8017D7C4 },
    { 5105, func_dryfield_water_tank_8017D7BC },
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
    { ACTOR_COMMAND_MESSAGE_APPLY, func_dryfield_water_tank_8017E174 },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_dryfield_water_tank_8017E0B4 },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_dryfield_water_tank_8017E194 }, { .value = DRYFIELD_WATER_TANK_PROP_SCENE_REQUEST_START_SLIDE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_dryfield_water_tank_8017E194 }, { .value = DRYFIELD_WATER_TANK_PROP_SCENE_REQUEST_PLAY_SOUNDS }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_dryfield_water_tank_8017E194 }, { .value = DRYFIELD_WATER_TANK_PROP_SCENE_REQUEST_SHOW_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_dryfield_water_tank_8017FEC8[8] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_dryfield_water_tank_8017E1B4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_dryfield_water_tank_8017EBA0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc D_dryfield_water_tank_80180764[4] = {
    { { { TASK_BODY_NONE, 192 } }, NULL, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_dryfield_water_tank_8017E568, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, screenFadeInTileTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_dryfield_water_tank_8017E3C4, { .value = 0 } },
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
    { 2534, -0x2EE0, 611, 0 },
    { 2534, -0x2EE0, 612, 0 },
    { 2534, -0x2EE0, 617, 0 },
    { 2534, -0x2EE0, 624, 0 },
    { 2534, -0x2EE0, 634, 0 },
    { 2534, -0x2EE0, 647, 0 },
    { 2534, -0x2EE0, 661, 0 },
    { 2534, -0x2EE0, 676, 0 },
    { 2534, -0x2EE0, 692, 0 },
    { 2534, -0x2EE0, 708, 0 },
    { 2534, -0x2EE0, 725, 0 },
    { 2534, -0x2EE0, 741, 0 },
    { 2534, -0x2EE0, 758, 0 },
    { 2534, -0x2EE0, 773, 0 },
    { 2534, -0x2EE0, 788, 0 },
    { 2534, -0x2EE0, 801, 0 },
    { 2534, -0x2EE0, 813, 0 },
    { 2534, -0x2EE0, 823, 0 },
    { 2534, -0x2EE0, 831, 0 },
    { 2534, -0x2EE0, 839, 0 },
    { 2534, -0x2EE0, 846, 0 },
    { 2534, -0x2EE0, 853, 0 },
    { 2534, -0x2EE0, 860, 0 },
    { 2534, -0x2EE0, 867, 0 },
    { 2534, -0x2EE0, 874, 0 },
    { 2534, -0x2EE0, 881, 0 },
    { 2534, -0x2EE0, 888, 0 },
    { 2534, -0x2EE0, 895, 0 },
    { 2534, -0x2EE0, 901, 0 },
    { 2534, -0x2EE0, 908, 0 },
    { 2534, -0x2EE0, 915, 0 },
    { 2534, -0x2EE0, 921, 0 },
    { 2534, -0x2EE0, 928, 0 },
    { 2534, -0x2EE0, 934, 0 },
    { 2534, -0x2EE0, 939, 0 },
    { 2534, -0x2EE0, 943, 0 },
    { 2534, -0x2EE0, 947, 0 },
    { 2534, -0x2EE0, 952, 0 },
    { 2534, -0x2EE0, 958, 0 },
    { 2534, -0x2EE0, 964, 0 },
    { 2534, -0x2EE0, 968, 0 },
    { 2534, -0x2EE0, 971, 0 },
    { 2534, -0x2EE0, 973, 0 },
    { 2534, -0x2EE0, 973, 0 },
    { 2534, -0x2EE0, 973, 0 },
    { 2534, -0x2EE0, 972, 0 },
    { 2534, -0x2EE0, 972, 0 },
    { 2534, -0x2EE0, 972, 0 },
    { 2534, -0x2EE0, 972, 0 },
    { 2534, -0x2EE0, 972, 0 },
    { 2534, -0x2EE0, 972, 0 },
    { 2534, -0x2EE0, 972, 0 },
    { 2534, -0x2EE0, 972, 0 },
    { 2534, -0x2EE0, 972, 0 },
    { 2534, -0x2EE0, 972, 0 },
    { 2534, -0x2EE0, 972, 0 },
    { 2533, -0x2EE0, 972, 0 },
    { 2533, -0x2EE0, 972, 0 },
    { 2533, -0x2EE0, 972, 0 },
    { 2533, -0x2EE0, 972, 0 },
    { 2533, -0x2EE0, 972, 0 },
    { 2533, -0x2EE0, 972, 0 },
    { 2533, -0x2EE0, 972, 0 },
    { 2533, -0x2EE0, 972, 0 },
    { 2533, -0x2EE0, 972, 0 },
    { 2533, -0x2EE0, 972, 0 },
    { 2533, -0x2EE0, 972, 0 },
    { 2533, -0x2EE0, 972, 0 },
    { 2533, -0x2EE0, 972, 0 },
    { 2532, -0x2EE0, 972, 0 },
    { 2532, -0x2EE0, 972, 0 },
    { 2532, -0x2EE0, 972, 0 },
    { 2532, -0x2EE0, 972, 0 },
    { 2532, -0x2EE0, 972, 0 },
    { 2532, -0x2EE0, 972, 0 },
    { 2532, -0x2EE0, 972, 0 },
    { 2532, -0x2EE0, 972, 0 },
    { 2532, -0x2EE0, 972, 0 },
    { 2532, -0x2EE0, 972, 0 },
    { 2532, -0x2EE0, 972, 0 },
    { 2532, -0x2EE0, 972, 0 },
    { 2532, -0x2EE0, 972, 0 },
};

SVECTOR D_dryfield_water_tank_801847C0[52] = {
    { 2532, -0x2EE0, 972, 0 },
    { 2527, -0x2EE0, 972, 0 },
    { 2523, -0x2EE0, 972, 0 },
    { 2518, -0x2EE0, 972, 0 },
    { 2506, -0x2EE0, 972, 0 },
    { 2493, -0x2EE0, 972, 0 },
    { 2481, -0x2EE0, 972, 0 },
    { 2465, -0x2EE0, 972, 0 },
    { 2450, -0x2EE0, 972, 0 },
    { 2434, -0x2EE0, 972, 0 },
    { 2418, -0x2EE0, 972, 0 },
    { 2401, -0x2EE0, 972, 0 },
    { 2385, -0x2EE0, 972, 0 },
    { 2370, -0x2EE0, 972, 0 },
    { 2356, -0x2EE0, 972, 0 },
    { 2341, -0x2EE0, 972, 0 },
    { 2331, -0x2EE0, 972, 0 },
    { 2321, -0x2EE0, 972, 0 },
    { 2311, -0x2EE0, 972, 0 },
    { 2304, -0x2EE0, 972, 0 },
    { 2297, -0x2EE0, 972, 0 },
    { 2290, -0x2EE0, 972, 0 },
    { 2284, -0x2EE0, 972, 0 },
    { 2277, -0x2EE0, 972, 0 },
    { 2270, -0x2EE0, 972, 0 },
    { 2263, -0x2EE0, 972, 0 },
    { 2256, -0x2EE0, 972, 0 },
    { 2249, -0x2EE0, 972, 0 },
    { 2242, -0x2EE0, 972, 0 },
    { 2235, -0x2EE0, 972, 0 },
    { 2229, -0x2EE0, 972, 0 },
    { 2222, -0x2EE0, 972, 0 },
    { 2215, -0x2EE0, 972, 0 },
    { 2208, -0x2EE0, 972, 0 },
    { 2204, -0x2EE0, 972, 0 },
    { 2200, -0x2EE0, 972, 0 },
    { 2196, -0x2EE0, 972, 0 },
    { 2190, -0x2EE0, 972, 0 },
    { 2184, -0x2EE0, 972, 0 },
    { 2178, -0x2EE0, 972, 0 },
    { 2175, -0x2EE0, 972, 0 },
    { 2172, -0x2EE0, 972, 0 },
    { 2169, -0x2EE0, 972, 0 },
    { 2170, -0x2EE0, 972, 0 },
    { 2170, -0x2EE0, 972, 0 },
    { 2170, -0x2EE0, 972, 0 },
    { 2170, -0x2EE0, 972, 0 },
    { 2170, -0x2EE0, 972, 0 },
    { 2170, -0x2EE0, 972, 0 },
    { 2170, -0x2EE0, 972, 0 },
    { 2170, -0x2EE0, 972, 0 },
    { 2170, -0x2EE0, 972, 0 },
};

static void func_dryfield_water_tank_8017E78C(Task* task);

/// Fade the water tank to white and tear the task down.
///
/// State 0 allocates the ramp at `Task::work` and zeroes it; a failed
/// allocation kills the task outright. State 1 runs every frame: it links a
/// semi-transparent full-screen `TILE` (`-0xA0,-0x78`, `0x140x0xF0`) plus the
/// `0xE1000240` `DR_TPAGE` into `gGpuCurrentOt[-16]`, tinting the tile `r`/`g`/`r`,
/// then steps all three channels by `Task::spawnArg1`. Once `r` saturates past
/// 0xFF the screen is fully covered, so the task kills itself. The fade-up half
/// of the same pair is `screenFadeInTileTask`.
void func_dryfield_water_tank_8017E3C4(Task* arg0)
{
    ScreenFadeWork* fade;
    ScreenFadeWork* alloc;
    u8              r;
    u8              g;
    TILE*           tile;
    DR_TPAGE*       dr;

    fade = arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = memMalloc(sizeof(*alloc), false);
            arg0->work = alloc;
            if (alloc == NULL) {
                goto kill;
            }
            fade         = alloc;
            fade->b      = 0;
            fade->g      = 0;
            fade->r      = 0;
            arg0->state += 1;
            /* fallthrough */
        case 1:
            r              = fade->r;
            g              = fade->g;
            tile           = gGpuPrimCursor;
            gGpuPrimCursor = tile + 1;
            setlen(tile, 3);
            setcode(tile, 0x62);
            tile->r0 = r;
            tile->g0 = g;
            tile->b0 = r;
            tile->x0 = -0xA0;
            tile->y0 = -0x78;
            tile->w  = 0x140;
            tile->h  = 0xF0;
            addPrim(gGpuCurrentOt - 16, tile);

            dr             = gGpuPrimCursor;
            gGpuPrimCursor = dr + 1;
            setlen(dr, 1);
            dr->code[0] = 0xE1000240;
            addPrim(gGpuCurrentOt - 16, dr);

            fade->r += (u16)arg0->spawnArg1.value;
            fade->g += (u16)arg0->spawnArg1.value;
            fade->b += (u16)arg0->spawnArg1.value;
            if (fade->r >= 0x100) {
            kill:
                taskKill(arg0);
            }
            break;
    }
}

/// Water-tank intro cutscene driver: fades out, streams the room's movie via
/// CdCmd, and on completion clears the image buffers and hands off to the
/// follow-up task.
void func_dryfield_water_tank_8017E568(Task* task)
{
    u8          slotParam[4];
    GameLoc     key;
    CdCmdQueue* queue;
    s16         slot;

    queue = &gCdCmdQueue;
    switch (task->state) {
        case 0:
            SetDispMask(0);
            Mem_AllocAuxWithImages(1);
            SndEvt_EnqueueType7(SOUND_WATER_TANK_AMBIENCE, 0x3C);
            task->state = task->state + 1;
            return;
        case 1:
            key          = gGameSession->location;
            key.loc.view = 0x64;
            slot         = Stream_FindSlot((u8*)&key, 0, 0);
            slotParam[0] = slot;
            CdCmd_Enqueue(CD_COMMAND_PLAY_STREAM, 0, slotParam);
            task->state = task->state + 1;
            return;
        case 2:
            if (queue->movieReady == 0) {
                return;
            }
            sndEvtRequestScriptStart(SOUND_WATER_TANK_MOVIE_SFX_A, 0, 0);
            sndEvtRequestScriptStart(SOUND_WATER_TANK_MOVIE_SFX_B, 0, 0);
            SetDispMask(1);
            task->state = task->state + 1;
            return;
        case 3:
            if (CdCmd_IsIdle() & 0xFFFF) {
                SetDispMask(0);
                task->state = task->state + 1;
                return;
            }
            if (Pad_CheckFlag800() == 0) {
                return;
            }
            SndEvt_EnqueueType7(SOUND_WATER_TANK_MOVIE_SFX_A, 0x1E);
            SndEvt_EnqueueType7(SOUND_WATER_TANK_MOVIE_SFX_B, 0x1E);
            SetDispMask(0);
            CdCmd_ActivatePhase1();
            task->state = task->state + 1;
            return;
        case 4:
            if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
                return;
            }
            Stream_ResetRestoreState();
            task->state = task->state + 1;
            return;
        case 5:
            if ((Stream_RestoreAfterLoad(0, 1) & 0xFFFF) == 0) {
                return;
            }
            sndEvtRequestScriptStart(SOUND_WATER_TANK_AMBIENCE, 0, 0);
            memFillBytes(Fs_ImgBuffers, 0, sizeof(*Fs_ImgBuffers));
            SetDispMask(1);
            taskKill(task);
            Task_SpawnOnDefaultList(D_dryfield_water_tank_80180764, 2, 8, 0);
            Display_ResetHeapWrapper();
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
                    Display_SpawnWithOt(D_dryfield_water_tank_80180764, 1, 0, 0);
                    gDisplayState.control.flags.flipMode = DISPLAY_FLIP_TASK_ONLY;
                    Gp_SpawnViewTasks();
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
            goto L_case0;
        case 1:
            goto L_case1;
        case 2:
            goto advance;
        case 3:
            goto advance;
        case 4:
            goto advance;
        case 5:
            goto L_case5;
    }
    return;

L_case0:
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
        func_800E8634(D_dryfield_water_tank_8018050C, 0,
                      D_dryfield_water_tank_8018068C);
        goto advance;
    }
    return;

L_case1:
    if (gGameSession->eventState == 0) {
        goto advance;
    }
    func_dryfield_water_tank_8017E78C(task);
    return;

advance:
    task->state = task->state + 1;
    return;

L_case5:
    Task_RequestKill(task, 0);
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

void func_dryfield_water_tank_8017EBA0(void)
{
    _DryfieldWaterTankEventWork* work;
    AnimationPlayRequest         rec;
    s32                          weaponId;
    s32                          anim;

    work = D_dryfield_water_tank_80188D50->work;
    TASK_MESSAGE_DISPATCH_POINTER(work->player, GAME_ACTOR_MESSAGE_PLACE, &D_dryfield_water_tank_801804F4, 0);
    weaponId                 = gPlayerStatus.weapon;
    anim                     = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
    rec.source.index         = anim;
    rec.animationId          = 1;
    rec.blend                = ANIMATION_BLEND_RESET;
    rec.blendFrames          = 0;
    rec.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &rec, 0);
    SetDispMask(1);
}

void func_dryfield_water_tank_8017EC38(u32 arg0)
{
    taskSpawnFromTable(D_dryfield_water_tank_80184DF4, arg0 & 0xFFFF, (s32)(arg0 >> 0x10), 0);
}

/// Walks the water tank one step along `D_dryfield_water_tank_80184530` per
/// frame: sends slot 3 that entry as an `ActorTransform` -- the spline position
/// with the tank's fixed half-turn about `y` -- and advances `killCountdown`.
/// At 0x34 the tank has finished its run, and the task kills itself.
void func_dryfield_water_tank_8017EC6C(Task* arg0)
{
    ActorTransform rec;

    if (arg0->killCountdown >= 0x34) {
        taskKill(arg0);
        return;
    }
    rec.pos.vx = D_dryfield_water_tank_80184530[arg0->killCountdown].vx;
    rec.pos.vy = D_dryfield_water_tank_80184530[arg0->killCountdown].vy;
    rec.pos.vz = D_dryfield_water_tank_80184530[arg0->killCountdown].vz;
    rec.rot.vx = 0;
    rec.rot.vy = -0x7FF;
    rec.rot.vz = 0;
    arg0->killCountdown++;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x3E9, &rec, 0);
}

/// The tank's second run leg, the continuation of `func_dryfield_water_tank_8017EC6C`:
/// walks it one step along `D_dryfield_water_tank_801847C0` per frame and sends
/// slot 3 that entry as an `ActorTransform`, this time with a quarter-turn about
/// `y` (0x400) instead of the first leg's half-turn. At 0x34 the tank has
/// finished its run a second time and the task kills itself.
void func_dryfield_water_tank_8017ED30(Task* arg0)
{
    ActorTransform rec;

    if (arg0->killCountdown >= 0x34) {
        taskKill(arg0);
        return;
    }
    rec.pos.vx = D_dryfield_water_tank_801847C0[arg0->killCountdown].vx;
    rec.pos.vy = D_dryfield_water_tank_801847C0[arg0->killCountdown].vy;
    rec.pos.vz = D_dryfield_water_tank_801847C0[arg0->killCountdown].vz;
    rec.rot.vx = 0;
    rec.rot.vy = 0x400;
    rec.rot.vz = 0;
    arg0->killCountdown++;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x3E9, &rec, 0);
}

#include "../../shared/water_tank_sway_task.inc.c"

/// Toggle the room's cutscene-“watched” state over two of the area's sprite
/// commands, hiding one and showing the other through their
/// `SpriteBatch::hidden`. Every use goes through one pointer variable: the compiler keeps it
/// in a global allocno, which is what pushes the two literals' constant into
/// `$v0` (see DECOMPILATION_LEARNINGS.md, "A one-constant toggle…").
void func_dryfield_water_tank_8017EFF4(s32 arg0)
{
    GameLocationKey* sess;
    SpriteView*      rec;
    SpriteBatch*     batches;

    sess = &gGameSession->location.loc;
    if (sess->stage == GAME_STAGE_DRYFIELD) {
        rec = Gp_SprtTables[sess->stage - 1]->areaViews[sess->area - 1];
        if (!(arg0 & 0xFF)) {
            batches           = rec[2].batches;
            batches[3].hidden = 0;
            batches           = rec[7].batches;
            batches[1].hidden = 1;
            return;
        }
        batches           = rec[2].batches;
        batches[3].hidden = 1;
        batches           = rec[7].batches;
        batches[1].hidden = 0;
    }
}

/// Publishes the variant index the current camera view maps to: reads the view
/// index back and stores `D_dryfield_water_tank_801868CC[view - 1]` into the
/// shared work block's `field_A`. Gameplay holds this address in its data
/// (0x80110614, pointing at the room overlay), and `dryfield_parking_lot` and
/// `dryfield_water_tower` carry the same body.
void func_dryfield_water_tank_8017F084(Task* unused)
{
    gRoomEffectState->roomEffectMode = D_dryfield_water_tank_801868CC[(viewGetMappedIndex() & 0xFF) - 1];
}
