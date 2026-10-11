#include "actors/actor_443500.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"
#include "actors/task_tables.h"

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
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
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

/// Execution priority of this package's actor and fade tasks.
enum { ACTOR_443500_TASK_PRIORITY = 192 };

/// Pierce's playback bank, fallback clip and room placement index.
///
/// The bank drives slots 1..19. His playback handler ignores the requests'
/// world-collision word; collision geometry is installed separately.
enum {
    ACTOR_443500_PIERCE_ANIMATION_BANK  = 0,
    ACTOR_443500_DEFAULT_ANIMATION      = 28,
    ACTOR_443500_PIERCE_PLACEMENT_INDEX = 0,
};

/// Player clip shared by both terminal-tour bank extensions.
enum { ACTOR_443500_PLAYER_SHARED_CLIP = ANIMATION_BANK_BASE_SET_COUNT + 1 };

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

/// Held-model handlers for task states 0 setup, 1 mirror draw policy and 2 exit.
///
/// There is no terminator; dispatch requires a state in 0..2. The child borrows
/// its parent model's part coordinate and lighting until parent teardown.
static const TaskFuncTable3 _gActor443500ChildModelStates = {
    { _modelPlacementAttachChild, _modelPlacementMirrorParentDrawFlags, taskKill }
};

/// Pierce's main-task handlers for states 0 setup, 1 frame update and 2 exit.
///
/// No terminator or bounds check; the dispatcher requires state 0..2 and
/// skips every handler while scene actor control is paused.
static const TaskFuncTable3 _gActor443500PierceStates = {
    .funcs = { _actor443500InitPierce, _actor443500TickPierce, _actor443500ExitPierce },
};

static TmdSource _gActor443500PierceCarradineBody;
static void      _actor443500PierceTask(Task* task);

static void _actor443500SelectEventCap(s32 useEventCap);
static void _actor443500SelectEventCapAfterPowerPlantClear(s32 useEventCap);
static void _actor443500RunPowerPlantProgressCap(void);
static void _actor443500OpenTimedMapTerminal(void);
static void _actor443500StartFadeFromBlack(void);
static void _actor443500StartCapCommand5(s16 variantKey);
static void _actor443500ApplyShelterAreaUpdates(void);
static void _actor443500SetSceneEvent(s8 sceneEvent);

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

/// Bodyless task recipe for the terminal scenes' 30-tick fade from black.
///
/// Spawn at entry 0 with zero arguments. The task owns its elapsed-tick counter;
/// this overlay must remain loaded until the fade kills the task.
static TaskDesc _gActor443500FadeFromBlackTaskDesc = {
    .header   = { .fields = { TASK_BODY_NONE, ACTOR_443500_TASK_PRIORITY } },
    .callback = _actor443500FadeFromBlackTask,
    .data     = { .value = 0 },
};

/// Aya's extension clips for the opening scene of Pierce's terminal tour.
///
/// Copied to player-bank entries 47..57, including NULL at 47. Clips 48..57
/// borrow this overlay's animation data for playback; the base bank is retained.
static AnimationSet* _gActor443500TourOpeningPlayerAnimationSets[11] = {
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

/// Request to copy the tour-opening clips into Aya's equipped bank extension.
///
/// Copies the complete set-pointer table in 32-bit words. The request is consumed
/// during dispatch; keep this overlay loaded until the copied clips stop playing.
static AnimationBankCopyRequest _gActor443500TourOpeningPlayerBankExtension = {
    .source    = { .sets = _gActor443500TourOpeningPlayerAnimationSets },
    .wordCount = ARRAY_SIZE(_gActor443500TourOpeningPlayerAnimationSets),
};

/// Unplayed clip-1 reset preset beside the tour-opening player requests.
///
/// Bank selector 1 and enabled world collision are retained. Its position in
/// the player request run suggests Aya as receiver; no dispatch proves that.
static AnimationPlayRequest _gActor443500UnusedTourOpeningPlayerClip1Request = {
    .source               = { .index = 1 },
    .animationId          = 1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's shared extension clip with a four-frame blend during the opening tour.
///
/// The event interpreter replaces the stored bank selector with the equipped
/// weapon's bank. Requires the copied extension and restores grid participation.
static AnimationPlayRequest _gActor443500PlayerClip48Blend4Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_PLAYER_SHARED_CLIP,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 4,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Player-bank set indices supplied by the opening-tour extension.
enum {
    ACTOR_443500_TOUR_OPENING_PLAYER_CLIP_49 = ANIMATION_BANK_BASE_SET_COUNT + 2,
    ACTOR_443500_TOUR_OPENING_PLAYER_CLIP_50 = ANIMATION_BANK_BASE_SET_COUNT + 3,
    ACTOR_443500_TOUR_OPENING_PLAYER_CLIP_51 = ANIMATION_BANK_BASE_SET_COUNT + 4,
    ACTOR_443500_TOUR_OPENING_PLAYER_CLIP_52 = ANIMATION_BANK_BASE_SET_COUNT + 5,
    ACTOR_443500_TOUR_OPENING_PLAYER_CLIP_53 = ANIMATION_BANK_BASE_SET_COUNT + 6,
    ACTOR_443500_TOUR_OPENING_PLAYER_CLIP_54 = ANIMATION_BANK_BASE_SET_COUNT + 7,
    ACTOR_443500_TOUR_OPENING_PLAYER_CLIP_55 = ANIMATION_BANK_BASE_SET_COUNT + 8,
    ACTOR_443500_TOUR_OPENING_PLAYER_CLIP_56 = ANIMATION_BANK_BASE_SET_COUNT + 9,
    ACTOR_443500_TOUR_OPENING_PLAYER_CLIP_57 = ANIMATION_BANK_BASE_SET_COUNT + 10,
};

/// Aya's opening-tour clip 49 preset, resetting playback without a pose blend.
///
/// Requires the opening-tour extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TourOpeningPlayerClip49Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TOUR_OPENING_PLAYER_CLIP_49,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's opening-tour clip 50 preset, blending over 4 frames.
///
/// Requires the opening-tour extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TourOpeningPlayerClip50Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TOUR_OPENING_PLAYER_CLIP_50,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 4,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's opening-tour clip 51 preset, blending over 4 frames.
///
/// Requires the opening-tour extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TourOpeningPlayerClip51Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TOUR_OPENING_PLAYER_CLIP_51,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 4,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's opening-tour clip 52 preset, blending over 4 frames.
///
/// Requires the opening-tour extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TourOpeningPlayerClip52Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TOUR_OPENING_PLAYER_CLIP_52,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 4,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's opening-tour clip 53 preset, blending over 4 frames.
///
/// Requires the opening-tour extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TourOpeningPlayerClip53Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TOUR_OPENING_PLAYER_CLIP_53,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 4,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's opening-tour clip 54 preset, blending over 4 frames.
///
/// Requires the opening-tour extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TourOpeningPlayerClip54Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TOUR_OPENING_PLAYER_CLIP_54,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 4,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's opening-tour clip 55 preset, blending over 4 frames.
///
/// Requires the opening-tour extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TourOpeningPlayerClip55Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TOUR_OPENING_PLAYER_CLIP_55,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 4,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's opening-tour clip 56 preset, blending over 4 frames.
///
/// Requires the opening-tour extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TourOpeningPlayerClip56Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TOUR_OPENING_PLAYER_CLIP_56,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 4,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's opening-tour clip 57 preset, blending over 4 frames.
///
/// Requires the opening-tour extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TourOpeningPlayerClip57Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TOUR_OPENING_PLAYER_CLIP_57,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 4,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's placement at the start of Pierce's terminal tour.
///
/// XYZ is in room-coordinate units; yaw is one eighth of a 4096-unit turn.
/// The placement message consumes XYZ and all three Euler angles during dispatch.
static ActorTransform _gActor443500PlayerTourOpeningEntryPlacement = {
    .pos = { 15000, -1000, 7600, 0 },
    .rot = { 0, ACTOR_TRANSFORM_ANGLE_TURN / 8, 0, 0 },
};

/// Aya's console position at the end of the opening tour and on its skip path.
///
/// XYZ is in room-coordinate units; Euler angles use 4096 units per turn.
/// Walking consumes only XYZ; the skip path places the full transform directly.
static ActorTransform _gActor443500PlayerConsolePlacement = {
    .pos = { 15600, -1000, 9000, 0 },
    .rot = { 0, ACTOR_TRANSFORM_ANGLE_TURN / 8, 0, 0 },
};

/// Aya's extension clips for the console and map-terminal follow-up scenes.
///
/// Copied to player-bank entries 47..71, including NULL at 47. Clips 48..71
/// borrow this overlay's animation data; clip 48 is shared with the opening tour.
static AnimationSet* _gActor443500TerminalFollowupPlayerAnimationSets[25] = {
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

/// Request to copy the terminal follow-up clips into Aya's bank extension.
///
/// Copies the complete set-pointer table in 32-bit words, including its NULL
/// entry. Keep this overlay loaded while any copied clip remains in playback.
static AnimationBankCopyRequest _gActor443500TerminalFollowupPlayerBankExtension = {
    .source    = { .sets = _gActor443500TerminalFollowupPlayerAnimationSets },
    .wordCount = ARRAY_SIZE(_gActor443500TerminalFollowupPlayerAnimationSets),
};

/// Unplayed clip-1 reset preset beside the terminal-follow-up player requests.
///
/// Bank selector 1 and enabled world collision are retained. Its position in
/// the player request run suggests Aya as receiver; no dispatch proves that.
static AnimationPlayRequest _gActor443500UnusedTerminalFollowupPlayerClip1Request = {
    .source               = { .index = 1 },
    .animationId          = 1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's shared extension clip with a ten-frame blend in terminal follow-ups.
///
/// The event interpreter replaces the stored bank selector with the equipped
/// weapon's bank. Requires the copied extension and restores grid participation.
static AnimationPlayRequest _gActor443500PlayerClip48Blend10Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_PLAYER_SHARED_CLIP,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Player-bank set indices supplied by the terminal-follow-up extension.
enum {
    ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_49 = ANIMATION_BANK_BASE_SET_COUNT + 2,
    ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_50 = ANIMATION_BANK_BASE_SET_COUNT + 3,
    ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_51 = ANIMATION_BANK_BASE_SET_COUNT + 4,
    ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_52 = ANIMATION_BANK_BASE_SET_COUNT + 5,
    ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_53 = ANIMATION_BANK_BASE_SET_COUNT + 6,
    ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_55 = ANIMATION_BANK_BASE_SET_COUNT + 8,
    ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_56 = ANIMATION_BANK_BASE_SET_COUNT + 9,
    ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_57 = ANIMATION_BANK_BASE_SET_COUNT + 10,
    ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_58 = ANIMATION_BANK_BASE_SET_COUNT + 11,
    ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_59 = ANIMATION_BANK_BASE_SET_COUNT + 12,
    ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_60 = ANIMATION_BANK_BASE_SET_COUNT + 13,
    ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_61 = ANIMATION_BANK_BASE_SET_COUNT + 14,
    ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_62 = ANIMATION_BANK_BASE_SET_COUNT + 15,
    ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_63 = ANIMATION_BANK_BASE_SET_COUNT + 16,
    ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_64 = ANIMATION_BANK_BASE_SET_COUNT + 17,
    ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_65 = ANIMATION_BANK_BASE_SET_COUNT + 18,
    ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_66 = ANIMATION_BANK_BASE_SET_COUNT + 19,
    ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_67 = ANIMATION_BANK_BASE_SET_COUNT + 20,
    ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_69 = ANIMATION_BANK_BASE_SET_COUNT + 22,
    ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_70 = ANIMATION_BANK_BASE_SET_COUNT + 23,
    ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_71 = ANIMATION_BANK_BASE_SET_COUNT + 24,
};

/// Aya's terminal-follow-up clip 49 preset, resetting playback without a pose blend.
///
/// Requires the terminal-follow-up extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TerminalFollowupPlayerClip49Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_49,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's terminal-follow-up clip 50 preset, resetting playback without a pose blend.
///
/// Requires the terminal-follow-up extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TerminalFollowupPlayerClip50Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_50,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's terminal-follow-up clip 51 preset, blending over 20 frames.
///
/// Requires the terminal-follow-up extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TerminalFollowupPlayerClip51Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_51,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 20,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's terminal-follow-up clip 52 preset, resetting playback without a pose blend.
///
/// Requires the terminal-follow-up extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TerminalFollowupPlayerClip52Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_52,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's terminal-follow-up clip 53 preset, resetting playback without a pose blend.
///
/// Requires the terminal-follow-up extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TerminalFollowupPlayerClip53Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_53,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

enum { ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_54 = ANIMATION_BANK_BASE_SET_COUNT + 7 };

/// Unplayed clip-54 reset preset in the terminal-follow-up player request run.
///
/// Requests world collision with bank selector 1. The receiver is inferred
/// from the neighboring player requests; no dispatch proves it.
static AnimationPlayRequest _gActor443500UnusedTerminalFollowupPlayerClip54Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_54,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's terminal-follow-up clip 55 preset, resetting playback without a pose blend.
///
/// Requires the terminal-follow-up extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TerminalFollowupPlayerClip55Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_55,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's terminal-follow-up clip 56 preset, blending over 10 frames.
///
/// Requires the terminal-follow-up extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TerminalFollowupPlayerClip56Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_56,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's terminal-follow-up clip 57 preset, resetting playback without a pose blend.
///
/// Requires the terminal-follow-up extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TerminalFollowupPlayerClip57Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_57,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's terminal-follow-up clip 58 preset, resetting playback without a pose blend.
///
/// Requires the terminal-follow-up extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TerminalFollowupPlayerClip58Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_58,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's terminal-follow-up clip 59 preset, resetting playback without a pose blend.
///
/// Requires the terminal-follow-up extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TerminalFollowupPlayerClip59Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_59,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's terminal-follow-up clip 60 preset, blending over 10 frames.
///
/// Requires the terminal-follow-up extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TerminalFollowupPlayerClip60Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_60,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's terminal-follow-up clip 61 preset, blending over 10 frames.
///
/// Requires the terminal-follow-up extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TerminalFollowupPlayerClip61Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_61,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's terminal-follow-up clip 62 preset, blending over 10 frames.
///
/// Requires the terminal-follow-up extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TerminalFollowupPlayerClip62Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_62,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's terminal-follow-up clip 63 preset, blending over 10 frames.
///
/// Requires the terminal-follow-up extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TerminalFollowupPlayerClip63Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_63,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's terminal-follow-up clip 64 preset, blending over 10 frames.
///
/// Requires the terminal-follow-up extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TerminalFollowupPlayerClip64Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_64,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's terminal-follow-up clip 65 preset, blending over 10 frames.
///
/// Requires the terminal-follow-up extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TerminalFollowupPlayerClip65Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_65,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's terminal-follow-up clip 66 preset, blending over 20 frames.
///
/// Requires the terminal-follow-up extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TerminalFollowupPlayerClip66Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_66,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 20,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's terminal-follow-up clip 67 preset, blending over 2 frames.
///
/// Requires the terminal-follow-up extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TerminalFollowupPlayerClip67Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_67,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 2,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

enum { ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_68 = ANIMATION_BANK_BASE_SET_COUNT + 21 };

/// Unplayed clip-68 preset with a ten-frame blend in the player request run.
///
/// Requests world collision with bank selector 1. The receiver is inferred
/// from the neighboring player requests; no dispatch proves it.
static AnimationPlayRequest _gActor443500UnusedTerminalFollowupPlayerClip68Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_68,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's terminal-follow-up clip 69 preset, blending over 20 frames.
///
/// Requires the terminal-follow-up extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TerminalFollowupPlayerClip69Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_69,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 20,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's terminal-follow-up clip 70 preset, blending over 10 frames.
///
/// Requires the terminal-follow-up extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TerminalFollowupPlayerClip70Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_70,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's terminal-follow-up clip 71 preset, blending over 10 frames.
///
/// Requires the terminal-follow-up extension in the equipped bank and requests grid
/// participation. Dispatch replaces the bank selector in a copy of this request;
/// keep this overlay loaded while the clip plays.
static AnimationPlayRequest _gActor443500TerminalFollowupPlayerClip71Request = {
    .source               = { .index = 1 },
    .animationId          = ACTOR_443500_TERMINAL_FOLLOWUP_PLAYER_CLIP_71,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Aya's entry placement for the scene after using Pierce's console terminal.
///
/// XYZ is in room-coordinate units; yaw is exactly 2047/4096 of a turn.
/// The placement message consumes XYZ and all three Euler angles during dispatch.
static ActorTransform _gActor443500PlayerTerminalFollowupEntryPlacement = {
    .pos = { 16200, -1000, 8000, 0 },
    .rot = { 0, ACTOR_TRANSFORM_ANGLE_HALF_TURN - 1, 0, 0 },
};

/// Aya's map-terminal destination and placement for follow-up and skip scenes.
///
/// XYZ is in room-coordinate units; yaw is one third of a 4096-unit turn,
/// truncated to 1365. Walking consumes XYZ; direct placement also consumes angles.
static ActorTransform _gActor443500PlayerMapTerminalPlacement = {
    .pos = { 16300, -1000, 6950, 0 },
    .rot = { 0, ACTOR_TRANSFORM_ANGLE_TURN / 3, 0, 0 },
};

/// Unplayed bank-0 clip-1 reset preset beside Pierce's animation requests.
///
/// Its receiver is inferred from the neighboring Pierce requests. That actor
/// ignores the retained world-collision word; no dispatch proves this preset's use.
static AnimationPlayRequest _gActor443500UnusedPierceClip1ResetRequest = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 1 preset for the console follow-up, requesting a 20-frame blend.
///
/// Consumed read-only during dispatch; the bank's clip data stays borrowed
/// through playback. Repeated requests restart; Pierce ignores the collision word.
static AnimationPlayRequest _gActor443500PierceClip1Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 1,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 20,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 2 preset for the tour opening, resetting its pose immediately.
///
/// Consumed read-only during dispatch; the bank's clip data stays borrowed
/// through playback. Repeated requests restart; Pierce ignores the collision word.
static AnimationPlayRequest _gActor443500PierceClip2Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 2,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 3 preset for the tour opening, requesting a 4-frame blend.
///
/// Consumed read-only during dispatch; the bank's clip data stays borrowed
/// through playback. Repeated requests restart; Pierce ignores the collision word.
static AnimationPlayRequest _gActor443500PierceClip3Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 3,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 4,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 4 preset for the tour opening, requesting a 4-frame blend.
///
/// Consumed read-only during dispatch; the bank's clip data stays borrowed
/// through playback. Repeated requests restart; Pierce ignores the collision word.
static AnimationPlayRequest _gActor443500PierceClip4Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 4,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 4,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 5 preset for the tour opening, requesting a 4-frame blend.
///
/// Consumed read-only during dispatch; the bank's clip data stays borrowed
/// through playback. Repeated requests restart; Pierce ignores the collision word.
static AnimationPlayRequest _gActor443500PierceClip5Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 5,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 4,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 6 preset for the tour opening, requesting a 4-frame blend.
///
/// Consumed read-only during dispatch; the bank's clip data stays borrowed
/// through playback. Repeated requests restart; Pierce ignores the collision word.
static AnimationPlayRequest _gActor443500PierceClip6Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 6,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 4,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 7 preset for the tour opening, requesting a 4-frame blend.
///
/// Consumed read-only during dispatch; the bank's clip data stays borrowed
/// through playback. Repeated requests restart; Pierce ignores the collision word.
static AnimationPlayRequest _gActor443500PierceClip7Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 7,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 4,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 8 preset for the tour opening, requesting a 10-frame blend.
///
/// Consumed read-only during dispatch; the bank's clip data stays borrowed
/// through playback. Repeated requests restart; Pierce ignores the collision word.
static AnimationPlayRequest _gActor443500PierceClip8Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 8,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 9 preset, blending for 20 frames in the tour scenes.
static AnimationPlayRequest _gActor443500PierceClip9Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 9,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 20,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 10 preset for the tour opening, requesting a 10-frame blend.
///
/// Consumed read-only during dispatch; the bank's clip data stays borrowed
/// through playback. Repeated requests restart; Pierce ignores the collision word.
static AnimationPlayRequest _gActor443500PierceClip10Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 10,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 11 preset for the tour opening, requesting a 10-frame blend.
///
/// Consumed read-only during dispatch; the bank's clip data stays borrowed
/// through playback. Repeated requests restart; Pierce ignores the collision word.
static AnimationPlayRequest _gActor443500PierceClip11Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 11,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 12 preset for the tour opening, requesting a 10-frame blend.
///
/// Consumed read-only during dispatch; the bank's clip data stays borrowed
/// through playback. Repeated requests restart; Pierce ignores the collision word.
static AnimationPlayRequest _gActor443500PierceClip12Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 12,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 13 preset, blending for 10 frames in the tour scenes.
static AnimationPlayRequest _gActor443500PierceClip13Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 13,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 14 preset for the console follow-up, requesting an 8-frame blend.
///
/// Consumed read-only during dispatch; the bank's clip data stays borrowed
/// through playback. Repeated requests restart; Pierce ignores the collision word.
static AnimationPlayRequest _gActor443500PierceClip14Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 14,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 8,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 15 preset for the console follow-up, requesting a 20-frame blend.
///
/// Consumed read-only during dispatch; the bank's clip data stays borrowed
/// through playback. Repeated requests restart; Pierce ignores the collision word.
static AnimationPlayRequest _gActor443500PierceClip15Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 15,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 20,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 16 preset, blending for 10 frames in terminal follow-ups.
static AnimationPlayRequest _gActor443500PierceClip16Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 16,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 17 preset, blending for 10 frames in terminal follow-ups.
static AnimationPlayRequest _gActor443500PierceClip17Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 17,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 18 preset, blending for 10 frames in terminal follow-ups.
static AnimationPlayRequest _gActor443500PierceClip18Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 18,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 19 preset, blending for 10 frames in terminal follow-ups.
static AnimationPlayRequest _gActor443500PierceClip19Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 19,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 20 preset, blending for 10 frames in terminal follow-ups.
static AnimationPlayRequest _gActor443500PierceClip20Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 20,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 21 preset for the console follow-up, requesting a 10-frame blend.
///
/// Consumed read-only during dispatch; the bank's clip data stays borrowed
/// through playback. Repeated requests restart; Pierce ignores the collision word.
static AnimationPlayRequest _gActor443500PierceClip21Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 21,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 22 preset for the console follow-up, requesting a 10-frame blend.
///
/// Consumed read-only during dispatch; the bank's clip data stays borrowed
/// through playback. Repeated requests restart; Pierce ignores the collision word.
static AnimationPlayRequest _gActor443500PierceClip22Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 22,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 23 preset, blending for 10 frames in terminal follow-ups.
static AnimationPlayRequest _gActor443500PierceClip23Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 23,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 24 preset for the map follow-up, requesting a 10-frame blend.
///
/// Consumed read-only during dispatch; the bank's clip data stays borrowed
/// through playback. Repeated requests restart; Pierce ignores the collision word.
static AnimationPlayRequest _gActor443500PierceClip24Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 24,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 25 preset, blending for 10 frames in terminal follow-ups.
static AnimationPlayRequest _gActor443500PierceClip25Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 25,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 26 preset for the map follow-up, requesting a 10-frame blend.
///
/// Consumed read-only during dispatch; the bank's clip data stays borrowed
/// through playback. Repeated requests restart; Pierce ignores the collision word.
static AnimationPlayRequest _gActor443500PierceClip26Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 26,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 27 preset for the console follow-up, requesting a 10-frame blend.
///
/// Consumed read-only during dispatch; the bank's clip data stays borrowed
/// through playback. Repeated requests restart; Pierce ignores the collision word.
static AnimationPlayRequest _gActor443500PierceClip27Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 27,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Ten-frame transition back to Pierce's fallback clip after the map tour.
///
/// The same preset ends the skipped scene. Pierce ignores its collision word.
static AnimationPlayRequest _gActor443500PierceDefaultClipBlend10Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = ACTOR_443500_DEFAULT_ANIMATION,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 29 preset for the power-plant-progress dialogue, requesting a 10-frame blend.
///
/// Consumed read-only during dispatch; the bank's clip data stays borrowed
/// through playback. Repeated requests restart; Pierce ignores the collision word.
static AnimationPlayRequest _gActor443500PierceClip29Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 29,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 30 preset for the power-plant-progress dialogue, requesting a 10-frame blend.
///
/// Consumed read-only during dispatch; the bank's clip data stays borrowed
/// through playback. Repeated requests restart; Pierce ignores the collision word.
static AnimationPlayRequest _gActor443500PierceClip30Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 30,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 31 preset for the console follow-up, requesting a 20-frame blend.
///
/// Consumed read-only during dispatch; the bank's clip data stays borrowed
/// through playback. Repeated requests restart; Pierce ignores the collision word.
static AnimationPlayRequest _gActor443500PierceClip31Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 31,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 20,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 32 preset for the console follow-up, requesting a 10-frame blend.
///
/// Consumed read-only during dispatch; the bank's clip data stays borrowed
/// through playback. Repeated requests restart; Pierce ignores the collision word.
static AnimationPlayRequest _gActor443500PierceClip32Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 32,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 33 preset for the console follow-up, requesting a 20-frame blend.
///
/// Consumed read-only during dispatch; the bank's clip data stays borrowed
/// through playback. Repeated requests restart; Pierce ignores the collision word.
static AnimationPlayRequest _gActor443500PierceClip33Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 33,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 20,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 34 preset for the console follow-up, requesting a 2-frame blend.
///
/// Consumed read-only during dispatch; the bank's clip data stays borrowed
/// through playback. Repeated requests restart; Pierce ignores the collision word.
static AnimationPlayRequest _gActor443500PierceClip34Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 34,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 2,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's bank-0 clip 35 preset for the console follow-up, requesting a 10-frame blend.
///
/// Consumed read-only during dispatch; the bank's clip data stays borrowed
/// through playback. Repeated requests restart; Pierce ignores the collision word.
static AnimationPlayRequest _gActor443500PierceClip35Request = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = 35,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Pierce's console placement for the opening tour and its skip path.
///
/// XYZ is in room-coordinate units; yaw is five eighths of a 4096-unit turn.
/// The placement message consumes all three position and Euler components.
static ActorTransform _gActor443500PierceConsolePlacement = {
    .pos = { 16000, -1000, 9000, 0 },
    .rot = { 0, 5 * (ACTOR_TRANSFORM_ANGLE_TURN / 8), 0, 0 },
};

/// Pierce's placement at the start of the console follow-up.
///
/// XYZ uses room-coordinate units; all Euler angles are zero (4096 per turn).
/// The placement message consumes XYZ and all angles synchronously.
static ActorTransform _gActor443500PierceConsoleFollowupEntryPlacement = {
    .pos = { 16200, -1000, 6200, 0 },
    .rot = { 0, 0, 0, 0 },
};

/// Pierce's map-terminal placement for follow-up and skip scenes.
///
/// XYZ is in room-coordinate units with zero Euler orientation. This is also
/// his initial room placement; the message consumes the complete XYZ and angles.
static ActorTransform _gActor443500PierceMapTerminalPlacement = {
    .pos = { 16000, -1000, 5910, 0 },
    .rot = { 0, 0, 0, 0 },
};

/// Terminal-tour CAP slots, camera views and saved music-event selector.
enum {
    ACTOR_443500_TOUR_OPENING_CAP_COMMAND     = 4,
    ACTOR_443500_CONSOLE_FOLLOWUP_CAP_COMMAND = 5,
    ACTOR_443500_MAP_FOLLOWUP_CAP_COMMAND     = 6,
    ACTOR_443500_CONSOLE_SKIP_CAP_VARIANT     = 1,
    ACTOR_443500_TOUR_OPENING_VIEW            = 15,
    ACTOR_443500_CONSOLE_FOLLOWUP_VIEW        = 13,
    ACTOR_443500_MAP_FOLLOWUP_VIEW            = 14,
    ACTOR_443500_TOUR_FINISHED_VIEW           = 5,
    ACTOR_443500_TOUR_SCENE_EVENT             = 15,
    ACTOR_443500_CAP_SELECT_DEFAULT           = 0,
    ACTOR_443500_CAP_SELECT_EVENT             = 1,
};

/// Room-bank scripts stopped or started by the terminal-tour scenes.
enum {
    ACTOR_443500_TOUR_DEFAULT_CLIP_SOUND = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R47, 1),
    ACTOR_443500_TOUR_OPENING_SOUND      = SOUND_AREA(GAME_STAGE_MINE_SHELTER, GAME_AREA_SHELTER_R47, 6),
    ACTOR_443500_TOUR_SOUND_FADE_TICKS   = 8,
};

EvsCommand gActor443500TourOpeningScript[74] = {
    // Fade out and install the opening player clips before staging both actors.
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor443500TourOpeningPlayerBankExtension } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor443500SelectEventCap }, { .value = ACTOR_443500_CAP_SELECT_EVENT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = ACTOR_443500_TOUR_DEFAULT_CLIP_SOUND }, { .value = ACTOR_443500_TOUR_SOUND_FADE_TICKS }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_443500_TOUR_OPENING_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = ACTOR_443500_TOUR_OPENING_SOUND }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor443500PlayerTourOpeningEntryPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor443500PierceConsolePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500PlayerClip48Blend4Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip2Request } }, { .value = 0 } },
    // CAP cues and frame delays coordinate the console introduction.
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_443500_TOUR_OPENING_CAP_COMMAND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TourOpeningPlayerClip49Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip3Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip4Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 120 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip11Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TourOpeningPlayerClip55Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 64 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TourOpeningPlayerClip50Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TourOpeningPlayerClip51Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip10Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TourOpeningPlayerClip52Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 140 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip12Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TourOpeningPlayerClip53Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip8Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip9Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip5Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TourOpeningPlayerClip54Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip13Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 35 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500PlayerClip48Blend4Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TourOpeningPlayerClip56Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 120 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TourOpeningPlayerClip57Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500PlayerClip48Blend4Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500PlayerClip48Blend4Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip6Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip7Request } }, { .value = 0 } },
    // Move Aya to the console, then restore ordinary room presentation.
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_MOVE_TO }, { .message = { .transform = &_gActor443500PlayerConsolePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_HIDE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor443500SelectEventCap }, { .value = ACTOR_443500_CAP_SELECT_DEFAULT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gActor443500TourOpeningSkipScript[16] = {
    // Settle the console placement and release the skipped scene under the fade.
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = ACTOR_443500_TOUR_DEFAULT_CLIP_SOUND }, { .value = ACTOR_443500_TOUR_SOUND_FADE_TICKS }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_HIDE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor443500PlayerConsolePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor443500PierceConsolePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor443500SelectEventCap }, { .value = ACTOR_443500_CAP_SELECT_DEFAULT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gActor443500ConsoleFollowupScript[137] = {
    // Install follow-up clips and stage the scene after the console session.
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor443500StartFadeFromBlack }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = ACTOR_443500_TOUR_DEFAULT_CLIP_SOUND }, { .value = ACTOR_443500_TOUR_SOUND_FADE_TICKS }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor443500TerminalFollowupPlayerBankExtension } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS8 = _actor443500SetSceneEvent }, { .value = ACTOR_443500_TOUR_SCENE_EVENT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor443500SelectEventCap }, { .value = ACTOR_443500_CAP_SELECT_EVENT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor443500PlayerTerminalFollowupEntryPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor443500PierceConsoleFollowupEntryPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_443500_CONSOLE_FOLLOWUP_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TerminalFollowupPlayerClip49Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip14Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // CAP command 5 drives the actor exchanges and transition to the map.
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_443500_CONSOLE_FOLLOWUP_CAP_COMMAND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TerminalFollowupPlayerClip50Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip15Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TerminalFollowupPlayerClip66Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip1Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 46 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TerminalFollowupPlayerClip67Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip13Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip33Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 25 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip34Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 90 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip9Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TerminalFollowupPlayerClip51Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip1Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip33Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip34Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 88 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500PlayerClip48Blend10Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip1Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip9Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip9Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TerminalFollowupPlayerClip69Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip1Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 189 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TerminalFollowupPlayerClip70Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500PlayerClip48Blend10Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip31Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip32Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 18 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip13Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip34Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 38 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip35Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip9Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 42 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip1Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TerminalFollowupPlayerClip50Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip1Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500PlayerClip48Blend10Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip27Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TerminalFollowupPlayerClip71Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_MOVE_TO }, { .message = { .transform = &_gActor443500PlayerMapTerminalPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip14Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE }, { .value = 0 } },
    // Give Aya the timed map display, then reinstall the scene clips.
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_HIDE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor443500OpenTimedMapTerminal }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor443500TerminalFollowupPlayerBankExtension } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TerminalFollowupPlayerClip52Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor443500PlayerMapTerminalPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor443500PierceMapTerminalPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip16Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip17Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip19Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip20Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TerminalFollowupPlayerClip53Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip18Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 80 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TerminalFollowupPlayerClip55Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip21Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TerminalFollowupPlayerClip56Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip22Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TerminalFollowupPlayerClip52Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip25Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip19Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip20Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip18Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TerminalFollowupPlayerClip57Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip23Request } }, { .value = 0 } },
    // Finish at the map terminal and restore player control and presentation.
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip16Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_HIDE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor443500SelectEventCap }, { .value = ACTOR_443500_CAP_SELECT_DEFAULT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gActor443500ConsoleFollowupSkipScript[18] = {
    // Settle the map-terminal placements without replaying the console scene.
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = ACTOR_443500_TOUR_DEFAULT_CLIP_SOUND }, { .value = ACTOR_443500_TOUR_SOUND_FADE_TICKS }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS8 = _actor443500SetSceneEvent }, { .value = ACTOR_443500_TOUR_SCENE_EVENT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_HIDE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor443500PlayerMapTerminalPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor443500PierceMapTerminalPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // Request the shortened CAP variant after releasing scene playback.
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS16 = _actor443500StartCapCommand5 }, { .value = ACTOR_443500_CONSOLE_SKIP_CAP_VARIANT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor443500SelectEventCap }, { .value = ACTOR_443500_CAP_SELECT_DEFAULT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gActor443500MapFollowupScript[73] = {
    // Stage the final map-terminal scene with the follow-up clip bank.
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor443500StartFadeFromBlack }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor443500TerminalFollowupPlayerBankExtension } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor443500SelectEventCap }, { .value = ACTOR_443500_CAP_SELECT_EVENT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor443500PlayerMapTerminalPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor443500PierceMapTerminalPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_443500_MAP_FOLLOWUP_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TerminalFollowupPlayerClip58Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip16Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    // CAP command 6 drives the final actor exchange.
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_443500_MAP_FOLLOWUP_CAP_COMMAND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TerminalFollowupPlayerClip59Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip17Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TerminalFollowupPlayerClip60Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500PlayerClip48Blend10Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip19Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip20Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TerminalFollowupPlayerClip61Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip18Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TerminalFollowupPlayerClip62Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip19Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip20Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TerminalFollowupPlayerClip63Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip18Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip24Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TerminalFollowupPlayerClip62Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip18Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip25Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip19Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 70 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip20Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 25 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip23Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip16Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TerminalFollowupPlayerClip64Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TerminalFollowupPlayerClip62Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip17Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip26Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500TerminalFollowupPlayerClip65Request }, { .value = 0 } },
    // Restore normal playback and apply the Shelter progress updates.
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceDefaultClipBlend10Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor443500SelectEventCap }, { .value = ACTOR_443500_CAP_SELECT_DEFAULT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor443500ApplyShelterAreaUpdates }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_443500_TOUR_FINISHED_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gActor443500MapFollowupSkipScript[17] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = GPU_BLEND_AVERAGE }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = ACTOR_MESSAGE_DRAW_SHOW }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor443500PlayerMapTerminalPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor443500PierceMapTerminalPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceDefaultClipBlend10Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_DIRTY_VIEW, { .value = ACTOR_443500_TOUR_FINISHED_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor443500SelectEventCap }, { .value = ACTOR_443500_CAP_SELECT_DEFAULT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor443500ApplyShelterAreaUpdates }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gActor443500PowerPlantProgressDialogueScript[10] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor443500TerminalFollowupPlayerBankExtension } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor443500PlayerClip48Blend10Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip29Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor443500SelectEventCapAfterPowerPlantClear }, { .value = ACTOR_443500_CAP_SELECT_EVENT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor443500RunPowerPlantProgressCap }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor443500SelectEventCap }, { .value = ACTOR_443500_CAP_SELECT_DEFAULT }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR_443500_PIERCE_PLACEMENT_INDEX }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor443500PierceClip30Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
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

/// Pierce's bank-0 clip table: NULL at 0, playable clips at 1..35.
///
/// Slots 1..19 borrow these descriptors and packed streams while the body
/// plays. Clip `ACTOR_443500_DEFAULT_ANIMATION` is the non-event fallback.
static AnimationSet* _gActor443500PierceAnimationSets[36] = {
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

/// Indexed playback banks for Pierce; only bank 0 is valid.
///
/// The animation context retains the selected set table. Keep this overlay's
/// descriptors and packed streams loaded until Pierce's playback ends.
static AnimationSet** _gActor443500PierceAnimationBanks[1] = {
    _gActor443500PierceAnimationSets,
};

/// Pierce's fallback playback preset, used at spawn and outside scene events.
///
/// Selects bank 0 and clip 28, blending for four frames from an active pose or
/// resetting if no pose is playing. The handler ignores the collision word
/// and restarts the loop-sound counter even when the same clip is requested.
static AnimationPlayRequest _gActor443500PierceDefaultAnimationRequest = {
    .source               = { .index = ACTOR_443500_PIERCE_ANIMATION_BANK },
    .animationId          = ACTOR_443500_DEFAULT_ANIMATION,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 4,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

static TmdSource _gActor443500Actor113100Model07960;

TaskDesc gActor443500PierceTaskDescs[2] = {
    { .header = { .fields = { TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER, ACTOR_443500_TASK_PRIORITY } }, .callback = _actor443500PierceTask, .data = { .model = &_gActor443500PierceCarradineBody } },
    { .header = { .fields = { TASK_BODY_TMD, ACTOR_443500_TASK_PRIORITY } }, .callback = _actor443500ChildModelTask, .data = { .model = &_gActor443500Actor113100Model07960 } },
};

/// Pierce synchronous playback, placement and model-draw messages, followed by the end marker.
///
/// Playback borrows AnimationPlayRequest, placement borrows ActorTransform, and
/// draw policy takes an integer mode. The task borrows this table while the actor
/// overlay is loaded; all three callbacks use the four-word message ABI.
static TaskMessageEntry _gActor443500PierceMessages[4] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actor443500PlayAnimation },
    { ACTOR_MESSAGE_PLACE, actorMsgPlaceEuler },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor443500SetModelDraw },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Unit normals of Pierce's room collision template, scaled by 4096.
static SVECTOR _gActor443500PierceCollisionNormals[2] = {
#include "assets/actor_443500_collision_269B8_normals.inc"
};

/// Pierce's collision-template vertices in signed room-coordinate units.
static SVECTOR _gActor443500PierceCollisionVerts[6] = {
#include "assets/actor_443500_collision_269B8_verts.inc"
};

/// Two obstacle quads indexing Pierce's six template vertices and two normals.
static WorldCollisionGridFace _gActor443500PierceCollisionFaces[2] = {
#include "assets/actor_443500_collision_269B8_faces.inc"
};

/// Face-index list for Pierce's one grid cell; its first -1 ends the list.
static s16 _gActor443500PierceCollisionCells[4] = {
#include "assets/actor_443500_collision_269B8_cells.inc"
};

// Asset include binding: each index is an element offset into the cell lists.
#define GRID_CELL(i) (&_gActor443500PierceCollisionCells[i])
/// Cell table of Pierce's collision template, borrowing its face-index list.
static s16* _gActor443500PierceCollisionTable[1] = {
#include "assets/actor_443500_collision_269B8_table.inc"
};
#undef GRID_CELL

/// Pierce's obstacle mesh template for the leading entries of room 47's grid.
///
/// Installation copies two normals, two faces and six vertices into the live
/// room grid, optionally raising Y by 2000 coordinate units. This template is
/// never installed as the active grid: its view coordinate stays NULL and its
/// one-cell indexing is retained only as part of the embedded asset.
static WorldCollisionGrid _gActor443500PierceCollision = {
    .viewCoord   = NULL,
    .normals     = _gActor443500PierceCollisionNormals,
    .vertices    = _gActor443500PierceCollisionVerts,
    .faces       = _gActor443500PierceCollisionFaces,
    .cellFaceIds = _gActor443500PierceCollisionTable,
    .xBias       = -16120,
    .zBias       = -5156,
    .cellCountX  = 1,
    .cellCountZ  = 1,
    .cellSize    = 4000,
    .faceCount   = ARRAY_SIZE(_gActor443500PierceCollisionFaces),
};

/// Selects the event CAP resource, or restores ordinary room CAP selection.
///
/// Nonzero `useEventCap` clears the current selection before selecting loaded
/// data resource 1 and its texture origin (576, 256) in VRAM pixels. Zero resets
/// CAP to its default resource. Playback must be stopped and the selected CAP
/// data and glyph texture must stay loaded while in use; this performs no I/O.
static void _actor443500SelectEventCap(s32 useEventCap)
{
    if (useEventCap != 0) {
        gCapFile = NULL;
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
            gCapFile = NULL;
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
    taskSpawnFromTable(&gShelterR47MapTerminalTaskDesc, 0, SHELTER_R47_MAP_MODE_TIMED, 0);
    playerActorSetDrawMode(PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE);
    playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
}

/// Starts this package's 30-tick fade from black, if a task can be allocated.
///
/// Keep the actor overlay loaded until the spawned callback finishes.
static void _actor443500StartFadeFromBlack(void)
{
    taskSpawnFromTable(&_gActor443500FadeFromBlackTaskDesc, 0, 0, 0);
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
    areaApplySavedUpdates(gShelterR47SavedAreaUpdates);
}

/// Stores the saved scene-event byte used to select stage music.
///
/// The event scripts pass 15. This only updates the live save; music selection
/// reads the event later. The signed-byte callback preserves its argument width.
static void _actor443500SetSceneEvent(s8 sceneEvent)
{
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.sceneEvent = sceneEvent;
}

/// Gives Pierce's held model its owner's placement texture and palette offsets.
///
/// Copies texture-page and CLUT-row offsets, then rebuilds each half of an
/// existing primitive buffer. A NULL buffer updates only the stored offsets.
/// Requires a live placement, model source and sufficient writable buffer space
/// for both halves; the two builds restore the original half selector. Borrows
/// both pointers through this call and neither allocates nor frees storage.
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
    work->model.animId    = ACTOR_MOTION_ANIMATION_INDEX_NONE;
    work->model.bank      = ACTOR_MOTION_ANIMATION_INDEX_NONE;
    work->freeCountdown   = ACTOR_443500_PIERCE_NO_BUFFER_RELEASE;
    work->savedModelFlags = task->extra.tmd->flags;
    childTask             = taskSpawnFromTable(gActor443500PierceTaskDescs, ACTOR_443500_PIERCE_CHILD_DESCRIPTOR, ACTOR_443500_PIERCE_CHILD_PART, task);
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
        placement = AREA_PLACEMENT_AT(areaVariant->placements, placementIndex);
        _actor443500ApplyPiercePlacementTextures(heldModel, placement);
    }
    _actor443500SetModelDraw(task, ACTOR_MESSAGE_SET_MODEL_DRAW, ACTOR_MESSAGE_DRAW_HIDE, 0);
    _actor443500PlayAnimation(task, ACTOR_MESSAGE_PLAY_ANIMATION, &_gActor443500PierceDefaultAnimationRequest, 0);
    _actor443500BindModelLighting(task);
    task->msgTable     = _gActor443500PierceMessages;
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
            _actor443500PlayAnimation(task, ACTOR_MESSAGE_PLAY_ANIMATION, &_gActor443500PierceDefaultAnimationRequest, 0);
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
        if (worldCollisionProjectGroundPoint(GRAPHICS_MATRIX_TRANSLATION(&task->extra.tmd->coords[1].workm), &groundPoint) != 0) {
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

    states = _gActor443500ChildModelStates;
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

    states = _gActor443500PierceStates;
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
        animationInitContext(&work->rig.anim, _gActor443500PierceAnimationBanks[work->model.bank], model, work->rig.poses,
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
/// `ACTOR_MOTION_ANIMATION_INDEX_NONE` before the first request. Slots 1..19 blend when
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

    roomGrid  = &gShelterR47CollisionGrid;
    actorGrid = &_gActor443500PierceCollision;

    // The room reserves its leading geometry entries for this actor's faces.
    for (elementIndex = 0; elementIndex < ARRAY_SIZE(_gActor443500PierceCollisionFaces); elementIndex++) {
        roomGrid->normals[elementIndex].vx = actorGrid->normals[elementIndex].vx;
        roomGrid->normals[elementIndex].vy = actorGrid->normals[elementIndex].vy;
        roomGrid->normals[elementIndex].vz = actorGrid->normals[elementIndex].vz;
        roomGrid->faces[elementIndex]      = actorGrid->faces[elementIndex];
    }

    for (elementIndex = 0; elementIndex < ARRAY_SIZE(_gActor443500PierceCollisionVerts); elementIndex++) {
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

    for (elementIndex = 0; elementIndex < ARRAY_SIZE(_gActor443500PierceCollisionVerts); elementIndex++) {
        roomGrid->vertices[elementIndex].vx += displacement.vx;
        roomGrid->vertices[elementIndex].vy += displacement.vy;
        roomGrid->vertices[elementIndex].vz += displacement.vz;
    }
}
