#include "actors/actor_303600.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "common.h"

#include "gameplay/companion_load.h"
#include "gameplay/animation.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
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

/// Y spacing and scroll-repeat period of the five shaft segments, in parent units.
///
/// Converted to signed 16.16 units for scrolling; wrapping occurs beyond
/// either half-period endpoint. This is the placement pitch, not an asset bound.
enum { ACTOR_303600_SHAFT_SEGMENT_HEIGHT = 8000 };

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

/// Borrowed singleton task of the Neo Ark shaft-and-figure cutscene.
///
/// Published only after controller work allocation succeeds. Event callbacks
/// require the live task and work; teardown does not clear this handle.
/// The handle neither owns the task nor extends its lifetime.
static Task* _gActor303600CutsceneControllerTask;
/// Cutscene fade cancellation slot, left unpopulated by this package.
///
/// Initialization, fade completion and cancellation only write NULL; fade
/// spawn results are discarded. Conditional task kills retain the original
/// behavior, although this package never publishes a live handle here.
static Task* _gActor303600FadeTaskSlot;
/// Main-executable globals with no module header yet: the attachment wheel being open
/// (`gAttachmentState.mode`) or a live `gDisplayState.pendingMode` holds the scene, and `gDisplayState.spriteVariant` is the
/// latch state 2 below sets alongside `gMcSaveData`.

static void _actor303600KillShaft(Task* task);
static void _actor303600SpawnShaftSegment(Task* task);
static void _actor303600IdleShaftSegment(Task* task);
static void _actor303600InitShaftSegmentLighting(Task* task);

static TmdSource _gActor303600Model0814C;
static s32       _actor303600HandleShaftCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg);
static void      _actor303600ShaftSegmentTask(Task* task);
static void      _actor303600ShaftTask(Task* task);

static void _actor303600DrawBlackCoverTask(Task* task);
static void _actor303600CutsceneTask(Task* task);
static void _actor303600FadeFromWhiteTask(Task* task);
static void _actor303600FadeToWhiteTask(Task* task);
static void _actor303600SendCutsceneEndCommand(void);
static void _actor303600FinishSkippedCutscene(void);
static void _actor303600PostCutsceneCue(s16 cue);
static void _actor303600LockCutsceneControls(void);
static void _actor303600StageCutsceneAudio(void);
static void _actor303600StartCutscenePlayback(void);
static void _actor303600CancelCutscenePlayback(void);

/// Scene/audio stream key for the Neo Ark shaft-and-figure cutscene.
///
/// Selects group 6, stream 12, qualifier 11; the absent second qualifier is zero.
/// Event playback borrows this key while the actor overlay remains loaded.
static EvsSceneKey _gActor303600CutsceneSceneKey = { .group = 6, .streamId = 12, .subId = 11 };

enum { ACTOR_303600_CUTSCENE_CAP_COMMAND = 1 };

/// Neo Ark shaft-and-figure presentation synchronized to CAP command 1.
///
/// The 31 commands include END. Selects scene/audio stream 6/12/11, locks
/// attachment controls, and posts pose, animation, lighting and cover cues
/// between CAP waits before broadcasting the final actor command. Frame waits
/// count interpreter updates; area-music stop uses fade control 150. Start with
/// `EVENT_SCRIPT_HUD_HIDE_RESTORE` and the paired skip script. The controller,
/// actor overlays, CAP data and scene resources must remain loaded through END.
static EvsCommand _gActor303600ShaftFigureCutsceneScript[31] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .message = { .value = ACTOR_303600_CUTSCENE_CAP_COMMAND } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &_gActor303600CutsceneSceneKey }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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

/// Covers and finishes the shaft-and-figure presentation when it is skipped.
///
/// The eight commands include END. Under the primary fade, sends the final
/// actor command at most once, starts black cover and cleans up scene streaming
/// before returning the fade. Requires the live controller and resources of
/// `_gActor303600ShaftFigureCutsceneScript`; its event task restores the HUD.
static EvsCommand _gActor303600ShaftFigureCutsceneSkipScript[8] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor303600FinishSkippedCutscene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc gActor303600CutsceneFadeTaskDescriptors[4] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _actor303600CutsceneTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _actor303600FadeFromWhiteTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _actor303600FadeToWhiteTask, { .value = 0 } },
    { { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _actor303600DrawBlackCoverTask, { .value = 0 } },
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

const Actor303600RotSample gActor303600SceneFigureRotationSamples[ACTOR_303600_ROT_SAMPLE_COUNT] SECTION(".data") = {
#include "assets/rooms/shelter/b2_pod_bottom/ending_figure_rotation_track.inc"
};

const Actor303600ViewKey gActor303600SceneCameraKeys[ACTOR_303600_VIEW_KEY_COUNT] SECTION(".data") = {
#include "assets/rooms/shelter/b2_pod_bottom/ending_camera_track.inc"
};

TaskDesc gActor303600ShaftTaskDescriptors[ACTOR_303600_SHAFT_DESCRIPTOR_COUNT] = {
    [ACTOR_303600_SHAFT_ROOT_DESCRIPTOR] = {
        .header   = { .fields = { .flags = TASK_BODY_COORD, .priority = TASK_PRIORITY_CONTROL } },
        .callback = _actor303600ShaftTask,
        .data     = { .value = 0 },
    },
    [ACTOR_303600_SHAFT_SEGMENT_DESCRIPTOR] = {
        .header   = { .fields = { .flags = TASK_BODY_TMD, .priority = TASK_PRIORITY_CONTROL } },
        .callback = _actor303600ShaftSegmentTask,
        .data     = { .model = &_gActor303600Model0814C },
    },
};

/// Scrolling-shaft actor-command binding followed by the message-table end marker.
///
/// Borrows an `ActorCommand` synchronously: 0 starts forward scroll, 1 reverses
/// its speed ramp, and every other command invokes teardown. The shaft task
/// borrows this table for its lifetime; unsupported message IDs return zero.
static TaskMessageEntry _gActor303600ShaftMessageTable[2] = {
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor303600HandleShaftCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Three axis-aligned directional lights for the scrolling shaft segments.
///
/// Directions are +Y, +X and +Z, with RGB intensity 128 on each light.
/// Each segment copies their normalized directions and colours into its own
/// matrices; the shared light records remain live for the overlay's lifetime.
static GsF_LIGHT _gActor303600ShaftSegmentLights[3] = {
    { 0, ONE, 0, 128, 128, 128 },
    { ONE, 0, 0, 128, 128, 128 },
    { 0, 0, ONE, 128, 128, 128 },
};

static void _actor303600DispatchCutsceneCue(Task* task);
static void _actor303600InitShaft(Task* task);
static void _actor303600ScrollShaft(Task* task);

/// Covers the centred 320x240 frame in opaque black for this callback tick.
///
/// Requires room in the frame arena for a TILE and DR_TPAGE, and a writable
/// foreground OT tag 15 entries before the current base. The GPU borrows both
/// packets until completion. Ignores the task and never ends it; the owner must
/// stop it when the cover is no longer needed.
static void _actor303600DrawBlackCoverTask(Task* task)
{
    enum {
        ACTOR_303600_BLACK_COVER_OT_OFFSET = 15
    };

    TILE*     tile;
    DR_TPAGE* drawMode;

    tile           = gGpuPrimCursor;
    gGpuPrimCursor = tile + 1;
    setTile(tile);
    tile->r0 = 0;
    tile->g0 = 0;
    tile->b0 = 0;
    tile->x0 = -DISPLAY_SCREEN_WIDTH / 2;
    tile->y0 = -DISPLAY_SCREEN_HEIGHT / 2;
    tile->w  = DISPLAY_SCREEN_WIDTH;
    tile->h  = DISPLAY_SCREEN_HEIGHT;
    addPrim(gGpuCurrentOt - ACTOR_303600_BLACK_COVER_OT_OFFSET, tile);

    // OT links prepend: the later draw-mode packet runs before the tile.
    drawMode       = gGpuPrimCursor;
    gGpuPrimCursor = drawMode + 1;
    setDrawTPage(drawMode, 0, 1, 0);
    addPrim(gGpuCurrentOt - ACTOR_303600_BLACK_COVER_OT_OFFSET, drawMode);
}

/// Consumes the pending cutscene cue, broadcasting actor commands or starting fades.
///
/// Requires live controller work, its published singleton and loaded scene tasks.
/// Actor cues 1..5 broadcast the current stage/area and record the cue after
/// synchronous dispatch. Showing the shaft also replaces the current white flash.
/// White fades advance by eight or four intensity units per tick; the black-cover
/// cue replaces the flash and starts an opaque cover beneath it. The command is
/// cleared even for unrecognized values. Recipients retain no request pointer;
/// only its stage, area and command fields are initialized for these receivers.
static void _actor303600DispatchCutsceneCue(Task* task)
{
    enum { ACTOR_303600_FLASH_TASK       = 1,
           ACTOR_303600_WHITE_FADE_TASK  = 2,
           ACTOR_303600_BLACK_COVER_TASK = 3,
           ACTOR_303600_FADE_SLOW_STEP   = 4,
           ACTOR_303600_FADE_FAST_STEP   = 8 };

    _Actor303600CutsceneWork* work = task->work;
    _Actor303600CutsceneWork* publishedWork;
    ActorCommand              request;

/// Broadcasts a scene actor cue, then records the cue on the published controller.
///
/// cue is a side-effect-free command constant in 1..5, evaluated twice. Captures
/// the live _gActor303600CutsceneControllerTask, writable publishedWork and
/// ActorCommand request locals, plus the session's current stage/area. Only stage,
/// area and command are initialized; these scene receivers must not read the
/// remaining context bytes. Dispatch borrows request synchronously. The cached
/// controller work receives lastActorCommand after dispatch. Invoke within braces.
#define ACTOR_303600_BROADCAST_ACTOR_CUE(cue)                                                                                                           \
    {                                                                                                                                                   \
        publishedWork             = _gActor303600CutsceneControllerTask->work;                                                                          \
        request.context.loc.stage = gGameSession->location.stage;                                                                                       \
        request.context.loc.area  = gGameSession->location.area;                                                                                        \
        request.command           = (cue);                                                                                                              \
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &request, ACTOR_COMMAND_MESSAGE_APPLY); \
        publishedWork->lastActorCommand = (cue);                                                                                                        \
    }

    switch (work->command) {
        case ACTOR_303600_CUTSCENE_CUE_NONE:
            break;
        case ACTOR_303600_CUTSCENE_CUE_PREPARE_POSE:
            ACTOR_303600_BROADCAST_ACTOR_CUE(ACTOR_303600_CUTSCENE_CUE_PREPARE_POSE);
            break;
        case ACTOR_303600_CUTSCENE_CUE_PLAY_ANIMATION:
            ACTOR_303600_BROADCAST_ACTOR_CUE(ACTOR_303600_CUTSCENE_CUE_PLAY_ANIMATION);
            break;
        case ACTOR_303600_CUTSCENE_CUE_BRIGHTEN_ACTOR:
            ACTOR_303600_BROADCAST_ACTOR_CUE(ACTOR_303600_CUTSCENE_CUE_BRIGHTEN_ACTOR);
            break;
        case ACTOR_303600_CUTSCENE_CUE_SHOW_SHAFT_AND_FLASH:
            ACTOR_303600_BROADCAST_ACTOR_CUE(ACTOR_303600_CUTSCENE_CUE_SHOW_SHAFT_AND_FLASH);
            if (_gActor303600FadeTaskSlot != NULL) {
                taskKill(_gActor303600FadeTaskSlot);
                _gActor303600FadeTaskSlot = NULL;
            }
            taskSpawnFromTable(gActor303600CutsceneFadeTaskDescriptors, ACTOR_303600_FLASH_TASK, ACTOR_303600_FADE_SLOW_STEP, 0);
            break;
        case ACTOR_303600_CUTSCENE_CUE_FADE_FIGURE:
            ACTOR_303600_BROADCAST_ACTOR_CUE(ACTOR_303600_CUTSCENE_CUE_FADE_FIGURE);
            break;
        case ACTOR_303600_CUTSCENE_CUE_FADE_TO_WHITE_FAST:
            taskSpawnFromTable(gActor303600CutsceneFadeTaskDescriptors, ACTOR_303600_WHITE_FADE_TASK, ACTOR_303600_FADE_FAST_STEP, 0);
            break;
        case ACTOR_303600_CUTSCENE_CUE_FADE_TO_WHITE_SLOW:
            taskSpawnFromTable(gActor303600CutsceneFadeTaskDescriptors, ACTOR_303600_WHITE_FADE_TASK, ACTOR_303600_FADE_SLOW_STEP, 0);
            break;
        case ACTOR_303600_CUTSCENE_CUE_BLACK_COVER_AND_FLASH:
            if (_gActor303600FadeTaskSlot != NULL) {
                taskKill(_gActor303600FadeTaskSlot);
                _gActor303600FadeTaskSlot = NULL;
            }
            taskSpawnFromTable(gActor303600CutsceneFadeTaskDescriptors, ACTOR_303600_BLACK_COVER_TASK, 0, 0);
            taskSpawnFromTable(gActor303600CutsceneFadeTaskDescriptors, ACTOR_303600_FLASH_TASK, ACTOR_303600_FADE_SLOW_STEP, 0);
            break;
    }
    work->command = ACTOR_303600_CUTSCENE_CUE_NONE;

#undef ACTOR_303600_BROADCAST_ACTOR_CUE
}

/// Allocates and publishes the shaft/figure scene controller and its player handle.
///
/// Requires a live bodyless controller and the current player. Owns the cleared
/// work until task teardown and clears the auxiliary scene-effect handle.
/// Failure kills the task without publishing it; the caller retains its subsequent
/// state increment and script start, so normal playback requires successful allocation.
static inline void _actor303600InitializeCutscene(Task* task)
{
    _Actor303600CutsceneWork* work;
    work       = memMalloc(sizeof(*work), false);
    task->work = work;
    if (work == NULL) {
        taskKill(task);
    } else {
        memFillBytes(work, 0, sizeof(*work));
        work->player                        = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        _gActor303600CutsceneControllerTask = task;
        _gActor303600FadeTaskSlot           = NULL;
    }
}

/// Runs the shaft/figure cutscene and reloads Neo Ark room 31 on completion.
///
/// Start at state 0 with the scene overlays and scripts loaded. Setup waits for
/// the attachment wheel and display transition, owns cleared controller work,
/// then starts the script on the same tick. While it runs, consumes its pending
/// cue once per update. Completion writes area 31, room/warp 1 and requests the
/// game reload. Allocation failure kills the task but retains script start and
/// state updates; the setup allocation must succeed for normal scene playback.
static void _actor303600CutsceneTask(Task* task)
{
    enum {
        ACTOR_303600_SCENE_INITIALIZE   = 0,
        ACTOR_303600_SCENE_START_SCRIPT = 1,
        ACTOR_303600_SCENE_WAIT_SCRIPT  = 2,
    };

    switch (task->state) {
        case ACTOR_303600_SCENE_INITIALIZE:
            if (gAttachmentState.mode == ATTACHMENT_MODE_WHEEL || gDisplayState.pendingMode != DISPLAY_MODE_NONE) {
                return;
            }
            _actor303600InitializeCutscene(task);
            task->state += 1;
            /* fallthrough */
        case ACTOR_303600_SCENE_START_SCRIPT:
            evsStartScriptWithSkip(_gActor303600ShaftFigureCutsceneScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gActor303600ShaftFigureCutsceneSkipScript);
            task->state += 1;
            break;
        case ACTOR_303600_SCENE_WAIT_SCRIPT:
            if (gGameSession->eventState == 0) {
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.stage     = GAME_STAGE_SHELTER_NEO_ARK;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area      = GAME_AREA_NEO_ARK_R31;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp      = 1;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = 1;
                gDisplayState.spriteVariant                                 = 1;
                taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_SKIP_BATTLE_ESCAPE, 0);
                taskKill(task);
                break;
            }
            _actor303600DispatchCutsceneCue(task);
            break;
    }
}

/// Allocates task-owned RGB state and advances a white-fade task to drawing.
///
/// Requires a live Task* fadeTask and distinct writable ScreenFadeWork* locals
/// fadeWork and allocatedFadeWork. All arguments must be side-effect-free because
/// they are evaluated repeatedly; initialIntensity is evaluated for each channel.
/// The callers use 0 or 255. Allocation failure kills the task and returns from
/// the enclosing void callback; success leaves the new state ready for fallthrough.
#define ACTOR_303600_BEGIN_WHITE_FADE(fadeTask, fadeWork, allocatedFadeWork, initialIntensity) \
    do {                                                                                       \
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
    } while (0)

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
                _gActor303600FadeTaskSlot = NULL;
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
                _gActor303600FadeTaskSlot = NULL;
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

    _Actor303600CutsceneWork* work = _gActor303600CutsceneControllerTask->work;
    ActorCommand              endCommand;

    if (work->endCommandSent == false) {
        endCommand.context.loc.stage = gGameSession->location.stage;
        endCommand.context.loc.area  = gGameSession->location.area;
        endCommand.command           = ACTOR_303600_CUTSCENE_END_ACTOR_COMMAND;
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &endCommand, ACTOR_COMMAND_MESSAGE_APPLY);
        work->lastActorCommand = ACTOR_303600_CUTSCENE_END_ACTOR_COMMAND;
        work->endCommandSent   = true;
    }
}

/// Broadcasts the skipped scene's final command through the shared once-only latch.
///
/// Invoke as a statement in a braced block. work is a side-effect-free live
/// controller-work pointer, endCommand a writable ActorCommand lvalue; both
/// arguments occur repeatedly. Requires ACTOR_303600_SKIP_END_ACTOR_COMMAND
/// in scope and the live session/scene task. Initializes only stage, area and
/// command, dispatches synchronously, then records and latches the command.
#define ACTOR_303600_SEND_SKIPPED_END_COMMAND(work, endCommand)                                                                                              \
    if ((work)->endCommandSent == false) {                                                                                                                   \
        (endCommand).context.loc.stage = gGameSession->location.stage;                                                                                       \
        (endCommand).context.loc.area  = gGameSession->location.area;                                                                                        \
        (endCommand).command           = ACTOR_303600_SKIP_END_ACTOR_COMMAND;                                                                                \
        TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_SCENE), SCENE_MESSAGE_BROADCAST_TO_ACTORS, &(endCommand), ACTOR_COMMAND_MESSAGE_APPLY); \
        (work)->lastActorCommand = ACTOR_303600_SKIP_END_ACTOR_COMMAND;                                                                                      \
        (work)->endCommandSent   = true;                                                                                                                     \
    }

/// Finishes the skipped cutscene by sending its final command and covering the screen.
///
/// Requires the live cutscene controller and scene task. Kills and clears the
/// optional saved fade handle, then synchronously broadcasts command 9 in the
/// current stage/area only if the shared end latch is clear. Eve interprets it
/// as battle completion with rewards. Starts a black-cover task even when the
/// command was already sent; repeated calls can spawn further covers. Fade and
/// cover storage remain owned by their tasks; spawn failure is ignored.
static void _actor303600FinishSkippedCutscene(void)
{
    enum { ACTOR_303600_SKIP_END_ACTOR_COMMAND = 9,
           ACTOR_303600_SKIP_BLACK_COVER_TASK  = 3 };

    _Actor303600CutsceneWork* work;
    ActorCommand              endCommand;

    if (_gActor303600FadeTaskSlot != NULL) {
        taskKill(_gActor303600FadeTaskSlot);
        _gActor303600FadeTaskSlot = NULL;
    }

    work = _gActor303600CutsceneControllerTask->work;
    ACTOR_303600_SEND_SKIPPED_END_COMMAND(work, endCommand);

    taskSpawnFromTable(gActor303600CutsceneFadeTaskDescriptors, ACTOR_303600_SKIP_BLACK_COVER_TASK, 0, NULL);
}

#undef ACTOR_303600_SEND_SKIPPED_END_COMMAND

/// Replaces the cutscene controller's pending cue for its next update.
///
/// Requires the live controller and its allocated work. Pass an
/// `ACTOR_303600_CUTSCENE_CUE_*` value (0..8); EVS narrows its operand to s16,
/// which is stored in the unsigned halfword cue slot. Posting also clears an
/// otherwise unused halfword whose role is unproven.
static void _actor303600PostCutsceneCue(s16 cue)
{
    _Actor303600CutsceneWork* work = _gActor303600CutsceneControllerTask->work;

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
    gAttachmentState.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
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

/// Initializes the coordinate-body shaft and its five scrolling model segments.
///
/// Requires a coordinate body in state 0. Owns a zeroed work block for the
/// task lifetime and resets root XYZ in parent-coordinate units. Segment roots
/// are placed at -16000, -8000, 0, 8000 and 16000 along Y; their initializer
/// parents them to this root and joins the teardown tree. Allocation failure
/// kills the shaft; a segment spawn failure retains the already-created prefix.
/// Installs actor-command handling and teardown, then advances to motion state.
static void _actor303600InitShaft(Task* task)
{
    enum { ACTOR_303600_SHAFT_SEGMENT_TASK_INDEX = 1 };

    _Actor303600ShaftWork* work;
    GfxCoord*              shaftCoord;
    GfxCoord*              segmentCoord;
    Task*                  segmentTask;
    s32                    segmentIndex;

    work = memCalloc(sizeof(*work), 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work             = work;
    shaftCoord             = task->extra.coordBody->coord;
    shaftCoord->coord.t[0] = 0;
    shaftCoord->coord.t[1] = 0;
    shaftCoord->coord.t[2] = 0;
    // Identical TMD segments follow the coordinate body, centred two pitches below it.
    for (segmentIndex = 0; segmentIndex < (s32)ARRAY_SIZE(work->segments); segmentIndex++) {
        segmentTask = taskSpawnFromTable(gActor303600ShaftTaskDescriptors, ACTOR_303600_SHAFT_SEGMENT_TASK_INDEX, 0, task);
        if (segmentTask == NULL) {
            break;
        }
        work->segments[segmentIndex] = segmentTask;
        segmentCoord                 = segmentTask->extra.tmd->coords;
        segmentCoord->coord.t[1]     = segmentIndex * ACTOR_303600_SHAFT_SEGMENT_HEIGHT - 2 * ACTOR_303600_SHAFT_SEGMENT_HEIGHT;
        segmentCoord->coord.t[0]     = 0;
        segmentCoord->coord.t[2]     = 0;
    }
    task->msgTable     = _gActor303600ShaftMessageTable;
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

/// Scrolling-shaft states 0 initialize, 1 scroll and 2 tear down.
///
/// `Task::state` indexes the complete three-handler copy without a terminator
/// or bounds check. Scrolling requires the root coordinate and initialized
/// work; teardown releases the shaft and child segments. Actor-control holds
/// suspend every slot.
static const TaskFuncTable3 _gActor303600ShaftTaskStates = { {
    _actor303600InitShaft,
    _actor303600ScrollShaft,
    _actor303600KillShaft,
} };

/// Shaft-segment states 0 attach, 1 idle and 2 kill.
///
/// The dispatcher copies this complete three-handler value and requires a
/// state in 0..2. Actor-control holds suspend dispatch; idle needs no movement
/// because the segment's coordinate follows the scrolling shaft.
static const TaskFuncTable3 _gActor303600ShaftSegmentStates = { {
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

    stateHandlers = _gActor303600ShaftSegmentStates;
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
    for (lightIndex = 0, light = _gActor303600ShaftSegmentLights; lightIndex < (s32)ARRAY_SIZE(_gActor303600ShaftSegmentLights); lightIndex++, light++) {
        gfxSetFlatLight(lightIndex, light, &lightingWork->lightMtx, &lightingWork->colorMtx);
    }
}

/// Dispatches shaft initialization, scrolling or teardown while actors run.
///
/// Requires a live coordinate body and state 0..2. Any other actor-control value
/// holds every state, including initialization and teardown. The copied table
/// is indexed without a bounds check; the selected callback may release the task.
static void _actor303600ShaftTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gActor303600ShaftTaskStates;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        stateHandlers.funcs[task->state](task);
    }
}
