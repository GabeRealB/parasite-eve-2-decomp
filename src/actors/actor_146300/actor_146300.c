#include "actors/actor_146300.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/display.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/sound_ids.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "../../shared/walk/scripted_walk.h"
#include "../../shared/actor/actor_messages.h"

/// Work block of the package's actor, a twenty-part figure that event scripts
/// place and give clips to but that never walks.
///
/// The spawn state allocates it zeroed and keeps it both at `Task::work` and
/// in `_gScriptedWalkWork`. The model object borrows `light` and `color` for as
/// long as the block lives. The matrices, the rig and `st` sit where the
/// scripted walkers' blocks keep theirs, which is what lets the package carry
/// that library's slot tick, its two reseeds and its placement under the
/// library's name for the block. The block ends at `st`: there is no turn
/// countdown behind it, and the walk's frame count in `st` stays zero.
typedef struct {
    MATRIX          light; // Light-direction matrix lent to the model object
    MATRIX          color; // Light-colour matrix lent to the model object
    ActorAnimRig20  rig;   // Playback storage of the twenty-part model; slots 1 to 19 are driven
    ActorEnemyState st;    // Animation request and the heading the last placement gave the root
} _Actor146300Work;
STATIC_ASSERT_SIZEOF(_Actor146300Work, 0x4EC);

/// Borrowed work block used by this actor's scripted-walk animation and placement.
///
/// The spawn and dispatcher publish the allocation also held by `Task::work`.
/// The included fragments require it to remain live; task teardown releases
/// it without clearing this pointer. This carrier has no walking countdown.
static _Actor146300Work* _gScriptedWalkWork;

/// Borrowed body task of this package's active scripted actor.
///
/// Published after successful spawn for animation messages, paired visibility
/// and model attachments. The body has 20 coordinates. Only the current live
/// instance may be used: a later spawn replaces the pointer, and teardown
/// leaves it dangling rather than clearing it. This cache owns no task.
static Task* _gActor146300BodyTask;

/// Borrowed helper-model task paired with this carrier's published body task.
///
/// Successful body setup publishes the handle for paired visibility and model
/// parenting. Teardown kills the helper before releasing the body; neither
/// published handle is cleared. Consumers require the current pair to be live.
static Task* _gActorMsgHelperTask;

static void _actor146300UpdateModel(Enemy* unusedEnemy, Task* task);
static void _actor146300Destroy(Task* task);
static void _actor146300UpdateAnimation(Task* unusedTask);

static TmdSource _gActor146300Model0895C;
static TmdSource _gActor146300Actor113100Model07960;
static void      _actor146300ScriptedFigureTask(Task* task);
static void      _actor146300AttachmentTask(Task* task);

static s32 _actor146300PlayAnimation(Task* unusedTask, s32 messageId, const AnimationPlayRequest* request, s32 unusedArgument);
static s32 _actor146300IgnoreCommand(Task* unusedTask, s32 messageId, const ActorCommand* unusedCommand, s32 unusedArgument);

static void _actor146300ApplyHandoverChoice(s32 phase);

static void _actor146300BeginWaterTankEvent(void);

static void _actor146300IceBagHandoverTask(Task* task);

// Progress counts the three bags consumed after the initial water-tank event.
enum {
    ACTOR146300_HANDOVER_AWAIT_FIRST_BAG  = 2,
    ACTOR146300_HANDOVER_AWAIT_SECOND_BAG = 3,
    ACTOR146300_HANDOVER_AWAIT_THIRD_BAG  = 4,
    ACTOR146300_HANDOVER_COMPLETE         = 5,
};

// CAP slots and two-bit object records for successive handover conversations.
enum {
    ACTOR146300_CAP_NO_ICE_BAG             = 0x12,
    ACTOR146300_CAP_FIRST_BAG              = 0x13,
    ACTOR146300_CAP_SECOND_BAG             = 0x14,
    ACTOR146300_CAP_FINAL_TALK             = 0x15,
    ACTOR146300_FIRST_BAG_TALK_OBJECT      = 0x1F,
    ACTOR146300_SECOND_BAG_TALK_OBJECT     = 0x20,
    ACTOR146300_FINAL_TALK_OBJECT          = 0x21,
    ACTOR146300_HANDOVER_OBJECT_TALK_READY = 1,
};

static AnimationPackedPose _gActor146300Animation01330Bank1[12] = {
#include "assets/actor_146300_animation_01330_bank1.inc"
};

static AnimationPackedRotation _gActor146300Animation01330Bank4[135] = {
#include "assets/actor_146300_animation_01330_bank4.inc"
};

static AnimationRecord _gActor146300Animation01330Records[176] = {
#include "assets/actor_146300_animation_01330_records.inc"
};

static u16 _gActor146300Animation01330Indices[20] = {
#include "assets/actor_146300_animation_01330_indices.inc"
};

static AnimationSet _gActor146300Animation01330 = {
    _gActor146300Animation01330Records,
    _gActor146300Animation01330Indices,
    { NULL, _gActor146300Animation01330Bank1, NULL, NULL, _gActor146300Animation01330Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor146300Animation016C8Bank1[5] = {
#include "assets/actor_146300_animation_016C8_bank1.inc"
};

static AnimationPackedRotation _gActor146300Animation016C8Bank4[57] = {
#include "assets/actor_146300_animation_016C8_bank4.inc"
};

static AnimationRecord _gActor146300Animation016C8Records[138] = {
#include "assets/actor_146300_animation_016C8_records.inc"
};

static u16 _gActor146300Animation016C8Indices[20] = {
#include "assets/actor_146300_animation_016C8_indices.inc"
};

static AnimationSet _gActor146300Animation016C8 = {
    _gActor146300Animation016C8Records,
    _gActor146300Animation016C8Indices,
    { NULL, _gActor146300Animation016C8Bank1, NULL, NULL, _gActor146300Animation016C8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor146300Animation01E30Bank1[23] = {
#include "assets/actor_146300_animation_01E30_bank1.inc"
};

static AnimationPackedRotation _gActor146300Animation01E30Bank4[166] = {
#include "assets/actor_146300_animation_01E30_bank4.inc"
};

static AnimationRecord _gActor146300Animation01E30Records[219] = {
#include "assets/actor_146300_animation_01E30_records.inc"
};

static u16 _gActor146300Animation01E30Indices[20] = {
#include "assets/actor_146300_animation_01E30_indices.inc"
};

static AnimationSet _gActor146300Animation01E30 = {
    _gActor146300Animation01E30Records,
    _gActor146300Animation01E30Indices,
    { NULL, _gActor146300Animation01E30Bank1, NULL, NULL, _gActor146300Animation01E30Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor146300Animation021A8Bank1[2] = {
#include "assets/actor_146300_animation_021A8_bank1.inc"
};

static AnimationPackedRotation _gActor146300Animation021A8Bank4[80] = {
#include "assets/actor_146300_animation_021A8_bank4.inc"
};

static AnimationRecord _gActor146300Animation021A8Records[116] = {
#include "assets/actor_146300_animation_021A8_records.inc"
};

static u16 _gActor146300Animation021A8Indices[20] = {
#include "assets/actor_146300_animation_021A8_indices.inc"
};

static AnimationSet _gActor146300Animation021A8 = {
    _gActor146300Animation021A8Records,
    _gActor146300Animation021A8Indices,
    { NULL, _gActor146300Animation021A8Bank1, NULL, NULL, _gActor146300Animation021A8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor146300Animation028E4Bank1[19] = {
#include "assets/actor_146300_animation_028E4_bank1.inc"
};

static AnimationPackedRotation _gActor146300Animation028E4Bank4[170] = {
#include "assets/actor_146300_animation_028E4_bank4.inc"
};

static AnimationRecord _gActor146300Animation028E4Records[216] = {
#include "assets/actor_146300_animation_028E4_records.inc"
};

static u16 _gActor146300Animation028E4Indices[20] = {
#include "assets/actor_146300_animation_028E4_indices.inc"
};

static AnimationSet _gActor146300Animation028E4 = {
    _gActor146300Animation028E4Records,
    _gActor146300Animation028E4Indices,
    { NULL, _gActor146300Animation028E4Bank1, NULL, NULL, _gActor146300Animation028E4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor146300Animation02B6CBank1[2] = {
#include "assets/actor_146300_animation_02B6C_bank1.inc"
};

static AnimationPackedRotation _gActor146300Animation02B6CBank4[51] = {
#include "assets/actor_146300_animation_02B6C_bank4.inc"
};

static AnimationRecord _gActor146300Animation02B6CRecords[85] = {
#include "assets/actor_146300_animation_02B6C_records.inc"
};

static u16 _gActor146300Animation02B6CIndices[20] = {
#include "assets/actor_146300_animation_02B6C_indices.inc"
};

static AnimationSet _gActor146300Animation02B6C = {
    _gActor146300Animation02B6CRecords,
    _gActor146300Animation02B6CIndices,
    { NULL, _gActor146300Animation02B6CBank1, NULL, NULL, _gActor146300Animation02B6CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor146300Animation0300CBank1[10] = {
#include "assets/actor_146300_animation_0300C_bank1.inc"
};

static AnimationPackedRotation _gActor146300Animation0300CBank4[96] = {
#include "assets/actor_146300_animation_0300C_bank4.inc"
};

static AnimationRecord _gActor146300Animation0300CRecords[150] = {
#include "assets/actor_146300_animation_0300C_records.inc"
};

static u16 _gActor146300Animation0300CIndices[20] = {
#include "assets/actor_146300_animation_0300C_indices.inc"
};

static AnimationSet _gActor146300Animation0300C = {
    _gActor146300Animation0300CRecords,
    _gActor146300Animation0300CIndices,
    { NULL, _gActor146300Animation0300CBank1, NULL, NULL, _gActor146300Animation0300CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor146300Animation0352CBank1[6] = {
#include "assets/actor_146300_animation_0352C_bank1.inc"
};

static AnimationPackedRotation _gActor146300Animation0352CBank4[124] = {
#include "assets/actor_146300_animation_0352C_bank4.inc"
};

static AnimationRecord _gActor146300Animation0352CRecords[166] = {
#include "assets/actor_146300_animation_0352C_records.inc"
};

static u16 _gActor146300Animation0352CIndices[20] = {
#include "assets/actor_146300_animation_0352C_indices.inc"
};

static AnimationSet _gActor146300Animation0352C = {
    _gActor146300Animation0352CRecords,
    _gActor146300Animation0352CIndices,
    { NULL, _gActor146300Animation0352CBank1, NULL, NULL, _gActor146300Animation0352CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor146300Animation03804Bank1[2] = {
#include "assets/actor_146300_animation_03804_bank1.inc"
};

static AnimationPackedRotation _gActor146300Animation03804Bank4[43] = {
#include "assets/actor_146300_animation_03804_bank4.inc"
};

static AnimationRecord _gActor146300Animation03804Records[113] = {
#include "assets/actor_146300_animation_03804_records.inc"
};

static u16 _gActor146300Animation03804Indices[20] = {
#include "assets/actor_146300_animation_03804_indices.inc"
};

static AnimationSet _gActor146300Animation03804 = {
    _gActor146300Animation03804Records,
    _gActor146300Animation03804Indices,
    { NULL, _gActor146300Animation03804Bank1, NULL, NULL, _gActor146300Animation03804Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor146300Animation03ACCBank1[3] = {
#include "assets/actor_146300_animation_03ACC_bank1.inc"
};

static AnimationPackedRotation _gActor146300Animation03ACCBank4[57] = {
#include "assets/actor_146300_animation_03ACC_bank4.inc"
};

static AnimationRecord _gActor146300Animation03ACCRecords[92] = {
#include "assets/actor_146300_animation_03ACC_records.inc"
};

static u16 _gActor146300Animation03ACCIndices[20] = {
#include "assets/actor_146300_animation_03ACC_indices.inc"
};

static AnimationSet _gActor146300Animation03ACC = {
    _gActor146300Animation03ACCRecords,
    _gActor146300Animation03ACCIndices,
    { NULL, _gActor146300Animation03ACCBank1, NULL, NULL, _gActor146300Animation03ACCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor146300Animation03FECBank1[6] = {
#include "assets/actor_146300_animation_03FEC_bank1.inc"
};

static AnimationPackedRotation _gActor146300Animation03FECBank4[124] = {
#include "assets/actor_146300_animation_03FEC_bank4.inc"
};

static AnimationRecord _gActor146300Animation03FECRecords[166] = {
#include "assets/actor_146300_animation_03FEC_records.inc"
};

static u16 _gActor146300Animation03FECIndices[20] = {
#include "assets/actor_146300_animation_03FEC_indices.inc"
};

static AnimationSet _gActor146300Animation03FEC = {
    _gActor146300Animation03FECRecords,
    _gActor146300Animation03FECIndices,
    { NULL, _gActor146300Animation03FECBank1, NULL, NULL, _gActor146300Animation03FECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor146300Animation04394Bank1[7] = {
#include "assets/actor_146300_animation_04394_bank1.inc"
};

static AnimationPackedRotation _gActor146300Animation04394Bank4[81] = {
#include "assets/actor_146300_animation_04394_bank4.inc"
};

static AnimationRecord _gActor146300Animation04394Records[112] = {
#include "assets/actor_146300_animation_04394_records.inc"
};

static u16 _gActor146300Animation04394Indices[20] = {
#include "assets/actor_146300_animation_04394_indices.inc"
};

static AnimationSet _gActor146300Animation04394 = {
    _gActor146300Animation04394Records,
    _gActor146300Animation04394Indices,
    { NULL, _gActor146300Animation04394Bank1, NULL, NULL, _gActor146300Animation04394Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor146300Animation04758Bank1[7] = {
#include "assets/actor_146300_animation_04758_bank1.inc"
};

static AnimationPackedRotation _gActor146300Animation04758Bank4[81] = {
#include "assets/actor_146300_animation_04758_bank4.inc"
};

static AnimationRecord _gActor146300Animation04758Records[119] = {
#include "assets/actor_146300_animation_04758_records.inc"
};

static u16 _gActor146300Animation04758Indices[20] = {
#include "assets/actor_146300_animation_04758_indices.inc"
};

static AnimationSet _gActor146300Animation04758 = {
    _gActor146300Animation04758Records,
    _gActor146300Animation04758Indices,
    { NULL, _gActor146300Animation04758Bank1, NULL, NULL, _gActor146300Animation04758Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor146300Animation049F0Bank1[2] = {
#include "assets/actor_146300_animation_049F0_bank1.inc"
};

static AnimationPackedRotation _gActor146300Animation049F0Bank4[52] = {
#include "assets/actor_146300_animation_049F0_bank4.inc"
};

static AnimationRecord _gActor146300Animation049F0Records[88] = {
#include "assets/actor_146300_animation_049F0_records.inc"
};

static u16 _gActor146300Animation049F0Indices[20] = {
#include "assets/actor_146300_animation_049F0_indices.inc"
};

static AnimationSet _gActor146300Animation049F0 = {
    _gActor146300Animation049F0Records,
    _gActor146300Animation049F0Indices,
    { NULL, _gActor146300Animation049F0Bank1, NULL, NULL, _gActor146300Animation049F0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor146300Animation04C7CBank1[2] = {
#include "assets/actor_146300_animation_04C7C_bank1.inc"
};

static AnimationPackedRotation _gActor146300Animation04C7CBank4[49] = {
#include "assets/actor_146300_animation_04C7C_bank4.inc"
};

static AnimationRecord _gActor146300Animation04C7CRecords[88] = {
#include "assets/actor_146300_animation_04C7C_records.inc"
};

static u16 _gActor146300Animation04C7CIndices[20] = {
#include "assets/actor_146300_animation_04C7C_indices.inc"
};

static AnimationSet _gActor146300Animation04C7C = {
    _gActor146300Animation04C7CRecords,
    _gActor146300Animation04C7CIndices,
    { NULL, _gActor146300Animation04C7CBank1, NULL, NULL, _gActor146300Animation04C7CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor146300Animation05354Bank1[14] = {
#include "assets/actor_146300_animation_05354_bank1.inc"
};

static AnimationPackedRotation _gActor146300Animation05354Bank4[155] = {
#include "assets/actor_146300_animation_05354_bank4.inc"
};

static AnimationRecord _gActor146300Animation05354Records[221] = {
#include "assets/actor_146300_animation_05354_records.inc"
};

static u16 _gActor146300Animation05354Indices[20] = {
#include "assets/actor_146300_animation_05354_indices.inc"
};

static AnimationSet _gActor146300Animation05354 = {
    _gActor146300Animation05354Records,
    _gActor146300Animation05354Indices,
    { NULL, _gActor146300Animation05354Bank1, NULL, NULL, _gActor146300Animation05354Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor146300Animation0568CBank1[5] = {
#include "assets/actor_146300_animation_0568C_bank1.inc"
};

static AnimationPackedRotation _gActor146300Animation0568CBank4[65] = {
#include "assets/actor_146300_animation_0568C_bank4.inc"
};

static AnimationRecord _gActor146300Animation0568CRecords[106] = {
#include "assets/actor_146300_animation_0568C_records.inc"
};

static u16 _gActor146300Animation0568CIndices[20] = {
#include "assets/actor_146300_animation_0568C_indices.inc"
};

static AnimationSet _gActor146300Animation0568C = {
    _gActor146300Animation0568CRecords,
    _gActor146300Animation0568CIndices,
    { NULL, _gActor146300Animation0568CBank1, NULL, NULL, _gActor146300Animation0568CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor146300Animation05A44Bank1[3] = {
#include "assets/actor_146300_animation_05A44_bank1.inc"
};

static AnimationPackedRotation _gActor146300Animation05A44Bank4[81] = {
#include "assets/actor_146300_animation_05A44_bank4.inc"
};

static AnimationRecord _gActor146300Animation05A44Records[128] = {
#include "assets/actor_146300_animation_05A44_records.inc"
};

static u16 _gActor146300Animation05A44Indices[20] = {
#include "assets/actor_146300_animation_05A44_indices.inc"
};

static AnimationSet _gActor146300Animation05A44 = {
    _gActor146300Animation05A44Records,
    _gActor146300Animation05A44Indices,
    { NULL, _gActor146300Animation05A44Bank1, NULL, NULL, _gActor146300Animation05A44Bank4, NULL, NULL, NULL },
};

enum { ACTOR146300_HANDOVER_TASK_PRIORITY = 192 };

TaskDesc gActor146300IceBagHandoverTaskDesc = {
    .header   = { .fields = { .flags = TASK_BODY_NONE, .priority = ACTOR146300_HANDOVER_TASK_PRIORITY } },
    .callback = _actor146300IceBagHandoverTask,
    .data     = { .value = 0 },
};

/// Player animation sets installed as weapon-bank clips 47 through 64.
///
/// The player borrows the descriptors while this package stays loaded. The
/// fixed-capacity copy also reads fourteen words after the eighteen pointers:
/// all of `_gActor146300PlayerClip47ResetRequest` and `_gActor146300PlayerClip47Request`,
/// then the first four words of `_gActor146300PlayerClip48Request`.
/// Keep this table and those records contiguous. Only the first eighteen
/// copied entries are playable clips; the remaining words are request data.
static AnimationSet* _gActor146300PlayerBankExtension[] = { &_gActor146300Animation01330, &_gActor146300Animation016C8, &_gActor146300Animation01E30, &_gActor146300Animation021A8, &_gActor146300Animation028E4, &_gActor146300Animation02B6C, &_gActor146300Animation0300C, &_gActor146300Animation0352C, &_gActor146300Animation03804, &_gActor146300Animation03ACC, &_gActor146300Animation03FEC, &_gActor146300Animation04394, &_gActor146300Animation04758, &_gActor146300Animation049F0, &_gActor146300Animation04C7C, &_gActor146300Animation05354, &_gActor146300Animation0568C, &_gActor146300Animation05A44 };

/// Retained player clip-47 reset request included in the raw bank-extension copy.
///
/// No script dispatches it. Its five words occupy copied bank entries 65..69,
/// beyond the playable extension's clips 47..64. Keep it immediately after
/// `_gActor146300PlayerBankExtension`; the separately dispatched clip-47 request
/// blends over five frames and has different contents.
static AnimationPlayRequest _gActor146300PlayerClip47ResetRequest = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays player clip 47 during the opening water-tank event, blending over 5 normal-rate frames.
///
/// Requires this package's copied bank extension. The event runner replaces
/// the stored bank index with the equipped weapon's index before dispatch.
/// Playback disables world-grid collision.
static AnimationPlayRequest _gActor146300PlayerClip47Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 5,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays player clip 48 during the opening water-tank event, restarting without a pose transition.
///
/// Requires this package's copied bank extension. The event runner replaces
/// the stored bank index with the equipped weapon's index before dispatch.
/// Playback disables world-grid collision.
static AnimationPlayRequest _gActor146300PlayerClip48Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays player clip 49 during the opening water-tank event, restarting without a pose transition.
///
/// Requires this package's copied bank extension. The event runner replaces
/// the stored bank index with the equipped weapon's index before dispatch.
/// Playback disables world-grid collision.
static AnimationPlayRequest _gActor146300PlayerClip49Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 2,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays player clip 50 during the opening water-tank event, restarting without a pose transition.
///
/// Requires this package's copied bank extension. The event runner replaces
/// the stored bank index with the equipped weapon's index before dispatch.
/// Playback disables world-grid collision.
static AnimationPlayRequest _gActor146300PlayerClip50Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 3,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays player clip 51 during the opening water-tank event, restarting without a pose transition.
///
/// Requires this package's copied bank extension. The event runner replaces
/// the stored bank index with the equipped weapon's index before dispatch.
/// Playback disables world-grid collision.
static AnimationPlayRequest _gActor146300PlayerClip51Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 4,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays player clip 52 during the opening water-tank event, restarting without a pose transition.
///
/// Requires this package's copied bank extension. The event runner replaces
/// the stored bank index with the equipped weapon's index before dispatch.
/// Playback disables world-grid collision.
static AnimationPlayRequest _gActor146300PlayerClip52Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 5,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays player clip 53 during the opening water-tank event, restarting without a pose transition.
///
/// Requires this package's copied bank extension. The event runner replaces
/// the stored bank index with the equipped weapon's index before dispatch.
/// Playback disables world-grid collision.
static AnimationPlayRequest _gActor146300PlayerClip53Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 6,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays player clip 54 during the opening water-tank event, blending over 10 normal-rate frames.
///
/// Requires this package's copied bank extension. The event runner replaces
/// the stored bank index with the equipped weapon's index before dispatch.
/// Playback disables world-grid collision.
static AnimationPlayRequest _gActor146300PlayerClip54Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 7,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays player clip 55 during the opening water-tank event, blending over 10 normal-rate frames.
///
/// Requires this package's copied bank extension. The event runner replaces
/// the stored bank index with the equipped weapon's index before dispatch.
/// Playback disables world-grid collision.
static AnimationPlayRequest _gActor146300PlayerClip55Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 8,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays player clip 56 during the opening water-tank event, restarting without a pose transition.
///
/// Requires this package's copied bank extension. The event runner replaces
/// the stored bank index with the equipped weapon's index before dispatch.
/// Playback disables world-grid collision.
static AnimationPlayRequest _gActor146300PlayerClip56Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 9,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays player clip 57 during the opening water-tank event, restarting without a pose transition.
///
/// Requires this package's copied bank extension. The event runner replaces
/// the stored bank index with the equipped weapon's index before dispatch.
/// Playback disables world-grid collision.
static AnimationPlayRequest _gActor146300PlayerClip57Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 10,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays player clip 58 during the opening water-tank event, blending over 10 normal-rate frames.
///
/// Requires this package's copied bank extension. The event runner replaces
/// the stored bank index with the equipped weapon's index before dispatch.
/// Playback disables world-grid collision.
static AnimationPlayRequest _gActor146300PlayerClip58Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 11,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays player clip 59 during the opening water-tank event, restarting without a pose transition.
///
/// Requires this package's copied bank extension. The event runner replaces
/// the stored bank index with the equipped weapon's index before dispatch.
/// Playback disables world-grid collision.
static AnimationPlayRequest _gActor146300PlayerClip59Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 12,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays player clip 60 during the opening water-tank event, restarting without a pose transition.
///
/// Requires this package's copied bank extension. The event runner replaces
/// the stored bank index with the equipped weapon's index before dispatch.
/// Playback disables world-grid collision.
static AnimationPlayRequest _gActor146300PlayerClip60Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 13,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays player clip 61 during the opening water-tank event, restarting without a pose transition.
///
/// Requires this package's copied bank extension. The event runner replaces
/// the stored bank index with the equipped weapon's index before dispatch.
/// Playback disables world-grid collision.
static AnimationPlayRequest _gActor146300PlayerClip61Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 14,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays player clip 62 during an ice-bag handover, restarting without a pose transition.
///
/// Requires this package's copied bank extension. The event runner replaces
/// the stored bank index with the equipped weapon's index before dispatch.
/// Playback disables world-grid collision.
static AnimationPlayRequest _gActor146300PlayerClip62Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 15,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays player clip 63 during an ice-bag handover, restarting without a pose transition.
///
/// Requires this package's copied bank extension. The event runner replaces
/// the stored bank index with the equipped weapon's index before dispatch.
/// Playback disables world-grid collision.
static AnimationPlayRequest _gActor146300PlayerClip63Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 16,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Retained clip-0 reset request beside the figure's playable clip requests.
///
/// No code dispatches this record. Figure bank entry 0 is NULL, so this request
/// cannot be played by the initialized figure. Preserve its five stored words.
static AnimationPlayRequest _gActor146300FigureClip0Request = {
    .source               = { .index = 1 },
    .animationId          = 0,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays figure clip 1 at the first opening-dialogue cue, restarting without a pose transition.
///
/// Placement 0's handler consumes the request synchronously. Its fixed
/// clip table ignores the stored bank index and world-collision word.
static AnimationPlayRequest _gActor146300FigureClip1Request = {
    .source               = { .index = 1 },
    .animationId          = 1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays figure clip 2 at the second opening-dialogue cue, restarting without a pose transition.
///
/// Placement 0's handler consumes the request synchronously. Its fixed
/// clip table ignores the stored bank index and world-collision word.
static AnimationPlayRequest _gActor146300FigureClip2Request = {
    .source               = { .index = 1 },
    .animationId          = 2,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays figure clip 3 at the third opening-dialogue cue, restarting without a pose transition.
///
/// Placement 0's handler consumes the request synchronously. Its fixed
/// clip table ignores the stored bank index and world-collision word.
static AnimationPlayRequest _gActor146300FigureClip3Request = {
    .source               = { .index = 1 },
    .animationId          = 3,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays figure clip 4 at water-tank and handover dialogue cues, blending over 10 normal-rate frames.
///
/// Placement 0's handler consumes the request synchronously. Its fixed
/// clip table ignores the stored bank index and world-collision word.
static AnimationPlayRequest _gActor146300FigureClip4Request = {
    .source               = { .index = 1 },
    .animationId          = 4,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays figure clip 5 for the handover conversation and its restored pose, blending over 10 normal-rate frames.
///
/// Placement 0's handler consumes the request synchronously. Its fixed
/// clip table ignores the stored bank index and world-collision word.
static AnimationPlayRequest _gActor146300FigureClip5Request = {
    .source               = { .index = 1 },
    .animationId          = 5,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays figure clip 6 at the water-tank event's timed exchange, restarting its pose.
///
/// Placement 0 consumes the request synchronously; its fixed clip table ignores
/// the stored bank index and world-collision word.
static AnimationPlayRequest _gActor146300FigureClip6Request = {
    .source               = { .index = 1 },
    .animationId          = 6,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays figure clip 7 after the player clip-55 cue, restarting without a transition.
///
/// Placement 0 consumes the request synchronously; its fixed clip table ignores
/// the stored bank index and world-collision word.
static AnimationPlayRequest _gActor146300FigureClip7Request = {
    .source               = { .index = 1 },
    .animationId          = 7,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays figure clip 8 at the next timed water-tank cue, restarting its pose.
///
/// Placement 0 consumes the request synchronously; its fixed clip table ignores
/// the stored bank index and world-collision word.
static AnimationPlayRequest _gActor146300FigureClip8Request = {
    .source               = { .index = 1 },
    .animationId          = 8,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays figure clip 9 with player clip 61 during the water-tank event.
///
/// Restarts without a transition. Placement 0 consumes the request synchronously;
/// its fixed clip table ignores the stored bank index and world-collision word.
static AnimationPlayRequest _gActor146300FigureClip9Request = {
    .source               = { .index = 1 },
    .animationId          = 9,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays figure clip 10 for the first retained CAP choice, blending over 30 normal-rate frames.
///
/// Placement 0's handler consumes the request synchronously. Its fixed
/// clip table ignores the stored bank index and world-collision word.
static AnimationPlayRequest _gActor146300FigureClip10Request = {
    .source               = { .index = 1 },
    .animationId          = 10,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 30,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts figure clip 11 before the water-tank event places the player.
///
/// Placement 0 consumes the request synchronously; its fixed clip table ignores
/// the stored bank index and enabled world-collision word.
static AnimationPlayRequest _gActor146300FigureClip11Request = {
    .source               = { .index = 1 },
    .animationId          = 11,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE,
};

/// Plays figure clip 12 when restoring the pose awaiting an ice bag, restarting without a pose transition.
///
/// Placement 0's handler consumes the request synchronously. Its fixed
/// clip table ignores the stored bank index and world-collision word.
static AnimationPlayRequest _gActor146300FigureClip12Request = {
    .source               = { .index = 1 },
    .animationId          = 12,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays figure clip 13 at a water-tank dialogue cue, blending over 10 normal-rate frames.
///
/// Placement 0 consumes the request synchronously; its fixed clip table ignores
/// the stored bank index and world-collision word.
static AnimationPlayRequest _gActor146300FigureClip13Request = {
    .source               = { .index = 1 },
    .animationId          = 13,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays figure clip 14 when restoring the final handover pose, restarting without a pose transition.
///
/// Placement 0's handler consumes the request synchronously. Its fixed
/// clip table ignores the stored bank index and world-collision word.
static AnimationPlayRequest _gActor146300FigureClip14Request = {
    .source               = { .index = 1 },
    .animationId          = 14,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Restarts figure clip 15 between the final ice-bag handover's CAP cues.
///
/// Placement 0 consumes the request synchronously; its fixed clip table ignores
/// the stored bank index and world-collision word.
static AnimationPlayRequest _gActor146300FigureClip15Request = {
    .source               = { .index = 1 },
    .animationId          = 15,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays figure clip 16 at the final-bag and final-talk dialogue cues, restarting without a pose transition.
///
/// Placement 0's handler consumes the request synchronously. Its fixed
/// clip table ignores the stored bank index and world-collision word.
static AnimationPlayRequest _gActor146300FigureClip16Request = {
    .source               = { .index = 1 },
    .animationId          = 16,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays player clip 1 on return from the water-tank event, restarting without a pose transition.
///
/// Uses the weapon bank's base clip. The event runner replaces
/// the stored bank index with the equipped weapon's index before dispatch.
/// Playback disables world-grid collision.
static AnimationPlayRequest _gActor146300PlayerClip1Request = {
    .source               = { .index = 1 },
    .animationId          = 1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Blends the player into weapon-bank clip 1 over 20 normal-rate frames at event entry.
///
/// The event runner copies this request and replaces its bank index with the
/// equipped weapon's index before dispatch. Playback disables world-grid collision.
static AnimationPlayRequest _gActor146300PlayerClip1BlendRequest = {
    .source               = { .index = 1 },
    .animationId          = 1,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 20,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Plays player clip 64 during an ice-bag handover, restarting without a pose transition.
///
/// Requires this package's copied bank extension. The event runner replaces
/// the stored bank index with the equipped weapon's index before dispatch.
/// Playback disables world-grid collision.
static AnimationPlayRequest _gActor146300PlayerClip64Request = {
    .source               = { .index = 1 },
    .animationId          = ANIMATION_BANK_BASE_SET_COUNT + 17,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Installs the water-tank player clips in the equipped weapon's bank extension.
///
/// Copies 32 words without starting playback: eighteen clip pointers followed
/// by fourteen adjacent request words. The playable range is 47..64; the copy
/// length is the bank capacity, rather than the number of clips. The request
/// is borrowed through dispatch; the copied clip data must remain loaded.
static AnimationBankCopyRequest _gActor146300CopyPlayerBankExtensionRequest = {
    .source    = { .sets = _gActor146300PlayerBankExtension },
    .wordCount = ANIMATION_BANK_EXTENSION_CAPACITY,
};

/// Player placement at the first animation of the water-tank event.
///
/// Position uses whole room-coordinate units; yaw is 1820/4096 of a turn.
/// The placement message consumes both vectors synchronously.
static ActorTransform _gActor146300PlayerEventTransform = {
    .pos = { 1110, -12000, -2500, 0 },
    .rot = { 0, 1820, 0, 0 },
};

/// Player placement restored after completion or skipping of the water-tank event.
///
/// Position uses whole room-coordinate units; yaw is 1820/4096 of a turn.
/// The placement message consumes both vectors synchronously before control resumes.
static ActorTransform _gActor146300PlayerResumeTransform = {
    .pos = { 1500, -12000, -1744, 0 },
    .rot = { 0, 1820, 0, 0 },
};

/// Figure placement restored for the final ice-bag handover pose.
///
/// Position uses whole room-coordinate units; yaw is 682/4096 of a turn.
/// Placement 0 consumes both vectors synchronously when progress is complete
/// or a third bag is already held.
static ActorTransform _gActor146300FigureFinalHandoverTransform = {
    .pos = { 1110, -12000, -2000, 0 },
    .rot = { 0, 682, 0, 0 },
};

// CAP command slots, placement selection and sound cues of these linear streams.
enum {
    ACTOR146300_CAP_WATER_TANK_EVENT         = 16,
    ACTOR146300_CAP_AFTER_WATER_TANK_EVENT   = 17,
    ACTOR146300_CAP_HANDOVER_REPEAT_TALK     = 22,
    ACTOR146300_EVENT_FIGURE_PLACEMENT       = 0,
    ACTOR146300_EVENT_RESUME_VIEW            = 3,
    ACTOR146300_EVENT_WEAPONS_PLAYER         = 0,
    ACTOR146300_HANDOVER_CHOICE_INITIAL_CUE  = 0,
    ACTOR146300_HANDOVER_CHOICE_FOLLOWUP_CUE = 1,
    ACTOR146300_EVENT_SOUND_CUE10            = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_WATER_TANK, 10),
    ACTOR146300_EVENT_SOUND_CUE11            = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_WATER_TANK, 11),
    ACTOR146300_EVENT_SOUND_CUE12            = SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_WATER_TANK, 12),
    ACTOR146300_EVENT_STOP_SOUND_TYPE        = 13,
};

EvsCommand gActor146300WaterTankEventScript[99] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor146300BeginWaterTankEvent }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR146300_CAP_WATER_TANK_EVENT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor146300CopyPlayerBankExtensionRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146300PlayerClip1BlendRequest }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = ACTOR146300_EVENT_STOP_SOUND_TYPE }, { .value = SOUND_SCRIPT_STOP_KEEP_RELEASE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = ACTOR146300_EVENT_WEAPONS_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip11Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor146300PlayerEventTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146300PlayerClip47Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = ACTOR146300_EVENT_SOUND_CUE10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146300PlayerClip47Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip1Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146300PlayerClip48Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146300PlayerClip49Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip2Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146300PlayerClip50Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip3Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146300PlayerClip51Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip4Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip5Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146300PlayerClip52Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146300PlayerClip54Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip4Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip5Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146300PlayerClip56Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 26 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146300PlayerClip57Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip4Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip4Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip4Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip13Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip5Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146300PlayerClip56Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 26 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146300PlayerClip53Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146300PlayerClip58Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip6Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 26 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146300PlayerClip59Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146300PlayerClip55Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip7Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 45 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip8Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 28 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = ACTOR146300_EVENT_SOUND_CUE11 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146300PlayerClip60Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146300PlayerClip61Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip9Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip4Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip10Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = ACTOR146300_EVENT_SOUND_CUE12 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146300PlayerClip58Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor146300PlayerResumeTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR146300_EVENT_RESUME_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = ACTOR146300_EVENT_WEAPONS_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146300PlayerClip1Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip5Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gActor146300WaterTankEventSkipScript[14] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR146300_EVENT_RESUME_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = ACTOR146300_EVENT_WEAPONS_PLAYER }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146300PlayerClip1Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip12Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor146300PlayerResumeTransform } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR146300_CAP_AFTER_WATER_TANK_EVENT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Synchronizes the first or second bag handover with the selected CAP slot's variant 0.
///
/// The task starts CAP slot 19 or 20 first. Hides the HUD, installs the player's
/// extension and plays clips 64, 62 and 63 at CAP cues while the figure uses
/// clip 4. All 14 commands, their actor requests and sets must stay loaded until
/// the stream ends; the task starts the choice variant afterwards.
static EvsCommand _gActor146300IceBagHandoverScript[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_HIDE_HUD }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor146300CopyPlayerBankExtensionRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146300PlayerClip64Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip4Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146300PlayerClip62Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146300PlayerClip63Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip4Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Applies both retained-choice cues during a handover CAP slot's variant 1.
///
/// The task starts slot 19 or 20 first. The two callbacks select figure clips
/// from the CAP variant key, then the stream restores the HUD. Requires the
/// initialized placement-0 figure and loaded CAP/script resources through all
/// eight commands; the handover task releases player control after completion.
static EvsCommand _gActor146300HandoverChoiceScript[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip4Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor146300ApplyHandoverChoice }, { .value = ACTOR146300_HANDOVER_CHOICE_INITIAL_CUE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor146300ApplyHandoverChoice }, { .value = ACTOR146300_HANDOVER_CHOICE_FOLLOWUP_CUE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_SHOW_HUD }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Starts CAP slot 21 and synchronizes the final bag handover's animation cues.
///
/// Hides the HUD, installs the player extension and alternates player clip 64
/// with figure clips 16 and 15. Requires live player/placement-0 tasks and loaded
/// CAP slots, request records and sets through all 15 commands. The handover task
/// starts slot 21's choice variant after this stream ends.
static EvsCommand _gActor146300FinalIceBagHandoverScript[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR146300_CAP_FINAL_TALK }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_HIDE_HUD }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor146300CopyPlayerBankExtensionRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip16Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146300PlayerClip64Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip15Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor146300PlayerClip64Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip16Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Finishes the final handover's CAP variant-1 conversation and restores its pose.
///
/// The task starts slot 21 first. Plays figure clip 16 through the CAP cue,
/// restores the HUD and requests clip 14 before ending. The six-command stream
/// borrows the initialized placement-0 figure and loaded requests; the task
/// releases player control after completion.
static EvsCommand _gActor146300FinalHandoverTalkScript[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip16Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_SHOW_HUD }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip14Request } }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

/// Runs CAP slot 22 for a repeat conversation after the final talk is no longer ready.
///
/// Plays figure clip 16 through the CAP cue, releases the player's scripted
/// control and restores figure clip 14. The spawner kills its handover task
/// immediately, so this seven-command stream completes independently. Requires
/// the player, initialized placement-0 figure and loaded CAP/script resources
/// until event completion; the interpreter restores the HUD at its end.
static EvsCommand _gActor146300HandoverRepeatTalkScript[] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR146300_CAP_HANDOVER_REPEAT_TALK }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip16Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = PLAYER_ACTOR_END_SCRIPTED_DEFAULT }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = ACTOR146300_EVENT_FIGURE_PLACEMENT }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor146300FigureClip14Request } }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

static TmdBone _gActor146300Model0895CSkeleton[20] = {
#include "assets/actor_146300_model_0895C_skeleton.inc"
};

static u32 _gActor146300Model0895CPartVerts[20] = {
#include "assets/actor_146300_model_0895C_partVerts.inc"
};

static SVECTOR _gActor146300Model0895CVerts[390] = {
#include "assets/actor_146300_model_0895C_verts.inc"
};

static SVECTOR _gActor146300Model0895CNormals[407] = {
#include "assets/actor_146300_model_0895C_normals.inc"
};

static u32 _gActor146300Model0895CStream[4476] = {
#include "assets/actor_146300_model_0895C_stream.inc"
};

static TmdSource _gActor146300Model0895C = {
    0,
    24444,
    6776,
    20,
    _gActor146300Model0895CPartVerts,
    _gActor146300Model0895CVerts,
    _gActor146300Model0895CNormals,
    _gActor146300Model0895CSkeleton,
    _gActor146300Model0895CStream,
};

static TmdBone _gActor146300Actor113100Model07960Skeleton[1] = {
#include "assets/actor_113100_model_07960_skeleton.inc"
};

static u32 _gActor146300Actor113100Model07960PartVerts[1] = {
#include "assets/actor_113100_model_07960_partVerts.inc"
};

static SVECTOR _gActor146300Actor113100Model07960Verts[14] = {
#include "assets/actor_113100_model_07960_verts.inc"
};

static SVECTOR _gActor146300Actor113100Model07960Normals[12] = {
#include "assets/actor_113100_model_07960_normals.inc"
};

static u32 _gActor146300Actor113100Model07960Stream[56] = {
#include "assets/actor_113100_model_07960_stream.inc"
};

static TmdSource _gActor146300Actor113100Model07960 = {
    0,
    340,
    0,
    1,
    _gActor146300Actor113100Model07960PartVerts,
    _gActor146300Actor113100Model07960Verts,
    _gActor146300Actor113100Model07960Normals,
    _gActor146300Actor113100Model07960Skeleton,
    _gActor146300Actor113100Model07960Stream,
};

static AnimationPackedPose _gActor146300Animation0D4CCBank1[6] = {
#include "assets/actor_146300_animation_0D4CC_bank1.inc"
};

static AnimationPackedRotation _gActor146300Animation0D4CCBank4[59] = {
#include "assets/actor_146300_animation_0D4CC_bank4.inc"
};

static AnimationRecord _gActor146300Animation0D4CCRecords[130] = {
#include "assets/actor_146300_animation_0D4CC_records.inc"
};

static u16 _gActor146300Animation0D4CCIndices[20] = {
#include "assets/actor_146300_animation_0D4CC_indices.inc"
};

static AnimationSet _gActor146300Animation0D4CC = {
    _gActor146300Animation0D4CCRecords,
    _gActor146300Animation0D4CCIndices,
    { NULL, _gActor146300Animation0D4CCBank1, NULL, NULL, _gActor146300Animation0D4CCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor146300Animation0DB80Bank1[9] = {
#include "assets/actor_146300_animation_0DB80_bank1.inc"
};

static AnimationPackedRotation _gActor146300Animation0DB80Bank4[163] = {
#include "assets/actor_146300_animation_0DB80_bank4.inc"
};

static AnimationRecord _gActor146300Animation0DB80Records[219] = {
#include "assets/actor_146300_animation_0DB80_records.inc"
};

static u16 _gActor146300Animation0DB80Indices[20] = {
#include "assets/actor_146300_animation_0DB80_indices.inc"
};

static AnimationSet _gActor146300Animation0DB80 = {
    _gActor146300Animation0DB80Records,
    _gActor146300Animation0DB80Indices,
    { NULL, _gActor146300Animation0DB80Bank1, NULL, NULL, _gActor146300Animation0DB80Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor146300Animation0E208Bank1[9] = {
#include "assets/actor_146300_animation_0E208_bank1.inc"
};

static AnimationPackedRotation _gActor146300Animation0E208Bank4[149] = {
#include "assets/actor_146300_animation_0E208_bank4.inc"
};

static AnimationRecord _gActor146300Animation0E208Records[222] = {
#include "assets/actor_146300_animation_0E208_records.inc"
};

static u16 _gActor146300Animation0E208Indices[20] = {
#include "assets/actor_146300_animation_0E208_indices.inc"
};

static AnimationSet _gActor146300Animation0E208 = {
    _gActor146300Animation0E208Records,
    _gActor146300Animation0E208Indices,
    { NULL, _gActor146300Animation0E208Bank1, NULL, NULL, _gActor146300Animation0E208Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor146300Animation0E5C8Bank1[2] = {
#include "assets/actor_146300_animation_0E5C8_bank1.inc"
};

static AnimationPackedRotation _gActor146300Animation0E5C8Bank4[78] = {
#include "assets/actor_146300_animation_0E5C8_bank4.inc"
};

static AnimationRecord _gActor146300Animation0E5C8Records[136] = {
#include "assets/actor_146300_animation_0E5C8_records.inc"
};

static u16 _gActor146300Animation0E5C8Indices[20] = {
#include "assets/actor_146300_animation_0E5C8_indices.inc"
};

static AnimationSet _gActor146300Animation0E5C8 = {
    _gActor146300Animation0E5C8Records,
    _gActor146300Animation0E5C8Indices,
    { NULL, _gActor146300Animation0E5C8Bank1, NULL, NULL, _gActor146300Animation0E5C8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor146300Animation0E7D4Bank1[2] = {
#include "assets/actor_146300_animation_0E7D4_bank1.inc"
};

static AnimationPackedRotation _gActor146300Animation0E7D4Bank4[21] = {
#include "assets/actor_146300_animation_0E7D4_bank4.inc"
};

static AnimationRecord _gActor146300Animation0E7D4Records[84] = {
#include "assets/actor_146300_animation_0E7D4_records.inc"
};

static u16 _gActor146300Animation0E7D4Indices[20] = {
#include "assets/actor_146300_animation_0E7D4_indices.inc"
};

static AnimationSet _gActor146300Animation0E7D4 = {
    _gActor146300Animation0E7D4Records,
    _gActor146300Animation0E7D4Indices,
    { NULL, _gActor146300Animation0E7D4Bank1, NULL, NULL, _gActor146300Animation0E7D4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor146300Animation0EA84Bank1[2] = {
#include "assets/actor_146300_animation_0EA84_bank1.inc"
};

static AnimationPackedRotation _gActor146300Animation0EA84Bank4[56] = {
#include "assets/actor_146300_animation_0EA84_bank4.inc"
};

static AnimationRecord _gActor146300Animation0EA84Records[90] = {
#include "assets/actor_146300_animation_0EA84_records.inc"
};

static u16 _gActor146300Animation0EA84Indices[20] = {
#include "assets/actor_146300_animation_0EA84_indices.inc"
};

static AnimationSet _gActor146300Animation0EA84 = {
    _gActor146300Animation0EA84Records,
    _gActor146300Animation0EA84Indices,
    { NULL, _gActor146300Animation0EA84Bank1, NULL, NULL, _gActor146300Animation0EA84Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor146300Animation0ECC4Bank1[2] = {
#include "assets/actor_146300_animation_0ECC4_bank1.inc"
};

static AnimationPackedRotation _gActor146300Animation0ECC4Bank4[42] = {
#include "assets/actor_146300_animation_0ECC4_bank4.inc"
};

static AnimationRecord _gActor146300Animation0ECC4Records[76] = {
#include "assets/actor_146300_animation_0ECC4_records.inc"
};

static u16 _gActor146300Animation0ECC4Indices[20] = {
#include "assets/actor_146300_animation_0ECC4_indices.inc"
};

static AnimationSet _gActor146300Animation0ECC4 = {
    _gActor146300Animation0ECC4Records,
    _gActor146300Animation0ECC4Indices,
    { NULL, _gActor146300Animation0ECC4Bank1, NULL, NULL, _gActor146300Animation0ECC4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor146300Animation0F18CBank1[2] = {
#include "assets/actor_146300_animation_0F18C_bank1.inc"
};

static AnimationPackedRotation _gActor146300Animation0F18CBank4[115] = {
#include "assets/actor_146300_animation_0F18C_bank4.inc"
};

static AnimationRecord _gActor146300Animation0F18CRecords[165] = {
#include "assets/actor_146300_animation_0F18C_records.inc"
};

static u16 _gActor146300Animation0F18CIndices[20] = {
#include "assets/actor_146300_animation_0F18C_indices.inc"
};

static AnimationSet _gActor146300Animation0F18C = {
    _gActor146300Animation0F18CRecords,
    _gActor146300Animation0F18CIndices,
    { NULL, _gActor146300Animation0F18CBank1, NULL, NULL, _gActor146300Animation0F18CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor146300Animation0F414Bank1[2] = {
#include "assets/actor_146300_animation_0F414_bank1.inc"
};

static AnimationPackedRotation _gActor146300Animation0F414Bank4[48] = {
#include "assets/actor_146300_animation_0F414_bank4.inc"
};

static AnimationRecord _gActor146300Animation0F414Records[88] = {
#include "assets/actor_146300_animation_0F414_records.inc"
};

static u16 _gActor146300Animation0F414Indices[20] = {
#include "assets/actor_146300_animation_0F414_indices.inc"
};

static AnimationSet _gActor146300Animation0F414 = {
    _gActor146300Animation0F414Records,
    _gActor146300Animation0F414Indices,
    { NULL, _gActor146300Animation0F414Bank1, NULL, NULL, _gActor146300Animation0F414Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor146300Animation0F888Bank1[2] = {
#include "assets/actor_146300_animation_0F888_bank1.inc"
};

static AnimationPackedRotation _gActor146300Animation0F888Bank4[93] = {
#include "assets/actor_146300_animation_0F888_bank4.inc"
};

static AnimationRecord _gActor146300Animation0F888Records[166] = {
#include "assets/actor_146300_animation_0F888_records.inc"
};

static u16 _gActor146300Animation0F888Indices[20] = {
#include "assets/actor_146300_animation_0F888_indices.inc"
};

static AnimationSet _gActor146300Animation0F888 = {
    _gActor146300Animation0F888Records,
    _gActor146300Animation0F888Indices,
    { NULL, _gActor146300Animation0F888Bank1, NULL, NULL, _gActor146300Animation0F888Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor146300Animation0FA24Bank1[2] = {
#include "assets/actor_146300_animation_0FA24_bank1.inc"
};

static AnimationPackedRotation _gActor146300Animation0FA24Bank4[17] = {
#include "assets/actor_146300_animation_0FA24_bank4.inc"
};

static AnimationRecord _gActor146300Animation0FA24Records[60] = {
#include "assets/actor_146300_animation_0FA24_records.inc"
};

static u16 _gActor146300Animation0FA24Indices[20] = {
#include "assets/actor_146300_animation_0FA24_indices.inc"
};

static AnimationSet _gActor146300Animation0FA24 = {
    _gActor146300Animation0FA24Records,
    _gActor146300Animation0FA24Indices,
    { NULL, _gActor146300Animation0FA24Bank1, NULL, NULL, _gActor146300Animation0FA24Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor146300Animation0FCC4Bank1[2] = {
#include "assets/actor_146300_animation_0FCC4_bank1.inc"
};

static AnimationPackedRotation _gActor146300Animation0FCC4Bank4[37] = {
#include "assets/actor_146300_animation_0FCC4_bank4.inc"
};

static AnimationRecord _gActor146300Animation0FCC4Records[105] = {
#include "assets/actor_146300_animation_0FCC4_records.inc"
};

static u16 _gActor146300Animation0FCC4Indices[20] = {
#include "assets/actor_146300_animation_0FCC4_indices.inc"
};

static AnimationSet _gActor146300Animation0FCC4 = {
    _gActor146300Animation0FCC4Records,
    _gActor146300Animation0FCC4Indices,
    { NULL, _gActor146300Animation0FCC4Bank1, NULL, NULL, _gActor146300Animation0FCC4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor146300Animation0FED0Bank1[2] = {
#include "assets/actor_146300_animation_0FED0_bank1.inc"
};

static AnimationPackedRotation _gActor146300Animation0FED0Bank4[31] = {
#include "assets/actor_146300_animation_0FED0_bank4.inc"
};

static AnimationRecord _gActor146300Animation0FED0Records[74] = {
#include "assets/actor_146300_animation_0FED0_records.inc"
};

static u16 _gActor146300Animation0FED0Indices[20] = {
#include "assets/actor_146300_animation_0FED0_indices.inc"
};

static AnimationSet _gActor146300Animation0FED0 = {
    _gActor146300Animation0FED0Records,
    _gActor146300Animation0FED0Indices,
    { NULL, _gActor146300Animation0FED0Bank1, NULL, NULL, _gActor146300Animation0FED0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor146300Animation1014CBank1[2] = {
#include "assets/actor_146300_animation_1014C_bank1.inc"
};

static AnimationPackedRotation _gActor146300Animation1014CBank4[32] = {
#include "assets/actor_146300_animation_1014C_bank4.inc"
};

static AnimationRecord _gActor146300Animation1014CRecords[101] = {
#include "assets/actor_146300_animation_1014C_records.inc"
};

static u16 _gActor146300Animation1014CIndices[20] = {
#include "assets/actor_146300_animation_1014C_indices.inc"
};

static AnimationSet _gActor146300Animation1014C = {
    _gActor146300Animation1014CRecords,
    _gActor146300Animation1014CIndices,
    { NULL, _gActor146300Animation1014CBank1, NULL, NULL, _gActor146300Animation1014CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor146300Animation103E0Bank1[5] = {
#include "assets/actor_146300_animation_103E0_bank1.inc"
};

static AnimationPackedRotation _gActor146300Animation103E0Bank4[36] = {
#include "assets/actor_146300_animation_103E0_bank4.inc"
};

static AnimationRecord _gActor146300Animation103E0Records[94] = {
#include "assets/actor_146300_animation_103E0_records.inc"
};

static u16 _gActor146300Animation103E0Indices[20] = {
#include "assets/actor_146300_animation_103E0_indices.inc"
};

static AnimationSet _gActor146300Animation103E0 = {
    _gActor146300Animation103E0Records,
    _gActor146300Animation103E0Indices,
    { NULL, _gActor146300Animation103E0Bank1, NULL, NULL, _gActor146300Animation103E0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor146300Animation10954Bank1[2] = {
#include "assets/actor_146300_animation_10954_bank1.inc"
};

static AnimationPackedRotation _gActor146300Animation10954Bank4[135] = {
#include "assets/actor_146300_animation_10954_bank4.inc"
};

static AnimationRecord _gActor146300Animation10954Records[188] = {
#include "assets/actor_146300_animation_10954_records.inc"
};

static u16 _gActor146300Animation10954Indices[20] = {
#include "assets/actor_146300_animation_10954_indices.inc"
};

static AnimationSet _gActor146300Animation10954 = {
    _gActor146300Animation10954Records,
    _gActor146300Animation10954Indices,
    { NULL, _gActor146300Animation10954Bank1, NULL, NULL, _gActor146300Animation10954Bank4, NULL, NULL, NULL },
};

/// Latched duration of the next child-part blend, in whole normal-rate frames.
///
/// Play requests narrow `AnimationPlayRequest.blendFrames` to this signed
/// halfword. Plain resets leave it intact; this walker has no walk-completion
/// transition. Zero requests no transition time; 0..2047 keeps the playback
/// timer nonnegative. The range is not checked.
static s16 _gScriptedWalkBlendFrames = SCRIPTED_WALK_DEFAULT_BLEND_FRAMES;

/// Messages accepted by the live water-tank figure, terminated by the task-message sentinel.
///
/// Animation and placement borrow their payloads through synchronous dispatch;
/// visibility controls the body/helper pair and actor commands are acknowledged
/// without action. Unsupported IDs return zero. The table and callbacks must
/// remain loaded while the figure's task retains the table.
static TaskMessageEntry _gActor146300FigureMessages[] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actor146300PlayAnimation },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, actorMsgSetPairVisibility },
    { ACTOR_MESSAGE_PLACE, _scriptedWalkPlace },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor146300IgnoreCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

enum { ACTOR146300_FIGURE_TASK_PRIORITY = 192 };

TaskDesc gActor146300FigureTasks[ACTOR146300_TASK_COUNT] = {
    {
        .header   = { .fields = { .flags = TASK_BODY_TMD, .priority = ACTOR146300_FIGURE_TASK_PRIORITY } },
        .callback = _actor146300ScriptedFigureTask,
        .data     = { .model = &_gActor146300Model0895C },
    },
    {
        .header   = { .fields = { .flags = TASK_BODY_TMD, .priority = ACTOR146300_FIGURE_TASK_PRIORITY } },
        .callback = _actor146300AttachmentTask,
        .data     = { .model = &_gActor146300Actor113100Model07960 },
    },
};

/// Local animation bank of the twenty-part water-tank figure.
///
/// Entry 0 is NULL; clips 1..16 borrow this package's sixteen animation sets.
/// The animation context retains the table and reseeds child tracks through it,
/// so the table and sets must remain loaded for the figure's lifetime. Requests
/// must select 1..16; the message handler's signed upper-bound check also admits
/// zero and negative values, which do not name playable entries.
static AnimationSet* _gActor146300FigureAnimationSets[] = {
    NULL,
    &_gActor146300Animation0D4CC,
    &_gActor146300Animation0DB80,
    &_gActor146300Animation0E208,
    &_gActor146300Animation0E5C8,
    &_gActor146300Animation0E7D4,
    &_gActor146300Animation0EA84,
    &_gActor146300Animation0ECC4,
    &_gActor146300Animation0F18C,
    &_gActor146300Animation0F414,
    &_gActor146300Animation0F888,
    &_gActor146300Animation0FA24,
    &_gActor146300Animation0FCC4,
    &_gActor146300Animation0FED0,
    &_gActor146300Animation1014C,
    &_gActor146300Animation103E0,
    &_gActor146300Animation10954,
};

/// Selected CAP slot for the first two ice-bag handovers and their follow-up choices.
///
/// Written as slot 19 or 20 before either scene starts and reused for variants
/// 0 (bag handover) and 1 (choice). Zero is the initial, unselected value; the
/// handover task sets the latch before reading it; run only one at a time. Keep word-sized
/// storage: the CAP start calls narrow the selected value to their signed halfword.
static s32 _gActor146300HandoverCapSlot = 0;

static void _actor146300SpawnScriptedWalker(Enemy* enemy, Task* task);

/// Runs one ice-bag handover or follow-up conversation, then releases scripted control.
///
/// The room holds player control before spawning this bodyless task in state 0.
/// Consumes the live-save collection bit only while advancing handover progress;
/// expired bags are melted before the first check. The loaded CAP slots and event
/// scripts must remain available until the scene ends. Event completion is the
/// session's idle event state, not CAP selection alone. A final repeat conversation
/// delegates control release to its script and kills this task immediately.
static void _actor146300IceBagHandoverTask(Task* task)
{
    enum {
        ACTOR146300_HANDOVER_CHECK                 = 0,
        ACTOR146300_HANDOVER_RELEASE_CONTROL       = 1,
        ACTOR146300_HANDOVER_START_BAG_SCENE       = 10,
        ACTOR146300_HANDOVER_WAIT_BAG_SCENE        = 11,
        ACTOR146300_HANDOVER_START_CHOICE_SCENE    = 20,
        ACTOR146300_HANDOVER_WAIT_CHOICE_SCENE     = 21,
        ACTOR146300_HANDOVER_START_FINAL_BAG_SCENE = 30,
        ACTOR146300_HANDOVER_WAIT_FINAL_BAG_SCENE  = 31,
        ACTOR146300_HANDOVER_START_FINAL_TALK      = 40,
        ACTOR146300_HANDOVER_WAIT_FINAL_TALK       = 41,
        ACTOR146300_CAP_HANDOVER_VARIANT           = 0,
        ACTOR146300_CAP_CHOICE_VARIANT             = 1,
    };

    /// Consumes a bag, selects its CAP slot, advances progress and queues the scene.
    ///
    /// Each argument is evaluated once: captionIndex, nextProgress, then handoverTask,
    /// after clearing the collection bit. Requires a live task, loaded CAP slot and
    /// next handover progress. Captures the overlay's caption latch and this function's
    /// start-scene state constant. Ends a block; use as a statement inside braces.
#define ACTOR_146300_ACCEPT_ICE_BAG(handoverTask, captionIndex, nextProgress)    \
    {                                                                            \
        inventoryClearCollectedBit(INVENTORY_COLLECTION_ID_ICE_BAG);             \
        _gActor146300HandoverCapSlot = (captionIndex);                           \
        gameFlagSetNibble(GAME_FLAG_ITEM_119_HANDOVER_PROGRESS, (nextProgress)); \
        (handoverTask)->state = ACTOR146300_HANDOVER_START_BAG_SCENE;            \
    }

    switch (task->state) {
        case ACTOR146300_HANDOVER_CHECK:
            // Expiration must be applied before testing whether a bag can be handed over.
            inventoryMeltIceBagIfExpired();
            switch (gameFlagGetNibble(GAME_FLAG_ITEM_119_HANDOVER_PROGRESS)) {
                case ACTOR146300_HANDOVER_AWAIT_FIRST_BAG:
                    if (inventoryHasCollectedBit(INVENTORY_COLLECTION_ID_ICE_BAG) == 0) {
                        capRunCommandWithTransition(ACTOR146300_CAP_NO_ICE_BAG);
                        task->state++;
                    } else {
                        ACTOR_146300_ACCEPT_ICE_BAG(task, ACTOR146300_CAP_FIRST_BAG, ACTOR146300_HANDOVER_AWAIT_SECOND_BAG);
                    }
                    break;
                case ACTOR146300_HANDOVER_AWAIT_SECOND_BAG:
                    if (inventoryHasCollectedBit(INVENTORY_COLLECTION_ID_ICE_BAG) == 0) {
                        if (areaGetCurrentObjectState(ACTOR146300_FIRST_BAG_TALK_OBJECT) == ACTOR146300_HANDOVER_OBJECT_TALK_READY) {
                            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_HIDE_HUD, 0, 0);
                            _gActor146300HandoverCapSlot = ACTOR146300_CAP_FIRST_BAG;
                            task->state                  = ACTOR146300_HANDOVER_START_CHOICE_SCENE;
                        } else {
                            capRunCommandWithTransition(ACTOR146300_CAP_NO_ICE_BAG);
                            task->state++;
                        }
                    } else {
                        ACTOR_146300_ACCEPT_ICE_BAG(task, ACTOR146300_CAP_SECOND_BAG, ACTOR146300_HANDOVER_AWAIT_THIRD_BAG);
                    }
                    break;
                case ACTOR146300_HANDOVER_AWAIT_THIRD_BAG:
                    if (inventoryHasCollectedBit(INVENTORY_COLLECTION_ID_ICE_BAG) == 0) {
                        if (areaGetCurrentObjectState(ACTOR146300_SECOND_BAG_TALK_OBJECT) == ACTOR146300_HANDOVER_OBJECT_TALK_READY) {
                            taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_HIDE_HUD, 0, 0);
                            _gActor146300HandoverCapSlot = ACTOR146300_CAP_SECOND_BAG;
                            task->state                  = ACTOR146300_HANDOVER_START_CHOICE_SCENE;
                        } else {
                            capRunCommandWithTransition(ACTOR146300_CAP_NO_ICE_BAG);
                            task->state++;
                        }
                    } else {
                        inventoryClearCollectedBit(INVENTORY_COLLECTION_ID_ICE_BAG);
                        gameFlagSetNibble(GAME_FLAG_ITEM_119_HANDOVER_PROGRESS, ACTOR146300_HANDOVER_COMPLETE);
                        task->state = ACTOR146300_HANDOVER_START_FINAL_BAG_SCENE;
                    }
                    break;
                case ACTOR146300_HANDOVER_COMPLETE:
                    if (areaGetCurrentObjectState(ACTOR146300_FINAL_TALK_OBJECT) == ACTOR146300_HANDOVER_OBJECT_TALK_READY) {
                        taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_CAP_CONTROL), CAP_CONTROL_MESSAGE_HIDE_HUD, 0, 0);
                        task->state = ACTOR146300_HANDOVER_START_FINAL_TALK;
                    } else {
                        evsStartScript(_gActor146300HandoverRepeatTalkScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
                        taskKill(task);
                    }
                    break;
                default:
                    task->state++;
                    break;
            }
            break;
        case ACTOR146300_HANDOVER_RELEASE_CONTROL:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            taskKill(task);
            break;
        case ACTOR146300_HANDOVER_START_BAG_SCENE:
            // The same CAP slot serves the handover and its later choice conversation.
            capStartSequenceSlot((s16)_gActor146300HandoverCapSlot, CAP_PLAYBACK_IN_PLACE, ACTOR146300_CAP_HANDOVER_VARIANT);
            evsStartScript(_gActor146300IceBagHandoverScript, EVENT_SCRIPT_HUD_KEEP);
            task->state++;
            break;
        case ACTOR146300_HANDOVER_WAIT_BAG_SCENE:
            if (gGameSession->eventState == 0) {
                task->state = ACTOR146300_HANDOVER_START_CHOICE_SCENE;
            }
            break;
        case ACTOR146300_HANDOVER_START_CHOICE_SCENE:
            capStartSequenceSlot((s16)_gActor146300HandoverCapSlot, CAP_PLAYBACK_IN_PLACE, ACTOR146300_CAP_CHOICE_VARIANT);
            evsStartScript(_gActor146300HandoverChoiceScript, EVENT_SCRIPT_HUD_KEEP);
            task->state++;
            break;
        case ACTOR146300_HANDOVER_START_FINAL_BAG_SCENE:
            evsStartScript(_gActor146300FinalIceBagHandoverScript, EVENT_SCRIPT_HUD_KEEP);
            task->state++;
            break;
        case ACTOR146300_HANDOVER_WAIT_FINAL_BAG_SCENE:
            if (gGameSession->eventState == 0) {
                task->state = ACTOR146300_HANDOVER_START_FINAL_TALK;
            }
            break;
        case ACTOR146300_HANDOVER_START_FINAL_TALK:
            capStartSequenceSlot(ACTOR146300_CAP_FINAL_TALK, CAP_PLAYBACK_IN_PLACE, ACTOR146300_CAP_CHOICE_VARIANT);
            evsStartScript(_gActor146300FinalHandoverTalkScript, EVENT_SCRIPT_HUD_KEEP);
            task->state++;
            break;
        case ACTOR146300_HANDOVER_WAIT_CHOICE_SCENE:
        case ACTOR146300_HANDOVER_WAIT_FINAL_TALK:
            if (gGameSession->eventState == 0) {
                task->state = ACTOR146300_HANDOVER_RELEASE_CONTROL;
            }
            break;
    }
#undef ACTOR_146300_ACCEPT_ICE_BAG
}

void actor146300RestoreHandoverPose(void)
{
    switch (gameFlagGetNibble(GAME_FLAG_ITEM_119_HANDOVER_PROGRESS)) {
        case ACTOR146300_HANDOVER_AWAIT_FIRST_BAG:
            TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(0), ACTOR_MESSAGE_PLAY_ANIMATION, &_gActor146300FigureClip12Request, 0);
            break;
        case ACTOR146300_HANDOVER_AWAIT_SECOND_BAG:
            if (inventoryHasCollectedBit(INVENTORY_COLLECTION_ID_ICE_BAG) == 0) {
                if (areaGetCurrentObjectState(ACTOR146300_FIRST_BAG_TALK_OBJECT) == ACTOR146300_HANDOVER_OBJECT_TALK_READY) {
                    TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(0), ACTOR_MESSAGE_PLAY_ANIMATION, &_gActor146300FigureClip5Request, 0);
                } else {
                    TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(0), ACTOR_MESSAGE_PLAY_ANIMATION, &_gActor146300FigureClip12Request, 0);
                }
            } else {
                TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(0), ACTOR_MESSAGE_PLAY_ANIMATION, &_gActor146300FigureClip12Request, 0);
            }
            break;
        case ACTOR146300_HANDOVER_AWAIT_THIRD_BAG:
            if (inventoryHasCollectedBit(INVENTORY_COLLECTION_ID_ICE_BAG) == 0) {
                if (areaGetCurrentObjectState(ACTOR146300_SECOND_BAG_TALK_OBJECT) == ACTOR146300_HANDOVER_OBJECT_TALK_READY) {
                    TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(0), ACTOR_MESSAGE_PLAY_ANIMATION, &_gActor146300FigureClip5Request, 0);
                } else {
                    TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(0), ACTOR_MESSAGE_PLAY_ANIMATION, &_gActor146300FigureClip12Request, 0);
                }
                break;
            }
            // A third bag already present restores the final pose before its handover.
            /* fallthrough */
        case ACTOR146300_HANDOVER_COMPLETE:
            TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(0), ACTOR_MESSAGE_PLACE, &_gActor146300FigureFinalHandoverTransform, 0);
            TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(0), ACTOR_MESSAGE_PLAY_ANIMATION, &_gActor146300FigureClip14Request, 0);
            break;
    }
}

/// Cancels room effects and locks attachment actions for the water-tank event.
///
/// The opening script calls this before taking control of player presentation.
static void _actor146300BeginWaterTankEvent(void)
{
    roomEffectRequestCancelAll();
    gAttachmentState.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
}

/// Applies the retained CAP choice at either cue of the handover conversation.
///
/// Phase 0 selects clip 10 for key 1; phase 1 selects clip 5 for key 2.
/// Other phases and keys leave the actor alone. Requires the package's live
/// initialized placement-0 actor and its loaded request records.
static void _actor146300ApplyHandoverChoice(s32 phase)
{
    enum {
        ACTOR146300_CHOICE_FIRST_CUE  = 0,
        ACTOR146300_CHOICE_SECOND_CUE = 1,
        ACTOR146300_CHOICE_FIRST_KEY  = 1,
        ACTOR146300_CHOICE_SECOND_KEY = 2,
    };

    switch (phase) {
        case ACTOR146300_CHOICE_FIRST_CUE:
            if (capGetVariantKey() == ACTOR146300_CHOICE_FIRST_KEY) {
                TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(0), ACTOR_MESSAGE_PLAY_ANIMATION, &_gActor146300FigureClip10Request, 0);
            }
            break;
        case ACTOR146300_CHOICE_SECOND_CUE:
            if (capGetVariantKey() == ACTOR146300_CHOICE_SECOND_KEY) {
                TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(0), ACTOR_MESSAGE_PLAY_ANIMATION, &_gActor146300FigureClip5Request, 0);
            }
            break;
    }
}

/// Samples three model light rows at cached root XYZ with Y decreased by 800.
///
/// Requires a live model with writable light/color matrices and stable
/// MATRIX-bearing rootCoord; workm is read without composition. lightPosition
/// is a writable VECTOR lvalue. Root/sample occur repeatedly; model occurs
/// once. Captures the local Y-offset and light-count constants. The query
/// consumes XYZ synchronously; invoke within a braced statement body.
#define ACTOR_146300_RELIGHT_SPAWN(model, rootCoord, lightPosition)                         \
    {                                                                                       \
        (lightPosition).vx = (rootCoord)->workm.t[0];                                       \
        (lightPosition).vy = (rootCoord)->workm.t[1] - ACTOR_146300_LIGHT_SAMPLE_Y_OFFSET;  \
        (lightPosition).vz = (rootCoord)->workm.t[2];                                       \
        worldCoordSetModelLighting((model), &(lightPosition), 0, ACTOR_146300_LIGHT_COUNT); \
    }

/// Initializes the ice-bag handover scripted walker and creates its attachment task.
///
/// Allocates zeroed task-owned work, destroying the enemy on failure. Requires
/// a live model/Enemy and descriptor slot 1 to produce a live attachment. Parents
/// the untargetable root to the view, textures the attachment from the owner’s
/// area placement and adopts it into task teardown. The model borrows work-owned
/// lighting matrices and samples cached XYZ with Y minus 800, before composition.
/// Binds the loaded clip table, requests clip 11, installs messages/exit, updates
/// once and advances spawn state 0 to running state 1. Resources must outlive work.
static void _actor146300SpawnScriptedWalker(Enemy* enemy, Task* task)
{
    enum {
        ACTOR_146300_SPAWN_CLIP            = 11,
        ACTOR_146300_LIGHT_SAMPLE_Y_OFFSET = 800,
        ACTOR_146300_LIGHT_COUNT           = 3,
    };

    VECTOR            lightPosition;
    _Actor146300Work* work;
    TmdObject*        model;
    GfxCoord*         rootCoord;
    Task*             attachmentTask;

    model              = task->extra.tmd;
    rootCoord          = model->coords;
    work               = memCalloc(sizeof(_Actor146300Work), false);
    _gScriptedWalkWork = work;
    task->work         = work;
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->exitCallback               = _actor146300Destroy;
    rootCoord->parent                = &gGfxViewCoord;
    enemy->field_4                   = &rootCoord->coord;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = 0;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    model->otOffset                  = 1;
    model->flags                     = 0;
    _gActor146300BodyTask            = task;
    attachmentTask                   = taskSpawnFromTable(gActor146300FigureTasks, ACTOR146300_TASK_ATTACHMENT, 0, 0);
    _gActorMsgHelperTask             = attachmentTask;
    _actorRenderApplyTaskPlacementTextureOffsets(attachmentTask, enemy);
    taskReparent(task, _gActorMsgHelperTask);
    model->lightMtx = &_gScriptedWalkWork->light;
    model->colorMtx = &_gScriptedWalkWork->color;
    ACTOR_146300_RELIGHT_SPAWN(model, rootCoord, lightPosition);
    animationInitContext(&_gScriptedWalkWork->rig.anim, _gActor146300FigureAnimationSets, model,
                         _gScriptedWalkWork->rig.poses, _gScriptedWalkWork->rig.slots);
    _gScriptedWalkWork->st.animId = ACTOR_146300_SPAWN_CLIP;
    _gScriptedWalkWork->st.state  = ACTOR_ENEMY_ANIM_RESET;
    task->msgTable                = _gActor146300FigureMessages;
    _actor146300UpdateAnimation(task);
    task->state++;
}
#undef ACTOR_146300_RELIGHT_SPAWN

/// Dispatches the scripted figure's initialization or model update.
///
/// Descriptor 0 requires a live Enemy in `spawnArg2.pointer`, a twenty-part
/// TMD body and state 0..1. Publishes the task-owned work for the singleton
/// animation/placement handlers before dispatch; state 0 allocates it, while
/// state 1 composes, relights and animates the stationary figure. Initialization
/// may destroy the enemy and task on allocation failure. There is no bounds check.
static void _actor146300ScriptedFigureTask(Task* task)
{
    EnemyTaskFunc stateHandlers[] = {
        _actor146300SpawnScriptedWalker,
        _actor146300UpdateModel,
    };

    _gScriptedWalkWork = task->work;
    stateHandlers[task->state](task->spawnArg2.pointer, task);
}

/// Relights an actor or its attachment from an offset of the actor's cached origin.
///
/// Samples 800 game-coordinate units along view-space negative Y, requesting
/// all three model-light rows. `actorRoot->workm` must already include the
/// current view transform; this helper neither composes nor changes the root.
/// `model` may be the attachment rather than the actor supplying the root.
/// Both inputs and the model's borrowed writable light/colour matrices must
/// remain live. Scratch and GTE requirements follow `worldCoordSetModelLighting`;
/// the sample's three signed words are consumed synchronously, retaining no pointer.
static inline void _actor146300RelightFromRoot(const TmdObject* model, const GfxCoord* actorRoot)
{
    enum { ACTOR146300_LIGHT_SAMPLE_HEIGHT = 800 };
    VECTOR3 lightPosition;

    lightPosition.vx = actorRoot->workm.t[0];
    lightPosition.vy = actorRoot->workm.t[1] - ACTOR146300_LIGHT_SAMPLE_HEIGHT;
    lightPosition.vz = actorRoot->workm.t[2];
    worldCoordSetModelLighting(model, &lightPosition, 0, ARRAY_SIZE(model->lightMtx->m));
}

/// Composes and relights the actor model before updating its child-part animation.
///
/// Running state 1 requires a live TMD task, a root parented to the view and the
/// receiver's initialized work published in `_gScriptedWalkWork`. Samples all
/// three light rows at the root's view-space negative-Y offset of 800 units.
/// `unusedEnemy` is ignored.
static void _actor146300UpdateModel(Enemy* unusedEnemy, Task* task)
{
    TmdObject* model;
    GfxCoord*  rootCoord;

    model     = task->extra.tmd;
    rootCoord = model->coords;
    actorRenderComposeCoord(rootCoord);
    _actor146300RelightFromRoot(model, rootCoord);
    _actor146300UpdateAnimation(task);
}

/// Destroys this actor's enemy and begins task teardown when its task exits.
///
/// Requires the live enemy borrowed in `spawnArg2.pointer` and its owning task;
/// destruction follows `enemyDestroy`. Task teardown owns the model and work.
static void _actor146300Destroy(Task* task)
{
    enemyDestroy(task->spawnArg2.pointer, task);
}

/// Reseeds or advances the published actor's child-part animation without moving its root.
///
/// Requires live initialized `_gScriptedWalkWork` and loaded clip data for its
/// twenty-part rig. Blend/reset seed slots 1..19 and enter tick state without
/// ticking again in that call; tick advances those slots. Blend uses the latched
/// whole-frame duration. State 0 and other states do nothing. `unusedTask` is ignored.
static void _actor146300UpdateAnimation(Task* unusedTask)
{
    if (_gScriptedWalkWork->st.state == ACTOR_ENEMY_ANIM_BLEND) {
        _scriptedWalkBlendAnim();
        _gScriptedWalkWork->st.state = ACTOR_ENEMY_ANIM_TICK;
        return;
    }
    if (_gScriptedWalkWork->st.state == ACTOR_ENEMY_ANIM_RESET) {
        _scriptedWalkResetAnim();
        _gScriptedWalkWork->st.state = ACTOR_ENEMY_ANIM_TICK;
        return;
    }
    if (_gScriptedWalkWork->st.state == ACTOR_ENEMY_ANIM_TICK) {
        _scriptedWalkTickAnim();
    }
}

#include "../../shared/walk/scripted_walk_tick_anim.inc.c"

#include "../../shared/walk/scripted_walk_reset_anim.inc.c"

#include "../../shared/walk/scripted_walk_blend_anim.inc.c"

/// Applies an animation request immediately to the published stationary actor.
///
/// Handles `ACTOR_MESSAGE_PLAY_ANIMATION`; borrows a readable word-aligned request
/// through dispatch, retaining no request pointer. Requires the live initialized
/// singleton work and its loaded rig/model/table. Playable IDs are 1..16: entry 0
/// is NULL. The signed check rejects only IDs >=17; zero and negative IDs pass,
/// so callers must enforce the playable range before the signed-halfword narrowing.
/// Every nonzero blend value selects a blend and narrows whole normal-rate frames
/// to s16 (0..2047 keeps its playback timer nonnegative); reset leaves that latch
/// intact. Repeated clips restart. Source index and collision mode are ignored,
/// as are the other callback arguments. Returns 0 after reseeding, -1 on rejection.
static s32 _actor146300PlayAnimation(Task* unusedTask, s32 messageId, const AnimationPlayRequest* request, s32 unusedArgument)
{
    enum {
        ACTOR146300_ANIMATION_REJECTED = -1,
    };

    if (request->animationId < (s32)ARRAY_SIZE(_gActor146300FigureAnimationSets)) {
        _gScriptedWalkWork->st.animId = request->animationId;
        if (request->blend != ANIMATION_BLEND_RESET) {
            _gScriptedWalkWork->st.state = ACTOR_ENEMY_ANIM_BLEND;
            _gScriptedWalkBlendFrames    = request->blendFrames;
        } else {
            _gScriptedWalkWork->st.state = ACTOR_ENEMY_ANIM_RESET;
        }
        _gScriptedWalkWork->st.field_6 = 0;
        _actor146300UpdateAnimation(_gActor146300BodyTask);
        return 0;
    }
    return ACTOR146300_ANIMATION_REJECTED;
}

/// Selects the published water-tank figure's body for paired visibility.
///
/// Expands to a `Task*` identifier with no evaluation side effects; this
/// fragment only reads it, independently of the message receiver. Requires
/// the current live TMD body and its matching helper in `_gActorMsgHelperTask`.
/// Successful spawn replaces the borrowed pointer; teardown does not clear it.
/// Bind only around the pair-visibility fragment and never dispatch after teardown.
#define ACTOR_MESSAGE_BODY_TASK _gActor146300BodyTask
#include "../../shared/actor/actor_messages_pair_visibility.inc.c"
#undef ACTOR_MESSAGE_BODY_TASK

#include "../../shared/walk/scripted_walk_place.inc.c"

/// Acknowledges `ACTOR_COMMAND_MESSAGE_APPLY` without changing this actor.
///
/// Returns 0 and ignores every argument, including the borrowed command pointer;
/// the pointer need not address readable storage because it is never dereferenced.
static s32 _actor146300IgnoreCommand(Task* unusedTask, s32 messageId, const ActorCommand* unusedCommand, s32 unusedArgument)
{
    return 0;
}

/// Attaches the companion model to actor part 4, then relights it from the actor root.
///
/// Requires this live TMD task and `_gActor146300BodyTask` with its twenty coordinates;
/// both models remain live while attached. State 0 replaces the companion root's
/// parent, marks composition dirty, enables drawing and enters state 1. Later
/// ticks sample all three light rows at the actor root's view-space negative-Y
/// offset of 800 units. The actor's frame state must compose that root; this task
/// does not compose it.
static void _actor146300AttachmentTask(Task* task)
{
    enum {
        ACTOR146300_ATTACHMENT_ATTACH  = 0,
        ACTOR146300_ATTACHMENT_RELIGHT = 1,
        ACTOR146300_ATTACHMENT_PART    = 4,
    };
    TmdObject* attachmentModel = task->extra.tmd;
    GfxCoord*  attachmentRoot  = attachmentModel->coords;
    GfxCoord*  actorRoot       = _gActor146300BodyTask->extra.tmd->coords;
    GfxCoord*  attachmentPart  = actorRoot + ACTOR146300_ATTACHMENT_PART;

    switch (task->state) {
        case ACTOR146300_ATTACHMENT_ATTACH:
            attachmentRoot->composeStamp = GRAPHICS_COORD_DIRTY;
            attachmentModel->flags       = 0;
            attachmentRoot->parent       = attachmentPart;
            task->state++;
            break;
        case ACTOR146300_ATTACHMENT_RELIGHT:
            _actor146300RelightFromRoot(attachmentModel, actorRoot);
            break;
    }
}
