#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/area_transitions.h"
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
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
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

#include "rooms/dryfield_garage.h"
#include "../../shared/screen_fade.h"
#include "../../shared/actor_messages.h"

/// Values of `_Actor120300Work::interaction`: what the player's action button
/// started.
enum {
    ACTOR_120300_INTERACTION_TALK   = 0, // action trigger 1: the next of the three talks `talkStage` counts through
    ACTOR_120300_INTERACTION_REMARK = 1  // action trigger 2: one exchange during which the player looks toward the actor
};

/// Work block of Gary Douglas in Dryfield's garage.
///
/// The actor's task allocates it zeroed when it starts and keeps it at
/// `Task::work`. It holds the body's animation rig, storage for the model's
/// matrices, the tasks the scene addresses besides the body, and the state of
/// the scene's choreography. The tasks of the head and rifle models allocate a
/// block of this type as well and use only its `light` and `color`; they read
/// `scale` from this one through the task that spawned them.
///
/// The event scripts drive the scene by posting numbered requests: one for
/// the player in `playerRequest` and one for the actor's body in
/// `bodyRequest`. The tick performs a posted request once and clears it; a
/// request that takes several ticks counts its stages in the step beside it
/// and clears the request when it is done. Request 0 is none. Player request
/// 20 is the exception: it keeps the player's aim on the actor until another
/// request replaces it.
///
/// Angles are 4096ths of a turn. Timers count ticks. Nothing accesses the two
/// `pad` runs; their roles are unproven.
typedef struct {
    ActorAnimRig20 rig;                    // playback of the body's parts; slots 1 to 19 are driven
    MATRIX         light;                  // storage for the model's `TmdObject::lightMtx`
    MATRIX         color;                  // storage for the model's `TmdObject::colorMtx`
    Task*          playerTask;             // the player's task, which the player requests are sent to
    Task*          headTask;               // child task drawing the head-and-hat model, which hangs from the body's part 4
    Task*          rifleTask;              // child task drawing the rifle model: spawned hanging from the body's part 8, then set down at a fixed place in the room
    u16            playerRequest;          // request posted for the player, 0 to 21 (0 none)
    u16            playerRequestStep;      // stage of `playerRequest`, 0 when it is posted
    u16            playerRequestFrames;    // ticks player request 9 has waited; it plays its animation on the sixteenth
    byte           pad_4C6[0x2];           // never accessed
    u16            bodyRequest;            // request posted for the actor's body, 0 to 19 (0 none)
    u16            bodyRequestStep;        // stage of `bodyRequest`, 0 when it is posted
    byte           pad_4CC[0x6];           // never accessed
    u16            playerAnimation;        // animation last asked of the player, an index into the package's player animation sets; selects the animation that follows it
    u16            bodyAnimation;          // animation the body's slots were last started on, an index into the package's animation sets; selects the animation that follows it
    u16            talkStage;              // talks already held, which selects the script of the next (0 first, 1 second, 2 third and every one after); 1 from the start when the room is entered with the motel room 6 door unlocked
    u16            interactionStep;        // stage of the running interaction (0 start its script, 1 wait for the script to end)
    u16            interaction;            // `ACTOR_120300_INTERACTION_*`
    s16            playerAimYaw;           // yaw player requests 20 and 21 step by 48 a tick and store to the player's `GameActor::aimYaw`
    s16            playerEquipmentRemoved; // 1 from the scene killing the player's equipment tasks until it spawns the weapon's again (0 otherwise)
    u16            scale;                  // scale of the body, head and rifle models on all three axes, 0x1000 for 1.0
} _Actor120300Work;
STATIC_ASSERT_SIZEOF(_Actor120300Work, 0x4E4);

extern Task* D_actor_120300_80141BA8;

/// The actor's five-entry task table, spawned from by index. Entries 2 and 3
/// are the child tasks kept in `_Actor120300Work::headTask` and
/// `_Actor120300Work::rifleTask`; entry 4 is the fade to black.
extern TaskDesc D_actor_120300_80141B6C[];

extern AnimationSet* D_actor_120300_801408CC[];
extern AnimationSet* D_actor_120300_80140910[19];
extern s16           D_actor_120300_8014095C[];

/// Animation that follows each `_Actor120300Work::anim` once it has settled;
/// -1 skips the restart.
extern s16 D_actor_120300_80140980[];

extern s32 D_actor_120300_801409A8[6];
extern s32 D_actor_120300_801409C0[24];
extern s32 D_actor_120300_80140A20[9];
// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry D_actor_120300_80140A44[2];
extern ActorTransform   D_actor_120300_80140A54[13];
extern EvsCommand       D_actor_120300_80140B94[];
extern EvsCommand       D_actor_120300_80141524[];
extern EvsCommand       D_actor_120300_801416D4[];
extern EvsCommand       D_actor_120300_801417AC[];
extern EvsCommand       D_actor_120300_80141884[];
extern EvsCommand       D_actor_120300_8014195C[];
extern EvsCommand       D_actor_120300_80141A34[];

static TmdSource _gActor120300GaryDouglasBody;
static TmdSource _gActor120300GaryDouglasHeadHat;
static TmdSource _gActor120300Model082F8;
void             func_actor_120300_80132004(Task*);
void             func_actor_120300_801321C8(Task*);
void             func_actor_120300_80133330(s32);
void             func_actor_120300_801337C4(Task*);
void             func_actor_120300_80133C38(Task*, s32, s32, s32);
void             func_actor_120300_80133D04(s32);
void             func_actor_120300_80133DA4(void);
void             func_actor_120300_80133DD4(void);
void             func_actor_120300_80133DF4(void);
void             func_actor_120300_80133E14(s16);
void             func_actor_120300_80133E34(s16);
void             func_actor_120300_80133E54(void);
void             func_actor_120300_80133E94(void);
void             func_actor_120300_80133EE4(void);
void             func_actor_120300_80133F14(Task*);

static TmdBone _gActor120300GaryDouglasBodySkeleton[20] = {
#include "assets/gary_douglas_body_skeleton.inc"
};

static u32 _gActor120300GaryDouglasBodyPartVerts[20] = {
#include "assets/gary_douglas_body_partVerts.inc"
};

static SVECTOR _gActor120300GaryDouglasBodyVerts[364] = {
#include "assets/gary_douglas_body_verts.inc"
};

static SVECTOR _gActor120300GaryDouglasBodyNormals[354] = {
#include "assets/gary_douglas_body_normals.inc"
};

static u32 _gActor120300GaryDouglasBodyStream[4151] = {
#include "assets/gary_douglas_body_stream.inc"
};

static TmdSource _gActor120300GaryDouglasBody = {
    0,
    22008,
    6952,
    20,
    _gActor120300GaryDouglasBodyPartVerts,
    _gActor120300GaryDouglasBodyVerts,
    _gActor120300GaryDouglasBodyNormals,
    _gActor120300GaryDouglasBodySkeleton,
    _gActor120300GaryDouglasBodyStream,
};

static TmdBone _gActor120300GaryDouglasHeadHatSkeleton[1] = {
#include "assets/gary_douglas_head_hat_skeleton.inc"
};

static u32 _gActor120300GaryDouglasHeadHatPartVerts[1] = {
#include "assets/gary_douglas_head_hat_partVerts.inc"
};

static SVECTOR _gActor120300GaryDouglasHeadHatVerts[21] = {
#include "assets/gary_douglas_head_hat_verts.inc"
};

static SVECTOR _gActor120300GaryDouglasHeadHatNormals[21] = {
#include "assets/gary_douglas_head_hat_normals.inc"
};

static u32 _gActor120300GaryDouglasHeadHatStream[207] = {
#include "assets/gary_douglas_head_hat_stream.inc"
};

static TmdSource _gActor120300GaryDouglasHeadHat = {
    0,
    1352,
    0,
    1,
    _gActor120300GaryDouglasHeadHatPartVerts,
    _gActor120300GaryDouglasHeadHatVerts,
    _gActor120300GaryDouglasHeadHatNormals,
    _gActor120300GaryDouglasHeadHatSkeleton,
    _gActor120300GaryDouglasHeadHatStream,
};

static TmdBone _gActor120300Model082F8Skeleton[1] = {
#include "assets/actor_120300_model_082F8_skeleton.inc"
};

static u32 _gActor120300Model082F8PartVerts[1] = {
#include "assets/actor_120300_model_082F8_partVerts.inc"
};

static SVECTOR _gActor120300Model082F8Verts[34] = {
#include "assets/actor_120300_model_082F8_verts.inc"
};

static SVECTOR _gActor120300Model082F8Normals[28] = {
#include "assets/actor_120300_model_082F8_normals.inc"
};

static u32 _gActor120300Model082F8Stream[238] = {
#include "assets/actor_120300_model_082F8_stream.inc"
};

static TmdSource _gActor120300Model082F8 = {
    0,
    1692,
    0,
    1,
    _gActor120300Model082F8PartVerts,
    _gActor120300Model082F8Verts,
    _gActor120300Model082F8Normals,
    _gActor120300Model082F8Skeleton,
    _gActor120300Model082F8Stream,
};

static AnimationPackedPose _gActor120300Animation08A6CBank1[3] = {
#include "assets/actor_120300_animation_08A6C_bank1.inc"
};

static AnimationPackedRotation _gActor120300Animation08A6CBank4[74] = {
#include "assets/actor_120300_animation_08A6C_bank4.inc"
};

static AnimationRecord _gActor120300Animation08A6CRecords[137] = {
#include "assets/actor_120300_animation_08A6C_records.inc"
};

static u16 _gActor120300Animation08A6CIndices[20] = {
#include "assets/actor_120300_animation_08A6C_indices.inc"
};

static AnimationSet _gActor120300Animation08A6C = {
    _gActor120300Animation08A6CRecords,
    _gActor120300Animation08A6CIndices,
    { NULL, _gActor120300Animation08A6CBank1, NULL, NULL, _gActor120300Animation08A6CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120300Animation08C54Bank1[2] = {
#include "assets/actor_120300_animation_08C54_bank1.inc"
};

static AnimationPackedRotation _gActor120300Animation08C54Bank4[28] = {
#include "assets/actor_120300_animation_08C54_bank4.inc"
};

static AnimationRecord _gActor120300Animation08C54Records[68] = {
#include "assets/actor_120300_animation_08C54_records.inc"
};

static u16 _gActor120300Animation08C54Indices[20] = {
#include "assets/actor_120300_animation_08C54_indices.inc"
};

static AnimationSet _gActor120300Animation08C54 = {
    _gActor120300Animation08C54Records,
    _gActor120300Animation08C54Indices,
    { NULL, _gActor120300Animation08C54Bank1, NULL, NULL, _gActor120300Animation08C54Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120300Animation08E18Bank1[2] = {
#include "assets/actor_120300_animation_08E18_bank1.inc"
};

static AnimationPackedRotation _gActor120300Animation08E18Bank4[23] = {
#include "assets/actor_120300_animation_08E18_bank4.inc"
};

static AnimationRecord _gActor120300Animation08E18Records[64] = {
#include "assets/actor_120300_animation_08E18_records.inc"
};

static u16 _gActor120300Animation08E18Indices[20] = {
#include "assets/actor_120300_animation_08E18_indices.inc"
};

static AnimationSet _gActor120300Animation08E18 = {
    _gActor120300Animation08E18Records,
    _gActor120300Animation08E18Indices,
    { NULL, _gActor120300Animation08E18Bank1, NULL, NULL, _gActor120300Animation08E18Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120300Animation09058Bank1[2] = {
#include "assets/actor_120300_animation_09058_bank1.inc"
};

static AnimationPackedRotation _gActor120300Animation09058Bank4[41] = {
#include "assets/actor_120300_animation_09058_bank4.inc"
};

static AnimationRecord _gActor120300Animation09058Records[77] = {
#include "assets/actor_120300_animation_09058_records.inc"
};

static u16 _gActor120300Animation09058Indices[20] = {
#include "assets/actor_120300_animation_09058_indices.inc"
};

static AnimationSet _gActor120300Animation09058 = {
    _gActor120300Animation09058Records,
    _gActor120300Animation09058Indices,
    { NULL, _gActor120300Animation09058Bank1, NULL, NULL, _gActor120300Animation09058Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120300Animation0928CBank1[2] = {
#include "assets/actor_120300_animation_0928C_bank1.inc"
};

static AnimationPackedRotation _gActor120300Animation0928CBank4[40] = {
#include "assets/actor_120300_animation_0928C_bank4.inc"
};

static AnimationRecord _gActor120300Animation0928CRecords[75] = {
#include "assets/actor_120300_animation_0928C_records.inc"
};

static u16 _gActor120300Animation0928CIndices[20] = {
#include "assets/actor_120300_animation_0928C_indices.inc"
};

static AnimationSet _gActor120300Animation0928C = {
    _gActor120300Animation0928CRecords,
    _gActor120300Animation0928CIndices,
    { NULL, _gActor120300Animation0928CBank1, NULL, NULL, _gActor120300Animation0928CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120300Animation094DCBank1[2] = {
#include "assets/actor_120300_animation_094DC_bank1.inc"
};

static AnimationPackedRotation _gActor120300Animation094DCBank4[29] = {
#include "assets/actor_120300_animation_094DC_bank4.inc"
};

static AnimationRecord _gActor120300Animation094DCRecords[93] = {
#include "assets/actor_120300_animation_094DC_records.inc"
};

static u16 _gActor120300Animation094DCIndices[20] = {
#include "assets/actor_120300_animation_094DC_indices.inc"
};

static AnimationSet _gActor120300Animation094DC = {
    _gActor120300Animation094DCRecords,
    _gActor120300Animation094DCIndices,
    { NULL, _gActor120300Animation094DCBank1, NULL, NULL, _gActor120300Animation094DCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120300Animation09770Bank1[3] = {
#include "assets/actor_120300_animation_09770_bank1.inc"
};

static AnimationPackedRotation _gActor120300Animation09770Bank4[41] = {
#include "assets/actor_120300_animation_09770_bank4.inc"
};

static AnimationRecord _gActor120300Animation09770Records[95] = {
#include "assets/actor_120300_animation_09770_records.inc"
};

static u16 _gActor120300Animation09770Indices[20] = {
#include "assets/actor_120300_animation_09770_indices.inc"
};

static AnimationSet _gActor120300Animation09770 = {
    _gActor120300Animation09770Records,
    _gActor120300Animation09770Indices,
    { NULL, _gActor120300Animation09770Bank1, NULL, NULL, _gActor120300Animation09770Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120300Animation09B34Bank1[8] = {
#include "assets/actor_120300_animation_09B34_bank1.inc"
};

static AnimationPackedRotation _gActor120300Animation09B34Bank4[75] = {
#include "assets/actor_120300_animation_09B34_bank4.inc"
};

static AnimationRecord _gActor120300Animation09B34Records[122] = {
#include "assets/actor_120300_animation_09B34_records.inc"
};

static u16 _gActor120300Animation09B34Indices[20] = {
#include "assets/actor_120300_animation_09B34_indices.inc"
};

static AnimationSet _gActor120300Animation09B34 = {
    _gActor120300Animation09B34Records,
    _gActor120300Animation09B34Indices,
    { NULL, _gActor120300Animation09B34Bank1, NULL, NULL, _gActor120300Animation09B34Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120300Animation09F24Bank1[8] = {
#include "assets/actor_120300_animation_09F24_bank1.inc"
};

static AnimationPackedRotation _gActor120300Animation09F24Bank4[80] = {
#include "assets/actor_120300_animation_09F24_bank4.inc"
};

static AnimationRecord _gActor120300Animation09F24Records[128] = {
#include "assets/actor_120300_animation_09F24_records.inc"
};

static u16 _gActor120300Animation09F24Indices[20] = {
#include "assets/actor_120300_animation_09F24_indices.inc"
};

static AnimationSet _gActor120300Animation09F24 = {
    _gActor120300Animation09F24Records,
    _gActor120300Animation09F24Indices,
    { NULL, _gActor120300Animation09F24Bank1, NULL, NULL, _gActor120300Animation09F24Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120300Animation0A178Bank1[2] = {
#include "assets/actor_120300_animation_0A178_bank1.inc"
};

static AnimationPackedRotation _gActor120300Animation0A178Bank4[38] = {
#include "assets/actor_120300_animation_0A178_bank4.inc"
};

static AnimationRecord _gActor120300Animation0A178Records[85] = {
#include "assets/actor_120300_animation_0A178_records.inc"
};

static u16 _gActor120300Animation0A178Indices[20] = {
#include "assets/actor_120300_animation_0A178_indices.inc"
};

static AnimationSet _gActor120300Animation0A178 = {
    _gActor120300Animation0A178Records,
    _gActor120300Animation0A178Indices,
    { NULL, _gActor120300Animation0A178Bank1, NULL, NULL, _gActor120300Animation0A178Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120300Animation0A5B8Bank1[8] = {
#include "assets/actor_120300_animation_0A5B8_bank1.inc"
};

static AnimationPackedRotation _gActor120300Animation0A5B8Bank4[88] = {
#include "assets/actor_120300_animation_0A5B8_bank4.inc"
};

static AnimationRecord _gActor120300Animation0A5B8Records[140] = {
#include "assets/actor_120300_animation_0A5B8_records.inc"
};

static u16 _gActor120300Animation0A5B8Indices[20] = {
#include "assets/actor_120300_animation_0A5B8_indices.inc"
};

static AnimationSet _gActor120300Animation0A5B8 = {
    _gActor120300Animation0A5B8Records,
    _gActor120300Animation0A5B8Indices,
    { NULL, _gActor120300Animation0A5B8Bank1, NULL, NULL, _gActor120300Animation0A5B8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120300Animation0A854Bank1[2] = {
#include "assets/actor_120300_animation_0A854_bank1.inc"
};

static AnimationPackedRotation _gActor120300Animation0A854Bank4[26] = {
#include "assets/actor_120300_animation_0A854_bank4.inc"
};

static AnimationRecord _gActor120300Animation0A854Records[115] = {
#include "assets/actor_120300_animation_0A854_records.inc"
};

static u16 _gActor120300Animation0A854Indices[20] = {
#include "assets/actor_120300_animation_0A854_indices.inc"
};

static AnimationSet _gActor120300Animation0A854 = {
    _gActor120300Animation0A854Records,
    _gActor120300Animation0A854Indices,
    { NULL, _gActor120300Animation0A854Bank1, NULL, NULL, _gActor120300Animation0A854Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120300Animation0AAA4Bank1[2] = {
#include "assets/actor_120300_animation_0AAA4_bank1.inc"
};

static AnimationPackedRotation _gActor120300Animation0AAA4Bank4[26] = {
#include "assets/actor_120300_animation_0AAA4_bank4.inc"
};

static AnimationRecord _gActor120300Animation0AAA4Records[96] = {
#include "assets/actor_120300_animation_0AAA4_records.inc"
};

static u16 _gActor120300Animation0AAA4Indices[20] = {
#include "assets/actor_120300_animation_0AAA4_indices.inc"
};

static AnimationSet _gActor120300Animation0AAA4 = {
    _gActor120300Animation0AAA4Records,
    _gActor120300Animation0AAA4Indices,
    { NULL, _gActor120300Animation0AAA4Bank1, NULL, NULL, _gActor120300Animation0AAA4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120300Animation0ACACBank1[2] = {
#include "assets/actor_120300_animation_0ACAC_bank1.inc"
};

static AnimationPackedRotation _gActor120300Animation0ACACBank4[32] = {
#include "assets/actor_120300_animation_0ACAC_bank4.inc"
};

static AnimationRecord _gActor120300Animation0ACACRecords[72] = {
#include "assets/actor_120300_animation_0ACAC_records.inc"
};

static u16 _gActor120300Animation0ACACIndices[20] = {
#include "assets/actor_120300_animation_0ACAC_indices.inc"
};

static AnimationSet _gActor120300Animation0ACAC = {
    _gActor120300Animation0ACACRecords,
    _gActor120300Animation0ACACIndices,
    { NULL, _gActor120300Animation0ACACBank1, NULL, NULL, _gActor120300Animation0ACACBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120300Animation0B2E4Bank1[2] = {
#include "assets/actor_120300_animation_0B2E4_bank1.inc"
};

static AnimationPackedRotation _gActor120300Animation0B2E4Bank4[160] = {
#include "assets/actor_120300_animation_0B2E4_bank4.inc"
};

static AnimationRecord _gActor120300Animation0B2E4Records[212] = {
#include "assets/actor_120300_animation_0B2E4_records.inc"
};

static u16 _gActor120300Animation0B2E4Indices[20] = {
#include "assets/actor_120300_animation_0B2E4_indices.inc"
};

static AnimationSet _gActor120300Animation0B2E4 = {
    _gActor120300Animation0B2E4Records,
    _gActor120300Animation0B2E4Indices,
    { NULL, _gActor120300Animation0B2E4Bank1, NULL, NULL, _gActor120300Animation0B2E4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120300Animation0B83CBank1[7] = {
#include "assets/actor_120300_animation_0B83C_bank1.inc"
};

static AnimationPackedRotation _gActor120300Animation0B83CBank4[104] = {
#include "assets/actor_120300_animation_0B83C_bank4.inc"
};

static AnimationRecord _gActor120300Animation0B83CRecords[197] = {
#include "assets/actor_120300_animation_0B83C_records.inc"
};

static u16 _gActor120300Animation0B83CIndices[20] = {
#include "assets/actor_120300_animation_0B83C_indices.inc"
};

static AnimationSet _gActor120300Animation0B83C = {
    _gActor120300Animation0B83CRecords,
    _gActor120300Animation0B83CIndices,
    { NULL, _gActor120300Animation0B83CBank1, NULL, NULL, _gActor120300Animation0B83CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120300Animation0BBD0Bank1[6] = {
#include "assets/actor_120300_animation_0BBD0_bank1.inc"
};

static AnimationPackedRotation _gActor120300Animation0BBD0Bank4[79] = {
#include "assets/actor_120300_animation_0BBD0_bank4.inc"
};

static AnimationRecord _gActor120300Animation0BBD0Records[112] = {
#include "assets/actor_120300_animation_0BBD0_records.inc"
};

static u16 _gActor120300Animation0BBD0Indices[20] = {
#include "assets/actor_120300_animation_0BBD0_indices.inc"
};

static AnimationSet _gActor120300Animation0BBD0 = {
    _gActor120300Animation0BBD0Records,
    _gActor120300Animation0BBD0Indices,
    { NULL, _gActor120300Animation0BBD0Bank1, NULL, NULL, _gActor120300Animation0BBD0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120300Animation0BED4Bank1[6] = {
#include "assets/actor_120300_animation_0BED4_bank1.inc"
};

static AnimationPackedRotation _gActor120300Animation0BED4Bank4[46] = {
#include "assets/actor_120300_animation_0BED4_bank4.inc"
};

static AnimationRecord _gActor120300Animation0BED4Records[109] = {
#include "assets/actor_120300_animation_0BED4_records.inc"
};

static u16 _gActor120300Animation0BED4Indices[20] = {
#include "assets/actor_120300_animation_0BED4_indices.inc"
};

static AnimationSet _gActor120300Animation0BED4 = {
    _gActor120300Animation0BED4Records,
    _gActor120300Animation0BED4Indices,
    { NULL, _gActor120300Animation0BED4Bank1, NULL, NULL, _gActor120300Animation0BED4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120300Animation0C154Bank1[2] = {
#include "assets/actor_120300_animation_0C154_bank1.inc"
};

static AnimationPackedRotation _gActor120300Animation0C154Bank4[52] = {
#include "assets/actor_120300_animation_0C154_bank4.inc"
};

static AnimationRecord _gActor120300Animation0C154Records[82] = {
#include "assets/actor_120300_animation_0C154_records.inc"
};

static u16 _gActor120300Animation0C154Indices[20] = {
#include "assets/actor_120300_animation_0C154_indices.inc"
};

static AnimationSet _gActor120300Animation0C154 = {
    _gActor120300Animation0C154Records,
    _gActor120300Animation0C154Indices,
    { NULL, _gActor120300Animation0C154Bank1, NULL, NULL, _gActor120300Animation0C154Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120300Animation0C4F8Bank1[4] = {
#include "assets/actor_120300_animation_0C4F8_bank1.inc"
};

static AnimationPackedRotation _gActor120300Animation0C4F8Bank4[81] = {
#include "assets/actor_120300_animation_0C4F8_bank4.inc"
};

static AnimationRecord _gActor120300Animation0C4F8Records[120] = {
#include "assets/actor_120300_animation_0C4F8_records.inc"
};

static u16 _gActor120300Animation0C4F8Indices[20] = {
#include "assets/actor_120300_animation_0C4F8_indices.inc"
};

static AnimationSet _gActor120300Animation0C4F8 = {
    _gActor120300Animation0C4F8Records,
    _gActor120300Animation0C4F8Indices,
    { NULL, _gActor120300Animation0C4F8Bank1, NULL, NULL, _gActor120300Animation0C4F8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120300Animation0C7BCBank1[3] = {
#include "assets/actor_120300_animation_0C7BC_bank1.inc"
};

static AnimationPackedRotation _gActor120300Animation0C7BCBank4[61] = {
#include "assets/actor_120300_animation_0C7BC_bank4.inc"
};

static AnimationRecord _gActor120300Animation0C7BCRecords[87] = {
#include "assets/actor_120300_animation_0C7BC_records.inc"
};

static u16 _gActor120300Animation0C7BCIndices[20] = {
#include "assets/actor_120300_animation_0C7BC_indices.inc"
};

static AnimationSet _gActor120300Animation0C7BC = {
    _gActor120300Animation0C7BCRecords,
    _gActor120300Animation0C7BCIndices,
    { NULL, _gActor120300Animation0C7BCBank1, NULL, NULL, _gActor120300Animation0C7BCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120300Animation0C948Bank1[2] = {
#include "assets/actor_120300_animation_0C948_bank1.inc"
};

static AnimationPackedRotation _gActor120300Animation0C948Bank4[16] = {
#include "assets/actor_120300_animation_0C948_bank4.inc"
};

static AnimationRecord _gActor120300Animation0C948Records[57] = {
#include "assets/actor_120300_animation_0C948_records.inc"
};

static u16 _gActor120300Animation0C948Indices[20] = {
#include "assets/actor_120300_animation_0C948_indices.inc"
};

static AnimationSet _gActor120300Animation0C948 = {
    _gActor120300Animation0C948Records,
    _gActor120300Animation0C948Indices,
    { NULL, _gActor120300Animation0C948Bank1, NULL, NULL, _gActor120300Animation0C948Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120300Animation0CBD4Bank1[2] = {
#include "assets/actor_120300_animation_0CBD4_bank1.inc"
};

static AnimationPackedRotation _gActor120300Animation0CBD4Bank4[45] = {
#include "assets/actor_120300_animation_0CBD4_bank4.inc"
};

static AnimationRecord _gActor120300Animation0CBD4Records[92] = {
#include "assets/actor_120300_animation_0CBD4_records.inc"
};

static u16 _gActor120300Animation0CBD4Indices[20] = {
#include "assets/actor_120300_animation_0CBD4_indices.inc"
};

static AnimationSet _gActor120300Animation0CBD4 = {
    _gActor120300Animation0CBD4Records,
    _gActor120300Animation0CBD4Indices,
    { NULL, _gActor120300Animation0CBD4Bank1, NULL, NULL, _gActor120300Animation0CBD4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120300Animation0CE80Bank1[2] = {
#include "assets/actor_120300_animation_0CE80_bank1.inc"
};

static AnimationPackedRotation _gActor120300Animation0CE80Bank4[49] = {
#include "assets/actor_120300_animation_0CE80_bank4.inc"
};

static AnimationRecord _gActor120300Animation0CE80Records[96] = {
#include "assets/actor_120300_animation_0CE80_records.inc"
};

static u16 _gActor120300Animation0CE80Indices[20] = {
#include "assets/actor_120300_animation_0CE80_indices.inc"
};

static AnimationSet _gActor120300Animation0CE80 = {
    _gActor120300Animation0CE80Records,
    _gActor120300Animation0CE80Indices,
    { NULL, _gActor120300Animation0CE80Bank1, NULL, NULL, _gActor120300Animation0CE80Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120300Animation0D09CBank1[2] = {
#include "assets/actor_120300_animation_0D09C_bank1.inc"
};

static AnimationPackedRotation _gActor120300Animation0D09CBank4[35] = {
#include "assets/actor_120300_animation_0D09C_bank4.inc"
};

static AnimationRecord _gActor120300Animation0D09CRecords[74] = {
#include "assets/actor_120300_animation_0D09C_records.inc"
};

static u16 _gActor120300Animation0D09CIndices[20] = {
#include "assets/actor_120300_animation_0D09C_indices.inc"
};

static AnimationSet _gActor120300Animation0D09C = {
    _gActor120300Animation0D09CRecords,
    _gActor120300Animation0D09CIndices,
    { NULL, _gActor120300Animation0D09CBank1, NULL, NULL, _gActor120300Animation0D09CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120300Animation0D42CBank1[3] = {
#include "assets/actor_120300_animation_0D42C_bank1.inc"
};

static AnimationPackedRotation _gActor120300Animation0D42CBank4[87] = {
#include "assets/actor_120300_animation_0D42C_bank4.inc"
};

static AnimationRecord _gActor120300Animation0D42CRecords[112] = {
#include "assets/actor_120300_animation_0D42C_records.inc"
};

static u16 _gActor120300Animation0D42CIndices[20] = {
#include "assets/actor_120300_animation_0D42C_indices.inc"
};

static AnimationSet _gActor120300Animation0D42C = {
    _gActor120300Animation0D42CRecords,
    _gActor120300Animation0D42CIndices,
    { NULL, _gActor120300Animation0D42CBank1, NULL, NULL, _gActor120300Animation0D42CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120300Animation0D674Bank1[2] = {
#include "assets/actor_120300_animation_0D674_bank1.inc"
};

static AnimationPackedRotation _gActor120300Animation0D674Bank4[35] = {
#include "assets/actor_120300_animation_0D674_bank4.inc"
};

static AnimationRecord _gActor120300Animation0D674Records[85] = {
#include "assets/actor_120300_animation_0D674_records.inc"
};

static u16 _gActor120300Animation0D674Indices[20] = {
#include "assets/actor_120300_animation_0D674_indices.inc"
};

static AnimationSet _gActor120300Animation0D674 = {
    _gActor120300Animation0D674Records,
    _gActor120300Animation0D674Indices,
    { NULL, _gActor120300Animation0D674Bank1, NULL, NULL, _gActor120300Animation0D674Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120300Animation0D99CBank1[2] = {
#include "assets/actor_120300_animation_0D99C_bank1.inc"
};

static AnimationPackedRotation _gActor120300Animation0D99CBank4[35] = {
#include "assets/actor_120300_animation_0D99C_bank4.inc"
};

static AnimationRecord _gActor120300Animation0D99CRecords[141] = {
#include "assets/actor_120300_animation_0D99C_records.inc"
};

static u16 _gActor120300Animation0D99CIndices[20] = {
#include "assets/actor_120300_animation_0D99C_indices.inc"
};

static AnimationSet _gActor120300Animation0D99C = {
    _gActor120300Animation0D99CRecords,
    _gActor120300Animation0D99CIndices,
    { NULL, _gActor120300Animation0D99CBank1, NULL, NULL, _gActor120300Animation0D99CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120300Animation0DB94Bank1[2] = {
#include "assets/actor_120300_animation_0DB94_bank1.inc"
};

static AnimationPackedRotation _gActor120300Animation0DB94Bank4[25] = {
#include "assets/actor_120300_animation_0DB94_bank4.inc"
};

static AnimationRecord _gActor120300Animation0DB94Records[75] = {
#include "assets/actor_120300_animation_0DB94_records.inc"
};

static u16 _gActor120300Animation0DB94Indices[20] = {
#include "assets/actor_120300_animation_0DB94_indices.inc"
};

static AnimationSet _gActor120300Animation0DB94 = {
    _gActor120300Animation0DB94Records,
    _gActor120300Animation0DB94Indices,
    { NULL, _gActor120300Animation0DB94Bank1, NULL, NULL, _gActor120300Animation0DB94Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120300Animation0DD8CBank1[3] = {
#include "assets/actor_120300_animation_0DD8C_bank1.inc"
};

static AnimationPackedRotation _gActor120300Animation0DD8CBank4[34] = {
#include "assets/actor_120300_animation_0DD8C_bank4.inc"
};

static AnimationRecord _gActor120300Animation0DD8CRecords[63] = {
#include "assets/actor_120300_animation_0DD8C_records.inc"
};

static u16 _gActor120300Animation0DD8CIndices[20] = {
#include "assets/actor_120300_animation_0DD8C_indices.inc"
};

static AnimationSet _gActor120300Animation0DD8C = {
    _gActor120300Animation0DD8CRecords,
    _gActor120300Animation0DD8CIndices,
    { NULL, _gActor120300Animation0DD8CBank1, NULL, NULL, _gActor120300Animation0DD8CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120300Animation0E0E0Bank1[5] = {
#include "assets/actor_120300_animation_0E0E0_bank1.inc"
};

static AnimationPackedRotation _gActor120300Animation0E0E0Bank4[58] = {
#include "assets/actor_120300_animation_0E0E0_bank4.inc"
};

static AnimationRecord _gActor120300Animation0E0E0Records[120] = {
#include "assets/actor_120300_animation_0E0E0_records.inc"
};

static u16 _gActor120300Animation0E0E0Indices[20] = {
#include "assets/actor_120300_animation_0E0E0_indices.inc"
};

static AnimationSet _gActor120300Animation0E0E0 = {
    _gActor120300Animation0E0E0Records,
    _gActor120300Animation0E0E0Indices,
    { NULL, _gActor120300Animation0E0E0Bank1, NULL, NULL, _gActor120300Animation0E0E0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120300Animation0E2D8Bank1[3] = {
#include "assets/actor_120300_animation_0E2D8_bank1.inc"
};

static AnimationPackedRotation _gActor120300Animation0E2D8Bank4[34] = {
#include "assets/actor_120300_animation_0E2D8_bank4.inc"
};

static AnimationRecord _gActor120300Animation0E2D8Records[63] = {
#include "assets/actor_120300_animation_0E2D8_records.inc"
};

static u16 _gActor120300Animation0E2D8Indices[20] = {
#include "assets/actor_120300_animation_0E2D8_indices.inc"
};

static AnimationSet _gActor120300Animation0E2D8 = {
    _gActor120300Animation0E2D8Records,
    _gActor120300Animation0E2D8Indices,
    { NULL, _gActor120300Animation0E2D8Bank1, NULL, NULL, _gActor120300Animation0E2D8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120300Animation0E700Bank1[5] = {
#include "assets/actor_120300_animation_0E700_bank1.inc"
};

static AnimationPackedRotation _gActor120300Animation0E700Bank4[90] = {
#include "assets/actor_120300_animation_0E700_bank4.inc"
};

static AnimationRecord _gActor120300Animation0E700Records[141] = {
#include "assets/actor_120300_animation_0E700_records.inc"
};

static u16 _gActor120300Animation0E700Indices[20] = {
#include "assets/actor_120300_animation_0E700_indices.inc"
};

static AnimationSet _gActor120300Animation0E700 = {
    _gActor120300Animation0E700Records,
    _gActor120300Animation0E700Indices,
    { NULL, _gActor120300Animation0E700Bank1, NULL, NULL, _gActor120300Animation0E700Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor120300Animation0EA84Bank1[6] = {
#include "assets/actor_120300_animation_0EA84_bank1.inc"
};

static AnimationPackedRotation _gActor120300Animation0EA84Bank4[69] = {
#include "assets/actor_120300_animation_0EA84_bank4.inc"
};

static AnimationRecord _gActor120300Animation0EA84Records[118] = {
#include "assets/actor_120300_animation_0EA84_records.inc"
};

static u16 _gActor120300Animation0EA84Indices[20] = {
#include "assets/actor_120300_animation_0EA84_indices.inc"
};

static AnimationSet _gActor120300Animation0EA84 = {
    _gActor120300Animation0EA84Records,
    _gActor120300Animation0EA84Indices,
    { NULL, _gActor120300Animation0EA84Bank1, NULL, NULL, _gActor120300Animation0EA84Bank4, NULL, NULL, NULL },
};

AnimationSet* D_actor_120300_801408CC[17] = {
    &_gActor120300Animation0BED4,
    &_gActor120300Animation0C154,
    &_gActor120300Animation0C4F8,
    &_gActor120300Animation0C7BC,
    &_gActor120300Animation0C948,
    &_gActor120300Animation0CBD4,
    &_gActor120300Animation0CE80,
    &_gActor120300Animation0D09C,
    &_gActor120300Animation0D42C,
    &_gActor120300Animation0D674,
    &_gActor120300Animation0D99C,
    &_gActor120300Animation0DD8C,
    &_gActor120300Animation0E0E0,
    &_gActor120300Animation0E2D8,
    &_gActor120300Animation0E700,
    &_gActor120300Animation0EA84,
    &_gActor120300Animation0DB94,
};

AnimationSet* D_actor_120300_80140910[19] = {
    NULL,
    &_gActor120300Animation08A6C,
    NULL,
    NULL,
    &_gActor120300Animation08C54,
    &_gActor120300Animation08E18,
    &_gActor120300Animation09058,
    &_gActor120300Animation0928C,
    &_gActor120300Animation094DC,
    &_gActor120300Animation09770,
    &_gActor120300Animation09B34,
    &_gActor120300Animation09F24,
    &_gActor120300Animation0A178,
    &_gActor120300Animation0A5B8,
    &_gActor120300Animation0A854,
    &_gActor120300Animation0B83C,
    &_gActor120300Animation0BBD0,
    &_gActor120300Animation0AAA4,
    &_gActor120300Animation0ACAC,
};

s16 D_actor_120300_8014095C[18] = {
    -1,
    -1,
    -1,
    10,
    10,
    10,
    10,
    10,
    0,
    0,
    -1,
    12,
    -1,
    0,
    0,
    0,
    10,
    0,
};

s16 D_actor_120300_80140980[20] = {
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    17,
    -1,
    -1,
    17,
    17,
    -1,
    14,
    0,
};

s32 D_actor_120300_801409A8[6] = { 0xF000, 0, 0, 0xF000, 4096, 0 };

s32 D_actor_120300_801409C0[24] = { -0x3E7F380, 2460, -0x3E7F380, 2000, 3200, 2460, 3200, 2000, -0x3E7F380, 2000, -0x3E7F0C4, 2000, 3200, 2000, 3900, 2000, -0x3E7F0C4, 2000, -0x3E7F0C4, 2460, 3900, 2000, 3900, 2460 };

s32 D_actor_120300_80140A20[9] = { 0x10000, 0x30002, 0, 0x50004, 0x70006, 1, 0x90008, 0xB000A, 2 };

TaskMessageEntry D_actor_120300_80140A44[2] = {
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_120300_80133C38 },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceInView },
};

ActorTransform D_actor_120300_80140A54[13] = {
    { { 2414, 0, 2529, 0 }, { 0, 0, 0, 0 } },
    { { 2553, 0, 2396, 0 }, { 0, 1024, 0, 0 } },
    { { 2553, 0, 1700, 0 }, { 0, 1024, 0, 0 } },
    { { 2589, 0, 2068, 0 }, { 0, 1024, 0, 0 } },
    { { 1559, 0, 3583, 0 }, { 0, 1536, 0, 0 } },
    { { 2250, 0, 2200, 0 }, { 0, 1024, 0, 0 } },
    { { 800, 0, 3360, 0 }, { 0, 1536, 0, 0 } },
    { { 5000, 0, 1531, 0 }, { 0, 3072, 0, 0 } },
    { { 3792, 0, 1531, 0 }, { 0, 3072, 0, 0 } },
    { { 3475, 0, 2165, 0 }, { 0, 0, 0, 0 } },
    { { 3475, 0, 2165, 0 }, { 0, 3072, 0, 0 } },
    { { 3900, -300, 2070, 0 }, { 1638, 0, 0, 0 } },
    { { 8500, 0, 1531, 0 }, { 0, 3072, 0, 0 } },
};

EvsSceneKey D_actor_120300_80140B8C = { 2, 3, 11 };

EvsCommand D_actor_120300_80140B94[102] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_actor_120300_80140B8C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_120300_80133DA4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_120300_80133DD4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_120300_80133D04 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_120300_80133E54 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_120300_80133D04 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 13 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_120300_80133D04 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_120300_80133D04 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 13 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_120300_80133D04 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_120300_80133D04 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 19 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_120300_80133D04 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_120300_80133D04 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 11 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 17 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 12 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 17 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 12 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 11 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 13 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_120300_80133EE4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_120300_80133E94 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_120300_80133DF4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_120300_80141524[18] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_120300_80133E94 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_120300_80133330 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 12 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_120300_801416D4[9] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_120300_801417AC[9] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 3 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_120300_80141884[9] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_120300_8014195C[9] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 5 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_120300_80141A34[13] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 14 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 21 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_120300_80133E34 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

TaskDesc D_actor_120300_80141B6C[5] = {
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_120300_801337C4, { .model = &_gActor120300GaryDouglasBody } },
    { { { TASK_BODY_NONE, 192 } }, func_actor_120300_80133F14, { .value = 0 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_120300_80132004, { .model = &_gActor120300GaryDouglasHeadHat } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_120300_801321C8, { .model = &_gActor120300Model082F8 } },
    { { { TASK_BODY_NONE, 192 } }, screenFadeOutTask, { .value = 0 } },
};

Task* D_actor_120300_80141BA8;

static inline void func_actor_120300_FillLight(Task* arg0, TmdObject* tmd, VECTOR* vec);
static s32         func_actor_120300_80131EE0(Task* arg0);
static inline s16  _actor120300InitChild(Task* arg0, s32 part);
static inline void _actor120300PlayAnim(Task* task, u16 anim);
static inline void _actor120300SetAnim(Task* task, u16 anim);
static void        func_actor_120300_80132338(Task* arg0);
static inline void _actor120300BlendAll(Task* task, u16 anim);
static inline void _actor120300ResetAll(Task* task, u16 anim);
static void        func_actor_120300_80132C60(Task* arg0);
static s32         func_actor_120300_801334A4(Task* arg0);
static void        func_actor_120300_801335D8(Task* task);

/// Fill part-1 translation and hand it to `worldCoordSetModelLighting`. `vec` is a
/// parameter rather than a local so its address stays out of the CSE class of
/// the `ScaleMatrix` argument that follows.
static inline void func_actor_120300_FillLight(Task* arg0, TmdObject* tmd, VECTOR* vec)
{
    vec->vx = tmd->coords[1].workm.t[0];
    vec->vy = arg0->extra.tmd->coords[1].workm.t[1];
    vec->vz = arg0->extra.tmd->coords[1].workm.t[2];
    worldCoordSetModelLighting(tmd, vec, 0, 3);
}

/// Ticks slots 1..19 of a task's animation context and, if every one of them
/// then has `ANIMATION_SLOT_SETTLED` set, re-reads the work block and
/// restarts slots 1..19 on the id `D_actor_120300_80140980` gives for the
/// block's `bodyAnimation`, which it stores there; a negative entry restarts
/// nothing. Returns 1 when every slot had settled and 0 otherwise. The gotos
/// reproduce retail's block layout.
static s32 func_actor_120300_80131EE0(Task* arg0)
{
    _Actor120300Work* work;
    _Actor120300Work* animWork;
    u16               anim;
    u16               i;
    u16               done;

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
        if (D_actor_120300_80140980[work->bodyAnimation] >= 0) {
            anim                    = D_actor_120300_80140980[work->bodyAnimation];
            animWork                = arg0->work;
            animWork->bodyAnimation = anim;
            goto loop;
        fail:
            done = 0;
            goto check;
        loop:
            for (i = 1; i < ARRAY_SIZE(animWork->rig.slots); i++) {
                animationSeekSlotWithBlend(&animWork->rig.anim, i, anim, 0, 10);
            }
        }
        return 1;
    }
    return 0;
}

/// Allocates and wires up a child's work block, anchoring its root
/// coordinate under part `part` of the spawning task's model. Returns
/// nonzero when the allocation failed.
static inline s16 _actor120300InitChild(Task* arg0, s32 part)
{
    TmdObject*        tmd   = arg0->extra.tmd;
    GfxCoord*         coord = tmd->coords;
    _Actor120300Work* work;

    work       = memMalloc(sizeof(*work), false);
    arg0->work = work;
    if (work == NULL) {
        return 1;
    }
    memFillBytes(work, 0, sizeof(*work));
    coord->parent          = ((Task*)arg0->spawnArg2.pointer)->extra.tmd->coords + part;
    arg0->extra.tmd->flags = 0;
    tmdAllocPrimitiveBuffer(tmd);
    tmd->lightMtx  = &work->light;
    tmd->colorMtx  = &work->color;
    arg0->msgTable = D_actor_120300_80140A44;
    return 0;
}

/// Spawn tick of a child actor that keeps the model facing the player: state 0
/// allocates a zeroed `_Actor120300Work` block, parks it in
/// `Task::work`, points the model's light and colour matrices at the block's
/// `lightMtx` / `colorMtx`, clears `TmdObject::flags` and anchors the root
/// coordinate `parent` under part 4 of the spawning task's model
/// (`Task::spawnArg2->extra`); a failed allocation kills the task instead of
/// stepping to state 1. The texture page / CLUT row then come from the
/// placement record at the nested area table's `field_0` list with resource-entry ID 0x6A (or the end record if that ID is absent). Every tick after that reads
/// the parent work block's `scale` and primes the colour matrix with the
/// root coordinate's own translation through `worldCoordSetModelLighting`, then replaces
/// that translation with the parent scale broadcast over all three axes and
/// folds it in with `ScaleMatrix`.
void func_actor_120300_80132004(Task* task)
{
    VECTOR         vec;
    AreaPlacement* place;
    s32            scale;
    u16            scaleRaw;
    TmdObject*     tmd2;
    u8             id;

    if (task->state == 0) {
        if (_actor120300InitChild(task, 4) != 0) {
            taskKill(task);
            return;
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
        tmdSetTextureOffsets(task->extra.tmd, place->texturePageOffset, place->clutRowOffset);
        task->state += 1;
    }
    tmd2     = task->extra.tmd;
    scaleRaw = ((_Actor120300Work*)((Task*)task->spawnArg2.pointer)->work)->scale;
    vec.vx   = tmd2->coords->workm.t[0];
    vec.vy   = task->extra.tmd->coords->workm.t[1];
    vec.vz   = task->extra.tmd->coords->workm.t[2];
    worldCoordSetModelLighting(tmd2, &vec, 0, 3);
    scale  = scaleRaw;
    vec.vz = scale;
    vec.vy = scale;
    vec.vx = scale;
    ScaleMatrix(tmd2->colorMtx, &vec);
}

/// Spawn tick of a child actor. State 0 allocates a zeroed
/// `_Actor120300Work` block, parks it in `Task::work`, points the model's
/// light and colour matrices at the block's `light` / `color`, clears
/// `TmdObject::flags` and anchors the root coordinate `parent` under part 8 of
/// the spawning task's model (`Task::spawnArg2->extra`). A failed allocation
/// kills the task rather than stepping to state 1.
/// Every later tick reads the parent work block's `scale` and primes the
/// colour matrix with the root coordinate's own translation through
/// `worldCoordSetModelLighting`, then replaces that translation with the parent scale
/// broadcast over all three axes and folds it in with `ScaleMatrix`.
void func_actor_120300_801321C8(Task* arg0)
{
    VECTOR     vec;
    s32        scale;
    u16        scaleRaw;
    TmdObject* tmd2;

    if (arg0->state == 0) {
        if (_actor120300InitChild(arg0, 8) != 0) {
            taskKill(arg0);
            return;
        }
        arg0->state += 1;
    }
    tmd2     = arg0->extra.tmd;
    scaleRaw = ((_Actor120300Work*)((Task*)arg0->spawnArg2.pointer)->work)->scale;
    vec.vx   = tmd2->coords->workm.t[0];
    vec.vy   = arg0->extra.tmd->coords->workm.t[1];
    vec.vz   = arg0->extra.tmd->coords->workm.t[2];
    worldCoordSetModelLighting(tmd2, &vec, 0, 3);
    scale  = scaleRaw;
    vec.vz = scale;
    vec.vy = scale;
    vec.vx = scale;
    ScaleMatrix(tmd2->colorMtx, &vec);
}

/// Records `anim` in the block's `playerAnimation` and sends the player's task
/// `ANIMATION_MESSAGE_INSTALL_AND_PLAY` to blend into animation `anim` of
/// `D_actor_120300_801408CC`. Does nothing while `playerTask` is unset.
static inline void _actor120300PlayAnim(Task* task, u16 anim)
{
    _Actor120300Work*    work = task->work;
    AnimationPlayRequest msg;

    if (work->playerTask != NULL) {
        msg.source.sets          = D_actor_120300_801408CC;
        work->playerAnimation    = anim;
        msg.animationId          = anim;
        msg.blend                = ANIMATION_BLEND_INTERPOLATE;
        msg.blendFrames          = 0xA;
        msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
        TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg, 0);
    }
}

/// As `_actor120300PlayAnim`, but with `field_8` zero, so the receiver resets
/// its animation slots to `anim` instead of blending.
static inline void _actor120300SetAnim(Task* task, u16 anim)
{
    _Actor120300Work*    work = task->work;
    AnimationPlayRequest msg;

    if (work->playerTask != NULL) {
        msg.source.sets          = D_actor_120300_801408CC;
        work->playerAnimation    = anim;
        msg.animationId          = anim;
        msg.blend                = ANIMATION_BLEND_RESET;
        msg.blendFrames          = 0;
        msg.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
        TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &msg, 0);
    }
}

/// Sends an equipped-weapon playback request, with world collision disabled.
///
/// All arguments are evaluated once. The bank selector depends on the current
/// weapon and save character; `frames` is a whole-frame blend duration.
/// Dispatch consumes the block-local request synchronously.
#define ACTOR_120300_PLAY_PLAYER_WEAPON_ANIMATION(target, blendChoice, frames)                                                       \
    {                                                                                                                                \
        AnimationPlayRequest request;                                                                                                \
        s32                  weaponId;                                                                                               \
                                                                                                                                     \
        weaponId                     = gPlayerStatus.weapon;                                                                         \
        request.source.index         = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22; \
        request.animationId          = 1;                                                                                            \
        request.blend                = (blendChoice);                                                                                \
        request.blendFrames          = (frames);                                                                                     \
        request.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;                                                            \
        TASK_MESSAGE_DISPATCH_POINTER((target), ANIMATION_MESSAGE_PLAY, &request, 0);                                                \
    }

/// Performs the request posted in `playerRequest`. While the session event
/// state is set and the player's animation has finished, it first plays the
/// animation `D_actor_120300_8014095C` gives as the follower of
/// `playerAnimation`.
/// The switch sends the player animation requests, placements and the weapon
/// animation, then clears the request. Request 9 counts 16 ticks in
/// `playerRequestFrames` first. Request 20 steps `playerAimYaw` by 0x30 a tick
/// toward the bearing between the actor and the player and request 21 steps it
/// back to zero, each storing it to the player's aim yaw; 20 is not cleared
/// here and holds the aim until another request is posted.
static void func_actor_120300_80132338(Task* arg0)
{
    _Actor120300Work* work;
    GameActor*        player;
    GfxCoord*         actorCoord;
    GfxCoord*         playerCoord;
    s32               dx;
    s32               dz;
    s32               diff;
    s16               target;
    s16               cur;
    ActorTransform*   rec;
    Task*             playerTask;

    work = arg0->work;
    if (gGameSession->eventState != 0) {
        if ((work->playerTask != NULL) && (taskMessageDispatch(work->playerTask, ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0)) {
            if (D_actor_120300_8014095C[work->playerAnimation] >= 0) {
                _actor120300PlayAnim(arg0, D_actor_120300_8014095C[work->playerAnimation]);
            }
        }
    }
    switch (work->playerRequest) {
        case 0:
            break;
        case 1:
            _actor120300SetAnim(arg0, 0);
            break;
        case 2:
            rec = &D_actor_120300_80140A54[6];
            TASK_MESSAGE_DISPATCH_POINTER(((_Actor120300Work*)arg0->work)->playerTask, GAME_ACTOR_MESSAGE_PLACE, rec, 0);
            /* Both views belong to the same placement table. */
            TASK_MESSAGE_DISPATCH_POINTER(((_Actor120300Work*)arg0->work)->playerTask, GAME_ACTOR_MESSAGE_MOVE_TO, rec - 5, 0);
            break;
        case 3:
            TASK_MESSAGE_DISPATCH_POINTER(((_Actor120300Work*)arg0->work)->playerTask, GAME_ACTOR_MESSAGE_PLACE, &D_actor_120300_80140A54[1], 0);
            _actor120300SetAnim(arg0, 1);
            break;
        case 4:
            _actor120300PlayAnim(arg0, 2);
            break;
        case 5:
            taskMessageDispatch(work->playerTask, GAME_ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
            TASK_MESSAGE_DISPATCH_POINTER(((_Actor120300Work*)arg0->work)->playerTask, GAME_ACTOR_MESSAGE_PLACE, &D_actor_120300_80140A54[2], 0);
            _actor120300PlayAnim(arg0, 3);
            break;
        case 7:
            _actor120300PlayAnim(arg0, 5);
            break;
        case 8:
            _actor120300PlayAnim(arg0, 6);
            break;
        case 9:
            switch (work->playerRequestStep) {
                case 0:
                    work->playerRequestFrames = 0;
                    work->playerRequestStep++;
                    break;
                case 1:
                    if (++work->playerRequestFrames >= 0x10) {
                        _actor120300PlayAnim(arg0, 7);
                        work->playerRequest = 0;
                    }
                    break;
            }
            return;
        case 10:
            _actor120300PlayAnim(arg0, 8);
            taskMessageDispatch(work->playerTask, ANIMATION_MESSAGE_SET_RATE, 8, 0);
            break;
        case 11:
            TASK_MESSAGE_DISPATCH_POINTER(((_Actor120300Work*)arg0->work)->playerTask, GAME_ACTOR_MESSAGE_PLACE, &D_actor_120300_80140A54[3], 0);
            _actor120300PlayAnim(arg0, 0xB);
            taskMessageDispatch(work->playerTask, ANIMATION_MESSAGE_SET_RATE, 8, 0);
            break;
        case 12:
            _actor120300PlayAnim(arg0, 9);
            taskMessageDispatch(work->playerTask, ANIMATION_MESSAGE_SET_RATE, 0x18, 0);
            break;
        case 13:
            TASK_MESSAGE_DISPATCH_POINTER(((_Actor120300Work*)arg0->work)->playerTask, GAME_ACTOR_MESSAGE_PLACE, &D_actor_120300_80140A54[5], 0);
            ACTOR_120300_PLAY_PLAYER_WEAPON_ANIMATION(work->playerTask, 0, 0);
            break;
        case 14:
            _actor120300PlayAnim(arg0, 0xF);
            taskMessageDispatch(work->playerTask, ANIMATION_MESSAGE_SET_RATE, 8, 0);
            break;
        case 15:
            _actor120300PlayAnim(arg0, 0xE);
            taskMessageDispatch(work->playerTask, ANIMATION_MESSAGE_SET_RATE, 8, 0);
            break;
        case 16:
            _actor120300PlayAnim(arg0, 0xD);
            taskMessageDispatch(work->playerTask, ANIMATION_MESSAGE_SET_RATE, 8, 0);
            break;
        case 17:
            _actor120300PlayAnim(arg0, 0xE);
            taskMessageDispatch(work->playerTask, ANIMATION_MESSAGE_SET_RATE, 8, 0);
            break;
        case 18:
            ACTOR_120300_PLAY_PLAYER_WEAPON_ANIMATION(work->playerTask, 1, 0xA);
            break;
        case 19:
            _actor120300PlayAnim(arg0, 0x10);
            break;
        case 20:
            playerTask  = work->playerTask;
            actorCoord  = arg0->extra.tmd->coords;
            playerCoord = playerTask->extra.tmd->coords;
            player      = (GameActor*)playerTask->work;
            switch (work->playerRequestStep) {
                case 0:
                    work->playerAimYaw = player->aimYaw;
                    work->playerRequestStep++;
                    /* fallthrough */
                case 1:
                    if (actorCoord->coord.t[0] > playerCoord->coord.t[0]) {
                        dx = (u16)actorCoord->coord.t[0] - (u16)playerCoord->coord.t[0];
                        dz = (u16)playerCoord->coord.t[2] - (u16)actorCoord->coord.t[2];
                    } else {
                        dx = (u16)playerCoord->coord.t[0] - (u16)actorCoord->coord.t[0];
                        dz = (u16)actorCoord->coord.t[2] - (u16)playerCoord->coord.t[2];
                    }
                    target = ratan2((s16)dz, (s16)dx);
                    cur    = work->playerAimYaw;
                    diff   = target - cur;
                    if (diff < 0) {
                        diff = -diff;
                    }
                    if (diff < 0x31) {
                        work->playerRequestStep++;
                    } else if (cur < target) {
                        work->playerAimYaw = cur + 0x30;
                    } else {
                        work->playerAimYaw = cur - 0x30;
                    }
                    /* fallthrough */
                case 2:
                    player->aimYaw = work->playerAimYaw;
                    return;
            }
            return;
        case 21: {
            GameActor* aim;

            aim = (GameActor*)work->playerTask->work;
            if (ABS(work->playerAimYaw) < 0x31) {
                work->playerAimYaw  = 0;
                work->playerRequest = 0;
            } else if (work->playerAimYaw < 0) {
                work->playerAimYaw += 0x30;
            } else {
                work->playerAimYaw -= 0x30;
            }
            aim->aimYaw = work->playerAimYaw;
            return;
        }
    }
    work->playerRequest = 0;
}

/// Cross-fades body slots 1..19 of `work`'s animation context to animation
/// `id` over `frames` frames.
#define _ACTOR120300_BLEND_SLOTS(work, id, frames)                                \
    do {                                                                          \
        u16 _i;                                                                   \
        for (_i = 1; _i < ARRAY_SIZE((work)->rig.slots); _i++) {                  \
            animationSeekSlotWithBlend(&(work)->rig.anim, _i, (id), 0, (frames)); \
        }                                                                         \
    } while (0)

/// Records `anim` in the block's `bodyAnimation` and cross-fades every body
/// slot to it over ten frames.
static inline void _actor120300BlendAll(Task* task, u16 anim)
{
    _Actor120300Work* work = task->work;

    work->bodyAnimation = anim;
    _ACTOR120300_BLEND_SLOTS(work, anim, 10);
}

/// Records `anim` in the block's `bodyAnimation` and restarts every body slot
/// on it at rate 0x10.
static inline void _actor120300ResetAll(Task* task, u16 anim)
{
    _Actor120300Work* work = task->work;
    u16               i;

    work->bodyAnimation = anim;
    for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
        work->rig.slots[i].rate = ANIMATION_RATE_ONE;
        animationResetSlot(&work->rig.anim, i, anim);
    }
}

/// After `func_actor_120300_80131EE0`, performs the request posted in
/// `bodyRequest` (0..19): most start an animation on slots 1..19, recording it
/// in `bodyAnimation`, through `animationSeekSlotWithBlend` or
/// `animationResetSlot`; a few also place the body or the rifle with
/// `ACTOR_MESSAGE_PLACE` or change `scale`. Request 1 has two stages, counted
/// in `bodyRequestStep`: stage 1 slides the model on X until
/// `coord.t[0] < 0xF3D`. Every other request, and request 1 once the slide
/// ends, clears `bodyRequest`.
static void func_actor_120300_80132C60(Task* arg0)
{
    TmdObject*        tmd;
    GfxCoord*         coord;
    _Actor120300Work* work;
    s32               x;
    ActorTransform*   msg;

    tmd   = arg0->extra.tmd;
    work  = arg0->work;
    coord = tmd->coords;
    func_actor_120300_80131EE0(arg0);
    switch (work->bodyRequest) {
        case 1:
            switch (work->bodyRequestStep) {
                case 0:
                    TASK_MESSAGE_DISPATCH_POINTER(arg0, ACTOR_MESSAGE_PLACE, &D_actor_120300_80140A54[7], 0);
                    _actor120300ResetAll(arg0, 1);
                    work->bodyRequestStep++;
                    return;
                case 1:
                    x                   = coord->coord.t[0];
                    coord->composeStamp = GRAPHICS_COORD_DIRTY;
                    x                  -= 0x14;
                    coord->coord.t[0]   = x;
                    if (x < 0xF3D) {
                        _actor120300BlendAll(arg0, 0xE);
                        work->bodyRequest = 0;
                    }
                    return;
            }
            return;
        case 2:
            TASK_MESSAGE_DISPATCH_POINTER(arg0, ACTOR_MESSAGE_PLACE, &D_actor_120300_80140A54[8], 0);
            break;
        case 3:
            _actor120300BlendAll(arg0, 4);
            break;
        case 4:
            _actor120300BlendAll(arg0, 0x12);
            break;
        case 5:
            _actor120300BlendAll(arg0, 6);
            break;
        case 6:
            _actor120300BlendAll(arg0, 7);
            break;
        case 7:
            _actor120300BlendAll(arg0, 0xD);
            break;
        case 8:
            msg = &D_actor_120300_80140A54[9];
            TASK_MESSAGE_DISPATCH_POINTER(arg0, ACTOR_MESSAGE_PLACE, msg, 0);
            TASK_MESSAGE_DISPATCH_POINTER(work->rifleTask, ACTOR_MESSAGE_PLACE, msg + 2, 0);
            _actor120300ResetAll(arg0, 8);
            break;
        case 9:
            _actor120300BlendAll(arg0, 0xB);
            break;
        case 10:
            _actor120300BlendAll(arg0, 9);
            break;
        case 11:
            _actor120300BlendAll(arg0, 0xA);
            break;
        case 12:
            _actor120300BlendAll(arg0, 0xC);
            break;
        case 13:
            work->scale = 0x400;
            TASK_MESSAGE_DISPATCH_POINTER(arg0, ACTOR_MESSAGE_PLACE, &D_actor_120300_80140A54[12], 0);
            _actor120300ResetAll(arg0, 0xE);
            break;
        case 14:
            work->scale = 0x1000;
            break;
        case 15:
            _actor120300BlendAll(arg0, 0xF);
            break;
        case 16:
            _actor120300BlendAll(arg0, 0x10);
            break;
        case 17:
            TASK_MESSAGE_DISPATCH_POINTER(arg0, ACTOR_MESSAGE_PLACE, &D_actor_120300_80140A54[10], 0);
            _actor120300ResetAll(arg0, 0x11);
            break;
        case 18:
            TASK_MESSAGE_DISPATCH_POINTER(arg0, ACTOR_MESSAGE_PLACE, &D_actor_120300_80140A54[9], 0);
            _actor120300ResetAll(arg0, 8);
            break;
        case 19:
            _actor120300BlendAll(arg0, 5);
            break;
        case 0:
        default:
            break;
    }
    work->bodyRequest = 0;
}

/// Sets the actor up for play: clears the model's flags, places the body,
/// resets animation slots 1..19 to animation 8, which it records in
/// `bodyAnimation`, shows the models of `headTask` and `rifleTask` and sets
/// the rifle down at its place in the room.
/// `func_actor_120300_801337C4` calls it with 1 once flag nibble 0x2D is set; a
/// zero argument additionally sends `playerTask` the equipped weapon's
/// animation (`AnimationPlayRequest`, built from `gPlayerStatus.weapon`) and a
/// placement, restores `scale` to 0x1000 and drops the pending overlay
/// replacement. `playerRequest` and `bodyRequest` are cleared either way.
void func_actor_120300_80133330(s32 arg0)
{
    Task*                task;
    _Actor120300Work*    work;
    _Actor120300Work*    animWork;
    SVECTOR              unused;
    AnimationPlayRequest rec;
    s32                  i;
    s32                  weaponId;
    s32                  id;

    task                   = D_actor_120300_80141BA8;
    work                   = task->work;
    task->extra.tmd->flags = 0;
    TASK_MESSAGE_DISPATCH_POINTER(task, ACTOR_MESSAGE_PLACE, &D_actor_120300_80140A54[9], 0);

    animWork                = task->work;
    animWork->bodyAnimation = 8;
    i                       = 1;
    do {
        animWork->rig.slots[(u16)i].rate = ANIMATION_RATE_ONE;
        animationResetSlot(&animWork->rig.anim, (u16)i, 8);
        i++;
    } while ((u16)i < ARRAY_SIZE(animWork->rig.slots));

    taskMessageDispatch(work->headTask, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
    taskMessageDispatch(work->rifleTask, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
    TASK_MESSAGE_DISPATCH_POINTER(work->rifleTask, ACTOR_MESSAGE_PLACE, &D_actor_120300_80140A54[11], 0);
    if (arg0 == 0) {
        weaponId                 = gPlayerStatus.weapon;
        id                       = (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
        rec.source.index         = id;
        rec.animationId          = 1;
        rec.blend                = ANIMATION_BLEND_RESET;
        rec.blendFrames          = 0;
        rec.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
        TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, ANIMATION_MESSAGE_PLAY, &rec, 0);
        TASK_MESSAGE_DISPATCH_POINTER(work->playerTask, GAME_ACTOR_MESSAGE_PLACE, &D_actor_120300_80140A54[5], 0);
        work->scale = 0x1000;
        CdCmd_CancelReplaceAndActivate();
    }
    work->playerRequest = 0;
    work->bodyRequest   = 0;
}

/// Runs the interaction selected in the block's `interaction`. On
/// `interactionStep` 0 it starts the interaction's event script and advances
/// the step: a talk starts the script `talkStage` selects (the first, then
/// the second, then the third from then on), a remark its one script. On
/// step 1 it returns 1 once `gGameSession->eventState` is back to 0, which
/// tells the caller the interaction is over; otherwise it returns 0.
static s32 func_actor_120300_801334A4(Task* arg0)
{
    _Actor120300Work* work;

    work = arg0->work;
    switch (work->interaction) {
        case ACTOR_120300_INTERACTION_TALK:
            switch (work->interactionStep) {
                case 0:
                    switch (work->talkStage) {
                        case 0:
                            func_800E8614(D_actor_120300_801416D4, 0);
                            work->talkStage++;
                            break;
                        case 1:
                            func_800E8614(D_actor_120300_801417AC, 0);
                            work->talkStage++;
                            break;
                        default:
                            func_800E8614(D_actor_120300_80141884, 0);
                            break;
                    }
                    work->interactionStep++;
                    break;
                case 1:
                    if (gGameSession->eventState == 0) {
                        return 1;
                    }
                    break;
            }
            break;
        case ACTOR_120300_INTERACTION_REMARK:
            switch (work->interactionStep) {
                case 0:
                    func_800E8614(D_actor_120300_80141A34, 0);
                    work->interactionStep++;
                    break;
                case 1:
                    if (gGameSession->eventState == 0) {
                        return 1;
                    }
                    break;
            }
            break;
    }
    return 0;
}

/// Initialize the cutscene actor's model, animations and child tasks.
///
/// Uses the area placement for resource-entry 0x6A, or the end record when
/// that entry is absent. Allocation failure kills `task`.
static void func_actor_120300_801335D8(Task* task)
{
    enum { TEXTURE_RESOURCE_ENTRY_ID = 0x6A };

    _Actor120300Work* work;
    _Actor120300Work* allocatedWork;
    _Actor120300Work* animWork;
    TmdObject*        tmd;
    GfxCoord*         coord;
    AreaPlacement*    place;
    u8                entryId;
    s32               slotIndex;

    tmd           = task->extra.tmd;
    coord         = tmd->coords;
    allocatedWork = memMalloc(sizeof(_Actor120300Work), false);
    task->work    = allocatedWork;
    if (allocatedWork == NULL) {
        taskKill(task);
        return;
    }
    work = allocatedWork;
    memFillBytes(work, 0, sizeof(*work));
    work->playerTask        = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    D_actor_120300_80141BA8 = task;
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
    tmdSetTextureOffsets(tmd, place->texturePageOffset, place->clutRowOffset);
    animationInitContext(&work->rig.anim, D_actor_120300_80140910, tmd, work->rig.poses, work->rig.slots);
    animWork                = task->work;
    animWork->bodyAnimation = 0xE;
    slotIndex               = 1;
    do {
        animWork->rig.slots[(u16)slotIndex].rate = ANIMATION_RATE_ONE;
        animationResetSlot(&animWork->rig.anim, (u16)slotIndex, 0xE);
        slotIndex++;
    } while ((u16)slotIndex < ARRAY_SIZE(animWork->rig.slots));
    work->headTask  = taskSpawnFromTable(D_actor_120300_80141B6C, 2, 0, task);
    work->rifleTask = taskSpawnFromTable(D_actor_120300_80141B6C, 3, 0, task);
    task->msgTable  = D_actor_120300_80140A44;
    work->scale     = 0x1000;
    taskReparent(task, work->headTask);
    taskReparent(task, work->rifleTask);
}

/// Main tick of the cutscene actor. State 0 waits until no other cutscene is
/// up (`Gp_StateC08.mode` / `gDisplayState.pendingMode`), builds the work block, then either arms
/// play (`func_actor_120300_80133330`) once flag nibble 0x2D is set or sends
/// the slot-3 weapon record and starts the script. States 1-4 step the area
/// records, the pending `Gp_TakePendingObj4C` cue, and the overlay-load
/// phases. Every path but the cutscene-busy early-out then ticks the two
/// animation helpers, draws the floor quad, and scales the model.
void func_actor_120300_801337C4(Task* arg0)
{
    union {
        struct {
            SVECTOR shadowOffset;
            VECTOR  vec;
        } draw;
        AnimationPlayRequest rec;
    } scratch;
    _Actor120300Work* work;
    _Actor120300Work* temp;
    TmdObject*        tmd;
    s32               state;
    s32               weaponId;
    s32               scale;
    s16               ready;
    s32               take;
    u16               scaleRaw;
    u16               evtId;
    u8                evtKind;
    u8                evtSub;

    state = arg0->state;
    work  = arg0->work;
    switch (state) {
        case 0:
            if ((Gp_StateC08.mode != ATTACHMENT_MODE_WHEEL) && (gDisplayState.pendingMode == DISPLAY_MODE_NONE)) {
                func_actor_120300_801335D8(arg0);
                work = arg0->work;
                if (gameFlagGetNibble(GAME_FLAG_GARAGE_GARY_SCENE_SEEN) != 0) {
                    func_actor_120300_80133330(1);
                    if (gameFlagGetNibble(GAME_FLAG_MOTEL_ROOM_6_DOOR_UNLOCKED) != 0) {
                        work->talkStage = 1;
                    }
                    arg0->state = 4;
                } else {
                    weaponId = gPlayerStatus.weapon;
                    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId == 1) {
                        weaponId = weaponId + 1;
                    } else {
                        weaponId = weaponId + 0x22;
                    }
                    scratch.rec.source.index         = weaponId;
                    scratch.rec.animationId          = 1;
                    scratch.rec.blend                = ANIMATION_BLEND_RESET;
                    scratch.rec.blendFrames          = 0;
                    scratch.rec.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
                    TASK_MESSAGE_DISPATCH_POINTER(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), ANIMATION_MESSAGE_PLAY, &scratch.rec, 0);
                    gameFlagSetNibble(GAME_FLAG_02C, 1);
                    gameFlagSetNibble(GAME_FLAG_GARAGE_GARY_SCENE_SEEN, 1);
                    func_800E3FAC(0xA2, 0xB);
                    func_800E8634(D_actor_120300_80140B94, 0, D_actor_120300_80141524);
                    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = 2;
                    arg0->state                                        += 1;
                }
                memCopyBytes(&D_actor_120300_801409A8, gDryfieldGarageCollision0108CNormals, sizeof(D_actor_120300_801409A8));
                memCopyBytes(&D_actor_120300_80140A20, gDryfieldGarageCollision0108CFaces, sizeof(D_actor_120300_80140A20));
                memCopyBytes(&D_actor_120300_801409C0, gDryfieldGarageCollision0108CVerts, sizeof(D_actor_120300_801409C0));
                break;
            }
            return;
        case 1:
            if (gGameSession->eventState == 0) {
                Gp_ApplyAreaRecs(D_dryfield_garage_80180204);
                arg0->state += 1;
            }
            break;
        case 2:
            ready = 0;
            temp  = arg0->work;
            if ((s16)Gp_TakePendingObj4C(&evtId, &evtKind, &evtSub) != 0) {
                if (!((s16)evtId & WORLD_COLLISION_TRIGGER_AUTOMATIC)) {
                    if ((evtId & (0xFFFF ^ WORLD_COLLISION_TRIGGER_AUTOMATIC)) == WORLD_COLLISION_TRIGGER_ACTION_ROOM) {
                        ready = gPlayerStatus.interactionPressed != 0;
                    }
                }
            }
            if (ready != 0) {
                if ((s8)evtKind == 1) {
                    temp->interaction = ACTOR_120300_INTERACTION_TALK;
                }
                if ((s8)evtKind == 2) {
                    temp->interaction = ACTOR_120300_INTERACTION_REMARK;
                }
                temp->interactionStep = 0;
                take                  = 1;
            } else {
                take = 0;
            }
            if (take != 0) {
                arg0->state += 1;
            }
            break;
        case 3:
            if ((s16)func_actor_120300_801334A4(arg0) != 0) {
                arg0->state -= 1;
            }
            break;
        case 4:
            TASK_MESSAGE_DISPATCH_POINTER(work->rifleTask, ACTOR_MESSAGE_PLACE, &D_actor_120300_80140A54[11], 0);
            arg0->state = 2;
            break;
    }

    func_actor_120300_80132338(arg0);
    func_actor_120300_80132C60(arg0);
    scratch.draw.shadowOffset.vx = 0;
    scratch.draw.shadowOffset.vy = 0x380;
    scratch.draw.shadowOffset.vz = 0;
    actorRenderDrawGroundShadow(&arg0->extra.tmd->coords[1], 0x300, &scratch.draw.shadowOffset);
    tmd      = arg0->extra.tmd;
    scaleRaw = work->scale;
    func_actor_120300_FillLight(arg0, tmd, &scratch.draw.vec);
    scale               = scaleRaw;
    scratch.draw.vec.vz = scale;
    scratch.draw.vec.vy = scale;
    scratch.draw.vec.vx = scale;
    ScaleMatrix(tmd->colorMtx, &scratch.draw.vec);
}

#include "../../shared/screen_fade_out.inc.c"

/// Message 0x7D5 handler: a nonzero `arg2` shows the task's model (clears
/// `TmdObject` flag 0x80), zero hides it. `arg1` is the message id.
void func_actor_120300_80133C38(Task* task, s32 arg1, s32 arg2, s32 arg3)
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

/// Broadcasts message 0x7D5 -- the visibility control the actor's display task
/// handles -- to the actor itself and to the `headTask` and `rifleTask` of
/// its work block.
/// Sending it is the whole body: `arg0` is the message's payload and only 0/1
/// are accepted.
void func_actor_120300_80133D04(s32 arg0)
{
    _Actor120300Work* work = D_actor_120300_80141BA8->work;

    if (arg0 == 0) {
        taskMessageDispatch(D_actor_120300_80141BA8, ACTOR_MESSAGE_SET_MODEL_DRAW, 0, 0);
        taskMessageDispatch(work->headTask, ACTOR_MESSAGE_SET_MODEL_DRAW, 0, 0);
        taskMessageDispatch(work->rifleTask, ACTOR_MESSAGE_SET_MODEL_DRAW, 0, 0);
    } else if (arg0 == 1) {
        taskMessageDispatch(D_actor_120300_80141BA8, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
        taskMessageDispatch(work->headTask, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
        taskMessageDispatch(work->rifleTask, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
    }
}

void func_actor_120300_80133DA4(void)
{
    CdCmd_EnqueueReplaceOverlay82();
    gGameSession->viewDirty = 1;
}

void func_actor_120300_80133DD4(void)
{
    CdCmd_EnqueueOverlay81();
}

void func_actor_120300_80133DF4(void)
{
    Gp_RestoreStreamRng();
}

void func_actor_120300_80133E14(s16 arg0)
{
    _Actor120300Work* work = D_actor_120300_80141BA8->work;

    work->playerRequest     = arg0;
    work->playerRequestStep = 0;
}

void func_actor_120300_80133E34(s16 arg0)
{
    _Actor120300Work* work = D_actor_120300_80141BA8->work;

    work->bodyRequest     = arg0;
    work->bodyRequestStep = 0;
}

/// Kills the player's equipment tasks for the scene, once: sets the block's
/// `playerEquipmentRemoved`, which `func_actor_120300_80133E94` clears.
void func_actor_120300_80133E54(void)
{
    _Actor120300Work* work = D_actor_120300_80141BA8->work;

    if (work->playerEquipmentRemoved == 0) {
        work->playerEquipmentRemoved = 1;
        Gp_KillPlayerEffs();
    }
}

/// Gives the player's equipment back if the scene removed it: spawns the
/// weapon's task again, clears the block's `playerEquipmentRemoved` and has
/// the player play the equipped weapon's animation.
void func_actor_120300_80133E94(void)
{
    _Actor120300Work* work = D_actor_120300_80141BA8->work;

    if (work->playerEquipmentRemoved != 0) {
        Gp_SpawnWeaponEff();
        work->playerEquipmentRemoved = 0;
        Gp_MsgPlayerWeapon(0);
    }
}

/// Spawns the fade task (entry 4 of the actor's task table) at rate 9.
void func_actor_120300_80133EE4(void)
{
    taskSpawnFromTable(D_actor_120300_80141B6C, 4, 9, 0);
}

void func_actor_120300_80133F14(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            func_800E8614(D_actor_120300_8014195C, 0);
            arg0->state += 1;
            break;
        case 1:
            if (gGameSession->eventState == 0) {
                taskKill(arg0);
            }
            break;
    }
}
