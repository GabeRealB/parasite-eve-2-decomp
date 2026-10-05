#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "actors/actor.h"

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
#include "gameplay/player_actor.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

#include "rooms/dryfield_night_main_street.h"
#include "../../shared/screen_fade.h"
#include "../../shared/actor_messages.h"

extern ActorTransform D_actor_136100_8013F334[2];

extern ActorTransform D_actor_136100_8013F304[2];

/// Which of its two scenes the actor stages, held in `_Actor136100Work::scene`.
///
/// The scenes use different placements, scripts and request handlers; the
/// choice is made once, when the actor spawns.
enum {
    ACTOR_136100_SCENE_AFTER_BURNER  = 0, // `GAME_FLAG_BURNER_DEFEATED` is set
    ACTOR_136100_SCENE_BEFORE_BURNER = 1, // `GAME_FLAG_BURNER_DEFEATED` is clear
};

/// Work block of Gary Douglas on Dryfield's main street at night, allocated at
/// its full size, zeroed and kept at `Task::work`.
///
/// The actor directs a scene with three performers: the player, its own body
/// and the companion. For each it keeps the task to address, a request the
/// event script posts for the per-frame handlers to carry out, and the
/// animation last started, which selects the follow-on animation once that one
/// has finished. A request code is 0 when none is pending and is otherwise
/// particular to the scene; posting one restarts its step.
///
/// The tasks of the head and rifle models allocate a block of this type as
/// well and use only its `light` and `color`.
///
/// Nothing accesses the four `pad` runs; their roles are unproven.
typedef struct {
    ActorAnimRig20 rig;                    // Body rig; the actor drives slots 1 to 19
    MATRIX         light;                  // Light matrix the task's model is lit with
    MATRIX         color;                  // Colour matrix the task's model is lit with
    Task*          playerTask;             // Player's task
    Task*          headTask;               // Task of the head-and-hat model, which hangs from rig part 4
    Task*          rifleTask;              // Task of the rifle model: placed in the view after the Burner, hung from rig part 8 before it
    Task*          companionTask;          // Companion's task, NULL when none is registered
    s16            playerRequest;          // Request for the player (0 none)
    s16            playerRequestStep;      // Step of the player request in progress, counted from 0; the head turn counts its steps here too
    s16            playerRequestFrames;    // Frames the current step of the player request has waited
    byte           pad_4CA[0x2];
    s16            bodyRequest;            // Request for the actor's own body (0 none)
    s16            bodyRequestStep;        // Step of the body request in progress, counted from 0
    s16            bodyRequestFrames;      // Frames the current step of the body request has waited
    byte           pad_4D2[0x2];
    s16            companionRequest;       // Request for the companion (0 none)
    s16            companionRequestStep;   // Step of the companion request in progress, counted from 0
    s16            companionRepeatCount;   // Times the repeated sound and animation of a companion request have played
    s16            companionRepeatDelay;   // Frames until that sound and animation play again
    u16            followUpSeen;           // Whether a talk after the opening scene has played (0 the next plays the full script, 1 the short one)
    s16            playerAnimation;        // Animation last requested of the player
    s16            bodyAnimation;          // Animation set the rig last started, an index into the actor's set table
    u16            companionAnimation;     // Animation last requested of the companion, an index into the sets the actor installs on it
    u16            scene;                  // Scene staged (0 `ACTOR_136100_SCENE_AFTER_BURNER`, 1 `ACTOR_136100_SCENE_BEFORE_BURNER`)
    byte           pad_4E6[0x4];
    s16            headYaw;                // Yaw the head turn gives rig part 4 in the scene before the Burner, 4096 to a turn
    s16            playerEquipmentRemoved; // 1 while the player's equipment tasks are killed for a scene, so its end spawns the weapon again
    byte           pad_4EE[0x2];
} _Actor136100Work;
STATIC_ASSERT_SIZEOF(_Actor136100Work, 0x4F0);

/// Set by `func_actor_136100_801348F8` when the cutscene wants the display
/// back on; while it is non-zero the fade task kills itself instead of fading.
extern u16 D_actor_136100_8013F17C;

/// Next-animation table indexed by `playerAnimation` - 0x2F; negative entries end
/// the chain, and live entries are sent as animation ids with 0x2F added.
extern s16 D_actor_136100_8013F1EC[];

/// Next animation indexed by `bodyAnimation`; negative entries skip the restart.
extern s16 D_actor_136100_8013F1FC[];

/// The actor's six-entry task table, spawned from by index: 0 is the actor
/// itself, 2 and 3 the tasks kept in `headTask` / `rifleTask`, 4 the fade-in and
/// 5 the fade-out.
extern TaskDesc D_actor_136100_80140744[];

extern ActorTransform D_actor_136100_8013F3C4;
extern ActorTransform D_actor_136100_8013F3DC;

extern AnimationSet* D_actor_136100_8013F180[8];
extern AnimationSet* D_actor_136100_8013F1A0[13];
extern AnimationSet* D_actor_136100_8013F1D4[6];
extern s16           D_actor_136100_8013F218[];
extern s32           D_actor_136100_8013F224[8];
extern s32           D_actor_136100_8013F244[32];
extern s32           D_actor_136100_8013F2C4[12];
// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry D_actor_136100_8013F2F4[2];
extern ActorTransform   D_actor_136100_8013F364;
extern ActorTransform   D_actor_136100_8013F37C;
extern ActorTransform   D_actor_136100_8013F394;
extern ActorTransform   D_actor_136100_8013F3AC;
extern ActorTransform   D_actor_136100_8013F3F4;
extern ActorTransform   D_actor_136100_8013F40C;
extern ActorTransform   D_actor_136100_8013F424;
extern ActorTransform   D_actor_136100_8013F43C;
extern ActorTransform   D_actor_136100_8013F454;
extern EvsCommand       D_actor_136100_8013F46C[];
extern EvsCommand       D_actor_136100_8013F784[];
extern EvsCommand       D_actor_136100_8013F94C[];
extern EvsCommand       D_actor_136100_8013FAE4[];
extern EvsCommand       D_actor_136100_8013FC64[];
extern EvsCommand       D_actor_136100_8013FD84[];
extern EvsCommand       D_actor_136100_80140114[];
extern EvsCommand       D_actor_136100_801402C4[];
extern EvsCommand       D_actor_136100_801404EC[];
extern EvsCommand       D_actor_136100_8014063C[];
extern Task*            D_actor_136100_8014078C;

static void func_actor_136100_80132748(Task* arg0);
static void func_actor_136100_80133238(Task* arg0);
static void func_actor_136100_80134A18(Task* task);

static AnimationSet _gActor136100Animation09414;
static AnimationSet _gActor136100Animation09754;
static AnimationSet _gActor136100Animation09A3C;
static AnimationSet _gActor136100Animation09CF8;
static AnimationSet _gActor136100Animation09EAC;
static AnimationSet _gActor136100Animation0A100;
static AnimationSet _gActor136100Animation0A310;
static AnimationSet _gActor136100Animation0A5A4;
static AnimationSet _gActor136100Animation0A8B4;
static AnimationSet _gActor136100Animation0AAA8;
static AnimationSet _gActor136100Animation0AD3C;
static AnimationSet _gActor136100Animation0AF30;
static AnimationSet _gActor136100Animation0C954;
static AnimationSet _gActor136100Animation0CBE4;
static AnimationSet _gActor136100Animation0CD74;
static AnimationSet _gActor136100Animation0CF1C;
static AnimationSet _gActor136100Animation0D0DC;
static AnimationSet _gActor136100Animation0D334;

static TmdSource _gActor136100GaryDouglasBody;
static TmdSource _gActor136100GaryDouglasHeadHat;
static TmdSource _gActor136100Actor120300Model082F8;
void             func_actor_136100_801320E0(Task*);
void             func_actor_136100_80132284(Task*);
void             func_actor_136100_80133690(void);
void             func_actor_136100_8013379C(s32);
void             func_actor_136100_80133BC8(Task*);
void             func_actor_136100_80134588(Task*);
void             func_actor_136100_8013467C(void);
void             func_actor_136100_801346EC(Task*, s32, s32, s32);
void             func_actor_136100_801347B8(void);
void             func_actor_136100_80134838(s16);
void             func_actor_136100_80134858(s16);
void             func_actor_136100_80134878(s16);
void             func_actor_136100_80134898(void);
void             func_actor_136100_801348C8(void);
void             func_actor_136100_801348F8(void);
void             func_actor_136100_80134924(void);
void             func_actor_136100_80134964(void);
void             func_actor_136100_801349B4(s32);

static TmdBone _gActor136100GaryDouglasBodySkeleton[20] = {
#include "assets/gary_douglas_body_skeleton.inc"
};

static u32 _gActor136100GaryDouglasBodyPartVerts[20] = {
#include "assets/gary_douglas_body_partVerts.inc"
};

static SVECTOR _gActor136100GaryDouglasBodyVerts[364] = {
#include "assets/gary_douglas_body_verts.inc"
};

static SVECTOR _gActor136100GaryDouglasBodyNormals[354] = {
#include "assets/gary_douglas_body_normals.inc"
};

static u32 _gActor136100GaryDouglasBodyStream[4151] = {
#include "assets/gary_douglas_body_stream.inc"
};

static TmdSource _gActor136100GaryDouglasBody = {
    0,
    22008,
    6952,
    20,
    _gActor136100GaryDouglasBodyPartVerts,
    _gActor136100GaryDouglasBodyVerts,
    _gActor136100GaryDouglasBodyNormals,
    _gActor136100GaryDouglasBodySkeleton,
    _gActor136100GaryDouglasBodyStream,
};

static TmdBone _gActor136100GaryDouglasHeadHatSkeleton[1] = {
#include "assets/gary_douglas_head_hat_skeleton.inc"
};

static u32 _gActor136100GaryDouglasHeadHatPartVerts[1] = {
#include "assets/gary_douglas_head_hat_partVerts.inc"
};

static SVECTOR _gActor136100GaryDouglasHeadHatVerts[21] = {
#include "assets/gary_douglas_head_hat_verts.inc"
};

static SVECTOR _gActor136100GaryDouglasHeadHatNormals[21] = {
#include "assets/gary_douglas_head_hat_normals.inc"
};

static u32 _gActor136100GaryDouglasHeadHatStream[207] = {
#include "assets/gary_douglas_head_hat_stream.inc"
};

static TmdSource _gActor136100GaryDouglasHeadHat = {
    0,
    1352,
    0,
    1,
    _gActor136100GaryDouglasHeadHatPartVerts,
    _gActor136100GaryDouglasHeadHatVerts,
    _gActor136100GaryDouglasHeadHatNormals,
    _gActor136100GaryDouglasHeadHatSkeleton,
    _gActor136100GaryDouglasHeadHatStream,
};

static TmdBone _gActor136100Actor120300Model082F8Skeleton[1] = {
#include "assets/actor_120300_model_082F8_skeleton.inc"
};

static u32 _gActor136100Actor120300Model082F8PartVerts[1] = {
#include "assets/actor_120300_model_082F8_partVerts.inc"
};

static SVECTOR _gActor136100Actor120300Model082F8Verts[34] = {
#include "assets/actor_120300_model_082F8_verts.inc"
};

static SVECTOR _gActor136100Actor120300Model082F8Normals[28] = {
#include "assets/actor_120300_model_082F8_normals.inc"
};

static u32 _gActor136100Actor120300Model082F8Stream[238] = {
#include "assets/actor_120300_model_082F8_stream.inc"
};

static TmdSource _gActor136100Actor120300Model082F8 = {
    0,
    1692,
    0,
    1,
    _gActor136100Actor120300Model082F8PartVerts,
    _gActor136100Actor120300Model082F8Verts,
    _gActor136100Actor120300Model082F8Normals,
    _gActor136100Actor120300Model082F8Skeleton,
    _gActor136100Actor120300Model082F8Stream,
};

static AnimationPackedPose _gActor136100Animation09414Bank1[2] = {
#include "assets/actor_136100_animation_09414_bank1.inc"
};

static AnimationPackedRotation _gActor136100Animation09414Bank4[24] = {
#include "assets/actor_136100_animation_09414_bank4.inc"
};

static AnimationRecord _gActor136100Animation09414Records[105] = {
#include "assets/actor_136100_animation_09414_records.inc"
};

static u16 _gActor136100Animation09414Indices[20] = {
#include "assets/actor_136100_animation_09414_indices.inc"
};

static AnimationSet _gActor136100Animation09414 = {
    _gActor136100Animation09414Records,
    _gActor136100Animation09414Indices,
    { NULL, _gActor136100Animation09414Bank1, NULL, NULL, _gActor136100Animation09414Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136100Animation09754Bank1[2] = {
#include "assets/actor_136100_animation_09754_bank1.inc"
};

static AnimationPackedRotation _gActor136100Animation09754Bank4[54] = {
#include "assets/actor_136100_animation_09754_bank4.inc"
};

static AnimationRecord _gActor136100Animation09754Records[128] = {
#include "assets/actor_136100_animation_09754_records.inc"
};

static u16 _gActor136100Animation09754Indices[20] = {
#include "assets/actor_136100_animation_09754_indices.inc"
};

static AnimationSet _gActor136100Animation09754 = {
    _gActor136100Animation09754Records,
    _gActor136100Animation09754Indices,
    { NULL, _gActor136100Animation09754Bank1, NULL, NULL, _gActor136100Animation09754Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136100Animation09A3CBank1[2] = {
#include "assets/actor_136100_animation_09A3C_bank1.inc"
};

static AnimationPackedRotation _gActor136100Animation09A3CBank4[64] = {
#include "assets/actor_136100_animation_09A3C_bank4.inc"
};

static AnimationRecord _gActor136100Animation09A3CRecords[96] = {
#include "assets/actor_136100_animation_09A3C_records.inc"
};

static u16 _gActor136100Animation09A3CIndices[20] = {
#include "assets/actor_136100_animation_09A3C_indices.inc"
};

static AnimationSet _gActor136100Animation09A3C = {
    _gActor136100Animation09A3CRecords,
    _gActor136100Animation09A3CIndices,
    { NULL, _gActor136100Animation09A3CBank1, NULL, NULL, _gActor136100Animation09A3CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136100Animation09CF8Bank1[2] = {
#include "assets/actor_136100_animation_09CF8_bank1.inc"
};

static AnimationPackedRotation _gActor136100Animation09CF8Bank4[41] = {
#include "assets/actor_136100_animation_09CF8_bank4.inc"
};

static AnimationRecord _gActor136100Animation09CF8Records[108] = {
#include "assets/actor_136100_animation_09CF8_records.inc"
};

static u16 _gActor136100Animation09CF8Indices[20] = {
#include "assets/actor_136100_animation_09CF8_indices.inc"
};

static AnimationSet _gActor136100Animation09CF8 = {
    _gActor136100Animation09CF8Records,
    _gActor136100Animation09CF8Indices,
    { NULL, _gActor136100Animation09CF8Bank1, NULL, NULL, _gActor136100Animation09CF8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136100Animation09EACBank1[2] = {
#include "assets/actor_136100_animation_09EAC_bank1.inc"
};

static AnimationPackedRotation _gActor136100Animation09EACBank4[23] = {
#include "assets/actor_136100_animation_09EAC_bank4.inc"
};

static AnimationRecord _gActor136100Animation09EACRecords[60] = {
#include "assets/actor_136100_animation_09EAC_records.inc"
};

static u16 _gActor136100Animation09EACIndices[20] = {
#include "assets/actor_136100_animation_09EAC_indices.inc"
};

static AnimationSet _gActor136100Animation09EAC = {
    _gActor136100Animation09EACRecords,
    _gActor136100Animation09EACIndices,
    { NULL, _gActor136100Animation09EACBank1, NULL, NULL, _gActor136100Animation09EACBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136100Animation0A100Bank1[2] = {
#include "assets/actor_136100_animation_0A100_bank1.inc"
};

static AnimationPackedRotation _gActor136100Animation0A100Bank4[35] = {
#include "assets/actor_136100_animation_0A100_bank4.inc"
};

static AnimationRecord _gActor136100Animation0A100Records[88] = {
#include "assets/actor_136100_animation_0A100_records.inc"
};

static u16 _gActor136100Animation0A100Indices[20] = {
#include "assets/actor_136100_animation_0A100_indices.inc"
};

static AnimationSet _gActor136100Animation0A100 = {
    _gActor136100Animation0A100Records,
    _gActor136100Animation0A100Indices,
    { NULL, _gActor136100Animation0A100Bank1, NULL, NULL, _gActor136100Animation0A100Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136100Animation0A310Bank1[2] = {
#include "assets/actor_136100_animation_0A310_bank1.inc"
};

static AnimationPackedRotation _gActor136100Animation0A310Bank4[20] = {
#include "assets/actor_136100_animation_0A310_bank4.inc"
};

static AnimationRecord _gActor136100Animation0A310Records[86] = {
#include "assets/actor_136100_animation_0A310_records.inc"
};

static u16 _gActor136100Animation0A310Indices[20] = {
#include "assets/actor_136100_animation_0A310_indices.inc"
};

static AnimationSet _gActor136100Animation0A310 = {
    _gActor136100Animation0A310Records,
    _gActor136100Animation0A310Indices,
    { NULL, _gActor136100Animation0A310Bank1, NULL, NULL, _gActor136100Animation0A310Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136100Animation0A5A4Bank1[2] = {
#include "assets/actor_136100_animation_0A5A4_bank1.inc"
};

static AnimationPackedRotation _gActor136100Animation0A5A4Bank4[34] = {
#include "assets/actor_136100_animation_0A5A4_bank4.inc"
};

static AnimationRecord _gActor136100Animation0A5A4Records[105] = {
#include "assets/actor_136100_animation_0A5A4_records.inc"
};

static u16 _gActor136100Animation0A5A4Indices[20] = {
#include "assets/actor_136100_animation_0A5A4_indices.inc"
};

static AnimationSet _gActor136100Animation0A5A4 = {
    _gActor136100Animation0A5A4Records,
    _gActor136100Animation0A5A4Indices,
    { NULL, _gActor136100Animation0A5A4Bank1, NULL, NULL, _gActor136100Animation0A5A4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136100Animation0A8B4Bank1[4] = {
#include "assets/actor_136100_animation_0A8B4_bank1.inc"
};

static AnimationPackedRotation _gActor136100Animation0A8B4Bank4[59] = {
#include "assets/actor_136100_animation_0A8B4_bank4.inc"
};

static AnimationRecord _gActor136100Animation0A8B4Records[105] = {
#include "assets/actor_136100_animation_0A8B4_records.inc"
};

static u16 _gActor136100Animation0A8B4Indices[20] = {
#include "assets/actor_136100_animation_0A8B4_indices.inc"
};

static AnimationSet _gActor136100Animation0A8B4 = {
    _gActor136100Animation0A8B4Records,
    _gActor136100Animation0A8B4Indices,
    { NULL, _gActor136100Animation0A8B4Bank1, NULL, NULL, _gActor136100Animation0A8B4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136100Animation0AAA8Bank1[2] = {
#include "assets/actor_136100_animation_0AAA8_bank1.inc"
};

static AnimationPackedRotation _gActor136100Animation0AAA8Bank4[34] = {
#include "assets/actor_136100_animation_0AAA8_bank4.inc"
};

static AnimationRecord _gActor136100Animation0AAA8Records[65] = {
#include "assets/actor_136100_animation_0AAA8_records.inc"
};

static u16 _gActor136100Animation0AAA8Indices[20] = {
#include "assets/actor_136100_animation_0AAA8_indices.inc"
};

static AnimationSet _gActor136100Animation0AAA8 = {
    _gActor136100Animation0AAA8Records,
    _gActor136100Animation0AAA8Indices,
    { NULL, _gActor136100Animation0AAA8Bank1, NULL, NULL, _gActor136100Animation0AAA8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136100Animation0AD3CBank1[2] = {
#include "assets/actor_136100_animation_0AD3C_bank1.inc"
};

static AnimationPackedRotation _gActor136100Animation0AD3CBank4[39] = {
#include "assets/actor_136100_animation_0AD3C_bank4.inc"
};

static AnimationRecord _gActor136100Animation0AD3CRecords[100] = {
#include "assets/actor_136100_animation_0AD3C_records.inc"
};

static u16 _gActor136100Animation0AD3CIndices[20] = {
#include "assets/actor_136100_animation_0AD3C_indices.inc"
};

static AnimationSet _gActor136100Animation0AD3C = {
    _gActor136100Animation0AD3CRecords,
    _gActor136100Animation0AD3CIndices,
    { NULL, _gActor136100Animation0AD3CBank1, NULL, NULL, _gActor136100Animation0AD3CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136100Animation0AF30Bank1[2] = {
#include "assets/actor_136100_animation_0AF30_bank1.inc"
};

static AnimationPackedRotation _gActor136100Animation0AF30Bank4[34] = {
#include "assets/actor_136100_animation_0AF30_bank4.inc"
};

static AnimationRecord _gActor136100Animation0AF30Records[65] = {
#include "assets/actor_136100_animation_0AF30_records.inc"
};

static u16 _gActor136100Animation0AF30Indices[20] = {
#include "assets/actor_136100_animation_0AF30_indices.inc"
};

static AnimationSet _gActor136100Animation0AF30 = {
    _gActor136100Animation0AF30Records,
    _gActor136100Animation0AF30Indices,
    { NULL, _gActor136100Animation0AF30Bank1, NULL, NULL, _gActor136100Animation0AF30Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136100Animation0B234Bank1[6] = {
#include "assets/actor_136100_animation_0B234_bank1.inc"
};

static AnimationPackedRotation _gActor136100Animation0B234Bank4[46] = {
#include "assets/actor_136100_animation_0B234_bank4.inc"
};

static AnimationRecord _gActor136100Animation0B234Records[109] = {
#include "assets/actor_136100_animation_0B234_records.inc"
};

static u16 _gActor136100Animation0B234Indices[20] = {
#include "assets/actor_136100_animation_0B234_indices.inc"
};

static AnimationSet _gActor136100Animation0B234 = {
    _gActor136100Animation0B234Records,
    _gActor136100Animation0B234Indices,
    { NULL, _gActor136100Animation0B234Bank1, NULL, NULL, _gActor136100Animation0B234Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136100Animation0B590Bank1[2] = {
#include "assets/actor_136100_animation_0B590_bank1.inc"
};

static AnimationPackedRotation _gActor136100Animation0B590Bank4[67] = {
#include "assets/actor_136100_animation_0B590_bank4.inc"
};

static AnimationRecord _gActor136100Animation0B590Records[122] = {
#include "assets/actor_136100_animation_0B590_records.inc"
};

static u16 _gActor136100Animation0B590Indices[20] = {
#include "assets/actor_136100_animation_0B590_indices.inc"
};

static AnimationSet _gActor136100Animation0B590 = {
    _gActor136100Animation0B590Records,
    _gActor136100Animation0B590Indices,
    { NULL, _gActor136100Animation0B590Bank1, NULL, NULL, _gActor136100Animation0B590Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136100Animation0B7ECBank1[3] = {
#include "assets/actor_136100_animation_0B7EC_bank1.inc"
};

static AnimationPackedRotation _gActor136100Animation0B7ECBank4[35] = {
#include "assets/actor_136100_animation_0B7EC_bank4.inc"
};

static AnimationRecord _gActor136100Animation0B7ECRecords[87] = {
#include "assets/actor_136100_animation_0B7EC_records.inc"
};

static u16 _gActor136100Animation0B7ECIndices[20] = {
#include "assets/actor_136100_animation_0B7EC_indices.inc"
};

static AnimationSet _gActor136100Animation0B7EC = {
    _gActor136100Animation0B7ECRecords,
    _gActor136100Animation0B7ECIndices,
    { NULL, _gActor136100Animation0B7ECBank1, NULL, NULL, _gActor136100Animation0B7ECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136100Animation0BBC0Bank1[8] = {
#include "assets/actor_136100_animation_0BBC0_bank1.inc"
};

static AnimationPackedRotation _gActor136100Animation0BBC0Bank4[84] = {
#include "assets/actor_136100_animation_0BBC0_bank4.inc"
};

static AnimationRecord _gActor136100Animation0BBC0Records[117] = {
#include "assets/actor_136100_animation_0BBC0_records.inc"
};

static u16 _gActor136100Animation0BBC0Indices[20] = {
#include "assets/actor_136100_animation_0BBC0_indices.inc"
};

static AnimationSet _gActor136100Animation0BBC0 = {
    _gActor136100Animation0BBC0Records,
    _gActor136100Animation0BBC0Indices,
    { NULL, _gActor136100Animation0BBC0Bank1, NULL, NULL, _gActor136100Animation0BBC0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136100Animation0BFB0Bank1[7] = {
#include "assets/actor_136100_animation_0BFB0_bank1.inc"
};

static AnimationPackedRotation _gActor136100Animation0BFB0Bank4[62] = {
#include "assets/actor_136100_animation_0BFB0_bank4.inc"
};

static AnimationRecord _gActor136100Animation0BFB0Records[149] = {
#include "assets/actor_136100_animation_0BFB0_records.inc"
};

static u16 _gActor136100Animation0BFB0Indices[20] = {
#include "assets/actor_136100_animation_0BFB0_indices.inc"
};

static AnimationSet _gActor136100Animation0BFB0 = {
    _gActor136100Animation0BFB0Records,
    _gActor136100Animation0BFB0Indices,
    { NULL, _gActor136100Animation0BFB0Bank1, NULL, NULL, _gActor136100Animation0BFB0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136100Animation0C31CBank1[7] = {
#include "assets/actor_136100_animation_0C31C_bank1.inc"
};

static AnimationPackedRotation _gActor136100Animation0C31CBank4[74] = {
#include "assets/actor_136100_animation_0C31C_bank4.inc"
};

static AnimationRecord _gActor136100Animation0C31CRecords[104] = {
#include "assets/actor_136100_animation_0C31C_records.inc"
};

static u16 _gActor136100Animation0C31CIndices[20] = {
#include "assets/actor_136100_animation_0C31C_indices.inc"
};

static AnimationSet _gActor136100Animation0C31C = {
    _gActor136100Animation0C31CRecords,
    _gActor136100Animation0C31CIndices,
    { NULL, _gActor136100Animation0C31CBank1, NULL, NULL, _gActor136100Animation0C31CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136100Animation0C620Bank1[5] = {
#include "assets/actor_136100_animation_0C620_bank1.inc"
};

static AnimationPackedRotation _gActor136100Animation0C620Bank4[55] = {
#include "assets/actor_136100_animation_0C620_bank4.inc"
};

static AnimationRecord _gActor136100Animation0C620Records[103] = {
#include "assets/actor_136100_animation_0C620_records.inc"
};

static u16 _gActor136100Animation0C620Indices[20] = {
#include "assets/actor_136100_animation_0C620_indices.inc"
};

static AnimationSet _gActor136100Animation0C620 = {
    _gActor136100Animation0C620Records,
    _gActor136100Animation0C620Indices,
    { NULL, _gActor136100Animation0C620Bank1, NULL, NULL, _gActor136100Animation0C620Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136100Animation0C954Bank1[5] = {
#include "assets/actor_136100_animation_0C954_bank1.inc"
};

static AnimationPackedRotation _gActor136100Animation0C954Bank4[46] = {
#include "assets/actor_136100_animation_0C954_bank4.inc"
};

static AnimationRecord _gActor136100Animation0C954Records[124] = {
#include "assets/actor_136100_animation_0C954_records.inc"
};

static u16 _gActor136100Animation0C954Indices[20] = {
#include "assets/actor_136100_animation_0C954_indices.inc"
};

static AnimationSet _gActor136100Animation0C954 = {
    _gActor136100Animation0C954Records,
    _gActor136100Animation0C954Indices,
    { NULL, _gActor136100Animation0C954Bank1, NULL, NULL, _gActor136100Animation0C954Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136100Animation0CBE4Bank1[2] = {
#include "assets/actor_136100_animation_0CBE4_bank1.inc"
};

static AnimationPackedRotation _gActor136100Animation0CBE4Bank4[36] = {
#include "assets/actor_136100_animation_0CBE4_bank4.inc"
};

static AnimationRecord _gActor136100Animation0CBE4Records[102] = {
#include "assets/actor_136100_animation_0CBE4_records.inc"
};

static u16 _gActor136100Animation0CBE4Indices[20] = {
#include "assets/actor_136100_animation_0CBE4_indices.inc"
};

static AnimationSet _gActor136100Animation0CBE4 = {
    _gActor136100Animation0CBE4Records,
    _gActor136100Animation0CBE4Indices,
    { NULL, _gActor136100Animation0CBE4Bank1, NULL, NULL, _gActor136100Animation0CBE4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136100Animation0CD74Bank1[2] = {
#include "assets/actor_136100_animation_0CD74_bank1.inc"
};

static AnimationPackedRotation _gActor136100Animation0CD74Bank4[17] = {
#include "assets/actor_136100_animation_0CD74_bank4.inc"
};

static AnimationRecord _gActor136100Animation0CD74Records[57] = {
#include "assets/actor_136100_animation_0CD74_records.inc"
};

static u16 _gActor136100Animation0CD74Indices[20] = {
#include "assets/actor_136100_animation_0CD74_indices.inc"
};

static AnimationSet _gActor136100Animation0CD74 = {
    _gActor136100Animation0CD74Records,
    _gActor136100Animation0CD74Indices,
    { NULL, _gActor136100Animation0CD74Bank1, NULL, NULL, _gActor136100Animation0CD74Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136100Animation0CF1CBank1[2] = {
#include "assets/actor_136100_animation_0CF1C_bank1.inc"
};

static AnimationPackedRotation _gActor136100Animation0CF1CBank4[23] = {
#include "assets/actor_136100_animation_0CF1C_bank4.inc"
};

static AnimationRecord _gActor136100Animation0CF1CRecords[57] = {
#include "assets/actor_136100_animation_0CF1C_records.inc"
};

static u16 _gActor136100Animation0CF1CIndices[20] = {
#include "assets/actor_136100_animation_0CF1C_indices.inc"
};

static AnimationSet _gActor136100Animation0CF1C = {
    _gActor136100Animation0CF1CRecords,
    _gActor136100Animation0CF1CIndices,
    { NULL, _gActor136100Animation0CF1CBank1, NULL, NULL, _gActor136100Animation0CF1CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136100Animation0D0DCBank1[2] = {
#include "assets/actor_136100_animation_0D0DC_bank1.inc"
};

static AnimationPackedRotation _gActor136100Animation0D0DCBank4[26] = {
#include "assets/actor_136100_animation_0D0DC_bank4.inc"
};

static AnimationRecord _gActor136100Animation0D0DCRecords[60] = {
#include "assets/actor_136100_animation_0D0DC_records.inc"
};

static u16 _gActor136100Animation0D0DCIndices[20] = {
#include "assets/actor_136100_animation_0D0DC_indices.inc"
};

static AnimationSet _gActor136100Animation0D0DC = {
    _gActor136100Animation0D0DCRecords,
    _gActor136100Animation0D0DCIndices,
    { NULL, _gActor136100Animation0D0DCBank1, NULL, NULL, _gActor136100Animation0D0DCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor136100Animation0D334Bank1[2] = {
#include "assets/actor_136100_animation_0D334_bank1.inc"
};

static AnimationPackedRotation _gActor136100Animation0D334Bank4[40] = {
#include "assets/actor_136100_animation_0D334_bank4.inc"
};

static AnimationRecord _gActor136100Animation0D334Records[84] = {
#include "assets/actor_136100_animation_0D334_records.inc"
};

static u16 _gActor136100Animation0D334Indices[20] = {
#include "assets/actor_136100_animation_0D334_indices.inc"
};

static AnimationSet _gActor136100Animation0D334 = {
    _gActor136100Animation0D334Records,
    _gActor136100Animation0D334Indices,
    { NULL, _gActor136100Animation0D334Bank1, NULL, NULL, _gActor136100Animation0D334Bank4, NULL, NULL, NULL },
};

u16 D_actor_136100_8013F17C = 0;

AnimationSet* D_actor_136100_8013F180[8] = {
    &_gActor136100Animation0B234,
    &_gActor136100Animation0B590,
    &_gActor136100Animation0B7EC,
    &_gActor136100Animation0BBC0,
    &_gActor136100Animation0BFB0,
    &_gActor136100Animation0C31C,
    &_gActor136100Animation0C620,
    NULL,
};

AnimationSet* D_actor_136100_8013F1A0[13] = {
    NULL,
    &_gActor136100Animation09414,
    &_gActor136100Animation09754,
    &_gActor136100Animation0A310,
    &_gActor136100Animation0A5A4,
    &_gActor136100Animation09A3C,
    &_gActor136100Animation0A8B4,
    &_gActor136100Animation0AAA8,
    &_gActor136100Animation0AD3C,
    &_gActor136100Animation0AF30,
    &_gActor136100Animation09CF8,
    &_gActor136100Animation09EAC,
    &_gActor136100Animation0A100,
};

AnimationSet* D_actor_136100_8013F1D4[6] = {
    &_gActor136100Animation0C954,
    &_gActor136100Animation0CBE4,
    &_gActor136100Animation0D334,
    &_gActor136100Animation0CF1C,
    &_gActor136100Animation0D0DC,
    &_gActor136100Animation0CD74,
};

s16 D_actor_136100_8013F1EC[8] = {
    -1,
    -1,
    -1,
    4,
    -1,
    0,
    0,
    0,
};

s16 D_actor_136100_8013F1FC[14] = {
    -1,
    -1,
    -1,
    -1,
    -1,
    1,
    -1,
    8,
    -1,
    1,
    -1,
    1,
    1,
    0,
};

s16 D_actor_136100_8013F218[6] = {
    -1,
    -1,
    -1,
    -1,
    1,
    -1,
};

s32 D_actor_136100_8013F224[8] = { 0xF000, 0, 0, 0xF000, 4096, 0, 0, 4096 };

s32 D_actor_136100_8013F244[32] = { -0x12B0BB8, 8400, -0x12B0BB8, 7000, 0xF448, 8400, 0xF448, 7000, -0x12B0BB8, 7000, -0x12B0708, 7000, 0xF448, 7000, 0xF8F8, 7000, -0x12B0708, 7000, -0x12B0708, 8400, 0xF8F8, 7000, 0xF8F8, 8400, -0x12B0708, 8400, -0x12B0BB8, 8400, 0xF8F8, 8400, 0xF448, 8400 };

s32 D_actor_136100_8013F2C4[12] = { 0x10000, 0x30002, 0, 0x50004, 0x70006, 1, 0x90008, 0xB000A, 2, 0xD000C, 0xF000E, 3 };

TaskMessageEntry D_actor_136100_8013F2F4[2] = {
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_136100_801346EC },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceInView },
};

// The following record is dereferenced through an indexed view of this base; keep the complete bounded pool.
ActorTransform D_actor_136100_8013F304[2] = {
    { { -3500, 0, -3700, 0 }, { 0, -1024, 0, 0 } },
    { { -4800, 0, -4100, 0 }, { 0, -1024, 0, 0 } },
};

// The following record is dereferenced through an indexed view of this base; keep the complete bounded pool.
ActorTransform D_actor_136100_8013F334[2] = {
    { { -3200, 0, 6500, 0 }, { 0, 910, 0, 0 } },
    { { -2520, 0, 7690, 0 }, { 0, 910, 0, 0 } },
};

ActorTransform D_actor_136100_8013F364 = { { -3300, 0, 6400, 0 }, { 0, 910, 0, 0 } };

ActorTransform D_actor_136100_8013F37C = { { -5400, 0, -4100, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_136100_8013F394 = { { -6400, 0, -4000, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_actor_136100_8013F3AC = { { -2600, 0, 7400, 0 }, { 0, 420, 0, 0 } };

ActorTransform D_actor_136100_8013F3C4 = { { -1300, 0, 7000, 0 }, { 0, 420, 0, 0 } };

ActorTransform D_actor_136100_8013F3DC = { { -1540, 0, 7600, 0 }, { 0, 420, 0, 0 } };

ActorTransform D_actor_136100_8013F3F4 = { { -5800, 0, -3200, 0 }, { 0, 1365, 0, 0 } };

ActorTransform D_actor_136100_8013F40C = { { -2210, 0, 8000, 0 }, { 0, 2048, 0, 0 } };

ActorTransform D_actor_136100_8013F424 = { { -1000, 0, 7725, 0 }, { 0, 2048, 0, 0 } };

ActorTransform D_actor_136100_8013F43C = { { -910, 0, 8130, 0 }, { 0, 2048, 0, 0 } };

ActorTransform D_actor_136100_8013F454 = { { -5800, -400, -4400, 0 }, { 1843, 0, 0, 0 } };

EvsCommand D_actor_136100_8013F46C[33] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 12 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_8013467C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_80134924 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_80134898 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134838 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134878 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134858 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134838 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134858 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134858 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_801348C8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134838 }, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_80134964 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_136100_801349B4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_801348F8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134838 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134858 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134878 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_136100_8013F784[19] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134838 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134858 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134878 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_80134964 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_80133690 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_136100_801349B4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_801348F8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 18 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_136100_8013F94C[17] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 13 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_8013467C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134838 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_80134924 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134858 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134838 }, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_80134964 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134838 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134858 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134878 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_136100_8013FAE4[16] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_136100_8013F37C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_80134964 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_8013467C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_801347B8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134838 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134858 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134878 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_136100_8013FC64[12] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 14 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134838 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134858 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134838 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134858 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134878 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_136100_8013FD84[38] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 15 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_8013467C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_80134924 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_80134898 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134838 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134858 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134878 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134858 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134878 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134838 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134838 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134858 }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134858 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134838 }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_801348C8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134838 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134858 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134878 }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_80134964 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_136100_801349B4 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_801348F8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134838 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134858 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134878 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_136100_80140114[18] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134838 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134858 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134878 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_80134964 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_136100_8013379C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_136100_801349B4 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_801348F8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 18 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_136100_801402C4[23] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 16 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_8013467C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_136100_8013F334[1] } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_80134924 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134838 }, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134858 }, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134878 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134858 }, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134838 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134858 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134878 }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_80134964 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134838 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134858 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134878 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_136100_801404EC[14] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134838 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134858 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134878 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_80134964 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_136100_8013379C }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_136100_8014063C[11] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 17 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134838 }, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134838 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134858 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_136100_80134878 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc D_actor_136100_80140744[6] = {
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_136100_80133BC8, { .model = &_gActor136100GaryDouglasBody } },
    { { { TASK_BODY_NONE, 192 } }, NULL, { .value = 0 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_136100_801320E0, { .model = &_gActor136100GaryDouglasHeadHat } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_136100_80132284, { .model = &_gActor136100Actor120300Model082F8 } },
    { { { TASK_BODY_NONE, 192 } }, screenFadeInTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_actor_136100_80134588, { .value = 0 } },
};

Task* D_actor_136100_8014078C = NULL;

static s32         func_actor_136100_80131EC4(Task* arg0);
static s32         func_actor_136100_80131FBC(Task* arg0);
static void        func_actor_136100_801323F8(Task* arg0);
static inline void func_actor_136100_SetAnim(Task* task, s16 anim);
static inline void func_actor_136100_ResetSlots(Task* task, s32 count);
static inline void func_actor_136100_PlayAnim(Task* task, u16 anim, s32 blend, s32 speed);
static void        func_actor_136100_80132BC0(Task* arg0);
static void        func_actor_136100_80132E78(Task* arg0);
static void        func_actor_136100_80133558(Task* arg0);
static s32         func_actor_136100_80133904(Task* task);
static void        func_actor_136100_80133A88(Task* task);
static inline s16  func_actor_136100_TakeStartCue(u16* evtId, u8* evtKind, u8* evtSub);
static inline void func_actor_136100_UpdateShadow(Task* arg0, VECTOR* vec);

static s32 func_actor_136100_80131EC4(Task* arg0)
{
    _Actor136100Work*    work;
    _Actor136100Work*    msgWork;
    AnimationPlayRequest rec;
    s16*                 sel;
    s32                  i;
    u16                  idx;
    s32                  weaponId;
    s32                  id;

    work = arg0->work;
    if (work->playerTask == NULL) {
    ret1:
        return 1;
    }
    if (taskMessageDispatch(work->playerTask, ANIMATION_MESSAGE_IS_PLAYING, 0, 0) != 0) {
        return 0;
    }
    if ((u16)work->playerAnimation < 0x2FU) {
        goto ret1;
    }

    i   = (u16)work->playerAnimation - 0x2FU;
    sel = &D_actor_136100_8013F1EC[i];
    id  = *sel;
    if (id < 0) {
        goto ret1;
    }
    idx = (u16)*sel + 0x2FU;

    msgWork                  = arg0->work;
    weaponId                 = gPlayerStatus.weapon;
    id                       = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
    rec.source.index         = id;
    msgWork->playerAnimation = idx;
    rec.animationId          = idx;
    rec.blend                = ANIMATION_BLEND_INTERPOLATE;
    rec.blendFrames          = 0xA;
    rec.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    TASK_MESSAGE_DISPATCH_POINTER(msgWork->playerTask, ANIMATION_MESSAGE_PLAY, &rec, 0);
    goto ret1;
}

static s32 func_actor_136100_80131FBC(Task* arg0)
{
    _Actor136100Work* work;
    _Actor136100Work* animWork;
    u16               i;
    u16               done;
    u16               anim;
    u16               id;

    work = arg0->work;
    for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
        animationTickSlot(&work->rig.anim, i);
    }
    for (done = i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
        if (!(work->rig.slots[i].status.fields.flags & ANIMATION_SLOT_SETTLED)) {
            goto fail;
        }
    }
check:
    if (done) {
        anim = D_actor_136100_8013F1FC[(u16)work->bodyAnimation];
        if (D_actor_136100_8013F1FC[(u16)work->bodyAnimation] >= 0) {
            id                      = anim;
            animWork                = arg0->work;
            animWork->bodyAnimation = id;
            goto loop;
        fail:
            done = 0;
            goto check;
        loop:
            for (i = 1; i < ARRAY_SIZE(animWork->rig.slots); i++) {
                animationSeekSlotWithBlend(&animWork->rig.anim, i, id, 0, 0xA);
            }
        }
        return 1;
    }
    return 0;
}

/// Tick of the head-and-hat model's task: on its first run it allocates a
/// whole `_Actor136100Work` block, zeroes it and parks it in `Task::work`, then wires
/// the model object up -- `tmdAllocPrimitiveBuffer`, `TmdObject::flags` cleared, the
/// work block's light/colour matrices into `TmdObject::lightMtx` / `colorMtx`
/// and the animation-context task reparented under `D_actor_136100_8014078C`.
/// The texture page / CLUT row come from the placement record at the nested
/// area table's `field_0` list with resource-entry ID 0x6A (or the end record if that ID is absent), and this all runs even on the `memMalloc` failure path,
/// which still advances the state after killing the task.
///
/// The dead `VECTOR` is read back through `task->extra` rather than the local
/// `obj`, which is what makes the original reload `Task::extra` for each of the
/// three coordinate reads (see `func_actor_136100_80132284`).
void func_actor_136100_801320E0(Task* task)
{
    _Actor136100Work* work;
    VECTOR            vec;
    AreaPlacement*    place;
    u8                id;

    if (task->state == 0) {
        TmdObject* tmd   = task->extra.tmd;
        GfxCoord*  coord = tmd->coords;

        work       = memMalloc(sizeof(_Actor136100Work), false);
        task->work = work;
        if (work == NULL) {
            taskKill(task);
        } else {
            memFillBytes(work, 0, sizeof(*work));
            coord->parent          = task->spawnArg2.pointer;
            task->extra.tmd->flags = 0;
            tmdAllocPrimitiveBuffer(tmd);
            tmd->lightMtx  = &work->light;
            tmd->colorMtx  = &work->color;
            task->msgTable = D_actor_136100_8013F2F4;
            taskReparent(D_actor_136100_8014078C, task);
        }
        place = Gp_GetNestedAreaRec(&gGameSession->location.loc)->placements;
        id    = place->entryId;
        while (id != AREA_PLACEMENT_END) {
            if (id == 0x6A) {
                break;
            }
            place++;
            id = place->entryId;
        }
        Gp_SetTmdBytes(task->extra.tmd, place->texturePageOffset, place->clutRowOffset);
        task->state += 1;
    }
    {
        TmdObject* obj = task->extra.tmd;

        actorRenderComposeCoord(obj->coords);
        vec.vx = task->extra.tmd->coords->workm.t[0];
        vec.vy = task->extra.tmd->coords->workm.t[1];
        vec.vz = task->extra.tmd->coords->workm.t[2];
        func_800D7A9C(obj, &vec, 0, 3);
    }
}

void func_actor_136100_80132284(Task* arg0)
{
    _Actor136100Work* work;
    VECTOR            vec;

    if (arg0->state == 0) {
        TmdObject* tmd   = arg0->extra.tmd;
        GfxCoord*  coord = tmd->coords;

        work       = memMalloc(sizeof(*work), false);
        arg0->work = work;
        if (work == NULL) {
            taskKill(arg0);
        } else {
            memFillBytes(work, 0, sizeof(*work));
            coord->parent          = arg0->spawnArg2.pointer;
            arg0->extra.tmd->flags = 0;
            tmdAllocPrimitiveBuffer(tmd);
            tmd->lightMtx  = &work->light;
            tmd->colorMtx  = &work->color;
            arg0->msgTable = D_actor_136100_8013F2F4;
            taskReparent(D_actor_136100_8014078C, arg0);
        }
        arg0->state += 1;
        if (arg0->spawnArg1.value != 0) {
            GfxCoord* reset = arg0->extra.tmd->coords;

            gfxRotMatrixX(&reset->coord, 0x400, GRAPHICS_ROTATION_REPLACE);
            reset->coord.t[1]   = 0xC8;
            reset->composeStamp = GRAPHICS_COORD_DIRTY;
        }
    }
    {
        TmdObject* obj = arg0->extra.tmd;

        actorRenderComposeCoord(obj->coords);
        vec.vx = arg0->extra.tmd->coords->workm.t[0];
        vec.vy = arg0->extra.tmd->coords->workm.t[1];
        vec.vz = arg0->extra.tmd->coords->workm.t[2];
        func_800D7A9C(obj, &vec, 0, 3);
    }
}

/// Builds and sends the player's equipped-weapon animation request.
///
/// `record` is accessed repeatedly and must be a side-effect-free request
/// lvalue. `anim` is evaluated twice and must be side-effect-free; `task` is
/// evaluated once. Dispatch consumes the request synchronously.
/// The bank selector depends on the current weapon and save character.
/// Per-expansion work pointers preserve the original call scheduling.
#define ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(task, anim, blendChoice, frames, record)                                            \
    {                                                                                                                                 \
        _Actor136100Work* msgWork;                                                                                                    \
        s32               weaponId;                                                                                                   \
        s32               id;                                                                                                         \
                                                                                                                                      \
        msgWork                       = (task)->work;                                                                                 \
        weaponId                      = gPlayerStatus.weapon;                                                                         \
        id                            = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22; \
        (record).source.index         = id;                                                                                           \
        msgWork->playerAnimation      = anim;                                                                                         \
        (record).animationId          = anim;                                                                                         \
        (record).blend                = (blendChoice);                                                                                \
        (record).blendFrames          = (frames);                                                                                     \
        (record).enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;                                                            \
        TASK_MESSAGE_DISPATCH_POINTER(msgWork->playerTask, ANIMATION_MESSAGE_PLAY, &(record), 0);                                     \
    }

/// Step the cutscene actor's `playerRequest`.  Request 1 runs a three-step
/// sequence on `playerRequestStep` (send the 0x3E9 / 0x3F2 placement, wait for 0x3F0,
/// then wait six ticks on `playerRequestFrames`) before sending the weapon record; 2..6
/// send it straight away with their own animation.  A finished request is
/// cleared.
static void func_actor_136100_801323F8(Task* arg0)
{
    _Actor136100Work*    work = arg0->work;
    AnimationPlayRequest rec;

    if (gGameSession->eventState != 0) {
        func_actor_136100_80131EC4(arg0);
    }
    switch ((u16)work->playerRequest) {
        case 0:
            break;
        case 1:
            switch ((u16)work->playerRequestStep) {
                case 0:
                    TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, 0x3E9, &D_actor_136100_8013F304[0], 0);
                    TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, 0x3F2, &D_actor_136100_8013F304[1], 0);
                    work->playerRequestFrames = 0;
                    work->playerRequestStep++;
                    return;
                case 1:
                    if (taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
                        work->playerRequestStep++;
                    }
                    return;
                case 2:
                    if (++work->playerRequestFrames < 6) {
                        return;
                    }
                    ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(arg0, 0x2F, 1, 5, rec);
                    break;
                default:
                    return;
            }
            break;
        case 2:
            ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(arg0, 0x32, 1, 0xA, rec);
            break;
        case 3:
            ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(arg0, 0x34, 1, 0xA, rec);
            break;
        case 4:
            TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, 0x3E9, &D_actor_136100_8013F37C, 0);
            ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(arg0, 0x2F, 0, 0, rec);
            break;
        case 5:
            ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(arg0, 1, 1, 0xA, rec);
            break;
        case 6:
            ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(arg0, 1, 0, 0, rec);
            break;
    }
    work->playerRequest = 0;
}

/// Installs the linked actor's animation table and requests a blended clip.
///
/// `record` is accessed repeatedly and must be a side-effect-free request
/// lvalue. `anim` is evaluated twice and must be side-effect-free; `task` is
/// evaluated once. Dispatch consumes the request synchronously.
/// A null linked task suppresses evaluation of the clip and record arguments.
/// Per-expansion work pointers preserve the original call scheduling.
#define ACTOR_136100_PLAY_LINKED_ANIMATION(task, anim, blendChoice, frames, record)                                   \
    {                                                                                                                 \
        _Actor136100Work* animWork = (task)->work;                                                                    \
                                                                                                                      \
        if (animWork->companionTask != NULL) {                                                                        \
            (record).source.sets          = D_actor_136100_8013F1D4;                                                  \
            animWork->companionAnimation  = anim;                                                                     \
            (record).animationId          = anim;                                                                     \
            (record).blend                = (blendChoice);                                                            \
            (record).blendFrames          = (frames);                                                                 \
            (record).enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;                                        \
            TASK_MESSAGE_DISPATCH_POINTER(animWork->companionTask, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &(record), 0); \
        }                                                                                                             \
    }

/// Record `id` as the work block's current animation (`bodyAnimation`).
#define _actor136100SetAnimId(work, id) \
    do {                                \
        (work)->bodyAnimation = (id);   \
    } while (0)

/// Record `anim` as the current animation and start it on rig slots 1..19.
static inline void func_actor_136100_SetAnim(Task* task, s16 anim)
{
    _Actor136100Work* work = task->work;
    s32               i;

    _actor136100SetAnimId(work, anim);
    for (i = 1; (u16)i < ARRAY_SIZE(work->rig.slots); i++) {
        animationSeekSlotWithBlend(&work->rig.anim, i & 0xFFFF, anim, 0, 0xA);
    }
}

/// Re-arm animation slots 1..19 with slot count `count`
/// (`func_actor_136100_801347B8`'s loop, reaching the work block through `task`).
static inline void func_actor_136100_ResetSlots(Task* task, s32 count)
{
    _Actor136100Work* work = task->work;
    s32               i;

    work->bodyAnimation = count;
    i                   = 1;
    do {
        work->rig.slots[(u16)i].rate = ANIMATION_RATE_ONE;
        animationResetSlot(&work->rig.anim, (u16)i, count);
        i++;
    } while ((u16)i < ARRAY_SIZE(work->rig.slots));
}

static void func_actor_136100_80132748(Task* arg0)
{
    _Actor136100Work*    work = arg0->work;
    AnimationPlayRequest rec;

    func_actor_136100_80131FBC(arg0);
    switch ((u16)work->bodyRequest) {
        case 0:
            break;
        case 1:
            switch ((u16)work->bodyRequestStep) {
                case 0:
                    work->bodyRequestFrames = 0;
                    func_actor_136100_SetAnim(arg0, 2);
                    work->bodyRequestStep++;
                    return;
                case 1:
                    if (++work->bodyRequestFrames < 0x3D) {
                        return;
                    }
                    ACTOR_136100_PLAY_LINKED_ANIMATION(arg0, 3, 1, 0xA, rec);
                    break;
                default:
                    return;
            }
            break;
        case 2:
            switch ((u16)work->bodyRequestStep) {
                case 0:
                    work->bodyRequestFrames = 0;
                    func_actor_136100_SetAnim(arg0, 5);
                    work->bodyRequestStep++;
                    return;
                case 1:
                    if (++work->bodyRequestFrames == 0x11) {
                        sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MAIN_STREET, 0x0E), 0, 0);
                    }
                    if (work->bodyRequestFrames < 0x15) {
                        return;
                    }
                    {
                        _Actor136100Work* msgWork;
                        s32               weaponId;
                        s32               id;

                        msgWork                  = arg0->work;
                        weaponId                 = gPlayerStatus.weapon;
                        id                       = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
                        rec.source.index         = id;
                        msgWork->playerAnimation = 0x30;
                        rec.animationId          = 0x30;
                        rec.blend                = ANIMATION_BLEND_INTERPOLATE;
                        rec.blendFrames          = 0xA;
                        rec.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;

                        TASK_MESSAGE_DISPATCH_POINTER(msgWork->playerTask, ANIMATION_MESSAGE_PLAY, &rec, 0);
                    }
                    ACTOR_136100_PLAY_LINKED_ANIMATION(arg0, 4, 1, 0xA, rec);
                    break;
                default:
                    return;
            }
            break;
        case 3:
            func_actor_136100_SetAnim(arg0, 7);
            break;
        case 4:
            func_actor_136100_SetAnim(arg0, 9);
            break;
        case 5:
            switch ((u16)work->bodyRequestStep) {
                case 0:
                    work->bodyRequestFrames = 0;
                    func_actor_136100_SetAnim(arg0, 0xA);
                    work->bodyRequestStep++;
                    return;
                case 1:
                    if (++work->bodyRequestFrames < 0x50) {
                        return;
                    }
                    func_actor_136100_SetAnim(arg0, 0xB);
                    break;
                default:
                    return;
            }
            break;
        case 6: {
            _Actor136100Work* animWork = arg0->work;
            s32               i;

            animWork->bodyAnimation = 1;
            for (i = 1; (u16)i < ARRAY_SIZE(animWork->rig.slots); i++) {
                animWork->rig.slots[(u16)i].rate = ANIMATION_RATE_ONE;
                animationResetSlot(&animWork->rig.anim, (u16)i, 1);
            }
        } break;
    }
    work->bodyRequest = 0;
}

/// Play `anim` on `companionTask` (message 0x3F4) and record it as the
/// current `companionAnimation` chain entry; does nothing while that task is unset.
static inline void func_actor_136100_PlayAnim(Task* task, u16 anim, s32 blend, s32 speed)
{
    _Actor136100Work*    work = task->work;
    AnimationPlayRequest msg;

    if (work->companionTask != NULL) {
        msg.source.sets          = D_actor_136100_8013F1D4;
        work->companionAnimation = anim;
        msg.animationId          = anim;
        msg.blend                = blend;
        msg.blendFrames          = speed;
        msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
        TASK_MESSAGE_DISPATCH_POINTER(work->companionTask, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg, 0);
    }
}

/// Advance the animation chain like `func_actor_136100_80133558`, then run the
/// `companionRequest`: 1 and 3 play a fixed animation, 2 steps the
/// `companionRequestStep` sequence -- a sound and an animation three times, 15
/// ticks apart (`companionRepeatDelay` countdown, `companionRepeatCount` plays
/// so far) before a final animation. Every request that finishes clears
/// `companionRequest`.
static void func_actor_136100_80132BC0(Task* arg0)
{
    _Actor136100Work* work;
    s16               anim;

    work = arg0->work;
    if (work->companionTask != NULL && taskMessageDispatch(work->companionTask, ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
        anim = D_actor_136100_8013F218[work->companionAnimation];
        if (anim >= 0) {
            func_actor_136100_PlayAnim(arg0, anim, 1, 0xA);
        }
    }
    switch ((u16)work->companionRequest) {
        case 0:
            break;
        case 1:
            func_actor_136100_PlayAnim(arg0, 1, 0, 0);
            break;
        case 2:
            switch ((u16)work->companionRequestStep) {
                case 0:
                    work->companionRepeatCount = 0;
                    work->companionRepeatDelay = 0;
                    work->companionRequestStep++;
                    return;
                case 1:
                    if (--work->companionRepeatDelay > 0) {
                        return;
                    }
                    if (work->companionRepeatCount >= 3) {
                        func_actor_136100_PlayAnim(arg0, 1, 1, 0xA);
                        break;
                    }
                    sndEvtRequestScriptStart(SOUND_CHARACTER(SOUND_BANK_ACTOR_800200, 9), 0, 0);
                    func_actor_136100_PlayAnim(arg0, 2, 1, 0xA);
                    work->companionRepeatDelay = 0xF;
                    work->companionRepeatCount++;
                    return;
                default:
                    return;
            }
            break;
        case 3:
            TASK_MESSAGE_DISPATCH_POINTER(work->companionTask, 0x3E9, &D_actor_136100_8013F3F4, 0);
            func_actor_136100_PlayAnim(arg0, 0, 0, 0);
            break;
    }
    work->companionRequest = 0;
}

/// Step the cutscene actor's `playerRequest` in the scene before the Burner, the sibling of
/// `func_actor_136100_801323F8`: request 2 runs the three-step `playerRequestStep`
/// sequence (0x3F3 / 0x3E9 / 0x3F2 placement, wait for 0x3F0, six ticks on
/// `playerRequestFrames`), the others send the weapon record straight away.  A finished
/// request is cleared.
static void func_actor_136100_80132E78(Task* arg0)
{
    _Actor136100Work*    work = arg0->work;
    AnimationPlayRequest rec;

    if (gGameSession->eventState != 0) {
        func_actor_136100_80131EC4(arg0);
    }
    switch ((u16)work->playerRequest) {
        case 0:
            break;
        case 1:
            taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, 0, 0);
            ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(arg0, 1, 0, 0, rec);
            break;
        case 2:
            switch ((u16)work->playerRequestStep) {
                case 0:
                    taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
                    TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, 0x3E9, &D_actor_136100_8013F334[0], 0);
                    TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, 0x3F2, &D_actor_136100_8013F334[1], 0);
                    work->playerRequestFrames = 0;
                    work->playerRequestStep++;
                    return;
                case 1:
                    if (taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
                        work->playerRequestStep++;
                    }
                    return;
                case 2:
                    if (++work->playerRequestFrames < 6) {
                        return;
                    }
                    ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(arg0, 0x2F, 1, 5, rec);
                    break;
                default:
                    return;
            }
            break;
        case 3:
            ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(arg0, 0x31, 1, 0xA, rec);
            break;
        case 4:
            ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(arg0, 0x35, 1, 0xA, rec);
            break;
        case 5:
            TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, 0x3E9, &D_actor_136100_8013F364, 0);
            ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(arg0, 1, 0, 0, rec);
            break;
        case 6:
            ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(arg0, 0x2F, 0, 0, rec);
            break;
        case 7:
            ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(arg0, 1, 1, 0xA, rec);
            break;
    }
    work->playerRequest = 0;
}

static void func_actor_136100_80133238(Task* arg0)
{
    _Actor136100Work* work;
    GfxCoord*         coords;

    work = arg0->work;
    func_actor_136100_80131FBC(arg0);
    switch ((u16)work->bodyRequest) {
        case 0:
            break;
        case 1:
            TASK_MESSAGE_DISPATCH_POINTER(arg0, 0x7D4, &D_actor_136100_8013F3C4, 0);
            break;
        case 2:
            TASK_MESSAGE_DISPATCH_POINTER(arg0, 0x7D4, &D_actor_136100_8013F3DC, 0);
            break;
        case 3:
            func_actor_136100_SetAnim(arg0, 4);
            break;
        case 4:
            func_actor_136100_ResetSlots(arg0, 3);
            TASK_MESSAGE_DISPATCH_POINTER(arg0, 0x7D4, &D_actor_136100_8013F3AC, 0);
            break;
        case 5:
            switch ((u16)work->bodyRequestStep) {
                case 0:
                    func_actor_136100_SetAnim(arg0, 6);
                    work->bodyRequestFrames = 0;
                    work->bodyRequestStep++;
                    return;
                case 1:
                    if (++work->bodyRequestFrames == 0xF) {
                        sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MAIN_STREET, 0x0F), 0, 0);
                        work->bodyRequest = 0;
                    }
                    return;
            }
            return;
        case 6:
            // The head turn counts its steps on the player request's step, which
            // the player request the script posts just before it leaves at 0.
            switch ((u16)work->playerRequestStep) {
                case 0:
                    TASK_MESSAGE_DISPATCH_POINTER(arg0, 0x7D4, &D_actor_136100_8013F3DC, 0);
                    work->headYaw = 0x1000;
                    work->playerRequestStep++;
                    return;
                case 1:
                    gfxRotMatrixY(&arg0->extra.tmd->coords[4].coord, work->headYaw, 1);
                    if (work->headYaw >= 0xD56) {
                        work->headYaw -= 0x40;
                    }
                    return;
            }
            return;
        case 7:
            coords = arg0->extra.tmd->coords;
            if ((work->headYaw += 0x40) > 0x1000) {
                work->headYaw     = 0;
                work->bodyRequest = 0;
            }
            gfxRotMatrixY(&coords[4].coord, work->headYaw, 1);
            return;
        case 8:
            func_actor_136100_ResetSlots(arg0, 3);
            break;
    }
    work->bodyRequest = 0;
}

/// Advance the cutscene actor's animation chain and send its pending placement.
///
/// Once `companionTask` reports its current animation done (message
/// 0x3ED), steps `companionAnimation` to the next entry of the `D_actor_136100_8013F218`
/// chain (negative ends it) and plays it with 0x3F4.  Then sends the 0x3E9
/// placement selected by `companionRequest` (1..3) and clears the request.
static void func_actor_136100_80133558(Task* arg0)
{
    _Actor136100Work*    work;
    _Actor136100Work*    msgWork;
    AnimationPlayRequest msg;
    u16                  anim;

    work = arg0->work;
    if (work->companionTask != NULL && taskMessageDispatch(work->companionTask, ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
        anim = D_actor_136100_8013F218[work->companionAnimation];
        if (D_actor_136100_8013F218[work->companionAnimation] >= 0) {
            msgWork = arg0->work;
            if (msgWork->companionTask != NULL) {
                msg.source.sets             = D_actor_136100_8013F1D4;
                msgWork->companionAnimation = anim;
                msg.animationId             = anim;
                msg.blend                   = ANIMATION_BLEND_INTERPOLATE;
                msg.blendFrames             = 0xA;
                msg.enableWorldCollision    = ANIMATION_WORLD_COLLISION_DISABLE;
                TASK_MESSAGE_DISPATCH_POINTER(msgWork->companionTask, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg, 0);
            }
        }
    }
    switch ((u16)work->companionRequest) {
        case 0:
            break;
        case 1:
            TASK_MESSAGE_DISPATCH_POINTER(work->companionTask, 0x3E9, &D_actor_136100_8013F424, 0);
            break;
        case 2:
            TASK_MESSAGE_DISPATCH_POINTER(work->companionTask, 0x3E9, &D_actor_136100_8013F43C, 0);
            break;
        case 3:
            TASK_MESSAGE_DISPATCH_POINTER(work->companionTask, 0x3E9, &D_actor_136100_8013F40C, 0);
            break;
    }
    work->companionRequest = 0;
}

/// Reset the cutscene actor's animation state and re-send the weapon record.
///
/// Clears the first two value/countdown pairs, re-arms all nineteen animation
/// slots through `animationResetSlot` with the work block's slot count at 1, then
/// sends slot 3 the 0x3E9 placement and the 0x3E8 weapon record
/// (`AnimationPlayRequest`) built from the equip-slot addend (`gPlayerStatus.weapon`), the pair
/// `func_actor_136100_8013467C` sends on its own.  `playerAnimation` is armed on the
/// way past.
///
/// Three separate `task->work` loads are what the original reaches the block
/// with -- the stores to `playerRequest` / `bodyRequest` invalidate the first in cse,
/// and the first is still live for the 0x3E9 send after the loop.  The dead
/// `SVECTOR` is not read; it reserves the 8-byte local the frame has between
/// the outgoing-arg area and `rec` (see `func_actor_136100_801347B8`).
void func_actor_136100_80133690(void)
{
    Task*                task;
    _Actor136100Work*    work;
    _Actor136100Work*    animWork;
    _Actor136100Work*    msgWork;
    SVECTOR              unused;
    AnimationPlayRequest rec;
    s32                  i;
    s32                  weaponId;
    s32                  id;

    task                = D_actor_136100_8014078C;
    work                = task->work;
    work->playerRequest = 0;
    work->bodyRequest   = 0;

    animWork                = task->work;
    animWork->bodyAnimation = 1;
    i                       = 1;
    do {
        animWork->rig.slots[(u16)i].rate = ANIMATION_RATE_ONE;
        animationResetSlot(&animWork->rig.anim, (u16)i, 1);
        i++;
    } while ((u16)i < ARRAY_SIZE(animWork->rig.slots));

    TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, 0x3E9, &D_actor_136100_8013F304[1], 0);

    msgWork                  = task->work;
    weaponId                 = gPlayerStatus.weapon;
    id                       = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
    rec.source.index         = id;
    msgWork->playerAnimation = 1;
    rec.animationId          = 1;
    rec.blend                = ANIMATION_BLEND_RESET;
    rec.blendFrames          = 0;
    rec.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    TASK_MESSAGE_DISPATCH_POINTER(msgWork->playerTask, ANIMATION_MESSAGE_PLAY, &rec, 0);
}

/// Second half of the cutscene actor's re-arm: clears the first two
/// value/countdown pairs, sends the 0x7D4 cue to the host task, re-arms all
/// nineteen animation slots with the work block's slot count at 3, then re-sends
/// the two placement cues and the 0x3E8 weapon record (`AnimationPlayRequest`) built from the
/// equip-slot addend (`gPlayerStatus.weapon`).  `arg0 == 1` additionally resets the
/// fourth bone's rotation to zero.
///
/// Two `task->work` loads reach the block: the stores to `playerRequest` /
/// `bodyRequest` invalidate the first in cse, and it is still live for the 0x3E9
/// and 0x3E9/`companionTask` sends after the loop.  The dead `SVECTOR` is not read;
/// it reserves the 8-byte local the frame has between the outgoing-arg area and
/// `rec` (see `func_actor_136100_80133690`).
void func_actor_136100_8013379C(s32 arg0)
{
    Task*                task;
    _Actor136100Work*    work;
    _Actor136100Work*    animWork;
    _Actor136100Work*    msgWork;
    SVECTOR              unused;
    AnimationPlayRequest rec;
    s32                  i;
    s32                  weaponId;
    s32                  id;

    task                = D_actor_136100_8014078C;
    work                = task->work;
    work->playerRequest = 0;
    work->bodyRequest   = 0;

    TASK_MESSAGE_DISPATCH_POINTER(task, 0x7D4, &D_actor_136100_8013F3AC, 0);

    animWork                = task->work;
    animWork->bodyAnimation = 3;
    i                       = 1;
    do {
        animWork->rig.slots[(u16)i].rate = ANIMATION_RATE_ONE;
        animationResetSlot(&animWork->rig.anim, (u16)i, 3);
        i++;
    } while ((u16)i < ARRAY_SIZE(animWork->rig.slots));

    TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, 0x3E9, D_actor_136100_8013F334, 0);

    msgWork                  = task->work;
    weaponId                 = gPlayerStatus.weapon;
    id                       = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
    rec.source.index         = id;
    msgWork->playerAnimation = 1;
    rec.animationId          = 1;
    rec.blend                = ANIMATION_BLEND_RESET;
    rec.blendFrames          = 0;
    rec.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    TASK_MESSAGE_DISPATCH_POINTER(msgWork->playerTask, ANIMATION_MESSAGE_PLAY, &rec, 0);

    TASK_MESSAGE_DISPATCH_POINTER(work->companionTask, 0x3E9, &D_actor_136100_8013F40C, 0);

    if (arg0 == 1) {
        gfxRotMatrixY(&task->extra.tmd->coords[4].coord, 0, 1);
    }
}

/// Cue handler: when the pending `Gp_TakePendingObj4C` event is a positive
/// id 5 (and `gPlayerStatus.interactionPressed` is set), kind 0x12 in phase 0 or kind 0x13 in phase 1
/// notifies via `func_actor_136100_80134A18` and plays the phase's first cue on
/// the first hit (`func_800E8634`, advancing `followUpSeen`) or its repeat cue after.
/// `ready` must be `s16`: as `s32` the `!= 0` store fuses into the callee-saved
/// home and the join copy into `$v0` disappears.
static s32 func_actor_136100_80133904(Task* task)
{
    _Actor136100Work* work = task->work;
    u16               evtId;
    u8                evtKind;
    u8                evtSub;
    s16               ready;

    ready = 0;
    if (Gp_TakePendingObj4C(&evtId, &evtKind, &evtSub) != 0) {
        if (!((s16)evtId & WORLD_COLLISION_TRIGGER_AUTOMATIC)) {
            if ((evtId & (0xFFFF ^ WORLD_COLLISION_TRIGGER_AUTOMATIC)) == WORLD_COLLISION_TRIGGER_ACTION_ROOM) {
                ready = gPlayerStatus.interactionPressed != 0;
            }
        }
    }
    if (ready == 0 || Gp_StateC08.mode == ATTACHMENT_MODE_WHEEL) {
        return 0;
    }
    if (gDisplayState.pendingMode != DISPLAY_MODE_NONE) {
        return 0;
    }
    if ((s8)evtKind == 0x12 && work->scene == ACTOR_136100_SCENE_AFTER_BURNER) {
        func_actor_136100_80134A18(task);
        if (work->followUpSeen == 0) {
            func_800E8634(D_actor_136100_8013F94C, 0, D_actor_136100_8013FAE4);
            work->followUpSeen++;
        } else {
            func_800E8614(D_actor_136100_8013FC64, 0);
        }
        return 1;
    }
    if ((s8)evtKind == 0x13 && work->scene == ACTOR_136100_SCENE_BEFORE_BURNER) {
        func_actor_136100_80134A18(task);
        if (work->followUpSeen == 0) {
            func_800E8634(D_actor_136100_801402C4, 0, D_actor_136100_801404EC);
            work->followUpSeen++;
        } else {
            func_800E8614(D_actor_136100_8014063C, 0);
        }
        return 1;
    }
    return 0;
}

/// Initialize the cutscene actor's model and animations.
///
/// Uses the area placement for resource-entry 0x6A, or the end record when
/// that entry is absent. Allocation failure kills `task`.
static void func_actor_136100_80133A88(Task* task)
{
    enum { TEXTURE_RESOURCE_ENTRY_ID = 0x6A };

    _Actor136100Work* work;
    _Actor136100Work* allocatedWork;
    TmdObject*        tmd;
    GfxCoord*         coord;
    AreaPlacement*    place;
    u8                entryId;

    tmd           = task->extra.tmd;
    coord         = tmd->coords;
    allocatedWork = memMalloc(sizeof(_Actor136100Work), false);
    task->work    = allocatedWork;
    if (allocatedWork == NULL) {
        taskKill(task);
        return;
    }
    work = allocatedWork;
    memFillBytes(work, 0, sizeof(*work));
    work->playerTask        = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    D_actor_136100_8014078C = task;
    coord->parent           = &gGfxViewCoord;
    tmdAllocPrimitiveBuffer(tmd);
    tmd->lightMtx = &work->light;
    tmd->colorMtx = &work->color;
    tmd->flags   &= (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
    place         = Gp_GetNestedAreaRec(&gGameSession->location.loc)->placements;
    entryId       = place->entryId;
    while (entryId != AREA_PLACEMENT_END) {
        if (entryId == TEXTURE_RESOURCE_ENTRY_ID) {
            break;
        }
        place++;
        entryId = place->entryId;
    }
    Gp_SetTmdBytes(tmd, place->texturePageOffset, place->clutRowOffset);
    animationInitContext(&work->rig.anim, D_actor_136100_8013F1A0, tmd, work->rig.poses, work->rig.slots);
    task->msgTable = D_actor_136100_8013F2F4;
}

/// Classify the pending `Gp_TakePendingObj4C` event for the cutscene's start
/// cue: id 5 with kind 0x10 is 1 (phase 0), kind 0x11 is 2 (phase 1), anything
/// else 0.  The `s16` return is what keeps the result in its own pseudo, copied
/// into the caller's compare register after the join.
static inline s16 func_actor_136100_TakeStartCue(u16* evtId, u8* evtKind, u8* evtSub)
{
    if (Gp_TakePendingObj4C(evtId, evtKind, evtSub) != 0) {
        if ((*evtId & (0xFFFF ^ WORLD_COLLISION_TRIGGER_AUTOMATIC)) == WORLD_COLLISION_TRIGGER_ACTION_ROOM) {
            if ((s8)*evtKind == 0x10) {
                return 1;
            }
            if ((s8)*evtKind == 0x11) {
                return 2;
            }
        }
    }
    return 0;
}

/// Installs the linked actor's animation table and resets to a clip.
///
/// `record` is accessed repeatedly and must be a side-effect-free request
/// lvalue. `anim` is evaluated twice and must be side-effect-free; `task` is
/// evaluated once. Dispatch consumes the request synchronously.
/// A null linked task suppresses evaluation of the clip and record arguments.
/// Per-expansion work pointers preserve the original call scheduling.
#define ACTOR_136100_RESET_LINKED_ANIMATION(task, anim, record)                                                       \
    {                                                                                                                 \
        _Actor136100Work* animWork = (task)->work;                                                                    \
                                                                                                                      \
        if (animWork->companionTask != NULL) {                                                                        \
            (record).source.sets          = D_actor_136100_8013F1D4;                                                  \
            animWork->companionAnimation  = anim;                                                                     \
            (record).animationId          = anim;                                                                     \
            (record).blend                = ANIMATION_BLEND_RESET;                                                    \
            (record).blendFrames          = 0;                                                                        \
            (record).enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;                                        \
            TASK_MESSAGE_DISPATCH_POINTER(animWork->companionTask, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &(record), 0); \
        }                                                                                                             \
    }

/// Copies seven set addresses into the player's writable animation-bank extension.
///
/// `record` must be a side-effect-free `AnimationBankCopyRequest` lvalue. The table is
/// null-terminated; the receiver copies only the preceding entries.
/// Per-expansion work pointers preserve the original call scheduling.
#define ACTOR_136100_COPY_PLAYER_ANIMATION_SETS(task, record)                                                    \
    {                                                                                                            \
        _Actor136100Work* msgWork = (task)->work;                                                                \
        s32               n;                                                                                     \
                                                                                                                 \
        n = 0;                                                                                                   \
        while (D_actor_136100_8013F180[n & 0xFFFF] != 0) {                                                       \
            n += 1;                                                                                              \
        }                                                                                                        \
        (record).source.sets = &D_actor_136100_8013F180[0];                                                      \
        (record).wordCount   = n & 0xFFFF;                                                                       \
        TASK_MESSAGE_DISPATCH_POINTER(msgWork->playerTask, ANIMATION_MESSAGE_COPY_BANK_EXTENSION, &(record), 0); \
    }

/// Refresh the shadow coordinate and hand its translation to `func_800D7A9C`.
/// `vec` is a parameter rather than a local so the caller's buffer address
/// stays out of the CSE class of the `Gp_DrawFloorQuad` argument that follows.
static inline void func_actor_136100_UpdateShadow(Task* arg0, VECTOR* vec)
{
    TmdObject* obj = arg0->extra.tmd;

    actorRenderComposeCoord(&obj->coords[1]);
    vec->vx = arg0->extra.tmd->coords[1].workm.t[0];
    vec->vy = arg0->extra.tmd->coords[1].workm.t[1];
    vec->vz = arg0->extra.tmd->coords[1].workm.t[2];
    func_800D7A9C(obj, vec, 0, 3);
}

/// Main tick of the cutscene actor.  State 0 allocates the work block, picks
/// the scene (`scene`, from `GAME_FLAG_BURNER_DEFEATED`) and spawns the head and rifle tasks;
/// state 1 sends the phase's opening cues; state 2 waits for the matching start
/// cue, sends the weapon record and table and sets game flag 0x7C; states 3..5
/// wait on `gGameSession->eventState` and `func_actor_136100_80133904`.  Every
/// state then runs the phase's three per-frame handlers and redraws the shadow.
///
/// Animation, table-copy and geometry values use separate views of the same
/// temporary storage; each is consumed before the next view is written.
void func_actor_136100_80133BC8(Task* arg0)
{
    _Actor136100Work* work = arg0->work;
    SVECTOR           unused;
    union {
        AnimationPlayRequest     animation;
        AnimationBankCopyRequest copy;
        VECTOR                   shadowPosition;
        SVECTOR                  floorOffset;
    } message;
    s32 cue;
    u16 evtId;
    u8  evtKind;
    u8  evtSub;
    u16 evtId2;
    u8  evtKind2;
    u8  evtSub2;

    switch (arg0->state) {
        case 0:
            if (gameFlagGetNibble(GAME_FLAG_NIGHT_MAIN_STREET_CUTSCENE_SEEN) != 0) {
                taskKill(arg0);
                return;
            }
            func_actor_136100_80133A88(arg0);
            work           = arg0->work;
            work->scene    = gameFlagGetNibble(GAME_FLAG_BURNER_DEFEATED) == 0;
            work->headTask = taskSpawnFromTable(D_actor_136100_80140744, 2, 0,
                                                arg0->extra.tmd->coords + 4);
            if (work->scene == ACTOR_136100_SCENE_AFTER_BURNER) {
                func_actor_136100_ResetSlots(arg0, 1);
                work->rifleTask = taskSpawnFromTable(D_actor_136100_80140744, 3, 0, &gGfxViewCoord);
            } else {
                func_actor_136100_ResetSlots(arg0, 3);
                work->rifleTask = taskSpawnFromTable(D_actor_136100_80140744, 3, 1,
                                                     arg0->extra.tmd->coords + 8);
                memCopyBytes(&D_actor_136100_8013F224, gDryfieldNightMainStreetCollision06F80Normals, sizeof(D_actor_136100_8013F224));
                memCopyBytes(&D_actor_136100_8013F2C4, gDryfieldNightMainStreetCollision06F80Faces, sizeof(D_actor_136100_8013F2C4));
                memCopyBytes(&D_actor_136100_8013F244, gDryfieldNightMainStreetCollision06F80Verts, sizeof(D_actor_136100_8013F244));
            }
            work->companionTask = gameGetTaskSlot(GAME_TASK_SLOT_COMPANION);
            arg0->state++;
            break;
        case 1:
            if (work->scene == ACTOR_136100_SCENE_AFTER_BURNER) {
                func_800E3FAC(0xA2, 0x19);
                taskMessageDispatch(arg0, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
                taskMessageDispatch(work->headTask, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
                taskMessageDispatch(work->rifleTask, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
                TASK_MESSAGE_DISPATCH_POINTER(work->rifleTask, 0x7D4, &D_actor_136100_8013F454, 0);
                if (work->companionTask != NULL) {
                    TASK_MESSAGE_DISPATCH_POINTER(work->companionTask, 0x3E9, &D_actor_136100_8013F3F4, 0);
                }
                TASK_MESSAGE_DISPATCH_POINTER(arg0, 0x7D4, &D_actor_136100_8013F394, 0);
                func_actor_136100_ResetSlots(arg0, 1);
                ACTOR_136100_RESET_LINKED_ANIMATION(arg0, 1, message.animation);
            } else {
                func_800E3FAC(0xA2, 0x1A);
                taskMessageDispatch(arg0, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
                taskMessageDispatch(work->headTask, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
                taskMessageDispatch(work->rifleTask, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
                if (work->companionTask != NULL) {
                    TASK_MESSAGE_DISPATCH_POINTER(work->companionTask, 0x3E9, &D_actor_136100_8013F40C, 0);
                }
                TASK_MESSAGE_DISPATCH_POINTER(arg0, 0x7D4, &D_actor_136100_8013F3AC, 0);
                func_actor_136100_ResetSlots(arg0, 3);
                ACTOR_136100_RESET_LINKED_ANIMATION(arg0, 5, message.animation);
            }
            arg0->state++;
            break;
        case 2:
            cue = func_actor_136100_TakeStartCue(&evtId, &evtKind, &evtSub);
            if (cue == 1 && work->scene == ACTOR_136100_SCENE_AFTER_BURNER) {
                if (Gp_StateC08.mode == ATTACHMENT_MODE_WHEEL) {
                    return;
                }
                if (gDisplayState.pendingMode != DISPLAY_MODE_NONE) {
                    return;
                }
                ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(arg0, 1, 1, 0xA, message.animation);
                func_800E8634(D_actor_136100_8013F46C, 0, D_actor_136100_8013F784);
                Gp_UnlinkObj4A(0, &D_dryfield_night_main_street_8018824C[8]);
                ACTOR_136100_COPY_PLAYER_ANIMATION_SETS(arg0, message.copy);
                gameFlagSetNibble(GAME_FLAG_NIGHT_MAIN_STREET_CUTSCENE_SEEN, 1);
                arg0->state++;
                break;
            }
            if (func_actor_136100_TakeStartCue(&evtId2, &evtKind2, &evtSub2) == 2 && work->scene == ACTOR_136100_SCENE_BEFORE_BURNER) {
                if (Gp_StateC08.mode == ATTACHMENT_MODE_WHEEL) {
                    return;
                }
                if (gDisplayState.pendingMode != DISPLAY_MODE_NONE) {
                    return;
                }
                ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(arg0, 1, 1, 0xA, message.animation);
                func_800E8634(D_actor_136100_8013FD84, 0, D_actor_136100_80140114);
                Gp_UnlinkObj4A(0, &D_dryfield_night_main_street_8018824C[9]);
                ACTOR_136100_COPY_PLAYER_ANIMATION_SETS(arg0, message.copy);
                gameFlagSetNibble(GAME_FLAG_NIGHT_MAIN_STREET_CUTSCENE_SEEN, 1);
                arg0->state++;
            }
            break;
        case 3:
            if (gGameSession->eventState == 0) {
                arg0->state++;
            }
            break;
        case 4:
            if ((s16)func_actor_136100_80133904(arg0) != 0) {
                arg0->state++;
            }
            break;
        case 5:
            if (gGameSession->eventState == 0) {
                arg0->state--;
            }
            break;
    }
    if (work->scene == ACTOR_136100_SCENE_AFTER_BURNER) {
        func_actor_136100_801323F8(arg0);
        func_actor_136100_80132748(arg0);
        func_actor_136100_80132BC0(arg0);
    } else {
        func_actor_136100_80132E78(arg0);
        func_actor_136100_80133238(arg0);
        func_actor_136100_80133558(arg0);
    }
    func_actor_136100_UpdateShadow(arg0, &message.shadowPosition);
    message.floorOffset.vx = 0;
    message.floorOffset.vy = 0x380;
    message.floorOffset.vz = 0;
    Gp_DrawFloorQuad(&arg0->extra.tmd->coords[1], 0x300, &message.floorOffset);
}

#include "../../shared/screen_fade_in.inc.c"

/// Display-fade task: on its first tick it allocates the 8-byte `r`/`g`/`b`
/// block, then every frame draws the fade overlay and steps all three channels
/// up by `spawnArg1`.  The fade ends once `r` reaches 0x100, at which point the
/// task blanks the display and kills itself; `D_actor_136100_8013F17C` makes it
/// kill itself immediately instead (the cutscene wants the display back).
void func_actor_136100_80134588(Task* arg0)
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
            if (D_actor_136100_8013F17C != 0) {
                taskKill(arg0);
                return;
            }
            if (fade->r < 0x100) {
                return;
            }
            SetDispMask(0);
            taskKill(arg0);
            break;
    }
}

void func_actor_136100_8013467C(void)
{
    AnimationPlayRequest rec;
    s32                  weaponId;
    s32                  id;

    weaponId                 = gPlayerStatus.weapon;
    id                       = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
    rec.source.index         = id;
    rec.animationId          = 1;
    rec.blend                = ANIMATION_BLEND_RESET;
    rec.blendFrames          = 0;
    rec.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &rec, 0);
}

/// Shows the task's model when `arg2` is non-zero and hides it (bit 0x80 of
/// its `TmdObject` flags) otherwise; `arg1` is unused.
void func_actor_136100_801346EC(Task* task, s32 arg1, s32 arg2, s32 arg3)
{
    TmdObject* obj;

    obj = task->extra.tmd;
    if (arg2 != 0) {
        obj->flags = obj->flags & (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
        return;
    }
    obj->flags = obj->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW;
}

#include "../../shared/actor_messages_place_in_view.inc.c"

void func_actor_136100_801347B8(void)
{
    _Actor136100Work* work = D_actor_136100_8014078C->work;
    SVECTOR           unused;
    s32               i;

    work->bodyAnimation = 1;
    i                   = 1;
    do {
        work->rig.slots[(u16)i].rate = ANIMATION_RATE_ONE;
        animationResetSlot(&work->rig.anim, (u16)i, 1);
        i++;
    } while ((u16)i < ARRAY_SIZE(work->rig.slots));
}

void func_actor_136100_80134838(s16 arg0)
{
    _Actor136100Work* work = D_actor_136100_8014078C->work;

    work->playerRequest     = arg0;
    work->playerRequestStep = 0;
}

void func_actor_136100_80134858(s16 arg0)
{
    _Actor136100Work* work = D_actor_136100_8014078C->work;

    work->bodyRequest     = arg0;
    work->bodyRequestStep = 0;
}

void func_actor_136100_80134878(s16 arg0)
{
    _Actor136100Work* work = D_actor_136100_8014078C->work;

    work->companionRequest     = arg0;
    work->companionRequestStep = 0;
}

/// Starts the fade-in (entry 4 of the actor's task table).
void func_actor_136100_80134898(void)
{
    taskSpawnFromTable(D_actor_136100_80140744, 4, 9, 0);
}

void func_actor_136100_801348C8(void)
{
    taskSpawnFromTable(D_actor_136100_80140744, 5, 9, 0);
}

void func_actor_136100_801348F8(void)
{
    D_actor_136100_8013F17C = 1;
    SetDispMask(1);
}

void func_actor_136100_80134924(void)
{
    _Actor136100Work* work = D_actor_136100_8014078C->work;

    if (work->playerEquipmentRemoved == 0) {
        work->playerEquipmentRemoved = 1;
        Gp_KillPlayerEffs();
    }
}

void func_actor_136100_80134964(void)
{
    _Actor136100Work* work = D_actor_136100_8014078C->work;

    if (work->playerEquipmentRemoved != 0) {
        Gp_SpawnWeaponEff();
        work->playerEquipmentRemoved = 0;
        Gp_MsgPlayerWeapon(0);
    }
}

void func_actor_136100_801349B4(s32 arg0)
{
    gameFlagSetNibble(GAME_FLAG_046, 0);
    gameFlagSetNibble(GAME_FLAG_COMPANION_1_SCHEDULE, 3);
    if (arg0 == 0) {
        gameFlagSetNibble(GAME_FLAG_COMPANION_2_SCHEDULE, 6);
    } else {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = 8;
        gameFlagSetNibble(GAME_FLAG_COMPANION_2_SCHEDULE, 0);
    }
}

/// Reports the live entries of the actor's pointer table to the slot-3 task:
/// counts the leading non-null words of `D_actor_136100_8013F180` and hands
/// the table and that count to message 0x3F7.
static void func_actor_136100_80134A18(Task* arg0)
{
    _Actor136100Work*        work = arg0->work;
    AnimationBankCopyRequest msg;
    s32                      n;

    n = 0;
    while (D_actor_136100_8013F180[n & 0xFFFF] != 0) {
        n += 1;
    }
    msg.source.sets = &D_actor_136100_8013F180[0];
    msg.wordCount   = n & 0xFFFF;
    TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, ANIMATION_MESSAGE_COPY_BANK_EXTENSION, &msg, 0);
}
