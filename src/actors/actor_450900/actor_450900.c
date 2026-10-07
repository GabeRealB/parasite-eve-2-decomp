#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/rand.h>

#include "common.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

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

/// The clips the scene adds to the player's and the companion's animation
/// banks, with both copy requests and the first companion play requests stored
/// after them.
///
/// The package's event scripts and its two periodic reaction handlers send a
/// receiver its copy request before they play any of these clips on it. Each
/// copy takes `ANIMATION_BANK_EXTENSION_CAPACITY` words, which is more than
/// either clip table holds. The player's starts at the first word of the
/// storage: its ten clips occupy extended ids 47-56, and the companion's six
/// clips, both copy requests and the first twelve words of the play requests
/// are written into the player's bank after them. The companion's starts at the
/// companion's own clips, which occupy extended ids 47-52 of its bank, and runs
/// on through both copy requests and the first 22 words of the play requests,
/// stopping three words short of the end of the storage. The requests that are
/// played select ids 51 and 53-56 on the player and ids 47, 48, 51 and 52 on
/// the companion, so none of the words following either table is played as a
/// clip.
///
/// The play requests are the first five of the run of requests the package
/// keeps for the companion and are part of this object only because the copied
/// spans reach over them; the rest of the run follows as separate objects. The
/// companion's periodic handler fills in the bank index of the request it plays
/// in place, so the storage is written as well as read.
typedef union {
    struct {
        AnimationSet*            playerSets[10];           // Player clips for extended ids 47-56
        AnimationSet*            companionSets[6];         // Companion clips for extended ids 47-52 of the companion's bank
        AnimationBankCopyRequest companionCopy;            // Installs the 32 words from `companionSets` on in the companion's bank extension
        AnimationBankCopyRequest playerCopy;               // Installs the first 32 words of this storage in the player's bank extension
        AnimationPlayRequest     companionPlayRequests[5]; // Requests for the companion's extended ids 47, 47, 48, 49 and 50; only the second and third are referenced
    } data;                                                // The records by name
    s32 words[45];                                         // The same storage as the copies read it
} _Actor450900AnimationBankExtensionStorage;
STATIC_ASSERT_SIZEOF(_Actor450900AnimationBankExtensionStorage, 180);
STATIC_ASSERT(OFFSET_OF(_Actor450900AnimationBankExtensionStorage, data.companionPlayRequests[1]) == 100, actor450900_ally_anim_offset);

extern _Actor450900AnimationBankExtensionStorage D_actor_450900_80135EC0;

// This record has an independently materialized address in the caller.
// The symbol view shares the union backing; it allocates no extra storage.
extern AnimationPlayRequest Actor450900AllyAnim __asm__("D_actor_450900_80135EC0+100");

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

/// The save-point capture task spawned by `func_actor_450900_80131E38`, kept
/// alive until `func_actor_450900_80132548` kills it. Script opcode 0xD reaches
/// both this and `func_actor_450900_80132678`, so its one argument is the
/// opcode's immediate.
extern Task* D_actor_450900_80136C9C;

/// This overlay's own spawn table, six `TaskDesc` entries. Index 0 is the exit
/// handler `taskKill`; 1..5 are the overlay's state handlers, and the "next
/// stage" of each is the next entry: `func_actor_450900_80131E38` spawns 5 on
/// its way through, and `func_actor_450900_80132834` spawns 4
/// (`func_actor_450900_8013235C`, the save-data teardown) when the ally has
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
void                        func_actor_450900_80132518(s32);
void                        func_actor_450900_80132678(u8);
void                        func_actor_450900_80132684(s32);
void                        func_actor_450900_80132724(void);

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

void func_actor_450900_80131E38(Task*);
void func_actor_450900_8013207C(Task*);
void func_actor_450900_8013223C(Task*);
void func_actor_450900_8013235C(Task*);
void func_actor_450900_80132548(Task*);

TaskDesc D_actor_450900_80135E78[6] = {
    { { { TASK_BODY_NONE, 192 } }, taskKill, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_actor_450900_80131E38, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_actor_450900_8013207C, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_actor_450900_8013223C, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_actor_450900_8013235C, { .value = 0 } },
    { { { TASK_BODY_NONE, 97 } }, func_actor_450900_80132548, { .value = 0 } },
};

_Actor450900AnimationBankExtensionStorage D_actor_450900_80135EC0 = { .data = { { &_gActor450900Animation020EC, &_gActor450900Animation02428, &_gActor450900Animation025F0, &_gActor450900Animation02A30, &_gActor450900Animation030F4, &_gActor450900Animation04028, &_gActor450900Animation03338, &_gActor450900Animation03738, &_gActor450900Animation03934, &_gActor450900Animation03C70 }, { &_gActor450900Animation00F80, &_gActor450900Animation012D8, &_gActor450900Animation0176C, &_gActor450900Animation01934, &_gActor450900Animation01B50, &_gActor450900Animation01D04 }, { { .words = &D_actor_450900_80135EC0.words[ARRAY_SIZE(D_actor_450900_80135EC0.data.playerSets)] }, ANIMATION_BANK_EXTENSION_CAPACITY }, { { .words = D_actor_450900_80135EC0.words }, ANIMATION_BANK_EXTENSION_CAPACITY }, { { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE }, { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE } } } };

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
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_450900_80135EC0.data.playerCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_450900_80135EC0.data.companionCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_450900_801360F8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 10 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b6_growth_room_8017D82C }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
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
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_450900_80135EC0.data.playerCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_450900_80135EC0.data.companionCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_450900_80132724 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1006 }, { .message = { .pointer = &D_actor_450900_80136458 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1018 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_450900_80132518 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = func_actor_450900_80132678 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450900_80136064 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450900_801360A0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1018 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_450900_80132518 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = func_actor_450900_80132678 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_450900_80136680[22] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = func_actor_450900_80132678 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_450900_80132724 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1006 }, { .message = { .pointer = &D_actor_450900_80136458 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1018 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_450900_80132518 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450900_80136064 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450900_80135EC0.data.companionPlayRequests[2] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_450900_80132684 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450900_801360B4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1018 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_450900_80132518 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = func_actor_450900_80132678 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_450900_80136890[26] = {
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_shelter_b6_growth_room_8017D82C }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 5 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = func_actor_450900_80132678 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_450900_80135EC0.data.playerCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_450900_80135EC0.data.companionCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = func_actor_450900_80132724 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1006 }, { .message = { .pointer = &D_actor_450900_80136458 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1018 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_450900_80132518 }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_450900_80135F74 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1006 }, { .message = { .pointer = &D_actor_450900_801360E0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1018 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = func_actor_450900_80132518 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = func_actor_450900_80132678 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_450900_80136B00[9] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_450900_80135EC0.data.playerCopy } }, { .value = 0 } },
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
void func_actor_450900_80132834(void);

/// State handler that runs the save-point capture. State 0 spawns the capture
/// task `func_actor_450900_80132548` into `D_actor_450900_80136C9C`; state 1
/// waits for `D_map_neo_ark_8017A99C`, the AI tick counter, to pass 0x30C with save data in
/// the slot, then arms the flag `func_actor_450900_80132518` toggles and, on
/// every 210th tick, plays the ally's voice cue at its own pan and depth and
/// posts the `0x3F7` / `0x3E8` / `0x3F9` messages to the slot-0xA task; 0x3C
/// ticks later it posts `0x3E8` alone, with the capture-indicator animation.
/// The one-shot `D_actor_450900_80135E74` retires the handler after one pass.
void func_actor_450900_80131E38(Task* task)
{
    GfxCoord* coord;
    s32       state;
    s32       t;
    s8        pan;
    s8        depth;
    Task*     companionTask;

    companionTask = gameGetTaskSlot(GAME_TASK_SLOT_COMPANION);
    state         = task->state;
    switch (state) {
        case 0:
            D_actor_450900_80135E70 = 0;
            D_actor_450900_80136C9C = taskSpawnFromTable(D_actor_450900_80135E78, 5, 0, 0);
            task->state             = task->state + 1;
            break;
        case 1:
            if (capIsBusy() != 0) {
                break;
            }
            if (gGameSession->eventState != 0) {
                break;
            }
            if (gSceneCombatState.actorControl != SCENE_COMBAT_ACTORS_RUNNING) {
                break;
            }
            if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene != 0xB) {
                D_map_neo_ark_8017A99C = D_map_neo_ark_8017A99C + 1;
            }
            t = D_map_neo_ark_8017A99C - 0x30C;
            if (D_actor_450900_80135E74 == 0 && gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionHp > 0 && t >= 0) {
                D_actor_450900_80135E70 = state;
                if (t % 210 == 0) {
                    coord = (gameGetTaskSlot(GAME_TASK_SLOT_COMPANION))->extra.tmd->coords;
                    pan   = (s8)worldCoordGetOriginAudioPan(coord);
                    depth = (s8)worldCoordGetOriginAudioDepth(coord);
                    if (rand() & 1) {
                        sndEvtRequestScriptStart(SOUND_SHELTER_B6_GROWTH_ALLY_VOICE_1, pan, depth);
                    } else {
                        sndEvtRequestScriptStart(SOUND_SHELTER_B6_GROWTH_ALLY_VOICE_2, pan, depth);
                    }
                    companionWriteAnimationBankIndex(&Actor450900AllyAnim.source.index);
                    TASK_MESSAGE_DISPATCH_POINTER(companionTask, ANIMATION_MESSAGE_COPY_BANK_EXTENSION, &D_actor_450900_80135EC0.data.companionCopy, 0);
                    TASK_MESSAGE_DISPATCH_POINTER(companionTask, ANIMATION_MESSAGE_PLAY, &Actor450900AllyAnim, 0);
                    taskMessageDispatch(companionTask, GAME_ACTOR_MESSAGE_APPLY_DAMAGE, 0x40010, 0);
                } else if (t % 210 == 0x3C) {
                    companionWriteAnimationBankIndex(&D_actor_450900_801360B4.source.index);
                    TASK_MESSAGE_DISPATCH_POINTER(companionTask, ANIMATION_MESSAGE_PLAY, &D_actor_450900_801360B4, 0);
                }
            }
            break;
    }
}

void func_actor_450900_8013207C(Task* task)
{
    GfxCoord* coord;
    Task*     slot;
    s32       value;
    s8        pan;
    s8        depth;

    slot  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    value = task->state;
    switch (value) {
        case 0:
            task->killCountdown = 0;
            task->state         = task->state + 1;
            return;
        case 1:
            if ((capIsBusy() == 0) && (gGameSession->eventState == 0) && (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) && ((D_map_neo_ark_8017A99C - 0x456) >= 0)) {
                if ((D_map_neo_ark_8017A99C - 0x456) % 210 == 0) {
                    coord = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
                    pan   = (s8)worldCoordGetOriginAudioPan(coord);
                    depth = (s8)worldCoordGetOriginAudioDepth(coord);
                    if (rand() & 1) {
                        sndEvtRequestScriptStart(SOUND_SHELTER_B6_GROWTH_PLAYER_VOICE_1, pan, depth);
                    } else {
                        sndEvtRequestScriptStart(SOUND_SHELTER_B6_GROWTH_PLAYER_VOICE_2, pan, depth);
                    }
                    TASK_MESSAGE_DISPATCH_POINTER(slot, ANIMATION_MESSAGE_COPY_BANK_EXTENSION, &D_actor_450900_80135EC0.data.playerCopy, 0);
                    playerActorWriteWeaponAnimationBankIndex(&D_actor_450900_80135FEC.source.index);
                    TASK_MESSAGE_DISPATCH_POINTER(slot, ANIMATION_MESSAGE_PLAY, &D_actor_450900_80135FEC, 0);
                } else if ((D_map_neo_ark_8017A99C - 0x456) % 210 == 0x46) {
                    /* The image has the counter's lw/addiu/sw alone in a block: the load's
                     * stall is an unfilled nop and the call's four argument moves all sit
                     * below the store, so a branch stood between the store and the call and
                     * left no instruction. Equal arms on the new count reproduce that; what
                     * the original tested, and what differed between its arms, is unknown.
                     * The count is kept in `value` (the state local) because only a variable
                     * that lives in more than one block gets `$v1` behind the `%hi`. */
                    value = D_actor_450900_80136C98;
                    value++;
                    D_actor_450900_80136C98 = value;
                    if (value != 0) {
                        taskMessageDispatch(slot, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                    } else {
                        taskMessageDispatch(slot, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
                    }
                }
            }
            return;
    }
}

void func_actor_450900_8013223C(Task* task)
{
    switch (task->state) {
        case 0:
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            capRunCommand(1, CAP_PLAYBACK_IN_PLACE);
            task->state = task->state + 1;
            break;
        case 1:
            if (capIsBusy() == 0) {
                task->state = task->state + 1;
            }
            break;
        case 2:
            if (capGetVariantKey() != 0xB) {
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                taskKill(task);
                break;
            }
            gameFlagSetNibble(GAME_FLAG_0D8, 1);
            gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_PAUSED;
            capRunCommand(2, CAP_PLAYBACK_IN_PLACE);
            evsStartScript(D_actor_450900_80136B00, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            task->state = task->state + 1;
            break;
        case 3:
            if (gGameSession->eventState == 0) {
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
                gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
                taskKill(task);
            }
            break;
    }
}

void func_actor_450900_8013235C(Task* task)
{
    switch (task->state) {
        case 0:
            capStartSequenceSlot(0xB, 1, 1);
            task->state = task->state + 1;
            break;
        case 1:
            if (capIsBusy() == 0) {
                task->state = task->state + 1;
            }
            break;
        case 2:
            if (capGetVariantKey() != 0xB) {
                taskKill(task);
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_RESUME);
            } else {
                evsStartScript(D_actor_450900_80136BD8, EVENT_SCRIPT_HUD_HIDE_RESTORE);
                task->state = task->state + 1;
            }
            break;
        case 3:
            if (gGameSession->eventState == 2) {
                task->state = task->state + 1;
            }
            break;
        case 4:
            gameFlagSetNibble(GAME_FLAG_COMPANION_3_SCHEDULE, 0);
            gameFlagSetNibble(GAME_FLAG_0FC, 1);
            gameFlagSetNibble(GAME_FLAG_B1_CORRIDOR_ELEVATOR_HALL_UNLOCKED, 0);
            gameFlagSetNibble(GAME_FLAG_NEO_ARK_ALTAR_SWITCH_STATE, 0);
            gameFlagSetNibble(GAME_FLAG_B2_CORRIDOR_ELEVATOR_HALL_UNLOCKED, 1);
            gameFlagSetNibble(GAME_FLAG_MAP_MARK_SHELTER_1C7, 0);
            gameFlagSetNibble(GAME_FLAG_SHELTER_WATCHERS_DISABLED, 0);
            gameFlagSetNibble(GAME_FLAG_CUTSCENE_FOLLOW_UP_STATE, 0);
            gameFlagSetNibble(GAME_FLAG_STORY_DIALOGUE_INDEX, 8);
            sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = 0xF;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = 3;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType     = 0;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = 1;
            gDisplayState.spriteVariant                                = 1;
            taskSpawn(0, 0x11, 0, 0);
            streamFinishScene();
            taskKill(task);
            break;
    }
}

/// Script callback: arms or disarms the save-point capture task's flag
/// (`Task::spawnArg1`, the value `func_actor_450900_80132548` tests to decide
/// which way the capture cursor sweeps).
void func_actor_450900_80132518(s32 arg0)
{
    if (D_actor_450900_80136C9C != NULL) {
        if (arg0 == 1) {
            D_actor_450900_80136C9C->spawnArg1.value = 0;
            return;
        }
        D_actor_450900_80136C9C->spawnArg1.value = 1;
    }
}

/// State handler of the save-point capture task `func_actor_450900_80131E38`
/// spawns. State 0 allocates the head-aim record the capture cursor sweeps with
/// (an `AnimationHeadAim` in `Task::work`); state 1 ramps its `rate` one
/// 0x200 step per frame, up or down according to `Task::spawnArg1` (the flag
/// `func_actor_450900_80132518` arms), and hands the record to `animationAimHeadAt`
/// between the slot-3 task and the ally's own slot-0xA task. Any other state
/// kills the task and drops the overlay's handle to it.
void func_actor_450900_80132548(Task* task)
{
    AnimationHeadAim* aim;
    Task*             playerTask;
    u16               rate;

    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    switch (task->state) {
        case 0:
            aim = memCalloc(sizeof(AnimationHeadAim), false);
            if (aim == NULL) {
                taskKill(task);
                return;
            }
            task->work      = aim;
            aim->yawLimit   = 0x100;
            aim->pitchLimit = 0x200;
            task->state++;
            /* fallthrough */
        case 1:
            aim = task->work;
            if (task->spawnArg1.value != 0) {
                rate      = aim->rate + 0x200;
                aim->rate = rate;
                if ((s16)rate > ONE) {
                    aim->rate = ONE;
                }
            } else {
                rate      = aim->rate - 0x200;
                aim->rate = rate;
                if ((s16)rate < 0) {
                    aim->rate = 0;
                }
            }
            animationAimHeadAt(playerTask, gameGetTaskSlot(GAME_TASK_SLOT_COMPANION), aim);
            return;
        default:
            taskKill(task);
            D_actor_450900_80136C9C = NULL;
            return;
    }
}

void func_actor_450900_80132678(u8 arg0)
{
    gSceneCombatState.actorControl = arg0;
}

/// Plays the ally's voice cue at its own pan and depth: `arg0` picks the
/// non-random id, otherwise one of the two `0x55170005/6` takes is chosen.
void func_actor_450900_80132684(s32 arg0)
{
    GfxCoord* coord;
    s8        pan;
    s8        depth;

    coord = (gameGetTaskSlot(GAME_TASK_SLOT_COMPANION))->extra.tmd->coords;
    pan   = (s8)worldCoordGetOriginAudioPan(coord);
    depth = (s8)worldCoordGetOriginAudioDepth(coord);
    if (arg0 != 0) {
        sndEvtRequestScriptStart(SOUND_SHELTER_B6_GROWTH_ALLY_VOICE_3, pan, depth);
    } else if (rand() & 1) {
        sndEvtRequestScriptStart(SOUND_SHELTER_B6_GROWTH_ALLY_VOICE_1, pan, depth);
    } else {
        sndEvtRequestScriptStart(SOUND_SHELTER_B6_GROWTH_ALLY_VOICE_2, pan, depth);
    }
}

/// Script callback (opcode 0xD): refreshes the root coordinates of the slot-0xA
/// and slot-3 tasks and stores the 12-bit `ratan2` heading from the slot-3
/// object to the slot-0xA object in `D_actor_450900_80136458.rot.vy`, the halfword at
/// offset 0x12 of the `D_actor_450900_80136458` block the same scripts then post
/// with message `0x3EE`.
void func_actor_450900_80132724(void)
{
    GfxCoord* target;
    GfxCoord* origin;

    target = (gameGetTaskSlot(GAME_TASK_SLOT_COMPANION))->extra.tmd->coords;
    origin = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
    actorRenderComposeCoord(target);
    actorRenderComposeCoord(origin);
    D_actor_450900_80136458.rot.vy =
        ratan2(target->coord.t[0] - origin->coord.t[0], target->coord.t[2] - origin->coord.t[2]) & 0xFFF;
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

/// Reads the root coordinate of the slot-0xA task's `TmdObject` (reached through
/// `Task::extra`, as `func_actor_450900_80132684` does) and, when its world Z is
/// below -0x76C, spawns entry 4 of `D_actor_450900_80135E78`
/// (`func_actor_450900_8013235C`); otherwise it starts capture slot 0xB with
/// `capStartSequenceSlot`.
void func_actor_450900_80132834(void)
{
    GfxCoord* coord;

    coord = (gameGetTaskSlot(GAME_TASK_SLOT_COMPANION))->extra.tmd->coords;
    if (coord->coord.t[2] < -0x76C) {
        taskSpawnFromTable(D_actor_450900_80135E78, 4, 0, 0);
    } else {
        capStartSequenceSlot(0xB, 1, 0);
    }
}
