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
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "../../shared/scripted_walk.h"
#include "../../shared/actor_messages.h"

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

/// The actor's own task, published by the spawn routine: the 0x7D3 handler
/// runs the per-frame update on it, and the 0x7D5 handler and the companion's
/// handler `_actor146300AttachmentTask` reach the actor's model through its
/// `extra`.
extern Task* gActorSelfTask;

/// The companion task the spawn routine starts from
/// `D_actor_146300_801427C8`; its `extra` is the model whose texture page and
/// CLUT row come out of the area record, and the actor's own task is reparented
/// under it.
extern Task* gActorHelperTask;

/// Spawn table of the actor's two tasks: index 0 runs
/// `func_actor_146300_801326CC`, index 1 the companion's
/// `_actor146300AttachmentTask`, which the spawn routine starts.
extern TaskDesc D_actor_146300_801427C8[];

/// Animation stream the spawn routine binds into the work block's animation
/// context with `animationInitContext`.
extern u8 D_actor_146300_801427E0[];

/// Message handler table the spawn routine publishes as `Task::msgTable`.
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.

extern TaskMessageEntry D_actor_146300_801427A0[];

extern AnimationPlayRequest D_actor_146300_80137AAC;
extern AnimationPlayRequest D_actor_146300_80137B10;
extern AnimationPlayRequest D_actor_146300_80137B38;
extern AnimationPlayRequest D_actor_146300_80137B60;
extern ActorTransform       D_actor_146300_80137C10;
extern EvsCommand           D_actor_146300_801386C0[];
extern EvsCommand           D_actor_146300_80138810[];
extern EvsCommand           D_actor_146300_801388D0[];
extern EvsCommand           D_actor_146300_80138A38[];
extern EvsCommand           D_actor_146300_80138AC8[];
extern s32                  D_actor_146300_80142824;

static void _actor146300UpdateModel(Enemy* unusedEnemy, Task* task);
static void _actor146300Destroy(Task* task);
static void _actor146300UpdateAnimation(Task* unusedTask);

static TmdSource _gActor146300Model0895C;
static TmdSource _gActor146300Actor113100Model07960;
void             func_actor_146300_801326CC(Task*);
static void      _actor146300AttachmentTask(Task* task);

static s32 _actor146300PlayAnimation(Task* unusedTask, s32 messageId, const AnimationPlayRequest* request, s32 unusedArgument);
static s32 _actor146300IgnoreCommand(Task* unusedTask, s32 messageId, const ActorCommand* unusedCommand, s32 unusedArgument);

extern AnimationPlayRequest     D_actor_146300_80137A20;
extern AnimationPlayRequest     D_actor_146300_80137A34;
extern AnimationPlayRequest     D_actor_146300_80137A98;
extern AnimationPlayRequest     D_actor_146300_80137B74;
extern AnimationPlayRequest     D_actor_146300_80137B88;
extern AnimationPlayRequest     D_actor_146300_80137BC4;
extern AnimationBankCopyRequest D_actor_146300_80137BD8;
static void                     _actor146300ApplyHandoverChoice(s32 phase);

extern AnimationPlayRequest D_actor_146300_8013791C;
extern AnimationPlayRequest D_actor_146300_80137930;
extern AnimationPlayRequest D_actor_146300_80137944;
extern AnimationPlayRequest D_actor_146300_80137958;
extern AnimationPlayRequest D_actor_146300_8013796C;
extern AnimationPlayRequest D_actor_146300_80137980;
extern AnimationPlayRequest D_actor_146300_80137994;
extern AnimationPlayRequest D_actor_146300_801379A8;
extern AnimationPlayRequest D_actor_146300_801379BC;
extern AnimationPlayRequest D_actor_146300_801379D0;
extern AnimationPlayRequest D_actor_146300_801379E4;
extern AnimationPlayRequest D_actor_146300_801379F8;
extern AnimationPlayRequest D_actor_146300_80137A0C;
extern AnimationPlayRequest D_actor_146300_80137A5C;
extern AnimationPlayRequest D_actor_146300_80137A70;
extern AnimationPlayRequest D_actor_146300_80137A84;
extern AnimationPlayRequest D_actor_146300_80137AC0;
extern AnimationPlayRequest D_actor_146300_80137AD4;
extern AnimationPlayRequest D_actor_146300_80137AE8;
extern AnimationPlayRequest D_actor_146300_80137AFC;
extern AnimationPlayRequest D_actor_146300_80137B24;
extern AnimationPlayRequest D_actor_146300_80137B4C;
extern AnimationPlayRequest D_actor_146300_80137B9C;
extern AnimationPlayRequest D_actor_146300_80137BB0;
extern ActorTransform       D_actor_146300_80137BE0;
extern ActorTransform       D_actor_146300_80137BF8;
static void                 _actor146300BeginWaterTankEvent(void);

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

TaskDesc D_actor_146300_8013788C = { { { TASK_BODY_NONE, 192 } }, _actor146300IceBagHandoverTask, { .value = 0 } };

/// Player clips for extended ids 47-64.
///
/// The package's event scripts send the player its copy request before they
/// play one of these clips. `D_actor_146300_80137BD8` copies
/// `ANIMATION_BANK_EXTENSION_CAPACITY` (32) words starting here into the
/// player's bank, which is 14 words past the end of this array: the read runs
/// on through `D_actor_146300_801378E0`, `D_actor_146300_801378F4` and the
/// first four words of `D_actor_146300_80137908`. That overrun is the
/// original's and is kept as it is: the request carries the bank's fixed
/// capacity, while the table was stored with only its own entries. No request
/// selects an id past 64, so none of the words installed after the eighteen
/// clips is played as one.
AnimationSet* D_actor_146300_80137898[18] = { &_gActor146300Animation01330, &_gActor146300Animation016C8, &_gActor146300Animation01E30, &_gActor146300Animation021A8, &_gActor146300Animation028E4, &_gActor146300Animation02B6C, &_gActor146300Animation0300C, &_gActor146300Animation0352C, &_gActor146300Animation03804, &_gActor146300Animation03ACC, &_gActor146300Animation03FEC, &_gActor146300Animation04394, &_gActor146300Animation04758, &_gActor146300Animation049F0, &_gActor146300Animation04C7C, &_gActor146300Animation05354, &_gActor146300Animation0568C, &_gActor146300Animation05A44 };

// Requests for the player's extended ids start here. This one is not referenced.
AnimationPlayRequest D_actor_146300_801378E0 = { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_801378F4 = { { .index = 1 }, 47, ANIMATION_BLEND_INTERPOLATE, 5, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_80137908 = { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_8013791C = { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_80137930 = { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_80137944 = { { .index = 1 }, 51, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_80137958 = { { .index = 1 }, 52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_8013796C = { { .index = 1 }, 53, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_80137980 = { { .index = 1 }, 54, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_80137994 = { { .index = 1 }, 55, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_801379A8 = { { .index = 1 }, 56, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_801379BC = { { .index = 1 }, 57, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_801379D0 = { { .index = 1 }, 58, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_801379E4 = { { .index = 1 }, 59, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_801379F8 = { { .index = 1 }, 60, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_80137A0C = { { .index = 1 }, 61, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_80137A20 = { { .index = 1 }, 62, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_80137A34 = { { .index = 1 }, 63, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_80137A48 = { { .index = 1 }, 0, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_80137A5C = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_80137A70 = { { .index = 1 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_80137A84 = { { .index = 1 }, 3, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_80137A98 = { { .index = 1 }, 4, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_80137AAC = { { .index = 1 }, 5, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_80137AC0 = { { .index = 1 }, 6, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_80137AD4 = { { .index = 1 }, 7, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_80137AE8 = { { .index = 1 }, 8, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_80137AFC = { { .index = 1 }, 9, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_80137B10 = { { .index = 1 }, 10, ANIMATION_BLEND_INTERPOLATE, 30, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_80137B24 = { { .index = 1 }, 11, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_146300_80137B38 = { { .index = 1 }, 12, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_80137B4C = { { .index = 1 }, 13, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_80137B60 = { { .index = 1 }, 14, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_80137B74 = { { .index = 1 }, 15, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_80137B88 = { { .index = 1 }, 16, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_80137B9C = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_80137BB0 = { { .index = 1 }, 1, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_146300_80137BC4 = { { .index = 1 }, 64, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationBankCopyRequest D_actor_146300_80137BD8 = { { .sets = D_actor_146300_80137898 }, ANIMATION_BANK_EXTENSION_CAPACITY };

ActorTransform D_actor_146300_80137BE0 = { { 1110, -0x2EE0, -2500, 0 }, { 0, 1820, 0, 0 } };

ActorTransform D_actor_146300_80137BF8 = { { 1500, -0x2EE0, -1744, 0 }, { 0, 1820, 0, 0 } };

ActorTransform D_actor_146300_80137C10 = { { 1110, -0x2EE0, -2000, 0 }, { 0, 682, 0, 0 } };

EvsCommand D_actor_146300_80137C28[99] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor146300BeginWaterTankEvent }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 16 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_146300_80137BD8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146300_80137BB0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = 13 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_146300_80137B24 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_146300_80137BE0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146300_801378F4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x5315000A }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146300_801378F4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_146300_80137A5C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146300_80137908 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146300_8013791C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_146300_80137A70 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146300_80137930 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_146300_80137A84 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146300_80137944 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_146300_80137A98 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_146300_80137AAC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146300_80137958 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 14 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146300_80137980 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_146300_80137A98 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_146300_80137AAC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146300_801379A8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 26 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146300_801379BC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_146300_80137A98 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_146300_80137A98 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_146300_80137A98 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_146300_80137B4C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_146300_80137AAC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146300_801379A8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 26 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146300_8013796C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146300_801379D0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_146300_80137AC0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 26 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146300_801379E4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146300_80137994 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_146300_80137AD4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 45 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_146300_80137AE8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 28 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x5315000B }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146300_801379F8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146300_80137A0C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_146300_80137AFC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_146300_80137A98 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_146300_80137B10 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x5315000C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146300_801379D0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_146300_80137BF8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146300_80137B9C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_146300_80137AAC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_146300_80138570[14] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146300_80137B9C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_146300_80137B38 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_146300_80137BF8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 17 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_146300_801386C0[14] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4004 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_146300_80137BD8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146300_80137BC4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_146300_80137A98 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146300_80137A20 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146300_80137A34 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_146300_80137A98 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_146300_80138810[8] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_146300_80137A98 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor146300ApplyHandoverChoice }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor146300ApplyHandoverChoice }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4005 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_146300_801388D0[15] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 21 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4004 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_146300_80137BD8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_146300_80137B88 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146300_80137BC4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_146300_80137B74 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146300_80137BC4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_146300_80137B88 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_146300_80138A38[6] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_146300_80137B88 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_146300_80137B60 } }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_146300_80138AC8[7] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 22 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_146300_80137B88 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_146300_80137B60 } }, { .value = 0 } },
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

TaskMessageEntry D_actor_146300_801427A0[5] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actor146300PlayAnimation },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, actorMsgSetPairVisibility },
    { ACTOR_MESSAGE_PLACE, _scriptedWalkPlace },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor146300IgnoreCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_actor_146300_801427C8[2] = {
    { { { TASK_BODY_TMD, 192 } }, func_actor_146300_801326CC, { .model = &_gActor146300Model0895C } },
    { { { TASK_BODY_TMD, 192 } }, _actor146300AttachmentTask, { .model = &_gActor146300Actor113100Model07960 } },
};

u8 D_actor_146300_801427E0[68] = {
    0,
    0,
    0,
    0,
    236,
    242,
    19,
    128,
    160,
    249,
    19,
    128,
    40,
    0,
    20,
    128,
    232,
    3,
    20,
    128,
    244,
    5,
    20,
    128,
    164,
    8,
    20,
    128,
    228,
    10,
    20,
    128,
    172,
    15,
    20,
    128,
    52,
    18,
    20,
    128,
    168,
    22,
    20,
    128,
    68,
    24,
    20,
    128,
    228,
    26,
    20,
    128,
    240,
    28,
    20,
    128,
    108,
    31,
    20,
    128,
    0,
    34,
    20,
    128,
    116,
    39,
    20,
    128,
};

s32 D_actor_146300_80142824 = 0;

Task* gActorSelfTask;

Task* gActorHelperTask;

static void func_actor_146300_801324AC(Enemy* enemy, Task* task);

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
        D_actor_146300_80142824 = (captionIndex);                                \
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
                            D_actor_146300_80142824 = ACTOR146300_CAP_FIRST_BAG;
                            task->state             = ACTOR146300_HANDOVER_START_CHOICE_SCENE;
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
                            D_actor_146300_80142824 = ACTOR146300_CAP_SECOND_BAG;
                            task->state             = ACTOR146300_HANDOVER_START_CHOICE_SCENE;
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
                        evsStartScript(D_actor_146300_80138AC8, EVENT_SCRIPT_HUD_HIDE_RESTORE);
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
            capStartSequenceSlot((s16)D_actor_146300_80142824, CAP_PLAYBACK_IN_PLACE, ACTOR146300_CAP_HANDOVER_VARIANT);
            evsStartScript(D_actor_146300_801386C0, EVENT_SCRIPT_HUD_KEEP);
            task->state++;
            break;
        case ACTOR146300_HANDOVER_WAIT_BAG_SCENE:
            if (gGameSession->eventState == 0) {
                task->state = ACTOR146300_HANDOVER_START_CHOICE_SCENE;
            }
            break;
        case ACTOR146300_HANDOVER_START_CHOICE_SCENE:
            capStartSequenceSlot((s16)D_actor_146300_80142824, CAP_PLAYBACK_IN_PLACE, ACTOR146300_CAP_CHOICE_VARIANT);
            evsStartScript(D_actor_146300_80138810, EVENT_SCRIPT_HUD_KEEP);
            task->state++;
            break;
        case ACTOR146300_HANDOVER_START_FINAL_BAG_SCENE:
            evsStartScript(D_actor_146300_801388D0, EVENT_SCRIPT_HUD_KEEP);
            task->state++;
            break;
        case ACTOR146300_HANDOVER_WAIT_FINAL_BAG_SCENE:
            if (gGameSession->eventState == 0) {
                task->state = ACTOR146300_HANDOVER_START_FINAL_TALK;
            }
            break;
        case ACTOR146300_HANDOVER_START_FINAL_TALK:
            capStartSequenceSlot(ACTOR146300_CAP_FINAL_TALK, CAP_PLAYBACK_IN_PLACE, ACTOR146300_CAP_CHOICE_VARIANT);
            evsStartScript(D_actor_146300_80138A38, EVENT_SCRIPT_HUD_KEEP);
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
            TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(0), ACTOR_MESSAGE_PLAY_ANIMATION, &D_actor_146300_80137B38, 0);
            break;
        case ACTOR146300_HANDOVER_AWAIT_SECOND_BAG:
            if (inventoryHasCollectedBit(INVENTORY_COLLECTION_ID_ICE_BAG) == 0) {
                if (areaGetCurrentObjectState(ACTOR146300_FIRST_BAG_TALK_OBJECT) == ACTOR146300_HANDOVER_OBJECT_TALK_READY) {
                    TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(0), ACTOR_MESSAGE_PLAY_ANIMATION, &D_actor_146300_80137AAC, 0);
                } else {
                    TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(0), ACTOR_MESSAGE_PLAY_ANIMATION, &D_actor_146300_80137B38, 0);
                }
            } else {
                TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(0), ACTOR_MESSAGE_PLAY_ANIMATION, &D_actor_146300_80137B38, 0);
            }
            break;
        case ACTOR146300_HANDOVER_AWAIT_THIRD_BAG:
            if (inventoryHasCollectedBit(INVENTORY_COLLECTION_ID_ICE_BAG) == 0) {
                if (areaGetCurrentObjectState(ACTOR146300_SECOND_BAG_TALK_OBJECT) == ACTOR146300_HANDOVER_OBJECT_TALK_READY) {
                    TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(0), ACTOR_MESSAGE_PLAY_ANIMATION, &D_actor_146300_80137AAC, 0);
                } else {
                    TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(0), ACTOR_MESSAGE_PLAY_ANIMATION, &D_actor_146300_80137B38, 0);
                }
                break;
            }
            // A third bag already present restores the final pose before its handover.
            /* fallthrough */
        case ACTOR146300_HANDOVER_COMPLETE:
            TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(0), ACTOR_MESSAGE_PLACE, &D_actor_146300_80137C10, 0);
            TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(0), ACTOR_MESSAGE_PLAY_ANIMATION, &D_actor_146300_80137B60, 0);
            break;
    }
}

/// Cancels room effects and locks attachment actions for the water-tank event.
///
/// The opening script calls this before taking control of player presentation.
static void _actor146300BeginWaterTankEvent(void)
{
    roomEffectRequestCancelAll();
    Gp_StateC08.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
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
                TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(0), ACTOR_MESSAGE_PLAY_ANIMATION, &D_actor_146300_80137B10, 0);
            }
            break;
        case ACTOR146300_CHOICE_SECOND_CUE:
            if (capGetVariantKey() == ACTOR146300_CHOICE_SECOND_KEY) {
                TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(0), ACTOR_MESSAGE_PLAY_ANIMATION, &D_actor_146300_80137AAC, 0);
            }
            break;
    }
}

/// Spawn routine, state 0 of the task handler `func_actor_146300_801326CC`:
/// allocates the work block and publishes it in `_gScriptedWalkWork`
/// and the task's `work` slot (destroying the enemy if the allocation fails),
/// installs the exit callback, binds the model's coordinate frame to the view
/// and publishes the task in `gActorSelfTask`.
///
/// The companion task from `D_actor_146300_801427C8` carries the model whose
/// texture page and CLUT row come out of the current area record - the session
/// location key is copied onto the stack, `areaSyncLocationVariant` fills in its
/// nested index and the enemy's `placeKey >> ENEMY_PLACE_INDEX_SHIFT` selects the 0x10-byte record.
/// The actor's task is then reparented under that companion, the model gets the
/// block's light and colour matrices and is relit from a point 0x320 above its
/// root translation, the animation stream is bound, the animation state is
/// seeded with mode 2 / id 0xB, the message table is published and the
/// per-frame update runs once before the state advances.
static void func_actor_146300_801324AC(Enemy* enemy, Task* task)
{
    VECTOR            vec;
    _Actor146300Work* work;
    TmdObject*        obj;
    GfxCoord*         coord;
    Task*             helper;

    obj                = task->extra.tmd;
    coord              = obj->coords;
    work               = memCalloc(sizeof(_Actor146300Work), false);
    _gScriptedWalkWork = work;
    task->work         = work;
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->exitCallback               = _actor146300Destroy;
    coord->parent                    = &gGfxViewCoord;
    enemy->field_4                   = &coord->coord;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = 0;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    obj->otOffset                    = 1;
    obj->flags                       = 0;
    gActorSelfTask                   = task;
    helper                           = taskSpawnFromTable(D_actor_146300_801427C8, 1, 0, 0);
    gActorHelperTask                 = helper;
    actorTintTask(helper, enemy);
    taskReparent(task, gActorHelperTask);
    obj->lightMtx = &_gScriptedWalkWork->light;
    obj->colorMtx = &_gScriptedWalkWork->color;
    vec.vx        = coord->workm.t[0];
    vec.vy        = coord->workm.t[1] - 0x320;
    vec.vz        = coord->workm.t[2];
    worldCoordSetModelLighting(obj, &vec, 0, 3);
    animationInitContext(&_gScriptedWalkWork->rig.anim, (AnimationSet**)D_actor_146300_801427E0, obj,
                         _gScriptedWalkWork->rig.poses, _gScriptedWalkWork->rig.slots);
    _gScriptedWalkWork->st.animId = 0xB;
    _gScriptedWalkWork->st.state  = ACTOR_ENEMY_ANIM_RESET;
    task->msgTable                = D_actor_146300_801427A0;
    _actor146300UpdateAnimation(task);
    task->state++;
}

/// The actor's task handler: publishes the task's work block in
/// `_gScriptedWalkWork` on the way through, then runs the handler its
/// state selects from a table built on the stack - the spawn routine for state
/// 0, the per-frame update after it.
void func_actor_146300_801326CC(Task* task)
{
    void (*fns[2])(Enemy*, Task*) = {
        func_actor_146300_801324AC,
        _actor146300UpdateModel,
    };

    _gScriptedWalkWork = task->work;
    fns[task->state](task->spawnArg2.pointer, task);
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

#include "../../shared/scripted_walk_tick_anim.inc.c"

#include "../../shared/scripted_walk_reset_anim.inc.c"

#include "../../shared/scripted_walk_blend_anim.inc.c"

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
        ACTOR146300_ANIMATION_SET_COUNT = 17, // NULL at 0, loaded twenty-part clips at 1..16
        ACTOR146300_ANIMATION_REJECTED  = -1,
    };

    if (request->animationId < ACTOR146300_ANIMATION_SET_COUNT) {
        _gScriptedWalkWork->st.animId = request->animationId;
        if (request->blend != ANIMATION_BLEND_RESET) {
            _gScriptedWalkWork->st.state = ACTOR_ENEMY_ANIM_BLEND;
            _gScriptedWalkBlendFrames    = request->blendFrames;
        } else {
            _gScriptedWalkWork->st.state = ACTOR_ENEMY_ANIM_RESET;
        }
        _gScriptedWalkWork->st.field_6 = 0;
        _actor146300UpdateAnimation(gActorSelfTask);
        return 0;
    }
    return ACTOR146300_ANIMATION_REJECTED;
}

#include "../../shared/actor_messages_pair_visibility.inc.c"

#include "../../shared/scripted_walk_place.inc.c"

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
/// Requires this live TMD task and `gActorSelfTask` with its twenty coordinates;
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
    GfxCoord*  actorRoot       = gActorSelfTask->extra.tmd->coords;
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
