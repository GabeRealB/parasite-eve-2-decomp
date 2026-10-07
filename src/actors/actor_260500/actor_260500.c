#include "actors/actor_260500.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/captions.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "../../shared/footstep_walk.h"
#include "../../shared/walker.h"

static void _footstepWalkQuietUpdate(Task* task);
static void _footstepWalkQuietResetAnim(void);
static void _footstepWalkQuietBlendAnim(void);
static void _footstepWalkTickAnim(void);
static s32  _footstepWalkPlace(Task* task, s32 messageId, const ActorTransform* placement, s32 unusedArgument);

static FootstepWalkQuietWork* _gFootstepWalkWork;

/// The actor's own task, published by the spawn routine: the play-animation
/// handler runs the update on it and the visibility handler reaches its model.
extern Task* D_actor_260500_80159E50;

static s16 _gFootstepWalkMode;

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */

extern EvsCommand D_actor_260500_8014CBF8[];
extern EvsCommand D_actor_260500_8014D630[];
extern EvsCommand D_actor_260500_8014D7C8[];
extern EvsCommand D_actor_260500_8014D948[];
extern EvsCommand D_actor_260500_8014DAB0[];
extern EvsCommand D_actor_260500_8014DCC0[];
// Handler views preserve the signatures used by this TU. The dispatcher
// transports each argument in a word register.

extern TaskMessageEntry D_actor_260500_80159D80[];
extern u8               D_actor_260500_80159DBC[];

static void _actorRenderWalkerFrame(Enemy* unusedEnemy, Task* task);
static void _footstepWalkExit(Task* task);
static void _footstepWalkQuietSpawn(Enemy* enemy, Task* task);

/// Task-state indices of this package's quiet walker.
enum {
    ACTOR_260500_TASK_SPAWN       = 0,
    ACTOR_260500_TASK_FRAME       = 1,
    ACTOR_260500_TASK_STATE_COUNT = 2
};

static TmdSource _gActor260500JodieBouquetBody2;
static void      _actor260500Task(Task* task);

static s32 _footstepWalkQuietPlayAnimation(Task* unusedTask, s32 messageId, const AnimationPlayRequest* request, s32 unusedArgument);
static s32 _footstepWalkSetModelDraw(Task* unusedTask, s32 messageId, s32 drawMode, s32 unusedArgument);
static s32 _footstepWalkApplyTurnCommand(Task* unusedTask, s32 messageId, const ActorCommand* command, s32 unusedArgument);
static s32 _footstepWalkSetWalkTarget(Task* task, s32 messageId, const VECTOR* target, s32 mode);

extern AnimationPlayRequest D_actor_260500_8014C874;
extern AnimationPlayRequest D_actor_260500_8014C888;
extern AnimationPlayRequest D_actor_260500_8014C89C;
extern AnimationPlayRequest D_actor_260500_8014C8B0;
extern AnimationPlayRequest D_actor_260500_8014C8C4;
extern AnimationPlayRequest D_actor_260500_8014C8D8;
extern AnimationPlayRequest D_actor_260500_8014C8EC;
extern AnimationPlayRequest D_actor_260500_8014C900;
extern AnimationPlayRequest D_actor_260500_8014C914;
extern AnimationPlayRequest D_actor_260500_8014C928;
extern AnimationPlayRequest D_actor_260500_8014C93C;
extern AnimationPlayRequest D_actor_260500_8014C950;
extern AnimationPlayRequest D_actor_260500_8014C964;
extern AnimationPlayRequest D_actor_260500_8014C978;
extern AnimationPlayRequest D_actor_260500_8014C98C;
extern AnimationPlayRequest D_actor_260500_8014C9A0;
extern AnimationPlayRequest D_actor_260500_8014C9B4;
extern AnimationPlayRequest D_actor_260500_8014C9C8;
extern AnimationPlayRequest D_actor_260500_8014C9DC;
extern AnimationPlayRequest D_actor_260500_8014C9F0;
extern AnimationPlayRequest D_actor_260500_8014CA04;
extern AnimationPlayRequest D_actor_260500_8014CA18;
extern AnimationPlayRequest D_actor_260500_8014CA2C;
extern AnimationPlayRequest D_actor_260500_8014CA54;
extern AnimationPlayRequest D_actor_260500_8014CA68;
extern AnimationPlayRequest D_actor_260500_8014CA7C;
extern AnimationPlayRequest D_actor_260500_8014CA90;
extern AnimationPlayRequest D_actor_260500_8014CAA4;
extern AnimationPlayRequest D_actor_260500_8014CAB8;
extern AnimationPlayRequest D_actor_260500_8014CACC;
extern AnimationPlayRequest D_actor_260500_8014CAE0;
static void                 _actor260500SelectConversationCaptions(s32 useConversationFile);

static AnimationSet _gActor260500Animation01024;
static AnimationSet _gActor260500Animation01314;
static AnimationSet _gActor260500Animation01A10;
static AnimationSet _gActor260500Animation01CAC;
static AnimationSet _gActor260500Animation02064;
static AnimationSet _gActor260500Animation023E8;
static AnimationSet _gActor260500Animation026EC;
static AnimationSet _gActor260500Animation02A18;

static AnimationPackedPose _gActor260500Animation01024Bank1[3] = {
#include "assets/actor_260500_animation_01024_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation01024Bank4[81] = {
#include "assets/actor_260500_animation_01024_bank4.inc"
};

static AnimationRecord _gActor260500Animation01024Records[163] = {
#include "assets/actor_260500_animation_01024_records.inc"
};

static u16 _gActor260500Animation01024Indices[20] = {
#include "assets/actor_260500_animation_01024_indices.inc"
};

static AnimationSet _gActor260500Animation01024 = {
    _gActor260500Animation01024Records,
    _gActor260500Animation01024Indices,
    { NULL, _gActor260500Animation01024Bank1, NULL, NULL, _gActor260500Animation01024Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation01314Bank1[4] = {
#include "assets/actor_260500_animation_01314_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation01314Bank4[40] = {
#include "assets/actor_260500_animation_01314_bank4.inc"
};

static AnimationRecord _gActor260500Animation01314Records[116] = {
#include "assets/actor_260500_animation_01314_records.inc"
};

static u16 _gActor260500Animation01314Indices[20] = {
#include "assets/actor_260500_animation_01314_indices.inc"
};

static AnimationSet _gActor260500Animation01314 = {
    _gActor260500Animation01314Records,
    _gActor260500Animation01314Indices,
    { NULL, _gActor260500Animation01314Bank1, NULL, NULL, _gActor260500Animation01314Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation01A10Bank1[8] = {
#include "assets/actor_260500_animation_01A10_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation01A10Bank4[157] = {
#include "assets/actor_260500_animation_01A10_bank4.inc"
};

static AnimationRecord _gActor260500Animation01A10Records[246] = {
#include "assets/actor_260500_animation_01A10_records.inc"
};

static u16 _gActor260500Animation01A10Indices[20] = {
#include "assets/actor_260500_animation_01A10_indices.inc"
};

static AnimationSet _gActor260500Animation01A10 = {
    _gActor260500Animation01A10Records,
    _gActor260500Animation01A10Indices,
    { NULL, _gActor260500Animation01A10Bank1, NULL, NULL, _gActor260500Animation01A10Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation01CACBank1[2] = {
#include "assets/actor_260500_animation_01CAC_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation01CACBank4[51] = {
#include "assets/actor_260500_animation_01CAC_bank4.inc"
};

static AnimationRecord _gActor260500Animation01CACRecords[90] = {
#include "assets/actor_260500_animation_01CAC_records.inc"
};

static u16 _gActor260500Animation01CACIndices[20] = {
#include "assets/actor_260500_animation_01CAC_indices.inc"
};

static AnimationSet _gActor260500Animation01CAC = {
    _gActor260500Animation01CACRecords,
    _gActor260500Animation01CACIndices,
    { NULL, _gActor260500Animation01CACBank1, NULL, NULL, _gActor260500Animation01CACBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation02064Bank1[3] = {
#include "assets/actor_260500_animation_02064_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation02064Bank4[81] = {
#include "assets/actor_260500_animation_02064_bank4.inc"
};

static AnimationRecord _gActor260500Animation02064Records[128] = {
#include "assets/actor_260500_animation_02064_records.inc"
};

static u16 _gActor260500Animation02064Indices[20] = {
#include "assets/actor_260500_animation_02064_indices.inc"
};

static AnimationSet _gActor260500Animation02064 = {
    _gActor260500Animation02064Records,
    _gActor260500Animation02064Indices,
    { NULL, _gActor260500Animation02064Bank1, NULL, NULL, _gActor260500Animation02064Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation023E8Bank1[6] = {
#include "assets/actor_260500_animation_023E8_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation023E8Bank4[69] = {
#include "assets/actor_260500_animation_023E8_bank4.inc"
};

static AnimationRecord _gActor260500Animation023E8Records[118] = {
#include "assets/actor_260500_animation_023E8_records.inc"
};

static u16 _gActor260500Animation023E8Indices[20] = {
#include "assets/actor_260500_animation_023E8_indices.inc"
};

static AnimationSet _gActor260500Animation023E8 = {
    _gActor260500Animation023E8Records,
    _gActor260500Animation023E8Indices,
    { NULL, _gActor260500Animation023E8Bank1, NULL, NULL, _gActor260500Animation023E8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation026ECBank1[6] = {
#include "assets/actor_260500_animation_026EC_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation026ECBank4[46] = {
#include "assets/actor_260500_animation_026EC_bank4.inc"
};

static AnimationRecord _gActor260500Animation026ECRecords[109] = {
#include "assets/actor_260500_animation_026EC_records.inc"
};

static u16 _gActor260500Animation026ECIndices[20] = {
#include "assets/actor_260500_animation_026EC_indices.inc"
};

static AnimationSet _gActor260500Animation026EC = {
    _gActor260500Animation026ECRecords,
    _gActor260500Animation026ECIndices,
    { NULL, _gActor260500Animation026ECBank1, NULL, NULL, _gActor260500Animation026ECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation02A18Bank1[7] = {
#include "assets/actor_260500_animation_02A18_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation02A18Bank4[56] = {
#include "assets/actor_260500_animation_02A18_bank4.inc"
};

static AnimationRecord _gActor260500Animation02A18Records[106] = {
#include "assets/actor_260500_animation_02A18_records.inc"
};

static u16 _gActor260500Animation02A18Indices[20] = {
#include "assets/actor_260500_animation_02A18_indices.inc"
};

static AnimationSet _gActor260500Animation02A18 = {
    _gActor260500Animation02A18Records,
    _gActor260500Animation02A18Indices,
    { NULL, _gActor260500Animation02A18Bank1, NULL, NULL, _gActor260500Animation02A18Bank4, NULL, NULL, NULL },
};

AnimationPlayRequest D_actor_260500_8014C860 = { { .index = 1 }, 0, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C874 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C888 = { { .index = 1 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C89C = { { .index = 1 }, 3, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C8B0 = { { .index = 1 }, 4, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C8C4 = { { .index = 1 }, 5, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C8D8 = { { .index = 1 }, 6, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C8EC = { { .index = 1 }, 7, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C900 = { { .index = 1 }, 8, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C914 = { { .index = 1 }, 9, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C928 = { { .index = 1 }, 10, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C93C = { { .index = 1 }, 11, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C950 = { { .index = 1 }, 12, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C964 = { { .index = 1 }, 13, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C978 = { { .index = 1 }, 14, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C98C = { { .index = 1 }, 15, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C9A0 = { { .index = 1 }, 16, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C9B4 = { { .index = 1 }, 17, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C9C8 = { { .index = 1 }, 18, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C9DC = { { .index = 1 }, 19, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014C9F0 = { { .index = 1 }, 20, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014CA04 = { { .index = 1 }, 21, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014CA18 = { { .index = 1 }, 22, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014CA2C = { { .index = 1 }, 23, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

// Retained parameter record; layout follows the adjacent script arguments.
AnimationPlayRequest D_actor_260500_8014CA40 = { { .index = 1 }, 24, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014CA54 = { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014CA68 = { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014CA7C = { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014CA90 = { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014CAA4 = { { .index = 1 }, 53, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014CAB8 = { { .index = 1 }, 54, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014CACC = { { .index = 1 }, 55, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_260500_8014CAE0 = { { .index = 1 }, 56, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

/// Player clips for extended ids 47-56; the entries for ids 51 and 52 are NULL
/// and nothing requests them.
///
/// The package's event scripts send the player its copy request.
/// `D_actor_260500_8014CB1C` copies `ANIMATION_BANK_EXTENSION_CAPACITY` (32)
/// words starting here into the player's bank, which is 22 words past the end
/// of this array: the read runs on through `D_actor_260500_8014CB1C`,
/// `D_actor_260500_8014CB24`, `D_actor_260500_8014CB38`,
/// `D_actor_260500_8014CB50` and the first three words of
/// `D_actor_260500_8014CB68`. That overrun is the original's and is kept as it
/// is: the request carries the bank's fixed capacity, while the table was
/// stored with only its own entries. The player's requests select ids 47-50,
/// 53-56 and the bank's own id 1 only, so neither a NULL entry nor a word
/// installed after the table is played as a clip.
AnimationSet* D_actor_260500_8014CAF4[10] = { &_gActor260500Animation01024, &_gActor260500Animation01314, &_gActor260500Animation01A10, &_gActor260500Animation01CAC, NULL, NULL, &_gActor260500Animation02064, &_gActor260500Animation023E8, &_gActor260500Animation026EC, &_gActor260500Animation02A18 };

// Installs the player's clips; the count is the bank's capacity, not the ten entries of its source.
AnimationBankCopyRequest D_actor_260500_8014CB1C = { { .sets = D_actor_260500_8014CAF4 }, ANIMATION_BANK_EXTENSION_CAPACITY };

// Restarts the player bank's own clip 1; played as the first conversation opens and when it ends or is skipped.
AnimationPlayRequest D_actor_260500_8014CB24 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

// The four records from here on are where the scripts stand the actor at scene placement 0. This one: after the first conversation ends or is skipped.
ActorTransform D_actor_260500_8014CB38 = { { 860, 0, 6730, 0 }, { 0, -2161, 0, 0 } };

// The same spot, as the first conversation opens and resumes.
ActorTransform D_actor_260500_8014CB50 = { { 860, 0, 6730, 0 }, { 0, -2161, 0, 0 } };

// The actor's other spot during the first conversation.
ActorTransform D_actor_260500_8014CB68 = { { 860, 0, 6910, 0 }, { 0, -2048, 0, 0 } };

ActorTransform D_actor_260500_8014CB80 = { { 860, 0, 6640, 0 }, { 0, -2161, 0, 0 } };

ActorTransform D_actor_260500_8014CB98 = { { 920, 0, 6000, 0 }, { 0, -113, 0, 0 } };

ActorTransform D_actor_260500_8014CBB0 = { { 1210, 0, 5610, 0 }, { 0, -227, 0, 0 } };

ActorTransform D_actor_260500_8014CBC8 = { { 1010, 0, 5840, 0 }, { 0, -113, 0, 0 } };

ActorTransform D_actor_260500_8014CBE0 = { { 860, 0, 6180, 0 }, { 0, 0, 0, 0 } };

EvsCommand D_actor_260500_8014CBF8[109] = {
    { EVENT_SCRIPT_OPCODE_SET_SKIP_KEEP_SOUND, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor260500SelectConversationCaptions }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CB24 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_260500_8014CB1C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_260500_8014CB98 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_260500_8014CB50 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CACC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C874 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CACC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_260500_8014CB68 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C888 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C89C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 22 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C8B0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CAA4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 62 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CACC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C8C4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C8D8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_260500_8014CBB0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_260500_8014CB80 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_260500_8014CBC8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C8EC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CAB8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 34 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CACC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C8D8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 100 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C8EC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C900 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 25 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C914 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C928 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CAA4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CA54 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CA90 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 28 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CACC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C93C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_260500_8014CB50 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C950 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_260500_8014CBB0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CA68 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CACC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_260500_8014CB50 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C964 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_260500_8014CB68 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_260500_8014CB98 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CACC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C978 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 25 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C98C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CA7C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 62 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CACC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C9A0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014CA2C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_260500_8014CB38 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C8B0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_260500_8014CBE0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CB24 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor260500SelectConversationCaptions }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_260500_8014D630[17] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 4 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CB24 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_260500_8014CB38 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C8B0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_260500_8014CBE0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor260500SelectConversationCaptions }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_260500_8014D7C8[16] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_260500_8014CB1C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CACC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C8D8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C8EC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C9B4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CAA4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C8B0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_260500_8014D948[15] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_260500_8014CB1C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 3 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CACC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C8D8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C8EC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C9C8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CAA4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C8B0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_260500_8014DAB0[22] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_260500_8014CB1C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CACC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C8D8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C8EC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C9DC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CA68 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CACC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C9F0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CAE0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014CA04 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C8B0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_260500_8014DCC0[9] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_260500_8014CB1C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 5 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_260500_8014CACC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014CA18 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_260500_8014C8B0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

static AnimationPackedPose _gActor260500Animation042F8Bank1[4] = {
#include "assets/actor_260500_animation_042F8_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation042F8Bank4[55] = {
#include "assets/actor_260500_animation_042F8_bank4.inc"
};

static AnimationRecord _gActor260500Animation042F8Records[147] = {
#include "assets/actor_260500_animation_042F8_records.inc"
};

static u16 _gActor260500Animation042F8Indices[20] = {
#include "assets/actor_260500_animation_042F8_indices.inc"
};

static AnimationSet _gActor260500Animation042F8 = {
    _gActor260500Animation042F8Records,
    _gActor260500Animation042F8Indices,
    { NULL, _gActor260500Animation042F8Bank1, NULL, NULL, _gActor260500Animation042F8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation0483CBank1[5] = {
#include "assets/actor_260500_animation_0483C_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation0483CBank4[103] = {
#include "assets/actor_260500_animation_0483C_bank4.inc"
};

static AnimationRecord _gActor260500Animation0483CRecords[199] = {
#include "assets/actor_260500_animation_0483C_records.inc"
};

static u16 _gActor260500Animation0483CIndices[20] = {
#include "assets/actor_260500_animation_0483C_indices.inc"
};

static AnimationSet _gActor260500Animation0483C = {
    _gActor260500Animation0483CRecords,
    _gActor260500Animation0483CIndices,
    { NULL, _gActor260500Animation0483CBank1, NULL, NULL, _gActor260500Animation0483CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation04AC4Bank1[2] = {
#include "assets/actor_260500_animation_04AC4_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation04AC4Bank4[53] = {
#include "assets/actor_260500_animation_04AC4_bank4.inc"
};

static AnimationRecord _gActor260500Animation04AC4Records[83] = {
#include "assets/actor_260500_animation_04AC4_records.inc"
};

static u16 _gActor260500Animation04AC4Indices[20] = {
#include "assets/actor_260500_animation_04AC4_indices.inc"
};

static AnimationSet _gActor260500Animation04AC4 = {
    _gActor260500Animation04AC4Records,
    _gActor260500Animation04AC4Indices,
    { NULL, _gActor260500Animation04AC4Bank1, NULL, NULL, _gActor260500Animation04AC4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation04D18Bank1[3] = {
#include "assets/actor_260500_animation_04D18_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation04D18Bank4[27] = {
#include "assets/actor_260500_animation_04D18_bank4.inc"
};

static AnimationRecord _gActor260500Animation04D18Records[93] = {
#include "assets/actor_260500_animation_04D18_records.inc"
};

static u16 _gActor260500Animation04D18Indices[20] = {
#include "assets/actor_260500_animation_04D18_indices.inc"
};

static AnimationSet _gActor260500Animation04D18 = {
    _gActor260500Animation04D18Records,
    _gActor260500Animation04D18Indices,
    { NULL, _gActor260500Animation04D18Bank1, NULL, NULL, _gActor260500Animation04D18Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation04ED8Bank1[3] = {
#include "assets/actor_260500_animation_04ED8_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation04ED8Bank4[20] = {
#include "assets/actor_260500_animation_04ED8_bank4.inc"
};

static AnimationRecord _gActor260500Animation04ED8Records[63] = {
#include "assets/actor_260500_animation_04ED8_records.inc"
};

static u16 _gActor260500Animation04ED8Indices[20] = {
#include "assets/actor_260500_animation_04ED8_indices.inc"
};

static AnimationSet _gActor260500Animation04ED8 = {
    _gActor260500Animation04ED8Records,
    _gActor260500Animation04ED8Indices,
    { NULL, _gActor260500Animation04ED8Bank1, NULL, NULL, _gActor260500Animation04ED8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation05234Bank1[3] = {
#include "assets/actor_260500_animation_05234_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation05234Bank4[49] = {
#include "assets/actor_260500_animation_05234_bank4.inc"
};

static AnimationRecord _gActor260500Animation05234Records[137] = {
#include "assets/actor_260500_animation_05234_records.inc"
};

static u16 _gActor260500Animation05234Indices[20] = {
#include "assets/actor_260500_animation_05234_indices.inc"
};

static AnimationSet _gActor260500Animation05234 = {
    _gActor260500Animation05234Records,
    _gActor260500Animation05234Indices,
    { NULL, _gActor260500Animation05234Bank1, NULL, NULL, _gActor260500Animation05234Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation05400Bank1[3] = {
#include "assets/actor_260500_animation_05400_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation05400Bank4[29] = {
#include "assets/actor_260500_animation_05400_bank4.inc"
};

static AnimationRecord _gActor260500Animation05400Records[57] = {
#include "assets/actor_260500_animation_05400_records.inc"
};

static u16 _gActor260500Animation05400Indices[20] = {
#include "assets/actor_260500_animation_05400_indices.inc"
};

static AnimationSet _gActor260500Animation05400 = {
    _gActor260500Animation05400Records,
    _gActor260500Animation05400Indices,
    { NULL, _gActor260500Animation05400Bank1, NULL, NULL, _gActor260500Animation05400Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation057B0Bank1[4] = {
#include "assets/actor_260500_animation_057B0_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation057B0Bank4[82] = {
#include "assets/actor_260500_animation_057B0_bank4.inc"
};

static AnimationRecord _gActor260500Animation057B0Records[122] = {
#include "assets/actor_260500_animation_057B0_records.inc"
};

static u16 _gActor260500Animation057B0Indices[20] = {
#include "assets/actor_260500_animation_057B0_indices.inc"
};

static AnimationSet _gActor260500Animation057B0 = {
    _gActor260500Animation057B0Records,
    _gActor260500Animation057B0Indices,
    { NULL, _gActor260500Animation057B0Bank1, NULL, NULL, _gActor260500Animation057B0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation059D4Bank1[2] = {
#include "assets/actor_260500_animation_059D4_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation059D4Bank4[26] = {
#include "assets/actor_260500_animation_059D4_bank4.inc"
};

static AnimationRecord _gActor260500Animation059D4Records[85] = {
#include "assets/actor_260500_animation_059D4_records.inc"
};

static u16 _gActor260500Animation059D4Indices[20] = {
#include "assets/actor_260500_animation_059D4_indices.inc"
};

static AnimationSet _gActor260500Animation059D4 = {
    _gActor260500Animation059D4Records,
    _gActor260500Animation059D4Indices,
    { NULL, _gActor260500Animation059D4Bank1, NULL, NULL, _gActor260500Animation059D4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation05DB8Bank1[6] = {
#include "assets/actor_260500_animation_05DB8_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation05DB8Bank4[63] = {
#include "assets/actor_260500_animation_05DB8_bank4.inc"
};

static AnimationRecord _gActor260500Animation05DB8Records[148] = {
#include "assets/actor_260500_animation_05DB8_records.inc"
};

static u16 _gActor260500Animation05DB8Indices[20] = {
#include "assets/actor_260500_animation_05DB8_indices.inc"
};

static AnimationSet _gActor260500Animation05DB8 = {
    _gActor260500Animation05DB8Records,
    _gActor260500Animation05DB8Indices,
    { NULL, _gActor260500Animation05DB8Bank1, NULL, NULL, _gActor260500Animation05DB8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation060B8Bank1[3] = {
#include "assets/actor_260500_animation_060B8_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation060B8Bank4[62] = {
#include "assets/actor_260500_animation_060B8_bank4.inc"
};

static AnimationRecord _gActor260500Animation060B8Records[101] = {
#include "assets/actor_260500_animation_060B8_records.inc"
};

static u16 _gActor260500Animation060B8Indices[20] = {
#include "assets/actor_260500_animation_060B8_indices.inc"
};

static AnimationSet _gActor260500Animation060B8 = {
    _gActor260500Animation060B8Records,
    _gActor260500Animation060B8Indices,
    { NULL, _gActor260500Animation060B8Bank1, NULL, NULL, _gActor260500Animation060B8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation06474Bank1[5] = {
#include "assets/actor_260500_animation_06474_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation06474Bank4[76] = {
#include "assets/actor_260500_animation_06474_bank4.inc"
};

static AnimationRecord _gActor260500Animation06474Records[128] = {
#include "assets/actor_260500_animation_06474_records.inc"
};

static u16 _gActor260500Animation06474Indices[20] = {
#include "assets/actor_260500_animation_06474_indices.inc"
};

static AnimationSet _gActor260500Animation06474 = {
    _gActor260500Animation06474Records,
    _gActor260500Animation06474Indices,
    { NULL, _gActor260500Animation06474Bank1, NULL, NULL, _gActor260500Animation06474Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation06640Bank1[3] = {
#include "assets/actor_260500_animation_06640_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation06640Bank4[29] = {
#include "assets/actor_260500_animation_06640_bank4.inc"
};

static AnimationRecord _gActor260500Animation06640Records[57] = {
#include "assets/actor_260500_animation_06640_records.inc"
};

static u16 _gActor260500Animation06640Indices[20] = {
#include "assets/actor_260500_animation_06640_indices.inc"
};

static AnimationSet _gActor260500Animation06640 = {
    _gActor260500Animation06640Records,
    _gActor260500Animation06640Indices,
    { NULL, _gActor260500Animation06640Bank1, NULL, NULL, _gActor260500Animation06640Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation06878Bank1[4] = {
#include "assets/actor_260500_animation_06878_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation06878Bank4[41] = {
#include "assets/actor_260500_animation_06878_bank4.inc"
};

static AnimationRecord _gActor260500Animation06878Records[69] = {
#include "assets/actor_260500_animation_06878_records.inc"
};

static u16 _gActor260500Animation06878Indices[20] = {
#include "assets/actor_260500_animation_06878_indices.inc"
};

static AnimationSet _gActor260500Animation06878 = {
    _gActor260500Animation06878Records,
    _gActor260500Animation06878Indices,
    { NULL, _gActor260500Animation06878Bank1, NULL, NULL, _gActor260500Animation06878Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation06AF0Bank1[3] = {
#include "assets/actor_260500_animation_06AF0_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation06AF0Bank4[28] = {
#include "assets/actor_260500_animation_06AF0_bank4.inc"
};

static AnimationRecord _gActor260500Animation06AF0Records[101] = {
#include "assets/actor_260500_animation_06AF0_records.inc"
};

static u16 _gActor260500Animation06AF0Indices[20] = {
#include "assets/actor_260500_animation_06AF0_indices.inc"
};

static AnimationSet _gActor260500Animation06AF0 = {
    _gActor260500Animation06AF0Records,
    _gActor260500Animation06AF0Indices,
    { NULL, _gActor260500Animation06AF0Bank1, NULL, NULL, _gActor260500Animation06AF0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation06DF0Bank1[6] = {
#include "assets/actor_260500_animation_06DF0_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation06DF0Bank4[61] = {
#include "assets/actor_260500_animation_06DF0_bank4.inc"
};

static AnimationRecord _gActor260500Animation06DF0Records[93] = {
#include "assets/actor_260500_animation_06DF0_records.inc"
};

static u16 _gActor260500Animation06DF0Indices[20] = {
#include "assets/actor_260500_animation_06DF0_indices.inc"
};

static AnimationSet _gActor260500Animation06DF0 = {
    _gActor260500Animation06DF0Records,
    _gActor260500Animation06DF0Indices,
    { NULL, _gActor260500Animation06DF0Bank1, NULL, NULL, _gActor260500Animation06DF0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation07118Bank1[4] = {
#include "assets/actor_260500_animation_07118_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation07118Bank4[57] = {
#include "assets/actor_260500_animation_07118_bank4.inc"
};

static AnimationRecord _gActor260500Animation07118Records[113] = {
#include "assets/actor_260500_animation_07118_records.inc"
};

static u16 _gActor260500Animation07118Indices[20] = {
#include "assets/actor_260500_animation_07118_indices.inc"
};

static AnimationSet _gActor260500Animation07118 = {
    _gActor260500Animation07118Records,
    _gActor260500Animation07118Indices,
    { NULL, _gActor260500Animation07118Bank1, NULL, NULL, _gActor260500Animation07118Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation07698Bank1[8] = {
#include "assets/actor_260500_animation_07698_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation07698Bank4[119] = {
#include "assets/actor_260500_animation_07698_bank4.inc"
};

static AnimationRecord _gActor260500Animation07698Records[189] = {
#include "assets/actor_260500_animation_07698_records.inc"
};

static u16 _gActor260500Animation07698Indices[20] = {
#include "assets/actor_260500_animation_07698_indices.inc"
};

static AnimationSet _gActor260500Animation07698 = {
    _gActor260500Animation07698Records,
    _gActor260500Animation07698Indices,
    { NULL, _gActor260500Animation07698Bank1, NULL, NULL, _gActor260500Animation07698Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation07870Bank1[3] = {
#include "assets/actor_260500_animation_07870_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation07870Bank4[32] = {
#include "assets/actor_260500_animation_07870_bank4.inc"
};

static AnimationRecord _gActor260500Animation07870Records[57] = {
#include "assets/actor_260500_animation_07870_records.inc"
};

static u16 _gActor260500Animation07870Indices[20] = {
#include "assets/actor_260500_animation_07870_indices.inc"
};

static AnimationSet _gActor260500Animation07870 = {
    _gActor260500Animation07870Records,
    _gActor260500Animation07870Indices,
    { NULL, _gActor260500Animation07870Bank1, NULL, NULL, _gActor260500Animation07870Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation07B3CBank1[4] = {
#include "assets/actor_260500_animation_07B3C_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation07B3CBank4[51] = {
#include "assets/actor_260500_animation_07B3C_bank4.inc"
};

static AnimationRecord _gActor260500Animation07B3CRecords[96] = {
#include "assets/actor_260500_animation_07B3C_records.inc"
};

static u16 _gActor260500Animation07B3CIndices[20] = {
#include "assets/actor_260500_animation_07B3C_indices.inc"
};

static AnimationSet _gActor260500Animation07B3C = {
    _gActor260500Animation07B3CRecords,
    _gActor260500Animation07B3CIndices,
    { NULL, _gActor260500Animation07B3CBank1, NULL, NULL, _gActor260500Animation07B3CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation07D1CBank1[3] = {
#include "assets/actor_260500_animation_07D1C_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation07D1CBank4[34] = {
#include "assets/actor_260500_animation_07D1C_bank4.inc"
};

static AnimationRecord _gActor260500Animation07D1CRecords[57] = {
#include "assets/actor_260500_animation_07D1C_records.inc"
};

static u16 _gActor260500Animation07D1CIndices[20] = {
#include "assets/actor_260500_animation_07D1C_indices.inc"
};

static AnimationSet _gActor260500Animation07D1C = {
    _gActor260500Animation07D1CRecords,
    _gActor260500Animation07D1CIndices,
    { NULL, _gActor260500Animation07D1CBank1, NULL, NULL, _gActor260500Animation07D1CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation07FC8Bank1[3] = {
#include "assets/actor_260500_animation_07FC8_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation07FC8Bank4[41] = {
#include "assets/actor_260500_animation_07FC8_bank4.inc"
};

static AnimationRecord _gActor260500Animation07FC8Records[101] = {
#include "assets/actor_260500_animation_07FC8_records.inc"
};

static u16 _gActor260500Animation07FC8Indices[20] = {
#include "assets/actor_260500_animation_07FC8_indices.inc"
};

static AnimationSet _gActor260500Animation07FC8 = {
    _gActor260500Animation07FC8Records,
    _gActor260500Animation07FC8Indices,
    { NULL, _gActor260500Animation07FC8Bank1, NULL, NULL, _gActor260500Animation07FC8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation081E4Bank1[2] = {
#include "assets/actor_260500_animation_081E4_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation081E4Bank4[24] = {
#include "assets/actor_260500_animation_081E4_bank4.inc"
};

static AnimationRecord _gActor260500Animation081E4Records[85] = {
#include "assets/actor_260500_animation_081E4_records.inc"
};

static u16 _gActor260500Animation081E4Indices[20] = {
#include "assets/actor_260500_animation_081E4_indices.inc"
};

static AnimationSet _gActor260500Animation081E4 = {
    _gActor260500Animation081E4Records,
    _gActor260500Animation081E4Indices,
    { NULL, _gActor260500Animation081E4Bank1, NULL, NULL, _gActor260500Animation081E4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation084E4Bank1[6] = {
#include "assets/actor_260500_animation_084E4_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation084E4Bank4[61] = {
#include "assets/actor_260500_animation_084E4_bank4.inc"
};

static AnimationRecord _gActor260500Animation084E4Records[93] = {
#include "assets/actor_260500_animation_084E4_records.inc"
};

static u16 _gActor260500Animation084E4Indices[20] = {
#include "assets/actor_260500_animation_084E4_indices.inc"
};

static AnimationSet _gActor260500Animation084E4 = {
    _gActor260500Animation084E4Records,
    _gActor260500Animation084E4Indices,
    { NULL, _gActor260500Animation084E4Bank1, NULL, NULL, _gActor260500Animation084E4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation087E8Bank1[6] = {
#include "assets/actor_260500_animation_087E8_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation087E8Bank4[46] = {
#include "assets/actor_260500_animation_087E8_bank4.inc"
};

static AnimationRecord _gActor260500Animation087E8Records[109] = {
#include "assets/actor_260500_animation_087E8_records.inc"
};

static u16 _gActor260500Animation087E8Indices[20] = {
#include "assets/actor_260500_animation_087E8_indices.inc"
};

static AnimationSet _gActor260500Animation087E8 = {
    _gActor260500Animation087E8Records,
    _gActor260500Animation087E8Indices,
    { NULL, _gActor260500Animation087E8Bank1, NULL, NULL, _gActor260500Animation087E8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation08D68Bank1[10] = {
#include "assets/actor_260500_animation_08D68_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation08D68Bank4[104] = {
#include "assets/actor_260500_animation_08D68_bank4.inc"
};

static AnimationRecord _gActor260500Animation08D68Records[198] = {
#include "assets/actor_260500_animation_08D68_records.inc"
};

static u16 _gActor260500Animation08D68Indices[20] = {
#include "assets/actor_260500_animation_08D68_indices.inc"
};

static AnimationSet _gActor260500Animation08D68 = {
    _gActor260500Animation08D68Records,
    _gActor260500Animation08D68Indices,
    { NULL, _gActor260500Animation08D68Bank1, NULL, NULL, _gActor260500Animation08D68Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation09148Bank1[8] = {
#include "assets/actor_260500_animation_09148_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation09148Bank4[85] = {
#include "assets/actor_260500_animation_09148_bank4.inc"
};

static AnimationRecord _gActor260500Animation09148Records[119] = {
#include "assets/actor_260500_animation_09148_records.inc"
};

static u16 _gActor260500Animation09148Indices[20] = {
#include "assets/actor_260500_animation_09148_indices.inc"
};

static AnimationSet _gActor260500Animation09148 = {
    _gActor260500Animation09148Records,
    _gActor260500Animation09148Indices,
    { NULL, _gActor260500Animation09148Bank1, NULL, NULL, _gActor260500Animation09148Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation094B0Bank1[5] = {
#include "assets/actor_260500_animation_094B0_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation094B0Bank4[74] = {
#include "assets/actor_260500_animation_094B0_bank4.inc"
};

static AnimationRecord _gActor260500Animation094B0Records[109] = {
#include "assets/actor_260500_animation_094B0_records.inc"
};

static u16 _gActor260500Animation094B0Indices[20] = {
#include "assets/actor_260500_animation_094B0_indices.inc"
};

static AnimationSet _gActor260500Animation094B0 = {
    _gActor260500Animation094B0Records,
    _gActor260500Animation094B0Indices,
    { NULL, _gActor260500Animation094B0Bank1, NULL, NULL, _gActor260500Animation094B0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation097C8Bank1[5] = {
#include "assets/actor_260500_animation_097C8_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation097C8Bank4[66] = {
#include "assets/actor_260500_animation_097C8_bank4.inc"
};

static AnimationRecord _gActor260500Animation097C8Records[97] = {
#include "assets/actor_260500_animation_097C8_records.inc"
};

static u16 _gActor260500Animation097C8Indices[20] = {
#include "assets/actor_260500_animation_097C8_indices.inc"
};

static AnimationSet _gActor260500Animation097C8 = {
    _gActor260500Animation097C8Records,
    _gActor260500Animation097C8Indices,
    { NULL, _gActor260500Animation097C8Bank1, NULL, NULL, _gActor260500Animation097C8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation09B9CBank1[8] = {
#include "assets/actor_260500_animation_09B9C_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation09B9CBank4[84] = {
#include "assets/actor_260500_animation_09B9C_bank4.inc"
};

static AnimationRecord _gActor260500Animation09B9CRecords[117] = {
#include "assets/actor_260500_animation_09B9C_records.inc"
};

static u16 _gActor260500Animation09B9CIndices[20] = {
#include "assets/actor_260500_animation_09B9C_indices.inc"
};

static AnimationSet _gActor260500Animation09B9C = {
    _gActor260500Animation09B9CRecords,
    _gActor260500Animation09B9CIndices,
    { NULL, _gActor260500Animation09B9CBank1, NULL, NULL, _gActor260500Animation09B9CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation09F8CBank1[7] = {
#include "assets/actor_260500_animation_09F8C_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation09F8CBank4[62] = {
#include "assets/actor_260500_animation_09F8C_bank4.inc"
};

static AnimationRecord _gActor260500Animation09F8CRecords[149] = {
#include "assets/actor_260500_animation_09F8C_records.inc"
};

static u16 _gActor260500Animation09F8CIndices[20] = {
#include "assets/actor_260500_animation_09F8C_indices.inc"
};

static AnimationSet _gActor260500Animation09F8C = {
    _gActor260500Animation09F8CRecords,
    _gActor260500Animation09F8CIndices,
    { NULL, _gActor260500Animation09F8CBank1, NULL, NULL, _gActor260500Animation09F8CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor260500Animation0A2F8Bank1[7] = {
#include "assets/actor_260500_animation_0A2F8_bank1.inc"
};

static AnimationPackedRotation _gActor260500Animation0A2F8Bank4[74] = {
#include "assets/actor_260500_animation_0A2F8_bank4.inc"
};

static AnimationRecord _gActor260500Animation0A2F8Records[104] = {
#include "assets/actor_260500_animation_0A2F8_records.inc"
};

static u16 _gActor260500Animation0A2F8Indices[20] = {
#include "assets/actor_260500_animation_0A2F8_indices.inc"
};

static AnimationSet _gActor260500Animation0A2F8 = {
    _gActor260500Animation0A2F8Records,
    _gActor260500Animation0A2F8Indices,
    { NULL, _gActor260500Animation0A2F8Bank1, NULL, NULL, _gActor260500Animation0A2F8Bank4, NULL, NULL, NULL },
};

static TmdBone _gActor260500JodieBouquetBody2Skeleton[19] = {
#include "assets/jodie_bouquet_body_2_skeleton.inc"
};

static u32 _gActor260500JodieBouquetBody2PartVerts[19] = {
#include "assets/jodie_bouquet_body_2_partVerts.inc"
};

static SVECTOR _gActor260500JodieBouquetBody2Verts[369] = {
#include "assets/jodie_bouquet_body_2_verts.inc"
};

static SVECTOR _gActor260500JodieBouquetBody2Normals[369] = {
#include "assets/jodie_bouquet_body_2_normals.inc"
};

static u32 _gActor260500JodieBouquetBody2Stream[4228] = {
#include "assets/jodie_bouquet_body_2_stream.inc"
};

static TmdSource _gActor260500JodieBouquetBody2 = {
    0,
    22752,
    6928,
    19,
    _gActor260500JodieBouquetBody2PartVerts,
    _gActor260500JodieBouquetBody2Verts,
    _gActor260500JodieBouquetBody2Normals,
    _gActor260500JodieBouquetBody2Skeleton,
    _gActor260500JodieBouquetBody2Stream,
};

/// Duration of the next animation blend, in whole normal-rate frames.
///
/// Starts at eight frames. Play requests narrow their duration to this
/// signed halfword; travel completion sets ten frames for the idle blend.
/// Reset requests leave it unchanged. Blending accepts 0..2047 without
/// validation; this latch is a duration, never a remaining-frame count.
static s16 _gFootstepWalkBlendFrames = FOOTSTEP_WALK_DEFAULT_BLEND_FRAMES;

TaskMessageEntry D_actor_260500_80159D80[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _footstepWalkQuietPlayAnimation },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _footstepWalkSetModelDraw },
    { ACTOR_MESSAGE_PLACE, _footstepWalkPlace },
    { ACTOR_COMMAND_MESSAGE_APPLY, _footstepWalkApplyTurnCommand },
    { ACTOR_MESSAGE_WALK_TO, _footstepWalkSetWalkTarget },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_actor_260500_80159DB0 = { { { TASK_BODY_TMD, 192 } }, _actor260500Task, { .model = &_gActor260500JodieBouquetBody2 } };

u8 D_actor_260500_80159DBC[144] = {
    0,
    0,
    0,
    0,
    24,
    225,
    20,
    128,
    92,
    230,
    20,
    128,
    228,
    232,
    20,
    128,
    56,
    235,
    20,
    128,
    248,
    236,
    20,
    128,
    84,
    240,
    20,
    128,
    32,
    242,
    20,
    128,
    208,
    245,
    20,
    128,
    244,
    247,
    20,
    128,
    216,
    251,
    20,
    128,
    216,
    254,
    20,
    128,
    148,
    2,
    21,
    128,
    96,
    4,
    21,
    128,
    152,
    6,
    21,
    128,
    16,
    9,
    21,
    128,
    16,
    12,
    21,
    128,
    56,
    15,
    21,
    128,
    184,
    20,
    21,
    128,
    144,
    22,
    21,
    128,
    92,
    25,
    21,
    128,
    60,
    27,
    21,
    128,
    232,
    29,
    21,
    128,
    4,
    32,
    21,
    128,
    4,
    35,
    21,
    128,
    0,
    0,
    0,
    0,
    8,
    38,
    21,
    128,
    136,
    43,
    21,
    128,
    104,
    47,
    21,
    128,
    208,
    50,
    21,
    128,
    232,
    53,
    21,
    128,
    188,
    57,
    21,
    128,
    172,
    61,
    21,
    128,
    24,
    65,
    21,
    128,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

/// Borrowed pointer to the quiet walker's task-owned work block.
///
/// Spawn publishes the zeroed allocation also held by `Task::work` and
/// the dispatcher refreshes this pointer before each task-state call.
/// Animation and singleton message handlers require the same live block.
/// The model borrows its lighting matrices and the rig borrows its slots
/// and poses. Task teardown releases the block without clearing this
/// pointer; it confers no ownership and must not be used after teardown.
static FootstepWalkQuietWork* _gFootstepWalkWork = NULL;

Task* D_actor_260500_80159E50;

/// Travel mode selected by the last walk-target request.
///
/// Stored as a signed halfword: 0 moves forward 60, 1 backward 15, and
/// 2 forward 25 parent-coordinate units per moving update. Backward
/// requests face away from the target. This selects distance independently
/// of the animation clip; request values narrow to 16 bits without checking.
static s16 _gFootstepWalkMode;

/// Selects the opening conversation's CAP file and texture page, or restores defaults.
///
/// Event scripts pass nonzero on entry and zero after completion or skip.
/// Uses already loaded data resource ordinal 2 and texture origin (832, 0)
/// in VRAM pixels. Resources must remain live through playback. A missing
/// resource leaves the cleared file pointer NULL; zero resets CAP selection
/// and restores the bundle's default file and texture origin.
static void _actor260500SelectConversationCaptions(s32 useConversationFile)
{
    enum {
        ACTOR_260500_CONVERSATION_CAP_FILE_ORDINAL = 2,
        ACTOR_260500_CONVERSATION_TEXTURE_VRAM_X   = 832,
        ACTOR_260500_CONVERSATION_TEXTURE_VRAM_Y   = 0
    };

    if (useConversationFile != 0) {
        Gp_CapFile = NULL;
        capSelectLoadedFile(ACTOR_260500_CONVERSATION_CAP_FILE_ORDINAL);
        capSetTexturePage(ACTOR_260500_CONVERSATION_TEXTURE_VRAM_X, ACTOR_260500_CONVERSATION_TEXTURE_VRAM_Y);
        return;
    }
    capReset();
}

void actor260500RestoreHeliportPlacement(void)
{
    enum { ACTOR_260500_HELIPORT_PLACEMENT_INDEX = 0 };
    Task* walkerTask;

    walkerTask = sceneFindPlacedActor(ACTOR_260500_HELIPORT_PLACEMENT_INDEX);
    if (walkerTask != NULL) {
        TASK_MESSAGE_DISPATCH_POINTER(walkerTask, ACTOR_MESSAGE_PLACE, &D_actor_260500_8014CB38, 0);
    }
}

void actor260500StartHeliportConversation(void)
{
    enum {
        ACTOR_260500_TALK_INTRO  = 0,
        ACTOR_260500_TALK_SECOND = 1,
        ACTOR_260500_TALK_THIRD  = 2,
        ACTOR_260500_TALK_FOURTH = 3,
        ACTOR_260500_TALK_REPEAT = 4
    };

    switch (gameFlagGetNibble(GAME_FLAG_HELIPORT_TALK_PROGRESS)) {
        case ACTOR_260500_TALK_INTRO:
            evsStartScriptWithSkip(D_actor_260500_8014CBF8, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_actor_260500_8014D630);
            gameFlagSetNibble(GAME_FLAG_HELIPORT_TALK_PROGRESS, ACTOR_260500_TALK_SECOND);
            break;
        case ACTOR_260500_TALK_SECOND:
            evsStartScript(D_actor_260500_8014D7C8, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            gameFlagSetNibble(GAME_FLAG_HELIPORT_TALK_PROGRESS, ACTOR_260500_TALK_THIRD);
            break;
        case ACTOR_260500_TALK_THIRD:
            evsStartScript(D_actor_260500_8014D948, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            gameFlagSetNibble(GAME_FLAG_HELIPORT_TALK_PROGRESS, ACTOR_260500_TALK_FOURTH);
            break;
        case ACTOR_260500_TALK_FOURTH:
            evsStartScript(D_actor_260500_8014DAB0, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            gameFlagSetNibble(GAME_FLAG_HELIPORT_TALK_PROGRESS, ACTOR_260500_TALK_REPEAT);
            break;
        case ACTOR_260500_TALK_REPEAT:
            evsStartScript(D_actor_260500_8014DCC0, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            break;
    }
}

/// Binds the quiet walker's playback storage and requests this package's startup clip.
///
/// The published `_gFootstepWalkWork` must be a live, writable task-owned block.
/// `model` must be a `tmdCreateModel` allocation with nineteen part coordinates.
/// The context borrows that allocation's coordinate tail, the work block's slots
/// and encoded-pose buffer, and the package's native clip table. All borrowed
/// storage and clip data must stay live and unmoved throughout playback.
///
/// Requests clip 4 in `ACTOR_ENEMY_ANIM_RESET`; the caller must process that
/// request before ticking parts 1 through 18. Binding neither initializes slots
/// nor clears poses or writes model coordinates. Remaining travel and turn
/// updates start at zero.
static __inline__ void _actor260500PrepareWalkerAnimation(TmdObject* model)
{
    enum { ACTOR_260500_STARTUP_ANIM_ID = 4 };

    animationInitContext(&_gFootstepWalkWork->rig.anim, (AnimationSet**)D_actor_260500_80159DBC, model,
                         _gFootstepWalkWork->rig.poses, _gFootstepWalkWork->rig.slots);
    _gFootstepWalkWork->st.animId  = ACTOR_260500_STARTUP_ANIM_ID;
    _gFootstepWalkWork->st.state   = ACTOR_ENEMY_ANIM_RESET;
    _gFootstepWalkWork->st.travel  = 0;
    _gFootstepWalkWork->turnFrames = 0;
}

/// Initializes the quiet walker's task-owned work, lighting and animation in state 0.
///
/// `enemy` is the live allocation in `task->spawnArg2.pointer`; the task must
/// own a nineteen-part model and have no work allocation yet. Its zeroed
/// `FootstepWalkQuietWork` is published for singleton handlers. The model
/// borrows its matrices and the rig borrows the loaded clips and model storage.
/// The view, primary heap and lighting query's scratch/GTE state must be ready.
///
/// Starts clip 4 without a pose tick, installs message and exit callbacks and
/// enters frame state. Allocation failure destroys the enemy and starts task
/// teardown; neither argument may be used afterwards. Exit releases the work
/// without clearing the published pointers, which must then remain unused.
static void _footstepWalkQuietSpawn(Enemy* enemy, Task* task)
{
    enum {
        FOOTSTEP_WALK_OT_ENTRY_OFFSET       = 1,
        FOOTSTEP_WALK_LIGHT_SAMPLE_Y_OFFSET = -800
    };

    VECTOR3                lightSamplePosition;
    GfxCoord*              rootCoord;
    TmdObject*             model;
    FootstepWalkQuietWork* allocatedWork;

    model              = task->extra.tmd;
    rootCoord          = model->coords;
    allocatedWork      = memCalloc(sizeof(*allocatedWork), false);
    _gFootstepWalkWork = allocatedWork;
    task->work         = allocatedWork;
    if (allocatedWork == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    // The task owns the allocation; the model borrows its lighting matrices.
    task->exitCallback               = _footstepWalkExit;
    rootCoord->parent                = &gGfxViewCoord;
    enemy->field_4                   = &rootCoord->coord;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = false;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    model->otOffset                  = FOOTSTEP_WALK_OT_ENTRY_OFFSET;
    model->lightMtx                  = &_gFootstepWalkWork->light;
    model->colorMtx                  = &_gFootstepWalkWork->color;

    // Sample the cached root position with the actor's vertical light offset.
    lightSamplePosition.vx  = rootCoord->workm.t[0];
    lightSamplePosition.vy  = rootCoord->workm.t[1] + FOOTSTEP_WALK_LIGHT_SAMPLE_Y_OFFSET;
    D_actor_260500_80159E50 = task;
    lightSamplePosition.vz  = rootCoord->workm.t[2];
    worldCoordSetModelLighting(model, &lightSamplePosition, 0, ARRAY_SIZE(model->colorMtx->m[0]));

    // Reset the driven slots before entering the ordinary frame state.
    _actor260500PrepareWalkerAnimation(model);
    task->msgTable = D_actor_260500_80159D80;
    _footstepWalkQuietUpdate(task);
    task->state++;
}

#include "../../shared/footstep_walk_quiet_update.inc.c"

/// Dispatches actor 260500's quiet-walker spawn or ordinary model frame.
///
/// The descriptor supplies a nineteen-part model and a live enemy in
/// `spawnArg2.pointer`. State must be `ACTOR_260500_TASK_SPAWN` or
/// `ACTOR_260500_TASK_FRAME`; indexing is unchecked. Publishes the task's
/// current work before dispatch, including its initially NULL work at spawn.
static void _actor260500Task(Task* task)
{
    void (*stateHandlers[ACTOR_260500_TASK_STATE_COUNT])(Enemy*, Task*) = {
        _footstepWalkQuietSpawn,
        _actorRenderWalkerFrame,
    };

    _gFootstepWalkWork = task->work;
    stateHandlers[task->state](task->spawnArg2.pointer, task);
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
#define ACTOR_RENDER_UPDATE_WALKER _footstepWalkQuietUpdate
/// Selects the declared static void(Task*) ground-shadow drawer for this inclusion.
#define ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW _actorRenderDrawFixedWalkerGroundShadow
#include "../../shared/walker_frame.inc.c"
#undef ACTOR_RENDER_WALKER_FRAME
#undef ACTOR_RENDER_UPDATE_WALKER
#undef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW

/// Releases the walker's enemy and starts default task/work/model teardown.
///
/// `task` must be live with its enemy in `spawnArg2.pointer`. Published task
/// and work aliases are left unchanged and must not be used afterwards.
static void _footstepWalkExit(Task* task)
{
    enemyDestroy(task->spawnArg2.pointer, task);
}

#include "../../shared/footstep_walk_tick_anim.inc.c"

#include "../../shared/footstep_walk_quiet_reset_anim.inc.c"

#include "../../shared/footstep_walk_quiet_blend_anim.inc.c"

/// Starts the published quiet walker's requested animation synchronously.
///
/// Handles `ACTOR_MESSAGE_PLAY_ANIMATION`; receiver, ID and second payload
/// are ignored. The work and published actor task must be live. Borrows the
/// request through dispatch, selecting this package's native clip table;
/// bank and collision options are ignored. Nonzero blend uses whole-frame
/// `blendFrames` (0..2047), narrowed to s16; reset leaves the duration unchanged.
///
/// Returns -1 without writes for IDs 36 and above, otherwise zero. The upper
/// bound alone admits negative IDs and NULL table entries; callers must select
/// a loaded clip with tracks 1..18 and keep the clip storage live for playback.
static s32 _footstepWalkQuietPlayAnimation(Task* unusedTask, s32 messageId, const AnimationPlayRequest* request, s32 unusedArgument)
{
    enum { FOOTSTEP_WALK_CLIP_ID_LIMIT = 36 };

    if (request->animationId < FOOTSTEP_WALK_CLIP_ID_LIMIT) {
        _gFootstepWalkWork->st.animId = request->animationId;
        if (request->blend != ANIMATION_BLEND_RESET) {
            _gFootstepWalkWork->st.state = ACTOR_ENEMY_ANIM_BLEND;
            _gFootstepWalkBlendFrames    = request->blendFrames;
        } else {
            _gFootstepWalkWork->st.state = ACTOR_ENEMY_ANIM_RESET;
        }
        _gFootstepWalkWork->st.field_6 = 0;
        _footstepWalkQuietUpdate(D_actor_260500_80159E50);
        return 0;
    }
    return -1;
}

/// Replaces the published walker's model flags from a draw-mode bitmask.
///
/// Handles `ACTOR_MESSAGE_SET_MODEL_DRAW` for a live published task and model;
/// ignores receiver, ID and second payload. Bit 0 permits active drawing,
/// bit 1 suppresses automatic primitive-buffer allocation, and other bits are
/// ignored. All prior model flags are discarded. Returns zero.
static s32 _footstepWalkSetModelDraw(Task* unusedTask, s32 messageId, s32 drawMode, s32 unusedArgument)
{
    enum {
        FOOTSTEP_WALK_DRAW_VISIBLE          = 1,
        FOOTSTEP_WALK_DRAW_SKIP_AUTO_BUFFER = 2
    };
    TmdObject* model;

    model = D_actor_260500_80159E50->extra.tmd;
    if (drawMode & FOOTSTEP_WALK_DRAW_VISIBLE) {
        model->flags = 0;
    } else {
        model->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    if (drawMode & FOOTSTEP_WALK_DRAW_SKIP_AUTO_BUFFER) {
        model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    }
    return 0;
}

#include "../../shared/footstep_walk_place.inc.c"

/// Schedules twenty turning updates for quiet-walker command 0.
///
/// Handles `ACTOR_COMMAND_MESSAGE_APPLY` through the live published work block.
/// Borrows the command for this call; ignores its context, receiver, ID and
/// second payload. Other commands do nothing. The count is consumed only while
/// the turn clip is playing, at 51/4096 turns per update; no clip is selected
/// here. Repeated requests restart the count. Returns zero.
static s32 _footstepWalkApplyTurnCommand(Task* unusedTask, s32 messageId, const ActorCommand* command, s32 unusedArgument)
{
    enum {
        FOOTSTEP_WALK_COMMAND_TURN = 0,
        FOOTSTEP_WALK_TURN_UPDATES = 20
    };

    if (command->command == FOOTSTEP_WALK_COMMAND_TURN) {
        _gFootstepWalkWork->turnFrames = FOOTSTEP_WALK_TURN_UPDATES;
    }
    return 0;
}

/// Faces a planar target and schedules whole moving updates to approach it.
///
/// Handles `ACTOR_MESSAGE_WALK_TO` for a live quiet-walker task and model.
/// Borrows `target` in the root's parent space; only X/Z are read. Mode must
/// be `FOOTSTEP_WALK_MODE_*` (0 forward 60, 1 backward 15, 2 forward 25
/// units per update). Backward mode faces away from the target. The mode and
/// heading narrow to s16; angles use 4096 units per turn.
///
/// Stores floor(planar distance / step distance) in s16 `st.travel`, without
/// selecting a walk clip or retaining the target. Coordinate differences and
/// their squared sum must fit s32, and callers must keep the count in 0..32767.
/// Mode bounds are unchecked; an unsupported narrowed mode divides by zero.
/// Returns zero; message ID is ignored.
static s32 _footstepWalkSetWalkTarget(Task* task, s32 messageId, const VECTOR* target, s32 mode)
{
    GfxCoord*              rootCoord;
    FootstepWalkQuietWork* work;
    s32                    stepDistance;
    s32                    deltaX;
    s32                    deltaZ;
    s32                    planarDistance;
    s32                    targetYaw;

    stepDistance       = 0;
    rootCoord          = task->extra.tmd->coords;
    work               = task->work;
    _gFootstepWalkMode = mode;
    deltaX             = target->vx - rootCoord->coord.t[0];
    deltaZ             = target->vz - rootCoord->coord.t[2];
    targetYaw          = ratan2(deltaX, deltaZ);
    work->st.yaw       = targetYaw;
    if (_gFootstepWalkMode == FOOTSTEP_WALK_MODE_BACKWARD) {
        work->st.yaw = targetYaw + ACTOR_TRANSFORM_ANGLE_HALF_TURN;
    }
    gfxRotMatrixY(&rootCoord->coord, work->st.yaw, GRAPHICS_ROTATION_REPLACE);
    // Travel counts moving updates; a fractional update is discarded.
    planarDistance = SquareRoot0(deltaX * deltaX + deltaZ * deltaZ);
    switch (_gFootstepWalkMode) {
        case FOOTSTEP_WALK_MODE_FORWARD:
            stepDistance = FOOTSTEP_WALK_FORWARD_DISTANCE;
            break;
        case FOOTSTEP_WALK_MODE_BACKWARD:
            stepDistance = FOOTSTEP_WALK_BACKWARD_DISTANCE;
            break;
        case FOOTSTEP_WALK_MODE_SLOW_FORWARD:
            stepDistance = FOOTSTEP_WALK_SLOW_FORWARD_DISTANCE;
            break;
    }
    work->st.travel = planarDistance / stepDistance;
    return 0;
}

#define ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW _actorRenderDrawFixedWalkerGroundShadow
#include "../../shared/walker_shadow.inc.c"
#undef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW
