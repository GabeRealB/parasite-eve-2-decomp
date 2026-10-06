#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/captions.h"
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
#include "../../shared/walker.h"
#include "../../shared/pair_walk.h"

static s32  _pairWalkPlay(Task* task, s32 messageId, const AnimationPlayRequest* request, s32 unusedArg);
static s32  _pairWalkTo(Task* task, s32 messageId, const ActorTransform* target, s32 unusedArg);
static void _pairWalkSubModelTask(Task* task);

/// The clips the package's event scripts add to the player's animation bank,
/// with the first three player play requests stored after them.
///
/// A script sends the player the copy request for this storage before it plays
/// any of these clips. The copy takes `ANIMATION_BANK_EXTENSION_CAPACITY` words,
/// which is more than the clip table holds: the seventeen set pointers occupy
/// extended ids 47-63, and the three play requests fill the fifteen words of
/// the bank's extension that remain, so the copied span is exactly this
/// storage. The requests the scripts play on the player select base id 1 and
/// extended ids 47-63 only, so none of the request words is played as a clip.
///
/// The play requests are the first three of the run the package keeps for the
/// player and are part of this object only because the copied span reaches
/// over them; the rest of the run follows as separate objects. The storage is
/// only read: a script resolves the player's weapon bank in its own copy of a
/// request before it dispatches it.
typedef union {
    struct {
        AnimationSet*        sets[17];            // Player clips for extended ids 47-63
        AnimationPlayRequest playRequests[3];     // Requests for extended ids 47, 47 and 48; nothing references the first
    } data;                                       // The records by name
    s32 words[ANIMATION_BANK_EXTENSION_CAPACITY]; // The same storage as the copy reads it
} _Actor450800PlayerAnimationBankExtensionStorage;
STATIC_ASSERT_SIZEOF(_Actor450800PlayerAnimationBankExtensionStorage, 128);

extern _Actor450800PlayerAnimationBankExtensionStorage D_actor_450800_80139310;

/// The clips the package's event scripts add to the companion's animation
/// bank, with the first five companion play requests stored after them.
///
/// A script sends the companion the copy request for this storage before it
/// plays any of these clips. The copy takes `ANIMATION_BANK_EXTENSION_CAPACITY`
/// words, which is more than the clip table holds: the eleven set pointers
/// occupy extended ids 47-57, and the first 21 words of the play requests -
/// four whole requests and the bank selector of the fifth - are written into
/// the bank after them. The requests the scripts play on the companion select
/// base id 1 and extended ids 47-57 only, so none of the request words is
/// played as a clip.
///
/// The play requests are the first five of the run the package keeps for the
/// companion and are part of this object only because the copied span reaches
/// into the fifth; the rest of the run follows as separate objects. The storage
/// is only read: a script resolves the companion's bank in its own copy of a
/// request before it dispatches it.
typedef union {
    struct {
        AnimationSet*        sets[11];        // Companion clips for extended ids 47-57
        AnimationPlayRequest playRequests[5]; // Requests for extended ids 47, 47, 48, 49 and 50; nothing references the first
    } data;                                   // The records by name
    s32 words[36];                            // The same storage as the copy reads it; the last four words lie beyond the copied span
} _Actor450800CompanionAnimationBankExtensionStorage;
STATIC_ASSERT_SIZEOF(_Actor450800CompanionAnimationBankExtensionStorage, 144);

extern _Actor450800CompanionAnimationBankExtensionStorage D_actor_450800_801394BC;

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

/// Spawn offset `func_actor_450800_80132108` copies into a local and hands to
/// `Gp_SpawnEff` as the effect's position.
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

static void func_actor_450800_80132448(Task* task);
static void func_actor_450800_801327E4(Enemy* enemy, Task* task);
static void func_actor_450800_80132868(Task* task);
static void func_actor_450800_80132AE0(Task* task);
static void func_actor_450800_801332B8(Enemy* enemy, Task* task);
static void func_actor_450800_80133364(Task* task);

static TmdSource _gActor450800Body;
static TmdSource _gActor450800KyleMadiganBody;
static TmdSource _gActor450800KyleMadiganHandRight;
static TmdSource _gActor450800KyleMadiganHandLeft;
static TmdSource _gActor450800KyleMadiganGun;
void             func_actor_450800_80132790(Task*);
void             func_actor_450800_80132958(Task*);

s32 func_actor_450800_80132B44(Task*, s32, AnimationPlayRequest*, s32);
s32 func_actor_450800_80132BB0(Task*, s32, s32, s32);
s32 func_actor_450800_80132CE0(Task* task, s32 msgId, ActorCommand* msg, s32);
s32 func_actor_450800_80132D74(Task*, s32, VECTOR*, s32);

static TmdSource _gActor450800PawnGolemBody;
static TmdSource _gActor450800GolemBeamSword;
s32              func_actor_450800_80133670(Task*, s32, s32, s32);
void             func_actor_450800_80133264(Task*);

extern AnimationPlayRequest D_actor_450800_80139560;
extern AnimationPlayRequest D_actor_450800_80139628;
extern AnimationPlayRequest D_actor_450800_80139894;
extern ActorTransform       D_actor_450800_8013994C;
void                        func_actor_450800_80131F28(s32);
void                        func_actor_450800_80132080(void);

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
void                            func_actor_450800_80131F28(s32);
void                            func_actor_450800_80132080(void);
void                            func_actor_450800_801320E8(s32);
void                            func_actor_450800_80132108(void);

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
void                        func_actor_450800_80131F28(s32);
void                        func_actor_450800_80131F70(u32);
void                        func_actor_450800_80131F98(s32);

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

_Actor450800PlayerAnimationBankExtensionStorage D_actor_450800_80139310 = { .data = { { &_gActor450800Animation01B20, &_gActor450800Animation01DFC, &_gActor450800Animation02070, &_gActor450800Animation022D8, &_gActor450800Animation02538, &_gActor450800Animation02840, &_gActor450800Animation02A04, &_gActor450800Animation02C30, &_gActor450800Animation02DE4, &_gActor450800Animation03348, &_gActor450800Animation0359C, &_gActor450800Animation03954, &_gActor450800Animation03D3C, &_gActor450800Animation04078, &_gActor450800Animation04240, &_gActor450800Animation04680, &_gActor450800Animation074C4 }, { { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE } } } };

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

_Actor450800CompanionAnimationBankExtensionStorage D_actor_450800_801394BC = { .data = { { &_gActor450800Animation048B8, &_gActor450800Animation04C24, &_gActor450800Animation04E48, &_gActor450800Animation04FD8, &_gActor450800Animation05CB4, &_gActor450800Animation05EDC, &_gActor450800Animation061D0, &_gActor450800Animation063E8, &_gActor450800Animation0690C, &_gActor450800Animation06C64, &_gActor450800Animation070F8 }, { { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE } } } };

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

AnimationBankCopyRequest D_actor_450800_801398D0 = { { .words = D_actor_450800_801394BC.words }, ANIMATION_BANK_EXTENSION_CAPACITY };

AnimationBankCopyRequest D_actor_450800_801398D8 = { { .words = D_actor_450800_80139310.words }, ANIMATION_BANK_EXTENSION_CAPACITY };

ActorCommand D_actor_450800_801398E0 = { { .loc = { 5, 22 } }, 0 };

ActorCommand D_actor_450800_801398E4 = { { .loc = { 5, 22 } }, 1 };

ActorCommand D_actor_450800_801398E8 = { { .loc = { 5, 22 } }, 2 };

ActorTransform D_actor_450800_801398EC = { { 5650, 0, 2400, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_450800_80139904 = { { 6080, 0, 900, 0 }, { 0, 853, 0, 0 } };

ActorTransform D_actor_450800_8013991C = { { 5951, 0, 900, 0 }, { 0, 853, 0, 0 } };

ActorTransform D_actor_450800_80139934 = { { 6770, 0, 900, 0 }, { 0, -1137, 0, 0 } };

ActorTransform D_actor_450800_8013994C = { { 6650, 0, 401, 0 }, { 0, -1024, 0, 0 } };

EvsCommand D_actor_450800_80139964[105] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_450800_80131F28 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_80139310.data.playRequests[1] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_801394BC.data.playRequests[1] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_80139310.data.playRequests[2] }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_801394BC.data.playRequests[2] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 25 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_801394BC.data.playRequests[3] }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_801394BC.data.playRequests[4] }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_450800_80131F28 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_450800_80131F28 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_450800_8013A564[12] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_450800_80131F28 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_450800_801398D0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_801398A8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_80139574 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_8013986C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_450800_80131F28 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_450800_8013A684[10] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_450800_80131F28 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_450800_80131F98 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_801398A8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_450800_801396C8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_450800_801396DC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_450800_80131F28 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_450800_8013A774[9] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_450800_80131F28 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_450800_80131F98 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_801398A8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_450800_80139790 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_450800_80131F28 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_450800_80131F28 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_450800_80131F28 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_450800_8013AB7C[16] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_450800_80131F28 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_450800_80131F28 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_450800_8013ACFC[10] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_450800_80131F28 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_450800_80131F98 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450800_801398A8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_450800_801396F0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_450800_80139704 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_450800_80131F28 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_450800_80131F28 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_450800_80132108 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_450800_801320E8 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_450800_80132080 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CANCEL_SECONDARY_FADE, { .value = 0 }, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_450800_80131F28 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_450800_8013BA84[20] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_450800_80132080 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_450800_80131F28 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { ACTOR_MESSAGE_PLAY_ANIMATION, func_actor_450800_80132B44 },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, func_actor_450800_80132BB0 },
    { ACTOR_MESSAGE_PLACE, pacedWalkPlace },
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_450800_80132CE0 },
    { ACTOR_MESSAGE_WALK_TO, func_actor_450800_80132D74 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_actor_450800_8014AC88[5] = {
    { { { TASK_BODY_TMD, 192 } }, func_actor_450800_80132790, { .model = &_gActor450800KyleMadiganBody } },
    { { { TASK_BODY_TMD, 192 } }, func_actor_450800_80132958, { .model = &_gActor450800KyleMadiganHandLeft } },
    { { { TASK_BODY_TMD, 192 } }, func_actor_450800_80132958, { .model = &_gActor450800KyleMadiganHandRight } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 192 } }, func_actor_450800_80132790, { .model = &_gActor450800Body } },
    { { { TASK_BODY_TMD, 192 } }, func_actor_450800_80132958, { .model = &_gActor450800KyleMadiganGun } },
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
    { ACTOR_COMMAND_MESSAGE_APPLY, func_actor_450800_80133670 },
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
void               func_actor_450800_80132000(void);
void               func_actor_450800_80132028(void);
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
                func_800E8614(D_actor_450800_8013A774, 0);
            } else {
                func_800E8614(D_actor_450800_8013A684, 0);
            }
        } else {
            n = gameFlagGetNibble(GAME_FLAG_B6_NURSERY_SCENE_COUNT) + 1;
            if (n >= 4) {
                n = 3;
            }
            gameFlagSetNibble(GAME_FLAG_B6_NURSERY_SCENE_COUNT, n);
            if (n == 1) {
                if (gameFlagGetNibble(GAME_FLAG_083) == n) {
                    func_800E8614(D_actor_450800_8013A984, 0);
                } else {
                    func_800E8614(D_actor_450800_8013AB7C, 0);
                }
                func_800E3FAC(0xA2, 0x32);
            } else {
                func_800E8614(D_actor_450800_8013ACFC, 0);
            }
        }
    }
}

/// Callback the overlay's event scripts name: a non-zero `arg0` clears
/// `Gp_CapFile`, loads capture file 2 and hands 0x340 to `capSetTexturePage`; zero
/// resets the capture state instead.
void func_actor_450800_80131F28(s32 arg0)
{
    if (arg0 != 0) {
        Gp_CapFile = 0;
        Gp_LoadCapFile(2);
        capSetTexturePage(0x340, 0);
        return;
    }
    Gp_ResetCap();
}

void func_actor_450800_80131F70(u32 arg0)
{
    func_shelter_b6_nursery_80182D14(arg0 >> 16, arg0 & 0xFFFF);
}

/// Two call sites, not one: `Gp_StartCapSlot` is written out in both arms of
/// the outer test. The tail-call cross-jump in `jump.c` merges them only from
/// the `jal` onward, because sched2 hoists the `a1`/`a2` setup away from the
/// call in the first arm before that pass runs - which is why the object sets
/// `$a1`/`$a2` twice and shares one `jal`.
///
/// The global is an `s32` (see `func_actor_450800_80131E2C`, which increments
/// it whole), but this arm only wants its low half, which is the `lhu`.
void func_actor_450800_80131F98(s32 arg0)
{
    s16 var_a0;

    if (arg0 == 1) {
        var_a0 = (u16)D_actor_450800_8013930C + 2;
        Gp_StartCapSlot(var_a0, 0, 0);
    } else {
        if (gameFlagGetNibble(GAME_FLAG_B6_NURSERY_SCENE_COUNT) == 2) {
            var_a0 = 8;
        } else {
            var_a0 = 9;
        }
        Gp_StartCapSlot(var_a0, 0, 0);
    }
}

void func_actor_450800_80132000(void)
{
    func_800E8614(D_actor_450800_8013A564, 0);
}

void func_actor_450800_80132028(void)
{
    TASK_MESSAGE_DISPATCH_POINTER(Gp_LookupSlot4(0), 0x7D3, &D_actor_450800_801397A4, 0);
    TASK_MESSAGE_DISPATCH_POINTER(Gp_LookupSlot4(0), 0x7D4, &D_actor_450800_801398EC, 0);
}

void func_actor_450800_80132080(void)
{
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene != 9) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.stage = GAME_STAGE_SHELTER_NEO_ARK;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area  = GAME_AREA_SHELTER_B6_GROWTH_ROOM;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp  = 1;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room  = 1;
        gDisplayState.spriteVariant                                 = 1;
        Task_Spawn(0, 0x11, 0, 0);
    }
}

void func_actor_450800_801320E8(s32 arg0)
{
    shelterB6NurserySetView13SpriteHidden(arg0 & 0xFF);
}

void func_actor_450800_80132108(void)
{
    SVECTOR pos;

    pos = D_actor_450800_80131E24;
    Gp_SpawnEff(EFFECT_IMPACT_SPARK, NULL, 0x200, &pos);
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
    entry                    = gpAreaPlaceAt(Gp_GetNestedAreaRec(&key)->placements, idx);
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
    task->exitCallback               = func_actor_450800_80132868;
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
    func_actor_450800_80132448(task);
    task->state++;
}

static void func_actor_450800_80132448(Task* task)
{
    GfxCoord*                    coord = task->extra.tmd->coords;
    _Actor450800KyleMadiganWork* work  = task->work;

    if (work->st.state == ACTOR_ENEMY_ANIM_BLEND) {
        func_actor_450800_80132AE0(task);
        work->st.state = ACTOR_ENEMY_ANIM_TICK;
    } else if (work->st.state == ACTOR_ENEMY_ANIM_RESET) {
        PACED_WALK_RESET_ANIM(task);
        work->st.state = ACTOR_ENEMY_ANIM_TICK;
    } else if (work->st.state == ACTOR_ENEMY_ANIM_TICK) {
        if (work->st.animId == 0xE || work->st.animId == 2 || work->st.animId == 0xF) {
            if (work->st.travel != 0) {
                switch (work->walkMode) {
                    case ACTOR_450800_WALK_FAST:
                        _actorMovementStepModelForward(task, 0x3C);
                        break;
                    case ACTOR_450800_WALK_BACKWARD:
                        _actorMovementStepModelForward(task, -0xF);
                        break;
                    case ACTOR_450800_WALK_SLOW:
                        _actorMovementStepModelForward(task, 0x19);
                        break;
                }
                if (--work->st.travel == 0) {
                    work->st.state    = ACTOR_ENEMY_ANIM_BLEND;
                    work->blendFrames = 0xA;
                    work->st.animId   = 0xD;
                }
            }
        }
        if (work->st.animId == 3 && work->turnFrames != 0) {
            work->st.yaw += 0x33;
            gfxRotMatrixY(&coord->coord, work->st.yaw, 1);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            work->turnFrames--;
        }
        _pacedWalkTickAnim(task);
    }
}

void func_actor_450800_80132790(Task* task)
{
    EnemyTaskFunc fns[2] = { func_actor_450800_80132160, func_actor_450800_801327E4 };

    fns[task->state](task->spawnArg2.pointer, task);
}

#define walkerFrame      func_actor_450800_801327E4
#define walkerUpdate     func_actor_450800_80132448
#define walkerDrawShadow walkerDrawShadowShaded
#include "../../shared/walker_frame.inc.c"
#undef walkerFrame
#undef walkerUpdate
#undef walkerDrawShadow

static void func_actor_450800_80132868(Task* task)
{
    _Actor450800KyleMadiganWork* work = task->work;

    enemyDestroy(task->spawnArg2.pointer, task);
    taskKill(work->handLeftTask);
    taskKill(work->handRightTask);
    taskKill(work->gunTask);
}

#include "../../shared/walker_shadow_shaded.inc.c"

/// State handler of one of the actor's model tasks: the spawn tick hangs this
/// task's own coordinate frame off part `spawnArg1` of the actor's model and
/// every later tick hands that part's world translation, dropped by 0x320 in y,
/// to `worldCoordSetModelLighting` for the part colour matrix. The parts come from
/// `task->parent`, the actor task that spawned this one
/// (`func_actor_450800_80132160`, which also tests the same halfword on itself).
///
/// The model flags are cleared only for spawn variant 1: the high half of the
/// parent's `spawnArg1`.
void func_actor_450800_80132958(Task* task)
{
    TmdObject* extra = task->extra.tmd;
    GfxCoord*  coord = extra->coords;
    GfxCoord*  parts = task->parent->extra.tmd->coords;
    GfxCoord*  part  = parts + task->spawnArg1.value;
    VECTOR     vec;

    switch (task->state) {
        case 0:
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            if ((s16)(task->parent->spawnArg1.value >> 16) == 1) {
                extra->flags = 0;
            }
            coord->parent = part;
            task->state++;
            break;
        case 1:
            vec.vx = parts->workm.t[0];
            vec.vy = parts->workm.t[1] - 0x320;
            vec.vz = parts->workm.t[2];
            worldCoordSetModelLighting(extra, &vec, 0, 3);
            break;
    }
}

#include "../../shared/paced_walk_tick_anim.inc.c"

#include "../../shared/paced_walk_reset_anim.inc.c"

static void func_actor_450800_80132AE0(Task* task)
{
    _Actor450800KyleMadiganWork* work;
    s32                          i;

    work = task->work;
    i    = 1;
    do {
        animationSeekSlotWithBlend(&work->rig.anim, i, work->st.animId, 0, work->blendFrames);
        i++;
    } while (i < 0x14);
    work->st.appliedAnimId = work->st.animId;
}

/// Starts the actor's scripted animation selected by the request.
///
/// Rejects ids 0x1F and above before changing playback state.
/// The blend path carries the requested duration in whole frames.
s32 func_actor_450800_80132B44(Task* task, s32 arg1, AnimationPlayRequest* args, s32 arg3)
{
    _Actor450800KyleMadiganWork* work;

    work = task->work;
    if (args->animationId < 0x1F) {
        work->st.animId = args->animationId;
        if (args->blend != ANIMATION_BLEND_RESET) {
            work->st.state    = ACTOR_ENEMY_ANIM_BLEND;
            work->blendFrames = args->blendFrames;
        } else {
            work->st.state = ACTOR_ENEMY_ANIM_RESET;
        }
        work->st.field_6 = 0;
        func_actor_450800_80132448(task);
        return 0;
    }
    return -1;
}

/// Message handler 0x7D5 of `D_actor_450800_8014AC58`: sets `TmdObject::flags`
/// on this actor's own model and on the hands' and the gun's at once.
///
/// `arg2` bit 0 selects 0 rather than 0x80, and bit 1 ORs 4 in.
/// `_Actor450800KyleMadiganWork::gunShown` overrides the last of them: while
/// it is 0 the gun keeps the 0x84 handler 0x7DB's mode 2 gave it, instead of
/// the flags just computed.
s32 func_actor_450800_80132BB0(Task* task, s32 arg1, s32 arg2, s32 arg3)
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

    if (arg2 & 1) {
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
    if (arg2 & 2) {
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
            Gp_SpawnEff(EFFECT_HANDGUN_MUZZLE_FLASH, coord, 0x21, 0);
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

/// Message handler 0x7DD of `D_actor_450800_8014AC58`, the payload's first two
/// words being the target position: turns the actor's model to face it -- away
/// from it in mode 1 -- and latches the per-step distance over the step count
/// the mode selects, 60 in mode 0, 15 in mode 1 and 25 otherwise. The mode and
/// both results are kept on the work block.
///
/// The mode store sits after the two differences on purpose. Its place in the
/// source sets its RTL uid, and the uid is what the scheduler's ready-list
/// tie-break compares once `-O2` has CSE'd the constant 1 into a register and
/// every candidate carries the same priority; from before them the whole entry
/// block comes out in a different order and on different registers.
s32 func_actor_450800_80132D74(Task* task, s32 arg1, VECTOR* target, s32 mode)
{
    _Actor450800KyleMadiganWork* work;
    GfxCoord*                    coord;
    s32                          dx;
    s32                          dz;
    s32                          steps;
    s32                          dist;
    s32                          angle;

    coord          = task->extra.tmd->coords;
    work           = task->work;
    dx             = target->vx - coord->coord.t[0];
    dz             = target->vz - coord->coord.t[2];
    work->walkMode = mode;
    angle          = ratan2(dx, dz);
    work->st.yaw   = angle;
    if (work->walkMode == ACTOR_450800_WALK_BACKWARD) {
        work->st.yaw = angle + 0x800;
    }
    gfxRotMatrixY(&coord->coord, work->st.yaw, 1);
    dist  = SquareRoot0(dx * dx + dz * dz);
    steps = 0x19;
    switch (work->walkMode) {
        case ACTOR_450800_WALK_FAST:
            steps = 0x3C;
            break;
        case ACTOR_450800_WALK_BACKWARD:
            steps = 0xF;
            break;
        case ACTOR_450800_WALK_SLOW:
            break;
    }
    work->st.travel = dist / steps;
    return 0;
}

#include "../../shared/pair_walk_spawn.inc.c"

#include "../../shared/pair_walk_update_model.inc.c"

void func_actor_450800_80133264(Task* task)
{
    EnemyTaskFunc fns[2] = { pairWalkSpawn, func_actor_450800_801332B8 };

    fns[task->state](task->spawnArg2.pointer, task);
}

#define walkerFrame      func_actor_450800_801332B8
#define walkerUpdate     _pairWalkUpdate
#define walkerDrawShadow func_actor_450800_80133364
#include "../../shared/walker_frame.inc.c"
#undef walkerFrame
#undef walkerUpdate
#undef walkerDrawShadow

/// Exit callback of the enemy's task, set by its spawn handler
/// `pairWalkSpawn`: releases the enemy slot the task was spawned
/// for.
void pairWalkExit(Task* task)
{
    enemyDestroy(task->spawnArg2.pointer, task);
}

/// A further copy of the shadow, under this file's own name.
#define walkerDrawShadowShaded func_actor_450800_80133364
#include "../../shared/walker_shadow_shaded.inc.c"
#undef walkerDrawShadowShaded

#include "../../shared/pair_walk_tick_anim.inc.c"

#include "../../shared/pair_walk_reset_anim.inc.c"

#include "../../shared/pair_walk_reseed_anim.inc.c"

#include "../../shared/pair_walk_play.inc.c"

#include "../../shared/pair_walk_visibility.inc.c"

#include "../../shared/pair_walk_place.inc.c"

s32 func_actor_450800_80133670(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

#include "../../shared/pair_walk_to.inc.c"

#include "../../shared/pair_walk_sub_model.inc.c"
