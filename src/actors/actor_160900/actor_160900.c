#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/rand.h>

#include "common.h"
#include "gte.h"

#include "actors/actor.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/effect_tasks.h"
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
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"
#include "../../shared/screen_wave.h"
#include "../../shared/actor_messages.h"

/// Work block of one light quad, a gradient quad the cutscene blends
/// additively over the scene.
///
/// The spawner allocates the block zeroed for the quad's task, keeps it at
/// `Task::work` and fills in the corners; the task's teardown frees it. The
/// quad's task projects the corners through its own coordinate each frame and
/// shades each corner black or white, in one of six patterns its first spawn
/// argument selects (0 corner 3 white, 1 corner 2, 2 corners 1 and 3,
/// 3 corners 0 and 2, 4 corners 2 and 3, 5 all four).
///
/// Every quad the package spawns is an upright rectangle in the Y/Z plane:
/// `vx` is zero in every corner, corners 0 and 1 share the upper edge and
/// corners 2 and 3 the lower, with the greater Z first on each. No corner's
/// `pad` is written or read.
typedef struct {
    SVECTOR corners[4]; // The quad's corners as offsets from its task's coordinate, in the order the primitive takes its vertices
} _Actor160900LightQuadWork;
STATIC_ASSERT_SIZEOF(_Actor160900LightQuadWork, 0x20);

/// Cues of `_Actor160900CutsceneWork::playerCue`. The clips are those of
/// `_gActor160900PlayerAnimationSets`.
enum {
    ACTOR_160900_PLAYER_CUE_TAKE_PLACE   = 1,  // places Aya for the scene and restarts her in clip 10
    ACTOR_160900_PLAYER_CUE_CLIP_1       = 2,  // restarts her in clip 1
    ACTOR_160900_PLAYER_CUE_WALK_TO_MARK = 3,  // once: clip 1 of her own weapon's bank, a second placement, and a walk to a fixed point; stays posted afterwards
    ACTOR_160900_PLAYER_CUE_WAVE_START   = 4,  // starts the screen wave on an eight-frame rise
    ACTOR_160900_PLAYER_CUE_WAVE_END     = 5,  // ends the screen wave's task
    ACTOR_160900_PLAYER_CUE_CLIP_2       = 6,  // restarts her in clip 2, which chains on through clips 3, 4 and 5
    ACTOR_160900_PLAYER_CUE_CLIP_6       = 7,  // blends her into clip 6
    ACTOR_160900_PLAYER_CUE_CLIP_7       = 8,  // blends her into clip 7, which chains back to clip 0
    ACTOR_160900_PLAYER_CUE_CLIP_8       = 9,  // blends her into clip 8, which chains back to clip 0
    ACTOR_160900_PLAYER_CUE_CLIP_9       = 10, // blends her into clip 9, which chains back to clip 0
};

/// Cues of `_Actor160900CutsceneWork::kyleCue`.
enum {
    ACTOR_160900_KYLE_CUE_HIDE_AND_PLACE = 1, // hides Kyle's body and places it
    ACTOR_160900_KYLE_CUE_APPEAR         = 2, // shows his hands, his gun if there is one, and his body, and spawns a ground decal at an offset from him
    ACTOR_160900_KYLE_CUE_CLIP_1         = 3, // blends his body into clip 1 of its own animation script
    ACTOR_160900_KYLE_CUE_GROUND_DECAL   = 4, // spawns a ground decal at a second offset from him
};

/// Work block of the task that runs the package's cutscene, allocated and
/// zeroed at its full size by that task's first state and kept at its
/// `Task::work`.
///
/// The scene plays Aya Brea and Kyle Madigan against one event script. The
/// block holds the cast - the player's own task and the model tasks this
/// package spawns for Kyle - and the cues through which the script tells Aya,
/// Kyle and the scene's effects what to do next. Kyle's body hangs below the
/// cutscene task in the teardown tree and his hands below his body, so they
/// die with the scene; the player's task is borrowed and outlives it.
///
/// The player keeps the animation player of an ordinary actor, so the scene
/// tracks which of its own clips Aya is in and chains the next one itself.
/// Kyle runs his animation script inside his own task.
typedef struct {
    ScreenWaveCtx    wave;           // Ramp of the screen-wave task the player cue starts and ends; the scene seeds the rise and strength and leaves the tint off
    Task*            lightQuads[10]; // Additive gradient quads the script puts up a set at a time, of which the largest fills six slots; NULL where a slot is empty, and killed and cleared together when the script takes the set down
    Task*            player;         // The player's task, playing Aya; borrowed, never killed here
    Task*            kyle;           // Kyle Madigan's body
    Task*            kyleGunHand;    // Kyle's hand model on the body part that would also carry the gun
    Task*            kyleFreeHand;   // Kyle's other hand model
    Task*            kyleGun;        // Slot for Kyle's handgun model, shown with his hands when set; nothing here spawns the gun, so it stays NULL
    byte             unknown_48[4];  // Never accessed; role unproven
    ActorCutsceneCue playerCue;      // Aya's cue (`ACTOR_160900_PLAYER_CUE_*`): her placement and clips, and the screen wave; only the walk to the mark reads `step`
    ActorCutsceneCue kyleCue;        // Kyle's cue (`ACTOR_160900_KYLE_CUE_*`): hiding, placing and showing him, his clip, and the ground decals; `step` is never read
    ActorCutsceneCue effectCue;      // Which of the five point lists the scene spawns its effect over, one frame in eight (0 none, 1-5 the list); stays posted until the script changes it, and `step` is never read
    u16              playerAnimId;   // Clip Aya is playing, an index into the package's player animation sets and into the script that names its successor
    s16              playerAnimHold; // Frames the current player clip has been held, for a script step that lasts a fixed time
} _Actor160900CutsceneWork;
STATIC_ASSERT_SIZEOF(_Actor160900CutsceneWork, 0x68);

/// Work block of one of Kyle Madigan's models: his body, and a hand or handgun
/// that rides on a part of the body.
///
/// Each of those tasks allocates the block zeroed at its full size and keeps
/// it at `Task::work`; the model object borrows `light` and `color` for as
/// long as the block lives. The body binds `rig` to its model and plays one
/// clip at a time over slots 1 to 19, following `animChain` from clip to clip,
/// and the cutscene's cue handler changes the clip from outside. An attachment
/// uses the two matrices and leaves the rest zero.
typedef struct {
    ActorAnimRig20      rig;       // Playback storage of the twenty-part body model; slots 1 to 19 are driven
    MATRIX              light;     // Light-direction matrix lent to the model object
    MATRIX              color;     // Light-colour matrix lent to the model object
    ActorAnimChainLink* animChain; // The body's chain of clips: one link per clip, indexed by `animId`
    u16                 animId;    // Clip the slots were last seeded with, an index into the body's animation sets
    s16                 animHold;  // Frames `animId` has been held, for a link that lasts a fixed time
} _Actor160900KyleModelWork;
STATIC_ASSERT_SIZEOF(_Actor160900KyleModelWork, 0x4BC);

/// The overlay's task table. Entry 0 is `_actor160900CutsceneTask`, which
/// spawns entries 3, 5 and 6 into `_Actor160900CutsceneWork`; entries 1 and 2 are
/// spawned by `_actor160900SpawnFadeIn` / `_actor160900SpawnFadeOut`.
extern TaskDesc D_actor_160900_8013FB50[];

/// Clip chain `_actor160900AdvancePlayerAnimChain` follows for the player.
extern ActorAnimChainLink D_actor_160900_8013F1CC[];

extern u8       D_actor_160900_8013F210[];
extern u8       D_actor_160900_8013F228[];
extern TaskDesc D_actor_160900_8013F17C[];

/// Point lists `_actor160900CutsceneTask` hands `_actor160900SpawnDriftingSprites`
/// for `_Actor160900CutsceneWork::effectCue` ids 1-5.
extern SVECTOR D_actor_160900_8013F258[];
extern SVECTOR D_actor_160900_8013F2E0[];
extern SVECTOR D_actor_160900_8013F3B0[];
extern SVECTOR D_actor_160900_8013F400[];
extern SVECTOR D_actor_160900_8013F458[];

/// Pair of blocks `_actor160900CutsceneTask` passes to `evsStartScriptWithSkip`.
extern EvsCommand D_actor_160900_8013F538[];
extern EvsCommand D_actor_160900_8013FAA8[];

/// Distortion amplitude of the screen wave, `frame * scale / span` of the
/// running context, recomputed every frame.
extern s32 gScreenWaveRamp;

/// The context the running wave task was spawned with, parked at spawn so
/// the tick reads the ramp through it.
extern ScreenWaveCtx* gScreenWaveCtx;

extern Task* D_actor_160900_8013FBB4;

/// Per-column and per-row phase records: each is seeded with a random offset
/// and speed at spawn and advanced by its speed every frame.
extern ScreenWaveOscillator gScreenWaveColumns[13];

extern ScreenWaveOscillator gScreenWaveRows[30];

/// Clip transitions are measured in normal-rate frames; cue zero means idle.
enum {
    ACTOR_160900_ANIM_BLEND_FRAMES = 10,
    ACTOR_160900_CUE_NONE          = 0,
};

static TmdSource _gActor160900KyleMadiganBody;
static TmdSource _gActor160900KyleMadiganGun;
static TmdSource _gActor160900KyleMadiganLeft;
static TmdSource _gActor160900KyleMadiganHandRight;
static void      _actor160900KyleAttachmentTask(Task* task);
static void      _actor160900KyleBodyTask(Task* task);
static void      _actor160900DrawLightQuadTask(Task* task);
static void      _actor160900SpawnOpeningLightPair(void);
static void      _actor160900SpawnKyleRevealLightQuads(void);
static void      _actor160900SpawnWalkLightPair(void);
static void      _actor160900CutsceneTask(Task* task);
static void      _actor160900FadeOutTask(Task* task);
static void      _actor160900FadeInTask(Task* task);
static void      _actor160900SpawnFadeIn(s32 intensityStep);
static void      _actor160900SpawnFadeOut(s32 intensityStep);
static void      _actor160900KillLightQuads(void);
static void      _actor160900PostPlayerCue(s16 cueId);
static void      _actor160900PostKyleCue(s16 cueId);
static void      _actor160900PostEffectCue(s16 cueId);
static void      _actor160900SkipCutscene(void);
static void      _actor160900StageSceneAudioStart(void);
static void      _actor160900EnqueueScenePlayback(void);
static void      _actor160900FinishScenePlayback(void);

static void _actor160900SetModelDraw(Task* task, s32 messageId, s32 mode, s32 unusedArg);

/// Entries spawned from this package's task table.
enum {
    ACTOR_160900_TASK_FADE_IN        = 1,
    ACTOR_160900_TASK_FADE_OUT       = 2,
    ACTOR_160900_TASK_KYLE_BODY      = 3,
    ACTOR_160900_TASK_KYLE_GUN_HAND  = 5,
    ACTOR_160900_TASK_KYLE_FREE_HAND = 6,
    ACTOR_160900_TASK_LIGHT_QUAD     = 7,
};

/// Attachment selectors and their body coordinates; both hands reuse Kyle's texture placement.
enum {
    ACTOR_160900_KYLE_FREE_HAND      = 0,
    ACTOR_160900_KYLE_GUN_HAND       = 1,
    ACTOR_160900_KYLE_GUN            = 2,
    ACTOR_160900_KYLE_FREE_HAND_PART = 12,
    ACTOR_160900_KYLE_GUN_HAND_PART  = 8,
    ACTOR_160900_KYLE_PLACEMENT_ID   = 0x65,
};

/// White-corner patterns passed to the light-quad task; other corners are black.
enum {
    ACTOR_160900_LIGHT_PATTERN_CORNER_3    = 0,
    ACTOR_160900_LIGHT_PATTERN_CORNER_2    = 1,
    ACTOR_160900_LIGHT_PATTERN_CORNERS_1_3 = 2,
    ACTOR_160900_LIGHT_PATTERN_CORNERS_0_2 = 3,
    ACTOR_160900_LIGHT_PATTERN_CORNERS_2_3 = 4,
    ACTOR_160900_LIGHT_PATTERN_ALL_CORNERS = 5,
};

static TmdBone _gActor160900KyleMadiganBodySkeleton[20] = {
#include "assets/kyle_madigan_body_skeleton.inc"
};

static u32 _gActor160900KyleMadiganBodyPartVerts[20] = {
#include "assets/kyle_madigan_body_partVerts.inc"
};

static SVECTOR _gActor160900KyleMadiganBodyVerts[300] = {
#include "assets/kyle_madigan_body_verts.inc"
};

static SVECTOR _gActor160900KyleMadiganBodyNormals[298] = {
#include "assets/kyle_madigan_body_normals.inc"
};

static u32 _gActor160900KyleMadiganBodyStream[3412] = {
#include "assets/kyle_madigan_body_stream.inc"
};

static TmdSource _gActor160900KyleMadiganBody = {
    0,
    18224,
    5696,
    20,
    _gActor160900KyleMadiganBodyPartVerts,
    _gActor160900KyleMadiganBodyVerts,
    _gActor160900KyleMadiganBodyNormals,
    _gActor160900KyleMadiganBodySkeleton,
    _gActor160900KyleMadiganBodyStream,
};

static TmdBone _gActor160900KyleMadiganGunSkeleton[1] = {
#include "assets/kyle_madigan_gun_skeleton.inc"
};

static u32 _gActor160900KyleMadiganGunPartVerts[1] = {
#include "assets/kyle_madigan_gun_partVerts.inc"
};

static SVECTOR _gActor160900KyleMadiganGunVerts[22] = {
#include "assets/kyle_madigan_gun_verts.inc"
};

static SVECTOR _gActor160900KyleMadiganGunNormals[24] = {
#include "assets/kyle_madigan_gun_normals.inc"
};

static u32 _gActor160900KyleMadiganGunStream[162] = {
#include "assets/kyle_madigan_gun_stream.inc"
};

static TmdSource _gActor160900KyleMadiganGun = {
    0,
    1108,
    0,
    1,
    _gActor160900KyleMadiganGunPartVerts,
    _gActor160900KyleMadiganGunVerts,
    _gActor160900KyleMadiganGunNormals,
    _gActor160900KyleMadiganGunSkeleton,
    _gActor160900KyleMadiganGunStream,
};

static TmdBone _gActor160900KyleMadiganLeftSkeleton[1] = {
#include "assets/kyle_madigan_left_skeleton.inc"
};

static u32 _gActor160900KyleMadiganLeftPartVerts[1] = {
#include "assets/kyle_madigan_left_partVerts.inc"
};

static SVECTOR _gActor160900KyleMadiganLeftVerts[23] = {
#include "assets/kyle_madigan_left_verts.inc"
};

static SVECTOR _gActor160900KyleMadiganLeftNormals[23] = {
#include "assets/kyle_madigan_left_normals.inc"
};

static u32 _gActor160900KyleMadiganLeftStream[166] = {
#include "assets/kyle_madigan_left_stream.inc"
};

static TmdSource _gActor160900KyleMadiganLeft = {
    0,
    1148,
    0,
    1,
    _gActor160900KyleMadiganLeftPartVerts,
    _gActor160900KyleMadiganLeftVerts,
    _gActor160900KyleMadiganLeftNormals,
    _gActor160900KyleMadiganLeftSkeleton,
    _gActor160900KyleMadiganLeftStream,
};

static TmdBone _gActor160900KyleMadiganHandRightSkeleton[1] = {
#include "assets/kyle_madigan_hand_right_skeleton.inc"
};

static u32 _gActor160900KyleMadiganHandRightPartVerts[1] = {
#include "assets/kyle_madigan_hand_right_partVerts.inc"
};

static SVECTOR _gActor160900KyleMadiganHandRightVerts[23] = {
#include "assets/kyle_madigan_hand_right_verts.inc"
};

static SVECTOR _gActor160900KyleMadiganHandRightNormals[23] = {
#include "assets/kyle_madigan_hand_right_normals.inc"
};

static u32 _gActor160900KyleMadiganHandRightStream[166] = {
#include "assets/kyle_madigan_hand_right_stream.inc"
};

static TmdSource _gActor160900KyleMadiganHandRight = {
    0,
    1148,
    0,
    1,
    _gActor160900KyleMadiganHandRightPartVerts,
    _gActor160900KyleMadiganHandRightVerts,
    _gActor160900KyleMadiganHandRightNormals,
    _gActor160900KyleMadiganHandRightSkeleton,
    _gActor160900KyleMadiganHandRightStream,
};

static AnimationPackedPose _gActor160900Animation09138Bank1[39] = {
#include "assets/actor_160900_animation_09138_bank1.inc"
};

static AnimationPackedRotation _gActor160900Animation09138Bank4[364] = {
#include "assets/actor_160900_animation_09138_bank4.inc"
};

static AnimationRecord _gActor160900Animation09138Records[441] = {
#include "assets/actor_160900_animation_09138_records.inc"
};

static u16 _gActor160900Animation09138Indices[20] = {
#include "assets/actor_160900_animation_09138_indices.inc"
};

static AnimationSet _gActor160900Animation09138 = {
    _gActor160900Animation09138Records,
    _gActor160900Animation09138Indices,
    { NULL, _gActor160900Animation09138Bank1, NULL, NULL, _gActor160900Animation09138Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160900Animation09D3CBank1[60] = {
#include "assets/actor_160900_animation_09D3C_bank1.inc"
};

static AnimationPackedRotation _gActor160900Animation09D3CBank4[241] = {
#include "assets/actor_160900_animation_09D3C_bank4.inc"
};

static AnimationRecord _gActor160900Animation09D3CRecords[328] = {
#include "assets/actor_160900_animation_09D3C_records.inc"
};

static u16 _gActor160900Animation09D3CIndices[20] = {
#include "assets/actor_160900_animation_09D3C_indices.inc"
};

static AnimationSet _gActor160900Animation09D3C = {
    _gActor160900Animation09D3CRecords,
    _gActor160900Animation09D3CIndices,
    { NULL, _gActor160900Animation09D3CBank1, NULL, NULL, _gActor160900Animation09D3CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160900Animation0A35CBank1[18] = {
#include "assets/actor_160900_animation_0A35C_bank1.inc"
};

static AnimationPackedRotation _gActor160900Animation0A35CBank4[86] = {
#include "assets/actor_160900_animation_0A35C_bank4.inc"
};

static AnimationRecord _gActor160900Animation0A35CRecords[232] = {
#include "assets/actor_160900_animation_0A35C_records.inc"
};

static u16 _gActor160900Animation0A35CIndices[20] = {
#include "assets/actor_160900_animation_0A35C_indices.inc"
};

static AnimationSet _gActor160900Animation0A35C = {
    _gActor160900Animation0A35CRecords,
    _gActor160900Animation0A35CIndices,
    { NULL, _gActor160900Animation0A35CBank1, NULL, NULL, _gActor160900Animation0A35CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160900Animation0A7F0Bank1[10] = {
#include "assets/actor_160900_animation_0A7F0_bank1.inc"
};

static AnimationPackedRotation _gActor160900Animation0A7F0Bank4[71] = {
#include "assets/actor_160900_animation_0A7F0_bank4.inc"
};

static AnimationRecord _gActor160900Animation0A7F0Records[172] = {
#include "assets/actor_160900_animation_0A7F0_records.inc"
};

static u16 _gActor160900Animation0A7F0Indices[20] = {
#include "assets/actor_160900_animation_0A7F0_indices.inc"
};

static AnimationSet _gActor160900Animation0A7F0 = {
    _gActor160900Animation0A7F0Records,
    _gActor160900Animation0A7F0Indices,
    { NULL, _gActor160900Animation0A7F0Bank1, NULL, NULL, _gActor160900Animation0A7F0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160900Animation0A9A8Bank1[3] = {
#include "assets/actor_160900_animation_0A9A8_bank1.inc"
};

static AnimationPackedRotation _gActor160900Animation0A9A8Bank4[24] = {
#include "assets/actor_160900_animation_0A9A8_bank4.inc"
};

static AnimationRecord _gActor160900Animation0A9A8Records[57] = {
#include "assets/actor_160900_animation_0A9A8_records.inc"
};

static u16 _gActor160900Animation0A9A8Indices[20] = {
#include "assets/actor_160900_animation_0A9A8_indices.inc"
};

static AnimationSet _gActor160900Animation0A9A8 = {
    _gActor160900Animation0A9A8Records,
    _gActor160900Animation0A9A8Indices,
    { NULL, _gActor160900Animation0A9A8Bank1, NULL, NULL, _gActor160900Animation0A9A8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160900Animation0AB80Bank1[2] = {
#include "assets/actor_160900_animation_0AB80_bank1.inc"
};

static AnimationPackedRotation _gActor160900Animation0AB80Bank4[16] = {
#include "assets/actor_160900_animation_0AB80_bank4.inc"
};

static AnimationRecord _gActor160900Animation0AB80Records[76] = {
#include "assets/actor_160900_animation_0AB80_records.inc"
};

static u16 _gActor160900Animation0AB80Indices[20] = {
#include "assets/actor_160900_animation_0AB80_indices.inc"
};

static AnimationSet _gActor160900Animation0AB80 = {
    _gActor160900Animation0AB80Records,
    _gActor160900Animation0AB80Indices,
    { NULL, _gActor160900Animation0AB80Bank1, NULL, NULL, _gActor160900Animation0AB80Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160900Animation0BA0CBank1[37] = {
#include "assets/actor_160900_animation_0BA0C_bank1.inc"
};

static AnimationPackedRotation _gActor160900Animation0BA0CBank4[358] = {
#include "assets/actor_160900_animation_0BA0C_bank4.inc"
};

static AnimationRecord _gActor160900Animation0BA0CRecords[442] = {
#include "assets/actor_160900_animation_0BA0C_records.inc"
};

static u16 _gActor160900Animation0BA0CIndices[20] = {
#include "assets/actor_160900_animation_0BA0C_indices.inc"
};

static AnimationSet _gActor160900Animation0BA0C = {
    _gActor160900Animation0BA0CRecords,
    _gActor160900Animation0BA0CIndices,
    { NULL, _gActor160900Animation0BA0CBank1, NULL, NULL, _gActor160900Animation0BA0CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160900Animation0BD10Bank1[6] = {
#include "assets/actor_160900_animation_0BD10_bank1.inc"
};

static AnimationPackedRotation _gActor160900Animation0BD10Bank4[46] = {
#include "assets/actor_160900_animation_0BD10_bank4.inc"
};

static AnimationRecord _gActor160900Animation0BD10Records[109] = {
#include "assets/actor_160900_animation_0BD10_records.inc"
};

static u16 _gActor160900Animation0BD10Indices[20] = {
#include "assets/actor_160900_animation_0BD10_indices.inc"
};

static AnimationSet _gActor160900Animation0BD10 = {
    _gActor160900Animation0BD10Records,
    _gActor160900Animation0BD10Indices,
    { NULL, _gActor160900Animation0BD10Bank1, NULL, NULL, _gActor160900Animation0BD10Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160900Animation0C028Bank1[7] = {
#include "assets/actor_160900_animation_0C028_bank1.inc"
};

static AnimationPackedRotation _gActor160900Animation0C028Bank4[54] = {
#include "assets/actor_160900_animation_0C028_bank4.inc"
};

static AnimationRecord _gActor160900Animation0C028Records[103] = {
#include "assets/actor_160900_animation_0C028_records.inc"
};

static u16 _gActor160900Animation0C028Indices[20] = {
#include "assets/actor_160900_animation_0C028_indices.inc"
};

static AnimationSet _gActor160900Animation0C028 = {
    _gActor160900Animation0C028Records,
    _gActor160900Animation0C028Indices,
    { NULL, _gActor160900Animation0C028Bank1, NULL, NULL, _gActor160900Animation0C028Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160900Animation0C774Bank1[6] = {
#include "assets/actor_160900_animation_0C774_bank1.inc"
};

static AnimationPackedRotation _gActor160900Animation0C774Bank4[187] = {
#include "assets/actor_160900_animation_0C774_bank4.inc"
};

static AnimationRecord _gActor160900Animation0C774Records[242] = {
#include "assets/actor_160900_animation_0C774_records.inc"
};

static u16 _gActor160900Animation0C774Indices[20] = {
#include "assets/actor_160900_animation_0C774_indices.inc"
};

static AnimationSet _gActor160900Animation0C774 = {
    _gActor160900Animation0C774Records,
    _gActor160900Animation0C774Indices,
    { NULL, _gActor160900Animation0C774Bank1, NULL, NULL, _gActor160900Animation0C774Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160900Animation0CA18Bank1[2] = {
#include "assets/actor_160900_animation_0CA18_bank1.inc"
};

static AnimationPackedRotation _gActor160900Animation0CA18Bank4[49] = {
#include "assets/actor_160900_animation_0CA18_bank4.inc"
};

static AnimationRecord _gActor160900Animation0CA18Records[94] = {
#include "assets/actor_160900_animation_0CA18_records.inc"
};

static u16 _gActor160900Animation0CA18Indices[20] = {
#include "assets/actor_160900_animation_0CA18_indices.inc"
};

static AnimationSet _gActor160900Animation0CA18 = {
    _gActor160900Animation0CA18Records,
    _gActor160900Animation0CA18Indices,
    { NULL, _gActor160900Animation0CA18Bank1, NULL, NULL, _gActor160900Animation0CA18Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160900Animation0CDF4Bank1[4] = {
#include "assets/actor_160900_animation_0CDF4_bank1.inc"
};

static AnimationPackedRotation _gActor160900Animation0CDF4Bank4[56] = {
#include "assets/actor_160900_animation_0CDF4_bank4.inc"
};

static AnimationRecord _gActor160900Animation0CDF4Records[159] = {
#include "assets/actor_160900_animation_0CDF4_records.inc"
};

static u16 _gActor160900Animation0CDF4Indices[20] = {
#include "assets/actor_160900_animation_0CDF4_indices.inc"
};

static AnimationSet _gActor160900Animation0CDF4 = {
    _gActor160900Animation0CDF4Records,
    _gActor160900Animation0CDF4Indices,
    { NULL, _gActor160900Animation0CDF4Bank1, NULL, NULL, _gActor160900Animation0CDF4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160900Animation0D334Bank1[9] = {
#include "assets/actor_160900_animation_0D334_bank1.inc"
};

static AnimationPackedRotation _gActor160900Animation0D334Bank4[124] = {
#include "assets/actor_160900_animation_0D334_bank4.inc"
};

static AnimationRecord _gActor160900Animation0D334Records[165] = {
#include "assets/actor_160900_animation_0D334_records.inc"
};

static u16 _gActor160900Animation0D334Indices[20] = {
#include "assets/actor_160900_animation_0D334_indices.inc"
};

static AnimationSet _gActor160900Animation0D334 = {
    _gActor160900Animation0D334Records,
    _gActor160900Animation0D334Indices,
    { NULL, _gActor160900Animation0D334Bank1, NULL, NULL, _gActor160900Animation0D334Bank4, NULL, NULL, NULL },
};

TaskDesc D_actor_160900_8013F17C[2] = {
    { { { TASK_BODY_NONE, 192 } }, _screenWaveTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

s32 gScreenWaveRamp = 256;

/// Borrowed player clips for the overlay's scripted animation requests.
static AnimationSet* _gActor160900PlayerAnimationSets[11] = {
    &_gActor160900Animation0BD10,
    &_gActor160900Animation09138,
    &_gActor160900Animation09D3C,
    &_gActor160900Animation0A35C,
    &_gActor160900Animation0A7F0,
    &_gActor160900Animation0A9A8,
    &_gActor160900Animation0BA0C,
    &_gActor160900Animation0CA18,
    &_gActor160900Animation0C774,
    &_gActor160900Animation0C028,
    &_gActor160900Animation0AB80,
};

AnimationSet* D_actor_160900_8013F1C4[2] = {
    &_gActor160900Animation0CDF4,
    &_gActor160900Animation0D334,
};

ActorAnimChainLink D_actor_160900_8013F1CC[11] = {
    { 0, -1 },
    { 0, -1 },
    { 0, 3 },
    { 0, 4 },
    { 0, 5 },
    { 0, -1 },
    { 0, -1 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, -1 },
};

ActorAnimChainLink D_actor_160900_8013F1F8[2] = {
    { 0, -1 },
    { 0, -1 },
};

// Message-table callbacks use the argument views required by this TU.

TaskMessageEntry D_actor_160900_8013F200[2] = {
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor160900SetModelDraw },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceYawPitchRoll },
};

u8 D_actor_160900_8013F210[24] = {
    148,
    17,
    0,
    0,
    0,
    0,
    0,
    0,
    128,
    12,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    4,
    0,
    0,
    0,
    0,
};

u8 D_actor_160900_8013F228[24] = {
    148,
    17,
    0,
    0,
    0,
    0,
    0,
    0,
    128,
    12,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    12,
    0,
    0,
    0,
    0,
};

u8 D_actor_160900_8013F240[24] = {
    16,
    39,
    0,
    0,
    0,
    0,
    0,
    0,
    128,
    12,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    252,
    0,
    0,
    0,
    0,
};

SVECTOR D_actor_160900_8013F258[17] = {
    { 6000, 1000, 2000, 0 },
    { 6000, 500, 2000, 0 },
    { 6000, 0, 2000, 0 },
    { 6000, 500, 2500, 0 },
    { 6000, 0, 2500, 0 },
    { 6000, -500, 2500, 0 },
    { 6000, 1000, 3000, 0 },
    { 6000, 500, 3000, 0 },
    { 6000, 0, 3000, 0 },
    { 6000, -500, 3000, 0 },
    { 6000, 500, 3500, 0 },
    { 6000, 0, 3500, 0 },
    { 6000, -500, 3500, 0 },
    { 6000, 1000, 4000, 0 },
    { 6000, 500, 4000, 0 },
    { 6000, 0, 4000, 0 },
    { 0, 0, 0, -1 },
};

SVECTOR D_actor_160900_8013F2E0[26] = {
    { 6000, 500, 2000, 0 },
    { 6000, 0, 2000, 0 },
    { 6000, -500, 2000, 0 },
    { 6000, -1000, 2000, 0 },
    { 6000, -1500, 2000, 0 },
    { 6000, 500, 2500, 0 },
    { 6000, 0, 2500, 0 },
    { 6000, -500, 2500, 0 },
    { 6000, -1000, 2500, 0 },
    { 6000, -1500, 2500, 0 },
    { 6000, 500, 3000, 0 },
    { 6000, 0, 3000, 0 },
    { 6000, -500, 3000, 0 },
    { 6000, -1000, 3000, 0 },
    { 6000, -1500, 3000, 0 },
    { 6000, 500, 3500, 0 },
    { 6000, 0, 3500, 0 },
    { 6000, -500, 3500, 0 },
    { 6000, -1000, 3500, 0 },
    { 6000, -1500, 3500, 0 },
    { 6000, 500, 4000, 0 },
    { 6000, 0, 4000, 0 },
    { 6000, -500, 4000, 0 },
    { 6000, -1000, 4000, 0 },
    { 6000, -1500, 4000, 0 },
    { 0, 0, 0, -1 },
};

SVECTOR D_actor_160900_8013F3B0[10] = {
    { 6000, -1000, 2500, 0 },
    { 6000, -500, 2500, 0 },
    { 6000, 0, 2500, 0 },
    { 6000, -1000, 3000, 0 },
    { 6000, -500, 3000, 0 },
    { 6000, 0, 3000, 0 },
    { 6000, -1000, 3500, 0 },
    { 6000, -500, 3500, 0 },
    { 6000, 0, 3500, 0 },
    { 0, 0, 0, -1 },
};

SVECTOR D_actor_160900_8013F400[11] = {
    { 8000, 0, 3000, 0 },
    { 8000, -500, 3000, 0 },
    { 8000, 0, 2900, 0 },
    { 8000, -500, 2900, 0 },
    { 8000, 0, 2850, 0 },
    { 8000, -500, 2850, 0 },
    { 8000, 0, 2800, 0 },
    { 8000, -500, 2800, 0 },
    { 8000, 0, 2750, 0 },
    { 8000, -500, 2750, 0 },
    { 0, 0, 0, -1 },
};

SVECTOR D_actor_160900_8013F458[27] = {
    { 7000, 3000, 3500, 0 },
    { 7000, 3000, 3000, 0 },
    { 7000, 3000, 2500, 0 },
    { 7000, 2000, 3500, 0 },
    { 7000, 2000, 3000, 0 },
    { 7000, 2000, 2500, 0 },
    { 7000, 1000, 3500, 0 },
    { 7000, 1000, 3000, 0 },
    { 7000, 1000, 2500, 0 },
    { 7000, 0, 4000, 0 },
    { 7000, 0, 3500, 0 },
    { 7000, 0, 3000, 0 },
    { 7000, 0, 2500, 0 },
    { 7000, 0, 2000, 0 },
    { 7000, -1500, 4000, 0 },
    { 7000, -1500, 3500, 0 },
    { 7000, -1500, 3000, 0 },
    { 7000, -1500, 2500, 0 },
    { 7000, -1500, 2000, 0 },
    { 7000, -3000, 4500, 0 },
    { 7000, -3000, 4000, 0 },
    { 7000, -3000, 3500, 0 },
    { 7000, -3000, 3000, 0 },
    { 7000, -3000, 2500, 0 },
    { 7000, -3000, 2000, 0 },
    { 7000, -3000, 1500, 0 },
    { 0, 0, 0, -1 },
};

EvsSceneKey D_actor_160900_8013F530 = { 6, 9, 11 };

EvsCommand D_actor_160900_8013F538[58] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = SetDispMask }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor160900PostPlayerCue }, { .value = ACTOR_160900_PLAYER_CUE_TAKE_PLACE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor160900PostKyleCue }, { .value = ACTOR_160900_KYLE_CUE_HIDE_AND_PLACE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor160900PostEffectCue }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_actor_160900_8013F530 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor160900StageSceneAudioStart }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor160900SpawnOpeningLightPair }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor160900EnqueueScenePlayback }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor160900SpawnFadeIn }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor160900PostPlayerCue }, { .value = ACTOR_160900_PLAYER_CUE_CLIP_1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor160900PostEffectCue }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor160900KillLightQuads }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor160900SpawnKyleRevealLightQuads }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor160900PostKyleCue }, { .value = ACTOR_160900_KYLE_CUE_APPEAR }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor160900PostPlayerCue }, { .value = ACTOR_160900_PLAYER_CUE_CLIP_2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor160900PostPlayerCue }, { .value = ACTOR_160900_PLAYER_CUE_CLIP_6 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor160900PostPlayerCue }, { .value = ACTOR_160900_PLAYER_CUE_CLIP_9 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor160900PostPlayerCue }, { .value = ACTOR_160900_PLAYER_CUE_CLIP_7 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor160900PostPlayerCue }, { .value = ACTOR_160900_PLAYER_CUE_CLIP_8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor160900PostEffectCue }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor160900KillLightQuads }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor160900SpawnWalkLightPair }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor160900PostPlayerCue }, { .value = ACTOR_160900_PLAYER_CUE_WALK_TO_MARK }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor160900PostPlayerCue }, { .value = ACTOR_160900_PLAYER_CUE_WAVE_START }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor160900PostEffectCue }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor160900KillLightQuads }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor160900PostPlayerCue }, { .value = ACTOR_160900_PLAYER_CUE_WAVE_END }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor160900PostKyleCue }, { .value = ACTOR_160900_KYLE_CUE_CLIP_1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor160900PostEffectCue }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = roomEffectRequestCancelAll }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor160900SpawnFadeOut }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor160900PostKyleCue }, { .value = ACTOR_160900_KYLE_CUE_GROUND_DECAL }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor160900FinishScenePlayback }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_160900_8013FAA8[7] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor160900SkipCutscene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 43 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc D_actor_160900_8013FB50[8] = {
    { { { TASK_BODY_NONE, 192 } }, _actor160900CutsceneTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _actor160900FadeInTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _actor160900FadeOutTask, { .value = 0 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, _actor160900KyleBodyTask, { .model = &_gActor160900KyleMadiganBody } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, _actor160900KyleAttachmentTask, { .model = &_gActor160900KyleMadiganGun } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, _actor160900KyleAttachmentTask, { .model = &_gActor160900KyleMadiganLeft } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, _actor160900KyleAttachmentTask, { .model = &_gActor160900KyleMadiganHandRight } },
    { { { TASK_BODY_COORD, 192 } }, _actor160900DrawLightQuadTask, { .value = 0 } },
};

ScreenWaveCtx* gScreenWaveCtx;

Task* D_actor_160900_8013FBB4;

ScreenWaveOscillator gScreenWaveColumns[13];

ScreenWaveOscillator gScreenWaveRows[30];

extern u8 D_actor_160900_8013F240[];

/// Animation-set table `animationInitContext` binds to the child's context, the table
/// published as `_Actor160900KyleModelWork::animChain`, and the message table
/// published as `Task::msgTable`.
extern AnimationSet* D_actor_160900_8013F1C4[];

extern ActorAnimChainLink D_actor_160900_8013F1F8[];

extern TaskMessageEntry D_actor_160900_8013F200[2];

static s32         _actor160900AdvancePlayerAnimChain(Task* task);
static inline void _actor160900BlendKyleAnim(Task* task, u16 animationId);
static s32         _actor160900AdvanceKyleAnimChain(Task* task);
static inline void _actor160900InitKyleAnimation(Task* task, TmdObject* bodyModel);
static inline void _actor160900BlendPlayerAnim(Task* task, u16 animationId);
static inline void _actor160900ResetPlayerAnim(Task* task, u16 animationId);
static void        _actor160900UpdatePlayerCue(Task* task);
static void        _actor160900UpdateKyleCue(Task* task);
static void        _actor160900SpawnDriftingSprites(const SVECTOR* points);

#include "../../shared/screen_wave.inc.c"

/// Advances the cutscene player's clip chain by one update.
///
/// Requires a live cutscene work block and a valid player clip index (0..10).
/// A nonzero hold duration counts updates; zero waits for playback to finish.
/// Successors start with a ten-frame blend and world collision enabled. The
/// borrowed player task and animation data must remain live during dispatch.
/// Returns 1 when the player is absent or a reached link has no successor,
/// otherwise 0; this does not clear a cue or stop the last clip.
static s32 _actor160900AdvancePlayerAnimChain(Task* task)
{
    _Actor160900CutsceneWork* work;
    ActorAnimChainLink*       chain;
    ActorAnimChainLink*       link;
    ActorAnimChainLink*       finishedLink;
    AnimationPlayRequest      request;
    u16                       nextAnimationId;
    u16                       finishedAnimationId;

    /// Starts a successor clip and resets its hold counter after dispatch.
    ///
    /// work is a live cutscene-work pointer, clipId a valid u16 set index,
    /// and request a writable AnimationPlayRequest lvalue. Arguments must be
    /// stable locals without side effects: each is evaluated repeatedly.
    /// Borrows the package's player animation table and retains no payload;
    /// dispatch is synchronous and its result is ignored.
#define ACTOR_160900_START_PLAYER_CHAIN_CLIP(work, clipId, request)                                       \
    do {                                                                                                  \
        (request).source.sets          = _gActor160900PlayerAnimationSets;                                \
        (work)->playerAnimId           = (clipId);                                                        \
        (request).animationId          = (clipId);                                                        \
        (request).blend                = ANIMATION_BLEND_INTERPOLATE;                                     \
        (request).blendFrames          = ACTOR_160900_ANIM_BLEND_FRAMES;                                  \
        (request).enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;                                \
        TASK_MESSAGE_DISPATCH_POINTER((work)->player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &(request), 0); \
        (work)->playerAnimHold = 0;                                                                       \
    } while (0)

    work = task->work;
    if (work->player == NULL) {
        return 1;
    }
    chain = D_actor_160900_8013F1CC;
    link  = &chain[work->playerAnimId];
    if (link->holdFrames != 0) {
        if (work->playerAnimHold >= link->holdFrames) {
            if (link->nextAnimId < 0) {
                return 1;
            }
            nextAnimationId = link->nextAnimId;
            ACTOR_160900_START_PLAYER_CHAIN_CLIP(work, nextAnimationId, request);
        } else {
            work->playerAnimHold += 1;
        }
    } else {
        if (taskMessageDispatch(work->player, ANIMATION_MESSAGE_IS_PLAYING, 0, 0) != 0) {
            return 0;
        }
        finishedLink = &D_actor_160900_8013F1CC[work->playerAnimId];
        if (finishedLink->nextAnimId < 0) {
            return 1;
        }
        work = task->work;
        if (work->player != NULL) {
            finishedAnimationId = finishedLink->nextAnimId;
            ACTOR_160900_START_PLAYER_CHAIN_CLIP(work, finishedAnimationId, request);
        }
    }
#undef ACTOR_160900_START_PLAYER_CHAIN_CLIP
    return 0;
}
/// Clears Kyle's current clip hold counter in a live model work block.
static inline void _actor160900ResetKyleAnimHold(_Actor160900KyleModelWork* work)
{
    // Fitted branch: identical arms preserve the blend loop's instruction order.
    // The original condition is unknown; work must be non-NULL.
    if (work != NULL) {
        work->animHold = 0;
    } else {
        work->animHold = 0;
    }
}

/// Restarts Kyle's clip chain on a selected clip with a ten-frame pose blend.
///
/// Requires the body's live, bound twenty-part rig and an animation id in 0..1.
/// Resets the hold counter and seeks slots 1..19 to record zero; slot 0 and
/// the slots' playback rates are retained. The model and clip data are borrowed.
static inline void _actor160900BlendKyleAnim(Task* task, u16 animationId)
{
    _Actor160900KyleModelWork* work;
    u16                        slotIndex;

    work         = task->work;
    work->animId = animationId;
    _actor160900ResetKyleAnimHold(work);
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        animationSeekSlotWithBlend(&work->rig.anim, slotIndex, animationId, 0, ACTOR_160900_ANIM_BLEND_FRAMES);
    }
}

/// Ticks Kyle's visible body and advances its two-clip animation chain.
///
/// Requires live model work, a bound twenty-part rig and clip id 0..1. A hidden
/// body leaves slots and hold time unchanged and returns 0. Slots 1..19 tick;
/// a zero-duration link advances only when all are settled, otherwise its hold
/// counter counts callback updates. Successors use a ten-frame blend. Returns
/// 1 when a reached link has no successor, otherwise 0; the last clip remains
/// selected. The work, model, chain and clip resources stay borrowed throughout.
static s32 _actor160900AdvanceKyleAnimChain(Task* task)
{
    _Actor160900KyleModelWork* work;
    ActorAnimChainLink*        chain;
    u16                        slotIndex;
    u16                        allSettled;

    work = task->work;
    if (task->extra.tmd->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) {
        return 0;
    }
    // Tick every driven part before testing whether the whole clip has settled.
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        animationTickSlot(&work->rig.anim, slotIndex);
    }
    slotIndex  = 1;
    allSettled = 1;
    for (; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        if (!(work->rig.slots[slotIndex].status.fields.flags & ANIMATION_SLOT_SETTLED)) {
            allSettled = 0;
            break;
        }
    }
    chain = work->animChain;
    if (chain[work->animId].holdFrames != 0) {
        if (work->animHold >= chain[work->animId].holdFrames) {
            if (chain[work->animId].nextAnimId >= 0) {
                _actor160900BlendKyleAnim(task, chain[work->animId].nextAnimId);
            } else {
                return 1;
            }
        } else {
            work->animHold++;
        }
    } else if (allSettled) {
        if (chain[work->animId].nextAnimId >= 0) {
            _actor160900BlendKyleAnim(task, chain[work->animId].nextAnimId);
        } else {
            return 1;
        }
    }
    return 0;
}

/// Attaches one of Kyle's hand or handgun models and updates its room lighting.
///
/// Start at state zero with a live TMD body and Kyle's body task in spawnArg2.
/// spawnArg1 selects the free hand (0, part 12), gun hand (1, part 8), or gun
/// (2, part 8). The body must provide those coordinates and outlive this task.
/// Hands use placement 0x65's texture offsets; the gun uses zero offsets.
/// Allocates owned primary-heap work for the model's lighting matrices and joins
/// the body's teardown tree. Allocation failure kills the task. Initialization
/// returns without sampling lighting; later updates use the composed root's XYZ.
static void _actor160900KyleAttachmentTask(Task* task)
{
    enum { ACTOR_160900_ATTACHMENT_INITIALIZE = 0 };
    VECTOR worldPosition;

    if (task->state == ACTOR_160900_ATTACHMENT_INITIALIZE) {
        TmdObject*                 model     = task->extra.tmd;
        Task*                      bodyTask  = task->spawnArg2.pointer;
        GfxCoord*                  rootCoord = model->coords;
        _Actor160900KyleModelWork* work;
        _Actor160900KyleModelWork* allocatedWork;
        AreaPlacement*             placement;
        u8                         entryId;

        allocatedWork = memMalloc(sizeof(*allocatedWork), false);
        task->work    = allocatedWork;
        if (allocatedWork == NULL) {
            taskKill(task);
            return;
        }
        work = allocatedWork;
        // Coordinate parenting follows the hand; task parenting owns teardown.
        switch (task->spawnArg1.value) {
            case ACTOR_160900_KYLE_FREE_HAND:
                rootCoord->parent = &bodyTask->extra.tmd->coords[ACTOR_160900_KYLE_FREE_HAND_PART];
                break;
            case ACTOR_160900_KYLE_GUN_HAND:
            case ACTOR_160900_KYLE_GUN:
                rootCoord->parent = &bodyTask->extra.tmd->coords[ACTOR_160900_KYLE_GUN_HAND_PART];
                break;
        }
        memFillBytes(task->work, 0, sizeof(*work));
        model->lightMtx = &work->light;
        model->colorMtx = &work->color;
        if (task->spawnArg1.value < ACTOR_160900_KYLE_GUN) {
            placement = areaGetVariant(&gGameSession->location.loc)->placements;
            entryId   = placement->entryId;
            while (entryId != AREA_PLACEMENT_END) {
                if (entryId == ACTOR_160900_KYLE_PLACEMENT_ID) {
                    break;
                }
                placement++;
                entryId = placement->entryId;
            }
            tmdSetTextureOffsets(task->extra.tmd, placement->texturePageOffset, placement->clutRowOffset);
        } else if (task->spawnArg1.value == ACTOR_160900_KYLE_GUN) {
            tmdSetTextureOffsets(task->extra.tmd, 0, 0);
        }
        taskReparent(bodyTask, task);
        task->msgTable = D_actor_160900_8013F200;
        task->state   += 1;
        return;
    } else {
        TmdObject* model = task->extra.tmd;

        worldPosition.vx = model->coords->workm.t[0];
        worldPosition.vy = task->extra.tmd->coords->workm.t[1];
        worldPosition.vz = task->extra.tmd->coords->workm.t[2];
        worldCoordSetModelLighting(model, &worldPosition, 0, 3);
    }
}

/// Restarts Kyle's body at clip zero and normal playback rate.
///
/// Requires live model work with a bound twenty-part rig. Clears the clip id
/// and hold counter and resets slots 1..19; slot 0 remains untouched.
static inline void _actor160900ResetKyleAnimSlots(Task* task)
{
    _Actor160900KyleModelWork* work;
    u16                        slotIndex;

    work           = task->work;
    work->animId   = 0;
    work->animHold = 0;
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        work->rig.slots[slotIndex].rate = ANIMATION_RATE_ONE;
        animationResetSlot(&work->rig.anim, slotIndex, 0);
    }
}

/// Binds Kyle's body rig and starts clip zero at normal playback rate.
///
/// Requires live model work and the twenty-part body model. The context borrows
/// the work's pose/slot arrays, model coordinates and two loaded animation sets
/// through playback; its clip chain has two entries. Resets slots 1..19 and the
/// hold counter, retaining the placed root in slot 0. Does not allocate storage.
static inline void _actor160900InitKyleAnimation(Task* task, TmdObject* bodyModel)
{
    _Actor160900KyleModelWork* work;

    work = task->work;
    animationInitContext(&work->rig.anim, D_actor_160900_8013F1C4, bodyModel, work->rig.poses, work->rig.slots);
    work->animChain = D_actor_160900_8013F1F8;
    _actor160900ResetKyleAnimSlots(task);
}

/// Initializes Kyle's hidden cutscene body, then advances its clip and room lighting.
///
/// Start at state zero with the twenty-part TMD body and a live published
/// cutscene task. Owns primary-heap rig and lighting work, released by teardown;
/// allocation failure kills the task. Placement 0x65 supplies texture offsets.
/// The body joins the cutscene's teardown tree and binds two animation sets.
/// Every update applies the view-dependent part-18 rotation (4096 units/turn)
/// and samples lighting at the already-composed root; hidden clips do not tick.
static void _actor160900KyleBodyTask(Task* task)
{
    enum {
        ACTOR_160900_BODY_INITIALIZE  = 0,
        ACTOR_160900_KYLE_POSE_PART   = 18,
        ACTOR_160900_KYLE_ROLL_VIEW   = 0x2E,
        ACTOR_160900_KYLE_PITCH_ANGLE = 0x79C,
    };
    TmdObject*                 model;
    TmdObject*                 litModel;
    GfxCoord*                  rootCoord;
    _Actor160900KyleModelWork* work;
    AreaPlacement*             placement;
    VECTOR                     worldPosition;
    u16                        allocationFailed;

    if (task->state == ACTOR_160900_BODY_INITIALIZE) {
        model      = task->extra.tmd;
        rootCoord  = model->coords;
        work       = memMalloc(sizeof(*work), false);
        task->work = work;
        if (work == NULL) {
            allocationFailed = true;
        } else {
            rootCoord->parent = &gGfxViewCoord;
            memFillBytes(task->work, 0, sizeof(*work));
            model->lightMtx = &work->light;
            model->colorMtx = &work->color;
            model->flags   |= (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            task->msgTable  = D_actor_160900_8013F200;
            placement       = areaGetVariant(&gGameSession->location.loc)->placements;
            while (placement->entryId != AREA_PLACEMENT_END && placement->entryId != ACTOR_160900_KYLE_PLACEMENT_ID) {
                placement++;
            }
            tmdSetTextureOffsets(task->extra.tmd, placement->texturePageOffset, placement->clutRowOffset);
            taskReparent(D_actor_160900_8013FBB4, task);
            allocationFailed = false;
        }
        if (allocationFailed) {
            taskKill(task);
            return;
        }
        _actor160900InitKyleAnimation(task, task->extra.tmd);
        task->state++;
    }
    _actor160900AdvanceKyleAnimChain(task);
    // Override this part's animated rotation for the current camera pose.
    if (gGameSession->location.loc.view == ACTOR_160900_KYLE_ROLL_VIEW) {
        gfxRotMatrixZ(&task->extra.tmd->coords[ACTOR_160900_KYLE_POSE_PART].coord, ACTOR_TRANSFORM_ANGLE_HALF_TURN, GRAPHICS_ROTATION_REPLACE);
    } else {
        gfxRotMatrixX(&task->extra.tmd->coords[ACTOR_160900_KYLE_POSE_PART].coord, ACTOR_160900_KYLE_PITCH_ANGLE, GRAPHICS_ROTATION_REPLACE);
    }
    litModel         = task->extra.tmd;
    worldPosition.vx = litModel->coords->workm.t[0];
    worldPosition.vy = task->extra.tmd->coords->workm.t[1];
    worldPosition.vz = task->extra.tmd->coords->workm.t[2];
    worldCoordSetModelLighting(litModel, &worldPosition, 0, 3);
}

/// Shades a light quad from a live task's six-valued spawn pattern.
///
/// quad must be writable; task's first spawn argument is 0..5. Writes only RGB
/// bytes, retaining packet metadata and coordinates. Neither pointer is retained.
static inline void _actor160900ShadeLightQuad(POLY_G4* quad, const Task* task)
{
    enum {
        ACTOR_160900_LIGHT_WHITE_CORNER_3    = 0,
        ACTOR_160900_LIGHT_WHITE_CORNER_2    = 1,
        ACTOR_160900_LIGHT_WHITE_CORNERS_1_3 = 2,
        ACTOR_160900_LIGHT_WHITE_CORNERS_0_2 = 3,
        ACTOR_160900_LIGHT_WHITE_CORNERS_2_3 = 4,
        ACTOR_160900_LIGHT_WHITE_ALL_CORNERS = 5,
        ACTOR_160900_LIGHT_MAX_INTENSITY     = 255,
    };
    quad->r0 = 0;
    quad->g0 = 0;
    quad->b0 = 0;
    quad->r1 = 0;
    quad->g1 = 0;
    quad->b1 = 0;
    switch (task->spawnArg1.value) {
        case ACTOR_160900_LIGHT_WHITE_CORNER_3:
            quad->r0 = 0;
            quad->g0 = 0;
            quad->b0 = 0;
            quad->r1 = 0;
            quad->g1 = 0;
            quad->b1 = 0;
            quad->r2 = 0;
            quad->g2 = 0;
            quad->b2 = 0;
            quad->r3 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->g3 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->b3 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            break;
        case ACTOR_160900_LIGHT_WHITE_CORNER_2:
            quad->r0 = 0;
            quad->g0 = 0;
            quad->b0 = 0;
            quad->r1 = 0;
            quad->g1 = 0;
            quad->b1 = 0;
            quad->r3 = 0;
            quad->g3 = 0;
            quad->b3 = 0;
            quad->r2 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->g2 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->b2 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            break;
        case ACTOR_160900_LIGHT_WHITE_CORNERS_1_3:
            quad->r0 = 0;
            quad->g0 = 0;
            quad->b0 = 0;
            quad->r1 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->g1 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->b1 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->r2 = 0;
            quad->g2 = 0;
            quad->b2 = 0;
            quad->r3 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->g3 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->b3 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            break;
        case ACTOR_160900_LIGHT_WHITE_CORNERS_0_2:
            quad->r0 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->g0 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->b0 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->r1 = 0;
            quad->g1 = 0;
            quad->b1 = 0;
            quad->r2 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->g2 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->b2 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->r3 = 0;
            quad->g3 = 0;
            quad->b3 = 0;
            break;
        case ACTOR_160900_LIGHT_WHITE_CORNERS_2_3:
            quad->r0 = 0;
            quad->g0 = 0;
            quad->b0 = 0;
            quad->r1 = 0;
            quad->g1 = 0;
            quad->b1 = 0;
            quad->r2 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->g2 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->b2 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->r3 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->g3 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->b3 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            break;
        case ACTOR_160900_LIGHT_WHITE_ALL_CORNERS:
            quad->r0 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->g0 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->b0 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->r1 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->g1 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->b1 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->r2 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->g2 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->b2 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->r3 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->g3 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            quad->b3 = ACTOR_160900_LIGHT_MAX_INTENSITY;
            break;
    }
}

/// Draws one additive gradient light quad in the cutscene.
///
/// Requires a live coordinate body and four local-space corners in the task's
/// work. The first spawn argument selects white corners (0 corner 3, 1 corner 2,
/// 2 corners 1 and 3, 3 corners 0 and 2, 4 corners 2 and 3, 5 all four);
/// remaining corners are black. Callers must supply one of these six patterns.
/// The coordinate origin supplies the ordering depth, quantized by four bits;
/// that tag must fit the current ordering table. The current frame arena must
/// hold a POLY_G4 and DR_TPAGE through GPU drawing. Projection changes GTE state.
static void _actor160900DrawLightQuadTask(Task* task)
{
    enum { ACTOR_160900_LIGHT_DEPTH_SHIFT = 4 };
    _Actor160900LightQuadWork* work;
    s16                        screenX[ARRAY_SIZE(work->corners)];
    s16                        screenY[ARRAY_SIZE(work->corners)];
    SVECTOR                    origin;
    s32                        screenXY;
    s32                        originDepth;
    GfxCoord*                  coord;
    POLY_G4*                   quad;
    DR_TPAGE*                  drawMode;
    s16                        cornerIndex;

    coord = task->extra.coordBody->coord;
    work  = task->work;
    actorRenderComposeCoord(coord);
    gte_SetTransMatrix(&coord->workm);
    gte_SetRotMatrix(&coord->workm);
    // Sort the whole quad by its origin, then project its four local corners.
    origin.vz = 0;
    origin.vy = 0;
    origin.vx = 0;
    gte_ldv0(&origin);
    gte_rtps();
    gte_stsxy(&screenXY);
    gte_stszotz(&originDepth);
    for (cornerIndex = 0; cornerIndex < ARRAY_SIZE(work->corners); cornerIndex++) {
        gte_ldv0(&work->corners[cornerIndex]);
        gte_rtps();
        gte_stsxy(&screenXY);
        screenX[cornerIndex] = screenXY;
        screenY[cornerIndex] = screenXY >> 16;
    }

    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setPolyG4(quad);
    setSemiTrans(quad, true);
    _actor160900ShadeLightQuad(quad, task);
    quad->x0 = screenX[0];
    quad->y0 = screenY[0];
    quad->x1 = screenX[1];
    quad->y1 = screenY[1];
    quad->x2 = screenX[2];
    quad->y2 = screenY[2];
    quad->x3 = screenX[3];
    quad->y3 = screenY[3];
    addPrim(&gGpuCurrentOt[originDepth >> ACTOR_160900_LIGHT_DEPTH_SHIFT], quad);
    // Prepending the mode packet makes additive blending active before the quad.
    drawMode       = gGpuPrimCursor;
    gGpuPrimCursor = drawMode + 1;
    setlen(drawMode, 1);
    drawMode->code[0] = _get_mode(false, true, getTPage(0, GPU_BLEND_ADD, 0, 0));
    addPrim(&gGpuCurrentOt[originDepth >> ACTOR_160900_LIGHT_DEPTH_SHIFT], drawMode);
}

/// Restarts a cutscene player clip with a ten-frame pose blend.
///
/// `task` owns live cutscene work; clip id 0..10 selects its loaded player set
/// and successor-chain entry. An absent borrowed player is a no-op. Dispatch
/// synchronously borrows the request, enables world collision and resets hold
/// time; the player and set resources must stay live throughout playback.
static inline void _actor160900BlendPlayerAnim(Task* task, u16 animationId)
{
    _Actor160900CutsceneWork* work;
    AnimationPlayRequest      request;

    work = task->work;
    if (work->player != NULL) {
        request.source.sets          = _gActor160900PlayerAnimationSets;
        work->playerAnimId           = animationId;
        request.animationId          = animationId;
        request.blend                = ANIMATION_BLEND_INTERPOLATE;
        request.blendFrames          = ACTOR_160900_ANIM_BLEND_FRAMES;
        request.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
        TASK_MESSAGE_DISPATCH_POINTER(work->player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &request, 0);
        work->playerAnimHold = 0;
    }
}

/// Restarts a cutscene player clip immediately, without blending from its pose.
///
/// Requires live cutscene work and a valid player clip id 0..10. A NULL player
/// is a no-op. Synchronous dispatch installs the loaded player sets, enables
/// world collision and resets hold time. The request lasts only through dispatch;
/// the borrowed player and animation data must stay live throughout playback.
static inline void _actor160900ResetPlayerAnim(Task* task, u16 animationId)
{
    _Actor160900CutsceneWork* work;
    AnimationPlayRequest      request;

    work = task->work;
    if (work->player != NULL) {
        request.source.sets          = _gActor160900PlayerAnimationSets;
        work->playerAnimId           = animationId;
        request.animationId          = animationId;
        request.blend                = ANIMATION_BLEND_RESET;
        request.blendFrames          = 0;
        request.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
        TASK_MESSAGE_DISPATCH_POINTER(work->player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &request, 0);
        work->playerAnimHold = 0;
    }
}

/// Advances Aya's clip chain and applies the cutscene's pending player cue.
///
/// Requires live cutscene work and loaded player clips 0..10. Placement and walk
/// cues require the borrowed player task. Messages consume stack requests
/// synchronously, while playback borrows the loaded animation resources.
/// The walk cue runs once at step zero and remains posted; other cues clear
/// after handling. Wave start lends the embedded context to a separate task
/// for an eight-update rise, so the cutscene work must outlive that task.
/// The walk cue's resident animation bank requires characterId 1 and weapon 0..32.
static void _actor160900UpdatePlayerCue(Task* task)
{
    enum {
        ACTOR_160900_PLAYER_PLACE_CLIP          = 10,
        ACTOR_160900_PLAYER_READY_CLIP          = 1,
        ACTOR_160900_PRIMARY_CHARACTER          = 1,
        ACTOR_160900_PRIMARY_WEAPON_BANK_BASE   = 1,
        ACTOR_160900_ALTERNATE_WEAPON_BANK_BASE = 0x22,
        ACTOR_160900_WAVE_RISE_UPDATES          = 8,
        ACTOR_160900_WAVE_FULL_SCALE            = 0x80,
    };
    _Actor160900CutsceneWork* work;
    union {
        AnimationPlayRequest animation;
        VECTOR3              destination;
    } message;

    _Actor160900CutsceneWork* requestWork;
    s32                       animationBankIndex;
    s32                       weaponId;

    /// Reloads cutscene work, restarts a clip immediately and resets its hold.
    ///
    /// task is a live Task pointer, clipId a valid u16 player-clip id, cueWork a
    /// writable work-pointer local and request a writable AnimationPlayRequest
    /// lvalue. Arguments must be stable locals/constants without side effects;
    /// cueWork, clipId and request are evaluated repeatedly. The request is
    /// borrowed synchronously, and the package's loaded sets through playback.
#define ACTOR_160900_RESET_PLAYER_CUE_CLIP(task, clipId, cueWork, request)                                       \
    do {                                                                                                         \
        (cueWork) = (task)->work;                                                                                \
        if ((cueWork)->player != NULL) {                                                                         \
            (request).source.sets          = _gActor160900PlayerAnimationSets;                                   \
            (cueWork)->playerAnimId        = (clipId);                                                           \
            (request).animationId          = (clipId);                                                           \
            (request).blend                = ANIMATION_BLEND_RESET;                                              \
            (request).blendFrames          = 0;                                                                  \
            (request).enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;                                   \
            TASK_MESSAGE_DISPATCH_POINTER((cueWork)->player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &(request), 0); \
            (cueWork)->playerAnimHold = 0;                                                                       \
        }                                                                                                        \
    } while (0)

    work = task->work;
    _actor160900AdvancePlayerAnimChain(task);
    switch (work->playerCue.id) {
        case ACTOR_160900_CUE_NONE:
            break;
        case ACTOR_160900_PLAYER_CUE_TAKE_PLACE:
            TASK_MESSAGE_DISPATCH_POINTER(work->player, GAME_ACTOR_MESSAGE_PLACE, D_actor_160900_8013F210, 0);
            ACTOR_160900_RESET_PLAYER_CUE_CLIP(task, ACTOR_160900_PLAYER_PLACE_CLIP, requestWork, message.animation);
            break;
        case ACTOR_160900_PLAYER_CUE_CLIP_1:
            ACTOR_160900_RESET_PLAYER_CUE_CLIP(task, ACTOR_160900_PLAYER_READY_CLIP, requestWork, message.animation);
            break;
        case ACTOR_160900_PLAYER_CUE_WALK_TO_MARK:
            if (work->playerCue.step == 0) {
                weaponId = gPlayerStatus.weapon;
                if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == ACTOR_160900_PRIMARY_CHARACTER) {
                    animationBankIndex = weaponId + ACTOR_160900_PRIMARY_WEAPON_BANK_BASE;
                } else {
                    animationBankIndex = weaponId + ACTOR_160900_ALTERNATE_WEAPON_BANK_BASE;
                }
                // Return to the equipped bank before taking scripted movement control.
                message.animation.source.index         = animationBankIndex;
                message.animation.animationId          = ACTOR_160900_PLAYER_READY_CLIP;
                message.animation.blend                = ANIMATION_BLEND_RESET;
                message.animation.blendFrames          = 0;
                message.animation.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;

                TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &message.animation, 0);
                TASK_MESSAGE_DISPATCH_POINTER(work->player, GAME_ACTOR_MESSAGE_PLACE, D_actor_160900_8013F228, 0);
                message.destination.vx = -0x7D0;
                message.destination.vy = 0;
                message.destination.vz = 0xC80;
                TASK_MESSAGE_DISPATCH_POINTER(work->player, GAME_ACTOR_MESSAGE_RUN_TO, &message.destination, 0);
                work->playerCue.step++;
            }
            return;
        case ACTOR_160900_PLAYER_CUE_WAVE_START:
            work->wave.span  = ACTOR_160900_WAVE_RISE_UPDATES;
            work->wave.scale = ACTOR_160900_WAVE_FULL_SCALE;
            taskSpawnFromTable(D_actor_160900_8013F17C, 0, 0, &work->wave);
            work->playerCue.id = ACTOR_160900_CUE_NONE;
            return;
        case ACTOR_160900_PLAYER_CUE_WAVE_END:
            work->wave.state = SCREEN_WAVE_RAMP_FINISHED;
            break;
        case ACTOR_160900_PLAYER_CUE_CLIP_2:
            _actor160900ResetPlayerAnim(task, 2);
            break;
        case ACTOR_160900_PLAYER_CUE_CLIP_6:
            _actor160900BlendPlayerAnim(task, 6);
            break;
        case ACTOR_160900_PLAYER_CUE_CLIP_7:
            _actor160900BlendPlayerAnim(task, 7);
            break;
        case ACTOR_160900_PLAYER_CUE_CLIP_8:
            _actor160900BlendPlayerAnim(task, 8);
            break;
        case ACTOR_160900_PLAYER_CUE_CLIP_9:
            _actor160900BlendPlayerAnim(task, 9);
            break;
    }
    work->playerCue.id = ACTOR_160900_CUE_NONE;
#undef ACTOR_160900_RESET_PLAYER_CUE_CLIP
}

/// Applies and consumes one posted Kyle cue for the cutscene.
///
/// Requires live cutscene work and Kyle's body and two hand tasks. Cue ids 1..4
/// hide/place him, show his models and spawn a decal, restart clip 1, or spawn
/// another decal. The optional handgun may be NULL. Decal offsets are signed
/// game-coordinate units in Kyle's model-root frame; spawns copy them. Clears
/// the cue id after handling, including zero or an unsupported id; step is kept.
static void _actor160900UpdateKyleCue(Task* task)
{
    enum { KYLE_CUE_CLIP        = 1,
           KYLE_DECAL_HALF_SIDE = 256 };
    _Actor160900CutsceneWork* work;
    SVECTOR                   appearDecalOffset;
    SVECTOR                   secondDecalOffset;

    work = task->work;
    switch (work->kyleCue.id) {
        case ACTOR_160900_CUE_NONE:
            break;
        case ACTOR_160900_KYLE_CUE_HIDE_AND_PLACE:
            taskMessageDispatch(work->kyle, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER, 0);
            TASK_MESSAGE_DISPATCH_POINTER(work->kyle, ACTOR_MESSAGE_PLACE, D_actor_160900_8013F240, 0);
            break;
        case ACTOR_160900_KYLE_CUE_APPEAR:
            taskMessageDispatch(work->kyleGunHand, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);
            taskMessageDispatch(work->kyleFreeHand, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);
            if (work->kyleGun != NULL) {
                taskMessageDispatch(work->kyleGun, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);
            }
            taskMessageDispatch(work->kyle, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);
            appearDecalOffset.vx = -100;
            appearDecalOffset.vy = 100;
            appearDecalOffset.vz = -1200;
            effectSpawn(EFFECT_GROUND_DECAL, work->kyle->extra.tmd->coords,
                        EFFECT_GROUND_DECAL_START_FULL_BRIGHT | KYLE_DECAL_HALF_SIDE, &appearDecalOffset);
            break;
        case ACTOR_160900_KYLE_CUE_CLIP_1:
            _actor160900BlendKyleAnim(work->kyle, KYLE_CUE_CLIP);
            break;
        case ACTOR_160900_KYLE_CUE_GROUND_DECAL:
            secondDecalOffset.vx = -200;
            secondDecalOffset.vy = 100;
            secondDecalOffset.vz = -400;
            effectSpawn(EFFECT_GROUND_DECAL, work->kyle->extra.tmd->coords,
                        EFFECT_GROUND_DECAL_START_FULL_BRIGHT | KYLE_DECAL_HALF_SIDE, &secondDecalOffset);
            break;
    }
    work->kyleCue.id = ACTOR_160900_CUE_NONE;
}

/// Emits drifting sprites over a point list every eighth display frame.
///
/// `points` must reach a `pad == -1` terminator within readable storage. XYZ
/// are signed game coordinates in the input frame of `GsWSMATRIX`;
/// X receives jitter in 100-unit steps from -700 through +700 and narrows to a
/// halfword. Each point consumes two LCG samples. Uses perspective-size numerator
/// 1024, three running updates per cell, speed 32 coordinate units per update,
/// upward random drift and the alternate sprite sheet.
/// Spawning borrows the local offset only for placement; this sprite callback
/// uses the copied coordinates. Failed spawns are ignored and still consume RNG.
static void _actor160900SpawnDriftingSprites(const SVECTOR* points)
{
    enum {
        ACTOR_160900_DRIFT_FRAME_MASK     = 7,
        ACTOR_160900_DRIFT_LIST_END       = -1,
        ACTOR_160900_DRIFT_JITTER_MASK    = 7,
        ACTOR_160900_DRIFT_JITTER_STEP    = 100,
        ACTOR_160900_DRIFT_SPAWN_ARGUMENT = 0x81203400,
    };
    SVECTOR spawnOffset;
    s32     jitteredX;
    u32     seed;
    s32     spawnArgument;

    if (!(gDisplayState.animFrame & ACTOR_160900_DRIFT_FRAME_MASK) && points->pad != ACTOR_160900_DRIFT_LIST_END) {
        spawnArgument = ACTOR_160900_DRIFT_SPAWN_ARGUMENT;
        do {
            // Draw sign and magnitude separately; the LCG and sum wrap as unsigned words.
            seed            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState = seed;
            jitteredX       = points->vx + (((seed >> 16) & 1) ? ((gRandomLcgState = seed * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & ACTOR_160900_DRIFT_JITTER_MASK
                                                               : -(((gRandomLcgState = seed * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & ACTOR_160900_DRIFT_JITTER_MASK)) *
                                         ACTOR_160900_DRIFT_JITTER_STEP;
            spawnOffset.vx = jitteredX;
            spawnOffset.vy = points->vy;
            spawnOffset.vz = points->vz;
            effectSpawn(EFFECT_1B4, NULL, spawnArgument, &spawnOffset);
            points++;
        } while (points->pad != ACTOR_160900_DRIFT_LIST_END);
    }
}

/// Spawns the two additive light quads used at the opening playback cue.
///
/// Requires the published cutscene task and work, and empty light slots 0..1.
/// Places two 1500-by-1000 rectangles beside Z=3000, at root (6000,1000,3000)
/// in view-parent coordinates; patterns 0 and 1 whiten their outside corners.
/// Each quad owns primary-heap corner work and a coordinate body. Failure stops
/// the sequence without rolling back earlier quads. Work-allocation failure
/// kills that task but retains its saved slot; normal removal kills all slots.
static void _actor160900SpawnOpeningLightPair(void)
{
    _Actor160900CutsceneWork*  cutsceneWork;
    _Actor160900LightQuadWork* allocatedQuadWork;
    _Actor160900LightQuadWork* quadWork;
    Task*                      quadTask;

    cutsceneWork                = D_actor_160900_8013FBB4->work;
    quadTask                    = taskSpawnFromTable(D_actor_160900_8013FB50, ACTOR_160900_TASK_LIGHT_QUAD, ACTOR_160900_LIGHT_PATTERN_CORNER_3, 0);
    cutsceneWork->lightQuads[0] = quadTask;
    if (quadTask == NULL) {
        return;
    }
    allocatedQuadWork = memCalloc(sizeof(*allocatedQuadWork), false);
    quadTask->work    = allocatedQuadWork;
    if (allocatedQuadWork == NULL) {
        taskKill(quadTask);
        return;
    }
    quadWork = allocatedQuadWork;
    memFillBytes(quadWork, 0, sizeof(*quadWork));
    quadTask->extra.coordBody->coord->parent     = &gGfxViewCoord;
    quadTask->extra.coordBody->coord->coord.t[0] = 0x1770;
    quadTask->extra.coordBody->coord->coord.t[1] = 0x3E8;
    quadTask->extra.coordBody->coord->coord.t[2] = 0xBB8;
    quadWork->corners[0].vx                      = 0;
    quadWork->corners[0].vy                      = -0x5DC;
    quadWork->corners[0].vz                      = 0x3E8;
    quadWork->corners[1].vx                      = 0;
    quadWork->corners[1].vy                      = -0x5DC;
    quadWork->corners[1].vz                      = 0;
    quadWork->corners[2].vx                      = 0;
    quadWork->corners[2].vy                      = 0;
    quadWork->corners[2].vz                      = 0x3E8;
    quadWork->corners[3].vx                      = 0;
    quadWork->corners[3].vy                      = 0;
    quadWork->corners[3].vz                      = 0;
    quadTask                                     = taskSpawnFromTable(D_actor_160900_8013FB50, ACTOR_160900_TASK_LIGHT_QUAD, ACTOR_160900_LIGHT_PATTERN_CORNER_2, 0);
    cutsceneWork->lightQuads[1]                  = quadTask;
    if (quadTask == NULL) {
        return;
    }
    allocatedQuadWork = memCalloc(sizeof(*allocatedQuadWork), false);
    quadTask->work    = allocatedQuadWork;
    if (allocatedQuadWork == NULL) {
        taskKill(quadTask);
        return;
    }
    quadWork = allocatedQuadWork;
    memFillBytes(quadWork, 0, sizeof(*quadWork));
    quadTask->extra.coordBody->coord->parent     = &gGfxViewCoord;
    quadTask->extra.coordBody->coord->coord.t[0] = 0x1770;
    quadTask->extra.coordBody->coord->coord.t[1] = 0x3E8;
    quadTask->extra.coordBody->coord->coord.t[2] = 0xBB8;
    quadWork->corners[0].vx                      = 0;
    quadWork->corners[0].vy                      = -0x5DC;
    quadWork->corners[0].vz                      = 0;
    quadWork->corners[1].vx                      = 0;
    quadWork->corners[1].vy                      = -0x5DC;
    quadWork->corners[1].vz                      = -0x3E8;
    quadWork->corners[2].vx                      = 0;
    quadWork->corners[2].vy                      = 0;
    quadWork->corners[2].vz                      = 0;
    quadWork->corners[3].vx                      = 0;
    quadWork->corners[3].vy                      = 0;
    quadWork->corners[3].vz                      = -0x3E8;
}
/// Parents one reveal light quad to the view and lays out its local Y/Z rectangle.
///
/// Requires a live coordinate body and four writable corners. The root is placed
/// at X=6000, Y=rootY, Z=2700; rootY is a full-width game coordinate, while the
/// local Z edges are signed halfwords. Corners 0/1 are at Y=-1000 and 2/3 at Y=0,
/// all at X=0. Keeps corner pads and the composition stamp; retains neither pointer.
static inline void _actor160900PlaceRevealLightQuad(Task* quadTask, _Actor160900LightQuadWork* quadWork, s32 rootY, s16 rightZ, s16 leftZ)
{
    quadTask->extra.coordBody->coord->parent     = &gGfxViewCoord;
    quadTask->extra.coordBody->coord->coord.t[0] = 6000;
    quadTask->extra.coordBody->coord->coord.t[1] = rootY;
    quadTask->extra.coordBody->coord->coord.t[2] = 2700;
    quadWork->corners[0].vx                      = 0;
    quadWork->corners[0].vy                      = -1000;
    quadWork->corners[0].vz                      = rightZ;
    quadWork->corners[1].vx                      = 0;
    quadWork->corners[1].vy                      = -1000;
    quadWork->corners[1].vz                      = leftZ;
    quadWork->corners[2].vx                      = 0;
    quadWork->corners[2].vy                      = 0;
    quadWork->corners[2].vz                      = rightZ;
    quadWork->corners[3].vx                      = 0;
    quadWork->corners[3].vy                      = 0;
    quadWork->corners[3].vz                      = leftZ;
}

/// Spawns the six additive light quads for Kyle's reveal.
///
/// Requires the published cutscene's live work and empty light slots 0..5.
/// Builds two three-quad rows at roots (6000,+/-500,2700), parented to the view.
/// Each 1000-unit-high row spans local Z=-1000..1000, with the middle quad
/// 1000 units wide and the outer quads 500 units wide. Patterns 5,2,3 and
/// 4,0,1 respectively select the white corners. Owns each coordinate body and
/// primary-heap corner block. Failure stops with earlier quads retained; a
/// failed work allocation kills its task without clearing the saved slot.
static void _actor160900SpawnKyleRevealLightQuads(void)
{
    _Actor160900CutsceneWork*  cutsceneWork;
    _Actor160900LightQuadWork* allocatedQuadWork;
    _Actor160900LightQuadWork* quadWork;
    Task*                      quadTask;

    cutsceneWork                = D_actor_160900_8013FBB4->work;
    quadTask                    = taskSpawnFromTable(D_actor_160900_8013FB50, ACTOR_160900_TASK_LIGHT_QUAD, ACTOR_160900_LIGHT_PATTERN_ALL_CORNERS, 0);
    cutsceneWork->lightQuads[0] = quadTask;
    if (quadTask == NULL) {
        return;
    }
    allocatedQuadWork = memCalloc(sizeof(*allocatedQuadWork), false);
    quadTask->work    = allocatedQuadWork;
    if (allocatedQuadWork == NULL) {
        taskKill(quadTask);
        return;
    }
    quadWork = allocatedQuadWork;
    memFillBytes(quadWork, 0, sizeof(*quadWork));
    _actor160900PlaceRevealLightQuad(quadTask, quadWork, 0x1F4, 0x1F4, -0x1F4);
    quadTask                    = taskSpawnFromTable(D_actor_160900_8013FB50, ACTOR_160900_TASK_LIGHT_QUAD, ACTOR_160900_LIGHT_PATTERN_CORNERS_1_3, 0);
    cutsceneWork->lightQuads[1] = quadTask;
    if (quadTask == NULL) {
        return;
    }
    allocatedQuadWork = memCalloc(sizeof(*allocatedQuadWork), false);
    quadTask->work    = allocatedQuadWork;
    if (allocatedQuadWork == NULL) {
        taskKill(quadTask);
        return;
    }
    quadWork = allocatedQuadWork;
    memFillBytes(quadWork, 0, sizeof(*quadWork));
    _actor160900PlaceRevealLightQuad(quadTask, quadWork, 0x1F4, 0x3E8, 0x1F4);
    quadTask                    = taskSpawnFromTable(D_actor_160900_8013FB50, ACTOR_160900_TASK_LIGHT_QUAD, ACTOR_160900_LIGHT_PATTERN_CORNERS_0_2, 0);
    cutsceneWork->lightQuads[2] = quadTask;
    if (quadTask == NULL) {
        return;
    }
    allocatedQuadWork = memCalloc(sizeof(*allocatedQuadWork), false);
    quadTask->work    = allocatedQuadWork;
    if (allocatedQuadWork == NULL) {
        taskKill(quadTask);
        return;
    }
    quadWork = allocatedQuadWork;
    memFillBytes(quadWork, 0, sizeof(*quadWork));
    _actor160900PlaceRevealLightQuad(quadTask, quadWork, 0x1F4, -0x1F4, -0x3E8);
    quadTask                    = taskSpawnFromTable(D_actor_160900_8013FB50, ACTOR_160900_TASK_LIGHT_QUAD, ACTOR_160900_LIGHT_PATTERN_CORNERS_2_3, 0);
    cutsceneWork->lightQuads[3] = quadTask;
    if (quadTask == NULL) {
        return;
    }
    allocatedQuadWork = memCalloc(sizeof(*allocatedQuadWork), false);
    quadTask->work    = allocatedQuadWork;
    if (allocatedQuadWork == NULL) {
        taskKill(quadTask);
        return;
    }
    quadWork = allocatedQuadWork;
    memFillBytes(quadWork, 0, sizeof(*quadWork));
    _actor160900PlaceRevealLightQuad(quadTask, quadWork, -0x1F4, 0x1F4, -0x1F4);
    quadTask                    = taskSpawnFromTable(D_actor_160900_8013FB50, ACTOR_160900_TASK_LIGHT_QUAD, ACTOR_160900_LIGHT_PATTERN_CORNER_3, 0);
    cutsceneWork->lightQuads[4] = quadTask;
    if (quadTask == NULL) {
        return;
    }
    allocatedQuadWork = memCalloc(sizeof(*allocatedQuadWork), false);
    quadTask->work    = allocatedQuadWork;
    if (allocatedQuadWork == NULL) {
        taskKill(quadTask);
        return;
    }
    quadWork = allocatedQuadWork;
    memFillBytes(quadWork, 0, sizeof(*quadWork));
    _actor160900PlaceRevealLightQuad(quadTask, quadWork, -0x1F4, 0x3E8, 0x1F4);
    quadTask                    = taskSpawnFromTable(D_actor_160900_8013FB50, ACTOR_160900_TASK_LIGHT_QUAD, ACTOR_160900_LIGHT_PATTERN_CORNER_2, 0);
    cutsceneWork->lightQuads[5] = quadTask;
    if (quadTask == NULL) {
        return;
    }
    allocatedQuadWork = memCalloc(sizeof(*allocatedQuadWork), false);
    quadTask->work    = allocatedQuadWork;
    if (allocatedQuadWork == NULL) {
        taskKill(quadTask);
        return;
    }
    quadWork = allocatedQuadWork;
    memFillBytes(quadWork, 0, sizeof(*quadWork));
    _actor160900PlaceRevealLightQuad(quadTask, quadWork, -0x1F4, -0x1F4, -0x3E8);
}
/// Spawns the two additive light quads used when Aya walks to her mark.
///
/// Requires the published cutscene's live work and empty light slots 0..1.
/// Places two 1500-by-1000 rectangles beside Z=3000, at root (6000,0,3000)
/// in view-parent coordinates; patterns 0 and 1 whiten their outside corners.
/// Owns each coordinate body and primary-heap corner block. Failure stops with
/// earlier quads retained; a failed work allocation kills its task without
/// clearing the saved slot. The script removes the set before reusing slots.
static void _actor160900SpawnWalkLightPair(void)
{
    _Actor160900CutsceneWork*  cutsceneWork;
    _Actor160900LightQuadWork* allocatedQuadWork;
    _Actor160900LightQuadWork* quadWork;
    Task*                      quadTask;

    cutsceneWork                = D_actor_160900_8013FBB4->work;
    quadTask                    = taskSpawnFromTable(D_actor_160900_8013FB50, ACTOR_160900_TASK_LIGHT_QUAD, ACTOR_160900_LIGHT_PATTERN_CORNER_3, 0);
    cutsceneWork->lightQuads[0] = quadTask;
    if (quadTask == NULL) {
        return;
    }
    allocatedQuadWork = memCalloc(sizeof(*allocatedQuadWork), false);
    quadTask->work    = allocatedQuadWork;
    if (allocatedQuadWork == NULL) {
        taskKill(quadTask);
        return;
    }
    quadWork = allocatedQuadWork;
    memFillBytes(quadWork, 0, sizeof(*quadWork));
    quadTask->extra.coordBody->coord->parent     = &gGfxViewCoord;
    quadTask->extra.coordBody->coord->coord.t[0] = 0x1770;
    quadTask->extra.coordBody->coord->coord.t[1] = 0;
    quadTask->extra.coordBody->coord->coord.t[2] = 0xBB8;
    quadWork->corners[0].vx                      = 0;
    quadWork->corners[0].vy                      = -0x5DC;
    quadWork->corners[0].vz                      = 0x3E8;
    quadWork->corners[1].vx                      = 0;
    quadWork->corners[1].vy                      = -0x5DC;
    quadWork->corners[1].vz                      = 0;
    quadWork->corners[2].vx                      = 0;
    quadWork->corners[2].vy                      = 0;
    quadWork->corners[2].vz                      = 0x3E8;
    quadWork->corners[3].vx                      = 0;
    quadWork->corners[3].vy                      = 0;
    quadWork->corners[3].vz                      = 0;
    quadTask                                     = taskSpawnFromTable(D_actor_160900_8013FB50, ACTOR_160900_TASK_LIGHT_QUAD, ACTOR_160900_LIGHT_PATTERN_CORNER_2, 0);
    cutsceneWork->lightQuads[1]                  = quadTask;
    if (quadTask == NULL) {
        return;
    }
    allocatedQuadWork = memCalloc(sizeof(*allocatedQuadWork), false);
    quadTask->work    = allocatedQuadWork;
    if (allocatedQuadWork == NULL) {
        taskKill(quadTask);
        return;
    }
    quadWork = allocatedQuadWork;
    memFillBytes(quadWork, 0, sizeof(*quadWork));
    quadTask->extra.coordBody->coord->parent     = &gGfxViewCoord;
    quadTask->extra.coordBody->coord->coord.t[0] = 0x1770;
    quadTask->extra.coordBody->coord->coord.t[1] = 0;
    quadTask->extra.coordBody->coord->coord.t[2] = 0xBB8;
    quadWork->corners[0].vx                      = 0;
    quadWork->corners[0].vy                      = -0x5DC;
    quadWork->corners[0].vz                      = 0;
    quadWork->corners[1].vx                      = 0;
    quadWork->corners[1].vy                      = -0x5DC;
    quadWork->corners[1].vz                      = -0x3E8;
    quadWork->corners[2].vx                      = 0;
    quadWork->corners[2].vy                      = 0;
    quadWork->corners[2].vz                      = 0;
    quadWork->corners[3].vx                      = 0;
    quadWork->corners[3].vy                      = 0;
    quadWork->corners[3].vz                      = -0x3E8;
}
/// Runs the package's Aya/Kyle cutscene and its three script-controlled cue lanes.
///
/// State zero waits for the attachment wheel and display transition to finish,
/// allocates owned cutscene work and publishes this task for script callbacks.
/// Borrows the player, spawns Kyle's body and hands, and selects CAP file 3.
/// State one starts the main/skip event scripts; state two waits for eventState
/// zero, posts scene event 30 and requests teardown. Both states process cues.
/// Successful setup requires live player/model resources and successful child
/// spawns. Kyle's models join the teardown tree; light and wave task lifetimes
/// are controlled by the scripts and must end before the cutscene work dies.
static void _actor160900CutsceneTask(Task* task)
{
    enum {
        ACTOR_160900_CUTSCENE_INITIALIZE   = 0,
        ACTOR_160900_CUTSCENE_START_SCRIPT = 1,
        ACTOR_160900_CUTSCENE_RUN_SCRIPT   = 2,
        ACTOR_160900_EVENT_FINISHED        = 0,
        ACTOR_160900_CAP_FILE              = 3,
        ACTOR_160900_CAP_TEXTURE_X         = 384,
        ACTOR_160900_CAP_TEXTURE_Y         = 0,
        ACTOR_160900_NEXT_SCENE_EVENT      = 30,
        ACTOR_160900_EFFECT_CUE_LIST_1     = 1,
        ACTOR_160900_EFFECT_CUE_LIST_2     = 2,
        ACTOR_160900_EFFECT_CUE_LIST_3     = 3,
        ACTOR_160900_EFFECT_CUE_LIST_4     = 4,
        ACTOR_160900_EFFECT_CUE_LIST_5     = 5,
    };
    _Actor160900CutsceneWork* work;
    _Actor160900CutsceneWork* cueWork;

    switch (task->state) {
        case ACTOR_160900_CUTSCENE_INITIALIZE:
            if (Gp_StateC08.mode == ATTACHMENT_MODE_WHEEL || gDisplayState.pendingMode != DISPLAY_MODE_NONE) {
                return;
            }
            work       = memMalloc(sizeof(*work), false);
            task->work = work;
            if (work == NULL) {
                taskKill(task);
            } else {
                memFillBytes(work, 0, sizeof(*work));
                work->player            = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
                D_actor_160900_8013FBB4 = task;
                // Publish the cast before event-script callbacks can address it.
                work->kyle         = taskSpawnFromTable(D_actor_160900_8013FB50, ACTOR_160900_TASK_KYLE_BODY, 0, task);
                work->kyleGunHand  = taskSpawnFromTable(D_actor_160900_8013FB50, ACTOR_160900_TASK_KYLE_GUN_HAND, ACTOR_160900_KYLE_GUN_HAND, work->kyle);
                work->kyleFreeHand = taskSpawnFromTable(D_actor_160900_8013FB50, ACTOR_160900_TASK_KYLE_FREE_HAND, ACTOR_160900_KYLE_FREE_HAND, work->kyle);
            }
            // Caption setup and the state increment also run after allocation failure.
            Gp_CapFile = NULL;
            capSelectLoadedFile(ACTOR_160900_CAP_FILE);
            capSetTexturePage(ACTOR_160900_CAP_TEXTURE_X, ACTOR_160900_CAP_TEXTURE_Y);
            task->state += 1;
            return;
        case ACTOR_160900_CUTSCENE_START_SCRIPT:
            evsStartScriptWithSkip(D_actor_160900_8013F538, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_actor_160900_8013FAA8);
            task->state += 1;
            break;
        case ACTOR_160900_CUTSCENE_RUN_SCRIPT:
            if (gGameSession->eventState == ACTOR_160900_EVENT_FINISHED) {
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = ACTOR_160900_NEXT_SCENE_EVENT;
                taskRequestKill(task, 0);
            }
            break;
    }
    _actor160900UpdatePlayerCue(task);
    _actor160900UpdateKyleCue(task);
    // Player and Kyle consume their cues; the sprite lane stays posted across frames.
    cueWork = task->work;
    switch (cueWork->effectCue.id) {
        case ACTOR_160900_EFFECT_CUE_LIST_1:
            _actor160900SpawnDriftingSprites(D_actor_160900_8013F258);
            break;
        case ACTOR_160900_EFFECT_CUE_LIST_2:
            _actor160900SpawnDriftingSprites(D_actor_160900_8013F2E0);
            break;
        case ACTOR_160900_EFFECT_CUE_LIST_3:
            _actor160900SpawnDriftingSprites(D_actor_160900_8013F3B0);
            break;
        case ACTOR_160900_EFFECT_CUE_LIST_4:
            _actor160900SpawnDriftingSprites(D_actor_160900_8013F400);
            break;
        case ACTOR_160900_EFFECT_CUE_LIST_5:
            _actor160900SpawnDriftingSprites(D_actor_160900_8013F458);
            break;
        case ACTOR_160900_CUE_NONE:
        default:
            cueWork->effectCue.id = ACTOR_160900_CUE_NONE;
            break;
    }
}

/// Darkens the cutscene to black and holds its subtractive overlay.
///
/// Start at state 0 with no owned work. Allocates primary-heap fade work,
/// killing the task on failure; normal task teardown releases the work.
/// The unsigned low half of spawnArg1 adds intensity units per update, after
/// drawing the low red/green/red bytes. Stored channels narrow to signed
/// halfwords before the red >= 256 test clamps all three to 255. Large rates
/// can wrap negative; zero holds the current level. Other states do nothing.
/// Requires the frame arena and foreground tag used by `fadeDrawOverlay`.
static void _actor160900FadeOutTask(Task* task)
{
    enum {
        ACTOR_160900_FADE_OUT_INITIALIZE = 0,
        ACTOR_160900_FADE_OUT_RAMP       = 1,
        ACTOR_160900_FADE_MAX_INTENSITY  = 255,
    };
    ScreenFadeWork* fade;
    ScreenFadeWork* allocatedFade;

    fade = task->work;
    switch (task->state) {
        case ACTOR_160900_FADE_OUT_INITIALIZE:
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
            // Initialization also draws and advances the ramp.
            /* fallthrough */
        case ACTOR_160900_FADE_OUT_RAMP:
            fadeDrawOverlay(fade->r, fade->g, fade->r, GPU_BLEND_SUBTRACT);
            fade->r += task->spawnArg1.halves.low;
            fade->g += task->spawnArg1.halves.low;
            fade->b += task->spawnArg1.halves.low;
            if (fade->r > ACTOR_160900_FADE_MAX_INTENSITY) {
                fade->b = ACTOR_160900_FADE_MAX_INTENSITY;
                fade->g = ACTOR_160900_FADE_MAX_INTENSITY;
                fade->r = ACTOR_160900_FADE_MAX_INTENSITY;
            }
            break;
    }
}
/// Reveals the cutscene while restoring display output after three updates.
///
/// Start at state 0 with no owned work. Allocates primary-heap fade work at
/// intensity 255, killing the task on failure; teardown releases the work.
/// States 0..4 draw and step each update; state 3 enables display output, then
/// state 4 holds the ramp phase. Other states do nothing. The unsigned low half
/// of spawnArg1 subtracts intensity units after drawing low red/green/red bytes.
/// Channels narrow to signed halfwords; negative stored red kills the task.
/// Large rates can wrap, and zero holds indefinitely. Requires the frame arena
/// and foreground ordering-table tag used by `fadeDrawOverlay`.
static void _actor160900FadeInTask(Task* task)
{
    enum {
        ACTOR_160900_FADE_IN_INITIALIZE     = 0,
        ACTOR_160900_FADE_IN_WAIT_FIRST     = 1,
        ACTOR_160900_FADE_IN_WAIT_SECOND    = 2,
        ACTOR_160900_FADE_IN_ENABLE_DISPLAY = 3,
        ACTOR_160900_FADE_IN_RAMP           = 4,
        ACTOR_160900_FADE_MAX_INTENSITY     = 255,
    };
    ScreenFadeWork* fade;
    ScreenFadeWork* allocatedFade;

    fade = task->work;
    switch (task->state) {
        case ACTOR_160900_FADE_IN_INITIALIZE:
            allocatedFade = memMalloc(sizeof(*allocatedFade), false);
            task->work    = allocatedFade;
            if (allocatedFade == NULL) {
                taskKill(task);
                return;
            }
            fade         = allocatedFade;
            fade->b      = ACTOR_160900_FADE_MAX_INTENSITY;
            fade->g      = ACTOR_160900_FADE_MAX_INTENSITY;
            fade->r      = ACTOR_160900_FADE_MAX_INTENSITY;
            task->state += 1;
            break;
        case ACTOR_160900_FADE_IN_ENABLE_DISPLAY:
            SetDispMask(true);
            /* fallthrough */
        case ACTOR_160900_FADE_IN_WAIT_FIRST:
        case ACTOR_160900_FADE_IN_WAIT_SECOND:
            task->state += 1;
            break;
        case ACTOR_160900_FADE_IN_RAMP:
            break;
        default:
            return;
    }
    // Fade progress continues while the display is masked.
    fadeDrawOverlay(fade->r, fade->g, fade->r, GPU_BLEND_SUBTRACT);
    fade->r -= task->spawnArg1.halves.low;
    fade->g -= task->spawnArg1.halves.low;
    fade->b -= task->spawnArg1.halves.low;
    if (fade->r < 0) {
        taskKill(task);
    }
}
/// Applies a draw request to one of Kyle's live TMD models.
///
/// SHOW clears active-draw and automatic-buffer exclusions; HIDE_SKIP_AUTO_BUFFER
/// sets both. Mode 0 and other values leave all flags intact. Neither allocates
/// nor releases a buffer. The message id and second payload are ignored;
/// callers must ignore the dispatch result because no return value is defined.
static void _actor160900SetModelDraw(Task* task, s32 messageId, s32 mode, s32 unusedArg)
{
    enum { ACTOR_160900_MODEL_DRAW_KEEP = 0 };
    TmdObject* model;

    model = task->extra.tmd;
    switch (mode) {
        case ACTOR_160900_MODEL_DRAW_KEEP:
            break;
        case ACTOR_MESSAGE_DRAW_SHOW:
            model->flags = model->flags & (u16) ~(TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            return;
        case ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER:
            model->flags = model->flags | (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            return;
    }
}

#include "../../shared/actor_messages_place_ypr.inc.c"

/// Starts the cutscene's reveal fade, restoring display output on its fourth update.
///
/// The unsigned low half of intensityStep is subtracted from intensity after
/// each draw; the upper half is ignored. Zero holds black indefinitely, and
/// large rates retain signed-halfword wrap. Task/work allocation may fail.
static void _actor160900SpawnFadeIn(s32 intensityStep)
{
    taskSpawnFromTable(D_actor_160900_8013FB50, ACTOR_160900_TASK_FADE_IN, intensityStep, 0);
}

/// Starts the cutscene's darkening fade and holds the resulting black overlay.
///
/// The unsigned low half of intensityStep is added to intensity after each draw;
/// the upper half is ignored. Zero holds the starting level, and large rates
/// retain signed-halfword wrap. Task/work allocation may fail.
static void _actor160900SpawnFadeOut(s32 intensityStep)
{
    taskSpawnFromTable(D_actor_160900_8013FB50, ACTOR_160900_TASK_FADE_OUT, intensityStep, 0);
}

/// Kills and clears every light quad held by the running cutscene.
///
/// Requires the published cutscene task and its work to remain live. Empty
/// slots are skipped; killing a quad releases its owned corner work and body.
static void _actor160900KillLightQuads(void)
{
    _Actor160900CutsceneWork* work;
    Task*                     quadTask;
    s16                       quadIndex;

    work = D_actor_160900_8013FBB4->work;
    for (quadIndex = 0; quadIndex < ARRAY_SIZE(work->lightQuads); quadIndex++) {
        quadTask = work->lightQuads[quadIndex];
        if (quadTask != NULL) {
            taskKill(quadTask);
            work->lightQuads[quadIndex] = NULL;
        }
    }
}
/// Replaces Aya's pending cutscene cue and restarts its step at zero.
///
/// Requires the running cutscene's published work. cueId is zero for none or
/// an ACTOR_160900_PLAYER_CUE_* value; it narrows to the stored unsigned halfword.
/// Posting retains the cue counter; the action that uses it initializes it.
static void _actor160900PostPlayerCue(s16 cueId)
{
    _Actor160900CutsceneWork* work;

    work                 = D_actor_160900_8013FBB4->work;
    work->playerCue.id   = cueId;
    work->playerCue.step = 0;
}
/// Replaces Kyle's pending cutscene cue and restarts its step at zero.
///
/// Requires the running cutscene's published work. cueId is zero for none or
/// an ACTOR_160900_KYLE_CUE_* value; it narrows to the stored unsigned halfword.
/// Posting leaves the cue counter intact.
static void _actor160900PostKyleCue(s16 cueId)
{
    _Actor160900CutsceneWork* work;

    work               = D_actor_160900_8013FBB4->work;
    work->kyleCue.id   = cueId;
    work->kyleCue.step = 0;
}

/// Selects the point list for the cutscene's repeating effect cue.
///
/// Requires the running cutscene's published work. cueId is zero for none or
/// 1..5 for a point list, stored as an unsigned halfword. Replaces the pending
/// id and clears its step, retaining its counter; the handler emits every
/// eighth frame until the script changes or clears the cue.
static void _actor160900PostEffectCue(s16 cueId)
{
    _Actor160900CutsceneWork* work;

    work                 = D_actor_160900_8013FBB4->work;
    work->effectCue.id   = cueId;
    work->effectCue.step = 0;
}
/// Stops pending cutscene cues and scene playback when the skip script runs.
///
/// Requires the published cutscene work and a previously selected scene.
/// Clears the three cue ids, retaining their progress, requests CD cancellation
/// and restores display output. The following script command handles teardown.
static void _actor160900SkipCutscene(void)
{
    _Actor160900CutsceneWork* work;

    work               = D_actor_160900_8013FBB4->work;
    work->playerCue.id = ACTOR_160900_CUE_NONE;
    work->kyleCue.id   = ACTOR_160900_CUE_NONE;
    work->effectCue.id = ACTOR_160900_CUE_NONE;
    cdCmdCancelScene();
    SetDispMask(true);
}

/// Stages the cutscene's selected audio session for deferred start.
///
/// The scene descriptor and playback buffers must survive the later command
/// commit and consumption. Called by the event script after scene selection.
static void _actor160900StageSceneAudioStart(void)
{
    cdCmdStageSceneAudioStart();
}

/// Enqueues playback of the cutscene's selected scene/audio session.
///
/// Requires prepared scene storage that survives command consumption and the
/// free command-ring capacity required by `cdCmdEnqueueScenePlayback`.
static void _actor160900EnqueueScenePlayback(void)
{
    cdCmdEnqueueScenePlayback();
}

/// Cancels cutscene playback and finishes its scene-streaming state.
///
/// Requires a previously selected scene. Keeps both finish operations: the CD
/// cancellation path finishes the scene first, then this callback repeats it,
/// restoring the saved random generators again. Task teardown is script-owned.
static void _actor160900FinishScenePlayback(void)
{
    cdCmdCancelScene();
    streamFinishScene();
}
