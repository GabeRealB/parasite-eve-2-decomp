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
#include "gameplay/actor_presentation.h"
#include "gameplay/player_actor.h"
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

/// Loaded CAP resource and texture origin used by this package's event scripts.
enum {
    ACTOR_443500_EVENT_CAP_FILE_ORDINAL   = 1,
    ACTOR_443500_EVENT_CAP_TEXTURE_VRAM_X = 576,
    ACTOR_443500_EVENT_CAP_TEXTURE_VRAM_Y = 256,
};

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

static void _actor443500InitPierce(Task* task);
static void _modelPlacementMirrorParentDrawFlags(Task* childTask);
static void _actor443500TickPierce(Task* task);
static void _actor443500ExitPierce(Task* task);
static void _actor443500BindModelLighting(Task* task);
static s32  _actor443500PlayAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg);
static s32  _actor443500SetModelDraw(Task* task, s32 messageId, s32 drawMode, s32 unusedArg);
static void _actor443500InstallPierceCollision(s32 shiftY);
static void _actor443500ChildModelTask(Task* task);

/// State table of the actor's child task (`TaskDesc` entry 1): setup, the
/// per-frame active-draw and buffer-flag mirror and `taskKill`.
static const TaskFuncTable3 D_actor_443500_80131E24 = {
    { _modelPlacementAttachChild, _modelPlacementMirrorParentDrawFlags, taskKill }
};

/// State table of the actor's main task (`TaskDesc` entry 0): the spawn
/// handler, the per-frame tick and the exit callback.
static const TaskFuncTable3 D_actor_443500_80131E30 = {
    { _actor443500InitPierce, _actor443500TickPierce, _actor443500ExitPierce }
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

/// The bank table `_actor443500PlayAnimation` re-seeds the work block's slot
/// array off: one entry, the animation bank the default preset's `source.index` of
/// zero selects.
extern AnimationSet*  D_actor_443500_80158694[36];
extern AnimationSet** D_actor_443500_80158724[1];

extern WorldCollisionGrid D_actor_443500_801587D8;

static TmdSource _gActor443500PierceCarradineBody;
static void      _actor443500PierceTask(Task* task);

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
static void                     _actor443500SelectEventCap(s32 useEventCap);
static void                     _actor443500SelectEventCapAfterPowerPlantClear(s32 useEventCap);
static void                     _actor443500RunPowerPlantProgressCap(void);
static void                     _actor443500OpenTimedMapTerminal(void);
static void                     _actor443500StartFadeFromBlack(void);
static void                     _actor443500StartCapCommand5(s16 variantKey);
static void                     _actor443500ApplyShelterAreaUpdates(void);
static void                     _actor443500SetSceneEvent(s8 sceneEvent);

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

static void _actor443500FadeFromBlackTask(Task* task);

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

TaskDesc D_actor_443500_80140E38 = { { { TASK_BODY_NONE, 192 } }, _actor443500FadeFromBlackTask, { .value = 0 } };

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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor443500SelectEventCap }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor443500SelectEventCap }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor443500SelectEventCap }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_443500_80141D9C[137] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor443500StartFadeFromBlack }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = 0x542F0001 }, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_443500_80140FE8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS8 = _actor443500SetSceneEvent }, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor443500SelectEventCap }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor443500OpenTimedMapTerminal }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor443500SelectEventCap }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_443500_80142A74[18] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = 0x542F0001 }, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS8 = _actor443500SetSceneEvent }, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_443500_801411FC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_443500_80141514 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor443500StartCapCommand5 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor443500SelectEventCap }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_443500_80142C24[73] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor443500StartFadeFromBlack }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_443500_80140FE8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor443500SelectEventCap }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor443500SelectEventCap }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor443500ApplyShelterAreaUpdates }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor443500SelectEventCap }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor443500ApplyShelterAreaUpdates }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_443500_80143494[10] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_443500_80140FE8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_443500_80141004 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_443500_80141458 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor443500SelectEventCapAfterPowerPlantClear }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor443500RunPowerPlantProgressCap }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor443500SelectEventCap }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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

static TmdSource _gActor443500Actor113100Model07960;

TaskDesc D_actor_443500_8015873C[2] = {
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, _actor443500PierceTask, { .model = &_gActor443500PierceCarradineBody } },
    { { { TASK_BODY_TMD, 192 } }, _actor443500ChildModelTask, { .model = &_gActor443500Actor113100Model07960 } },
};

TaskMessageEntry D_actor_443500_80158754[4] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actor443500PlayAnimation },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor443500SetModelDraw },
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

/// Selects the event CAP resource, or restores ordinary room CAP selection.
///
/// Nonzero `useEventCap` clears the current selection before selecting loaded
/// data resource 1 and its texture origin (576, 256) in VRAM pixels. Zero resets
/// CAP to its default resource. Playback must be stopped and the selected CAP
/// data and glyph texture must stay loaded while in use; this performs no I/O.
static void _actor443500SelectEventCap(s32 useEventCap)
{
    if (useEventCap != 0) {
        Gp_CapFile = NULL;
        capSelectLoadedFile(ACTOR_443500_EVENT_CAP_FILE_ORDINAL);
        capSetTexturePage(ACTOR_443500_EVENT_CAP_TEXTURE_VRAM_X, ACTOR_443500_EVENT_CAP_TEXTURE_VRAM_Y);
        return;
    }
    capReset();
}

/// Selects or resets event CAP only after Neo Ark power plant 2 has been cleared.
///
/// Otherwise leaves CAP untouched. Selection and resource lifetime requirements
/// follow `_actor443500SelectEventCap`; zero resets and nonzero selects event CAP.
static void _actor443500SelectEventCapAfterPowerPlantClear(s32 useEventCap)
{
    if (gameFlagGetNibble(GAME_FLAG_NEO_ARK_POWER_PLANT_2_CLEARED) > 0) {
        if (useEventCap != 0) {
            Gp_CapFile = NULL;
            capSelectLoadedFile(ACTOR_443500_EVENT_CAP_FILE_ORDINAL);
            capSetTexturePage(ACTOR_443500_EVENT_CAP_TEXTURE_VRAM_X, ACTOR_443500_EVENT_CAP_TEXTURE_VRAM_Y);
            return;
        }
        capReset();
    }
}

/// Runs CAP command 6 before power plant 2 is cleared, or command 9 afterwards.
///
/// Starts in the current display. The active CAP resource must contain the
/// selected slot and remain loaded through playback; busy CAP does not restart.
static void _actor443500RunPowerPlantProgressCap(void)
{
    enum {
        ACTOR_443500_CAP_BEFORE_POWER_PLANT_CLEAR = 6,
        ACTOR_443500_CAP_AFTER_POWER_PLANT_CLEAR  = 9,
    };
    capRunCommand(gameFlagGetNibble(GAME_FLAG_NEO_ARK_POWER_PLANT_2_CLEARED) == 0 ? ACTOR_443500_CAP_BEFORE_POWER_PLANT_CLEAR : ACTOR_443500_CAP_AFTER_POWER_PLANT_CLEAR,
                  CAP_PLAYBACK_IN_PLACE);
}

/// Starts the room's timed map-terminal display and holds the hidden player.
///
/// Shelter room 47 and the player task must be live. The room task owns the
/// terminal's lifetime and later restores player control and presentation.
/// The player is hidden and held even if the terminal task cannot be allocated.
static void _actor443500OpenTimedMapTerminal(void)
{
    enum { ACTOR_443500_MAP_TERMINAL_TIMED = 1 };
    taskSpawnFromTable(&D_shelter_r47_80187618, 0, ACTOR_443500_MAP_TERMINAL_TIMED, 0);
    playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
}

/// Starts this package's 30-tick fade from black, if a task can be allocated.
///
/// Keep the actor overlay loaded until the spawned callback finishes.
static void _actor443500StartFadeFromBlack(void)
{
    taskSpawnFromTable(&D_actor_443500_80140E38, 0, 0, 0);
}

/// Draws a subtractive black overlay that fades away over 30 task ticks.
///
/// Requires a zero-initialized bodyless task and the current GPU packet arena.
/// `killCountdown` counts elapsed ticks here: the last draw uses tick 29, then
/// the callback kills its task. The signed counter retains 16-bit wrap behavior.
static void _actor443500FadeFromBlackTask(Task* task)
{
    enum { ACTOR_443500_FADE_TICKS    = 30,
           ACTOR_443500_FULL_DARKNESS = 255 };
    s16 nextTick;
    s32 darkness;

    darkness = (((ACTOR_443500_FADE_TICKS - task->killCountdown) * ACTOR_443500_FULL_DARKNESS) /
                ACTOR_443500_FADE_TICKS) &
               0xFF;
    fadeDrawOverlay(darkness, darkness, darkness, GPU_BLEND_SUBTRACT);
    nextTick            = (u16)task->killCountdown + 1;
    task->killCountdown = nextTick;
    if (nextTick >= ACTOR_443500_FADE_TICKS) {
        taskKill(task);
    }
}

/// Starts CAP command slot 5 with the supplied variant and a display transition.
///
/// `variantKey` is a signed halfword; 0..255 can match CAP record keys. Requires
/// a live relocated command table containing slot 5 and its playback resources.
/// Busy CAP does nothing; the start result is discarded.
static void _actor443500StartCapCommand5(s16 variantKey)
{
    enum { ACTOR_443500_CAP_VARIANT_COMMAND = 5 };
    capStartSequenceSlot(ACTOR_443500_CAP_VARIANT_COMMAND, CAP_PLAYBACK_DISPLAY_TRANSITION, variantKey);
}

/// Applies the Shelter saved-area layouts and map marks after the Pierce event.
///
/// Requires the loaded room-47 record list, live save and saved-area tables.
/// Accepted records also discard saved enemy poses; save-mode policies select
/// the appropriate layouts. Used by both normal and skip event scripts.
static void _actor443500ApplyShelterAreaUpdates(void)
{
    areaApplySavedUpdates(D_shelter_r47_8018A638);
}

/// Stores the saved scene-event byte used to select stage music.
///
/// The event scripts pass 15. This only updates the live save; music selection
/// reads the event later. The signed-byte callback preserves its argument width.
static void _actor443500SetSceneEvent(s8 sceneEvent)
{
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = sceneEvent;
}

/// Applies a held model's placement texture offsets to both primitive-buffer halves.
///
/// Both borrowed pointers, model source and existing buffer capacities must
/// stay live through this call; a NULL buffer changes only the stored offsets.
static inline void _actor443500ApplyPiercePlacementTextures(TmdObject* model, const AreaPlacement* placement)
{
    model->texturePageOffset = placement->texturePageOffset;
    model->clutRowOffset     = placement->clutRowOffset;
    if (model->buffer != NULL) {
        tmdBuildBufferHalf(model);
        tmdBuildBufferHalf(model);
    }
}

/// Initializes Pierce's hidden body, held model and animation messages.
///
/// Requires the twenty-part body and live owning enemy in spawnArg2.pointer.
/// Allocates primary-heap work owned by the task; allocation failure tears down
/// the enemy. An optional held-model child at part 4 inherits texture offsets
/// from the enemy's area placement. Work-owned lighting survives until teardown;
/// initial animation, messages and exit handling are installed before state 1.
static void _actor443500InitPierce(Task* task)
{
    enum {
        ACTOR_443500_PIERCE_CHILD_DESCRIPTOR  = 1,
        ACTOR_443500_PIERCE_CHILD_PART        = 4,
        ACTOR_443500_PIERCE_NO_BUFFER_RELEASE = -1,
    };

    _Actor443500PierceCarradineWork* work;
    GameLocationKey                  locationKey;
    const GameLocationKey*           sessionLocation;
    u8                               viewIndex;
    AreaVariant*                     areaVariant;
    AreaPlacement*                   placement;
    TmdObject*                       heldModel;
    Task*                            childTask;
    s32                              placementIndex;
    u32                              placementKey;
    Enemy*                           enemy;

    work = memCalloc(sizeof(*work), false);
    if (work == NULL) {
        enemyTaskExit(task);
        return;
    }
    task->work            = work;
    work->model.animId    = ACTOR_MODEL_STATE_NONE;
    work->model.bank      = ACTOR_MODEL_STATE_NONE;
    work->freeCountdown   = ACTOR_443500_PIERCE_NO_BUFFER_RELEASE;
    work->savedModelFlags = task->extra.tmd->flags;
    childTask             = taskSpawnFromTable(D_actor_443500_8015873C, ACTOR_443500_PIERCE_CHILD_DESCRIPTOR, ACTOR_443500_PIERCE_CHILD_PART, task);
    if (childTask != NULL) {
        sessionLocation   = &gGameSession->location.loc;
        enemy             = task->spawnArg2.pointer;
        placementKey      = enemy->placeKey;
        heldModel         = childTask->extra.tmd;
        locationKey.stage = sessionLocation->stage;
        locationKey.area  = sessionLocation->area;
        locationKey.room  = sessionLocation->room;
        viewIndex         = sessionLocation->view;
        placementIndex    = placementKey >> ENEMY_PLACE_INDEX_SHIFT;
        locationKey.view  = viewIndex;
        areaSyncLocationVariant(&locationKey);
        areaVariant = areaGetVariant(&locationKey);
        // The held model inherits its owner's area-placement texture offsets.
        placement = gpAreaPlaceAt(areaVariant->placements, placementIndex);
        _actor443500ApplyPiercePlacementTextures(heldModel, placement);
    }
    _actor443500SetModelDraw(task, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_HIDE, 0);
    _actor443500PlayAnimation(task, ACTOR_MESSAGE_PLAY_ANIMATION, &D_actor_443500_80158728, 0);
    _actor443500BindModelLighting(task);
    task->msgTable     = D_actor_443500_80158754;
    task->exitCallback = _actor443500ExitPierce;
    task->state++;
}

/// Advances Pierce's view-dependent presentation, animation and buffer lifetime.
///
/// Requires the initialized twenty-part TMD task, its allocated work, live clip
/// data and shelter room 47's writable collision grid. Idle ready views 0..3
/// save and hide the model; views 4..5 restore its saved flags and, when flag
/// `GAME_FLAG_083` is nonzero, install its collision and request visible drawing.
/// Slots 1..19 tick after a play request. Outside an event, a slot-1 boundary
/// restarts the default request. Clip 28 drives a sound at loop tick 15 and
/// resets that counter on a slot-1 control jump. Visible models receive a
/// ground shadow and updated lighting; pending buffer release continues even
/// while the model is hidden. The room overlay and sound bank must stay loaded.
static void _actor443500TickPierce(Task* task)
{
    enum {
        ACTOR_443500_DEFAULT_ANIMATION       = 28,
        ACTOR_443500_DEFAULT_CLIP_SOUND      = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R47, 1),
        ACTOR_443500_LOOP_SOUND_FADE_UPDATES = 30,
        ACTOR_443500_SHADOW_HALF_SIZE        = 768,
    };
    _Actor443500PierceCarradineWork* work;
    TmdObject*                       model;
    VECTOR3                          groundPoint;
    s32                              slotIndex;
    u8                               view;

    model = task->extra.tmd;
    work  = task->work;
    // Camera visibility changes are suspended while an event holds the scene.
    if (gGameSession->viewReady != 0 && gGameSession->eventState == 0 &&
        gGameSession->cutsceneHold == 0) {
        view = gGameSession->location.loc.view;
        if (view < 4) {
            work->savedModelFlags = model->flags;
            model->flags          = model->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW;
        } else if (view < 6) {
            if (gameFlagGetNibble(GAME_FLAG_083) > 0) {
                _actor443500InstallPierceCollision(0);
                _actor443500SetModelDraw(task, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_SHOW, 0);
            }
            model->flags = work->savedModelFlags;
        }
    }
    if (work->model.ticking != 0) {
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationTickSlot(&work->rig.anim, slotIndex);
        }
        if (gGameSession->eventState == 0 && (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY)) {
            _actor443500PlayAnimation(task, ACTOR_MESSAGE_PLAY_ANIMATION, &D_actor_443500_80158728, 0);
        }
    }
    // The cue follows animation loop timing; camera views select its audible mix.
/// Queues the default clip's start or mix request for the current camera view.
///
/// `queueSound` must be the start or mix function identifier; it is called at
/// most once, in views 4 or 5. View 3 instead fades the sound script. Captures
/// `gGameSession` and this block's sound ID/fade constants, requires the live
/// shelter room 47 sound bank, and is undefined after the two uses. Expands to
/// a standalone switch statement; no caller expression is evaluated twice.
#define ACTOR_443500_QUEUE_DEFAULT_CLIP_SOUND(queueSound)                                                   \
    switch (gGameSession->location.loc.view) {                                                              \
        case 5:                                                                                             \
            (queueSound)(ACTOR_443500_DEFAULT_CLIP_SOUND, 9, 0);                                            \
            break;                                                                                          \
        case 4:                                                                                             \
            (queueSound)(ACTOR_443500_DEFAULT_CLIP_SOUND, -10, 64);                                         \
            break;                                                                                          \
        case 3:                                                                                             \
            sndEvtRequestScriptStop(ACTOR_443500_DEFAULT_CLIP_SOUND, ACTOR_443500_LOOP_SOUND_FADE_UPDATES); \
            break;                                                                                          \
    }
    if (work->model.animId == ACTOR_443500_DEFAULT_ANIMATION) {
        work->loopTicks++;
        if (work->loopTicks == ACTOR_443500_PIERCE_CARRADINE_LOOP_SOUND_TICK) {
            ACTOR_443500_QUEUE_DEFAULT_CLIP_SOUND(sndEvtRequestScriptStart);
        } else if (gGameSession->viewReady != 0) {
            ACTOR_443500_QUEUE_DEFAULT_CLIP_SOUND(sndEvtRequestScriptMix);
        }
#undef ACTOR_443500_QUEUE_DEFAULT_CLIP_SOUND
        if (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_FOLLOWED_JUMP) {
            work->loopTicks = 0;
        }
    }
    // Keep shadow projection before the current coordinate composition.
    if (!(model->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        if (worldCollisionProjectGroundPoint(MATRIX_TRANS(&task->extra.tmd->coords[1].workm), &groundPoint) != 0) {
            effectDrawGroundShadow(&groundPoint, ACTOR_443500_SHADOW_HALF_SIZE, gRoomEffectState->groundShadowShade);
        }
        if (gGameSession->viewReady != 0) {
            actorRenderComposeCoord(&task->extra.tmd->coords[1]);
            worldCoordSetModelLighting(model, task->extra.tmd->coords[1].workm.t, 0, 3);
        }
    }
    if (work->freeCountdown >= 0) {
        if (work->freeCountdown == 0) {
            tmdFreePrimitiveBuffer(model);
        }
        work->freeCountdown--;
    }
}

/// Dispatches the attached child model's setup, flag-mirroring or kill state.
///
/// Requires a live TMD child with `state` in 0..2. Setup borrows its parent task
/// from `spawnArg2.pointer` and the parent part index from `spawnArg1.value`;
/// the parent model and its selected part must outlive the child's drawing.
static void _actor443500ChildModelTask(Task* task)
{
    TaskFuncTable3 states;

    states = D_actor_443500_80131E24;
    states.funcs[task->state](task);
}

#include "../../shared/model_placement_attach.inc.c"

#include "../../shared/model_placement_mirror_parent.inc.c"

/// Dispatches Pierce's setup, frame update or exit while scene actors are running.
///
/// Requires a live TMD task with `state` in 0..2 and this overlay loaded.
/// Setup and exit require the live enemy record in `spawnArg2.pointer`.
/// Paused actor control skips every state, including setup and exit.
static void _actor443500PierceTask(Task* task)
{
    TaskFuncTable3 states;

    states = D_actor_443500_80131E30;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        states.funcs[task->state](task);
    }
}

/// Releases Pierce's task through the enemy-task teardown path.
///
/// Serves both exit state 2 and the installed exit callback; task and owned
/// work must still be live on entry; `spawnArg2.pointer` must be its live enemy
/// record. Neither may be used afterwards.
static void _actor443500ExitPierce(Task* task)
{
    enemyTaskExit(task);
}

/// Lends the work block's lighting matrices to Pierce's model.
///
/// Requires allocated work and a live TMD object. Its borrowed matrix pointers
/// remain valid only for that work block's lifetime.
static void _actor443500BindModelLighting(Task* task)
{
    TmdObject*                       model;
    _Actor443500PierceCarradineWork* work;

    model           = task->extra.tmd;
    work            = task->work;
    model->lightMtx = &work->model.light;
    model->colorMtx = &work->model.color;
}

/// Applies a clip request to the actor rig and restarts its loop cue.
///
/// Borrows live work, model coordinates and loaded bank data through playback;
/// the read-only request is consumed during this call and must not overlap work.
/// Bank 0 and clips 1..35 are valid. Blend duration counts whole frames.
static inline void _actor443500ApplyAnimationRequest(_Actor443500PierceCarradineWork* work, TmdObject* model,
                                                     const AnimationPlayRequest* request)
{
    enum { ACTOR_443500_FIRST_DRIVEN_SLOT = 1 };
    s32 slotIndex;

    if (request->source.index != work->model.bank) {
        work->model.bank = request->source.index;
        animationInitContext(&work->rig.anim, D_actor_443500_80158724[work->model.bank], model, work->rig.poses,
                             work->rig.slots);
    }
    work->model.animId = request->animationId;
    if (request->blend != ANIMATION_BLEND_RESET && work->model.ticking != 0) {
        for (slotIndex = ACTOR_443500_FIRST_DRIVEN_SLOT; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationSeekSlotWithBlend(&work->rig.anim, slotIndex, work->model.animId, 0, request->blendFrames);
        }
    } else {
        for (slotIndex = ACTOR_443500_FIRST_DRIVEN_SLOT; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationResetSlot(&work->rig.anim, slotIndex, work->model.animId);
        }
    }
    // Seed the new pose before the next frame and restart its synchronized cue.
    for (slotIndex = ACTOR_443500_FIRST_DRIVEN_SLOT; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
        animationTickSlot(&work->rig.anim, slotIndex);
    }
    work->model.ticking = 1;
    work->loopTicks     = 0;
}

/// Starts the requested clip and restarts Pierce's animation-loop sound timing.
///
/// Handles `ACTOR_MESSAGE_PLAY_ANIMATION` on initialized twenty-part work.
/// `request` is borrowed read-only through dispatch and must not overlap the rig.
/// Bank index must be 0 and clip index 1..35; their data, model coordinates and
/// work-owned slots/poses stay borrowed through playback. Initialize bank to
/// `ACTOR_MODEL_STATE_NONE` before the first request. Slots 1..19 blend when
/// already ticking and requested, using `blendFrames` in whole frames (normally
/// 0..2047), or reset otherwise, then tick once. Even a repeated clip restarts.
/// Ignores message ID, collision choice and fourth argument. Returns 0.
static s32 _actor443500PlayAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg)
{
    _Actor443500PierceCarradineWork* work;
    TmdObject*                       model;

    work  = task->work;
    model = task->extra.tmd;
    _actor443500ApplyAnimationRequest(work, model, request);
    return 0;
}

#include "../../shared/actor_messages_place_euler.inc.c"

/// Sets Pierce's model visibility and automatic-buffer policy.
///
/// Handles `ACTOR_MESSAGE_SET_MODEL_DRAW` on a live TMD task with allocated work.
/// Modes: 0 hide with automatic buffering; 1 show and request a buffer; 2 hide,
/// disable automatic buffering and start a two-tick countdown; 3 show with
/// automatic buffering disabled. The tick observing countdown zero frees the
/// buffer. Later show requests do not cancel a pending release. Mode 1 returns
/// 0 even if allocation fails. Known modes return 0, others 1; every request
/// saves the resulting flags for camera-view restoration. Ignores message ID
/// and fourth argument. Unrelated flags and existing buffers remain intact.
static s32 _actor443500SetModelDraw(Task* task, s32 messageId, s32 drawMode, s32 unusedArg)
{
    enum { ACTOR_443500_DRAW_SHOW_SKIP_AUTO_BUFFER = 3 };
    TmdObject*                       model;
    s32                              result;
    _Actor443500PierceCarradineWork* work;

    model  = task->extra.tmd;
    work   = task->work;
    result = 0;
    switch (drawMode) {
        case ACTOR_MESSAGE_DRAW_HIDE:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            model->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_MESSAGE_DRAW_SHOW:
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            tmdAllocPrimitiveBuffer(model);
            model->flags &= ~TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_MESSAGE_DRAW_HIDE_SKIP_AUTO_BUFFER:
            model->flags       |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            work->freeCountdown = drawMode;
            model->flags       |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        case ACTOR_443500_DRAW_SHOW_SKIP_AUTO_BUFFER:
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
            break;
        default:
            result = 1;
            break;
    }
    work->savedModelFlags = model->flags;
    return result;
}

/// Installs Pierce's two collision faces into shelter room 47's live grid.
///
/// Both overlays must be loaded and the destination arrays writable. Replaces
/// the first two normals/faces and six vertices, retaining the room grid's
/// other geometry, cell indexing and SDK vector pad words. Nonzero `shiftY`
/// adds 2000 world-coordinate units to these vertices' Y after installation;
/// zero uses the template position. Vertex additions narrow to signed 16 bits.
/// The room grid must remain loaded while collision queries use the result.
static void _actor443500InstallPierceCollision(s32 shiftY)
{
    enum { ACTOR_443500_COLLISION_Y_DISPLACEMENT = 2000 };
    WorldCollisionGrid*       roomGrid;
    const WorldCollisionGrid* actorGrid;
    SVECTOR                   displacement;
    s32                       elementIndex;

    roomGrid  = &D_shelter_r47_8018828C;
    actorGrid = &D_actor_443500_801587D8;

    // The room reserves its leading geometry entries for this actor's faces.
    for (elementIndex = 0; elementIndex < ARRAY_SIZE(_gActor443500Collision269B8Faces); elementIndex++) {
        roomGrid->normals[elementIndex].vx = actorGrid->normals[elementIndex].vx;
        roomGrid->normals[elementIndex].vy = actorGrid->normals[elementIndex].vy;
        roomGrid->normals[elementIndex].vz = actorGrid->normals[elementIndex].vz;
        roomGrid->faces[elementIndex]      = actorGrid->faces[elementIndex];
    }

    for (elementIndex = 0; elementIndex < ARRAY_SIZE(_gActor443500Collision269B8Verts); elementIndex++) {
        roomGrid->vertices[elementIndex].vx = actorGrid->vertices[elementIndex].vx;
        roomGrid->vertices[elementIndex].vy = actorGrid->vertices[elementIndex].vy;
        roomGrid->vertices[elementIndex].vz = actorGrid->vertices[elementIndex].vz;
    }

    if (shiftY == 0) {
        displacement.vx = 0;
        displacement.vy = 0;
    } else {
        displacement.vx = 0;
        displacement.vy = ACTOR_443500_COLLISION_Y_DISPLACEMENT;
    }
    displacement.vz = 0;

    for (elementIndex = 0; elementIndex < ARRAY_SIZE(_gActor443500Collision269B8Verts); elementIndex++) {
        roomGrid->vertices[elementIndex].vx += displacement.vx;
        roomGrid->vertices[elementIndex].vy += displacement.vy;
        roomGrid->vertices[elementIndex].vz += displacement.vz;
    }
}
