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
#include "gameplay/items.h"
#include "gameplay/gpu_image_upload.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_targets.h"
#include "gameplay/scene_combat.h"
#include "gameplay/view.h"

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
static s32 _actionPromptHitTestDefault(ActionPromptHotspot* hotspots, s16 cursorX, s16 cursorY);
#define ACTION_PROMPT_HIT_TEST _actionPromptHitTestDefault
#include "../../shared/action_prompt.h"
#include "../../shared/room_events.h"
#include "../../shared/glow_draw.h"

/// Motion, fade and texture units of the breezeway's bouncing sprite particle.
enum {
    DRYFIELD_BREEZEWAY_PARTICLE_INITIALIZE        = 0,
    DRYFIELD_BREEZEWAY_PARTICLE_FLYING            = 1,
    DRYFIELD_BREEZEWAY_PARTICLE_SETTLED           = 2,
    DRYFIELD_BREEZEWAY_PARTICLE_INITIAL_SPEED     = 80,
    DRYFIELD_BREEZEWAY_PARTICLE_SIZE_MASK         = 0xFFF,
    DRYFIELD_BREEZEWAY_PARTICLE_SETTLE_SPEED      = 32,
    DRYFIELD_BREEZEWAY_PARTICLE_SETTLE_INTERVAL   = 8,
    DRYFIELD_BREEZEWAY_PARTICLE_GRAVITY_Q12       = 0x5000,
    DRYFIELD_BREEZEWAY_PARTICLE_FADE_START        = 30,
    DRYFIELD_BREEZEWAY_PARTICLE_LIFETIME          = 60,
    DRYFIELD_BREEZEWAY_PARTICLE_FADE_STEP         = 4,
    DRYFIELD_BREEZEWAY_PARTICLE_FRAME_COUNT       = 8,
    DRYFIELD_BREEZEWAY_PARTICLE_FRAME_PERIOD_MASK = 7,
    DRYFIELD_BREEZEWAY_PARTICLE_FRAME_TEXELS      = 16,
    DRYFIELD_BREEZEWAY_PARTICLE_TEXTURE_ROW       = 0xF0,
    DRYFIELD_BREEZEWAY_PARTICLE_TEXTURE_PAGE      = 0x2B,
    DRYFIELD_BREEZEWAY_PARTICLE_CLUT              = 0x43C0,
    DRYFIELD_BREEZEWAY_PARTICLE_RADIUS_SCALE      = 23,
    // Dust uses two ticks per texture frame and a size 256 units larger than the particle.
    DRYFIELD_BREEZEWAY_PARTICLE_DUST_ARG_BIAS = (2 << 12) + 256,
};

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

/// Screen-pixel geometry of the key-item line and its two steered halves.
enum {
    DRYFIELD_BREEZEWAY_LINE_ANCHOR_X              = 0,
    DRYFIELD_BREEZEWAY_LINE_ANCHOR_Y              = -80,
    DRYFIELD_BREEZEWAY_LINE_REACH                 = 112,
    DRYFIELD_BREEZEWAY_LINE_GRAB_RADIUS           = 32,
    DRYFIELD_BREEZEWAY_LINE_SEGMENT_LENGTH        = 4,
    DRYFIELD_BREEZEWAY_LINE_MAX_SEGMENTS_PER_HALF = 30,
    DRYFIELD_BREEZEWAY_LINE_TARGET_DISTANCE_LIMIT = 9,
    DRYFIELD_BREEZEWAY_LINE_HALF_WIDTH            = 4,
};

/// Angle units and fixed-point precision used to steer and bend the line.
enum {
    DRYFIELD_BREEZEWAY_LINE_SCALE_FRACTION_BITS = 12,
    DRYFIELD_BREEZEWAY_LINE_SIGNED_ANGLE_SHIFT  = 20,
    DRYFIELD_BREEZEWAY_LINE_TURN_LARGE          = 0x200,
    DRYFIELD_BREEZEWAY_LINE_TURN_MEDIUM         = 0x100,
    DRYFIELD_BREEZEWAY_LINE_TURN_SMALL          = 0x80,
};

/// Texture binding and ordering-table entry for the key-item line's quads.
enum {
    DRYFIELD_BREEZEWAY_LINE_TEXTURE_PAGE         = 0x8E,
    DRYFIELD_BREEZEWAY_LINE_CLUT                 = 0x4000,
    DRYFIELD_BREEZEWAY_LINE_ORDERING_TABLE_INDEX = 100,
    DRYFIELD_BREEZEWAY_LINE_TEXTURE_U_END        = 16,
    DRYFIELD_BREEZEWAY_LINE_TEXTURE_V_END        = 4,
};

/// State selectors used by the key-item event's seven-entry dispatch table.
enum {
    DRYFIELD_BREEZEWAY_KEY_ITEM_STATE_INITIALIZE    = 0,
    DRYFIELD_BREEZEWAY_KEY_ITEM_STATE_ARM_PROMPT    = 1,
    DRYFIELD_BREEZEWAY_KEY_ITEM_STATE_SELECT_MODEL  = 2,
    DRYFIELD_BREEZEWAY_KEY_ITEM_STATE_OPEN_PROMPT   = 3,
    DRYFIELD_BREEZEWAY_KEY_ITEM_STATE_WAIT_FOR_ITEM = 4,
    DRYFIELD_BREEZEWAY_KEY_ITEM_STATE_EXIT          = 5,
    DRYFIELD_BREEZEWAY_KEY_ITEM_STATE_LEAD_LINE     = 6,
};

/// Equipped-bank clip the opening encounter plays before starting pursuit.
enum { DRYFIELD_BREEZEWAY_FIRST_EVENT_PLAYER_ANIMATION = 9 };

/// Actor commands used by this room to stage and start its desert-chaser encounter.
enum { DRYFIELD_BREEZEWAY_ACTOR_COMMAND_STAGE         = 1,
       DRYFIELD_BREEZEWAY_ACTOR_COMMAND_START_PURSUIT = 2 };

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
    MATRIX lightMatrix;      // Light-direction matrix of the task's model
    MATRIX colorMatrix;      // Light-colour matrix of the task's model; its translation is the background colour
    s32    keyItemAccepted;  // Answer to the last key-item query (0 refused, 1 the room's item was offered)
    byte   field_44[8];      // Never accessed; role unproven
    s16    hotspotId;        // `id` of the hotspot the player confirmed on the model; never read
    s16    lineEndX;         // Free end of the line, pixels right of the screen centre
    s16    lineEndY;         // Free end of the line, pixels below the screen centre
    s16    fallSpeed;        // Pixels the released end drops per frame; grows by one a frame and clears at the rest height
    s16    swingSpeed;       // Horizontal speed of the released end, before the `swingDamping` shift
    s16    swingDamping;     // Right shift applied to `swingSpeed` (`DRYFIELD_BREEZEWAY_LINE_SWING_DAMPING_*`); one more each time the end crosses the centre
    s16    previousLineEndX; // `lineEndX` before this frame's move
    s16    previousLineEndY; // `lineEndY` before this frame's move; kept only while the cursor leads the end
    s8     promptKind;       // `promptKind` of the confirmed hotspot, forwarded when the prompt opens
} _DryfieldBreezewayKeyItemEventWork;
STATIC_ASSERT_SIZEOF(_DryfieldBreezewayKeyItemEventWork, 0x60);

/// The `TaskDesc` `_dryfieldBreezewayInitializeKeyItemEvent` spawns the room's prompt
/// task (`_dryfieldBreezewayActionPromptTask`) from, and the single-entry `TaskMessageEntry[]` it parks in `Task::msgTable`
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
/// `_dryfieldBreezewayStageSecondDesertChaser`, `_dryfieldBreezewayStageFirstEventSkip` and
/// `_dryfieldBreezewayProcessFirstEventAction`: world x 17000, y 0, z 3000, yaw 0xA00.
extern ActorTransform D_dryfield_breezeway_80181E28;

/// The two placements that follow it in the same three-record run, which
/// `_dryfieldBreezewayProcessFirstEventAction` sends to slot 3 as the second and third
/// message of its state-1 sequence: `[0]` is the record message 0x3E9 places the
/// player with, and `[1]` -- the run's third record -- the one message 0x3EE
/// does. The label the decomp references is the start of this array, so the
/// third record is reached as `[1]` rather than by a symbol of its own.
extern ActorTransform D_dryfield_breezeway_80181E40[];

/// The key-item prompt's own hotspot table: the one-entry
/// `ActionPromptHotspot` run, ended by `ACTION_PROMPT_HOTSPOT_END`, that
/// `_dryfieldBreezewayScanKeyItemHotspot` hit-tests at the
/// prompt's own screen position and walks for the entry the cursor landed on,
/// where the prop table below is hit-tested at the cursor itself. Its `id` is
/// the script variant the prompt confirms, which the scan parks in the event
/// work block (`_DryfieldBreezewayKeyItemEventWork::hotspotId`, with `promptKind`) before state
/// 3. `_dryfieldBreezewayInitializeKeyItemEvent` clears its `hit` along with the other
/// table's.
extern ActionPromptHotspot D_dryfield_breezeway_80182E00[];

/// This room's prop hotspot table: an `ActionPromptHotspot` run ended by
/// `ACTION_PROMPT_HOTSPOT_END`. `_actionPromptHitTestDefault` hit-tests the action
/// cursor against it. Its entries are the room's interactive props:
/// `_dryfieldBreezewayInitializeKeyItemEvent` clears every entry's `hit` through it
/// before the first frame -- both tables', so the key-item prompt above starts
/// clean too -- and the scan in
/// `func_dryfield_breezeway_8017E81C` walks it for the entry the cursor landed
/// on.
extern ActionPromptHotspot D_dryfield_breezeway_80182DDC[];

static void _actionPromptResetDefault(Task* task);
static void _dryfieldBreezewayInitializeKeyItemEvent(Task* task);
static void _dryfieldBreezewayScanKeyItemHotspot(Task* task);
static void func_dryfield_breezeway_8017E81C(Task* task);
static void _dryfieldBreezewayUpdateKeyItemLine(Task* task, s16 leadX, s16 leadY);
static void _dryfieldBreezewayDrawKeyItemLineSegment(s16 angle, s16 length, const SVECTOR* start, SVECTOR* tipOut, _DryfieldBreezewayLineEdge* edge);
static s16  _dryfieldBreezewayIsLinePointNearTarget(const SVECTOR* target, const SVECTOR* point);
static void _dryfieldBreezewayPlaceKeyItemModelAtLineTip(Task* task, s16 tipX, s16 tipY);
static s16  _dryfieldBreezewayGetLineBearing(s16 fromX, s16 fromY, s16 toX, s16 toY);
static void _dryfieldBreezewayArmKeyItemPrompt(Task* task);
static void _dryfieldBreezewayOpenKeyItemCommands(Task* task);
static void func_dryfield_breezeway_8017FE08(Task* task);
static void _actionPromptEventEnd(Task* eventTask);
static void _dryfieldBreezewayDrawRedDiamondGlow(GfxCoord* coord, const SVECTOR* localPoint, s16 pulseRate, s16 radiusScale);
static void _dryfieldBreezewayDrawBouncingParticle(Task* task, const u8 rgb[3]);

/// State handlers of the room's key-item event task, indexed by its state
/// through `_dryfieldBreezewayKeyItemEventTask`: set-up, prompt arming, the
/// prompt-position scan, prompt spawning, the key-item answer, the exit and
/// the cursor-hotspot scan.
static const TaskFuncTable7 D_dryfield_breezeway_8017D5E8 = {
    {
        _dryfieldBreezewayInitializeKeyItemEvent,
        _dryfieldBreezewayArmKeyItemPrompt,
        _dryfieldBreezewayScanKeyItemHotspot,
        _dryfieldBreezewayOpenKeyItemCommands,
        func_dryfield_breezeway_8017FE08,
        _actionPromptEventEnd,
        func_dryfield_breezeway_8017E81C,
    }
};

static void _dryfieldBreezewayStageSecondDesertChaser(void);
static void _dryfieldBreezewayRequestFirstEventAction(s16 action);

static void _dryfieldBreezewayInitFirstEventTask(Task* task);
static void _dryfieldBreezewayFirstEventTask(Task* task);
static void _dryfieldBreezewayEngageFirstEventBattle(void);
static void _dryfieldBreezewayStageFirstEventSkip(void);

static s32  _dryfieldBreezewayUseBottlecapMagnet(Task* task, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg);
static void _dryfieldBreezewayActionPromptTask(Task* task);
static void _dryfieldBreezewayKeyItemEventTask(Task* task);

TaskDesc gRoomEventTaskDesc = { { { TASK_BODY_NONE, 32 } }, roomEventTask, { .value = 0 } };

TaskMessageEntry D_dryfield_breezeway_80181DE0[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_dryfield_breezeway_8017D940 },
    { ROOM_MESSAGE_USE_KEY_ITEM, dryfieldBreezewayForwardKeyItemUse },
    { ROOM_MESSAGE_COMMAND, func_dryfield_breezeway_8017DA48 },
    { ROOM_MESSAGE_SOUND, dryfieldBreezewayHandleSoundMessage },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _dryfieldBreezewayRequestFirstEventAction }, { .value = DRYFIELD_BREEZEWAY_FIRST_EVENT_ACTION_STAGE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _dryfieldBreezewayRequestFirstEventAction }, { .value = DRYFIELD_BREEZEWAY_FIRST_EVENT_ACTION_PLAY_ANIMATION_9 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _dryfieldBreezewayStageSecondDesertChaser }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _dryfieldBreezewayEngageFirstEventBattle }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_dryfield_breezeway_80181F90[12] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _dryfieldBreezewayStageFirstEventSkip }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _dryfieldBreezewayEngageFirstEventBattle }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc D_dryfield_breezeway_801820B0[2] = {
    { { { TASK_BODY_NONE, 192 } }, _dryfieldBreezewayInitFirstEventTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _dryfieldBreezewayFirstEventTask, { .value = 0 } },
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

TaskDesc D_dryfield_breezeway_80182DC0 = { { { TASK_BODY_NONE, 192 } }, _dryfieldBreezewayActionPromptTask, { .value = 0 } };

TaskMessageEntry D_dryfield_breezeway_80182DCC[2] = {
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldBreezewayUseBottlecapMagnet },
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

TaskDesc D_dryfield_breezeway_80182E18 = { { { TASK_BODY_TMD, 192 } }, _dryfieldBreezewayKeyItemEventTask, { .model = &_gDryfieldBreezewayModel04E8C } };

u_long D_dryfield_breezeway_80182E24[64] = {
#include "assets/dryfield_breezeway_image_05864.inc"
};

GpuImageUpload D_dryfield_breezeway_80182F24[2] = {
    { GPU_IMAGE_UPLOAD_COPY, 0, { 896, 0, 8, 16 }, D_dryfield_breezeway_80182E24 },
    { GP_IMG_REC_END, 0, { 0, 0, 0, 0 }, NULL },
};

/// The event script and its input skip script started with `evsStartScriptWithSkip`.
/// Both live in the room's trailing data blob.
extern EvsCommand D_dryfield_breezeway_80181E70[];

extern EvsCommand D_dryfield_breezeway_80181F90[];

static void _dryfieldBreezewayProcessFirstEventAction(Task* task);

/// Broadcasts a stage/area-scoped command to the scene's placed actors.
///
/// `command` is a full u16 selector in each actor's command namespace. The scene
/// and current session must be live. Dispatch consumes the stack request
/// synchronously and discards the actors' replies.
static inline void _dryfieldBreezewayBroadcastActorCommand(u16 command)
{
    ActorCommand msg;

    msg.context.loc.stage = gGameSession->location.loc.stage;
    msg.context.loc.area  = gGameSession->location.loc.area;
    msg.command           = command;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &msg, ACTOR_COMMAND_MESSAGE_APPLY);
}

/// Plays a clip from the current character's equipped-weapon bank under scripted control.
///
/// `animationId`, `blend` and `blendFrames` are u16 selectors/durations promoted
/// into the request's signed words; frames are normal playback frames and `blend`
/// is an `ANIMATION_BLEND_*` choice. The current bank table requires character
/// 1, weapon slot 0..32 and a loaded bank/clip. The other-character branch
/// retains offset 34; its reachable bank storage is unproven.
/// World collision is disabled. Dispatch consumes the request synchronously;
/// the animation resources remain borrowed through playback.
static inline void _dryfieldBreezewayPlayPlayerAnimation(u16 animationId, u16 blend, u16 blendFrames)
{
    enum { DRYFIELD_BREEZEWAY_PLAYER_PRIMARY_CHARACTER    = 1,
           DRYFIELD_BREEZEWAY_PRIMARY_WEAPON_BANK_FIRST   = 1,
           DRYFIELD_BREEZEWAY_OTHER_CHARACTER_BANK_OFFSET = 0x22 };

    AnimationPlayRequest request;
    s32                  weapon;

    // Character 1 selects the equipped-weapon bank; the retained other-character offset is unproven.
    weapon                       = gPlayerStatus.weapon;
    request.source.index         = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == DRYFIELD_BREEZEWAY_PLAYER_PRIMARY_CHARACTER) ? weapon + DRYFIELD_BREEZEWAY_PRIMARY_WEAPON_BANK_FIRST : weapon + DRYFIELD_BREEZEWAY_OTHER_CHARACTER_BANK_OFFSET;
    request.animationId          = animationId;
    request.blend                = blend;
    request.blendFrames          = blendFrames;
    request.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &request, 0);
}

/// Consumes the opening script's pending actor-staging or player-animation request.
///
/// Staging places the first desert chaser, places and turns the player, and
/// saves logical view 4. The other request plays equipped-bank clip 9 with a
/// ten-frame interpolation. Every selector, including unsupported values, is
/// cleared. The task needs initialized work and live borrowed actor tasks;
/// messages consume their placement and animation records synchronously.
static void _dryfieldBreezewayProcessFirstEventAction(Task* task)
{
    enum { DRYFIELD_BREEZEWAY_FIRST_EVENT_CUTSCENE_CHASER = 0,
           DRYFIELD_BREEZEWAY_FIRST_EVENT_STAGING_VIEW    = 4 };

    _DryfieldBreezewayFirstEventWork* work;
    s32                               action;

    work   = task->work;
    action = work->action;

    switch (action) {
        default:
            work->action = DRYFIELD_BREEZEWAY_FIRST_EVENT_ACTION_NONE;
            return;
        case DRYFIELD_BREEZEWAY_FIRST_EVENT_ACTION_NONE:
            break;
        case DRYFIELD_BREEZEWAY_FIRST_EVENT_ACTION_STAGE:
            _dryfieldBreezewayBroadcastActorCommand(DRYFIELD_BREEZEWAY_ACTOR_COMMAND_STAGE);
            TASK_MESSAGE_DISPATCH_POINTER(work->desertChaserTasks[DRYFIELD_BREEZEWAY_FIRST_EVENT_CUTSCENE_CHASER], ACTOR_MESSAGE_PLACE, &D_dryfield_breezeway_80181E28, 0);
            TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, GAME_ACTOR_MESSAGE_PLACE, &D_dryfield_breezeway_80181E40[0], 0);
            TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, GAME_ACTOR_MESSAGE_TURN_TO_YAW, &D_dryfield_breezeway_80181E40[1], 0);
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = viewFindLogicalIndex(DRYFIELD_BREEZEWAY_FIRST_EVENT_STAGING_VIEW);
            break;
        case DRYFIELD_BREEZEWAY_FIRST_EVENT_ACTION_PLAY_ANIMATION_9:
            _dryfieldBreezewayPlayPlayerAnimation(DRYFIELD_BREEZEWAY_FIRST_EVENT_PLAYER_ANIMATION, ANIMATION_BLEND_INTERPOLATE, 10);
            break;
    }
    work->action = DRYFIELD_BREEZEWAY_FIRST_EVENT_ACTION_NONE;
}

/// Allocates the first-event work and binds its borrowed room actors.
///
/// Requires a bodyless task without existing work and live player/desert-chaser
/// placements 0 and 1 in the current stage/area. Clears the entire work block
/// and publishes the task only after allocation succeeds. Task teardown releases
/// the work, while the borrowed actors must outlive its use. Allocation failure
/// kills the task and leaves the published slot unchanged.
static inline void _dryfieldBreezewayInitializeFirstEventWork(Task* task)
{
    _DryfieldBreezewayFirstEventWork* work;
    s32                               placeKey;

    work       = memMalloc(sizeof(*work), false);
    task->work = work;
    if (work == NULL) {
        taskKill(task);
    } else {
        memFillBytes(work, 0, sizeof(*work));
        work->playerTask              = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        D_dryfield_breezeway_801843C0 = task;
        // Placement indices 0 and 1 identify the two actors in this room.
        placeKey                   = gGameSession->location.loc.area | (gGameSession->location.loc.stage << ENEMY_PLACE_STAGE_SHIFT);
        work->desertChaserTasks[0] = sceneFindEnemyByPlaceKey(placeKey)->task;
        placeKey                   = ((gGameSession->location.loc.stage << ENEMY_PLACE_STAGE_SHIFT) | (1 << ENEMY_PLACE_INDEX_SHIFT)) | gGameSession->location.loc.area;
        work->desertChaserTasks[1] = sceneFindEnemyByPlaceKey(placeKey)->task;
    }
}

/// Initializes and publishes the opening encounter's task/work, then retires on its next tick.
///
/// The work owns no actor tasks: it borrows the player and the current room's
/// desert-chaser placements 0 and 1, which must already exist. Allocation failure
/// kills the task; otherwise task teardown owns the work. No cutscene is started
/// here. The published task slot is not cleared on retirement.
static void _dryfieldBreezewayInitFirstEventTask(Task* task)
{
    enum { DRYFIELD_BREEZEWAY_FIRST_EVENT_TASK_INITIALIZE = 0,
           DRYFIELD_BREEZEWAY_FIRST_EVENT_TASK_RETIRE     = 1 };

    switch (task->state) {
        case DRYFIELD_BREEZEWAY_FIRST_EVENT_TASK_INITIALIZE:
            _dryfieldBreezewayInitializeFirstEventWork(task);
            task->state += 1;
            return;
        case DRYFIELD_BREEZEWAY_FIRST_EVENT_TASK_RETIRE:
            taskKill(task);
            return;
    }
}

/// Starts the breezeway's first encounter and services its script until the event ends.
///
/// State 0 waits while the attachment wheel or a pending display mode blocks
/// startup, then publishes borrowed player/desert-chaser bindings, plays clip
/// 1 and starts the normal/skip scripts. State 1 consumes script requests until
/// eventState becomes zero, then releases the task and its work. The published
/// handle is not cleared. Placements 0/1 and the equipped animation bank must
/// be live. The retained allocation-failure path continues after task teardown.
static void _dryfieldBreezewayFirstEventTask(Task* task)
{
    enum { DRYFIELD_BREEZEWAY_FIRST_EVENT_TASK_WAIT_TO_START = 0,
           DRYFIELD_BREEZEWAY_FIRST_EVENT_TASK_RUN_SCRIPT    = 1 };

    switch (task->state) {
        case DRYFIELD_BREEZEWAY_FIRST_EVENT_TASK_WAIT_TO_START:
            if (Gp_StateC08.mode == ATTACHMENT_MODE_WHEEL || gDisplayState.pendingMode != DISPLAY_MODE_NONE) {
                return;
            }
            // Bind the actors before scripts can send their staging requests.
            _dryfieldBreezewayInitializeFirstEventWork(task);
            _dryfieldBreezewayPlayPlayerAnimation(1, ANIMATION_BLEND_INTERPOLATE, 10);
            evsStartScriptWithSkip(D_dryfield_breezeway_80181E70, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_dryfield_breezeway_80181F90);
            task->state += 1;
            break;
        case DRYFIELD_BREEZEWAY_FIRST_EVENT_TASK_RUN_SCRIPT:
            if (gGameSession->eventState == 0) {
                taskKill(task);
                return;
            }
            break;
    }
    _dryfieldBreezewayProcessFirstEventAction(task);
}

/// Hands the opening encounter from its cutscene chaser to its pursuing combat chaser.
///
/// The published first-event task, work, scene and both placed actors must be live.
/// The scene command hides the cutscene chaser and starts the combat chaser;
/// placement then sets the latter's room-coordinate position and yaw. Both
/// requests are consumed synchronously, and neither actor is owned by this helper.
static inline void _dryfieldBreezewayStartFirstEventPursuit(void)
{
    enum { DRYFIELD_BREEZEWAY_FIRST_EVENT_COMBAT_CHASER = 1 };
    _DryfieldBreezewayFirstEventWork* work;

    work = D_dryfield_breezeway_801843C0->work;
    _dryfieldBreezewayBroadcastActorCommand(DRYFIELD_BREEZEWAY_ACTOR_COMMAND_START_PURSUIT);
    TASK_MESSAGE_DISPATCH_POINTER(work->desertChaserTasks[DRYFIELD_BREEZEWAY_FIRST_EVENT_COMBAT_CHASER], ACTOR_MESSAGE_PLACE, &D_dryfield_breezeway_80181E28, 0);
}

/// Starts the opening encounter's desert-chaser pursuit and places its second chaser at the mark.
///
/// Broadcasts actor command 2 before placing the second chaser. The published
/// first-event task, its work and the selected actor must still be live.
static void _dryfieldBreezewayStageSecondDesertChaser(void)
{
    _dryfieldBreezewayStartFirstEventPursuit();
}

/// Engages the scene battle after the opening encounter's normal or skip script.
static void _dryfieldBreezewayEngageFirstEventBattle(void)
{
    sceneEngageBattle(1);
}

/// Queues an opening-script action for the published first-event task's next tick.
///
/// `action` is a `DRYFIELD_BREEZEWAY_FIRST_EVENT_ACTION_*` selector, stored as
/// u16; unsupported selectors are cleared by the task. The action-step counter
/// is reset. The published task and its work must still be live.
static void _dryfieldBreezewayRequestFirstEventAction(s16 action)
{
    _DryfieldBreezewayFirstEventWork* work;

    work             = D_dryfield_breezeway_801843C0->work;
    work->action     = action;
    work->actionStep = 0;
}

/// Stages the opening encounter's final player pose and second chaser during the skip fade.
///
/// Plays equipped-bank animation 9 without blending or world collision, then
/// broadcasts actor command 2 and places the second desert chaser at its mark.
/// The player, loaded animation bank and published first-event task must be live.
static void _dryfieldBreezewayStageFirstEventSkip(void)
{
    _dryfieldBreezewayPlayPlayerAnimation(DRYFIELD_BREEZEWAY_FIRST_EVENT_PLAYER_ANIMATION, ANIMATION_BLEND_RESET, 0);
    _dryfieldBreezewayStartFirstEventPursuit();
}

/// Clears prior cursor hits from the key-item event's hotspots.
///
/// Borrows a writable, contiguous run ending at `ACTION_PROMPT_HOTSPOT_END`.
/// Leaves the sentinel and each hotspot's geometry, ID and prompt kind intact.
static inline void _dryfieldBreezewayClearKeyItemHotspotHits(ActionPromptHotspot* hotspot)
{
    while (hotspot->id != ACTION_PROMPT_HOTSPOT_END) {
        hotspot->hit = 0;
        hotspot++;
    }
}

/// Installs fixed lighting for the key-item interaction's model.
///
/// Requires a live TMD body and its owned key-item event work. Both matrices
/// use Q12 coefficients (ONE is unity); the colour rows are white, the light
/// rows are (1,1,1), (0,1,1), (1,1,0), and ambient RGB is half unity.
/// The model borrows the work's matrices through teardown. Translation in
/// the light matrix stays intact; ambient RGB is stored in the colour matrix.
static inline void _dryfieldBreezewayInitializeKeyItemLighting(Task* task)
{
    _DryfieldBreezewayKeyItemEventWork* eventWork  = task->work;
    TmdObject*                          eventModel = task->extra.tmd;

    gfxSetRotIdentity(&eventWork->lightMatrix);
    gfxSetRotIdentity(&eventWork->colorMatrix);

    eventModel->lightMtx = &eventWork->lightMatrix;

    eventWork->colorMatrix.m[0][0] = ONE;
    eventWork->colorMatrix.m[0][1] = ONE;
    eventWork->colorMatrix.m[0][2] = ONE;
    eventWork->colorMatrix.m[1][0] = ONE;
    eventWork->colorMatrix.m[1][1] = ONE;
    eventWork->colorMatrix.m[1][2] = ONE;
    eventWork->colorMatrix.m[2][0] = ONE;
    eventWork->colorMatrix.m[2][1] = ONE;
    eventWork->colorMatrix.m[2][2] = ONE;

    eventWork->lightMatrix.m[0][0] = ONE;
    eventWork->lightMatrix.m[0][1] = ONE;
    eventWork->lightMatrix.m[0][2] = ONE;
    eventWork->lightMatrix.m[1][0] = 0;
    eventWork->lightMatrix.m[1][1] = ONE;
    eventWork->lightMatrix.m[1][2] = ONE;
    eventWork->lightMatrix.m[2][0] = ONE;
    eventWork->lightMatrix.m[2][1] = ONE;
    eventWork->lightMatrix.m[2][2] = 0;

    eventModel->colorMtx = &eventWork->colorMatrix;
    worldCoordSetModelAmbientColor(eventModel, ONE / 2, ONE / 2, ONE / 2);
}

/// Initializes the model-based bottlecap-magnet interaction and its cursor.
///
/// State 0 owns zeroed event work, installs the model event's key-item receiver
/// and retains a separate port-0 cursor child in `spawnArg2.pointer`. Allocation
/// failure kills the model event; cursor-spawn failure is left unchecked.
/// Selects saved view 6, clears both hotspot tables, holds play/HUD presentation
/// and starts the hanging line at rest. The model borrows the work's Q12 light
/// and colour matrices through teardown. Requires loaded model/cursor resources.
static void _dryfieldBreezewayInitializeKeyItemEvent(Task* task)
{
    enum { DRYFIELD_BREEZEWAY_KEY_ITEM_VIEW          = 6,
           DRYFIELD_BREEZEWAY_KEY_ITEM_CURSOR_PORT_0 = 1 };

    TmdObject*                          model;
    GfxCoord*                           rootCoord;
    _DryfieldBreezewayKeyItemEventWork* work;

    model     = task->extra.tmd;
    rootCoord = model->coords;

    work = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        taskKill(task);
        return;
    }

    task->spawnArg2.pointer                                    = taskSpawnFromTable(&D_dryfield_breezeway_80182DC0, 0, DRYFIELD_BREEZEWAY_KEY_ITEM_CURSOR_PORT_0, NULL);
    task->msgTable                                             = D_dryfield_breezeway_80182DCC;
    task->work                                                 = work;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = DRYFIELD_BREEZEWAY_KEY_ITEM_VIEW;
    task->state                                               += 1;
    work->keyItemAccepted                                      = 0;
    displayAcquireMenuHold();

    // Clear both the model-confirmation and cursor-prop hit latches.
    _dryfieldBreezewayClearKeyItemHotspotHits(D_dryfield_breezeway_80182E00);

    _dryfieldBreezewayClearKeyItemHotspotHits(D_dryfield_breezeway_80182DDC);

    model->colorMtx   = &work->colorMatrix;
    model->flags      = 0;
    model->lightMtx   = &work->lightMatrix;
    rootCoord->parent = NULL;

    gGameSession->eventState   = 1;
    gGameSession->cutsceneHold = 1;
    gGameSession->hideHud      = 1;
    work->lineEndX             = 0;
    work->lineEndY             = DRYFIELD_BREEZEWAY_LINE_REST_Y;

    _dryfieldBreezewayInitializeKeyItemLighting(task);
}

/// Waits for confirmation on the key-item model or cancellation of the event.
///
/// Uploads the line artwork once, oscillates the model's yaw with a 256-frame
/// sine period, and updates the line using its rest point as the lead request.
/// A busy caption hides and stops the cursor. Confirmation records the hit
/// hotspot's id and prompt kind and opens the item prompt on the next state;
/// cancel selects the exit state. The task needs its initialized TMD body/work
/// and a live action-prompt slot.
static void _dryfieldBreezewayScanKeyItemHotspot(Task* task)
{
    _DryfieldBreezewayKeyItemEventWork* work;
    ActionPromptHotspot*                hotspot;
    ActionPrompt*                       prompt;
    GfxCoord*                           coord;

    coord   = task->extra.tmd->coords;
    work    = task->work;
    hotspot = D_dryfield_breezeway_80182E00;
    prompt  = D_80114D28;

    // Upload the line artwork once per entry into this state.
    if (task->killCountdown == 0) {
        gpuUploadImages(&D_dryfield_breezeway_80182F24[0]);
        gpuUploadImages(&D_dryfield_breezeway_80183144[0]);
        task->killCountdown = (u16)task->killCountdown + 1;
    }

    gfxSetRotIdentity(&coord->coord);
    RotMatrixY(rsin(gDisplayState.animFrame * 0x10), &coord->coord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    _dryfieldBreezewayUpdateKeyItemLine(task, 0, DRYFIELD_BREEZEWAY_LINE_REST_Y);
    gGameSession->hideHud    = 1;
    gGameSession->eventState = 1;
    if (capIsBusy() != 0) {
        prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
        prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
        return;
    }
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_AIM;
    if (_actionPromptHitTestDefault(hotspot, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = ACTION_PROMPT_MODE_HOTSPOT;
        if ((prompt->buttons.slots[0].state == ACTION_PROMPT_BUTTON_PRESSED) && (hotspot->id != ACTION_PROMPT_HOTSPOT_END)) {
            do {
                if (hotspot->hit != 0) {
                    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
                    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
                    work->hotspotId     = hotspot->id;
                    work->promptKind    = hotspot->promptKind;
                    task->state         = DRYFIELD_BREEZEWAY_KEY_ITEM_STATE_OPEN_PROMPT;
                    return;
                }
                hotspot++;
            } while (hotspot->id != ACTION_PROMPT_HOTSPOT_END);
        }
    } else {
        prompt->mode = ACTION_PROMPT_MODE_IDLE;
    }
    if (prompt->buttons.slots[1].state == ACTION_PROMPT_BUTTON_PRESSED) {
        task->state = DRYFIELD_BREEZEWAY_KEY_ITEM_STATE_EXIT;
    }
}

/// Breathes the room's hanging prop: rebuilds the display object's coordinate
/// matrix as a pure Y rotation of `rsin(gDisplayState.animFrame * 16)` -- one full turn
/// every 256 frames -- off an identity built the same word-at-a-time way
/// `_dryfieldBreezewayInitializeKeyItemEvent` builds the event work's two matrices, then
/// updates the hanging line with `_dryfieldBreezewayUpdateKeyItemLine` using the
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
    _dryfieldBreezewayUpdateKeyItemLine(task, prompt->screen.xy.x, prompt->screen.xy.y);

    if (_actionPromptHitTestDefault(hs, work->lineEndX, work->lineEndY) != 0) {
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

/// Moves and draws the hanging key-item line, placing the task's model at the drawn tip.
///
/// `leadX` and `leadY` are screen-centred pixels used to gate whether the action
/// cursor may lead the free end. Callers pass the cursor or the rest point. An
/// in-reach request and a nearby cursor make the end close a quarter of the
/// cursor gap each frame; otherwise it falls to rest and swings toward centre.
/// Two runs of at most 30 four-pixel segments connect a bend toward the anchor
/// and toward the free end. The second run inherits the first run's edge state.
/// The initialized work, TMD body, prompt and primitive arena must be live.
static void _dryfieldBreezewayUpdateKeyItemLine(Task* task, s16 leadX, s16 leadY)
{
    // Advance to the drawn tip and steer toward the target in wrapped 4096-unit angles.
    // Arguments have no side effects and may be read repeatedly. The angle and combined
    // angle are s16 lvalues, turnError an s32 lvalue, and both points SVECTOR lvalues.
#define DRYFIELD_BREEZEWAY_ADVANCE_LINE_SEGMENT(segmentAngle, segmentStart, segmentTip, targetX, targetY, combinedAngle, turnError)      \
    {                                                                                                                                    \
        (combinedAngle) = (segmentAngle) + _dryfieldBreezewayGetLineBearing((segmentTip).vx, (segmentTip).vy, (targetX), (targetY));     \
        (turnError)     = ((combinedAngle) << DRYFIELD_BREEZEWAY_LINE_SIGNED_ANGLE_SHIFT) >> DRYFIELD_BREEZEWAY_LINE_SIGNED_ANGLE_SHIFT; \
        (segmentStart)  = (segmentTip);                                                                                                  \
        if ((turnError) > DRYFIELD_BREEZEWAY_LINE_TURN_LARGE) {                                                                          \
            (segmentAngle) -= DRYFIELD_BREEZEWAY_LINE_TURN_LARGE;                                                                        \
        } else if ((turnError) > DRYFIELD_BREEZEWAY_LINE_TURN_MEDIUM) {                                                                  \
            (segmentAngle) -= DRYFIELD_BREEZEWAY_LINE_TURN_MEDIUM;                                                                       \
        } else if ((turnError) > DRYFIELD_BREEZEWAY_LINE_TURN_SMALL) {                                                                   \
            (segmentAngle) -= DRYFIELD_BREEZEWAY_LINE_TURN_SMALL;                                                                        \
        } else if ((turnError) < -DRYFIELD_BREEZEWAY_LINE_TURN_LARGE) {                                                                  \
            (segmentAngle) += DRYFIELD_BREEZEWAY_LINE_TURN_LARGE;                                                                        \
        } else if ((turnError) < -DRYFIELD_BREEZEWAY_LINE_TURN_MEDIUM) {                                                                 \
            (segmentAngle) += DRYFIELD_BREEZEWAY_LINE_TURN_MEDIUM;                                                                       \
        } else if ((turnError) < -DRYFIELD_BREEZEWAY_LINE_TURN_SMALL) {                                                                  \
            (segmentAngle) += DRYFIELD_BREEZEWAY_LINE_TURN_SMALL;                                                                        \
        } else {                                                                                                                         \
            (segmentAngle) = -_dryfieldBreezewayGetLineBearing((segmentTip).vx, (segmentTip).vy, (targetX), (targetY));                  \
        }                                                                                                                                \
    }
    s32                                 lineX;
    s32                                 requestedY;
    s32                                 anchorOffsetY;
    SVECTOR                             segmentStart;
    SVECTOR                             segmentTip;
    SVECTOR                             target;
    _DryfieldBreezewayLineEdge          edge;
    _DryfieldBreezewayKeyItemEventWork* work;
    s32                                 leadDistance;
    s32                                 cursorDistance;
    s32                                 cursorDeltaX;
    s32                                 cursorDeltaY;
    s16                                 lineY;
    s16                                 endX;
    s16                                 endY;
    s32                                 bendScale;
    s32                                 bendX;
    s32                                 bendY;
    s32                                 slack;
    s32                                 turnError;
    s32                                 segmentIndex;
    s16                                 anchorBearing;
    s16                                 segmentAngle;
    s16                                 combinedAngle;

    ActionPrompt* prompt;

    lineX          = leadX;
    requestedY     = leadY;
    work           = task->work;
    prompt         = D_80114D28;
    anchorOffsetY  = requestedY - DRYFIELD_BREEZEWAY_LINE_ANCHOR_Y;
    leadDistance   = SquareRoot0(lineX * lineX + anchorOffsetY * anchorOffsetY);
    endX           = work->lineEndX;
    cursorDeltaX   = endX - prompt->screen.xy.x;
    endY           = work->lineEndY;
    cursorDeltaY   = endY - prompt->screen.xy.y;
    cursorDistance = SquareRoot0(cursorDeltaX * cursorDeltaX + cursorDeltaY * cursorDeltaY);
    if (leadDistance >= DRYFIELD_BREEZEWAY_LINE_REACH || requestedY < DRYFIELD_BREEZEWAY_LINE_ANCHOR_Y + 1 || cursorDistance > DRYFIELD_BREEZEWAY_LINE_GRAB_RADIUS) {
        // Nothing leads the free end: it drops to its rest height and swings about the centre.
        prompt->mode    = ACTION_PROMPT_MODE_IDLE;
        work->lineEndY += work->fallSpeed;
        leadDistance    = DRYFIELD_BREEZEWAY_LINE_REACH;
        if (work->lineEndY >= DRYFIELD_BREEZEWAY_LINE_REST_Y) {
            work->lineEndY  = DRYFIELD_BREEZEWAY_LINE_REST_Y;
            work->fallSpeed = 0;
        } else {
            work->fallSpeed++;
        }
        if (work->swingDamping < DRYFIELD_BREEZEWAY_LINE_SWING_DAMPING_STILL) {
            work->previousLineEndX = work->lineEndX;
            // Narrow the intermediate sum to s16 before applying the damping adjustment.
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

    // Choose the bend from the end height and slack, using a 12-bit fractional scale.
    lineY         = work->lineEndY;
    lineX         = work->lineEndX;
    anchorOffsetY = lineY - DRYFIELD_BREEZEWAY_LINE_ANCHOR_Y;
    bendScale     = (ONE / 2) - (anchorOffsetY << DRYFIELD_BREEZEWAY_LINE_SCALE_FRACTION_BITS) / (DRYFIELD_BREEZEWAY_LINE_REACH * 2);
    bendScale     = (ONE * 7 / 8) - bendScale;
    endX          = work->lineEndX;
    endY          = work->lineEndY;
    slack         = DRYFIELD_BREEZEWAY_LINE_REACH - leadDistance;
    // Keep the division before the shift: negative products round at each stage.
    bendX  = (lineX * bendScale / 8) >> 9;
    bendY  = ((anchorOffsetY * bendScale / 8) >> 9) + ((slack * bendScale / 8) >> 9);
    bendY += DRYFIELD_BREEZEWAY_LINE_ANCHOR_Y;

    // Draw from the bend toward the anchor, then from the bend toward the free end.
    target.vx       = DRYFIELD_BREEZEWAY_LINE_ANCHOR_X;
    target.vy       = DRYFIELD_BREEZEWAY_LINE_ANCHOR_Y;
    target.vz       = 0;
    segmentStart.vx = bendX;
    segmentStart.vy = bendY;
    segmentStart.vz = 0;
    anchorBearing   = _dryfieldBreezewayGetLineBearing(DRYFIELD_BREEZEWAY_LINE_ANCHOR_X, DRYFIELD_BREEZEWAY_LINE_ANCHOR_Y, bendX, bendY);
    segmentAngle    = -((_dryfieldBreezewayGetLineBearing(bendX, bendY, lineX, lineY) + anchorBearing) / 2) + ACTOR_TRANSFORM_ANGLE_HALF_TURN;
    for (segmentIndex = 0; segmentIndex < DRYFIELD_BREEZEWAY_LINE_MAX_SEGMENTS_PER_HALF; segmentIndex++) {
        if (segmentIndex == 0) {
            edge.joined = 0;
        } else {
            edge.joined = 1;
        }
        _dryfieldBreezewayDrawKeyItemLineSegment(segmentAngle, DRYFIELD_BREEZEWAY_LINE_SEGMENT_LENGTH, &segmentStart, &segmentTip, &edge);
        if (_dryfieldBreezewayIsLinePointNearTarget(&target, &segmentTip) != 0) {
            break;
        }
        DRYFIELD_BREEZEWAY_ADVANCE_LINE_SEGMENT(segmentAngle, segmentStart, segmentTip, DRYFIELD_BREEZEWAY_LINE_ANCHOR_X, DRYFIELD_BREEZEWAY_LINE_ANCHOR_Y, combinedAngle, turnError);
    }

    target.vx       = endX;
    target.vy       = endY;
    target.vz       = 0;
    segmentStart.vx = bendX;
    segmentStart.vy = bendY;
    segmentStart.vz = 0;
    anchorBearing   = _dryfieldBreezewayGetLineBearing(DRYFIELD_BREEZEWAY_LINE_ANCHOR_X, DRYFIELD_BREEZEWAY_LINE_ANCHOR_Y, bendX, bendY);
    segmentAngle    = -((_dryfieldBreezewayGetLineBearing(bendX, bendY, endX, endY) + anchorBearing) / 2);
    for (segmentIndex = 0; segmentIndex < DRYFIELD_BREEZEWAY_LINE_MAX_SEGMENTS_PER_HALF; segmentIndex++) {
        _dryfieldBreezewayDrawKeyItemLineSegment(segmentAngle, DRYFIELD_BREEZEWAY_LINE_SEGMENT_LENGTH, &segmentStart, &segmentTip, &edge);
        if (_dryfieldBreezewayIsLinePointNearTarget(&target, &segmentTip) != 0) {
            break;
        }
        DRYFIELD_BREEZEWAY_ADVANCE_LINE_SEGMENT(segmentAngle, segmentStart, segmentTip, endX, endY, combinedAngle, turnError);
    }
    _dryfieldBreezewayPlaceKeyItemModelAtLineTip(task, segmentTip.vx, segmentTip.vy);
}
#undef DRYFIELD_BREEZEWAY_ADVANCE_LINE_SEGMENT

/// Draws one textured segment of the key-item line and returns its far-edge centre.
///
/// `angle` is a Z rotation in 4096 units per turn; `length` is pixels along the
/// rotated +Y axis. `start` and `tipOut` are screen-centred points. With
/// `edge->joined` zero the quad starts from its own near edge; otherwise it uses
/// the saved corners. Each call overwrites the saved far corners, but does not
/// change `joined`. Only the corners' X/Y components are used. One POLY_FT4 is
/// reserved from the live primitive arena and linked at the line's fixed depth.
static void _dryfieldBreezewayDrawKeyItemLineSegment(s16 angle, s16 length, const SVECTOR* start, SVECTOR* tipOut, _DryfieldBreezewayLineEdge* edge)
{
    // Build an XY point and transform it through the installed GTE matrices.
    // localPoint is evaluated four times; arguments must have no side effects.
    // The input/output vectors and long flags word are borrowed writable storage.
#define DRYFIELD_BREEZEWAY_ROTATE_LINE_POINT(localPoint, x, y, rotatedPoint, flags) \
    {                                                                               \
        (localPoint)->vx = (x);                                                     \
        (localPoint)->vy = (y);                                                     \
        (localPoint)->vz = 0;                                                       \
        RotTransSV((localPoint), (rotatedPoint), (flags));                          \
    }
    SVECTOR   localTip;
    SVECTOR   rotatedTip;
    SVECTOR   localNearLeft;
    SVECTOR   localNearRight;
    SVECTOR   localFarLeft;
    SVECTOR   localFarRight;
    SVECTOR   rotatedNearLeft;
    SVECTOR   rotatedNearRight;
    SVECTOR   rotatedFarLeft;
    SVECTOR   rotatedFarRight;
    MATRIX    rotation;
    long      transformFlags;
    POLY_FT4* quad;

    gfxSetRotIdentity(&rotation);
    rotation.t[0] = 0;
    rotation.t[1] = 0;
    rotation.t[2] = 0;
    RotMatrixZ(angle, &rotation);
    SetRotMatrix(&rotation);
    SetTransMatrix(&rotation);

    DRYFIELD_BREEZEWAY_ROTATE_LINE_POINT(&localTip, 0, length, &rotatedTip, &transformFlags);
    tipOut->vx = start->vx + rotatedTip.vx;
    tipOut->vy = start->vy + rotatedTip.vy;
    tipOut->vz = start->vz + rotatedTip.vz;

    if (edge->joined == 0) {
        DRYFIELD_BREEZEWAY_ROTATE_LINE_POINT(&localNearLeft, -DRYFIELD_BREEZEWAY_LINE_HALF_WIDTH, 0, &rotatedNearLeft, &transformFlags);
        DRYFIELD_BREEZEWAY_ROTATE_LINE_POINT(&localNearRight, DRYFIELD_BREEZEWAY_LINE_HALF_WIDTH, 0, &rotatedNearRight, &transformFlags);
    }

    DRYFIELD_BREEZEWAY_ROTATE_LINE_POINT(&localFarLeft, -DRYFIELD_BREEZEWAY_LINE_HALF_WIDTH, length, &rotatedFarLeft, &transformFlags);
    DRYFIELD_BREEZEWAY_ROTATE_LINE_POINT(&localFarRight, DRYFIELD_BREEZEWAY_LINE_HALF_WIDTH, length, &rotatedFarRight, &transformFlags);

    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setPolyFT4(quad);
    quad->tpage = DRYFIELD_BREEZEWAY_LINE_TEXTURE_PAGE;
    quad->clut  = DRYFIELD_BREEZEWAY_LINE_CLUT;

    if (edge->joined == 0) {
        quad->x0 = rotatedNearLeft.vx + start->vx;
        quad->y0 = rotatedNearLeft.vy + start->vy;
        quad->x1 = rotatedNearRight.vx + start->vx;
        quad->y1 = rotatedNearRight.vy + start->vy;
    } else {
        quad->x0 = edge->left.vx;
        quad->y0 = edge->left.vy;
        quad->x1 = edge->right.vx;
        quad->y1 = edge->right.vy;
    }
    quad->x2 = rotatedFarLeft.vx + start->vx;
    quad->y2 = rotatedFarLeft.vy + start->vy;
    quad->x3 = rotatedFarRight.vx + start->vx;
    quad->y3 = rotatedFarRight.vy + start->vy;

    quad->u0 = 0;
    quad->v0 = 0;
    quad->u1 = DRYFIELD_BREEZEWAY_LINE_TEXTURE_U_END;
    quad->v1 = 0;
    quad->u2 = 0;
    quad->v2 = DRYFIELD_BREEZEWAY_LINE_TEXTURE_V_END;
    quad->u3 = DRYFIELD_BREEZEWAY_LINE_TEXTURE_U_END;
    quad->v3 = DRYFIELD_BREEZEWAY_LINE_TEXTURE_V_END;

    setShadeTex(quad, 1);
    addPrim(&gGpuCurrentOt[DRYFIELD_BREEZEWAY_LINE_ORDERING_TABLE_INDEX], quad);

    edge->left.vx  = rotatedFarLeft.vx + start->vx;
    edge->left.vy  = rotatedFarLeft.vy + start->vy;
    edge->right.vx = rotatedFarRight.vx + start->vx;
    edge->right.vy = rotatedFarRight.vy + start->vy;
}
#undef DRYFIELD_BREEZEWAY_ROTATE_LINE_POINT

#include "../../shared/action_prompt_move_cursors.inc.c"

#include "../../shared/action_prompt_draw_cursor.inc.c"

/// Runs the key-item event's cursor task: reset at state 0, move/draw at state 1.
///
/// The event owns this task through its spawn handle. `task->state` must be 0
/// or 1; the shared reset advances to 1 and movement leaves it there.
static void _dryfieldBreezewayActionPromptTask(Task* task)
{
    TaskFunc states[] = { _actionPromptResetDefault, _actionPromptMoveCursorsDefault };

    states[task->state](task);
}

/// Returns 1 when a line point is less than nine screen pixels from its target, else 0.
///
/// Only X/Y participate. Each difference narrows to s16 before squaring; the
/// integer square root is compared, so the truncation is part of the test.
static s16 _dryfieldBreezewayIsLinePointNearTarget(const SVECTOR* target, const SVECTOR* point)
{
    s16 dx = point->vx - target->vx;
    s16 dy = point->vy - target->vy;

    return SquareRoot0((dx * dx) + (dy * dy)) < DRYFIELD_BREEZEWAY_LINE_TARGET_DISTANCE_LIMIT;
}

/// Places the key-item model at a screen-centred line tip at fixed view depth.
///
/// `tipX` and `tipY` are pixels. The TMD root translation is set to depth 1500,
/// with X/Y scaled by 1500/680 and truncated toward zero. Its composed matrix
/// is marked dirty; the existing rotation and parent are retained.
static void _dryfieldBreezewayPlaceKeyItemModelAtLineTip(Task* task, s16 tipX, s16 tipY)
{
    GfxCoord* coord = task->extra.tmd->coords;

    enum { DRYFIELD_BREEZEWAY_KEY_ITEM_MODEL_DEPTH      = 1500,
           DRYFIELD_BREEZEWAY_KEY_ITEM_PROJECTION_SCALE = 680 };

    coord->coord.t[2]   = DRYFIELD_BREEZEWAY_KEY_ITEM_MODEL_DEPTH;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    coord->coord.t[0]   = (tipX * DRYFIELD_BREEZEWAY_KEY_ITEM_MODEL_DEPTH) / DRYFIELD_BREEZEWAY_KEY_ITEM_PROJECTION_SCALE;
    coord->coord.t[1]   = (tipY * DRYFIELD_BREEZEWAY_KEY_ITEM_MODEL_DEPTH) / DRYFIELD_BREEZEWAY_KEY_ITEM_PROJECTION_SCALE;
}

/// Answers a key-item-use query by latching whether the bottlecap magnet was offered.
///
/// `itemId` is the collected-item catalogue id; the other payload and message id
/// are unused. Returns the item menu's used-notice reply for the magnet and the
/// refused reply otherwise, without consuming inventory. The initialized event
/// work holds the answer until the item-prompt state checks it.
static s32 _dryfieldBreezewayUseBottlecapMagnet(Task* task, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg)
{
    _DryfieldBreezewayKeyItemEventWork* work = task->work;

    if (itemId == INVENTORY_COLLECTION_ID_BOTTLECAP_MAGNET) {
        work->keyItemAccepted = true;
        return ROOM_KEY_ITEM_USE_SHOW_USED_NOTICE;
    }
    work->keyItemAccepted = false;
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Returns the screen-plane bearing from one point to another, in 4096 units per turn.
///
/// Coordinates are screen-centred pixels. X/Y differences narrow to s16 before
/// normalization. Zero angle is +Y and positive quarter-turn is +X; a coincident
/// pair follows the SDK's zero-vector normalization and angle behavior.
static s16 _dryfieldBreezewayGetLineBearing(s16 fromX, s16 fromY, s16 toX, s16 toY)
{
    SVECTOR direction;

    direction.vx = toX - fromX;
    direction.vy = toY - fromY;
    direction.vz = 0;
    VectorNormalSS(&direction, &direction);
    return ratan2(direction.vx, direction.vy);
}

/// Runs the model-and-line interaction used to offer the bottlecap magnet to the room.
///
/// Starts with a TMD body at state 0. The seven handlers initialize the model,
/// arm and scan its prompt, open and wait for item commands, exit, or let the
/// accepted magnet lead the line toward a prop hotspot. States must remain
/// within 0..6. The event owns its work; it borrows the live action-prompt slot.
static void _dryfieldBreezewayKeyItemEventTask(Task* task)
{
    TaskFuncTable7 stateHandlers;

    stateHandlers = D_dryfield_breezeway_8017D5E8;
    stateHandlers.funcs[task->state](task);
}

#include "../../shared/action_prompt_hit_test.inc.c"

/// Arms the key-item event's cursor at screen centre and advances to model selection.
///
/// Resets the aiming speed, idle cursor, X/Y and one-time upload latch. The cursor
/// task later moves these coordinates; the item menu copies them when opening
/// the hotspot commands.
static void _dryfieldBreezewayArmKeyItemPrompt(Task* task)
{
    ActionPrompt* prompt = D_80114D28;

    prompt->cursorSpeed = ACTION_PROMPT_SPEED_AIM;
    prompt->mode        = ACTION_PROMPT_MODE_IDLE;
    prompt->screen.xy.x = 0;
    prompt->screen.xy.y = 0;
    task->killCountdown = 0;
    task->state         = task->state + 1;
}

/// Opens the confirmed model hotspot's item commands and waits for their answer.
///
/// Keeps the hanging line at rest, hides/stops the cursor, and opens the menu
/// at its current screen-pixel position with the hotspot's prompt kind. Requires
/// initialized event work and a live action-prompt slot; selects the item-wait state.
static void _dryfieldBreezewayOpenKeyItemCommands(Task* task)
{
    ActionPrompt*                       prompt = D_80114D28;
    _DryfieldBreezewayKeyItemEventWork* work   = task->work;

    _dryfieldBreezewayUpdateKeyItemLine(task, 0, DRYFIELD_BREEZEWAY_LINE_REST_Y);
    prompt->mode        = ACTION_PROMPT_MODE_HIDDEN;
    prompt->cursorSpeed = ACTION_PROMPT_SPEED_STOPPED;
    itemMenuOpenHotspotCommands(prompt->screen.xy.x, prompt->screen.xy.y, work->promptKind);
    task->state = DRYFIELD_BREEZEWAY_KEY_ITEM_STATE_WAIT_FOR_ITEM;
}

/// Closes whatever the hotspot scan left up and picks the room's next state:
/// updates the line through `_dryfieldBreezewayUpdateKeyItemLine` using the rest point and clears the
/// prompt's highlight state as the arm above does, then interrogates the
/// gameplay side. If `itemMenuIsHotspotActionConfirmed` reports that the
/// Examine/Push row was accepted, it starts cap slot 7 and returns to state 2.
/// Otherwise `_DryfieldBreezewayKeyItemEventWork::keyItemAccepted`, written by
/// `_dryfieldBreezewayUseBottlecapMagnet`, selects state 6 for an accepted key item
/// or state 2 to resume scanning.
static void func_dryfield_breezeway_8017FE08(Task* task)
{
    ActionPrompt*                       prompt = D_80114D28;
    _DryfieldBreezewayKeyItemEventWork* work   = task->work;
    s32                                 state;

    _dryfieldBreezewayUpdateKeyItemLine(task, 0, DRYFIELD_BREEZEWAY_LINE_REST_Y);
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

void dryfieldBreezewayAmbientEffectsTask(Task* task)
{
    enum {
        DRYFIELD_BREEZEWAY_DIAMOND_GLOW_VIEWS     = (1 << 3) | (1 << 4),
        DRYFIELD_BREEZEWAY_RAY_GLOW_VIEW          = 1 << 5,
        DRYFIELD_BREEZEWAY_GLOW_PULSE_RATE        = 0x600,
        DRYFIELD_BREEZEWAY_DIAMOND_GLOW_SCALE     = 128,
        DRYFIELD_BREEZEWAY_RAY_GLOW_SCALE         = 16,
        DRYFIELD_BREEZEWAY_PARTICLE_APPROACH_VIEW = 2,
        DRYFIELD_BREEZEWAY_PARTICLE_NEAR_VIEW     = 3,
        DRYFIELD_BREEZEWAY_PARTICLE_MIN_SIZE      = 64,
        DRYFIELD_BREEZEWAY_PARTICLE_END_TICKS     = 16,
        DRYFIELD_BREEZEWAY_SOUND_NOT_STARTED      = 0,
        DRYFIELD_BREEZEWAY_SOUND_LOOP_STARTED     = 1,
        DRYFIELD_BREEZEWAY_SOUND_BURST_STARTED    = 2,
    };

    s32         viewMask;
    EffectWork* work;
    GfxCoord*   coord;
    GfxCoord*   playerCoord;
    s32         spawnChance;
    s32         panOffset;

    viewMask    = 1 << gGameSession->location.loc.view;
    work        = task->spawnArg2.pointer;
    coord       = task->extra.coordBody->coord;
    playerCoord = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
    // Draw the fixed glow before the control gate; paused emission still draws it.
    if (viewMask & DRYFIELD_BREEZEWAY_DIAMOND_GLOW_VIEWS) {
        _dryfieldBreezewayDrawRedDiamondGlow(coord, &D_dryfield_breezeway_80183164, DRYFIELD_BREEZEWAY_GLOW_PULSE_RATE, DRYFIELD_BREEZEWAY_DIAMOND_GLOW_SCALE);
    } else if (viewMask & DRYFIELD_BREEZEWAY_RAY_GLOW_VIEW) {
        _glowDrawRayStar(coord, &D_dryfield_breezeway_80183164, DRYFIELD_BREEZEWAY_GLOW_PULSE_RATE, DRYFIELD_BREEZEWAY_RAY_GLOW_SCALE);
    }
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        return;
    }
    gRoomEffectState->roomEffectMode = ROOM_EFFECT_VIEW_ENABLED;
    if (gameFlagGetNibble(GAME_FLAG_BREEZEWAY_FIRST_EVENT_SEEN) == 0) {
        // Offsets use this task's placement frame; work->move stages each spawn position.
        if (gGameSession->location.loc.view == DRYFIELD_BREEZEWAY_PARTICLE_APPROACH_VIEW) {
            spawnChance     = (playerCoord->coord.t[0] - 5856) >> 7;
            work->move.vx   = 12000;
            work->move.vy   = -3000;
            work->move.vz   = 3000;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if ((u16)((gRandomLcgState >> 16) % 100) < spawnChance) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                effectSpawn(EFFECT_DRYFIELD_BREEZEWAY_BOUNCING_PARTICLE, coord, (s32)(gRandomLcgState >> 16) % spawnChance + DRYFIELD_BREEZEWAY_PARTICLE_MIN_SIZE, &work->move);
            }
            if (work->step == DRYFIELD_BREEZEWAY_SOUND_NOT_STARTED) {
                sndEvtRequestScriptStart(SOUND_BREEZEWAY_EFFECT_LOOP, 0, 0);
                work->step = DRYFIELD_BREEZEWAY_SOUND_LOOP_STARTED;
            }
        } else if (gGameSession->location.loc.view == DRYFIELD_BREEZEWAY_PARTICLE_NEAR_VIEW) {
            work->scale     = DRYFIELD_BREEZEWAY_PARTICLE_END_TICKS;
            work->move.vx   = playerCoord->coord.t[0] + 0x100;
            work->move.vy   = -3000;
            work->move.vz   = 3000;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            effectSpawn(EFFECT_DRYFIELD_BREEZEWAY_BOUNCING_PARTICLE, coord, ((gRandomLcgState >> 16) & 0x7F) + DRYFIELD_BREEZEWAY_PARTICLE_MIN_SIZE, &work->move);
            if (work->step < DRYFIELD_BREEZEWAY_SOUND_BURST_STARTED) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                if (!((gRandomLcgState >> 16) & 3)) {
                    panOffset = (s8)worldCoordGetOriginAudioPan(coord);
                    sndEvtRequestScriptStart(SOUND_BREEZEWAY_EFFECT_BURST, panOffset, (s8)worldCoordGetOriginAudioDepth(coord));
                    work->step = DRYFIELD_BREEZEWAY_SOUND_BURST_STARTED;
                }
            }
        }
    } else if (gameFlagGetNibble(GAME_FLAG_BREEZEWAY_FIRST_EVENT_SEEN) == 1) {
        // Once the encounter is seen, stop the loop and drain the final paired emission.
        if (work->step != DRYFIELD_BREEZEWAY_SOUND_NOT_STARTED) {
            sndEvtRequestScriptStop(SOUND_BREEZEWAY_EFFECT_LOOP, SOUND_SCRIPT_STOP_NO_FADE);
            work->step = DRYFIELD_BREEZEWAY_SOUND_NOT_STARTED;
        }
        if (work->scale != 0) {
            work->scale--;
            work->move.vx   = 16000;
            work->move.vy   = -3000;
            work->move.vz   = 2750;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            effectSpawn(EFFECT_DRYFIELD_BREEZEWAY_BOUNCING_PARTICLE, coord, ((gRandomLcgState >> 16) & 0xFF) + DRYFIELD_BREEZEWAY_PARTICLE_MIN_SIZE, &work->move);
            work->move.vx   = 17000;
            work->move.vy   = -3000;
            work->move.vz   = 4000;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            effectSpawn(EFFECT_DRYFIELD_BREEZEWAY_BOUNCING_PARTICLE, coord, ((gRandomLcgState >> 16) & 0xFF) + DRYFIELD_BREEZEWAY_PARTICLE_MIN_SIZE, &work->move);
        }
    }
}

/// Reserves and initializes a diamond half with a red centre and black rim.
///
/// Requires a word-aligned frame cursor with space for one `POLY_G4`; advances
/// it by that packet's full extent without a capacity check. Vertex 2 takes the
/// low red byte and the other vertices are black. The returned packet belongs
/// to the frame arena and must stay live through GPU consumption; the caller
/// supplies coordinates, ordering-table linkage and blending.
static inline POLY_G4* _dryfieldBreezewayAllocateDiamondHalf(s32 redIntensity)
{
    POLY_G4* quad;

    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setPolyG4(quad);
    setRGB0(quad, 0, 0, 0);
    setRGB1(quad, 0, 0, 0);
    setRGB2(quad, redIntensity, 0, 0);
    setRGB3(quad, 0, 0, 0);
    return quad;
}

/// Reserves a glow diagonal fading from a red centre to two black endpoints.
///
/// Requires a word-aligned frame cursor with space for one `LINE_G3`; advances
/// it by that packet's full extent without a capacity check. Vertex 1 takes the
/// low red byte; the endpoints are black and the polyline terminator is set.
/// The returned packet belongs to the frame arena and must stay live through
/// GPU consumption; the caller supplies coordinates, sorting and blending.
static inline LINE_G3* _dryfieldBreezewayAllocateGlowDiagonal(s32 redIntensity)
{
    LINE_G3* diagonal;

    diagonal       = gGpuPrimCursor;
    gGpuPrimCursor = diagonal + 1;
    setLineG3(diagonal);
    setRGB0(diagonal, 0, 0, 0);
    setRGB1(diagonal, redIntensity, 0, 0);
    setRGB2(diagonal, 0, 0, 0);
    return diagonal;
}

/// Draws an additive pulsing red diamond and two diagonals around a local point.
///
/// Composes `coord`, transforms the borrowed point to world space, and projects
/// it through the view. `pulseRate` is in 4096 angle units per animation frame;
/// `radiusScale` gives a pixel half-extent of scale * 32 / (camera Z / 4).
/// Depths below 17 emit nothing. The second diagonal extends twice as far as
/// the diamond. Requires composed view matrices, scratch-stack space and room
/// for four packets plus blend commands in the current frame arena/ordering table.
static void _dryfieldBreezewayDrawRedDiamondGlow(GfxCoord* coord, const SVECTOR* localPoint, s16 pulseRate, s16 radiusScale)
{
    RoomGlowSpriteScratch* block;
    POLY_G4*               quad;
    LINE_G3*               diagonal;
    s32                    partIndex;
    s32                    redIntensity;
    s32                    pulseSine;
    s32                    verticalSide;
    s32                    xRadiusMultiple;
    s32                    yRadiusMultiple;

    actorRenderComposeCoord(coord);
    block = SCRATCH_STACK_RESERVE_BLOCK(RoomGlowSpriteScratch);

    // Transform the local centre, narrowing the resulting world point to halfwords.
    gte_SetRotMatrix(&coord->workm);
    gte_ldv0(localPoint);
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
    if (block->otz >= GLOW_MIN_DEPTH) {
        pulseSine         = rsin(gDisplayState.animFrame * pulseRate);
        partIndex         = 0;
        block->halfExtent = (radiusScale * GLOW_DIAMOND_RADIUS_SCALE) / block->otz;
        redIntensity      = pulseSine / GLOW_PULSE_DIVISOR + GLOW_PULSE_BASE_INTENSITY;
        do {
            quad     = _dryfieldBreezewayAllocateDiamondHalf(redIntensity);
            quad->x0 = block->screenPos.vx - block->halfExtent;
            quad->x1 = quad->x2 = block->screenPos.vx;
            quad->x3            = block->screenPos.vx + block->halfExtent;
            quad->y0 = quad->y2 = quad->y3 = block->screenPos.vy;
            verticalSide                   = partIndex << 1;
            quad->y1                       = (block->screenPos.vy - block->halfExtent) + block->halfExtent * verticalSide;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
            gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, block->otz);
            partIndex++;
        } while (partIndex < 2);

        // The longer second diagonal gives the glow its asymmetric rays.
        partIndex = 0;
        do {
            diagonal        = _dryfieldBreezewayAllocateGlowDiagonal(redIntensity);
            xRadiusMultiple = partIndex * 3 - 1;
            yRadiusMultiple = partIndex + 1;
            diagonal->x0    = block->screenPos.vx + (block->halfExtent * xRadiusMultiple);
            diagonal->y0    = block->screenPos.vy - (block->halfExtent * yRadiusMultiple);
            diagonal->x1    = block->screenPos.vx;
            diagonal->y1    = block->screenPos.vy;
            diagonal->x2    = block->screenPos.vx - (block->halfExtent * xRadiusMultiple);
            diagonal->y2    = block->screenPos.vy + (block->halfExtent * yRadiusMultiple);
            addPrim((&gGpuCurrentOt[((u32)block->otz << gDisplayState.otDepthShift) >> 4 & (GPU_ORDERING_TABLE_DEPTH_BYTE_MASK >> 2)]),
                    diagonal);
            gpuSetPrimitiveBlendMode(diagonal, GPU_BLEND_ADD, block->otz);
            partIndex = yRadiusMultiple;
        } while (partIndex < 2);
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomGlowSpriteScratch);
}

#define GLOW_DRAW_RAY_STAR_OUTER(p, c, h) setRGB2(p, h, 0, 0)
#define GLOW_DRAW_RAY_STAR_INNER(p, c, h) setRGB2(p, c, 0, 0)
#define GLOW_DRAW_RAY_STAR_RAY(p, c)      setRGB2(p, c, 0, 0)
#define GLOW_DRAW_RAY_STAR_RAY_HALFWORD   1
#include "../../shared/glow_draw_ray_star.inc.c"

void dryfieldBreezewayBouncingParticleTask(Task* task)
{
    EffectWork* work  = task->spawnArg2.pointer;
    GfxCoord*   coord = task->extra.coordBody->coord;
    SVECTOR     localStep;
    SVECTOR     viewTarget;
    SVECTOR     viewStartOrNormal;
    u8          fadeRgb[3];

/// Applies a Q12 direction-times-speed step and writes its XYZ for collision undo.
///
/// Arguments must be side-effect-free work, coordinate and writable SVECTOR
/// pointers. Repeated evaluation preserves the same live objects throughout.
/// Leaves coordinate composition to the caller; changes the GTE vector registers.
/// Expands to statements; invoke only in a braced block.
#define DRYFIELD_BREEZEWAY_ADVANCE_PARTICLE(effectWork, particleCoord, stepOut) \
    gte_lddp((effectWork)->scale);                                              \
    gte_ldsv(&(effectWork)->move);                                              \
    gte_gpf12();                                                                \
    gte_stsv(stepOut);                                                          \
    (particleCoord)->coord.t[0] += (stepOut)->vx;                               \
    (particleCoord)->coord.t[1] += (stepOut)->vy;                               \
    (particleCoord)->coord.t[2] += (stepOut)->vz

/// Draws the age-dependent fade or releases the particle at its lifetime limit.
///
/// Arguments must be side-effect-free task/work pointers and a writable RGB
/// byte array of length three. The drawer consumes the bytes synchronously.
/// Expands to an if/else chain; invoke only in a braced block. Teardown ends
/// the task/work lifetime, so callers must not use them afterward.
#define DRYFIELD_BREEZEWAY_DRAW_OR_RETIRE_PARTICLE(particleTask, effectWork, rgbOut) \
    if ((effectWork)->age < DRYFIELD_BREEZEWAY_PARTICLE_FADE_START) {                \
        _dryfieldBreezewayDrawBouncingParticle((particleTask), NULL);                \
    } else if ((effectWork)->age < DRYFIELD_BREEZEWAY_PARTICLE_LIFETIME) {           \
        (rgbOut)[0] = (rgbOut)[1] = (rgbOut)[2] =                                    \
            (DRYFIELD_BREEZEWAY_PARTICLE_LIFETIME - (effectWork)->age) *             \
            DRYFIELD_BREEZEWAY_PARTICLE_FADE_STEP;                                   \
        _dryfieldBreezewayDrawBouncingParticle((particleTask), (rgbOut));            \
    } else {                                                                         \
        effectKillTask((effectWork), (particleTask));                                \
    }

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
        case DRYFIELD_BREEZEWAY_PARTICLE_INITIALIZE:
            // Choose independent texture timing, spin and a Q12 launch direction.
            gfxSetRotIdentity(&coord->coord);
            work->pos.vx    = (u16)task->spawnArg1.value & DRYFIELD_BREEZEWAY_PARTICLE_SIZE_MASK;
            work->scale     = DRYFIELD_BREEZEWAY_PARTICLE_INITIAL_SPEED;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->pos.vy    = (gRandomLcgState >> 16) & DRYFIELD_BREEZEWAY_PARTICLE_FRAME_PERIOD_MASK;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->index     = (gRandomLcgState >> 16) & (DRYFIELD_BREEZEWAY_PARTICLE_FRAME_COUNT - 1);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->pos.vz    = (gRandomLcgState >> 16) & ACTOR_TRANSFORM_ANGLE_MASK;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->period    = 0x200 - ((gRandomLcgState >> 16) & 0x3FF);
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
            task->state         = DRYFIELD_BREEZEWAY_PARTICLE_FLYING;
            break;
        case DRYFIELD_BREEZEWAY_PARTICLE_FLYING:
            if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                work->age--;
            } else {
                work->pos.vz += work->period;
                if (work->pos.vy != 0 && work->age % work->pos.vy == 0) {
                    work->index++;
                }
                DRYFIELD_BREEZEWAY_ADVANCE_PARTICLE(work, coord, &localStep);
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                // Probe the attempted step in view space; the start also receives the room normal.
                gte_SetRotMatrix(&gGfxViewCoord.workm);
                gte_ldv0(&localStep);
                gte_rtv0();
                gte_stsv(&viewTarget);
                viewStartOrNormal.vx = coord->workm.t[0];
                viewStartOrNormal.vy = coord->workm.t[1];
                viewStartOrNormal.vz = coord->workm.t[2];
                viewTarget.vx       += viewStartOrNormal.vx;
                viewTarget.vy       += viewStartOrNormal.vy;
                viewTarget.vz       += viewStartOrNormal.vz;
                if (worldCollisionProbeGridSegment(&viewTarget, &viewStartOrNormal, &viewTarget, &viewStartOrNormal) == 1) {
                    // Undo the rejected step, blend the normal into direction, and retry at half speed.
                    coord->coord.t[0] -= localStep.vx;
                    coord->coord.t[1] -= localStep.vy;
                    coord->coord.t[2] -= localStep.vz;
                    work->move.vx      = (viewStartOrNormal.vx >> 1) + (work->move.vx >> 1);
                    work->move.vy      = viewStartOrNormal.vy + (work->move.vy >> 1);
                    work->move.vz      = (viewStartOrNormal.vz >> 1) + (work->move.vz >> 1);
                    VectorNormalSS(&work->move, &work->move);
                    work->scale  = work->scale >> 1;
                    work->period = work->period >> 1;
                    DRYFIELD_BREEZEWAY_ADVANCE_PARTICLE(work, coord, &localStep);
                    if (work->age < DRYFIELD_BREEZEWAY_PARTICLE_LIFETIME) {
                        effectSpawn(EFFECT_DUST_PUFF, coord, work->pos.vx + DRYFIELD_BREEZEWAY_PARTICLE_DUST_ARG_BIAS, NULL);
                    }
                    if (work->age - work->step < DRYFIELD_BREEZEWAY_PARTICLE_SETTLE_INTERVAL && work->scale < DRYFIELD_BREEZEWAY_PARTICLE_SETTLE_SPEED) {
                        task->state = DRYFIELD_BREEZEWAY_PARTICLE_SETTLED;
                    } else {
                        work->step = work->age;
                    }
                } else if (work->scale > 0) {
                    work->move.vy += DRYFIELD_BREEZEWAY_PARTICLE_GRAVITY_Q12 / work->scale;
                }
            }
            DRYFIELD_BREEZEWAY_DRAW_OR_RETIRE_PARTICLE(task, work, fadeRgb);
            break;
        case DRYFIELD_BREEZEWAY_PARTICLE_SETTLED:
            if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                work->age--;
            }
            DRYFIELD_BREEZEWAY_DRAW_OR_RETIRE_PARTICLE(task, work, fadeRgb);
            break;
    }
#undef DRYFIELD_BREEZEWAY_ADVANCE_PARTICLE
#undef DRYFIELD_BREEZEWAY_DRAW_OR_RETIRE_PARTICLE
}

/// Draws the particle's spinning eight-frame billboard with an optional fade tint.
///
/// The task needs a composed coordinate body and its effect work. `pos.vx`
/// supplies the size scale and `pos.vz` the angle (4096 units per turn); texture
/// frames wrap modulo eight. NULL `rgb` selects unmodulated texture colour;
/// three borrowed RGB bytes select a tinted semitransparent quad. Negative GTE
/// flags emit nothing; accepted projections must have nonzero depth. Scratch
/// storage is released before return; the packet belongs to the current frame.
static void _dryfieldBreezewayDrawBouncingParticle(Task* task, const u8 rgb[3])
{
    ModelObjectCoordBody* coordBody = task->extra.coordBody;
    EffectWork*           work      = task->spawnArg2.pointer;
    GfxCoord*             coord;
    EffectShapeScratch*   block;
    POLY_FT4*             sprite;

    coord                = coordBody->coord;
    block                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
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
        sprite         = gGpuPrimCursor;
        gGpuPrimCursor = sprite + 1;
        setPolyFT4(sprite);
        if (rgb != NULL) {
            sprite->r0 = rgb[0];
            sprite->g0 = rgb[1];
            sprite->b0 = rgb[2];
            setSemiTrans(sprite, 1);
        } else {
            setShadeTex(sprite, 1);
        }
        sprite->tpage = DRYFIELD_BREEZEWAY_PARTICLE_TEXTURE_PAGE;
        sprite->clut  = DRYFIELD_BREEZEWAY_PARTICLE_CLUT;
        sprite->u0    = (work->index & (DRYFIELD_BREEZEWAY_PARTICLE_FRAME_COUNT - 1)) * DRYFIELD_BREEZEWAY_PARTICLE_FRAME_TEXELS;
        sprite->v0    = DRYFIELD_BREEZEWAY_PARTICLE_TEXTURE_ROW;
        sprite->u1    = (work->index & (DRYFIELD_BREEZEWAY_PARTICLE_FRAME_COUNT - 1)) * DRYFIELD_BREEZEWAY_PARTICLE_FRAME_TEXELS + (DRYFIELD_BREEZEWAY_PARTICLE_FRAME_TEXELS - 1);
        sprite->v1    = DRYFIELD_BREEZEWAY_PARTICLE_TEXTURE_ROW;
        sprite->u2    = (work->index & (DRYFIELD_BREEZEWAY_PARTICLE_FRAME_COUNT - 1)) * DRYFIELD_BREEZEWAY_PARTICLE_FRAME_TEXELS;
        sprite->v2    = DRYFIELD_BREEZEWAY_PARTICLE_TEXTURE_ROW + DRYFIELD_BREEZEWAY_PARTICLE_FRAME_TEXELS - 1;
        sprite->u3    = (work->index & (DRYFIELD_BREEZEWAY_PARTICLE_FRAME_COUNT - 1)) * DRYFIELD_BREEZEWAY_PARTICLE_FRAME_TEXELS + (DRYFIELD_BREEZEWAY_PARTICLE_FRAME_TEXELS - 1);
        sprite->v3    = DRYFIELD_BREEZEWAY_PARTICLE_TEXTURE_ROW + DRYFIELD_BREEZEWAY_PARTICLE_FRAME_TEXELS - 1;
        // Project two rotated diagonal offsets to form the four screen-space corners.
        block->extent.corner.x = (((work->pos.vx * DRYFIELD_BREEZEWAY_PARTICLE_RADIUS_SCALE) / block->depth) * rsin(work->pos.vz)) >> GLOW_TRIG_SHIFT;
        block->extent.corner.y = (((work->pos.vx * DRYFIELD_BREEZEWAY_PARTICLE_RADIUS_SCALE) / block->depth) * rcos(work->pos.vz)) >> GLOW_TRIG_SHIFT;
        sprite->x0             = block->screenX + block->extent.corner.x;
        sprite->x3             = block->screenX - block->extent.corner.x;
        sprite->y0             = block->screenY - block->extent.corner.y;
        sprite->y3             = block->screenY + block->extent.corner.y;
        block->extent.corner.x = (((work->pos.vx * DRYFIELD_BREEZEWAY_PARTICLE_RADIUS_SCALE) / block->depth) * rsin(work->pos.vz + GLOW_QUARTER_TURN)) >> GLOW_TRIG_SHIFT;
        block->extent.corner.y = (((work->pos.vx * DRYFIELD_BREEZEWAY_PARTICLE_RADIUS_SCALE) / block->depth) * rcos(work->pos.vz + GLOW_QUARTER_TURN)) >> GLOW_TRIG_SHIFT;
        sprite->x1             = block->screenX + block->extent.corner.x;
        sprite->x2             = block->screenX - block->extent.corner.x;
        sprite->y1             = block->screenY - block->extent.corner.y;
        sprite->y2             = block->screenY + block->extent.corner.y;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), sprite);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}
