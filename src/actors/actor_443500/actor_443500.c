#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/area_transitions.h"
#include "gameplay/areaplace.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
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

#include "rooms/shelter_r47.h"
#include "../../shared/model_placement.h"
#include "../../shared/actor_messages.h"

/// Tick of each pass of the default clip on which the clip's sound is started.
enum { ACTOR_443500_PIERCE_CARRADINE_LOOP_SOUND_TICK = 15 };

/// Work block of Pierce Carradine in the shelter's room 47.
///
/// The actor's task allocates it zeroed in its spawn state and keeps it at
/// `Task::work` for the task's life. It opens with the rig and the model state
/// a twenty-part actor that only plays clips keeps (`ActorMotionPlayWork`
/// names the pair); the package's own play handler runs on them, and the model
/// object borrows `model.light` and `model.color` for as long as the block
/// lives.
///
/// What follows is the package's own: the timing of the default clip's sound,
/// the delayed free of the model's buffers once the model has been hidden, and
/// the model's flags as kept across the camera views that hide it.
typedef struct {
    ActorAnimRig20  rig;              // Playback storage of the twenty-part body model; slots 1 to 19 are driven
    ActorModelState model;            // Clip and bank the rig plays, and the matrices the model is lit with
    byte            unknown_4B8[0x2]; // Never accessed; role unproven
    s16             loopTicks;        // Ticks the default clip has played since a play request or the clip's loop jump last zeroed it; the tick that takes it to `ACTOR_443500_PIERCE_CARRADINE_LOOP_SOUND_TICK` starts the clip's sound
    s32             freeCountdown;    // Ticks left before the model's buffers are freed, which the tick finding 0 does (-1 no free pending)
    u32             savedModelFlags;  // The model's `TmdObject::flags` as last recorded: at spawn, after each draw-mode message, and, while no event holds the scene, on each tick in camera views 0 to 3 before that tick hides the model; written back to the model on such a tick in views 4 and 5
} _Actor443500PierceCarradineWork;
STATIC_ASSERT_SIZEOF(_Actor443500PierceCarradineWork, 0x4C4);

static void func_actor_443500_80132078(Task* task);
static void func_actor_443500_801321F0(Task* task);
static void func_actor_443500_801327A4(Task* arg0);
static void func_actor_443500_801327C4(Task* task);
s32         func_actor_443500_801327E0(Task* task, s32 anim, AnimationPlayRequest* params, s32 arg3);
s32         func_actor_443500_8013297C(Task* task, s32 anim, s32 mode, s32 arg3);
static void func_actor_443500_80132A68(s32 arg0);

/// State table of the actor's child task (`TaskDesc` entry 1): setup, the
/// per-frame flag mirror and `taskKill`.
static const TaskFuncTable3 D_actor_443500_80131E24 = {
    { modelPlacementAttachChild, modelPlacementMirrorParent, taskKill }
};

/// State table of the actor's main task (`TaskDesc` entry 0): the spawn
/// handler, the per-frame tick and the exit callback.
static const TaskFuncTable3 D_actor_443500_80131E30 = {
    { func_actor_443500_80132078, func_actor_443500_801321F0, func_actor_443500_801327A4 }
};

extern TaskDesc D_actor_443500_80140E38;

/// Default animation arguments, 0x14 bytes: `{ NULL, 0x1C, 1, 4, 0 }`.
extern AnimationPlayRequest D_actor_443500_80158728;

/// The actor's two-entry `TaskDesc` table; the spawn handler starts entry 1.
extern TaskDesc D_actor_443500_8015873C[];

/// The actor's animation table: `(anim id, handler)` pairs for 0x7D3 / 0x7D4 /
/// 0x7D5, ended by `TASK_MESSAGE_TABLE_END`. The spawn handler parks its address in
/// `Task::msgTable` (0x24).
// Message-table callbacks use the argument views required by this TU.

extern TaskMessageEntry D_actor_443500_80158754[4];

/// The bank table `func_actor_443500_801327E0` re-seeds the work block's slot
/// array off: one entry, the animation bank the default preset's `field_0` of
/// zero selects.
extern AnimationSet*  D_actor_443500_80158694[36];
extern AnimationSet** D_actor_443500_80158724[1];

extern WorldCollisionGrid D_actor_443500_801587D8;

static TmdSource _gActor443500PierceCarradineBody;
void             func_actor_443500_80132738(Task*);

static WorldCollisionGridFace _gActor443500Collision269B8Faces[2];
static SVECTOR                _gActor443500Collision269B8Normals[2];
static SVECTOR                _gActor443500Collision269B8Verts[6];

extern AnimationPlayRequest     D_actor_443500_80140E8C;
extern AnimationPlayRequest     D_actor_443500_80140EA0;
extern AnimationPlayRequest     D_actor_443500_80140EB4;
extern AnimationPlayRequest     D_actor_443500_80140EC8;
extern AnimationPlayRequest     D_actor_443500_80140EDC;
extern AnimationPlayRequest     D_actor_443500_80140EF0;
extern AnimationPlayRequest     D_actor_443500_80140F04;
extern AnimationPlayRequest     D_actor_443500_80140F18;
extern AnimationPlayRequest     D_actor_443500_80140F2C;
extern AnimationPlayRequest     D_actor_443500_80140F40;
extern AnimationPlayRequest     D_actor_443500_80141004;
extern AnimationPlayRequest     D_actor_443500_80141018;
extern AnimationPlayRequest     D_actor_443500_8014102C;
extern AnimationPlayRequest     D_actor_443500_80141040;
extern AnimationPlayRequest     D_actor_443500_80141054;
extern AnimationPlayRequest     D_actor_443500_80141068;
extern AnimationPlayRequest     D_actor_443500_80141090;
extern AnimationPlayRequest     D_actor_443500_801410A4;
extern AnimationPlayRequest     D_actor_443500_801410B8;
extern AnimationPlayRequest     D_actor_443500_801410CC;
extern AnimationPlayRequest     D_actor_443500_801410E0;
extern AnimationPlayRequest     D_actor_443500_801410F4;
extern AnimationPlayRequest     D_actor_443500_80141108;
extern AnimationPlayRequest     D_actor_443500_8014111C;
extern AnimationPlayRequest     D_actor_443500_80141130;
extern AnimationPlayRequest     D_actor_443500_80141144;
extern AnimationPlayRequest     D_actor_443500_80141158;
extern AnimationPlayRequest     D_actor_443500_8014116C;
extern AnimationPlayRequest     D_actor_443500_80141180;
extern AnimationPlayRequest     D_actor_443500_801411A8;
extern AnimationPlayRequest     D_actor_443500_801411BC;
extern AnimationPlayRequest     D_actor_443500_801411D0;
extern AnimationBankCopyRequest D_actor_443500_80140E70;
extern AnimationBankCopyRequest D_actor_443500_80140FE8;
extern ActorTransform           D_actor_443500_80140F54;
extern ActorTransform           D_actor_443500_80140F6C;
extern ActorTransform           D_actor_443500_801411E4;
extern ActorTransform           D_actor_443500_801411FC;
void                            func_actor_443500_80131E3C(s32);
void                            func_actor_443500_80131E84(s32);
void                            func_actor_443500_80131EE4(void);
void                            func_actor_443500_80131F18(void);
void                            func_actor_443500_80131F58(void);
void                            func_actor_443500_8013201C(s16);
void                            func_actor_443500_80132048(void);
void                            func_actor_443500_8013206C(s8);

static AnimationSet _gActor443500Animation010D0;
static AnimationSet _gActor443500Animation026FC;
static AnimationSet _gActor443500Animation0321C;
static AnimationSet _gActor443500Animation03D34;
static AnimationSet _gActor443500Animation03F54;
static AnimationSet _gActor443500Animation049F8;
static AnimationSet _gActor443500Animation04C48;
static AnimationSet _gActor443500Animation05074;
static AnimationSet _gActor443500Animation05570;
static AnimationSet _gActor443500Animation06034;
static AnimationSet _gActor443500Animation062BC;
static AnimationSet _gActor443500Animation065D8;
static AnimationSet _gActor443500Animation0771C;
static AnimationSet _gActor443500Animation083E8;
static AnimationSet _gActor443500Animation086BC;
static AnimationSet _gActor443500Animation08E08;
static AnimationSet _gActor443500Animation09350;
static AnimationSet _gActor443500Animation09A00;
static AnimationSet _gActor443500Animation0DB60;
static AnimationSet _gActor443500Animation0DF34;
static AnimationSet _gActor443500Animation0E324;
static AnimationSet _gActor443500Animation0E690;
static AnimationSet _gActor443500Animation0EC10;
static AnimationSet _gActor443500Animation0EFF0;

void func_actor_443500_80131F88(Task*);

static AnimationPackedPose _gActor443500Animation010D0Bank1[6] = {
#include "assets/actor_443500_animation_010D0_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation010D0Bank4[46] = {
#include "assets/actor_443500_animation_010D0_bank4.inc"
};

static AnimationRecord _gActor443500Animation010D0Records[109] = {
#include "assets/actor_443500_animation_010D0_records.inc"
};

static u16 _gActor443500Animation010D0Indices[20] = {
#include "assets/actor_443500_animation_010D0_indices.inc"
};

static AnimationSet _gActor443500Animation010D0 = {
    _gActor443500Animation010D0Records,
    _gActor443500Animation010D0Indices,
    { NULL, _gActor443500Animation010D0Bank1, NULL, NULL, _gActor443500Animation010D0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation026FCBank1[53] = {
#include "assets/actor_443500_animation_026FC_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation026FCBank4[561] = {
#include "assets/actor_443500_animation_026FC_bank4.inc"
};

static AnimationRecord _gActor443500Animation026FCRecords[679] = {
#include "assets/actor_443500_animation_026FC_records.inc"
};

static u16 _gActor443500Animation026FCIndices[20] = {
#include "assets/actor_443500_animation_026FC_indices.inc"
};

static AnimationSet _gActor443500Animation026FC = {
    _gActor443500Animation026FCRecords,
    _gActor443500Animation026FCIndices,
    { NULL, _gActor443500Animation026FCBank1, NULL, NULL, _gActor443500Animation026FCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation0321CBank1[10] = {
#include "assets/actor_443500_animation_0321C_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation0321CBank4[295] = {
#include "assets/actor_443500_animation_0321C_bank4.inc"
};

static AnimationRecord _gActor443500Animation0321CRecords[367] = {
#include "assets/actor_443500_animation_0321C_records.inc"
};

static u16 _gActor443500Animation0321CIndices[20] = {
#include "assets/actor_443500_animation_0321C_indices.inc"
};

static AnimationSet _gActor443500Animation0321C = {
    _gActor443500Animation0321CRecords,
    _gActor443500Animation0321CIndices,
    { NULL, _gActor443500Animation0321CBank1, NULL, NULL, _gActor443500Animation0321CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation03D34Bank1[5] = {
#include "assets/actor_443500_animation_03D34_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation03D34Bank4[300] = {
#include "assets/actor_443500_animation_03D34_bank4.inc"
};

static AnimationRecord _gActor443500Animation03D34Records[375] = {
#include "assets/actor_443500_animation_03D34_records.inc"
};

static u16 _gActor443500Animation03D34Indices[20] = {
#include "assets/actor_443500_animation_03D34_indices.inc"
};

static AnimationSet _gActor443500Animation03D34 = {
    _gActor443500Animation03D34Records,
    _gActor443500Animation03D34Indices,
    { NULL, _gActor443500Animation03D34Bank1, NULL, NULL, _gActor443500Animation03D34Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation03F54Bank1[4] = {
#include "assets/actor_443500_animation_03F54_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation03F54Bank4[20] = {
#include "assets/actor_443500_animation_03F54_bank4.inc"
};

static AnimationRecord _gActor443500Animation03F54Records[84] = {
#include "assets/actor_443500_animation_03F54_records.inc"
};

static u16 _gActor443500Animation03F54Indices[20] = {
#include "assets/actor_443500_animation_03F54_indices.inc"
};

static AnimationSet _gActor443500Animation03F54 = {
    _gActor443500Animation03F54Records,
    _gActor443500Animation03F54Indices,
    { NULL, _gActor443500Animation03F54Bank1, NULL, NULL, _gActor443500Animation03F54Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation049F8Bank1[4] = {
#include "assets/actor_443500_animation_049F8_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation049F8Bank4[273] = {
#include "assets/actor_443500_animation_049F8_bank4.inc"
};

static AnimationRecord _gActor443500Animation049F8Records[376] = {
#include "assets/actor_443500_animation_049F8_records.inc"
};

static u16 _gActor443500Animation049F8Indices[20] = {
#include "assets/actor_443500_animation_049F8_indices.inc"
};

static AnimationSet _gActor443500Animation049F8 = {
    _gActor443500Animation049F8Records,
    _gActor443500Animation049F8Indices,
    { NULL, _gActor443500Animation049F8Bank1, NULL, NULL, _gActor443500Animation049F8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation04C48Bank1[2] = {
#include "assets/actor_443500_animation_04C48_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation04C48Bank4[27] = {
#include "assets/actor_443500_animation_04C48_bank4.inc"
};

static AnimationRecord _gActor443500Animation04C48Records[95] = {
#include "assets/actor_443500_animation_04C48_records.inc"
};

static u16 _gActor443500Animation04C48Indices[20] = {
#include "assets/actor_443500_animation_04C48_indices.inc"
};

static AnimationSet _gActor443500Animation04C48 = {
    _gActor443500Animation04C48Records,
    _gActor443500Animation04C48Indices,
    { NULL, _gActor443500Animation04C48Bank1, NULL, NULL, _gActor443500Animation04C48Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation05074Bank1[2] = {
#include "assets/actor_443500_animation_05074_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation05074Bank4[99] = {
#include "assets/actor_443500_animation_05074_bank4.inc"
};

static AnimationRecord _gActor443500Animation05074Records[142] = {
#include "assets/actor_443500_animation_05074_records.inc"
};

static u16 _gActor443500Animation05074Indices[20] = {
#include "assets/actor_443500_animation_05074_indices.inc"
};

static AnimationSet _gActor443500Animation05074 = {
    _gActor443500Animation05074Records,
    _gActor443500Animation05074Indices,
    { NULL, _gActor443500Animation05074Bank1, NULL, NULL, _gActor443500Animation05074Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation05570Bank1[4] = {
#include "assets/actor_443500_animation_05570_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation05570Bank4[128] = {
#include "assets/actor_443500_animation_05570_bank4.inc"
};

static AnimationRecord _gActor443500Animation05570Records[159] = {
#include "assets/actor_443500_animation_05570_records.inc"
};

static u16 _gActor443500Animation05570Indices[20] = {
#include "assets/actor_443500_animation_05570_indices.inc"
};

static AnimationSet _gActor443500Animation05570 = {
    _gActor443500Animation05570Records,
    _gActor443500Animation05570Indices,
    { NULL, _gActor443500Animation05570Bank1, NULL, NULL, _gActor443500Animation05570Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation06034Bank1[23] = {
#include "assets/actor_443500_animation_06034_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation06034Bank4[267] = {
#include "assets/actor_443500_animation_06034_bank4.inc"
};

static AnimationRecord _gActor443500Animation06034Records[333] = {
#include "assets/actor_443500_animation_06034_records.inc"
};

static u16 _gActor443500Animation06034Indices[20] = {
#include "assets/actor_443500_animation_06034_indices.inc"
};

static AnimationSet _gActor443500Animation06034 = {
    _gActor443500Animation06034Records,
    _gActor443500Animation06034Indices,
    { NULL, _gActor443500Animation06034Bank1, NULL, NULL, _gActor443500Animation06034Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation062BCBank1[2] = {
#include "assets/actor_443500_animation_062BC_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation062BCBank4[33] = {
#include "assets/actor_443500_animation_062BC_bank4.inc"
};

static AnimationRecord _gActor443500Animation062BCRecords[103] = {
#include "assets/actor_443500_animation_062BC_records.inc"
};

static u16 _gActor443500Animation062BCIndices[20] = {
#include "assets/actor_443500_animation_062BC_indices.inc"
};

static AnimationSet _gActor443500Animation062BC = {
    _gActor443500Animation062BCRecords,
    _gActor443500Animation062BCIndices,
    { NULL, _gActor443500Animation062BCBank1, NULL, NULL, _gActor443500Animation062BCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation065D8Bank1[2] = {
#include "assets/actor_443500_animation_065D8_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation065D8Bank4[60] = {
#include "assets/actor_443500_animation_065D8_bank4.inc"
};

static AnimationRecord _gActor443500Animation065D8Records[113] = {
#include "assets/actor_443500_animation_065D8_records.inc"
};

static u16 _gActor443500Animation065D8Indices[20] = {
#include "assets/actor_443500_animation_065D8_indices.inc"
};

static AnimationSet _gActor443500Animation065D8 = {
    _gActor443500Animation065D8Records,
    _gActor443500Animation065D8Indices,
    { NULL, _gActor443500Animation065D8Bank1, NULL, NULL, _gActor443500Animation065D8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation0771CBank1[20] = {
#include "assets/actor_443500_animation_0771C_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation0771CBank4[447] = {
#include "assets/actor_443500_animation_0771C_bank4.inc"
};

static AnimationRecord _gActor443500Animation0771CRecords[578] = {
#include "assets/actor_443500_animation_0771C_records.inc"
};

static u16 _gActor443500Animation0771CIndices[20] = {
#include "assets/actor_443500_animation_0771C_indices.inc"
};

static AnimationSet _gActor443500Animation0771C = {
    _gActor443500Animation0771CRecords,
    _gActor443500Animation0771CIndices,
    { NULL, _gActor443500Animation0771CBank1, NULL, NULL, _gActor443500Animation0771CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation083E8Bank1[10] = {
#include "assets/actor_443500_animation_083E8_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation083E8Bank4[331] = {
#include "assets/actor_443500_animation_083E8_bank4.inc"
};

static AnimationRecord _gActor443500Animation083E8Records[438] = {
#include "assets/actor_443500_animation_083E8_records.inc"
};

static u16 _gActor443500Animation083E8Indices[20] = {
#include "assets/actor_443500_animation_083E8_indices.inc"
};

static AnimationSet _gActor443500Animation083E8 = {
    _gActor443500Animation083E8Records,
    _gActor443500Animation083E8Indices,
    { NULL, _gActor443500Animation083E8Bank1, NULL, NULL, _gActor443500Animation083E8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation086BCBank1[2] = {
#include "assets/actor_443500_animation_086BC_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation086BCBank4[40] = {
#include "assets/actor_443500_animation_086BC_bank4.inc"
};

static AnimationRecord _gActor443500Animation086BCRecords[115] = {
#include "assets/actor_443500_animation_086BC_records.inc"
};

static u16 _gActor443500Animation086BCIndices[20] = {
#include "assets/actor_443500_animation_086BC_indices.inc"
};

static AnimationSet _gActor443500Animation086BC = {
    _gActor443500Animation086BCRecords,
    _gActor443500Animation086BCIndices,
    { NULL, _gActor443500Animation086BCBank1, NULL, NULL, _gActor443500Animation086BCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation08E08Bank1[7] = {
#include "assets/actor_443500_animation_08E08_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation08E08Bank4[171] = {
#include "assets/actor_443500_animation_08E08_bank4.inc"
};

static AnimationRecord _gActor443500Animation08E08Records[255] = {
#include "assets/actor_443500_animation_08E08_records.inc"
};

static u16 _gActor443500Animation08E08Indices[20] = {
#include "assets/actor_443500_animation_08E08_indices.inc"
};

static AnimationSet _gActor443500Animation08E08 = {
    _gActor443500Animation08E08Records,
    _gActor443500Animation08E08Indices,
    { NULL, _gActor443500Animation08E08Bank1, NULL, NULL, _gActor443500Animation08E08Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation09350Bank1[2] = {
#include "assets/actor_443500_animation_09350_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation09350Bank4[120] = {
#include "assets/actor_443500_animation_09350_bank4.inc"
};

static AnimationRecord _gActor443500Animation09350Records[192] = {
#include "assets/actor_443500_animation_09350_records.inc"
};

static u16 _gActor443500Animation09350Indices[20] = {
#include "assets/actor_443500_animation_09350_indices.inc"
};

static AnimationSet _gActor443500Animation09350 = {
    _gActor443500Animation09350Records,
    _gActor443500Animation09350Indices,
    { NULL, _gActor443500Animation09350Bank1, NULL, NULL, _gActor443500Animation09350Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation09A00Bank1[29] = {
#include "assets/actor_443500_animation_09A00_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation09A00Bank4[127] = {
#include "assets/actor_443500_animation_09A00_bank4.inc"
};

static AnimationRecord _gActor443500Animation09A00Records[194] = {
#include "assets/actor_443500_animation_09A00_records.inc"
};

static u16 _gActor443500Animation09A00Indices[20] = {
#include "assets/actor_443500_animation_09A00_indices.inc"
};

static AnimationSet _gActor443500Animation09A00 = {
    _gActor443500Animation09A00Records,
    _gActor443500Animation09A00Indices,
    { NULL, _gActor443500Animation09A00Bank1, NULL, NULL, _gActor443500Animation09A00Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation0A4E4Bank1[25] = {
#include "assets/actor_443500_animation_0A4E4_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation0A4E4Bank4[250] = {
#include "assets/actor_443500_animation_0A4E4_bank4.inc"
};

static AnimationRecord _gActor443500Animation0A4E4Records[352] = {
#include "assets/actor_443500_animation_0A4E4_records.inc"
};

static u16 _gActor443500Animation0A4E4Indices[20] = {
#include "assets/actor_443500_animation_0A4E4_indices.inc"
};

static AnimationSet _gActor443500Animation0A4E4 = {
    _gActor443500Animation0A4E4Records,
    _gActor443500Animation0A4E4Indices,
    { NULL, _gActor443500Animation0A4E4Bank1, NULL, NULL, _gActor443500Animation0A4E4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation0ACA0Bank1[14] = {
#include "assets/actor_443500_animation_0ACA0_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation0ACA0Bank4[196] = {
#include "assets/actor_443500_animation_0ACA0_bank4.inc"
};

static AnimationRecord _gActor443500Animation0ACA0Records[237] = {
#include "assets/actor_443500_animation_0ACA0_records.inc"
};

static u16 _gActor443500Animation0ACA0Indices[20] = {
#include "assets/actor_443500_animation_0ACA0_indices.inc"
};

static AnimationSet _gActor443500Animation0ACA0 = {
    _gActor443500Animation0ACA0Records,
    _gActor443500Animation0ACA0Indices,
    { NULL, _gActor443500Animation0ACA0Bank1, NULL, NULL, _gActor443500Animation0ACA0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation0AFB8Bank1[6] = {
#include "assets/actor_443500_animation_0AFB8_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation0AFB8Bank4[52] = {
#include "assets/actor_443500_animation_0AFB8_bank4.inc"
};

static AnimationRecord _gActor443500Animation0AFB8Records[108] = {
#include "assets/actor_443500_animation_0AFB8_records.inc"
};

static u16 _gActor443500Animation0AFB8Indices[20] = {
#include "assets/actor_443500_animation_0AFB8_indices.inc"
};

static AnimationSet _gActor443500Animation0AFB8 = {
    _gActor443500Animation0AFB8Records,
    _gActor443500Animation0AFB8Indices,
    { NULL, _gActor443500Animation0AFB8Bank1, NULL, NULL, _gActor443500Animation0AFB8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation0BC74Bank1[24] = {
#include "assets/actor_443500_animation_0BC74_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation0BC74Bank4[309] = {
#include "assets/actor_443500_animation_0BC74_bank4.inc"
};

static AnimationRecord _gActor443500Animation0BC74Records[414] = {
#include "assets/actor_443500_animation_0BC74_records.inc"
};

static u16 _gActor443500Animation0BC74Indices[20] = {
#include "assets/actor_443500_animation_0BC74_indices.inc"
};

static AnimationSet _gActor443500Animation0BC74 = {
    _gActor443500Animation0BC74Records,
    _gActor443500Animation0BC74Indices,
    { NULL, _gActor443500Animation0BC74Bank1, NULL, NULL, _gActor443500Animation0BC74Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation0BEC0Bank1[4] = {
#include "assets/actor_443500_animation_0BEC0_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation0BEC0Bank4[24] = {
#include "assets/actor_443500_animation_0BEC0_bank4.inc"
};

static AnimationRecord _gActor443500Animation0BEC0Records[91] = {
#include "assets/actor_443500_animation_0BEC0_records.inc"
};

static u16 _gActor443500Animation0BEC0Indices[20] = {
#include "assets/actor_443500_animation_0BEC0_indices.inc"
};

static AnimationSet _gActor443500Animation0BEC0 = {
    _gActor443500Animation0BEC0Records,
    _gActor443500Animation0BEC0Indices,
    { NULL, _gActor443500Animation0BEC0Bank1, NULL, NULL, _gActor443500Animation0BEC0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation0C5DCBank1[8] = {
#include "assets/actor_443500_animation_0C5DC_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation0C5DCBank4[180] = {
#include "assets/actor_443500_animation_0C5DC_bank4.inc"
};

static AnimationRecord _gActor443500Animation0C5DCRecords[231] = {
#include "assets/actor_443500_animation_0C5DC_records.inc"
};

static u16 _gActor443500Animation0C5DCIndices[20] = {
#include "assets/actor_443500_animation_0C5DC_indices.inc"
};

static AnimationSet _gActor443500Animation0C5DC = {
    _gActor443500Animation0C5DCRecords,
    _gActor443500Animation0C5DCIndices,
    { NULL, _gActor443500Animation0C5DCBank1, NULL, NULL, _gActor443500Animation0C5DCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation0CCD8Bank1[10] = {
#include "assets/actor_443500_animation_0CCD8_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation0CCD8Bank4[147] = {
#include "assets/actor_443500_animation_0CCD8_bank4.inc"
};

static AnimationRecord _gActor443500Animation0CCD8Records[250] = {
#include "assets/actor_443500_animation_0CCD8_records.inc"
};

static u16 _gActor443500Animation0CCD8Indices[20] = {
#include "assets/actor_443500_animation_0CCD8_indices.inc"
};

static AnimationSet _gActor443500Animation0CCD8 = {
    _gActor443500Animation0CCD8Records,
    _gActor443500Animation0CCD8Indices,
    { NULL, _gActor443500Animation0CCD8Bank1, NULL, NULL, _gActor443500Animation0CCD8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation0D12CBank1[5] = {
#include "assets/actor_443500_animation_0D12C_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation0D12CBank4[80] = {
#include "assets/actor_443500_animation_0D12C_bank4.inc"
};

static AnimationRecord _gActor443500Animation0D12CRecords[162] = {
#include "assets/actor_443500_animation_0D12C_records.inc"
};

static u16 _gActor443500Animation0D12CIndices[20] = {
#include "assets/actor_443500_animation_0D12C_indices.inc"
};

static AnimationSet _gActor443500Animation0D12C = {
    _gActor443500Animation0D12CRecords,
    _gActor443500Animation0D12CIndices,
    { NULL, _gActor443500Animation0D12CBank1, NULL, NULL, _gActor443500Animation0D12CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation0D3C0Bank1[4] = {
#include "assets/actor_443500_animation_0D3C0_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation0D3C0Bank4[51] = {
#include "assets/actor_443500_animation_0D3C0_bank4.inc"
};

static AnimationRecord _gActor443500Animation0D3C0Records[82] = {
#include "assets/actor_443500_animation_0D3C0_records.inc"
};

static u16 _gActor443500Animation0D3C0Indices[20] = {
#include "assets/actor_443500_animation_0D3C0_indices.inc"
};

static AnimationSet _gActor443500Animation0D3C0 = {
    _gActor443500Animation0D3C0Records,
    _gActor443500Animation0D3C0Indices,
    { NULL, _gActor443500Animation0D3C0Bank1, NULL, NULL, _gActor443500Animation0D3C0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation0DB60Bank1[13] = {
#include "assets/actor_443500_animation_0DB60_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation0DB60Bank4[179] = {
#include "assets/actor_443500_animation_0DB60_bank4.inc"
};

static AnimationRecord _gActor443500Animation0DB60Records[250] = {
#include "assets/actor_443500_animation_0DB60_records.inc"
};

static u16 _gActor443500Animation0DB60Indices[20] = {
#include "assets/actor_443500_animation_0DB60_indices.inc"
};

static AnimationSet _gActor443500Animation0DB60 = {
    _gActor443500Animation0DB60Records,
    _gActor443500Animation0DB60Indices,
    { NULL, _gActor443500Animation0DB60Bank1, NULL, NULL, _gActor443500Animation0DB60Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation0DF34Bank1[8] = {
#include "assets/actor_443500_animation_0DF34_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation0DF34Bank4[84] = {
#include "assets/actor_443500_animation_0DF34_bank4.inc"
};

static AnimationRecord _gActor443500Animation0DF34Records[117] = {
#include "assets/actor_443500_animation_0DF34_records.inc"
};

static u16 _gActor443500Animation0DF34Indices[20] = {
#include "assets/actor_443500_animation_0DF34_indices.inc"
};

static AnimationSet _gActor443500Animation0DF34 = {
    _gActor443500Animation0DF34Records,
    _gActor443500Animation0DF34Indices,
    { NULL, _gActor443500Animation0DF34Bank1, NULL, NULL, _gActor443500Animation0DF34Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation0E324Bank1[7] = {
#include "assets/actor_443500_animation_0E324_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation0E324Bank4[62] = {
#include "assets/actor_443500_animation_0E324_bank4.inc"
};

static AnimationRecord _gActor443500Animation0E324Records[149] = {
#include "assets/actor_443500_animation_0E324_records.inc"
};

static u16 _gActor443500Animation0E324Indices[20] = {
#include "assets/actor_443500_animation_0E324_indices.inc"
};

static AnimationSet _gActor443500Animation0E324 = {
    _gActor443500Animation0E324Records,
    _gActor443500Animation0E324Indices,
    { NULL, _gActor443500Animation0E324Bank1, NULL, NULL, _gActor443500Animation0E324Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation0E690Bank1[7] = {
#include "assets/actor_443500_animation_0E690_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation0E690Bank4[74] = {
#include "assets/actor_443500_animation_0E690_bank4.inc"
};

static AnimationRecord _gActor443500Animation0E690Records[104] = {
#include "assets/actor_443500_animation_0E690_records.inc"
};

static u16 _gActor443500Animation0E690Indices[20] = {
#include "assets/actor_443500_animation_0E690_indices.inc"
};

static AnimationSet _gActor443500Animation0E690 = {
    _gActor443500Animation0E690Records,
    _gActor443500Animation0E690Indices,
    { NULL, _gActor443500Animation0E690Bank1, NULL, NULL, _gActor443500Animation0E690Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation0EC10Bank1[10] = {
#include "assets/actor_443500_animation_0EC10_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation0EC10Bank4[104] = {
#include "assets/actor_443500_animation_0EC10_bank4.inc"
};

static AnimationRecord _gActor443500Animation0EC10Records[198] = {
#include "assets/actor_443500_animation_0EC10_records.inc"
};

static u16 _gActor443500Animation0EC10Indices[20] = {
#include "assets/actor_443500_animation_0EC10_indices.inc"
};

static AnimationSet _gActor443500Animation0EC10 = {
    _gActor443500Animation0EC10Records,
    _gActor443500Animation0EC10Indices,
    { NULL, _gActor443500Animation0EC10Bank1, NULL, NULL, _gActor443500Animation0EC10Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation0EFF0Bank1[8] = {
#include "assets/actor_443500_animation_0EFF0_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation0EFF0Bank4[85] = {
#include "assets/actor_443500_animation_0EFF0_bank4.inc"
};

static AnimationRecord _gActor443500Animation0EFF0Records[119] = {
#include "assets/actor_443500_animation_0EFF0_records.inc"
};

static u16 _gActor443500Animation0EFF0Indices[20] = {
#include "assets/actor_443500_animation_0EFF0_indices.inc"
};

static AnimationSet _gActor443500Animation0EFF0 = {
    _gActor443500Animation0EFF0Records,
    _gActor443500Animation0EFF0Indices,
    { NULL, _gActor443500Animation0EFF0Bank1, NULL, NULL, _gActor443500Animation0EFF0Bank4, NULL, NULL, NULL },
};

TaskDesc D_actor_443500_80140E38 = { { { TASK_BODY_NONE, 192 } }, func_actor_443500_80131F88, { .value = 0 } };

AnimationSet* D_actor_443500_80140E44[11] = {
    NULL,
    &_gActor443500Animation010D0,
    &_gActor443500Animation0A4E4,
    &_gActor443500Animation0ACA0,
    &_gActor443500Animation0AFB8,
    &_gActor443500Animation0BC74,
    &_gActor443500Animation0BEC0,
    &_gActor443500Animation0C5DC,
    &_gActor443500Animation0CCD8,
    &_gActor443500Animation0D12C,
    &_gActor443500Animation0D3C0,
};

AnimationBankCopyRequest D_actor_443500_80140E70 = { { .sets = D_actor_443500_80140E44 }, ARRAY_SIZE(D_actor_443500_80140E44) };

AnimationPlayRequest D_actor_443500_80140E78 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80140E8C = { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 4, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80140EA0 = { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80140EB4 = { { .index = 1 }, 50, ANIMATION_BLEND_INTERPOLATE, 4, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80140EC8 = { { .index = 1 }, 51, ANIMATION_BLEND_INTERPOLATE, 4, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80140EDC = { { .index = 1 }, 52, ANIMATION_BLEND_INTERPOLATE, 4, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80140EF0 = { { .index = 1 }, 53, ANIMATION_BLEND_INTERPOLATE, 4, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80140F04 = { { .index = 1 }, 54, ANIMATION_BLEND_INTERPOLATE, 4, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80140F18 = { { .index = 1 }, 55, ANIMATION_BLEND_INTERPOLATE, 4, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80140F2C = { { .index = 1 }, 56, ANIMATION_BLEND_INTERPOLATE, 4, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80140F40 = { { .index = 1 }, 57, ANIMATION_BLEND_INTERPOLATE, 4, ANIMATION_WORLD_COLLISION_ENABLE };

ActorTransform D_actor_443500_80140F54 = { { 0x3A98, -1000, 7600, 0 }, { 0, 512, 0, 0 } };

ActorTransform D_actor_443500_80140F6C = { { 0x3CF0, -1000, 9000, 0 }, { 0, 512, 0, 0 } };

AnimationSet* D_actor_443500_80140F84[25] = {
    NULL,
    &_gActor443500Animation010D0,
    &_gActor443500Animation026FC,
    &_gActor443500Animation0321C,
    &_gActor443500Animation03D34,
    &_gActor443500Animation03F54,
    &_gActor443500Animation049F8,
    &_gActor443500Animation04C48,
    &_gActor443500Animation05074,
    &_gActor443500Animation05570,
    &_gActor443500Animation06034,
    &_gActor443500Animation062BC,
    &_gActor443500Animation065D8,
    &_gActor443500Animation0771C,
    &_gActor443500Animation083E8,
    &_gActor443500Animation086BC,
    &_gActor443500Animation08E08,
    &_gActor443500Animation09350,
    &_gActor443500Animation09A00,
    &_gActor443500Animation0DF34,
    &_gActor443500Animation0E324,
    &_gActor443500Animation0E690,
    &_gActor443500Animation0EC10,
    &_gActor443500Animation0EFF0,
    &_gActor443500Animation0DB60,
};

AnimationBankCopyRequest D_actor_443500_80140FE8 = { { .sets = D_actor_443500_80140F84 }, ARRAY_SIZE(D_actor_443500_80140F84) };

AnimationPlayRequest D_actor_443500_80140FF0 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141004 = { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141018 = { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_8014102C = { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141040 = { { .index = 1 }, 51, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141054 = { { .index = 1 }, 52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141068 = { { .index = 1 }, 53, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_8014107C = { { .index = 1 }, 54, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141090 = { { .index = 1 }, 55, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_801410A4 = { { .index = 1 }, 56, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_801410B8 = { { .index = 1 }, 57, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_801410CC = { { .index = 1 }, 58, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_801410E0 = { { .index = 1 }, 59, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_801410F4 = { { .index = 1 }, 60, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141108 = { { .index = 1 }, 61, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_8014111C = { { .index = 1 }, 62, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141130 = { { .index = 1 }, 63, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141144 = { { .index = 1 }, 64, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141158 = { { .index = 1 }, 65, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_8014116C = { { .index = 1 }, 66, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141180 = { { .index = 1 }, 67, ANIMATION_BLEND_INTERPOLATE, 2, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141194 = { { .index = 1 }, 68, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_801411A8 = { { .index = 1 }, 69, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_801411BC = { { .index = 1 }, 70, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_801411D0 = { { .index = 1 }, 71, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

ActorTransform D_actor_443500_801411E4 = { { 0x3F48, -1000, 8000, 0 }, { 0, 2047, 0, 0 } };

ActorTransform D_actor_443500_801411FC = { { 0x3FAC, -1000, 6950, 0 }, { 0, 1365, 0, 0 } };

AnimationPlayRequest D_actor_443500_80141214 = { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141228 = { { .index = 0 }, 1, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_8014123C = { { .index = 0 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141250 = { { .index = 0 }, 3, ANIMATION_BLEND_INTERPOLATE, 4, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141264 = { { .index = 0 }, 4, ANIMATION_BLEND_INTERPOLATE, 4, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141278 = { { .index = 0 }, 5, ANIMATION_BLEND_INTERPOLATE, 4, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_8014128C = { { .index = 0 }, 6, ANIMATION_BLEND_INTERPOLATE, 4, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_801412A0 = { { .index = 0 }, 7, ANIMATION_BLEND_INTERPOLATE, 4, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_801412B4 = { { .index = 0 }, 8, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_801412C8 = { { .index = 0 }, 9, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_801412DC = { { .index = 0 }, 10, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_801412F0 = { { .index = 0 }, 11, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141304 = { { .index = 0 }, 12, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141318 = { { .index = 0 }, 13, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_8014132C = { { .index = 0 }, 14, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141340 = { { .index = 0 }, 15, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141354 = { { .index = 0 }, 16, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141368 = { { .index = 0 }, 17, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_8014137C = { { .index = 0 }, 18, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141390 = { { .index = 0 }, 19, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_801413A4 = { { .index = 0 }, 20, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_801413B8 = { { .index = 0 }, 21, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_801413CC = { { .index = 0 }, 22, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_801413E0 = { { .index = 0 }, 23, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_801413F4 = { { .index = 0 }, 24, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141408 = { { .index = 0 }, 25, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_8014141C = { { .index = 0 }, 26, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141430 = { { .index = 0 }, 27, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141444 = { { .index = 0 }, 28, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141458 = { { .index = 0 }, 29, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_8014146C = { { .index = 0 }, 30, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141480 = { { .index = 0 }, 31, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_80141494 = { { .index = 0 }, 32, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_801414A8 = { { .index = 0 }, 33, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_801414BC = { { .index = 0 }, 34, ANIMATION_BLEND_INTERPOLATE, 2, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_443500_801414D0 = { { .index = 0 }, 35, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

ActorTransform D_actor_443500_801414E4 = { { 0x3E80, -1000, 9000, 0 }, { 0, 2560, 0, 0 } };

ActorTransform D_actor_443500_801414FC = { { 0x3F48, -1000, 6200, 0 }, { 0, 0, 0, 0 } };

ActorTransform D_actor_443500_80141514 = { { 0x3E80, -1000, 5910, 0 }, { 0, 0, 0, 0 } };

EvsCommand D_actor_443500_8014152C[74] = {
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_443500_80140E70 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_443500_80131E3C }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = 0x542F0001 }, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x542F0006 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_443500_80140F54 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_443500_801414E4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80140E8C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_8014123C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80140EA0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_80141250 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_80141264 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 120 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_801412F0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80140F18 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 64 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80140EB4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80140EC8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_801412DC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80140EDC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 140 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_80141304 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80140EF0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_801412B4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_801412C8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_80141278 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80140F04 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_80141318 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 35 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80140E8C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80140F2C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 120 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80140F40 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80140E8C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80140E8C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_8014128C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_801412A0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1010 }, { .message = { .pointer = &D_actor_443500_80140F6C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_443500_80131E3C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_443500_80141C1C[16] = {
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = 0x542F0001 }, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_443500_80140F6C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_443500_801414E4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_443500_80131E3C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_443500_80141D9C[137] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_443500_80131F58 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = 0x542F0001 }, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_443500_80140FE8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS8 = func_actor_443500_8013206C }, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_443500_80131E3C }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_443500_801411E4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_443500_801414FC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 13 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80141018 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_8014132C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 5 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_8014102C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_80141340 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_8014116C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_80141228 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 46 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80141180 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_80141318 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_801414A8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 25 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_801414BC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 90 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_801412C8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80141040 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_80141228 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_801414A8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_801414BC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 88 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80141004 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_80141228 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_801412C8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_801412C8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_801411A8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_80141228 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 189 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_801411BC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80141004 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_80141480 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_80141494 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_80141318 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_801414BC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 38 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_801414D0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_801412C8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 42 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_80141228 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_8014102C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_80141228 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80141004 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_80141430 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_801411D0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1010 }, { .message = { .pointer = &D_actor_443500_801411FC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_8014132C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_443500_80131F18 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_443500_80140FE8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80141054 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_443500_801411FC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_443500_80141514 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_80141354 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_80141368 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_80141390 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_801413A4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80141068 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_8014137C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 80 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80141090 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_801413B8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_801410A4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_801413CC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80141054 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_80141408 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_80141390 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_801413A4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_8014137C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_801410B8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_801413E0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_80141354 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_443500_80131E3C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_443500_80142A74[18] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = 0x542F0001 }, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS8 = func_actor_443500_8013206C }, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_443500_801411FC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_443500_80141514 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = func_actor_443500_8013201C }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_443500_80131E3C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_443500_80142C24[73] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_443500_80131F58 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_443500_80140FE8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_443500_80131E3C }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_443500_801411FC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_443500_80141514 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_801410CC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_80141354 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 6 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_801410E0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_80141368 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_801410F4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80141004 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_80141390 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_801413A4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80141108 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_8014137C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_8014111C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_80141390 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_801413A4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80141130 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_8014137C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_801413F4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_8014111C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_8014137C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_80141408 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_80141390 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 70 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_801413A4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 25 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_801413E0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_80141354 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80141144 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_8014111C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_80141368 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_8014141C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80141158 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_80141444 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_443500_80131E3C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_443500_80132048 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_443500_801432FC[17] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_443500_801411FC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_443500_80141514 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_80141444 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_DIRTY_VIEW, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_443500_80131E3C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_443500_80132048 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_443500_80143494[10] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_443500_80140FE8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80141004 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_80141458 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_443500_80131E84 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_443500_80131EE4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_443500_80131E3C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_8014146C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

static TmdBone _gActor443500PierceCarradineBodySkeleton[20] = {
#include "assets/pierce_carradine_body_skeleton.inc"
};

static u32 _gActor443500PierceCarradineBodyPartVerts[20] = {
#include "assets/pierce_carradine_body_partVerts.inc"
};

static SVECTOR _gActor443500PierceCarradineBodyVerts[390] = {
#include "assets/pierce_carradine_body_verts.inc"
};

static SVECTOR _gActor443500PierceCarradineBodyNormals[407] = {
#include "assets/pierce_carradine_body_normals.inc"
};

static u32 _gActor443500PierceCarradineBodyStream[4476] = {
#include "assets/pierce_carradine_body_stream.inc"
};

static TmdSource _gActor443500PierceCarradineBody = {
    0,
    24444,
    6776,
    20,
    _gActor443500PierceCarradineBodyPartVerts,
    _gActor443500PierceCarradineBodyVerts,
    _gActor443500PierceCarradineBodyNormals,
    _gActor443500PierceCarradineBodySkeleton,
    _gActor443500PierceCarradineBodyStream,
};

static TmdBone _gActor443500Actor113100Model07960Skeleton[1] = {
#include "assets/actor_113100_model_07960_skeleton.inc"
};

static u32 _gActor443500Actor113100Model07960PartVerts[1] = {
#include "assets/actor_113100_model_07960_partVerts.inc"
};

static SVECTOR _gActor443500Actor113100Model07960Verts[14] = {
#include "assets/actor_113100_model_07960_verts.inc"
};

static SVECTOR _gActor443500Actor113100Model07960Normals[12] = {
#include "assets/actor_113100_model_07960_normals.inc"
};

static u32 _gActor443500Actor113100Model07960Stream[56] = {
#include "assets/actor_113100_model_07960_stream.inc"
};

static TmdSource _gActor443500Actor113100Model07960 = {
    0,
    340,
    0,
    1,
    _gActor443500Actor113100Model07960PartVerts,
    _gActor443500Actor113100Model07960Verts,
    _gActor443500Actor113100Model07960Normals,
    _gActor443500Actor113100Model07960Skeleton,
    _gActor443500Actor113100Model07960Stream,
};

static AnimationPackedPose _gActor443500Animation17DD0Bank1[2] = {
#include "assets/actor_443500_animation_17DD0_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation17DD0Bank4[32] = {
#include "assets/actor_443500_animation_17DD0_bank4.inc"
};

static AnimationRecord _gActor443500Animation17DD0Records[101] = {
#include "assets/actor_443500_animation_17DD0_records.inc"
};

static u16 _gActor443500Animation17DD0Indices[20] = {
#include "assets/actor_443500_animation_17DD0_indices.inc"
};

static AnimationSet _gActor443500Animation17DD0 = {
    _gActor443500Animation17DD0Records,
    _gActor443500Animation17DD0Indices,
    { NULL, _gActor443500Animation17DD0Bank1, NULL, NULL, _gActor443500Animation17DD0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation18308Bank1[4] = {
#include "assets/actor_443500_animation_18308_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation18308Bank4[112] = {
#include "assets/actor_443500_animation_18308_bank4.inc"
};

static AnimationRecord _gActor443500Animation18308Records[190] = {
#include "assets/actor_443500_animation_18308_records.inc"
};

static u16 _gActor443500Animation18308Indices[20] = {
#include "assets/actor_443500_animation_18308_indices.inc"
};

static AnimationSet _gActor443500Animation18308 = {
    _gActor443500Animation18308Records,
    _gActor443500Animation18308Indices,
    { NULL, _gActor443500Animation18308Bank1, NULL, NULL, _gActor443500Animation18308Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation18914Bank1[10] = {
#include "assets/actor_443500_animation_18914_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation18914Bank4[134] = {
#include "assets/actor_443500_animation_18914_bank4.inc"
};

static AnimationRecord _gActor443500Animation18914Records[203] = {
#include "assets/actor_443500_animation_18914_records.inc"
};

static u16 _gActor443500Animation18914Indices[20] = {
#include "assets/actor_443500_animation_18914_indices.inc"
};

static AnimationSet _gActor443500Animation18914 = {
    _gActor443500Animation18914Records,
    _gActor443500Animation18914Indices,
    { NULL, _gActor443500Animation18914Bank1, NULL, NULL, _gActor443500Animation18914Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation197CCBank1[33] = {
#include "assets/actor_443500_animation_197CC_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation197CCBank4[322] = {
#include "assets/actor_443500_animation_197CC_bank4.inc"
};

static AnimationRecord _gActor443500Animation197CCRecords[501] = {
#include "assets/actor_443500_animation_197CC_records.inc"
};

static u16 _gActor443500Animation197CCIndices[20] = {
#include "assets/actor_443500_animation_197CC_indices.inc"
};

static AnimationSet _gActor443500Animation197CC = {
    _gActor443500Animation197CCRecords,
    _gActor443500Animation197CCIndices,
    { NULL, _gActor443500Animation197CCBank1, NULL, NULL, _gActor443500Animation197CCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation1A180Bank1[10] = {
#include "assets/actor_443500_animation_1A180_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation1A180Bank4[231] = {
#include "assets/actor_443500_animation_1A180_bank4.inc"
};

static AnimationRecord _gActor443500Animation1A180Records[340] = {
#include "assets/actor_443500_animation_1A180_records.inc"
};

static u16 _gActor443500Animation1A180Indices[20] = {
#include "assets/actor_443500_animation_1A180_indices.inc"
};

static AnimationSet _gActor443500Animation1A180 = {
    _gActor443500Animation1A180Records,
    _gActor443500Animation1A180Indices,
    { NULL, _gActor443500Animation1A180Bank1, NULL, NULL, _gActor443500Animation1A180Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation1AA30Bank1[22] = {
#include "assets/actor_443500_animation_1AA30_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation1AA30Bank4[193] = {
#include "assets/actor_443500_animation_1AA30_bank4.inc"
};

static AnimationRecord _gActor443500Animation1AA30Records[277] = {
#include "assets/actor_443500_animation_1AA30_records.inc"
};

static u16 _gActor443500Animation1AA30Indices[20] = {
#include "assets/actor_443500_animation_1AA30_indices.inc"
};

static AnimationSet _gActor443500Animation1AA30 = {
    _gActor443500Animation1AA30Records,
    _gActor443500Animation1AA30Indices,
    { NULL, _gActor443500Animation1AA30Bank1, NULL, NULL, _gActor443500Animation1AA30Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation1BA70Bank1[41] = {
#include "assets/actor_443500_animation_1BA70_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation1BA70Bank4[384] = {
#include "assets/actor_443500_animation_1BA70_bank4.inc"
};

static AnimationRecord _gActor443500Animation1BA70Records[513] = {
#include "assets/actor_443500_animation_1BA70_records.inc"
};

static u16 _gActor443500Animation1BA70Indices[20] = {
#include "assets/actor_443500_animation_1BA70_indices.inc"
};

static AnimationSet _gActor443500Animation1BA70 = {
    _gActor443500Animation1BA70Records,
    _gActor443500Animation1BA70Indices,
    { NULL, _gActor443500Animation1BA70Bank1, NULL, NULL, _gActor443500Animation1BA70Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation1C41CBank1[9] = {
#include "assets/actor_443500_animation_1C41C_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation1C41CBank4[245] = {
#include "assets/actor_443500_animation_1C41C_bank4.inc"
};

static AnimationRecord _gActor443500Animation1C41CRecords[327] = {
#include "assets/actor_443500_animation_1C41C_records.inc"
};

static u16 _gActor443500Animation1C41CIndices[20] = {
#include "assets/actor_443500_animation_1C41C_indices.inc"
};

static AnimationSet _gActor443500Animation1C41C = {
    _gActor443500Animation1C41CRecords,
    _gActor443500Animation1C41CIndices,
    { NULL, _gActor443500Animation1C41CBank1, NULL, NULL, _gActor443500Animation1C41CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation1C990Bank1[2] = {
#include "assets/actor_443500_animation_1C990_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation1C990Bank4[135] = {
#include "assets/actor_443500_animation_1C990_bank4.inc"
};

static AnimationRecord _gActor443500Animation1C990Records[188] = {
#include "assets/actor_443500_animation_1C990_records.inc"
};

static u16 _gActor443500Animation1C990Indices[20] = {
#include "assets/actor_443500_animation_1C990_indices.inc"
};

static AnimationSet _gActor443500Animation1C990 = {
    _gActor443500Animation1C990Records,
    _gActor443500Animation1C990Indices,
    { NULL, _gActor443500Animation1C990Bank1, NULL, NULL, _gActor443500Animation1C990Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation1CFD4Bank1[15] = {
#include "assets/actor_443500_animation_1CFD4_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation1CFD4Bank4[123] = {
#include "assets/actor_443500_animation_1CFD4_bank4.inc"
};

static AnimationRecord _gActor443500Animation1CFD4Records[213] = {
#include "assets/actor_443500_animation_1CFD4_records.inc"
};

static u16 _gActor443500Animation1CFD4Indices[20] = {
#include "assets/actor_443500_animation_1CFD4_indices.inc"
};

static AnimationSet _gActor443500Animation1CFD4 = {
    _gActor443500Animation1CFD4Records,
    _gActor443500Animation1CFD4Indices,
    { NULL, _gActor443500Animation1CFD4Bank1, NULL, NULL, _gActor443500Animation1CFD4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation1D394Bank1[6] = {
#include "assets/actor_443500_animation_1D394_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation1D394Bank4[84] = {
#include "assets/actor_443500_animation_1D394_bank4.inc"
};

static AnimationRecord _gActor443500Animation1D394Records[118] = {
#include "assets/actor_443500_animation_1D394_records.inc"
};

static u16 _gActor443500Animation1D394Indices[20] = {
#include "assets/actor_443500_animation_1D394_indices.inc"
};

static AnimationSet _gActor443500Animation1D394 = {
    _gActor443500Animation1D394Records,
    _gActor443500Animation1D394Indices,
    { NULL, _gActor443500Animation1D394Bank1, NULL, NULL, _gActor443500Animation1D394Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation1D758Bank1[7] = {
#include "assets/actor_443500_animation_1D758_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation1D758Bank4[82] = {
#include "assets/actor_443500_animation_1D758_bank4.inc"
};

static AnimationRecord _gActor443500Animation1D758Records[118] = {
#include "assets/actor_443500_animation_1D758_records.inc"
};

static u16 _gActor443500Animation1D758Indices[20] = {
#include "assets/actor_443500_animation_1D758_indices.inc"
};

static AnimationSet _gActor443500Animation1D758 = {
    _gActor443500Animation1D758Records,
    _gActor443500Animation1D758Indices,
    { NULL, _gActor443500Animation1D758Bank1, NULL, NULL, _gActor443500Animation1D758Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation1DDD4Bank1[10] = {
#include "assets/actor_443500_animation_1DDD4_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation1DDD4Bank4[142] = {
#include "assets/actor_443500_animation_1DDD4_bank4.inc"
};

static AnimationRecord _gActor443500Animation1DDD4Records[223] = {
#include "assets/actor_443500_animation_1DDD4_records.inc"
};

static u16 _gActor443500Animation1DDD4Indices[20] = {
#include "assets/actor_443500_animation_1DDD4_indices.inc"
};

static AnimationSet _gActor443500Animation1DDD4 = {
    _gActor443500Animation1DDD4Records,
    _gActor443500Animation1DDD4Indices,
    { NULL, _gActor443500Animation1DDD4Bank1, NULL, NULL, _gActor443500Animation1DDD4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation1E65CBank1[4] = {
#include "assets/actor_443500_animation_1E65C_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation1E65CBank4[205] = {
#include "assets/actor_443500_animation_1E65C_bank4.inc"
};

static AnimationRecord _gActor443500Animation1E65CRecords[309] = {
#include "assets/actor_443500_animation_1E65C_records.inc"
};

static u16 _gActor443500Animation1E65CIndices[20] = {
#include "assets/actor_443500_animation_1E65C_indices.inc"
};

static AnimationSet _gActor443500Animation1E65C = {
    _gActor443500Animation1E65CRecords,
    _gActor443500Animation1E65CIndices,
    { NULL, _gActor443500Animation1E65CBank1, NULL, NULL, _gActor443500Animation1E65CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation1FF38Bank1[74] = {
#include "assets/actor_443500_animation_1FF38_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation1FF38Bank4[574] = {
#include "assets/actor_443500_animation_1FF38_bank4.inc"
};

static AnimationRecord _gActor443500Animation1FF38Records[775] = {
#include "assets/actor_443500_animation_1FF38_records.inc"
};

static u16 _gActor443500Animation1FF38Indices[20] = {
#include "assets/actor_443500_animation_1FF38_indices.inc"
};

static AnimationSet _gActor443500Animation1FF38 = {
    _gActor443500Animation1FF38Records,
    _gActor443500Animation1FF38Indices,
    { NULL, _gActor443500Animation1FF38Bank1, NULL, NULL, _gActor443500Animation1FF38Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation201E0Bank1[2] = {
#include "assets/actor_443500_animation_201E0_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation201E0Bank4[32] = {
#include "assets/actor_443500_animation_201E0_bank4.inc"
};

static AnimationRecord _gActor443500Animation201E0Records[112] = {
#include "assets/actor_443500_animation_201E0_records.inc"
};

static u16 _gActor443500Animation201E0Indices[20] = {
#include "assets/actor_443500_animation_201E0_indices.inc"
};

static AnimationSet _gActor443500Animation201E0 = {
    _gActor443500Animation201E0Records,
    _gActor443500Animation201E0Indices,
    { NULL, _gActor443500Animation201E0Bank1, NULL, NULL, _gActor443500Animation201E0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation204F0Bank1[7] = {
#include "assets/actor_443500_animation_204F0_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation204F0Bank4[53] = {
#include "assets/actor_443500_animation_204F0_bank4.inc"
};

static AnimationRecord _gActor443500Animation204F0Records[102] = {
#include "assets/actor_443500_animation_204F0_records.inc"
};

static u16 _gActor443500Animation204F0Indices[20] = {
#include "assets/actor_443500_animation_204F0_indices.inc"
};

static AnimationSet _gActor443500Animation204F0 = {
    _gActor443500Animation204F0Records,
    _gActor443500Animation204F0Indices,
    { NULL, _gActor443500Animation204F0Bank1, NULL, NULL, _gActor443500Animation204F0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation2071CBank1[2] = {
#include "assets/actor_443500_animation_2071C_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation2071CBank4[21] = {
#include "assets/actor_443500_animation_2071C_bank4.inc"
};

static AnimationRecord _gActor443500Animation2071CRecords[92] = {
#include "assets/actor_443500_animation_2071C_records.inc"
};

static u16 _gActor443500Animation2071CIndices[20] = {
#include "assets/actor_443500_animation_2071C_indices.inc"
};

static AnimationSet _gActor443500Animation2071C = {
    _gActor443500Animation2071CRecords,
    _gActor443500Animation2071CIndices,
    { NULL, _gActor443500Animation2071CBank1, NULL, NULL, _gActor443500Animation2071CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation20CB4Bank1[3] = {
#include "assets/actor_443500_animation_20CB4_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation20CB4Bank4[113] = {
#include "assets/actor_443500_animation_20CB4_bank4.inc"
};

static AnimationRecord _gActor443500Animation20CB4Records[216] = {
#include "assets/actor_443500_animation_20CB4_records.inc"
};

static u16 _gActor443500Animation20CB4Indices[20] = {
#include "assets/actor_443500_animation_20CB4_indices.inc"
};

static AnimationSet _gActor443500Animation20CB4 = {
    _gActor443500Animation20CB4Records,
    _gActor443500Animation20CB4Indices,
    { NULL, _gActor443500Animation20CB4Bank1, NULL, NULL, _gActor443500Animation20CB4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation20FA8Bank1[2] = {
#include "assets/actor_443500_animation_20FA8_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation20FA8Bank4[62] = {
#include "assets/actor_443500_animation_20FA8_bank4.inc"
};

static AnimationRecord _gActor443500Animation20FA8Records[101] = {
#include "assets/actor_443500_animation_20FA8_records.inc"
};

static u16 _gActor443500Animation20FA8Indices[20] = {
#include "assets/actor_443500_animation_20FA8_indices.inc"
};

static AnimationSet _gActor443500Animation20FA8 = {
    _gActor443500Animation20FA8Records,
    _gActor443500Animation20FA8Indices,
    { NULL, _gActor443500Animation20FA8Bank1, NULL, NULL, _gActor443500Animation20FA8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation21738Bank1[16] = {
#include "assets/actor_443500_animation_21738_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation21738Bank4[182] = {
#include "assets/actor_443500_animation_21738_bank4.inc"
};

static AnimationRecord _gActor443500Animation21738Records[234] = {
#include "assets/actor_443500_animation_21738_records.inc"
};

static u16 _gActor443500Animation21738Indices[20] = {
#include "assets/actor_443500_animation_21738_indices.inc"
};

static AnimationSet _gActor443500Animation21738 = {
    _gActor443500Animation21738Records,
    _gActor443500Animation21738Indices,
    { NULL, _gActor443500Animation21738Bank1, NULL, NULL, _gActor443500Animation21738Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation21E4CBank1[13] = {
#include "assets/actor_443500_animation_21E4C_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation21E4CBank4[173] = {
#include "assets/actor_443500_animation_21E4C_bank4.inc"
};

static AnimationRecord _gActor443500Animation21E4CRecords[221] = {
#include "assets/actor_443500_animation_21E4C_records.inc"
};

static u16 _gActor443500Animation21E4CIndices[20] = {
#include "assets/actor_443500_animation_21E4C_indices.inc"
};

static AnimationSet _gActor443500Animation21E4C = {
    _gActor443500Animation21E4CRecords,
    _gActor443500Animation21E4CIndices,
    { NULL, _gActor443500Animation21E4CBank1, NULL, NULL, _gActor443500Animation21E4CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation22120Bank1[8] = {
#include "assets/actor_443500_animation_22120_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation22120Bank4[45] = {
#include "assets/actor_443500_animation_22120_bank4.inc"
};

static AnimationRecord _gActor443500Animation22120Records[92] = {
#include "assets/actor_443500_animation_22120_records.inc"
};

static u16 _gActor443500Animation22120Indices[20] = {
#include "assets/actor_443500_animation_22120_indices.inc"
};

static AnimationSet _gActor443500Animation22120 = {
    _gActor443500Animation22120Records,
    _gActor443500Animation22120Indices,
    { NULL, _gActor443500Animation22120Bank1, NULL, NULL, _gActor443500Animation22120Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation22A1CBank1[4] = {
#include "assets/actor_443500_animation_22A1C_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation22A1CBank4[225] = {
#include "assets/actor_443500_animation_22A1C_bank4.inc"
};

static AnimationRecord _gActor443500Animation22A1CRecords[318] = {
#include "assets/actor_443500_animation_22A1C_records.inc"
};

static u16 _gActor443500Animation22A1CIndices[20] = {
#include "assets/actor_443500_animation_22A1C_indices.inc"
};

static AnimationSet _gActor443500Animation22A1C = {
    _gActor443500Animation22A1CRecords,
    _gActor443500Animation22A1CIndices,
    { NULL, _gActor443500Animation22A1CBank1, NULL, NULL, _gActor443500Animation22A1CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation22F94Bank1[2] = {
#include "assets/actor_443500_animation_22F94_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation22F94Bank4[135] = {
#include "assets/actor_443500_animation_22F94_bank4.inc"
};

static AnimationRecord _gActor443500Animation22F94Records[189] = {
#include "assets/actor_443500_animation_22F94_records.inc"
};

static u16 _gActor443500Animation22F94Indices[20] = {
#include "assets/actor_443500_animation_22F94_indices.inc"
};

static AnimationSet _gActor443500Animation22F94 = {
    _gActor443500Animation22F94Records,
    _gActor443500Animation22F94Indices,
    { NULL, _gActor443500Animation22F94Bank1, NULL, NULL, _gActor443500Animation22F94Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation23814Bank1[2] = {
#include "assets/actor_443500_animation_23814_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation23814Bank4[229] = {
#include "assets/actor_443500_animation_23814_bank4.inc"
};

static AnimationRecord _gActor443500Animation23814Records[289] = {
#include "assets/actor_443500_animation_23814_records.inc"
};

static u16 _gActor443500Animation23814Indices[20] = {
#include "assets/actor_443500_animation_23814_indices.inc"
};

static AnimationSet _gActor443500Animation23814 = {
    _gActor443500Animation23814Records,
    _gActor443500Animation23814Indices,
    { NULL, _gActor443500Animation23814Bank1, NULL, NULL, _gActor443500Animation23814Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation24544Bank1[30] = {
#include "assets/actor_443500_animation_24544_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation24544Bank4[312] = {
#include "assets/actor_443500_animation_24544_bank4.inc"
};

static AnimationRecord _gActor443500Animation24544Records[422] = {
#include "assets/actor_443500_animation_24544_records.inc"
};

static u16 _gActor443500Animation24544Indices[20] = {
#include "assets/actor_443500_animation_24544_indices.inc"
};

static AnimationSet _gActor443500Animation24544 = {
    _gActor443500Animation24544Records,
    _gActor443500Animation24544Indices,
    { NULL, _gActor443500Animation24544Bank1, NULL, NULL, _gActor443500Animation24544Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation24C1CBank1[3] = {
#include "assets/actor_443500_animation_24C1C_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation24C1CBank4[83] = {
#include "assets/actor_443500_animation_24C1C_bank4.inc"
};

static AnimationRecord _gActor443500Animation24C1CRecords[326] = {
#include "assets/actor_443500_animation_24C1C_records.inc"
};

static u16 _gActor443500Animation24C1CIndices[20] = {
#include "assets/actor_443500_animation_24C1C_indices.inc"
};

static AnimationSet _gActor443500Animation24C1C = {
    _gActor443500Animation24C1CRecords,
    _gActor443500Animation24C1CIndices,
    { NULL, _gActor443500Animation24C1CBank1, NULL, NULL, _gActor443500Animation24C1CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation25234Bank1[11] = {
#include "assets/actor_443500_animation_25234_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation25234Bank4[120] = {
#include "assets/actor_443500_animation_25234_bank4.inc"
};

static AnimationRecord _gActor443500Animation25234Records[217] = {
#include "assets/actor_443500_animation_25234_records.inc"
};

static u16 _gActor443500Animation25234Indices[20] = {
#include "assets/actor_443500_animation_25234_indices.inc"
};

static AnimationSet _gActor443500Animation25234 = {
    _gActor443500Animation25234Records,
    _gActor443500Animation25234Indices,
    { NULL, _gActor443500Animation25234Bank1, NULL, NULL, _gActor443500Animation25234Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation25700Bank1[11] = {
#include "assets/actor_443500_animation_25700_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation25700Bank4[106] = {
#include "assets/actor_443500_animation_25700_bank4.inc"
};

static AnimationRecord _gActor443500Animation25700Records[148] = {
#include "assets/actor_443500_animation_25700_records.inc"
};

static u16 _gActor443500Animation25700Indices[20] = {
#include "assets/actor_443500_animation_25700_indices.inc"
};

static AnimationSet _gActor443500Animation25700 = {
    _gActor443500Animation25700Records,
    _gActor443500Animation25700Indices,
    { NULL, _gActor443500Animation25700Bank1, NULL, NULL, _gActor443500Animation25700Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation25D38Bank1[9] = {
#include "assets/actor_443500_animation_25D38_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation25D38Bank4[132] = {
#include "assets/actor_443500_animation_25D38_bank4.inc"
};

static AnimationRecord _gActor443500Animation25D38Records[219] = {
#include "assets/actor_443500_animation_25D38_records.inc"
};

static u16 _gActor443500Animation25D38Indices[20] = {
#include "assets/actor_443500_animation_25D38_indices.inc"
};

static AnimationSet _gActor443500Animation25D38 = {
    _gActor443500Animation25D38Records,
    _gActor443500Animation25D38Indices,
    { NULL, _gActor443500Animation25D38Bank1, NULL, NULL, _gActor443500Animation25D38Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation2609CBank1[6] = {
#include "assets/actor_443500_animation_2609C_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation2609CBank4[72] = {
#include "assets/actor_443500_animation_2609C_bank4.inc"
};

static AnimationRecord _gActor443500Animation2609CRecords[107] = {
#include "assets/actor_443500_animation_2609C_records.inc"
};

static u16 _gActor443500Animation2609CIndices[20] = {
#include "assets/actor_443500_animation_2609C_indices.inc"
};

static AnimationSet _gActor443500Animation2609C = {
    _gActor443500Animation2609CRecords,
    _gActor443500Animation2609CIndices,
    { NULL, _gActor443500Animation2609CBank1, NULL, NULL, _gActor443500Animation2609CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation26274Bank1[3] = {
#include "assets/actor_443500_animation_26274_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation26274Bank4[29] = {
#include "assets/actor_443500_animation_26274_bank4.inc"
};

static AnimationRecord _gActor443500Animation26274Records[60] = {
#include "assets/actor_443500_animation_26274_records.inc"
};

static u16 _gActor443500Animation26274Indices[20] = {
#include "assets/actor_443500_animation_26274_indices.inc"
};

static AnimationSet _gActor443500Animation26274 = {
    _gActor443500Animation26274Records,
    _gActor443500Animation26274Indices,
    { NULL, _gActor443500Animation26274Bank1, NULL, NULL, _gActor443500Animation26274Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation26690Bank1[3] = {
#include "assets/actor_443500_animation_26690_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation26690Bank4[80] = {
#include "assets/actor_443500_animation_26690_bank4.inc"
};

static AnimationRecord _gActor443500Animation26690Records[154] = {
#include "assets/actor_443500_animation_26690_records.inc"
};

static u16 _gActor443500Animation26690Indices[20] = {
#include "assets/actor_443500_animation_26690_indices.inc"
};

static AnimationSet _gActor443500Animation26690 = {
    _gActor443500Animation26690Records,
    _gActor443500Animation26690Indices,
    { NULL, _gActor443500Animation26690Bank1, NULL, NULL, _gActor443500Animation26690Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor443500Animation2684CBank1[2] = {
#include "assets/actor_443500_animation_2684C_bank1.inc"
};

static AnimationPackedRotation _gActor443500Animation2684CBank4[25] = {
#include "assets/actor_443500_animation_2684C_bank4.inc"
};

static AnimationRecord _gActor443500Animation2684CRecords[60] = {
#include "assets/actor_443500_animation_2684C_records.inc"
};

static u16 _gActor443500Animation2684CIndices[20] = {
#include "assets/actor_443500_animation_2684C_indices.inc"
};

static AnimationSet _gActor443500Animation2684C = {
    _gActor443500Animation2684CRecords,
    _gActor443500Animation2684CIndices,
    { NULL, _gActor443500Animation2684CBank1, NULL, NULL, _gActor443500Animation2684CBank4, NULL, NULL, NULL },
};

AnimationSet* D_actor_443500_80158694[36] = {
    NULL,
    &_gActor443500Animation17DD0,
    &_gActor443500Animation18308,
    &_gActor443500Animation18914,
    &_gActor443500Animation197CC,
    &_gActor443500Animation1A180,
    &_gActor443500Animation1AA30,
    &_gActor443500Animation1BA70,
    &_gActor443500Animation1C41C,
    &_gActor443500Animation1C990,
    &_gActor443500Animation1CFD4,
    &_gActor443500Animation1D394,
    &_gActor443500Animation1D758,
    &_gActor443500Animation1DDD4,
    &_gActor443500Animation1E65C,
    &_gActor443500Animation1FF38,
    &_gActor443500Animation201E0,
    &_gActor443500Animation204F0,
    &_gActor443500Animation2071C,
    &_gActor443500Animation20CB4,
    &_gActor443500Animation20FA8,
    &_gActor443500Animation21738,
    &_gActor443500Animation21E4C,
    &_gActor443500Animation22120,
    &_gActor443500Animation22A1C,
    &_gActor443500Animation22F94,
    &_gActor443500Animation23814,
    &_gActor443500Animation24544,
    &_gActor443500Animation24C1C,
    &_gActor443500Animation25234,
    &_gActor443500Animation25700,
    &_gActor443500Animation25D38,
    &_gActor443500Animation2609C,
    &_gActor443500Animation26274,
    &_gActor443500Animation26690,
    &_gActor443500Animation2684C,
};

AnimationSet** D_actor_443500_80158724[1] = {
    D_actor_443500_80158694,
};

AnimationPlayRequest D_actor_443500_80158728 = { { .sets = NULL }, 28, ANIMATION_BLEND_INTERPOLATE, 4, ANIMATION_WORLD_COLLISION_DISABLE };

void             func_actor_443500_8013253C(Task*);
void             func_actor_443500_80132738(Task*);
static TmdSource _gActor443500Actor113100Model07960;

TaskDesc D_actor_443500_8015873C[2] = {
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_443500_80132738, { .model = &_gActor443500PierceCarradineBody } },
    { { { TASK_BODY_TMD, 192 } }, func_actor_443500_8013253C, { .model = &_gActor443500Actor113100Model07960 } },
};

s32 func_actor_443500_801327E0(Task*, s32, AnimationPlayRequest*, s32);
s32 func_actor_443500_8013297C(Task*, s32, s32, s32);

TaskMessageEntry D_actor_443500_80158754[4] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, func_actor_443500_801327E0 },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_443500_8013297C },
    { TASK_MESSAGE_TABLE_END, NULL },
};

static SVECTOR _gActor443500Collision269B8Normals[2] = {
#include "assets/actor_443500_collision_269B8_normals.inc"
};

static SVECTOR _gActor443500Collision269B8Verts[6] = {
#include "assets/actor_443500_collision_269B8_verts.inc"
};

static WorldCollisionGridFace _gActor443500Collision269B8Faces[2] = {
#include "assets/actor_443500_collision_269B8_faces.inc"
};

static s16 _gActor443500Collision269B8Cells[4] = {
#include "assets/actor_443500_collision_269B8_cells.inc"
};

#define GRID_CELL(i) (&_gActor443500Collision269B8Cells[i])
static s16* _gActor443500Collision269B8Table[1] = {
#include "assets/actor_443500_collision_269B8_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_actor_443500_801587D8 = { NULL, _gActor443500Collision269B8Normals, _gActor443500Collision269B8Verts, _gActor443500Collision269B8Faces, _gActor443500Collision269B8Table, -0x3EF8, -5156, 1, 1, 4000, 2 };

void func_actor_443500_80131E3C(s32 arg0)
{
    if (arg0 != 0) {
        Gp_CapFile = 0;
        Gp_LoadCapFile(1);
        func_800E6D4C(0x240, 0x100);
        return;
    }
    Gp_ResetCap();
}

void func_actor_443500_80131E84(s32 arg0)
{
    if (gameFlagGetNibble(GAME_FLAG_NEO_ARK_POWER_PLANT_2_CLEARED) > 0) {
        if (arg0 != 0) {
            Gp_CapFile = 0;
            Gp_LoadCapFile(1);
            func_800E6D4C(0x240, 0x100);
            return;
        }
        Gp_ResetCap();
    }
}

void func_actor_443500_80131EE4(void)
{
    Gp_RunCapCmd(gameFlagGetNibble(GAME_FLAG_NEO_ARK_POWER_PLANT_2_CLEARED) == 0 ? 6 : 9, 0);
}

void func_actor_443500_80131F18(void)
{
    Task_SpawnFromTable(&D_shelter_r47_80187618, 0, 1, 0);
    Gp_MsgPlayer3F3(0);
    Gp_MsgPlayerWeapon(0);
}

void func_actor_443500_80131F58(void)
{
    Task_SpawnFromTable(&D_actor_443500_80140E38, 0, 0, 0);
}

void func_actor_443500_80131F88(Task* arg0)
{
    s16 temp_v0;
    s32 temp_a0;

    temp_a0 = (((0x1E - arg0->killCountdown) * 0xFF) / 30) & 0xFF;
    Fade_DrawOverlay(temp_a0, temp_a0, temp_a0, GPU_BLEND_SUBTRACT);
    temp_v0             = (u16)arg0->killCountdown + 1;
    arg0->killCountdown = temp_v0;
    if (temp_v0 >= 0x1E) {
        taskKill(arg0);
    }
}

void func_actor_443500_8013201C(s16 arg0)
{
    Gp_StartCapSlot(5, 1, arg0);
}

/// Applies the 0xFF-terminated area record list at `D_shelter_r47_8018A638` through
/// `Gp_ApplyAreaRecs`. It is reached only through the function pointers in
/// the actor's data.
void func_actor_443500_80132048(void)
{
    Gp_ApplyAreaRecs(D_shelter_r47_8018A638);
}

void func_actor_443500_8013206C(s8 arg0)
{
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = arg0;
}

/// Spawn handler: allocates the work block, seeds its head from the parent
/// model, starts the actor's child task and copies the location it spawns over
/// from the session key onto that child's model, then installs the animation
/// table, the exit callback and the tick handler.
static void func_actor_443500_80132078(Task* task)
{
    _Actor443500PierceCarradineWork* work;
    GameLocationKey                  key;
    GameLocationKey*                 sessionKey;
    u8                               areaByte0;
    AreaVariant*                     layout;
    AreaPlacement*                   entry;
    TmdObject*                       model;
    Task*                            spawned;
    s32                              idx;
    u32                              raw;

    work = memCalloc(sizeof(_Actor443500PierceCarradineWork), 0);
    if (work == NULL) {
        enemyTaskExit(task);
        return;
    }
    task->work            = work;
    work->model.animId    = ACTOR_MODEL_STATE_NONE;
    work->model.bank      = ACTOR_MODEL_STATE_NONE;
    work->freeCountdown   = -1;
    work->savedModelFlags = task->extra.tmd->flags;
    spawned               = Task_SpawnFromTable(D_actor_443500_8015873C, 1, 4, task);
    if (spawned != NULL) {
        sessionKey = &gGameSession->location.loc;
        raw        = ((Enemy*)task->spawnArg2.pointer)->placeKey;
        model      = spawned->extra.tmd;
        key.stage  = sessionKey->stage;
        key.area   = sessionKey->area;
        key.room   = sessionKey->room;
        areaByte0  = sessionKey->view;
        idx        = raw >> 12;
        key.view   = areaByte0;
        areaSyncLocationVariant(&key);
        layout = Gp_GetNestedAreaRec(&key);
        /* offset + base, not `&layout->placements[idx]`: the ROM adds the scaled
           index onto the table (`addu s0, s0, v0`). */
        entry                    = gpAreaPlaceAt(layout->placements, idx);
        model->texturePageOffset = entry->texturePageOffset;
        model->clutRowOffset     = entry->clutRowOffset;
        if (model->buffer != NULL) {
            tmdProcessStream(model);
            tmdProcessStream(model);
        }
    }
    func_actor_443500_8013297C(task, ACTOR_MESSAGE_SET_MODEL_DRAW, 0, 0);
    func_actor_443500_801327E0(task, ACTOR_MESSAGE_PLAY_ANIMATION, &D_actor_443500_80158728, 0);
    func_actor_443500_801327C4(task);
    task->msgTable     = D_actor_443500_80158754;
    task->exitCallback = func_actor_443500_801327A4;
    task->state++;
}

/// Per-frame tick: while the view is live and idle, views 0..3 hide the model
/// (saving `TmdObject::flags` into `savedModelFlags`) and views 4..5 restore that
/// saved word, showing the model through message 0x7D5 when flag 0x83 is set.
/// Ticks animation slots 1..0x13 once `model.ticking` is latched, restarting 0x7D3
/// when slot 1 reports the clip ended. The `model.animId == 0x1C` path is the
/// default clip's sound: `loopTicks` counts to 0xF for a Type6 (views 4/5) or
/// Type7 (view 3) cue, TypeA otherwise while the view is ready, and resets when
/// slot 1 reports `ANIMATION_SLOT_FOLLOWED_JUMP`. A visible model gets a ground
/// shadow and a rebuilt child-part matrix; `freeCountdown` then counts down to free
/// the buffers.
static void func_actor_443500_801321F0(Task* task)
{
    _Actor443500PierceCarradineWork* work;
    TmdObject*                       extra;
    VECTOR3                          pos;
    s32                              i;
    u8                               view;

    extra = task->extra.tmd;
    work  = task->work;
    if (gGameSession->viewReady != 0 && gGameSession->eventState == 0 &&
        gGameSession->cutsceneHold == 0) {
        view = gGameSession->location.loc.view;
        if (view < 4) {
            work->savedModelFlags = extra->flags;
            extra->flags          = extra->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW;
        } else if (view < 6) {
            if (gameFlagGetNibble(GAME_FLAG_083) > 0) {
                func_actor_443500_80132A68(0);
                func_actor_443500_8013297C(task, ACTOR_MESSAGE_SET_MODEL_DRAW, 1, 0);
            }
            extra->flags = work->savedModelFlags;
        }
    }
    if (work->model.ticking != 0) {
        for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
            animationTickSlot(&work->rig.anim, i);
        }
        if (gGameSession->eventState == 0 && (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY)) {
            func_actor_443500_801327E0(task, ACTOR_MESSAGE_PLAY_ANIMATION, &D_actor_443500_80158728, 0);
        }
    }
    if (work->model.animId == 0x1C) {
        work->loopTicks++;
        if (work->loopTicks == ACTOR_443500_PIERCE_CARRADINE_LOOP_SOUND_TICK) {
            switch (gGameSession->location.loc.view) {
                case 5:
                    SndEvt_EnqueueType6(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R47, 1), 9, 0);
                    break;
                case 4:
                    SndEvt_EnqueueType6(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R47, 1), -0xA, 0x40);
                    break;
                case 3:
                    SndEvt_EnqueueType7(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R47, 1), 0x1E);
                    break;
            }
        } else if (gGameSession->viewReady != 0) {
            switch (gGameSession->location.loc.view) {
                case 5:
                    SndEvt_EnqueueTypeA(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R47, 1), 9, 0);
                    break;
                case 4:
                    SndEvt_EnqueueTypeA(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R47, 1), -0xA, 0x40);
                    break;
                case 3:
                    SndEvt_EnqueueType7(SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R47, 1), 0x1E);
                    break;
            }
        }
        if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_FOLLOWED_JUMP) {
            work->loopTicks = 0;
        }
    }
    if (!(extra->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        if (func_800EA1A8(MATRIX_TRANS(&task->extra.tmd->coords[1].workm), &pos) != 0) {
            Gp_DrawEffGroundQuad(&pos, 0x300, gRoomEffectState->groundShadowShade);
        }
        if (gGameSession->viewReady != 0) {
            actorRenderComposeCoord(&task->extra.tmd->coords[1]);
            func_800D7A9C(extra, (VECTOR*)task->extra.tmd->coords[1].workm.t, 0, 3);
        }
    }
    if (work->freeCountdown >= 0) {
        if (work->freeCountdown == 0) {
            Tmd_FreeBuffers(extra);
        }
        work->freeCountdown--;
    }
}

/// Dispatcher of the actor's child task: runs its current state handler from
/// `D_actor_443500_80131E24`, copying the table onto the stack before the call.
void func_actor_443500_8013253C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_443500_80131E24;
    sp.funcs[task->state](task);
}

#include "../../shared/model_placement_attach.inc.c"

#include "../../shared/model_placement_mirror_parent.inc.c"

/// Per-frame dispatcher of the main task: runs its spawn, tick or exit state
/// from `D_actor_443500_80131E30`, skipping the frame while `gSceneCombatState.actorControl` is
/// set.
void func_actor_443500_80132738(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_443500_80131E30;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        sp.funcs[task->state](task);
    }
}

/// Exit callback the spawn handler installs, and the third state of the main
/// task: hands the task to `enemyTaskExit`.
static void func_actor_443500_801327A4(Task* arg0)
{
    enemyTaskExit(arg0);
}

/// Points the model's `TmdObject::lightMtx` / `colorMtx` at the work block's
/// own `model.light` / `model.color` matrices, so the actor draws with its own lighting.
static void func_actor_443500_801327C4(Task* task)
{
    TmdObject*                       ext;
    _Actor443500PierceCarradineWork* work;

    ext           = task->extra.tmd;
    work          = task->work;
    ext->lightMtx = &work->model.light;
    ext->colorMtx = &work->model.color;
}

/// Applies the requested animation bank and clip to this actor's rig.
///
/// A changed bank installs its set table. The requested clip is applied to the slots.
/// Blends an already ticking rig when requested, using a whole-frame duration;
/// otherwise resets the slots before ticking them.
s32 func_actor_443500_801327E0(Task* task, s32 anim, AnimationPlayRequest* params, s32 arg3)
{
    _Actor443500PierceCarradineWork* work;
    TmdObject*                       ext;
    s32                              i;

    work = task->work;
    ext  = task->extra.tmd;
    if (params->source.index != work->model.bank) {
        work->model.bank = params->source.index;
        animationInitContext(&work->rig.anim, D_actor_443500_80158724[work->model.bank], ext, work->rig.poses,
                             work->rig.slots);
    }
    work->model.animId = params->animationId;
    if (params->blend != ANIMATION_BLEND_RESET && work->model.ticking != 0) {
        for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
            animationSeekSlotWithBlend(&work->rig.anim, i, work->model.animId, 0, params->blendFrames);
        }
    } else {
        for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
            animationResetSlot(&work->rig.anim, i, work->model.animId);
        }
    }
    for (i = 1; i < ARRAY_SIZE(work->rig.slots); i++) {
        animationTickSlot(&work->rig.anim, i);
    }
    work->model.ticking = 1;
    work->loopTicks     = 0;
    return 0;
}

#include "../../shared/actor_messages_place_euler.inc.c"

/// Message-0x7D5 handler: the four-way switch on `mode` over the `TmdObject`
/// parked in `Task::extra`. `mode` drives `TmdObject::flags`: bit 0x80 marks
/// the actor hidden, and `TMD_OBJECT_SKIP_AUTO_BUFFER` opts it out of
/// missing-buffer recovery.
///
///   mode 0  hide, clear `TMD_OBJECT_SKIP_AUTO_BUFFER`
///   mode 1  show, `Tmd_AllocBuffers`, clear `TMD_OBJECT_SKIP_AUTO_BUFFER`
///   mode 2  hide, latch `mode` in the work block's `freeCountdown`, set `TMD_OBJECT_SKIP_AUTO_BUFFER`
///   mode 3  show, set `TMD_OBJECT_SKIP_AUTO_BUFFER`
///
/// Any other mode returns 1; the four known ones return 0. Either way the
/// resulting flags are mirrored onto the work block's `savedModelFlags`, the slot
/// the spawn handler seeds from the model's own flags. `freeCountdown` is the word
/// the spawn handler seeds to -1 and the tick counts down to free the buffers.
/// `anim` and `arg3` are unused -- the dispatch passes four arguments.
s32 func_actor_443500_8013297C(Task* task, s32 anim, s32 mode, s32 arg3)
{
    TmdObject*                       obj;
    s32                              ret;
    _Actor443500PierceCarradineWork* work;

    obj  = task->extra.tmd;
    work = task->work;
    ret  = 0;
    switch (mode) {
        case 0:
            obj->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            obj->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 1:
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            Tmd_AllocBuffers(obj);
            obj->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 2:
            obj->flags         |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->freeCountdown = mode;
            obj->flags         |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case 3:
            obj->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            obj->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            ret = 1;
            break;
    }
    work->savedModelFlags = obj->flags;
    return ret;
}

/// Copy the overlay's layout template `D_actor_443500_801587D8` into the live
/// table at `D_shelter_r47_8018828C`. When `arg0` is nonzero, shift the six live target
/// positions by `(0, 0x7D0, 0)` afterwards. The per-frame tick calls this
/// with 0 before showing the model.
static void func_actor_443500_80132A68(s32 arg0)
{
    WorldCollisionGrid* dst;
    WorldCollisionGrid* src;
    SVECTOR             d;
    s32                 i;

    dst = &D_shelter_r47_8018828C;
    src = &D_actor_443500_801587D8;

    for (i = 0; i < 2; i++) {
        dst->normals[i].vx = src->normals[i].vx;
        dst->normals[i].vy = src->normals[i].vy;
        dst->normals[i].vz = src->normals[i].vz;
        dst->faces[i]      = src->faces[i];
    }

    for (i = 0; i < 6; i++) {
        dst->vertices[i].vx = src->vertices[i].vx;
        dst->vertices[i].vy = src->vertices[i].vy;
        dst->vertices[i].vz = src->vertices[i].vz;
    }

    if (arg0 == 0) {
        d.vx = 0;
        d.vy = 0;
    } else {
        d.vx = 0;
        d.vy = 0x7D0;
    }
    d.vz = 0;

    for (i = 0; i < 6; i++) {
        dst->vertices[i].vx += d.vx;
        dst->vertices[i].vy += d.vy;
        dst->vertices[i].vz += d.vz;
    }
}
