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
#include "main/gfx_types.h"
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
/// (`func_actor_113100_801331E8`), 0x7D4 (`actorMsgPlaceEuler`), 0x7D5
/// (`func_actor_113100_80132790`), 0x7DD (`func_actor_113100_801328EC`) and
/// 0x7DB (`func_actor_113100_801333B8`), terminated by `TASK_MESSAGE_TABLE_END`.
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.

extern TaskMessageEntry D_actor_113100_80144338[];

/// Animation bank table the 0x7D3 handler `func_actor_113100_801331E8` indexes
/// by the animation id it has latched into `_Actor113100PierceCarradineWork::model.bank`; the
/// entry is the `AnimationSet**` passed to `animationInitContext`.
extern AnimationSet*  D_actor_113100_80144250[36];
extern AnimationSet** D_actor_113100_801442E0[1];

/// Per-animation byte the same handler copies into
/// `_Actor113100PierceCarradineWork::turnUp` from
/// `AnimationPlayRequest::animationId`.
extern u8 D_actor_113100_801442E4[];

/// Main-executable routine the turn handler `func_actor_113100_801324DC` calls
/// with a yaw angle and the root coordinate's matrix, after resetting its 3x3
/// to the identity.

/// Gameplay import, called with 1 by the setup handler and with 0 by
/// `func_actor_113100_80132F40`.

static void func_actor_113100_80131E58(Task* task);
static void func_actor_113100_80132104(Task* task);
static void func_actor_113100_801324DC(Task* task);
static void func_actor_113100_8013264C(Task* task);
s32         func_actor_113100_80132790(Task* task, s32 msgId, s32 mode, s32 arg3);
static void func_actor_113100_80132B30(Task* task);
static void func_actor_113100_80132BDC(Task* task);
static void func_actor_113100_80132EF0(Task* task);
static void func_actor_113100_80132F24(Task* task);
static void func_actor_113100_80132F40(Task* task);
static void func_actor_113100_80132FB4(Task* task);
static void func_actor_113100_8013301C(Task* task);
static void func_actor_113100_801330E8(Task* task);
s32         func_actor_113100_801331E8(Task* task, s32 msgId, AnimationPlayRequest* preset, s32 arg3);

/// States of a child task posed on one of the parent's parts: attach to the
/// parent, rebuild its display matrix every frame, then `taskKill`. Dispatched
/// by `func_actor_113100_80132AD8`.
static const TaskFuncTable3 D_actor_113100_80131E24 = { {
    func_actor_113100_80132B30,
    func_actor_113100_80132BDC,
    taskKill,
} };

/// States of the child task that follows the parent's model flags: attach to
/// the parent's part, mirror its flags every frame, then `taskKill`.
/// Dispatched by `func_actor_113100_80132C9C`.
static const TaskFuncTable3 D_actor_113100_80131E30 = { {
    modelPlacementAttachChild,
    modelPlacementMirrorParent,
    taskKill,
} };

/// The actor's own three states - setup, per-frame tick and exit -
/// dispatched by `func_actor_113100_80132E98`.
static const TaskFuncTable3 D_actor_113100_80131E3C = { {
    func_actor_113100_80131E58,
    func_actor_113100_80132104,
    func_actor_113100_80132EF0,
} };

/// The four main-body handlers, dispatched by `func_actor_113100_80132FB4`
/// through `_Actor113100PierceCarradineWork::walk.motionStep`.
static const TaskFuncTable4 D_actor_113100_80131E48 = { {
    func_actor_113100_8013301C,
    func_actor_113100_801324DC,
    func_actor_113100_8013264C,
    func_actor_113100_801330E8,
} };

static TmdSource _gActor113100PierceCarradineBody;
void             func_actor_113100_80132E98(Task*);

s32 func_actor_113100_80132790(Task*, s32, s32, s32);
s32 func_actor_113100_801328EC(Task* task, s32 msgId, ActorTransform* place, ActorMotionWalkAnim*);
s32 func_actor_113100_801331E8(Task*, s32, AnimationPlayRequest*, s32);
s32 func_actor_113100_801333B8(Task* task, s32 msgId, ActorCommand* msg, s32 arg3);

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

void             func_actor_113100_80132AD8(Task*);
void             func_actor_113100_80132C9C(Task*);
void             func_actor_113100_80132E98(Task*);
static TmdSource _gActor113100Model07BC4;
static TmdSource _gActor113100Model07960;
static TmdSource _gActor113100Model07AB4;

TaskDesc D_actor_113100_80144308[4] = {
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_113100_80132E98, { .model = &_gActor113100PierceCarradineBody } },
    { { { TASK_BODY_TMD, 192 } }, func_actor_113100_80132AD8, { .model = &_gActor113100Model07BC4 } },
    { { { TASK_BODY_TMD, 192 } }, func_actor_113100_80132C9C, { .model = &_gActor113100Model07960 } },
    { { { TASK_BODY_TMD, 192 } }, func_actor_113100_80132C9C, { .model = &_gActor113100Model07AB4 } },
};

TaskMessageEntry D_actor_113100_80144338[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, func_actor_113100_801331E8 },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_113100_80132790 },
    { ACTOR_MESSAGE_WALK_TO, func_actor_113100_801328EC },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_113100_801333B8 },
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
        work->billboardTask = Task_SpawnFromTable(D_actor_113100_80144308, 1, 8, task);
    }

    child2 = Task_SpawnFromTable(D_actor_113100_80144308, 2, 4, task);
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
        entry2                    = gpAreaPlaceAt(Gp_GetNestedAreaRec(&key)->placements, index2);
        model2->texturePageOffset = entry2->texturePageOffset;
        model2->clutRowOffset     = entry2->clutRowOffset;
        if (model2->buffer != NULL) {
            tmdProcessStream(model2);
            tmdProcessStream(model2);
        }
    }

    child3 = Task_SpawnFromTable(D_actor_113100_80144308, 3, 2, task);
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
        entry3                    = gpAreaPlaceAt(Gp_GetNestedAreaRec(&key)->placements, index3);
        model3->texturePageOffset = entry3->texturePageOffset;
        model3->clutRowOffset     = entry3->clutRowOffset;
        if (model3->buffer != NULL) {
            tmdProcessStream(model3);
            tmdProcessStream(model3);
        }
    }

    func_actor_113100_80132F24(task);

    obj                   = &work->body;
    obj->coord            = &task->extra.tmd->coords[1];
    obj->context.contacts = work->contacts;
    obj->key              = 0x30000;
    obj->radius           = 0x100;
    obj->pos.vx           = 0;
    obj->pos.vy           = 0;
    obj->pos.vz           = 0;
    obj->flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, obj);
    obj->flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    Gp_InitRec18Table(obj->context.contacts, ARRAY_SIZE(work->contacts), 0);

    task->msgTable = D_actor_113100_80144338;
    func_mist_parking_80183BAC(1);
    func_actor_113100_80132790(task, 0, 0, 0);
    task->exitCallback = func_actor_113100_80132EF0;
    task->state       += 1;
}

/// Per-frame tick of the actor's live state. While the model is not deferred
/// (bit 0x80 of `TmdObject::flags`) it rebuilds part 1's world matrix and
/// draws the ground shadow under that part. `gSceneCombatState.actorControl` gates the rest: a
/// nonzero value skips it. The live path dispatches `func_actor_113100_80132F40`
/// or `func_actor_113100_80132FB4` from a two-entry stack table indexed by
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
    TaskFunc                         funcs[2] = { func_actor_113100_80132F40, func_actor_113100_80132FB4 };
    VECTOR3                          pos;
    GfxCoord*                        coord;
    const AnimationRecord*           rec;
    s32                              i;
    s32                              snd;
    s8                               mode;
    u16                              rate;

    if (!(extra->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        task->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
        Gp_UpdateCoord(&task->extra.tmd->coords[1]);
        if (func_800EA1A8(MATRIX_TRANS(&task->extra.tmd->coords[1].workm), &pos) != 0) {
            Gp_DrawEffGroundQuad(&pos, 0x200, gRoomEffectState->groundShadowShade);
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
            rec = Gp_AnimGetRec(&work->rig.anim, &work->rig.slots[1]);
            if (rec != NULL) {
                if (rec->flags & ANIMATION_RECORD_CUE_2) {
                    snd = 0x5113000F;
                    if (gGameSession->location.loc.view == 0x10) {
                        snd = 0x51130013;
                    }
                    SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                }
                if ((rec->flags & ANIMATION_RECORD_CUE_1) && (gGameSession->location.loc.view != 0x10)) {
                    SndEvt_EnqueueType6(SOUND_MIST_PARKING_PIERCE_STEP_2, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                }
            }
        }
        if (!(extra->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
            Gp_ClearRec18Occupied(work->contacts);
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
            func_800B0928(task, gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), 0x200, 0x100, work->turnWeight);
        }
        if (gGameSession->viewReady != 0) {
            task->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(&task->extra.tmd->coords[1]);
            func_800D7A9C(extra, (VECTOR*)task->extra.tmd->coords[1].workm.t, 0, 3);
        }
        if (work->freeCountdown >= 0) {
            if (work->freeCountdown == 0) {
                Tmd_FreeBuffers(extra);
            }
            work->freeCountdown--;
        }
    }
}

/// Per-frame turn handler, one of the four bodies `func_actor_113100_80132FB4`
/// dispatches through `D_actor_113100_80131E48`. It recovers the root
/// coordinate's yaw from its 3x3 (`m[0][2]` over `m[2][2]`) and compares it
/// with the heading the work block latched in `walkYaw`: more than 0x41 away
/// it turns the root coordinate 0x40 toward `walkYaw` and only re-splats the
/// identity 3x3, within 0x41 it turns the root coordinate to `walkYaw` and then
/// rotates the local forward offset (0, 0, 0x200000) into `walk.velocity` with
/// `ApplyMatrixLV`, raises the three halves at `walk.lastDistance` to 0x7FFF and
/// publishes preset 0x7D3. Both arms clear `GfxCoord::composeStamp` -- the node's
/// recompute bit -- and end at the same epilogue.
///
/// `yaw` carries two different values on purpose: it holds the work block's
/// `walkYaw` for the comparison, and the snapped heading on the turn arm.
/// One variable for both is what puts the snapped value in `$a0` -- the
/// pseudo then spans the whole body, so `$v0` (written by both `ratan2` and
/// the identity constant) is denied it and the `(s16)angle` temporary takes
/// `$v0` instead. `words` and `turnWords` are likewise two pointers rather
/// than one: a single `words` would make the turn arm and the normal arm share
/// a pseudo, which lengthens its life across the branch and adds a copy.
static void func_actor_113100_801324DC(Task* task)
{
    _Actor113100PierceCarradineWork* work;
    GfxCoord*                        coord;
    GfxRotationWords*                words;
    GfxRotationWords*                turnWords;
    VECTOR                           delta;
    AnimationPlayRequest             preset;
    s32                              angle;
    s32                              angle16;
    u16                              yaw;
    s16                              diff;

    coord = task->extra.tmd->coords;
    work  = task->work;
    angle = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]);
    yaw   = work->walkYaw;
    diff  = yaw - angle;
    if (ABS(diff) >= 0x41) {
        angle16 = (s16)angle;
        if (diff < 0) {
            yaw = angle16 - 0x40;
        } else {
            yaw = angle16 + 0x40;
        }
        turnWords         = (GfxRotationWords*)&coord->coord;
        turnWords->m00M01 = ONE;
        turnWords->m02M10 = 0;
        turnWords->m11M12 = ONE;
        turnWords->m20M21 = 0;
        turnWords->m22    = ONE;
        RotMatrixY((s16)yaw, &coord->coord);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
    } else {
        words         = (GfxRotationWords*)&coord->coord;
        words->m00M01 = ONE;
        words->m02M10 = 0;
        words->m11M12 = ONE;
        words->m20M21 = 0;
        words->m22    = ONE;
        RotMatrixY((s16)yaw, &coord->coord);
        delta.vx = 0;
        delta.vy = 0;
        delta.vz = 0x200000;
        ApplyMatrixLV(&coord->coord, &delta, &work->walk.velocity);
        work->walk.lastDistance.vx  = ACTOR_WALK_DISTANCE_NONE;
        work->walk.lastDistance.vy  = ACTOR_WALK_DISTANCE_NONE;
        work->walk.lastDistance.vz  = ACTOR_WALK_DISTANCE_NONE;
        preset.source.index         = 0;
        preset.animationId          = 2;
        preset.blend                = ANIMATION_BLEND_INTERPOLATE;
        preset.blendFrames          = 4;
        preset.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
        func_actor_113100_801331E8(task, ACTOR_MESSAGE_PLAY_ANIMATION, &preset, 0);
        work->walk.motionStep++;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
    }
}

/// One of the four main-body handlers `_Actor113100PierceCarradineWork::walk.motionStep` dispatches
/// through `D_actor_113100_80131E48`. It measures how far the work block's
/// `walk.target.vx` / `walk.target.vz` have drifted from the root coordinate's
/// translation -- each axis as the 16-bit magnitude of the difference, the
/// signed 32-bit subtraction only picking the direction -- and once both
/// magnitudes are no smaller than `walk.lastDistance.vx` / `.vz` it publishes the
/// 0x7D3 preset (`field_4` the animation id, `field_C` 5) and clears the
/// `walk.velocity` vector, bumping `walk.motionStep` on to the next handler. While
/// either is still shrinking it latches the magnitudes back into
/// `walk.lastDistance`, so the pair tracks the distance at the last check.
static void func_actor_113100_8013264C(Task* task)
{
    _Actor113100PierceCarradineWork* work;
    GfxCoord*                        coord;
    SVECTOR                          d;
    s32                              dx;
    s32                              dz;
    AnimationPlayRequest             preset;

    work  = task->work;
    coord = task->extra.tmd->coords;
    if (work->walk.target.vx - coord->coord.t[0] >= 0) {
        dx = (u16)work->walk.target.vx - (u16)coord->coord.t[0];
    } else {
        dx = (u16)coord->coord.t[0] - (u16)work->walk.target.vx;
    }
    d.vx = dx;
    if (work->walk.target.vz - coord->coord.t[2] >= 0) {
        dz = (u16)work->walk.target.vz - (u16)coord->coord.t[2];
    } else {
        dz = (u16)coord->coord.t[2] - (u16)work->walk.target.vz;
    }
    d.vz = dz;
    if (d.vx >= work->walk.lastDistance.vx && d.vz >= work->walk.lastDistance.vz) {
        preset.source.index         = 0;
        preset.animationId          = work->model.nextAnimId;
        preset.blend                = ANIMATION_BLEND_INTERPOLATE;
        preset.blendFrames          = 5;
        preset.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
        func_actor_113100_801331E8(task, ACTOR_MESSAGE_PLAY_ANIMATION, &preset, 0);
        work->walk.velocity.vx = 0;
        work->walk.velocity.vy = 0;
        work->walk.velocity.vz = 0;
        work->walk.motionStep++;
        return;
    }
    work->walk.lastDistance.vx = d.vx < 0 ? -d.vx : d.vx;
    work->walk.lastDistance.vz = d.vz < 0 ? -d.vz : d.vz;
}

/// The 0x7D5 entry of `D_actor_113100_80144338`: the visibility control the
/// setup handler and `func_actor_113100_80132F40` drive. This one takes its
/// payload as a mode word rather than a pointer -- both call sites pass a
/// literal, and a mode outside 0..3 is answered with 1, the "not handled"
/// return the dispatch expects. All four modes lift the 0x8000 bit the setup
/// handler raised on the work block's collision `body` and then rewrite the
/// actor's own `TmdObject::flags`, whose `TMD_OBJECT_SKIP_ACTIVE_DRAW` bit
/// excludes active drawing and whose 0x4 is the flag `Tmd_Create` seeds from `flags & 1`:
/// mode 0 shows the model and clears 0x4; mode 1 hides it, hands the object to
/// `Tmd_AllocBuffers` and clears 0x4; mode 2 hides it, latches 2 into
/// `freeCountdown` -- the countdown `func_actor_113100_80132104` walks down to
/// `Tmd_FreeBuffers` -- and raises 0x4; mode 3 shows it and raises 0x4.
///
/// `work` and `work2` are the same `Task::work` read twice. The second read
/// becomes a register copy at the entry, which is what leaves the block in
/// `$v1` for the node base mode 3 folds out of `work` while the hoisted `head`
/// and the `freeCountdown` latch run off the copy in `$a1`; one read and one local
/// for the node instead collapses all four arms onto a single register
/// (98.517%).
s32 func_actor_113100_80132790(Task* task, s32 msgId, s32 mode, s32 arg3)
{
    _Actor113100PierceCarradineWork* work;
    _Actor113100PierceCarradineWork* work2;
    WorldCollisionBody*              head;
    WorldCollisionBody*              node;
    TmdObject*                       obj;
    s32                              i;
    s32                              ret;

    work  = task->work;
    obj   = task->extra.tmd;
    work2 = task->work;
    head  = &work2->body;
    ret   = 0;

    switch (mode) {
        case 0:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            node        = head;
            for (i = 0; i <= 0; i++) {
                node->flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                node++;
            }
            obj->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 1:
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            node        = head;
            for (i = 0; i <= 0; i++) {
                node->flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                node++;
            }
            Tmd_AllocBuffers(obj);
            obj->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 2:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            node        = head;
            for (i = 0; i <= 0; i++) {
                node->flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                node++;
            }
            work2->freeCountdown = 2;
            obj->flags          |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 3:
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            node        = &work->body;
            for (i = 0; i <= 0; i++) {
                node->flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                node++;
            }
            obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}

/// The 0x7DD entry of `D_actor_113100_80144338`: the placement command. It
/// latches its payload's position and rotation into the work block, flags the
/// actor as placed through `walk.motion` / `walk.motionStep`, then applies the start
/// preset in place -- the body of the 0x7D3 handler
/// `func_actor_113100_801331E8` written out inline against a preset built on
/// this function's own stack from `anim->animationId` and `anim->nextAnimId`.
s32 func_actor_113100_801328EC(Task* task, s32 msgId, ActorTransform* place, ActorMotionWalkAnim* anim)
{
    _Actor113100PierceCarradineWork* work;
    _Actor113100PierceCarradineWork* w;
    AnimationPlayRequest             preset;
    AnimationPlayRequest*            msg;
    s32                              i;
    TmdObject*                       ext;

    w                    = task->work;
    w->walk.motion       = ACTOR_WALK_MOTION_WALKING;
    w->walk.motionStep   = 0;
    w->walk.target.vx    = place->pos.vx;
    w->walk.target.vy    = place->pos.vy;
    w->walk.target.vz    = place->pos.vz;
    w->walk.targetRot.vx = place->rot.vx;
    w->walk.targetRot.vy = place->rot.vy;
    w->walk.targetRot.vz = place->rot.vz;
    preset.source.index  = 0;
    if (anim != NULL) {
        preset.animationId  = anim->animationId;
        w->model.nextAnimId = anim->nextAnimId;
    } else {
        preset.animationId  = 2;
        w->model.nextAnimId = 1;
    }
    preset.blend                = ANIMATION_BLEND_INTERPOLATE;
    preset.blendFrames          = 5;
    preset.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;

    msg  = &preset;
    work = task->work;
    ext  = task->extra.tmd;
    if (msg->source.index != work->model.bank) {
        work->model.bank   = msg->source.index;
        work->model.animId = ACTOR_MODEL_STATE_NONE;
        animationInitContext(&work->rig.anim, D_actor_113100_801442E0[work->model.bank], ext, work->rig.poses,
                             work->rig.slots);
    }
    if (msg->animationId != work->model.animId) {
        work->model.animId = msg->animationId;
        if (msg->blend != ANIMATION_BLEND_RESET && work->model.ticking != 0) {
            for (i = 1; i < 0x14; i++) {
                animationSeekSlotWithBlend(&work->rig.anim, i, work->model.animId, 0, msg->blendFrames);
            }
        } else {
            for (i = 1; i < 0x14; i++) {
                animationResetSlot(&work->rig.anim, i, work->model.animId);
            }
        }
        for (i = 1; i < 0x14; i++) {
            animationTickSlot(&work->rig.anim, i);
        }
        work->model.ticking = 1;
    }
    work->turnUp = D_actor_113100_801442E4[msg->animationId];
    return 0;
}

void func_actor_113100_80132AD8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_113100_80131E24;
    sp.funcs[task->state](task);
}

static void func_actor_113100_80132B30(Task* task)
{
    TmdObject* model;
    Task*      parent;
    s32        index;
    GfxCoord*  node;
    GfxCoord*  part;

    model  = task->extra.tmd;
    parent = task->spawnArg2.pointer;
    index  = task->spawnArg1.value;
    node   = model->coords;
    part   = parent->extra.tmd->coords;

    node->coord.t[0]   = 0;
    node->coord.t[1]   = 0x64;
    node->coord.t[2]   = 0;
    node->composeStamp = GRAPHICS_COORD_DIRTY;
    node->parent       = &part[index];

    taskReparent(parent, task);
    if (GameFlag_GetNibble(GAME_FLAG_0F1) == 0) {
        model->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
    } else {
        model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    task->state += 1;
}

/// Builds the display matrix of the modelled part this actor is posed on.
/// `spawnArg1` indexes the part in the model task's coordinate array: the part's
/// `workm` is transposed into the actor coordinate, the stage view is multiplied
/// in, and the part's X euler angle is applied, after which the coordinate's
/// update flag is cleared so the GTE sees the new matrix.
static void func_actor_113100_80132BDC(Task* task)
{
    MATRIX    sp10;
    SVECTOR   sp30;
    MATRIX*   view;
    MATRIX*   coord;
    GfxCoord* part;
    GfxCoord* node;
    s32       index;

    index = task->spawnArg1.value;
    node  = task->extra.tmd->coords;
    part  = &((Task*)task->spawnArg2.pointer)->extra.tmd->coords[index];
    view  = &Gp_GetStageView(&gGameSession->location.loc)->transform;
    coord = &node->coord;
    TransposeMatrix(&part->workm, coord);
    TransposeMatrix(view, &sp10);
    MulMatrix0(coord, &sp10, coord);
    gfxExtractSmallestEuler(&sp30, &part->coord);
    RotMatrixX(sp30.vy, coord);
    node->composeStamp = GRAPHICS_COORD_DIRTY;
}

void func_actor_113100_80132C9C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_113100_80131E30;
    sp.funcs[task->state](task);
}

#include "../../shared/model_placement_attach.inc.c"

#include "../../shared/model_placement_mirror_parent.inc.c"

void func_actor_113100_80132E98(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_113100_80131E3C;
    sp.funcs[task->state](task);
}

/// The task's exit callback: it unlinks the work block's collision `body` and
/// destroys the task.
static void func_actor_113100_80132EF0(Task* arg0)
{
    Gp_UnlinkObj(&((_Actor113100PierceCarradineWork*)arg0->work)->body);
    enemyTaskExit(arg0);
}

/// Points the model's light and colour matrices at the work block's own
/// `model.light` / `model.color` pair; the setup handler calls it once.
static void func_actor_113100_80132F24(Task* task)
{
    TmdObject*                       ext;
    _Actor113100PierceCarradineWork* work;

    ext           = task->extra.tmd;
    work          = task->work;
    ext->lightMtx = &work->model.light;
    ext->colorMtx = &work->model.color;
}

static void func_actor_113100_80132F40(Task* arg0)
{
    _Actor113100PierceCarradineWork* work;
    s32                              flag;

    work = arg0->work;
    flag = GameFlag_GetNibble(GAME_FLAG_0ED);
    if (flag > 0 && work->lastAppearFlag == 0) {
        func_actor_113100_80132790(arg0, 0, 1, 0);
        func_mist_parking_80183BAC(0);
    }
    work->lastAppearFlag = flag;
}

/// Dispatches the actor's four main-body handlers by the animation slot index
/// `walk.motionStep` counts up in `func_actor_113100_8013301C`.
static void func_actor_113100_80132FB4(Task* arg0)
{
    _Actor113100PierceCarradineWork* work;
    TaskFuncTable4                   handlers;

    work     = arg0->work;
    handlers = D_actor_113100_80131E48;
    handlers.funcs[work->walk.motionStep](arg0);
}

/// Builds the offset from the root part's coordinate translation to
/// `walk.target` and stores its yaw into `walkYaw`,
/// then dispatches animation preset 0x7D3 through `func_actor_113100_801331E8`
/// and counts the frame. The preset is built on this function's stack: it
/// carries the slot index, the animation id and the two per-slot arguments.
///
/// `preset` is declared before `delta` / `dir` on purpose -- the stack slots
/// land at 0x10, 0x28 and 0x38 only in that order (GCC assigns the frame in
/// declaration order, and the 16-byte `VECTOR` is 8-byte aligned).
static void func_actor_113100_8013301C(Task* arg0)
{
    _Actor113100PierceCarradineWork* work;
    GfxCoord*                        coord;
    AnimationPlayRequest             preset;
    VECTOR                           delta;
    SVECTOR                          dir;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;

    delta.vx = work->walk.target.vx - coord->coord.t[0];
    delta.vy = work->walk.target.vy - coord->coord.t[1];
    delta.vz = work->walk.target.vz - coord->coord.t[2];
    VectorNormalS(&delta, &dir);
    work->walkYaw = ratan2(dir.vx, dir.vz);

    preset.source.index         = 0;
    preset.animationId          = 0x16;
    preset.blend                = ANIMATION_BLEND_INTERPOLATE;
    preset.blendFrames          = 4;
    preset.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
    func_actor_113100_801331E8(arg0, ACTOR_MESSAGE_PLAY_ANIMATION, &preset, 0);
    work->walk.motionStep++;
}

static void func_actor_113100_801330E8(Task* arg0)
{
    _Actor113100PierceCarradineWork* work;
    GfxRotationWords*                words;
    GfxCoord*                        coord;
    SVECTOR                          vec;
    AnimationPlayRequest             preset;
    s32                              vy;
    s16                              diff;

    coord = arg0->extra.tmd->coords;
    work  = arg0->work;

    gfxExtractSmallestEuler(&vec, &coord->coord);
    diff = (u16)work->walk.targetRot.vy - (u16)vec.vy;
    if (ABS(diff) >= 0x41) {
        vy = vec.vy;
        if (diff < 0) {
            vec.vy = vy - 0x40;
        } else {
            vec.vy = vy + 0x40;
        }
    } else {
        vec.vy                      = work->walk.targetRot.vy;
        preset.source.index         = 0;
        preset.animationId          = work->model.nextAnimId;
        preset.blend                = ANIMATION_BLEND_INTERPOLATE;
        preset.blendFrames          = 5;
        preset.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
        func_actor_113100_801331E8(arg0, ACTOR_MESSAGE_PLAY_ANIMATION, &preset, 0);
        work->walk.motion     = ACTOR_WALK_MOTION_IDLE;
        work->walk.motionStep = 0;
    }

    words         = (GfxRotationWords*)&coord->coord;
    words->m00M01 = ONE;
    words->m02M10 = 0;
    words->m11M12 = ONE;
    words->m20M21 = 0;
    words->m22    = ONE;
    RotMatrix(&vec, &coord->coord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Selects this actor's animation bank and clip, then updates its animation mode.
///
/// A bank change invalidates the previous clip. A changed clip blends only
/// when requested and the rig is already ticking; otherwise it resets.
s32 func_actor_113100_801331E8(Task* task, s32 msgId, AnimationPlayRequest* preset, s32 arg3)
{
    _Actor113100PierceCarradineWork* work;
    TmdObject*                       ext;
    s32                              i;

    work = task->work;
    ext  = task->extra.tmd;
    if (preset->source.index != work->model.bank) {
        work->model.bank   = preset->source.index;
        work->model.animId = ACTOR_MODEL_STATE_NONE;
        animationInitContext(&work->rig.anim, D_actor_113100_801442E0[work->model.bank], ext, work->rig.poses,
                             work->rig.slots);
    }
    if (preset->animationId != work->model.animId) {
        work->model.animId = preset->animationId;
        if (preset->blend != ANIMATION_BLEND_RESET && work->model.ticking != 0) {
            for (i = 1; i < 0x14; i++) {
                animationSeekSlotWithBlend(&work->rig.anim, i, work->model.animId, 0, preset->blendFrames);
            }
        } else {
            for (i = 1; i < 0x14; i++) {
                animationResetSlot(&work->rig.anim, i, work->model.animId);
            }
        }
        for (i = 1; i < 0x14; i++) {
            animationTickSlot(&work->rig.anim, i);
        }
        work->model.ticking = 1;
    }
    work->turnUp = D_actor_113100_801442E4[preset->animationId];
    return 0;
}

#include "../../shared/actor_messages_place_euler.inc.c"

/// Message 0x7DB handler, listed in `D_actor_113100_80144338` after the 0x7D3 /
/// 0x7D5 / 0x7DD ones. The payload halfword selects one of four actions: 0 and
/// 1 clear and raise `TMD_OBJECT_SKIP_ACTIVE_DRAW` in the child task's
/// `TmdObject::flags`, enabling and excluding active drawing; 2 and 3 set the
/// work block's `turnUp` to 1 and 0. Nothing reads the opcode
/// itself, hence `msgId`.
s32 func_actor_113100_801333B8(Task* task, s32 msgId, ActorCommand* msg, s32 arg3)
{
    _Actor113100PierceCarradineWork* work;
    TmdObject*                       model;

    work = task->work;

    switch (msg->command) {
        case 0:
            if (work->billboardTask != NULL) {
                model         = work->billboardTask->extra.tmd;
                model->flags &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            break;

        case 1:
            if (work->billboardTask != NULL) {
                model         = work->billboardTask->extra.tmd;
                model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
            break;

        case 2:
            work->turnUp = 1;
            break;

        case 3:
            work->turnUp = 0;
            break;

        default:
            return 0;
    }
    return 0;
}
