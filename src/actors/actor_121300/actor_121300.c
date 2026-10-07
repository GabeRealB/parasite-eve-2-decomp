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
/// that goes out, 4 to 8 are the debris variants `func_actor_121300_80133064`
/// scatters around a lamp, 9 is spawned once the session event has
/// ended, and 0xA by `func_actor_121300_80134224`.
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
/// effect spawners gate on: `func_actor_121300_8013343C` only runs on every
/// fourth frame (`& 3`), `func_actor_121300_80133580` too.
extern s32 D_actor_121300_8013CC00;

/// The two position tables `func_actor_121300_8013343C` walks, each an array
/// of `SVECTOR`s ending on a zeroed one -- the walker's guard is `vx != 0`, so
/// the sentinel is read with the position.  Both trace the same ring around
/// the arena (`vx` 2500..6500 at `vz` 4700, then back at 1500) and differ only
/// in height: `8013CCB8` sits at ground level, `8013CD48` at `vy` -0xC8.
extern SVECTOR D_actor_121300_8013CCB8[];
extern SVECTOR D_actor_121300_8013CD48[];
/// Spawn points of `func_actor_121300_80133580`, of which the first
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

s32 func_actor_121300_80134224(Task*, s32, s32, s32);

static TmdSource _gActor121300AyaBreaBody;
static TmdSource _gActor121300Model07E84;
static TmdSource _gActor121300Model080F0;
static TmdSource _gActor121300Model0834C;
static TmdSource _gActor121300Model08580;
static TmdSource _gActor121300Model08768;
void             func_actor_121300_801326EC(Task*);
void             func_actor_121300_8013293C(Task*);
void             func_actor_121300_80133064(Task*);
void             func_actor_121300_8013322C(Task*);
void             func_actor_121300_80133D98(Task*);
void             func_actor_121300_8013400C(Task*);
void             func_actor_121300_801340F0(Task*);
void             func_actor_121300_80134250(s16);
void             func_actor_121300_80134270(void);
void             func_actor_121300_8013427C(void);
void             func_actor_121300_801342D4(s32);
void             func_actor_121300_80134304(s32);
void             func_actor_121300_80134334(s32);
void             func_actor_121300_80134364(void);
void             func_actor_121300_80134384(void);
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
    { { { TASK_BODY_NONE, 192 } }, screenWaveTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

s32 gScreenWaveRamp = 256;

u_long D_actor_121300_8013BBE8[250] = {
    0x4B372A2B,
    0x76915959,
    0x808455D,
    0x35080808,
    0x4A897464,
    0x704B6C49,
    0xAAA6AAAA,
    0x8295498,
    0x8080808,
    0x7E290808,
    0x54090850,
    0x49494C70,
    0x3A3E2A39,
    0x8A6C4A4C,
    0x3547517F,
    0x8082929,
    0x65352908,
    0x596D4A89,
    0xAAAA9170,
    0x356576AA,
    0x8080808,
    0x9080808,
    0x8350809,
    0x41672908,
    0x3A4B4A49,
    0x594B4C3A,
    0x537D535C,
    0x7D787878,
    0x29292947,
    0x49705235,
    0xAA91876D,
    0x6598AAAA,
    0x8080829,
    0x9080808,
    0x787D4708,
    0x4653785E,
    0x4A4A4648,
    0x4B39384A,
    0xD59F6E4A,
    0x85E0E05C,
    0x789F8585,
    0x67684653,
    0xAA4A4941,
    0x76919191,
    0x29293551,
    0x29080829,
    0x5C784636,
    0xDEDEDEDE,
    0x785E5C85,
    0x4B4A4A5B,
    0x6E594B4C,
    0xE6BAB89F,
    0x85E0D587,
    0x85DEDEDE,
    0x498A5E9F,
    0x73735087,
    0x43655F75,
    0x35294543,
    0xDE5C7858,
    0x9FE085DE,
    0xD58A8AD5,
    0x4A9F5C9F,
    0x4A4A4A4A,
    0xC2864A59,
    0x7D91AD6F,
    0x9FED8AAD,
    0xDEDE85E0,
    0x415CE085,
    0x50777373,
    0x7F617750,
    0xDE855E53,
    0x8A9FE085,
    0x7D7D7DAD,
    0x8A6FAD76,
    0x4A4A5BED,
    0xC249594A,
    0x6858AD87,
    0x81696969,
    0xE66F917F,
    0x8585E0B8,
    0x5D739F85,
    0x7373755D,
    0xDEDED573,
    0x8786E085,
    0x36697FAD,
    0x80573636,
    0x879DAD98,
    0x59494A4B,
    0x53789D4B,
    0x9F9F9F56,
    0x7D569F9F,
    0xED9DD6AD,
    0x9F8585E0,
    0x73725D70,
    0xE06F7373,
    0xE6B8E085,
    0x53416F9D,
    0x9F9F9F56,
    0x7D539F9F,
    0x4A4CBF6F,
    0x6FE64959,
    0xAEAED25E,
    0xAEAEAEAE,
    0x9F5CCFD2,
    0xBABF878A,
    0x72A6E6B8,
    0x918C7373,
    0xE6B89F6F,
    0xCF9F87C2,
    0xAEAED2D2,
    0xAEAEAEAE,
    0x878ACFD2,
    0x49494A5B,
    0xAED29F87,
    0xAEAEAEAE,
    0xAEAEAEAE,
    0x875CAEAE,
    0xC2E2BF86,
    0x8C7373A1,
    0xC2898C8C,
    0xCFEDBFE2,
    0xAEAEAEBB,
    0xAEAEAEAE,
    0xAEAEAEAE,
    0x49878ACF,
    0xD29F5949,
    0x107D2AE,
    0x1010201,
    0xAEAE0101,
    0xE6ED5CBB,
    0x8CCAC2E2,
    0xA68C8C73,
    0x86E2C0CB,
    0xAEB7D29F,
    0x1010102,
    0x7010102,
    0xCFAEAED2,
    0x5C49498A,
    0x5E5CD2CF,
    0x2040504,
    0x7070202,
    0xCFBBD207,
    0xC2C29D9D,
    0x8C738CCA,
    0xC0C5A68C,
    0xD29F9DC0,
    0x110707D2,
    0x1020205,
    0x5E5E0405,
    0x599FD2BB,
    0xCF5C5B4A,
    0x9B11537D,
    0x2FA076A,
    0x5E040404,
    0xC59D5C5E,
    0x73CAC5C5,
    0xD4737376,
    0x9DC5C5C5,
    0x4565C9F,
    0xAE9B9B05,
    0x5110702,
    0xCF5C9856,
    0x87494A6F,
    0x587DCF8A,
    0x4116A11,
    0x5110702,
    0x8A535304,
    0xC5C5CB87,
    0x75777389,
    0xC5C58C75,
    0x568A9DC5,
    0x6A040546,
    0x4010205,
    0x58460511,
    0x86AD5C5C,
    0x5CA6894A,
    0x56A485E,
    0x6A050505,
    0x5811059B,
    0xCA9D7053,
    0x76A1CACA,
    0x8C7A5F62,
    0xC5CBC5C5,
    0x5648566F,
    0x56A1104,
    0x11050505,
    0x78CF5348,
    0x894A8791,
    0x53CF8A8C,
    0x9B6A6A46,
    0x56A9B9B,
    0xA6985853,
    0xA6A1A1A1,
    0x515175A6,
    0xCBC59162,
    0x73A1CACB,
    0x11054658,
    0x6A6A9B6A,
    0x5E469B11,
    0x87A97D5E,
    0x768F894A,
    0x53535E5E,
    0x6A6A6A6A,
    0x50585305,
    0x8C717171,
    0x778C8C8C,
    0xAA7E4365,
    0xA1CACBC5,
    0x58988CA6,
    0x6A115653,
    0x53539B6A,
    0x7E985E5E,
    0xCA4A9D8F,
    0x467F7EAB,
    0x46467D53,
    0x517F5846,
    0xB0957A63,
    0x8C7192B0,
    0x45527C73,
    0x89CBAA51,
    0x928CA6A1,
    0x7F635F7B,
    0x46464646,
    0x7F53537D,
    0x87DC5254,
    0x52B0894A,
    0x57575757,
    0x29293535,
    0x52453535,
    0x92B09560,
    0x6292718F,
    0xAA514552,
    0xAAA6A189,
    0x65519A73,
    0x35455265,
    0x57575735,
    0x57353535,
    0xA14C5B75,
    0x35575295,
    0x8292935,
    0x29290808,
    0x7C645729,
    0x8F8F92B0,
    0x45456375,
    0xA6A1AA51,
    0x656191AA,
    0x35574552,
    0x29292935,
    0x35292929,
    0x59704529,
};

u_long D_actor_121300_8013BFD0[250] = {
    0x4B372A2B,
    0x76915959,
    0x808455D,
    0x35080808,
    0x4A897464,
    0x704B6C49,
    0xAAA6AAAA,
    0x8295498,
    0x8080808,
    0x7E290808,
    0x54090850,
    0x49494C70,
    0x3A3E2A39,
    0x8A6C4A4C,
    0x3547517F,
    0x8082929,
    0x65352908,
    0x596D4A89,
    0xAAAA9170,
    0x356576AA,
    0x8080808,
    0x9080808,
    0x8350809,
    0x41672908,
    0x3A4B4A49,
    0x594B4C3A,
    0x537D535C,
    0x7D787878,
    0x29292947,
    0x49705235,
    0xAA91876D,
    0x6598AAAA,
    0x8080829,
    0x9080808,
    0x787D4708,
    0x4653785E,
    0x4A4A4648,
    0x4B39384A,
    0xD59F6E4A,
    0x85E0E05C,
    0x789F8585,
    0x67684653,
    0xAA4A4941,
    0x76919191,
    0x29293551,
    0x29080829,
    0x5C784636,
    0xDEDEDEDE,
    0x785E5C85,
    0x4B4A4A5B,
    0x6E594B4C,
    0xE6BAB89F,
    0x85E0D587,
    0x85DEDEDE,
    0x498A5E9F,
    0x73735087,
    0x43655F75,
    0x35294543,
    0xDE5C7858,
    0x9FE085DE,
    0xD58A8AD5,
    0x4A9F5C9F,
    0x4A4A4A4A,
    0xC2864A59,
    0x7D91AD87,
    0x9FED8AAD,
    0xDEDE85E0,
    0x415CE085,
    0x50777373,
    0x7F617750,
    0xDE855E53,
    0x8A9FE085,
    0x7D7D7DAD,
    0x8A6FAD76,
    0x4A4A5BED,
    0xC249594A,
    0x687F8787,
    0x81686868,
    0xE66F917F,
    0x8585E0B8,
    0x5D739F85,
    0x7373755D,
    0xDEDED573,
    0x8786E085,
    0x81817FAD,
    0x98818181,
    0x879DAD6F,
    0x59494A4B,
    0x91789D4B,
    0x69686868,
    0x7F816969,
    0xED9DD691,
    0x9F8585E0,
    0x73725D70,
    0xE06F7373,
    0xE6B8E085,
    0x817F6F9D,
    0x81696969,
    0x7D988181,
    0x4A4CBF6F,
    0x6FE64959,
    0x69687F5E,
    0x69696969,
    0xD67F8169,
    0xBABF87D6,
    0x72A6E6B8,
    0x918C7373,
    0xE6B89F6F,
    0x7F6F6FC2,
    0x69696981,
    0x81816969,
    0x878A7D98,
    0x49494A5B,
    0xD6D69F87,
    0xBBED8787,
    0xEDBBBBBB,
    0x87D68787,
    0xC2E2BF86,
    0x8C7373A1,
    0xC2898C8C,
    0x6F87BFE2,
    0xBBED876F,
    0xD2BBBBBB,
    0xD6875BCF,
    0x49878AD6,
    0x9F9F5949,
    0xAEAED25C,
    0xAEAEAEAE,
    0x5CBBAEAE,
    0xE6ED87ED,
    0x8CCAC2E2,
    0xA68C8C73,
    0x86E2C0CB,
    0xAEAE9F9F,
    0xAEAEAEAE,
    0xAEAEAEAE,
    0x875BBBAE,
    0x5C49498A,
    0xAEAED2CF,
    0x2020202,
    0x2020202,
    0xED5CAEAE,
    0xC2C29D9D,
    0x8C738CCA,
    0xC0C5A68C,
    0xD25C9DC0,
    0x20202D2,
    0x2020202,
    0xAEAE0202,
    0x599FD2BB,
    0xD25C5B4A,
    0x20702AE,
    0x2FA0202,
    0x5C070707,
    0xC59D5CBB,
    0x73CAC5C5,
    0xD4737376,
    0x9DC5C5C5,
    0x4565CD2,
    0x1070707,
    0x2020202,
    0xCFAEAE5E,
    0x87494A6F,
    0x5C02CF8A,
    0x2070704,
    0x5050702,
    0x8A5E5304,
    0xC5C5CB87,
    0x75777389,
    0xC5C58C75,
    0x568A9DC5,
    0x11040546,
    0x4010205,
    0x5E460404,
    0x86AD5CAE,
    0x5CA6894A,
    0x5045ED2,
    0x6A05056A,
    0x5811056A,
    0xCA9D7053,
    0x76A1CACA,
    0x8C7A5F62,
    0xC5CBC5C5,
    0x5648566F,
    0x56A1104,
    0x4040505,
    0x78CF5348,
    0x894A8791,
    0x535E8A8C,
    0x11050446,
    0x5116A6A,
    0xA6985853,
    0xA6A1A1A1,
    0x515175A6,
    0xCBC59162,
    0x73A1CACB,
    0x4054658,
    0x46A9B6A,
    0x5E460504,
    0x87A97D5E,
    0x768F894A,
    0x53535E8A,
    0x5050504,
    0x50585305,
    0x8C717171,
    0x778C8C8C,
    0xAA7E4365,
    0xA1CACBC5,
    0x58988CA6,
    0x5115653,
    0x53040405,
    0x7E985E5E,
    0xCA4A9D8F,
    0x467F7EAB,
    0x53535353,
    0x517F5846,
    0xB0957A63,
    0x8C7192B0,
    0x45527C73,
    0x89CBAA51,
    0x928CA6A1,
    0x7F635F7B,
    0x53464646,
    0x7F535353,
    0x87DC5254,
    0x52B0894A,
    0x57575757,
    0x29293535,
    0x52453535,
    0x92B09560,
    0x6292718F,
    0xAA514552,
    0xAAA6A189,
    0x65519A73,
    0x35455265,
    0x57575735,
    0x57353535,
    0xA14C5B75,
    0x35575295,
    0x8292935,
    0x29290808,
    0x7C645729,
    0x8F8F92B0,
    0x45456375,
    0xA6A1AA51,
    0x656191AA,
    0x35574552,
    0x29292935,
    0x35292929,
    0x59704529,
};

u_long D_actor_121300_8013C3B8[250] = {
    0x4B372A2B,
    0x76915959,
    0x808455D,
    0x35080808,
    0x4A897464,
    0x704B6C49,
    0xAAA6AAAA,
    0x8295498,
    0x8080808,
    0x7E290808,
    0x54090850,
    0x49494C70,
    0x3A3E2A39,
    0x8A6C4A4C,
    0x3547517F,
    0x8082929,
    0x65352908,
    0x596D4A89,
    0xAAAA9170,
    0x356576AA,
    0x8080808,
    0x9080808,
    0x8350809,
    0x41672908,
    0x3A4B4A49,
    0x594B4C3A,
    0x537D535C,
    0x7D787878,
    0x29292947,
    0x49705235,
    0xAA91876D,
    0x6598AAAA,
    0x8080829,
    0x9080808,
    0x787D4708,
    0x4653785E,
    0x4A4A4648,
    0x4B39384A,
    0xD59F6E4A,
    0x85E0E05C,
    0x789F8585,
    0x67684653,
    0xAA4A4941,
    0x76919191,
    0x29293551,
    0x29080829,
    0x5C784636,
    0xDEDEDEDE,
    0x785E5C85,
    0x4B4A4A5B,
    0x6E594B4C,
    0xE6BAB89F,
    0x85E0D587,
    0x85DEDEDE,
    0x498A5E9F,
    0x73735087,
    0x43655F75,
    0x35294543,
    0xDE5C7858,
    0x9FE085DE,
    0xD58A8AD5,
    0x4A9F5C9F,
    0x4A4A4A4A,
    0xC2864A59,
    0x7D91AD87,
    0x9FED8AAD,
    0xDEDE85E0,
    0x415CE085,
    0x50777373,
    0x7F617750,
    0xDE855E53,
    0x8A9FE085,
    0x7D7D7DAD,
    0x8A6FAD76,
    0x4A4A5BED,
    0xC249594A,
    0x91918787,
    0x91919191,
    0xE66F9191,
    0x8585E0B8,
    0x5D739F85,
    0x7373755D,
    0xDEDED573,
    0x8786E085,
    0x767676AD,
    0x76767676,
    0x879DAD6F,
    0x59494A4B,
    0x91789D4B,
    0x7F7F9191,
    0x917F7F7F,
    0xED9DD691,
    0x9F8585E0,
    0x73725D70,
    0xE06F7373,
    0xE6B8E085,
    0x76766F9D,
    0x7F7F7F7F,
    0x7D76767F,
    0x4A4CBF6F,
    0x78E64959,
    0x7F7F9191,
    0x7F7F7F7F,
    0xD6917F7F,
    0xBABF87D6,
    0x72A6E6B8,
    0x918C7373,
    0xE6B89F6F,
    0x76916FC2,
    0x7F7F7F7F,
    0x7F7F7F7F,
    0x878A7D76,
    0x49494A5B,
    0x7F91D687,
    0x6969697F,
    0x81696969,
    0x87D6917F,
    0xC2E2BF86,
    0x8C7373A1,
    0xC2898C8C,
    0x9187BFE2,
    0x69817F91,
    0x69696969,
    0x767F7F69,
    0x49878AD6,
    0xD69F5949,
    0x69817F91,
    0x69696969,
    0x91816969,
    0xE6EDD6D6,
    0x8CCAC2E2,
    0xA68C8C73,
    0x86E2C0CB,
    0x81919187,
    0x69696969,
    0x69696969,
    0x8776767F,
    0x5C49498A,
    0x7F91D69F,
    0x69696981,
    0x69696969,
    0xD6919181,
    0xC2C29D9D,
    0x8C738CCA,
    0xC0C5A68C,
    0x919D9DC0,
    0x69698191,
    0x69696969,
    0x767F6969,
    0x59878776,
    0xD65C5B4A,
    0x817F7FD6,
    0x81818181,
    0x917F7F81,
    0xC59DD691,
    0x73CAC5C5,
    0xD4737376,
    0x9DC5C5C5,
    0x7F919191,
    0x8181817F,
    0x7F818181,
    0x8787767F,
    0x87494A87,
    0xD2D2ED8A,
    0xD6D6D6ED,
    0xEDD6D6D6,
    0xEDAEAEED,
    0xC5C5CB87,
    0x75777389,
    0xC5C58C75,
    0xAEED9DC5,
    0xD6EDEDAE,
    0xD6D6D6D6,
    0xD2EDD6D6,
    0x86ADEDD2,
    0x9FA6894A,
    0xAEAEAE5C,
    0xAEAEAEAE,
    0xD2AEAEAE,
    0xCA9D70ED,
    0x76A1CACA,
    0x8C7A5F62,
    0xC5CBC5C5,
    0xAED2ED6F,
    0xAEAEAEAE,
    0xAEAEAEAE,
    0x785CAEAE,
    0x894A8791,
    0xED8A8A8C,
    0xAEAED25C,
    0xD2AEAEAE,
    0xA698EDED,
    0xA6A1A1A1,
    0x515175A6,
    0xCBC59162,
    0x73A1CACB,
    0xD2EDED8C,
    0xAEAEAEAE,
    0xED5CD2AE,
    0x87A97D76,
    0x768F894A,
    0xA1767676,
    0xEDEDEDED,
    0x5076A1ED,
    0x8C717171,
    0x778C8C8C,
    0xAA7E4365,
    0xA1CACBC5,
    0x8C8C8CA6,
    0xEDEDA18C,
    0xA1EDEDED,
    0x7E987676,
    0xCA4A9D8F,
    0x7F7F7EAB,
    0x7F7F7F7F,
    0x7F7F7F7F,
    0xB0957A63,
    0x8C7192B0,
    0x45527C73,
    0x89CBAA51,
    0x928CA6A1,
    0x7F635F7B,
    0x7F7F7F7F,
    0x7F7F7F7F,
    0x87DC5254,
    0x52B0894A,
    0x57575757,
    0x29293535,
    0x52453535,
    0x92B09560,
    0x6292718F,
    0xAA514552,
    0xAAA6A189,
    0x65519A73,
    0x35455265,
    0x57575735,
    0x57353535,
    0xA14C5B75,
    0x35575295,
    0x8292935,
    0x29290808,
    0x7C645729,
    0x8F8F92B0,
    0x45456375,
    0xA6A1AA51,
    0x656191AA,
    0x35574552,
    0x29292935,
    0x35292929,
    0x59704529,
};

u_long D_actor_121300_8013C7A0[140] = {
    0x42524545,
    0x79606042,
    0x41AA9074,
    0x6F8A9D6F,
    0x6079908C,
    0x45525242,
    0x35353545,
    0x42425245,
    0x42424242,
    0x64424242,
    0x52635061,
    0x52525245,
    0x45455252,
    0x45454545,
    0x42424242,
    0x42424242,
    0x45524242,
    0x35353535,
    0x57353535,
    0x45525252,
    0x52525245,
    0x42424242,
    0x42424242,
    0x45455242,
    0x35353535,
    0x57353529,
    0x52525252,
    0x52525252,
    0x42424242,
    0x42424242,
    0x52986052,
    0x57292929,
    0x45457E7F,
    0x52525252,
    0x64525252,
    0x42424264,
    0x735F4242,
    0x87B88689,
    0x8770ADAD,
    0x709D5ABA,
    0x4242607B,
    0x60606464,
    0x60426460,
    0x595B707A,
    0x6C6C6D6D,
    0x6D6C6D6D,
    0x596E6E6E,
    0x607589BA,
    0x79796064,
    0x98606464,
    0x788A8A6F,
    0x8A787878,
    0x8AD5D5D5,
    0x8A8A8A8A,
    0x9187EDED,
    0x79606079,
    0x9A626464,
    0x4552657E,
    0x8083557,
    0x29080808,
    0x67553529,
    0x767D987F,
    0x6060605F,
    0x60606464,
    0x65656464,
    0x29354565,
    0x29290829,
    0x617E5735,
    0x7A7A9898,
    0x6060605F,
    0x60606460,
    0x76927A5F,
    0x657E7BB1,
    0x65656565,
    0xA6AA759A,
    0x609691A6,
    0x60796060,
    0x60646460,
    0x91767A5F,
    0xB1DB9191,
    0x91DBACB1,
    0xAAAAA6AA,
    0x607A8F8C,
    0x60606060,
    0x60646060,
    0x91737762,
    0xD4AAAAAA,
    0xAA8C8CD4,
    0x73918CAA,
    0x60629575,
    0x79796060,
    0x60606079,
    0x76777960,
    0xAC737373,
    0x92927292,
    0x7A959292,
    0x60606062,
    0x93747979,
    0x60797979,
    0x63646464,
    0x65667E62,
    0x64646465,
    0x64656464,
    0x60646464,
    0x93937479,
    0x60797493,
    0x45524264,
    0x29293535,
    0x35353529,
    0x45573535,
    0x60644252,
    0x8D937479,
    0x79749390,
    0x35456460,
    0x9080808,
    0x8090909,
    0x35290808,
    0x64645245,
    0x8D937479,
    0x7493908E,
    0x35526079,
    0x8080829,
    0x9090909,
    0x29080808,
    0x60645257,
    0x908D7479,
    0x8E8FA98C,
    0x646074AF,
    0x29353545,
    0x8080829,
    0x35080808,
    0x79606545,
    0x8BD99093,
    0xA4D4D4CE,
    0x96B08FA9,
    0x52656462,
    0x35354545,
    0x52573535,
    0x90937964,
    0xA2D3D7D9,
};

u_long D_actor_121300_8013C9D0[140] = {
    0x42524545,
    0x79606042,
    0x41AA9074,
    0x6F8A9D6F,
    0x6079908C,
    0x45525242,
    0x35353545,
    0x42425245,
    0x42424242,
    0x64424242,
    0x52635061,
    0x52525245,
    0x45455252,
    0x45454545,
    0x42424242,
    0x42424242,
    0x45524242,
    0x35353535,
    0x57353535,
    0x45525252,
    0x52525245,
    0x42424242,
    0x42424242,
    0x7F7F6042,
    0x7F7F7F7F,
    0x57607F7F,
    0x52525252,
    0x52525252,
    0x42424242,
    0x42424242,
    0x6D6D5A89,
    0x6D6D6D6D,
    0x899D5A6D,
    0x52525289,
    0x64525252,
    0x42424264,
    0x6D604242,
    0x40401DD,
    0x4040404,
    0xDD010404,
    0x42646DDD,
    0x60606464,
    0x60426460,
    0xDD6D6065,
    0xF6F6F6F6,
    0xF6F6F6F6,
    0xF6F6F6F6,
    0x646DDDF6,
    0x79796064,
    0x65606464,
    0xF6DD6D60,
    0xF7F7F6F6,
    0xF7F7F7F7,
    0xF6F7F7F7,
    0x6DDDF6F6,
    0x79606079,
    0x65626464,
    0xF6F6DD6D,
    0xDDF7F7F6,
    0xDDDDDDDD,
    0xF7F7DDDD,
    0xDDF6F6F6,
    0x6060606D,
    0x60606464,
    0xF6F6DD6D,
    0xDDDDF6F6,
    0x6B6B6BDE,
    0xF6DDDDDE,
    0xDDF6F6F6,
    0x6060606D,
    0x60606460,
    0xF6F6DD6D,
    0x6BDDF6F6,
    0x6B6B6B6B,
    0xF6DD6B6B,
    0xDDF6F6F6,
    0x6077646D,
    0x45776460,
    0xF6F6DD6D,
    0xDEDDF6F6,
    0x6B6B6B6B,
    0xF6DDDE6B,
    0xDDF6F6F6,
    0x6077646D,
    0x45776060,
    0xF6DD6DD3,
    0xDDF6F6F6,
    0xDEDEDEDE,
    0xF6F6DDDE,
    0x6DDDF6F6,
    0x797764D3,
    0x45776079,
    0xDDD3D335,
    0xF6F6F6F6,
    0xF6F6F6F6,
    0xF6F6F6F6,
    0xD3D3DDF6,
    0x93776435,
    0x64777779,
    0x77353545,
    0x5A8989D3,
    0x5A5A5A5A,
    0xD389895A,
    0x35356477,
    0x93776445,
    0x77777793,
    0x35456464,
    0x64353535,
    0x64646464,
    0x35356464,
    0x45453535,
    0x8D937764,
    0x77777790,
    0x64647777,
    0x9083545,
    0x8090909,
    0x35290808,
    0x77776464,
    0x8D937477,
    0x7777908E,
    0x77777777,
    0x77777777,
    0x77777777,
    0x77777777,
    0x60777777,
    0x908D7479,
    0x8E8FA98C,
    0x646074AF,
    0x52525252,
    0x52525252,
    0x52525252,
    0x79606552,
    0x8BD99093,
    0xA4D4D4CE,
    0x96B08FA9,
    0x52656462,
    0x52525245,
    0x79525252,
    0x90937979,
    0xA2D3D7D9,
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
    { 2016, func_actor_121300_80134224 },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_121300_80134250 }, { .value = ACTOR_121300_STEP_REPLACE_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = SetDispMask }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_actor_121300_8013CE00 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_121300_80134364 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_121300_80134384 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_121300_801342D4 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_121300_80134250 }, { .value = ACTOR_121300_STEP_PLAY_SET_2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_121300_80134250 }, { .value = ACTOR_121300_STEP_MASK_STREAM }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_121300_80134334 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_121300_80134250 }, { .value = ACTOR_121300_STEP_WAVE_PULSE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_121300_80134250 }, { .value = ACTOR_121300_STEP_SHATTER_LAMPS }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_121300_80134250 }, { .value = ACTOR_121300_STEP_SHATTER_LAMPS_SLOW }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_121300_80134250 }, { .value = ACTOR_121300_STEP_SHATTER_LAMPS_WAVE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_121300_80134304 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_121300_80134334 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = SetDispMask }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_121300_80134250 }, { .value = ACTOR_121300_STEP_END_SHATTER }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_121300_80134250 }, { .value = ACTOR_121300_STEP_HOLD_ON_MARK }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_121300_801342D4 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_121300_80134250 }, { .value = ACTOR_121300_STEP_SPRITES_DWINDLE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_121300_80134250 }, { .value = ACTOR_121300_STEP_PLAY_SET_3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_121300_80134304 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_121300_80134334 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_121300_801342D4 }, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_121300_80134250 }, { .value = ACTOR_121300_STEP_RAISED_RING }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_121300_80134304 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_121300_80134270 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_121300_801343A4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_121300_80134250 }, { .value = ACTOR_121300_STEP_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_121300_8013D2E8[7] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_121300_80134250 }, { .value = ACTOR_121300_STEP_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_121300_8013427C }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc D_actor_121300_8013D390[11] = {
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_121300_80133D98, { .model = &_gActor121300AyaBreaBody } },
    { { { TASK_BODY_NONE, 192 } }, func_actor_121300_801326EC, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_actor_121300_8013400C, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_actor_121300_80133064, { .value = 0 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_121300_8013293C, { .model = &_gActor121300Model07E84 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_121300_8013293C, { .model = &_gActor121300Model080F0 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_121300_8013293C, { .model = &_gActor121300Model0834C } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_121300_8013293C, { .model = &_gActor121300Model08580 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_121300_8013293C, { .model = &_gActor121300Model08768 } },
    { { { TASK_BODY_NONE, 192 } }, func_actor_121300_801340F0, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_actor_121300_8013322C, { .value = 0 } },
};

ScreenWaveCtx* gScreenWaveCtx = NULL;

Task* D_actor_121300_8013D418;

u16 D_actor_121300_8013D41C;

ScreenWaveOscillator gScreenWaveColumns[13];

ScreenWaveOscillator gScreenWaveRows[30];

static s32         func_actor_121300_80132818(Task* arg0);
static void        func_actor_121300_8013343C(Task* arg0, s16 arg1);
static void        func_actor_121300_80133580(Task* arg0, s16 arg1);
static void        func_actor_121300_80133730(Task* arg0);
static inline void func_actor_121300_PlayAll(Task* arg0, s32 anim);
static inline void func_actor_121300_SetCC04(s32 v);
static void        func_actor_121300_80133854(Task* arg0);
static void        func_actor_121300_80133BFC(Task* task);

#include "../../shared/screen_wave.inc.c"

void func_actor_121300_801326EC(Task* arg0)
{
    ScreenFadeWork* fade;
    ScreenFadeWork* alloc;

    fade = arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = memMalloc(sizeof(*alloc), false);
            arg0->work = alloc;
            if (alloc == NULL) {
                taskKill(arg0);
                return;
            }
            fade    = alloc;
            fade->r = 0xFF;
            fade->g = 0xFF;
            fade->b = 0xFF;
            fadeDrawOverlay(fade->r, fade->g, fade->b, GPU_BLEND_SUBTRACT);
            arg0->state += 1;
            break;
        case 2:
            SetDispMask(1);
            /* fallthrough */
        case 1:
            fadeDrawOverlay(fade->r, fade->g, fade->b, GPU_BLEND_SUBTRACT);
            arg0->state += 1;
            break;
        case 3:
            fadeDrawOverlay(fade->r, fade->g, fade->b, GPU_BLEND_SUBTRACT);
            fade->r -= (u16)arg0->spawnArg1.value;
            fade->g -= (u16)arg0->spawnArg1.value;
            fade->b -= (u16)arg0->spawnArg1.value;
            if (fade->r < 0) {
                taskKill(arg0);
            }
            break;
    }
}

/// Slot re-arm of the cutscene actor: ticks all nineteen animation slots, and
/// once every one of slots 1..18 has `ANIMATION_SLOT_SETTLED` set,
/// hands them the animation id `D_actor_121300_8013CC18` holds for the current
/// `animSet`, blending it in over ten frames.  A negative table entry leaves
/// the slots alone and only the return value follows.  The gotos reproduce
/// retail's block layout.
static s32 func_actor_121300_80132818(Task* arg0)
{
    _Actor121300AyaBreaWork* work;
    _Actor121300AyaBreaWork* ctx;
    u16                      i;
    u16                      done;
    u16                      anim;

    work = arg0->work;
    for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
        animationTickSlot(&work->rig.anim, i);
    }
    i    = 1;
    done = 1;
    for (; i < ARRAY_SIZE(work->rig.slots); i++) {
        if (!(work->rig.slots[i].status.fields.flags & ANIMATION_SLOT_SETTLED)) {
            goto fail;
        }
    }
check:
    if (done) {
        if (D_actor_121300_8013CC18[work->animSet] >= 0) {
            anim         = D_actor_121300_8013CC18[work->animSet];
            ctx          = arg0->work;
            ctx->animSet = anim;
            goto loop;
        fail:
            done = 0;
            goto check;
        loop:
            for (i = 1; i < ARRAY_SIZE(ctx->rig.slots); i++) {
                animationSeekSlotWithBlend(&ctx->rig.anim, i, anim, 0, 10);
            }
        }
        return 1;
    }
    return 0;
}

void func_actor_121300_8013293C(Task* arg0)
{
    _Actor121300DebrisWork* work;
    TmdObject*              obj;
    GfxCoord*               coord;
    VECTOR                  pos;
    _Actor121300DebrisWork* alloc;
    s16                     r;
    TmdObject*              tail;

    work  = arg0->work;
    obj   = arg0->extra.tmd;
    coord = obj->coords;
    if (D_actor_121300_8013D41C == 0) {
        taskKill(arg0);
        return;
    }
    switch (arg0->state) {
        case 0:
            alloc      = memMalloc(sizeof(*alloc), false);
            arg0->work = alloc;
            if (alloc == NULL) {
                taskKill(arg0);
                return;
            }
            work = alloc;
            memFillBytes(work, 0, sizeof(*work));
            coord->parent     = &gGfxViewCoord;
            coord->coord.t[0] = D_actor_121300_8013CC20[arg0->spawnArg1.value].x;
            coord->coord.t[1] = D_actor_121300_8013CC20[arg0->spawnArg1.value].y;
            coord->coord.t[2] = D_actor_121300_8013CC20[arg0->spawnArg1.value].z;
            switch (arg0->spawnArg2.unsignedValue) {
                case 0:
                case 14:
                    break;
                case 1:
                    coord->coord.t[0] += 50;
                    coord->coord.t[1] += 50;
                    break;
                case 2:
                    coord->coord.t[0] -= 50;
                    coord->coord.t[1] += 50;
                    break;
                case 3:
                    coord->coord.t[0] += 50;
                    coord->coord.t[1] -= 50;
                    break;
                case 4:
                    coord->coord.t[0] -= 50;
                    coord->coord.t[1] -= 50;
                    break;
                case 5:
                    coord->coord.t[0] += 80;
                    coord->coord.t[1] += 80;
                    break;
                case 6:
                    coord->coord.t[0] -= 80;
                    coord->coord.t[1] += 80;
                    break;
                case 7:
                    coord->coord.t[0] += 80;
                    coord->coord.t[1] -= 80;
                    break;
                case 8:
                    coord->coord.t[0] -= 80;
                    coord->coord.t[1] -= 80;
                    break;
                case 10:
                    coord->coord.t[0] += 120;
                    coord->coord.t[1] += 120;
                    break;
                case 11:
                    coord->coord.t[0] -= 120;
                    coord->coord.t[1] += 120;
                    break;
                case 12:
                    coord->coord.t[0] += 120;
                    coord->coord.t[1] -= 120;
                    break;
                case 13:
                    coord->coord.t[0] -= 120;
                    coord->coord.t[1] -= 120;
                    break;
            }
            obj->lightMtx = &work->lightMtx;
            obj->colorMtx = &work->colorMtx;

            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if ((gRandomLcgState >> 16) & 1) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                r               = ((gRandomLcgState >> 16) + 10) & 7;
            } else {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                r               = -10 - ((gRandomLcgState >> 16) & 7);
            }
            work->velX      = r;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->velY      = ((gRandomLcgState >> 16) & 3) + 3;

            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if ((gRandomLcgState >> 16) & 1) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                r               = ((gRandomLcgState >> 16) + 10) & 7;
            } else {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                r               = -10 - ((gRandomLcgState >> 16) & 7);
            }
            work->velZ = r;

            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if ((gRandomLcgState >> 16) & 1) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                r               = (gRandomLcgState >> 16) & 0x7F;
            } else {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                r               = -((gRandomLcgState >> 16) & 0x7F);
            }
            work->spinX     = r;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if ((gRandomLcgState >> 16) & 1) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                r               = (gRandomLcgState >> 16) & 0x7F;
            } else {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                r               = -((gRandomLcgState >> 16) & 0x7F);
            }
            work->spinY     = r;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if ((gRandomLcgState >> 16) & 1) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                r               = (gRandomLcgState >> 16) & 0x7F;
            } else {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                r               = -((gRandomLcgState >> 16) & 0x7F);
            }
            work->spinZ     = r;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->delay     = (gRandomLcgState >> 16) & 3;
            arg0->state++;
            break;
        case 1:
            if (work->delay == 0) {
                tmdAllocPrimitiveBuffer(obj);
                obj->flags = 0;
                arg0->state++;
            } else {
                work->delay--;
            }
            break;
        case 2:
            work->velY        += D_actor_121300_8013CC04 * 3 / 100;
            coord->coord.t[0] += work->velX * D_actor_121300_8013CC04 / 100;
            coord->coord.t[1] += work->velY * D_actor_121300_8013CC04 / 100;
            coord->coord.t[2] += work->velZ * D_actor_121300_8013CC04 / 100;
            work->rotX        += work->spinX * D_actor_121300_8013CC04 / 100;
            work->rotY        += work->spinY * D_actor_121300_8013CC04 / 100;
            work->rotZ        += work->spinZ * D_actor_121300_8013CC04 / 100;
            gfxRotMatrixY(&coord->coord, work->rotY, 1);
            gfxRotMatrixX(&coord->coord, work->rotX, GRAPHICS_ROTATION_COMPOSE);
            gfxRotMatrixZ(&coord->coord, work->rotZ, GRAPHICS_ROTATION_COMPOSE);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            if (coord->coord.t[1] >= -499) {
                taskKill(arg0);
            }
            break;
    }
    tail   = arg0->extra.tmd;
    pos.vx = tail->coords->workm.t[0];
    pos.vy = arg0->extra.tmd->coords->workm.t[1];
    pos.vz = arg0->extra.tmd->coords->workm.t[2];
    worldCoordSetModelLighting(tail, &pos, 0, 3);
}

/// Spawns the fifteen debris variants for one lamp, then releases this task.
void func_actor_121300_80133064(Task* task)
{
    void* alloc;

    if (D_actor_121300_8013D41C == 0) {
        taskKill(task);
        return;
    }
    switch (task->state) {
        case 0:
            alloc      = memMalloc(8, false);
            task->work = alloc;
            if (alloc != NULL) {
                memFillBytes(alloc, 0, 8);
                task->state += 1;
                return;
            }
            break;
        case 1:
            taskSpawnFromTable(D_actor_121300_8013D390, 4, task->spawnArg1, 0);
            taskSpawnFromTable(D_actor_121300_8013D390, 5, task->spawnArg1, 1);
            taskSpawnFromTable(D_actor_121300_8013D390, 6, task->spawnArg1, 2);
            taskSpawnFromTable(D_actor_121300_8013D390, 7, task->spawnArg1, 3);
            taskSpawnFromTable(D_actor_121300_8013D390, 8, task->spawnArg1, 4);
            taskSpawnFromTable(D_actor_121300_8013D390, 4, task->spawnArg1, 5);
            taskSpawnFromTable(D_actor_121300_8013D390, 5, task->spawnArg1, 6);
            taskSpawnFromTable(D_actor_121300_8013D390, 6, task->spawnArg1, 7);
            taskSpawnFromTable(D_actor_121300_8013D390, 7, task->spawnArg1, 8);
            taskSpawnFromTable(D_actor_121300_8013D390, 8, task->spawnArg1, 9);
            taskSpawnFromTable(D_actor_121300_8013D390, 4, task->spawnArg1, 0xA);
            taskSpawnFromTable(D_actor_121300_8013D390, 5, task->spawnArg1, 0xB);
            taskSpawnFromTable(D_actor_121300_8013D390, 6, task->spawnArg1, 0xC);
            taskSpawnFromTable(D_actor_121300_8013D390, 7, task->spawnArg1, 0xD);
            taskSpawnFromTable(D_actor_121300_8013D390, 8, task->spawnArg1, 0xE);
            break;
        default:
            return;
    }
    taskKill(task);
}

/// Uploads this actor's images using its parent's texture-page offset.
///
/// `_Actor121300AyaBreaWork::texturePageOffset` holds the signed offset, and `spawnArg2`
/// names the parent task. `spawnArg1` selects the images; the two-state variants
/// upload one block per frame before killing the task.
void func_actor_121300_8013322C(Task* task)
{
    RECT rect;
    s32  imageX;

    // Relocate each image by the placement's 64-word texture-page offset.
    switch (task->spawnArg1.value) {
        case 0:
            imageX   = ((_Actor121300AyaBreaWork*)((Task*)task->spawnArg2.pointer)->work)->texturePageOffset;
            imageX <<= 6;
            imageX  += 0x180;
            rect.x   = imageX;
            rect.y   = 0x140;
            rect.w   = 0x19;
            rect.h   = 0x14;
            LoadImage(&rect, D_actor_121300_8013BBE8);
            imageX   = ((_Actor121300AyaBreaWork*)((Task*)task->spawnArg2.pointer)->work)->texturePageOffset;
            imageX <<= 6;
            imageX  += 0x18C;
            rect.x   = imageX;
            rect.y   = 0x1A0;
            rect.w   = 0xE;
            rect.h   = 0x14;
            LoadImage(&rect, D_actor_121300_8013C7A0);
            taskKill(task);
            break;
        case 1:
            switch (task->state) {
                case 0:
                    imageX   = ((_Actor121300AyaBreaWork*)((Task*)task->spawnArg2.pointer)->work)->texturePageOffset;
                    imageX <<= 6;
                    imageX  += 0x180;
                    rect.x   = imageX;
                    rect.y   = 0x140;
                    rect.w   = 0x19;
                    rect.h   = 0x14;
                    LoadImage(&rect, D_actor_121300_8013BFD0);
                    task->state++;
                    break;
                case 1:
                    imageX   = ((_Actor121300AyaBreaWork*)((Task*)task->spawnArg2.pointer)->work)->texturePageOffset;
                    imageX <<= 6;
                    imageX  += 0x180;
                    rect.x   = imageX;
                    rect.y   = 0x140;
                    rect.w   = 0x19;
                    rect.h   = 0x14;
                    LoadImage(&rect, D_actor_121300_8013C3B8);
                    taskKill(task);
                    break;
            }
            break;
        case 2:
            switch (task->state) {
                case 0:
                    imageX   = ((_Actor121300AyaBreaWork*)((Task*)task->spawnArg2.pointer)->work)->texturePageOffset;
                    imageX <<= 6;
                    imageX  += 0x180;
                    rect.x   = imageX;
                    rect.y   = 0x140;
                    rect.w   = 0x19;
                    rect.h   = 0x14;
                    LoadImage(&rect, D_actor_121300_8013BFD0);
                    task->state++;
                    break;
                case 1:
                    imageX   = ((_Actor121300AyaBreaWork*)((Task*)task->spawnArg2.pointer)->work)->texturePageOffset;
                    imageX <<= 6;
                    imageX  += 0x180;
                    rect.x   = imageX;
                    rect.y   = 0x140;
                    rect.w   = 0x19;
                    rect.h   = 0x14;
                    LoadImage(&rect, D_actor_121300_8013BBE8);
                    taskKill(task);
                    break;
            }
            break;
        case 3:
            break;
        case 4:
        case 5:
            imageX   = ((_Actor121300AyaBreaWork*)((Task*)task->spawnArg2.pointer)->work)->texturePageOffset;
            imageX <<= 6;
            imageX  += 0x18C;
            rect.x   = imageX;
            rect.y   = 0x1A0;
            rect.w   = 0xE;
            rect.h   = 0x14;
            LoadImage(&rect, D_actor_121300_8013C9D0);
            taskKill(task);
            break;
    }
}

/// Effect spawner: on every fourth frame, walks one of the two arena-ring
/// position tables `D_actor_121300_8013CCB8` / `D_actor_121300_8013CD48`
/// (`arg1` non-zero picks the lowered one) and spawns effect 0x601B7 at each
/// entry, jittered along `vx` by up to +/-70 -- two LCG draws, the second only
/// when the first one's bit 16 is set, which is also the sign of the step.
/// The walk stops on the zeroed `SVECTOR` that ends both tables.
static void func_actor_121300_8013343C(Task* arg0, s16 arg1)
{
    SVECTOR  pos;
    SVECTOR* pts;
    s16      x;
    s32      flags;
    u32      seed;
    s32      vx;

    if (!(D_actor_121300_8013CC00 & 3)) {
        if (arg1 == 0) {
            pts = D_actor_121300_8013CCB8;
        } else {
            pts = D_actor_121300_8013CD48;
        }
        x = pts->vx;
        if (pts->vx != 0) {
            flags = 0x81202400;
            do {
                seed            = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState = seed;
                vx              = x + (((seed >> 16) & 1) ? ((gRandomLcgState = (seed * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT) >> 16) & 7
                                                          : -(((gRandomLcgState = (seed * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT) >> 16) & 7)) *
                             10;
                pos.vx = vx;
                pos.vy = pts->vy;
                pos.vz = pts->vz;
                effectSpawn(EFFECT_DRYFIELD_R08_ARENA_RING_SPRITE, NULL, flags, &pos);
                pts++;
                x = pts->vx;
            } while (pts->vx != 0);
        }
    }
}

/// Effect spawner: bumps the `_Actor121300AyaBreaWork::spritePointsDropped` falloff every 20
/// calls with `arg1` set, then on every fourth frame spawns effect 0x601B7 at
/// the first `6 - spritePointsDropped` entries of `D_actor_121300_8013CDC8`, jittered
/// along `vx` by up to +/-70 as in `func_actor_121300_8013343C`.
static void func_actor_121300_80133580(Task* arg0, s16 arg1)
{
    SVECTOR                  pos;
    _Actor121300AyaBreaWork* work;
    s16                      i;
    u32                      seed;
    s32                      flags;
    SVECTOR*                 tbl;
    s32                      vx;

    work = arg0->work;
    if (arg1 != 0) {
        if (++work->spriteDropFrames >= 20) {
            work->spriteDropFrames = 0;
            work->spritePointsDropped++;
        }
    }
    if (!(D_actor_121300_8013CC00 & 3)) {
        for (i = 0; i < 6 - work->spritePointsDropped; i++) {
            flags           = 0x81202400;
            tbl             = D_actor_121300_8013CDC8;
            seed            = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            gRandomLcgState = seed;
            vx              = tbl[i].vx + (((seed >> 16) & 1) ? ((gRandomLcgState = (seed * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT) >> 16) & 7
                                                              : -(((gRandomLcgState = (seed * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT) >> 16) & 7)) *
                                 10;
            pos.vx = vx;
            pos.vy = tbl[i].vy;
            pos.vz = tbl[i].vz;
            effectSpawn(EFFECT_DRYFIELD_R08_ARENA_RING_SPRITE, NULL, flags, &pos);
        }
    }
}

/// Lamp shattering: while the `_Actor121300AyaBreaWork::lampCursor` record of
/// `D_actor_121300_8013CC20` is a lamp, counts three ticks, then hides the
/// next lamp's sprites through `dryfieldR08SetLampSpritesHidden`, tells the room
/// through `dryfieldR08SetShatteredLampCount` how many lamps are out and spawns the
/// `D_actor_121300_8013D390[3]` child that scatters that lamp's debris.
static void func_actor_121300_80133730(Task* arg0)
{
    _Actor121300AyaBreaWork* work = arg0->work;

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
                    if (work->lampsShattered < 6) {
                        dryfieldR08SetLampSpritesHidden(work->lampsShattered, 1);
                    } else if (work->lampsShattered >= 7) {
                        dryfieldR08SetLampSpritesHidden(work->lampsShattered - 1, 1);
                    }
                    dryfieldR08SetShatteredLampCount(work->lampsShattered + 1);
                    taskSpawnFromTable(D_actor_121300_8013D390, 3, (s32)work->lampsShattered, 0);
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

static inline void func_actor_121300_PlayAll(Task* arg0, s32 anim)
{
    _Actor121300AyaBreaWork* work;
    u16                      i;

    work = arg0->work;
    SET_ANIM_ID(work, anim);
    for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
        animationSeekSlotWithBlend(&work->rig.anim, i, anim, 0, 10);
    }
}

static inline void func_actor_121300_SetCC04(s32 v)
{
    D_actor_121300_8013CC04 = v;
}

static void func_actor_121300_80133854(Task* arg0)
{
    _Actor121300AyaBreaWork* work;
    CdCmdQueue*              queue;

    work  = arg0->work;
    queue = &gCdCmdQueue;
    func_actor_121300_80132818(arg0);
    switch (work->step) {
        case ACTOR_121300_STEP_REPLACE_PLAYER:
            taskMessageDispatch(work->player, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, 2, 0);
            TASK_MESSAGE_DISPATCH_POINTER(arg0, 0x7D4, &D_actor_121300_8013CCA0, 0);
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
            func_actor_121300_PlayAll(arg0, 2);
            work->step = ACTOR_121300_STEP_NONE;
            break;
        case ACTOR_121300_STEP_SHATTER_LAMPS_WAVE:
            if (work->stepState == 0) {
                func_actor_121300_SetCC04(10);
                work->wave.span  = 0x3C;
                work->wave.scale = 0x100;
                work->waveTask   = taskSpawnFromTable(D_actor_121300_8013BBCC, 0, 0, &work->wave);
                work->stepState++;
            }
        case ACTOR_121300_STEP_SHATTER_LAMPS:
            func_actor_121300_80133730(arg0);
            break;
        case ACTOR_121300_STEP_SHATTER_LAMPS_SLOW:
            func_actor_121300_SetCC04(0x1E);
            func_actor_121300_80133730(arg0);
            break;
        case ACTOR_121300_STEP_END_SHATTER:
            work->wave.state        = SCREEN_WAVE_RAMP_FINISHED;
            queue->imageMdecMode    = MDEC_IMAGE_MODE_RGB16;
            D_actor_121300_8013D41C = 0;
            work->step              = 0;
            break;
        case ACTOR_121300_STEP_HOLD_ON_MARK:
            if (work->stepState == 0) {
                TASK_MESSAGE_DISPATCH_POINTER(arg0, 0x7D4, &D_actor_121300_8013CCA0, 0);
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
            func_actor_121300_8013343C(arg0, 0);
            func_actor_121300_80133580(arg0, 0);
            break;
        case ACTOR_121300_STEP_PLAY_SET_3:
            if (work->stepState == 0) {
                func_actor_121300_PlayAll(arg0, 3);
                work->stepState++;
            }
            func_actor_121300_8013343C(arg0, 0);
            break;
        case ACTOR_121300_STEP_SPRITES_DWINDLE:
            func_actor_121300_80133580(arg0, 1);
            func_actor_121300_8013343C(arg0, 0);
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
            func_actor_121300_8013343C(arg0, 1);
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
    place                   = Gp_GetNestedAreaRec(&gGameSession->location.loc)->placements;
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
/// zero.  State 1 hands the cutscene's two script blocks to `func_800E8634`,
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
            func_800E8634(D_actor_121300_8013CE08, 0, D_actor_121300_8013D2E8);
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

void func_actor_121300_8013400C(Task* arg0)
{
    ScreenFadeWork* fade;
    ScreenFadeWork* alloc;

    fade = arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = memMalloc(sizeof(*alloc), false);
            arg0->work = alloc;
            if (alloc == NULL) {
                taskKill(arg0);
                return;
            }
            fade         = alloc;
            fade->b      = 0;
            fade->g      = 0;
            fade->r      = 0;
            arg0->state += 1;
            /* fallthrough */
        case 1:
            fadeDrawOverlay(fade->r, fade->g, fade->r, GPU_BLEND_SUBTRACT);
            fade->r += (u16)arg0->spawnArg1.value;
            fade->g += (u16)arg0->spawnArg1.value;
            fade->b += (u16)arg0->spawnArg1.value;
            if (fade->r < 0x100) {
                return;
            }
            SetDispMask(0);
            taskKill(arg0);
            break;
    }
}

void func_actor_121300_801340F0(Task* task)
{
    fadeDrawOverlay(0xFF, 0xFF, 0xFF, GPU_BLEND_SUBTRACT);
}

#include "../../shared/actor_messages_place_ypr.inc.c"

#include "../../shared/actor_messages_draw_mode.inc.c"

s32 func_actor_121300_80134224(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    taskSpawnFromTable(D_actor_121300_8013D390, 0xA, arg2, arg0);
}

void func_actor_121300_80134250(s16 arg0)
{
    _Actor121300AyaBreaWork* work = D_actor_121300_8013D418->work;

    work->step      = arg0;
    work->stepState = 0;
}

void func_actor_121300_80134270(void)
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
    CdCmd_CancelReplaceAndActivate();
}

/// Spawns entry 1 of the overlay's spawn table with `arg0` as its argument.
void func_actor_121300_801342D4(s32 arg0)
{
    taskSpawnFromTable(D_actor_121300_8013D390, 1, arg0, 0);
}

/// Spawns entry 2 of the overlay's spawn table with `arg0` as its argument.
void func_actor_121300_80134304(s32 arg0)
{
    taskSpawnFromTable(D_actor_121300_8013D390, 2, arg0, 0);
}

void func_actor_121300_80134334(s32 arg0)
{
    taskMessageDispatch(D_actor_121300_8013D418, ACTOR_MESSAGE_SET_MODEL_DRAW, arg0, 0);
}

void func_actor_121300_80134364(void)
{
    cdCmdStageSceneAudioStart();
}

void func_actor_121300_80134384(void)
{
    cdCmdEnqueueScenePlayback();
}

void func_actor_121300_801343A4(void)
{
    Gp_RestoreStreamRng();
    CdCmd_CancelReplaceAndActivate();
}
