#include "actors/task_tables.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/rand.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/companion_load.h"
#include "gameplay/captions.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gameflow.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

#include "rooms/dryfield_r08.h"
#include "../../shared/screen/screen_wave.h"
#include "../../shared/actor/actor_messages.h"

/// Scene step of Eve, stored in `_Actor121300EveWork::step`.
///
/// The event script selects one at each of its cues and the double's tick
/// carries it out every frame. A step that has to happen only once puts
/// `ACTOR_121300_STEP_NONE` back when it is done; the others run until the
/// script selects the next.
enum {
    ACTOR_121300_STEP_NONE               = 0,  // Nothing beyond ticking the animation
    ACTOR_121300_STEP_REPLACE_PLAYER     = 1,  // Hide the player's model, put the double on its mark and restart animation set 1 (once)
    ACTOR_121300_STEP_PLAY_SET_2         = 2,  // Blend every part into animation set 2 (once)
    ACTOR_121300_STEP_SHATTER_LAMPS      = 3,  // Shatter the room's lamps one after another
    ACTOR_121300_STEP_SHATTER_LAMPS_WAVE = 4,  // Slow the debris to a tenth and start the long screen wave, then keep shattering
    ACTOR_121300_STEP_END_SHATTER        = 5,  // Finish the screen wave, release the debris and restore the stream's image mode (once)
    ACTOR_121300_STEP_HOLD_ON_MARK       = 6,  // Hold the double at the start of set 1 on its mark under the room's second lights, spawning the ground ring and the upper row of drift sprites
    ACTOR_121300_STEP_PLAY_SET_3         = 7,  // Blend every part into animation set 3, then spawn the ground ring of drift sprites
    ACTOR_121300_STEP_SHATTER_LAMPS_SLOW = 8,  // Slow the debris to three tenths and keep shattering
    ACTOR_121300_STEP_SPRITES_DWINDLE    = 9,  // Spawn the ground ring, and the upper row with a point fewer every 20 ticks
    ACTOR_121300_STEP_WAVE_PULSE         = 10, // Short screen wave: 8 ticks rising and held, then left to fall (ends the step)
    ACTOR_121300_STEP_RAISED_RING        = 11, // Spawn the raised ring of drift sprites
    ACTOR_121300_STEP_MASK_STREAM        = 12, // Have the stream's images decoded with the mask bit set (once)
};

/// Progress of the lamp shattering, stored in
/// `_Actor121300EveWork::shatterState`.
enum {
    ACTOR_121300_SHATTER_START   = 0, // Enable the debris and clear the lamp count and timer
    ACTOR_121300_SHATTER_RUNNING = 1, // Put out one lamp every `ACTOR_121300_SHATTER_INTERVAL` ticks
};

enum {
    /// Ticks of a shattering step between one lamp going out and the next.
    ACTOR_121300_SHATTER_INTERVAL = 3,
    /// Value of `_Actor121300Lamp::endMarker` in the record closing the lamp table.
    ACTOR_121300_LAMP_END = -1,
};

/// Work block of Eve, the body model this package animates over
/// the streamed scene, and the state of that scene.
///
/// The task's setup allocates it zeroed and keeps it at `Task::work` for the
/// task's life. The model object borrows `lightMtx` and `colorMtx`, the
/// animation context is bound to `rig`, and the screen-wave task borrows
/// `wave`, so the block has to outlive all three.
///
/// The event script drives the scene by storing a step; the double's tick
/// carries the step out. The steps shatter the room's twelve lamps one at a
/// time, distort the frame with a screen wave and spawn drift sprites around
/// the room.
typedef struct {
    ActorAnimRig19 rig;                 // Playback storage of the nineteen-part body model; slots 1 to 18 are driven
    MATRIX         lightMtx;            // Light matrix the model object is lit with
    MATRIX         colorMtx;            // Light colour matrix the model object is lit with
    ScreenWaveCtx  wave;                // Ramp of the screen wave the scene runs; the wave task reads it every frame
    Task*          player;              // The player's task, whose model the double stands in for
    Task*          waveTask;            // The screen-wave task as last spawned; stored and never read
    byte           field_490[0x8];      // Allocated but never accessed; role unproven
    u16            step;                // Scene step being carried out (`ACTOR_121300_STEP_*`)
    u16            stepState;           // Progress within the step, cleared when the script selects one (0 its opening work is still to do)
    s16            pulseFrames;         // Ticks since `ACTOR_121300_STEP_WAVE_PULSE` started its wave
    s16            lampCursor;          // Lamp whose `endMarker` gates the shattering; nothing advances it, so it stays the first lamp
    u16            animSet;             // Index of the animation set the driven slots play
    u16            shatterState;        // (`ACTOR_121300_SHATTER_START`, `ACTOR_121300_SHATTER_RUNNING`)
    s16            shatterFrames;       // Ticks of shattering since the last lamp went out
    s16            lampsShattered;      // Lamps put out so far, which is also the index of the next lamp to go
    s16            spritePointsDropped; // Points at the end of the upper drift-sprite row no longer spawned at
    s16            spriteDropFrames;    // Ticks of `ACTOR_121300_STEP_SPRITES_DWINDLE` since a point was last dropped
    s16            texturePageOffset;   // Texture relocation for image uploads, in 64-word VRAM columns
} _Actor121300EveWork;
STATIC_ASSERT_SIZEOF(_Actor121300EveWork, 0x4B0);

/// One of the room's overhead lamps, in the order the scene shatters them.
///
/// The positions are those the room draws its twelve lamp glows at. A record
/// with `ACTOR_121300_LAMP_END` closes the table.
typedef struct {
    s16 x;         // World X of the lamp
    s16 y;         // World Y of the lamp
    s16 z;         // World Z of the lamp
    s16 endMarker; // (0 a lamp, `ACTOR_121300_LAMP_END` the closing record)
} _Actor121300Lamp;
STATIC_ASSERT_SIZEOF(_Actor121300Lamp, 0x8);

/// Work block of one piece of lamp debris, kept at `Task::work`.
///
/// The piece's first state allocates it zeroed and rolls the velocity, the
/// spin and the delay from `gRandomLcgState`. The model object borrows the
/// two matrices for the task's life. Velocity and spin are per tick at full
/// speed; each tick applies the scene's speed percentage to them.
typedef struct {
    MATRIX lightMtx;      // Light matrix the model object is lit with
    MATRIX colorMtx;      // Light colour matrix the model object is lit with
    s16    rotX;          // Rotation about X, 4096 units per turn
    s16    rotY;          // Rotation about Y, 4096 units per turn
    s16    rotZ;          // Rotation about Z, 4096 units per turn
    byte   field_46[0x2]; // Allocated but never accessed; role unproven
    s16    spinX;         // Change of `rotX` per tick (-127..127)
    s16    spinY;         // Change of `rotY` per tick (-127..127)
    s16    spinZ;         // Change of `rotZ` per tick (-127..127)
    byte   field_4E[0x2]; // Allocated but never accessed; role unproven
    s16    velX;          // World X travelled per tick
    s16    velY;          // World Y travelled per tick; starts 3 to 6 downward and gains 3 a tick at full speed
    s16    velZ;          // World Z travelled per tick
    byte   field_56[0x2]; // Allocated but never accessed; role unproven
    s16    delay;         // Ticks left before the piece gets its model buffers and starts to fall (0-3)
} _Actor121300DebrisWork;
STATIC_ASSERT_SIZEOF(_Actor121300DebrisWork, 0x5C);

/// Main-executable globals with no module header yet: `gPlayerStatus.weapon` is the
/// equipped-weapon index the slot-3 message 0x3E8 record is keyed on,
/// `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId` picks which of the two weapon-id bases that record uses, and
/// `gDisplayState.pendingMode` / `gAttachmentState.mode` (1 while the attachment wheel is open) gate the actor's setup.

static s32 _gScreenWaveAmplitude;

static ScreenWaveCtx* _gScreenWaveContext;

// These first declarations retain BSS order ahead of the oscillator arrays.
static Task* _gActor121300DoubleTask;
static u16   _gActor121300DebrisEnabled;

ScreenWaveOscillator _gScreenWaveColumnWaves[13];

ScreenWaveOscillator _gScreenWaveRowWaves[30];

enum { ACTOR_121300_MESSAGE_SET_TEXTURE_SEQUENCE = 2016 };

static s32 _actor121300SetTextureSequence(Task* task, s32 unusedMessageId, s32 textureSequence, s32 unusedArg);

static TmdSource _gEveBody;
static TmdSource _gActor121300Model07E84;
static TmdSource _gActor121300Model080F0;
static TmdSource _gActor121300Model0834C;
static TmdSource _gActor121300Model08580;
static TmdSource _gActor121300Model08768;
static void      _actor121300FadeInTask(Task* task);
static void      _actor121300LampDebrisTask(Task* task);
static void      _actor121300SpawnLampDebrisTask(Task* task);
static void      _actor121300UploadTexturesTask(Task* task);
static void      _actor121300EveSceneTask(Task* task);
static void      _actor121300FadeOutTask(Task* task);
static void      _actor121300BlackoutTask(Task* task);
static void      _actor121300SelectSceneStep(s16 step);
static void      _actor121300RestoreStreamImageMode(void);
static void      _actor121300SkipScene(void);
static void      _actor121300StartFadeIn(s32 intensityStep);
static void      _actor121300StartFadeOut(s32 intensityStep);
static void      _actor121300SetDoubleDrawMode(s32 drawMode);
static void      _actor121300StageSceneAudioStart(void);
static void      _actor121300QueueScenePlayback(void);
static void      _actor121300FinishScene(void);

/// Execution priority of the scene double and its effect children.
enum { ACTOR_121300_SCENE_TASK_PRIORITY = 192 };

/// Full-rate percentage used by debris gravity, translation and spin.
enum { ACTOR_121300_DEBRIS_FULL_SPEED_PERCENT = 100 };

/// Negative follow-up entries leave the current animation set selected.
enum { ACTOR_121300_ANIMATION_NO_FOLLOWUP = -1 };

/// CAP command, fade rates and frame waits of the two scene programs.
enum {
    ACTOR_121300_SCENE_CAP_COMMAND           = 1, // CAP command slot
    ACTOR_121300_SCENE_PLAYBACK_DELAY_FRAMES = 2, // Updates before queuing stream playback
    ACTOR_121300_SCENE_FINISH_DELAY_FRAMES   = 3, // Updates after the final scene cue
    ACTOR_121300_SCENE_FADE_STEP             = 4, // Intensity units per update
    ACTOR_121300_SCENE_FINAL_FADE_IN_STEP    = 9, // Intensity units per update
    ACTOR_121300_SKIP_FADE_WAIT_FRAMES       = 8, // Updates before cancelling the scene
    ACTOR_121300_SKIP_CANCEL_WAIT_FRAMES     = 5, // Updates before releasing scene control
};

#include "assets/characters/eve/body.inc"

static TmdBone _gActor121300Model07E84Skeleton[1] = {
#include "assets/actor_121300_model_07E84_skeleton.inc"
};

static u32 _gActor121300Model07E84PartVerts[1] = {
#include "assets/actor_121300_model_07E84_partVerts.inc"
};

static SVECTOR _gActor121300Model07E84Verts[10] = {
#include "assets/actor_121300_model_07E84_verts.inc"
};

static SVECTOR _gActor121300Model07E84Normals[17] = {
#include "assets/actor_121300_model_07E84_normals.inc"
};

static u32 _gActor121300Model07E84Stream[80] = {
#include "assets/actor_121300_model_07E84_stream.inc"
};

static TmdSource _gActor121300Model07E84 = {
    0,
    500,
    0,
    1,
    _gActor121300Model07E84PartVerts,
    _gActor121300Model07E84Verts,
    _gActor121300Model07E84Normals,
    _gActor121300Model07E84Skeleton,
    _gActor121300Model07E84Stream,
};

static TmdBone _gActor121300Model080F0Skeleton[1] = {
#include "assets/actor_121300_model_080F0_skeleton.inc"
};

static u32 _gActor121300Model080F0PartVerts[1] = {
#include "assets/actor_121300_model_080F0_partVerts.inc"
};

static SVECTOR _gActor121300Model080F0Verts[10] = {
#include "assets/actor_121300_model_080F0_verts.inc"
};

static SVECTOR _gActor121300Model080F0Normals[18] = {
#include "assets/actor_121300_model_080F0_normals.inc"
};

static u32 _gActor121300Model080F0Stream[80] = {
#include "assets/actor_121300_model_080F0_stream.inc"
};

static TmdSource _gActor121300Model080F0 = {
    0,
    500,
    0,
    1,
    _gActor121300Model080F0PartVerts,
    _gActor121300Model080F0Verts,
    _gActor121300Model080F0Normals,
    _gActor121300Model080F0Skeleton,
    _gActor121300Model080F0Stream,
};

static TmdBone _gActor121300Model0834CSkeleton[1] = {
#include "assets/actor_121300_model_0834C_skeleton.inc"
};

static u32 _gActor121300Model0834CPartVerts[1] = {
#include "assets/actor_121300_model_0834C_partVerts.inc"
};

static SVECTOR _gActor121300Model0834CVerts[10] = {
#include "assets/actor_121300_model_0834C_verts.inc"
};

static SVECTOR _gActor121300Model0834CNormals[16] = {
#include "assets/actor_121300_model_0834C_normals.inc"
};

static u32 _gActor121300Model0834CStream[80] = {
#include "assets/actor_121300_model_0834C_stream.inc"
};

static TmdSource _gActor121300Model0834C = {
    0,
    500,
    0,
    1,
    _gActor121300Model0834CPartVerts,
    _gActor121300Model0834CVerts,
    _gActor121300Model0834CNormals,
    _gActor121300Model0834CSkeleton,
    _gActor121300Model0834CStream,
};

static TmdBone _gActor121300Model08580Skeleton[1] = {
#include "assets/actor_121300_model_08580_skeleton.inc"
};

static u32 _gActor121300Model08580PartVerts[1] = {
#include "assets/actor_121300_model_08580_partVerts.inc"
};

static SVECTOR _gActor121300Model08580Verts[8] = {
#include "assets/actor_121300_model_08580_verts.inc"
};

static SVECTOR _gActor121300Model08580Normals[13] = {
#include "assets/actor_121300_model_08580_normals.inc"
};

static u32 _gActor121300Model08580Stream[61] = {
#include "assets/actor_121300_model_08580_stream.inc"
};

static TmdSource _gActor121300Model08580 = {
    0,
    368,
    0,
    1,
    _gActor121300Model08580PartVerts,
    _gActor121300Model08580Verts,
    _gActor121300Model08580Normals,
    _gActor121300Model08580Skeleton,
    _gActor121300Model08580Stream,
};

static TmdBone _gActor121300Model08768Skeleton[1] = {
#include "assets/actor_121300_model_08768_skeleton.inc"
};

static u32 _gActor121300Model08768PartVerts[1] = {
#include "assets/actor_121300_model_08768_partVerts.inc"
};

static SVECTOR _gActor121300Model08768Verts[8] = {
#include "assets/actor_121300_model_08768_verts.inc"
};

static SVECTOR _gActor121300Model08768Normals[13] = {
#include "assets/actor_121300_model_08768_normals.inc"
};

static u32 _gActor121300Model08768Stream[61] = {
#include "assets/actor_121300_model_08768_stream.inc"
};

static TmdSource _gActor121300Model08768 = {
    0,
    368,
    0,
    1,
    _gActor121300Model08768PartVerts,
    _gActor121300Model08768Verts,
    _gActor121300Model08768Normals,
    _gActor121300Model08768Skeleton,
    _gActor121300Model08768Stream,
};

#include "assets/rooms/dryfield/r08/eve_clips.inc"

/// Spawn recipe for the scene's per-frame screen distortion, followed by a table end.
///
/// Spawn only index 0 with argument 2 borrowing a positive-span `ScreenWaveCtx`.
/// Keep the context and this overlay live through the wave's final draw; waves
/// must not overlap because they share the renderer's oscillator storage.
static TaskDesc _gActor121300ScreenWaveTasks[2] = {
    { { { TASK_BODY_NONE, ACTOR_121300_SCENE_TASK_PRIORITY } }, _screenWaveTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

/// Current sine-displacement strength, recomputed as frame * scale / span.
///
/// Strength 32 corresponds to one pixel at the sine peak. The seed tick resets
/// the initial 256 to zero; the context must have a positive span while drawing.
static s32 _gScreenWaveAmplitude = 256;

/// Open-eye patch of the double's 8-bit body texture.
///
/// Packed as 250 32-bit transfer words (50 by 20 texels). Sequence 0 installs it; sequence 2
/// restores it after the half-closed patch. Uploaded at base VRAM (384, 320),
/// relocated by the double's 64-word texture-page offset.
static u_long _gActor121300EyesOpenPixels[250] = {
#include "assets/characters/eve/eyes_open.inc"
};

/// Half-closed-eye patch between the double's open and closed eye states.
///
/// Packed as 250 32-bit transfer words (50 by 20 8-bit texels), uploaded at base VRAM
/// (384, 320) on the first update of both closing and reopening sequences.
/// The double's texture-page offset relocates X in 64-word columns.
static u_long _gActor121300EyesHalfClosedPixels[250] = {
#include "assets/characters/eve/eyes_half_closed.inc"
};

/// Closed-eye patch completing the double's blink-closing sequence.
///
/// Packed as 250 32-bit transfer words (50 by 20 8-bit texels), uploaded at base VRAM
/// (384, 320) on sequence 1's second update. The double's texture-page offset
/// relocates X in 64-word columns.
static u_long _gActor121300EyesClosedPixels[250] = {
#include "assets/characters/eve/eyes_closed.inc"
};

/// Closed-mouth patch installed by the double's initial texture sequence.
///
/// Packed as 140 32-bit transfer words (28 by 20 8-bit texels), uploaded at base VRAM
/// (396, 416), with X relocated by the double's 64-word texture-page offset.
static u_long _gActor121300MouthClosedPixels[140] = {
#include "assets/characters/eve/mouth_closed.inc"
};

/// Open-mouth patch selected by the double's texture sequences 4 and 5.
///
/// Packed as 140 32-bit transfer words (28 by 20 8-bit texels), uploaded at base VRAM
/// (396, 416), with X relocated by the double's 64-word texture-page offset.
static u_long _gActor121300MouthOpenPixels[140] = {
#include "assets/characters/eve/mouth_open.inc"
};

/// Scene update counter whose low two bits gate drift sprites every fourth update.
///
/// Starts at zero when the overlay loads and is not reset when the double starts.
/// Stored and incremented as a signed word; only the periodic gate reads it.
static s32 _gActor121300SceneFrame = 0;

/// Debris time scale in signed-halfword percent, initially 100 for full speed.
///
/// Scene steps select 30 or 10. Each gravity, translation and spin calculation
/// reads it separately; assignments retain the signed low halfword without clamping.
static s16 _gActor121300DebrisSpeedPercent = ACTOR_121300_DEBRIS_FULL_SPEED_PERCENT;

/// Borrowed animation sets of the double: index 0 unused, active sets 1 through 3.
///
/// The animation context retains this table and its sets for the double's life.
/// Set 1 holds its mark; sets 2 and 3 are selected by scene cues.
#include "assets/rooms/dryfield/r08/eve_bank.inc"

/// Follow-up animation set for each double set once all driven tracks settle.
///
/// Indexed by active set 1..3; index 0 is unused. Negative entries mean no
/// follow-up. All four entries disable it, so the selected set stays active.
static s16 _gActor121300AnimationFollowupSets[4] = {
    ACTOR_121300_ANIMATION_NO_FOLLOWUP,
    ACTOR_121300_ANIMATION_NO_FOLLOWUP,
    ACTOR_121300_ANIMATION_NO_FOLLOWUP,
    ACTOR_121300_ANIMATION_NO_FOLLOWUP,
};

/// Twelve overhead lamps in shattering order, followed by a closing record.
///
/// Positions use world-coordinate units. Debris spawn arguments must index
/// 0..11; record 12 only closes the sentinel walk. Shattering checks the separate,
/// unadvanced lamp cursor, so its cue timing must stop further debris spawns.
static _Actor121300Lamp _gActor121300Lamps[13] = {
    { 4950, -1750, 2570, 0 },
    { 5170, -1750, 2570, 0 },
    { 5340, -1750, 2700, 0 },
    { 5410, -1750, 2900, 0 },
    { 5340, -1750, 3100, 0 },
    { 5170, -1750, 3230, 0 },
    { 4950, -1750, 3230, 0 },
    { 4770, -1750, 3100, 0 },
    { 4690, -1750, 2900, 0 },
    { 4770, -1750, 2700, 0 },
    { 4900, -1750, 2900, 0 },
    { 5200, -1750, 2900, 0 },
    { 0, 0, 0, ACTOR_121300_LAMP_END },
};

/// The double's placement, draw-mode and texture-sequence message handlers.
///
/// Dispatch only `ACTOR_MESSAGE_PLACE`, `ACTOR_MESSAGE_SET_MODEL_DRAW` or
/// `ACTOR_121300_MESSAGE_SET_TEXTURE_SEQUENCE`: this exact three-entry table
/// has no end marker. Placement borrows an `ActorTransform` during dispatch;
/// draw modes use `ACTOR_MESSAGE_DRAW_*`. Texture sequences 0, 1, 2, 4 and 5
/// upload patches; 3 and unsupported selectors retain an idle upload task.
/// The texture handler returns the child task address as a message word; the
/// scene's direct and CAP senders discard results. Keep work and code live.
static TaskMessageEntry _gActor121300DoubleMessages[3] = {
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceYawPitchRoll },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, actorMsgSetDrawMode },
    { ACTOR_121300_MESSAGE_SET_TEXTURE_SEQUENCE, _actor121300SetTextureSequence },
};

/// The scene double's standing mark in its model root's parent frame.
///
/// Placement messages borrow this record synchronously; all three rotations
/// are zero on the 4096-units-per-turn angle scale.
static ActorTransform _gActor121300DoubleMark = { .pos = { 5140, -140, 3010, 0 }, .rot = { 0, 0, 0, 0 } };

/// Seventeen ground-level drift-sprite points, followed by an X-zero sentinel.
///
/// World coordinates outline the room at Y zero; the spawner walks to X zero
/// and adds X jitter. The final zero record is not a spawn point.
static SVECTOR _gActor121300GroundRingPoints[18] = {
    { 2500, 0, 4700, 0 },
    { 3000, 0, 4700, 0 },
    { 3500, 0, 4700, 0 },
    { 4000, 0, 4700, 0 },
    { 4500, 0, 4700, 0 },
    { 5000, 0, 4700, 0 },
    { 5500, 0, 4700, 0 },
    { 6000, 0, 4700, 0 },
    { 6500, 0, 4700, 0 },
    { 6500, 0, 1500, 0 },
    { 5500, 0, 1500, 0 },
    { 4500, 0, 1500, 0 },
    { 4000, 0, 1500, 0 },
    { 6000, 0, 1500, 0 },
    { 5000, 0, 1500, 0 },
    { 2550, 0, 3800, 0 },
    { 2550, 0, 4600, 0 },
    { 0, 0, 0, 0 },
};

/// Fifteen raised drift-sprite points, followed by an X-zero sentinel.
///
/// World Y is -200 along the far edge and -300 along the near edge. The
/// spawner walks to X zero and adds X jitter; the closing record is not spawned.
static SVECTOR _gActor121300RaisedRingPoints[16] = {
    { 2500, -200, 4700, 0 },
    { 3000, -200, 4700, 0 },
    { 3500, -200, 4700, 0 },
    { 4000, -200, 4700, 0 },
    { 4500, -200, 4700, 0 },
    { 5000, -200, 4700, 0 },
    { 5500, -200, 4700, 0 },
    { 6000, -200, 4700, 0 },
    { 6500, -200, 4700, 0 },
    { 6500, -300, 1500, 0 },
    { 5500, -300, 1500, 0 },
    { 4500, -300, 1500, 0 },
    { 4000, -300, 1500, 0 },
    { 6000, -300, 1500, 0 },
    { 5000, -300, 1500, 0 },
    { 0, 0, 0, 0 },
};

/// Six drift-sprite points at world Y -500, followed by a zero closing record.
///
/// The spawner uses only the active prefix, removing trailing points as the
/// scene dwindles. It adds X jitter; the seventh stored record is never spawned.
static SVECTOR _gActor121300UpperRowPoints[7] = {
    { 6500, -500, 2000, 0 },
    { 5500, -500, 2000, 0 },
    { 4500, -500, 2000, 0 },
    { 4000, -500, 2000, 0 },
    { 6000, -500, 2000, 0 },
    { 5000, -500, 2000, 0 },
    { 0, 0, 0, 0 },
};

/// Stream-selection key for the double's scene: group 2, stream 13, qualifier 11.
///
/// The event interpreter lends its address to CAP/CD scene selection. Keep
/// this overlay loaded throughout playback; the absent second qualifier is zero.
static EvsSceneKey _gActor121300SceneKey = { .group = 2, .streamId = 13, .subId = 11 };

/// CAP-cued lamp-shattering scene with Eve over streamed images.
///
/// Borrows the initialized double, room resources and selected stream through
/// completion. Starts CAP command 1, drives animation, blink textures, lamp
/// debris, waves and fades, then ends with a three-update delay and opcode END.
/// The live scene has an alternate skip program in `_gActor121300SceneSkipScript`.
static EvsCommand _gActor121300SceneScript[52] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_121300_SCENE_CAP_COMMAND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor121300SelectSceneStep }, { .value = ACTOR_121300_STEP_REPLACE_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = SetDispMask }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &_gActor121300SceneKey }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor121300StageSceneAudioStart }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = ACTOR_121300_SCENE_PLAYBACK_DELAY_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor121300QueueScenePlayback }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor121300StartFadeIn }, { .value = ACTOR_121300_SCENE_FADE_STEP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor121300SelectSceneStep }, { .value = ACTOR_121300_STEP_PLAY_SET_2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor121300SelectSceneStep }, { .value = ACTOR_121300_STEP_MASK_STREAM }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor121300SetDoubleDrawMode }, { .value = ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor121300SelectSceneStep }, { .value = ACTOR_121300_STEP_WAVE_PULSE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor121300SelectSceneStep }, { .value = ACTOR_121300_STEP_SHATTER_LAMPS }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor121300SelectSceneStep }, { .value = ACTOR_121300_STEP_SHATTER_LAMPS_SLOW }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor121300SelectSceneStep }, { .value = ACTOR_121300_STEP_SHATTER_LAMPS_WAVE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor121300StartFadeOut }, { .value = ACTOR_121300_SCENE_FADE_STEP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor121300SetDoubleDrawMode }, { .value = ACTOR_MESSAGE_DRAW_SHOW }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = SetDispMask }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor121300SelectSceneStep }, { .value = ACTOR_121300_STEP_END_SHATTER }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor121300SelectSceneStep }, { .value = ACTOR_121300_STEP_HOLD_ON_MARK }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor121300StartFadeIn }, { .value = ACTOR_121300_SCENE_FADE_STEP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor121300SelectSceneStep }, { .value = ACTOR_121300_STEP_SPRITES_DWINDLE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor121300SelectSceneStep }, { .value = ACTOR_121300_STEP_PLAY_SET_3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor121300StartFadeOut }, { .value = ACTOR_121300_SCENE_FADE_STEP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor121300SetDoubleDrawMode }, { .value = ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor121300StartFadeIn }, { .value = ACTOR_121300_SCENE_FINAL_FADE_IN_STEP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor121300SelectSceneStep }, { .value = ACTOR_121300_STEP_RAISED_RING }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor121300StartFadeOut }, { .value = ACTOR_121300_SCENE_FADE_STEP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor121300RestoreStreamImageMode }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor121300FinishScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor121300SelectSceneStep }, { .value = ACTOR_121300_STEP_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = ACTOR_121300_SCENE_FINISH_DELAY_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Skip program that fades and restores the double's scene before event cleanup.
///
/// Waits eight updates after starting the primary fade, cancels the scene's
/// effects and stream with live double work, then waits five updates before
/// releasing scene control. Opcode END closes this seven-command program.
static EvsCommand _gActor121300SceneSkipScript[7] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = ACTOR_121300_SKIP_FADE_WAIT_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor121300SelectSceneStep }, { .value = ACTOR_121300_STEP_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor121300SkipScene }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = ACTOR_121300_SKIP_CANCEL_WAIT_FRAMES }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc gActor121300SceneTasks[11] = {
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), ACTOR_121300_SCENE_TASK_PRIORITY } }, _actor121300EveSceneTask, { .model = &_gEveBody } },
    { { { TASK_BODY_NONE, ACTOR_121300_SCENE_TASK_PRIORITY } }, _actor121300FadeInTask, { .value = 0 } },
    { { { TASK_BODY_NONE, ACTOR_121300_SCENE_TASK_PRIORITY } }, _actor121300FadeOutTask, { .value = 0 } },
    { { { TASK_BODY_NONE, ACTOR_121300_SCENE_TASK_PRIORITY } }, _actor121300SpawnLampDebrisTask, { .value = 0 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), ACTOR_121300_SCENE_TASK_PRIORITY } }, _actor121300LampDebrisTask, { .model = &_gActor121300Model07E84 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), ACTOR_121300_SCENE_TASK_PRIORITY } }, _actor121300LampDebrisTask, { .model = &_gActor121300Model080F0 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), ACTOR_121300_SCENE_TASK_PRIORITY } }, _actor121300LampDebrisTask, { .model = &_gActor121300Model0834C } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), ACTOR_121300_SCENE_TASK_PRIORITY } }, _actor121300LampDebrisTask, { .model = &_gActor121300Model08580 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), ACTOR_121300_SCENE_TASK_PRIORITY } }, _actor121300LampDebrisTask, { .model = &_gActor121300Model08768 } },
    { { { TASK_BODY_NONE, ACTOR_121300_SCENE_TASK_PRIORITY } }, _actor121300BlackoutTask, { .value = 0 } },
    { { { TASK_BODY_NONE, ACTOR_121300_SCENE_TASK_PRIORITY } }, _actor121300UploadTexturesTask, { .value = 0 } },
};

/// Borrowed context of this renderer's active wave task.
///
/// Set on the seed tick and left unchanged at teardown. Only one wave may use
/// this storage; the context must stay live through its final drawing tick.
static ScreenWaveCtx* _gScreenWaveContext = NULL;

/// Borrowed singleton task of Eve, published after work allocation.
///
/// Step, draw and skip callbacks require successful initialization and a live
/// task/work block. Teardown does not clear this pointer; stop callbacks before
/// task release or overlay unload. This pointer does not own the task.
static Task* _gActor121300DoubleTask;

/// Lifetime gate for lamp debris and its spawners (0 release, nonzero enabled).
///
/// Starts at zero on overlay load. Shattering sets 1; normal completion or skip
/// clears it, and children kill themselves on their next update. Stored as a u16.
static u16 _gActor121300DebrisEnabled;

/// Column sine oscillators for horizontal screen displacement.
///
/// The per-frame renderer uses entries 0..10; the prebuilt-grid renderer uses
/// 0..8. Any remaining declared capacity is unaccessed; its role is unproven.
ScreenWaveOscillator _gScreenWaveColumnWaves[13];

/// Thirty active row sine oscillators for vertical screen displacement.
///
/// Seeded at wave startup and advanced on drawing updates. Any declared
/// entries beyond 29 are unaccessed; their role is unproven.
ScreenWaveOscillator _gScreenWaveRowWaves[30];

/// Packed argument for the alternate upward drift sprite: size numerator 1024,
/// two updates per cell, speed 32, movement kind 1, palette bank 0, alternate drawer.
enum { ACTOR_121300_DRIFT_SPRITE_ARGUMENT = 0x81202400 };

enum { ACTOR_121300_ANIMATION_BLEND_FRAMES = 10 };

enum { ACTOR_121300_SPRITE_FRAME_MASK = 3 };

enum {
    ACTOR_121300_DEBRIS_WAVE_SPEED_PERCENT = 10,
    ACTOR_121300_DEBRIS_SLOW_SPEED_PERCENT = 30,
};

static s32         _actor121300TickAnimations(Task* task);
static void        _actor121300SpawnRingSprites(Task* unusedTask, s16 raised);
static void        _actor121300SpawnUpperRowSprites(Task* task, s16 dwindle);
static void        _actor121300ShatterLamps(Task* task);
static inline void _actor121300BlendAnimationSet(Task* task, s32 animSet);
static inline void _actor121300SetDebrisSpeed(s32 speedPercent);
static void        _actor121300RunSceneStep(Task* task);
static void        _actor121300InitBody(Task* task);

#include "../../shared/screen/screen_wave.inc.c"

#include "../../shared/screen/screen_fade_step_down.inc.c"

/// Reveals the streamed scene after three black updates, enabling output on the third.
///
/// Start at state 0. Owns primary-heap `ScreenFadeWork` until task teardown;
/// allocation failure kills the task. `spawnArg1`'s unsigned low halfword is
/// intensity removed per reveal update, with signed-halfword narrowing and no
/// clamping. Zero holds indefinitely; negative stored red ends the task.
/// Requires the frame packet arena and foreground ordering tag for `fadeDrawOverlay`.
static void _actor121300FadeInTask(Task* task)
{
    enum {
        ACTOR_121300_FADE_IN_INITIALIZE     = 0,
        ACTOR_121300_FADE_IN_HOLD_BLACK     = 1,
        ACTOR_121300_FADE_IN_ENABLE_DISPLAY = 2,
        ACTOR_121300_FADE_IN_REVEAL         = 3,
        ACTOR_121300_FADE_IN_MAX_INTENSITY  = 255,
    };
    ScreenFadeWork* fade;
    ScreenFadeWork* allocatedFade;

    fade = task->work;
    switch (task->state) {
        case ACTOR_121300_FADE_IN_INITIALIZE:
            allocatedFade = memMalloc(sizeof(*allocatedFade), false);
            task->work    = allocatedFade;
            if (allocatedFade == NULL) {
                taskKill(task);
                return;
            }
            fade    = allocatedFade;
            fade->r = ACTOR_121300_FADE_IN_MAX_INTENSITY;
            fade->g = ACTOR_121300_FADE_IN_MAX_INTENSITY;
            fade->b = ACTOR_121300_FADE_IN_MAX_INTENSITY;
            fadeDrawOverlay(fade->r, fade->g, fade->b, GPU_BLEND_SUBTRACT);
            task->state += 1;
            break;
        case ACTOR_121300_FADE_IN_ENABLE_DISPLAY:
            // Keep the frame black while enabling output before the reveal.
            SetDispMask(1);
            /* fallthrough */
        case ACTOR_121300_FADE_IN_HOLD_BLACK:
            fadeDrawOverlay(fade->r, fade->g, fade->b, GPU_BLEND_SUBTRACT);
            task->state += 1;
            break;
        case ACTOR_121300_FADE_IN_REVEAL:
            fadeDrawOverlay(fade->r, fade->g, fade->b, GPU_BLEND_SUBTRACT);
            _screenFadeStepDown(fade, task);
            if (fade->r < 0) {
                taskKill(task);
            }
            break;
    }
}

/// Ticks the double's driven animation slots and starts a configured follow-up set.
///
/// Requires live `_Actor121300EveWork` and an animation-set index in 1..3.
/// Slots 1..18 are driven; slot 0 is left alone. Returns 1 when all driven slots
/// are settled, including when a nonnegative follow-up table entry starts a
/// ten-frame blend at frame 0; returns 0 while any slot is unsettled.
static s32 _actor121300TickAnimations(Task* task)
{
    _Actor121300EveWork* work;
    _Actor121300EveWork* blendWork;
    u16                  slotIndex;
    u16                  allSettled;
    u16                  nextAnimSet;

    work = task->work;
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        animationTickSlot(&work->rig.anim, slotIndex);
    }
    // Keep the unsettled path beside the reseed loop to retain the original block order.
    slotIndex  = 1;
    allSettled = 1;
    for (; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        if (!(work->rig.slots[slotIndex].status.fields.flags & ANIMATION_SLOT_SETTLED)) {
            goto unsettled;
        }
    }
checkSettled:
    if (allSettled) {
        if (_gActor121300AnimationFollowupSets[work->animSet] >= 0) {
            nextAnimSet        = _gActor121300AnimationFollowupSets[work->animSet];
            blendWork          = task->work;
            blendWork->animSet = nextAnimSet;
            goto blendSlots;
        unsettled:
            allSettled = 0;
            goto checkSettled;
        blendSlots:
            for (slotIndex = 1; slotIndex < ARRAY_SIZE(blendWork->rig.slots); slotIndex++) {
                animationSeekSlotWithBlend(&blendWork->rig.anim, slotIndex, nextAnimSet, 0, ACTOR_121300_ANIMATION_BLEND_FRAMES);
            }
        }
        return 1;
    }
    return 0;
}

/// Rolls one lamp fragment's angular velocity at full scene speed.
///
/// Returns -127..127 in 4096-units-per-turn angles per update. Advances the
/// shared unsigned LCG twice: bit 16 of the first sample chooses the sign
/// (set is positive), and bits 16..22 of the second give the magnitude.
static inline s16 _actor121300RollDebrisSpin(void)
{
    enum { ACTOR_121300_DEBRIS_SPIN_MAGNITUDE_MASK = 127 };
    s16 spin;

    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    if ((gRandomLcgState >> 16) & 1) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        spin            = (gRandomLcgState >> 16) & ACTOR_121300_DEBRIS_SPIN_MAGNITUDE_MASK;
    } else {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        spin            = -((gRandomLcgState >> 16) & ACTOR_121300_DEBRIS_SPIN_MAGNITUDE_MASK);
    }
    return spin;
}

/// Delays, spins and drops one model fragment from a shattered lamp.
///
/// Start at state 0 with a TMD body. `spawnArg1.value` selects a real lamp (0..11)
/// and `spawnArg2.unsignedValue` selects fragment placement 0..14. Owns its
/// primary-heap work until task teardown; the model borrows the work's lighting
/// matrices. Allocation failure or a cleared scene debris flag kills the task.
/// Motion uses game-coordinate units and angles use 4096 units per turn, scaled
/// by the scene speed percentage. Releases the task at view-parent Y >= -499.
static void _actor121300LampDebrisTask(Task* task)
{
    enum {
        ACTOR_121300_DEBRIS_INITIALIZE = 0,
        ACTOR_121300_DEBRIS_DELAY      = 1,
        ACTOR_121300_DEBRIS_FALL       = 2,
        ACTOR_121300_DEBRIS_DESPAWN_Y  = -499,
    };
    _Actor121300DebrisWork* work;
    TmdObject*              model;
    GfxCoord*               rootCoord;
    VECTOR                  lightingPosition;
    _Actor121300DebrisWork* allocatedWork;
    s16                     lateralVelocity;
    TmdObject*              lightingModel;

    work      = task->work;
    model     = task->extra.tmd;
    rootCoord = model->coords;
    if (_gActor121300DebrisEnabled == 0) {
        taskKill(task);
        return;
    }
    switch (task->state) {
        case ACTOR_121300_DEBRIS_INITIALIZE:
            allocatedWork = memMalloc(sizeof(*allocatedWork), false);
            task->work    = allocatedWork;
            if (allocatedWork == NULL) {
                taskKill(task);
                return;
            }
            work = allocatedWork;
            memFillBytes(work, 0, sizeof(*work));
            rootCoord->parent     = &gGfxViewCoord;
            rootCoord->coord.t[0] = _gActor121300Lamps[task->spawnArg1.value].x;
            rootCoord->coord.t[1] = _gActor121300Lamps[task->spawnArg1.value].y;
            rootCoord->coord.t[2] = _gActor121300Lamps[task->spawnArg1.value].z;
            switch (task->spawnArg2.unsignedValue) {
                case 0:
                case 14:
                    break;
                case 1:
                    rootCoord->coord.t[0] += 50;
                    rootCoord->coord.t[1] += 50;
                    break;
                case 2:
                    rootCoord->coord.t[0] -= 50;
                    rootCoord->coord.t[1] += 50;
                    break;
                case 3:
                    rootCoord->coord.t[0] += 50;
                    rootCoord->coord.t[1] -= 50;
                    break;
                case 4:
                    rootCoord->coord.t[0] -= 50;
                    rootCoord->coord.t[1] -= 50;
                    break;
                case 5:
                    rootCoord->coord.t[0] += 80;
                    rootCoord->coord.t[1] += 80;
                    break;
                case 6:
                    rootCoord->coord.t[0] -= 80;
                    rootCoord->coord.t[1] += 80;
                    break;
                case 7:
                    rootCoord->coord.t[0] += 80;
                    rootCoord->coord.t[1] -= 80;
                    break;
                case 8:
                    rootCoord->coord.t[0] -= 80;
                    rootCoord->coord.t[1] -= 80;
                    break;
                case 10:
                    rootCoord->coord.t[0] += 120;
                    rootCoord->coord.t[1] += 120;
                    break;
                case 11:
                    rootCoord->coord.t[0] -= 120;
                    rootCoord->coord.t[1] += 120;
                    break;
                case 12:
                    rootCoord->coord.t[0] += 120;
                    rootCoord->coord.t[1] -= 120;
                    break;
                case 13:
                    rootCoord->coord.t[0] -= 120;
                    rootCoord->coord.t[1] -= 120;
                    break;
            }
            model->lightMtx = &work->lightMtx;
            model->colorMtx = &work->colorMtx;

            // The positive lateral roll is 0..7; the negative roll is -17..-10.
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if ((gRandomLcgState >> 16) & 1) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                lateralVelocity = ((gRandomLcgState >> 16) + 10) & 7;
            } else {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                lateralVelocity = -10 - ((gRandomLcgState >> 16) & 7);
            }
            work->velX      = lateralVelocity;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->velY      = ((gRandomLcgState >> 16) & 3) + 3;

            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if ((gRandomLcgState >> 16) & 1) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                lateralVelocity = ((gRandomLcgState >> 16) + 10) & 7;
            } else {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                lateralVelocity = -10 - ((gRandomLcgState >> 16) & 7);
            }
            work->velZ = lateralVelocity;

            work->spinX     = _actor121300RollDebrisSpin();
            work->spinY     = _actor121300RollDebrisSpin();
            work->spinZ     = _actor121300RollDebrisSpin();
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->delay     = (gRandomLcgState >> 16) & 3;
            task->state++;
            break;
        case ACTOR_121300_DEBRIS_DELAY:
            if (work->delay == 0) {
                tmdAllocPrimitiveBuffer(model);
                model->flags = 0;
                task->state++;
            } else {
                work->delay--;
            }
            break;
        case ACTOR_121300_DEBRIS_FALL:
            // Apply the scene speed to gravity, translation and spin independently.
            work->velY            += _gActor121300DebrisSpeedPercent * 3 / ACTOR_121300_DEBRIS_FULL_SPEED_PERCENT;
            rootCoord->coord.t[0] += work->velX * _gActor121300DebrisSpeedPercent / ACTOR_121300_DEBRIS_FULL_SPEED_PERCENT;
            rootCoord->coord.t[1] += work->velY * _gActor121300DebrisSpeedPercent / ACTOR_121300_DEBRIS_FULL_SPEED_PERCENT;
            rootCoord->coord.t[2] += work->velZ * _gActor121300DebrisSpeedPercent / ACTOR_121300_DEBRIS_FULL_SPEED_PERCENT;
            work->rotX            += work->spinX * _gActor121300DebrisSpeedPercent / ACTOR_121300_DEBRIS_FULL_SPEED_PERCENT;
            work->rotY            += work->spinY * _gActor121300DebrisSpeedPercent / ACTOR_121300_DEBRIS_FULL_SPEED_PERCENT;
            work->rotZ            += work->spinZ * _gActor121300DebrisSpeedPercent / ACTOR_121300_DEBRIS_FULL_SPEED_PERCENT;
            gfxRotMatrixY(&rootCoord->coord, work->rotY, GRAPHICS_ROTATION_REPLACE);
            gfxRotMatrixX(&rootCoord->coord, work->rotX, GRAPHICS_ROTATION_COMPOSE);
            gfxRotMatrixZ(&rootCoord->coord, work->rotZ, GRAPHICS_ROTATION_COMPOSE);
            rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            if (rootCoord->coord.t[1] >= ACTOR_121300_DEBRIS_DESPAWN_Y) {
                taskKill(task);
            }
            break;
    }
    // Light the piece at its composed view-coordinate translation.
    lightingModel       = task->extra.tmd;
    lightingPosition.vx = lightingModel->coords->workm.t[0];
    lightingPosition.vy = task->extra.tmd->coords->workm.t[1];
    lightingPosition.vz = task->extra.tmd->coords->workm.t[2];
    worldCoordSetModelLighting(lightingModel, &lightingPosition, 0, ARRAY_SIZE(work->colorMtx.m[0]));
}

/// Scatters three copies of each debris model through the lamp's fifteen placements.
///
/// Copies the real lamp index (0..11) from the spawner's first argument.
/// Placements offset world X/Y by 50, 80 or 120 units; 0, 9 and 14 stay centred.
/// Each spawn is independent; allocation failure leaves that piece absent.
static inline void _actor121300ScatterLampDebris(Task* task)
{
    enum {
        ACTOR_121300_DEBRIS_MODEL_FIRST                      = 4,
        ACTOR_121300_DEBRIS_MODEL_SECOND                     = 5,
        ACTOR_121300_DEBRIS_MODEL_THIRD                      = 6,
        ACTOR_121300_DEBRIS_MODEL_FOURTH                     = 7,
        ACTOR_121300_DEBRIS_MODEL_FIFTH                      = 8,
        ACTOR_121300_DEBRIS_PLACEMENT_CENTER_FIRST           = 0,
        ACTOR_121300_DEBRIS_PLACEMENT_INNER_PLUS_X_PLUS_Y    = 1,
        ACTOR_121300_DEBRIS_PLACEMENT_INNER_MINUS_X_PLUS_Y   = 2,
        ACTOR_121300_DEBRIS_PLACEMENT_INNER_PLUS_X_MINUS_Y   = 3,
        ACTOR_121300_DEBRIS_PLACEMENT_INNER_MINUS_X_MINUS_Y  = 4,
        ACTOR_121300_DEBRIS_PLACEMENT_MIDDLE_PLUS_X_PLUS_Y   = 5,
        ACTOR_121300_DEBRIS_PLACEMENT_MIDDLE_MINUS_X_PLUS_Y  = 6,
        ACTOR_121300_DEBRIS_PLACEMENT_MIDDLE_PLUS_X_MINUS_Y  = 7,
        ACTOR_121300_DEBRIS_PLACEMENT_MIDDLE_MINUS_X_MINUS_Y = 8,
        ACTOR_121300_DEBRIS_PLACEMENT_CENTER_MIDDLE          = 9,
        ACTOR_121300_DEBRIS_PLACEMENT_OUTER_PLUS_X_PLUS_Y    = 10,
        ACTOR_121300_DEBRIS_PLACEMENT_OUTER_MINUS_X_PLUS_Y   = 11,
        ACTOR_121300_DEBRIS_PLACEMENT_OUTER_PLUS_X_MINUS_Y   = 12,
        ACTOR_121300_DEBRIS_PLACEMENT_OUTER_MINUS_X_MINUS_Y  = 13,
        ACTOR_121300_DEBRIS_PLACEMENT_CENTER_LAST            = 14,
    };

    taskSpawnFromTable(gActor121300SceneTasks, ACTOR_121300_DEBRIS_MODEL_FIRST, task->spawnArg1, ACTOR_121300_DEBRIS_PLACEMENT_CENTER_FIRST);
    taskSpawnFromTable(gActor121300SceneTasks, ACTOR_121300_DEBRIS_MODEL_SECOND, task->spawnArg1, ACTOR_121300_DEBRIS_PLACEMENT_INNER_PLUS_X_PLUS_Y);
    taskSpawnFromTable(gActor121300SceneTasks, ACTOR_121300_DEBRIS_MODEL_THIRD, task->spawnArg1, ACTOR_121300_DEBRIS_PLACEMENT_INNER_MINUS_X_PLUS_Y);
    taskSpawnFromTable(gActor121300SceneTasks, ACTOR_121300_DEBRIS_MODEL_FOURTH, task->spawnArg1, ACTOR_121300_DEBRIS_PLACEMENT_INNER_PLUS_X_MINUS_Y);
    taskSpawnFromTable(gActor121300SceneTasks, ACTOR_121300_DEBRIS_MODEL_FIFTH, task->spawnArg1, ACTOR_121300_DEBRIS_PLACEMENT_INNER_MINUS_X_MINUS_Y);
    taskSpawnFromTable(gActor121300SceneTasks, ACTOR_121300_DEBRIS_MODEL_FIRST, task->spawnArg1, ACTOR_121300_DEBRIS_PLACEMENT_MIDDLE_PLUS_X_PLUS_Y);
    taskSpawnFromTable(gActor121300SceneTasks, ACTOR_121300_DEBRIS_MODEL_SECOND, task->spawnArg1, ACTOR_121300_DEBRIS_PLACEMENT_MIDDLE_MINUS_X_PLUS_Y);
    taskSpawnFromTable(gActor121300SceneTasks, ACTOR_121300_DEBRIS_MODEL_THIRD, task->spawnArg1, ACTOR_121300_DEBRIS_PLACEMENT_MIDDLE_PLUS_X_MINUS_Y);
    taskSpawnFromTable(gActor121300SceneTasks, ACTOR_121300_DEBRIS_MODEL_FOURTH, task->spawnArg1, ACTOR_121300_DEBRIS_PLACEMENT_MIDDLE_MINUS_X_MINUS_Y);
    taskSpawnFromTable(gActor121300SceneTasks, ACTOR_121300_DEBRIS_MODEL_FIFTH, task->spawnArg1, ACTOR_121300_DEBRIS_PLACEMENT_CENTER_MIDDLE);
    taskSpawnFromTable(gActor121300SceneTasks, ACTOR_121300_DEBRIS_MODEL_FIRST, task->spawnArg1, ACTOR_121300_DEBRIS_PLACEMENT_OUTER_PLUS_X_PLUS_Y);
    taskSpawnFromTable(gActor121300SceneTasks, ACTOR_121300_DEBRIS_MODEL_SECOND, task->spawnArg1, ACTOR_121300_DEBRIS_PLACEMENT_OUTER_MINUS_X_PLUS_Y);
    taskSpawnFromTable(gActor121300SceneTasks, ACTOR_121300_DEBRIS_MODEL_THIRD, task->spawnArg1, ACTOR_121300_DEBRIS_PLACEMENT_OUTER_PLUS_X_MINUS_Y);
    taskSpawnFromTable(gActor121300SceneTasks, ACTOR_121300_DEBRIS_MODEL_FOURTH, task->spawnArg1, ACTOR_121300_DEBRIS_PLACEMENT_OUTER_MINUS_X_MINUS_Y);
    taskSpawnFromTable(gActor121300SceneTasks, ACTOR_121300_DEBRIS_MODEL_FIFTH, task->spawnArg1, ACTOR_121300_DEBRIS_PLACEMENT_CENTER_LAST);
}

/// Spawns fifteen model fragments from one shattered lamp, then ends itself.
///
/// Start at state 0 with no body. `spawnArg1.value` is a real lamp index (0..11).
/// The first update owns and clears an eight-byte primary-heap work allocation;
/// its contents are never accessed and their role is unproven. The next update
/// scatters the fragments. Allocation failure or a cleared scene debris flag
/// ends the task; default teardown frees the work. Spawned pieces are independent
/// tasks; this overlay and their model data must remain loaded until they end.
static void _actor121300SpawnLampDebrisTask(Task* task)
{
    enum {
        ACTOR_121300_DEBRIS_SPAWNER_INITIALIZE = 0,
        ACTOR_121300_DEBRIS_SPAWNER_SCATTER    = 1,
        ACTOR_121300_DEBRIS_SPAWNER_WORK_BYTES = 8,
    };
    void* reservedWork;

    if (_gActor121300DebrisEnabled == 0) {
        taskKill(task);
        return;
    }
    switch (task->state) {
        case ACTOR_121300_DEBRIS_SPAWNER_INITIALIZE:
            // Preserve the work reservation and the update before scattering.
            reservedWork = memMalloc(ACTOR_121300_DEBRIS_SPAWNER_WORK_BYTES, false);
            task->work   = reservedWork;
            if (reservedWork != NULL) {
                memFillBytes(reservedWork, 0, ACTOR_121300_DEBRIS_SPAWNER_WORK_BYTES);
                task->state += 1;
                return;
            }
            break;
        case ACTOR_121300_DEBRIS_SPAWNER_SCATTER:
            _actor121300ScatterLampDebris(task);
            break;
        default:
            return;
    }
    taskKill(task);
}

/// Uploads one image at the double's relocated texture page.
///
/// `task->spawnArg2.pointer` borrows a live double task with initialized work.
/// The parent's signed page offset adds 64 VRAM words per column to `baseXWords`;
/// the final X narrows to a signed halfword. X and width count 16-bit VRAM words,
/// Y and height count rows. The relocated rectangle must fit VRAM (1024 by 512).
/// `imageData` borrows word-aligned packed pixels, two VRAM words per `u_long`,
/// covering the rectangle area rounded up to a whole `u_long`. Keep the pixels
/// readable and unchanged until GPU transfer finishes; the SDK copies the RECT.
static inline void _actor121300UploadTexture(Task* task, s32 baseXWords, s16 row,
                                             s16 widthWords, s16 heightRows, u_long* imageData)
{
    enum { ACTOR_121300_TEXTURE_PAGE_WIDTH_WORDS = 64 };
    RECT                 uploadRect;
    s32                  relocatedXWords;
    Task*                parentTask;
    _Actor121300EveWork* parentWork;

    parentTask      = task->spawnArg2.pointer;
    parentWork      = parentTask->work;
    relocatedXWords = parentWork->texturePageOffset * ACTOR_121300_TEXTURE_PAGE_WIDTH_WORDS + baseXWords;
    uploadRect.x    = relocatedXWords;
    uploadRect.y    = row;
    uploadRect.w    = widthWords;
    uploadRect.h    = heightRows;
    LoadImage(&uploadRect, imageData);
}

/// Uploads the double's texture changes at its parent's relocated VRAM pages.
///
/// `spawnArg2.pointer` borrows a live double task and work for this child's life.
/// `spawnArg1.value` selects a sequence: 0 uploads both initial blocks and ends,
/// 1 uploads the two replacement blocks on successive updates, 2 uploads the
/// first replacement then restores the initial block, 4/5 replace the smaller
/// block and end. Selector 3 and unsupported selectors leave the task alive.
/// The parent offset is in 64-word VRAM columns; rectangles use word widths
/// and pixel rows. Image storage must remain readable through GPU transfer.
static void _actor121300UploadTexturesTask(Task* task)
{
    enum {
        ACTOR_121300_TEXTURE_INITIAL           = 0,
        ACTOR_121300_TEXTURE_REPLACE           = 1,
        ACTOR_121300_TEXTURE_RESTORE           = 2,
        ACTOR_121300_TEXTURE_NO_UPLOAD         = 3,
        ACTOR_121300_TEXTURE_REPLACE_SMALL_4   = 4,
        ACTOR_121300_TEXTURE_REPLACE_SMALL_5   = 5,
        ACTOR_121300_TEXTURE_FIRST_UPDATE      = 0,
        ACTOR_121300_TEXTURE_SECOND_UPDATE     = 1,
        ACTOR_121300_TEXTURE_LARGE_X_WORDS     = 384,
        ACTOR_121300_TEXTURE_LARGE_Y           = 320,
        ACTOR_121300_TEXTURE_LARGE_WIDTH_WORDS = 25,
        ACTOR_121300_TEXTURE_SMALL_X_WORDS     = 396,
        ACTOR_121300_TEXTURE_SMALL_Y           = 416,
        ACTOR_121300_TEXTURE_SMALL_WIDTH_WORDS = 14,
        ACTOR_121300_TEXTURE_HEIGHT            = 20,
    };
    // Relocate each image by the placement's 64-word texture-page offset.
    switch (task->spawnArg1.value) {
        case ACTOR_121300_TEXTURE_INITIAL:
            _actor121300UploadTexture(task, ACTOR_121300_TEXTURE_LARGE_X_WORDS, ACTOR_121300_TEXTURE_LARGE_Y, ACTOR_121300_TEXTURE_LARGE_WIDTH_WORDS, ACTOR_121300_TEXTURE_HEIGHT, _gActor121300EyesOpenPixels);
            _actor121300UploadTexture(task, ACTOR_121300_TEXTURE_SMALL_X_WORDS, ACTOR_121300_TEXTURE_SMALL_Y, ACTOR_121300_TEXTURE_SMALL_WIDTH_WORDS, ACTOR_121300_TEXTURE_HEIGHT, _gActor121300MouthClosedPixels);
            taskKill(task);
            break;
        case ACTOR_121300_TEXTURE_REPLACE:
            switch (task->state) {
                case ACTOR_121300_TEXTURE_FIRST_UPDATE:
                    _actor121300UploadTexture(task, ACTOR_121300_TEXTURE_LARGE_X_WORDS, ACTOR_121300_TEXTURE_LARGE_Y, ACTOR_121300_TEXTURE_LARGE_WIDTH_WORDS, ACTOR_121300_TEXTURE_HEIGHT, _gActor121300EyesHalfClosedPixels);
                    task->state++;
                    break;
                case ACTOR_121300_TEXTURE_SECOND_UPDATE:
                    _actor121300UploadTexture(task, ACTOR_121300_TEXTURE_LARGE_X_WORDS, ACTOR_121300_TEXTURE_LARGE_Y, ACTOR_121300_TEXTURE_LARGE_WIDTH_WORDS, ACTOR_121300_TEXTURE_HEIGHT, _gActor121300EyesClosedPixels);
                    taskKill(task);
                    break;
            }
            break;
        case ACTOR_121300_TEXTURE_RESTORE:
            switch (task->state) {
                case ACTOR_121300_TEXTURE_FIRST_UPDATE:
                    _actor121300UploadTexture(task, ACTOR_121300_TEXTURE_LARGE_X_WORDS, ACTOR_121300_TEXTURE_LARGE_Y, ACTOR_121300_TEXTURE_LARGE_WIDTH_WORDS, ACTOR_121300_TEXTURE_HEIGHT, _gActor121300EyesHalfClosedPixels);
                    task->state++;
                    break;
                case ACTOR_121300_TEXTURE_SECOND_UPDATE:
                    _actor121300UploadTexture(task, ACTOR_121300_TEXTURE_LARGE_X_WORDS, ACTOR_121300_TEXTURE_LARGE_Y, ACTOR_121300_TEXTURE_LARGE_WIDTH_WORDS, ACTOR_121300_TEXTURE_HEIGHT, _gActor121300EyesOpenPixels);
                    taskKill(task);
                    break;
            }
            break;
        case ACTOR_121300_TEXTURE_NO_UPLOAD:
            break;
        case ACTOR_121300_TEXTURE_REPLACE_SMALL_4:
        case ACTOR_121300_TEXTURE_REPLACE_SMALL_5:
            _actor121300UploadTexture(task, ACTOR_121300_TEXTURE_SMALL_X_WORDS, ACTOR_121300_TEXTURE_SMALL_Y, ACTOR_121300_TEXTURE_SMALL_WIDTH_WORDS, ACTOR_121300_TEXTURE_HEIGHT, _gActor121300MouthOpenPixels);
            taskKill(task);
            break;
    }
}

/// Spawns upward drift sprites around the ground or raised ring every fourth frame.
///
/// Zero `raised` selects the ground ring; nonzero selects the raised ring.
/// Both tables end at X zero. Every spawn consumes two LCG samples and jitters
/// X by -70..70 game-coordinate units in steps of ten. The task is unused;
/// Dryfield R08's effect callback and its texture resources must be loaded.
static void _actor121300SpawnRingSprites(Task* unusedTask, s16 raised)
{
    enum { ACTOR_121300_RING_END_X = 0 };
    SVECTOR  spawnPosition;
    SVECTOR* point;
    s16      pointX;
    s32      spriteArg;
    u32      randomSeed;
    s32      jitteredX;

    if (!(_gActor121300SceneFrame & ACTOR_121300_SPRITE_FRAME_MASK)) {
        if (raised == 0) {
            point = _gActor121300GroundRingPoints;
        } else {
            point = _gActor121300RaisedRingPoints;
        }
        pointX = point->vx;
        if (point->vx != ACTOR_121300_RING_END_X) {
            spriteArg = ACTOR_121300_DRIFT_SPRITE_ARGUMENT;
            do {
                randomSeed      = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState = randomSeed;
                jitteredX       = pointX + (((randomSeed >> 16) & 1) ? ((gRandomLcgState = (randomSeed * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT) >> 16) & 7
                                                                     : -(((gRandomLcgState = (randomSeed * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT) >> 16) & 7)) *
                                         10;
                spawnPosition.vx = jitteredX;
                spawnPosition.vy = point->vy;
                spawnPosition.vz = point->vz;
                effectSpawn(EFFECT_DRYFIELD_R08_ARENA_RING_SPRITE, NULL, spriteArg, &spawnPosition);
                point++;
                pointX = point->vx;
            } while (point->vx != ACTOR_121300_RING_END_X);
        }
    }
}

/// Spawns upward drift sprites along the remaining points of the upper row.
///
/// Requires live double work with `spritePointsDropped` initially zero. Nonzero
/// `dwindle` removes one trailing point every twenty calls; spawning occurs
/// every fourth scene frame with X jitter -70..70 in steps of ten, using two
/// LCG samples per point. After six removals no points spawn. Dryfield R08's
/// callback and textures must remain loaded for the spawned effects.
static void _actor121300SpawnUpperRowSprites(Task* task, s16 dwindle)
{
    enum { ACTOR_121300_SPRITE_DROP_INTERVAL = 20 };
    SVECTOR              spawnPosition;
    _Actor121300EveWork* work;
    s16                  pointIndex;
    u32                  randomSeed;
    s32                  spriteArg;
    SVECTOR*             points;
    s32                  jitteredX;

    work = task->work;
    if (dwindle != 0) {
        if (++work->spriteDropFrames >= ACTOR_121300_SPRITE_DROP_INTERVAL) {
            work->spriteDropFrames = 0;
            work->spritePointsDropped++;
        }
    }
    if (!(_gActor121300SceneFrame & ACTOR_121300_SPRITE_FRAME_MASK)) {
        for (pointIndex = 0; pointIndex < ARRAY_SIZE(_gActor121300UpperRowPoints) - 1 - work->spritePointsDropped; pointIndex++) {
            spriteArg       = ACTOR_121300_DRIFT_SPRITE_ARGUMENT;
            points          = _gActor121300UpperRowPoints;
            randomSeed      = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            gRandomLcgState = randomSeed;
            jitteredX       = points[pointIndex].vx + (((randomSeed >> 16) & 1) ? ((gRandomLcgState = (randomSeed * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT) >> 16) & 7
                                                                                : -(((gRandomLcgState = (randomSeed * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT) >> 16) & 7)) *
                                                    10;
            spawnPosition.vx = jitteredX;
            spawnPosition.vy = points[pointIndex].vy;
            spawnPosition.vz = points[pointIndex].vz;
            effectSpawn(EFFECT_DRYFIELD_R08_ARENA_RING_SPRITE, NULL, spriteArg, &spawnPosition);
        }
    }
}

/// Puts out a lamp every three shattering updates and starts its debris child.
///
/// Requires live double work and the loaded Dryfield R08 room. Initialization
/// enables debris and clears the count/timer; subsequent updates hide the next
/// lamp's sprites, suppress its glow and spawn fifteen pieces. The sentinel
/// check uses `lampCursor`, which this function never advances; the script must
/// end shattering after at most twelve spawns; another would use the closing
/// record as a lamp position. The actual cue-dependent maximum is unproven.
static void _actor121300ShatterLamps(Task* task)
{
    enum {
        ACTOR_121300_SPRITELESS_LAMP_INDEX       = 6,
        ACTOR_121300_LAMP_AFTER_SPRITELESS_INDEX = 7,
        ACTOR_121300_LAMP_DEBRIS_SPAWNER_INDEX   = 3,
    };
    _Actor121300EveWork* work = task->work;

    switch (work->shatterState) {
        case ACTOR_121300_SHATTER_START:
            _gActor121300DebrisEnabled = 1;
            work->shatterFrames        = 0;
            work->lampsShattered       = 0;
            work->shatterState        += 1;
            break;
        case ACTOR_121300_SHATTER_RUNNING:
            if (_gActor121300Lamps[work->lampCursor].endMarker != ACTOR_121300_LAMP_END) {
                if (++work->shatterFrames >= ACTOR_121300_SHATTER_INTERVAL) {
                    // Hide the lamp's sprite batch in the room's view; the seventh lamp has none.
                    if (work->lampsShattered < ACTOR_121300_SPRITELESS_LAMP_INDEX) {
                        dryfieldR08SetLampSpritesHidden(work->lampsShattered, true);
                    } else if (work->lampsShattered >= ACTOR_121300_LAMP_AFTER_SPRITELESS_INDEX) {
                        dryfieldR08SetLampSpritesHidden(work->lampsShattered - 1, true);
                    }
                    dryfieldR08SetShatteredLampCount(work->lampsShattered + 1);
                    taskSpawnFromTable(gActor121300SceneTasks, ACTOR_121300_LAMP_DEBRIS_SPAWNER_INDEX, (s32)work->lampsShattered, 0);
                    work->shatterFrames = 0;
                    work->lampsShattered++;
                }
            }
            break;
    }
}

/// Records the body animation-set index in the work block's unsigned halfword.
///
/// Both arguments evaluate once; work must address live double work. The low
/// 16 bits are retained without clamping. Current requests select loaded sets 1..3.
/// Captures no identifiers and does not seek tracks or change their playback rates.
#define ACTOR_121300_RECORD_BODY_ANIMATION_SET(work, animationSet) \
    do {                                                           \
        (work)->animSet = (animationSet);                          \
    } while (0)

/// Blends the double's driven slots into one animation set over ten frames.
///
/// Requires live double work and `animSet` in 1..3. Records the low halfword
/// as the active set, then seeks slots 1..18 to frame 0. Slot 0 is untouched.
static inline void _actor121300BlendAnimationSet(Task* task, s32 animSet)
{
    _Actor121300EveWork* work;
    u16                  slotIndex;

    work = task->work;
    // The existing statement macro keeps the set store ahead of loop setup.
    ACTOR_121300_RECORD_BODY_ANIMATION_SET(work, animSet);
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        animationSeekSlotWithBlend(&work->rig.anim, slotIndex, animSet, 0, ACTOR_121300_ANIMATION_BLEND_FRAMES);
    }
}

/// Stores the scene debris speed as a signed halfword percentage (100 full speed).
///
/// The low signed halfword is retained without clamping; existing scene steps
/// request 10 or 30. Translation, spin and gravity use this scale on each update.
static inline void _actor121300SetDebrisSpeed(s32 speedPercent)
{
    _gActor121300DebrisSpeedPercent = speedPercent;
}

/// Restarts the double's eighteen non-root tracks at its mark animation.
///
/// Requires initialized live work, model and loaded set 1. Resets rates to
/// normal speed and records the set; the root slot remains untouched.
static inline void _actor121300RestartMarkTracks(Task* task)
{
    enum {
        ACTOR_121300_MARK_CLIP = 1,
    };

    _Actor121300EveWork* work;
    s32                  slotIndex;

    work          = task->work;
    work->animSet = ACTOR_121300_MARK_CLIP;
    for (slotIndex = 1; (u16)slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        work->rig.slots[(u16)slotIndex].rate = ANIMATION_RATE_ONE;
        animationResetSlot(&work->rig.anim, (u16)slotIndex, ACTOR_121300_MARK_CLIP);
    }
}

/// Executes the posted Dryfield lamp-shattering scene step and ticks the double.
///
/// Requires initialized live double work, player task, room lights and loaded
/// scene/effect tables. One-shot steps clear themselves; sustained steps run until
/// the script replaces them. Drives clips 1..3, debris speed percentages, room lighting, sprite rings
/// and screen waves. Wave contexts are borrowed
/// from the work block and must outlive their spawned tasks.
static void _actor121300RunSceneStep(Task* task)
{
    enum {
        ACTOR_121300_STEP_FIRST_TICK   = 0,
        ACTOR_121300_LONG_WAVE_UPDATES = 60,
        ACTOR_121300_WAVE_SCALE        = 256,
        ACTOR_121300_PULSE_UPDATES     = 8,
        ACTOR_121300_PULSE_RISING      = 0,
        ACTOR_121300_PULSE_HELD        = 1,
    };

    _Actor121300EveWork* work;
    CdCmdQueue*          cdQueue;

    work    = task->work;
    cdQueue = &gCdCmdQueue;
    _actor121300TickAnimations(task);
    switch (work->step) {
        case ACTOR_121300_STEP_REPLACE_PLAYER:
            taskMessageDispatch(work->player, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER, 0);
            TASK_MESSAGE_DISPATCH_POINTER(task, ACTOR_MESSAGE_PLACE, &_gActor121300DoubleMark, 0);
            gGameSession->viewDirty = 1;
            _actor121300RestartMarkTracks(task);
            work->step = ACTOR_121300_STEP_NONE;
            break;
        case ACTOR_121300_STEP_PLAY_SET_2:
            _actor121300BlendAnimationSet(task, 2);
            work->step = ACTOR_121300_STEP_NONE;
            break;
        case ACTOR_121300_STEP_SHATTER_LAMPS_WAVE:
            if (work->stepState == ACTOR_121300_STEP_FIRST_TICK) {
                _actor121300SetDebrisSpeed(ACTOR_121300_DEBRIS_WAVE_SPEED_PERCENT);
                work->wave.span  = ACTOR_121300_LONG_WAVE_UPDATES;
                work->wave.scale = ACTOR_121300_WAVE_SCALE;
                work->waveTask   = taskSpawnFromTable(_gActor121300ScreenWaveTasks, 0, 0, &work->wave);
                work->stepState++;
            }
            // The wave setup shares the ordinary shatter update on this tick.
            /* fallthrough */
        case ACTOR_121300_STEP_SHATTER_LAMPS:
            _actor121300ShatterLamps(task);
            break;
        case ACTOR_121300_STEP_SHATTER_LAMPS_SLOW:
            _actor121300SetDebrisSpeed(ACTOR_121300_DEBRIS_SLOW_SPEED_PERCENT);
            _actor121300ShatterLamps(task);
            break;
        case ACTOR_121300_STEP_END_SHATTER:
            work->wave.state           = SCREEN_WAVE_RAMP_FINISHED;
            cdQueue->imageMdecMode     = MDEC_IMAGE_MODE_RGB16;
            _gActor121300DebrisEnabled = 0;
            work->step                 = ACTOR_121300_STEP_NONE;
            break;
        case ACTOR_121300_STEP_HOLD_ON_MARK:
            if (work->stepState == ACTOR_121300_STEP_FIRST_TICK) {
                TASK_MESSAGE_DISPATCH_POINTER(task, ACTOR_MESSAGE_PLACE, &_gActor121300DoubleMark, 0);
                _actor121300RestartMarkTracks(task);
                dryfieldR08SelectLightingBank(1);
            }
            _actor121300SpawnRingSprites(task, 0);
            _actor121300SpawnUpperRowSprites(task, 0);
            break;
        case ACTOR_121300_STEP_PLAY_SET_3:
            if (work->stepState == ACTOR_121300_STEP_FIRST_TICK) {
                _actor121300BlendAnimationSet(task, 3);
                work->stepState++;
            }
            _actor121300SpawnRingSprites(task, 0);
            break;
        case ACTOR_121300_STEP_SPRITES_DWINDLE:
            _actor121300SpawnUpperRowSprites(task, 1);
            _actor121300SpawnRingSprites(task, 0);
            break;
        case ACTOR_121300_STEP_WAVE_PULSE:
            switch (work->stepState) {
                case ACTOR_121300_PULSE_RISING:
                    work->wave.span   = ACTOR_121300_PULSE_UPDATES;
                    work->wave.scale  = ACTOR_121300_WAVE_SCALE;
                    work->waveTask    = taskSpawnFromTable(_gActor121300ScreenWaveTasks, 0, 0, &work->wave);
                    work->pulseFrames = 0;
                    work->stepState++;
                    break;
                case ACTOR_121300_PULSE_HELD:
                    if (++work->pulseFrames >= ACTOR_121300_PULSE_UPDATES) {
                        work->wave.state = SCREEN_WAVE_RAMP_FALLING;
                        work->wave.span  = ACTOR_121300_PULSE_UPDATES;
                        work->step       = ACTOR_121300_STEP_NONE;
                    }
                    break;
            }
            break;
        case ACTOR_121300_STEP_RAISED_RING:
            _actor121300SpawnRingSprites(task, 1);
            break;
        case ACTOR_121300_STEP_MASK_STREAM:
            cdQueue->imageMdecMode = MDEC_IMAGE_MODE_RGB16_MASK_BIT;
            /* fallthrough */
        case ACTOR_121300_STEP_NONE:
        default:
            work->step = ACTOR_121300_STEP_NONE;
            break;
    }
}

/// Initializes Eve, clip playback and streamed-texture offset.
///
/// Allocates zeroed task-owned work and publishes the double task; allocation
/// failure kills the task. Parents the root to the view and lends work matrices
/// to the model. Placement entry 132, or the end record when absent, supplies
/// texture offsets; its page offset is retained for later uploads. Starts clip 1
/// on tracks 1..18 and installs messages. Model, work and borrowed clips stay live.
static void _actor121300InitBody(Task* task)
{
    enum {
        ACTOR_121300_INITIAL_CLIP = 1,
    };

    enum { TEXTURE_RESOURCE_ENTRY_ID = 0x84 };

    _Actor121300EveWork* work;
    _Actor121300EveWork* allocatedWork;
    _Actor121300EveWork* slotsWork;
    TmdObject*           tmd;
    GfxCoord*            coord;
    AreaPlacement*       place;
    s32                  slotIndex;
    u8                   entryId;

    tmd           = task->extra.tmd;
    coord         = tmd->coords;
    allocatedWork = memMalloc(sizeof(_Actor121300EveWork), false);
    task->work    = allocatedWork;
    if (allocatedWork == NULL) {
        taskKill(task);
        return;
    }
    work = allocatedWork;
    memFillBytes(work, 0, sizeof(*work));
    work->player            = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    _gActor121300DoubleTask = task;
    coord->parent           = &gGfxViewCoord;
    tmd->lightMtx           = &work->lightMtx;
    tmd->flags              = 0;
    tmd->colorMtx           = &work->colorMtx;
    place                   = areaGetVariant(&gGameSession->location)->placements;
    entryId                 = place->entryId;
    while (entryId != AREA_PLACEMENT_END) {
        if (entryId == TEXTURE_RESOURCE_ENTRY_ID) {
            break;
        }
        place++;
        entryId = place->entryId;
    }
    tmdSetTextureOffsets(tmd, place->texturePageOffset, place->clutRowOffset);
    // Keep the image-column offset for later streamed texture uploads.
    work->texturePageOffset = place->texturePageOffset;
    animationInitContext(&work->rig.anim, _gActor121300AnimationSets, tmd, work->rig.poses,
                         work->rig.slots);
    slotsWork          = task->work;
    slotsWork->animSet = ACTOR_121300_INITIAL_CLIP;
    slotIndex          = 1;
    do {
        slotsWork->rig.slots[(u16)slotIndex].rate = ANIMATION_RATE_ONE;
        animationResetSlot(&slotsWork->rig.anim, (u16)slotIndex, ACTOR_121300_INITIAL_CLIP);
        slotIndex++;
    } while ((u16)slotIndex < ARRAY_SIZE(slotsWork->rig.slots));
    task->msgTable = _gActor121300DoubleMessages;
}

/// Runs Eve, the lamp-shattering scene and the final night-room reload.
///
/// Start at state 0 with the body and scene resources loaded. Setup waits for
/// the attachment wheel and display transition, then runs the scene step on
/// that same tick. The script and its skip path share the controller's live work;
/// part 1 supplies lighting and the scene counter advances once per active tick.
/// When the script ends, starts the scene-end child, then clears both 320x240
/// framebuffers and reloads night area 9, warp 3. Initialization failure retains
/// the subsequent scene update; successful work allocation is required.
/// Weapon reset requires saved character 1 and weapon 0..32 for the loaded
/// player-bank table. The retained alternate +34 selector's reachability is
/// unproven.
static void _actor121300EveSceneTask(Task* task)
{
    enum {
        ACTOR_121300_SCENE_INITIALIZE          = 0,
        ACTOR_121300_SCENE_START_SCRIPT        = 1,
        ACTOR_121300_SCENE_WAIT_SCRIPT         = 2,
        ACTOR_121300_SCENE_RELOAD              = 3,
        ACTOR_121300_SCENE_PRIMARY_CHARACTER   = 1,
        ACTOR_121300_SCENE_PRIMARY_BANK_BASE   = 1,
        ACTOR_121300_SCENE_ALTERNATE_BANK_BASE = 34,
        ACTOR_121300_SCENE_WEAPON_CLIP         = 1,
        ACTOR_121300_SCENE_END_TASK            = 9,
        ACTOR_121300_SCENE_FRAME_WIDTH         = 320,
        ACTOR_121300_SCENE_FRAME_HEIGHT        = 240,
        ACTOR_121300_SCENE_SECOND_FRAME_Y      = 272,
        ACTOR_121300_SCENE_RETURN_WARP         = 3,
        ACTOR_121300_SCENE_LIGHT_COUNT         = 3,
    };
    TmdObject* model;
    s32        sceneState;
    s32        weaponId;
    s32        animationBank;

    sceneState = task->state;
    switch (sceneState) {
        case ACTOR_121300_SCENE_INITIALIZE: {
            AnimationPlayRequest request;

            if ((gAttachmentState.mode != ATTACHMENT_MODE_WHEEL) && (gDisplayState.pendingMode == DISPLAY_MODE_NONE)) {
                weaponId                     = gPlayerStatus.weapon;
                animationBank                = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == ACTOR_121300_SCENE_PRIMARY_CHARACTER) ? weaponId + ACTOR_121300_SCENE_PRIMARY_BANK_BASE : weaponId + ACTOR_121300_SCENE_ALTERNATE_BANK_BASE;
                request.source.index         = animationBank;
                request.animationId          = ACTOR_121300_SCENE_WEAPON_CLIP;
                request.blend                = ANIMATION_BLEND_RESET;
                request.blendFrames          = 0;
                request.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &request, 0);
                _actor121300InitBody(task);
                task->state += 1;
                break;
            }
            return;
        }
        case ACTOR_121300_SCENE_START_SCRIPT:
            evsStartScriptWithSkip(_gActor121300SceneScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, _gActor121300SceneSkipScript);
            task->state += 1;
            break;
        case ACTOR_121300_SCENE_WAIT_SCRIPT:
            if (gGameSession->eventState == 0) {
                taskSpawnFromTable(gActor121300SceneTasks, ACTOR_121300_SCENE_END_TASK, 0, 0);
                task->state += 1;
            }
            break;
        case ACTOR_121300_SCENE_RELOAD: {
            RECT rect;

            rect.x = 0;
            rect.y = 0;
            rect.w = ACTOR_121300_SCENE_FRAME_WIDTH;
            rect.h = ACTOR_121300_SCENE_FRAME_HEIGHT;
            ClearImage(&rect, 0, 0, 0);
            rect.y = ACTOR_121300_SCENE_SECOND_FRAME_Y;
            ClearImage(&rect, 0, 0, 0);
            memFillBytes(gFsImageWorkspacePtr, 0, sizeof(*gFsImageWorkspacePtr));
            SetDispMask(1);
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.stage = GAME_STAGE_DRYFIELD_NIGHT;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area  = GAME_AREA_DRYFIELD_NIGHT_DILAPIDATED_HOUSE;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp  = ACTOR_121300_SCENE_RETURN_WARP;
            gDisplayState.spriteVariant                             = 1;
            taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
            taskKill(task);
            return;
        }
    }
        /// Sets scene-model lighting from part 1's cached translation.
        ///
        /// The arguments must be side-effect-free Task*, TmdObject* and VECTOR
        /// lvalues; they are used repeatedly. Captures the local light-count constant.
        /// Does not compose the coordinate. Uses all three room lights, leaves
        /// the VECTOR's fourth word intact and retains no sample pointer.
#define ACTOR_121300_UPDATE_SCENE_MODEL_LIGHTING(task, model, samplePosition)                      \
    {                                                                                              \
        (model)             = (task)->extra.tmd;                                                   \
        (samplePosition).vx = (model)->coords[1].workm.t[0];                                       \
        (samplePosition).vy = (task)->extra.tmd->coords[1].workm.t[1];                             \
        (samplePosition).vz = (task)->extra.tmd->coords[1].workm.t[2];                             \
        worldCoordSetModelLighting((model), &(samplePosition), 0, ACTOR_121300_SCENE_LIGHT_COUNT); \
    }

    // Run the posted scene step before sampling part 1 and advancing the frame gate.
    _actor121300RunSceneStep(task);
    {
        VECTOR samplePosition;

        ACTOR_121300_UPDATE_SCENE_MODEL_LIGHTING(task, model, samplePosition);
    }
    _gActor121300SceneFrame += 1;
}
#undef ACTOR_121300_UPDATE_SCENE_MODEL_LIGHTING

/// Darkens the scene, then disables display output when the ramp reaches 256.
///
/// Start at state 0. Owns primary-heap `ScreenFadeWork` until task teardown;
/// allocation failure kills the task. Initialization draws and steps immediately.
/// `spawnArg1`'s unsigned low halfword is intensity added per update, narrowed to
/// signed halfwords without clamping. Zero holds indefinitely; large rates may
/// wrap. Drawing uses the low red/green/red bytes and requires the frame arena
/// and foreground ordering tag for `fadeDrawOverlay`.
static void _actor121300FadeOutTask(Task* task)
{
    enum {
        ACTOR_121300_FADE_OUT_INITIALIZE    = 0,
        ACTOR_121300_FADE_OUT_DARKEN        = 1,
        ACTOR_121300_FADE_OUT_END_INTENSITY = 256,
    };
    ScreenFadeWork* fade;
    ScreenFadeWork* allocatedFade;

    fade = task->work;
    switch (task->state) {
        case ACTOR_121300_FADE_OUT_INITIALIZE:
            allocatedFade = memMalloc(sizeof(*allocatedFade), false);
            task->work    = allocatedFade;
            if (allocatedFade == NULL) {
                taskKill(task);
                return;
            }
            fade         = allocatedFade;
            fade->b      = 0;
            fade->g      = 0;
            fade->r      = 0;
            task->state += 1;
            /* fallthrough */
        case ACTOR_121300_FADE_OUT_DARKEN:
            fadeDrawOverlay(fade->r, fade->g, fade->r, GPU_BLEND_SUBTRACT);
            fade->r += task->spawnArg1.halves.low;
            fade->g += task->spawnArg1.halves.low;
            fade->b += task->spawnArg1.halves.low;
            if (fade->r < ACTOR_121300_FADE_OUT_END_INTENSITY) {
                return;
            }
            SetDispMask(0);
            taskKill(task);
            break;
    }
}

/// Keeps the frame black with a full-strength subtractive overlay each update.
///
/// The callback ignores its task and neither allocates work nor ends itself.
/// Requires the frame arena and foreground ordering tag for `fadeDrawOverlay`.
static void _actor121300BlackoutTask(Task* task)
{
    enum { ACTOR_121300_BLACKOUT_INTENSITY = 255 };

    fadeDrawOverlay(ACTOR_121300_BLACKOUT_INTENSITY, ACTOR_121300_BLACKOUT_INTENSITY,
                    ACTOR_121300_BLACKOUT_INTENSITY, GPU_BLEND_SUBTRACT);
}

#include "../../shared/actor/actor_messages_place_ypr.inc.c"

#include "../../shared/actor/actor_messages_draw_mode.inc.c"

/// Starts a child to upload the double's requested texture sequence.
///
/// The first payload selects 0 (initial), 1 (two replacements), 2 (replace then
/// restore), or 4/5 (small replacement); 3 and unsupported values leave an idle
/// child alive. The message ID and second payload are ignored. The child borrows
/// this live task and its initialized work; neither may end before the child.
/// Returns the spawned child's address as a message word, or zero on failure.
static s32 _actor121300SetTextureSequence(Task* task, s32 unusedMessageId, s32 textureSequence, s32 unusedArg)
{
    enum { ACTOR_121300_TEXTURE_UPLOAD_TASK_INDEX = 10 };

    return (s32)taskSpawnFromTable(gActor121300SceneTasks, ACTOR_121300_TEXTURE_UPLOAD_TASK_INDEX, textureSequence, task);
}

/// Selects the double's scene step and resets its progress for the next update.
///
/// Requires the published double task and work to be live. `step` is an
/// `ACTOR_121300_STEP_*` value, stored as its unsigned low halfword.
static void _actor121300SelectSceneStep(s16 step)
{
    _Actor121300EveWork* work = _gActor121300DoubleTask->work;

    work->step      = step;
    work->stepState = 0;
}

/// Restores RGB16 scene-image decoding without the mask bit.
static void _actor121300RestoreStreamImageMode(void)
{
    gCdCmdQueue.imageMdecMode = MDEC_IMAGE_MODE_RGB16;
}

/// Ends the shattering scene's effects and restores the player when skipped.
///
/// Requires the published double task, its work and a previously selected stream.
/// Clears the debris lifetime gate, finishes the screen-wave ramp, restores
/// unmasked RGB16 image decoding and shows the player before requesting CD
/// cancellation. Effect tasks release themselves on later updates; this call
/// releases no work or model storage.
static void _actor121300SkipScene(void)
{
    _Actor121300EveWork* work = _gActor121300DoubleTask->work;

    _gActor121300DebrisEnabled = 0;
    work->wave.state           = SCREEN_WAVE_RAMP_FINISHED;
    gCdCmdQueue.imageMdecMode  = MDEC_IMAGE_MODE_RGB16;
    taskMessageDispatch(work->player, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);
    cdCmdCancelScene();
}

/// Starts a fade that reveals the scene after three black updates.
///
/// `intensityStep` supplies intensity removed per update from its unsigned
/// low halfword, with signed-halfword narrowing and no clamping. The script uses
/// 4 or 9; zero never completes the ramp. Spawns an independent task on the
/// selected execution list and ignores allocation failure.
static void _actor121300StartFadeIn(s32 intensityStep)
{
    enum { ACTOR_121300_FADE_IN_TASK_INDEX = 1 };

    taskSpawnFromTable(gActor121300SceneTasks, ACTOR_121300_FADE_IN_TASK_INDEX, intensityStep, 0);
}

/// Starts a fade that darkens the scene and disables display output.
///
/// `intensityStep` supplies intensity added per update from its unsigned
/// low halfword, with signed-halfword narrowing and no clamping. The script uses
/// 4; zero never completes the ramp. Spawns an independent task on the
/// selected execution list and ignores allocation failure.
static void _actor121300StartFadeOut(s32 intensityStep)
{
    enum { ACTOR_121300_FADE_OUT_TASK_INDEX = 2 };

    taskSpawnFromTable(gActor121300SceneTasks, ACTOR_121300_FADE_OUT_TASK_INDEX, intensityStep, 0);
}

/// Sets the double model's draw mode from an event-script callback.
///
/// Requires the published double task to be live. `drawMode` uses
/// `ACTOR_MESSAGE_DRAW_*`; the actor's message handler leaves other model flags
/// intact. The script uses SHOW and HIDE_SKIP_AUTO_BUFFER.
static void _actor121300SetDoubleDrawMode(s32 drawMode)
{
    taskMessageDispatch(_gActor121300DoubleTask, ACTOR_MESSAGE_SET_MODEL_DRAW, drawMode, 0);
}

/// Stages the selected scene's deferred audio-start request at the script cue.
///
/// Requires the selected scene descriptor and playback buffers to remain live
/// until the CD dispatcher consumes the staged request.
static void _actor121300StageSceneAudioStart(void)
{
    cdCmdStageSceneAudioStart();
}

/// Queues playback of the selected scene/audio session at the script cue.
///
/// Requires the selected scene descriptor and playback buffers through request
/// consumption, and room in the CD command queue for the playback request.
static void _actor121300QueueScenePlayback(void)
{
    cdCmdEnqueueScenePlayback();
}

/// Completes the scene stream and requests CD cancellation at the script's end.
///
/// Requires a successfully selected scene. Restores streaming flags and saved
/// random state immediately; CD cancellation completes on subsequent dispatches.
/// The scene and task owners retain responsibility for their buffers and teardown.
static void _actor121300FinishScene(void)
{
    streamFinishScene();
    cdCmdCancelScene();
}
