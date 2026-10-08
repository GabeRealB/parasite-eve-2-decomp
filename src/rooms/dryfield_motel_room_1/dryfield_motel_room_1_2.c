#include "rooms/dryfield_motel_room_1.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "dryfield_motel_room_1_private.h"

#include "actors/task_tables.h"

#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_targets.h"
#include "gameplay/scene_combat.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "mapui/map_dryfield.h"

/// Commands in the motel room 1 actors' stage/area namespace.
///
/// The staged sucklers handle 1..3; the enemy sucklers handle 3 and 4
/// only while alive. Other placed actors decide whether to handle the command.
enum {
    DRYFIELD_MOTEL_ROOM_1_ACTOR_COMMAND_FIRST_STAGING  = 1, // Walk the staged sucklers; placement 1 is fully lit, placement 0 dimmed
    DRYFIELD_MOTEL_ROOM_1_ACTOR_COMMAND_SECOND_STAGING = 2, // Walk both staged sucklers with full lighting
    DRYFIELD_MOTEL_ROOM_1_ACTOR_COMMAND_START_COMBAT   = 3, // Hide the staged sucklers and let the enemy sucklers chase the player
    DRYFIELD_MOTEL_ROOM_1_ACTOR_COMMAND_WALK_IN_PLACE  = 4, // Enemy sucklers loop their walk without moving
};

/// Requests the event script makes of the event task, held in
/// `_DryfieldMotelRoom1EventWork::action`.
///
/// The script sets one between its caption cues and the task carries it out on
/// its next frame. Every request but the turn completes in that frame.
enum {
    DRYFIELD_MOTEL_ROOM_1_EVENT_ACTION_NONE            = 0,
    DRYFIELD_MOTEL_ROOM_1_EVENT_ACTION_FIRST_STAGING   = 1, // Actor command 1; staged sucklers to their first marks
    DRYFIELD_MOTEL_ROOM_1_EVENT_ACTION_SECOND_STAGING  = 2, // Actor command 2; player model shown; staged sucklers to their second marks
    DRYFIELD_MOTEL_ROOM_1_EVENT_ACTION_SOUND_5         = 3, // Character bank 0xC, sound 5
    DRYFIELD_MOTEL_ROOM_1_EVENT_ACTION_SOUND_2         = 4, // Character bank 0xC, sound 2
    DRYFIELD_MOTEL_ROOM_1_EVENT_ACTION_OPENING_SOUND_5 = 5, // The same sound as 3, requested as the script opens
    DRYFIELD_MOTEL_ROOM_1_EVENT_ACTION_TURN_PLAYER     = 6, // Turn the player in place; runs over several frames
};

/// Steps of `DRYFIELD_MOTEL_ROOM_1_EVENT_ACTION_TURN_PLAYER`, held in
/// `_DryfieldMotelRoom1EventWork::actionStep`.
enum {
    DRYFIELD_MOTEL_ROOM_1_EVENT_TURN_BEGIN    = 0, // Capture the player's position and yaw and pick the direction
    DRYFIELD_MOTEL_ROOM_1_EVENT_TURN_YAW_UP   = 1, // Yaw rising
    DRYFIELD_MOTEL_ROOM_1_EVENT_TURN_YAW_DOWN = 2, // Yaw falling
    DRYFIELD_MOTEL_ROOM_1_EVENT_TURN_SETTLE   = 3, // Turn finished; hold before the closing animation
};

/// Tuning of the player's turn. Angles are in `ActorTransform` units.
enum {
    DRYFIELD_MOTEL_ROOM_1_EVENT_TURN_FACING_YAW    = ACTOR_TRANSFORM_ANGLE_TURN / 4, // Yaw the turn stops on crossing
    DRYFIELD_MOTEL_ROOM_1_EVENT_TURN_YAW_STEP      = 0x96,                           // Yaw turned per frame
    DRYFIELD_MOTEL_ROOM_1_EVENT_TURN_SETTLE_FRAMES = 4,                              // Frames held in the settle step
};

/// Work block of the room's event task, parked at `Task::work`.
///
/// The room plays the event once, on a variant-3 visit. Its task allocates the
/// block when it starts and resolves the tasks the event moves: the player and
/// the room's four placed actors. Placements 0 and 1 are the bone sucklers
/// staged for the event; placements 2 and 3 are the bone suckler enemies set
/// down where the event ends. The pointers are borrowed, and all five tasks
/// have to outlive the event task.
///
/// The event script drives the task through `action`. The turn is the only
/// request with state of its own: it takes the shorter way round to the facing
/// yaw, re-placing the player every frame from `playerPlacement`.
typedef struct {
    Task*          playerTask;            // Player task
    Task*          stagedSucklerTasks[2]; // Staged bone sucklers, placements 0 and 1
    Task*          enemySucklerTasks[2];  // Bone suckler enemies, placements 2 and 3
    ActorTransform playerPlacement;       // Placement sent to the player: the position captured as the turn or the skip script begins, and the angles to take
    u16            action;                // Pending request (`DRYFIELD_MOTEL_ROOM_1_EVENT_ACTION_*`); cleared once carried out
    u16            actionStep;            // Step of the pending request (`DRYFIELD_MOTEL_ROOM_1_EVENT_TURN_*`); 0 whenever a request is set
    u16            settleFrames;          // Frames spent in the turn's settle step
    byte           field_32[2];           // Never accessed; role unproven
    s16            turnYaw;               // Player yaw less the facing yaw: 0..4095 as the turn begins, stepped until it leaves 0..4096
} _DryfieldMotelRoom1EventWork;
STATIC_ASSERT_SIZEOF(_DryfieldMotelRoom1EventWork, 0x38);

/// The room's event task, whose `work` holds a `_DryfieldMotelRoom1EventWork`.
extern Task* D_dryfield_motel_room_1_8018159C;

/// Where the event leaves the two bone suckler enemies: `[0]` and `[1]` go to
/// the matching `_DryfieldMotelRoom1EventWork::enemySucklerTasks` entry as
/// `ACTOR_MESSAGE_PLACE`.
extern ActorTransform D_dryfield_motel_room_1_8017E130[2];

/// The staged bone sucklers' marks, sent as `ACTOR_MESSAGE_PLACE` to the matching
/// `_DryfieldMotelRoom1EventWork::stagedSucklerTasks` entry: the first staging uses
/// this pair and the second `D_dryfield_motel_room_1_8017E100`.
extern ActorTransform D_dryfield_motel_room_1_8017E0D0[2];

extern ActorTransform D_dryfield_motel_room_1_8017E100[2];

/// Main loop of the room's cutscene task. State 0 arms it once -- it waits while
/// the attachment wheel is open (`Gp_StateC08.mode`) or `gDisplayState.pendingMode` is live, so the task
/// only steps the script. Otherwise it builds the work block, sends the slot-3
/// weapon record as message 0x3E8 and hands the cutscene's two script blocks to
/// `evsStartScriptWithSkip`. States 0 and 1 then advance the state and step the driver;
/// state 1 does that only while the session is still up, and state 2 only once
/// the session's `location.loc.view` has reached 2, which is where the task kills itself.
void func_dryfield_motel_room_1_8017DD3C(Task* arg0);

/// Install the player's weapon animation set on slot 3 (message 0x3E8: the
/// equip-slot id `gPlayerStatus.weapon` plus 1 in the alternate weapon block, plus 0x22
/// in the base one, `field_4` 9, the rest of the frame zero), then copy the
/// player matrix translation into `_DryfieldMotelRoom1EventWork::playerPlacement`, set its
/// yaw to 0x500 and place the player there with `GAME_ACTOR_MESSAGE_PLACE`. Same slot-3 record the actors'
/// `func_actor_341900_801635A4` builds.
void func_dryfield_motel_room_1_8017DFD0(void);

/// Set the event task's pending `action`, resetting the `actionStep` that goes with
/// it - the same body as `_actor444000EventRequestPlayerAction`.
void func_dryfield_motel_room_1_8017DFB0(s16 arg0);

/// Arm the player's weapon, then re-issue the room task's messages: the 0x7DA
/// poke at the slot-4 task and the `ACTOR_MESSAGE_PLACE` of both bone suckler enemies.
void func_dryfield_motel_room_1_8017DF08(void);

/// `gPlayerStatus.weapon` is the
/// equipped-weapon index the slot-3 msg 0x3E8 record is keyed on,
/// `gDisplayState.pendingMode` and `Gp_StateC08.mode` (1 while the attachment wheel is open) gate the room task's
/// setup, and `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId` picks which of the two weapon-id bases that record
/// uses.

/// The cutscene script's two blocks, handed to `evsStartScriptWithSkip` by the room
/// task's state 0.
extern EvsCommand D_dryfield_motel_room_1_8017E160[];
extern EvsCommand D_dryfield_motel_room_1_8017E340[];

extern WorldCollisionGrid     D_dryfield_motel_room_1_8017EABC[1];
extern WorldCollisionOccluder D_dryfield_motel_room_1_801811BC[1];
extern WorldCollisionTrigger  D_dryfield_motel_room_1_80180CFC[8];
extern WorldCollisionTrigger  D_dryfield_motel_room_1_80180F5C[8];
extern WorldCoordRoomLights   D_dryfield_motel_room_1_801813D8[1];

extern SpriteBatch  D_dryfield_motel_room_1_8017EC24[2];
extern SpriteBatch  D_dryfield_motel_room_1_8017EC34[2];
extern SpriteBatch  D_dryfield_motel_room_1_8017EC44[2];
extern SpriteBatch  D_dryfield_motel_room_1_8017EF9C[6];
extern SpriteBatch  D_dryfield_motel_room_1_8017F670[7];
extern SpriteBatch  D_dryfield_motel_room_1_8017FB6C[5];
extern SpriteBatch  D_dryfield_motel_room_1_8017FDD8[6];
extern SpriteBatch  D_dryfield_motel_room_1_801806B4[10];
extern SpriteSource D_dryfield_motel_room_1_8017EC54[42];
extern SpriteSource D_dryfield_motel_room_1_8017EFCC[85];
extern SpriteSource D_dryfield_motel_room_1_8017F6A8[61];
extern SpriteSource D_dryfield_motel_room_1_8017FB94[29];
extern SpriteSource D_dryfield_motel_room_1_8017FE08[111];

ActorTransform D_dryfield_motel_room_1_8017E0D0[2] = {
    { { 500, 0, 1400, 0 }, { 0, 0, 0, 0 } },
    { { 1200, 0, 2450, 0 }, { 0, 0, 0, 0 } },
};

ActorTransform D_dryfield_motel_room_1_8017E100[2] = {
    { { 500, 0, 2000, 0 }, { 0, 0, 0, 0 } },
    { { 1200, 0, 2400, 0 }, { 0, 0, 0, 0 } },
};

ActorTransform D_dryfield_motel_room_1_8017E130[2] = {
    { { 500, 0, 2800, 0 }, { 0, 0, 0, 0 } },
    { { 1000, 0, 3200, 0 }, { 0, 0, 0, 0 } },
};

EvsCommand D_dryfield_motel_room_1_8017E160[20] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 5 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_dryfield_motel_room_1_8017DFB0 }, { .value = DRYFIELD_MOTEL_ROOM_1_EVENT_ACTION_OPENING_SOUND_5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_dryfield_motel_room_1_8017DFB0 }, { .value = DRYFIELD_MOTEL_ROOM_1_EVENT_ACTION_TURN_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_dryfield_motel_room_1_8017DFB0 }, { .value = DRYFIELD_MOTEL_ROOM_1_EVENT_ACTION_SOUND_5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_dryfield_motel_room_1_8017DFB0 }, { .value = DRYFIELD_MOTEL_ROOM_1_EVENT_ACTION_SOUND_2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_dryfield_motel_room_1_8017DFB0 }, { .value = DRYFIELD_MOTEL_ROOM_1_EVENT_ACTION_FIRST_STAGING }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_dryfield_motel_room_1_8017DFB0 }, { .value = DRYFIELD_MOTEL_ROOM_1_EVENT_ACTION_SECOND_STAGING }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_dryfield_motel_room_1_8017DF08 }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_dryfield_motel_room_1_8017E340[13] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_dryfield_motel_room_1_8017DFD0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_dryfield_motel_room_1_8017DF08 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_dryfield_motel_room_1_8017DF08 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc D_dryfield_motel_room_1_8017E478 = { { { TASK_BODY_NONE, 192 } }, func_dryfield_motel_room_1_8017DD3C, { .value = 0 } };

WorldCollisionRoomResources D_dryfield_motel_room_1_8017E484[2] = {
    { D_dryfield_motel_room_1_8017EABC, D_dryfield_motel_room_1_80180CFC, D_dryfield_motel_room_1_80180F5C, D_dryfield_motel_room_1_801811BC },
    { D_dryfield_motel_room_1_8017EABC, D_dryfield_motel_room_1_80180CFC, D_dryfield_motel_room_1_80180F5C, D_dryfield_motel_room_1_801811BC },
};

u8 D_dryfield_motel_room_1_8017E4A4[12] = {
    1,
    9,
    8,
    4,
    5,
    6,
    7,
    3,
    2,
    0,
    0,
    0,
};

u8* D_dryfield_motel_room_1_8017E4B0[2] = {
    D_dryfield_motel_room_1_8017E4A4,
    D_dryfield_motel_room_1_8017E4A4,
};

ViewCount D_dryfield_motel_room_1_8017E4B8[2] = { 9, 9 };

WorldCoordRoomLighting D_dryfield_motel_room_1_8017E4BC[2] = {
    { D_dryfield_motel_room_1_801813D8, NULL },
    { D_dryfield_motel_room_1_801813D8, NULL },
};

DirectionWarpEntry D_dryfield_motel_room_1_8017E4CC[1] = {
    { { { .word = 3072 }, 4369, 0, 3388 }, { 0, 0, 0, 0 }, { { .word = 3072 }, 4369, 0, 3388 }, { 0, 0, 0, 0 }, 0x520B0002, 0x520B0001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, 487 },
};

static SVECTOR _gDryfieldMotelRoom1Collision014FCNormals[9] = {
#include "assets/dryfield_motel_room_1_collision_014FC_normals.inc"
};

static SVECTOR _gDryfieldMotelRoom1Collision014FCVerts[85] = {
#include "assets/dryfield_motel_room_1_collision_014FC_verts.inc"
};

static WorldCollisionGridFace _gDryfieldMotelRoom1Collision014FCFaces[38] = {
#include "assets/dryfield_motel_room_1_collision_014FC_faces.inc"
};

static s16 _gDryfieldMotelRoom1Collision014FCCells[120] = {
#include "assets/dryfield_motel_room_1_collision_014FC_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldMotelRoom1Collision014FCCells[i])
static s16* _gDryfieldMotelRoom1Collision014FCTable[4] = {
#include "assets/dryfield_motel_room_1_collision_014FC_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_motel_room_1_8017EABC[1] = {
    { NULL, _gDryfieldMotelRoom1Collision014FCNormals, _gDryfieldMotelRoom1Collision014FCVerts, _gDryfieldMotelRoom1Collision014FCFaces, _gDryfieldMotelRoom1Collision014FCTable, -200, -200, 2, 2, 4000, 38 },
};

ViewCamera D_dryfield_motel_room_1_8017EAE0[9] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -2500, 0x2968, -3000 } }, 257 },
    { { { { 1367, 0, -3861 }, { -708, 4026, -250 }, { 3795, 751, 1344 } }, { -200, 1600, -940 } }, 212 },
    { { { { 1557, 0, 3788 }, { 539, 4054, -221 }, { -3749, 583, 1541 } }, { -4800, 1450, -840 } }, 216 },
    { { { { 4033, 0, 714 }, { 53, 4084, -300 }, { -712, 305, 4021 } }, { -4400, 1350, -1640 } }, 269 },
    { { { { -508, 0, -4064 }, { -1596, 3766, 199 }, { 3737, 1609, -467 } }, { -200, 1850, -5690 } }, 246 },
    { { { { -459, 0, 4070 }, { 2448, 3271, 276 }, { -3250, 2464, -367 } }, { -3200, 2600, -5740 } }, 246 },
    { { { { -242, 0, 4088 }, { 396, 4076, 23 }, { -4069, 397, -240 } }, { -3361, 275, -2562 } }, 329 },
    { { { { 1271, 0, 3893 }, { 1803, 3630, -588 }, { -3450, 1897, 1126 } }, { -4561, 2558, -1173 } }, 257 },
    { { { { 1324, 0, -3876 }, { -1721, 3669, -588 }, { 3472, 1819, 1186 } }, { -239, 2334, -1269 } }, 225 },
};

SpriteBatch D_dryfield_motel_room_1_8017EC24[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_motel_room_1_8017EC34[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_motel_room_1_8017EC44[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_motel_room_1_8017EC54[42] = {
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, 64, 625, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, 48, 625, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -160, 16, 625, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -40, 32, 725, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -120, 712, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, -80, 712, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -40, 712, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, 0, 712, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, 40, 712, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -160, 80, 712, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -120, 80, 712, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -80, 80, 712, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -120, 40, 712, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -80, 40, 712, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -120, 0, 712, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -120, -40, 712, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -120, -80, 712, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -120, -120, 712, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -80, -120, 712, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -80, -80, 712, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -80, -40, 712, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -80, 0, 712, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -40, 40, 725, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -40, 80, 725, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -56, 64, 725, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -56, 24, 725, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -56, -16, 725, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -56, -56, 725, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -56, -96, 725, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -64, -96, 712, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -64, -120, 712, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -56, -120, 725, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -32, -120, 1025, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -32, -80, 1025, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -32, -40, 1025, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -32, 0, 1025, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -32, 40, 1025, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -8, 40, 1056, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -8, 0, 1056, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -8, -40, 1056, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -8, -80, 1056, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -8, -96, 1056, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_motel_room_1_8017EF9C[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 3, 0 } },
    { 3, 21, 0, 0, { 0, 0 } },
    { 24, 8, 0, 0, { 2, 0 } },
    { 32, 10, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_motel_room_1_8017EFCC[85] = {
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -120, 725, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -80, 725, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -40, 725, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, 0, 725, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -160, 40, 725, { .fields = { 72, 8 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -120, 40, 725, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -120, 0, 700, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -120, -40, 700, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -120, -80, 700, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -120, -120, 700, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -80, -120, 700, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -80, -80, 700, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -80, 0, 731, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -80, -40, 728, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -40, 0, 731, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -40, -40, 725, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -40, -80, 700, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -40, -120, 700, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 32, 0, 836, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 32, -40, 836, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 40, -80, 800, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 40, -120, 800, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -32, -120, 750, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 8, -120, 750, { .fields = { 0, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 120, -120, 537, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 120, -80, 537, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 120, -40, 537, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 120, 0, 537, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 120, 40, 537, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 120, 80, 537, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, 88, 80, 656, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, 88, 40, 656, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 88, 0, 653, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, 88, -40, 637, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, 88, -80, 631, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, 88, -120, 631, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 64, -120, 700, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 64, -80, 700, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 40 } }, 64, -40, 736, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 64, 0, 750, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 64, 40, 756, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, 72, 80, 762, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 48, 40, 837, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 48, 0, 831, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 48, -40, 831, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 48, -80, 800, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 48, -120, 800, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 40, -120, 831, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 40, -80, 831, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 40, -40, 840, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 40, 0, 846, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 40, 40, 846, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 32, 0, 866, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 32, -40, 837, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 24 } }, 32, -64, 831, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 24 } }, -80, 56, 675, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -40, 40, 687, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -40, 24, 700, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, -96, 16, 750, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -80, 24, 700, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -80, 40, 687, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -88, 40, 687, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -96, 24, 700, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -112, 56, 575, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, -120, 40, 600, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -120, 32, 625, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -112, 24, 637, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, -96, 72, 437, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, -88, 112, 437, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -160, -120, 412, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -160, -80, 412, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, -40, 412, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, 0, 412, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -160, 40, 412, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 24 } }, -144, 80, 412, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -136, -120, 412, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -136, -80, 412, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 40 } }, -136, -40, 412, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 40 } }, -136, 0, 412, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 40 } }, -136, 40, 412, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 40 } }, -136, 80, 412, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -112, 0, 437, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -112, 40, 437, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 40 } }, -112, 80, 437, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -96, 80, 437, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_motel_room_1_8017F670[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 24, 0, 0, { 0, 0 } },
    { 24, 31, 0, 0, { 3, 0 } },
    { 55, 8, 0, 0, { 2, 0 } },
    { 63, 4, 0, 0, { 4, 0 } },
    { 67, 18, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_motel_room_1_8017F6A8[61] = {
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -160, -120, 400, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -160, -80, 407, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -160, -40, 431, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -160, 0, 456, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -160, 40, 456, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -160, 80, 456, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -128, -120, 425, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -128, -80, 437, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -128, -40, 450, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -128, 0, 570, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -128, 40, 575, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -128, 80, 575, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -104, -120, 750, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -104, -80, 750, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -104, -40, 800, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -104, 0, 830, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -104, 40, 830, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -104, 80, 830, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -80, -120, 867, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -80, -80, 868, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -80, -40, 878, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -80, 0, 900, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -80, 40, 906, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, 80, 881, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -56, -120, 915, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -56, -80, 921, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -56, -40, 937, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -56, 0, 962, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 80, 40, 475, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 40, 80, 500, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 56, 80, 50, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 64, 80, 500, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 80, -120, 475, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 88, -120, 475, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 104, -120, 475, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 104, -80, 475, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 88, -80, 475, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 80, -80, 475, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 72, -80, 475, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 88, -40, 475, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 80, -40, 475, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 72, -40, 475, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 64, -40, 475, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 88, 0, 500, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 80, 0, 500, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 64, 0, 500, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 72, 0, 500, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 56, 0, 500, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 72, 40, 500, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 64, 40, 500, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 56, 40, 500, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 48, 40, 500, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 48, 80, 50, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, 80, 562, { .fields = { 32, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 56, 104, 500, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, 104, 500, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 56, 96, 500, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 88, 96, 500, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 88, 88, 525, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 56, 88, 525, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 56, 80, 562, { .fields = { 0, 248 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_motel_room_1_8017FB6C[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 28, 0, 0, { 1, 0 } },
    { 28, 25, 0, 0, { 2, 0 } },
    { 53, 8, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_motel_room_1_8017FB94[29] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -32, -104, 500, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -160, -120, 500, { .fields = { 40, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -160, -96, 500, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -136, -120, 500, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -136, -96, 500, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -112, -120, 500, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -112, -96, 500, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -88, -120, 500, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -88, -96, 500, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -64, -120, 500, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -64, -96, 500, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -32, -96, 500, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, -104, 375, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -160, -64, 375, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -120, -64, 375, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -120, -104, 375, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -80, -104, 375, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -80, -64, 375, { .fields = { 16, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -40, -104, 375, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, -40, -64, 375, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, -64, 375, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -24, -40, 375, { .fields = { 64, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -24, -16, 375, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -64, -96, 330, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -56, -56, 330, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -56, -16, 330, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -56, 16, 330, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -8, -56, 812, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -8, -24, 812, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_motel_room_1_8017FDD8[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 12, 0, 0, { 3, 0 } },
    { 12, 11, 0, 0, { 0, 0 } },
    { 23, 4, 0, 0, { 2, 0 } },
    { 27, 2, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_motel_room_1_8017FE08[111] = {
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 40, 56, 500, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -88, 64, 487, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -128, 72, 475, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 80, 112, 425, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -160, 96, 425, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -120, 96, 425, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -80, 96, 425, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -40, 96, 425, { .fields = { 72, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, 0, 96, 425, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, 40, 96, 425, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -160, 80, 462, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -120, 80, 462, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -80, 80, 462, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -40, 80, 462, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 0, 80, 462, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 40, 80, 462, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -160, 64, 475, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -120, 72, 475, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -80, 72, 475, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -40, 72, 475, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 0, 72, 475, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 40, 72, 475, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -80, 64, 487, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -40, 64, 487, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 0, 64, 487, { .fields = { 48, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 40, 64, 487, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -40, 56, 500, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 0, 56, 500, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -16, 48, 537, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -56, 56, 500, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 0, 48, 537, { .fields = { 32, 8 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -56, -16, 1075, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -8, -16, 1075, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -8, -8, 1042, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 0, -8, 1042, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -144, 48, 1000, { .fields = { 112, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -120, 48, 1000, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -96, 48, 1006, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -72, 48, 1012, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -144, 24, 1050, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -120, 24, 1050, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -96, 24, 1050, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -72, 24, 1050, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -48, 24, 1025, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -144, 0, 1050, { .fields = { 0, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -120, 0, 1050, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -96, 0, 1050, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -72, 0, 1045, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -48, 0, 1045, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -48, -8, 1045, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -48, -16, 1075, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -72, -8, 1050, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -96, -8, 1062, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -120, -8, 1062, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -24, 0, 1045, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -24, -8, 1045, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -24, -16, 1075, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -8, 0, 1037, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 0, 0, 1037, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 8, 8, 1037, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -8, 24, 1025, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 0, 24, 1025, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 8, 24, 1025, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -24, 24, 1025, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -152, 16, 887, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -120, 24, 900, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -160, 48, 875, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -120, 48, 887, { .fields = { 40, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -152, 80, 862, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -120, 80, 875, { .fields = { 8, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 136, 40, 1125, { .fields = { 8, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 96, 0, 1200, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 96, -120, 1152, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 88, -120, 1375, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 80, -40, 1311, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 80, 0, 1327, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 72, -24, 1450, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 120, -16, 1025, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, -16, 950, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 88, -80, 1292, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 80, -80, 1376, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 104, -40, 1126, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, -16, 1206, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 96, -40, 1203, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 96, -80, 1125, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 96, -64, 1099, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 120, -80, 1064, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 136, -80, 960, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 152, -80, 944, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 152, -40, 898, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 144, -40, 931, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 120, -120, 985, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, -120, 888, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 136, -40, 1030, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 120, -40, 992, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 120, 0, 1050, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 136, 0, 950, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 152, 0, 950, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 104, 16, 1050, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 104, -24, 1075, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 136, -16, 1075, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 136, 16, 1050, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, -16, 1150, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 80, 8, 1125, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 104, 8, 1125, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 80, -24, 1150, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 104, -24, 1150, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 80, -48, 1227, { .fields = { 8, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 64, -40, 1230, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 80, -24, 1267, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 48, -24, 1250, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_motel_room_1_801806B4[10] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 31, 0, 0, { 6, 0 } },
    { 31, 33, 0, 0, { 1, 0 } },
    { 64, 6, 0, 0, { 4, 0 } },
    { 70, 28, 0, 0, { 0, 0 } },
    { 98, 4, 0, 0, { 5, 0 } },
    { 102, 5, 0, 0, { 3, 0 } },
    { 107, 3, 0, 0, { 7, 0 } },
    { 110, 1, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_motel_room_1_80180704[67] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -8, 8, 837, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, 16, 800, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 128, 96, 800, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 96, 96, 800, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 80, 96, 800, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 128, 72, 800, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 128, 48, 825, { .fields = { 120, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 128, 24, 800, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 96, 72, 800, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 96, 48, 825, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 96, 24, 800, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 64, 72, 800, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 64, 48, 825, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 64, 24, 800, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 64, 16, 800, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 32, 72, 800, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 0, 72, 800, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -32, 72, 800, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 32, 48, 825, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 32, 24, 800, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 32, 8, 800, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 0, 8, 800, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 0, 24, 800, { .fields = { 24, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 0, 48, 825, { .fields = { 24, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -32, 48, 812, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -40, 56, 825, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -32, 32, 812, { .fields = { 88, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -24, 24, 825, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -16, 16, 831, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 80, 24, 1025, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 64, 0, 1025, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 64, -24, 1037, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 112, 24, 1025, { .fields = { 48, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, 0, 1025, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, -24, 1050, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 112, -24, 1050, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 112, 0, 1025, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, 24, 1025, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, 112, 587, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 136, 96, 562, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 104, 96, 575, { .fields = { 40, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 104, 72, 575, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 136, 80, 562, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -136, 40, 1046, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, -120, 963, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, -80, 963, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, -40, 963, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, 0, 963, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -160, 40, 963, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -136, -120, 1046, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -136, -80, 1046, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -136, -40, 1046, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -136, 0, 1046, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -120, -104, 1112, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -120, -80, 1112, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -120, -40, 1112, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -120, 0, 1112, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -104, 0, 1175, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -104, -32, 1175, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -160, 0, 787, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -144, 8, 825, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -160, 32, 787, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -144, 32, 825, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -160, 64, 787, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -144, 64, 825, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 32, 16 } }, -88, 104, 237, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 8 } }, -56, 112, 237, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_motel_room_1_80180C40[10] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 0, 0, 0, { 4, 0 } },
    { 0, 29, 0, 0, { 5, 0 } },
    { 29, 3, 0, 0, { 2, 0 } },
    { 32, 6, 0, 0, { 6, 0 } },
    { 38, 5, 0, 0, { 1, 0 } },
    { 43, 16, 0, 0, { 7, 0 } },
    { 59, 6, 0, 0, { 3, 0 } },
    { 65, 2, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_dryfield_motel_room_1_80180C90[9] = {
    { { .empty = D_dryfield_motel_room_1_8017EC24 }, D_dryfield_motel_room_1_8017EC24, NULL },
    { { .empty = D_dryfield_motel_room_1_8017EC34 }, D_dryfield_motel_room_1_8017EC34, NULL },
    { { .empty = D_dryfield_motel_room_1_8017EC44 }, D_dryfield_motel_room_1_8017EC44, NULL },
    { { .elements = D_dryfield_motel_room_1_8017EC54 }, D_dryfield_motel_room_1_8017EF9C, NULL },
    { { .elements = D_dryfield_motel_room_1_8017EFCC }, D_dryfield_motel_room_1_8017F670, NULL },
    { { .elements = D_dryfield_motel_room_1_8017F6A8 }, D_dryfield_motel_room_1_8017FB6C, NULL },
    { { .elements = D_dryfield_motel_room_1_8017FB94 }, D_dryfield_motel_room_1_8017FDD8, NULL },
    { { .elements = D_dryfield_motel_room_1_8017FE08 }, D_dryfield_motel_room_1_801806B4, NULL },
    { { .elements = D_dryfield_motel_room_1_80180704 }, D_dryfield_motel_room_1_80180C40, NULL },
};

WorldCollisionTrigger D_dryfield_motel_room_1_80180CFC[8] = {
    { NULL, NULL, NULL, { 2527, -1344, 2927, 0 }, { { 22, 1888, 1715, 0 }, { 22, -1888, 1715, 0 }, { -23, 1888, -1716, 0 }, { -23, -1888, -1716, 0 } }, { -4104, 0, 53, 0 }, { 0, 0, 4096, 0 }, 2547, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2368, -1232, 2880, 0 }, { { -23, 1808, -1716, 0 }, { -23, -1808, -1716, 0 }, { 22, 1808, 1715, 0 }, { 22, -1808, 1715, 0 } }, { 4108, 0, -55, 0 }, { 0, 0, 4096, 0 }, 2482, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4047, -1264, 4559, 0 }, { { -995, 1776, -5, 0 }, { -995, -1776, -5, 0 }, { 995, 1776, 5, 0 }, { 995, -1776, 5, 0 } }, { 19, 0, -4105, 0 }, { 0, 0, 4096, 0 }, 2031, 0, 2, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4095, -1248, 4463, 0 }, { { 1011, 1792, -11, 0 }, { 1011, -1792, -11, 0 }, { -1011, 1792, 11, 0 }, { -1011, -1792, 11, 0 } }, { 43, 0, 4095, 0 }, { 0, 0, 4096, 0 }, 2048, 0, 4, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3200, -1280, 5632, 0 }, { { 5, 1776, -995, 0 }, { 5, -1776, -995, 0 }, { -5, 1776, 995, 0 }, { -5, -1776, 995, 0 } }, { 4101, 0, 19, 0 }, { 0, 0, 4096, 0 }, 2031, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3296, -1344, 5824, 0 }, { { -5, 1776, 995, 0 }, { -5, -1776, 995, 0 }, { 5, 1776, -995, 0 }, { 5, -1776, -995, 0 } }, { -4105, 0, -22, 0 }, { 0, 0, 4096, 0 }, 2031, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1920, -1312, 5408, 0 }, { { -5, 1776, 995, 0 }, { -5, -1776, 995, 0 }, { 5, 1776, -995, 0 }, { 5, -1776, -995, 0 } }, { -4105, 0, -22, 0 }, { 0, 0, 4096, 0 }, 2031, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1760, -1344, 5408, 0 }, { { 5, 1776, -995, 0 }, { 5, -1776, -995, 0 }, { -5, 1776, 995, 0 }, { -5, -1776, 995, 0 } }, { 4101, 0, 19, 0 }, { 0, 0, 4096, 0 }, 2031, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_dryfield_motel_room_1_80180F5C[8] = {
    { NULL, NULL, NULL, { 4544, -50, 3488, 0 }, { { -256, 0, -480, 0 }, { 256, 0, -480, 0 }, { -256, 0, 480, 0 }, { 256, 0, 480, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 543, WORLD_COLLISION_TRIGGER_ACTION_WARP, 2, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 384, -64, 3216, 0 }, { { -224, 0, -368, 0 }, { 224, 0, -368, 0 }, { -224, 0, 368, 0 }, { 224, 0, 368, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 430, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2880, -64, 5504, 0 }, { { -160, 0, -1024, 0 }, { 160, 0, -1024, 0 }, { -160, 0, 1024, 0 }, { 160, 0, 1024, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 1031, WORLD_COLLISION_TRIGGER_ACTION_ROOM | WORLD_COLLISION_TRIGGER_AUTOMATIC, 1, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 2168, -64, 3760, 0 }, { { -568, 0, -336, 0 }, { 232, 0, -336, 0 }, { -568, 0, 336, 0 }, { 904, 0, 336, 0 } }, { 0, 4096, 0, 0 }, { 799, 0, -4017, 0 }, 964, WORLD_COLLISION_TRIGGER_ACTION_CAP, 8, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 720, -64, 3600, 0 }, { { -432, 0, -256, 0 }, { 432, 0, -256, 0 }, { -432, 0, 256, 0 }, { 432, 0, 256, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, -4096, 0 }, 501, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1728, -64, 5632, 0 }, { { -368, 0, -256, 0 }, { 368, 0, -256, 0 }, { -368, 0, 256, 0 }, { 368, 0, 256, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, -4096, 0 }, 448, WORLD_COLLISION_TRIGGER_ACTION_CAP, 6, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2689, -64, 5600, 0 }, { { -432, 0, -256, 0 }, { 432, 0, -256, 0 }, { -432, 0, 256, 0 }, { 432, 0, 256, 0 } }, { 0, 4097, 0, 0 }, { 201, 0, -4091, 0 }, 501, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4176, -64, 5600, 0 }, { { -736, 0, -256, 0 }, { 736, 0, -256, 0 }, { -736, 0, 256, 0 }, { 736, 0, 256, 0 } }, { 0, 4099, 0, 0 }, { 0, 0, -4096, 0 }, 778, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionOccluder D_dryfield_motel_room_1_801811BC[1] = {
    { NULL, NULL, { 1760, -1408, 4512, 0 }, { { -1632, -2432, 0, 0 }, { 1632, -2432, 0, 0 }, { -1632, 2432, 0, 0 }, { 1632, 2432, 0, 0 } }, { 0, 0, -4098, 0 }, 2918, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

/// Five white point lights contributing in every view of Dryfield motel room 1.
///
/// Positions and falloff radii use integer world units; RGB intensities have
/// 12 fractional bits (`ONE` is full intensity). The loaded room overlay owns
/// these writable records: coordinate updates parent and compose their
/// transforms, and lighting queries overwrite attenuation. Borrowed pointers
/// must not survive unloading the overlay.
static WorldCoordPointLight _gDryfieldMotelRoom1PointLights[] = {
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 1234, -1685, 5738 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 3300, 3300, 3300 },
        },
        .inner = 1222,
        .outer = 4450,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3463, -1645, 1027 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 4250, 4250, 4250 },
        },
        .inner = 2300,
        .outer = 4100,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 926, -1645, 83 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 3800, 3800, 3800 },
        },
        .inner = 2153,
        .outer = 4701,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4433, -1645, 1308 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 2800, 2800, 2800 },
        },
        .inner = 852,
        .outer = 3970,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4671, -1479, 4462 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 4000, 4000, 4000 },
        },
        .inner = 1487,
        .outer = 3789,
    },
};

WorldCoordRoomLights D_dryfield_motel_room_1_801813D8[1] = {
    { 0, NULL, ARRAY_SIZE(_gDryfieldMotelRoom1PointLights), _gDryfieldMotelRoom1PointLights, 0, NULL },
};

AreaResource D_dryfield_motel_room_1_801813F0[1] = {
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_motel_room_1_801813FC[3] = {
    { 18, 18, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &D_actor_401800_80155AC4 },
    { 12, 12, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, D_actor_301200_80168E98 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_motel_room_1_80181420[3] = {
    { 112, 232, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &D_actor_123200_80137234 },
    { 12, 12, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_201200_80150E98 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_motel_room_1_80181444[3] = {
    { 18, 18, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &D_actor_401800_80155AC4 },
    { 40, 40, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, D_actor_304000_8016E500 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_motel_room_1_80181468[3] = {
    { 112, 232, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &D_actor_123200_80137234 },
    { 12, 12, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_201200_80150E98 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_dryfield_motel_room_1_8018148C[5] = {
    { 112, 0, 0, -3473, 0, 993, 4096, 0, 0, 2, 0 },
    { 112, 0, 0, -3473, 0, 993, 4096, 0, 0, 2, 0 },
    { 12, 0, 0, -3473, 0, 993, 4096, 0, 0, 2, 0 },
    { 12, 0, 0, -3473, 0, 993, 4096, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant D_dryfield_motel_room_1_801814DC[13] = {
    { NULL, NULL },
    { D_map_dryfield_8017AF94, D_dryfield_motel_room_1_801813F0 },
    { D_map_dryfield_8017AFA4, D_dryfield_motel_room_1_801813FC },
    { D_map_dryfield_8017B004, D_dryfield_motel_room_1_80181420 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_dryfield_8017B054, D_dryfield_motel_room_1_80181444 },
    { NULL, NULL },
    { NULL, NULL },
    { D_dryfield_motel_room_1_8018148C, D_dryfield_motel_room_1_80181468 },
    { NULL, NULL },
    { NULL, NULL },
};

WorldCollisionFootstepSounds D_dryfield_motel_room_1_80181544 = {
    0x10000035,
    0x10000037,
    0x10000035,
};

WorldCollisionFootstepSounds D_dryfield_motel_room_1_80181550 = {
    0x10000001,
    0x10000003,
    0x10000001,
};

WorldCollisionSurfaceProperties D_dryfield_motel_room_1_8018155C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_motel_room_1_80181564[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_motel_room_1_80181544 },
};

WorldCollisionSurfaceProperties D_dryfield_motel_room_1_8018156C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_motel_room_1_80181550 },
};

WorldCollisionSurfaceProperties D_dryfield_motel_room_1_80181574[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties* D_dryfield_motel_room_1_8018157C[8] = {
    D_dryfield_motel_room_1_8018155C,
    D_dryfield_motel_room_1_80181564,
    D_dryfield_motel_room_1_8018156C,
    D_dryfield_motel_room_1_80181574,
    D_dryfield_motel_room_1_8018155C,
    D_dryfield_motel_room_1_8018155C,
    D_dryfield_motel_room_1_8018155C,
    D_dryfield_motel_room_1_8018155C,
};

Task* D_dryfield_motel_room_1_8018159C = NULL;

static void func_dryfield_motel_room_1_8017D7AC(Task* arg0);
static void func_dryfield_motel_room_1_8017DC2C(Task* arg0);

/// Broadcasts an actor command in the active stage/area namespace.
///
/// Requires the active session and a live scene-manager task. `command` is a
/// 16-bit receiver-specific selector; this room uses
/// `DRYFIELD_MOTEL_ROOM_1_ACTOR_COMMAND_*`. The scene forwards the request to
/// every placed actor with a zero second payload, and discards their results.
/// Dispatch is synchronous: receivers borrow the stack record for the call.
static inline void _dryfieldMotelRoom1BroadcastActorCommand(u16 command)
{
    ActorCommand request;

    request.context.loc.stage = gGameSession->location.loc.stage;
    request.context.loc.area  = gGameSession->location.loc.area;
    request.command           = command;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &request, ACTOR_COMMAND_MESSAGE_APPLY);
}

/// Plays `animationId` from the player's bank for the equipped weapon, off the
/// collision grid.
///
/// `blend` is an `ANIMATION_BLEND_*` choice and `blendFrames` the length of the
/// transition in frames. The request is consumed by the dispatch.
static inline void _dryfieldMotelRoom1PlayPlayerAnimation(u16 animationId, u16 blend, u16 blendFrames)
{
    AnimationPlayRequest request;
    s32                  weapon;

    // Each character has a bank per weapon slot: the primary's start at 1, the alternate's at 0x22.
    weapon                       = gPlayerStatus.weapon;
    request.source.index         = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weapon + 1 : weapon + 0x22;
    request.animationId          = animationId;
    request.blend                = blend;
    request.blendFrames          = blendFrames;
    request.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &request, 0);
}

/// The room's script driver: runs the action `func_dryfield_motel_room_1_8017DFB0`
/// left in `_DryfieldMotelRoom1EventWork::action`. Actions 1 and 2 send the 0x7DA message to the
/// slot-4 task and one of the two placement pairs as `ACTOR_MESSAGE_PLACE` (action 2
/// also sends 0x3F3 to the slot-3 task); 3, 4 and 5 play a sound. Each of these
/// runs once and clears the action. Action 6 runs over several frames with
/// `actionStep` as its step: it sends a slot-3 weapon record, turns the
/// `turnYaw` angle one way or the other each frame while re-placing the player
/// from `playerPlacement` with `GAME_ACTOR_MESSAGE_PLACE`, and ends four frames after the turn
/// completes, when it clears the action itself. Every path through
/// `func_dryfield_motel_room_1_8017DD3C` except its early return and its kill
/// ends here.
static void func_dryfield_motel_room_1_8017D7AC(Task* arg0)
{
    _DryfieldMotelRoom1EventWork* work = arg0->work;
    PlayerStatus*                 cfg;

    switch (work->action) {
        case DRYFIELD_MOTEL_ROOM_1_EVENT_ACTION_FIRST_STAGING:
            _dryfieldMotelRoom1BroadcastActorCommand(DRYFIELD_MOTEL_ROOM_1_ACTOR_COMMAND_FIRST_STAGING);
            TASK_MESSAGE_DISPATCH_POINTER(work->stagedSucklerTasks[0], ACTOR_MESSAGE_PLACE, &D_dryfield_motel_room_1_8017E0D0[0], 0);
            TASK_MESSAGE_DISPATCH_POINTER(work->stagedSucklerTasks[1], ACTOR_MESSAGE_PLACE, &D_dryfield_motel_room_1_8017E0D0[1], 0);
            break;
        case DRYFIELD_MOTEL_ROOM_1_EVENT_ACTION_SECOND_STAGING:
            _dryfieldMotelRoom1BroadcastActorCommand(DRYFIELD_MOTEL_ROOM_1_ACTOR_COMMAND_SECOND_STAGING);
            taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
            TASK_MESSAGE_DISPATCH_POINTER(work->stagedSucklerTasks[0], ACTOR_MESSAGE_PLACE, &D_dryfield_motel_room_1_8017E100[0], 0);
            TASK_MESSAGE_DISPATCH_POINTER(work->stagedSucklerTasks[1], ACTOR_MESSAGE_PLACE, &D_dryfield_motel_room_1_8017E100[1], 0);
            break;
        case DRYFIELD_MOTEL_ROOM_1_EVENT_ACTION_SOUND_5:
        case DRYFIELD_MOTEL_ROOM_1_EVENT_ACTION_OPENING_SOUND_5:
            sndEvtRequestScriptStart(SOUND_ID(SOUND_BANK_TYPE_CHARACTER, 0xC, 5), 0, 0);
            break;
        case DRYFIELD_MOTEL_ROOM_1_EVENT_ACTION_SOUND_2:
            sndEvtRequestScriptStart(SOUND_ID(SOUND_BANK_TYPE_CHARACTER, 0xC, 2), 0, 0);
            break;
        case DRYFIELD_MOTEL_ROOM_1_EVENT_ACTION_TURN_PLAYER:
            switch (work->actionStep) {
                case DRYFIELD_MOTEL_ROOM_1_EVENT_TURN_BEGIN:
                    cfg                          = &gPlayerStatus;
                    work->playerPlacement.pos.vx = cfg->coordMtx->t[0];
                    work->playerPlacement.pos.vy = cfg->coordMtx->t[1];
                    work->playerPlacement.pos.vz = cfg->coordMtx->t[2];
                    work->playerPlacement.rot.vx = 0;
                    work->playerPlacement.rot.vy = 0;
                    work->playerPlacement.rot.vz = 0;
                    // Measure the yaw from the facing yaw, so that either direction ends at a range limit.
                    work->turnYaw =
                        (((GameActor*)work->playerTask->work)->rotation.vy + (ACTOR_TRANSFORM_ANGLE_TURN - DRYFIELD_MOTEL_ROOM_1_EVENT_TURN_FACING_YAW)) % ACTOR_TRANSFORM_ANGLE_TURN;
                    if (work->turnYaw > ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
                        _dryfieldMotelRoom1PlayPlayerAnimation(5, ANIMATION_BLEND_INTERPOLATE, 5);
                        taskMessageDispatch(work->playerTask, ANIMATION_MESSAGE_SET_RATE, 0x30, 0);
                        work->actionStep += 1;
                    } else {
                        _dryfieldMotelRoom1PlayPlayerAnimation(6, ANIMATION_BLEND_INTERPOLATE, 5);
                        taskMessageDispatch(work->playerTask, ANIMATION_MESSAGE_SET_RATE, 0x30, 0);
                        work->actionStep += 2;
                    }
                    return;
                case DRYFIELD_MOTEL_ROOM_1_EVENT_TURN_YAW_UP:
                    work->turnYaw               += DRYFIELD_MOTEL_ROOM_1_EVENT_TURN_YAW_STEP;
                    work->playerPlacement.rot.vy = work->turnYaw + DRYFIELD_MOTEL_ROOM_1_EVENT_TURN_FACING_YAW;
                    if (work->turnYaw > ACTOR_TRANSFORM_ANGLE_TURN) {
                        _dryfieldMotelRoom1PlayPlayerAnimation(1, ANIMATION_BLEND_INTERPOLATE, 3);
                        work->settleFrames = 0;
                        work->actionStep   = DRYFIELD_MOTEL_ROOM_1_EVENT_TURN_SETTLE;
                        return;
                    }
                    TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, GAME_ACTOR_MESSAGE_PLACE, &work->playerPlacement, 0);
                    return;
                case DRYFIELD_MOTEL_ROOM_1_EVENT_TURN_YAW_DOWN:
                    work->turnYaw               -= DRYFIELD_MOTEL_ROOM_1_EVENT_TURN_YAW_STEP;
                    work->playerPlacement.rot.vy = work->turnYaw + DRYFIELD_MOTEL_ROOM_1_EVENT_TURN_FACING_YAW;
                    if (work->turnYaw < 0) {
                        _dryfieldMotelRoom1PlayPlayerAnimation(1, ANIMATION_BLEND_INTERPOLATE, 3);
                        work->settleFrames = 0;
                        work->actionStep   = DRYFIELD_MOTEL_ROOM_1_EVENT_TURN_SETTLE;
                        return;
                    }
                    TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, GAME_ACTOR_MESSAGE_PLACE, &work->playerPlacement, 0);
                    return;
                case DRYFIELD_MOTEL_ROOM_1_EVENT_TURN_SETTLE:
                    work->settleFrames += 1;
                    if (work->settleFrames >= DRYFIELD_MOTEL_ROOM_1_EVENT_TURN_SETTLE_FRAMES) {
                        _dryfieldMotelRoom1PlayPlayerAnimation(9, ANIMATION_BLEND_INTERPOLATE, 10);
                        work->action = DRYFIELD_MOTEL_ROOM_1_EVENT_ACTION_NONE;
                    }
                    return;
                default:
                    return;
            }
            return;
        case DRYFIELD_MOTEL_ROOM_1_EVENT_ACTION_NONE:
        default:
            break;
    }
    work->action = DRYFIELD_MOTEL_ROOM_1_EVENT_ACTION_NONE;
}

/// Room entry point: allocate the `_DryfieldMotelRoom1EventWork` the event task hangs off
/// `Task::work` (killing the task if the allocation fails), zero it, park the
/// slot-3 task in `playerTask` and the event task itself in
/// `D_dryfield_motel_room_1_8018159C`, then resolve the four placed actors into
/// `stagedSucklerTasks` and `enemySucklerTasks` by place key: the session's
/// stage and area with placement index 0 to 3.
static void func_dryfield_motel_room_1_8017DC2C(Task* arg0)
{
    _DryfieldMotelRoom1EventWork* work;
    s32                           id;

    work       = memMalloc(sizeof(*work), false);
    arg0->work = work;
    if (work == NULL) {
        taskKill(arg0);
        return;
    }
    memFillBytes(work, 0, sizeof(*work));
    work->playerTask                 = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    D_dryfield_motel_room_1_8018159C = arg0;
    id                               = gGameSession->location.loc.area | (gGameSession->location.loc.stage << ENEMY_PLACE_STAGE_SHIFT);
    work->stagedSucklerTasks[0]      = sceneFindEnemyByPlaceKey(id)->task;
    id                               = ((gGameSession->location.loc.stage << ENEMY_PLACE_STAGE_SHIFT) | (1 << ENEMY_PLACE_INDEX_SHIFT)) | gGameSession->location.loc.area;
    work->stagedSucklerTasks[1]      = sceneFindEnemyByPlaceKey(id)->task;
    id                               = ((gGameSession->location.loc.stage << ENEMY_PLACE_STAGE_SHIFT) | (2 << ENEMY_PLACE_INDEX_SHIFT)) | gGameSession->location.loc.area;
    work->enemySucklerTasks[0]       = sceneFindEnemyByPlaceKey(id)->task;
    id                               = ((gGameSession->location.loc.stage << ENEMY_PLACE_STAGE_SHIFT) | (3 << ENEMY_PLACE_INDEX_SHIFT)) | gGameSession->location.loc.area;
    work->enemySucklerTasks[1]       = sceneFindEnemyByPlaceKey(id)->task;
}
void func_dryfield_motel_room_1_8017DD3C(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            if ((Gp_StateC08.mode != ATTACHMENT_MODE_WHEEL) && (gDisplayState.pendingMode == DISPLAY_MODE_NONE)) {
                func_dryfield_motel_room_1_8017DC2C(arg0);
                _dryfieldMotelRoom1PlayPlayerAnimation(1, ANIMATION_BLEND_INTERPOLATE, 5);
                evsStartScriptWithSkip(D_dryfield_motel_room_1_8017E160, EVENT_SCRIPT_HUD_HIDE_RESTORE,
                                       D_dryfield_motel_room_1_8017E340);
                arg0->state = arg0->state + 1;
                break;
            }
            return;
        case 1:
            if (gGameSession->eventState == 0) {
                _dryfieldMotelRoom1BroadcastActorCommand(DRYFIELD_MOTEL_ROOM_1_ACTOR_COMMAND_WALK_IN_PLACE);
                arg0->state = arg0->state + 1;
                break;
            }
            break;
        case 2:
            if (gGameSession->location.loc.view == arg0->state) {
                _dryfieldMotelRoom1BroadcastActorCommand(DRYFIELD_MOTEL_ROOM_1_ACTOR_COMMAND_START_COMBAT);
                taskKill(arg0);
                return;
            }
            break;
    }
    func_dryfield_motel_room_1_8017D7AC(arg0);
}

void func_dryfield_motel_room_1_8017DF08(void)
{
    _DryfieldMotelRoom1EventWork* work = D_dryfield_motel_room_1_8018159C->work;
    ActorCommand                  msg;

    sceneEngageBattle(1);
    msg.context.loc.stage = gGameSession->location.loc.stage;
    msg.context.loc.area  = gGameSession->location.loc.area;
    msg.command           = 3;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &msg, ACTOR_COMMAND_MESSAGE_APPLY);
    TASK_MESSAGE_DISPATCH_POINTER(work->enemySucklerTasks[0], ACTOR_MESSAGE_PLACE, &D_dryfield_motel_room_1_8017E130[0], 0);
    TASK_MESSAGE_DISPATCH_POINTER(work->enemySucklerTasks[1], ACTOR_MESSAGE_PLACE, &D_dryfield_motel_room_1_8017E130[1], 0);
}

void func_dryfield_motel_room_1_8017DFB0(s16 arg0)
{
    _DryfieldMotelRoom1EventWork* work = D_dryfield_motel_room_1_8018159C->work;

    work->action     = arg0;
    work->actionStep = DRYFIELD_MOTEL_ROOM_1_EVENT_TURN_BEGIN;
}

void func_dryfield_motel_room_1_8017DFD0(void)
{
    _DryfieldMotelRoom1EventWork* work;
    AnimationPlayRequest          msg;
    PlayerStatus*                 cfg;
    s32                           weaponId;
    s32                           anim;

    work                     = D_dryfield_motel_room_1_8018159C->work;
    weaponId                 = gPlayerStatus.weapon;
    anim                     = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
    msg.source.index         = anim;
    msg.animationId          = 9;
    msg.blend                = ANIMATION_BLEND_RESET;
    msg.blendFrames          = 0;
    msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &msg, 0);
    cfg                          = &gPlayerStatus;
    work->playerPlacement.pos.vx = cfg->coordMtx->t[0];
    work->playerPlacement.pos.vy = cfg->coordMtx->t[1];
    work->playerPlacement.pos.vz = cfg->coordMtx->t[2];
    work->playerPlacement.rot.vx = 0;
    work->playerPlacement.rot.vy = 0x500;
    work->playerPlacement.rot.vz = 0;
    TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, GAME_ACTOR_MESSAGE_PLACE, &work->playerPlacement, 0);
}
void dryfieldMotelRoom1NoOpEffectTask(Task* unusedTask)
{
}
