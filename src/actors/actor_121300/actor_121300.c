#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/rand.h>

#include "common.h"

#include "actors/actor.h"

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
#include "../../shared/screen_wave.h"
#include "../../shared/actor_messages.h"

/// The overlay's spawn table: entries 1 and 2 are spawned by the one-line
/// spawners the scene script calls, 3 by the lamp shattering for each lamp
/// that goes out, 4 to 8 are the debris variants `_actor121300SpawnLampDebrisTask`
/// scatters around a lamp, 9 is spawned once the session event has
/// ended, and 0xA by `_actor121300SetTextureSequence`.
extern TaskDesc D_actor_121300_8013D390[];

/// Scene step of Aya's double, stored in `_Actor121300AyaBreaWork::step`.
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
/// `_Actor121300AyaBreaWork::shatterState`.
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

/// Work block of Aya Brea's double, the body model this package animates over
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
} _Actor121300AyaBreaWork;
STATIC_ASSERT_SIZEOF(_Actor121300AyaBreaWork, 0x4B0);

/// Animation set to follow each set with, indexed by
/// `_Actor121300AyaBreaWork::animSet`: once every driven slot has settled, a
/// non-negative entry becomes the set the slots play.  All four entries are
/// -1, so no set is followed by another.
extern s16 D_actor_121300_8013CC18[];

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

/// The lamp table: the room's twelve lamps and a closing record.  The debris
/// of a lamp starts at its position.
extern _Actor121300Lamp D_actor_121300_8013CC20[];

/// Frame counter `func_actor_121300_80133D98` bumps once a frame and the
/// effect spawners gate on: `_actor121300SpawnRingSprites` only runs on every
/// fourth frame (`& 3`), `_actor121300SpawnUpperRowSprites` too.
extern s32 D_actor_121300_8013CC00;

/// The two position tables `_actor121300SpawnRingSprites` walks, each an array
/// of `SVECTOR`s ending on a zeroed one -- the walker's guard is `vx != 0`, so
/// the sentinel is read with the position.  Both trace the same ring around
/// the arena (`vx` 2500..6500 at `vz` 4700, then back at 1500) and differ only
/// in height: `8013CCB8` sits at ground level, `8013CD48` at `vy` -0xC8.
extern SVECTOR D_actor_121300_8013CCB8[];
extern SVECTOR D_actor_121300_8013CD48[];
/// Spawn points of `_actor121300SpawnUpperRowSprites`, of which the first
/// `6 - _Actor121300AyaBreaWork::spritePointsDropped` are used.
extern SVECTOR D_actor_121300_8013CDC8[];

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
/// `gDisplayState.pendingMode` / `Gp_StateC08.mode` (1 while the attachment wheel is open) gate the actor's setup.

/// Distortion amplitude of the screen wave, `frame * scale / span` of the
/// running ramp, recomputed every frame.
extern s32 gScreenWaveRamp;

/// The ramp the running wave task was spawned with, parked at spawn so the
/// tick reads it back every frame.
extern ScreenWaveCtx* gScreenWaveCtx;

extern TaskDesc      D_actor_121300_8013BBCC[];
extern u_long        D_actor_121300_8013BBE8[];
extern u_long        D_actor_121300_8013BFD0[];
extern u_long        D_actor_121300_8013C3B8[];
extern u_long        D_actor_121300_8013C7A0[];
extern u_long        D_actor_121300_8013C9D0[];
extern s16           D_actor_121300_8013CC04;
extern AnimationSet* D_actor_121300_8013CC08[4];
// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry D_actor_121300_8013CC88[3];
extern ActorTransform   D_actor_121300_8013CCA0;
extern EvsCommand       D_actor_121300_8013CE08[];
extern EvsCommand       D_actor_121300_8013D2E8[];

extern Task* D_actor_121300_8013D418;

extern u16 D_actor_121300_8013D41C;

/// Per-column and per-row phase records: each is seeded with a random offset
/// and speed at spawn and advanced by its speed every frame.
extern ScreenWaveOscillator gScreenWaveColumns[13];

extern ScreenWaveOscillator gScreenWaveRows[30];

enum { ACTOR_121300_MESSAGE_SET_TEXTURE_SEQUENCE = 2016 };

static s32 _actor121300SetTextureSequence(Task* task, s32 unusedMessageId, s32 textureSequence, s32 unusedArg);

static TmdSource _gActor121300AyaBreaBody;
static TmdSource _gActor121300Model07E84;
static TmdSource _gActor121300Model080F0;
static TmdSource _gActor121300Model0834C;
static TmdSource _gActor121300Model08580;
static TmdSource _gActor121300Model08768;
static void      _actor121300FadeInTask(Task* task);
static void      _actor121300LampDebrisTask(Task* task);
static void      _actor121300SpawnLampDebrisTask(Task* task);
static void      _actor121300UploadTexturesTask(Task* task);
void             func_actor_121300_80133D98(Task*);
static void      _actor121300FadeOutTask(Task* task);
static void      _actor121300BlackoutTask(Task* task);
static void      _actor121300SelectSceneStep(s16 step);
static void      _actor121300RestoreStreamImageMode(void);
void             func_actor_121300_8013427C(void);
static void      _actor121300StartFadeIn(s32 intensityStep);
static void      _actor121300StartFadeOut(s32 intensityStep);
static void      _actor121300SetDoubleDrawMode(s32 drawMode);
static void      _actor121300StageSceneAudioStart(void);
static void      _actor121300QueueScenePlayback(void);
void             func_actor_121300_801343A4(void);

static TmdBone _gActor121300AyaBreaBodySkeleton[19] = {
#include "assets/aya_brea_body_skeleton.inc"
};

static u32 _gActor121300AyaBreaBodyPartVerts[19] = {
#include "assets/aya_brea_body_partVerts.inc"
};

static SVECTOR _gActor121300AyaBreaBodyVerts[365] = {
#include "assets/aya_brea_body_verts.inc"
};

static SVECTOR _gActor121300AyaBreaBodyNormals[385] = {
#include "assets/aya_brea_body_normals.inc"
};

static u32 _gActor121300AyaBreaBodyStream[3923] = {
#include "assets/aya_brea_body_stream.inc"
};

static TmdSource _gActor121300AyaBreaBody = {
    0,
    21760,
    5992,
    19,
    _gActor121300AyaBreaBodyPartVerts,
    _gActor121300AyaBreaBodyVerts,
    _gActor121300AyaBreaBodyNormals,
    _gActor121300AyaBreaBodySkeleton,
    _gActor121300AyaBreaBodyStream,
};

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

static AnimationPackedPose _gActor121300Animation089E8Bank1[2] = {
#include "assets/actor_121300_animation_089E8_bank1.inc"
};

static AnimationPackedRotation _gActor121300Animation089E8Bank4[17] = {
#include "assets/actor_121300_animation_089E8_bank4.inc"
};

static AnimationRecord _gActor121300Animation089E8Records[57] = {
#include "assets/actor_121300_animation_089E8_records.inc"
};

static u16 _gActor121300Animation089E8Indices[20] = {
#include "assets/actor_121300_animation_089E8_indices.inc"
};

static AnimationSet _gActor121300Animation089E8 = {
    _gActor121300Animation089E8Records,
    _gActor121300Animation089E8Indices,
    { NULL, _gActor121300Animation089E8Bank1, NULL, NULL, _gActor121300Animation089E8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor121300Animation08C8CBank1[2] = {
#include "assets/actor_121300_animation_08C8C_bank1.inc"
};

static AnimationPackedRotation _gActor121300Animation08C8CBank4[36] = {
#include "assets/actor_121300_animation_08C8C_bank4.inc"
};

static AnimationRecord _gActor121300Animation08C8CRecords[107] = {
#include "assets/actor_121300_animation_08C8C_records.inc"
};

static u16 _gActor121300Animation08C8CIndices[20] = {
#include "assets/actor_121300_animation_08C8C_indices.inc"
};

static AnimationSet _gActor121300Animation08C8C = {
    _gActor121300Animation08C8CRecords,
    _gActor121300Animation08C8CIndices,
    { NULL, _gActor121300Animation08C8CBank1, NULL, NULL, _gActor121300Animation08C8CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor121300Animation09D84Bank1[29] = {
#include "assets/actor_121300_animation_09D84_bank1.inc"
};

static AnimationPackedRotation _gActor121300Animation09D84Bank4[439] = {
#include "assets/actor_121300_animation_09D84_bank4.inc"
};

static AnimationRecord _gActor121300Animation09D84Records[540] = {
#include "assets/actor_121300_animation_09D84_records.inc"
};

static u16 _gActor121300Animation09D84Indices[20] = {
#include "assets/actor_121300_animation_09D84_indices.inc"
};

static AnimationSet _gActor121300Animation09D84 = {
    _gActor121300Animation09D84Records,
    _gActor121300Animation09D84Indices,
    { NULL, _gActor121300Animation09D84Bank1, NULL, NULL, _gActor121300Animation09D84Bank4, NULL, NULL, NULL },
};

TaskDesc D_actor_121300_8013BBCC[2] = {
    { { { TASK_BODY_NONE, 192 } }, _screenWaveTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

s32 gScreenWaveRamp = 256;

u_long D_actor_121300_8013BBE8[250] = {
#include "assets/actor_121300_image_09DC8.inc"
};

u_long D_actor_121300_8013BFD0[250] = {
#include "assets/actor_121300_image_0A1B0.inc"
};

u_long D_actor_121300_8013C3B8[250] = {
#include "assets/actor_121300_image_0A598.inc"
};

u_long D_actor_121300_8013C7A0[140] = {
#include "assets/actor_121300_image_0A980.inc"
};

u_long D_actor_121300_8013C9D0[140] = {
#include "assets/actor_121300_image_0ABB0.inc"
};

s32 D_actor_121300_8013CC00 = 0;

s16 D_actor_121300_8013CC04 = 100;

AnimationSet* D_actor_121300_8013CC08[4] = { NULL, &_gActor121300Animation089E8, &_gActor121300Animation08C8C, &_gActor121300Animation09D84 };

s16 D_actor_121300_8013CC18[4] = {
    -1,
    -1,
    -1,
    -1,
};

_Actor121300Lamp D_actor_121300_8013CC20[13] = {
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

TaskMessageEntry D_actor_121300_8013CC88[3] = {
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceYawPitchRoll },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, actorMsgSetDrawMode },
    { ACTOR_121300_MESSAGE_SET_TEXTURE_SEQUENCE, _actor121300SetTextureSequence },
};

ActorTransform D_actor_121300_8013CCA0 = { { 5140, -140, 3010, 0 }, { 0, 0, 0, 0 } };

SVECTOR D_actor_121300_8013CCB8[18] = {
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

SVECTOR D_actor_121300_8013CD48[16] = {
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

SVECTOR D_actor_121300_8013CDC8[7] = {
    { 6500, -500, 2000, 0 },
    { 5500, -500, 2000, 0 },
    { 4500, -500, 2000, 0 },
    { 4000, -500, 2000, 0 },
    { 6000, -500, 2000, 0 },
    { 5000, -500, 2000, 0 },
    { 0, 0, 0, 0 },
};

EvsSceneKey D_actor_121300_8013CE00 = { 2, 13, 11 };

EvsCommand D_actor_121300_8013CE08[52] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor121300SelectSceneStep }, { .value = ACTOR_121300_STEP_REPLACE_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = SetDispMask }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_actor_121300_8013CE00 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor121300StageSceneAudioStart }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor121300QueueScenePlayback }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor121300StartFadeIn }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor121300StartFadeOut }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor121300SetDoubleDrawMode }, { .value = ACTOR_MESSAGE_DRAW_SHOW }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = SetDispMask }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor121300SelectSceneStep }, { .value = ACTOR_121300_STEP_END_SHATTER }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor121300SelectSceneStep }, { .value = ACTOR_121300_STEP_HOLD_ON_MARK }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor121300StartFadeIn }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor121300SelectSceneStep }, { .value = ACTOR_121300_STEP_SPRITES_DWINDLE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor121300SelectSceneStep }, { .value = ACTOR_121300_STEP_PLAY_SET_3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor121300StartFadeOut }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor121300SetDoubleDrawMode }, { .value = ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor121300StartFadeIn }, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor121300SelectSceneStep }, { .value = ACTOR_121300_STEP_RAISED_RING }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor121300StartFadeOut }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor121300RestoreStreamImageMode }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_121300_801343A4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor121300SelectSceneStep }, { .value = ACTOR_121300_STEP_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_121300_8013D2E8[7] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor121300SelectSceneStep }, { .value = ACTOR_121300_STEP_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_121300_8013427C }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc D_actor_121300_8013D390[11] = {
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_121300_80133D98, { .model = &_gActor121300AyaBreaBody } },
    { { { TASK_BODY_NONE, 192 } }, _actor121300FadeInTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _actor121300FadeOutTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _actor121300SpawnLampDebrisTask, { .value = 0 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, _actor121300LampDebrisTask, { .model = &_gActor121300Model07E84 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, _actor121300LampDebrisTask, { .model = &_gActor121300Model080F0 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, _actor121300LampDebrisTask, { .model = &_gActor121300Model0834C } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, _actor121300LampDebrisTask, { .model = &_gActor121300Model08580 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, _actor121300LampDebrisTask, { .model = &_gActor121300Model08768 } },
    { { { TASK_BODY_NONE, 192 } }, _actor121300BlackoutTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _actor121300UploadTexturesTask, { .value = 0 } },
};

ScreenWaveCtx* gScreenWaveCtx = NULL;

Task* D_actor_121300_8013D418;

u16 D_actor_121300_8013D41C;

ScreenWaveOscillator gScreenWaveColumns[13];

ScreenWaveOscillator gScreenWaveRows[30];

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
static void        func_actor_121300_80133854(Task* arg0);
static void        func_actor_121300_80133BFC(Task* task);

#include "../../shared/screen_wave.inc.c"

#include "../../shared/screen_fade_step_down.inc.c"

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
/// Requires live `_Actor121300AyaBreaWork` and an animation-set index in 1..3.
/// Slots 1..18 are driven; slot 0 is left alone. Returns 1 when all driven slots
/// are settled, including when a nonnegative follow-up table entry starts a
/// ten-frame blend at frame 0; returns 0 while any slot is unsettled.
static s32 _actor121300TickAnimations(Task* task)
{
    _Actor121300AyaBreaWork* work;
    _Actor121300AyaBreaWork* blendWork;
    u16                      slotIndex;
    u16                      allSettled;
    u16                      nextAnimSet;

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
        if (D_actor_121300_8013CC18[work->animSet] >= 0) {
            nextAnimSet        = D_actor_121300_8013CC18[work->animSet];
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
        ACTOR_121300_DEBRIS_INITIALIZE         = 0,
        ACTOR_121300_DEBRIS_DELAY              = 1,
        ACTOR_121300_DEBRIS_FALL               = 2,
        ACTOR_121300_DEBRIS_FULL_SPEED_PERCENT = 100,
        ACTOR_121300_DEBRIS_DESPAWN_Y          = -499,
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
    if (D_actor_121300_8013D41C == 0) {
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
            rootCoord->coord.t[0] = D_actor_121300_8013CC20[task->spawnArg1.value].x;
            rootCoord->coord.t[1] = D_actor_121300_8013CC20[task->spawnArg1.value].y;
            rootCoord->coord.t[2] = D_actor_121300_8013CC20[task->spawnArg1.value].z;
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
            work->velY            += D_actor_121300_8013CC04 * 3 / ACTOR_121300_DEBRIS_FULL_SPEED_PERCENT;
            rootCoord->coord.t[0] += work->velX * D_actor_121300_8013CC04 / ACTOR_121300_DEBRIS_FULL_SPEED_PERCENT;
            rootCoord->coord.t[1] += work->velY * D_actor_121300_8013CC04 / ACTOR_121300_DEBRIS_FULL_SPEED_PERCENT;
            rootCoord->coord.t[2] += work->velZ * D_actor_121300_8013CC04 / ACTOR_121300_DEBRIS_FULL_SPEED_PERCENT;
            work->rotX            += work->spinX * D_actor_121300_8013CC04 / ACTOR_121300_DEBRIS_FULL_SPEED_PERCENT;
            work->rotY            += work->spinY * D_actor_121300_8013CC04 / ACTOR_121300_DEBRIS_FULL_SPEED_PERCENT;
            work->rotZ            += work->spinZ * D_actor_121300_8013CC04 / ACTOR_121300_DEBRIS_FULL_SPEED_PERCENT;
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

    taskSpawnFromTable(D_actor_121300_8013D390, ACTOR_121300_DEBRIS_MODEL_FIRST, task->spawnArg1, ACTOR_121300_DEBRIS_PLACEMENT_CENTER_FIRST);
    taskSpawnFromTable(D_actor_121300_8013D390, ACTOR_121300_DEBRIS_MODEL_SECOND, task->spawnArg1, ACTOR_121300_DEBRIS_PLACEMENT_INNER_PLUS_X_PLUS_Y);
    taskSpawnFromTable(D_actor_121300_8013D390, ACTOR_121300_DEBRIS_MODEL_THIRD, task->spawnArg1, ACTOR_121300_DEBRIS_PLACEMENT_INNER_MINUS_X_PLUS_Y);
    taskSpawnFromTable(D_actor_121300_8013D390, ACTOR_121300_DEBRIS_MODEL_FOURTH, task->spawnArg1, ACTOR_121300_DEBRIS_PLACEMENT_INNER_PLUS_X_MINUS_Y);
    taskSpawnFromTable(D_actor_121300_8013D390, ACTOR_121300_DEBRIS_MODEL_FIFTH, task->spawnArg1, ACTOR_121300_DEBRIS_PLACEMENT_INNER_MINUS_X_MINUS_Y);
    taskSpawnFromTable(D_actor_121300_8013D390, ACTOR_121300_DEBRIS_MODEL_FIRST, task->spawnArg1, ACTOR_121300_DEBRIS_PLACEMENT_MIDDLE_PLUS_X_PLUS_Y);
    taskSpawnFromTable(D_actor_121300_8013D390, ACTOR_121300_DEBRIS_MODEL_SECOND, task->spawnArg1, ACTOR_121300_DEBRIS_PLACEMENT_MIDDLE_MINUS_X_PLUS_Y);
    taskSpawnFromTable(D_actor_121300_8013D390, ACTOR_121300_DEBRIS_MODEL_THIRD, task->spawnArg1, ACTOR_121300_DEBRIS_PLACEMENT_MIDDLE_PLUS_X_MINUS_Y);
    taskSpawnFromTable(D_actor_121300_8013D390, ACTOR_121300_DEBRIS_MODEL_FOURTH, task->spawnArg1, ACTOR_121300_DEBRIS_PLACEMENT_MIDDLE_MINUS_X_MINUS_Y);
    taskSpawnFromTable(D_actor_121300_8013D390, ACTOR_121300_DEBRIS_MODEL_FIFTH, task->spawnArg1, ACTOR_121300_DEBRIS_PLACEMENT_CENTER_MIDDLE);
    taskSpawnFromTable(D_actor_121300_8013D390, ACTOR_121300_DEBRIS_MODEL_FIRST, task->spawnArg1, ACTOR_121300_DEBRIS_PLACEMENT_OUTER_PLUS_X_PLUS_Y);
    taskSpawnFromTable(D_actor_121300_8013D390, ACTOR_121300_DEBRIS_MODEL_SECOND, task->spawnArg1, ACTOR_121300_DEBRIS_PLACEMENT_OUTER_MINUS_X_PLUS_Y);
    taskSpawnFromTable(D_actor_121300_8013D390, ACTOR_121300_DEBRIS_MODEL_THIRD, task->spawnArg1, ACTOR_121300_DEBRIS_PLACEMENT_OUTER_PLUS_X_MINUS_Y);
    taskSpawnFromTable(D_actor_121300_8013D390, ACTOR_121300_DEBRIS_MODEL_FOURTH, task->spawnArg1, ACTOR_121300_DEBRIS_PLACEMENT_OUTER_MINUS_X_MINUS_Y);
    taskSpawnFromTable(D_actor_121300_8013D390, ACTOR_121300_DEBRIS_MODEL_FIFTH, task->spawnArg1, ACTOR_121300_DEBRIS_PLACEMENT_CENTER_LAST);
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

    if (D_actor_121300_8013D41C == 0) {
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
    RECT                     uploadRect;
    s32                      relocatedXWords;
    Task*                    parentTask;
    _Actor121300AyaBreaWork* parentWork;

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
            _actor121300UploadTexture(task, ACTOR_121300_TEXTURE_LARGE_X_WORDS, ACTOR_121300_TEXTURE_LARGE_Y, ACTOR_121300_TEXTURE_LARGE_WIDTH_WORDS, ACTOR_121300_TEXTURE_HEIGHT, D_actor_121300_8013BBE8);
            _actor121300UploadTexture(task, ACTOR_121300_TEXTURE_SMALL_X_WORDS, ACTOR_121300_TEXTURE_SMALL_Y, ACTOR_121300_TEXTURE_SMALL_WIDTH_WORDS, ACTOR_121300_TEXTURE_HEIGHT, D_actor_121300_8013C7A0);
            taskKill(task);
            break;
        case ACTOR_121300_TEXTURE_REPLACE:
            switch (task->state) {
                case ACTOR_121300_TEXTURE_FIRST_UPDATE:
                    _actor121300UploadTexture(task, ACTOR_121300_TEXTURE_LARGE_X_WORDS, ACTOR_121300_TEXTURE_LARGE_Y, ACTOR_121300_TEXTURE_LARGE_WIDTH_WORDS, ACTOR_121300_TEXTURE_HEIGHT, D_actor_121300_8013BFD0);
                    task->state++;
                    break;
                case ACTOR_121300_TEXTURE_SECOND_UPDATE:
                    _actor121300UploadTexture(task, ACTOR_121300_TEXTURE_LARGE_X_WORDS, ACTOR_121300_TEXTURE_LARGE_Y, ACTOR_121300_TEXTURE_LARGE_WIDTH_WORDS, ACTOR_121300_TEXTURE_HEIGHT, D_actor_121300_8013C3B8);
                    taskKill(task);
                    break;
            }
            break;
        case ACTOR_121300_TEXTURE_RESTORE:
            switch (task->state) {
                case ACTOR_121300_TEXTURE_FIRST_UPDATE:
                    _actor121300UploadTexture(task, ACTOR_121300_TEXTURE_LARGE_X_WORDS, ACTOR_121300_TEXTURE_LARGE_Y, ACTOR_121300_TEXTURE_LARGE_WIDTH_WORDS, ACTOR_121300_TEXTURE_HEIGHT, D_actor_121300_8013BFD0);
                    task->state++;
                    break;
                case ACTOR_121300_TEXTURE_SECOND_UPDATE:
                    _actor121300UploadTexture(task, ACTOR_121300_TEXTURE_LARGE_X_WORDS, ACTOR_121300_TEXTURE_LARGE_Y, ACTOR_121300_TEXTURE_LARGE_WIDTH_WORDS, ACTOR_121300_TEXTURE_HEIGHT, D_actor_121300_8013BBE8);
                    taskKill(task);
                    break;
            }
            break;
        case ACTOR_121300_TEXTURE_NO_UPLOAD:
            break;
        case ACTOR_121300_TEXTURE_REPLACE_SMALL_4:
        case ACTOR_121300_TEXTURE_REPLACE_SMALL_5:
            _actor121300UploadTexture(task, ACTOR_121300_TEXTURE_SMALL_X_WORDS, ACTOR_121300_TEXTURE_SMALL_Y, ACTOR_121300_TEXTURE_SMALL_WIDTH_WORDS, ACTOR_121300_TEXTURE_HEIGHT, D_actor_121300_8013C9D0);
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

    if (!(D_actor_121300_8013CC00 & ACTOR_121300_SPRITE_FRAME_MASK)) {
        if (raised == 0) {
            point = D_actor_121300_8013CCB8;
        } else {
            point = D_actor_121300_8013CD48;
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
    SVECTOR                  spawnPosition;
    _Actor121300AyaBreaWork* work;
    s16                      pointIndex;
    u32                      randomSeed;
    s32                      spriteArg;
    SVECTOR*                 points;
    s32                      jitteredX;

    work = task->work;
    if (dwindle != 0) {
        if (++work->spriteDropFrames >= ACTOR_121300_SPRITE_DROP_INTERVAL) {
            work->spriteDropFrames = 0;
            work->spritePointsDropped++;
        }
    }
    if (!(D_actor_121300_8013CC00 & ACTOR_121300_SPRITE_FRAME_MASK)) {
        for (pointIndex = 0; pointIndex < ARRAY_SIZE(D_actor_121300_8013CDC8) - 1 - work->spritePointsDropped; pointIndex++) {
            spriteArg       = ACTOR_121300_DRIFT_SPRITE_ARGUMENT;
            points          = D_actor_121300_8013CDC8;
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
    _Actor121300AyaBreaWork* work = task->work;

    switch (work->shatterState) {
        case ACTOR_121300_SHATTER_START:
            D_actor_121300_8013D41C = 1;
            work->shatterFrames     = 0;
            work->lampsShattered    = 0;
            work->shatterState     += 1;
            break;
        case ACTOR_121300_SHATTER_RUNNING:
            if (D_actor_121300_8013CC20[work->lampCursor].endMarker != ACTOR_121300_LAMP_END) {
                if (++work->shatterFrames >= ACTOR_121300_SHATTER_INTERVAL) {
                    // Hide the lamp's sprite batch in the room's view; the seventh lamp has none.
                    if (work->lampsShattered < ACTOR_121300_SPRITELESS_LAMP_INDEX) {
                        dryfieldR08SetLampSpritesHidden(work->lampsShattered, true);
                    } else if (work->lampsShattered >= ACTOR_121300_LAMP_AFTER_SPRITELESS_INDEX) {
                        dryfieldR08SetLampSpritesHidden(work->lampsShattered - 1, true);
                    }
                    dryfieldR08SetShatteredLampCount(work->lampsShattered + 1);
                    taskSpawnFromTable(D_actor_121300_8013D390, ACTOR_121300_LAMP_DEBRIS_SPAWNER_INDEX, (s32)work->lampsShattered, 0);
                    work->shatterFrames = 0;
                    work->lampsShattered++;
                }
            }
            break;
    }
}

/// Records `anim` as the animation the actor's slots are playing.
#define SET_ANIM_ID(work, anim)   \
    do {                          \
        (work)->animSet = (anim); \
    } while (0)

/// Blends the double's driven slots into one animation set over ten frames.
///
/// Requires live double work and `animSet` in 1..3. Records the low halfword
/// as the active set, then seeks slots 1..18 to frame 0. Slot 0 is untouched.
static inline void _actor121300BlendAnimationSet(Task* task, s32 animSet)
{
    _Actor121300AyaBreaWork* work;
    u16                      slotIndex;

    work = task->work;
    // The existing statement macro keeps the set store ahead of loop setup.
    SET_ANIM_ID(work, animSet);
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
    D_actor_121300_8013CC04 = speedPercent;
}

static void func_actor_121300_80133854(Task* arg0)
{
    _Actor121300AyaBreaWork* work;
    CdCmdQueue*              queue;

    work  = arg0->work;
    queue = &gCdCmdQueue;
    _actor121300TickAnimations(arg0);
    switch (work->step) {
        case ACTOR_121300_STEP_REPLACE_PLAYER:
            taskMessageDispatch(work->player, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, 2, 0);
            TASK_MESSAGE_DISPATCH_POINTER(arg0, ACTOR_MESSAGE_PLACE, &D_actor_121300_8013CCA0, 0);
            gGameSession->viewDirty = 1;
            {
                _Actor121300AyaBreaWork* slotsWork;
                s32                      i;

                slotsWork          = arg0->work;
                slotsWork->animSet = 1;
                for (i = 1; (u16)i < ARRAY_SIZE(slotsWork->rig.slots); i++) {
                    slotsWork->rig.slots[(u16)i].rate = ANIMATION_RATE_ONE;
                    animationResetSlot(&slotsWork->rig.anim, (u16)i, 1);
                }
            }
            work->step = ACTOR_121300_STEP_NONE;
            break;
        case ACTOR_121300_STEP_PLAY_SET_2:
            _actor121300BlendAnimationSet(arg0, 2);
            work->step = ACTOR_121300_STEP_NONE;
            break;
        case ACTOR_121300_STEP_SHATTER_LAMPS_WAVE:
            if (work->stepState == 0) {
                _actor121300SetDebrisSpeed(ACTOR_121300_DEBRIS_WAVE_SPEED_PERCENT);
                work->wave.span  = 0x3C;
                work->wave.scale = 0x100;
                work->waveTask   = taskSpawnFromTable(D_actor_121300_8013BBCC, 0, 0, &work->wave);
                work->stepState++;
            }
        case ACTOR_121300_STEP_SHATTER_LAMPS:
            _actor121300ShatterLamps(arg0);
            break;
        case ACTOR_121300_STEP_SHATTER_LAMPS_SLOW:
            _actor121300SetDebrisSpeed(ACTOR_121300_DEBRIS_SLOW_SPEED_PERCENT);
            _actor121300ShatterLamps(arg0);
            break;
        case ACTOR_121300_STEP_END_SHATTER:
            work->wave.state        = SCREEN_WAVE_RAMP_FINISHED;
            queue->imageMdecMode    = MDEC_IMAGE_MODE_RGB16;
            D_actor_121300_8013D41C = 0;
            work->step              = 0;
            break;
        case ACTOR_121300_STEP_HOLD_ON_MARK:
            if (work->stepState == 0) {
                TASK_MESSAGE_DISPATCH_POINTER(arg0, ACTOR_MESSAGE_PLACE, &D_actor_121300_8013CCA0, 0);
                {
                    _Actor121300AyaBreaWork* slotsWork;
                    s32                      i;

                    slotsWork          = arg0->work;
                    slotsWork->animSet = 1;
                    for (i = 1; (u16)i < ARRAY_SIZE(slotsWork->rig.slots); i++) {
                        slotsWork->rig.slots[(u16)i].rate = ANIMATION_RATE_ONE;
                        animationResetSlot(&slotsWork->rig.anim, (u16)i, 1);
                    }
                }
                dryfieldR08SelectLightingBank(1);
            }
            _actor121300SpawnRingSprites(arg0, 0);
            _actor121300SpawnUpperRowSprites(arg0, 0);
            break;
        case ACTOR_121300_STEP_PLAY_SET_3:
            if (work->stepState == 0) {
                _actor121300BlendAnimationSet(arg0, 3);
                work->stepState++;
            }
            _actor121300SpawnRingSprites(arg0, 0);
            break;
        case ACTOR_121300_STEP_SPRITES_DWINDLE:
            _actor121300SpawnUpperRowSprites(arg0, 1);
            _actor121300SpawnRingSprites(arg0, 0);
            break;
        case ACTOR_121300_STEP_WAVE_PULSE:
            switch (work->stepState) {
                case 0:
                    work->wave.span   = 8;
                    work->wave.scale  = 0x100;
                    work->waveTask    = taskSpawnFromTable(D_actor_121300_8013BBCC, 0, 0, &work->wave);
                    work->pulseFrames = 0;
                    work->stepState++;
                    break;
                case 1:
                    if (++work->pulseFrames >= 8) {
                        work->wave.state = SCREEN_WAVE_RAMP_FALLING;
                        work->wave.span  = 8;
                        work->step       = 0;
                    }
                    break;
            }
            break;
        case ACTOR_121300_STEP_RAISED_RING:
            _actor121300SpawnRingSprites(arg0, 1);
            break;
        case ACTOR_121300_STEP_MASK_STREAM:
            queue->imageMdecMode = MDEC_IMAGE_MODE_RGB16_MASK_BIT;
        case ACTOR_121300_STEP_NONE:
        default:
            work->step = ACTOR_121300_STEP_NONE;
            break;
    }
}

/// Initialize the cutscene model, animations and image-upload relocation.
///
/// Uses the area placement for resource-entry 0x84, or the end record when
/// that entry is absent. Allocation failure kills `task`.
static void func_actor_121300_80133BFC(Task* task)
{
    enum { TEXTURE_RESOURCE_ENTRY_ID = 0x84 };

    _Actor121300AyaBreaWork* work;
    _Actor121300AyaBreaWork* allocatedWork;
    _Actor121300AyaBreaWork* slotsWork;
    TmdObject*               tmd;
    GfxCoord*                coord;
    AreaPlacement*           place;
    s32                      slotIndex;
    u8                       entryId;

    tmd           = task->extra.tmd;
    coord         = tmd->coords;
    allocatedWork = memMalloc(sizeof(_Actor121300AyaBreaWork), false);
    task->work    = allocatedWork;
    if (allocatedWork == NULL) {
        taskKill(task);
        return;
    }
    work = allocatedWork;
    memFillBytes(work, 0, sizeof(*work));
    work->player            = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    D_actor_121300_8013D418 = task;
    coord->parent           = &gGfxViewCoord;
    tmd->lightMtx           = &work->lightMtx;
    tmd->flags              = 0;
    tmd->colorMtx           = &work->colorMtx;
    place                   = areaGetVariant(&gGameSession->location.loc)->placements;
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
    animationInitContext(&work->rig.anim, D_actor_121300_8013CC08, tmd, work->rig.poses,
                         work->rig.slots);
    slotsWork          = task->work;
    slotsWork->animSet = 1;
    slotIndex          = 1;
    do {
        slotsWork->rig.slots[(u16)slotIndex].rate = ANIMATION_RATE_ONE;
        animationResetSlot(&slotsWork->rig.anim, (u16)slotIndex, 1);
        slotIndex++;
    } while ((u16)slotIndex < ARRAY_SIZE(slotsWork->rig.slots));
    task->msgTable = D_actor_121300_8013CC88;
}

/// State machine of the cutscene actor, run once per frame from its slot.
/// State 0 waits while the attachment wheel is open (`Gp_StateC08.mode`) or
/// `gDisplayState.pendingMode` is live, and then builds the work block through
/// `func_actor_121300_80133BFC` and arms the player's weapon: the slot-3
/// message 0x3E8 record is `gPlayerStatus.weapon` plus 1 in the alternate weapon block
/// and plus 0x22 in the base one, with `field_4` 1 and the rest of the frame
/// zero.  State 1 hands the cutscene's two script blocks to `evsStartScriptWithSkip`,
/// state 2 spawns the `D_actor_121300_8013D390[9]` child while the session is
/// still down, and state 3 blanks the display, marks save slot 9 / the state
/// and re-arms the first tick before killing the task.
///
/// States 0, 1 and 2 all leave through the same `Task::state` increment; the
/// compiler cross-jumps the three copies, so it appears once, after state 2's
/// body.  Every path but state 3 also steps the actor through
/// `func_actor_121300_80133854` and hands the model's part-1 translation to
/// `worldCoordSetModelLighting`.
void func_actor_121300_80133D98(Task* arg0)
{
    TmdObject* extra;
    s32        state;
    s32        weaponId;
    s32        anim;

    state = arg0->state;
    switch (state) {
        case 0: {
            AnimationPlayRequest request;

            if ((Gp_StateC08.mode != ATTACHMENT_MODE_WHEEL) && (gDisplayState.pendingMode == DISPLAY_MODE_NONE)) {
                weaponId                     = gPlayerStatus.weapon;
                anim                         = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
                request.source.index         = anim;
                request.animationId          = 1;
                request.blend                = ANIMATION_BLEND_RESET;
                request.blendFrames          = 0;
                request.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &request, 0);
                func_actor_121300_80133BFC(arg0);
                arg0->state += 1;
                break;
            }
            return;
        }
        case 1:
            evsStartScriptWithSkip(D_actor_121300_8013CE08, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_actor_121300_8013D2E8);
            arg0->state += 1;
            break;
        case 2:
            if (gGameSession->eventState == 0) {
                taskSpawnFromTable(D_actor_121300_8013D390, 9, 0, 0);
                arg0->state += 1;
            }
            break;
        case 3: {
            RECT rect;

            rect.x = 0;
            rect.y = 0;
            rect.w = 0x140;
            rect.h = 0xF0;
            ClearImage(&rect, 0, 0, 0);
            rect.y = 0x110;
            ClearImage(&rect, 0, 0, 0);
            memFillBytes(Fs_ImgBuffers, 0, sizeof(*Fs_ImgBuffers));
            SetDispMask(1);
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage = state;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area  = 9;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp  = state;
            gDisplayState.spriteVariant                                 = 1;
            taskSpawn(0, 0x11, 0, 0);
            taskKill(arg0);
            return;
        }
    }
    func_actor_121300_80133854(arg0);
    {
        VECTOR pos;

        extra  = arg0->extra.tmd;
        pos.vx = extra->coords[1].workm.t[0];
        pos.vy = arg0->extra.tmd->coords[1].workm.t[1];
        pos.vz = arg0->extra.tmd->coords[1].workm.t[2];
        worldCoordSetModelLighting(extra, &pos, 0, 3);
    }
    D_actor_121300_8013CC00 += 1;
}

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

#include "../../shared/actor_messages_place_ypr.inc.c"

#include "../../shared/actor_messages_draw_mode.inc.c"

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

    return (s32)taskSpawnFromTable(D_actor_121300_8013D390, ACTOR_121300_TEXTURE_UPLOAD_TASK_INDEX, textureSequence, task);
}

/// Selects the double's scene step and resets its progress for the next update.
///
/// Requires the published double task and work to be live. `step` is an
/// `ACTOR_121300_STEP_*` value, stored as its unsigned low halfword.
static void _actor121300SelectSceneStep(s16 step)
{
    _Actor121300AyaBreaWork* work = D_actor_121300_8013D418->work;

    work->step      = step;
    work->stepState = 0;
}

/// Restores RGB16 scene-image decoding without the mask bit.
static void _actor121300RestoreStreamImageMode(void)
{
    gCdCmdQueue.imageMdecMode = MDEC_IMAGE_MODE_RGB16;
}

void func_actor_121300_8013427C(void)
{
    _Actor121300AyaBreaWork* work = D_actor_121300_8013D418->work;

    D_actor_121300_8013D41C   = 0;
    work->wave.state          = SCREEN_WAVE_RAMP_FINISHED;
    gCdCmdQueue.imageMdecMode = MDEC_IMAGE_MODE_RGB16;
    taskMessageDispatch(work->player, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
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

    taskSpawnFromTable(D_actor_121300_8013D390, ACTOR_121300_FADE_IN_TASK_INDEX, intensityStep, 0);
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

    taskSpawnFromTable(D_actor_121300_8013D390, ACTOR_121300_FADE_OUT_TASK_INDEX, intensityStep, 0);
}

/// Sets the double model's draw mode from an event-script callback.
///
/// Requires the published double task to be live. `drawMode` uses
/// `ACTOR_MESSAGE_DRAW_*`; the actor's message handler leaves other model flags
/// intact. The script uses SHOW and HIDE_SKIP_AUTO_BUFFER.
static void _actor121300SetDoubleDrawMode(s32 drawMode)
{
    taskMessageDispatch(D_actor_121300_8013D418, ACTOR_MESSAGE_SET_MODEL_DRAW, drawMode, 0);
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

void func_actor_121300_801343A4(void)
{
    streamFinishScene();
    cdCmdCancelScene();
}
