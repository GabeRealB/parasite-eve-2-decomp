#include "actors/actor_303600.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "common.h"

#include "gameplay/animation.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/display.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/geometry.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/room_effects.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "overlay.h"

/// Work block of the package's cutscene controller, the task that starts the
/// cutscene's event script and carries out the cues that script posts.
///
/// The controller allocates it cleared into `Task::work` when it arms, so its
/// size is that allocation's. A script callback posts a cue in `command`, and
/// the controller's next tick carries it out and clears it:
///
/// - 0: none.
/// - 1 to 5: broadcast the same-numbered actor command to the scene's actors;
///   4 also starts a white flash that fades away.
/// - 6 and 7: start a fade to white, fast and slow.
/// - 8: cover the screen in black and start the fading white flash over it.
///
/// Actor command 9, the last the cutscene sends, does not go through
/// `command`: either of two script callbacks broadcasts it directly.
typedef struct {
    Task* player;           // Player task when the controller armed; never read
    u16   command;          // Cue the script posted, carried out and cleared on the controller's next tick (values above)
    s16   field_6;          // Cleared with each posted cue and never read; role unproven
    byte  pad_8[0x4];       // Never accessed
    s16   lastActorCommand; // Actor command last broadcast (1 to 5, or 9); never read
    u16   endCommandSent;   // Actor command 9 has been broadcast (0 not yet, 1 sent), so its two senders send it once between them
} _Actor303600CutsceneWork;
STATIC_ASSERT_SIZEOF(_Actor303600CutsceneWork, 0x10);

/// Height of one shaft segment's model in world units, and so the pitch the
/// segments are stacked at.
#define ACTOR_303600_SHAFT_SEGMENT_HEIGHT 8000

/// Work block of one segment of the package's scrolling shaft: the light
/// matrices its model draws with.
///
/// The segment's task allocates it zeroed into `Task::work`, so its size is
/// that allocation's, and points its model at the two matrices, which the
/// model borrows for as long as the task lives.
typedef struct {
    MATRIX lightMtx;    // Light-direction matrix the model borrows, filled from the package's three flat lights
    MATRIX colorMtx;    // Light-colour matrix the model borrows, filled with `lightMtx`
    byte   pad_40[0x4]; // Never accessed
} _Actor303600ShaftSegmentWork;
STATIC_ASSERT_SIZEOF(_Actor303600ShaftSegmentWork, 0x44);

/// Work block of the package's scrolling shaft, a tube of five identical
/// segments stacked along Y that slides past without end.
///
/// The shaft's task allocates it zeroed into `Task::work`, so its size is that
/// allocation's. The segments hang under the task's own coordinate, so moving
/// that coordinate moves the whole stack. Each frame `scrollSpeed` changes by
/// `scrollAccel` until it passes `scrollSpeedLimit`, and is added to `scrollY`,
/// which wraps by one segment height to stay within half a segment of zero;
/// the segments being identical, the wrap does not show. An actor command
/// sent to the task sets the ramp.
///
/// The speed, its step and its limit are signed 16.16 world units a frame.
typedef struct {
    Task*   segments[5];      // Segment model tasks in order of increasing Y; left NULL from the first failed spawn on, never read
    s32     field_14;         // Never accessed; role unproven
    Fixed16 scrollY;          // Y translation of the stack, within half a segment height of zero
    s32     field_1C;         // Never accessed; role unproven
    s32     field_20;         // Never accessed; role unproven
    s32     field_24;         // Never accessed; role unproven
    s32     scrollSpeed;      // Added to `scrollY` each frame
    s32     field_2C;         // Never accessed; role unproven
    s32     field_30;         // Never accessed; role unproven
    s32     scrollAccel;      // Added to `scrollSpeed` each frame; cleared once the speed passes the limit
    s32     scrollSpeedLimit; // Speed the ramp ends at: above it for a positive `scrollAccel`, below it otherwise
} _Actor303600ShaftWork;
STATIC_ASSERT_SIZEOF(_Actor303600ShaftWork, 0x3C);

/// Cues posted by the cutscene script; actor cues 1..5 are forwarded unchanged.
///
/// Only one cue is pending: another post replaces it before the controller runs.
/// The pose/animation and figure cues are interpreted by the Eve package.
enum {
    ACTOR_303600_CUTSCENE_CUE_NONE                  = 0,
    ACTOR_303600_CUTSCENE_CUE_PREPARE_POSE          = 1,
    ACTOR_303600_CUTSCENE_CUE_PLAY_ANIMATION        = 2,
    ACTOR_303600_CUTSCENE_CUE_BRIGHTEN_ACTOR        = 3,
    ACTOR_303600_CUTSCENE_CUE_SHOW_SHAFT_AND_FLASH  = 4,
    ACTOR_303600_CUTSCENE_CUE_FADE_FIGURE           = 5,
    ACTOR_303600_CUTSCENE_CUE_FADE_TO_WHITE_FAST    = 6,
    ACTOR_303600_CUTSCENE_CUE_FADE_TO_WHITE_SLOW    = 7,
    ACTOR_303600_CUTSCENE_CUE_BLACK_COVER_AND_FLASH = 8,
};

/// White-fade task states and the byte intensity interval it draws.
enum {
    ACTOR_303600_WHITE_FADE_ALLOCATE      = 0,
    ACTOR_303600_WHITE_FADE_DRAW          = 1,
    ACTOR_303600_WHITE_FADE_MAX_INTENSITY = 255,
    ACTOR_303600_WHITE_FADE_END_INTENSITY = 256,
};

/// Fractional bits of the shaft's translation, speed, acceleration and limit.
enum { ACTOR_303600_SHAFT_FRACTION_BITS = 16 };

extern Task*    D_actor_303600_8016E4C0;
extern Task*    D_actor_303600_8016E4C4;
extern TaskDesc D_actor_303600_80162E98[];
extern TaskDesc D_actor_303600_8016E468[];
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.

extern TaskMessageEntry D_actor_303600_8016E480[];

/// The overlay's three flat lights, loaded into the model by
/// `_actor303600InitShaftSegmentLighting`; one `GsF_LIGHT` (0x10 bytes) each.
extern GsF_LIGHT D_actor_303600_8016E490[3];

/// The cutscene's two script blocks, handed to `evsStartScriptWithSkip` together when the
/// controller below arms the cutscene.
extern EvsCommand D_actor_303600_80162AF0[];
extern EvsCommand D_actor_303600_80162DD8[];

/// Main-executable globals with no module header yet: the attachment wheel being open
/// (`Gp_StateC08.mode`) or a live `gDisplayState.pendingMode` holds the scene, and `gDisplayState.spriteVariant` is the
/// latch state 2 below sets alongside `gMcSaveData`.

static void _actor303600KillShaft(Task* task);
static void _actor303600SpawnShaftSegment(Task* task);
static void _actor303600IdleShaftSegment(Task* task);
static void _actor303600InitShaftSegmentLighting(Task* task);

static TmdSource _gActor303600Model0814C;
static s32       _actor303600HandleShaftCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg);
static void      _actor303600ShaftSegmentTask(Task* task);
void             func_actor_303600_80162A7C(Task*);

static void _actor303600DrawBlackCoverTask(Task* task);
void        func_actor_303600_8016216C(Task*);
static void _actor303600FadeFromWhiteTask(Task* task);
static void _actor303600FadeToWhiteTask(Task* task);
static void _actor303600SendCutsceneEndCommand(void);
void        func_actor_303600_8016253C(void);
static void _actor303600PostCutsceneCue(s16 cue);
static void _actor303600LockCutsceneControls(void);
static void _actor303600StageCutsceneAudio(void);
static void _actor303600StartCutscenePlayback(void);
static void _actor303600CancelCutscenePlayback(void);

EvsSceneKey D_actor_303600_80162AE8 = { 6, 12, 11 };

EvsCommand D_actor_303600_80162AF0[31] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_actor_303600_80162AE8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor303600StageCutsceneAudio }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor303600LockCutsceneControls }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor303600PostCutsceneCue }, { .value = ACTOR_303600_CUTSCENE_CUE_PREPARE_POSE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor303600StartCutscenePlayback }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor303600PostCutsceneCue }, { .value = ACTOR_303600_CUTSCENE_CUE_PLAY_ANIMATION }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 150 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor303600PostCutsceneCue }, { .value = ACTOR_303600_CUTSCENE_CUE_BRIGHTEN_ACTOR }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor303600PostCutsceneCue }, { .value = ACTOR_303600_CUTSCENE_CUE_FADE_TO_WHITE_FAST }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor303600PostCutsceneCue }, { .value = ACTOR_303600_CUTSCENE_CUE_SHOW_SHAFT_AND_FLASH }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor303600PostCutsceneCue }, { .value = ACTOR_303600_CUTSCENE_CUE_FADE_FIGURE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor303600PostCutsceneCue }, { .value = ACTOR_303600_CUTSCENE_CUE_FADE_TO_WHITE_SLOW }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor303600PostCutsceneCue }, { .value = ACTOR_303600_CUTSCENE_CUE_BLACK_COVER_AND_FLASH }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor303600CancelCutscenePlayback }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor303600SendCutsceneEndCommand }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_303600_80162DD8[8] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_303600_8016253C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc D_actor_303600_80162E98[4] = {
    { { { TASK_BODY_NONE, 192 } }, func_actor_303600_8016216C, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _actor303600FadeFromWhiteTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _actor303600FadeToWhiteTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _actor303600DrawBlackCoverTask, { .value = 0 } },
};

static TmdBone _gActor303600Model02DD0Skeleton[1] = {
#include "assets/actor_303600_model_02DD0_skeleton.inc"
};

static u32 _gActor303600Model02DD0PartVerts[1] = {
#include "assets/actor_303600_model_02DD0_partVerts.inc"
};

static SVECTOR _gActor303600Model02DD0Verts[458] = {
#include "assets/actor_303600_model_02DD0_verts.inc"
};

static SVECTOR _gActor303600Model02DD0Normals[470] = {
#include "assets/actor_303600_model_02DD0_normals.inc"
};

static u32 _gActor303600Model02DD0Stream[4397] = {
#include "assets/actor_303600_model_02DD0_stream.inc"
};

TmdSource gActor303600Model02DD0 = {
    0,
    30316,
    0,
    1,
    _gActor303600Model02DD0PartVerts,
    _gActor303600Model02DD0Verts,
    _gActor303600Model02DD0Normals,
    _gActor303600Model02DD0Skeleton,
    _gActor303600Model02DD0Stream,
};

static AnimationPackedPose _gActor303600Animation075A0Bank1[5] = {
#include "assets/actor_303600_animation_075A0_bank1.inc"
};

static AnimationPackedRotation _gActor303600Animation075A0Bank4[61] = {
#include "assets/actor_303600_animation_075A0_bank4.inc"
};

static AnimationRecord _gActor303600Animation075A0Records[104] = {
#include "assets/actor_303600_animation_075A0_records.inc"
};

static u16 _gActor303600Animation075A0Indices[20] = {
#include "assets/actor_303600_animation_075A0_indices.inc"
};

AnimationSet gActor303600Animation075A0 = {
    _gActor303600Animation075A0Records,
    _gActor303600Animation075A0Indices,
    { NULL, _gActor303600Animation075A0Bank1, NULL, NULL, _gActor303600Animation075A0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor303600Animation077F0Bank1[3] = {
#include "assets/actor_303600_animation_077F0_bank1.inc"
};

static AnimationPackedRotation _gActor303600Animation077F0Bank4[29] = {
#include "assets/actor_303600_animation_077F0_bank4.inc"
};

static AnimationRecord _gActor303600Animation077F0Records[90] = {
#include "assets/actor_303600_animation_077F0_records.inc"
};

static u16 _gActor303600Animation077F0Indices[20] = {
#include "assets/actor_303600_animation_077F0_indices.inc"
};

AnimationSet gActor303600Animation077F0 = {
    _gActor303600Animation077F0Records,
    _gActor303600Animation077F0Indices,
    { NULL, _gActor303600Animation077F0Bank1, NULL, NULL, _gActor303600Animation077F0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor303600Animation07C30Bank1[6] = {
#include "assets/actor_303600_animation_07C30_bank1.inc"
};

static AnimationPackedRotation _gActor303600Animation07C30Bank4[79] = {
#include "assets/actor_303600_animation_07C30_bank4.inc"
};

static AnimationRecord _gActor303600Animation07C30Records[155] = {
#include "assets/actor_303600_animation_07C30_records.inc"
};

static u16 _gActor303600Animation07C30Indices[20] = {
#include "assets/actor_303600_animation_07C30_indices.inc"
};

AnimationSet gActor303600Animation07C30 = {
    _gActor303600Animation07C30Records,
    _gActor303600Animation07C30Indices,
    { NULL, _gActor303600Animation07C30Bank1, NULL, NULL, _gActor303600Animation07C30Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor303600Animation07E5CBank1[2] = {
#include "assets/actor_303600_animation_07E5C_bank1.inc"
};

static AnimationPackedRotation _gActor303600Animation07E5CBank4[26] = {
#include "assets/actor_303600_animation_07E5C_bank4.inc"
};

static AnimationRecord _gActor303600Animation07E5CRecords[87] = {
#include "assets/actor_303600_animation_07E5C_records.inc"
};

static u16 _gActor303600Animation07E5CIndices[20] = {
#include "assets/actor_303600_animation_07E5C_indices.inc"
};

AnimationSet gActor303600Animation07E5C = {
    _gActor303600Animation07E5CRecords,
    _gActor303600Animation07E5CIndices,
    { NULL, _gActor303600Animation07E5CBank1, NULL, NULL, _gActor303600Animation07E5CBank4, NULL, NULL, NULL },
};

static TmdBone _gActor303600Model0814CSkeleton[1] = {
#include "assets/actor_303600_model_0814C_skeleton.inc"
};

static u32 _gActor303600Model0814CPartVerts[1] = {
#include "assets/actor_303600_model_0814C_partVerts.inc"
};

static SVECTOR _gActor303600Model0814CVerts[84] = {
#include "assets/actor_303600_model_0814C_verts.inc"
};

static u32 _gActor303600Model0814CStream[286] = {
#include "assets/actor_303600_model_0814C_stream.inc"
};

static TmdSource _gActor303600Model0814C = {
    0,
    2240,
    0,
    1,
    _gActor303600Model0814CPartVerts,
    _gActor303600Model0814CVerts,
    &_gActor303600Model0814CVerts[84],
    _gActor303600Model0814CSkeleton,
    _gActor303600Model0814CStream,
};

/// The scene figure's turn, one sample per frame; `actor_403600` plays it.
Actor303600RotSample D_actor_303600_8016A408[ACTOR_303600_ROT_SAMPLE_COUNT] = {
#include "assets/actor_303600_motion_085E8.inc"
};

/// The scene's camera path, one key per frame; `actor_403600` plays it and rests
/// on the last key.
Actor303600ViewKey D_actor_303600_8016AEF8[ACTOR_303600_VIEW_KEY_COUNT] = {
#include "assets/actor_303600_path_090D8.inc"
};

TaskDesc D_actor_303600_8016E468[2] = {
    { { { TASK_BODY_COORD, 192 } }, func_actor_303600_80162A7C, { .value = 0 } },
    { { { TASK_BODY_TMD, 192 } }, _actor303600ShaftSegmentTask, { .model = &_gActor303600Model0814C } },
};

TaskMessageEntry D_actor_303600_8016E480[2] = {
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor303600HandleShaftCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

GsF_LIGHT D_actor_303600_8016E490[3] = {
    { 0, 4096, 0, 128, 128, 128 },
    { 4096, 0, 0, 128, 128, 128 },
    { 0, 0, 4096, 128, 128, 128 },
};

Task* D_actor_303600_8016E4C0;

Task* D_actor_303600_8016E4C4;

static void func_actor_303600_80161F40(Task* arg0);
static void func_actor_303600_801626C0(Task* task);
static void _actor303600ScrollShaft(Task* task);

/// Covers the centred 320x240 frame in opaque black for this callback tick.
///
/// Requires room in the frame arena for a TILE and DR_TPAGE, and a writable
/// foreground OT tag 15 entries before the current base. The GPU borrows both
/// packets until completion. Ignores the task and never ends it; the owner must
/// stop it when the cover is no longer needed.
static void _actor303600DrawBlackCoverTask(Task* task)
{
    enum { ACTOR_303600_BLACK_COVER_WIDTH     = 320,
           ACTOR_303600_BLACK_COVER_HEIGHT    = 240,
           ACTOR_303600_BLACK_COVER_OT_OFFSET = 15 };

    TILE*     tile;
    DR_TPAGE* drawMode;

    tile           = gGpuPrimCursor;
    gGpuPrimCursor = tile + 1;
    setTile(tile);
    tile->r0 = 0;
    tile->g0 = 0;
    tile->b0 = 0;
    tile->x0 = -ACTOR_303600_BLACK_COVER_WIDTH / 2;
    tile->y0 = -ACTOR_303600_BLACK_COVER_HEIGHT / 2;
    tile->w  = ACTOR_303600_BLACK_COVER_WIDTH;
    tile->h  = ACTOR_303600_BLACK_COVER_HEIGHT;
    addPrim(gGpuCurrentOt - ACTOR_303600_BLACK_COVER_OT_OFFSET, tile);

    // OT links prepend: the later draw-mode packet runs before the tile.
    drawMode       = gGpuPrimCursor;
    gGpuPrimCursor = drawMode + 1;
    setDrawTPage(drawMode, 0, 1, 0);
    addPrim(gGpuCurrentOt - ACTOR_303600_BLACK_COVER_OT_OFFSET, drawMode);
}

/// Command dispatcher the cutscene controller steps while the cutscene is up.
/// Commands 1-5 send the slot-4 task message 0x7DA carrying the session's two id
/// bytes and the command as selector, latching it in the published work block's
/// `lastActorCommand`; 4 then kills the fade in `D_actor_303600_8016E4C4` and spawns
/// `D_actor_303600_80162E98` entry 1. 6 and 7 spawn entry 2, and 8 kills the fade
/// and spawns entries 3 and 1. The command is cleared on the way out.
static void func_actor_303600_80161F40(Task* arg0)
{
    _Actor303600CutsceneWork* work = arg0->work;
    _Actor303600CutsceneWork* w;
    ActorCommand              msg;

    switch (work->command) {
        case 0:
            break;
        case 1:
            w                     = D_actor_303600_8016E4C0->work;
            msg.context.loc.stage = gGameSession->location.loc.stage;
            msg.context.loc.area  = gGameSession->location.loc.area;
            msg.command           = 1;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &msg, ACTOR_COMMAND_MESSAGE_APPLY);
            w->lastActorCommand = 1;
            break;
        case 2:
            w                     = D_actor_303600_8016E4C0->work;
            msg.context.loc.stage = gGameSession->location.loc.stage;
            msg.context.loc.area  = gGameSession->location.loc.area;
            msg.command           = 2;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &msg, ACTOR_COMMAND_MESSAGE_APPLY);
            w->lastActorCommand = 2;
            break;
        case 3:
            w                     = D_actor_303600_8016E4C0->work;
            msg.context.loc.stage = gGameSession->location.loc.stage;
            msg.context.loc.area  = gGameSession->location.loc.area;
            msg.command           = 3;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &msg, ACTOR_COMMAND_MESSAGE_APPLY);
            w->lastActorCommand = 3;
            break;
        case 4:
            w                     = D_actor_303600_8016E4C0->work;
            msg.context.loc.stage = gGameSession->location.loc.stage;
            msg.context.loc.area  = gGameSession->location.loc.area;
            msg.command           = 4;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &msg, ACTOR_COMMAND_MESSAGE_APPLY);
            w->lastActorCommand = 4;
            if (D_actor_303600_8016E4C4 != NULL) {
                taskKill(D_actor_303600_8016E4C4);
                D_actor_303600_8016E4C4 = NULL;
            }
            taskSpawnFromTable(D_actor_303600_80162E98, 1, 4, 0);
            break;
        case 5:
            w                     = D_actor_303600_8016E4C0->work;
            msg.context.loc.stage = gGameSession->location.loc.stage;
            msg.context.loc.area  = gGameSession->location.loc.area;
            msg.command           = 5;
            TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &msg, ACTOR_COMMAND_MESSAGE_APPLY);
            w->lastActorCommand = 5;
            break;
        case 6:
            taskSpawnFromTable(D_actor_303600_80162E98, 2, 8, 0);
            break;
        case 7:
            taskSpawnFromTable(D_actor_303600_80162E98, 2, 4, 0);
            break;
        case 8:
            if (D_actor_303600_8016E4C4 != NULL) {
                taskKill(D_actor_303600_8016E4C4);
                D_actor_303600_8016E4C4 = NULL;
            }
            taskSpawnFromTable(D_actor_303600_80162E98, 3, 0, 0);
            taskSpawnFromTable(D_actor_303600_80162E98, 1, 4, 0);
            break;
    }
    work->command = 0;
}

/// Cutscene controller for the overlay. State 0 arms it once: it waits while the
/// attachment wheel is open (`Gp_StateC08.mode`) or `gDisplayState.pendingMode` is live, so the state is
/// left where it is and the task returns; otherwise it allocates the
/// `_Actor303600CutsceneWork` block, zeroes it, parks the `gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)` task in
/// `player` and publishes itself in `D_actor_303600_8016E4C0` with
/// `D_actor_303600_8016E4C4` cleared, then falls into state 1, which hands the
/// overlay's two cutscene script blocks to `evsStartScriptWithSkip`. State 2 waits for
/// the session's `eventState` to clear -- the cutscene having finished -- and then
/// sets the saved location in `gMcSaveData` to stage 5, area 0x1F, warp 1,
/// room 1, raises the `gDisplayState.spriteVariant` latch, starts the stage-0 type-0x11 task and
/// kills itself; while the cutscene is still up it steps the state machine
/// instead.
void func_actor_303600_8016216C(Task* arg0)
{
    _Actor303600CutsceneWork* work;

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
                work->player            = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                D_actor_303600_8016E4C0 = arg0;
                D_actor_303600_8016E4C4 = NULL;
            }
            arg0->state += 1;
            /* fallthrough */
        case 1:
            evsStartScriptWithSkip(D_actor_303600_80162AF0, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_actor_303600_80162DD8);
            arg0->state += 1;
            break;
        case 2:
            if (gGameSession->eventState == 0) {
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage = GAME_STAGE_SHELTER_NEO_ARK;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area  = GAME_AREA_NEO_ARK_R31;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp  = 1;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room  = 1;
                gDisplayState.spriteVariant                                 = 1;
                taskSpawn(0, 0x11, 0x10, 0);
                taskKill(arg0);
                break;
            }
            func_actor_303600_80161F40(arg0);
            break;
    }
}

/// Allocates, initializes and advances one white-fade task into its draw state.
///
/// Scoped to the two white-fade callbacks below. fadeTask is a live Task*;
/// fadeWork and allocatedFadeWork are distinct ScreenFadeWork* local lvalues.
/// Arguments are used repeatedly and must have no side effects. On allocation
/// failure this kills fadeTask and returns from the containing void callback.
/// initialIntensity must be zero or 255 and is evaluated once per channel.
#define ACTOR_303600_BEGIN_WHITE_FADE(fadeTask, fadeWork, allocatedFadeWork, initialIntensity) \
    {                                                                                          \
        (allocatedFadeWork) = memMalloc(sizeof(*(allocatedFadeWork)), false);                  \
        (fadeTask)->work    = (allocatedFadeWork);                                             \
        if ((allocatedFadeWork) == NULL) {                                                     \
            taskKill(fadeTask);                                                                \
            return;                                                                            \
        }                                                                                      \
        (fadeWork)         = (allocatedFadeWork);                                              \
        (fadeWork)->b      = (initialIntensity);                                               \
        (fadeWork)->g      = (initialIntensity);                                               \
        (fadeWork)->r      = (initialIntensity);                                               \
        (fadeTask)->state += 1;                                                                \
    }

/// Removes an additive white flash, starting at intensity 255.
///
/// State 0 allocates task-owned `ScreenFadeWork` and draws the first frame in
/// the same tick; allocation failure kills the task. State 1 draws r/g/r and
/// then steps all three signed halfword channels. `spawnArg1.value` supplies
/// the low 16-bit intensity step per callback tick; the controller uses 4 or 8.
/// After red becomes negative, clears the singleton fade pointer and kills the
/// task. Keep the frame arena and foreground OT available for drawing.
static void _actor303600FadeFromWhiteTask(Task* task)
{
    ScreenFadeWork* work;
    ScreenFadeWork* allocatedWork;

    work = task->work;
    switch (task->state) {
        case ACTOR_303600_WHITE_FADE_ALLOCATE:
            ACTOR_303600_BEGIN_WHITE_FADE(task, work, allocatedWork, ACTOR_303600_WHITE_FADE_MAX_INTENSITY);
            /* fallthrough */
        case ACTOR_303600_WHITE_FADE_DRAW:
            // Draw the current intensity before stepping the signed channels.
            fadeDrawOverlay(work->r, work->g, work->r, GPU_BLEND_ADD);
            work->r -= (u16)task->spawnArg1.value;
            work->g -= (u16)task->spawnArg1.value;
            work->b -= (u16)task->spawnArg1.value;
            if (work->r < 0) {
                D_actor_303600_8016E4C4 = NULL;
                taskKill(task);
            }
            break;
    }
}

/// Builds an additive fade to white, starting at intensity zero.
///
/// State 0 allocates task-owned `ScreenFadeWork` and draws the first frame in
/// the same tick; allocation failure kills the task. State 1 draws r/g/r and
/// then steps all three signed halfword channels. `spawnArg1.value` supplies
/// the low 16-bit intensity step per callback tick; the controller uses 4 or 8.
/// After red reaches 256 or more, clears the singleton fade pointer and kills the
/// task. Keep the frame arena and foreground OT available for drawing.
static void _actor303600FadeToWhiteTask(Task* task)
{
    ScreenFadeWork* work;
    ScreenFadeWork* allocatedWork;

    work = task->work;
    switch (task->state) {
        case ACTOR_303600_WHITE_FADE_ALLOCATE:
            ACTOR_303600_BEGIN_WHITE_FADE(task, work, allocatedWork, 0);
            /* fallthrough */
        case ACTOR_303600_WHITE_FADE_DRAW:
            // Draw the current intensity before stepping the signed channels.
            fadeDrawOverlay(work->r, work->g, work->r, GPU_BLEND_ADD);
            work->r += (u16)task->spawnArg1.value;
            work->g += (u16)task->spawnArg1.value;
            work->b += (u16)task->spawnArg1.value;
            if (work->r >= ACTOR_303600_WHITE_FADE_END_INTENSITY) {
                D_actor_303600_8016E4C4 = NULL;
                taskKill(task);
            }
            break;
    }
}

#undef ACTOR_303600_BEGIN_WHITE_FADE

/// Broadcasts the cutscene's final actor command at most once.
///
/// Requires the live cutscene controller and its allocated work. The scene
/// receives a borrowed stack command with the current stage and area; dispatch
/// is synchronous. Eve interprets command 9 as battle completion with rewards.
/// The controller's latch is shared with the skip callback, preventing a second
/// broadcast when either path has already sent it.
static void _actor303600SendCutsceneEndCommand(void)
{
    enum { ACTOR_303600_CUTSCENE_END_ACTOR_COMMAND = 9 };

    _Actor303600CutsceneWork* work = D_actor_303600_8016E4C0->work;
    ActorCommand              endCommand;

    if (work->endCommandSent == false) {
        endCommand.context.loc.stage = gGameSession->location.loc.stage;
        endCommand.context.loc.area  = gGameSession->location.loc.area;
        endCommand.command           = ACTOR_303600_CUTSCENE_END_ACTOR_COMMAND;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &endCommand, ACTOR_COMMAND_MESSAGE_APPLY);
        work->lastActorCommand = ACTOR_303600_CUTSCENE_END_ACTOR_COMMAND;
        work->endCommandSent   = true;
    }
}

/// Cutscene teardown: kill the task a previous cutscene left in
/// `D_actor_303600_8016E4C4`, then, while the work block's `endCommandSent`
/// latch is still clear, send the same 0x7DA announcement
/// `_actor303600SendCutsceneEndCommand` sends and latch selector 9.  Finishes by
/// spawning the overlay's own continuation task -- `D_actor_303600_80162E98`
/// entry 3 -- so this runs exactly once per cutscene.
void func_actor_303600_8016253C(void)
{
    _Actor303600CutsceneWork* work;
    ActorCommand              msg;

    if (D_actor_303600_8016E4C4 != NULL) {
        taskKill(D_actor_303600_8016E4C4);
        D_actor_303600_8016E4C4 = NULL;
    }

    work = D_actor_303600_8016E4C0->work;
    if (work->endCommandSent == 0) {
        msg.context.loc.stage = gGameSession->location.loc.stage;
        msg.context.loc.area  = gGameSession->location.loc.area;
        msg.command           = 9;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &msg, ACTOR_COMMAND_MESSAGE_APPLY);
        work->lastActorCommand = 9;
        work->endCommandSent   = 1;
    }

    taskSpawnFromTable(D_actor_303600_80162E98, 3, 0, 0);
}

/// Replaces the cutscene controller's pending cue for its next update.
///
/// Requires the live controller and its allocated work. Pass an
/// `ACTOR_303600_CUTSCENE_CUE_*` value (0..8); EVS narrows its operand to s16,
/// which is stored in the unsigned halfword cue slot. Posting also clears an
/// otherwise unused halfword whose role is unproven.
static void _actor303600PostCutsceneCue(s16 cue)
{
    _Actor303600CutsceneWork* work = D_actor_303600_8016E4C0->work;

    work->command = cue;
    work->field_6 = 0;
}

/// Locks the attachment controls and requests PE-effect cancellation.
///
/// Requires the room-effect controller. Cancellation is observed on its next
/// update; the event lock remains set for the surrounding cutscene to release.
static void _actor303600LockCutsceneControls(void)
{
    roomEffectRequestCancelPe();
    Gp_StateC08.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
}

/// Stages the selected cutscene's deferred audio-start request.
///
/// Requires a successfully selected scene and prepared playback buffers.
/// The CD scheduler later commits this replacement request.
static void _actor303600StageCutsceneAudio(void)
{
    cdCmdStageSceneAudioStart();
}

/// Requests playback of the selected cutscene's audio session.
///
/// Keep the selected scene and its prepared buffers live until the CD request
/// is consumed. A scene without an audio slot enters playback immediately.
static void _actor303600StartCutscenePlayback(void)
{
    cdCmdEnqueueScenePlayback();
}

/// Finishes scene streaming and requests cancellation of cutscene CD work.
///
/// Requires a previously selected scene. Restores the saved random state before
/// dropping the deferred request; `cdCmdCancelScene` repeats stream completion.
/// Retains both calls, including their random reseeding. Releases no buffers or
/// tasks; their owners handle teardown.
static void _actor303600CancelCutscenePlayback(void)
{
    streamFinishScene();
    cdCmdCancelScene();
}

/// Spawn state of the package's scrolling shaft: allocates the work block the
/// later states read through `Task::work`, clears the task's own root
/// coordinate, then spawns the five segment models -- one `taskSpawnFromTable`
/// of `D_actor_303600_8016E468` entry 1 each, parked in `segments` and stacked
/// one segment height apart in Y, centred on the task's coordinate.  The spread
/// reaches the coordinate through the strength-reduced `i * 8000 - 16000`
/// loop.c folds into an accumulator, so its initialiser is scheduled at the
/// loop head beside the hoisted `%hi` of the spawn table.  A failed spawn stops the loop early, a
/// failed allocation kills the task instead of leaving a half-built controller,
/// and the last three statements install the 0x7DB handler table at
/// `Task::msgTable`, the shared kill callback and the next state.
static void func_actor_303600_801626C0(Task* task)
{
    _Actor303600ShaftWork* work;
    GfxCoord*              coord;
    GfxCoord*              childCoord;
    Task*                  child;
    s32                    i;

    work = memCalloc(sizeof(*work), 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work        = work;
    coord             = task->extra.tmd->coords;
    coord->coord.t[0] = 0;
    coord->coord.t[1] = 0;
    coord->coord.t[2] = 0;
    for (i = 0; i < (s32)ARRAY_SIZE(work->segments); i++) {
        child = taskSpawnFromTable(D_actor_303600_8016E468, 1, 0, task);
        if (child == NULL) {
            break;
        }
        work->segments[i]      = child;
        childCoord             = child->extra.tmd->coords;
        childCoord->coord.t[1] = i * ACTOR_303600_SHAFT_SEGMENT_HEIGHT - 2 * ACTOR_303600_SHAFT_SEGMENT_HEIGHT;
        childCoord->coord.t[0] = 0;
        childCoord->coord.t[2] = 0;
    }
    task->msgTable     = D_actor_303600_8016E480;
    task->exitCallback = _actor303600KillShaft;
    task->state       += 1;
}

/// Advances the shaft's speed ramp and repeating Y translation by one tick.
///
/// Requires initialized shaft work and a live coordinate body. Motion uses
/// signed 16.16 world-coordinate units; the ramp stops after crossing its
/// directional limit and retains the overshoot. Wraps once by a segment height
/// when Y leaves +/-4000, then publishes the signed integer half and marks the
/// transform dirty. Commanded speeds are smaller than one segment per tick.
static void _actor303600ScrollShaft(Task* task)
{
    _Actor303600ShaftWork* shaftWork  = task->work;
    GfxCoord*              shaftCoord = task->extra.coordBody->coord;
    s32                    nextSpeed;
    s32                    nextScrollY;
    s32                    crossedSpeedLimit;

    // Stop the ramp after crossing the limit; retain the overshoot.
    nextSpeed              = shaftWork->scrollSpeed + shaftWork->scrollAccel;
    shaftWork->scrollSpeed = nextSpeed;
    if (shaftWork->scrollAccel > 0) {
        crossedSpeedLimit = nextSpeed > shaftWork->scrollSpeedLimit;
    } else {
        crossedSpeedLimit = nextSpeed < shaftWork->scrollSpeedLimit;
    }
    if (crossedSpeedLimit != 0) {
        shaftWork->scrollAccel = 0;
    }
    nextScrollY             = shaftWork->scrollY.word + shaftWork->scrollSpeed;
    shaftWork->scrollY.word = nextScrollY;
    // The segments are identical, so a jump of one segment height is unseen.
    if (nextScrollY > (ACTOR_303600_SHAFT_SEGMENT_HEIGHT / 2) << ACTOR_303600_SHAFT_FRACTION_BITS) {
        shaftWork->scrollY.word = nextScrollY - (ACTOR_303600_SHAFT_SEGMENT_HEIGHT << ACTOR_303600_SHAFT_FRACTION_BITS);
    } else if (nextScrollY < -((ACTOR_303600_SHAFT_SEGMENT_HEIGHT / 2) << ACTOR_303600_SHAFT_FRACTION_BITS)) {
        shaftWork->scrollY.word = nextScrollY + (ACTOR_303600_SHAFT_SEGMENT_HEIGHT << ACTOR_303600_SHAFT_FRACTION_BITS);
    }
    shaftCoord->coord.t[1]   = shaftWork->scrollY.halves.integer;
    shaftCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Tears down the shaft and dispatches exits to its child segment tasks.
///
/// Used both as the installed exit callback and the shaft's terminal state.
/// Follows `taskKill`'s release rules; callers must not keep using the task.
static void _actor303600KillShaft(Task* task)
{
    taskKill(task);
}

/// Applies a scrolling command to the live shaft, or exits it for other commands.
///
/// Receives `ACTOR_COMMAND_MESSAGE_APPLY` with a borrowed, read-only command
/// valid through synchronous dispatch. Only its selector is read; stage, area,
/// messageId and unusedArg are ignored. Forward starts at +384 with a +8 ramp
/// toward +768; reverse ramps the current speed by -6 toward -768, all in
/// world units per callback tick. Every other selector invokes the installed
/// exit callback. Returns zero for every selector, including teardown.
static s32 _actor303600HandleShaftCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg)
{
    _Actor303600ShaftWork* work;

    work = task->work;
    switch (command->command) {
        case ACTOR_303600_SHAFT_COMMAND_SCROLL_FORWARD:
            work->scrollSpeed      = 384 << ACTOR_303600_SHAFT_FRACTION_BITS;
            work->scrollAccel      = 8 << ACTOR_303600_SHAFT_FRACTION_BITS;
            work->scrollSpeedLimit = 768 << ACTOR_303600_SHAFT_FRACTION_BITS;
            break;
        case ACTOR_303600_SHAFT_COMMAND_REVERSE_SCROLL:
            work->scrollAccel      = -(6 << ACTOR_303600_SHAFT_FRACTION_BITS);
            work->scrollSpeedLimit = -(768 << ACTOR_303600_SHAFT_FRACTION_BITS);
            break;
        default:
            task->exitCallback(task);
            break;
    }
    return 0;
}

/// State table of the scrolling shaft: spawn, per-frame motion and the kill
/// callback. Dispatched by `func_actor_303600_80162A7C`.
static const TaskFuncTable3 D_actor_303600_80161E48 = { {
    func_actor_303600_801626C0,
    _actor303600ScrollShaft,
    _actor303600KillShaft,
} };

/// State table of the shaft's segment tasks: spawn, an empty per-frame tick and
/// `taskKill`. Dispatched by `_actor303600ShaftSegmentTask`.
static const TaskFuncTable3 D_actor_303600_80161E54 = { {
    _actor303600SpawnShaftSegment,
    _actor303600IdleShaftSegment,
    taskKill,
} };

/// Dispatches the shaft segment's spawn, idle or exit state while actors run.
///
/// `Task::state` must be 0..2; the table is copied before dispatch. A non-running
/// actor-control value holds initialization and state callbacks. The idle state
/// is empty because the segment follows its parent's transform and the model
/// draw pass renders it independently.
static void _actor303600ShaftSegmentTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_actor_303600_80161E54;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        stateHandlers.funcs[task->state](task);
    }
}

/// Allocates a segment's lighting and attaches it to the scrolling shaft.
///
/// Requires a TMD task and a live coordinate-body shaft task in spawnArg2.
/// The task owns its zeroed lighting work; the model borrows its matrices until
/// teardown. Links both the coordinate and task to the shaft, enables active
/// drawing and enters idle state. Allocation failure kills the segment.
static void _actor303600SpawnShaftSegment(Task* task)
{
    Task*                         shaftTask    = task->spawnArg2.pointer;
    TmdObject*                    segmentModel = task->extra.tmd;
    GfxCoord*                     segmentCoord = segmentModel->coords;
    ModelObjectCoordBody*         shaftBody    = shaftTask->extra.coordBody;
    GfxCoord*                     shaftCoord   = shaftBody->coord;
    _Actor303600ShaftSegmentWork* lightingWork;

    lightingWork = memCalloc(sizeof(*lightingWork), 0);
    if (lightingWork == NULL) {
        taskKill(task);
        return;
    }

    // Coordinate parenting supplies motion; task parenting supplies teardown.
    task->work                 = lightingWork;
    segmentCoord->parent       = shaftCoord;
    segmentCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    _actor303600InitShaftSegmentLighting(task);
    taskReparent(shaftTask, task);
    segmentModel->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    task->state         += 1;
}

/// Keeps a shaft segment idle while parent motion and model drawing continue.
///
/// This state deliberately performs no per-frame work.
static void _actor303600IdleShaftSegment(Task* task)
{
}

/// Binds a segment's owned lighting matrices and fills them from the three lights.
///
/// Requires allocated segment work and a live TMD model. Each flat-light index
/// is a matrix row (0..2); the model borrows both matrices for the task's life.
static void _actor303600InitShaftSegmentLighting(Task* task)
{
    _Actor303600ShaftSegmentWork* lightingWork = task->work;
    TmdObject*                    segmentModel = task->extra.tmd;
    GsF_LIGHT*                    light;
    s32                           lightIndex;

    segmentModel->lightMtx = &lightingWork->lightMtx;
    segmentModel->colorMtx = &lightingWork->colorMtx;
    for (lightIndex = 0, light = D_actor_303600_8016E490; lightIndex < (s32)ARRAY_SIZE(D_actor_303600_8016E490); lightIndex++, light++) {
        gfxSetFlatLight(lightIndex, light, &lightingWork->lightMtx, &lightingWork->colorMtx);
    }
}

/// Per-frame dispatcher of the scrolling shaft: runs its spawn, motion or exit
/// state from `D_actor_303600_80161E48`, skipping the frame while
/// `gSceneCombatState.actorControl` is set.
void func_actor_303600_80162A7C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_303600_80161E48;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        sp.funcs[task->state](task);
    }
}
