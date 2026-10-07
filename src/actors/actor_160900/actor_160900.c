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

/// The overlay's task table. Entry 0 is `func_actor_160900_8013418C`, which
/// spawns entries 3, 5 and 6 into `_Actor160900CutsceneWork`; entries 1 and 2 are
/// spawned by `func_actor_160900_801346B0` / `func_actor_160900_801346E0`.
extern TaskDesc D_actor_160900_8013FB50[];

/// Animation script `func_actor_160900_801326EC` walks and the animation-set
/// table it hands the player task as message 0x3F4's `field_0`.
extern ActorAnimChainLink D_actor_160900_8013F1CC[];

extern u8       D_actor_160900_8013F210[];
extern u8       D_actor_160900_8013F228[];
extern TaskDesc D_actor_160900_8013F17C[];

/// Point lists `func_actor_160900_8013418C` hands `func_actor_160900_80133758`
/// for `_Actor160900CutsceneWork::effectCue` ids 1-5.
extern SVECTOR D_actor_160900_8013F258[];
extern SVECTOR D_actor_160900_8013F2E0[];
extern SVECTOR D_actor_160900_8013F3B0[];
extern SVECTOR D_actor_160900_8013F400[];
extern SVECTOR D_actor_160900_8013F458[];

/// Pair of blocks `func_actor_160900_8013418C` passes to `evsStartScriptWithSkip`.
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

static TmdSource _gActor160900KyleMadiganBody;
static TmdSource _gActor160900KyleMadiganGun;
static TmdSource _gActor160900KyleMadiganLeft;
static TmdSource _gActor160900KyleMadiganHandRight;
void             func_actor_160900_80132A14(Task*);
void             func_actor_160900_80132C08(Task*);
void             func_actor_160900_80132E80(Task*);
void             func_actor_160900_80133880(void);
void             func_actor_160900_80133A84(void);
void             func_actor_160900_80133F90(void);
void             func_actor_160900_8013418C(Task*);
void             func_actor_160900_801343E4(Task*);
void             func_actor_160900_801344D8(Task*);
void             func_actor_160900_801346B0(s32);
void             func_actor_160900_801346E0(s32);
void             func_actor_160900_80134710(void);
void             func_actor_160900_80134790(s16);
void             func_actor_160900_801347B0(s16);
void             func_actor_160900_801347D0(s16);
void             func_actor_160900_801347F0(void);
void             func_actor_160900_80134830(void);
void             func_actor_160900_80134850(void);
void             func_actor_160900_80134870(void);

void func_actor_160900_801345D0(Task*, s32, s32, s32);

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
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_160900_801345D0 },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_160900_80134790 }, { .value = ACTOR_160900_PLAYER_CUE_TAKE_PLACE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_160900_801347B0 }, { .value = ACTOR_160900_KYLE_CUE_HIDE_AND_PLACE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_160900_801347D0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_actor_160900_8013F530 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_160900_80134830 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_160900_80133880 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_160900_80134850 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_160900_801346B0 }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_160900_80134790 }, { .value = ACTOR_160900_PLAYER_CUE_CLIP_1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_160900_801347D0 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_160900_80134710 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_160900_80133A84 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_160900_801347B0 }, { .value = ACTOR_160900_KYLE_CUE_APPEAR }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_160900_80134790 }, { .value = ACTOR_160900_PLAYER_CUE_CLIP_2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_160900_80134790 }, { .value = ACTOR_160900_PLAYER_CUE_CLIP_6 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_160900_80134790 }, { .value = ACTOR_160900_PLAYER_CUE_CLIP_9 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_160900_80134790 }, { .value = ACTOR_160900_PLAYER_CUE_CLIP_7 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_160900_80134790 }, { .value = ACTOR_160900_PLAYER_CUE_CLIP_8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_160900_801347D0 }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_160900_80134710 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_160900_80133F90 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_160900_80134790 }, { .value = ACTOR_160900_PLAYER_CUE_WALK_TO_MARK }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_160900_80134790 }, { .value = ACTOR_160900_PLAYER_CUE_WAVE_START }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_160900_801347D0 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_160900_80134710 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_160900_80134790 }, { .value = ACTOR_160900_PLAYER_CUE_WAVE_END }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_160900_801347B0 }, { .value = ACTOR_160900_KYLE_CUE_CLIP_1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_160900_801347D0 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = roomEffectRequestCancelAll }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_160900_801346E0 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_160900_801347B0 }, { .value = ACTOR_160900_KYLE_CUE_GROUND_DECAL }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_160900_80134870 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_160900_8013FAA8[7] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_160900_801347F0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 43 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc D_actor_160900_8013FB50[8] = {
    { { { TASK_BODY_NONE, 192 } }, func_actor_160900_8013418C, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_actor_160900_801344D8, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_actor_160900_801343E4, { .value = 0 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_160900_80132C08, { .model = &_gActor160900KyleMadiganBody } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_160900_80132A14, { .model = &_gActor160900KyleMadiganGun } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_160900_80132A14, { .model = &_gActor160900KyleMadiganLeft } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_160900_80132A14, { .model = &_gActor160900KyleMadiganHandRight } },
    { { { TASK_BODY_COORD, 192 } }, func_actor_160900_80132E80, { .value = 0 } },
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

static s32         func_actor_160900_801326EC(Task* arg0);
static inline void func_actor_160900_Reseed(Task* arg0, u16 anim);
static s32         func_actor_160900_80132844(Task* arg0);
static inline void func_actor_160900_InitAnim(Task* task, TmdObject* obj);
static inline void func_actor_160900_SetAnim(Task* task, u16 anim);
static inline void func_actor_160900_SetAnimZ(Task* task, u16 anim);
static void        func_actor_160900_80133238(Task* arg0);
static void        func_actor_160900_8013358C(Task* arg0);
static void        func_actor_160900_80133758(SVECTOR* pts);

#include "../../shared/screen_wave.inc.c"

static s32 func_actor_160900_801326EC(Task* arg0)
{
    _Actor160900CutsceneWork* work;
    ActorAnimChainLink*       table;
    ActorAnimChainLink*       entry;
    ActorAnimChainLink*       entry2;
    AnimationPlayRequest      msg;
    u16                       anim;
    u16                       anim2;

    work = arg0->work;
    if (work->player == NULL) {
        return 1;
    }
    table = D_actor_160900_8013F1CC;
    entry = &table[work->playerAnimId];
    if (entry->holdFrames != 0) {
        if (work->playerAnimHold >= entry->holdFrames) {
            if (entry->nextAnimId < 0) {
                return 1;
            }
            anim                     = entry->nextAnimId;
            msg.source.sets          = _gActor160900PlayerAnimationSets;
            work->playerAnimId       = anim;
            msg.animationId          = anim;
            msg.blend                = ANIMATION_BLEND_INTERPOLATE;
            msg.blendFrames          = 0xA;
            msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
            TASK_MESSAGE_DISPATCH_POINTER(work->player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg, 0);
            work->playerAnimHold = 0;
        } else {
            work->playerAnimHold += 1;
        }
    } else {
        if (taskMessageDispatch(work->player, ANIMATION_MESSAGE_IS_PLAYING, 0, 0) != 0) {
            return 0;
        }
        entry2 = &D_actor_160900_8013F1CC[work->playerAnimId];
        if (entry2->nextAnimId < 0) {
            return 1;
        }
        work = arg0->work;
        if (work->player != NULL) {
            anim2                    = entry2->nextAnimId;
            msg.source.sets          = _gActor160900PlayerAnimationSets;
            work->playerAnimId       = anim2;
            msg.animationId          = anim2;
            msg.blend                = ANIMATION_BLEND_INTERPOLATE;
            msg.blendFrames          = 0xA;
            msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
            TASK_MESSAGE_DISPATCH_POINTER(work->player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg, 0);
            work->playerAnimHold = 0;
        }
    }
    return 0;
}
/// Restarts the hold counter of the model's current clip.
///
/// FITTED: the two arms are the same store. The image has one `sh zero` here
/// and no test, but what follows it was compiled as a new basic block: the
/// blend loop's `li 1` and `li 10` stay below the two stores (in one block
/// sched1 lifts both above them), which is also what lets the delay-slot pass
/// reach the copy of the clip id and make the `animId` store read the
/// argument register. A branch whose arms jump2 merges leaves exactly that.
/// The original condition is unknown; `work` is used because testing it costs
/// no instruction. Same shape as `_actor560800ResetAnimHold`.
static inline void func_actor_160900_ResetAnimHold(_Actor160900KyleModelWork* work)
{
    if (work != NULL) {
        work->animHold = 0;
    } else {
        work->animHold = 0;
    }
}

static inline void func_actor_160900_Reseed(Task* arg0, u16 anim)
{
    _Actor160900KyleModelWork* work;
    u16                        i;

    work         = arg0->work;
    work->animId = anim;
    func_actor_160900_ResetAnimHold(work);
    for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
        animationSeekSlotWithBlend(&work->rig.anim, i, anim, 0, 0xA);
    }
}

static s32 func_actor_160900_80132844(Task* arg0)
{
    _Actor160900KyleModelWork* work;
    ActorAnimChainLink*        table;
    u16                        i;
    u16                        done;

    work = arg0->work;
    if (arg0->extra.tmd->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) {
        return 0;
    }
    for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
        animationTickSlot(&work->rig.anim, i);
    }
    i    = 1;
    done = 1;
    for (; i < ARRAY_SIZE(work->rig.slots); i++) {
        if (!(work->rig.slots[i].status.fields.flags & ANIMATION_SLOT_SETTLED)) {
            done = 0;
            break;
        }
    }
    table = work->animChain;
    if (table[work->animId].holdFrames != 0) {
        if (work->animHold >= table[work->animId].holdFrames) {
            if (table[work->animId].nextAnimId >= 0) {
                func_actor_160900_Reseed(arg0, table[work->animId].nextAnimId);
            } else {
                return 1;
            }
        } else {
            work->animHold++;
        }
    } else if (done) {
        if (table[work->animId].nextAnimId >= 0) {
            func_actor_160900_Reseed(arg0, table[work->animId].nextAnimId);
        } else {
            return 1;
        }
    }
    return 0;
}

void func_actor_160900_80132A14(Task* arg0)
{
    VECTOR pos;

    if (arg0->state == 0) {
        TmdObject*                 tmd    = arg0->extra.tmd;
        Task*                      parent = arg0->spawnArg2.pointer;
        GfxCoord*                  coord  = tmd->coords;
        _Actor160900KyleModelWork* work;
        _Actor160900KyleModelWork* block;
        AreaPlacement*             place;
        u8                         id;

        block      = memMalloc(sizeof(*block), false);
        arg0->work = block;
        if (block == NULL) {
            taskKill(arg0);
            return;
        }
        work = block;
        switch (arg0->spawnArg1.value) {
            case 0:
                coord->parent = &parent->extra.tmd->coords[12];
                break;
            case 1:
            case 2:
                coord->parent = &parent->extra.tmd->coords[8];
                break;
        }
        memFillBytes(arg0->work, 0, sizeof(*work));
        tmd->lightMtx = &work->light;
        tmd->colorMtx = &work->color;
        if (arg0->spawnArg1.value < 2) {
            place = areaGetVariant(&gGameSession->location.loc)->placements;
            id    = place->entryId;
            while (id != AREA_PLACEMENT_END) {
                if (id == 0x65) {
                    break;
                }
                place++;
                id = place->entryId;
            }
            tmdSetTextureOffsets(arg0->extra.tmd, place->texturePageOffset, place->clutRowOffset);
        } else if (arg0->spawnArg1.value == 2) {
            tmdSetTextureOffsets(arg0->extra.tmd, 0, 0);
        }
        taskReparent(parent, arg0);
        arg0->msgTable = D_actor_160900_8013F200;
        arg0->state   += 1;
        return;
    } else {
        TmdObject* obj = arg0->extra.tmd;

        pos.vx = obj->coords->workm.t[0];
        pos.vy = arg0->extra.tmd->coords->workm.t[1];
        pos.vz = arg0->extra.tmd->coords->workm.t[2];
        worldCoordSetModelLighting(obj, &pos, 0, 3);
    }
}

/// Clears the model's clip id and hold counter and resets slots 1-19 to unit rate.
static inline void func_actor_160900_ResetAnimSlots(Task* task)
{
    _Actor160900KyleModelWork* work;
    u16                        i;

    work           = task->work;
    work->animId   = 0;
    work->animHold = 0;
    for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
        work->rig.slots[i].rate = ANIMATION_RATE_ONE;
        animationResetSlot(&work->rig.anim, i, 0);
    }
}

/// Binds the child's animation context and resets slots 1-19. Taking the model
/// as a parameter is what schedules its load after the work-block load.
static inline void func_actor_160900_InitAnim(Task* task, TmdObject* obj)
{
    _Actor160900KyleModelWork* work;

    work = task->work;
    animationInitContext(&work->rig.anim, D_actor_160900_8013F1C4, obj, work->rig.poses, work->rig.slots);
    work->animChain = D_actor_160900_8013F1F8;
    func_actor_160900_ResetAnimSlots(task);
}

void func_actor_160900_80132C08(Task* task)
{
    TmdObject*                 obj;
    TmdObject*                 obj2;
    GfxCoord*                  coord;
    _Actor160900KyleModelWork* work;
    AreaPlacement*             place;
    VECTOR                     pos;
    s32                        failed;

    if (task->state == 0) {
        obj        = task->extra.tmd;
        coord      = obj->coords;
        work       = memMalloc(sizeof(*work), false);
        task->work = work;
        if (work == NULL) {
            failed = 1;
        } else {
            coord->parent = &gGfxViewCoord;
            memFillBytes(task->work, 0, sizeof(*work));
            obj->lightMtx  = &work->light;
            obj->colorMtx  = &work->color;
            obj->flags    |= (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            task->msgTable = D_actor_160900_8013F200;
            place          = areaGetVariant(&gGameSession->location.loc)->placements;
            while (place->entryId != AREA_PLACEMENT_END && place->entryId != 0x65) {
                place++;
            }
            tmdSetTextureOffsets(task->extra.tmd, place->texturePageOffset, place->clutRowOffset);
            taskReparent(D_actor_160900_8013FBB4, task);
            failed = 0;
        }
        if ((u16)failed) {
            taskKill(task);
            return;
        }
        func_actor_160900_InitAnim(task, task->extra.tmd);
        task->state++;
    }
    func_actor_160900_80132844(task);
    if (gGameSession->location.loc.view == 0x2E) {
        gfxRotMatrixZ(&task->extra.tmd->coords[18].coord, 0x800, GRAPHICS_ROTATION_REPLACE);
    } else {
        gfxRotMatrixX(&task->extra.tmd->coords[18].coord, 0x79C, GRAPHICS_ROTATION_REPLACE);
    }
    obj2   = task->extra.tmd;
    pos.vx = obj2->coords->workm.t[0];
    pos.vy = task->extra.tmd->coords->workm.t[1];
    pos.vz = task->extra.tmd->coords->workm.t[2];
    worldCoordSetModelLighting(obj2, &pos, 0, 3);
}

void func_actor_160900_80132E80(Task* task)
{
    s16                        xs[4];
    s16                        ys[4];
    SVECTOR                    origin;
    s32                        sxy;
    s32                        otz;
    GfxCoord*                  coord;
    _Actor160900LightQuadWork* work;
    POLY_G4*                   poly;
    DR_TPAGE*                  tp;
    s16                        i;

    coord = task->extra.coordBody->coord;
    work  = task->work;
    actorRenderComposeCoord(coord);
    gte_SetTransMatrix(&coord->workm);
    gte_SetRotMatrix(&coord->workm);
    origin.vz = 0;
    origin.vy = 0;
    origin.vx = 0;
    gte_ldv0(&origin);
    gte_rtps();
    gte_stsxy(&sxy);
    gte_stszotz(&otz);
    for (i = 0; i < ARRAY_SIZE(work->corners); i++) {
        gte_ldv0(&work->corners[i]);
        gte_rtps();
        gte_stsxy(&sxy);
        xs[i] = sxy;
        ys[i] = sxy >> 16;
    }

    poly           = gGpuPrimCursor;
    gGpuPrimCursor = poly + 1;
    setlen(poly, 8);
    setcode(poly, 0x3A);
    poly->r0 = 0;
    poly->g0 = 0;
    poly->b0 = 0;
    poly->r1 = 0;
    poly->g1 = 0;
    poly->b1 = 0;
    switch (task->spawnArg1.value) {
        case 0:
            poly->r0 = 0;
            poly->g0 = 0;
            poly->b0 = 0;
            poly->r1 = 0;
            poly->g1 = 0;
            poly->b1 = 0;
            poly->r2 = 0;
            poly->g2 = 0;
            poly->b2 = 0;
            poly->r3 = 0xFF;
            poly->g3 = 0xFF;
            poly->b3 = 0xFF;
            break;
        case 1:
            poly->r0 = 0;
            poly->g0 = 0;
            poly->b0 = 0;
            poly->r1 = 0;
            poly->g1 = 0;
            poly->b1 = 0;
            poly->r3 = 0;
            poly->g3 = 0;
            poly->b3 = 0;
            poly->r2 = 0xFF;
            poly->g2 = 0xFF;
            poly->b2 = 0xFF;
            break;
        case 2:
            poly->r0 = 0;
            poly->g0 = 0;
            poly->b0 = 0;
            poly->r1 = 0xFF;
            poly->g1 = 0xFF;
            poly->b1 = 0xFF;
            poly->r2 = 0;
            poly->g2 = 0;
            poly->b2 = 0;
            poly->r3 = 0xFF;
            poly->g3 = 0xFF;
            poly->b3 = 0xFF;
            break;
        case 3:
            poly->r0 = 0xFF;
            poly->g0 = 0xFF;
            poly->b0 = 0xFF;
            poly->r1 = 0;
            poly->g1 = 0;
            poly->b1 = 0;
            poly->r2 = 0xFF;
            poly->g2 = 0xFF;
            poly->b2 = 0xFF;
            poly->r3 = 0;
            poly->g3 = 0;
            poly->b3 = 0;
            break;
        case 4:
            poly->r0 = 0;
            poly->g0 = 0;
            poly->b0 = 0;
            poly->r1 = 0;
            poly->g1 = 0;
            poly->b1 = 0;
            poly->r2 = 0xFF;
            poly->g2 = 0xFF;
            poly->b2 = 0xFF;
            poly->r3 = 0xFF;
            poly->g3 = 0xFF;
            poly->b3 = 0xFF;
            break;
        case 5:
            poly->r0 = 0xFF;
            poly->g0 = 0xFF;
            poly->b0 = 0xFF;
            poly->r1 = 0xFF;
            poly->g1 = 0xFF;
            poly->b1 = 0xFF;
            poly->r2 = 0xFF;
            poly->g2 = 0xFF;
            poly->b2 = 0xFF;
            poly->r3 = 0xFF;
            poly->g3 = 0xFF;
            poly->b3 = 0xFF;
            break;
    }
    poly->x0 = xs[0];
    poly->y0 = ys[0];
    poly->x1 = xs[1];
    poly->y1 = ys[1];
    poly->x2 = xs[2];
    poly->y2 = ys[2];
    poly->x3 = xs[3];
    poly->y3 = ys[3];
    addPrim(&gGpuCurrentOt[otz >> 4], poly);
    tp             = gGpuPrimCursor;
    gGpuPrimCursor = tp + 1;
    setlen(tp, 1);
    tp->code[0] = 0xE1000220;
    addPrim(&gGpuCurrentOt[otz >> 4], tp);
}

static inline void func_actor_160900_SetAnim(Task* task, u16 anim)
{
    _Actor160900CutsceneWork* work;
    AnimationPlayRequest      msg;
    AnimationPlayRequest*     p;

    work = task->work;
    p    = &msg;
    if (work->player != NULL) {
        p->source.sets          = _gActor160900PlayerAnimationSets;
        work->playerAnimId      = anim;
        p->animationId          = anim;
        p->blend                = ANIMATION_BLEND_INTERPOLATE;
        p->blendFrames          = 10;
        p->enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
        TASK_MESSAGE_DISPATCH_POINTER(work->player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, p, 0);
        work->playerAnimHold = 0;
    }
}

static inline void func_actor_160900_SetAnimZ(Task* task, u16 anim)
{
    _Actor160900CutsceneWork* work;
    AnimationPlayRequest      msg;
    AnimationPlayRequest*     p;

    work = task->work;
    p    = &msg;
    if (work->player != NULL) {
        p->source.sets          = _gActor160900PlayerAnimationSets;
        work->playerAnimId      = anim;
        p->animationId          = anim;
        p->blend                = ANIMATION_BLEND_RESET;
        p->blendFrames          = 0;
        p->enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
        TASK_MESSAGE_DISPATCH_POINTER(work->player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, p, 0);
        work->playerAnimHold = 0;
    }
}

/// Advances Aya's clip chain, then acts on `_Actor160900CutsceneWork::playerCue`
/// and clears it, except for the walk to the mark, which stays posted.
static void func_actor_160900_80133238(Task* arg0)
{
    _Actor160900CutsceneWork* work;
    union {
        AnimationPlayRequest animation;
        VECTOR3              destination;
    } message;

    _Actor160900CutsceneWork* messageWork;
    s32                       bankIndex;
    s32                       weaponId;

    work = arg0->work;
    func_actor_160900_801326EC(arg0);
    switch (work->playerCue.id) {
        case 0:
            break;
        case ACTOR_160900_PLAYER_CUE_TAKE_PLACE:
            TASK_MESSAGE_DISPATCH_POINTER(work->player, GAME_ACTOR_MESSAGE_PLACE, D_actor_160900_8013F210, 0);
            messageWork = arg0->work;
            if (messageWork->player != NULL) {
                message.animation.source.sets          = _gActor160900PlayerAnimationSets;
                messageWork->playerAnimId              = 10;
                message.animation.animationId          = 10;
                message.animation.blend                = ANIMATION_BLEND_RESET;
                message.animation.blendFrames          = 0;
                message.animation.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                TASK_MESSAGE_DISPATCH_POINTER(messageWork->player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &message.animation, 0);
                messageWork->playerAnimHold = 0;
            }
            break;
        case ACTOR_160900_PLAYER_CUE_CLIP_1:
            messageWork = arg0->work;
            if (messageWork->player != NULL) {
                message.animation.source.sets          = _gActor160900PlayerAnimationSets;
                messageWork->playerAnimId              = 1;
                message.animation.animationId          = 1;
                message.animation.blend                = ANIMATION_BLEND_RESET;
                message.animation.blendFrames          = 0;
                message.animation.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                TASK_MESSAGE_DISPATCH_POINTER(messageWork->player, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &message.animation, 0);
                messageWork->playerAnimHold = 0;
            }
            break;
        case ACTOR_160900_PLAYER_CUE_WALK_TO_MARK:
            if (work->playerCue.step == 0) {
                weaponId = gPlayerStatus.weapon;
                if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) {
                    bankIndex = weaponId + 1;
                } else {
                    bankIndex = weaponId + 0x22;
                }
                message.animation.source.index         = bankIndex;
                message.animation.animationId          = 1;
                message.animation.blend                = ANIMATION_BLEND_RESET;
                message.animation.blendFrames          = 0;
                message.animation.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;

                TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &message.animation, 0);
                TASK_MESSAGE_DISPATCH_POINTER(work->player, GAME_ACTOR_MESSAGE_PLACE, D_actor_160900_8013F228, 0);
                message.destination.vx = -0x7D0;
                message.destination.vy = 0;
                message.destination.vz = 0xC80;
                TASK_MESSAGE_DISPATCH_POINTER(work->player, 0x3FB, &message.destination, 0);
                work->playerCue.step++;
            }
            return;
        case ACTOR_160900_PLAYER_CUE_WAVE_START:
            work->wave.span  = 8;
            work->wave.scale = 0x80;
            taskSpawnFromTable(D_actor_160900_8013F17C, 0, 0, &work->wave);
            work->playerCue.id = 0;
            return;
        case ACTOR_160900_PLAYER_CUE_WAVE_END:
            work->wave.state = SCREEN_WAVE_RAMP_FINISHED;
            break;
        case ACTOR_160900_PLAYER_CUE_CLIP_2:
            func_actor_160900_SetAnimZ(arg0, 2);
            break;
        case ACTOR_160900_PLAYER_CUE_CLIP_6:
            func_actor_160900_SetAnim(arg0, 6);
            break;
        case ACTOR_160900_PLAYER_CUE_CLIP_7:
            func_actor_160900_SetAnim(arg0, 7);
            break;
        case ACTOR_160900_PLAYER_CUE_CLIP_8:
            func_actor_160900_SetAnim(arg0, 8);
            break;
        case ACTOR_160900_PLAYER_CUE_CLIP_9:
            func_actor_160900_SetAnim(arg0, 9);
            break;
    }
    work->playerCue.id = 0;
}

static void func_actor_160900_8013358C(Task* arg0)
{
    _Actor160900CutsceneWork*  work;
    _Actor160900KyleModelWork* child;
    SVECTOR                    ofs;
    SVECTOR                    ofs2;
    s32                        i;

    work = arg0->work;
    switch (work->kyleCue.id) {
        case 0:
            break;
        case ACTOR_160900_KYLE_CUE_HIDE_AND_PLACE:
            taskMessageDispatch(work->kyle, ACTOR_MESSAGE_SET_MODEL_DRAW, 2, 0);
            TASK_MESSAGE_DISPATCH_POINTER(work->kyle, ACTOR_MESSAGE_PLACE, D_actor_160900_8013F240, 0);
            break;
        case ACTOR_160900_KYLE_CUE_APPEAR:
            taskMessageDispatch(work->kyleGunHand, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
            taskMessageDispatch(work->kyleFreeHand, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
            if (work->kyleGun != NULL) {
                taskMessageDispatch(work->kyleGun, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
            }
            taskMessageDispatch(work->kyle, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
            ofs.vx = -100;
            ofs.vy = 100;
            ofs.vz = -1200;
            effectSpawn(EFFECT_GROUND_DECAL, work->kyle->extra.tmd->coords, 0x20000100, &ofs);
            break;
        case ACTOR_160900_KYLE_CUE_CLIP_1:
            child           = work->kyle->work;
            child->animId   = 1;
            child->animHold = 0;
            do {
            } while (0);
            for (i = 1; (u16)i < ARRAY_SIZE(child->rig.slots); i++) {
                animationSeekSlotWithBlend(&child->rig.anim, (u16)i, 1, 0, 10);
            }
            break;
        case ACTOR_160900_KYLE_CUE_GROUND_DECAL:
            ofs2.vx = -200;
            ofs2.vy = 100;
            ofs2.vz = -400;
            effectSpawn(EFFECT_GROUND_DECAL, work->kyle->extra.tmd->coords, 0x20000100, &ofs2);
            break;
    }
    work->kyleCue.id = 0;
}

/// Spawn effect 0x601B4 at each point of a `pad == -1` terminated list, x
/// jittered by up to +-700; runs one frame in eight.
static void func_actor_160900_80133758(SVECTOR* pts)
{
    SVECTOR pos;
    s32     x;
    u32     seed;
    s32     flags;

    if (!(gDisplayState.animFrame & 7) && pts->pad != -1) {
        flags = 0x81203400;
        do {
            seed            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState = seed;
            x               = pts->vx + (((seed >> 16) & 1) ? ((gRandomLcgState = seed * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 7
                                                            : -(((gRandomLcgState = seed * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 7)) *
                              100;
            pos.vx = x;
            pos.vy = pts->vy;
            pos.vz = pts->vz;
            effectSpawn(EFFECT_1B4, NULL, flags, &pos);
            pts++;
        } while (pts->pad != -1);
    }
}

void func_actor_160900_80133880(void)
{
    _Actor160900CutsceneWork*  data;
    _Actor160900LightQuadWork* alloc;
    _Actor160900LightQuadWork* work;
    Task*                      task;

    data                = D_actor_160900_8013FBB4->work;
    task                = taskSpawnFromTable(D_actor_160900_8013FB50, 7, 0, 0);
    data->lightQuads[0] = task;
    if (task == NULL) {
        return;
    }
    alloc      = memCalloc(sizeof(*alloc), false);
    task->work = alloc;
    if (alloc == NULL) {
        taskKill(task);
        return;
    }
    work = alloc;
    memFillBytes(work, 0, sizeof(*work));
    task->extra.tmd->coords->parent     = &gGfxViewCoord;
    task->extra.tmd->coords->coord.t[0] = 0x1770;
    task->extra.tmd->coords->coord.t[1] = 0x3E8;
    task->extra.tmd->coords->coord.t[2] = 0xBB8;
    work->corners[0].vx                 = 0;
    work->corners[0].vy                 = -0x5DC;
    work->corners[0].vz                 = 0x3E8;
    work->corners[1].vx                 = 0;
    work->corners[1].vy                 = -0x5DC;
    work->corners[1].vz                 = 0;
    work->corners[2].vx                 = 0;
    work->corners[2].vy                 = 0;
    work->corners[2].vz                 = 0x3E8;
    work->corners[3].vx                 = 0;
    work->corners[3].vy                 = 0;
    work->corners[3].vz                 = 0;
    task                                = taskSpawnFromTable(D_actor_160900_8013FB50, 7, 1, 0);
    data->lightQuads[1]                 = task;
    if (task == NULL) {
        return;
    }
    alloc      = memCalloc(sizeof(*alloc), false);
    task->work = alloc;
    if (alloc == NULL) {
        taskKill(task);
        return;
    }
    work = alloc;
    memFillBytes(work, 0, sizeof(*work));
    task->extra.tmd->coords->parent     = &gGfxViewCoord;
    task->extra.tmd->coords->coord.t[0] = 0x1770;
    task->extra.tmd->coords->coord.t[1] = 0x3E8;
    task->extra.tmd->coords->coord.t[2] = 0xBB8;
    work->corners[0].vx                 = 0;
    work->corners[0].vy                 = -0x5DC;
    work->corners[0].vz                 = 0;
    work->corners[1].vx                 = 0;
    work->corners[1].vy                 = -0x5DC;
    work->corners[1].vz                 = -0x3E8;
    work->corners[2].vx                 = 0;
    work->corners[2].vy                 = 0;
    work->corners[2].vz                 = 0;
    work->corners[3].vx                 = 0;
    work->corners[3].vy                 = 0;
    work->corners[3].vz                 = -0x3E8;
}
void func_actor_160900_80133A84(void)
{
    _Actor160900CutsceneWork*  data;
    _Actor160900LightQuadWork* alloc;
    _Actor160900LightQuadWork* work;
    Task*                      task;

    data                = D_actor_160900_8013FBB4->work;
    task                = taskSpawnFromTable(D_actor_160900_8013FB50, 7, 5, 0);
    data->lightQuads[0] = task;
    if (task == NULL) {
        return;
    }
    alloc      = memCalloc(sizeof(*alloc), false);
    task->work = alloc;
    if (alloc == NULL) {
        taskKill(task);
        return;
    }
    work = alloc;
    memFillBytes(work, 0, sizeof(*work));
    task->extra.tmd->coords->parent     = &gGfxViewCoord;
    task->extra.tmd->coords->coord.t[0] = 0x1770;
    task->extra.tmd->coords->coord.t[1] = 0x1F4;
    task->extra.tmd->coords->coord.t[2] = 0xA8C;
    work->corners[0].vx                 = 0;
    work->corners[0].vy                 = -0x3E8;
    work->corners[0].vz                 = 0x1F4;
    work->corners[1].vx                 = 0;
    work->corners[1].vy                 = -0x3E8;
    work->corners[1].vz                 = -0x1F4;
    work->corners[2].vx                 = 0;
    work->corners[2].vy                 = 0;
    work->corners[2].vz                 = 0x1F4;
    work->corners[3].vx                 = 0;
    work->corners[3].vy                 = 0;
    work->corners[3].vz                 = -0x1F4;
    task                                = taskSpawnFromTable(D_actor_160900_8013FB50, 7, 2, 0);
    data->lightQuads[1]                 = task;
    if (task == NULL) {
        return;
    }
    alloc      = memCalloc(sizeof(*alloc), false);
    task->work = alloc;
    if (alloc == NULL) {
        taskKill(task);
        return;
    }
    work = alloc;
    memFillBytes(work, 0, sizeof(*work));
    task->extra.tmd->coords->parent     = &gGfxViewCoord;
    task->extra.tmd->coords->coord.t[0] = 0x1770;
    task->extra.tmd->coords->coord.t[1] = 0x1F4;
    task->extra.tmd->coords->coord.t[2] = 0xA8C;
    work->corners[0].vx                 = 0;
    work->corners[0].vy                 = -0x3E8;
    work->corners[0].vz                 = 0x3E8;
    work->corners[1].vx                 = 0;
    work->corners[1].vy                 = -0x3E8;
    work->corners[1].vz                 = 0x1F4;
    work->corners[2].vx                 = 0;
    work->corners[2].vy                 = 0;
    work->corners[2].vz                 = 0x3E8;
    work->corners[3].vx                 = 0;
    work->corners[3].vy                 = 0;
    work->corners[3].vz                 = 0x1F4;
    task                                = taskSpawnFromTable(D_actor_160900_8013FB50, 7, 3, 0);
    data->lightQuads[2]                 = task;
    if (task == NULL) {
        return;
    }
    alloc      = memCalloc(sizeof(*alloc), false);
    task->work = alloc;
    if (alloc == NULL) {
        taskKill(task);
        return;
    }
    work = alloc;
    memFillBytes(work, 0, sizeof(*work));
    task->extra.tmd->coords->parent     = &gGfxViewCoord;
    task->extra.tmd->coords->coord.t[0] = 0x1770;
    task->extra.tmd->coords->coord.t[1] = 0x1F4;
    task->extra.tmd->coords->coord.t[2] = 0xA8C;
    work->corners[0].vx                 = 0;
    work->corners[0].vy                 = -0x3E8;
    work->corners[0].vz                 = -0x1F4;
    work->corners[1].vx                 = 0;
    work->corners[1].vy                 = -0x3E8;
    work->corners[1].vz                 = -0x3E8;
    work->corners[2].vx                 = 0;
    work->corners[2].vy                 = 0;
    work->corners[2].vz                 = -0x1F4;
    work->corners[3].vx                 = 0;
    work->corners[3].vy                 = 0;
    work->corners[3].vz                 = -0x3E8;
    task                                = taskSpawnFromTable(D_actor_160900_8013FB50, 7, 4, 0);
    data->lightQuads[3]                 = task;
    if (task == NULL) {
        return;
    }
    alloc      = memCalloc(sizeof(*alloc), false);
    task->work = alloc;
    if (alloc == NULL) {
        taskKill(task);
        return;
    }
    work = alloc;
    memFillBytes(work, 0, sizeof(*work));
    task->extra.tmd->coords->parent     = &gGfxViewCoord;
    task->extra.tmd->coords->coord.t[0] = 0x1770;
    task->extra.tmd->coords->coord.t[1] = -0x1F4;
    task->extra.tmd->coords->coord.t[2] = 0xA8C;
    work->corners[0].vx                 = 0;
    work->corners[0].vy                 = -0x3E8;
    work->corners[0].vz                 = 0x1F4;
    work->corners[1].vx                 = 0;
    work->corners[1].vy                 = -0x3E8;
    work->corners[1].vz                 = -0x1F4;
    work->corners[2].vx                 = 0;
    work->corners[2].vy                 = 0;
    work->corners[2].vz                 = 0x1F4;
    work->corners[3].vx                 = 0;
    work->corners[3].vy                 = 0;
    work->corners[3].vz                 = -0x1F4;
    task                                = taskSpawnFromTable(D_actor_160900_8013FB50, 7, 0, 0);
    data->lightQuads[4]                 = task;
    if (task == NULL) {
        return;
    }
    alloc      = memCalloc(sizeof(*alloc), false);
    task->work = alloc;
    if (alloc == NULL) {
        taskKill(task);
        return;
    }
    work = alloc;
    memFillBytes(work, 0, sizeof(*work));
    task->extra.tmd->coords->parent     = &gGfxViewCoord;
    task->extra.tmd->coords->coord.t[0] = 0x1770;
    task->extra.tmd->coords->coord.t[1] = -0x1F4;
    task->extra.tmd->coords->coord.t[2] = 0xA8C;
    work->corners[0].vx                 = 0;
    work->corners[0].vy                 = -0x3E8;
    work->corners[0].vz                 = 0x3E8;
    work->corners[1].vx                 = 0;
    work->corners[1].vy                 = -0x3E8;
    work->corners[1].vz                 = 0x1F4;
    work->corners[2].vx                 = 0;
    work->corners[2].vy                 = 0;
    work->corners[2].vz                 = 0x3E8;
    work->corners[3].vx                 = 0;
    work->corners[3].vy                 = 0;
    work->corners[3].vz                 = 0x1F4;
    task                                = taskSpawnFromTable(D_actor_160900_8013FB50, 7, 1, 0);
    data->lightQuads[5]                 = task;
    if (task == NULL) {
        return;
    }
    alloc      = memCalloc(sizeof(*alloc), false);
    task->work = alloc;
    if (alloc == NULL) {
        taskKill(task);
        return;
    }
    work = alloc;
    memFillBytes(work, 0, sizeof(*work));
    task->extra.tmd->coords->parent     = &gGfxViewCoord;
    task->extra.tmd->coords->coord.t[0] = 0x1770;
    task->extra.tmd->coords->coord.t[1] = -0x1F4;
    task->extra.tmd->coords->coord.t[2] = 0xA8C;
    work->corners[0].vx                 = 0;
    work->corners[0].vy                 = -0x3E8;
    work->corners[0].vz                 = -0x1F4;
    work->corners[1].vx                 = 0;
    work->corners[1].vy                 = -0x3E8;
    work->corners[1].vz                 = -0x3E8;
    work->corners[2].vx                 = 0;
    work->corners[2].vy                 = 0;
    work->corners[2].vz                 = -0x1F4;
    work->corners[3].vx                 = 0;
    work->corners[3].vy                 = 0;
    work->corners[3].vz                 = -0x3E8;
}
void func_actor_160900_80133F90(void)
{
    _Actor160900CutsceneWork*  data;
    _Actor160900LightQuadWork* alloc;
    _Actor160900LightQuadWork* work;
    Task*                      task;

    data                = D_actor_160900_8013FBB4->work;
    task                = taskSpawnFromTable(D_actor_160900_8013FB50, 7, 0, 0);
    data->lightQuads[0] = task;
    if (task == NULL) {
        return;
    }
    alloc      = memCalloc(sizeof(*alloc), false);
    task->work = alloc;
    if (alloc == NULL) {
        taskKill(task);
        return;
    }
    work = alloc;
    memFillBytes(work, 0, sizeof(*work));
    task->extra.tmd->coords->parent     = &gGfxViewCoord;
    task->extra.tmd->coords->coord.t[0] = 0x1770;
    task->extra.tmd->coords->coord.t[1] = 0;
    task->extra.tmd->coords->coord.t[2] = 0xBB8;
    work->corners[0].vx                 = 0;
    work->corners[0].vy                 = -0x5DC;
    work->corners[0].vz                 = 0x3E8;
    work->corners[1].vx                 = 0;
    work->corners[1].vy                 = -0x5DC;
    work->corners[1].vz                 = 0;
    work->corners[2].vx                 = 0;
    work->corners[2].vy                 = 0;
    work->corners[2].vz                 = 0x3E8;
    work->corners[3].vx                 = 0;
    work->corners[3].vy                 = 0;
    work->corners[3].vz                 = 0;
    task                                = taskSpawnFromTable(D_actor_160900_8013FB50, 7, 1, 0);
    data->lightQuads[1]                 = task;
    if (task == NULL) {
        return;
    }
    alloc      = memCalloc(sizeof(*alloc), false);
    task->work = alloc;
    if (alloc == NULL) {
        taskKill(task);
        return;
    }
    work = alloc;
    memFillBytes(work, 0, sizeof(*work));
    task->extra.tmd->coords->parent     = &gGfxViewCoord;
    task->extra.tmd->coords->coord.t[0] = 0x1770;
    task->extra.tmd->coords->coord.t[1] = 0;
    task->extra.tmd->coords->coord.t[2] = 0xBB8;
    work->corners[0].vx                 = 0;
    work->corners[0].vy                 = -0x5DC;
    work->corners[0].vz                 = 0;
    work->corners[1].vx                 = 0;
    work->corners[1].vy                 = -0x5DC;
    work->corners[1].vz                 = -0x3E8;
    work->corners[2].vx                 = 0;
    work->corners[2].vy                 = 0;
    work->corners[2].vz                 = 0;
    work->corners[3].vx                 = 0;
    work->corners[3].vy                 = 0;
    work->corners[3].vz                 = -0x3E8;
}
void func_actor_160900_8013418C(Task* arg0)
{
    _Actor160900CutsceneWork* work;
    _Actor160900CutsceneWork* data;

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
                D_actor_160900_8013FBB4 = arg0;
                work->kyle              = taskSpawnFromTable(D_actor_160900_8013FB50, 3, 0, arg0);
                work->kyleGunHand       = taskSpawnFromTable(D_actor_160900_8013FB50, 5, 1, work->kyle);
                work->kyleFreeHand      = taskSpawnFromTable(D_actor_160900_8013FB50, 6, 0, work->kyle);
            }
            Gp_CapFile = 0;
            capSelectLoadedFile(3);
            capSetTexturePage(0x180, 0);
            arg0->state += 1;
            return;
        case 1:
            evsStartScriptWithSkip(D_actor_160900_8013F538, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_actor_160900_8013FAA8);
            arg0->state += 1;
            break;
        case 2:
            if (gGameSession->eventState == 0) {
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = 0x1E;
                taskRequestKill(arg0, 0);
            }
            break;
    }
    func_actor_160900_80133238(arg0);
    func_actor_160900_8013358C(arg0);
    data = arg0->work;
    switch (data->effectCue.id) {
        case 1:
            func_actor_160900_80133758(D_actor_160900_8013F258);
            break;
        case 2:
            func_actor_160900_80133758(D_actor_160900_8013F2E0);
            break;
        case 3:
            func_actor_160900_80133758(D_actor_160900_8013F3B0);
            break;
        case 4:
            func_actor_160900_80133758(D_actor_160900_8013F400);
            break;
        case 5:
            func_actor_160900_80133758(D_actor_160900_8013F458);
            break;
        case 0:
        default:
            data->effectCue.id = 0;
            break;
    }
}

void func_actor_160900_801343E4(Task* arg0)
{
    ScreenFadeWork* work;
    ScreenFadeWork* alloc;

    work = arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = memMalloc(sizeof(*alloc), false);
            arg0->work = alloc;
            if (alloc == NULL) {
                taskKill(arg0);
                return;
            }
            work         = alloc;
            work->b      = 0;
            work->g      = 0;
            work->r      = 0;
            arg0->state += 1;
            /* fallthrough */
        case 1:
            fadeDrawOverlay(work->r, work->g, work->r, GPU_BLEND_SUBTRACT);
            work->r += (u16)arg0->spawnArg1.value;
            work->g += (u16)arg0->spawnArg1.value;
            work->b += (u16)arg0->spawnArg1.value;
            if (work->r >= 0x100) {
                work->b = 0xFF;
                work->g = 0xFF;
                work->r = 0xFF;
            }
            break;
    }
}
void func_actor_160900_801344D8(Task* arg0)
{
    ScreenFadeWork* work;
    ScreenFadeWork* alloc;

    work = arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = memMalloc(sizeof(*alloc), false);
            arg0->work = alloc;
            if (alloc == NULL) {
                taskKill(arg0);
                return;
            }
            work         = alloc;
            work->b      = 0xFF;
            work->g      = 0xFF;
            work->r      = 0xFF;
            arg0->state += 1;
            break;
        case 3:
            SetDispMask(1);
            /* fallthrough */
        case 1:
        case 2:
            arg0->state += 1;
            break;
        case 4:
            break;
        default:
            return;
    }
    fadeDrawOverlay(work->r, work->g, work->r, GPU_BLEND_SUBTRACT);
    work->r -= (u16)arg0->spawnArg1.value;
    work->g -= (u16)arg0->spawnArg1.value;
    work->b -= (u16)arg0->spawnArg1.value;
    if (work->r < 0) {
        taskKill(arg0);
    }
}
/// Message 0x7D5 handler of `D_actor_160900_8013F200`: argument 1 clears the
/// 0x84 bits of the task's `TmdObject` flags, argument 2 sets them, and any
/// other value does nothing.
void func_actor_160900_801345D0(Task* task, s32 arg1, s32 arg2, s32 arg3)
{
    TmdObject* extra;

    extra = task->extra.tmd;
    switch (arg2) {
        case 0:
            break;
        case 1:
            extra->flags = extra->flags & (u16) ~(TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            return;
        case 2:
            extra->flags = extra->flags | (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            return;
    }
}

#include "../../shared/actor_messages_place_ypr.inc.c"

/// Spawns `func_actor_160900_801344D8`, entry 1 of `D_actor_160900_8013FB50`,
/// with `arg0` as its first spawn argument.
void func_actor_160900_801346B0(s32 arg0)
{
    taskSpawnFromTable(D_actor_160900_8013FB50, 1, arg0, 0);
}

/// Spawns the fade task `func_actor_160900_801343E4`, entry 2 of
/// `D_actor_160900_8013FB50`, with `arg0` as its first spawn argument.
void func_actor_160900_801346E0(s32 arg0)
{
    taskSpawnFromTable(D_actor_160900_8013FB50, 2, arg0, 0);
}

void func_actor_160900_80134710(void)
{
    _Actor160900CutsceneWork* work;
    Task*                     task;
    s16                       i;

    work = D_actor_160900_8013FBB4->work;
    for (i = 0; i < ARRAY_SIZE(work->lightQuads); i++) {
        task = work->lightQuads[i];
        if (task != NULL) {
            taskKill(task);
            work->lightQuads[i] = NULL;
        }
    }
}
void func_actor_160900_80134790(s16 arg0)
{
    _Actor160900CutsceneWork* work;

    work                 = D_actor_160900_8013FBB4->work;
    work->playerCue.id   = arg0;
    work->playerCue.step = 0;
}
void func_actor_160900_801347B0(s16 arg0)
{
    _Actor160900CutsceneWork* work;

    work               = D_actor_160900_8013FBB4->work;
    work->kyleCue.id   = arg0;
    work->kyleCue.step = 0;
}

void func_actor_160900_801347D0(s16 arg0)
{
    _Actor160900CutsceneWork* work;

    work                 = D_actor_160900_8013FBB4->work;
    work->effectCue.id   = arg0;
    work->effectCue.step = 0;
}
void func_actor_160900_801347F0(void)
{
    _Actor160900CutsceneWork* work;

    work               = D_actor_160900_8013FBB4->work;
    work->playerCue.id = 0;
    work->kyleCue.id   = 0;
    work->effectCue.id = 0;
    cdCmdCancelScene();
    SetDispMask(1);
}

void func_actor_160900_80134830(void)
{
    cdCmdStageSceneAudioStart();
}

void func_actor_160900_80134850(void)
{
    cdCmdEnqueueScenePlayback();
}

void func_actor_160900_80134870(void)
{
    cdCmdCancelScene();
    streamFinishScene();
}
