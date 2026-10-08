#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/abs.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "rooms/mist_parking.h"
#include "../../shared/model_placement.h"
#include "../../shared/actor_messages.h"
#include "../../shared/actor_motion_walk_helpers.h"

/// Body drawing and primitive-buffer policies; every mode disables sphere pairs.
enum {
    ACTOR_113100_DRAW_HIDE                  = 0,
    ACTOR_113100_DRAW_SHOW                  = 1,
    ACTOR_113100_DRAW_HIDE_AND_RELEASE      = 2,
    ACTOR_113100_DRAW_SHOW_SKIP_AUTO_BUFFER = 3,
    ACTOR_113100_BUFFER_RELEASE_DELAY_TICKS = 2,
};

/// Pierce's walk clips and transitions; velocity uses signed 16.16 units per tick.
enum {
    ACTOR_113100_BODY_ANIMATION_BANK     = 0,
    ACTOR_113100_ANIM_WALK               = 2,
    ACTOR_113100_WALK_STEP_QUEUE_HEADING = 0,
    ACTOR_113100_TURN_BLEND_FRAMES       = 4,
    ACTOR_113100_WALK_BLEND_FRAMES       = 5,
    ACTOR_113100_YAW_STEP                = 64,
};

static void _modelPlacementMirrorParentDrawFlags(Task* childTask);

/// Work block of Pierce Carradine's body, the package's scripted walker.
///
/// The task's spawn state allocates it zeroed and keeps it at `Task::work`
/// for the task's life. It opens with the twenty-part rig and the model
/// state, and the model object borrows `model.light` and `model.color` for as
/// long as the block lives.
///
/// The actor has a collision sphere of its own, which the spawn state links
/// and the exit callback unlinks, so `body` and the table it borrows sit
/// between the model state and the walk a room script sends the actor on.
/// What follows the walk is the package's own: the model that faces the
/// camera, the heading the walk opens on, the head turn toward the player and
/// the delayed free of the model's buffers once the model has been hidden.
typedef struct {
    ActorAnimRig20        rig;            // Playback storage of the twenty-part body model; slots 1 to 19 are driven
    ActorModelState       model;          // Clip and bank the rig plays, and the matrices the model is lit with
    WorldCollisionBody    body;           // Sphere on part 1, linked for the task's life; the spawn state enables its pair pass and every visibility command disables it
    WorldCollisionContact contacts[1];    // One-entry table `body` borrows. The entry is marked LAST; an occupied contact is cleared each frame the model is drawn and never read
    ActorWalkState        walk;           // Destination, closing rotation, per-frame velocity and step of the walk in progress
    Task*                 billboardTask;  // Task of the camera-facing model carried on part 8, which an actor command shows and hides; started only in placement variant 2, NULL otherwise
    s16                   turnWeight;     // Weight of the per-frame head turn toward the player, 0 to 0x1000; stepped by 0x100 a frame while the model is drawn
    s16                   walkYaw;        // Heading from the root to `walk.target` as a walk starts, 4096 to a turn; the opening turn steers the root yaw to it
    u8                    turnUp;         // Direction `turnWeight` is ramped in (0 down to 0, 1 up to 0x1000); every play request sets it from the clip, and an actor command overrides it
    s8                    freeCountdown;  // Ticks left before the model's buffers are freed, which the tick finding 0 does (-1 no free pending)
    s8                    lastAppearFlag; // Game flag 0xED as the idle handler last read it; the actor appears on the tick the flag is positive and this is still 0
    byte                  pad_53F[1];
} _Actor113100PierceCarradineWork;
STATIC_ASSERT_SIZEOF(_Actor113100PierceCarradineWork, 0x540);

/// Child task table the setup handler `func_actor_113100_80131E58` spawns
/// from, four `TaskDesc` entries. Index 1 is spawned only when
/// `gGameSession->location.loc.variant == 2` and its task lands in
/// `_Actor113100PierceCarradineWork::billboardTask`; indices 2 and 3 are the two
/// modelled parts the handler re-dresses from the area record.
extern TaskDesc D_actor_113100_80144308[];

/// The actor's message table, stored in `Task::msgTable`: 0x7D3
/// (`_actor113100PlayAnimation`), 0x7D4 (`actorMsgPlaceEuler`), 0x7D5
/// (`_actor113100SetModelDraw`), 0x7DD (`_actor113100StartWalk`) and
/// 0x7DB (`_actor113100ApplyCommand`), terminated by `TASK_MESSAGE_TABLE_END`.
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.

extern TaskMessageEntry D_actor_113100_80144338[];

/// Animation bank table the 0x7D3 handler `_actor113100PlayAnimation` indexes
/// by the animation id it has latched into `_Actor113100PierceCarradineWork::model.bank`; the
/// entry is the `AnimationSet**` passed to `animationInitContext`.
extern AnimationSet*  D_actor_113100_80144250[36];
extern AnimationSet** D_actor_113100_801442E0[1];

/// Per-animation byte the same handler copies into
/// `_Actor113100PierceCarradineWork::turnUp` from
/// `AnimationPlayRequest::animationId`.
extern u8 D_actor_113100_801442E4[];

static void func_actor_113100_80131E58(Task* task);
static void func_actor_113100_80132104(Task* task);
static void _actor113100TurnAndBeginWalk(Task* task);
static void _actor113100CheckWalkArrival(Task* task);
static s32  _actor113100SetModelDraw(Task* task, s32 messageId, s32 drawMode, s32 unusedArg);
static void _actor113100AttachBillboard(Task* task);
static void _actor113100FaceBillboardToCamera(Task* task);
static void _actor113100Exit(Task* task);
static void _actor113100BindLighting(Task* task);
static void _actor113100CheckPierceAppearance(Task* task);
static void _actor113100StepWalk(Task* task);
static void _actor113100QueueWalkHeading(Task* task);
static void _actor113100TurnToArrivalYaw(Task* task);
static s32  _actor113100PlayAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg);
static s32  _actor113100StartWalk(Task* task, s32 messageId, const ActorTransform* placement, const ActorMotionWalkAnim* walkAnim);
static s32  _actor113100ApplyCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg);
static void _actor113100BillboardTask(Task* task);
static void _actor113100AttachedModelTask(Task* task);

/// States of a child task posed on one of the parent's parts: attach to the
/// parent, rebuild its display matrix every frame, then `taskKill`. Dispatched
/// by `_actor113100BillboardTask`.
static const TaskFuncTable3 D_actor_113100_80131E24 = { {
    _actor113100AttachBillboard,
    _actor113100FaceBillboardToCamera,
    taskKill,
} };

/// States of the child task: attach to the parent's part, mirror its active-draw
/// and buffer flags each frame, then `taskKill`.
/// Dispatched by `_actor113100AttachedModelTask`.
static const TaskFuncTable3 D_actor_113100_80131E30 = { {
    _modelPlacementAttachChild,
    _modelPlacementMirrorParentDrawFlags,
    taskKill,
} };

/// The actor's own three states - setup, per-frame tick and exit -
/// dispatched by `_actor113100PierceCarradineTask`.
static const TaskFuncTable3 D_actor_113100_80131E3C = { {
    func_actor_113100_80131E58,
    func_actor_113100_80132104,
    _actor113100Exit,
} };

/// The four main-body handlers, dispatched by `_actor113100StepWalk`
/// through `_Actor113100PierceCarradineWork::walk.motionStep`.
static const TaskFuncTable4 D_actor_113100_80131E48 = { {
    _actor113100QueueWalkHeading,
    _actor113100TurnAndBeginWalk,
    _actor113100CheckWalkArrival,
    _actor113100TurnToArrivalYaw,
} };

static TmdSource _gActor113100PierceCarradineBody;
static void      _actor113100PierceCarradineTask(Task* task);

static TmdBone _gActor113100PierceCarradineBodySkeleton[20] = {
#include "assets/pierce_carradine_body_skeleton.inc"
};

static u32 _gActor113100PierceCarradineBodyPartVerts[20] = {
#include "assets/pierce_carradine_body_partVerts.inc"
};

static SVECTOR _gActor113100PierceCarradineBodyVerts[390] = {
#include "assets/pierce_carradine_body_verts.inc"
};

static SVECTOR _gActor113100PierceCarradineBodyNormals[407] = {
#include "assets/pierce_carradine_body_normals.inc"
};

static u32 _gActor113100PierceCarradineBodyStream[4476] = {
#include "assets/pierce_carradine_body_stream.inc"
};

static TmdSource _gActor113100PierceCarradineBody = {
    0,
    24444,
    6776,
    20,
    _gActor113100PierceCarradineBodyPartVerts,
    _gActor113100PierceCarradineBodyVerts,
    _gActor113100PierceCarradineBodyNormals,
    _gActor113100PierceCarradineBodySkeleton,
    _gActor113100PierceCarradineBodyStream,
};

static TmdBone _gActor113100Model07960Skeleton[1] = {
#include "assets/actor_113100_model_07960_skeleton.inc"
};

static u32 _gActor113100Model07960PartVerts[1] = {
#include "assets/actor_113100_model_07960_partVerts.inc"
};

static SVECTOR _gActor113100Model07960Verts[14] = {
#include "assets/actor_113100_model_07960_verts.inc"
};

static SVECTOR _gActor113100Model07960Normals[12] = {
#include "assets/actor_113100_model_07960_normals.inc"
};

static u32 _gActor113100Model07960Stream[56] = {
#include "assets/actor_113100_model_07960_stream.inc"
};

static TmdSource _gActor113100Model07960 = {
    0,
    340,
    0,
    1,
    _gActor113100Model07960PartVerts,
    _gActor113100Model07960Verts,
    _gActor113100Model07960Normals,
    _gActor113100Model07960Skeleton,
    _gActor113100Model07960Stream,
};

static TmdBone _gActor113100Model07AB4Skeleton[1] = {
#include "assets/actor_113100_model_07AB4_skeleton.inc"
};

static u32 _gActor113100Model07AB4PartVerts[1] = {
#include "assets/actor_113100_model_07AB4_partVerts.inc"
};

static SVECTOR _gActor113100Model07AB4Verts[4] = {
#include "assets/actor_113100_model_07AB4_verts.inc"
};

static SVECTOR _gActor113100Model07AB4Normals[1] = {
#include "assets/actor_113100_model_07AB4_normals.inc"
};

static u32 _gActor113100Model07AB4Stream[13] = {
#include "assets/actor_113100_model_07AB4_stream.inc"
};

static TmdSource _gActor113100Model07AB4 = {
    0,
    52,
    0,
    1,
    _gActor113100Model07AB4PartVerts,
    _gActor113100Model07AB4Verts,
    _gActor113100Model07AB4Normals,
    _gActor113100Model07AB4Skeleton,
    _gActor113100Model07AB4Stream,
};

static TmdBone _gActor113100Model07BC4Skeleton[1] = {
#include "assets/actor_113100_model_07BC4_skeleton.inc"
};

static u32 _gActor113100Model07BC4PartVerts[1] = {
#include "assets/actor_113100_model_07BC4_partVerts.inc"
};

static SVECTOR _gActor113100Model07BC4Verts[18] = {
#include "assets/actor_113100_model_07BC4_verts.inc"
};

static u32 _gActor113100Model07BC4Stream[109] = {
#include "assets/actor_113100_model_07BC4_stream.inc"
};

static TmdSource _gActor113100Model07BC4 = {
    0,
    672,
    0,
    1,
    _gActor113100Model07BC4PartVerts,
    _gActor113100Model07BC4Verts,
    &_gActor113100Model07BC4Verts[18],
    _gActor113100Model07BC4Skeleton,
    _gActor113100Model07BC4Stream,
};

static AnimationPackedPose _gActor113100Animation07FF0Bank1[2] = {
#include "assets/actor_113100_animation_07FF0_bank1.inc"
};

static AnimationPackedRotation _gActor113100Animation07FF0Bank4[32] = {
#include "assets/actor_113100_animation_07FF0_bank4.inc"
};

static AnimationRecord _gActor113100Animation07FF0Records[101] = {
#include "assets/actor_113100_animation_07FF0_records.inc"
};

static u16 _gActor113100Animation07FF0Indices[20] = {
#include "assets/actor_113100_animation_07FF0_indices.inc"
};

static AnimationSet _gActor113100Animation07FF0 = {
    _gActor113100Animation07FF0Records,
    _gActor113100Animation07FF0Indices,
    { NULL, _gActor113100Animation07FF0Bank1, NULL, NULL, _gActor113100Animation07FF0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor113100Animation08784Bank1[21] = {
#include "assets/actor_113100_animation_08784_bank1.inc"
};

static AnimationPackedRotation _gActor113100Animation08784Bank4[156] = {
#include "assets/actor_113100_animation_08784_bank4.inc"
};

static AnimationRecord _gActor113100Animation08784Records[246] = {
#include "assets/actor_113100_animation_08784_records.inc"
};

static u16 _gActor113100Animation08784Indices[20] = {
#include "assets/actor_113100_animation_08784_indices.inc"
};

static AnimationSet _gActor113100Animation08784 = {
    _gActor113100Animation08784Records,
    _gActor113100Animation08784Indices,
    { NULL, _gActor113100Animation08784Bank1, NULL, NULL, _gActor113100Animation08784Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor113100Animation08CF8Bank1[2] = {
#include "assets/actor_113100_animation_08CF8_bank1.inc"
};

static AnimationPackedRotation _gActor113100Animation08CF8Bank4[135] = {
#include "assets/actor_113100_animation_08CF8_bank4.inc"
};

static AnimationRecord _gActor113100Animation08CF8Records[188] = {
#include "assets/actor_113100_animation_08CF8_records.inc"
};

static u16 _gActor113100Animation08CF8Indices[20] = {
#include "assets/actor_113100_animation_08CF8_indices.inc"
};

static AnimationSet _gActor113100Animation08CF8 = {
    _gActor113100Animation08CF8Records,
    _gActor113100Animation08CF8Indices,
    { NULL, _gActor113100Animation08CF8Bank1, NULL, NULL, _gActor113100Animation08CF8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor113100Animation09734Bank1[23] = {
#include "assets/actor_113100_animation_09734_bank1.inc"
};

static AnimationPackedRotation _gActor113100Animation09734Bank4[243] = {
#include "assets/actor_113100_animation_09734_bank4.inc"
};

static AnimationRecord _gActor113100Animation09734Records[323] = {
#include "assets/actor_113100_animation_09734_records.inc"
};

static u16 _gActor113100Animation09734Indices[20] = {
#include "assets/actor_113100_animation_09734_indices.inc"
};

static AnimationSet _gActor113100Animation09734 = {
    _gActor113100Animation09734Records,
    _gActor113100Animation09734Indices,
    { NULL, _gActor113100Animation09734Bank1, NULL, NULL, _gActor113100Animation09734Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor113100Animation098F0Bank1[2] = {
#include "assets/actor_113100_animation_098F0_bank1.inc"
};

static AnimationPackedRotation _gActor113100Animation098F0Bank4[25] = {
#include "assets/actor_113100_animation_098F0_bank4.inc"
};

static AnimationRecord _gActor113100Animation098F0Records[60] = {
#include "assets/actor_113100_animation_098F0_records.inc"
};

static u16 _gActor113100Animation098F0Indices[20] = {
#include "assets/actor_113100_animation_098F0_indices.inc"
};

static AnimationSet _gActor113100Animation098F0 = {
    _gActor113100Animation098F0Records,
    _gActor113100Animation098F0Indices,
    { NULL, _gActor113100Animation098F0Bank1, NULL, NULL, _gActor113100Animation098F0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor113100Animation09D40Bank1[3] = {
#include "assets/actor_113100_animation_09D40_bank1.inc"
};

static AnimationPackedRotation _gActor113100Animation09D40Bank4[92] = {
#include "assets/actor_113100_animation_09D40_bank4.inc"
};

static AnimationRecord _gActor113100Animation09D40Records[155] = {
#include "assets/actor_113100_animation_09D40_records.inc"
};

static u16 _gActor113100Animation09D40Indices[20] = {
#include "assets/actor_113100_animation_09D40_indices.inc"
};

static AnimationSet _gActor113100Animation09D40 = {
    _gActor113100Animation09D40Records,
    _gActor113100Animation09D40Indices,
    { NULL, _gActor113100Animation09D40Bank1, NULL, NULL, _gActor113100Animation09D40Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor113100Animation09EFCBank1[2] = {
#include "assets/actor_113100_animation_09EFC_bank1.inc"
};

static AnimationPackedRotation _gActor113100Animation09EFCBank4[25] = {
#include "assets/actor_113100_animation_09EFC_bank4.inc"
};

static AnimationRecord _gActor113100Animation09EFCRecords[60] = {
#include "assets/actor_113100_animation_09EFC_records.inc"
};

static u16 _gActor113100Animation09EFCIndices[20] = {
#include "assets/actor_113100_animation_09EFC_indices.inc"
};

static AnimationSet _gActor113100Animation09EFC = {
    _gActor113100Animation09EFCRecords,
    _gActor113100Animation09EFCIndices,
    { NULL, _gActor113100Animation09EFCBank1, NULL, NULL, _gActor113100Animation09EFCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor113100Animation0A1D0Bank1[2] = {
#include "assets/actor_113100_animation_0A1D0_bank1.inc"
};

static AnimationPackedRotation _gActor113100Animation0A1D0Bank4[47] = {
#include "assets/actor_113100_animation_0A1D0_bank4.inc"
};

static AnimationRecord _gActor113100Animation0A1D0Records[108] = {
#include "assets/actor_113100_animation_0A1D0_records.inc"
};

static u16 _gActor113100Animation0A1D0Indices[20] = {
#include "assets/actor_113100_animation_0A1D0_indices.inc"
};

static AnimationSet _gActor113100Animation0A1D0 = {
    _gActor113100Animation0A1D0Records,
    _gActor113100Animation0A1D0Indices,
    { NULL, _gActor113100Animation0A1D0Bank1, NULL, NULL, _gActor113100Animation0A1D0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor113100Animation0AA3CBank1[29] = {
#include "assets/actor_113100_animation_0AA3C_bank1.inc"
};

static AnimationPackedRotation _gActor113100Animation0AA3CBank4[175] = {
#include "assets/actor_113100_animation_0AA3C_bank4.inc"
};

static AnimationRecord _gActor113100Animation0AA3CRecords[257] = {
#include "assets/actor_113100_animation_0AA3C_records.inc"
};

static u16 _gActor113100Animation0AA3CIndices[20] = {
#include "assets/actor_113100_animation_0AA3C_indices.inc"
};

static AnimationSet _gActor113100Animation0AA3C = {
    _gActor113100Animation0AA3CRecords,
    _gActor113100Animation0AA3CIndices,
    { NULL, _gActor113100Animation0AA3CBank1, NULL, NULL, _gActor113100Animation0AA3CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor113100Animation0B3C4Bank1[18] = {
#include "assets/actor_113100_animation_0B3C4_bank1.inc"
};

static AnimationPackedRotation _gActor113100Animation0B3C4Bank4[227] = {
#include "assets/actor_113100_animation_0B3C4_bank4.inc"
};

static AnimationRecord _gActor113100Animation0B3C4Records[309] = {
#include "assets/actor_113100_animation_0B3C4_records.inc"
};

static u16 _gActor113100Animation0B3C4Indices[20] = {
#include "assets/actor_113100_animation_0B3C4_indices.inc"
};

static AnimationSet _gActor113100Animation0B3C4 = {
    _gActor113100Animation0B3C4Records,
    _gActor113100Animation0B3C4Indices,
    { NULL, _gActor113100Animation0B3C4Bank1, NULL, NULL, _gActor113100Animation0B3C4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor113100Animation0B624Bank1[4] = {
#include "assets/actor_113100_animation_0B624_bank1.inc"
};

static AnimationPackedRotation _gActor113100Animation0B624Bank4[36] = {
#include "assets/actor_113100_animation_0B624_bank4.inc"
};

static AnimationRecord _gActor113100Animation0B624Records[84] = {
#include "assets/actor_113100_animation_0B624_records.inc"
};

static u16 _gActor113100Animation0B624Indices[20] = {
#include "assets/actor_113100_animation_0B624_indices.inc"
};

static AnimationSet _gActor113100Animation0B624 = {
    _gActor113100Animation0B624Records,
    _gActor113100Animation0B624Indices,
    { NULL, _gActor113100Animation0B624Bank1, NULL, NULL, _gActor113100Animation0B624Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor113100Animation0BFD0Bank1[9] = {
#include "assets/actor_113100_animation_0BFD0_bank1.inc"
};

static AnimationPackedRotation _gActor113100Animation0BFD0Bank4[245] = {
#include "assets/actor_113100_animation_0BFD0_bank4.inc"
};

static AnimationRecord _gActor113100Animation0BFD0Records[327] = {
#include "assets/actor_113100_animation_0BFD0_records.inc"
};

static u16 _gActor113100Animation0BFD0Indices[20] = {
#include "assets/actor_113100_animation_0BFD0_indices.inc"
};

static AnimationSet _gActor113100Animation0BFD0 = {
    _gActor113100Animation0BFD0Records,
    _gActor113100Animation0BFD0Indices,
    { NULL, _gActor113100Animation0BFD0Bank1, NULL, NULL, _gActor113100Animation0BFD0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor113100Animation0CBDCBank1[29] = {
#include "assets/actor_113100_animation_0CBDC_bank1.inc"
};

static AnimationPackedRotation _gActor113100Animation0CBDCBank4[281] = {
#include "assets/actor_113100_animation_0CBDC_bank4.inc"
};

static AnimationRecord _gActor113100Animation0CBDCRecords[383] = {
#include "assets/actor_113100_animation_0CBDC_records.inc"
};

static u16 _gActor113100Animation0CBDCIndices[20] = {
#include "assets/actor_113100_animation_0CBDC_indices.inc"
};

static AnimationSet _gActor113100Animation0CBDC = {
    _gActor113100Animation0CBDCRecords,
    _gActor113100Animation0CBDCIndices,
    { NULL, _gActor113100Animation0CBDCBank1, NULL, NULL, _gActor113100Animation0CBDCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor113100Animation0D5A0Bank1[28] = {
#include "assets/actor_113100_animation_0D5A0_bank1.inc"
};

static AnimationPackedRotation _gActor113100Animation0D5A0Bank4[214] = {
#include "assets/actor_113100_animation_0D5A0_bank4.inc"
};

static AnimationRecord _gActor113100Animation0D5A0Records[307] = {
#include "assets/actor_113100_animation_0D5A0_records.inc"
};

static u16 _gActor113100Animation0D5A0Indices[20] = {
#include "assets/actor_113100_animation_0D5A0_indices.inc"
};

static AnimationSet _gActor113100Animation0D5A0 = {
    _gActor113100Animation0D5A0Records,
    _gActor113100Animation0D5A0Indices,
    { NULL, _gActor113100Animation0D5A0Bank1, NULL, NULL, _gActor113100Animation0D5A0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor113100Animation0D834Bank1[5] = {
#include "assets/actor_113100_animation_0D834_bank1.inc"
};

static AnimationPackedRotation _gActor113100Animation0D834Bank4[36] = {
#include "assets/actor_113100_animation_0D834_bank4.inc"
};

static AnimationRecord _gActor113100Animation0D834Records[94] = {
#include "assets/actor_113100_animation_0D834_records.inc"
};

static u16 _gActor113100Animation0D834Indices[20] = {
#include "assets/actor_113100_animation_0D834_indices.inc"
};

static AnimationSet _gActor113100Animation0D834 = {
    _gActor113100Animation0D834Records,
    _gActor113100Animation0D834Indices,
    { NULL, _gActor113100Animation0D834Bank1, NULL, NULL, _gActor113100Animation0D834Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor113100Animation0DE94Bank1[11] = {
#include "assets/actor_113100_animation_0DE94_bank1.inc"
};

static AnimationPackedRotation _gActor113100Animation0DE94Bank4[144] = {
#include "assets/actor_113100_animation_0DE94_bank4.inc"
};

static AnimationRecord _gActor113100Animation0DE94Records[211] = {
#include "assets/actor_113100_animation_0DE94_records.inc"
};

static u16 _gActor113100Animation0DE94Indices[20] = {
#include "assets/actor_113100_animation_0DE94_indices.inc"
};

static AnimationSet _gActor113100Animation0DE94 = {
    _gActor113100Animation0DE94Records,
    _gActor113100Animation0DE94Indices,
    { NULL, _gActor113100Animation0DE94Bank1, NULL, NULL, _gActor113100Animation0DE94Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor113100Animation0E25CBank1[6] = {
#include "assets/actor_113100_animation_0E25C_bank1.inc"
};

static AnimationPackedRotation _gActor113100Animation0E25CBank4[62] = {
#include "assets/actor_113100_animation_0E25C_bank4.inc"
};

static AnimationRecord _gActor113100Animation0E25CRecords[142] = {
#include "assets/actor_113100_animation_0E25C_records.inc"
};

static u16 _gActor113100Animation0E25CIndices[20] = {
#include "assets/actor_113100_animation_0E25C_indices.inc"
};

static AnimationSet _gActor113100Animation0E25C = {
    _gActor113100Animation0E25CRecords,
    _gActor113100Animation0E25CIndices,
    { NULL, _gActor113100Animation0E25CBank1, NULL, NULL, _gActor113100Animation0E25CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor113100Animation0E4A4Bank1[3] = {
#include "assets/actor_113100_animation_0E4A4_bank1.inc"
};

static AnimationPackedRotation _gActor113100Animation0E4A4Bank4[42] = {
#include "assets/actor_113100_animation_0E4A4_bank4.inc"
};

static AnimationRecord _gActor113100Animation0E4A4Records[75] = {
#include "assets/actor_113100_animation_0E4A4_records.inc"
};

static u16 _gActor113100Animation0E4A4Indices[20] = {
#include "assets/actor_113100_animation_0E4A4_indices.inc"
};

static AnimationSet _gActor113100Animation0E4A4 = {
    _gActor113100Animation0E4A4Records,
    _gActor113100Animation0E4A4Indices,
    { NULL, _gActor113100Animation0E4A4Bank1, NULL, NULL, _gActor113100Animation0E4A4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor113100Animation0E830Bank1[5] = {
#include "assets/actor_113100_animation_0E830_bank1.inc"
};

static AnimationPackedRotation _gActor113100Animation0E830Bank4[76] = {
#include "assets/actor_113100_animation_0E830_bank4.inc"
};

static AnimationRecord _gActor113100Animation0E830Records[116] = {
#include "assets/actor_113100_animation_0E830_records.inc"
};

static u16 _gActor113100Animation0E830Indices[20] = {
#include "assets/actor_113100_animation_0E830_indices.inc"
};

static AnimationSet _gActor113100Animation0E830 = {
    _gActor113100Animation0E830Records,
    _gActor113100Animation0E830Indices,
    { NULL, _gActor113100Animation0E830Bank1, NULL, NULL, _gActor113100Animation0E830Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor113100Animation0EAC8Bank1[5] = {
#include "assets/actor_113100_animation_0EAC8_bank1.inc"
};

static AnimationPackedRotation _gActor113100Animation0EAC8Bank4[29] = {
#include "assets/actor_113100_animation_0EAC8_bank4.inc"
};

static AnimationRecord _gActor113100Animation0EAC8Records[102] = {
#include "assets/actor_113100_animation_0EAC8_records.inc"
};

static u16 _gActor113100Animation0EAC8Indices[20] = {
#include "assets/actor_113100_animation_0EAC8_indices.inc"
};

static AnimationSet _gActor113100Animation0EAC8 = {
    _gActor113100Animation0EAC8Records,
    _gActor113100Animation0EAC8Indices,
    { NULL, _gActor113100Animation0EAC8Bank1, NULL, NULL, _gActor113100Animation0EAC8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor113100Animation0EDE8Bank1[4] = {
#include "assets/actor_113100_animation_0EDE8_bank1.inc"
};

static AnimationPackedRotation _gActor113100Animation0EDE8Bank4[67] = {
#include "assets/actor_113100_animation_0EDE8_bank4.inc"
};

static AnimationRecord _gActor113100Animation0EDE8Records[101] = {
#include "assets/actor_113100_animation_0EDE8_records.inc"
};

static u16 _gActor113100Animation0EDE8Indices[20] = {
#include "assets/actor_113100_animation_0EDE8_indices.inc"
};

static AnimationSet _gActor113100Animation0EDE8 = {
    _gActor113100Animation0EDE8Records,
    _gActor113100Animation0EDE8Indices,
    { NULL, _gActor113100Animation0EDE8Bank1, NULL, NULL, _gActor113100Animation0EDE8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor113100Animation0F130Bank1[6] = {
#include "assets/actor_113100_animation_0F130_bank1.inc"
};

static AnimationPackedRotation _gActor113100Animation0F130Bank4[69] = {
#include "assets/actor_113100_animation_0F130_bank4.inc"
};

static AnimationRecord _gActor113100Animation0F130Records[103] = {
#include "assets/actor_113100_animation_0F130_records.inc"
};

static u16 _gActor113100Animation0F130Indices[20] = {
#include "assets/actor_113100_animation_0F130_indices.inc"
};

static AnimationSet _gActor113100Animation0F130 = {
    _gActor113100Animation0F130Records,
    _gActor113100Animation0F130Indices,
    { NULL, _gActor113100Animation0F130Bank1, NULL, NULL, _gActor113100Animation0F130Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor113100Animation0F36CBank1[3] = {
#include "assets/actor_113100_animation_0F36C_bank1.inc"
};

static AnimationPackedRotation _gActor113100Animation0F36CBank4[24] = {
#include "assets/actor_113100_animation_0F36C_bank4.inc"
};

static AnimationRecord _gActor113100Animation0F36CRecords[90] = {
#include "assets/actor_113100_animation_0F36C_records.inc"
};

static u16 _gActor113100Animation0F36CIndices[20] = {
#include "assets/actor_113100_animation_0F36C_indices.inc"
};

static AnimationSet _gActor113100Animation0F36C = {
    _gActor113100Animation0F36CRecords,
    _gActor113100Animation0F36CIndices,
    { NULL, _gActor113100Animation0F36CBank1, NULL, NULL, _gActor113100Animation0F36CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor113100Animation0F67CBank1[6] = {
#include "assets/actor_113100_animation_0F67C_bank1.inc"
};

static AnimationPackedRotation _gActor113100Animation0F67CBank4[61] = {
#include "assets/actor_113100_animation_0F67C_bank4.inc"
};

static AnimationRecord _gActor113100Animation0F67CRecords[97] = {
#include "assets/actor_113100_animation_0F67C_records.inc"
};

static u16 _gActor113100Animation0F67CIndices[20] = {
#include "assets/actor_113100_animation_0F67C_indices.inc"
};

static AnimationSet _gActor113100Animation0F67C = {
    _gActor113100Animation0F67CRecords,
    _gActor113100Animation0F67CIndices,
    { NULL, _gActor113100Animation0F67CBank1, NULL, NULL, _gActor113100Animation0F67CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor113100Animation0FA08Bank1[3] = {
#include "assets/actor_113100_animation_0FA08_bank1.inc"
};

static AnimationPackedRotation _gActor113100Animation0FA08Bank4[73] = {
#include "assets/actor_113100_animation_0FA08_bank4.inc"
};

static AnimationRecord _gActor113100Animation0FA08Records[125] = {
#include "assets/actor_113100_animation_0FA08_records.inc"
};

static u16 _gActor113100Animation0FA08Indices[20] = {
#include "assets/actor_113100_animation_0FA08_indices.inc"
};

static AnimationSet _gActor113100Animation0FA08 = {
    _gActor113100Animation0FA08Records,
    _gActor113100Animation0FA08Indices,
    { NULL, _gActor113100Animation0FA08Bank1, NULL, NULL, _gActor113100Animation0FA08Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor113100Animation0FDC4Bank1[5] = {
#include "assets/actor_113100_animation_0FDC4_bank1.inc"
};

static AnimationPackedRotation _gActor113100Animation0FDC4Bank4[77] = {
#include "assets/actor_113100_animation_0FDC4_bank4.inc"
};

static AnimationRecord _gActor113100Animation0FDC4Records[127] = {
#include "assets/actor_113100_animation_0FDC4_records.inc"
};

static u16 _gActor113100Animation0FDC4Indices[20] = {
#include "assets/actor_113100_animation_0FDC4_indices.inc"
};

static AnimationSet _gActor113100Animation0FDC4 = {
    _gActor113100Animation0FDC4Records,
    _gActor113100Animation0FDC4Indices,
    { NULL, _gActor113100Animation0FDC4Bank1, NULL, NULL, _gActor113100Animation0FDC4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor113100Animation100A8Bank1[3] = {
#include "assets/actor_113100_animation_100A8_bank1.inc"
};

static AnimationPackedRotation _gActor113100Animation100A8Bank4[42] = {
#include "assets/actor_113100_animation_100A8_bank4.inc"
};

static AnimationRecord _gActor113100Animation100A8Records[114] = {
#include "assets/actor_113100_animation_100A8_records.inc"
};

static u16 _gActor113100Animation100A8Indices[20] = {
#include "assets/actor_113100_animation_100A8_indices.inc"
};

static AnimationSet _gActor113100Animation100A8 = {
    _gActor113100Animation100A8Records,
    _gActor113100Animation100A8Indices,
    { NULL, _gActor113100Animation100A8Bank1, NULL, NULL, _gActor113100Animation100A8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor113100Animation106ECBank1[15] = {
#include "assets/actor_113100_animation_106EC_bank1.inc"
};

static AnimationPackedRotation _gActor113100Animation106ECBank4[123] = {
#include "assets/actor_113100_animation_106EC_bank4.inc"
};

static AnimationRecord _gActor113100Animation106ECRecords[213] = {
#include "assets/actor_113100_animation_106EC_records.inc"
};

static u16 _gActor113100Animation106ECIndices[20] = {
#include "assets/actor_113100_animation_106EC_indices.inc"
};

static AnimationSet _gActor113100Animation106EC = {
    _gActor113100Animation106ECRecords,
    _gActor113100Animation106ECIndices,
    { NULL, _gActor113100Animation106ECBank1, NULL, NULL, _gActor113100Animation106ECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor113100Animation10C44Bank1[7] = {
#include "assets/actor_113100_animation_10C44_bank1.inc"
};

static AnimationPackedRotation _gActor113100Animation10C44Bank4[104] = {
#include "assets/actor_113100_animation_10C44_bank4.inc"
};

static AnimationRecord _gActor113100Animation10C44Records[197] = {
#include "assets/actor_113100_animation_10C44_records.inc"
};

static u16 _gActor113100Animation10C44Indices[20] = {
#include "assets/actor_113100_animation_10C44_indices.inc"
};

static AnimationSet _gActor113100Animation10C44 = {
    _gActor113100Animation10C44Records,
    _gActor113100Animation10C44Indices,
    { NULL, _gActor113100Animation10C44Bank1, NULL, NULL, _gActor113100Animation10C44Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor113100Animation10FD8Bank1[6] = {
#include "assets/actor_113100_animation_10FD8_bank1.inc"
};

static AnimationPackedRotation _gActor113100Animation10FD8Bank4[79] = {
#include "assets/actor_113100_animation_10FD8_bank4.inc"
};

static AnimationRecord _gActor113100Animation10FD8Records[112] = {
#include "assets/actor_113100_animation_10FD8_records.inc"
};

static u16 _gActor113100Animation10FD8Indices[20] = {
#include "assets/actor_113100_animation_10FD8_indices.inc"
};

static AnimationSet _gActor113100Animation10FD8 = {
    _gActor113100Animation10FD8Records,
    _gActor113100Animation10FD8Indices,
    { NULL, _gActor113100Animation10FD8Bank1, NULL, NULL, _gActor113100Animation10FD8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor113100Animation113C4Bank1[4] = {
#include "assets/actor_113100_animation_113C4_bank1.inc"
};

static AnimationPackedRotation _gActor113100Animation113C4Bank4[67] = {
#include "assets/actor_113100_animation_113C4_bank4.inc"
};

static AnimationRecord _gActor113100Animation113C4Records[152] = {
#include "assets/actor_113100_animation_113C4_records.inc"
};

static u16 _gActor113100Animation113C4Indices[20] = {
#include "assets/actor_113100_animation_113C4_indices.inc"
};

static AnimationSet _gActor113100Animation113C4 = {
    _gActor113100Animation113C4Records,
    _gActor113100Animation113C4Indices,
    { NULL, _gActor113100Animation113C4Bank1, NULL, NULL, _gActor113100Animation113C4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor113100Animation1164CBank1[4] = {
#include "assets/actor_113100_animation_1164C_bank1.inc"
};

static AnimationPackedRotation _gActor113100Animation1164CBank4[52] = {
#include "assets/actor_113100_animation_1164C_bank4.inc"
};

static AnimationRecord _gActor113100Animation1164CRecords[78] = {
#include "assets/actor_113100_animation_1164C_records.inc"
};

static u16 _gActor113100Animation1164CIndices[20] = {
#include "assets/actor_113100_animation_1164C_indices.inc"
};

static AnimationSet _gActor113100Animation1164C = {
    _gActor113100Animation1164CRecords,
    _gActor113100Animation1164CIndices,
    { NULL, _gActor113100Animation1164CBank1, NULL, NULL, _gActor113100Animation1164CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor113100Animation11A0CBank1[6] = {
#include "assets/actor_113100_animation_11A0C_bank1.inc"
};

static AnimationPackedRotation _gActor113100Animation11A0CBank4[84] = {
#include "assets/actor_113100_animation_11A0C_bank4.inc"
};

static AnimationRecord _gActor113100Animation11A0CRecords[118] = {
#include "assets/actor_113100_animation_11A0C_records.inc"
};

static u16 _gActor113100Animation11A0CIndices[20] = {
#include "assets/actor_113100_animation_11A0C_indices.inc"
};

static AnimationSet _gActor113100Animation11A0C = {
    _gActor113100Animation11A0CRecords,
    _gActor113100Animation11A0CIndices,
    { NULL, _gActor113100Animation11A0CBank1, NULL, NULL, _gActor113100Animation11A0CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor113100Animation11DD0Bank1[7] = {
#include "assets/actor_113100_animation_11DD0_bank1.inc"
};

static AnimationPackedRotation _gActor113100Animation11DD0Bank4[82] = {
#include "assets/actor_113100_animation_11DD0_bank4.inc"
};

static AnimationRecord _gActor113100Animation11DD0Records[118] = {
#include "assets/actor_113100_animation_11DD0_records.inc"
};

static u16 _gActor113100Animation11DD0Indices[20] = {
#include "assets/actor_113100_animation_11DD0_indices.inc"
};

static AnimationSet _gActor113100Animation11DD0 = {
    _gActor113100Animation11DD0Records,
    _gActor113100Animation11DD0Indices,
    { NULL, _gActor113100Animation11DD0Bank1, NULL, NULL, _gActor113100Animation11DD0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor113100Animation12408Bank1[9] = {
#include "assets/actor_113100_animation_12408_bank1.inc"
};

static AnimationPackedRotation _gActor113100Animation12408Bank4[132] = {
#include "assets/actor_113100_animation_12408_bank4.inc"
};

static AnimationRecord _gActor113100Animation12408Records[219] = {
#include "assets/actor_113100_animation_12408_records.inc"
};

static u16 _gActor113100Animation12408Indices[20] = {
#include "assets/actor_113100_animation_12408_indices.inc"
};

static AnimationSet _gActor113100Animation12408 = {
    _gActor113100Animation12408Records,
    _gActor113100Animation12408Indices,
    { NULL, _gActor113100Animation12408Bank1, NULL, NULL, _gActor113100Animation12408Bank4, NULL, NULL, NULL },
};

AnimationSet* D_actor_113100_80144250[36] = {
    NULL,
    &_gActor113100Animation07FF0,
    &_gActor113100Animation08784,
    &_gActor113100Animation09734,
    &_gActor113100Animation08CF8,
    &_gActor113100Animation098F0,
    &_gActor113100Animation09D40,
    &_gActor113100Animation09EFC,
    &_gActor113100Animation0A1D0,
    &_gActor113100Animation0AA3C,
    &_gActor113100Animation0B3C4,
    &_gActor113100Animation0B624,
    &_gActor113100Animation0BFD0,
    &_gActor113100Animation0CBDC,
    &_gActor113100Animation0D5A0,
    &_gActor113100Animation0D834,
    &_gActor113100Animation0DE94,
    &_gActor113100Animation0E25C,
    &_gActor113100Animation0E4A4,
    &_gActor113100Animation0F130,
    &_gActor113100Animation0F36C,
    &_gActor113100Animation0F67C,
    &_gActor113100Animation0FA08,
    &_gActor113100Animation0E830,
    &_gActor113100Animation0EAC8,
    &_gActor113100Animation0EDE8,
    &_gActor113100Animation0FDC4,
    &_gActor113100Animation100A8,
    &_gActor113100Animation106EC,
    &_gActor113100Animation10C44,
    &_gActor113100Animation10FD8,
    &_gActor113100Animation113C4,
    &_gActor113100Animation1164C,
    &_gActor113100Animation11A0C,
    &_gActor113100Animation11DD0,
    &_gActor113100Animation12408,
};

AnimationSet** D_actor_113100_801442E0[1] = {
    D_actor_113100_80144250,
};

u8 D_actor_113100_801442E4[36] = {
    0,
    1,
    0,
    0,
    1,
    1,
    1,
    1,
    1,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    1,
    0,
    1,
    1,
    1,
    0,
    1,
    0,
    1,
    1,
    1,
    0,
    1,
    1,
    0,
};

static TmdSource _gActor113100Model07BC4;
static TmdSource _gActor113100Model07960;
static TmdSource _gActor113100Model07AB4;

TaskDesc D_actor_113100_80144308[4] = {
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, _actor113100PierceCarradineTask, { .model = &_gActor113100PierceCarradineBody } },
    { { { TASK_BODY_TMD, 192 } }, _actor113100BillboardTask, { .model = &_gActor113100Model07BC4 } },
    { { { TASK_BODY_TMD, 192 } }, _actor113100AttachedModelTask, { .model = &_gActor113100Model07960 } },
    { { { TASK_BODY_TMD, 192 } }, _actor113100AttachedModelTask, { .model = &_gActor113100Model07AB4 } },
};

TaskMessageEntry D_actor_113100_80144338[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actor113100PlayAnimation },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor113100SetModelDraw },
    { ACTOR_MESSAGE_WALK_TO, _actor113100StartWalk },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor113100ApplyCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Setup handler (state 0): allocates the work block, clears the
/// three "no id yet" sentinels and spawns the actor's children from
/// `D_actor_113100_80144308` -- index 1 only in arena mode
/// (`gGameSession->location.loc.variant == 2`), then indices 2 and 3, whose models get the
/// texture page and CLUT of the area record the actor's own location key
/// resolves to. It then builds the work block's collision sphere `body`: it
/// borrows the one-entry `contacts` table that follows it, sits on model part 1
/// with a zero offset and a radius of 0x100, and is linked with its pair pass
/// enabled. Finally it publishes the message
/// table, installs the exit callback and steps to the next state.
static void func_actor_113100_80131E58(Task* task)
{
    _Actor113100PierceCarradineWork* work;
    Task*                            child2;
    Task*                            child3;
    GameLocationKey                  key;
    GameLocationKey*                 sessionKey2;
    GameLocationKey*                 sessionKey3;
    TmdObject*                       model2;
    TmdObject*                       model3;
    AreaPlacement*                   entry2;
    AreaPlacement*                   entry3;
    WorldCollisionBody*              obj;
    u8                               areaByte0;
    u32                              raw2;
    u32                              raw3;
    u32                              index2;
    u32                              index3;

    work = memCalloc(sizeof(_Actor113100PierceCarradineWork), false);
    if (work == NULL) {
        enemyTaskExit(task);
        return;
    }
    task->work               = work;
    work->model.animId       = ACTOR_MODEL_STATE_NONE;
    work->model.bank         = ACTOR_MODEL_STATE_NONE;
    work->freeCountdown      = -1;
    work->walk.carry[0].word = 0;
    work->walk.carry[1].word = 0;
    work->walk.carry[2].word = 0;
    if (gGameSession->location.loc.variant == 2) {
        work->billboardTask = taskSpawnFromTable(D_actor_113100_80144308, 1, 8, task);
    }

    child2 = taskSpawnFromTable(D_actor_113100_80144308, 2, 4, task);
    if (child2 != NULL) {
        sessionKey2 = &gGameSession->location.loc;
        raw2        = ((Enemy*)task->spawnArg2.pointer)->placeKey;
        model2      = child2->extra.tmd;
        key.stage   = sessionKey2->stage;
        key.area    = sessionKey2->area;
        key.room    = sessionKey2->room;
        areaByte0   = gGameSession->location.loc.view;
        index2      = raw2 >> 12;
        key.view    = areaByte0;
        areaSyncLocationVariant(&key);
        entry2                    = gpAreaPlaceAt(areaGetVariant(&key)->placements, index2);
        model2->texturePageOffset = entry2->texturePageOffset;
        model2->clutRowOffset     = entry2->clutRowOffset;
        if (model2->buffer != NULL) {
            tmdBuildBufferHalf(model2);
            tmdBuildBufferHalf(model2);
        }
    }

    child3 = taskSpawnFromTable(D_actor_113100_80144308, 3, 2, task);
    if (child3 != NULL) {
        sessionKey3 = &gGameSession->location.loc;
        raw3        = ((Enemy*)task->spawnArg2.pointer)->placeKey;
        model3      = child3->extra.tmd;
        key.stage   = sessionKey3->stage;
        key.area    = sessionKey3->area;
        key.room    = sessionKey3->room;
        areaByte0   = gGameSession->location.loc.view;
        index3      = raw3 >> 12;
        key.view    = areaByte0;
        areaSyncLocationVariant(&key);
        entry3                    = gpAreaPlaceAt(areaGetVariant(&key)->placements, index3);
        model3->texturePageOffset = entry3->texturePageOffset;
        model3->clutRowOffset     = entry3->clutRowOffset;
        if (model3->buffer != NULL) {
            tmdBuildBufferHalf(model3);
            tmdBuildBufferHalf(model3);
        }
    }

    _actor113100BindLighting(task);

    obj                   = &work->body;
    obj->coord            = &task->extra.tmd->coords[1];
    obj->context.contacts = work->contacts;
    obj->key              = 0x30000;
    obj->radius           = 0x100;
    obj->pos.vx           = 0;
    obj->pos.vy           = 0;
    obj->pos.vz           = 0;
    obj->flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, obj);
    obj->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    worldCollisionInitContacts(obj->context.contacts, ARRAY_SIZE(work->contacts), 0);

    task->msgTable = D_actor_113100_80144338;
    mistParkingSetPierceCollisionPatchLowered(MIST_PARKING_PIERCE_PATCH_LOWERED);
    _actor113100SetModelDraw(task, 0, ACTOR_113100_DRAW_HIDE, 0);
    task->exitCallback = _actor113100Exit;
    task->state       += 1;
}

/// Per-frame tick of the actor's live state. While the model is not deferred
/// (bit 0x80 of `TmdObject::flags`) it rebuilds part 1's world matrix and
/// draws the ground shadow under that part. `gSceneCombatState.actorControl` gates the rest: a
/// nonzero value skips it. The live path dispatches `_actor113100CheckPierceAppearance`
/// or `_actor113100StepWalk` from a two-entry stack table indexed by
/// `walk.motion`, integrates the 16.16 step at `walk.velocity` into `walk.carry[0].word` /
/// `walk.carry[1].word` / `walk.carry[2].word` and the root translation, ticks slots 1..0x13
/// once `model.ticking` has latched, and plays ids 0x5113000F / 0x51130013 /
/// 0x51130010 from the slot-1 cue flags. While the model is visible it clears
/// the occupied entry of `contacts`, ramps `turnWeight` toward 0 or 0x1000
/// according to `turnUp`, and turns the head toward the player by that weight.
/// `viewReady` rebuilds part 1's lighting, and `freeCountdown` counts the buffer
/// free down to zero.
static void func_actor_113100_80132104(Task* task)
{
    TmdObject*                       extra    = task->extra.tmd;
    _Actor113100PierceCarradineWork* work     = task->work;
    TaskFunc                         funcs[2] = { _actor113100CheckPierceAppearance, _actor113100StepWalk };
    VECTOR3                          pos;
    GfxCoord*                        coord;
    const AnimationRecord*           rec;
    s32                              i;
    s32                              snd;
    s8                               mode;
    u16                              rate;

    if (!(extra->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        task->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&task->extra.tmd->coords[1]);
        if (worldCollisionProjectGroundPoint(MATRIX_TRANS(&task->extra.tmd->coords[1].workm), &pos) != 0) {
            effectDrawGroundShadow(&pos, 0x200, gRoomEffectState->groundShadowShade);
        }
    }
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        funcs[work->walk.motion](task);
        coord                     = task->extra.tmd->coords;
        work->walk.carry[0].word += work->walk.velocity.vx;
        work->walk.carry[1].word += work->walk.velocity.vy;
        work->walk.carry[2].word += work->walk.velocity.vz;
        coord->coord.t[0]        += work->walk.carry[0].halves.integer;
        coord->coord.t[1]        += work->walk.carry[1].halves.integer;
        coord->coord.t[2]        += work->walk.carry[2].halves.integer;
        coord->composeStamp       = GRAPHICS_COORD_DIRTY;
        work->walk.carry[0].word  = work->walk.carry[0].halves.fraction;
        work->walk.carry[1].word  = work->walk.carry[1].halves.fraction;
        work->walk.carry[2].word  = work->walk.carry[2].halves.fraction;
        if (work->model.ticking != 0) {
            for (i = 1; i < 0x14; i++) {
                animationTickSlot(&work->rig.anim, i);
            }
            rec = animationGetCurrentRecord(&work->rig.anim, &work->rig.slots[1]);
            if (rec != NULL) {
                if (rec->flags & ANIMATION_RECORD_CUE_2) {
                    snd = 0x5113000F;
                    if (gGameSession->location.loc.view == 0x10) {
                        snd = 0x51130013;
                    }
                    sndEvtRequestScriptStart(snd, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                }
                if ((rec->flags & ANIMATION_RECORD_CUE_1) && (gGameSession->location.loc.view != 0x10)) {
                    sndEvtRequestScriptStart(SOUND_MIST_PARKING_PIERCE_STEP_2, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                }
            }
        }
        if (!(extra->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
            worldCollisionClearContacts(work->contacts);
            mode = work->turnUp;
            switch (mode) {
                case 0:
                    rate             = work->turnWeight - 0x100;
                    work->turnWeight = rate;
                    if ((s16)rate < 0) {
                        work->turnWeight = 0;
                    }
                    break;
                case 1:
                    rate             = work->turnWeight + 0x100;
                    work->turnWeight = rate;
                    if ((s16)rate >= 0x1001) {
                        work->turnWeight = 0x1000;
                    }
                    break;
            }
            animationAimHeadAtTask(task, gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x200, 0x100, work->turnWeight);
        }
        if (gGameSession->viewReady != 0) {
            task->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(&task->extra.tmd->coords[1]);
            worldCoordSetModelLighting(extra, task->extra.tmd->coords[1].workm.t, 0, 3);
        }
        if (work->freeCountdown >= 0) {
            if (work->freeCountdown == 0) {
                tmdFreePrimitiveBuffer(extra);
            }
            work->freeCountdown--;
        }
    }
}

/// Applies Pierce's bank/clip transition and its clip-selected head-turn ramp.
///
/// Borrows initialized work, live model coordinates and a readable request with
/// `_actor113100PlayAnimation`'s bank, clip and lifetime contract. Repeated clips retain
/// their cursor. Slots 1..19 blend or reset and tick only for a changed clip.
static inline void _actor113100ApplyAnimationRequest(_Actor113100PierceCarradineWork* work,
                                                     TmdObject* model, const AnimationPlayRequest* request)
{
    s32 slotIndex;

    enum { ACTOR_113100_FIRST_DRIVEN_SLOT = 1 };

    if (request->source.index != work->model.bank) {
        work->model.bank   = request->source.index;
        work->model.animId = ACTOR_MODEL_STATE_NONE;
        animationInitContext(&work->rig.anim, D_actor_113100_801442E0[work->model.bank], model, work->rig.poses,
                             work->rig.slots);
    }
    // Repeated clips keep their cursor; a bank change invalidates the previous clip.
    if (request->animationId != work->model.animId) {
        work->model.animId = request->animationId;
        if (request->blend != ANIMATION_BLEND_RESET && work->model.ticking != 0) {
            for (slotIndex = ACTOR_113100_FIRST_DRIVEN_SLOT; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
                animationSeekSlotWithBlend(&work->rig.anim, slotIndex, work->model.animId, 0, request->blendFrames);
            }
        } else {
            for (slotIndex = ACTOR_113100_FIRST_DRIVEN_SLOT; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
                animationResetSlot(&work->rig.anim, slotIndex, work->model.animId);
            }
        }
        // Seed the new pose before the actor's normal frame update can tick it.
        for (slotIndex = ACTOR_113100_FIRST_DRIVEN_SLOT; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationTickSlot(&work->rig.anim, slotIndex);
        }
        work->model.ticking = 1;
    }
    work->turnUp = D_actor_113100_801442E4[request->animationId];
}

/// Turns to the queued heading, then starts the walk at 32 parent-space units per tick.
///
/// Requires initialized body work and a live model root. Angles use 4096 units
/// per turn; the yaw difference narrows to a signed halfword without shortest-turn
/// wrapping. Steps by 64 until within 64, then snaps to `walkYaw`, rotates a
/// 16.16 forward velocity, seeds all arrival gaps and plays clip 2 with a
/// four-frame blend. Advances the walk step only on completion. Rebuilds a pure
/// Y rotation and invalidates composition, retaining translation and stored Euler
/// parameters. The caller integrates velocity after this step.
static void _actor113100TurnAndBeginWalk(Task* task)
{
    _Actor113100PierceCarradineWork* work;
    GfxCoord*                        rootCoord;
    VECTOR                           forwardVelocity;
    AnimationPlayRequest             walkRequest;
    s32                              currentYaw;
    s32                              currentYaw16;
    u16                              yaw;
    s16                              yawGap;

    enum { ACTOR_113100_WALK_SPEED_16_16 = 32 * 0x10000 };

    rootCoord  = task->extra.tmd->coords;
    work       = task->work;
    currentYaw = ratan2(rootCoord->coord.m[0][2], rootCoord->coord.m[2][2]);
    yaw        = work->walkYaw;
    yawGap     = yaw - currentYaw;
    if (ABS(yawGap) >= ACTOR_113100_YAW_STEP + 1) {
        currentYaw16 = (s16)currentYaw;
        if (yawGap < 0) {
            yaw = currentYaw16 - ACTOR_113100_YAW_STEP;
        } else {
            yaw = currentYaw16 + ACTOR_113100_YAW_STEP;
        }
        gfxSetRotIdentity(&rootCoord->coord);
        RotMatrixY((s16)yaw, &rootCoord->coord);
        rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    } else {
        gfxSetRotIdentity(&rootCoord->coord);
        RotMatrixY((s16)yaw, &rootCoord->coord);
        forwardVelocity.vx = 0;
        forwardVelocity.vy = 0;
        forwardVelocity.vz = ACTOR_113100_WALK_SPEED_16_16;
        ApplyMatrixLV(&rootCoord->coord, &forwardVelocity, &work->walk.velocity);
        work->walk.lastDistance.vx       = ACTOR_WALK_DISTANCE_NONE;
        work->walk.lastDistance.vy       = ACTOR_WALK_DISTANCE_NONE;
        work->walk.lastDistance.vz       = ACTOR_WALK_DISTANCE_NONE;
        walkRequest.source.index         = ACTOR_113100_BODY_ANIMATION_BANK;
        walkRequest.animationId          = ACTOR_113100_ANIM_WALK;
        walkRequest.blend                = ANIMATION_BLEND_INTERPOLATE;
        walkRequest.blendFrames          = ACTOR_113100_TURN_BLEND_FRAMES;
        walkRequest.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
        _actor113100PlayAnimation(task, ACTOR_MESSAGE_PLAY_ANIMATION, &walkRequest, 0);
        work->walk.motionStep++;
        rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    }
}

/// Stops Pierce's walk when neither horizontal arrival gap gets smaller.
///
/// Requires a live root and initialized walk work. Gaps use root-parent units:
/// the full-word sign chooses a low-halfword subtraction, then each gap narrows
/// to a signed halfword. Previous gaps hold their absolute values. Arrival plays
/// the queued bank-0 clip with a five-frame blend, clears XYZ velocity and advances
/// the walk step. The queued clip must be loaded; position and fractional carry
/// are retained. Otherwise updates only the X/Z previous gaps.
static void _actor113100CheckWalkArrival(Task* task)
{
    _Actor113100PierceCarradineWork* work;
    GfxCoord*                        rootCoord;
    SVECTOR                          distance;
    s32                              deltaX;
    s32                              deltaZ;
    AnimationPlayRequest             arrivalRequest;

    work      = task->work;
    rootCoord = task->extra.tmd->coords;
    // The full-word sign selects a low-halfword gap; signed narrowing is intentional.
    deltaX      = _actorMotionWalkAxisGap(&work->walk.target.vx, &rootCoord->coord.t[0]);
    distance.vx = deltaX;
    deltaZ      = _actorMotionWalkAxisGap(&work->walk.target.vz, &rootCoord->coord.t[2]);
    distance.vz = deltaZ;
    if (distance.vx >= work->walk.lastDistance.vx && distance.vz >= work->walk.lastDistance.vz) {
        arrivalRequest.source.index         = ACTOR_113100_BODY_ANIMATION_BANK;
        arrivalRequest.animationId          = work->model.nextAnimId;
        arrivalRequest.blend                = ANIMATION_BLEND_INTERPOLATE;
        arrivalRequest.blendFrames          = ACTOR_113100_WALK_BLEND_FRAMES;
        arrivalRequest.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
        _actor113100PlayAnimation(task, ACTOR_MESSAGE_PLAY_ANIMATION, &arrivalRequest, 0);
        work->walk.velocity.vx = 0;
        work->walk.velocity.vy = 0;
        work->walk.velocity.vz = 0;
        work->walk.motionStep++;
        return;
    }
    work->walk.lastDistance.vx = distance.vx < 0 ? -distance.vx : distance.vx;
    work->walk.lastDistance.vz = distance.vz < 0 ? -distance.vz : distance.vz;
}

/// Sets Pierce's body drawing, sphere-pair participation and buffer policy.
///
/// Requires initialized body work and a live TMD task. Modes 0/2 hide and 1/3 show;
/// every accepted mode disables the body's sphere-pair pass. Modes 0/1 permit
/// automatic buffers, and 1 requests a missing buffer immediately. Mode 2 disables
/// automatic buffers and seeds a countdown of 2; two eligible ticks consume it,
/// then the tick reading zero frees the buffers. Mode 3 disables automatic buffers without allocating.
/// Other modes leave any pending release countdown intact. Ignores message ID and
/// second payload. Returns 0 for modes 0..3, or 1 without changes for another mode.
static s32 _actor113100SetModelDraw(Task* task, s32 messageId, s32 drawMode, s32 unusedArg)
{
    _Actor113100PierceCarradineWork* work;
    _Actor113100PierceCarradineWork* drawWork;
    WorldCollisionBody*              body;
    WorldCollisionBody*              bodyCursor;
    TmdObject*                       model;
    s32                              bodyIndex;
    s32                              result;

    enum { ACTOR_113100_COLLISION_BODY_COUNT = 1 };

    work     = task->work;
    model    = task->extra.tmd;
    drawWork = task->work;
    body     = &drawWork->body;
    result   = 0;

    switch (drawMode) {
        case ACTOR_113100_DRAW_HIDE:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            bodyCursor    = body;
            for (bodyIndex = 0; bodyIndex < ACTOR_113100_COLLISION_BODY_COUNT; bodyIndex++) {
                bodyCursor->flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                bodyCursor++;
            }
            model->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_113100_DRAW_SHOW:
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            bodyCursor    = body;
            for (bodyIndex = 0; bodyIndex < ACTOR_113100_COLLISION_BODY_COUNT; bodyIndex++) {
                bodyCursor->flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                bodyCursor++;
            }
            tmdAllocPrimitiveBuffer(model);
            model->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_113100_DRAW_HIDE_AND_RELEASE:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            bodyCursor    = body;
            for (bodyIndex = 0; bodyIndex < ACTOR_113100_COLLISION_BODY_COUNT; bodyIndex++) {
                bodyCursor->flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                bodyCursor++;
            }
            drawWork->freeCountdown = ACTOR_113100_BUFFER_RELEASE_DELAY_TICKS;
            model->flags           |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_113100_DRAW_SHOW_SKIP_AUTO_BUFFER:
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            bodyCursor    = &work->body;
            for (bodyIndex = 0; bodyIndex < ACTOR_113100_COLLISION_BODY_COUNT; bodyIndex++) {
                bodyCursor->flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                bodyCursor++;
            }
            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            result = 1;
            break;
    }
    return result;
}

/// Copies a walk destination and starts Pierce's four-stage walking sequence.
///
/// Requires initialized body work and a live root. Borrows a readable placement
/// through the call; XYZ position is in the root parent's frame, and Euler angles
/// use 4096 units per turn. Only the final yaw is used by the closing turn.
/// An optional readable clip pair selects the initial and queued arrival clips
/// in bank 0; NULL selects clips 2 and 1. Clip IDs must name loaded entries 1..35.
/// Starts at heading selection, applies the initial clip with a five-frame blend
/// when playback is ticking, and selects its head-turn ramp. Repeated clips keep
/// their cursor. Velocity and fractional carry are retained until later walk steps
/// replace them. Ignores message ID, retains no payload pointer, and returns 0.
static s32 _actor113100StartWalk(Task* task, s32 messageId, const ActorTransform* placement, const ActorMotionWalkAnim* walkAnim)
{
    _Actor113100PierceCarradineWork* playWork;
    _Actor113100PierceCarradineWork* walkWork;
    AnimationPlayRequest             walkRequest;
    TmdObject*                       model;

    enum { ACTOR_113100_ANIM_DEFAULT_ARRIVAL = 1 };

    walkWork                    = task->work;
    walkWork->walk.motion       = ACTOR_WALK_MOTION_WALKING;
    walkWork->walk.motionStep   = ACTOR_113100_WALK_STEP_QUEUE_HEADING;
    walkWork->walk.target.vx    = placement->pos.vx;
    walkWork->walk.target.vy    = placement->pos.vy;
    walkWork->walk.target.vz    = placement->pos.vz;
    walkWork->walk.targetRot.vx = placement->rot.vx;
    walkWork->walk.targetRot.vy = placement->rot.vy;
    walkWork->walk.targetRot.vz = placement->rot.vz;
    walkRequest.source.index    = ACTOR_113100_BODY_ANIMATION_BANK;
    if (walkAnim != NULL) {
        walkRequest.animationId    = walkAnim->animationId;
        walkWork->model.nextAnimId = walkAnim->nextAnimId;
    } else {
        walkRequest.animationId    = ACTOR_113100_ANIM_WALK;
        walkWork->model.nextAnimId = ACTOR_113100_ANIM_DEFAULT_ARRIVAL;
    }
    walkRequest.blend                = ANIMATION_BLEND_INTERPOLATE;
    walkRequest.blendFrames          = ACTOR_113100_WALK_BLEND_FRAMES;
    walkRequest.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;

    playWork = task->work;
    model    = task->extra.tmd;
    _actor113100ApplyAnimationRequest(playWork, model, &walkRequest);
    return 0;
}

/// Dispatches Pierce's billboard attachment, camera-facing tick or teardown.
///
/// Requires a live TMD task and state 0..2. Setup borrows the parent TMD task from
/// `spawnArg2.pointer` and its coordinate index from `spawnArg1.value`; those
/// resources must outlive the child. The selected callback may destroy the task.
/// There is no bounds check or return value.
static void _actor113100BillboardTask(Task* task)
{
    TaskFuncTable3 states;

    states = D_actor_113100_80131E24;
    states.funcs[task->state](task);
}

/// Attaches the billboard 100 units above the selected parent model part.
///
/// Setup requires live child and parent TMD tasks, child coordinate 0 and a valid
/// parent coordinate index in `spawnArg1.value`. Borrows the parent from
/// `spawnArg2.pointer` and joins its teardown tree; ancestry must remain acyclic.
/// The coordinate parent remains borrowed until teardown. Initially shows the
/// model only while game flag 0xF1 is zero, then advances to the camera-facing tick.
static void _actor113100AttachBillboard(Task* task)
{
    TmdObject* model;
    Task*      parentTask;
    s32        parentPartIndex;
    GfxCoord*  rootCoord;
    GfxCoord*  parentCoords;

    enum { ACTOR_113100_BILLBOARD_Y_OFFSET = 100 };

    model           = task->extra.tmd;
    parentTask      = task->spawnArg2.pointer;
    parentPartIndex = task->spawnArg1.value;
    rootCoord       = model->coords;
    parentCoords    = parentTask->extra.tmd->coords;

    rootCoord->coord.t[0]   = 0;
    rootCoord->coord.t[1]   = ACTOR_113100_BILLBOARD_Y_OFFSET;
    rootCoord->coord.t[2]   = 0;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    rootCoord->parent       = &parentCoords[parentPartIndex];

    taskReparent(parentTask, task);
    if (gameFlagGetNibble(GAME_FLAG_0F1) == 0) {
        model->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    task->state += 1;
}

/// Cancels the parent and camera rotations to face Pierce's billboard toward the camera.
///
/// Requires an attached child root, a live parent part selected by `spawnArg1.value`
/// and the active mapped camera. Uses transpose(parent world rotation) times
/// transpose(camera rotation), then applies the parent part's extracted local yaw
/// as an X rotation. Retains local translation and marks composition dirty.
/// The parent world matrix must already be current; no coordinate composition is
/// performed here.
static void _actor113100FaceBillboardToCamera(Task* task)
{
    Task*     parentTask;
    MATRIX    inverseView;
    SVECTOR   parentRotation;
    MATRIX*   viewMatrix;
    MATRIX*   billboardMatrix;
    GfxCoord* parentPart;
    GfxCoord* rootCoord;
    s32       parentPartIndex;

    parentPartIndex = task->spawnArg1.value;
    rootCoord       = task->extra.tmd->coords;
    parentTask      = task->spawnArg2.pointer;
    parentPart      = &parentTask->extra.tmd->coords[parentPartIndex];
    viewMatrix      = &viewGetMappedCamera(&gGameSession->location.loc)->transform;
    billboardMatrix = &rootCoord->coord;
    TransposeMatrix(&parentPart->workm, billboardMatrix);
    TransposeMatrix(viewMatrix, &inverseView);
    MulMatrix0(billboardMatrix, &inverseView, billboardMatrix);
    gfxExtractSmallestEuler(&parentRotation, &parentPart->coord);
    RotMatrixX(parentRotation.vy, billboardMatrix);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Dispatches an attached model's setup, inherited draw-policy tick or teardown.
///
/// Requires a live TMD task and state 0..2. Setup borrows the parent TMD task from
/// `spawnArg2.pointer` and a valid coordinate index from `spawnArg1.value`; the
/// parent coordinates and lighting must outlive the child. Setup joins the parent
/// teardown tree. Each tick mirrors drawing and buffer policy, allocating a missing
/// buffer when permitted. The selected callback may destroy the task. No bounds
/// check or return value.
static void _actor113100AttachedModelTask(Task* task)
{
    TaskFuncTable3 states;

    states = D_actor_113100_80131E30;
    states.funcs[task->state](task);
}

#include "../../shared/model_placement_attach.inc.c"

#include "../../shared/model_placement_mirror_parent.inc.c"

/// Dispatches Pierce Carradine's body setup, frame update or teardown.
///
/// Requires the twenty-part TMD task created by this package's body descriptor
/// and `state` in 0..2. Setup allocates owned work and collision storage; update
/// moves and animates the body; teardown unlinks collision before releasing it.
/// Dispatch continues independently of the scene's actor-control gate. A state
/// callback may destroy the task. The state index is unchecked.
static void _actor113100PierceCarradineTask(Task* task)
{
    TaskFuncTable3 states;

    states = D_actor_113100_80131E3C;
    states.funcs[task->state](task);
}

/// Unlinks Pierce's collision sphere before releasing his body task and work.
///
/// Requires a live initialized work block whose body is linked. The task exit
/// releases its work and descendants; no borrowed lighting or coordinates may be
/// used afterwards.
static void _actor113100Exit(Task* task)
{
    _Actor113100PierceCarradineWork* work = task->work;

    worldCollisionUnlinkBody(&work->body);
    enemyTaskExit(task);
}

/// Makes Pierce's model borrow the light and color matrices in its work block.
///
/// Requires a live TMD task with allocated body work. The matrices remain borrowed
/// until the model is destroyed, so the work block must outlive rendering.
static void _actor113100BindLighting(Task* task)
{
    TmdObject*                       model;
    _Actor113100PierceCarradineWork* work;

    model           = task->extra.tmd;
    work            = task->work;
    model->lightMtx = &work->model.light;
    model->colorMtx = &work->model.color;
}

/// Reveals Pierce when his appearance flag first becomes positive while he is idle.
///
/// Requires live body work. A positive flag after a recorded zero enables model
/// drawing and restores the parking collision patch. Every idle update records
/// the flag in its signed-byte latch, including updates that do not reveal him.
static void _actor113100CheckPierceAppearance(Task* task)
{
    _Actor113100PierceCarradineWork* work;
    s32                              appearFlag;

    work       = task->work;
    appearFlag = gameFlagGetNibble(GAME_FLAG_0ED);
    if (appearFlag > 0 && work->lastAppearFlag == 0) {
        _actor113100SetModelDraw(task, 0, ACTOR_113100_DRAW_SHOW, 0);
        mistParkingSetPierceCollisionPatchLowered(MIST_PARKING_PIERCE_PATCH_RESTORED);
    }
    work->lastAppearFlag = appearFlag;
}

/// Dispatches Pierce's heading, opening turn, arrival or closing-turn step.
///
/// Requires live initialized body work and a walk step in 0..3, in that order.
/// The caller integrates the step's velocity after dispatch. The stack copy of
/// the four callbacks is indexed without a bounds check; this is a motion-state
/// index, independent of animation slot indices.
static void _actor113100StepWalk(Task* task)
{
    _Actor113100PierceCarradineWork* work;
    TaskFuncTable4                   walkSteps;

    work      = task->work;
    walkSteps = D_actor_113100_80131E48;
    walkSteps.funcs[work->walk.motionStep](task);
}

/// Queues the destination heading and starts Pierce's opening-turn clip.
///
/// Requires initialized walk work and a live root coordinate. Normalizes the
/// root-parent offset to `walk.target`, stores its yaw in `walkYaw` (4096 units per
/// turn), and plays bank-0 clip 22 with a four-frame blend. Advances to the opening
/// turn without changing the root rotation or velocity; that next step applies
/// the queued heading.
static void _actor113100QueueWalkHeading(Task* task)
{
    _Actor113100PierceCarradineWork* work;
    GfxCoord*                        rootCoord;
    AnimationPlayRequest             turnRequest;
    VECTOR                           targetOffset;
    SVECTOR                          direction;

    enum { ACTOR_113100_ANIM_OPENING_TURN = 22 };

    work      = task->work;
    rootCoord = task->extra.tmd->coords;

    targetOffset.vx = work->walk.target.vx - rootCoord->coord.t[0];
    targetOffset.vy = work->walk.target.vy - rootCoord->coord.t[1];
    targetOffset.vz = work->walk.target.vz - rootCoord->coord.t[2];
    VectorNormalS(&targetOffset, &direction);
    work->walkYaw = ratan2(direction.vx, direction.vz);

    turnRequest.source.index         = ACTOR_113100_BODY_ANIMATION_BANK;
    turnRequest.animationId          = ACTOR_113100_ANIM_OPENING_TURN;
    turnRequest.blend                = ANIMATION_BLEND_INTERPOLATE;
    turnRequest.blendFrames          = ACTOR_113100_TURN_BLEND_FRAMES;
    turnRequest.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    _actor113100PlayAnimation(task, ACTOR_MESSAGE_PLAY_ANIMATION, &turnRequest, 0);
    work->walk.motionStep++;
}

/// Turns Pierce's root to the destination yaw, then returns the walk to idle.
///
/// Requires initialized walk work and a live root. Angles use 4096 units per turn;
/// the target-minus-current yaw narrows to a signed halfword without shortest-turn
/// wrapping. Steps by 64 until within 64, then snaps, plays the queued bank-0 clip
/// with a five-frame blend and resets both motion selectors. The queued clip must
/// be loaded. Rebuilds rotation using the extracted pitch and roll, retaining
/// translation and stored Euler parameters, and invalidates composition.
static void _actor113100TurnToArrivalYaw(Task* task)
{
    _Actor113100PierceCarradineWork* work;
    GfxCoord*                        rootCoord;
    SVECTOR                          rotation;
    AnimationPlayRequest             closingRequest;
    s32                              currentYaw;
    s16                              yawGap;

    rootCoord = task->extra.tmd->coords;
    work      = task->work;

    gfxExtractSmallestEuler(&rotation, &rootCoord->coord);
    yawGap = (u16)work->walk.targetRot.vy - (u16)rotation.vy;
    if (ABS(yawGap) >= ACTOR_113100_YAW_STEP + 1) {
        currentYaw = rotation.vy;
        if (yawGap < 0) {
            rotation.vy = currentYaw - ACTOR_113100_YAW_STEP;
        } else {
            rotation.vy = currentYaw + ACTOR_113100_YAW_STEP;
        }
    } else {
        rotation.vy                         = work->walk.targetRot.vy;
        closingRequest.source.index         = ACTOR_113100_BODY_ANIMATION_BANK;
        closingRequest.animationId          = work->model.nextAnimId;
        closingRequest.blend                = ANIMATION_BLEND_INTERPOLATE;
        closingRequest.blendFrames          = ACTOR_113100_WALK_BLEND_FRAMES;
        closingRequest.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
        _actor113100PlayAnimation(task, ACTOR_MESSAGE_PLAY_ANIMATION, &closingRequest, 0);
        work->walk.motion     = ACTOR_WALK_MOTION_IDLE;
        work->walk.motionStep = ACTOR_113100_WALK_STEP_QUEUE_HEADING;
    }

    // Rebuild only rotation; parent-space translation remains intact.
    gfxSetRotIdentity(&rootCoord->coord);
    RotMatrix(&rotation, &rootCoord->coord);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Applies a changed clip to Pierce's twenty-part rig and selects its head-turn ramp.
///
/// Requires live model coordinates and allocated body work with `model.bank`
/// initially `ACTOR_MODEL_STATE_NONE`. Borrows a nonoverlapping readable request
/// through the call. The loaded bank is 0; clip IDs 1..35 index both its sets and
/// the head-turn table, and stored IDs narrow to signed bytes. A bank change binds
/// the rig and invalidates the previous clip. A changed clip blends for whole
/// `blendFrames` (normally 0..2047) when blend is nonzero and the rig is ticking, or resets otherwise;
/// slots 1..19 tick once before subsequent frame ticks are enabled. A repeated
/// clip in the same bank keeps its cursor, but still selects the head-turn ramp.
/// Coordinates, slots, poses and loaded clip data remain borrowed for playback's
/// lifetime. Ignores message ID, collision choice and second payload. Returns 0.
static s32 _actor113100PlayAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg)
{
    _Actor113100PierceCarradineWork* work;
    TmdObject*                       model;

    work  = task->work;
    model = task->extra.tmd;
    _actor113100ApplyAnimationRequest(work, model, request);
    return 0;
}

#include "../../shared/actor_messages_place_euler.inc.c"

/// Shows or hides Pierce's billboard, or selects his head-turn ramp.
///
/// Requires initialized body work and a readable borrowed command through the call.
/// Actions 0/1 show/hide the optional billboard; 2 ramps the head toward the player
/// and 3 releases that turn. A later animation request can replace this ramp choice.
/// Ignores the command context, message ID and second payload. Unknown actions and
/// an absent billboard have no effect. Retains no payload pointer and returns 0.
static s32 _actor113100ApplyCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArg)
{
    _Actor113100PierceCarradineWork* work;
    TmdObject*                       billboardModel;

    enum {
        ACTOR_113100_COMMAND_SHOW_BILLBOARD = 0,
        ACTOR_113100_COMMAND_HIDE_BILLBOARD = 1,
        ACTOR_113100_COMMAND_AIM_HEAD       = 2,
        ACTOR_113100_COMMAND_RELEASE_HEAD   = 3,
        ACTOR_113100_HEAD_TURN_RELEASE      = 0,
        ACTOR_113100_HEAD_TURN_AIM          = 1,
    };

    work = task->work;

    switch (command->command) {
        case ACTOR_113100_COMMAND_SHOW_BILLBOARD:
            if (work->billboardTask != NULL) {
                billboardModel         = work->billboardTask->extra.tmd;
                billboardModel->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            break;

        case ACTOR_113100_COMMAND_HIDE_BILLBOARD:
            if (work->billboardTask != NULL) {
                billboardModel         = work->billboardTask->extra.tmd;
                billboardModel->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            break;

        case ACTOR_113100_COMMAND_AIM_HEAD:
            work->turnUp = ACTOR_113100_HEAD_TURN_AIM;
            break;

        case ACTOR_113100_COMMAND_RELEASE_HEAD:
            work->turnUp = ACTOR_113100_HEAD_TURN_RELEASE;
            break;

        default:
            return 0;
    }
    return 0;
}
