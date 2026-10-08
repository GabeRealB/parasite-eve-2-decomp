#include "actors/actor_450800.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/companion_load.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/captions.h"
#include "gameplay/gameflag.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd.h"
#include "main/tmd_types.h"

#include "rooms/shelter_b6_nursery.h"
// The paced walk helpers this package carries run on Kyle Madigan's block.
#define PACED_WALK_WORK_T _Actor450800KyleMadiganWork
// Exported instance: rooms spawn from this package's table by name.
#define gPairWalkTasks gActor450800PairWalkTasks
#include "../../shared/paced_walk.h"
#include "../../shared/pair_walk.h"

static s32  _pairWalkPlay(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg);
static s32  _pairWalkTo(Task* task, s32 messageId, const ActorTransform* target, s32 unusedArg);
static void _pairWalkSubModelTask(Task* task);

/// How Kyle Madigan covers a walk, kept in
/// `_Actor450800KyleMadiganWork::walkMode`.
///
/// The walk-to message's last argument selects it. Every mode ends at the
/// target: the distance is divided by the mode's stride to give the frames the
/// walk lasts. A value outside these three turns Kyle toward the target and
/// counts the slow stride, but no frame then moves him.
enum {
    ACTOR_450800_WALK_FAST     = 0, // Faces the target and advances 60 units a frame
    ACTOR_450800_WALK_BACKWARD = 1, // Faces away from the target and backs up to it, 15 units a frame
    ACTOR_450800_WALK_SLOW     = 2, // Faces the target and advances 25 units a frame
};

/// Kyle's parent-coordinate distance per travel frame in each walk mode.
enum {
    ACTOR_450800_KYLE_FAST_STRIDE     = 60,
    ACTOR_450800_KYLE_BACKWARD_STRIDE = 15,
    ACTOR_450800_KYLE_SLOW_STRIDE     = 25,
};

/// Work block of the package's Kyle Madigan actor, allocated zeroed at its
/// full size by the actor's spawn state and kept at `Task::work`.
///
/// Both bodies the package's task table runs through the actor's states use
/// it: the Kyle Madigan body and the second twenty-part body. The pawn golem
/// the package also carries keeps a `PairWalkWork` instead.
///
/// The block opens like the other scripted walkers' - matrices, twenty-slot
/// rig, animation state - so the paced walk library's slot tick, slot reset
/// and placement run on it unchanged. Behind that it holds what is Kyle's
/// own: the tasks of the two hands and the gun, drawn as separate models hung
/// off parts of the body, and the way he walks to a target. The model object
/// borrows `light` and `color`, and the three tasks are killed with the
/// actor, so all of it lives exactly as long as the actor's task.
typedef struct {
    MATRIX          light;         // Light-direction matrix lent to the body's model object
    MATRIX          color;         // Light-colour matrix lent to the body's model object
    ActorAnimRig20  rig;           // Playback storage of the body; slots 1 to 19 are driven
    ActorEnemyState st;            // Animation request, heading and frames of walk left
    s16             turnFrames;    // Frames left of a turn of 51/4096 a frame while clip 3 plays; only ever cleared here, so the turn never runs
    byte            pad_4EE[0x2];
    Task*           handLeftTask;  // Task drawing the left-hand model
    Task*           handRightTask; // Task drawing the right-hand model
    Task*           gunTask;       // Task drawing the gun model
    s16             blendFrames;   // Whole frames the next blended reseed takes to reach the requested clip
    s16             walkMode;      // How the walk in progress covers its distance (`ACTOR_450800_WALK_FAST`, `_BACKWARD` or `_SLOW`)
    u8              gunShown;      // 1 while the gun is drawn with the body, 0 while it stays hidden whatever the body's draw mode
    byte            pad_501[0x3];
} _Actor450800KyleMadiganWork;
STATIC_ASSERT_SIZEOF(_Actor450800KyleMadiganWork, 0x504);

/// Spawn offset `_actor450800SpawnNurseryImpactSpark` copies into a local and hands to
/// `effectSpawn` as the effect's position.
static const SVECTOR D_actor_450800_80131E24 = { 0x19C8, -0x578, 0x3C0, 0 };

/// Message table `func_actor_450800_80132160` hangs off `Task::msgTable`, and
/// the `TaskDesc` table its three helper tasks come from.
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.

extern TaskMessageEntry D_actor_450800_8014AC58[];
extern TaskDesc         D_actor_450800_8014AC88[];

/// Animation-set table bound to the work block's context by `animationInitContext`.
extern u8 D_actor_450800_8014ACC4[];

/// The enemy's message table, the `TaskDesc` table its model tasks come from,
/// and the animation data its work block's slots are seeded from - the same
/// three roles as the actor's tables above.
extern TaskMessageEntry gPairWalkMessages[];
extern TaskDesc         gPairWalkTasks[];
extern u8               gPairWalkAnimParams[];

extern s32                  D_actor_450800_8013930C;
extern AnimationPlayRequest D_actor_450800_801397A4;
extern ActorTransform       D_actor_450800_801398EC;
extern EvsCommand           D_actor_450800_8013A564[];
extern EvsCommand           D_actor_450800_8013A684[];
extern EvsCommand           D_actor_450800_8013A774[];
extern EvsCommand           D_actor_450800_8013A984[];
extern EvsCommand           D_actor_450800_8013AB7C[];
extern EvsCommand           D_actor_450800_8013ACFC[];

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

static void _actor450800UpdateKyleMadigan(Task* task);
static void _actorRenderWalkerFrame(Enemy* unusedEnemy, Task* task);
static void _actor450800ExitKyleMadigan(Task* task);
static void _actorRenderWalkerFrameSecond(Enemy* unusedEnemy, Task* task);
static void _actorRenderDrawWalkerGroundShadow(Task* task);
static void _actorRenderDrawSecondWalkerGroundShadow(Task* task);

static TmdSource _gActor450800Body;
static TmdSource _gActor450800KyleMadiganBody;
static TmdSource _gActor450800KyleMadiganHandRight;
static TmdSource _gActor450800KyleMadiganHandLeft;
static TmdSource _gActor450800KyleMadiganGun;
void             func_actor_450800_80132790(Task*);
static void      _actor450800KyleMadiganAttachmentTask(Task* task);

static s32 _actor450800PlayKyleMadiganAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArgument);
static s32 _actor450800SetKyleMadiganDrawFlags(Task* task, s32 messageId, s32 flags, s32 unusedArgument);
s32        func_actor_450800_80132CE0(Task* task, s32 msgId, ActorCommand* msg, s32);
static s32 _actor450800SetKyleMadiganWalkTarget(Task* task, s32 messageId, const VECTOR* target, s32 mode);

static TmdSource _gActor450800PawnGolemBody;
static TmdSource _gActor450800GolemBeamSword;
static s32       _actor450800IgnorePawnGolemCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArgument);
void             func_actor_450800_80133264(Task*);

extern AnimationPlayRequest D_actor_450800_80139560;
extern AnimationPlayRequest D_actor_450800_80139628;
extern AnimationPlayRequest D_actor_450800_80139894;
extern ActorTransform       D_actor_450800_8013994C;
static void                 _actor450800SetNurseryCaptions(s32 useNurseryCaptions);
static void                 _actor450800EnterGrowthRoom(void);

extern AnimationPlayRequest     D_actor_450800_80139458;
extern AnimationPlayRequest     D_actor_450800_8013946C;
extern AnimationPlayRequest     D_actor_450800_80139480;
extern AnimationPlayRequest     D_actor_450800_80139494;
extern AnimationPlayRequest     D_actor_450800_801394A8;
extern AnimationPlayRequest     D_actor_450800_801395C4;
extern AnimationPlayRequest     D_actor_450800_801395D8;
extern AnimationPlayRequest     D_actor_450800_801397CC;
extern AnimationPlayRequest     D_actor_450800_801397E0;
extern AnimationPlayRequest     D_actor_450800_801397F4;
extern AnimationPlayRequest     D_actor_450800_80139808;
extern AnimationPlayRequest     D_actor_450800_8013981C;
extern AnimationPlayRequest     D_actor_450800_80139830;
extern AnimationPlayRequest     D_actor_450800_801398A8;
extern AnimationPlayRequest     D_actor_450800_8013ADEC;
extern AnimationPlayRequest     D_actor_450800_8013AE00;
extern AnimationPlayRequest     D_actor_450800_8013AE14;
extern ActorCommand             D_actor_450800_801398E0;
extern ActorCommand             D_actor_450800_801398E4;
extern AnimationBankCopyRequest D_actor_450800_801398D0;
extern AnimationBankCopyRequest D_actor_450800_801398D8;
extern ActorTransform           D_actor_450800_8013AE30;
extern ActorTransform           D_actor_450800_8013AE48;
extern ActorTransform           D_actor_450800_8013AE60;
static void                     _actor450800SetNurseryView13SpriteHidden(s32 hidden);
static void                     _actor450800SpawnNurseryImpactSpark(void);

extern AnimationPlayRequest D_actor_450800_80139390;
extern AnimationPlayRequest D_actor_450800_801393A4;
extern AnimationPlayRequest D_actor_450800_801393CC;
extern AnimationPlayRequest D_actor_450800_801393E0;
extern AnimationPlayRequest D_actor_450800_801393F4;
extern AnimationPlayRequest D_actor_450800_80139408;
extern AnimationPlayRequest D_actor_450800_8013941C;
extern AnimationPlayRequest D_actor_450800_80139430;
extern AnimationPlayRequest D_actor_450800_8013954C;
extern AnimationPlayRequest D_actor_450800_80139574;
extern AnimationPlayRequest D_actor_450800_80139588;
extern AnimationPlayRequest D_actor_450800_80139650;
extern AnimationPlayRequest D_actor_450800_80139664;
extern AnimationPlayRequest D_actor_450800_80139678;
extern AnimationPlayRequest D_actor_450800_8013968C;
extern AnimationPlayRequest D_actor_450800_801396A0;
extern AnimationPlayRequest D_actor_450800_801396C8;
extern AnimationPlayRequest D_actor_450800_801396DC;
extern AnimationPlayRequest D_actor_450800_801396F0;
extern AnimationPlayRequest D_actor_450800_80139704;
extern AnimationPlayRequest D_actor_450800_80139718;
extern AnimationPlayRequest D_actor_450800_8013972C;
extern AnimationPlayRequest D_actor_450800_80139740;
extern AnimationPlayRequest D_actor_450800_80139754;
extern AnimationPlayRequest D_actor_450800_80139768;
extern AnimationPlayRequest D_actor_450800_8013977C;
extern AnimationPlayRequest D_actor_450800_80139790;
void                        func_actor_450800_80131F70(u32);
static void                 _actor450800StartNurseryRepeatDialogue(s32 dialogueSelector);

static AnimationPackedPose _gActor450800Animation01B20Bank1[2] = {
#include "assets/actor_450800_animation_01B20_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation01B20Bank4[22] = {
#include "assets/actor_450800_animation_01B20_bank4.inc"
};

static AnimationRecord _gActor450800Animation01B20Records[62] = {
#include "assets/actor_450800_animation_01B20_records.inc"
};

static u16 _gActor450800Animation01B20Indices[20] = {
#include "assets/actor_450800_animation_01B20_indices.inc"
};

static AnimationSet _gActor450800Animation01B20 = {
    _gActor450800Animation01B20Records,
    _gActor450800Animation01B20Indices,
    { NULL, _gActor450800Animation01B20Bank1, NULL, NULL, _gActor450800Animation01B20Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation01DFCBank1[2] = {
#include "assets/actor_450800_animation_01DFC_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation01DFCBank4[41] = {
#include "assets/actor_450800_animation_01DFC_bank4.inc"
};

static AnimationRecord _gActor450800Animation01DFCRecords[116] = {
#include "assets/actor_450800_animation_01DFC_records.inc"
};

static u16 _gActor450800Animation01DFCIndices[20] = {
#include "assets/actor_450800_animation_01DFC_indices.inc"
};

static AnimationSet _gActor450800Animation01DFC = {
    _gActor450800Animation01DFCRecords,
    _gActor450800Animation01DFCIndices,
    { NULL, _gActor450800Animation01DFCBank1, NULL, NULL, _gActor450800Animation01DFCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation02070Bank1[2] = {
#include "assets/actor_450800_animation_02070_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation02070Bank4[34] = {
#include "assets/actor_450800_animation_02070_bank4.inc"
};

static AnimationRecord _gActor450800Animation02070Records[97] = {
#include "assets/actor_450800_animation_02070_records.inc"
};

static u16 _gActor450800Animation02070Indices[20] = {
#include "assets/actor_450800_animation_02070_indices.inc"
};

static AnimationSet _gActor450800Animation02070 = {
    _gActor450800Animation02070Records,
    _gActor450800Animation02070Indices,
    { NULL, _gActor450800Animation02070Bank1, NULL, NULL, _gActor450800Animation02070Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation022D8Bank1[2] = {
#include "assets/actor_450800_animation_022D8_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation022D8Bank4[31] = {
#include "assets/actor_450800_animation_022D8_bank4.inc"
};

static AnimationRecord _gActor450800Animation022D8Records[97] = {
#include "assets/actor_450800_animation_022D8_records.inc"
};

static u16 _gActor450800Animation022D8Indices[20] = {
#include "assets/actor_450800_animation_022D8_indices.inc"
};

static AnimationSet _gActor450800Animation022D8 = {
    _gActor450800Animation022D8Records,
    _gActor450800Animation022D8Indices,
    { NULL, _gActor450800Animation022D8Bank1, NULL, NULL, _gActor450800Animation022D8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation02538Bank1[2] = {
#include "assets/actor_450800_animation_02538_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation02538Bank4[30] = {
#include "assets/actor_450800_animation_02538_bank4.inc"
};

static AnimationRecord _gActor450800Animation02538Records[96] = {
#include "assets/actor_450800_animation_02538_records.inc"
};

static u16 _gActor450800Animation02538Indices[20] = {
#include "assets/actor_450800_animation_02538_indices.inc"
};

static AnimationSet _gActor450800Animation02538 = {
    _gActor450800Animation02538Records,
    _gActor450800Animation02538Indices,
    { NULL, _gActor450800Animation02538Bank1, NULL, NULL, _gActor450800Animation02538Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation02840Bank1[2] = {
#include "assets/actor_450800_animation_02840_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation02840Bank4[44] = {
#include "assets/actor_450800_animation_02840_bank4.inc"
};

static AnimationRecord _gActor450800Animation02840Records[124] = {
#include "assets/actor_450800_animation_02840_records.inc"
};

static u16 _gActor450800Animation02840Indices[20] = {
#include "assets/actor_450800_animation_02840_indices.inc"
};

static AnimationSet _gActor450800Animation02840 = {
    _gActor450800Animation02840Records,
    _gActor450800Animation02840Indices,
    { NULL, _gActor450800Animation02840Bank1, NULL, NULL, _gActor450800Animation02840Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation02A04Bank1[2] = {
#include "assets/actor_450800_animation_02A04_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation02A04Bank4[24] = {
#include "assets/actor_450800_animation_02A04_bank4.inc"
};

static AnimationRecord _gActor450800Animation02A04Records[63] = {
#include "assets/actor_450800_animation_02A04_records.inc"
};

static u16 _gActor450800Animation02A04Indices[20] = {
#include "assets/actor_450800_animation_02A04_indices.inc"
};

static AnimationSet _gActor450800Animation02A04 = {
    _gActor450800Animation02A04Records,
    _gActor450800Animation02A04Indices,
    { NULL, _gActor450800Animation02A04Bank1, NULL, NULL, _gActor450800Animation02A04Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation02C30Bank1[2] = {
#include "assets/actor_450800_animation_02C30_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation02C30Bank4[33] = {
#include "assets/actor_450800_animation_02C30_bank4.inc"
};

static AnimationRecord _gActor450800Animation02C30Records[80] = {
#include "assets/actor_450800_animation_02C30_records.inc"
};

static u16 _gActor450800Animation02C30Indices[20] = {
#include "assets/actor_450800_animation_02C30_indices.inc"
};

static AnimationSet _gActor450800Animation02C30 = {
    _gActor450800Animation02C30Records,
    _gActor450800Animation02C30Indices,
    { NULL, _gActor450800Animation02C30Bank1, NULL, NULL, _gActor450800Animation02C30Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation02DE4Bank1[2] = {
#include "assets/actor_450800_animation_02DE4_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation02DE4Bank4[26] = {
#include "assets/actor_450800_animation_02DE4_bank4.inc"
};

static AnimationRecord _gActor450800Animation02DE4Records[57] = {
#include "assets/actor_450800_animation_02DE4_records.inc"
};

static u16 _gActor450800Animation02DE4Indices[20] = {
#include "assets/actor_450800_animation_02DE4_indices.inc"
};

static AnimationSet _gActor450800Animation02DE4 = {
    _gActor450800Animation02DE4Records,
    _gActor450800Animation02DE4Indices,
    { NULL, _gActor450800Animation02DE4Bank1, NULL, NULL, _gActor450800Animation02DE4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation03348Bank1[8] = {
#include "assets/actor_450800_animation_03348_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation03348Bank4[97] = {
#include "assets/actor_450800_animation_03348_bank4.inc"
};

static AnimationRecord _gActor450800Animation03348Records[204] = {
#include "assets/actor_450800_animation_03348_records.inc"
};

static u16 _gActor450800Animation03348Indices[20] = {
#include "assets/actor_450800_animation_03348_indices.inc"
};

static AnimationSet _gActor450800Animation03348 = {
    _gActor450800Animation03348Records,
    _gActor450800Animation03348Indices,
    { NULL, _gActor450800Animation03348Bank1, NULL, NULL, _gActor450800Animation03348Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation0359CBank1[3] = {
#include "assets/actor_450800_animation_0359C_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation0359CBank4[30] = {
#include "assets/actor_450800_animation_0359C_bank4.inc"
};

static AnimationRecord _gActor450800Animation0359CRecords[90] = {
#include "assets/actor_450800_animation_0359C_records.inc"
};

static u16 _gActor450800Animation0359CIndices[20] = {
#include "assets/actor_450800_animation_0359C_indices.inc"
};

static AnimationSet _gActor450800Animation0359C = {
    _gActor450800Animation0359CRecords,
    _gActor450800Animation0359CIndices,
    { NULL, _gActor450800Animation0359CBank1, NULL, NULL, _gActor450800Animation0359CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation03954Bank1[3] = {
#include "assets/actor_450800_animation_03954_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation03954Bank4[81] = {
#include "assets/actor_450800_animation_03954_bank4.inc"
};

static AnimationRecord _gActor450800Animation03954Records[128] = {
#include "assets/actor_450800_animation_03954_records.inc"
};

static u16 _gActor450800Animation03954Indices[20] = {
#include "assets/actor_450800_animation_03954_indices.inc"
};

static AnimationSet _gActor450800Animation03954 = {
    _gActor450800Animation03954Records,
    _gActor450800Animation03954Indices,
    { NULL, _gActor450800Animation03954Bank1, NULL, NULL, _gActor450800Animation03954Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation03D3CBank1[5] = {
#include "assets/actor_450800_animation_03D3C_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation03D3CBank4[67] = {
#include "assets/actor_450800_animation_03D3C_bank4.inc"
};

static AnimationRecord _gActor450800Animation03D3CRecords[148] = {
#include "assets/actor_450800_animation_03D3C_records.inc"
};

static u16 _gActor450800Animation03D3CIndices[20] = {
#include "assets/actor_450800_animation_03D3C_indices.inc"
};

static AnimationSet _gActor450800Animation03D3C = {
    _gActor450800Animation03D3CRecords,
    _gActor450800Animation03D3CIndices,
    { NULL, _gActor450800Animation03D3CBank1, NULL, NULL, _gActor450800Animation03D3CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation04078Bank1[3] = {
#include "assets/actor_450800_animation_04078_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation04078Bank4[53] = {
#include "assets/actor_450800_animation_04078_bank4.inc"
};

static AnimationRecord _gActor450800Animation04078Records[125] = {
#include "assets/actor_450800_animation_04078_records.inc"
};

static u16 _gActor450800Animation04078Indices[20] = {
#include "assets/actor_450800_animation_04078_indices.inc"
};

static AnimationSet _gActor450800Animation04078 = {
    _gActor450800Animation04078Records,
    _gActor450800Animation04078Indices,
    { NULL, _gActor450800Animation04078Bank1, NULL, NULL, _gActor450800Animation04078Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation04240Bank1[2] = {
#include "assets/actor_450800_animation_04240_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation04240Bank4[23] = {
#include "assets/actor_450800_animation_04240_bank4.inc"
};

static AnimationRecord _gActor450800Animation04240Records[65] = {
#include "assets/actor_450800_animation_04240_records.inc"
};

static u16 _gActor450800Animation04240Indices[20] = {
#include "assets/actor_450800_animation_04240_indices.inc"
};

static AnimationSet _gActor450800Animation04240 = {
    _gActor450800Animation04240Records,
    _gActor450800Animation04240Indices,
    { NULL, _gActor450800Animation04240Bank1, NULL, NULL, _gActor450800Animation04240Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation04680Bank1[6] = {
#include "assets/actor_450800_animation_04680_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation04680Bank4[89] = {
#include "assets/actor_450800_animation_04680_bank4.inc"
};

static AnimationRecord _gActor450800Animation04680Records[145] = {
#include "assets/actor_450800_animation_04680_records.inc"
};

static u16 _gActor450800Animation04680Indices[20] = {
#include "assets/actor_450800_animation_04680_indices.inc"
};

static AnimationSet _gActor450800Animation04680 = {
    _gActor450800Animation04680Records,
    _gActor450800Animation04680Indices,
    { NULL, _gActor450800Animation04680Bank1, NULL, NULL, _gActor450800Animation04680Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation048B8Bank1[2] = {
#include "assets/actor_450800_animation_048B8_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation048B8Bank4[26] = {
#include "assets/actor_450800_animation_048B8_bank4.inc"
};

static AnimationRecord _gActor450800Animation048B8Records[90] = {
#include "assets/actor_450800_animation_048B8_records.inc"
};

static u16 _gActor450800Animation048B8Indices[20] = {
#include "assets/actor_450800_animation_048B8_indices.inc"
};

static AnimationSet _gActor450800Animation048B8 = {
    _gActor450800Animation048B8Records,
    _gActor450800Animation048B8Indices,
    { NULL, _gActor450800Animation048B8Bank1, NULL, NULL, _gActor450800Animation048B8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation04C24Bank1[2] = {
#include "assets/actor_450800_animation_04C24_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation04C24Bank4[81] = {
#include "assets/actor_450800_animation_04C24_bank4.inc"
};

static AnimationRecord _gActor450800Animation04C24Records[112] = {
#include "assets/actor_450800_animation_04C24_records.inc"
};

static u16 _gActor450800Animation04C24Indices[20] = {
#include "assets/actor_450800_animation_04C24_indices.inc"
};

static AnimationSet _gActor450800Animation04C24 = {
    _gActor450800Animation04C24Records,
    _gActor450800Animation04C24Indices,
    { NULL, _gActor450800Animation04C24Bank1, NULL, NULL, _gActor450800Animation04C24Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation04E48Bank1[2] = {
#include "assets/actor_450800_animation_04E48_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation04E48Bank4[23] = {
#include "assets/actor_450800_animation_04E48_bank4.inc"
};

static AnimationRecord _gActor450800Animation04E48Records[88] = {
#include "assets/actor_450800_animation_04E48_records.inc"
};

static u16 _gActor450800Animation04E48Indices[20] = {
#include "assets/actor_450800_animation_04E48_indices.inc"
};

static AnimationSet _gActor450800Animation04E48 = {
    _gActor450800Animation04E48Records,
    _gActor450800Animation04E48Indices,
    { NULL, _gActor450800Animation04E48Bank1, NULL, NULL, _gActor450800Animation04E48Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation04FD8Bank1[2] = {
#include "assets/actor_450800_animation_04FD8_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation04FD8Bank4[17] = {
#include "assets/actor_450800_animation_04FD8_bank4.inc"
};

static AnimationRecord _gActor450800Animation04FD8Records[57] = {
#include "assets/actor_450800_animation_04FD8_records.inc"
};

static u16 _gActor450800Animation04FD8Indices[20] = {
#include "assets/actor_450800_animation_04FD8_indices.inc"
};

static AnimationSet _gActor450800Animation04FD8 = {
    _gActor450800Animation04FD8Records,
    _gActor450800Animation04FD8Indices,
    { NULL, _gActor450800Animation04FD8Bank1, NULL, NULL, _gActor450800Animation04FD8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation05CB4Bank1[24] = {
#include "assets/actor_450800_animation_05CB4_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation05CB4Bank4[302] = {
#include "assets/actor_450800_animation_05CB4_bank4.inc"
};

static AnimationRecord _gActor450800Animation05CB4Records[429] = {
#include "assets/actor_450800_animation_05CB4_records.inc"
};

static u16 _gActor450800Animation05CB4Indices[20] = {
#include "assets/actor_450800_animation_05CB4_indices.inc"
};

static AnimationSet _gActor450800Animation05CB4 = {
    _gActor450800Animation05CB4Records,
    _gActor450800Animation05CB4Indices,
    { NULL, _gActor450800Animation05CB4Bank1, NULL, NULL, _gActor450800Animation05CB4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation05EDCBank1[3] = {
#include "assets/actor_450800_animation_05EDC_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation05EDCBank4[24] = {
#include "assets/actor_450800_animation_05EDC_bank4.inc"
};

static AnimationRecord _gActor450800Animation05EDCRecords[85] = {
#include "assets/actor_450800_animation_05EDC_records.inc"
};

static u16 _gActor450800Animation05EDCIndices[20] = {
#include "assets/actor_450800_animation_05EDC_indices.inc"
};

static AnimationSet _gActor450800Animation05EDC = {
    _gActor450800Animation05EDCRecords,
    _gActor450800Animation05EDCIndices,
    { NULL, _gActor450800Animation05EDCBank1, NULL, NULL, _gActor450800Animation05EDCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation061D0Bank1[4] = {
#include "assets/actor_450800_animation_061D0_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation061D0Bank4[35] = {
#include "assets/actor_450800_animation_061D0_bank4.inc"
};

static AnimationRecord _gActor450800Animation061D0Records[122] = {
#include "assets/actor_450800_animation_061D0_records.inc"
};

static u16 _gActor450800Animation061D0Indices[20] = {
#include "assets/actor_450800_animation_061D0_indices.inc"
};

static AnimationSet _gActor450800Animation061D0 = {
    _gActor450800Animation061D0Records,
    _gActor450800Animation061D0Indices,
    { NULL, _gActor450800Animation061D0Bank1, NULL, NULL, _gActor450800Animation061D0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation063E8Bank1[2] = {
#include "assets/actor_450800_animation_063E8_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation063E8Bank4[31] = {
#include "assets/actor_450800_animation_063E8_bank4.inc"
};

static AnimationRecord _gActor450800Animation063E8Records[77] = {
#include "assets/actor_450800_animation_063E8_records.inc"
};

static u16 _gActor450800Animation063E8Indices[20] = {
#include "assets/actor_450800_animation_063E8_indices.inc"
};

static AnimationSet _gActor450800Animation063E8 = {
    _gActor450800Animation063E8Records,
    _gActor450800Animation063E8Indices,
    { NULL, _gActor450800Animation063E8Bank1, NULL, NULL, _gActor450800Animation063E8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation0690CBank1[5] = {
#include "assets/actor_450800_animation_0690C_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation0690CBank4[101] = {
#include "assets/actor_450800_animation_0690C_bank4.inc"
};

static AnimationRecord _gActor450800Animation0690CRecords[193] = {
#include "assets/actor_450800_animation_0690C_records.inc"
};

static u16 _gActor450800Animation0690CIndices[20] = {
#include "assets/actor_450800_animation_0690C_indices.inc"
};

static AnimationSet _gActor450800Animation0690C = {
    _gActor450800Animation0690CRecords,
    _gActor450800Animation0690CIndices,
    { NULL, _gActor450800Animation0690CBank1, NULL, NULL, _gActor450800Animation0690CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation06C64Bank1[4] = {
#include "assets/actor_450800_animation_06C64_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation06C64Bank4[41] = {
#include "assets/actor_450800_animation_06C64_bank4.inc"
};

static AnimationRecord _gActor450800Animation06C64Records[141] = {
#include "assets/actor_450800_animation_06C64_records.inc"
};

static u16 _gActor450800Animation06C64Indices[20] = {
#include "assets/actor_450800_animation_06C64_indices.inc"
};

static AnimationSet _gActor450800Animation06C64 = {
    _gActor450800Animation06C64Records,
    _gActor450800Animation06C64Indices,
    { NULL, _gActor450800Animation06C64Bank1, NULL, NULL, _gActor450800Animation06C64Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation070F8Bank1[8] = {
#include "assets/actor_450800_animation_070F8_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation070F8Bank4[90] = {
#include "assets/actor_450800_animation_070F8_bank4.inc"
};

static AnimationRecord _gActor450800Animation070F8Records[159] = {
#include "assets/actor_450800_animation_070F8_records.inc"
};

static u16 _gActor450800Animation070F8Indices[20] = {
#include "assets/actor_450800_animation_070F8_indices.inc"
};

static AnimationSet _gActor450800Animation070F8 = {
    _gActor450800Animation070F8Records,
    _gActor450800Animation070F8Indices,
    { NULL, _gActor450800Animation070F8Bank1, NULL, NULL, _gActor450800Animation070F8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation074C4Bank1[6] = {
#include "assets/actor_450800_animation_074C4_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation074C4Bank4[64] = {
#include "assets/actor_450800_animation_074C4_bank4.inc"
};

static AnimationRecord _gActor450800Animation074C4Records[141] = {
#include "assets/actor_450800_animation_074C4_records.inc"
};

static u16 _gActor450800Animation074C4Indices[20] = {
#include "assets/actor_450800_animation_074C4_indices.inc"
};

static AnimationSet _gActor450800Animation074C4 = {
    _gActor450800Animation074C4Records,
    _gActor450800Animation074C4Indices,
    { NULL, _gActor450800Animation074C4Bank1, NULL, NULL, _gActor450800Animation074C4Bank4, NULL, NULL, NULL },
};

s32 D_actor_450800_8013930C = 0;

/// Player clips for extended ids 47-63.
///
/// A script sends the player its copy request before it plays any of these
/// clips. `D_actor_450800_801398D8` copies `ANIMATION_BANK_EXTENSION_CAPACITY`
/// (32) words starting here into the player's bank, which is 15 words past the
/// end of this array: the read runs on through `D_actor_450800_80139354`,
/// `D_actor_450800_80139368` and `D_actor_450800_8013937C`. That overrun is the
/// original's and is kept as it is: the request carries the bank's fixed
/// capacity, while the table was stored with only its own entries. The requests
/// the scripts play on the player select base id 1 and extended ids 47-63 only,
/// so none of the words installed after the seventeen clips is played as one.
AnimationSet* D_actor_450800_80139310[17] = { &_gActor450800Animation01B20, &_gActor450800Animation01DFC, &_gActor450800Animation02070, &_gActor450800Animation022D8, &_gActor450800Animation02538, &_gActor450800Animation02840, &_gActor450800Animation02A04, &_gActor450800Animation02C30, &_gActor450800Animation02DE4, &_gActor450800Animation03348, &_gActor450800Animation0359C, &_gActor450800Animation03954, &_gActor450800Animation03D3C, &_gActor450800Animation04078, &_gActor450800Animation04240, &_gActor450800Animation04680, &_gActor450800Animation074C4 };

// The run of requests the package keeps for the player starts here; a script resolves the player's weapon bank in its own copy of a request before it dispatches it. This one is not referenced.
AnimationPlayRequest D_actor_450800_80139354 = { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_80139368 = { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_8013937C = { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_80139390 = { { .index = 1 }, 49, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_801393A4 = { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_801393B8 = { { .index = 1 }, 51, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_801393CC = { { .index = 1 }, 52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_801393E0 = { { .index = 1 }, 53, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_801393F4 = { { .index = 1 }, 54, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_80139408 = { { .index = 1 }, 55, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_8013941C = { { .index = 1 }, 56, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_80139430 = { { .index = 1 }, 57, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_80139444 = { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_80139458 = { { .index = 1 }, 59, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_8013946C = { { .index = 1 }, 60, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_80139480 = { { .index = 1 }, 61, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_80139494 = { { .index = 1 }, 62, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_801394A8 = { { .index = 1 }, 63, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

/// Companion clips for extended ids 47-57.
///
/// A script sends the companion its copy request before it plays any of these
/// clips. `D_actor_450800_801398D0` copies `ANIMATION_BANK_EXTENSION_CAPACITY`
/// (32) words starting here into the companion's bank, which is 21 words past
/// the end of this array: the read runs on through `D_actor_450800_801394E8`,
/// `D_actor_450800_801394FC`, `D_actor_450800_80139510`,
/// `D_actor_450800_80139524` and the first word of `D_actor_450800_80139538`.
/// That overrun is the original's and is kept as it is: the request carries the
/// bank's fixed capacity, while the table was stored with only its own entries.
/// The requests the scripts play on the companion select base id 1 and extended
/// ids 47-57 only, so none of the words installed after the eleven clips is
/// played as one.
AnimationSet* D_actor_450800_801394BC[11] = { &_gActor450800Animation048B8, &_gActor450800Animation04C24, &_gActor450800Animation04E48, &_gActor450800Animation04FD8, &_gActor450800Animation05CB4, &_gActor450800Animation05EDC, &_gActor450800Animation061D0, &_gActor450800Animation063E8, &_gActor450800Animation0690C, &_gActor450800Animation06C64, &_gActor450800Animation070F8 };

// The run of requests the package keeps for the companion starts here; a script resolves the companion's bank in its own copy of a request before it dispatches it. This one is not referenced.
AnimationPlayRequest D_actor_450800_801394E8 = { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_801394FC = { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_80139510 = { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_80139524 = { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_80139538 = { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_8013954C = { { .index = 1 }, 51, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_80139560 = { { .index = 1 }, 52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_80139574 = { { .index = 1 }, 53, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_80139588 = { { .index = 1 }, 54, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_8013959C[2] = {
    { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 55, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
};

AnimationPlayRequest D_actor_450800_801395C4 = { { .index = 1 }, 56, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_801395D8 = { { .index = 1 }, 57, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_801395EC[3] = {
    { { .index = 1 }, 58, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 59, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 0, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
};

AnimationPlayRequest D_actor_450800_80139628 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_8013963C = { { .index = 1 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_80139650 = { { .index = 1 }, 3, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_80139664 = { { .index = 1 }, 4, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_80139678 = { { .index = 1 }, 5, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_8013968C = { { .index = 1 }, 6, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_801396A0 = { { .index = 1 }, 7, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_801396B4 = { { .index = 1 }, 8, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_801396C8 = { { .index = 1 }, 9, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_801396DC = { { .index = 1 }, 10, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_801396F0 = { { .index = 1 }, 11, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_80139704 = { { .index = 1 }, 12, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_80139718 = { { .index = 1 }, 13, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_8013972C = { { .index = 1 }, 14, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_80139740 = { { .index = 1 }, 15, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_80139754 = { { .index = 1 }, 16, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_80139768 = { { .index = 1 }, 17, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_8013977C = { { .index = 1 }, 18, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_80139790 = { { .index = 1 }, 19, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_801397A4 = { { .index = 1 }, 20, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_801397B8 = { { .index = 1 }, 0, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_801397CC = { { .index = 1 }, 21, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_801397E0 = { { .index = 1 }, 22, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_801397F4 = { { .index = 1 }, 23, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_80139808 = { { .index = 1 }, 24, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_8013981C = { { .index = 1 }, 25, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_80139830 = { { .index = 1 }, 26, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_80139844[2] = {
    { { .index = 1 }, 27, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 28, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
};

AnimationPlayRequest D_actor_450800_8013986C = { { .index = 1 }, 52, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_80139880 = { { .index = 1 }, 58, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_80139894 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_801398A8 = { { .index = 1 }, 1, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_801398BC = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationBankCopyRequest D_actor_450800_801398D0 = { { .sets = D_actor_450800_801394BC }, ANIMATION_BANK_EXTENSION_CAPACITY };

AnimationBankCopyRequest D_actor_450800_801398D8 = { { .sets = D_actor_450800_80139310 }, ANIMATION_BANK_EXTENSION_CAPACITY };

ActorCommand D_actor_450800_801398E0 = { { .loc = { 5, 22 } }, 0 };

ActorCommand D_actor_450800_801398E4 = { { .loc = { 5, 22 } }, 1 };

ActorCommand D_actor_450800_801398E8 = { { .loc = { 5, 22 } }, 2 };

ActorTransform D_actor_450800_801398EC = { { 5650, 0, 2400, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_450800_80139904 = { { 6080, 0, 900, 0 }, { 0, 853, 0, 0 } };

ActorTransform D_actor_450800_8013991C = { { 5951, 0, 900, 0 }, { 0, 853, 0, 0 } };

ActorTransform D_actor_450800_80139934 = { { 6770, 0, 900, 0 }, { 0, -1137, 0, 0 } };

ActorTransform D_actor_450800_8013994C = { { 6650, 0, 401, 0 }, { 0, -1024, 0, 0 } };

EvsCommand D_actor_450800_80139964[105] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450800SetNurseryCaptions }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_KEEP_SOUND, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_450800_801398D8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_450800_801398D0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_450800_80139628 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_450800_801398E8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_80139894 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_801398BC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_450800_80139904 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_450800_80139934 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_450800_80139650 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 100 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_450800_80139678 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_450800_80139768 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_80139368 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_801394FC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_8013937C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_80139390 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_801393A4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_450800_8013977C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_801393CC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_80139510 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 25 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_80139524 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_450800_80139664 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_450800_8013972C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 61 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_450800_80139628 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_450800_80139740 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_450800_80139650 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_450800_8013968C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_450800_801396A0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_801393E0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_801393F4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU32 = func_actor_450800_80131F70 }, { .value = 0x10000 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b6_nursery_8017FFF4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_80139408 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_80139588 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_80139538 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_450800_80139754 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_8013941C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_8013954C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 78 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x55160006 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_450800_8013991C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_450800_8013994C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_80139560 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_80139894 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_REQUEST_SCENE_MUSIC, { .value = 25 }, { .value = 1 }, { .value = 1 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450800SetNurseryCaptions }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_450800_8013A33C[23] = {
    { EVENT_SCRIPT_OPCODE_STOP_SOUND, { .value = 0x55160006 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU32 = func_actor_450800_80131F70 }, { .value = 0x10000 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_450800_8013994C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_450800_8013991C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_REQUEST_SCENE_MUSIC, { .value = 25 }, { .value = 1 }, { .value = 1 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_shelter_b6_nursery_8017FFF4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_80139560 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_80139894 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_450800_80139628 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450800SetNurseryCaptions }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_450800_8013A564[12] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450800SetNurseryCaptions }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_450800_801398D0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_801398A8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_80139574 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_8013986C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450800SetNurseryCaptions }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_450800_8013A684[10] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450800SetNurseryCaptions }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450800StartNurseryRepeatDialogue }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_801398A8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_450800_801396C8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_450800_801396DC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450800SetNurseryCaptions }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_450800_8013A774[9] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450800SetNurseryCaptions }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450800StartNurseryRepeatDialogue }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_801398A8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_450800_80139790 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450800SetNurseryCaptions }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_450800_8013A84C[6] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_450800_801398D0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_450800_8013994C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_80139560 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_450800_80139628 } }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_450800_8013A8DC[7] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_450800_801398D0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_450800_8013994C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_80139560 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_450800_801397A4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_450800_801398EC } }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_450800_8013A984[21] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450800SetNurseryCaptions }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_450800_801398D8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 6 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_801398A8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_450800_801396F0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_450800_80139704 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_80139880 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_450800_801396F0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_450800_80139704 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_80139430 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_450800_80139718 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_450800_801397A4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450800SetNurseryCaptions }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_450800_8013AB7C[16] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450800SetNurseryCaptions }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_450800_801398D8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 7 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_801398A8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_450800_801396F0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_450800_80139704 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_80139880 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_450800_801396F0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_450800_80139704 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450800SetNurseryCaptions }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_450800_8013ACFC[10] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450800SetNurseryCaptions }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450800StartNurseryRepeatDialogue }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_801398A8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_450800_801396F0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_450800_80139704 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450800SetNurseryCaptions }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

AnimationPlayRequest D_actor_450800_8013ADEC = { { .index = 0 }, 4, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_8013AE00 = { { .index = 0 }, 5, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450800_8013AE14 = { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

GameActorMoveAnim D_actor_450800_8013AE28 = { 19, 1 };

ActorTransform D_actor_450800_8013AE30 = { { 6280, 0, 960, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_actor_450800_8013AE48 = { { 6400, 0, 960, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_actor_450800_8013AE60 = { { 5350, 0, -20, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_450800_8013AE78 = { { 5320, 0, 2900, 0 }, { 0, 2048, 0, 0 } };

ActorTransform D_actor_450800_8013AE90 = { { 5320, 0, 1180, 0 }, { 0, 2048, 0, 0 } };

ActorTransform D_actor_450800_8013AEA8 = { { 5320, 0, 540, 0 }, { 0, -1877, 0, 0 } };

ActorTransform D_actor_450800_8013AEC0 = { { 5280, 0, -290, 0 }, { 0, 455, 0, 0 } };

ActorTransform D_actor_450800_8013AED8 = { { 6500, 0, 1000, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_actor_450800_8013AEF0 = { { 5710, 0, -290, 0 }, { 0, -455, 0, 0 } };

ActorTransform D_actor_450800_8013AF08 = { { 7000, 0, 1200, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_actor_450800_8013AF20 = { { -1200, 0, -220, 0 }, { 0, 807, 0, 0 } };

ActorTransform D_actor_450800_8013AF38 = { { 800, 0, 450, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_actor_450800_8013AF50 = { { 0, 0, 0, 0 }, { 0, 853, 0, 0 } };

EvsSceneKey D_actor_450800_8013AF68 = { 5, 9, 11 };

PadScriptCmd D_actor_450800_8013AF70[5] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_WAIT, 1), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_LOOP, 3), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_JUMP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

PadScriptVibrationSegment D_actor_450800_8013AF84[2] = {
    { 210, 63, 9, 1 },
    { 0, 0, 2, 0 },
};

EvsCommand D_actor_450800_8013AF8C[117] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450800SetNurseryCaptions }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_actor_450800_8013AF68 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_801398A8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 13 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_450800_801398D8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_450800_801398D0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x55160007 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_801394A8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_801394A8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 150 }, { .value = 40 }, { .value = 40 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_SCENE_AUDIO, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_450800_801398E4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 3 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_450800_801398E4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1011 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_450800_8013AE90 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_801394A8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_450800_8013AEA8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_80139458 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_8013946C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 1 }, { .value = 15 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_450800_80139830 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_450800_8013AE30 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_VIBRATION, { .padCommands = D_actor_450800_8013AF70 }, { .vibrationSegments = D_actor_450800_8013AF84 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_450800_8013981C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_450800_801398E0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor450800SpawnNurseryImpactSpark }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450800SetNurseryView13SpriteHidden }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_VIBRATION, { .padCommands = D_actor_450800_8013AF70 }, { .vibrationSegments = D_actor_450800_8013AF84 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_450800_8013981C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_450800_801398E0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_VIBRATION, { .padCommands = D_actor_450800_8013AF70 }, { .vibrationSegments = D_actor_450800_8013AF84 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_450800_8013981C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_450800_801398E0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_VIBRATION, { .padCommands = D_actor_450800_8013AF70 }, { .vibrationSegments = D_actor_450800_8013AF84 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_450800_801397CC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_450800_8013AEC0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_450800_8013AEF0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_80139480 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_801395C4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_450800_801397E0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_450800_8013AE48 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1011 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 50 }, { .value = 10 }, { .value = 10 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_450800_8013AE14 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2003 }, { .message = { .pointer = &D_actor_450800_8013AE14 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2004 }, { .message = { .pointer = &D_actor_450800_8013AF20 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_450800_8013ADEC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2013 }, { .message = { .pointer = &D_actor_450800_8013AF38 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2004 }, { .message = { .pointer = &D_actor_450800_8013AF50 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2003 }, { .message = { .pointer = &D_actor_450800_8013AE00 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 150 }, { .value = 40 }, { .value = 40 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 2 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_450800_8013AF08 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_450800_8013AED8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_80139494 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_801395D8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1011 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 3 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 3 }, { .value = 2003 }, { .message = { .pointer = &D_actor_450800_801397F4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 3 }, { .value = 2004 }, { .message = { .pointer = &D_actor_450800_8013AE60 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 3 }, { .value = 2003 }, { .message = { .pointer = &D_actor_450800_80139808 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor450800EnterGrowthRoom }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CANCEL_SECONDARY_FADE, { .value = 0 }, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450800SetNurseryCaptions }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_450800_8013BA84[20] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor450800EnterGrowthRoom }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_450800_8013994C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_80139560 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_80139894 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_450800_80139628 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450800SetNurseryCaptions }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

static TmdBone _gActor450800BodySkeleton[20] = {
#include "assets/actor_450800_body_skeleton.inc"
};

static u32 _gActor450800BodyPartVerts[20] = {
#include "assets/actor_450800_body_partVerts.inc"
};

static SVECTOR _gActor450800BodyVerts[287] = {
#include "assets/actor_450800_body_verts.inc"
};

static SVECTOR _gActor450800BodyNormals[285] = {
#include "assets/actor_450800_body_normals.inc"
};

static u32 _gActor450800BodyStream[3368] = {
#include "assets/actor_450800_body_stream.inc"
};

static TmdSource _gActor450800Body = {
    0,
    17772,
    5724,
    20,
    _gActor450800BodyPartVerts,
    _gActor450800BodyVerts,
    _gActor450800BodyNormals,
    _gActor450800BodySkeleton,
    _gActor450800BodyStream,
};

static TmdBone _gActor450800KyleMadiganBodySkeleton[20] = {
#include "assets/kyle_madigan_body_skeleton.inc"
};

static u32 _gActor450800KyleMadiganBodyPartVerts[20] = {
#include "assets/kyle_madigan_body_partVerts.inc"
};

static SVECTOR _gActor450800KyleMadiganBodyVerts[300] = {
#include "assets/kyle_madigan_body_verts.inc"
};

static SVECTOR _gActor450800KyleMadiganBodyNormals[298] = {
#include "assets/kyle_madigan_body_normals.inc"
};

static u32 _gActor450800KyleMadiganBodyStream[3412] = {
#include "assets/kyle_madigan_body_stream.inc"
};

static TmdSource _gActor450800KyleMadiganBody = {
    0,
    18224,
    5696,
    20,
    _gActor450800KyleMadiganBodyPartVerts,
    _gActor450800KyleMadiganBodyVerts,
    _gActor450800KyleMadiganBodyNormals,
    _gActor450800KyleMadiganBodySkeleton,
    _gActor450800KyleMadiganBodyStream,
};

static AnimationPackedPose _gActor450800Animation13564Bank1[2] = {
#include "assets/actor_450800_animation_13564_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation13564Bank4[25] = {
#include "assets/actor_450800_animation_13564_bank4.inc"
};

static AnimationRecord _gActor450800Animation13564Records[93] = {
#include "assets/actor_450800_animation_13564_records.inc"
};

static u16 _gActor450800Animation13564Indices[20] = {
#include "assets/actor_450800_animation_13564_indices.inc"
};

static AnimationSet _gActor450800Animation13564 = {
    _gActor450800Animation13564Records,
    _gActor450800Animation13564Indices,
    { NULL, _gActor450800Animation13564Bank1, NULL, NULL, _gActor450800Animation13564Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation13870Bank1[2] = {
#include "assets/actor_450800_animation_13870_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation13870Bank4[53] = {
#include "assets/actor_450800_animation_13870_bank4.inc"
};

static AnimationRecord _gActor450800Animation13870Records[116] = {
#include "assets/actor_450800_animation_13870_records.inc"
};

static u16 _gActor450800Animation13870Indices[20] = {
#include "assets/actor_450800_animation_13870_indices.inc"
};

static AnimationSet _gActor450800Animation13870 = {
    _gActor450800Animation13870Records,
    _gActor450800Animation13870Indices,
    { NULL, _gActor450800Animation13870Bank1, NULL, NULL, _gActor450800Animation13870Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation13B28Bank1[2] = {
#include "assets/actor_450800_animation_13B28_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation13B28Bank4[37] = {
#include "assets/actor_450800_animation_13B28_bank4.inc"
};

static AnimationRecord _gActor450800Animation13B28Records[111] = {
#include "assets/actor_450800_animation_13B28_records.inc"
};

static u16 _gActor450800Animation13B28Indices[20] = {
#include "assets/actor_450800_animation_13B28_indices.inc"
};

static AnimationSet _gActor450800Animation13B28 = {
    _gActor450800Animation13B28Records,
    _gActor450800Animation13B28Indices,
    { NULL, _gActor450800Animation13B28Bank1, NULL, NULL, _gActor450800Animation13B28Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation13E14Bank1[2] = {
#include "assets/actor_450800_animation_13E14_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation13E14Bank4[42] = {
#include "assets/actor_450800_animation_13E14_bank4.inc"
};

static AnimationRecord _gActor450800Animation13E14Records[119] = {
#include "assets/actor_450800_animation_13E14_records.inc"
};

static u16 _gActor450800Animation13E14Indices[20] = {
#include "assets/actor_450800_animation_13E14_indices.inc"
};

static AnimationSet _gActor450800Animation13E14 = {
    _gActor450800Animation13E14Records,
    _gActor450800Animation13E14Indices,
    { NULL, _gActor450800Animation13E14Bank1, NULL, NULL, _gActor450800Animation13E14Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation14030Bank1[2] = {
#include "assets/actor_450800_animation_14030_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation14030Bank4[38] = {
#include "assets/actor_450800_animation_14030_bank4.inc"
};

static AnimationRecord _gActor450800Animation14030Records[71] = {
#include "assets/actor_450800_animation_14030_records.inc"
};

static u16 _gActor450800Animation14030Indices[20] = {
#include "assets/actor_450800_animation_14030_indices.inc"
};

static AnimationSet _gActor450800Animation14030 = {
    _gActor450800Animation14030Records,
    _gActor450800Animation14030Indices,
    { NULL, _gActor450800Animation14030Bank1, NULL, NULL, _gActor450800Animation14030Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation14304Bank1[2] = {
#include "assets/actor_450800_animation_14304_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation14304Bank4[42] = {
#include "assets/actor_450800_animation_14304_bank4.inc"
};

static AnimationRecord _gActor450800Animation14304Records[113] = {
#include "assets/actor_450800_animation_14304_records.inc"
};

static u16 _gActor450800Animation14304Indices[20] = {
#include "assets/actor_450800_animation_14304_indices.inc"
};

static AnimationSet _gActor450800Animation14304 = {
    _gActor450800Animation14304Records,
    _gActor450800Animation14304Indices,
    { NULL, _gActor450800Animation14304Bank1, NULL, NULL, _gActor450800Animation14304Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation14544Bank1[2] = {
#include "assets/actor_450800_animation_14544_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation14544Bank4[25] = {
#include "assets/actor_450800_animation_14544_bank4.inc"
};

static AnimationRecord _gActor450800Animation14544Records[93] = {
#include "assets/actor_450800_animation_14544_records.inc"
};

static u16 _gActor450800Animation14544Indices[20] = {
#include "assets/actor_450800_animation_14544_indices.inc"
};

static AnimationSet _gActor450800Animation14544 = {
    _gActor450800Animation14544Records,
    _gActor450800Animation14544Indices,
    { NULL, _gActor450800Animation14544Bank1, NULL, NULL, _gActor450800Animation14544Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation1478CBank1[2] = {
#include "assets/actor_450800_animation_1478C_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation1478CBank4[26] = {
#include "assets/actor_450800_animation_1478C_bank4.inc"
};

static AnimationRecord _gActor450800Animation1478CRecords[94] = {
#include "assets/actor_450800_animation_1478C_records.inc"
};

static u16 _gActor450800Animation1478CIndices[20] = {
#include "assets/actor_450800_animation_1478C_indices.inc"
};

static AnimationSet _gActor450800Animation1478C = {
    _gActor450800Animation1478CRecords,
    _gActor450800Animation1478CIndices,
    { NULL, _gActor450800Animation1478CBank1, NULL, NULL, _gActor450800Animation1478CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation14A70Bank1[2] = {
#include "assets/actor_450800_animation_14A70_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation14A70Bank4[42] = {
#include "assets/actor_450800_animation_14A70_bank4.inc"
};

static AnimationRecord _gActor450800Animation14A70Records[117] = {
#include "assets/actor_450800_animation_14A70_records.inc"
};

static u16 _gActor450800Animation14A70Indices[20] = {
#include "assets/actor_450800_animation_14A70_indices.inc"
};

static AnimationSet _gActor450800Animation14A70 = {
    _gActor450800Animation14A70Records,
    _gActor450800Animation14A70Indices,
    { NULL, _gActor450800Animation14A70Bank1, NULL, NULL, _gActor450800Animation14A70Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation14C98Bank1[2] = {
#include "assets/actor_450800_animation_14C98_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation14C98Bank4[39] = {
#include "assets/actor_450800_animation_14C98_bank4.inc"
};

static AnimationRecord _gActor450800Animation14C98Records[73] = {
#include "assets/actor_450800_animation_14C98_records.inc"
};

static u16 _gActor450800Animation14C98Indices[20] = {
#include "assets/actor_450800_animation_14C98_indices.inc"
};

static AnimationSet _gActor450800Animation14C98 = {
    _gActor450800Animation14C98Records,
    _gActor450800Animation14C98Indices,
    { NULL, _gActor450800Animation14C98Bank1, NULL, NULL, _gActor450800Animation14C98Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation14F8CBank1[2] = {
#include "assets/actor_450800_animation_14F8C_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation14F8CBank4[43] = {
#include "assets/actor_450800_animation_14F8C_bank4.inc"
};

static AnimationRecord _gActor450800Animation14F8CRecords[120] = {
#include "assets/actor_450800_animation_14F8C_records.inc"
};

static u16 _gActor450800Animation14F8CIndices[20] = {
#include "assets/actor_450800_animation_14F8C_indices.inc"
};

static AnimationSet _gActor450800Animation14F8C = {
    _gActor450800Animation14F8CRecords,
    _gActor450800Animation14F8CIndices,
    { NULL, _gActor450800Animation14F8CBank1, NULL, NULL, _gActor450800Animation14F8CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation1517CBank1[2] = {
#include "assets/actor_450800_animation_1517C_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation1517CBank4[30] = {
#include "assets/actor_450800_animation_1517C_bank4.inc"
};

static AnimationRecord _gActor450800Animation1517CRecords[68] = {
#include "assets/actor_450800_animation_1517C_records.inc"
};

static u16 _gActor450800Animation1517CIndices[20] = {
#include "assets/actor_450800_animation_1517C_indices.inc"
};

static AnimationSet _gActor450800Animation1517C = {
    _gActor450800Animation1517CRecords,
    _gActor450800Animation1517CIndices,
    { NULL, _gActor450800Animation1517CBank1, NULL, NULL, _gActor450800Animation1517CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation15494Bank1[2] = {
#include "assets/actor_450800_animation_15494_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation15494Bank4[56] = {
#include "assets/actor_450800_animation_15494_bank4.inc"
};

static AnimationRecord _gActor450800Animation15494Records[116] = {
#include "assets/actor_450800_animation_15494_records.inc"
};

static u16 _gActor450800Animation15494Indices[20] = {
#include "assets/actor_450800_animation_15494_indices.inc"
};

static AnimationSet _gActor450800Animation15494 = {
    _gActor450800Animation15494Records,
    _gActor450800Animation15494Indices,
    { NULL, _gActor450800Animation15494Bank1, NULL, NULL, _gActor450800Animation15494Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation157F0Bank1[2] = {
#include "assets/actor_450800_animation_157F0_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation157F0Bank4[70] = {
#include "assets/actor_450800_animation_157F0_bank4.inc"
};

static AnimationRecord _gActor450800Animation157F0Records[119] = {
#include "assets/actor_450800_animation_157F0_records.inc"
};

static u16 _gActor450800Animation157F0Indices[20] = {
#include "assets/actor_450800_animation_157F0_indices.inc"
};

static AnimationSet _gActor450800Animation157F0 = {
    _gActor450800Animation157F0Records,
    _gActor450800Animation157F0Indices,
    { NULL, _gActor450800Animation157F0Bank1, NULL, NULL, _gActor450800Animation157F0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation15A78Bank1[2] = {
#include "assets/actor_450800_animation_15A78_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation15A78Bank4[44] = {
#include "assets/actor_450800_animation_15A78_bank4.inc"
};

static AnimationRecord _gActor450800Animation15A78Records[92] = {
#include "assets/actor_450800_animation_15A78_records.inc"
};

static u16 _gActor450800Animation15A78Indices[20] = {
#include "assets/actor_450800_animation_15A78_indices.inc"
};

static AnimationSet _gActor450800Animation15A78 = {
    _gActor450800Animation15A78Records,
    _gActor450800Animation15A78Indices,
    { NULL, _gActor450800Animation15A78Bank1, NULL, NULL, _gActor450800Animation15A78Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation15C18Bank1[2] = {
#include "assets/actor_450800_animation_15C18_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation15C18Bank4[18] = {
#include "assets/actor_450800_animation_15C18_bank4.inc"
};

static AnimationRecord _gActor450800Animation15C18Records[60] = {
#include "assets/actor_450800_animation_15C18_records.inc"
};

static u16 _gActor450800Animation15C18Indices[20] = {
#include "assets/actor_450800_animation_15C18_indices.inc"
};

static AnimationSet _gActor450800Animation15C18 = {
    _gActor450800Animation15C18Records,
    _gActor450800Animation15C18Indices,
    { NULL, _gActor450800Animation15C18Bank1, NULL, NULL, _gActor450800Animation15C18Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation15E00Bank1[2] = {
#include "assets/actor_450800_animation_15E00_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation15E00Bank4[24] = {
#include "assets/actor_450800_animation_15E00_bank4.inc"
};

static AnimationRecord _gActor450800Animation15E00Records[72] = {
#include "assets/actor_450800_animation_15E00_records.inc"
};

static u16 _gActor450800Animation15E00Indices[20] = {
#include "assets/actor_450800_animation_15E00_indices.inc"
};

static AnimationSet _gActor450800Animation15E00 = {
    _gActor450800Animation15E00Records,
    _gActor450800Animation15E00Indices,
    { NULL, _gActor450800Animation15E00Bank1, NULL, NULL, _gActor450800Animation15E00Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation15FDCBank1[2] = {
#include "assets/actor_450800_animation_15FDC_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation15FDCBank4[23] = {
#include "assets/actor_450800_animation_15FDC_bank4.inc"
};

static AnimationRecord _gActor450800Animation15FDCRecords[70] = {
#include "assets/actor_450800_animation_15FDC_records.inc"
};

static u16 _gActor450800Animation15FDCIndices[20] = {
#include "assets/actor_450800_animation_15FDC_indices.inc"
};

static AnimationSet _gActor450800Animation15FDC = {
    _gActor450800Animation15FDCRecords,
    _gActor450800Animation15FDCIndices,
    { NULL, _gActor450800Animation15FDCBank1, NULL, NULL, _gActor450800Animation15FDCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation16320Bank1[2] = {
#include "assets/actor_450800_animation_16320_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation16320Bank4[55] = {
#include "assets/actor_450800_animation_16320_bank4.inc"
};

static AnimationRecord _gActor450800Animation16320Records[128] = {
#include "assets/actor_450800_animation_16320_records.inc"
};

static u16 _gActor450800Animation16320Indices[20] = {
#include "assets/actor_450800_animation_16320_indices.inc"
};

static AnimationSet _gActor450800Animation16320 = {
    _gActor450800Animation16320Records,
    _gActor450800Animation16320Indices,
    { NULL, _gActor450800Animation16320Bank1, NULL, NULL, _gActor450800Animation16320Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation16658Bank1[3] = {
#include "assets/actor_450800_animation_16658_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation16658Bank4[59] = {
#include "assets/actor_450800_animation_16658_bank4.inc"
};

static AnimationRecord _gActor450800Animation16658Records[118] = {
#include "assets/actor_450800_animation_16658_records.inc"
};

static u16 _gActor450800Animation16658Indices[20] = {
#include "assets/actor_450800_animation_16658_indices.inc"
};

static AnimationSet _gActor450800Animation16658 = {
    _gActor450800Animation16658Records,
    _gActor450800Animation16658Indices,
    { NULL, _gActor450800Animation16658Bank1, NULL, NULL, _gActor450800Animation16658Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation167F8Bank1[2] = {
#include "assets/actor_450800_animation_167F8_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation167F8Bank4[18] = {
#include "assets/actor_450800_animation_167F8_bank4.inc"
};

static AnimationRecord _gActor450800Animation167F8Records[60] = {
#include "assets/actor_450800_animation_167F8_records.inc"
};

static u16 _gActor450800Animation167F8Indices[20] = {
#include "assets/actor_450800_animation_167F8_indices.inc"
};

static AnimationSet _gActor450800Animation167F8 = {
    _gActor450800Animation167F8Records,
    _gActor450800Animation167F8Indices,
    { NULL, _gActor450800Animation167F8Bank1, NULL, NULL, _gActor450800Animation167F8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation169B8Bank1[2] = {
#include "assets/actor_450800_animation_169B8_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation169B8Bank4[26] = {
#include "assets/actor_450800_animation_169B8_bank4.inc"
};

static AnimationRecord _gActor450800Animation169B8Records[60] = {
#include "assets/actor_450800_animation_169B8_records.inc"
};

static u16 _gActor450800Animation169B8Indices[20] = {
#include "assets/actor_450800_animation_169B8_indices.inc"
};

static AnimationSet _gActor450800Animation169B8 = {
    _gActor450800Animation169B8Records,
    _gActor450800Animation169B8Indices,
    { NULL, _gActor450800Animation169B8Bank1, NULL, NULL, _gActor450800Animation169B8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation16C14Bank1[2] = {
#include "assets/actor_450800_animation_16C14_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation16C14Bank4[42] = {
#include "assets/actor_450800_animation_16C14_bank4.inc"
};

static AnimationRecord _gActor450800Animation16C14Records[83] = {
#include "assets/actor_450800_animation_16C14_records.inc"
};

static u16 _gActor450800Animation16C14Indices[20] = {
#include "assets/actor_450800_animation_16C14_indices.inc"
};

static AnimationSet _gActor450800Animation16C14 = {
    _gActor450800Animation16C14Records,
    _gActor450800Animation16C14Indices,
    { NULL, _gActor450800Animation16C14Bank1, NULL, NULL, _gActor450800Animation16C14Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation16EA8Bank1[3] = {
#include "assets/actor_450800_animation_16EA8_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation16EA8Bank4[31] = {
#include "assets/actor_450800_animation_16EA8_bank4.inc"
};

static AnimationRecord _gActor450800Animation16EA8Records[105] = {
#include "assets/actor_450800_animation_16EA8_records.inc"
};

static u16 _gActor450800Animation16EA8Indices[20] = {
#include "assets/actor_450800_animation_16EA8_indices.inc"
};

static AnimationSet _gActor450800Animation16EA8 = {
    _gActor450800Animation16EA8Records,
    _gActor450800Animation16EA8Indices,
    { NULL, _gActor450800Animation16EA8Bank1, NULL, NULL, _gActor450800Animation16EA8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation17080Bank1[3] = {
#include "assets/actor_450800_animation_17080_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation17080Bank4[23] = {
#include "assets/actor_450800_animation_17080_bank4.inc"
};

static AnimationRecord _gActor450800Animation17080Records[66] = {
#include "assets/actor_450800_animation_17080_records.inc"
};

static u16 _gActor450800Animation17080Indices[20] = {
#include "assets/actor_450800_animation_17080_indices.inc"
};

static AnimationSet _gActor450800Animation17080 = {
    _gActor450800Animation17080Records,
    _gActor450800Animation17080Indices,
    { NULL, _gActor450800Animation17080Bank1, NULL, NULL, _gActor450800Animation17080Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation17488Bank1[10] = {
#include "assets/actor_450800_animation_17488_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation17488Bank4[85] = {
#include "assets/actor_450800_animation_17488_bank4.inc"
};

static AnimationRecord _gActor450800Animation17488Records[123] = {
#include "assets/actor_450800_animation_17488_records.inc"
};

static u16 _gActor450800Animation17488Indices[20] = {
#include "assets/actor_450800_animation_17488_indices.inc"
};

static AnimationSet _gActor450800Animation17488 = {
    _gActor450800Animation17488Records,
    _gActor450800Animation17488Indices,
    { NULL, _gActor450800Animation17488Bank1, NULL, NULL, _gActor450800Animation17488Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation17700Bank1[2] = {
#include "assets/actor_450800_animation_17700_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation17700Bank4[50] = {
#include "assets/actor_450800_animation_17700_bank4.inc"
};

static AnimationRecord _gActor450800Animation17700Records[82] = {
#include "assets/actor_450800_animation_17700_records.inc"
};

static u16 _gActor450800Animation17700Indices[20] = {
#include "assets/actor_450800_animation_17700_indices.inc"
};

static AnimationSet _gActor450800Animation17700 = {
    _gActor450800Animation17700Records,
    _gActor450800Animation17700Indices,
    { NULL, _gActor450800Animation17700Bank1, NULL, NULL, _gActor450800Animation17700Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation178F4Bank1[2] = {
#include "assets/actor_450800_animation_178F4_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation178F4Bank4[19] = {
#include "assets/actor_450800_animation_178F4_bank4.inc"
};

static AnimationRecord _gActor450800Animation178F4Records[80] = {
#include "assets/actor_450800_animation_178F4_records.inc"
};

static u16 _gActor450800Animation178F4Indices[20] = {
#include "assets/actor_450800_animation_178F4_indices.inc"
};

static AnimationSet _gActor450800Animation178F4 = {
    _gActor450800Animation178F4Records,
    _gActor450800Animation178F4Indices,
    { NULL, _gActor450800Animation178F4Bank1, NULL, NULL, _gActor450800Animation178F4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation17DD0Bank1[8] = {
#include "assets/actor_450800_animation_17DD0_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation17DD0Bank4[114] = {
#include "assets/actor_450800_animation_17DD0_bank4.inc"
};

static AnimationRecord _gActor450800Animation17DD0Records[153] = {
#include "assets/actor_450800_animation_17DD0_records.inc"
};

static u16 _gActor450800Animation17DD0Indices[20] = {
#include "assets/actor_450800_animation_17DD0_indices.inc"
};

static AnimationSet _gActor450800Animation17DD0 = {
    _gActor450800Animation17DD0Records,
    _gActor450800Animation17DD0Indices,
    { NULL, _gActor450800Animation17DD0Bank1, NULL, NULL, _gActor450800Animation17DD0Bank4, NULL, NULL, NULL },
};

static TmdBone _gActor450800KyleMadiganHandRightSkeleton[1] = {
#include "assets/kyle_madigan_hand_right_skeleton.inc"
};

static u32 _gActor450800KyleMadiganHandRightPartVerts[1] = {
#include "assets/kyle_madigan_hand_right_partVerts.inc"
};

static SVECTOR _gActor450800KyleMadiganHandRightVerts[23] = {
#include "assets/kyle_madigan_hand_right_verts.inc"
};

static SVECTOR _gActor450800KyleMadiganHandRightNormals[23] = {
#include "assets/kyle_madigan_hand_right_normals.inc"
};

static u32 _gActor450800KyleMadiganHandRightStream[166] = {
#include "assets/kyle_madigan_hand_right_stream.inc"
};

static TmdSource _gActor450800KyleMadiganHandRight = {
    0,
    1148,
    0,
    1,
    _gActor450800KyleMadiganHandRightPartVerts,
    _gActor450800KyleMadiganHandRightVerts,
    _gActor450800KyleMadiganHandRightNormals,
    _gActor450800KyleMadiganHandRightSkeleton,
    _gActor450800KyleMadiganHandRightStream,
};

static TmdBone _gActor450800KyleMadiganHandLeftSkeleton[1] = {
#include "assets/kyle_madigan_hand_left_skeleton.inc"
};

static u32 _gActor450800KyleMadiganHandLeftPartVerts[1] = {
#include "assets/kyle_madigan_hand_left_partVerts.inc"
};

static SVECTOR _gActor450800KyleMadiganHandLeftVerts[27] = {
#include "assets/kyle_madigan_hand_left_verts.inc"
};

static SVECTOR _gActor450800KyleMadiganHandLeftNormals[27] = {
#include "assets/kyle_madigan_hand_left_normals.inc"
};

static u32 _gActor450800KyleMadiganHandLeftStream[189] = {
#include "assets/kyle_madigan_hand_left_stream.inc"
};

static TmdSource _gActor450800KyleMadiganHandLeft = {
    0,
    1328,
    0,
    1,
    _gActor450800KyleMadiganHandLeftPartVerts,
    _gActor450800KyleMadiganHandLeftVerts,
    _gActor450800KyleMadiganHandLeftNormals,
    _gActor450800KyleMadiganHandLeftSkeleton,
    _gActor450800KyleMadiganHandLeftStream,
};

static AnimationPackedPose _gActor450800Animation189CCBank1[3] = {
#include "assets/actor_450800_animation_189CC_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation189CCBank4[29] = {
#include "assets/actor_450800_animation_189CC_bank4.inc"
};

static AnimationRecord _gActor450800Animation189CCRecords[116] = {
#include "assets/actor_450800_animation_189CC_records.inc"
};

static u16 _gActor450800Animation189CCIndices[20] = {
#include "assets/actor_450800_animation_189CC_indices.inc"
};

static AnimationSet _gActor450800Animation189CC = {
    _gActor450800Animation189CCRecords,
    _gActor450800Animation189CCIndices,
    { NULL, _gActor450800Animation189CCBank1, NULL, NULL, _gActor450800Animation189CCBank4, NULL, NULL, NULL },
};

static TmdBone _gActor450800KyleMadiganGunSkeleton[1] = {
#include "assets/kyle_madigan_gun_skeleton.inc"
};

static u32 _gActor450800KyleMadiganGunPartVerts[1] = {
#include "assets/kyle_madigan_gun_partVerts.inc"
};

static SVECTOR _gActor450800KyleMadiganGunVerts[22] = {
#include "assets/kyle_madigan_gun_verts.inc"
};

static SVECTOR _gActor450800KyleMadiganGunNormals[24] = {
#include "assets/kyle_madigan_gun_normals.inc"
};

static u32 _gActor450800KyleMadiganGunStream[162] = {
#include "assets/kyle_madigan_gun_stream.inc"
};

static TmdSource _gActor450800KyleMadiganGun = {
    0,
    1108,
    0,
    1,
    _gActor450800KyleMadiganGunPartVerts,
    _gActor450800KyleMadiganGunVerts,
    _gActor450800KyleMadiganGunNormals,
    _gActor450800KyleMadiganGunSkeleton,
    _gActor450800KyleMadiganGunStream,
};

TaskMessageEntry D_actor_450800_8014AC58[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _actor450800PlayKyleMadiganAnimation },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _actor450800SetKyleMadiganDrawFlags },
    { ACTOR_MESSAGE_PLACE, _pacedWalkPlace },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_450800_80132CE0 },
    { ACTOR_MESSAGE_WALK_TO, _actor450800SetKyleMadiganWalkTarget },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_actor_450800_8014AC88[5] = {
    { { { TASK_BODY_TMD, 192 } }, func_actor_450800_80132790, { .model = &_gActor450800KyleMadiganBody } },
    { { { TASK_BODY_TMD, 192 } }, _actor450800KyleMadiganAttachmentTask, { .model = &_gActor450800KyleMadiganHandLeft } },
    { { { TASK_BODY_TMD, 192 } }, _actor450800KyleMadiganAttachmentTask, { .model = &_gActor450800KyleMadiganHandRight } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_450800_80132790, { .model = &_gActor450800Body } },
    { { { TASK_BODY_TMD, 192 } }, _actor450800KyleMadiganAttachmentTask, { .model = &_gActor450800KyleMadiganGun } },
};

u8 D_actor_450800_8014ACC4[124] = {
    0,
    0,
    0,
    0,
    132,
    83,
    20,
    128,
    144,
    86,
    20,
    128,
    72,
    89,
    20,
    128,
    52,
    92,
    20,
    128,
    80,
    94,
    20,
    128,
    36,
    97,
    20,
    128,
    100,
    99,
    20,
    128,
    172,
    101,
    20,
    128,
    144,
    104,
    20,
    128,
    184,
    106,
    20,
    128,
    172,
    109,
    20,
    128,
    156,
    111,
    20,
    128,
    180,
    114,
    20,
    128,
    16,
    118,
    20,
    128,
    152,
    120,
    20,
    128,
    56,
    122,
    20,
    128,
    32,
    124,
    20,
    128,
    252,
    125,
    20,
    128,
    64,
    129,
    20,
    128,
    236,
    167,
    20,
    128,
    120,
    132,
    20,
    128,
    24,
    134,
    20,
    128,
    216,
    135,
    20,
    128,
    52,
    138,
    20,
    128,
    200,
    140,
    20,
    128,
    160,
    142,
    20,
    128,
    168,
    146,
    20,
    128,
    32,
    149,
    20,
    128,
    20,
    151,
    20,
    128,
    240,
    155,
    20,
    128,
};

static TmdBone _gActor450800PawnGolemBodySkeleton[19] = {
#include "assets/pawn_golem_body_skeleton.inc"
};

static u32 _gActor450800PawnGolemBodyPartVerts[19] = {
#include "assets/pawn_golem_body_partVerts.inc"
};

static SVECTOR _gActor450800PawnGolemBodyVerts[339] = {
#include "assets/pawn_golem_body_verts.inc"
};

static SVECTOR _gActor450800PawnGolemBodyNormals[346] = {
#include "assets/pawn_golem_body_normals.inc"
};

static u32 _gActor450800PawnGolemBodyStream[3745] = {
#include "assets/pawn_golem_body_stream.inc"
};

static TmdSource _gActor450800PawnGolemBody = {
    0,
    20476,
    5672,
    19,
    _gActor450800PawnGolemBodyPartVerts,
    _gActor450800PawnGolemBodyVerts,
    _gActor450800PawnGolemBodyNormals,
    _gActor450800PawnGolemBodySkeleton,
    _gActor450800PawnGolemBodyStream,
};

static TmdBone _gActor450800GolemBeamSwordSkeleton[1] = {
#include "assets/golem_beam_sword_skeleton.inc"
};

static u32 _gActor450800GolemBeamSwordPartVerts[1] = {
#include "assets/golem_beam_sword_partVerts.inc"
};

static SVECTOR _gActor450800GolemBeamSwordVerts[29] = {
#include "assets/golem_beam_sword_verts.inc"
};

static SVECTOR _gActor450800GolemBeamSwordNormals[24] = {
#include "assets/golem_beam_sword_normals.inc"
};

static u32 _gActor450800GolemBeamSwordStream[212] = {
#include "assets/golem_beam_sword_stream.inc"
};

static TmdSource _gActor450800GolemBeamSword = {
    0,
    1436,
    0,
    1,
    _gActor450800GolemBeamSwordPartVerts,
    _gActor450800GolemBeamSwordVerts,
    _gActor450800GolemBeamSwordNormals,
    _gActor450800GolemBeamSwordSkeleton,
    _gActor450800GolemBeamSwordStream,
};

static AnimationPackedPose _gActor450800Animation1F37CBank1[21] = {
#include "assets/actor_450800_animation_1F37C_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation1F37CBank4[317] = {
#include "assets/actor_450800_animation_1F37C_bank4.inc"
};

static AnimationRecord _gActor450800Animation1F37CRecords[382] = {
#include "assets/actor_450800_animation_1F37C_records.inc"
};

static u16 _gActor450800Animation1F37CIndices[20] = {
#include "assets/actor_450800_animation_1F37C_indices.inc"
};

static AnimationSet _gActor450800Animation1F37C = {
    _gActor450800Animation1F37CRecords,
    _gActor450800Animation1F37CIndices,
    { NULL, _gActor450800Animation1F37CBank1, NULL, NULL, _gActor450800Animation1F37CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation1FAE4Bank1[12] = {
#include "assets/actor_450800_animation_1FAE4_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation1FAE4Bank4[187] = {
#include "assets/actor_450800_animation_1FAE4_bank4.inc"
};

static AnimationRecord _gActor450800Animation1FAE4Records[231] = {
#include "assets/actor_450800_animation_1FAE4_records.inc"
};

static u16 _gActor450800Animation1FAE4Indices[20] = {
#include "assets/actor_450800_animation_1FAE4_indices.inc"
};

static AnimationSet _gActor450800Animation1FAE4 = {
    _gActor450800Animation1FAE4Records,
    _gActor450800Animation1FAE4Indices,
    { NULL, _gActor450800Animation1FAE4Bank1, NULL, NULL, _gActor450800Animation1FAE4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation206D4Bank1[20] = {
#include "assets/actor_450800_animation_206D4_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation206D4Bank4[321] = {
#include "assets/actor_450800_animation_206D4_bank4.inc"
};

static AnimationRecord _gActor450800Animation206D4Records[363] = {
#include "assets/actor_450800_animation_206D4_records.inc"
};

static u16 _gActor450800Animation206D4Indices[20] = {
#include "assets/actor_450800_animation_206D4_indices.inc"
};

static AnimationSet _gActor450800Animation206D4 = {
    _gActor450800Animation206D4Records,
    _gActor450800Animation206D4Indices,
    { NULL, _gActor450800Animation206D4Bank1, NULL, NULL, _gActor450800Animation206D4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation2103CBank1[16] = {
#include "assets/actor_450800_animation_2103C_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation2103CBank4[237] = {
#include "assets/actor_450800_animation_2103C_bank4.inc"
};

static AnimationRecord _gActor450800Animation2103CRecords[297] = {
#include "assets/actor_450800_animation_2103C_records.inc"
};

static u16 _gActor450800Animation2103CIndices[20] = {
#include "assets/actor_450800_animation_2103C_indices.inc"
};

static AnimationSet _gActor450800Animation2103C = {
    _gActor450800Animation2103CRecords,
    _gActor450800Animation2103CIndices,
    { NULL, _gActor450800Animation2103CBank1, NULL, NULL, _gActor450800Animation2103CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450800Animation21B64Bank1[26] = {
#include "assets/actor_450800_animation_21B64_bank1.inc"
};

static AnimationPackedRotation _gActor450800Animation21B64Bank4[224] = {
#include "assets/actor_450800_animation_21B64_bank4.inc"
};

static AnimationRecord _gActor450800Animation21B64Records[392] = {
#include "assets/actor_450800_animation_21B64_records.inc"
};

static u16 _gActor450800Animation21B64Indices[20] = {
#include "assets/actor_450800_animation_21B64_indices.inc"
};

static AnimationSet _gActor450800Animation21B64 = {
    _gActor450800Animation21B64Records,
    _gActor450800Animation21B64Indices,
    { NULL, _gActor450800Animation21B64Bank1, NULL, NULL, _gActor450800Animation21B64Bank4, NULL, NULL, NULL },
};

TaskMessageEntry gPairWalkMessages[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _pairWalkPlay },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _pairWalkSetVisibility },
    { ACTOR_MESSAGE_PLACE, _pairWalkPlace },
    { ACTOR_COMMAND_MESSAGE_APPLY, _actor450800IgnorePawnGolemCommand },
    { ACTOR_MESSAGE_WALK_TO, _pairWalkTo },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc gPairWalkTasks[2] = {
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, func_actor_450800_80133264, { .model = &_gActor450800PawnGolemBody } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, _pairWalkSubModelTask, { .model = &_gActor450800GolemBeamSword } },
};

u8 gPairWalkAnimParams[24] = {
    0,
    0,
    0,
    0,
    156,
    17,
    21,
    128,
    4,
    25,
    21,
    128,
    244,
    36,
    21,
    128,
    92,
    46,
    21,
    128,
    132,
    57,
    21,
    128,
};

void               func_actor_450800_80131E2C(void);
static inline void _actor450800TintModel(Task* spawned, Task* actor);
static void        func_actor_450800_80132160(Enemy* enemy, Task* task);

void func_actor_450800_80131E2C(void)
{
    s32 temp_v0;
    s32 n;

    if (gGameSession->location.loc.view == 4) {
        if (gameFlagGetNibble(GAME_FLAG_B6_NURSERY_PROGRESS) == 1) {
            temp_v0                 = D_actor_450800_8013930C + 1;
            D_actor_450800_8013930C = temp_v0;
            if (temp_v0 >= 3) {
                D_actor_450800_8013930C = 3;
                evsStartScript(D_actor_450800_8013A774, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            } else {
                evsStartScript(D_actor_450800_8013A684, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            }
        } else {
            n = gameFlagGetNibble(GAME_FLAG_B6_NURSERY_SCENE_COUNT) + 1;
            if (n >= 4) {
                n = 3;
            }
            gameFlagSetNibble(GAME_FLAG_B6_NURSERY_SCENE_COUNT, n);
            if (n == 1) {
                if (gameFlagGetNibble(GAME_FLAG_083) == n) {
                    evsStartScript(D_actor_450800_8013A984, EVENT_SCRIPT_HUD_HIDE_RESTORE);
                } else {
                    evsStartScript(D_actor_450800_8013AB7C, EVENT_SCRIPT_HUD_HIDE_RESTORE);
                }
                gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, 0x32);
            } else {
                evsStartScript(D_actor_450800_8013ACFC, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            }
        }
    }
}

/// Selects the nursery scene's CAP resource and text texture, or restores defaults.
///
/// Nonzero `useNurseryCaptions` selects loaded data resource ordinal 2 with
/// text at VRAM (832, 0). The resource must be loaded and writable for CAP
/// relocation, and remain live through playback. Zero resets CAP playback
/// state and selects the default resource and texture.
static void _actor450800SetNurseryCaptions(s32 useNurseryCaptions)
{
    enum {
        ACTOR_450800_NURSERY_CAP_RESOURCE = 2,
        ACTOR_450800_NURSERY_TEXT_VRAM_X  = 832,
        ACTOR_450800_NURSERY_TEXT_VRAM_Y  = 0,
    };

    if (useNurseryCaptions != 0) {
        Gp_CapFile = NULL;
        capSelectLoadedFile(ACTOR_450800_NURSERY_CAP_RESOURCE);
        capSetTexturePage(ACTOR_450800_NURSERY_TEXT_VRAM_X, ACTOR_450800_NURSERY_TEXT_VRAM_Y);
        return;
    }
    capReset();
}

void func_actor_450800_80131F70(u32 arg0)
{
    shelterB6NurserySetEffectCues(arg0 >> 16, arg0 & 0xFFFF);
}

/// Starts the CAP sequence for a repeat nursery interaction.
///
/// Requires the nursery CAP resource to be selected. Selector 1 uses the low
/// half of the interaction counter plus 2; other selectors use sequence 8
/// when the saved scene count is 2, and sequence 9 otherwise. Starts ordinary
/// playback with variant key 0; the CAP start result is ignored.
static void _actor450800StartNurseryRepeatDialogue(s32 dialogueSelector)
{
    enum {
        ACTOR_450800_DIALOGUE_FROM_INTERACTION_COUNT = 1,
        ACTOR_450800_INTERACTION_SEQUENCE_BASE       = 2,
        ACTOR_450800_SECOND_REPEAT_SCENE_COUNT       = 2,
        ACTOR_450800_SECOND_REPEAT_SEQUENCE          = 8,
        ACTOR_450800_LATER_REPEAT_SEQUENCE           = 9,
    };
    s16 sequenceIndex;

    // Separate call sites preserve the two arms' argument setup in the binary.
    if (dialogueSelector == ACTOR_450800_DIALOGUE_FROM_INTERACTION_COUNT) {
        sequenceIndex = (u16)D_actor_450800_8013930C + ACTOR_450800_INTERACTION_SEQUENCE_BASE;
        capStartSequenceSlot(sequenceIndex, 0, 0);
    } else {
        if (gameFlagGetNibble(GAME_FLAG_B6_NURSERY_SCENE_COUNT) == ACTOR_450800_SECOND_REPEAT_SCENE_COUNT) {
            sequenceIndex = ACTOR_450800_SECOND_REPEAT_SEQUENCE;
        } else {
            sequenceIndex = ACTOR_450800_LATER_REPEAT_SEQUENCE;
        }
        capStartSequenceSlot(sequenceIndex, 0, 0);
    }
}

void actor450800StartNurseryCompanionDialogue(void)
{
    evsStartScript(D_actor_450800_8013A564, EVENT_SCRIPT_HUD_HIDE_RESTORE);
}

void actor450800PrepareNurseryKyleMadigan(void)
{
    enum { ACTOR_450800_NURSERY_KYLE_SCENE_ACTOR = 0 };

    TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(ACTOR_450800_NURSERY_KYLE_SCENE_ACTOR), ACTOR_MESSAGE_PLAY_ANIMATION, &D_actor_450800_801397A4, 0);
    TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(ACTOR_450800_NURSERY_KYLE_SCENE_ACTOR), ACTOR_MESSAGE_PLACE, &D_actor_450800_801398EC, 0);
}

/// Restarts play in growth room 1 through arrival 1 after the nursery scene.
///
/// Attract demo scene 9 suppresses the restart. Otherwise updates the live
/// save's destination and selects sprite variant 1 before spawning the
/// resident session-restart task; its spawn result is ignored.
static void _actor450800EnterGrowthRoom(void)
{
    enum {
        ACTOR_450800_NURSERY_DEMO_SCENE    = 9,
        ACTOR_450800_GROWTH_ROOM_ARRIVAL   = 1,
        ACTOR_450800_GROWTH_ROOM           = 1,
        ACTOR_450800_GROWTH_SPRITE_VARIANT = 1,
        ACTOR_450800_SESSION_TASK_BANK     = GAME_FLOW_RELOAD_TASK_BANK,
        ACTOR_450800_SESSION_RESTART_TASK  = GAME_FLOW_RELOAD_TASK_SLOT,
    };

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene != ACTOR_450800_NURSERY_DEMO_SCENE) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage = GAME_STAGE_SHELTER_NEO_ARK;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area  = GAME_AREA_SHELTER_B6_GROWTH_ROOM;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp  = ACTOR_450800_GROWTH_ROOM_ARRIVAL;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room  = ACTOR_450800_GROWTH_ROOM;
        gDisplayState.spriteVariant                                 = ACTOR_450800_GROWTH_SPRITE_VARIANT;
        taskSpawn(ACTOR_450800_SESSION_TASK_BANK, ACTOR_450800_SESSION_RESTART_TASK, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
    }
}

/// Forwards the script argument's low byte to the nursery's view-13 sprite visibility.
///
/// Requires the nursery sprite tables to be loaded. Low byte 0 shows the
/// sprite and 1 hides it; other low-byte values leave visibility unchanged.
static void _actor450800SetNurseryView13SpriteHidden(s32 hidden)
{
    shelterB6NurserySetView13SpriteHidden(hidden & 0xFF);
}

/// Spawns the nursery scene's world-space impact flash and six sparks.
///
/// Placement is copied synchronously from a stack vector. The factory also
/// records that vector's address, but the impact task never reads it after
/// spawning; it uses the resulting coordinate. The flash's depth-scaled size
/// is 512. Spawn failure is ignored.
static void _actor450800SpawnNurseryImpactSpark(void)
{
    enum { ACTOR_450800_NURSERY_IMPACT_FLASH_SIZE = 512 };
    SVECTOR worldPosition;

    worldPosition = D_actor_450800_80131E24;
    effectSpawn(EFFECT_IMPACT_SPARK, NULL, ACTOR_450800_NURSERY_IMPACT_FLASH_SIZE, &worldPosition);
}

/// Gives a freshly spawned helper model the texture page and palette of the
/// actor's placement in the current area, then streams it twice.
static inline void _actor450800TintModel(Task* spawned, Task* actor)
{
    GameLocationKey  key;
    GameLocationKey* sessionKey;
    AreaPlacement*   entry;
    TmdObject*       model;
    u32              idx;

    sessionKey = &gGameSession->location.loc;
    idx        = ((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
    model      = spawned->extra.tmd;
    key.stage  = sessionKey->stage;
    key.area   = sessionKey->area;
    key.room   = sessionKey->room;
    key.view   = gGameSession->location.loc.view;
    areaSyncLocationVariant(&key);
    entry                    = gpAreaPlaceAt(areaGetVariant(&key)->placements, idx);
    model->texturePageOffset = entry->texturePageOffset;
    model->clutRowOffset     = entry->clutRowOffset;
    if (model->buffer != NULL) {
        tmdBuildBufferHalf(model);
        tmdBuildBufferHalf(model);
    }
}

/// Spawn handler of the actor's own task, state 0 of the `fns` table
/// `func_actor_450800_80132790` dispatches through. Builds the actor's
/// `_Actor450800KyleMadiganWork` block, hangs its leading matrices off the
/// model's `lightMtx` / `colorMtx`, and starts the animation.
///
/// The three helper tasks come out of `D_actor_450800_8014AC88`: 1 and 2 are
/// the hands, textured from the placement the actor's `Task::spawnArg2` enemy
/// selects. Task 4, the gun, is spawned but not textured.
static void func_actor_450800_80132160(Enemy* enemy, Task* task)
{
    VECTOR                       vec;
    GfxCoord*                    coord;
    TmdObject*                   obj;
    _Actor450800KyleMadiganWork* work;
    Task*                        spawned;

    obj        = task->extra.tmd;
    coord      = obj->coords;
    work       = memCalloc(sizeof(_Actor450800KyleMadiganWork), false);
    task->work = work;
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->exitCallback               = _actor450800ExitKyleMadigan;
    coord->parent                    = &gGfxViewCoord;
    enemy->field_4                   = &coord->coord;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = 0;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    if ((s16)(task->spawnArg1.value >> 16) == 1) {
        obj->flags = 0;
    }
    obj->otOffset = 1;
    obj->lightMtx = &work->light;
    obj->colorMtx = &work->color;
    vec.vx        = coord->workm.t[0];
    vec.vy        = coord->workm.t[1] - 0x320;
    vec.vz        = coord->workm.t[2];
    worldCoordSetModelLighting(obj, &vec, 0, 3);
    animationInitContext(&work->rig.anim, (AnimationSet**)D_actor_450800_8014ACC4, obj, work->rig.poses,
                         work->rig.slots);
    work->st.animId = 1;
    work->st.state  = ACTOR_ENEMY_ANIM_RESET;

    spawned = taskSpawnFromTable(D_actor_450800_8014AC88, 1, 8, 0);
    if (spawned != NULL) {
        work->handLeftTask = spawned;
        spawned->parent    = task;
        _actor450800TintModel(spawned, task);
    }

    spawned = taskSpawnFromTable(D_actor_450800_8014AC88, 2, 0xC, 0);
    if (spawned != NULL) {
        work->handRightTask = spawned;
        spawned->parent     = task;
        _actor450800TintModel(spawned, task);
    }

    spawned = taskSpawnFromTable(D_actor_450800_8014AC88, 4, 8, 0);
    if (spawned != NULL) {
        spawned->parent = task;
        work->gunTask   = spawned;
    }

    work->blendFrames = 8;
    work->st.travel   = 0;
    work->turnFrames  = 0;
    work->gunShown    = 0;
    task->msgTable    = D_actor_450800_8014AC58;
    _actor450800UpdateKyleMadigan(task);
    task->state++;
}

/// Advances Kyle's scheduled turn by 51/4096 of a turn and consumes one update.
///
/// Borrows writable work and its live model root. The caller must select the
/// turn clip and check the nonzero countdown; this helper does neither.
/// Narrows the new heading to signed 16 bits before applying it in the root's
/// parent space. Replaces rotation at unit scale, retains translation and
/// marks composition dirty without composing the root.
/// Requires an initialized scratch stack with 0x24 free word-aligned bytes
/// disjoint from both arguments; the rotation releases them before returning.
static inline void _actor450800StepKyleMadiganTurn(_Actor450800KyleMadiganWork* work, GfxCoord* rootCoord)
{
    enum { ACTOR_450800_KYLE_TURN_ANGLE_STEP = 51 };

    work->st.yaw += ACTOR_450800_KYLE_TURN_ANGLE_STEP;
    gfxRotMatrixY(&rootCoord->coord, work->st.yaw, GRAPHICS_ROTATION_REPLACE);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    work->turnFrames--;
}

/// Reseeds Kyle's animation or advances one frame of his scripted travel and turn.
///
/// Requires a live TMD task with `_Actor450800KyleMadiganWork` and a loaded
/// twenty-slot rig. Reset and blend requests reseed slots 1..19 and enter
/// normal ticking without consuming travel. In ticking state, clips 2, 14
/// and 15 consume one remaining travel frame, even when actor freezing
/// suppresses movement. Fast, backward and slow strides are 60, -15 and 25
/// parent-coordinate units; other modes count frames without moving.
/// Arrival queues idle clip 13 with a ten-frame blend for the next update.
///
/// Clip 3 advances a nonzero turn countdown by 51/4096 of a turn per frame.
/// This package never seeds that countdown. Slot ticking follows movement
/// and turning; scratch/GTE requirements follow the movement and slot helpers.
static void _actor450800UpdateKyleMadigan(Task* task)
{
    enum {
        ACTOR_450800_KYLE_ANIM_TRAVEL_2     = 2,
        ACTOR_450800_KYLE_ANIM_TURN         = 3,
        ACTOR_450800_KYLE_ANIM_IDLE         = 13,
        ACTOR_450800_KYLE_ANIM_TRAVEL_14    = 14,
        ACTOR_450800_KYLE_ANIM_TRAVEL_15    = 15,
        ACTOR_450800_KYLE_IDLE_BLEND_FRAMES = 10,
    };
    GfxCoord*                    rootCoord = task->extra.tmd->coords;
    _Actor450800KyleMadiganWork* work      = task->work;

    // A reseed occupies the frame; ordinary playback moves only in ticking state.
    if (work->st.state == ACTOR_ENEMY_ANIM_BLEND) {
        PACED_WALK_BLEND_ANIM(task);
        work->st.state = ACTOR_ENEMY_ANIM_TICK;
    } else if (work->st.state == ACTOR_ENEMY_ANIM_RESET) {
        PACED_WALK_RESET_ANIM(task);
        work->st.state = ACTOR_ENEMY_ANIM_TICK;
    } else if (work->st.state == ACTOR_ENEMY_ANIM_TICK) {
        if (work->st.animId == ACTOR_450800_KYLE_ANIM_TRAVEL_14 || work->st.animId == ACTOR_450800_KYLE_ANIM_TRAVEL_2 || work->st.animId == ACTOR_450800_KYLE_ANIM_TRAVEL_15) {
            if (work->st.travel != 0) {
                switch (work->walkMode) {
                    case ACTOR_450800_WALK_FAST:
                        _actorMovementStepModelForward(task, ACTOR_450800_KYLE_FAST_STRIDE);
                        break;
                    case ACTOR_450800_WALK_BACKWARD:
                        _actorMovementStepModelForward(task, -ACTOR_450800_KYLE_BACKWARD_STRIDE);
                        break;
                    case ACTOR_450800_WALK_SLOW:
                        _actorMovementStepModelForward(task, ACTOR_450800_KYLE_SLOW_STRIDE);
                        break;
                }
                if (--work->st.travel == 0) {
                    work->st.state    = ACTOR_ENEMY_ANIM_BLEND;
                    work->blendFrames = ACTOR_450800_KYLE_IDLE_BLEND_FRAMES;
                    work->st.animId   = ACTOR_450800_KYLE_ANIM_IDLE;
                }
            }
        }
        if (work->st.animId == ACTOR_450800_KYLE_ANIM_TURN && work->turnFrames != 0) {
            _actor450800StepKyleMadiganTurn(work, rootCoord);
        }
        _pacedWalkTickAnim(task);
    }
}

void func_actor_450800_80132790(Task* task)
{
    EnemyTaskFunc fns[2] = { func_actor_450800_80132160, _actorRenderWalkerFrame };

    fns[task->state](task->spawnArg2.pointer, task);
}

/// Selects this carrier's private walker frame state for one fragment inclusion.
///
/// Bind to a static void(Enemy*, Task*) function declared in the prologue.
/// This identifier alias evaluates no arguments; undefine after the fragment.
#define ACTOR_RENDER_WALKER_FRAME _actorRenderWalkerFrame
/// Selects this frame instance's motion and animation update.
///
/// Bind to a declared static void(Task*) function for the same task and work.
/// The frame calls it once after lighting and before drawing the shadow.
/// This object-like identifier alias captures no locals or constructed tokens;
/// undefine it after each inclusion of walker_frame.inc.c.
#define ACTOR_RENDER_UPDATE_WALKER _actor450800UpdateKyleMadigan
/// Selects the declared static void(Task*) ground-shadow drawer for this inclusion.
#define ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW _actorRenderDrawWalkerGroundShadow
#include "../../shared/walker_frame.inc.c"
#undef ACTOR_RENDER_WALKER_FRAME
#undef ACTOR_RENDER_UPDATE_WALKER
#undef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW

/// Releases Kyle's enemy and actor task, then kills the two hands and gun tasks.
///
/// Requires a live enemy at `spawnArg2.pointer` and all three helper tasks.
/// The retained teardown order reads the helper pointers from the freed
/// `_Actor450800KyleMadiganWork`; those reads depend on its released storage
/// remaining intact until the helper kills finish.
static void _actor450800ExitKyleMadigan(Task* task)
{
    _Actor450800KyleMadiganWork* work = task->work;

    // The binary frees actor work before reading its three helper pointers.
    enemyDestroy(task->spawnArg2.pointer, task);
    taskKill(work->handLeftTask);
    taskKill(work->handRightTask);
    taskKill(work->gunTask);
}

/// Names this carrier's private room-shaded shadow function for one inclusion.
///
/// Bind to the prologue's `static void name(Task* task)` declaration; the
/// replacement is one identifier and evaluates no arguments or object state.
#define ACTOR_RENDER_DRAW_ROOM_GROUND_SHADOW _actorRenderDrawWalkerGroundShadow
#include "../../shared/actor_render_walker_shadow.inc.c"
#undef ACTOR_RENDER_DRAW_ROOM_GROUND_SHADOW

/// Attaches Kyle's hand or gun model to a body part and refreshes its room lighting.
///
/// Requires a live parent TMD task and a valid parent coordinate index in
/// `spawnArg1`. This package supplies part 8 for the left hand and gun, and
/// part 12 for the right hand. Initialization borrows that coordinate until
/// teardown; parent spawn variant 1 clears the attachment's draw flags.
/// Later frames sample lighting 800 world-coordinate units above the body's
/// root, rather than at the selected attachment part. The parent's composed
/// root transform must be current before those frames run.
static void _actor450800KyleMadiganAttachmentTask(Task* task)
{
    enum {
        ACTOR_450800_ATTACHMENT_INITIALIZE                = 0,
        ACTOR_450800_ATTACHMENT_LIGHT                     = 1,
        ACTOR_450800_ATTACHMENT_CLEAR_FLAGS_SPAWN_VARIANT = 1,
        ACTOR_450800_ATTACHMENT_LIGHT_HEIGHT              = 800,
        ACTOR_450800_ATTACHMENT_LIGHT_COUNT               = 3,
    };
    TmdObject* model           = task->extra.tmd;
    GfxCoord*  rootCoord       = model->coords;
    GfxCoord*  bodyCoords      = task->parent->extra.tmd->coords;
    GfxCoord*  attachmentCoord = bodyCoords + task->spawnArg1.value;
    VECTOR     lightSamplePosition;

    switch (task->state) {
        case ACTOR_450800_ATTACHMENT_INITIALIZE:
            rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            if ((s16)(task->parent->spawnArg1.value >> 16) == ACTOR_450800_ATTACHMENT_CLEAR_FLAGS_SPAWN_VARIANT) {
                model->flags = 0;
            }
            rootCoord->parent = attachmentCoord;
            task->state++;
            break;
        case ACTOR_450800_ATTACHMENT_LIGHT:
            lightSamplePosition.vx = bodyCoords->workm.t[0];
            lightSamplePosition.vy = bodyCoords->workm.t[1] - ACTOR_450800_ATTACHMENT_LIGHT_HEIGHT;
            lightSamplePosition.vz = bodyCoords->workm.t[2];
            worldCoordSetModelLighting(model, &lightSamplePosition, 0, ACTOR_450800_ATTACHMENT_LIGHT_COUNT);
            break;
    }
}

#include "../../shared/paced_walk_tick_anim.inc.c"

#include "../../shared/paced_walk_reset_anim.inc.c"

#include "../../shared/paced_walk_blend_anim.inc.c"

/// Records Kyle's animation reseed request without advancing his playing slots.
///
/// Both pointers are borrowed for the call. The caller must supply a loaded
/// clip; the ID and blend duration narrow to signed halfwords. Reset leaves
/// the prior duration intact. Clears `st.field_6`, whose role is unproven.
static inline void _actor450800ApplyKyleMadiganAnimationRequest(_Actor450800KyleMadiganWork* work, const AnimationPlayRequest* request)
{
    work->st.animId = request->animationId;
    if (request->blend != ANIMATION_BLEND_RESET) {
        work->st.state    = ACTOR_ENEMY_ANIM_BLEND;
        work->blendFrames = request->blendFrames;
    } else {
        work->st.state = ACTOR_ENEMY_ANIM_RESET;
    }
    work->st.field_6 = 0;
}

/// Immediately reseeds Kyle's non-root animation tracks from a scripted request.
///
/// Requires a live `_Actor450800KyleMadiganWork` and its loaded clips 1..30.
/// The signed check rejects only IDs >=31; callers must exclude negative IDs
/// and unloaded ID 0. Borrows the aligned request through dispatch without
/// retaining it. Nonzero blend selects a buffered blend, narrowing its whole
/// frame duration to a signed halfword; 0..2047 avoids a negative slot countdown.
/// Reset ignores the duration. Reseeds slots 1..19, leaving the root and travel
/// count unchanged. The bank selector, collision choice, message ID and last
/// argument are ignored. Returns 0 after reseeding or -1 on rejection.
static s32 _actor450800PlayKyleMadiganAnimation(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArgument)
{
    enum {
        ACTOR_450800_KYLE_ANIMATION_ID_LIMIT         = 31,
        ACTOR_450800_KYLE_ANIMATION_REQUEST_APPLIED  = 0,
        ACTOR_450800_KYLE_ANIMATION_REQUEST_REJECTED = -1,
    };
    _Actor450800KyleMadiganWork* work;

    work = task->work;
    if (request->animationId < ACTOR_450800_KYLE_ANIMATION_ID_LIMIT) {
        _actor450800ApplyKyleMadiganAnimationRequest(work, request);
        _actor450800UpdateKyleMadigan(task);
        return ACTOR_450800_KYLE_ANIMATION_REQUEST_APPLIED;
    }
    return ACTOR_450800_KYLE_ANIMATION_REQUEST_REJECTED;
}

/// Replaces draw flags on Kyle's body, hands and gun, retaining the gun's hidden latch.
///
/// Requires all three helper tasks and their models to be live. Bit 0 clears
/// all model flags; without it, flags become active-draw exclusion. Bit 1 adds
/// automatic-buffer exclusion; other request bits are ignored. A zero
/// `gunShown` forces both exclusions on the gun regardless of the request.
/// No buffers are allocated or freed. Message ID and last argument are ignored.
/// Returns 0.
static s32 _actor450800SetKyleMadiganDrawFlags(Task* task, s32 messageId, s32 flags, s32 unusedArgument)
{
    _Actor450800KyleMadiganWork* work;
    TmdObject*                   self;
    TmdObject*                   handLeft;
    TmdObject*                   handRight;
    TmdObject*                   gun;

    work      = task->work;
    self      = task->extra.tmd;
    handLeft  = work->handLeftTask->extra.tmd;
    handRight = work->handRightTask->extra.tmd;
    gun       = work->gunTask->extra.tmd;

    if (flags & ACTOR_MESSAGE_PAIR_SHOW) {
        self->flags      = 0;
        handLeft->flags  = 0;
        handRight->flags = 0;
        gun->flags       = 0;
    } else {
        self->flags      = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        handLeft->flags  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        handRight->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
        gun->flags       = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (flags & ACTOR_MESSAGE_PAIR_SKIP_AUTO_BUFFER) {
        self->flags      |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        handLeft->flags  |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        handRight->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        gun->flags       |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    if (work->gunShown == 0) {
        gun->flags = (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
    }
    return 0;
}

#include "../../shared/paced_walk_place.inc.c"

/// Message handler 0x7DB of `D_actor_450800_8014AC58`: recolour this actor's
/// body (or spawn its 0x6002B burst) according to the message's selector.
///
/// The model is the actor's own -- `task->extra.tmd`, the `TmdObject` a bodyKind-1
/// task carries -- and the one it is driven through is that of the gun task
/// in `_Actor450800KyleMadiganWork::gunTask`. Both pointers, and `field_8` of
/// the gun's model, are resolved before the switch: the ROM reads them there,
/// and a scheduler pass cannot lift the loads into the entry block on its own.
s32 func_actor_450800_80132CE0(Task* task, s32 arg1, ActorCommand* msg, s32 arg3)
{
    _Actor450800KyleMadiganWork* work  = task->work;
    TmdObject*                   obj   = work->gunTask->extra.tmd;
    GfxCoord*                    coord = obj->coords;
    TmdObject*                   self  = task->extra.tmd;
    s32                          mode  = msg->command;

    switch (mode) {
        case 0:
            effectSpawn(EFFECT_HANDGUN_MUZZLE_FLASH, coord, 0x21, 0);
            break;
        case 1:
            work->gunShown = mode;
            obj->flags     = self->flags;
            break;
        case 2:
            work->gunShown = 0;
            obj->flags     = (TMD_OBJECT_SKIP_ACTIVE_DRAW | TMD_OBJECT_SKIP_AUTO_BUFFER);
            break;
    }
    return 0;
}

/// Aims Kyle at a borrowed target and records the remaining travel frames.
///
/// Requires a live TMD model and `_Actor450800KyleMadiganWork`. Reads only X
/// and Z of the aligned target, in the model root parent's coordinate units;
/// retains no pointer. Mode narrows to a signed halfword: 0 faces forward
/// with stride 60, 1 faces away with backward stride 15, and 2 faces forward
/// with stride 25. Other
/// modes record the slow-stride count but the update makes no translation.
/// Angles use 4096 units per turn and narrow to signed halfwords. The XZ
/// distance divided by stride truncates to whole frames and narrows to a
/// signed halfword; callers must keep the squared-distance arithmetic in range
/// and the resulting count in 0..32767. Does not start a travel clip or dirty
/// composition. The frame driver composes and draws the model separately.
/// Message ID is ignored. Returns 0.
static s32 _actor450800SetKyleMadiganWalkTarget(Task* task, s32 messageId, const VECTOR* target, s32 mode)
{
    _Actor450800KyleMadiganWork* work;
    GfxCoord*                    coord;
    s32                          deltaX;
    s32                          deltaZ;
    s32                          stride;
    s32                          distance;
    s32                          yaw;

    coord  = task->extra.tmd->coords;
    work   = task->work;
    deltaX = target->vx - coord->coord.t[0];
    deltaZ = target->vz - coord->coord.t[2];
    // Keep this store after the differences to preserve entry-block scheduling.
    work->walkMode = mode;
    yaw            = ratan2(deltaX, deltaZ);
    work->st.yaw   = yaw;
    if (work->walkMode == ACTOR_450800_WALK_BACKWARD) {
        work->st.yaw = yaw + ACTOR_TRANSFORM_ANGLE_HALF_TURN;
    }
    gfxRotMatrixY(&coord->coord, work->st.yaw, GRAPHICS_ROTATION_REPLACE);
    distance = SquareRoot0(deltaX * deltaX + deltaZ * deltaZ);
    stride   = ACTOR_450800_KYLE_SLOW_STRIDE;
    switch (work->walkMode) {
        case ACTOR_450800_WALK_FAST:
            stride = ACTOR_450800_KYLE_FAST_STRIDE;
            break;
        case ACTOR_450800_WALK_BACKWARD:
            stride = ACTOR_450800_KYLE_BACKWARD_STRIDE;
            break;
        case ACTOR_450800_WALK_SLOW:
            break;
    }
    work->st.travel = distance / stride;
    return 0;
}

#include "../../shared/pair_walk_spawn.inc.c"

#include "../../shared/pair_walk_update_model.inc.c"

void func_actor_450800_80133264(Task* task)
{
    EnemyTaskFunc fns[2] = { pairWalkSpawn, _actorRenderWalkerFrameSecond };

    fns[task->state](task->spawnArg2.pointer, task);
}

/// Selects this carrier's private walker frame state for one fragment inclusion.
///
/// Bind to a static void(Enemy*, Task*) function declared in the prologue.
/// This identifier alias evaluates no arguments; undefine after the fragment.
#define ACTOR_RENDER_WALKER_FRAME _actorRenderWalkerFrameSecond
/// Selects this frame instance's motion and animation update.
///
/// Bind to a declared static void(Task*) function for the same task and work.
/// The frame calls it once after lighting and before drawing the shadow.
/// This object-like identifier alias captures no locals or constructed tokens;
/// undefine it after each inclusion of walker_frame.inc.c.
#define ACTOR_RENDER_UPDATE_WALKER             _pairWalkUpdate
#define ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW _actorRenderDrawSecondWalkerGroundShadow
#include "../../shared/walker_frame.inc.c"
#undef ACTOR_RENDER_WALKER_FRAME
#undef ACTOR_RENDER_UPDATE_WALKER
#undef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW

#include "../../shared/pair_walk_exit.inc.c"

/// Selects this overlay's private second-walker ground-shadow drawer.
///
/// Bind to the prologue's `static void name(Task* task)` declaration; the
/// following fragment defines it. This identifier alias lasts one inclusion.
#define ACTOR_RENDER_DRAW_ROOM_GROUND_SHADOW _actorRenderDrawSecondWalkerGroundShadow
#include "../../shared/actor_render_walker_shadow.inc.c"
#undef ACTOR_RENDER_DRAW_ROOM_GROUND_SHADOW

#include "../../shared/pair_walk_tick_anim.inc.c"

#include "../../shared/pair_walk_reset_anim.inc.c"

#include "../../shared/pair_walk_reseed_anim.inc.c"

#include "../../shared/pair_walk_play.inc.c"

#include "../../shared/pair_walk_visibility.inc.c"

#include "../../shared/pair_walk_place.inc.c"

/// Ignores every actor-command message delivered to the package's pawn golem.
///
/// The receiver and both payloads are unused; no command storage is read or
/// retained. Returns 0 without changing the golem or its carried model.
static s32 _actor450800IgnorePawnGolemCommand(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArgument)
{
    return 0;
}

#include "../../shared/pair_walk_to.inc.c"

#include "../../shared/pair_walk_sub_model.inc.c"
