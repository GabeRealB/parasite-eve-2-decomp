#include "actors/actor_310100.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/enemy.h"
#include "gameplay/message.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "rooms/acropolis_plaza.h"

/// Area placement entries the two police officers are spawned from; the same
/// IDs select each officer's placement record in the plaza's nested layout.
enum {
    ACTOR_310100_PLACEMENT_OFFICER_1 = 0x6C,
    ACTOR_310100_PLACEMENT_OFFICER_2 = 0x6D,
};

/// Ground shadow of an officer model, in the units of its part-1 frame.
enum {
    ACTOR_310100_SHADOW_SIDE = 0x300, // Side of the shadow square
    ACTOR_310100_SHADOW_DROP = 0x380, // Distance from the frame's origin down to the shadow
};

/// Values of `_Actor310100PoliceOfficerWork::playState`.
///
/// The body model draws its floor quad in every state. The culled-body model
/// never steps its animation after spawn; officer 1's draws its floor quad
/// only while posed, officer 2's until frozen.
enum {
    ACTOR_310100_PLAY_STATE_POSED   = 0, // Holding the spawn pose: relit each frame, animation not stepped
    ACTOR_310100_PLAY_STATE_PLAYING = 1, // A play request arrived: the body model steps its animation and follow-ups
    ACTOR_310100_PLAY_STATE_FROZEN  = 2, // Neither stepped nor relit
};

/// Work block of one acropolis-plaza police officer, kept at `Task::work` by
/// both of the officer's tasks.
///
/// The controller task is the placed actor the room's messages reach. It shows
/// and hides the officer's model as the plaza movie advances and uses only
/// `modelTask`, `spawnAnimationId` and `bodyAnimationId`. It allocates the
/// block without clearing it, so those members are indeterminate until written.
/// The model task clears its own block at spawn and uses the rig, the matrices
/// and the playback members; its `modelTask` stays `NULL`.
///
/// No access to the `pad_` runs has been observed; whether they are unused
/// members or padding is unproven.
typedef struct {
    ActorAnimRig19         rig;               // Model: playback storage of the nineteen-part officer model
    MATRIX                 light;             // Model: light matrix lent to the model object
    MATRIX                 color;             // Model: colour matrix lent to the model object
    byte                   pad_47C[0x68];
    Task*                  modelTask;         // Controller: the officer's live model task, `NULL` once hidden or torn down
    Task*                  playerTask;        // Model: the player's task, asked to play the animations paired with the officer's
    const AnimationRecord* lastCueRecord;     // Model: record slot 1 was on at the last step, so its sound cues fire once; borrowed from the bound set
    u16                    playState;         // Model: `ACTOR_310100_PLAY_STATE_*`
    byte                   pad_4F2[0x4];
    u16                    playerAnimationId; // Model: animation last requested of the player; indexes the player's follow-up table
    u16                    followUpTable;     // Model: follow-up table the officer chains through (0 the player's until the first request, 1 officer 1, 2 officer 2)
    u16                    animationId;       // Model: animation last requested of the officer or chained to; 0 at spawn whatever the spawn animation
    byte                   pad_4FC[0x8];
    u16                    spawnAnimationId;  // Controller: animation officer 2's culled-body model spawns on. Model: the one a culled-body model spawned on
    u16                    bodyAnimationId;   // Controller: animation the body model starts on when a draw-mode message swaps it in
    u16                    placementId;       // Model: its `ACTOR_310100_PLACEMENT_*`, stored just before the spawn clear and so always read back as 0
    u16                    stepSoundIndex;    // Model: next entry of the officer-1 step-sound sequence, stopping at the last
} _Actor310100PoliceOfficerWork;
STATIC_ASSERT_SIZEOF(_Actor310100PoliceOfficerWork, 0x50C);

/// Step sounds the model runs through while it is on the 0x6C display id:
/// `stepSoundIndex` indexes the first three.
extern s32 D_actor_310100_801798A8[];

/// Model frame handler: queues the step sound for the animation record slot 1
/// has just entered — from `D_actor_310100_801798A8` while the model is on the
/// 0x6C display id, from the fixed 0x51050006 / 0x51050007 pair otherwise — then
/// ticks slots 1..0x12 and returns slot 1's `ANIMATION_SLOT_REACHED_BOUNDARY`
/// result from `flags`.
static s32 func_actor_310100_80161E24(Task* task);

/// State handler for the display model spawned by `func_actor_310100_80162C64`:
/// the spawn tick seeds the tracker from the model's part-1 coordinate frame and
/// steps to state 1, and every later tick draws the floor quad until the display
/// state goes non-zero.
void func_actor_310100_801631B0(Task* task);

/// Second state handler of the display model spawned from
/// `D_actor_310100_801798FC` (descriptor arg 0x80168C00): the spawn tick hands
/// the model to `func_actor_310100_80162414` with display id 0x6C and steps to
/// state 1, and every later tick draws the floor quad at the model's part-1
/// frame, runs `func_actor_310100_80161F80` while the display state is 1 and
/// hands that frame's translation to `func_800D7A9C`. Display state 2, the
/// freeze parked by `func_actor_310100_80162CDC`, returns before either.
void func_actor_310100_80162F88(Task* task);

/// Second state handler of the display model spawned from
/// `D_actor_310100_80179920` (descriptor arg 0x801730B0): the spawn tick hands
/// the model to `func_actor_310100_80162414` with display id 0x6D and steps to
/// state 1, and every later tick draws the floor quad at the model's part-1
/// frame, runs `func_actor_310100_80161F80` while the display state is 1 and
/// hands that frame's translation to `func_800D7A9C`. Display state 2, the
/// freeze parked by `func_actor_310100_80162CDC`, returns before either.
void func_actor_310100_8016309C(Task* task);

/// The other display-model state handler (message 0x6D): the spawn tick seeds
/// the tracker from the model's part-1 coordinate frame and steps to state 1,
/// and every later tick draws the floor quad while the display state is still
/// below 2.
void func_actor_310100_801632B0(Task* task);

/// Message 0x7D5 handler: kills the display task hanging off the work block,
/// records the payload's `pos.vy` in the work block and spawns a fresh display
/// task from `D_actor_310100_801798E4`. The display task is handed `arg2` as its
/// `spawnArg1` and this task as its parent (`spawnArg2`); it spawns the model
/// task in turn, handing it `bodyAnimationId` as its `spawnArg1`.
s32 func_actor_310100_80162C64(Task* task, s32 msgId, s32 arg2, ActorTransform* placement);

/// Message 0x7D7 handler: parks the display task's work block at state 2 and
/// returns when handed mode 3, otherwise tears the display task down and spawns
/// a fresh one from `D_actor_310100_801798F0`.
s32 func_actor_310100_80162CDC(Task* task, s32 msgId, s32 arg2, s32 arg3);

/// Message 0x7D4 handler: drops the payload's translation into the display
/// task's root coordinate frame, yaws that frame to the payload's `rot.vy` and
/// marks it dirty.
s32 func_actor_310100_80162EC8(Task* task, s32 msgId, ActorTransform* placement, s32 arg3);

/// Teardown handler: kills the display task hanging off the work block and
/// parks this task in state 3.
s32 func_actor_310100_80162F34(Task* task, s32 msgId, s32 arg2, s32 arg3);

extern TaskDesc D_actor_310100_801798E4;
extern TaskDesc D_actor_310100_801798F0;

extern AnimationSet* D_actor_310100_80179754[16];
extern AnimationSet* D_actor_310100_80179794[26];

extern TaskMessageEntry D_actor_310100_801798B4[6];
extern s16*             D_actor_310100_8017989C[];

extern s16 D_actor_310100_80179830[12];
extern s16 D_actor_310100_80179848[16];
extern s16 D_actor_310100_80179868[26];

static TmdSource _gActor310100PoliceOfficer1Body;
static TmdSource _gActor310100PoliceOfficer1CulledBody;
static TmdSource _gActor310100PoliceOfficer2Body;
static TmdSource _gActor310100PoliceOfficer2CulledBody;
void             func_actor_310100_801620FC(Task*);
void             func_actor_310100_80162284(Task*);
void             func_actor_310100_801627BC(Task*);
void             func_actor_310100_801629FC(Task*);
void             func_actor_310100_80162F88(Task*);
void             func_actor_310100_8016309C(Task*);
void             func_actor_310100_801631B0(Task*);
void             func_actor_310100_801632B0(Task*);

static TmdBone _gActor310100PoliceOfficer1BodySkeleton[19] = {
#include "assets/police_officer_1_body_skeleton.inc"
};

static u32 _gActor310100PoliceOfficer1BodyPartVerts[19] = {
#include "assets/police_officer_1_body_partVerts.inc"
};

static SVECTOR _gActor310100PoliceOfficer1BodyVerts[351] = {
#include "assets/police_officer_1_body_verts.inc"
};

static SVECTOR _gActor310100PoliceOfficer1BodyNormals[426] = {
#include "assets/police_officer_1_body_normals.inc"
};

static u32 _gActor310100PoliceOfficer1BodyStream[3905] = {
#include "assets/police_officer_1_body_stream.inc"
};

static TmdSource _gActor310100PoliceOfficer1Body = {
    0,
    21260,
    5896,
    19,
    _gActor310100PoliceOfficer1BodyPartVerts,
    _gActor310100PoliceOfficer1BodyVerts,
    _gActor310100PoliceOfficer1BodyNormals,
    _gActor310100PoliceOfficer1BodySkeleton,
    _gActor310100PoliceOfficer1BodyStream,
};

static TmdBone _gActor310100PoliceOfficer1CulledBodySkeleton[19] = {
#include "assets/police_officer_1_culled_body_skeleton.inc"
};

static u32 _gActor310100PoliceOfficer1CulledBodyPartVerts[19] = {
#include "assets/police_officer_1_culled_body_partVerts.inc"
};

static SVECTOR _gActor310100PoliceOfficer1CulledBodyVerts[333] = {
#include "assets/police_officer_1_culled_body_verts.inc"
};

static SVECTOR _gActor310100PoliceOfficer1CulledBodyNormals[361] = {
#include "assets/police_officer_1_culled_body_normals.inc"
};

static u32 _gActor310100PoliceOfficer1CulledBodyStream[3056] = {
#include "assets/police_officer_1_culled_body_stream.inc"
};

static TmdSource _gActor310100PoliceOfficer1CulledBody = {
    0,
    16368,
    4548,
    19,
    _gActor310100PoliceOfficer1CulledBodyPartVerts,
    _gActor310100PoliceOfficer1CulledBodyVerts,
    _gActor310100PoliceOfficer1CulledBodyNormals,
    _gActor310100PoliceOfficer1CulledBodySkeleton,
    _gActor310100PoliceOfficer1CulledBodyStream,
};

static TmdBone _gActor310100PoliceOfficer2BodySkeleton[19] = {
#include "assets/police_officer_2_body_skeleton.inc"
};

static u32 _gActor310100PoliceOfficer2BodyPartVerts[19] = {
#include "assets/police_officer_2_body_partVerts.inc"
};

static SVECTOR _gActor310100PoliceOfficer2BodyVerts[361] = {
#include "assets/police_officer_2_body_verts.inc"
};

static SVECTOR _gActor310100PoliceOfficer2BodyNormals[434] = {
#include "assets/police_officer_2_body_normals.inc"
};

static u32 _gActor310100PoliceOfficer2BodyStream[4108] = {
#include "assets/police_officer_2_body_stream.inc"
};

static TmdSource _gActor310100PoliceOfficer2Body = {
    0,
    21848,
    6880,
    19,
    _gActor310100PoliceOfficer2BodyPartVerts,
    _gActor310100PoliceOfficer2BodyVerts,
    _gActor310100PoliceOfficer2BodyNormals,
    _gActor310100PoliceOfficer2BodySkeleton,
    _gActor310100PoliceOfficer2BodyStream,
};

static TmdBone _gActor310100PoliceOfficer2CulledBodySkeleton[19] = {
#include "assets/police_officer_2_culled_body_skeleton.inc"
};

static u32 _gActor310100PoliceOfficer2CulledBodyPartVerts[19] = {
#include "assets/police_officer_2_culled_body_partVerts.inc"
};

static SVECTOR _gActor310100PoliceOfficer2CulledBodyVerts[321] = {
#include "assets/police_officer_2_culled_body_verts.inc"
};

static SVECTOR _gActor310100PoliceOfficer2CulledBodyNormals[389] = {
#include "assets/police_officer_2_culled_body_normals.inc"
};

static u32 _gActor310100PoliceOfficer2CulledBodyStream[3052] = {
#include "assets/police_officer_2_culled_body_stream.inc"
};

static TmdSource _gActor310100PoliceOfficer2CulledBody = {
    0,
    16228,
    4760,
    19,
    _gActor310100PoliceOfficer2CulledBodyPartVerts,
    _gActor310100PoliceOfficer2CulledBodyVerts,
    _gActor310100PoliceOfficer2CulledBodyNormals,
    _gActor310100PoliceOfficer2CulledBodySkeleton,
    _gActor310100PoliceOfficer2CulledBodyStream,
};

static AnimationPackedPose _gActor310100Animation15FF8Bank1[21] = {
#include "assets/actor_310100_animation_15FF8_bank1.inc"
};

static AnimationPackedRotation _gActor310100Animation15FF8Bank4[77] = {
#include "assets/actor_310100_animation_15FF8_bank4.inc"
};

static AnimationRecord _gActor310100Animation15FF8Records[124] = {
#include "assets/actor_310100_animation_15FF8_records.inc"
};

static u16 _gActor310100Animation15FF8Indices[20] = {
#include "assets/actor_310100_animation_15FF8_indices.inc"
};

static AnimationSet _gActor310100Animation15FF8 = {
    _gActor310100Animation15FF8Records,
    _gActor310100Animation15FF8Indices,
    { NULL, _gActor310100Animation15FF8Bank1, NULL, NULL, _gActor310100Animation15FF8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor310100Animation1620CBank1[3] = {
#include "assets/actor_310100_animation_1620C_bank1.inc"
};

static AnimationPackedRotation _gActor310100Animation1620CBank4[24] = {
#include "assets/actor_310100_animation_1620C_bank4.inc"
};

static AnimationRecord _gActor310100Animation1620CRecords[80] = {
#include "assets/actor_310100_animation_1620C_records.inc"
};

static u16 _gActor310100Animation1620CIndices[20] = {
#include "assets/actor_310100_animation_1620C_indices.inc"
};

static AnimationSet _gActor310100Animation1620C = {
    _gActor310100Animation1620CRecords,
    _gActor310100Animation1620CIndices,
    { NULL, _gActor310100Animation1620CBank1, NULL, NULL, _gActor310100Animation1620CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor310100Animation164B8Bank1[14] = {
#include "assets/actor_310100_animation_164B8_bank1.inc"
};

static AnimationPackedRotation _gActor310100Animation164B8Bank4[33] = {
#include "assets/actor_310100_animation_164B8_bank4.inc"
};

static AnimationRecord _gActor310100Animation164B8Records[76] = {
#include "assets/actor_310100_animation_164B8_records.inc"
};

static u16 _gActor310100Animation164B8Indices[20] = {
#include "assets/actor_310100_animation_164B8_indices.inc"
};

static AnimationSet _gActor310100Animation164B8 = {
    _gActor310100Animation164B8Records,
    _gActor310100Animation164B8Indices,
    { NULL, _gActor310100Animation164B8Bank1, NULL, NULL, _gActor310100Animation164B8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor310100Animation16CE8Bank1[57] = {
#include "assets/actor_310100_animation_16CE8_bank1.inc"
};

static AnimationPackedRotation _gActor310100Animation16CE8Bank4[121] = {
#include "assets/actor_310100_animation_16CE8_bank4.inc"
};

static AnimationRecord _gActor310100Animation16CE8Records[212] = {
#include "assets/actor_310100_animation_16CE8_records.inc"
};

static u16 _gActor310100Animation16CE8Indices[20] = {
#include "assets/actor_310100_animation_16CE8_indices.inc"
};

static AnimationSet _gActor310100Animation16CE8 = {
    _gActor310100Animation16CE8Records,
    _gActor310100Animation16CE8Indices,
    { NULL, _gActor310100Animation16CE8Bank1, NULL, NULL, _gActor310100Animation16CE8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor310100Animation16F64Bank1[10] = {
#include "assets/actor_310100_animation_16F64_bank1.inc"
};

static AnimationPackedRotation _gActor310100Animation16F64Bank4[37] = {
#include "assets/actor_310100_animation_16F64_bank4.inc"
};

static AnimationRecord _gActor310100Animation16F64Records[72] = {
#include "assets/actor_310100_animation_16F64_records.inc"
};

static u16 _gActor310100Animation16F64Indices[20] = {
#include "assets/actor_310100_animation_16F64_indices.inc"
};

static AnimationSet _gActor310100Animation16F64 = {
    _gActor310100Animation16F64Records,
    _gActor310100Animation16F64Indices,
    { NULL, _gActor310100Animation16F64Bank1, NULL, NULL, _gActor310100Animation16F64Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor310100Animation17148Bank1[2] = {
#include "assets/actor_310100_animation_17148_bank1.inc"
};

static AnimationPackedRotation _gActor310100Animation17148Bank4[19] = {
#include "assets/actor_310100_animation_17148_bank4.inc"
};

static AnimationRecord _gActor310100Animation17148Records[76] = {
#include "assets/actor_310100_animation_17148_records.inc"
};

static u16 _gActor310100Animation17148Indices[20] = {
#include "assets/actor_310100_animation_17148_indices.inc"
};

static AnimationSet _gActor310100Animation17148 = {
    _gActor310100Animation17148Records,
    _gActor310100Animation17148Indices,
    { NULL, _gActor310100Animation17148Bank1, NULL, NULL, _gActor310100Animation17148Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor310100Animation1744CBank1[2] = {
#include "assets/actor_310100_animation_1744C_bank1.inc"
};

static AnimationPackedRotation _gActor310100Animation1744CBank4[61] = {
#include "assets/actor_310100_animation_1744C_bank4.inc"
};

static AnimationRecord _gActor310100Animation1744CRecords[106] = {
#include "assets/actor_310100_animation_1744C_records.inc"
};

static u16 _gActor310100Animation1744CIndices[20] = {
#include "assets/actor_310100_animation_1744C_indices.inc"
};

static AnimationSet _gActor310100Animation1744C = {
    _gActor310100Animation1744CRecords,
    _gActor310100Animation1744CIndices,
    { NULL, _gActor310100Animation1744CBank1, NULL, NULL, _gActor310100Animation1744CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor310100Animation17638Bank1[2] = {
#include "assets/actor_310100_animation_17638_bank1.inc"
};

static AnimationPackedRotation _gActor310100Animation17638Bank4[30] = {
#include "assets/actor_310100_animation_17638_bank4.inc"
};

static AnimationRecord _gActor310100Animation17638Records[67] = {
#include "assets/actor_310100_animation_17638_records.inc"
};

static u16 _gActor310100Animation17638Indices[20] = {
#include "assets/actor_310100_animation_17638_indices.inc"
};

static AnimationSet _gActor310100Animation17638 = {
    _gActor310100Animation17638Records,
    _gActor310100Animation17638Indices,
    { NULL, _gActor310100Animation17638Bank1, NULL, NULL, _gActor310100Animation17638Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor310100Animation1790CBank1[11] = {
#include "assets/actor_310100_animation_1790C_bank1.inc"
};

static AnimationPackedRotation _gActor310100Animation1790CBank4[45] = {
#include "assets/actor_310100_animation_1790C_bank4.inc"
};

static AnimationRecord _gActor310100Animation1790CRecords[83] = {
#include "assets/actor_310100_animation_1790C_records.inc"
};

static u16 _gActor310100Animation1790CIndices[20] = {
#include "assets/actor_310100_animation_1790C_indices.inc"
};

static AnimationSet _gActor310100Animation1790C = {
    _gActor310100Animation1790CRecords,
    _gActor310100Animation1790CIndices,
    { NULL, _gActor310100Animation1790CBank1, NULL, NULL, _gActor310100Animation1790CBank4, NULL, NULL, NULL },
};

AnimationSet* D_actor_310100_80179754[16] = {
    NULL,
    &gAcropolisPlazaAnimation181CC,
    &gAcropolisPlazaAnimation183B8,
    &gAcropolisPlazaAnimation18614,
    &gAcropolisPlazaAnimation18904,
    &gAcropolisPlazaAnimation18DE0,
    &_gActor310100Animation15FF8,
    &_gActor310100Animation1620C,
    &_gActor310100Animation164B8,
    &_gActor310100Animation16CE8,
    &_gActor310100Animation16F64,
    &_gActor310100Animation17148,
    &_gActor310100Animation1744C,
    &_gActor310100Animation17638,
    &_gActor310100Animation1790C,
    NULL,
};

AnimationSet* D_actor_310100_80179794[26] = {
    NULL,
    NULL,
    &gAcropolisPlazaAnimation148BC,
    NULL,
    &gAcropolisPlazaAnimation156B4,
    &gAcropolisPlazaAnimation15864,
    &gAcropolisPlazaAnimation15CC8,
    &gAcropolisPlazaAnimation15F78,
    &gAcropolisPlazaAnimation1622C,
    &gAcropolisPlazaAnimation1643C,
    &gAcropolisPlazaAnimation16610,
    NULL,
    NULL,
    NULL,
    &gAcropolisPlazaAnimation168FC,
    &gAcropolisPlazaAnimation16D00,
    &gAcropolisPlazaAnimation16EE4,
    &gAcropolisPlazaAnimation170C0,
    &gAcropolisPlazaAnimation17298,
    &gAcropolisPlazaAnimation1754C,
    &gAcropolisPlazaAnimation17818,
    &gAcropolisPlazaAnimation17A00,
    &gAcropolisPlazaAnimation17C68,
    &gAcropolisPlazaAnimation17E54,
    &gAcropolisPlazaAnimation17FF4,
    NULL,
};

AnimationSet* D_actor_310100_801797FC[13] = {
    &gAcropolisPlazaAnimation18F98,
    &gAcropolisPlazaAnimation19610,
    &gAcropolisPlazaAnimation19AD8,
    &gAcropolisPlazaAnimation19D64,
    &gAcropolisPlazaAnimation19F7C,
    &gAcropolisPlazaAnimation1A4F8,
    &gAcropolisPlazaAnimation1A784,
    &gAcropolisPlazaAnimation1AAAC,
    &gAcropolisPlazaAnimation1AE04,
    &gAcropolisPlazaAnimation1AFA4,
    NULL,
    &gAcropolisPlazaAnimation1B1F8,
    NULL,
};

s16 D_actor_310100_80179830[12] = {
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
};

s16 D_actor_310100_80179848[16] = {
    0,
    -1,
    3,
    4,
    4,
    4,
    7,
    7,
    -1,
    -1,
    -1,
    11,
    11,
    11,
    -1,
    0,
};

s16 D_actor_310100_80179868[26] = {
    0,
    -1,
    -1,
    -1,
    5,
    5,
    -1,
    -1,
    5,
    24,
    24,
    24,
    24,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    20,
    -1,
    18,
    -1,
    0,
};

s16* D_actor_310100_8017989C[3] = {
    D_actor_310100_80179830,
    D_actor_310100_80179848,
    D_actor_310100_80179868,
};

s32 D_actor_310100_801798A8[3] = {
    0x51050008,
    0x51050009,
    0x5105000A,
};

s32 func_actor_310100_80162D50(Task* task, s32 msgId, ActorTransform* placement, s32 arg3);

// The callbacks installed here take the `TaskMessageHandler` argument positions
// and leave the result word unset: the event scripts sending these ids discard it.
TaskMessageEntry D_actor_310100_801798B4[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, func_actor_310100_80162D50 },
    { ACTOR_MESSAGE_PLACE, func_actor_310100_80162EC8 },
    { 2007, func_actor_310100_80162CDC },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_310100_80162C64 },
    { 2002, func_actor_310100_80162F34 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_actor_310100_801798E4 = { { { TASK_BODY_NONE, 192 } }, func_actor_310100_801620FC, { .value = 0 } };

TaskDesc D_actor_310100_801798F0 = { { { TASK_BODY_NONE, 192 } }, func_actor_310100_80162284, { .value = 0 } };

TaskDesc D_actor_310100_801798FC[3] = {
    { { { TASK_BODY_NONE, 192 } }, func_actor_310100_801627BC, { .value = 0 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_310100_80162F88, { .model = &_gActor310100PoliceOfficer1Body } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_310100_801631B0, { .model = &_gActor310100PoliceOfficer1CulledBody } },
};

TaskDesc D_actor_310100_80179920[3] = {
    { { { TASK_BODY_NONE, 192 } }, func_actor_310100_801629FC, { .value = 0 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_310100_8016309C, { .model = &_gActor310100PoliceOfficer2Body } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_310100_801632B0, { .model = &_gActor310100PoliceOfficer2CulledBody } },
};

static void func_actor_310100_80161F80(Task* task);

static void func_actor_310100_801625E4(Task* task, s32 arg1);

static void func_actor_310100_80162414(Task* task, s32 arg1);

static s32 func_actor_310100_80161E24(Task* task)
{
    _Actor310100PoliceOfficerWork* work;
    const AnimationRecord*         rec;
    GfxCoord*                      obj;
    s32                            i;
    u16                            step;

    work = (_Actor310100PoliceOfficerWork*)task->work;
    obj  = task->extra.tmd->coords;
    rec  = Gp_AnimGetRec(&work->rig.anim, &work->rig.slots[1]);
    if (rec != work->lastCueRecord) {
        if (rec != NULL) {
            if (work->placementId == ACTOR_310100_PLACEMENT_OFFICER_1) {
                if (rec->flags & ANIMATION_RECORD_CUE_2) {
                    sndEvtRequestScriptStart(D_actor_310100_801798A8[work->stepSoundIndex], worldCoordGetOriginAudioPan(obj), 0);
                    step = work->stepSoundIndex;
                    if (step < 2U) {
                        work->stepSoundIndex = (u16)(step + 1);
                    }
                }
            } else {
                if (rec->flags & ANIMATION_RECORD_CUE_2) {
                    sndEvtRequestScriptStart(SOUND_ACROPOLIS_PLAZA_POLICE_STEP_1, worldCoordGetOriginAudioPan(obj), 0);
                }
                if (rec->flags & ANIMATION_RECORD_CUE_1) {
                    sndEvtRequestScriptStart(SOUND_ACROPOLIS_PLAZA_POLICE_STEP_2, worldCoordGetOriginAudioPan(obj), 0);
                }
            }
        }
        work->lastCueRecord = rec;
    }
    i = 1;
    do {
        animationTickSlot(&work->rig.anim, i & 0xFFFF);
        i += 1;
    } while ((u32)(i & 0xFFFF) < 0x13U);
    return work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY;
}

static void func_actor_310100_80161F80(Task* task)
{
    AnimationPlayRequest           arg;
    _Actor310100PoliceOfficerWork* work;
    _Actor310100PoliceOfficerWork* anim;
    _Actor310100PoliceOfficerWork* msg;
    Task*                          player;
    u16                            seed;
    u16                            ok;
    s32                            i;

    work = (_Actor310100PoliceOfficerWork*)task->work;
    if (func_actor_310100_80161E24(task) & 0xFFFF) {
        seed = D_actor_310100_8017989C[work->followUpTable][work->animationId];
        if (D_actor_310100_8017989C[work->followUpTable][work->animationId] >= 0) {
            anim = (_Actor310100PoliceOfficerWork*)task->work;
            i    = 1;
            do {
                animationSeekSlotWithBlend(&anim->rig.anim, i & 0xFFFF, seed & 0xFFFF, 0, 8);
                i += 1;
            } while ((u32)(i & 0xFFFF) < 0x13U);
            work->animationId = seed;
        }
    }
    player = ((_Actor310100PoliceOfficerWork*)task->work)->playerTask;
    if (player == NULL || taskMessageDispatch(player, ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0) {
        ok = 1;
    } else {
        ok = 0;
    }
    if (ok) {
        seed = D_actor_310100_8017989C[0][work->playerAnimationId];
        if (D_actor_310100_8017989C[0][work->playerAnimationId] >= 0) {
            msg = (_Actor310100PoliceOfficerWork*)task->work;
            if (msg->playerTask != NULL) {
                arg.source.sets          = D_actor_310100_801797FC;
                arg.animationId          = seed;
                arg.blend                = ANIMATION_BLEND_INTERPOLATE;
                arg.blendFrames          = 0xA;
                arg.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
                TASK_MESSAGE_DISPATCH_POINTER(msg->playerTask, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &arg, 0);
            }
            work->playerAnimationId = seed;
        }
    }
}

/// Spawn tick of the actor task, registered as its task state handler: states 1
/// and 2 — and state 0, which first parks `gDisplayState.control.flags.flipMode` at 2 — only step the
/// state, and state 3 spawns the display model. `spawnArg1` picks which one:
/// `D_actor_310100_80179920` with display id 0x6D, or `D_actor_310100_801798FC`
/// with 0x6C. It then walks the nested area place list for the record carrying
/// that id, drops the record's translation into the spawned model's root
/// coordinate frame, yaws that frame to the record's `yaw`, parks the
/// display work block's `playState` at 0 and tears this task down.
void func_actor_310100_801620FC(Task* task)
{
    _Actor310100PoliceOfficerWork* work;
    _Actor310100PoliceOfficerWork* display;
    Task*                          modelTask;
    TmdObject*                     obj;
    GfxCoord*                      coord;
    AreaPlacement*                 place;
    u8                             mode;

    work = (_Actor310100PoliceOfficerWork*)((Task*)task->spawnArg2.pointer)->work;
    switch (task->state) {
        case 0:
            gDisplayState.control.flags.flipMode = DISPLAY_FLIP_HOLD;
            /* fallthrough */
        case 1:
        case 2:
            task->state++;
            return;
        case 3:
            if (task->spawnArg1.value == 0) {
                do {
                    mode = ACTOR_310100_PLACEMENT_OFFICER_2;
                } while (0);
                work->modelTask = Task_SpawnOnDefaultList(D_actor_310100_80179920, 1, (s32)(work->bodyAnimationId), 0);
            } else if (task->spawnArg1.value == 1) {
                do {
                    do {
                        mode = ACTOR_310100_PLACEMENT_OFFICER_1;
                    } while (0);
                } while (0);
                work->modelTask = Task_SpawnOnDefaultList(D_actor_310100_801798FC, 1, (s32)(work->bodyAnimationId), 0);
            } else {
                goto skip;
            }
        skip:
            modelTask = work->modelTask;
            place     = Gp_GetNestedAreaRec(&gGameSession->location.loc)->placements;
            while (place->entryId != AREA_PLACEMENT_END && place->entryId != mode) {
                place++;
            }
            obj               = modelTask->extra.tmd;
            coord             = obj->coords;
            coord->coord.t[0] = place->x;
            coord->coord.t[1] = place->y;
            coord->coord.t[2] = place->z;
            gfxRotMatrixY(&coord->coord, place->yaw, 0);
            display            = (_Actor310100PoliceOfficerWork*)work->modelTask->work;
            display->playState = ACTOR_310100_PLAY_STATE_POSED;
            taskKill(task);
            Display_ResetHeapWrapper();
            break;
    }
}

/// Second spawn tick of the actor task: states 1 and 2 — and state 0, which
/// first parks `gDisplayState.control.flags.flipMode` at 2 — only step the state, and state 3 spawns the
/// display model on the default list: `D_actor_310100_80179920` with display id
/// 0x6D for `spawnArg1` 0, `D_actor_310100_801798FC` with 0x6C for 1, both at
/// table index 2 with `arg2` 5 and 7. It then walks the nested area place list
/// for the record carrying that id, drops the record's translation into the
/// spawned model's root coordinate frame, yaws that frame to the record's
/// `field_A`, parks the display work block's `playState` at 0 and tears this
/// task down.
void func_actor_310100_80162284(Task* task)
{
    _Actor310100PoliceOfficerWork* work;
    _Actor310100PoliceOfficerWork* display;
    Task*                          modelTask;
    TmdObject*                     obj;
    GfxCoord*                      coord;
    AreaPlacement*                 place;
    u8                             mode;

    work = (_Actor310100PoliceOfficerWork*)((Task*)task->spawnArg2.pointer)->work;
    switch (task->state) {
        case 0:
            gDisplayState.control.flags.flipMode = DISPLAY_FLIP_HOLD;
            /* fallthrough */
        case 1:
        case 2:
            task->state++;
            return;
        case 3:
            if (task->spawnArg1.value == 0) {
                do {
                    mode = ACTOR_310100_PLACEMENT_OFFICER_2;
                } while (0);
                work->modelTask = Task_SpawnOnDefaultList(D_actor_310100_80179920, 2, 5, 0);
            } else if (task->spawnArg1.value == 1) {
                do {
                    do {
                        mode = ACTOR_310100_PLACEMENT_OFFICER_1;
                    } while (0);
                } while (0);
                work->modelTask = Task_SpawnOnDefaultList(D_actor_310100_801798FC, 2, 7, 0);
            } else {
                goto skip;
            }
        skip:
            modelTask = work->modelTask;
            place     = Gp_GetNestedAreaRec(&gGameSession->location.loc)->placements;
            while (place->entryId != AREA_PLACEMENT_END && place->entryId != mode) {
                place++;
            }
            obj               = modelTask->extra.tmd;
            coord             = obj->coords;
            coord->coord.t[0] = place->x;
            coord->coord.t[1] = place->y;
            coord->coord.t[2] = place->z;
            gfxRotMatrixY(&coord->coord, place->yaw, 0);
            display            = (_Actor310100PoliceOfficerWork*)work->modelTask->work;
            display->playState = ACTOR_310100_PLAY_STATE_POSED;
            taskKill(task);
            Display_ResetHeapWrapper();
            break;
    }
}

/// Spawns the display model for `D_actor_310100_801798FC`: allocates the 0x50C
/// work block into `task->work`, hands it the view coordinate and the two TMD
/// buffers, binds the animation set selected by the display id (0x6C or 0x6D),
/// seeds its 18 slots, points `task->msgTable` at `D_actor_310100_801798B4` and
/// applies the nested area record matching that id through `Gp_SetTmdBytes`.
static void func_actor_310100_80162414(Task* task, s32 arg1)
{
    _Actor310100PoliceOfficerWork* work;
    _Actor310100PoliceOfficerWork* work2;
    TmdObject*                     obj;
    GfxCoord*                      coord;
    AreaPlacement*                 place;
    u16                            mode;
    u16                            active;
    u8                             id;
    s32                            i;

    coord      = task->extra.tmd->coords;
    obj        = task->extra.tmd;
    work       = memMalloc(sizeof(*work), false);
    mode       = arg1;
    task->work = work;
    if (work == NULL) {
        taskKill(task);
        return;
    }
    work->placementId = arg1;
    memFillBytes(task->work, 0U, sizeof(*work));
    work->playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    coord->parent    = &gGfxViewCoord;
    tmdAllocPrimitiveBuffer(obj);
    obj->lightMtx = &work->light;
    obj->colorMtx = &work->color;
    obj->flags    = 0;
    if (mode == ACTOR_310100_PLACEMENT_OFFICER_1) {
        animationInitContext(&work->rig.anim, D_actor_310100_80179754, obj, work->rig.poses,
                             &work->rig.slots[0]);
    } else {
        animationInitContext(&work->rig.anim, D_actor_310100_80179794, obj, work->rig.poses,
                             &work->rig.slots[0]);
    }
    i      = 1;
    active = task->spawnArg1.value;
    work2  = (_Actor310100PoliceOfficerWork*)task->work;
    do {
        work2->rig.slots[i & 0xFFFF].rate = ANIMATION_RATE_ONE;
        animationResetSlot(&work2->rig.anim, i & 0xFFFF, active);
        i += 1;
    } while ((u32)(i & 0xFFFF) < 0x13U);
    func_actor_310100_80161F80(task);
    task->msgTable = D_actor_310100_801798B4;
    id             = mode;
    place          = Gp_GetNestedAreaRec(&gGameSession->location.loc)->placements;
    while (place->entryId != AREA_PLACEMENT_END && place->entryId != id) {
        place++;
    }
    Gp_SetTmdBytes(obj, place->texturePageOffset, place->clutRowOffset);
}

/// Common spawn of the two floor-quad display handlers: `func_actor_310100_801631B0`
/// passes display id 0x6C and `func_actor_310100_801632B0` 0x6D. Does the same
/// 0x50C work block setup as `func_actor_310100_80162414`, except it also parks
/// the task's `spawnArg1` in `spawnAnimationId` — the argument `func_actor_310100_80162C64`
/// hands the display task it spawns — and reads it back as the payload the
/// eighteen animation slots are reset with.
static void func_actor_310100_801625E4(Task* task, s32 arg1)
{
    _Actor310100PoliceOfficerWork* work;
    _Actor310100PoliceOfficerWork* work2;
    TmdObject*                     obj;
    GfxCoord*                      coord;
    AreaPlacement*                 place;
    u16                            mode;
    u16                            active;
    u8                             id;
    s32                            i;

    coord      = task->extra.tmd->coords;
    obj        = task->extra.tmd;
    work       = memMalloc(sizeof(*work), false);
    mode       = arg1;
    task->work = work;
    if (work == NULL) {
        taskKill(task);
        return;
    }
    work->placementId = arg1;
    memFillBytes(task->work, 0U, sizeof(*work));
    work->playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    coord->parent    = &gGfxViewCoord;
    tmdAllocPrimitiveBuffer(obj);
    obj->lightMtx = &work->light;
    obj->colorMtx = &work->color;
    obj->flags    = 0;
    if (mode == ACTOR_310100_PLACEMENT_OFFICER_1) {
        animationInitContext(&work->rig.anim, D_actor_310100_80179754, obj, work->rig.poses,
                             &work->rig.slots[0]);
    } else {
        animationInitContext(&work->rig.anim, D_actor_310100_80179794, obj, work->rig.poses,
                             &work->rig.slots[0]);
    }
    i                      = 1;
    work->spawnAnimationId = task->spawnArg1.value;
    active                 = work->spawnAnimationId;
    work2                  = (_Actor310100PoliceOfficerWork*)task->work;
    do {
        work2->rig.slots[i & 0xFFFF].rate = ANIMATION_RATE_ONE;
        animationResetSlot(&work2->rig.anim, i & 0xFFFF, active);
        i += 1;
    } while ((u32)(i & 0xFFFF) < 0x13U);
    func_actor_310100_80161F80(task);
    task->msgTable = D_actor_310100_801798B4;
    id             = mode;
    place          = Gp_GetNestedAreaRec(&gGameSession->location.loc)->placements;
    while (place->entryId != AREA_PLACEMENT_END && place->entryId != id) {
        place++;
    }
    Gp_SetTmdBytes(obj, place->texturePageOffset, place->clutRowOffset);
}

/// Controller for the display model spawned from `D_actor_310100_801798FC`.
/// Plaza stream sub-ID 3 (`gCdCmdQueue.plazaStreamSubId`) forces the task into state
/// 3, where it idles. Otherwise state 0 allocates the 0x50C work block, and
/// states 1 and 2 toggle the model against the plaza scene frame: it is shown while
/// the stream sub-ID is 0/1 with `sceneFrame` at 0xE6 or above, or state 2
/// with the frame outside 7..0x6B. State 1 spawns it at the area place with
/// id 0x6C; state 2 kills it again once that condition drops.
void func_actor_310100_801627BC(Task* task)
{
    _Actor310100PoliceOfficerWork* work;
    _Actor310100PoliceOfficerWork* work2;
    AreaPlacement*                 place;
    GfxCoord*                      coord;
    Task*                          child;
    u16                            st;
    u16                            on;

    st = gCdCmdQueue.plazaStreamSubId;
    if (st == 3) {
        task->state = st;
    }
    switch (task->state) {
        case 0:
            task->work = memMalloc(sizeof(_Actor310100PoliceOfficerWork), false);
            if (task->work == NULL) {
                enemyDestroy(task->spawnArg2.pointer, task);
                return;
            }
            task->msgTable = D_actor_310100_801798B4;
            task->state++;
            break;
        case 1:
            on = 1;
            if (gCdCmdQueue.plazaStreamSubId < 2U) {
                on = gCdCmdQueue.sceneFrame >= 0xE6U;
            }
            if (gCdCmdQueue.plazaStreamSubId == 2 && (u32)(gCdCmdQueue.sceneFrame - 7) < 0x65U) {
                on = 0;
            }
            if (on) {
                work  = (_Actor310100PoliceOfficerWork*)task->work;
                place = Gp_GetNestedAreaRec(&gGameSession->location.loc)->placements;
                while (place->entryId != AREA_PLACEMENT_END && place->entryId != ACTOR_310100_PLACEMENT_OFFICER_1) {
                    place++;
                }
                child             = taskSpawnFromTable(D_actor_310100_801798FC, 2, 1, 0);
                work->modelTask   = child;
                coord             = child->extra.tmd->coords;
                coord->coord.t[0] = place->x;
                coord->coord.t[1] = place->y;
                coord->coord.t[2] = place->z;
                gfxRotMatrixY(&coord->coord, place->yaw, 0);
                task->state++;
            }
            break;
        case 2:
            on = 1;
            if (gCdCmdQueue.plazaStreamSubId < 2U) {
                on = gCdCmdQueue.sceneFrame >= 0xE6U;
            }
            if (gCdCmdQueue.plazaStreamSubId == 2 && (u32)(gCdCmdQueue.sceneFrame - 7) < 0x65U) {
                on = 0;
            }
            if (!on) {
                work2 = (_Actor310100PoliceOfficerWork*)task->work;
                task->state--;
                taskKill(work2->modelTask);
                work2->modelTask = NULL;
            }
            break;
    }
}

/// Controller for the display model spawned from `D_actor_310100_80179920`
/// (id 0x6D), the counterpart of `func_actor_310100_801627BC`. State 0 allocates
/// the 0x50C work block and seeds the display task's `spawnArg1` (`spawnAnimationId`)
/// with 0x18. The model is shown while the plaza stream sub-ID is 0/1 with frame
/// `sceneFrame` in 0x4B..0xC3; sub-IDs 2, 4 and 5 hide it. On hiding, state 2 keeps
/// the display task's `spawnAnimationId` before killing it.
void func_actor_310100_801629FC(Task* task)
{
    _Actor310100PoliceOfficerWork* work;
    AreaPlacement*                 place;
    GfxCoord*                      coord;
    Task*                          child;
    u16                            st;
    u16                            on;

    work = (_Actor310100PoliceOfficerWork*)task->work;
    st   = gCdCmdQueue.plazaStreamSubId;
    if (st == 3) {
        task->state = st;
    }
    switch (task->state) {
        case 0:
            task->work = (work = memMalloc(sizeof(_Actor310100PoliceOfficerWork), false));
            if (work == NULL) {
                enemyDestroy(task->spawnArg2.pointer, task);
                return;
            }
            task->msgTable         = D_actor_310100_801798B4;
            work->spawnAnimationId = 0x18;
            task->state++;
            break;
        case 1:
            on = 1;
            if (gCdCmdQueue.plazaStreamSubId < 2U) {
                on = (u32)(gCdCmdQueue.sceneFrame - 0x4B) < 0x79U;
            }
            if (gCdCmdQueue.plazaStreamSubId == 2) {
                on = 0;
            }
            if (gCdCmdQueue.plazaStreamSubId == 4) {
                on = 0;
            }
            if (gCdCmdQueue.plazaStreamSubId == 5) {
                on = 0;
            }
            if (on) {
                place = Gp_GetNestedAreaRec(&gGameSession->location.loc)->placements;
                while (place->entryId != AREA_PLACEMENT_END && place->entryId != ACTOR_310100_PLACEMENT_OFFICER_2) {
                    place++;
                }
                child             = taskSpawnFromTable(D_actor_310100_80179920, 2, (s32)(work->spawnAnimationId), 0);
                work->modelTask   = child;
                coord             = child->extra.tmd->coords;
                coord->coord.t[0] = place->x;
                coord->coord.t[1] = place->y;
                coord->coord.t[2] = place->z;
                gfxRotMatrixY(&coord->coord, place->yaw, 0);
                task->state++;
            }
            break;
        case 2:
            on = 1;
            if (gCdCmdQueue.plazaStreamSubId < 2U) {
                on = (u32)(gCdCmdQueue.sceneFrame - 0x4B) < 0x79U;
            }
            if (gCdCmdQueue.plazaStreamSubId == 2) {
                on = 0;
            }
            if (gCdCmdQueue.plazaStreamSubId == 4) {
                on = 0;
            }
            if (gCdCmdQueue.plazaStreamSubId == 5) {
                on = 0;
            }
            if (!on) {
                work->spawnAnimationId = ((_Actor310100PoliceOfficerWork*)work->modelTask->work)->spawnAnimationId;
                task->state--;
                taskKill(work->modelTask);
                work->modelTask = NULL;
            }
            break;
    }
}

s32 func_actor_310100_80162C64(Task* task, s32 msgId, s32 arg2, ActorTransform* placement)
{
    _Actor310100PoliceOfficerWork* work;

    work = (_Actor310100PoliceOfficerWork*)task->work;
    if (work->modelTask != NULL) {
        taskKill(work->modelTask);
    }
    work->bodyAnimationId = placement->pos.vy;
    Display_SpawnWithOt(&D_actor_310100_801798E4, 0, arg2, task);
}

/// Message 0x7D7 handler: parks the display task's work block at state 2 and
/// returns when handed mode 3, otherwise tears the display task down and spawns
/// a fresh one from `D_actor_310100_801798F0`.
s32 func_actor_310100_80162CDC(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    _Actor310100PoliceOfficerWork* work;
    _Actor310100PoliceOfficerWork* display;

    work    = (_Actor310100PoliceOfficerWork*)task->work;
    display = (_Actor310100PoliceOfficerWork*)work->modelTask->work;
    if (arg2 == 3) {
        display->playState = ACTOR_310100_PLAY_STATE_FROZEN;
        return;
    }
    if (work->modelTask != NULL) {
        taskKill(work->modelTask);
    }
    Display_SpawnWithOt(&D_actor_310100_801798F0, 0, arg2, task);
}

/// Message 0x7DD handler, and the display task's placement command: marks the
/// display work block dirty, then either forwards the payload to the animation
/// task (`pos.vx` zero — the seed carries the yaw into `playerAnimationId` and message
/// 0x3F4 gets `pos.vy` / `pos.vz` as a `AnimationPlayRequest`) or reseeds the nineteen
/// animation slots (`pos.vz` zero resets them through `animationResetSlot`,
/// otherwise `animationSeekSlotWithBlend` blends them) and records the new base in
/// `followUpTable` / `animationId`.
s32 func_actor_310100_80162D50(Task* task, s32 msgId, ActorTransform* placement, s32 arg3)
{
    AnimationPlayRequest           request;
    _Actor310100PoliceOfficerWork* work;
    _Actor310100PoliceOfficerWork* disp;
    _Actor310100PoliceOfficerWork* msgDisp;
    _Actor310100PoliceOfficerWork* resetDisp;
    Task*                          display;
    u16                            animationId;
    u16                            blendRequested;
    u16                            blend;
    u16                            active;
    s32                            i;

    work            = (_Actor310100PoliceOfficerWork*)task->work;
    display         = work->modelTask;
    disp            = (_Actor310100PoliceOfficerWork*)display->work;
    disp->playState = ACTOR_310100_PLAY_STATE_PLAYING;
    if (placement->pos.vx == 0) {
        disp->playerAnimationId = placement->pos.vy;
        msgDisp                 = (_Actor310100PoliceOfficerWork*)display->work;
        animationId             = placement->pos.vy;
        blendRequested          = placement->pos.vz;
        if (msgDisp->playerTask != NULL) {
            request.source.sets          = D_actor_310100_801797FC;
            request.animationId          = animationId;
            request.blend                = blendRequested;
            request.blendFrames          = 0xA;
            request.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;
            TASK_MESSAGE_DISPATCH_POINTER(msgDisp->playerTask, ANIMATION_MESSAGE_INSTALL_AND_PLAY, &request, 0);
        }
    } else {
        active    = placement->pos.vy;
        blend     = placement->pos.vz;
        resetDisp = (_Actor310100PoliceOfficerWork*)display->work;
        i         = 1;
        if (blend == 0) {
            do {
                resetDisp->rig.slots[i & 0xFFFF].rate = ANIMATION_RATE_ONE;
                animationResetSlot(&resetDisp->rig.anim, i & 0xFFFF, active);
                i += 1;
            } while ((u32)(i & 0xFFFF) < 0x13U);
        } else {
            do {
                animationSeekSlotWithBlend(&resetDisp->rig.anim, i & 0xFFFF, active, 0, 8);
                i += 1;
            } while ((u32)(i & 0xFFFF) < 0x13U);
        }
        disp->followUpTable = placement->pos.vx;
        disp->animationId   = placement->pos.vy;
    }
}

/// Message 0x7D4 handler: drops the payload's translation into the display
/// task's root coordinate frame, yaws that frame to the payload's `rot.vy` and
/// marks it dirty.
s32 func_actor_310100_80162EC8(Task* task, s32 msgId, ActorTransform* placement, s32 arg3)
{
    _Actor310100PoliceOfficerWork* work;
    GfxCoord*                      coord;

    work              = (_Actor310100PoliceOfficerWork*)task->work;
    coord             = work->modelTask->extra.tmd->coords;
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    gfxRotMatrixY(&coord->coord, placement->rot.vy, 0);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Teardown handler: kills the display task hanging off the work block and
/// parks this task in state 3.
s32 func_actor_310100_80162F34(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    _Actor310100PoliceOfficerWork* work;

    work = (_Actor310100PoliceOfficerWork*)task->work;
    if (work->modelTask != NULL) {
        taskKill(work->modelTask);
        work->modelTask = NULL;
    }
    task->state = 3;
}

/// Second state handler of the display model spawned from
/// `D_actor_310100_801798FC` (descriptor arg 0x80168C00): the spawn tick hands
/// the model to `func_actor_310100_80162414` with display id 0x6C and steps to
/// state 1, and every later tick draws the floor quad at the model's part-1
/// frame, runs `func_actor_310100_80161F80` while the display state is 1 and
/// hands that frame's translation to `func_800D7A9C`. Display state 2, the
/// freeze parked by `func_actor_310100_80162CDC`, returns before either.
void func_actor_310100_80162F88(Task* task)
{
    _Actor310100PoliceOfficerWork* work;
    SVECTOR                        rot;
    VECTOR                         vec;
    TmdObject*                     extra;

    work = (_Actor310100PoliceOfficerWork*)task->work;
    switch (task->state) {
        case 0:
            func_actor_310100_80162414(task, ACTOR_310100_PLACEMENT_OFFICER_1);
            task->state++;
            /* fallthrough */
        case 1:
            rot.vx = 0;
            rot.vy = 0x380;
            rot.vz = 0;
            Gp_DrawFloorQuad(&task->extra.tmd->coords[1], 0x300, &rot);
            switch (work->playState) {
                case ACTOR_310100_PLAY_STATE_POSED:
                    break;
                case ACTOR_310100_PLAY_STATE_PLAYING:
                    func_actor_310100_80161F80(task);
                    break;
                case ACTOR_310100_PLAY_STATE_FROZEN:
                default:
                    return;
            }
            extra  = task->extra.tmd;
            vec.vx = extra->coords[1].workm.t[0];
            vec.vy = task->extra.tmd->coords[1].workm.t[1];
            vec.vz = task->extra.tmd->coords[1].workm.t[2];
            func_800D7A9C(extra, &vec, 0, 3);
            break;
    }
}

/// Second state handler of the display model spawned from
/// `D_actor_310100_80179920` (descriptor arg 0x801730B0): the spawn tick hands
/// the model to `func_actor_310100_80162414` with display id 0x6D and steps to
/// state 1, and every later tick draws the floor quad at the model's part-1
/// frame, runs `func_actor_310100_80161F80` while the display state is 1 and
/// hands that frame's translation to `func_800D7A9C`. Display state 2, the
/// freeze parked by `func_actor_310100_80162CDC`, returns before either.
void func_actor_310100_8016309C(Task* task)
{
    _Actor310100PoliceOfficerWork* work;
    SVECTOR                        rot;
    VECTOR                         vec;
    TmdObject*                     extra;

    work = (_Actor310100PoliceOfficerWork*)task->work;
    switch (task->state) {
        case 0:
            func_actor_310100_80162414(task, ACTOR_310100_PLACEMENT_OFFICER_2);
            task->state++;
            /* fallthrough */
        case 1:
            rot.vx = 0;
            rot.vy = 0x380;
            rot.vz = 0;
            Gp_DrawFloorQuad(&task->extra.tmd->coords[1], 0x300, &rot);
            switch (work->playState) {
                case ACTOR_310100_PLAY_STATE_POSED:
                    break;
                case ACTOR_310100_PLAY_STATE_PLAYING:
                    func_actor_310100_80161F80(task);
                    break;
                case ACTOR_310100_PLAY_STATE_FROZEN:
                default:
                    return;
            }
            extra  = task->extra.tmd;
            vec.vx = extra->coords[1].workm.t[0];
            vec.vy = task->extra.tmd->coords[1].workm.t[1];
            vec.vz = task->extra.tmd->coords[1].workm.t[2];
            func_800D7A9C(extra, &vec, 0, 3);
            break;
    }
}

/// Relights an officer model for where it stands: rebuilds its colour matrix
/// from the room's lights as they fall on the origin of its part-1 frame.
///
/// The frame's world matrix is read as it stands, so it has to be current.
static inline void _actor310100LightOfficerModel(Task* task)
{
    VECTOR     position;
    TmdObject* model;

    model       = task->extra.tmd;
    position.vx = model->coords[1].workm.t[0];
    position.vy = task->extra.tmd->coords[1].workm.t[1];
    position.vz = task->extra.tmd->coords[1].workm.t[2];
    func_800D7A9C(model, &position, 0, 3);
}

/// Draws an officer model's ground shadow: a square in the XZ plane of its
/// part-1 frame, centred `ACTOR_310100_SHADOW_DROP` below that frame's origin.
static inline void _actor310100DrawOfficerShadow(Task* task)
{
    SVECTOR offset;

    offset.vx = 0;
    offset.vy = ACTOR_310100_SHADOW_DROP;
    offset.vz = 0;
    Gp_DrawFloorQuad(&task->extra.tmd->coords[1], ACTOR_310100_SHADOW_SIDE, &offset);
}

/// State handler for the display model spawned by `func_actor_310100_80162C64`:
/// the spawn tick seeds the tracker from the model's part-1 coordinate frame and
/// steps to state 1, and every later tick draws the floor quad until the display
/// state goes non-zero.
void func_actor_310100_801631B0(Task* task)
{
    _Actor310100PoliceOfficerWork* work;

    work = (_Actor310100PoliceOfficerWork*)task->work;
    switch (task->state) {
        case 0:
            func_actor_310100_801625E4(task, ACTOR_310100_PLACEMENT_OFFICER_1);
            actorRenderComposeCoord(&task->extra.tmd->coords[1]);
            _actor310100LightOfficerModel(task);
            task->state++;
            break;
        case 1:
            if (work->playState == ACTOR_310100_PLAY_STATE_POSED) {
                _actor310100DrawOfficerShadow(task);
            }
            break;
    }
}

/// The other display-model state handler (message 0x6D): the spawn tick seeds
/// the tracker from the model's part-1 coordinate frame and steps to state 1,
/// and every later tick draws the floor quad while the display state is still
/// below 2.
void func_actor_310100_801632B0(Task* task)
{
    _Actor310100PoliceOfficerWork* work;

    work = (_Actor310100PoliceOfficerWork*)task->work;
    switch (task->state) {
        case 0:
            func_actor_310100_801625E4(task, ACTOR_310100_PLACEMENT_OFFICER_2);
            actorRenderComposeCoord(&task->extra.tmd->coords[1]);
            _actor310100LightOfficerModel(task);
            task->state++;
            break;
        case 1:
            switch (work->playState) {
                case ACTOR_310100_PLAY_STATE_POSED:
                case ACTOR_310100_PLAY_STATE_PLAYING:
                    _actor310100DrawOfficerShadow(task);
                    break;
            }
            break;
    }
}
