#include "actors/actor_450900.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/rand.h>

#include "common.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/collision.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/areas.h"
#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "rooms/shelter_b6_growth_room.h"

extern s32                  D_map_neo_ark_8017A99C;
extern s8                   D_actor_450900_80135E70;
extern s32                  D_actor_450900_80135E74;
extern AnimationPlayRequest D_actor_450900_80135FEC;
extern AnimationPlayRequest D_actor_450900_801360B4;
extern ActorTransform       D_actor_450900_80136458;
extern EvsCommand           D_actor_450900_80136470[];
extern EvsCommand           D_actor_450900_80136680[];
extern EvsCommand           D_actor_450900_80136890[];
extern EvsCommand           D_actor_450900_80136B00[];
extern EvsCommand           D_actor_450900_80136BD8[];
extern s32                  D_actor_450900_80136C98;

/// The save-point capture task spawned by `_actor450900CompanionDistressTask`, kept
/// alive until `_actor450900HeadAimTask` kills it. Script opcode 0xD reaches
/// both this and `_actor450900SetActorControl`, so its one argument is the
/// opcode's immediate.
extern Task* D_actor_450900_80136C9C;

/// This overlay's own spawn table, six `TaskDesc` entries. Index 0 is the exit
/// handler `taskKill`; 1..5 are the overlay's state handlers, and the "next
/// stage" of each is the next entry: `_actor450900CompanionDistressTask` spawns 5 on
/// its way through, and `actor450900HandleDepartureTrigger` spawns 4
/// (`_actor450900DepartureTask`, the save-data teardown) when the ally has
/// walked past the trigger line.
extern TaskDesc D_actor_450900_80135E78[];

static AnimationSet _gActor450900Animation00F80;
static AnimationSet _gActor450900Animation012D8;
static AnimationSet _gActor450900Animation0176C;
static AnimationSet _gActor450900Animation01934;
static AnimationSet _gActor450900Animation01B50;
static AnimationSet _gActor450900Animation01D04;
static AnimationSet _gActor450900Animation020EC;
static AnimationSet _gActor450900Animation02428;
static AnimationSet _gActor450900Animation025F0;
static AnimationSet _gActor450900Animation02A30;
static AnimationSet _gActor450900Animation030F4;
static AnimationSet _gActor450900Animation03338;
static AnimationSet _gActor450900Animation03738;
static AnimationSet _gActor450900Animation03934;
static AnimationSet _gActor450900Animation03C70;
static AnimationSet _gActor450900Animation04028;

extern AnimationPlayRequest D_actor_450900_80135F74;
extern AnimationPlayRequest D_actor_450900_80136014;
extern AnimationPlayRequest D_actor_450900_80136028;
extern AnimationPlayRequest D_actor_450900_8013603C;
extern AnimationPlayRequest D_actor_450900_80136050;
extern AnimationPlayRequest D_actor_450900_80136064;
extern AnimationPlayRequest D_actor_450900_80136078;

// Shared timing and callback selectors for this package's growth-room scenes.
enum {
    ACTOR_450900_REACTION_PERIOD_TICKS          = 210,
    ACTOR_450900_COMPANION_DISTRESS_START_TICKS = 780,
    ACTOR_450900_HEAD_AIM_DISABLE               = 1,
    ACTOR_450900_HEAD_AIM_ENABLE                = 2,
    ACTOR_450900_COMPANION_VOICE_RANDOM         = 0,
    ACTOR_450900_COMPANION_VOICE_FIXED          = 1,
    ACTOR_450900_DEPARTURE_COMMAND              = 11,
};

static void _actor450900CompanionDistressTask(Task* task);
static void _actor450900PlayerReactionTask(Task* task);
static void _actor450900GrowthRoomActionTask(Task* task);
static void _actor450900DepartureTask(Task* task);
static void _actor450900ControlHeadAim(s32 command);
static void _actor450900HeadAimTask(Task* task);
static void _actor450900SetActorControl(u8 actorControl);
static void _actor450900PlayCompanionVoice(s32 voiceSelector);
static void _actor450900PreparePlayerFacingCompanion(void);

static AnimationPackedPose _gActor450900Animation00F80Bank1[5] = {
#include "assets/actor_450900_animation_00F80_bank1.inc"
};

static AnimationPackedRotation _gActor450900Animation00F80Bank4[101] = {
#include "assets/actor_450900_animation_00F80_bank4.inc"
};

static AnimationRecord _gActor450900Animation00F80Records[193] = {
#include "assets/actor_450900_animation_00F80_records.inc"
};

static u16 _gActor450900Animation00F80Indices[20] = {
#include "assets/actor_450900_animation_00F80_indices.inc"
};

static AnimationSet _gActor450900Animation00F80 = {
    _gActor450900Animation00F80Records,
    _gActor450900Animation00F80Indices,
    { NULL, _gActor450900Animation00F80Bank1, NULL, NULL, _gActor450900Animation00F80Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450900Animation012D8Bank1[4] = {
#include "assets/actor_450900_animation_012D8_bank1.inc"
};

static AnimationPackedRotation _gActor450900Animation012D8Bank4[41] = {
#include "assets/actor_450900_animation_012D8_bank4.inc"
};

static AnimationRecord _gActor450900Animation012D8Records[141] = {
#include "assets/actor_450900_animation_012D8_records.inc"
};

static u16 _gActor450900Animation012D8Indices[20] = {
#include "assets/actor_450900_animation_012D8_indices.inc"
};

static AnimationSet _gActor450900Animation012D8 = {
    _gActor450900Animation012D8Records,
    _gActor450900Animation012D8Indices,
    { NULL, _gActor450900Animation012D8Bank1, NULL, NULL, _gActor450900Animation012D8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450900Animation0176CBank1[8] = {
#include "assets/actor_450900_animation_0176C_bank1.inc"
};

static AnimationPackedRotation _gActor450900Animation0176CBank4[90] = {
#include "assets/actor_450900_animation_0176C_bank4.inc"
};

static AnimationRecord _gActor450900Animation0176CRecords[159] = {
#include "assets/actor_450900_animation_0176C_records.inc"
};

static u16 _gActor450900Animation0176CIndices[20] = {
#include "assets/actor_450900_animation_0176C_indices.inc"
};

static AnimationSet _gActor450900Animation0176C = {
    _gActor450900Animation0176CRecords,
    _gActor450900Animation0176CIndices,
    { NULL, _gActor450900Animation0176CBank1, NULL, NULL, _gActor450900Animation0176CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450900Animation01934Bank1[2] = {
#include "assets/actor_450900_animation_01934_bank1.inc"
};

static AnimationPackedRotation _gActor450900Animation01934Bank4[21] = {
#include "assets/actor_450900_animation_01934_bank4.inc"
};

static AnimationRecord _gActor450900Animation01934Records[67] = {
#include "assets/actor_450900_animation_01934_records.inc"
};

static u16 _gActor450900Animation01934Indices[20] = {
#include "assets/actor_450900_animation_01934_indices.inc"
};

static AnimationSet _gActor450900Animation01934 = {
    _gActor450900Animation01934Records,
    _gActor450900Animation01934Indices,
    { NULL, _gActor450900Animation01934Bank1, NULL, NULL, _gActor450900Animation01934Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450900Animation01B50Bank1[2] = {
#include "assets/actor_450900_animation_01B50_bank1.inc"
};

static AnimationPackedRotation _gActor450900Animation01B50Bank4[28] = {
#include "assets/actor_450900_animation_01B50_bank4.inc"
};

static AnimationRecord _gActor450900Animation01B50Records[81] = {
#include "assets/actor_450900_animation_01B50_records.inc"
};

static u16 _gActor450900Animation01B50Indices[20] = {
#include "assets/actor_450900_animation_01B50_indices.inc"
};

static AnimationSet _gActor450900Animation01B50 = {
    _gActor450900Animation01B50Records,
    _gActor450900Animation01B50Indices,
    { NULL, _gActor450900Animation01B50Bank1, NULL, NULL, _gActor450900Animation01B50Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450900Animation01D04Bank1[2] = {
#include "assets/actor_450900_animation_01D04_bank1.inc"
};

static AnimationPackedRotation _gActor450900Animation01D04Bank4[21] = {
#include "assets/actor_450900_animation_01D04_bank4.inc"
};

static AnimationRecord _gActor450900Animation01D04Records[62] = {
#include "assets/actor_450900_animation_01D04_records.inc"
};

static u16 _gActor450900Animation01D04Indices[20] = {
#include "assets/actor_450900_animation_01D04_indices.inc"
};

static AnimationSet _gActor450900Animation01D04 = {
    _gActor450900Animation01D04Records,
    _gActor450900Animation01D04Indices,
    { NULL, _gActor450900Animation01D04Bank1, NULL, NULL, _gActor450900Animation01D04Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450900Animation020ECBank1[5] = {
#include "assets/actor_450900_animation_020EC_bank1.inc"
};

static AnimationPackedRotation _gActor450900Animation020ECBank4[67] = {
#include "assets/actor_450900_animation_020EC_bank4.inc"
};

static AnimationRecord _gActor450900Animation020ECRecords[148] = {
#include "assets/actor_450900_animation_020EC_records.inc"
};

static u16 _gActor450900Animation020ECIndices[20] = {
#include "assets/actor_450900_animation_020EC_indices.inc"
};

static AnimationSet _gActor450900Animation020EC = {
    _gActor450900Animation020ECRecords,
    _gActor450900Animation020ECIndices,
    { NULL, _gActor450900Animation020ECBank1, NULL, NULL, _gActor450900Animation020ECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450900Animation02428Bank1[3] = {
#include "assets/actor_450900_animation_02428_bank1.inc"
};

static AnimationPackedRotation _gActor450900Animation02428Bank4[53] = {
#include "assets/actor_450900_animation_02428_bank4.inc"
};

static AnimationRecord _gActor450900Animation02428Records[125] = {
#include "assets/actor_450900_animation_02428_records.inc"
};

static u16 _gActor450900Animation02428Indices[20] = {
#include "assets/actor_450900_animation_02428_indices.inc"
};

static AnimationSet _gActor450900Animation02428 = {
    _gActor450900Animation02428Records,
    _gActor450900Animation02428Indices,
    { NULL, _gActor450900Animation02428Bank1, NULL, NULL, _gActor450900Animation02428Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450900Animation025F0Bank1[2] = {
#include "assets/actor_450900_animation_025F0_bank1.inc"
};

static AnimationPackedRotation _gActor450900Animation025F0Bank4[23] = {
#include "assets/actor_450900_animation_025F0_bank4.inc"
};

static AnimationRecord _gActor450900Animation025F0Records[65] = {
#include "assets/actor_450900_animation_025F0_records.inc"
};

static u16 _gActor450900Animation025F0Indices[20] = {
#include "assets/actor_450900_animation_025F0_indices.inc"
};

static AnimationSet _gActor450900Animation025F0 = {
    _gActor450900Animation025F0Records,
    _gActor450900Animation025F0Indices,
    { NULL, _gActor450900Animation025F0Bank1, NULL, NULL, _gActor450900Animation025F0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450900Animation02A30Bank1[6] = {
#include "assets/actor_450900_animation_02A30_bank1.inc"
};

static AnimationPackedRotation _gActor450900Animation02A30Bank4[89] = {
#include "assets/actor_450900_animation_02A30_bank4.inc"
};

static AnimationRecord _gActor450900Animation02A30Records[145] = {
#include "assets/actor_450900_animation_02A30_records.inc"
};

static u16 _gActor450900Animation02A30Indices[20] = {
#include "assets/actor_450900_animation_02A30_indices.inc"
};

static AnimationSet _gActor450900Animation02A30 = {
    _gActor450900Animation02A30Records,
    _gActor450900Animation02A30Indices,
    { NULL, _gActor450900Animation02A30Bank1, NULL, NULL, _gActor450900Animation02A30Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450900Animation030F4Bank1[4] = {
#include "assets/actor_450900_animation_030F4_bank1.inc"
};

static AnimationPackedRotation _gActor450900Animation030F4Bank4[151] = {
#include "assets/actor_450900_animation_030F4_bank4.inc"
};

static AnimationRecord _gActor450900Animation030F4Records[250] = {
#include "assets/actor_450900_animation_030F4_records.inc"
};

static u16 _gActor450900Animation030F4Indices[20] = {
#include "assets/actor_450900_animation_030F4_indices.inc"
};

static AnimationSet _gActor450900Animation030F4 = {
    _gActor450900Animation030F4Records,
    _gActor450900Animation030F4Indices,
    { NULL, _gActor450900Animation030F4Bank1, NULL, NULL, _gActor450900Animation030F4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450900Animation03338Bank1[3] = {
#include "assets/actor_450900_animation_03338_bank1.inc"
};

static AnimationPackedRotation _gActor450900Animation03338Bank4[28] = {
#include "assets/actor_450900_animation_03338_bank4.inc"
};

static AnimationRecord _gActor450900Animation03338Records[88] = {
#include "assets/actor_450900_animation_03338_records.inc"
};

static u16 _gActor450900Animation03338Indices[20] = {
#include "assets/actor_450900_animation_03338_indices.inc"
};

static AnimationSet _gActor450900Animation03338 = {
    _gActor450900Animation03338Records,
    _gActor450900Animation03338Indices,
    { NULL, _gActor450900Animation03338Bank1, NULL, NULL, _gActor450900Animation03338Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450900Animation03738Bank1[10] = {
#include "assets/actor_450900_animation_03738_bank1.inc"
};

static AnimationPackedRotation _gActor450900Animation03738Bank4[85] = {
#include "assets/actor_450900_animation_03738_bank4.inc"
};

static AnimationRecord _gActor450900Animation03738Records[121] = {
#include "assets/actor_450900_animation_03738_records.inc"
};

static u16 _gActor450900Animation03738Indices[20] = {
#include "assets/actor_450900_animation_03738_indices.inc"
};

static AnimationSet _gActor450900Animation03738 = {
    _gActor450900Animation03738Records,
    _gActor450900Animation03738Indices,
    { NULL, _gActor450900Animation03738Bank1, NULL, NULL, _gActor450900Animation03738Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450900Animation03934Bank1[4] = {
#include "assets/actor_450900_animation_03934_bank1.inc"
};

static AnimationPackedRotation _gActor450900Animation03934Bank4[17] = {
#include "assets/actor_450900_animation_03934_bank4.inc"
};

static AnimationRecord _gActor450900Animation03934Records[78] = {
#include "assets/actor_450900_animation_03934_records.inc"
};

static u16 _gActor450900Animation03934Indices[20] = {
#include "assets/actor_450900_animation_03934_indices.inc"
};

static AnimationSet _gActor450900Animation03934 = {
    _gActor450900Animation03934Records,
    _gActor450900Animation03934Indices,
    { NULL, _gActor450900Animation03934Bank1, NULL, NULL, _gActor450900Animation03934Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450900Animation03C70Bank1[8] = {
#include "assets/actor_450900_animation_03C70_bank1.inc"
};

static AnimationPackedRotation _gActor450900Animation03C70Bank4[65] = {
#include "assets/actor_450900_animation_03C70_bank4.inc"
};

static AnimationRecord _gActor450900Animation03C70Records[98] = {
#include "assets/actor_450900_animation_03C70_records.inc"
};

static u16 _gActor450900Animation03C70Indices[20] = {
#include "assets/actor_450900_animation_03C70_indices.inc"
};

static AnimationSet _gActor450900Animation03C70 = {
    _gActor450900Animation03C70Records,
    _gActor450900Animation03C70Indices,
    { NULL, _gActor450900Animation03C70Bank1, NULL, NULL, _gActor450900Animation03C70Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor450900Animation04028Bank1[3] = {
#include "assets/actor_450900_animation_04028_bank1.inc"
};

static AnimationPackedRotation _gActor450900Animation04028Bank4[81] = {
#include "assets/actor_450900_animation_04028_bank4.inc"
};

static AnimationRecord _gActor450900Animation04028Records[128] = {
#include "assets/actor_450900_animation_04028_records.inc"
};

static u16 _gActor450900Animation04028Indices[20] = {
#include "assets/actor_450900_animation_04028_indices.inc"
};

static AnimationSet _gActor450900Animation04028 = {
    _gActor450900Animation04028Records,
    _gActor450900Animation04028Indices,
    { NULL, _gActor450900Animation04028Bank1, NULL, NULL, _gActor450900Animation04028Bank4, NULL, NULL, NULL },
};

s8 D_actor_450900_80135E70 = 0;

s32 D_actor_450900_80135E74 = 0;

TaskDesc D_actor_450900_80135E78[6] = {
    { { { TASK_BODY_NONE, 192 } }, taskKill, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _actor450900CompanionDistressTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _actor450900PlayerReactionTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _actor450900GrowthRoomActionTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _actor450900DepartureTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 97 } }, _actor450900HeadAimTask, { .value = 0 } },
};

/// Player clips for extended ids 47-56.
///
/// The package's event scripts and its two periodic reaction handlers send a
/// receiver its copy request before they play any of the clips below on it.
/// `D_actor_450900_80135F08` copies `ANIMATION_BANK_EXTENSION_CAPACITY` (32)
/// words starting here into the player's bank, which is 22 words past the end
/// of this array: the read runs on through `D_actor_450900_80135EE8`, both copy
/// requests, `D_actor_450900_80135F10`, `Actor450900AllyAnim` and the first two
/// words of `D_actor_450900_80135F38`. That overrun is the original's and is
/// kept as it is. The requests played on the player select ids 51 and 53-56, so
/// none of the words installed after the ten clips is played as one.
AnimationSet* D_actor_450900_80135EC0[10] = { &_gActor450900Animation020EC, &_gActor450900Animation02428, &_gActor450900Animation025F0, &_gActor450900Animation02A30, &_gActor450900Animation030F4, &_gActor450900Animation04028, &_gActor450900Animation03338, &_gActor450900Animation03738, &_gActor450900Animation03934, &_gActor450900Animation03C70 };

/// Companion clips for extended ids 47-52 of the companion's bank.
///
/// `D_actor_450900_80135F00` copies `ANIMATION_BANK_EXTENSION_CAPACITY` (32)
/// words starting here into the companion's bank, which is 26 words past the
/// end of this array: the read runs on through both copy requests,
/// `D_actor_450900_80135F10`, `Actor450900AllyAnim`, `D_actor_450900_80135F38`
/// and the first seven words of `D_actor_450900_80135F4C`. That overrun is the
/// original's and is kept as it is. The requests played on the companion select
/// ids 47, 48, 51 and 52, so none of the words installed after the six clips is
/// played as one.
AnimationSet* D_actor_450900_80135EE8[6] = { &_gActor450900Animation00F80, &_gActor450900Animation012D8, &_gActor450900Animation0176C, &_gActor450900Animation01934, &_gActor450900Animation01B50, &_gActor450900Animation01D04 };

// Installs the companion's clips; the count is the bank's capacity, not the six entries of its source.
AnimationBankCopyRequest D_actor_450900_80135F00 = { { .sets = D_actor_450900_80135EE8 }, ANIMATION_BANK_EXTENSION_CAPACITY };

// Installs the player's clips; the count is the bank's capacity, not the ten entries of its source.
AnimationBankCopyRequest D_actor_450900_80135F08 = { { .sets = D_actor_450900_80135EC0 }, ANIMATION_BANK_EXTENSION_CAPACITY };

// Requests for the companion's extended ids start here. This one is not referenced.
AnimationPlayRequest D_actor_450900_80135F10 = { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

// The companion's periodic handler fills in the bank index in place before playing it.
AnimationPlayRequest Actor450900AllyAnim = { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450900_80135F38 = { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

// Not referenced.
AnimationPlayRequest D_actor_450900_80135F4C[2] = {
    { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
};

AnimationPlayRequest D_actor_450900_80135F74 = { { .index = 1 }, 51, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450900_80135F88[5] = {
    { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE },
    { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE },
};

AnimationPlayRequest D_actor_450900_80135FEC = { { .index = 1 }, 51, ANIMATION_BLEND_INTERPOLATE, 12, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_450900_80136000 = { { .index = 1 }, 52, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450900_80136014 = { { .index = 1 }, 53, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_450900_80136028 = { { .index = 1 }, 54, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450900_8013603C = { { .index = 1 }, 55, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450900_80136050 = { { .index = 1 }, 56, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450900_80136064 = { { .index = 1 }, 52, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450900_80136078 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_450900_8013608C = { { .index = 1 }, 1, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_450900_801360A0 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_450900_801360B4 = { { .index = 1 }, 1, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_actor_450900_801360C8 = { { 3450, 0, 6378, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_actor_450900_801360E0 = { { 3300, 0, 6378, 0 }, { 0, 2048, 0, 0 } };

ActorTransform D_actor_450900_801360F8 = { { 750, 0, 3710, 0 }, { 0, -1024, 0, 0 } };

EvsCommand D_actor_450900_80136110[21] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_450900_80135F08 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_450900_80135F00 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_450900_801360F8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 10 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = shelterB6GrowthRoomResetCollisionBox }, { .value = false }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450900_801360A0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 7 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450900_80136014 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450900_801360A0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_450900_801360C8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450900_80136078 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_450900_80136308[14] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_450900_801360C8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_450900_801360F8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450900_80136078 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

ActorTransform D_actor_450900_80136458 = { { 0, 0, 0, 0 }, { 0, -1024, 0, 0 } };

EvsCommand D_actor_450900_80136470[22] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_450900_801360C8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 3 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_450900_80135F08 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_450900_80135F00 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor450900PreparePlayerFacingCompanion }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1006 }, { .message = { .pointer = &D_actor_450900_80136458 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1018 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450900ControlHeadAim }, { .value = ACTOR_450900_HEAD_AIM_ENABLE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = _actor450900SetActorControl }, { .value = SCENE_COMBAT_ACTORS_PAUSED }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450900_80136064 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450900_801360A0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1018 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450900ControlHeadAim }, { .value = ACTOR_450900_HEAD_AIM_DISABLE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = _actor450900SetActorControl }, { .value = SCENE_COMBAT_ACTORS_RUNNING }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_450900_80136680[22] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = _actor450900SetActorControl }, { .value = SCENE_COMBAT_ACTORS_PAUSED }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor450900PreparePlayerFacingCompanion }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1006 }, { .message = { .pointer = &D_actor_450900_80136458 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1018 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450900ControlHeadAim }, { .value = ACTOR_450900_HEAD_AIM_ENABLE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450900_80136064 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450900_80135F38 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450900PlayCompanionVoice }, { .value = ACTOR_450900_COMPANION_VOICE_FIXED }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450900_801360B4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1018 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450900ControlHeadAim }, { .value = ACTOR_450900_HEAD_AIM_DISABLE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = _actor450900SetActorControl }, { .value = SCENE_COMBAT_ACTORS_RUNNING }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_450900_80136890[26] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = shelterB6GrowthRoomResetCollisionBox }, { .value = true }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 5 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = _actor450900SetActorControl }, { .value = SCENE_COMBAT_ACTORS_PAUSED }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_450900_80135F08 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_450900_80135F00 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor450900PreparePlayerFacingCompanion }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1006 }, { .message = { .pointer = &D_actor_450900_80136458 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1018 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450900ControlHeadAim }, { .value = ACTOR_450900_HEAD_AIM_ENABLE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450900_80135F74 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1006 }, { .message = { .pointer = &D_actor_450900_801360E0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1018 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor450900ControlHeadAim }, { .value = ACTOR_450900_HEAD_AIM_DISABLE }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = _actor450900SetActorControl }, { .value = SCENE_COMBAT_ACTORS_RUNNING }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_450900_80136B00[9] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_450900_80135F08 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450900_80136028 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 23 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450900_8013603C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450900_80136050 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 16 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_450900_80136BD8[8] = {
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 120 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x55170001 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_EVENT_STATE, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 900 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

s32 D_actor_450900_80136C98;

Task* D_actor_450900_80136C9C;

void func_actor_450900_801327A8(void);

/// Advances growth-room distress timing and periodically animates and hurts the companion.
///
/// State 0 spawns the tracked head-aim task. State 1 advances the persistent
/// counter only outside CAP/event playback and actor pauses, except in demo 11.
/// From tick 780, while the companion has HP and the conversation latch is zero,
/// every 210 ticks plays a random spatial voice and a power-16 damage reaction;
/// 60 ticks later restores its idle pose. Requires live companion/model and
/// gameplay sound, animation and damage resources; messages borrow the requests
/// synchronously. Other states do nothing.
static void _actor450900CompanionDistressTask(Task* task)
{
    enum {
        ACTOR_450900_DISTRESS_INIT             = 0,
        ACTOR_450900_DISTRESS_RUN              = 1,
        ACTOR_450900_HEAD_AIM_TASK_INDEX       = 5,
        ACTOR_450900_DISTRESS_IDLE_DELAY_TICKS = 60,
        ACTOR_450900_DISTRESS_DEMO_SCENE       = 11,
        ACTOR_450900_DISTRESS_ATTACK_KEY       = DAMAGE_ATTACK_CATEGORY | 16,
    };
    GfxCoord* companionCoord;
    s32       state;
    s32       distressTicks;
    s8        pan;
    s8        depth;
    Task*     companionTask;

    companionTask = gameGetTaskSlot(GAME_TASK_SLOT_COMPANION);
    state         = task->state;
    switch (state) {
        case ACTOR_450900_DISTRESS_INIT:
            D_actor_450900_80135E70 = 0;
            D_actor_450900_80136C9C = taskSpawnFromTable(D_actor_450900_80135E78, ACTOR_450900_HEAD_AIM_TASK_INDEX, 0, 0);
            task->state             = task->state + 1;
            break;
        case ACTOR_450900_DISTRESS_RUN:
            if (capIsBusy() != 0) {
                break;
            }
            if (gGameSession->eventState != 0) {
                break;
            }
            if (gSceneCombatState.actorControl != SCENE_COMBAT_ACTORS_RUNNING) {
                break;
            }
            // Count eligible updates across visits; demo playback retains the counter.
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene != ACTOR_450900_DISTRESS_DEMO_SCENE) {
                D_map_neo_ark_8017A99C = D_map_neo_ark_8017A99C + 1;
            }
            distressTicks = D_map_neo_ark_8017A99C - ACTOR_450900_COMPANION_DISTRESS_START_TICKS;
            if (D_actor_450900_80135E74 == 0 && gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionHp > 0 && distressTicks >= 0) {
                D_actor_450900_80135E70 = state;
                if (distressTicks % ACTOR_450900_REACTION_PERIOD_TICKS == 0) {
                    companionCoord = gameGetTaskSlot(GAME_TASK_SLOT_COMPANION)->extra.tmd->coords;
                    pan            = (s8)worldCoordGetOriginAudioPan(companionCoord);
                    depth          = (s8)worldCoordGetOriginAudioDepth(companionCoord);
                    if (rand() & 1) {
                        sndEvtRequestScriptStart(SOUND_SHELTER_B6_GROWTH_ALLY_VOICE_1, pan, depth);
                    } else {
                        sndEvtRequestScriptStart(SOUND_SHELTER_B6_GROWTH_ALLY_VOICE_2, pan, depth);
                    }
                    companionWriteAnimationBankIndex(&Actor450900AllyAnim.source.index);
                    TASK_MESSAGE_DISPATCH_POINTER(companionTask, ANIMATION_MESSAGE_COPY_BANK_EXTENSION, &D_actor_450900_80135F00, 0);
                    TASK_MESSAGE_DISPATCH_POINTER(companionTask, ANIMATION_MESSAGE_PLAY, &Actor450900AllyAnim, 0);
                    taskMessageDispatch(companionTask, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, ACTOR_450900_DISTRESS_ATTACK_KEY, 0);
                } else if (distressTicks % ACTOR_450900_REACTION_PERIOD_TICKS == ACTOR_450900_DISTRESS_IDLE_DELAY_TICKS) {
                    companionWriteAnimationBankIndex(&D_actor_450900_801360B4.source.index);
                    TASK_MESSAGE_DISPATCH_POINTER(companionTask, ANIMATION_MESSAGE_PLAY, &D_actor_450900_801360B4, 0);
                }
            }
            break;
    }
}

/// Plays the player's periodic growth-room response after the distress timer reaches 1110.
///
/// Outside CAP/event playback and actor pauses, starts a spatial voice and the
/// extended response animation every 210 counter ticks. Seventy ticks later
/// increments the completion counter and releases scripted player control.
/// Requires a live player TMD and this package's animation and sound resources.
/// State 0 clears the task countdown; state 1 runs; other states do nothing.
static void _actor450900PlayerReactionTask(Task* task)
{
    enum {
        ACTOR_450900_PLAYER_REACTION_INIT        = 0,
        ACTOR_450900_PLAYER_REACTION_RUN         = 1,
        ACTOR_450900_PLAYER_REACTION_START_TICKS = 1110,
        ACTOR_450900_PLAYER_RESPONSE_TICKS       = 70,
    };
    GfxCoord* playerCoord;
    Task*     playerTask;
    s32       stateOrReactionCount;
    s8        pan;
    s8        depth;

    playerTask           = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    stateOrReactionCount = task->state;
    switch (stateOrReactionCount) {
        case ACTOR_450900_PLAYER_REACTION_INIT:
            task->killCountdown = 0;
            task->state         = task->state + 1;
            return;
        case ACTOR_450900_PLAYER_REACTION_RUN:
            if ((capIsBusy() == 0) && (gGameSession->eventState == 0) && (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) && ((D_map_neo_ark_8017A99C - ACTOR_450900_PLAYER_REACTION_START_TICKS) >= 0)) {
                if ((D_map_neo_ark_8017A99C - ACTOR_450900_PLAYER_REACTION_START_TICKS) % ACTOR_450900_REACTION_PERIOD_TICKS == 0) {
                    playerCoord = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
                    pan         = (s8)worldCoordGetOriginAudioPan(playerCoord);
                    depth       = (s8)worldCoordGetOriginAudioDepth(playerCoord);
                    if (rand() & 1) {
                        sndEvtRequestScriptStart(SOUND_SHELTER_B6_GROWTH_PLAYER_VOICE_1, pan, depth);
                    } else {
                        sndEvtRequestScriptStart(SOUND_SHELTER_B6_GROWTH_PLAYER_VOICE_2, pan, depth);
                    }
                    TASK_MESSAGE_DISPATCH_POINTER(playerTask, ANIMATION_MESSAGE_COPY_BANK_EXTENSION, &D_actor_450900_80135F08, 0);
                    playerActorWriteWeaponAnimationBankIndex(&D_actor_450900_80135FEC.source.index);
                    TASK_MESSAGE_DISPATCH_POINTER(playerTask, ANIMATION_MESSAGE_PLAY, &D_actor_450900_80135FEC, 0);
                } else if ((D_map_neo_ark_8017A99C - ACTOR_450900_PLAYER_REACTION_START_TICKS) % ACTOR_450900_REACTION_PERIOD_TICKS == ACTOR_450900_PLAYER_RESPONSE_TICKS) {
                    // The shared temporary and equal arms retain the matching shape;
                    // the original counter condition is unproven.
                    stateOrReactionCount = D_actor_450900_80136C98;
                    stateOrReactionCount++;
                    D_actor_450900_80136C98 = stateOrReactionCount;
                    if (stateOrReactionCount != 0) {
                        taskMessageDispatch(playerTask, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                    } else {
                        taskMessageDispatch(playerTask, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                    }
                }
            }
            return;
    }
}

/// Runs the growth-room action choice and its optional player-animation follow-up.
///
/// CAP command 1 pauses actors. Variant 11 sets the persistent action flag,
/// starts command 2 and the player animation script; other variants resume play.
/// The follow-up resumes play when event state returns to zero. Requires this
/// package, the room's CAP commands, and the player model through completion.
static void _actor450900GrowthRoomActionTask(Task* task)
{
    enum {
        ACTOR_450900_ACTION_START             = 0,
        ACTOR_450900_ACTION_WAIT_CHOICE       = 1,
        ACTOR_450900_ACTION_HANDLE_CHOICE     = 2,
        ACTOR_450900_ACTION_WAIT_SCRIPT       = 3,
        ACTOR_450900_ACTION_COMMAND           = 1,
        ACTOR_450900_ACTION_FOLLOW_UP_COMMAND = 2,
        ACTOR_450900_ACTION_FOLLOW_UP_VARIANT = 11,
    };
    switch (task->state) {
        case ACTOR_450900_ACTION_START:
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            capRunCommand(ACTOR_450900_ACTION_COMMAND, CAP_PLAYBACK_IN_PLACE);
            task->state = task->state + 1;
            break;
        case ACTOR_450900_ACTION_WAIT_CHOICE:
            if (capIsBusy() == 0) {
                task->state = task->state + 1;
            }
            break;
        case ACTOR_450900_ACTION_HANDLE_CHOICE:
            if (capGetVariantKey() != ACTOR_450900_ACTION_FOLLOW_UP_VARIANT) {
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                taskKill(task);
                break;
            }
            gameFlagSetNibble(GAME_FLAG_0D8, 1);
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            capRunCommand(ACTOR_450900_ACTION_FOLLOW_UP_COMMAND, CAP_PLAYBACK_IN_PLACE);
            evsStartScript(D_actor_450900_80136B00, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            task->state = task->state + 1;
            break;
        case ACTOR_450900_ACTION_WAIT_SCRIPT:
            if (gGameSession->eventState == 0) {
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                taskKill(task);
            }
            break;
    }
}

/// Offers growth-room departure, then removes the companion and reloads Neo Ark Garden.
///
/// CAP command 11 uses variant 1. A returned key other than 11 resumes the
/// player and kills this task. Key 11 runs the fade script; its event-state 2
/// handshake commits story/route flags and destination room 1, warp 3, before
/// spawning the resident session-load task. Requires this package and the room's
/// CAP and event resources through the handshake. Other task states do nothing.
static void _actor450900DepartureTask(Task* task)
{
    enum {
        ACTOR_450900_DEPARTURE_START            = 0,
        ACTOR_450900_DEPARTURE_WAIT_CHOICE      = 1,
        ACTOR_450900_DEPARTURE_HANDLE_CHOICE    = 2,
        ACTOR_450900_DEPARTURE_WAIT_FADE        = 3,
        ACTOR_450900_DEPARTURE_COMMIT           = 4,
        ACTOR_450900_DEPARTURE_OFFER_VARIANT    = 1,
        ACTOR_450900_DEPARTURE_CONFIRMED_KEY    = 11,
        ACTOR_450900_DEPARTURE_FADE_READY       = 2,
        ACTOR_450900_DEPARTURE_STORY_DIALOGUE   = 8,
        ACTOR_450900_DEPARTURE_DESTINATION_WARP = 3,
        ACTOR_450900_DEPARTURE_DESTINATION_ROOM = 1,
        ACTOR_450900_COMPANION_NONE             = 0,
        ACTOR_450900_DEFAULT_DISPLAY_VARIANT    = 1,
        ACTOR_450900_SESSION_LOAD_TASK_INDEX    = 17,
    };
    switch (task->state) {
        case ACTOR_450900_DEPARTURE_START:
            capStartSequenceSlot(ACTOR_450900_DEPARTURE_COMMAND, CAP_PLAYBACK_DISPLAY_TRANSITION, ACTOR_450900_DEPARTURE_OFFER_VARIANT);
            task->state = task->state + 1;
            break;
        case ACTOR_450900_DEPARTURE_WAIT_CHOICE:
            if (capIsBusy() == 0) {
                task->state = task->state + 1;
            }
            break;
        case ACTOR_450900_DEPARTURE_HANDLE_CHOICE:
            if (capGetVariantKey() != ACTOR_450900_DEPARTURE_CONFIRMED_KEY) {
                taskKill(task);
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            } else {
                evsStartScript(D_actor_450900_80136BD8, EVENT_SCRIPT_HUD_HIDE_RESTORE);
                task->state = task->state + 1;
            }
            break;
        case ACTOR_450900_DEPARTURE_WAIT_FADE:
            if (gGameSession->eventState == ACTOR_450900_DEPARTURE_FADE_READY) {
                task->state = task->state + 1;
            }
            break;
        case ACTOR_450900_DEPARTURE_COMMIT:
            // Commit the post-companion route before the session-load task reads the save.
            gameFlagSetNibble(GAME_FLAG_COMPANION_3_SCHEDULE, 0);
            gameFlagSetNibble(GAME_FLAG_0FC, 1);
            gameFlagSetNibble(GAME_FLAG_B1_CORRIDOR_ELEVATOR_HALL_UNLOCKED, 0);
            gameFlagSetNibble(GAME_FLAG_NEO_ARK_ALTAR_SWITCH_STATE, 0);
            gameFlagSetNibble(GAME_FLAG_B2_CORRIDOR_ELEVATOR_HALL_UNLOCKED, 1);
            gameFlagSetNibble(GAME_FLAG_MAP_MARK_SHELTER_1C7, 0);
            gameFlagSetNibble(GAME_FLAG_SHELTER_WATCHERS_DISABLED, 0);
            gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, 0);
            gameFlagSetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX, ACTOR_450900_DEPARTURE_STORY_DIALOGUE);
            sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = GAME_AREA_NEO_ARK_GARDEN;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = ACTOR_450900_DEPARTURE_DESTINATION_WARP;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType     = ACTOR_450900_COMPANION_NONE;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = ACTOR_450900_DEPARTURE_DESTINATION_ROOM;
            gDisplayState.spriteVariant                                = ACTOR_450900_DEFAULT_DISPLAY_VARIANT;
            taskSpawn(0, ACTOR_450900_SESSION_LOAD_TASK_INDEX, 0, 0);
            streamFinishScene();
            taskKill(task);
            break;
    }
}

/// Sets whether the tracked task fades the player's head turn toward the companion.
///
/// Command 1 disables aiming; every other value enables it (scripts use 2).
/// Does nothing without a tracked task. Requires any retained handle to remain
/// live; this changes spawnArg1, leaving task state and lifetime untouched.
static void _actor450900ControlHeadAim(s32 command)
{
    if (D_actor_450900_80136C9C != NULL) {
        if (command == ACTOR_450900_HEAD_AIM_DISABLE) {
            D_actor_450900_80136C9C->spawnArg1.value = 0;
            return;
        }
        D_actor_450900_80136C9C->spawnArg1.value = 1;
    }
}

/// Fades the scene's Q12 head-aim blend weight toward full aiming or zero.
///
/// Requires writable `aim` with `rate` normally in 0..ONE; only that field
/// changes. Nonzero `enabled` adds ONE/8 and caps at ONE; zero subtracts ONE/8
/// and floors at zero. Holding either setting traverses the full range in
/// eight calls. The result narrows to a signed halfword before the selected
/// endpoint check, preserving wraparound for out-of-range inputs.
static inline void _actor450900RampHeadAimWeight(AnimationHeadAim* aim, s32 enabled)
{
    enum { ACTOR_450900_HEAD_AIM_FADE_TICKS = 8 };
    s16 nextWeight;

    if (enabled != 0) {
        nextWeight = aim->rate + ONE / ACTOR_450900_HEAD_AIM_FADE_TICKS;
        aim->rate  = nextWeight;
        if (nextWeight > ONE) {
            aim->rate = ONE;
        }
    } else {
        nextWeight = aim->rate - ONE / ACTOR_450900_HEAD_AIM_FADE_TICKS;
        aim->rate  = nextWeight;
        if (nextWeight < 0) {
            aim->rate = 0;
        }
    }
}

/// Fades the player's head turn toward the companion during growth-room scenes.
///
/// State 0 owns a zeroed `AnimationHeadAim` in `Task::work`, limits yaw to 1/16
/// turn and pitch to 1/8 turn, and immediately runs state 1. Nonzero spawnArg1
/// enables aiming. Both tasks require live TMD skeletons with five coordinates.
/// Other states kill this task, freeing its work and clearing the tracked
/// handle. Allocation failure kills without clearing that handle.
static void _actor450900HeadAimTask(Task* task)
{
    enum {
        ACTOR_450900_HEAD_AIM_INIT        = 0,
        ACTOR_450900_HEAD_AIM_RUN         = 1,
        ACTOR_450900_HEAD_AIM_YAW_LIMIT   = ACTOR_TRANSFORM_ANGLE_TURN / 16,
        ACTOR_450900_HEAD_AIM_PITCH_LIMIT = ACTOR_TRANSFORM_ANGLE_TURN / 8,
    };
    AnimationHeadAim* aim;
    Task*             playerTask;

    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    switch (task->state) {
        case ACTOR_450900_HEAD_AIM_INIT:
            aim = memCalloc(sizeof(AnimationHeadAim), false);
            if (aim == NULL) {
                taskKill(task);
                return;
            }
            task->work      = aim;
            aim->yawLimit   = ACTOR_450900_HEAD_AIM_YAW_LIMIT;
            aim->pitchLimit = ACTOR_450900_HEAD_AIM_PITCH_LIMIT;
            task->state++;
            /* fallthrough */
        case ACTOR_450900_HEAD_AIM_RUN:
            aim = task->work;
            _actor450900RampHeadAimWeight(aim, task->spawnArg1.value);
            animationAimHeadAt(playerTask, gameGetTaskSlot(GAME_TASK_SLOT_COMPANION), aim);
            return;
        default:
            taskKill(task);
            D_actor_450900_80136C9C = NULL;
            return;
    }
}

/// Supplies a script argument's low byte to the scene's actor-control gate.
///
/// Scripts use SCENE_COMBAT_ACTORS_RUNNING (0) and SCENE_COMBAT_ACTORS_PAUSED
/// (1); other byte values are stored unchanged. This does not change player
/// scripted-control state or resume tasks on its own.
static void _actor450900SetActorControl(u8 actorControl)
{
    gSceneCombatState.actorControl = actorControl;
}

/// Plays a spatial growth-room companion voice from its cached root coordinate.
///
/// Selector 0 randomly chooses voice 1 or 2; every nonzero selector uses voice 3.
/// Requires a live companion TMD, a composed local-to-view matrix, projection
/// scratch space and the room's loaded sound bank. Pan/depth retain signed-byte
/// narrowing before submission; the coordinate is borrowed only during the call.
static void _actor450900PlayCompanionVoice(s32 voiceSelector)
{
    GfxCoord* companionCoord;
    s8        pan;
    s8        depth;

    companionCoord = gameGetTaskSlot(GAME_TASK_SLOT_COMPANION)->extra.tmd->coords;
    pan            = (s8)worldCoordGetOriginAudioPan(companionCoord);
    depth          = (s8)worldCoordGetOriginAudioDepth(companionCoord);
    if (voiceSelector != ACTOR_450900_COMPANION_VOICE_RANDOM) {
        sndEvtRequestScriptStart(SOUND_SHELTER_B6_GROWTH_ALLY_VOICE_3, pan, depth);
    } else if (rand() & 1) {
        sndEvtRequestScriptStart(SOUND_SHELTER_B6_GROWTH_ALLY_VOICE_1, pan, depth);
    } else {
        sndEvtRequestScriptStart(SOUND_SHELTER_B6_GROWTH_ALLY_VOICE_2, pan, depth);
    }
}

/// Updates the player's scripted transform request to face the companion.
///
/// Requires both live TMD roots. Composes their coordinates and stores only
/// the player's yaw in 4096-unit turns, wrapped to 0..4095. The event script
/// then sends the package-owned request with GAME_ACTOR_MESSAGE_TURN_TO_YAW.
static void _actor450900PreparePlayerFacingCompanion(void)
{
    GfxCoord* companionCoord;
    GfxCoord* playerCoord;

    companionCoord = gameGetTaskSlot(GAME_TASK_SLOT_COMPANION)->extra.tmd->coords;
    playerCoord    = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
    actorRenderComposeCoord(companionCoord);
    actorRenderComposeCoord(playerCoord);
    D_actor_450900_80136458.rot.vy =
        ratan2(companionCoord->coord.t[0] - playerCoord->coord.t[0], companionCoord->coord.t[2] - playerCoord->coord.t[2]) & ACTOR_TRANSFORM_ANGLE_MASK;
}

/// Spawns the ally's save-point state handler. Once flag 0xD8 is set (the
/// capture ran) the one-shot `D_actor_450900_80135E74` swaps the ally onto the
/// `D_actor_450900_80136890` handler the first time through, and every later
/// call just re-arms the idle capture. Before the flag is set the handler is
/// picked by the AI tick counter `D_map_neo_ark_8017A99C`: the low-traffic
/// `D_actor_450900_80136470` below 0x30C, `D_actor_450900_80136680` at or above
/// it. The three calls are written out at each site - the `jal` is shared only
/// because `jump.c` cross-jumps the identical tails.
void func_actor_450900_801327A8(void)
{
    if (gameFlagGetNibble(GAME_FLAG_0D8) != 0) {
        if (D_actor_450900_80135E74 == 0) {
            D_actor_450900_80135E74 = 1;
            evsStartScript(D_actor_450900_80136890, EVENT_SCRIPT_HUD_HIDE_RESTORE);
        } else {
            capSpawnEventIfIdle(0xC, CAP_EVENT_PAUSE_ACTORS);
        }
    } else if (D_map_neo_ark_8017A99C < 0x30C) {
        evsStartScript(D_actor_450900_80136470, EVENT_SCRIPT_HUD_HIDE_RESTORE);
    } else {
        evsStartScript(D_actor_450900_80136680, EVENT_SCRIPT_HUD_HIDE_RESTORE);
    }
}

void actor450900HandleDepartureTrigger(void)
{
    enum {
        ACTOR_450900_DEPARTURE_TRIGGER_Z    = -1900,
        ACTOR_450900_DEPARTURE_TASK_INDEX   = 4,
        ACTOR_450900_DEPARTURE_PASS_VARIANT = 0,
    };
    GfxCoord* companionCoord;

    companionCoord = gameGetTaskSlot(GAME_TASK_SLOT_COMPANION)->extra.tmd->coords;
    if (companionCoord->coord.t[2] < ACTOR_450900_DEPARTURE_TRIGGER_Z) {
        taskSpawnFromTable(D_actor_450900_80135E78, ACTOR_450900_DEPARTURE_TASK_INDEX, 0, 0);
    } else {
        capStartSequenceSlot(ACTOR_450900_DEPARTURE_COMMAND, CAP_PLAYBACK_DISPLAY_TRANSITION, ACTOR_450900_DEPARTURE_PASS_VARIANT);
    }
}
