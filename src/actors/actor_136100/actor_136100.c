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
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
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

static void _screenFadeInTask(Task* task);
/// Selects this translation unit's private fade-in callback.
#define SCREEN_FADE_IN_TASK _screenFadeInTask
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

/// Requests consumed on later ticks; the selected scene determines each code.
///
/// Clip numbers identify loaded sets; their individual motions are unproven.
enum {
    ACTOR_136100_PLAYER_AFTER_ENTER             = 1,
    ACTOR_136100_PLAYER_AFTER_PLAY_ANIMATION_50 = 2,
    ACTOR_136100_PLAYER_AFTER_PLAY_ANIMATION_52 = 3,
    ACTOR_136100_PLAYER_AFTER_RESET_PLACEMENT   = 4,
    ACTOR_136100_PLAYER_AFTER_BLEND_WEAPON      = 5,
    ACTOR_136100_PLAYER_AFTER_RESET_WEAPON      = 6,
};

enum {
    ACTOR_136100_PLAYER_BEFORE_HIDE                  = 1,
    ACTOR_136100_PLAYER_BEFORE_ENTER                 = 2,
    ACTOR_136100_PLAYER_BEFORE_PLAY_ANIMATION_49     = 3,
    ACTOR_136100_PLAYER_BEFORE_PLAY_ANIMATION_53     = 4,
    ACTOR_136100_PLAYER_BEFORE_EXIT                  = 5,
    ACTOR_136100_PLAYER_BEFORE_RESET_SCENE_ANIMATION = 6,
    ACTOR_136100_PLAYER_BEFORE_BLEND_WEAPON          = 7,
};

enum {
    ACTOR_136100_BODY_AFTER_PLAY_ANIMATION_2          = 1,
    ACTOR_136100_BODY_AFTER_PLAY_ANIMATION_5          = 2,
    ACTOR_136100_BODY_AFTER_BLEND_ANIMATION_7         = 3,
    ACTOR_136100_BODY_AFTER_BLEND_ANIMATION_9         = 4,
    ACTOR_136100_BODY_AFTER_PLAY_ANIMATION_10_THEN_11 = 5,
    ACTOR_136100_BODY_AFTER_RESET                     = 6,
};

enum {
    ACTOR_136100_BODY_BEFORE_PLACE_FIRST                = 1,
    ACTOR_136100_BODY_BEFORE_PLACE_SECOND               = 2,
    ACTOR_136100_BODY_BEFORE_BLEND_ANIMATION_4          = 3,
    ACTOR_136100_BODY_BEFORE_RESET_AND_PLACE            = 4,
    ACTOR_136100_BODY_BEFORE_PLAY_ANIMATION_6_AND_SOUND = 5,
    ACTOR_136100_BODY_BEFORE_TURN_HEAD                  = 6,
    ACTOR_136100_BODY_BEFORE_RETURN_HEAD                = 7,
    ACTOR_136100_BODY_BEFORE_RESET                      = 8,
};

enum {
    ACTOR_136100_COMPANION_AFTER_RESET_ANIMATION_1             = 1,
    ACTOR_136100_COMPANION_AFTER_PLAY_REPEATED_CUE             = 2,
    ACTOR_136100_COMPANION_AFTER_RESET_PLACEMENT_AND_ANIMATION = 3,
};

enum {
    ACTOR_136100_COMPANION_BEFORE_PLACE_FIRST     = 1,
    ACTOR_136100_COMPANION_BEFORE_PLACE_SECOND    = 2,
    ACTOR_136100_COMPANION_BEFORE_RESET_PLACEMENT = 3,
};

/// Bank selection and playback defaults of this package's street scenes.
enum {
    ACTOR_136100_PRIMARY_CHARACTER          = 1,
    ACTOR_136100_PRIMARY_WEAPON_BANK_BASE   = 1,
    ACTOR_136100_ALTERNATE_WEAPON_BANK_BASE = 0x22,
    ACTOR_136100_PLAYER_WEAPON_ANIMATION    = 1,
    ACTOR_136100_BODY_AFTER_BURNER_START    = 1,
    ACTOR_136100_BODY_BEFORE_BURNER_START   = 3,
    ACTOR_136100_ANIMATION_BLEND_FRAMES     = 10,
    ACTOR_136100_REQUEST_NONE               = 0,
};

/// Opening-scene cue classifications and the room actions selecting street scenes.
enum {
    ACTOR_136100_OPENING_CUE_NONE                    = 0,
    ACTOR_136100_OPENING_CUE_AFTER_BURNER            = 1,
    ACTOR_136100_OPENING_CUE_BEFORE_BURNER           = 2,
    ACTOR_136100_ROOM_ACTION_OPEN_AFTER_BURNER       = 0x10,
    ACTOR_136100_ROOM_ACTION_OPEN_BEFORE_BURNER      = 0x11,
    ACTOR_136100_ROOM_ACTION_FOLLOW_UP_AFTER_BURNER  = 0x12,
    ACTOR_136100_ROOM_ACTION_FOLLOW_UP_BEFORE_BURNER = 0x13,
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

/// Set by `_actor136100RestoreDisplay` when the cutscene wants the display
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

extern AnimationSet*          D_actor_136100_8013F180[8];
extern AnimationSet*          D_actor_136100_8013F1A0[13];
extern AnimationSet*          D_actor_136100_8013F1D4[6];
extern s16                    D_actor_136100_8013F218[];
extern SVECTOR                D_actor_136100_8013F224[4];
extern SVECTOR                D_actor_136100_8013F244[16];
extern WorldCollisionGridFace D_actor_136100_8013F2C4[4];
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

static void _actor136100UpdateAfterBurnerBodyRequest(Task* task);
static void _actor136100UpdateBeforeBurnerBodyRequest(Task* task);
static void _actor136100CopyPlayerSceneAnimations(Task* task);

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
static void      _actor136100RifleTask(Task* task);
static void      _actor136100ResetAfterBurnerScene(void);
static void      _actor136100ResetBeforeBurnerScene(s32 resetHeadRotation);
void             func_actor_136100_80133BC8(Task*);
static void      _actor136100FadeOutTask(Task* task);
static void      _actor136100ResetPlayerWeaponAnimation(void);
static s32       _actor136100SetModelDraw(Task* task, s32 unusedMessageId, s32 visible, s32 unusedSecondArg);
static void      _actor136100ResetBodyAnimation(void);
static void      _actor136100PostPlayerRequest(s16 request);
static void      _actor136100PostBodyRequest(s16 request);
static void      _actor136100PostCompanionRequest(s16 request);
static void      _actor136100StartFadeIn(void);
static void      _actor136100StartFadeOut(void);
static void      _actor136100RestoreDisplay(void);
static void      _actor136100RemovePlayerEquipment(void);
void             func_actor_136100_80134964(void);
static void      _actor136100FinishOpeningScene(s32 beforeBurner);

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

SVECTOR D_actor_136100_8013F224[4] = {
#include "assets/actor_136100_collision_0D404.inc"
};

SVECTOR D_actor_136100_8013F244[16] = {
#include "assets/actor_136100_collision_0D424.inc"
};

WorldCollisionGridFace D_actor_136100_8013F2C4[4] = {
#include "assets/actor_136100_collision_0D4A4.inc"
};

TaskMessageEntry D_actor_136100_8013F2F4[2] = {
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor136100SetModelDraw },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor136100ResetPlayerWeaponAnimation }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor136100RemovePlayerEquipment }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor136100StartFadeIn }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostPlayerRequest }, { .value = ACTOR_136100_PLAYER_AFTER_ENTER }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostCompanionRequest }, { .value = ACTOR_136100_COMPANION_AFTER_PLAY_REPEATED_CUE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostBodyRequest }, { .value = ACTOR_136100_BODY_AFTER_PLAY_ANIMATION_10_THEN_11 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostPlayerRequest }, { .value = ACTOR_136100_PLAYER_AFTER_PLAY_ANIMATION_50 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostBodyRequest }, { .value = ACTOR_136100_BODY_AFTER_PLAY_ANIMATION_2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostBodyRequest }, { .value = ACTOR_136100_BODY_AFTER_PLAY_ANIMATION_5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor136100StartFadeOut }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostPlayerRequest }, { .value = ACTOR_136100_PLAYER_AFTER_RESET_WEAPON }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_80134964 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor136100FinishOpeningScene }, { .value = ACTOR_136100_SCENE_AFTER_BURNER }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor136100RestoreDisplay }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostPlayerRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostBodyRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostCompanionRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_136100_8013F784[19] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostPlayerRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostBodyRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostCompanionRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_80134964 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor136100ResetAfterBurnerScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor136100FinishOpeningScene }, { .value = ACTOR_136100_SCENE_AFTER_BURNER }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor136100RestoreDisplay }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor136100ResetPlayerWeaponAnimation }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostPlayerRequest }, { .value = ACTOR_136100_PLAYER_AFTER_RESET_PLACEMENT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor136100RemovePlayerEquipment }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostBodyRequest }, { .value = ACTOR_136100_BODY_AFTER_PLAY_ANIMATION_10_THEN_11 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostPlayerRequest }, { .value = ACTOR_136100_PLAYER_AFTER_RESET_WEAPON }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_80134964 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostPlayerRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostBodyRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostCompanionRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_136100_8013FAE4[16] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_136100_8013F37C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_80134964 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor136100ResetPlayerWeaponAnimation }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor136100ResetBodyAnimation }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostPlayerRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostBodyRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostCompanionRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostPlayerRequest }, { .value = ACTOR_136100_PLAYER_AFTER_BLEND_WEAPON }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostBodyRequest }, { .value = ACTOR_136100_BODY_AFTER_PLAY_ANIMATION_10_THEN_11 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostPlayerRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostBodyRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostCompanionRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_136100_8013FD84[38] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 15 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor136100ResetPlayerWeaponAnimation }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor136100RemovePlayerEquipment }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor136100StartFadeIn }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostPlayerRequest }, { .value = ACTOR_136100_PLAYER_BEFORE_HIDE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostBodyRequest }, { .value = ACTOR_136100_BODY_BEFORE_PLACE_FIRST }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostCompanionRequest }, { .value = ACTOR_136100_COMPANION_BEFORE_PLACE_FIRST }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostBodyRequest }, { .value = ACTOR_136100_BODY_BEFORE_PLACE_SECOND }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostCompanionRequest }, { .value = ACTOR_136100_COMPANION_BEFORE_PLACE_SECOND }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostPlayerRequest }, { .value = ACTOR_136100_PLAYER_BEFORE_ENTER }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostPlayerRequest }, { .value = ACTOR_136100_PLAYER_BEFORE_PLAY_ANIMATION_53 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostBodyRequest }, { .value = ACTOR_136100_BODY_BEFORE_BLEND_ANIMATION_4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostBodyRequest }, { .value = ACTOR_136100_BODY_BEFORE_PLAY_ANIMATION_6_AND_SOUND }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostPlayerRequest }, { .value = ACTOR_136100_PLAYER_BEFORE_PLAY_ANIMATION_49 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor136100StartFadeOut }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostPlayerRequest }, { .value = ACTOR_136100_PLAYER_BEFORE_EXIT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostBodyRequest }, { .value = ACTOR_136100_BODY_BEFORE_RESET_AND_PLACE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostCompanionRequest }, { .value = ACTOR_136100_COMPANION_BEFORE_RESET_PLACEMENT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_80134964 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor136100FinishOpeningScene }, { .value = ACTOR_136100_SCENE_BEFORE_BURNER }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor136100RestoreDisplay }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostPlayerRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostBodyRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostCompanionRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_136100_80140114[18] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostPlayerRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostBodyRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostCompanionRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_80134964 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor136100ResetBeforeBurnerScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor136100FinishOpeningScene }, { .value = ACTOR_136100_SCENE_BEFORE_BURNER }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor136100RestoreDisplay }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor136100ResetPlayerWeaponAnimation }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_136100_8013F334[1] } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor136100RemovePlayerEquipment }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostPlayerRequest }, { .value = ACTOR_136100_PLAYER_BEFORE_RESET_SCENE_ANIMATION }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostBodyRequest }, { .value = ACTOR_136100_BODY_BEFORE_TURN_HEAD }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostCompanionRequest }, { .value = ACTOR_136100_COMPANION_BEFORE_PLACE_SECOND }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostBodyRequest }, { .value = ACTOR_136100_BODY_BEFORE_RETURN_HEAD }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostPlayerRequest }, { .value = ACTOR_136100_PLAYER_BEFORE_EXIT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostBodyRequest }, { .value = ACTOR_136100_BODY_BEFORE_RESET_AND_PLACE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostCompanionRequest }, { .value = ACTOR_136100_COMPANION_BEFORE_RESET_PLACEMENT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_80134964 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostPlayerRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostBodyRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostCompanionRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_136100_801404EC[14] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostPlayerRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostBodyRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostCompanionRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_136100_80134964 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor136100ResetBeforeBurnerScene }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostPlayerRequest }, { .value = ACTOR_136100_PLAYER_BEFORE_BLEND_WEAPON }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostPlayerRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostBodyRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor136100PostCompanionRequest }, { .value = ACTOR_136100_REQUEST_NONE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc D_actor_136100_80140744[6] = {
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_136100_80133BC8, { .model = &_gActor136100GaryDouglasBody } },
    { { { TASK_BODY_NONE, 192 } }, NULL, { .value = 0 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_136100_801320E0, { .model = &_gActor136100GaryDouglasHeadHat } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, _actor136100RifleTask, { .model = &_gActor136100Actor120300Model082F8 } },
    { { { TASK_BODY_NONE, 192 } }, _screenFadeInTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _actor136100FadeOutTask, { .value = 0 } },
};

Task* D_actor_136100_8014078C = NULL;

static void        _actor136100UpdateAfterBurnerPlayerRequest(Task* task);
static inline void _actor136100ResetBodyTracks(Task* task, s32 setIndex);
static inline void _actor136100PlayCompanionAnimation(Task* task, u16 setIndex, s32 blendChoice, s32 blendFrames);
static void        _actor136100UpdateAfterBurnerCompanionRequest(Task* task);
static void        _actor136100UpdateBeforeBurnerPlayerRequest(Task* task);
static void        _actor136100UpdateBeforeBurnerCompanionRequest(Task* task);
static s16         _actor136100TryStartFollowUpScene(Task* task);
static void        func_actor_136100_80133A88(Task* task);
static inline s16  _actor136100ReadOpeningSceneCue(u16* triggerControl, u8* sceneAction, u8* actionArgument);
static inline void _actor136100UpdateBodyLighting(Task* task, VECTOR* samplePosition);

/// Polls the player's scene animation and starts its configured successor.
///
/// Returns 0 while the player animation is playing, otherwise 1, including
/// when a successor has just been started or no player is registered. Scene
/// animations use the installed extension at `ANIMATION_BANK_BASE_SET_COUNT`;
/// a negative successor ends the chain. Playback blends for ten normal-rate
/// frames and disables world collision. The controller work, player and
/// installed clip data must remain live through synchronous dispatch. Scene
/// animation IDs written by this package span 47 to 53; lower base-bank IDs
/// have no scene successor, and there is no upper-bound check here.
static s32 _actor136100AdvancePlayerAnimation(Task* task)
{
    _Actor136100Work*    work;
    _Actor136100Work*    requestWork;
    AnimationPlayRequest request;
    const s16*           successorEntry;
    s32                  chainIndex;
    u16                  nextAnimationId;
    s32                  weaponId;
    s32                  selection;

    // Poll before choosing a successor from the installed scene clips.
    work = task->work;
    if (work->playerTask == NULL) {
        return 1;
    }
    if (taskMessageDispatch(work->playerTask, ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
        if ((u16)work->playerAnimation >= (u32)ANIMATION_BANK_BASE_SET_COUNT) {
            chainIndex     = (u16)work->playerAnimation - (u32)ANIMATION_BANK_BASE_SET_COUNT;
            successorEntry = &D_actor_136100_8013F1EC[chainIndex];
            selection      = *successorEntry;
            if (selection >= 0) {
                nextAnimationId = (u16)*successorEntry + (u32)ANIMATION_BANK_BASE_SET_COUNT;

                requestWork                  = task->work;
                weaponId                     = gPlayerStatus.weapon;
                selection                    = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == ACTOR_136100_PRIMARY_CHARACTER) ? weaponId + ACTOR_136100_PRIMARY_WEAPON_BANK_BASE : weaponId + ACTOR_136100_ALTERNATE_WEAPON_BANK_BASE;
                request.source.index         = selection;
                requestWork->playerAnimation = nextAnimationId;
                request.animationId          = nextAnimationId;
                request.blend                = ANIMATION_BLEND_INTERPOLATE;
                request.blendFrames          = ACTOR_136100_ANIMATION_BLEND_FRAMES;
                request.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                TASK_MESSAGE_DISPATCH_POINTER(requestWork->playerTask, ANIMATION_MESSAGE_PLAY, &request, 0);
            }
        }
        return 1;
    }
    return 0;
}

/// Advances body tracks 1 to 19 and blends into their configured successor.
///
/// Returns 0 while any track is unsettled, otherwise 1, including when a
/// successor has just been started. `bodyAnimation` must index the successor
/// table; nonnegative successors must index the rig's loaded set table. A
/// negative entry leaves the settled pose. Blends last ten normal-rate frames;
/// slot 0 is untouched. The initialized rig and its model must remain live.
static s32 _actor136100TickBodyAnimation(Task* task)
{
    _Actor136100Work* work;
    _Actor136100Work* restartWork;
    u16               slotIndex;
    u16               settled;
    u16               nextSetIndex;
    u16               setIndex;

    // Tick every driven track before deciding whether the body has settled.
    work = task->work;
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        animationTickSlot(&work->rig.anim, slotIndex);
    }
    for (settled = slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        if (!(work->rig.slots[slotIndex].status.fields.flags & ANIMATION_SLOT_SETTLED)) {
            goto unsettled;
        }
    }
checkSettled:
    if (settled) {
        nextSetIndex = D_actor_136100_8013F1FC[(u16)work->bodyAnimation];
        if (D_actor_136100_8013F1FC[(u16)work->bodyAnimation] >= 0) {
            setIndex                   = nextSetIndex;
            restartWork                = task->work;
            restartWork->bodyAnimation = setIndex;
            goto restartSlots;
        unsettled:
            settled = 0;
            goto checkSettled;
        restartSlots:
            for (slotIndex = 1; slotIndex < ARRAY_SIZE(restartWork->rig.slots); slotIndex++) {
                animationSeekSlotWithBlend(&restartWork->rig.anim, slotIndex, setIndex, 0, ACTOR_136100_ANIMATION_BLEND_FRAMES);
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
/// three coordinate reads (see `_actor136100RifleTask`).
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
        place = areaGetVariant(&gGameSession->location.loc)->placements;
        id    = place->entryId;
        while (id != AREA_PLACEMENT_END) {
            if (id == 0x6A) {
                break;
            }
            place++;
            id = place->entryId;
        }
        tmdSetTextureOffsets(task->extra.tmd, place->texturePageOffset, place->clutRowOffset);
        task->state += 1;
    }
    {
        TmdObject* obj = task->extra.tmd;

        actorRenderComposeCoord(obj->coords);
        vec.vx = task->extra.tmd->coords->workm.t[0];
        vec.vy = task->extra.tmd->coords->workm.t[1];
        vec.vz = task->extra.tmd->coords->workm.t[2];
        worldCoordSetModelLighting(obj, &vec, 0, 3);
    }
}

/// Allocates the rifle's lighting work, attaches the model and applies its spawn pose.
///
/// Requires a live TMD model, the live scene controller and a borrowed parent
/// `GfxCoord` in `spawnArg2`. Owns a zeroed primary-heap `_Actor136100Work`,
/// using only its lighting matrices, and joins the controller's teardown tree.
/// Nonzero `spawnArg1` replaces pitch with a quarter turn and sets local Y to
/// 200 units. Allocation failure kills the task but still advances its state
/// and applies that optional pose.
static inline void _actor136100InitializeRifle(Task* task)
{
    _Actor136100Work* work;

    TmdObject* model     = task->extra.tmd;
    GfxCoord*  rootCoord = model->coords;

    work       = memMalloc(sizeof(*work), false);
    task->work = work;
    if (work == NULL) {
        taskKill(task);
    } else {
        memFillBytes(work, 0, sizeof(*work));
        rootCoord->parent      = task->spawnArg2.pointer;
        task->extra.tmd->flags = 0;
        tmdAllocPrimitiveBuffer(model);
        model->lightMtx = &work->light;
        model->colorMtx = &work->color;
        task->msgTable  = D_actor_136100_8013F2F4;
        taskReparent(D_actor_136100_8014078C, task);
    }
    task->state += 1;
    if (task->spawnArg1.value != 0) {
        GfxCoord* posedRoot = task->extra.tmd->coords;

        gfxRotMatrixX(&posedRoot->coord, ACTOR_TRANSFORM_ANGLE_TURN / 4, GRAPHICS_ROTATION_REPLACE);
        posedRoot->coord.t[1]   = 200;
        posedRoot->composeStamp = GRAPHICS_COORD_DIRTY;
    }
}

/// Initializes and lights Gary Douglas's rifle model for the street scene.
///
/// Start at state 0 with a live model and a borrowed parent `GfxCoord` in
/// `spawnArg2`. The task owns a full `_Actor136100Work` until teardown, using
/// only its lighting matrices, and joins the scene controller's teardown tree.
/// Nonzero `spawnArg1` gives the attached rifle a quarter-turn pitch and a
/// 200-unit local Y offset. Later ticks refresh lighting at the composed root.
/// Allocation failure kills the task but retains the remaining first-tick
/// state and pose updates; callers must preserve that behavior.
static void _actor136100RifleTask(Task* task)
{
    VECTOR worldPosition;

    if (task->state == 0) {
        _actor136100InitializeRifle(task);
    }
    // Refresh lighting in world coordinates after composing the model root.
    {
        TmdObject* model = task->extra.tmd;

        actorRenderComposeCoord(model->coords);
        worldPosition.vx = task->extra.tmd->coords->workm.t[0];
        worldPosition.vy = task->extra.tmd->coords->workm.t[1];
        worldPosition.vz = task->extra.tmd->coords->workm.t[2];
        worldCoordSetModelLighting(model, &worldPosition, 0, 3);
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

/// Places the player at a scene entrance and starts its scripted walk.
///
/// Requires the live player and two borrowed placements: start and destination.
/// Dispatch copies the destination before the entrance wait counters are reset.
static inline void _actor136100BeginPlayerEntrance(_Actor136100Work* work, const ActorTransform* path)
{
    TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, GAME_ACTOR_MESSAGE_PLACE, &path[0], 0);
    TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, GAME_ACTOR_MESSAGE_MOVE_TO, &path[1], 0);
    work->playerRequestFrames = 0;
    work->playerRequestStep++;
}

/// Advances player requests for the street scene after the Burner encounter.
///
/// Requires live controller work, player and loaded weapon/scene animation
/// banks. During an event, advances the player's animation chain first.
/// Entering places the player, starts a scripted walk, waits for it to finish
/// and waits six more ticks before blending into scene clip 47 for five frames.
/// Other requests place or start a clip immediately. Completed requests clear;
/// pending entrance steps survive to the next tick. Dispatch borrows requests
/// only during each call.
static void _actor136100UpdateAfterBurnerPlayerRequest(Task* task)
{
    enum { PLACE_AND_WALK        = 0,
           WAIT_FOR_WALK         = 1,
           WAIT_BEFORE_ANIMATION = 2,
           POSE_DELAY_FRAMES     = 6,
           POSE_BLEND_FRAMES     = 5 };

    _Actor136100Work*    work = task->work;
    AnimationPlayRequest request;

    if (gGameSession->eventState != 0) {
        _actor136100AdvancePlayerAnimation(task);
    }
    // Keep the entrance request pending through the walk and pose delay.
    switch ((u16)work->playerRequest) {
        case ACTOR_136100_REQUEST_NONE:
            break;
        case ACTOR_136100_PLAYER_AFTER_ENTER:
            switch ((u16)work->playerRequestStep) {
                case PLACE_AND_WALK:
                    _actor136100BeginPlayerEntrance(work, D_actor_136100_8013F304);
                    return;
                case WAIT_FOR_WALK:
                    if (taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
                        work->playerRequestStep++;
                    }
                    return;
                case WAIT_BEFORE_ANIMATION:
                    if (++work->playerRequestFrames < POSE_DELAY_FRAMES) {
                        return;
                    }
                    ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(task, ANIMATION_BANK_BASE_SET_COUNT, ANIMATION_BLEND_INTERPOLATE, POSE_BLEND_FRAMES, request);
                    break;
                default:
                    return;
            }
            break;
        case ACTOR_136100_PLAYER_AFTER_PLAY_ANIMATION_50:
            ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(task, 50, ANIMATION_BLEND_INTERPOLATE, ACTOR_136100_ANIMATION_BLEND_FRAMES, request);
            break;
        case ACTOR_136100_PLAYER_AFTER_PLAY_ANIMATION_52:
            ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(task, 52, ANIMATION_BLEND_INTERPOLATE, ACTOR_136100_ANIMATION_BLEND_FRAMES, request);
            break;
        case ACTOR_136100_PLAYER_AFTER_RESET_PLACEMENT:
            TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, GAME_ACTOR_MESSAGE_PLACE, &D_actor_136100_8013F37C, 0);
            ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(task, ANIMATION_BANK_BASE_SET_COUNT, ANIMATION_BLEND_RESET, 0, request);
            break;
        case ACTOR_136100_PLAYER_AFTER_BLEND_WEAPON:
            ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(task, ACTOR_136100_PLAYER_WEAPON_ANIMATION, ANIMATION_BLEND_INTERPOLATE, ACTOR_136100_ANIMATION_BLEND_FRAMES, request);
            break;
        case ACTOR_136100_PLAYER_AFTER_RESET_WEAPON:
            ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(task, ACTOR_136100_PLAYER_WEAPON_ANIMATION, ANIMATION_BLEND_RESET, 0, request);
            break;
    }
    work->playerRequest = ACTOR_136100_REQUEST_NONE;
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

/// Blends body tracks 1 to 19 into `setIndex` and records that selection.
///
/// `task` must own an initialized `_Actor136100Work` rig. The signed halfword
/// set index must select loaded animation data for every driven track; current
/// callers use 2, 4, 5, 6, 7, 9, 10 and 11. Blends last ten normal-rate frames
/// from record 0. Track 0 and playback rates are untouched.
static inline void _actor136100BlendBodyAnimation(Task* task, s16 setIndex)
{
    _Actor136100Work* work = task->work;
    s32               slotIndex;

    _actor136100SetAnimId(work, setIndex);
    for (slotIndex = 1; (u16)slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        animationSeekSlotWithBlend(&work->rig.anim, (u16)slotIndex, setIndex, 0, ACTOR_136100_ANIMATION_BLEND_FRAMES);
    }
}

/// Restarts body tracks 1 to 19 at normal rate on a selected animation set.
///
/// Requires an initialized controller rig and loaded data for all driven
/// tracks. `setIndex` selects the rig's set table, not a track count; callers
/// use 1 or 3. The selection is cached as a signed halfword, while the full
/// signed word is passed to slot reset, which uses its low unsigned halfword.
/// Track 0 is untouched. The rig, slots and borrowed set data must stay live.
static inline void _actor136100ResetBodyTracks(Task* task, s32 setIndex)
{
    _Actor136100Work* work = task->work;
    s32               slotIndex;

    work->bodyAnimation = setIndex;
    slotIndex           = 1;
    do {
        work->rig.slots[(u16)slotIndex].rate = ANIMATION_RATE_ONE;
        animationResetSlot(&work->rig.anim, (u16)slotIndex, setIndex);
        slotIndex++;
    } while ((u16)slotIndex < ARRAY_SIZE(work->rig.slots));
}

/// Ticks the body rig and advances requests after the Burner encounter.
///
/// Requires initialized body tracks 1..19, loaded scene clips and a live player.
/// Animation messages tolerate an absent companion. Timed requests coordinate
/// body clips, companion clips and sound cues using frame counters; pending
/// steps survive to the next tick and completed requests clear. Track 0 is
/// untouched. Player and companion messages consume stack requests synchronously.
static void _actor136100UpdateAfterBurnerBodyRequest(Task* task)
{
    enum { BEGIN_REQUEST                   = 0,
           WAIT_FOR_CUE                    = 1,
           COMPANION_CUE_FRAMES            = 61,
           BODY_SOUND_FRAMES               = 17,
           PLAYER_AND_COMPANION_CUE_FRAMES = 21,
           SECOND_BODY_ANIMATION_FRAMES    = 80,
           BODY_SOUND_SCRIPT               = 0x0E };

    _Actor136100Work*    work = task->work;
    AnimationPlayRequest request;

    // Tick the rig before applying this frame's timed performance cues.
    _actor136100TickBodyAnimation(task);
    switch ((u16)work->bodyRequest) {
        case ACTOR_136100_REQUEST_NONE:
            break;
        case ACTOR_136100_BODY_AFTER_PLAY_ANIMATION_2:
            switch ((u16)work->bodyRequestStep) {
                case BEGIN_REQUEST:
                    work->bodyRequestFrames = 0;
                    _actor136100BlendBodyAnimation(task, 2);
                    work->bodyRequestStep++;
                    return;
                case WAIT_FOR_CUE:
                    if (++work->bodyRequestFrames < COMPANION_CUE_FRAMES) {
                        return;
                    }
                    ACTOR_136100_PLAY_LINKED_ANIMATION(task, 3, ANIMATION_BLEND_INTERPOLATE, ACTOR_136100_ANIMATION_BLEND_FRAMES, request);
                    break;
                default:
                    return;
            }
            break;
        case ACTOR_136100_BODY_AFTER_PLAY_ANIMATION_5:
            switch ((u16)work->bodyRequestStep) {
                case BEGIN_REQUEST:
                    work->bodyRequestFrames = 0;
                    _actor136100BlendBodyAnimation(task, 5);
                    work->bodyRequestStep++;
                    return;
                case WAIT_FOR_CUE:
                    if (++work->bodyRequestFrames == BODY_SOUND_FRAMES) {
                        sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MAIN_STREET, BODY_SOUND_SCRIPT), 0, 0);
                    }
                    if (work->bodyRequestFrames < PLAYER_AND_COMPANION_CUE_FRAMES) {
                        return;
                    }
                    ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(task, ANIMATION_BANK_BASE_SET_COUNT + 1, ANIMATION_BLEND_INTERPOLATE, ACTOR_136100_ANIMATION_BLEND_FRAMES, request);
                    ACTOR_136100_PLAY_LINKED_ANIMATION(task, 4, ANIMATION_BLEND_INTERPOLATE, ACTOR_136100_ANIMATION_BLEND_FRAMES, request);
                    break;
                default:
                    return;
            }
            break;
        case ACTOR_136100_BODY_AFTER_BLEND_ANIMATION_7:
            _actor136100BlendBodyAnimation(task, 7);
            break;
        case ACTOR_136100_BODY_AFTER_BLEND_ANIMATION_9:
            _actor136100BlendBodyAnimation(task, 9);
            break;
        case ACTOR_136100_BODY_AFTER_PLAY_ANIMATION_10_THEN_11:
            switch ((u16)work->bodyRequestStep) {
                case BEGIN_REQUEST:
                    work->bodyRequestFrames = 0;
                    _actor136100BlendBodyAnimation(task, 10);
                    work->bodyRequestStep++;
                    return;
                case WAIT_FOR_CUE:
                    if (++work->bodyRequestFrames < SECOND_BODY_ANIMATION_FRAMES) {
                        return;
                    }
                    _actor136100BlendBodyAnimation(task, 11);
                    break;
                default:
                    return;
            }
            break;
        case ACTOR_136100_BODY_AFTER_RESET:
            _actor136100ResetBodyTracks(task, ACTOR_136100_BODY_AFTER_BURNER_START);
            break;
    }
    work->bodyRequest = ACTOR_136100_REQUEST_NONE;
}

/// Installs the scene's companion sets and starts the selected animation.
///
/// `setIndex` is 0..5 in the installed table. Zero `blendChoice` resets playback;
/// nonzero requests interpolation over `blendFrames` normal-rate frames.
/// Caches the selection and disables world collision. Does nothing when no
/// companion is registered. The controller and loaded clip data must remain
/// live; dispatch consumes the stack request synchronously and borrows the sets.
static inline void _actor136100PlayCompanionAnimation(Task* task, u16 setIndex, s32 blendChoice, s32 blendFrames)
{
    _Actor136100Work*    work = task->work;
    AnimationPlayRequest request;

    if (work->companionTask != NULL) {
        request.source.sets          = D_actor_136100_8013F1D4;
        work->companionAnimation     = setIndex;
        request.animationId          = setIndex;
        request.blend                = blendChoice;
        request.blendFrames          = blendFrames;
        request.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
        TASK_MESSAGE_DISPATCH_POINTER(work->companionTask, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &request, 0);
    }
}

/// Advances the companion's animation chain and after-Burner requests.
///
/// The cached set index must be 0..5; negative successors leave the final pose.
/// Animation updates tolerate an absent companion, but placement requests
/// require a live registered task. The repeated cue plays sound and set 2 three
/// times, fifteen ticks apart, then waits another interval before blending to
/// set 1. Pending repetitions survive and completed requests clear. Loaded
/// companion sets remain borrowed throughout playback.
static void _actor136100UpdateAfterBurnerCompanionRequest(Task* task)
{
    enum { BEGIN_REPEATS          = 0,
           PLAY_REPEATS           = 1,
           REPEAT_COUNT           = 3,
           REPEAT_INTERVAL_FRAMES = 15,
           REPEAT_SOUND_SCRIPT    = 9 };

    _Actor136100Work* work;
    s16               successorAnimation;

    work = task->work;
    if (work->companionTask != NULL && taskMessageDispatch(work->companionTask, ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
        successorAnimation = D_actor_136100_8013F218[work->companionAnimation];
        if (successorAnimation >= 0) {
            _actor136100PlayCompanionAnimation(task, successorAnimation, ANIMATION_BLEND_INTERPOLATE, ACTOR_136100_ANIMATION_BLEND_FRAMES);
        }
    }
    // Each repetition waits a full interval; the final wait returns to set 1.
    switch ((u16)work->companionRequest) {
        case ACTOR_136100_REQUEST_NONE:
            break;
        case ACTOR_136100_COMPANION_AFTER_RESET_ANIMATION_1:
            _actor136100PlayCompanionAnimation(task, 1, ANIMATION_BLEND_RESET, 0);
            break;
        case ACTOR_136100_COMPANION_AFTER_PLAY_REPEATED_CUE:
            switch ((u16)work->companionRequestStep) {
                case BEGIN_REPEATS:
                    work->companionRepeatCount = 0;
                    work->companionRepeatDelay = 0;
                    work->companionRequestStep++;
                    return;
                case PLAY_REPEATS:
                    if (--work->companionRepeatDelay > 0) {
                        return;
                    }
                    if (work->companionRepeatCount >= REPEAT_COUNT) {
                        _actor136100PlayCompanionAnimation(task, 1, ANIMATION_BLEND_INTERPOLATE, ACTOR_136100_ANIMATION_BLEND_FRAMES);
                        break;
                    }
                    sndEvtRequestScriptStart(SOUND_CHARACTER(SOUND_BANK_ACTOR_800200, REPEAT_SOUND_SCRIPT), 0, 0);
                    _actor136100PlayCompanionAnimation(task, 2, ANIMATION_BLEND_INTERPOLATE, ACTOR_136100_ANIMATION_BLEND_FRAMES);
                    work->companionRepeatDelay = REPEAT_INTERVAL_FRAMES;
                    work->companionRepeatCount++;
                    return;
                default:
                    return;
            }
            break;
        case ACTOR_136100_COMPANION_AFTER_RESET_PLACEMENT_AND_ANIMATION:
            TASK_MESSAGE_DISPATCH_POINTER(work->companionTask, GAME_ACTOR_MESSAGE_PLACE, &D_actor_136100_8013F3F4, 0);
            _actor136100PlayCompanionAnimation(task, 0, ANIMATION_BLEND_RESET, 0);
            break;
    }
    work->companionRequest = ACTOR_136100_REQUEST_NONE;
}

/// Advances player requests for the street scene before the Burner encounter.
///
/// Requires live controller work, player and loaded weapon/scene animation
/// banks. During an event, advances the player's animation chain first.
/// Entering shows and places the player, starts a scripted walk, waits for it
/// to finish and waits six ticks before blending to scene clip 47 for five
/// frames. Other requests hide, place or start an animation immediately.
/// Completed requests clear; pending entrance steps survive to the next tick.
static void _actor136100UpdateBeforeBurnerPlayerRequest(Task* task)
{
    enum { PLACE_AND_WALK        = 0,
           WAIT_FOR_WALK         = 1,
           WAIT_BEFORE_ANIMATION = 2,
           POSE_DELAY_FRAMES     = 6,
           POSE_BLEND_FRAMES     = 5 };

    _Actor136100Work*    work = task->work;
    AnimationPlayRequest request;

    if (gGameSession->eventState != 0) {
        _actor136100AdvancePlayerAnimation(task);
    }
    // Keep the entrance request pending through the walk and pose delay.
    switch ((u16)work->playerRequest) {
        case ACTOR_136100_REQUEST_NONE:
            break;
        case ACTOR_136100_PLAYER_BEFORE_HIDE:
            taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE, 0);
            ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(task, ACTOR_136100_PLAYER_WEAPON_ANIMATION, ANIMATION_BLEND_RESET, 0, request);
            break;
        case ACTOR_136100_PLAYER_BEFORE_ENTER:
            switch ((u16)work->playerRequestStep) {
                case PLACE_AND_WALK:
                    taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO, 0);
                    _actor136100BeginPlayerEntrance(work, D_actor_136100_8013F334);
                    return;
                case WAIT_FOR_WALK:
                    if (taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
                        work->playerRequestStep++;
                    }
                    return;
                case WAIT_BEFORE_ANIMATION:
                    if (++work->playerRequestFrames < POSE_DELAY_FRAMES) {
                        return;
                    }
                    ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(task, ANIMATION_BANK_BASE_SET_COUNT, ANIMATION_BLEND_INTERPOLATE, POSE_BLEND_FRAMES, request);
                    break;
                default:
                    return;
            }
            break;
        case ACTOR_136100_PLAYER_BEFORE_PLAY_ANIMATION_49:
            ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(task, 49, ANIMATION_BLEND_INTERPOLATE, ACTOR_136100_ANIMATION_BLEND_FRAMES, request);
            break;
        case ACTOR_136100_PLAYER_BEFORE_PLAY_ANIMATION_53:
            ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(task, 53, ANIMATION_BLEND_INTERPOLATE, ACTOR_136100_ANIMATION_BLEND_FRAMES, request);
            break;
        case ACTOR_136100_PLAYER_BEFORE_EXIT:
            TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, GAME_ACTOR_MESSAGE_PLACE, &D_actor_136100_8013F364, 0);
            ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(task, ACTOR_136100_PLAYER_WEAPON_ANIMATION, ANIMATION_BLEND_RESET, 0, request);
            break;
        case ACTOR_136100_PLAYER_BEFORE_RESET_SCENE_ANIMATION:
            ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(task, ANIMATION_BANK_BASE_SET_COUNT, ANIMATION_BLEND_RESET, 0, request);
            break;
        case ACTOR_136100_PLAYER_BEFORE_BLEND_WEAPON:
            ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(task, ACTOR_136100_PLAYER_WEAPON_ANIMATION, ANIMATION_BLEND_INTERPOLATE, ACTOR_136100_ANIMATION_BLEND_FRAMES, request);
            break;
    }
    work->playerRequest = ACTOR_136100_REQUEST_NONE;
}

/// Ticks the body rig and advances requests before the Burner encounter.
///
/// Requires initialized tracks 1..19 and loaded sets. Head turns replace part
/// 4's local yaw in 64-unit steps, with 4096 units per turn, and deliberately
/// use `playerRequestStep` rather than `bodyRequestStep`. Returning the head
/// clears the request only after passing a full turn. Other requests place the
/// body, reset or blend its tracks, or wait fifteen ticks before a sound cue.
/// Completed requests clear; pending timed and head-turn requests survive.
static void _actor136100UpdateBeforeBurnerBodyRequest(Task* task)
{
    enum { BEGIN_REQUEST     = 0,
           WAIT_FOR_CUE      = 1,
           BODY_SOUND_FRAMES = 15,
           HEAD_PART_INDEX   = 4,
           HEAD_TURN_STEP    = 64,
           HEAD_TURN_LIMIT   = 0xD56,
           BODY_SOUND_SCRIPT = 0x0F };

    _Actor136100Work* work;
    GfxCoord*         coords;

    work = task->work;
    _actor136100TickBodyAnimation(task);
    switch ((u16)work->bodyRequest) {
        case ACTOR_136100_REQUEST_NONE:
            break;
        case ACTOR_136100_BODY_BEFORE_PLACE_FIRST:
            TASK_MESSAGE_DISPATCH_POINTER(task, ACTOR_MESSAGE_PLACE, &D_actor_136100_8013F3C4, 0);
            break;
        case ACTOR_136100_BODY_BEFORE_PLACE_SECOND:
            TASK_MESSAGE_DISPATCH_POINTER(task, ACTOR_MESSAGE_PLACE, &D_actor_136100_8013F3DC, 0);
            break;
        case ACTOR_136100_BODY_BEFORE_BLEND_ANIMATION_4:
            _actor136100BlendBodyAnimation(task, 4);
            break;
        case ACTOR_136100_BODY_BEFORE_RESET_AND_PLACE:
            _actor136100ResetBodyTracks(task, ACTOR_136100_BODY_BEFORE_BURNER_START);
            TASK_MESSAGE_DISPATCH_POINTER(task, ACTOR_MESSAGE_PLACE, &D_actor_136100_8013F3AC, 0);
            break;
        case ACTOR_136100_BODY_BEFORE_PLAY_ANIMATION_6_AND_SOUND:
            switch ((u16)work->bodyRequestStep) {
                case BEGIN_REQUEST:
                    _actor136100BlendBodyAnimation(task, 6);
                    work->bodyRequestFrames = 0;
                    work->bodyRequestStep++;
                    return;
                case WAIT_FOR_CUE:
                    if (++work->bodyRequestFrames == BODY_SOUND_FRAMES) {
                        sndEvtRequestScriptStart(SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_MAIN_STREET, BODY_SOUND_SCRIPT), 0, 0);
                        work->bodyRequest = ACTOR_136100_REQUEST_NONE;
                    }
                    return;
            }
            return;
        case ACTOR_136100_BODY_BEFORE_TURN_HEAD:
            // The head turn counts its steps on the player request's step, which
            // the player request the script posts just before it leaves at 0.
            switch ((u16)work->playerRequestStep) {
                case BEGIN_REQUEST:
                    TASK_MESSAGE_DISPATCH_POINTER(task, ACTOR_MESSAGE_PLACE, &D_actor_136100_8013F3DC, 0);
                    work->headYaw = ACTOR_TRANSFORM_ANGLE_TURN;
                    work->playerRequestStep++;
                    return;
                case WAIT_FOR_CUE:
                    gfxRotMatrixY(&task->extra.tmd->coords[HEAD_PART_INDEX].coord, work->headYaw, GRAPHICS_ROTATION_REPLACE);
                    if (work->headYaw >= HEAD_TURN_LIMIT) {
                        work->headYaw -= HEAD_TURN_STEP;
                    }
                    return;
            }
            return;
        case ACTOR_136100_BODY_BEFORE_RETURN_HEAD:
            // Clear only after passing a full turn; equality takes one more tick.
            coords = task->extra.tmd->coords;
            if ((work->headYaw += HEAD_TURN_STEP) > ACTOR_TRANSFORM_ANGLE_TURN) {
                work->headYaw     = 0;
                work->bodyRequest = ACTOR_136100_REQUEST_NONE;
            }
            gfxRotMatrixY(&coords[HEAD_PART_INDEX].coord, work->headYaw, GRAPHICS_ROTATION_REPLACE);
            return;
        case ACTOR_136100_BODY_BEFORE_RESET:
            _actor136100ResetBodyTracks(task, ACTOR_136100_BODY_BEFORE_BURNER_START);
            break;
    }
    work->bodyRequest = ACTOR_136100_REQUEST_NONE;
}

/// Advances the companion's animation chain and before-Burner placements.
///
/// The cached set index must be 0..5 in the loaded companion table; a negative
/// successor leaves the final pose. Animation updates tolerate an absent task,
/// but placement requests require a live registered companion. Each placement
/// is consumed synchronously and clears its request. Loaded sets remain
/// borrowed during playback; blends last ten normal-rate frames.
static void _actor136100UpdateBeforeBurnerCompanionRequest(Task* task)
{
    _Actor136100Work* work;
    s16               nextSetIndex;

    work = task->work;
    if (work->companionTask != NULL && taskMessageDispatch(work->companionTask, ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
        nextSetIndex = D_actor_136100_8013F218[work->companionAnimation];
        if (nextSetIndex >= 0) {
            _actor136100PlayCompanionAnimation(task, nextSetIndex, ANIMATION_BLEND_INTERPOLATE, ACTOR_136100_ANIMATION_BLEND_FRAMES);
        }
    }
    switch ((u16)work->companionRequest) {
        case ACTOR_136100_REQUEST_NONE:
            break;
        case ACTOR_136100_COMPANION_BEFORE_PLACE_FIRST:
            TASK_MESSAGE_DISPATCH_POINTER(work->companionTask, GAME_ACTOR_MESSAGE_PLACE, &D_actor_136100_8013F424, 0);
            break;
        case ACTOR_136100_COMPANION_BEFORE_PLACE_SECOND:
            TASK_MESSAGE_DISPATCH_POINTER(work->companionTask, GAME_ACTOR_MESSAGE_PLACE, &D_actor_136100_8013F43C, 0);
            break;
        case ACTOR_136100_COMPANION_BEFORE_RESET_PLACEMENT:
            TASK_MESSAGE_DISPATCH_POINTER(work->companionTask, GAME_ACTOR_MESSAGE_PLACE, &D_actor_136100_8013F40C, 0);
            break;
    }
    work->companionRequest = ACTOR_136100_REQUEST_NONE;
}

/// Restores the player and body poses used after the Burner encounter.
///
/// Requires the live scene controller and player. Cancels their pending
/// requests without clearing step counters, restarts body tracks 1 to 19 at
/// normal rate, places the player at the walk's destination and resets the
/// equipped-weapon animation with world collision disabled. The companion's
/// request and pose are untouched.
static void _actor136100ResetAfterBurnerScene(void)
{
    Task*                task;
    _Actor136100Work*    work;
    _Actor136100Work*    requestWork;
    SVECTOR              unused; // Unreferenced; removing it changes the compiled frame.
    AnimationPlayRequest request;
    s32                  weaponId;
    s32                  bankIndex;

    task                = D_actor_136100_8014078C;
    work                = task->work;
    work->playerRequest = ACTOR_136100_REQUEST_NONE;
    work->bodyRequest   = ACTOR_136100_REQUEST_NONE;

    _actor136100ResetBodyTracks(task, ACTOR_136100_BODY_AFTER_BURNER_START);

    TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, GAME_ACTOR_MESSAGE_PLACE, &D_actor_136100_8013F304[1], 0);

    // Re-read work after placement dispatch before caching the player clip.
    requestWork                  = task->work;
    weaponId                     = gPlayerStatus.weapon;
    bankIndex                    = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == ACTOR_136100_PRIMARY_CHARACTER) ? weaponId + ACTOR_136100_PRIMARY_WEAPON_BANK_BASE : weaponId + ACTOR_136100_ALTERNATE_WEAPON_BANK_BASE;
    request.source.index         = bankIndex;
    requestWork->playerAnimation = ACTOR_136100_PLAYER_WEAPON_ANIMATION;
    request.animationId          = ACTOR_136100_PLAYER_WEAPON_ANIMATION;
    request.blend                = ANIMATION_BLEND_RESET;
    request.blendFrames          = 0;
    request.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    TASK_MESSAGE_DISPATCH_POINTER(requestWork->playerTask, ANIMATION_MESSAGE_PLAY, &request, 0);
}

/// Restores the performers' starting placement before the Burner encounter.
///
/// Requires the live scene controller, player and companion. Cancels player
/// and body requests without clearing step counters, places the body and
/// companion, restarts body tracks 1 to 19 at normal rate and resets the
/// equipped-weapon animation with world collision disabled. Exactly 1 in
/// `resetHeadRotation` also replaces the head's local rotation with identity;
/// other values preserve its current rotation.
static void _actor136100ResetBeforeBurnerScene(s32 resetHeadRotation)
{
    Task*                task;
    _Actor136100Work*    work;
    _Actor136100Work*    requestWork;
    SVECTOR              unused; // Unreferenced; removing it changes the compiled frame.
    AnimationPlayRequest request;
    s32                  weaponId;
    s32                  bankIndex;

    task                = D_actor_136100_8014078C;
    work                = task->work;
    work->playerRequest = ACTOR_136100_REQUEST_NONE;
    work->bodyRequest   = ACTOR_136100_REQUEST_NONE;

    TASK_MESSAGE_DISPATCH_POINTER(task, ACTOR_MESSAGE_PLACE, &D_actor_136100_8013F3AC, 0);

    _actor136100ResetBodyTracks(task, ACTOR_136100_BODY_BEFORE_BURNER_START);

    TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, GAME_ACTOR_MESSAGE_PLACE, D_actor_136100_8013F334, 0);

    // Re-read work after placement dispatch before caching the player clip.
    requestWork                  = task->work;
    weaponId                     = gPlayerStatus.weapon;
    bankIndex                    = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == ACTOR_136100_PRIMARY_CHARACTER) ? weaponId + ACTOR_136100_PRIMARY_WEAPON_BANK_BASE : weaponId + ACTOR_136100_ALTERNATE_WEAPON_BANK_BASE;
    request.source.index         = bankIndex;
    requestWork->playerAnimation = ACTOR_136100_PLAYER_WEAPON_ANIMATION;
    request.animationId          = ACTOR_136100_PLAYER_WEAPON_ANIMATION;
    request.blend                = ANIMATION_BLEND_RESET;
    request.blendFrames          = 0;
    request.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    TASK_MESSAGE_DISPATCH_POINTER(requestWork->playerTask, ANIMATION_MESSAGE_PLAY, &request, 0);

    TASK_MESSAGE_DISPATCH_POINTER(work->companionTask, GAME_ACTOR_MESSAGE_PLACE, &D_actor_136100_8013F40C, 0);

    if (resetHeadRotation == 1) {
        gfxRotMatrixY(&task->extra.tmd->coords[4].coord, 0, GRAPHICS_ROTATION_REPLACE);
    }
}

/// Starts a follow-up street conversation from the matching manual room action.
///
/// Returns 1 when a script starts, otherwise 0. Requires the controller and
/// live player. Automatic hits, an unpressed interaction button, the attachment
/// wheel or a pending display transition prevent starting. Room actions 18/19
/// select the after-/before-Burner scene respectively. Installs the player's
/// scene clip extension first; the first conversation has a skip script and
/// sets `followUpSeen`, while later conversations use the short script.
static s16 _actor136100TryStartFollowUpScene(Task* task)
{
    _Actor136100Work* work = task->work;
    u16               triggerControl;
    u8                sceneAction;
    u8                actionArgument;
    s16               interactionRequested;

    interactionRequested = 0;
    if (worldCollisionReadActionHit(&triggerControl, &sceneAction, &actionArgument) != 0) {
        if (!((s16)triggerControl & WORLD_COLLISION_TRIGGER_AUTOMATIC)) {
            if ((triggerControl & (0xFFFF ^ WORLD_COLLISION_TRIGGER_AUTOMATIC)) == WORLD_COLLISION_TRIGGER_ACTION_ROOM) {
                interactionRequested = gPlayerStatus.interactionPressed != 0;
            }
        }
    }
    if (interactionRequested == 0 || Gp_StateC08.mode == ATTACHMENT_MODE_WHEEL) {
        return 0;
    }
    if (gDisplayState.pendingMode != DISPLAY_MODE_NONE) {
        return 0;
    }
    if ((s8)sceneAction == ACTOR_136100_ROOM_ACTION_FOLLOW_UP_AFTER_BURNER && work->scene == ACTOR_136100_SCENE_AFTER_BURNER) {
        _actor136100CopyPlayerSceneAnimations(task);
        if (work->followUpSeen == 0) {
            evsStartScriptWithSkip(D_actor_136100_8013F94C, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_actor_136100_8013FAE4);
            work->followUpSeen++;
        } else {
            evsStartScript(D_actor_136100_8013FC64, EVENT_SCRIPT_HUD_HIDE_RESTORE);
        }
        return 1;
    }
    if ((s8)sceneAction == ACTOR_136100_ROOM_ACTION_FOLLOW_UP_BEFORE_BURNER && work->scene == ACTOR_136100_SCENE_BEFORE_BURNER) {
        _actor136100CopyPlayerSceneAnimations(task);
        if (work->followUpSeen == 0) {
            evsStartScriptWithSkip(D_actor_136100_801402C4, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_actor_136100_801404EC);
            work->followUpSeen++;
        } else {
            evsStartScript(D_actor_136100_8014063C, EVENT_SCRIPT_HUD_HIDE_RESTORE);
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
    place         = areaGetVariant(&gGameSession->location.loc)->placements;
    entryId       = place->entryId;
    while (entryId != AREA_PLACEMENT_END) {
        if (entryId == TEXTURE_RESOURCE_ENTRY_ID) {
            break;
        }
        place++;
        entryId = place->entryId;
    }
    tmdSetTextureOffsets(tmd, place->texturePageOffset, place->clutRowOffset);
    animationInitContext(&work->rig.anim, D_actor_136100_8013F1A0, tmd, work->rig.poses, work->rig.slots);
    task->msgTable = D_actor_136100_8013F2F4;
}

/// Classifies the latched room action selecting an opening street scene.
///
/// Returns `ACTOR_136100_OPENING_CUE_AFTER_BURNER` for room action 16,
/// `ACTOR_136100_OPENING_CUE_BEFORE_BURNER` for 17, otherwise
/// `ACTOR_136100_OPENING_CUE_NONE`. Both automatic and manual hits qualify.
/// The non-NULL, disjoint outputs receive the full control word and both action
/// bytes whenever a hit exists; no hit leaves them untouched. Reading retains
/// the latch, so another call before the next collision tick can see it again.
static inline s16 _actor136100ReadOpeningSceneCue(u16* triggerControl, u8* sceneAction, u8* actionArgument)
{
    if (worldCollisionReadActionHit(triggerControl, sceneAction, actionArgument) != 0) {
        if ((*triggerControl & (0xFFFF ^ WORLD_COLLISION_TRIGGER_AUTOMATIC)) == WORLD_COLLISION_TRIGGER_ACTION_ROOM) {
            if ((s8)*sceneAction == ACTOR_136100_ROOM_ACTION_OPEN_AFTER_BURNER) {
                return ACTOR_136100_OPENING_CUE_AFTER_BURNER;
            }
            if ((s8)*sceneAction == ACTOR_136100_ROOM_ACTION_OPEN_BEFORE_BURNER) {
                return ACTOR_136100_OPENING_CUE_BEFORE_BURNER;
            }
        }
    }
    return ACTOR_136100_OPENING_CUE_NONE;
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

/// Refreshes body lighting at the composed translation of model part 1.
///
/// Requires a live body model with part 1, writable lighting matrices and a
/// writable `VECTOR` in `samplePosition`. Composes the part, copies its three
/// signed translation components in integer coordinate units and passes them
/// unchanged to the full room-light query. The fourth vector word is untouched;
/// the query borrows the buffer only through the call.
static inline void _actor136100UpdateBodyLighting(Task* task, VECTOR* samplePosition)
{
    enum { LIGHT_SAMPLE_PART_INDEX = 1,
           ROOM_LIGHT_COUNT        = 3 };

    TmdObject* model = task->extra.tmd;

    actorRenderComposeCoord(&model->coords[LIGHT_SAMPLE_PART_INDEX]);
    samplePosition->vx = task->extra.tmd->coords[LIGHT_SAMPLE_PART_INDEX].workm.t[0];
    samplePosition->vy = task->extra.tmd->coords[LIGHT_SAMPLE_PART_INDEX].workm.t[1];
    samplePosition->vz = task->extra.tmd->coords[LIGHT_SAMPLE_PART_INDEX].workm.t[2];
    worldCoordSetModelLighting(model, samplePosition, 0, ROOM_LIGHT_COUNT);
}

/// Main tick of the cutscene actor.  State 0 allocates the work block, picks
/// the scene (`scene`, from `GAME_FLAG_BURNER_DEFEATED`) and spawns the head and rifle tasks;
/// state 1 sends the phase's opening cues; state 2 waits for the matching start
/// cue, sends the weapon record and table and sets game flag 0x7C; states 3..5
/// wait on `gGameSession->eventState` and `_actor136100TryStartFollowUpScene`.  Every
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
                _actor136100ResetBodyTracks(arg0, 1);
                work->rifleTask = taskSpawnFromTable(D_actor_136100_80140744, 3, 0, &gGfxViewCoord);
            } else {
                _actor136100ResetBodyTracks(arg0, 3);
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
                gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, 0x19);
                taskMessageDispatch(arg0, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
                taskMessageDispatch(work->headTask, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
                taskMessageDispatch(work->rifleTask, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
                TASK_MESSAGE_DISPATCH_POINTER(work->rifleTask, 0x7D4, &D_actor_136100_8013F454, 0);
                if (work->companionTask != NULL) {
                    TASK_MESSAGE_DISPATCH_POINTER(work->companionTask, 0x3E9, &D_actor_136100_8013F3F4, 0);
                }
                TASK_MESSAGE_DISPATCH_POINTER(arg0, 0x7D4, &D_actor_136100_8013F394, 0);
                _actor136100ResetBodyTracks(arg0, 1);
                ACTOR_136100_RESET_LINKED_ANIMATION(arg0, 1, message.animation);
            } else {
                gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, 0x1A);
                taskMessageDispatch(arg0, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
                taskMessageDispatch(work->headTask, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
                taskMessageDispatch(work->rifleTask, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
                if (work->companionTask != NULL) {
                    TASK_MESSAGE_DISPATCH_POINTER(work->companionTask, 0x3E9, &D_actor_136100_8013F40C, 0);
                }
                TASK_MESSAGE_DISPATCH_POINTER(arg0, 0x7D4, &D_actor_136100_8013F3AC, 0);
                _actor136100ResetBodyTracks(arg0, 3);
                ACTOR_136100_RESET_LINKED_ANIMATION(arg0, 5, message.animation);
            }
            arg0->state++;
            break;
        case 2:
            cue = _actor136100ReadOpeningSceneCue(&evtId, &evtKind, &evtSub);
            if (cue == ACTOR_136100_OPENING_CUE_AFTER_BURNER && work->scene == ACTOR_136100_SCENE_AFTER_BURNER) {
                if (Gp_StateC08.mode == ATTACHMENT_MODE_WHEEL) {
                    return;
                }
                if (gDisplayState.pendingMode != DISPLAY_MODE_NONE) {
                    return;
                }
                ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(arg0, 1, 1, 0xA, message.animation);
                evsStartScriptWithSkip(D_actor_136100_8013F46C, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_actor_136100_8013F784);
                worldCollisionUnlinkTrigger(0, &D_dryfield_night_main_street_8018824C[8]);
                ACTOR_136100_COPY_PLAYER_ANIMATION_SETS(arg0, message.copy);
                gameFlagSetNibble(GAME_FLAG_NIGHT_MAIN_STREET_CUTSCENE_SEEN, 1);
                arg0->state++;
                break;
            }
            if (_actor136100ReadOpeningSceneCue(&evtId2, &evtKind2, &evtSub2) == ACTOR_136100_OPENING_CUE_BEFORE_BURNER && work->scene == ACTOR_136100_SCENE_BEFORE_BURNER) {
                if (Gp_StateC08.mode == ATTACHMENT_MODE_WHEEL) {
                    return;
                }
                if (gDisplayState.pendingMode != DISPLAY_MODE_NONE) {
                    return;
                }
                ACTOR_136100_PLAY_PLAYER_WEAPON_ANIMATION(arg0, 1, 1, 0xA, message.animation);
                evsStartScriptWithSkip(D_actor_136100_8013FD84, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_actor_136100_80140114);
                worldCollisionUnlinkTrigger(0, &D_dryfield_night_main_street_8018824C[9]);
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
            if (_actor136100TryStartFollowUpScene(arg0) != 0) {
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
        _actor136100UpdateAfterBurnerPlayerRequest(arg0);
        _actor136100UpdateAfterBurnerBodyRequest(arg0);
        _actor136100UpdateAfterBurnerCompanionRequest(arg0);
    } else {
        _actor136100UpdateBeforeBurnerPlayerRequest(arg0);
        _actor136100UpdateBeforeBurnerBodyRequest(arg0);
        _actor136100UpdateBeforeBurnerCompanionRequest(arg0);
    }
    _actor136100UpdateBodyLighting(arg0, &message.shadowPosition);
    message.floorOffset.vx = 0;
    message.floorOffset.vy = 0x380;
    message.floorOffset.vz = 0;
    actorRenderDrawGroundShadow(&arg0->extra.tmd->coords[1], 0x300, &message.floorOffset);
}

#include "../../shared/screen_fade_in.inc.c"

/// Adds the task's intensity increment to each signed fade channel in order.
///
/// Requires writable fade work and a live task. The low unsigned halfword of
/// `spawnArg1` is intensity units per tick; zero holds the current channels.
/// Each addition reads the rate anew and narrows to a signed halfword without
/// clamping. Completion and display cancellation belong to the caller.
static inline void _actor136100StepFadeOut(ScreenFadeWork* fade, const Task* task)
{
    fade->r += task->spawnArg1.halves.low;
    fade->g += task->spawnArg1.halves.low;
    fade->b += task->spawnArg1.halves.low;
}

/// Darkens the street scene and blanks the display when the ramp completes.
///
/// Start at state 0 without owned work. Initialization allocates primary-heap
/// `ScreenFadeWork`, then draws and steps in the same tick; teardown releases
/// the work and allocation failure kills the task. The unsigned low halfword
/// of `spawnArg1` is intensity units added per tick (0 holds indefinitely).
/// Drawing uses the low red/green/red bytes before each step. Channels narrow
/// to signed halfwords without clamping; red reaching 256 ends the task and
/// disables display output. The display-restore latch cancels the task after
/// drawing and stepping, without blanking output. Other states do nothing.
static void _actor136100FadeOutTask(Task* task)
{
    enum { INITIALIZE              = 0,
           RAMP                    = 1,
           DISPLAY_BLANK_THRESHOLD = 256 };

    ScreenFadeWork* fade;
    ScreenFadeWork* allocatedFade;

    fade = task->work;
    switch (task->state) {
        case INITIALIZE:
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
            // The first tick also queues the overlay and advances the ramp.
            /* fallthrough */
        case RAMP:
            fadeDrawOverlay(fade->r, fade->g, fade->r, GPU_BLEND_SUBTRACT);
            _actor136100StepFadeOut(fade, task);
            if (D_actor_136100_8013F17C != 0) {
                taskKill(task);
                return;
            }
            if (fade->r < DISPLAY_BLANK_THRESHOLD) {
                return;
            }
            SetDispMask(0);
            taskKill(task);
            break;
    }
}

/// Restarts the registered player's equipped-weapon animation for a scene.
///
/// Requires a live player and loaded weapon bank. Selects the bank from the
/// equipped weapon and saved character, resets its starting animation without
/// blending and disables world collision. The stack request is consumed
/// synchronously; this does not change the controller's animation cache.
static void _actor136100ResetPlayerWeaponAnimation(void)
{
    AnimationPlayRequest request;
    s32                  weaponId;
    s32                  bankIndex;

    weaponId                     = gPlayerStatus.weapon;
    bankIndex                    = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == ACTOR_136100_PRIMARY_CHARACTER) ? weaponId + ACTOR_136100_PRIMARY_WEAPON_BANK_BASE : weaponId + ACTOR_136100_ALTERNATE_WEAPON_BANK_BASE;
    request.source.index         = bankIndex;
    request.animationId          = ACTOR_136100_PLAYER_WEAPON_ANIMATION;
    request.blend                = ANIMATION_BLEND_RESET;
    request.blendFrames          = 0;
    request.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &request, 0);
}

/// Shows the receiving model for a nonzero first payload and hides it for zero.
///
/// Requires a live `TmdObject`; only its active-draw suppression bit changes.
/// The message id and second payload are ignored. Senders must discard the
/// unspecified message result.
static s32 _actor136100SetModelDraw(Task* task, s32 unusedMessageId, s32 visible, s32 unusedSecondArg)
{
    TmdObject* model;

    model = task->extra.tmd;
    if (visible != 0) {
        model->flags = model->flags & (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        model->flags = model->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    // The binary leaves the message result unspecified.
}

#include "../../shared/actor_messages_place_in_view.inc.c"

/// Restarts the after-Burner body pose on tracks 1 to 19 at normal rate.
///
/// Requires the live scene controller and initialized rig. Slot 0, pending
/// requests and their step counters are untouched; this does not blend.
static void _actor136100ResetBodyAnimation(void)
{
    _Actor136100Work* work = D_actor_136100_8014078C->work;
    SVECTOR           unused; // Unreferenced; removing it changes the compiled frame.
    s32               slotIndex;

    work->bodyAnimation = ACTOR_136100_BODY_AFTER_BURNER_START;
    slotIndex           = 1;
    do {
        work->rig.slots[(u16)slotIndex].rate = ANIMATION_RATE_ONE;
        animationResetSlot(&work->rig.anim, (u16)slotIndex, ACTOR_136100_BODY_AFTER_BURNER_START);
        slotIndex++;
    } while ((u16)slotIndex < ARRAY_SIZE(work->rig.slots));
}

/// Posts a player request for the scene controller's next frame update.
///
/// Requires the live controller. Stores the signed halfword request and resets
/// its step to 0, leaving the frame counter intact. Zero cancels; other codes
/// are the `ACTOR_136100_PLAYER_AFTER_*` or `ACTOR_136100_PLAYER_BEFORE_*`
/// commands for the controller's selected scene.
static void _actor136100PostPlayerRequest(s16 request)
{
    _Actor136100Work* work = D_actor_136100_8014078C->work;

    work->playerRequest     = request;
    work->playerRequestStep = 0;
}

/// Posts a body request for the scene controller's next frame update.
///
/// Requires the live controller. Stores the signed halfword request and resets
/// its step to 0, leaving the frame counter intact. Zero cancels; other codes
/// are the `ACTOR_136100_BODY_AFTER_*` or `ACTOR_136100_BODY_BEFORE_*`
/// commands for the controller's selected scene.
static void _actor136100PostBodyRequest(s16 request)
{
    _Actor136100Work* work = D_actor_136100_8014078C->work;

    work->bodyRequest     = request;
    work->bodyRequestStep = 0;
}

/// Posts a companion request for the scene controller's next frame update.
///
/// Requires the live controller. Stores the signed halfword request and resets
/// its step to 0, leaving repeat count and delay intact. Zero cancels; other
/// codes are the `ACTOR_136100_COMPANION_AFTER_*` or
/// `ACTOR_136100_COMPANION_BEFORE_*` commands for the selected scene.
static void _actor136100PostCompanionRequest(s16 request)
{
    _Actor136100Work* work = D_actor_136100_8014078C->work;

    work->companionRequest     = request;
    work->companionRequestStep = 0;
}

/// Starts the street scene's subtractive fade-in at nine intensity units per tick.
///
/// Spawns task-table entry 4 with no parent-coordinate argument. The fade owns
/// its work until completion; this package must remain loaded while it runs.
static void _actor136100StartFadeIn(void)
{
    enum { FADE_TASK_INDEX    = 4,
           INTENSITY_PER_TICK = 9 };

    taskSpawnFromTable(D_actor_136100_80140744, FADE_TASK_INDEX, INTENSITY_PER_TICK, NULL);
}

/// Starts the street scene's subtractive fade-out at nine intensity units per tick.
///
/// Spawns task-table entry 5 with no parent-coordinate argument. The fade owns
/// its work and blanks the display on completion unless the restore latch
/// cancels it. This package must remain loaded while the fade runs.
static void _actor136100StartFadeOut(void)
{
    enum { FADE_TASK_INDEX    = 5,
           INTENSITY_PER_TICK = 9 };

    taskSpawnFromTable(D_actor_136100_80140744, FADE_TASK_INDEX, INTENSITY_PER_TICK, NULL);
}

/// Enables display output and latches cancellation of the scene's fade-out.
///
/// The latch remains set; a fade task observes it after drawing and stepping
/// its ramp. This does not clear the latch or synchronously destroy that task.
static void _actor136100RestoreDisplay(void)
{
    D_actor_136100_8013F17C = 1;
    SetDispMask(1);
}

/// Removes the player's equipment tasks once for the current scene.
///
/// Requires the live controller and player. Latches removal before tearing
/// down equipment; repeated calls do nothing until scene cleanup restores it.
static void _actor136100RemovePlayerEquipment(void)
{
    _Actor136100Work* work = D_actor_136100_8014078C->work;

    if (work->playerEquipmentRemoved == 0) {
        work->playerEquipmentRemoved = 1;
        playerActorRemoveEquipment();
    }
}

void func_actor_136100_80134964(void)
{
    _Actor136100Work* work = D_actor_136100_8014078C->work;

    if (work->playerEquipmentRemoved != 0) {
        playerActorRestoreEquipment();
        work->playerEquipmentRemoved = 0;
        playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
    }
}

/// Applies companion schedules when the opening street scene finishes or skips.
///
/// Clears the trailer-coach door restriction and selects companion 1's
/// post-scene schedule. Zero `beforeBurner` selects companion 2's night
/// schedule; any nonzero value removes that companion and arms scene event 8
/// for subsequent music selection. The live save and game flags must exist.
static void _actor136100FinishOpeningScene(s32 beforeBurner)
{
    enum { COMPANION_1_POST_SCENE_SCHEDULE    = 3,
           COMPANION_2_AFTER_BURNER_SCHEDULE  = 6,
           BEFORE_BURNER_FINISHED_SCENE_EVENT = 8 };

    gameFlagSetNibble(GAME_FLAG_046, 0);
    gameFlagSetNibble(GAME_FLAG_COMPANION_1_SCHEDULE, COMPANION_1_POST_SCENE_SCHEDULE);
    if (beforeBurner == 0) {
        gameFlagSetNibble(GAME_FLAG_COMPANION_2_SCHEDULE, COMPANION_2_AFTER_BURNER_SCHEDULE);
    } else {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = BEFORE_BURNER_FINISHED_SCENE_EVENT;
        gameFlagSetNibble(GAME_FLAG_COMPANION_2_SCHEDULE, 0);
    }
}

/// Installs seven scene clips in the player's writable animation-bank extension.
///
/// Requires a live player with its weapon bank loaded and writable. Counts the
/// leading set pointers up to the NULL terminator and copies their 32-bit
/// address words starting at `ANIMATION_BANK_BASE_SET_COUNT` (47). The count
/// uses an unsigned halfword. Dispatch consumes the stack record and copies
/// the pointers synchronously; the seven descriptors and clip data must remain
/// live for playback. The terminator is excluded from the copy.
static void _actor136100CopyPlayerSceneAnimations(Task* task)
{
    _Actor136100Work*        work = task->work;
    AnimationBankCopyRequest request;
    u16                      setCount;

    setCount = 0;
    while (D_actor_136100_8013F180[setCount] != NULL) {
        setCount += 1;
    }
    request.source.sets = &D_actor_136100_8013F180[0];
    request.wordCount   = setCount;
    TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, ANIMATION_MESSAGE_COPY_BANK_EXTENSION, &request, 0);
}
