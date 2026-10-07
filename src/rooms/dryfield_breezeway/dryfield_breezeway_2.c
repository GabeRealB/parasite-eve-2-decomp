#include "rooms/dryfield_breezeway.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "dryfield_breezeway_private.h"

#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/action_prompt.h"
#include "gameplay/actor_render.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/player_actor.h"
#include "gameplay/direction_input.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/item_menu.h"
#include "gameplay/gpu_image_upload.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"
#include "gameplay/scene_combat.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/pad_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

#include "rooms/room_common.h"
#include "../../shared/action_prompt.h"
#include "../../shared/room_events.h"
#include "../../shared/glow_draw.h"

/// Requests the first event's script makes of its task, held in
/// `_DryfieldBreezewayFirstEventWork::action`.
///
/// The script sets one between its caption cues and the task carries it out on
/// its next frame, then clears it.
enum {
    DRYFIELD_BREEZEWAY_FIRST_EVENT_ACTION_NONE             = 0,
    DRYFIELD_BREEZEWAY_FIRST_EVENT_ACTION_STAGE            = 1, // Actor command 1; first chaser to its mark; player placed and turned; view 4 saved as the live view
    DRYFIELD_BREEZEWAY_FIRST_EVENT_ACTION_PLAY_ANIMATION_9 = 2, // Player animation 9, blended over 10 frames
};

/// Work block of the room's first event task, parked at `Task::work`.
///
/// The room plays the event until `GAME_FLAG_BREEZEWAY_FIRST_EVENT_SEEN` is
/// set. Its task allocates the block when it starts and resolves the tasks the
/// event moves: the player and the room's placements 0 and 1, which are desert
/// chasers in the room's layouts 1 and 10. The pointers are borrowed, and all
/// three tasks have to outlive the event task.
///
/// The event script drives the task through `action`.
typedef struct {
    Task* playerTask;           // Player task
    Task* desertChaserTasks[2]; // Placed desert chasers, placements 0 and 1
    u16   action;               // Pending request (`DRYFIELD_BREEZEWAY_FIRST_EVENT_ACTION_*`); cleared once carried out
    s16   actionStep;           // Cleared whenever a request is set; no request of this room has steps, so nothing reads it
    byte  field_10[4];          // Never accessed; role unproven
} _DryfieldBreezewayFirstEventWork;
STATIC_ASSERT_SIZEOF(_DryfieldBreezewayFirstEventWork, 0x14);

/// Height the free end of the key-item event's line hangs at, in pixels below
/// the screen centre. Its X at rest is the centre itself.
#define DRYFIELD_BREEZEWAY_LINE_REST_Y 0x20

/// Values of `_DryfieldBreezewayKeyItemEventWork::swingDamping`.
enum {
    DRYFIELD_BREEZEWAY_LINE_SWING_DAMPING_START = 3, // Stored while the cursor leads the end, so a release starts from it
    DRYFIELD_BREEZEWAY_LINE_SWING_DAMPING_STILL = 8, // The swing has died out; the end is no longer moved sideways
};

/// Work block of the room's key-item event task, parked at `Task::work`.
///
/// The event hangs the task's model on a line from a fixed point above the
/// screen centre. The player first points at the model and confirms, which
/// opens the prompt a key item is offered through; once the room's item has
/// been offered the action cursor can lead the line's free end, and the event
/// ends when that end reaches one of the room's prop hotspots.
///
/// While the cursor is within reach of the free end the end follows it, a
/// quarter of the remaining distance per frame. Otherwise the end drops back
/// to its rest height and swings about the screen centre: each frame it is
/// pulled toward the centre and moved by `swingSpeed >> swingDamping`, and
/// every crossing of the centre raises `swingDamping` until the swing stops.
///
/// The block also owns the two matrices the task's model is lit with.
typedef struct {
    GfxMatrix lightMatrix;      // Light-direction matrix of the task's model
    GfxMatrix colorMatrix;      // Light-colour matrix of the task's model; its translation is the background colour
    s32       keyItemAccepted;  // Answer to the last key-item query (0 refused, 1 the room's item was offered)
    byte      field_44[8];      // Never accessed; role unproven
    s16       hotspotId;        // `id` of the hotspot the player confirmed on the model; never read
    s16       lineEndX;         // Free end of the line, pixels right of the screen centre
    s16       lineEndY;         // Free end of the line, pixels below the screen centre
    s16       fallSpeed;        // Pixels the released end drops per frame; grows by one a frame and clears at the rest height
    s16       swingSpeed;       // Horizontal speed of the released end, before the `swingDamping` shift
    s16       swingDamping;     // Right shift applied to `swingSpeed` (`DRYFIELD_BREEZEWAY_LINE_SWING_DAMPING_*`); one more each time the end crosses the centre
    s16       previousLineEndX; // `lineEndX` before this frame's move
    s16       previousLineEndY; // `lineEndY` before this frame's move; kept only while the cursor leads the end
    s8        promptKind;       // `promptKind` of the confirmed hotspot, forwarded when the prompt opens
} _DryfieldBreezewayKeyItemEventWork;
STATIC_ASSERT_SIZEOF(_DryfieldBreezewayKeyItemEventWork, 0x60);

/// The `TaskDesc` `func_dryfield_breezeway_8017E464` spawns the room's prompt
/// task (`func_dryfield_breezeway_8017FA80`) from, and the single-entry `TaskMessageEntry[]` it parks in `Task::msgTable`
/// so `taskMessageDispatch` routes the family's messages (the 0x13F1 "can this key
/// item be used here?" query) into it. Both sit in the room's trailing data
/// blob, the table immediately after the descriptor.
extern TaskDesc         D_dryfield_breezeway_80182DC0;
extern TaskMessageEntry D_dryfield_breezeway_80182DCC[];

/// Far edge of the line segment drawn last, which the next segment starts from.
///
/// The key-item event's line is a strip of textured quads. Each segment is
/// drawn from the previous segment's far corners to its own, and stores its
/// far corners here in turn, so consecutive segments share an edge however the
/// line bends. The corners are screen pixels; only `vx` and `vy` are used.
typedef struct {
    SVECTOR left;   // Far corner on the segment's -X side
    SVECTOR right;  // Far corner on the segment's +X side
    s16     joined; // 0 the segment opens a line and draws its own near edge; otherwise it starts from `left` and `right`
} _DryfieldBreezewayLineEdge;
STATIC_ASSERT_SIZEOF(_DryfieldBreezewayLineEdge, 0x12);

/// Placement this room hands on with message 0x7D4 from
/// `func_dryfield_breezeway_8017E2D4`, `func_dryfield_breezeway_8017E390` and
/// `func_dryfield_breezeway_8017DEC0`: world x 17000, y 0, z 3000, yaw 0xA00.
extern ActorTransform D_dryfield_breezeway_80181E28;

/// The two placements that follow it in the same three-record run, which
/// `func_dryfield_breezeway_8017DEC0` sends to slot 3 as the second and third
/// message of its state-1 sequence: `[0]` is the record message 0x3E9 places the
/// player with, and `[1]` -- the run's third record -- the one message 0x3EE
/// does. The label the decomp references is the start of this array, so the
/// third record is reached as `[1]` rather than by a symbol of its own.
extern ActorTransform D_dryfield_breezeway_80181E40[];

/// The key-item prompt's own hotspot table: the one-entry
/// `ActionPromptHotspot` run, ended by `ACTION_PROMPT_HOTSPOT_END`, that
/// `func_dryfield_breezeway_8017E65C` hit-tests at the
/// prompt's own screen position and walks for the entry the cursor landed on,
/// where the prop table below is hit-tested at the cursor itself. Its `id` is
/// the script variant the prompt confirms, which the scan parks in the event
/// work block (`_DryfieldBreezewayKeyItemEventWork::hotspotId`, with `promptKind`) before state
/// 3. `func_dryfield_breezeway_8017E464` clears its `hit` along with the other
/// table's.
extern ActionPromptHotspot D_dryfield_breezeway_80182E00[];

/// This room's prop hotspot table: an `ActionPromptHotspot` run ended by
/// `ACTION_PROMPT_HOTSPOT_END`. `actionPromptHitTest` hit-tests the action
/// cursor against it. Its entries are the room's interactive props:
/// `func_dryfield_breezeway_8017E464` clears every entry's `hit` through it
/// before the first frame -- both tables', so the key-item prompt above starts
/// clean too -- and the scan in
/// `func_dryfield_breezeway_8017E81C` walks it for the entry the cursor landed
/// on.
extern ActionPromptHotspot D_dryfield_breezeway_80182DDC[];

static void func_dryfield_breezeway_8017E464(Task* arg0);
static void func_dryfield_breezeway_8017E65C(Task* task);
static void func_dryfield_breezeway_8017E81C(Task* task);
static void func_dryfield_breezeway_8017EB8C(Task* task, s16 arg1, s16 arg2);
static void func_dryfield_breezeway_8017F1F4(s16 arg0, s16 arg1, SVECTOR* arg2, SVECTOR* arg3, _DryfieldBreezewayLineEdge* arg4);
static s16  func_dryfield_breezeway_8017FAD0(SVECTOR* target, SVECTOR* pos);
static void func_dryfield_breezeway_8017FB30(Task* task, s16 arg1, s16 arg2);
static s16  func_dryfield_breezeway_8017FBEC(s16 arg0, s16 arg1, s16 arg2, s16 arg3);
static void func_dryfield_breezeway_8017FD68(Task* task);
static void func_dryfield_breezeway_8017FD9C(Task* task);
static void func_dryfield_breezeway_8017FE08(Task* task);
static void func_dryfield_breezeway_8018034C(GfxCoord* coord, SVECTOR* data, s32 arg2, s32 arg3);
static void func_dryfield_breezeway_80181938(Task* task, u8* color);

/// State handlers of the room's key-item event task, indexed by its state
/// through `func_dryfield_breezeway_8017FC38`: set-up, prompt arming, the
/// prompt-position scan, prompt spawning, the key-item answer, the exit and
/// the cursor-hotspot scan.
static const TaskFuncTable7 D_dryfield_breezeway_8017D5E8 = {
    {
        func_dryfield_breezeway_8017E464,
        func_dryfield_breezeway_8017FD68,
        func_dryfield_breezeway_8017E65C,
        func_dryfield_breezeway_8017FD9C,
        func_dryfield_breezeway_8017FE08,
        actionPromptEventEnd,
        func_dryfield_breezeway_8017E81C,
    }
};

void func_dryfield_breezeway_8017E2D4(void);
void func_dryfield_breezeway_8017E350(void);
void func_dryfield_breezeway_8017E370(s16);

void func_dryfield_breezeway_8017E010(Task*);
void func_dryfield_breezeway_8017E114(Task*);
void func_dryfield_breezeway_8017E350(void);
void func_dryfield_breezeway_8017E390(void);

s32  func_dryfield_breezeway_8017FBC8(Task*, s32, s32, s32);
void func_dryfield_breezeway_8017FA80(Task*);
void func_dryfield_breezeway_8017FC38(Task*);

TaskDesc gRoomEventTaskDesc = { { { TASK_BODY_NONE, 32 } }, roomEventTask, { .value = 0 } };

TaskMessageEntry D_dryfield_breezeway_80181DE0[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_dryfield_breezeway_8017D940 },
    { 5105, func_dryfield_breezeway_8017D90C },
    { ROOM_MESSAGE_COMMAND, func_dryfield_breezeway_8017DA48 },
    { ROOM_MESSAGE_SOUND, func_dryfield_breezeway_8017DBA4 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_dryfield_breezeway_8017DBD8 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_dryfield_breezeway_80181E10[2] = {
    { { { TASK_BODY_NONE, 32 } }, func_dryfield_breezeway_8017DC3C, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, func_dryfield_breezeway_8017DCE4, { .value = 0 } },
};

ActorTransform D_dryfield_breezeway_80181E28 = { { 0x4268, 0, 3000, 0 }, { 0, 2560, 0, 0 } };

ActorTransform D_dryfield_breezeway_80181E40[2] = {
    { { 0x4074, 0, 1500, 0 }, { 0, 1024, 0, 0 } },
    { { 0x4074, 0, 1500, 0 }, { 0, 512, 0, 0 } },
};

EvsCommand D_dryfield_breezeway_80181E70[12] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 6 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_dryfield_breezeway_8017E370 }, { .value = DRYFIELD_BREEZEWAY_FIRST_EVENT_ACTION_STAGE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_dryfield_breezeway_8017E370 }, { .value = DRYFIELD_BREEZEWAY_FIRST_EVENT_ACTION_PLAY_ANIMATION_9 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_dryfield_breezeway_8017E2D4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_dryfield_breezeway_8017E350 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_dryfield_breezeway_80181F90[12] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_dryfield_breezeway_8017E390 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_dryfield_breezeway_8017E350 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc D_dryfield_breezeway_801820B0[2] = {
    { { { TASK_BODY_NONE, 192 } }, func_dryfield_breezeway_8017E010, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_dryfield_breezeway_8017E114, { .value = 0 } },
};

TaskDesc D_dryfield_breezeway_801820C8 = { { { TASK_BODY_NONE, 192 } }, taskKill, { .value = 0 } };

static TmdBone _gDryfieldBreezewayModel04E8CSkeleton[1] = {
#include "assets/dryfield_breezeway_model_04E8C_skeleton.inc"
};

static u32 _gDryfieldBreezewayModel04E8CPartVerts[1] = {
#include "assets/dryfield_breezeway_model_04E8C_partVerts.inc"
};

static SVECTOR _gDryfieldBreezewayModel04E8CVerts[88] = {
#include "assets/dryfield_breezeway_model_04E8C_verts.inc"
};

static SVECTOR _gDryfieldBreezewayModel04E8CNormals[18] = {
#include "assets/dryfield_breezeway_model_04E8C_normals.inc"
};

static u32 _gDryfieldBreezewayModel04E8CStream[596] = {
#include "assets/dryfield_breezeway_model_04E8C_stream.inc"
};

static TmdSource _gDryfieldBreezewayModel04E8C = {
    0,
    4324,
    0,
    1,
    _gDryfieldBreezewayModel04E8CPartVerts,
    _gDryfieldBreezewayModel04E8CVerts,
    _gDryfieldBreezewayModel04E8CNormals,
    _gDryfieldBreezewayModel04E8CSkeleton,
    _gDryfieldBreezewayModel04E8CStream,
};

TaskDesc D_dryfield_breezeway_80182DC0 = { { { TASK_BODY_NONE, 192 } }, func_dryfield_breezeway_8017FA80, { .value = 0 } };

TaskMessageEntry D_dryfield_breezeway_80182DCC[2] = {
    { 5105, func_dryfield_breezeway_8017FBC8 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

ActionPromptHotspot D_dryfield_breezeway_80182DDC[3] = {
    { 102, -80, 34, 30, 1, 0, 0 },
    { 115, -50, 20, 30, 1, 0, 0 },
    { 0, 0, 0, 0, ACTION_PROMPT_HOTSPOT_END, 0, 0 },
};

ActionPromptHotspot D_dryfield_breezeway_80182E00[2] = {
    { -16, 20, 32, 48, 1, 0, 0 },
    { 0, 0, 0, 0, ACTION_PROMPT_HOTSPOT_END, 0, 0 },
};

TaskDesc D_dryfield_breezeway_80182E18 = { { { TASK_BODY_TMD, 192 } }, func_dryfield_breezeway_8017FC38, { .model = &_gDryfieldBreezewayModel04E8C } };

u_long D_dryfield_breezeway_80182E24[64] = {
    0,
    0x30A0903,
    0x70C0D07,
    3,
    0,
    0x3070B04,
    0x7090F0C,
    5,
    0,
    0x7040A05,
    0x9060D0E,
    7,
    0,
    0xC040806,
    0xB060B0F,
    7,
    0x1000000,
    0xE060607,
    0xC08060E,
    5,
    0x1000000,
    0xE090305,
    0xC0C040A,
    3,
    0x2000000,
    0xC0B0303,
    0xB0D0605,
    2,
    0x1000000,
    0x70C0601,
    0x80D0B04,
    3,
    0,
    0x40A0903,
    0x70C0D06,
    3,
    0,
    0x3070B04,
    0x7080F0C,
    5,
    0,
    0x7040A05,
    0x9060D0E,
    7,
    0,
    0xC040806,
    0xB060B0F,
    7,
    0x1000000,
    0xE060607,
    0xC08060E,
    5,
    0x1000000,
    0xE090305,
    0xC0C040A,
    3,
    0x1000000,
    0xC0B0303,
    0xB0D0605,
    2,
    0x2000000,
    0x80C0602,
    0x80D0B04,
    3,
};

GpuImageUpload D_dryfield_breezeway_80182F24[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 896, 0, 8, 16 }, D_dryfield_breezeway_80182E24 },
    { GP_IMG_REC_END, 0, { 0, 0, 0, 0 }, NULL },
};

/// The event script and its input skip script started with `evsStartScriptWithSkip`.
/// Both live in the room's trailing data blob.
extern EvsCommand D_dryfield_breezeway_80181E70[];

extern EvsCommand D_dryfield_breezeway_80181F90[];

static void func_dryfield_breezeway_8017DEC0(Task* arg0);

/// Sends `command` to every placed actor of the room.
///
/// The command is addressed to the session's stage and area and broadcast
/// through the scene task; what each value does is up to the actor that
/// receives it.
static inline void _dryfieldBreezewayBroadcastActorCommand(u16 command)
{
    ActorCommand msg;

    msg.context.loc.stage = gGameSession->location.loc.stage;
    msg.context.loc.area  = gGameSession->location.loc.area;
    msg.command           = command;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &msg, ACTOR_COMMAND_MESSAGE_APPLY);
}

/// Plays `animationId` from the player's bank for the equipped weapon, off the
/// collision grid.
///
/// `blend` is an `ANIMATION_BLEND_*` choice and `blendFrames` the length of the
/// transition in frames. The request is consumed by the dispatch.
static inline void _dryfieldBreezewayPlayPlayerAnimation(u16 animationId, u16 blend, u16 blendFrames)
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

/// Carries out the request the first event's script left in
/// `_DryfieldBreezewayFirstEventWork::action`, then clears it.
///
/// `DRYFIELD_BREEZEWAY_FIRST_EVENT_ACTION_STAGE` broadcasts actor command 1,
/// puts the first desert chaser at `D_dryfield_breezeway_80181E28`, places the
/// player at `D_dryfield_breezeway_80181E40[0]` and turns the player to the yaw of
/// `[1]`, and saves the index of view 4 as the live view.
/// `DRYFIELD_BREEZEWAY_FIRST_EVENT_ACTION_PLAY_ANIMATION_9` plays the player's
/// animation 9. A value above the last request is cleared without the shared
/// tail; the empty `case` for no request is what gives the switch that shape.
static void func_dryfield_breezeway_8017DEC0(Task* arg0)
{
    _DryfieldBreezewayFirstEventWork* work;
    s32                               action;

    work   = arg0->work;
    action = work->action;

    switch (action) {
        default:
            work->action = DRYFIELD_BREEZEWAY_FIRST_EVENT_ACTION_NONE;
            return;
        case DRYFIELD_BREEZEWAY_FIRST_EVENT_ACTION_NONE:
            break;
        case DRYFIELD_BREEZEWAY_FIRST_EVENT_ACTION_STAGE:
            _dryfieldBreezewayBroadcastActorCommand(1);
            TASK_MESSAGE_DISPATCH_POINTER(work->desertChaserTasks[0], ACTOR_MESSAGE_PLACE, &D_dryfield_breezeway_80181E28, 0);
            TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, GAME_ACTOR_MESSAGE_PLACE, &D_dryfield_breezeway_80181E40[0], 0);
            TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, GAME_ACTOR_MESSAGE_TURN_TO_YAW, &D_dryfield_breezeway_80181E40[1], 0);
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = Gp_FindViewIndex(4);
            break;
        case DRYFIELD_BREEZEWAY_FIRST_EVENT_ACTION_PLAY_ANIMATION_9:
            _dryfieldBreezewayPlayPlayerAnimation(9, ANIMATION_BLEND_INTERPOLATE, 10);
            break;
    }
    work->action = DRYFIELD_BREEZEWAY_FIRST_EVENT_ACTION_NONE;
}

void func_dryfield_breezeway_8017E010(Task* arg0)
{
    _DryfieldBreezewayFirstEventWork* work;
    s32                               id;

    switch (arg0->state) {
        case 0:
            work       = memMalloc(sizeof(*work), false);
            arg0->work = work;
            if (work == NULL) {
                taskKill(arg0);
            } else {
                memFillBytes(work, 0, sizeof(*work));
                work->playerTask              = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                D_dryfield_breezeway_801843C0 = arg0;
                // The placed actors are found by place key: the session's stage and area with the placement index.
                id                         = gGameSession->location.loc.area | (gGameSession->location.loc.stage << ENEMY_PLACE_STAGE_SHIFT);
                work->desertChaserTasks[0] = sceneFindEnemyByPlaceKey(id)->task;
                id                         = ((gGameSession->location.loc.stage << ENEMY_PLACE_STAGE_SHIFT) | (1 << ENEMY_PLACE_INDEX_SHIFT)) | gGameSession->location.loc.area;
                work->desertChaserTasks[1] = sceneFindEnemyByPlaceKey(id)->task;
            }
            arg0->state += 1;
            return;
        case 1:
            taskKill(arg0);
            return;
    }
}

/// The long-lived half of the arming pair: `func_dryfield_breezeway_8017E010`
/// is the same state 0 with no sequencer and no cutscene behind it, and is the
/// one `dryfield_night_water_tank` spawns. This one arms the room and then
/// stays resident to run `func_dryfield_breezeway_8017DEC0` every frame.
///
/// State 0 arms the room, but only while no cutscene is running
/// (`Gp_StateC08.mode != ATTACHMENT_MODE_WHEEL`) and the area is not cleared (`gDisplayState.pendingMode == DISPLAY_MODE_NONE`) --
/// otherwise it returns having done nothing, which retires the task on the
/// next frame. It allocates the `_DryfieldBreezewayFirstEventWork` block, publishes the room task
/// in `D_dryfield_breezeway_801843C0`, plays the player's animation 1 for the
/// equipped weapon (`ANIMATION_MESSAGE_PLAY`, interpolated over 10 frames) and starts the room's
/// opening cutscene through `evsStartScriptWithSkip`, which is what raises
/// `gGameSession::eventState`. It then advances to state 1.
///
/// State 1 runs the sequencer every frame until the cutscene clears
/// `gGameSession::eventState`, at which point the task kills itself. Any other
/// state goes straight to the sequencer.
void func_dryfield_breezeway_8017E114(Task* arg0)
{
    AnimationPlayRequest              buf;
    _DryfieldBreezewayFirstEventWork* work;
    s32                               id;

    switch (arg0->state) {
        case 0:
            if (Gp_StateC08.mode == ATTACHMENT_MODE_WHEEL || gDisplayState.pendingMode != DISPLAY_MODE_NONE) {
                return;
            }
            work       = memMalloc(sizeof(*work), false);
            arg0->work = work;
            if (work == NULL) {
                taskKill(arg0);
            } else {
                memFillBytes(work, 0, sizeof(*work));
                work->playerTask              = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                D_dryfield_breezeway_801843C0 = arg0;
                // The placed actors are found by place key: the session's stage and area with the placement index.
                id                         = gGameSession->location.loc.area | (gGameSession->location.loc.stage << ENEMY_PLACE_STAGE_SHIFT);
                work->desertChaserTasks[0] = sceneFindEnemyByPlaceKey(id)->task;
                id                         = ((gGameSession->location.loc.stage << ENEMY_PLACE_STAGE_SHIFT) | (1 << ENEMY_PLACE_INDEX_SHIFT)) | gGameSession->location.loc.area;
                work->desertChaserTasks[1] = sceneFindEnemyByPlaceKey(id)->task;
            }
            id                       = gPlayerStatus.weapon;
            buf.source.index         = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? id + 1 : id + 0x22;
            buf.animationId          = 1;
            buf.blend                = ANIMATION_BLEND_INTERPOLATE;
            buf.blendFrames          = 0xA;
            buf.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &buf, 0);
            evsStartScriptWithSkip(D_dryfield_breezeway_80181E70, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_dryfield_breezeway_80181F90);
            arg0->state += 1;
            break;
        case 1:
            if (gGameSession->eventState == 0) {
                taskKill(arg0);
                return;
            }
            break;
    }
    func_dryfield_breezeway_8017DEC0(arg0);
}

void func_dryfield_breezeway_8017E2D4(void)
{
    _DryfieldBreezewayFirstEventWork* work;
    ActorCommand                      msg;

    work                  = D_dryfield_breezeway_801843C0->work;
    msg.context.loc.stage = gGameSession->location.loc.stage;
    msg.context.loc.area  = gGameSession->location.loc.area;
    msg.command           = 2;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &msg, ACTOR_COMMAND_MESSAGE_APPLY);
    TASK_MESSAGE_DISPATCH_POINTER(work->desertChaserTasks[1], ACTOR_MESSAGE_PLACE, &D_dryfield_breezeway_80181E28, 0);
}

void func_dryfield_breezeway_8017E350(void)
{
    sceneEngageBattle(1);
}

void func_dryfield_breezeway_8017E370(s16 arg0)
{
    _DryfieldBreezewayFirstEventWork* work;

    work             = D_dryfield_breezeway_801843C0->work;
    work->action     = arg0;
    work->actionStep = 0;
}

void func_dryfield_breezeway_8017E390(void)
{
    _DryfieldBreezewayFirstEventWork* work;

    _dryfieldBreezewayPlayPlayerAnimation(9, ANIMATION_BLEND_RESET, 0);
    work = D_dryfield_breezeway_801843C0->work;
    _dryfieldBreezewayBroadcastActorCommand(2);
    TASK_MESSAGE_DISPATCH_POINTER(work->desertChaserTasks[1], ACTOR_MESSAGE_PLACE, &D_dryfield_breezeway_80181E28, 0);
}

/// Brings up the room's second task family, the key-item event the prompt in
/// `func_dryfield_breezeway_8017E65C` rides on. The `_DryfieldBreezewayKeyItemEventWork` block
/// is allocated and published in `Task::work`, the family's own `TaskMessageEntry[]`
/// (`D_dryfield_breezeway_80182DCC`, the one 0x13F1 record) goes to
/// `Task::msgTable` -- which is what routes the key-item query into this room
/// at all -- and the room's own event task is spawned from
/// `D_dryfield_breezeway_80182DC0` into `Task::spawnArg2`. `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view` is
/// stamped with 6, the area-record index the view gate reads back.
///
/// The event object then draws with the room's lighting rather than the shared
/// defaults: the work block's `lightMatrix` / `colorMatrix` pair is splatted onto
/// `TmdObject::lightMtx` / `colorMtx` (the slots `_worldCoordInitPlayerLighting` otherwise
/// points at `Gp_DefaultMtx` / `Gp_DefaultMtx2`), the 0x800 translation goes
/// into the colour matrix, and the line's free end starts at its rest position
/// (0, `DRYFIELD_BREEZEWAY_LINE_REST_Y`). Both hotspot tables are walked to clear `hit`, so the
/// prompt and the prop cursor both start the room with nothing highlighted.
///
/// The three descriptor stores sit in a one-iteration `do { } while (0)`
/// because retail's source had them there, and the loop note that leaves
/// behind is load-bearing twice: its loop depth doubles those stores' ref
/// weights, which is what lifts the 6 above the state reload in `local-alloc`'s
/// quantity order, and it stops that reload being hoisted above the
/// `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view` store once it holds `$v0`. Three plain statements instead of
/// the wrapper score 98.5%; wrapping a fourth statement reweights it too and
/// does not match.
///
/// The block at the end is deliberately written against the task rather than
/// against `work` and `ext`: it re-reads both slots, which is what makes its
/// base pointers fresh values rather than the ones the middle of the function
/// already holds.
static void func_dryfield_breezeway_8017E464(Task* arg0)
{
    TmdObject*                          ext;
    GfxCoord*                           coord;
    _DryfieldBreezewayKeyItemEventWork* work;
    ActionPromptHotspot*                hs;

    ext   = arg0->extra.tmd;
    coord = ext->coords;

    work = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        taskKill(arg0);
        return;
    }

    arg0->spawnArg2.pointer = taskSpawnFromTable(&D_dryfield_breezeway_80182DC0, 0, 1, 0);
    do {
        arg0->msgTable                                             = D_dryfield_breezeway_80182DCC;
        arg0->work                                                 = work;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 6;
    } while (0);
    arg0->state          += 1;
    work->keyItemAccepted = 0;
    displayAcquireMenuHold();

    hs = D_dryfield_breezeway_80182E00;
    while (hs->id != ACTION_PROMPT_HOTSPOT_END) {
        hs->hit = 0;
        hs++;
    }

    hs = D_dryfield_breezeway_80182DDC;
    while (hs->id != ACTION_PROMPT_HOTSPOT_END) {
        hs->hit = 0;
        hs++;
    }

    ext->colorMtx = &work->colorMatrix.mat;
    ext->flags    = 0;
    ext->lightMtx = &work->lightMatrix.mat;
    coord->parent = NULL;

    gGameSession->eventState   = 1;
    gGameSession->cutsceneHold = 1;
    gGameSession->hideHud      = 1;
    work->lineEndX             = 0;
    work->lineEndY             = DRYFIELD_BREEZEWAY_LINE_REST_Y;

    {
        _DryfieldBreezewayKeyItemEventWork* eventWork = arg0->work;
        TmdObject*                          eventObj  = arg0->extra.tmd;

        gfxSetRotIdentity(&eventWork->lightMatrix.mat);
        gfxSetRotIdentity(&eventWork->colorMatrix.mat);

        eventObj->lightMtx = &eventWork->lightMatrix.mat;

        eventWork->colorMatrix.mat.m[0][0] = 0x1000;
        eventWork->colorMatrix.mat.m[0][1] = 0x1000;
        eventWork->colorMatrix.mat.m[0][2] = 0x1000;
        eventWork->colorMatrix.mat.m[1][0] = 0x1000;
        eventWork->colorMatrix.mat.m[1][1] = 0x1000;
        eventWork->colorMatrix.mat.m[1][2] = 0x1000;
        eventWork->colorMatrix.mat.m[2][0] = 0x1000;
        eventWork->colorMatrix.mat.m[2][1] = 0x1000;
        eventWork->colorMatrix.mat.m[2][2] = 0x1000;

        eventWork->lightMatrix.mat.m[0][0] = 0x1000;
        eventWork->lightMatrix.mat.m[0][1] = 0x1000;
        eventWork->lightMatrix.mat.m[0][2] = 0x1000;
        eventWork->lightMatrix.mat.m[1][0] = 0;
        eventWork->lightMatrix.mat.m[1][1] = 0x1000;
        eventWork->lightMatrix.mat.m[1][2] = 0x1000;
        eventWork->lightMatrix.mat.m[2][0] = 0x1000;
        eventWork->lightMatrix.mat.m[2][1] = 0x1000;
        eventWork->lightMatrix.mat.m[2][2] = 0;

        eventObj->colorMtx = &eventWork->colorMatrix.mat;
        worldCoordSetModelAmbientColor(eventObj, 0x800, 0x800, 0x800);
    }
}

/// Main-executable symbols with no module header yet: `gDisplayState.animFrame` is the
/// frame counter the prop's swing angle is derived from, and `RotMatrixY`
/// is the Y rotation builder `ActorsShared80139948` also reaches.
///
/// Its `angle` parameter is declared `s32` rather than the `s16` the actor
/// headers use because the calls below feed it `rsin`'s `int` result, which
/// the target passes through untruncated.

/// The two image records the key-item prompt's scan uploads the first time it
/// runs, taken from the room's trailing data blob: the confirm and cancel
/// artwork `gpuUploadImages` stages into VRAM.

/// Runs the key-item prompt's scan state: uploads this room's two prompt
/// `GpuImageUpload`s the first time it runs (`Task::killCountdown` is zero, and the
/// increment latches it so a later frame never reloads them), rebuilds the
/// event task's display object matrix as the same pure Y rotation of
/// `rsin(gDisplayState.animFrame * 16)` the prop's swing builds -- one full turn every 256
/// frames -- and re-seeds `func_dryfield_breezeway_8017EB8C` at the reset
/// position (0, 0x20) rather than at the cursor the prop's scan passes.
///
/// The idle cursor is the state the scan runs in; landing on
/// `D_dryfield_breezeway_80182E00` -- the key-item prompt's own one-entry table,
/// where `func_dryfield_breezeway_8017E81C` reaches the two-entry prop table --
/// shows the hotspot cursor and walks that table for the entry whose `hit` is
/// raised. The entry's `id` and `promptKind` go to the event work block
/// (`_DryfieldBreezewayKeyItemEventWork::hotspotId` / `promptKind`), which
/// `func_dryfield_breezeway_8017FD9C` re-spawns the prompt from, and the task
/// advances to state 3. A cancel press (`buttons.slots[1].state == ACTION_PROMPT_BUTTON_PRESSED`) ends the script
/// in state 5, and a busy cap abandons the scan with the prompt cleared.
static void func_dryfield_breezeway_8017E65C(Task* task)
{
    _DryfieldBreezewayKeyItemEventWork* work;
    ActionPromptHotspot*                hs;
    ActionPrompt*                       prompt;
    GfxCoord*                           coord;

    coord  = task->extra.tmd->coords;
    work   = task->work;
    hs     = D_dryfield_breezeway_80182E00;
    prompt = D_80114D28;

    if (task->killCountdown == 0) {
        gpuUploadImages(&D_dryfield_breezeway_80182F24[0]);
        gpuUploadImages(&D_dryfield_breezeway_80183144[0]);
        task->killCountdown = (u16)task->killCountdown + 1;
    }

    gfxSetRotIdentity(&coord->coord);
    RotMatrixY(rsin(gDisplayState.animFrame * 0x10), &coord->coord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    func_dryfield_breezeway_8017EB8C(task, 0, DRYFIELD_BREEZEWAY_LINE_REST_Y);
    gGameSession->hideHud    = 1;
    gGameSession->eventState = 1;
    if (capIsBusy() != 0) {
        prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
        prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
        return;
    }
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_AIM;
    if (actionPromptHitTest(hs, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = ACTION_PROMPT_MODE_HOTSPOT;
        if ((prompt->buttons.slots[0].state == ACTION_PROMPT_BUTTON_PRESSED) && (hs->id != ACTION_PROMPT_HOTSPOT_END)) {
            do {
                if (hs->hit != 0) {
                    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
                    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
                    work->hotspotId     = hs->id;
                    work->promptKind    = hs->promptKind;
                    task->state         = 3;
                    return;
                }
                hs++;
            } while (hs->id != ACTION_PROMPT_HOTSPOT_END);
        }
    } else {
        prompt->mode = ACTION_PROMPT_MODE_IDLE;
    }
    if (prompt->buttons.slots[1].state == ACTION_PROMPT_BUTTON_PRESSED) {
        task->state = 5;
    }
}

/// Breathes the room's hanging prop: rebuilds the display object's coordinate
/// matrix as a pure Y rotation of `rsin(gDisplayState.animFrame * 16)` -- one full turn
/// every 256 frames -- off an identity built the same word-at-a-time way
/// `func_dryfield_breezeway_8017E464` builds the event work's two matrices, then
/// re-seeds the hotspot scan `func_dryfield_breezeway_8017EB8C` at the
/// prompt's own screen position and hit-tests it against the room's table.
///
/// The idle cursor is the state the scan runs in; landing on
/// an entry shows the hotspot cursor and walks `D_dryfield_breezeway_80182DDC`
/// for the entry that was hit, which is the prop the player is looking at --
/// pressing confirm against it runs cap slot 3 and ends the script in state 5.
/// A cancel press (`buttons.slots[1].state == ACTION_PROMPT_BUTTON_PRESSED`) ends it in state 5 as well.
static void func_dryfield_breezeway_8017E81C(Task* task)
{
    ActionPrompt*                       prompt = D_80114D28;
    GfxCoord*                           coord  = task->extra.tmd->coords;
    _DryfieldBreezewayKeyItemEventWork* work   = task->work;
    ActionPromptHotspot*                hs     = D_dryfield_breezeway_80182DDC;

    prompt->mode        = ACTION_PROMPT_MODE_IDLE;
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_AIM;

    gfxSetRotIdentity(&coord->coord);
    RotMatrixY(rsin(gDisplayState.animFrame * 0x10), &coord->coord);
    func_dryfield_breezeway_8017EB8C(task, prompt->screen.xy.x, prompt->screen.xy.y);

    if (actionPromptHitTest(hs, work->lineEndX, work->lineEndY) != 0) {
        prompt->mode = ACTION_PROMPT_MODE_HOTSPOT;
        while (hs->id != ACTION_PROMPT_HOTSPOT_END) {
            if (hs->hit != 0) {
                capRunCommandWithTransition(3);
                task->state = 5;
                return;
            }
            hs++;
        }
    }

    if (prompt->buttons.slots[1].state == ACTION_PROMPT_BUTTON_PRESSED) {
        task->state = 5;
    }
}

#include "../../shared/action_prompt_outline_rect.inc.c"

/// The breezeway's cursor scan, run every frame its key-item event task is on a
/// state that watches the cursor. `arg1` / `arg2` are the position the scan
/// starts from - the prompt's own `screen` coordinates from
/// `func_dryfield_breezeway_8017E81C`, or the reset pair (0, 0x20) the other
/// states pass - and the scan answers by writing `D_80114D28::mode` (1 = over a
/// hotspot, 2 = confirmed) as well as advancing the hotspot's own animation.
static void func_dryfield_breezeway_8017EB8C(Task* task, s16 arg1, s16 arg2)
{
    s32                                 x;
    s32                                 ay;
    s32                                 ty;
    SVECTOR                             pos;
    SVECTOR                             out;
    SVECTOR                             target;
    _DryfieldBreezewayLineEdge          edge;
    _DryfieldBreezewayKeyItemEventWork* work;
    s32                                 dist;
    s32                                 cdist;
    s32                                 dx;
    s32                                 dy;
    s16                                 y;
    s16                                 sx;
    s16                                 sy;
    s32                                 scale;
    s32                                 px;
    s32                                 py;
    s32                                 r;
    s32                                 d;
    s32                                 i;
    s16                                 a;
    s16                                 angle;
    s16                                 t;

    ActionPrompt* prompt;

    x      = arg1;
    ay     = arg2;
    work   = task->work;
    prompt = D_80114D28;
    ty     = ay + 0x50;
    dist   = SquareRoot0(x * x + ty * ty);
    sx     = work->lineEndX;
    dx     = sx - prompt->screen.xy.x;
    sy     = work->lineEndY;
    dy     = sy - prompt->screen.xy.y;
    cdist  = SquareRoot0(dx * dx + dy * dy);
    if (dist >= 0x70 || ay < -0x4F || cdist > 0x20) {
        // Nothing leads the free end: it drops to its rest height and swings about the centre.
        prompt->mode    = ACTION_PROMPT_MODE_IDLE;
        work->lineEndY += work->fallSpeed;
        dist            = 0x70;
        if (work->lineEndY >= DRYFIELD_BREEZEWAY_LINE_REST_Y) {
            work->lineEndY  = DRYFIELD_BREEZEWAY_LINE_REST_Y;
            work->fallSpeed = 0;
        } else {
            work->fallSpeed++;
        }
        if (work->swingDamping < DRYFIELD_BREEZEWAY_LINE_SWING_DAMPING_STILL) {
            work->previousLineEndX = work->lineEndX;
            // The casts keep each two-step sum in the order written; without them it is reassociated.
            if (work->lineEndX > 0) {
                work->swingSpeed = (s16)(work->swingSpeed - 2) - work->swingDamping;
            }
            if (work->lineEndX < 0) {
                work->swingSpeed = work->swingDamping + (s16)(work->swingSpeed + 2);
            }
            work->lineEndX += work->swingSpeed >> work->swingDamping;
            if ((work->lineEndX > 0 && work->previousLineEndX <= 0) || (work->lineEndX < 0 && work->previousLineEndX >= 0)) {
                work->swingDamping++;
            }
        }
    } else {
        // The cursor leads the free end, which closes a quarter of the gap each frame.
        prompt->mode           = ACTION_PROMPT_MODE_HOTSPOT;
        work->swingDamping     = DRYFIELD_BREEZEWAY_LINE_SWING_DAMPING_START;
        work->swingSpeed       = 0;
        work->fallSpeed        = 0;
        work->previousLineEndX = work->lineEndX;
        work->previousLineEndY = work->lineEndY;
        work->lineEndX        += (prompt->screen.xy.x - work->lineEndX) >> 2;
        work->lineEndY        += (prompt->screen.xy.y - work->lineEndY) >> 2;
        if (work->lineEndX != work->previousLineEndX || work->lineEndY != work->previousLineEndY) {
            sndEvtRequestScriptStart(SOUND_BREEZEWAY_CURSOR_MOVE, 0, 0);
        }
    }

    y     = work->lineEndY;
    x     = work->lineEndX;
    ty    = y + 0x50;
    scale = 0x800 - (ty << 12) / 224;
    scale = 0xE00 - scale;
    sx    = work->lineEndX;
    sy    = work->lineEndY;
    r     = 0x70 - dist;
    px    = (x * scale / 8) >> 9;
    py    = ((ty * scale / 8) >> 9) + ((r * scale / 8) >> 9);
    py   -= 0x50;

    target.vx = 0;
    target.vy = -0x50;
    target.vz = 0;
    pos.vx    = px;
    pos.vy    = py;
    pos.vz    = 0;
    a         = func_dryfield_breezeway_8017FBEC(0, -0x50, px, py);
    angle     = -((func_dryfield_breezeway_8017FBEC(px, py, x, y) + a) / 2) + 0x800;
    for (i = 0; i < 30; i++) {
        if (i == 0) {
            edge.joined = 0;
        } else {
            edge.joined = 1;
        }
        func_dryfield_breezeway_8017F1F4(angle, 4, &pos, &out, &edge);
        if (func_dryfield_breezeway_8017FAD0(&target, &out) != 0) {
            break;
        }
        t   = angle + func_dryfield_breezeway_8017FBEC(out.vx, out.vy, 0, -0x50);
        d   = (t << 20) >> 20;
        pos = out;
        if (d > 0x200) {
            angle -= 0x200;
        } else if (d > 0x100) {
            angle -= 0x100;
        } else if (d > 0x80) {
            angle -= 0x80;
        } else if (d < -0x200) {
            angle += 0x200;
        } else if (d < -0x100) {
            angle += 0x100;
        } else if (d < -0x80) {
            angle += 0x80;
        } else {
            angle = -func_dryfield_breezeway_8017FBEC(out.vx, out.vy, 0, -0x50);
        }
    }

    target.vx = sx;
    target.vy = sy;
    target.vz = 0;
    pos.vx    = px;
    pos.vy    = py;
    pos.vz    = 0;
    a         = func_dryfield_breezeway_8017FBEC(0, -0x50, px, py);
    angle     = -((func_dryfield_breezeway_8017FBEC(px, py, sx, sy) + a) / 2);
    for (i = 0; i < 30; i++) {
        func_dryfield_breezeway_8017F1F4(angle, 4, &pos, &out, &edge);
        if (func_dryfield_breezeway_8017FAD0(&target, &out) != 0) {
            break;
        }
        t   = angle + func_dryfield_breezeway_8017FBEC(out.vx, out.vy, sx, sy);
        d   = (t << 20) >> 20;
        pos = out;
        if (d > 0x200) {
            angle -= 0x200;
        } else if (d > 0x100) {
            angle -= 0x100;
        } else if (d > 0x80) {
            angle -= 0x80;
        } else if (d < -0x200) {
            angle += 0x200;
        } else if (d < -0x100) {
            angle += 0x100;
        } else if (d < -0x80) {
            angle += 0x80;
        } else {
            angle = -func_dryfield_breezeway_8017FBEC(out.vx, out.vy, sx, sy);
        }
    }
    func_dryfield_breezeway_8017FB30(task, out.vx, out.vy);
}

/// Draws one segment of the breezeway's prompt beam: a raw-textured quad of
/// two `arg0`-rotated edges, eight halfwords wide, whose far edge is `arg1`
/// down the rotated frame from its near one. All five probe points go through
/// `RotTransSV` (so `arg0` has to be a real rotation: the identity matrix the
/// two `Set` calls start from is splatted word-wise and then handed to
/// `RotMatrixZ`), and the quad is carved from `gGpuPrimCursor` and linked into
/// `gGpuCurrentOt[0x64]` with the room's tpage 0x8E / clut 0x4000 texture.
///
/// `arg2` is the scan's own position, added to every projected point; the
/// rotated probe at (0, `arg1`, 0) -- the far edge's centre -- lands in `arg3`
/// as the segment's tip, which is the position the caller advances its cursor
/// to. `arg4` carries the near edge: `joined` 0 draws it from the `arg2` origin
/// (the first segment of a beam), anything else from the two corners stored in
/// `arg4`, which the tail of every call overwrites with the far edge -- so a
/// beam that keeps being redrawn starts where the previous segment ended.
///
/// Nothing is written to `arg4`'s corners on a `joined` 0 call beyond that tail,
/// which is what makes the first segment of a beam run from the origin.
static void func_dryfield_breezeway_8017F1F4(s16 arg0, s16 arg1, SVECTOR* arg2, SVECTOR* arg3, _DryfieldBreezewayLineEdge* arg4)
{
    SVECTOR   probe;
    SVECTOR   tip;
    SVECTOR   near0;
    SVECTOR   near1;
    SVECTOR   far0;
    SVECTOR   far1;
    SVECTOR   corner0;
    SVECTOR   corner1;
    SVECTOR   corner2;
    SVECTOR   corner3;
    GfxMatrix matw;
    long      flag;
    POLY_FT4* p;

    gfxSetRotIdentity(&matw.mat);
    matw.mat.t[0]             = 0;
    matw.mat.t[1]             = 0;
    matw.mat.t[2]             = 0;
    RotMatrixZ(arg0, &matw.mat);
    SetRotMatrix(&matw.mat);
    SetTransMatrix(&matw.mat);

    probe.vx = 0;
    probe.vy = arg1;
    probe.vz = 0;
    RotTransSV(&probe, &tip, &flag);
    arg3->vx = arg2->vx + tip.vx;
    arg3->vy = arg2->vy + tip.vy;
    arg3->vz = arg2->vz + tip.vz;

    if (arg4->joined == 0) {
        near0.vx = -4;
        near0.vy = 0;
        near0.vz = 0;
        RotTransSV(&near0, &corner0, &flag);
        near1.vx = 4;
        near1.vy = 0;
        near1.vz = 0;
        RotTransSV(&near1, &corner1, &flag);
    }

    far0.vx = -4;
    far0.vy = arg1;
    far0.vz = 0;
    RotTransSV(&far0, &corner2, &flag);
    far1.vx = 4;
    far1.vy = arg1;
    far1.vz = 0;
    RotTransSV(&far1, &corner3, &flag);

    p              = gGpuPrimCursor;
    gGpuPrimCursor = p + 1;
    setPolyFT4(p);
    p->tpage = 0x8E;
    p->clut  = 0x4000;

    if (arg4->joined == 0) {
        p->x0 = corner0.vx + arg2->vx;
        p->y0 = corner0.vy + arg2->vy;
        p->x1 = corner1.vx + arg2->vx;
        p->y1 = corner1.vy + arg2->vy;
    } else {
        p->x0 = arg4->left.vx;
        p->y0 = arg4->left.vy;
        p->x1 = arg4->right.vx;
        p->y1 = arg4->right.vy;
    }
    p->x2 = corner2.vx + arg2->vx;
    p->y2 = corner2.vy + arg2->vy;
    p->x3 = corner3.vx + arg2->vx;
    p->y3 = corner3.vy + arg2->vy;

    p->u0 = 0;
    p->v0 = 0;
    p->u1 = 0x10;
    p->v1 = 0;
    p->u2 = 0;
    p->v2 = 4;
    p->u3 = 0x10;
    p->v3 = 4;

    setShadeTex(p, 1);
    addPrim(&gGpuCurrentOt[0x64], p);

    arg4->left.vx  = corner2.vx + arg2->vx;
    arg4->left.vy  = corner2.vy + arg2->vy;
    arg4->right.vx = corner3.vx + arg2->vx;
    arg4->right.vy = corner3.vy + arg2->vy;
}

#include "../../shared/action_prompt_move_cursors.inc.c"

#include "../../shared/action_prompt_draw_cursor.inc.c"

/// The room's prompt task, run from `D_dryfield_breezeway_80182DC0`: state 0
/// resets both action-prompt slots (`actionPromptReset`), state
/// 1 drives the cursor every frame after that
/// (`actionPromptMoveCursors`). The handler pair is built on the stack
/// rather than read from rodata.
void func_dryfield_breezeway_8017FA80(Task* task)
{
    TaskFunc states[2] = { actionPromptReset, actionPromptMoveCursors };

    states[task->state](task);
}

/// 1 when `pos` is closer than 9 units to `target`: a real distance, since the
/// sum of the two squared component differences is square-rooted before the
/// comparison. Only `vx` and `vy` take part. `func_dryfield_breezeway_8017EB8C`
/// asks this of each point it generates while it looks for somewhere to put the
/// hotspot prompt, and leaves its loop on the first point this accepts, so the
/// answer marks the candidate that has converged onto the target.
static s16 func_dryfield_breezeway_8017FAD0(SVECTOR* target, SVECTOR* pos)
{
    s16 dx = pos->vx - target->vx;
    s16 dy = pos->vy - target->vy;

    return SquareRoot0((dx * dx) + (dy * dy)) < 9;
}

/// Parks the room task's display object on the hotspot cursor: the position the
/// scan `func_dryfield_breezeway_8017EB8C` advanced to is carried into the
/// object's coordinate scaled by the depth it is placed at (`0x5DC` over 680),
/// and `composeStamp` is cleared so the next coord-tree update rebuilds the world matrix
/// from the new translation. The scan calls this once, as it leaves its loop.
static void func_dryfield_breezeway_8017FB30(Task* task, s16 arg1, s16 arg2)
{
    GfxCoord* coord = task->extra.tmd->coords;

    coord->coord.t[2]   = 0x5DC;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    coord->coord.t[0]   = (arg1 * 0x5DC) / 680;
    coord->coord.t[1]   = (arg2 * 0x5DC) / 680;
}

/// `TaskMessageEntry` handler for message 0x13F1, the "can this key item be used
/// here?" query `Gp_UseKeyItemRow` sends to slot 7. `item` is the key item the
/// player highlighted; 0x11B is the only one the breezeway accepts, and the
/// answer is latched in the work block's `keyItemAccepted` for
/// `func_dryfield_breezeway_8017FE08` to pick its next state from.
s32 func_dryfield_breezeway_8017FBC8(Task* task, s32 msgId, s32 item, s32 arg3)
{
    _DryfieldBreezewayKeyItemEventWork* work = task->work;

    if (item == 0x11B) {
        work->keyItemAccepted = 1;
        return 1;
    }
    work->keyItemAccepted = 0;
    return 0;
}

/// The `ratan2` angle of the direction from (`arg0`, `arg1`) to (`arg2`, `arg3`).
static s16 func_dryfield_breezeway_8017FBEC(s16 arg0, s16 arg1, s16 arg2, s16 arg3)
{
    SVECTOR vec;

    vec.vx = arg2 - arg0;
    vec.vy = arg3 - arg1;
    vec.vz = 0;
    VectorNormalSS(&vec, &vec);
    return ratan2(vec.vx, vec.vy);
}

/// The room's key-item event task, run from `D_dryfield_breezeway_80182E18`:
/// dispatches the current state through the seven handlers of
/// `D_dryfield_breezeway_8017D5E8`, copied onto the stack first so the call
/// goes through a local table rather than through `.rodata`.
void func_dryfield_breezeway_8017FC38(Task* task)
{
    TaskFuncTable7 sp;

    sp = D_dryfield_breezeway_8017D5E8;
    sp.funcs[task->state](task);
}

#include "../../shared/action_prompt_hit_test.inc.c"

/// State 1 of the room's key-item event task: arms the action prompt and
/// resets the caller's kill countdown. It sets the aiming speed and the idle
/// cursor, clears the screen position that cursor movement updates and
/// `itemMenuOpenHotspotCommands` later copies, and advances the caller's state.
static void func_dryfield_breezeway_8017FD68(Task* task)
{
    ActionPrompt* prompt = D_80114D28;

    prompt->cursorSpeed = ACTION_PROMPT_SPEED_AIM;
    prompt->mode        = ACTION_PROMPT_MODE_IDLE;
    prompt->screen.xy.x = 0;
    prompt->screen.xy.y = 0;
    task->killCountdown = 0;
    task->state         = task->state + 1;
}

/// State 3 of the room's key-item event task, run once the room's hotspot
/// scan has landed on an entry: re-seeds the cursor scan
/// `func_dryfield_breezeway_8017EB8C` at its reset position, clears the
/// prompt's highlight state, then re-spawns the prompt at the coordinates the
/// gameplay side left in `D_80114D28` with the Examine/Push action the scan latched in
/// `_DryfieldBreezewayKeyItemEventWork::promptKind`, and steps the caller's script on one state.
static void func_dryfield_breezeway_8017FD9C(Task* task)
{
    ActionPrompt*                       prompt = D_80114D28;
    _DryfieldBreezewayKeyItemEventWork* work   = task->work;

    func_dryfield_breezeway_8017EB8C(task, 0, DRYFIELD_BREEZEWAY_LINE_REST_Y);
    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    itemMenuOpenHotspotCommands(prompt->screen.xy.x, prompt->screen.xy.y, work->promptKind);
    task->state = 4;
}

/// Closes whatever the hotspot scan left up and picks the room's next state:
/// re-seeds the cursor scan `func_dryfield_breezeway_8017EB8C` and clears the
/// prompt's highlight state as the arm above does, then interrogates the
/// gameplay side. If `itemMenuIsHotspotActionConfirmed` reports that the
/// Examine/Push row was accepted, it starts cap slot 7 and returns to state 2.
/// Otherwise `_DryfieldBreezewayKeyItemEventWork::keyItemAccepted`, written by
/// `func_dryfield_breezeway_8017FBC8`, selects state 6 for an accepted key item
/// or state 2 to resume scanning.
static void func_dryfield_breezeway_8017FE08(Task* task)
{
    ActionPrompt*                       prompt = D_80114D28;
    _DryfieldBreezewayKeyItemEventWork* work   = task->work;
    s32                                 state;

    func_dryfield_breezeway_8017EB8C(task, 0, DRYFIELD_BREEZEWAY_LINE_REST_Y);
    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    if (itemMenuIsHotspotActionConfirmed() != 0) {
        capStartSequenceSlot(7, 0, 0);
        state = 2;
    } else if (work->keyItemAccepted == 1) {
        state = 6;
    } else {
        state = 2;
    }
    /* `*&state`: taking the address keeps `state` in a stack slot, so the arms
       above are memory stores rather than the register assignments jump.c's
       `if (c) x = a; else x = b;` fold needs to hoist the else arm over the
       `keyItemAccepted` test. Keeping that arm in its own block is what puts the value
       in $v0. */
    task->state = *&state;
}

#include "../../shared/action_prompt_event_end.inc.c"

#include "../../shared/action_prompt_reset.inc.c"

void func_dryfield_breezeway_8017FF7C(Task* task)
{
    s32         mask;
    EffectWork* eff;
    GfxCoord*   coord;
    GfxCoord*   player;
    s32         limit;
    s32         pan;

    mask   = 1 << gGameSession->location.loc.view;
    eff    = task->spawnArg2.pointer;
    coord  = task->extra.coordBody->coord;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
    if (mask & 0x18) {
        func_dryfield_breezeway_8018034C(coord, &D_dryfield_breezeway_80183164, 0x600, 0x80);
    } else if (mask & 0x20) {
        _glowDrawRayStar(coord, &D_dryfield_breezeway_80183164, 0x600, 0x10);
    }
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        return;
    }
    gRoomEffectState->roomEffectMode = ROOM_EFFECT_VIEW_ENABLED;
    if (gameFlagGetNibble(GAME_FLAG_BREEZEWAY_FIRST_EVENT_SEEN) == 0) {
        if (gGameSession->location.loc.view == 2) {
            limit           = (player->coord.t[0] - 5856) >> 7;
            eff->move.vx    = 12000;
            eff->move.vy    = -3000;
            eff->move.vz    = 3000;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if ((u16)((gRandomLcgState >> 16) % 100) < limit) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                effectSpawn(EFFECT_DRYFIELD_BREEZEWAY_BOUNCING_PARTICLE, coord, (s32)(gRandomLcgState >> 16) % limit + 0x40, &eff->move);
            }
            if (eff->step == 0) {
                sndEvtRequestScriptStart(SOUND_BREEZEWAY_EFFECT_LOOP, 0, 0);
                eff->step = 1;
            }
        } else if (gGameSession->location.loc.view == 3) {
            eff->scale      = 0x10;
            eff->move.vx    = player->coord.t[0] + 0x100;
            eff->move.vy    = -3000;
            eff->move.vz    = 3000;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            effectSpawn(EFFECT_DRYFIELD_BREEZEWAY_BOUNCING_PARTICLE, coord, ((gRandomLcgState >> 16) & 0x7F) + 0x40, &eff->move);
            if (eff->step < 2) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                if (!((gRandomLcgState >> 16) & 3)) {
                    pan = (s8)worldCoordGetOriginAudioPan(coord);
                    sndEvtRequestScriptStart(SOUND_BREEZEWAY_EFFECT_BURST, pan, (s8)worldCoordGetOriginAudioDepth(coord));
                    eff->step = 2;
                }
            }
        }
    } else if (gameFlagGetNibble(GAME_FLAG_BREEZEWAY_FIRST_EVENT_SEEN) == 1) {
        if (eff->step != 0) {
            sndEvtRequestScriptStop(SOUND_BREEZEWAY_EFFECT_LOOP, SOUND_SCRIPT_STOP_NO_FADE);
            eff->step = 0;
        }
        if (eff->scale != 0) {
            eff->scale--;
            eff->move.vx    = 16000;
            eff->move.vy    = -3000;
            eff->move.vz    = 2750;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            effectSpawn(EFFECT_DRYFIELD_BREEZEWAY_BOUNCING_PARTICLE, coord, ((gRandomLcgState >> 16) & 0xFF) + 0x40, &eff->move);
            eff->move.vx    = 17000;
            eff->move.vy    = -3000;
            eff->move.vz    = 4000;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            effectSpawn(EFFECT_DRYFIELD_BREEZEWAY_BOUNCING_PARTICLE, coord, ((gRandomLcgState >> 16) & 0xFF) + 0x40, &eff->move);
        }
    }
}

/// Draws a red light shaft at a point. `data` is rotated by
/// `coord`'s `workm` and offset by its translation, then projected through
/// `GsWSMATRIX` into a `RoomGlowSpriteScratch` block; nothing is drawn when
/// `otz` is 0x10 or less. Two gouraud `POLY_G4` halves of half width
/// `(s16)arg3 * 32 / otz` and two `LINE_G3` diagonals meet at the projected
/// point, whose vertex pulses red as `rsin(animFrame * arg2) / 34 + 0x78`.
static void func_dryfield_breezeway_8018034C(GfxCoord* coord, SVECTOR* data, s32 arg2, s32 arg3)
{
    RoomGlowSpriteScratch* block;
    POLY_G4*               prim;
    LINE_G3*               line;
    s32                    i;
    s32                    color;
    s32                    pulse;
    s32                    twice;
    s32                    t;
    s32                    t2;

    actorRenderComposeCoord(coord);
    block = SCRATCH_STACK_RESERVE_BLOCK(RoomGlowSpriteScratch);

    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(data);
    gte_rtv0();
    gte_stsv(&block->worldPos);
    block->worldPos.vx += coord->workm.t[0];
    block->worldPos.vy += coord->workm.t[1];
    block->worldPos.vz += coord->workm.t[2];

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPos);
    gte_rtps();
    gte_stsxy(&block->screenPos);
    gte_stszotz(&block->otz);
    if (block->otz >= 0x11) {
        pulse             = rsin(gDisplayState.animFrame * (s16)arg2);
        i                 = 0;
        block->halfExtent = ((s16)arg3 << 5) / block->otz;
        color             = pulse / 34 + 0x78;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, color, 0, 0);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->screenPos.vx - block->halfExtent;
            prim->x1 = prim->x2 = block->screenPos.vx;
            prim->x3            = block->screenPos.vx + block->halfExtent;
            prim->y0 = prim->y2 = prim->y3 = block->screenPos.vy;
            twice                          = i << 1;
            prim->y1                       = (block->screenPos.vy - block->halfExtent) + block->halfExtent * twice;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz);
            i++;
        } while (i < 2);

        i = 0;
        do {
            line           = gGpuPrimCursor;
            gGpuPrimCursor = line + 1;
            setLineG3(line);
            setRGB0(line, 0, 0, 0);
            setRGB1(line, color, 0, 0);
            setRGB2(line, 0, 0, 0);
            t        = i * 3 - 1;
            t2       = i + 1;
            line->x0 = block->screenPos.vx + (block->halfExtent * t);
            line->y0 = block->screenPos.vy - (block->halfExtent * t2);
            line->x1 = block->screenPos.vx;
            line->y1 = block->screenPos.vy;
            line->x2 = block->screenPos.vx - (block->halfExtent * t);
            line->y2 = block->screenPos.vy + (block->halfExtent * t2);
            addPrim((&gGpuCurrentOt[((u32)block->otz << gDisplayState.otDepthShift) >> 4 & 0x3FF]),
                    line);
            gpuSetPrimitiveBlendMode(line, GPU_BLEND_ADD, block->otz);
            i = t2;
        } while (i < 2);
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomGlowSpriteScratch);
}

#define GLOW_DRAW_RAY_STAR_OUTER(p, c, h) setRGB2(p, h, 0, 0)
#define GLOW_DRAW_RAY_STAR_INNER(p, c, h) setRGB2(p, c, 0, 0)
#define GLOW_DRAW_RAY_STAR_RAY(p, c)      setRGB2(p, c, 0, 0)
#define GLOW_DRAW_RAY_STAR_RAY_HALFWORD   1
#include "../../shared/glow_draw_ray_star.inc.c"

/// Per-frame update for a bouncing sprite particle drawn by
/// `func_dryfield_breezeway_80181938`. The first frame resets the model's
/// rotation, rolls a frame period, start frame, angle and spin from the LCG,
/// picks a random direction in `move` when none was supplied, and
/// normalises it. Afterwards it steps along `move` at speed `scale`
/// and tests the step with `worldCollisionProbeGridSegment`; a hit undoes the step, blends the
/// direction with the returned vector, halves speed and spin, and spawns
/// effect 0x60054 while the particle is young, settling into state 2 once hits
/// come close together at low speed. A miss adds `0x5000 / scale` to the
/// direction's y component. Over `age` the sprite fades from 30 to 60 and
/// is then released. The age does not advance while an event is running.
void func_dryfield_breezeway_80181264(Task* task)
{
    EffectWork* work  = task->spawnArg2.pointer;
    GfxCoord*   coord = task->extra.coordBody->coord;
    MATRIX*     m;
    SVECTOR     delta;
    SVECTOR     dir;
    SVECTOR     pos;
    u8          color[3];

    if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_HIDDEN) {
        if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        effectKillTask(work, task);
        return;
    }

    actorRenderComposeCoord(coord);
    work->age++;

    switch (task->state) {
        case 0:
            m                    = &coord->coord;
            MATRIX_PAIR(m, 0, 0) = 0x1000;
            MATRIX_PAIR(m, 0, 2) = 0;
            MATRIX_PAIR(m, 1, 1) = 0x1000;
            MATRIX_PAIR(m, 2, 0) = 0;
            m->m[2][2]           = 0x1000;
            work->pos.vx         = (u16)task->spawnArg1.value & 0xFFF;
            work->scale          = 0x50;
            gRandomLcgState      = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->pos.vy         = (gRandomLcgState >> 16) & 7;
            gRandomLcgState      = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->index          = (gRandomLcgState >> 16) & 7;
            gRandomLcgState      = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->pos.vz         = (gRandomLcgState >> 16) & 0xFFF;
            gRandomLcgState      = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->period         = 0x200 - ((gRandomLcgState >> 16) & 0x3FF);
            if ((work->move.vx | work->move.vy | work->move.vz) == 0) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->move.vx   = 0x40 - ((gRandomLcgState >> 16) & 0x7F);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->move.vy   = ((gRandomLcgState >> 16) & 0x3F) + 0x40;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->move.vz   = 0x40 - ((gRandomLcgState >> 16) & 0x7F);
                gte_SetRotMatrix(&work->parent->coord);
                gte_ldv0(&work->move);
                gte_rtv0();
                gte_stsv(&work->move);
            }
            VectorNormalSS(&work->move, &work->move);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            task->state         = 1;
            break;
        case 1:
            if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                work->age--;
            } else {
                work->pos.vz += work->period;
                if (work->pos.vy != 0 && work->age % work->pos.vy == 0) {
                    work->index++;
                }
                gte_lddp(work->scale);
                gte_ldsv(&work->move);
                gte_gpf12();
                gte_stsv(&delta);
                coord->coord.t[0]  += delta.vx;
                coord->coord.t[1]  += delta.vy;
                coord->coord.t[2]  += delta.vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                gte_SetRotMatrix(&gGfxViewCoord.workm);
                gte_ldv0(&delta);
                gte_rtv0();
                gte_stsv(&dir);
                pos.vx  = coord->workm.t[0];
                pos.vy  = coord->workm.t[1];
                pos.vz  = coord->workm.t[2];
                dir.vx += pos.vx;
                dir.vy += pos.vy;
                dir.vz += pos.vz;
                if (worldCollisionProbeGridSegment(&dir, &pos, &dir, &pos) == 1) {
                    coord->coord.t[0] -= delta.vx;
                    coord->coord.t[1] -= delta.vy;
                    coord->coord.t[2] -= delta.vz;
                    work->move.vx      = (pos.vx >> 1) + (work->move.vx >> 1);
                    work->move.vy      = pos.vy + (work->move.vy >> 1);
                    work->move.vz      = (pos.vz >> 1) + (work->move.vz >> 1);
                    VectorNormalSS(&work->move, &work->move);
                    work->scale  = work->scale >> 1;
                    work->period = work->period >> 1;
                    gte_lddp(work->scale);
                    gte_ldsv(&work->move);
                    gte_gpf12();
                    gte_stsv(&delta);
                    coord->coord.t[0] += delta.vx;
                    coord->coord.t[1] += delta.vy;
                    coord->coord.t[2] += delta.vz;
                    if (work->age < 60) {
                        effectSpawn(EFFECT_DUST_PUFF, coord, work->pos.vx + 0x2100, NULL);
                    }
                    if (work->age - work->step < 8 && work->scale < 0x20) {
                        task->state = 2;
                    } else {
                        work->step = work->age;
                    }
                } else if (work->scale > 0) {
                    work->move.vy += 0x5000 / work->scale;
                }
            }
            if (work->age < 30) {
                func_dryfield_breezeway_80181938(task, NULL);
            } else if (work->age < 60) {
                color[0] = color[1] = color[2] = (60 - work->age) * 4;
                func_dryfield_breezeway_80181938(task, color);
            } else {
                effectKillTask(work, task);
            }
            break;
        case 2:
            if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                work->age--;
            }
            if (work->age < 30) {
                func_dryfield_breezeway_80181938(task, NULL);
            } else if (work->age < 60) {
                color[0] = color[1] = color[2] = (60 - work->age) * 4;
                func_dryfield_breezeway_80181938(task, color);
            } else {
                effectKillTask(work, task);
            }
            break;
    }
}

/// Draws `task`'s effect as a camera-facing 16x16 `POLY_FT4` sprite at the
/// translation of its body's single coordinate, through a 0x1C-byte scratch stack
/// block. Nothing is drawn when the projection flags a negative result. The
/// frame is `index & 7` along row 0xF0 of texture page 0x2B, and the quad's
/// half extent is `pos.vx * 23 / otz`, rotated by the angle in `pos.vz`.
/// A non-null `color` tints the sprite and makes it semi-transparent.
static void func_dryfield_breezeway_80181938(Task* task, u8* color)
{
    ModelObjectCoordBody* body = task->extra.coordBody;
    EffectWork*           work = task->spawnArg2.pointer;
    void**                scratch;
    GfxCoord*             coord;
    EffectShapeScratch*   block;
    POLY_FT4*             prim;

    scratch              = SCRATCH_HEAD_ADDR;
    coord                = body->coord;
    block                = SCRATCH_PUSH_AT(scratch, EffectShapeScratch);
    block->worldPoint.vx = coord->workm.t[0];
    block->worldPoint.vy = coord->workm.t[1];
    block->worldPoint.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPoint);
    gte_rtps();
    gte_stsxy(&block->screenX);
    gte_stflg(&block->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&block->depth);
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
        if (color != NULL) {
            prim->r0 = color[0];
            prim->g0 = color[1];
            prim->b0 = color[2];
            setSemiTrans(prim, 1);
        } else {
            setcode(prim, 0x2D);
        }
        prim->tpage            = 0x2B;
        prim->clut             = 0x43C0;
        prim->u0               = (work->index & 7) * 16;
        prim->v0               = 0xF0;
        prim->u1               = (work->index & 7) * 16 + 0xF;
        prim->v1               = 0xF0;
        prim->u2               = (work->index & 7) * 16;
        prim->v2               = 0xFF;
        prim->u3               = (work->index & 7) * 16 + 0xF;
        prim->v3               = 0xFF;
        block->extent.corner.x = (((work->pos.vx * 0x17) / block->depth) * rsin(work->pos.vz)) >> 12;
        block->extent.corner.y = (((work->pos.vx * 0x17) / block->depth) * rcos(work->pos.vz)) >> 12;
        prim->x0               = block->screenX + block->extent.corner.x;
        prim->x3               = block->screenX - block->extent.corner.x;
        prim->y0               = block->screenY - block->extent.corner.y;
        prim->y3               = block->screenY + block->extent.corner.y;
        block->extent.corner.x = (((work->pos.vx * 0x17) / block->depth) * rsin(work->pos.vz + 0x400)) >> 12;
        block->extent.corner.y = (((work->pos.vx * 0x17) / block->depth) * rcos(work->pos.vz + 0x400)) >> 12;
        prim->x1               = block->screenX + block->extent.corner.x;
        prim->x2               = block->screenX - block->extent.corner.x;
        prim->y1               = block->screenY - block->extent.corner.y;
        prim->y2               = block->screenY + block->extent.corner.y;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}
