#include "common.h"

#include "gameplay/animation.h"
#include "gameplay/area_transitions.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/scene_runtime.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"

#include "rooms/dryfield_night_water_hole.h"

/// Descriptor 0 of the task table this package publishes to the night water
/// hole room.
///
/// The room's area-resource entry for this package names the table by this
/// address with task index 0, and its entry tick spawns the descriptor after
/// this one from the same address. The layout that entry belongs to places no
/// actor, so this descriptor is only ever the table's base and is never
/// spawned.
extern TaskDesc D_actor_146000_801351FC;

/// The clips the package's scene adds to the player's animation bank, with the
/// play requests stored after them.
///
/// Both of the package's scene scripts send the player a copy request for this
/// storage before they play any of the clips. The copy takes
/// `ANIMATION_BANK_EXTENSION_CAPACITY` words from the start of the storage,
/// which is more than the clip table holds: the eight set pointers occupy
/// extended ids 47-54, and the first 24 words of the play requests are written
/// into the bank after them. The player's requests select ids 47-54 only, so
/// none of those request words is played as a clip.
///
/// The play requests are the first five of the nine the package keeps for
/// these clips, in id order, and are part of this object only because the
/// copied span reaches into the fifth; the other four follow as separate
/// objects.
typedef union {
    struct {
        AnimationSet*        sets[8];         // Player clips for extended ids 47-54
        AnimationPlayRequest playRequests[5]; // Requests for extended ids 47-51; the scripts play the first three on the player
    } data;                                   // The records by name
    s32 words[33];                            // The same storage as the copy reads it; the last word lies beyond the copied span
} _Actor146000PlayerAnimationBankExtensionStorage;
STATIC_ASSERT_SIZEOF(_Actor146000PlayerAnimationBankExtensionStorage, 132);

extern _Actor146000PlayerAnimationBankExtensionStorage D_actor_146000_801352BC;

/// The clips the package's scene adds to the companion's animation bank, with
/// the play requests stored after them.
///
/// The longer of the package's two scene scripts sends the companion a copy
/// request for this storage before it plays any of the clips; the shorter one
/// plays none and ends by jumping into the longer. The copy takes
/// `ANIMATION_BANK_EXTENSION_CAPACITY` words from the start of the storage,
/// which is exactly this object: the seven set pointers occupy extended ids
/// 47-53, and the 25 words of the play requests are written into the bank
/// after them. The companion's requests select ids 47-53 only, so none of
/// those request words is played as a clip.
///
/// The play requests are the first five of the seven the package keeps for
/// these clips, one per clip in id order, and are part of this object only
/// because the copied span covers them; the other two follow as separate
/// objects.
typedef union {
    struct {
        AnimationSet*        sets[7];         // Companion clips for extended ids 47-53
        AnimationPlayRequest playRequests[5]; // Requests for extended ids 47-51; the script plays the first four on the companion
    } data;                                   // The records by name
    s32 words[32];                            // The same storage as the copy reads it; the copied span ends with it
} _Actor146000CompanionAnimationBankExtensionStorage;
STATIC_ASSERT_SIZEOF(_Actor146000CompanionAnimationBankExtensionStorage, 128);

extern _Actor146000CompanionAnimationBankExtensionStorage D_actor_146000_80135214;

extern EvsCommand D_actor_146000_80135428[];
extern EvsCommand D_actor_146000_80135980[];
extern EvsCommand D_actor_146000_80135BD8[];

extern AnimationPlayRequest D_actor_146000_80135294;
extern AnimationPlayRequest D_actor_146000_801352A8;

static AnimationSet _gActor146000Animation00504;
static AnimationSet _gActor146000Animation00950;
static AnimationSet _gActor146000Animation00C78;
static AnimationSet _gActor146000Animation011F8;
static AnimationSet _gActor146000Animation015D8;
static AnimationSet _gActor146000Animation01898;
static AnimationSet _gActor146000Animation01AEC;
static AnimationSet _gActor146000Animation01EA4;
static AnimationSet _gActor146000Animation0207C;
static AnimationSet _gActor146000Animation02498;
static AnimationSet _gActor146000Animation02654;
static AnimationSet _gActor146000Animation0294C;
static AnimationSet _gActor146000Animation02C20;
static AnimationSet _gActor146000Animation02E40;
static AnimationSet _gActor146000Animation033B4;
void                func_actor_146000_80131E24(Task*);

static AnimationPackedPose _gActor146000Animation00504Bank1[6] = {
#include "assets/actor_146000_animation_00504_bank1.inc"
};

static AnimationPackedRotation _gActor146000Animation00504Bank4[64] = {
#include "assets/actor_146000_animation_00504_bank4.inc"
};

static AnimationRecord _gActor146000Animation00504Records[141] = {
#include "assets/actor_146000_animation_00504_records.inc"
};

static u16 _gActor146000Animation00504Indices[20] = {
#include "assets/actor_146000_animation_00504_indices.inc"
};

static AnimationSet _gActor146000Animation00504 = {
    _gActor146000Animation00504Records,
    _gActor146000Animation00504Indices,
    { NULL, _gActor146000Animation00504Bank1, NULL, NULL, _gActor146000Animation00504Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor146000Animation00950Bank1[6] = {
#include "assets/actor_146000_animation_00950_bank1.inc"
};

static AnimationPackedRotation _gActor146000Animation00950Bank4[78] = {
#include "assets/actor_146000_animation_00950_bank4.inc"
};

static AnimationRecord _gActor146000Animation00950Records[159] = {
#include "assets/actor_146000_animation_00950_records.inc"
};

static u16 _gActor146000Animation00950Indices[20] = {
#include "assets/actor_146000_animation_00950_indices.inc"
};

static AnimationSet _gActor146000Animation00950 = {
    _gActor146000Animation00950Records,
    _gActor146000Animation00950Indices,
    { NULL, _gActor146000Animation00950Bank1, NULL, NULL, _gActor146000Animation00950Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor146000Animation00C78Bank1[5] = {
#include "assets/actor_146000_animation_00C78_bank1.inc"
};

static AnimationPackedRotation _gActor146000Animation00C78Bank4[69] = {
#include "assets/actor_146000_animation_00C78_bank4.inc"
};

static AnimationRecord _gActor146000Animation00C78Records[98] = {
#include "assets/actor_146000_animation_00C78_records.inc"
};

static u16 _gActor146000Animation00C78Indices[20] = {
#include "assets/actor_146000_animation_00C78_indices.inc"
};

static AnimationSet _gActor146000Animation00C78 = {
    _gActor146000Animation00C78Records,
    _gActor146000Animation00C78Indices,
    { NULL, _gActor146000Animation00C78Bank1, NULL, NULL, _gActor146000Animation00C78Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor146000Animation011F8Bank1[10] = {
#include "assets/actor_146000_animation_011F8_bank1.inc"
};

static AnimationPackedRotation _gActor146000Animation011F8Bank4[104] = {
#include "assets/actor_146000_animation_011F8_bank4.inc"
};

static AnimationRecord _gActor146000Animation011F8Records[198] = {
#include "assets/actor_146000_animation_011F8_records.inc"
};

static u16 _gActor146000Animation011F8Indices[20] = {
#include "assets/actor_146000_animation_011F8_indices.inc"
};

static AnimationSet _gActor146000Animation011F8 = {
    _gActor146000Animation011F8Records,
    _gActor146000Animation011F8Indices,
    { NULL, _gActor146000Animation011F8Bank1, NULL, NULL, _gActor146000Animation011F8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor146000Animation015D8Bank1[8] = {
#include "assets/actor_146000_animation_015D8_bank1.inc"
};

static AnimationPackedRotation _gActor146000Animation015D8Bank4[85] = {
#include "assets/actor_146000_animation_015D8_bank4.inc"
};

static AnimationRecord _gActor146000Animation015D8Records[119] = {
#include "assets/actor_146000_animation_015D8_records.inc"
};

static u16 _gActor146000Animation015D8Indices[20] = {
#include "assets/actor_146000_animation_015D8_indices.inc"
};

static AnimationSet _gActor146000Animation015D8 = {
    _gActor146000Animation015D8Records,
    _gActor146000Animation015D8Indices,
    { NULL, _gActor146000Animation015D8Bank1, NULL, NULL, _gActor146000Animation015D8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor146000Animation01898Bank1[4] = {
#include "assets/actor_146000_animation_01898_bank1.inc"
};

static AnimationPackedRotation _gActor146000Animation01898Bank4[55] = {
#include "assets/actor_146000_animation_01898_bank4.inc"
};

static AnimationRecord _gActor146000Animation01898Records[89] = {
#include "assets/actor_146000_animation_01898_records.inc"
};

static u16 _gActor146000Animation01898Indices[20] = {
#include "assets/actor_146000_animation_01898_indices.inc"
};

static AnimationSet _gActor146000Animation01898 = {
    _gActor146000Animation01898Records,
    _gActor146000Animation01898Indices,
    { NULL, _gActor146000Animation01898Bank1, NULL, NULL, _gActor146000Animation01898Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor146000Animation01AECBank1[2] = {
#include "assets/actor_146000_animation_01AEC_bank1.inc"
};

static AnimationPackedRotation _gActor146000Animation01AECBank4[30] = {
#include "assets/actor_146000_animation_01AEC_bank4.inc"
};

static AnimationRecord _gActor146000Animation01AECRecords[93] = {
#include "assets/actor_146000_animation_01AEC_records.inc"
};

static u16 _gActor146000Animation01AECIndices[20] = {
#include "assets/actor_146000_animation_01AEC_indices.inc"
};

static AnimationSet _gActor146000Animation01AEC = {
    _gActor146000Animation01AECRecords,
    _gActor146000Animation01AECIndices,
    { NULL, _gActor146000Animation01AECBank1, NULL, NULL, _gActor146000Animation01AECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor146000Animation01EA4Bank1[3] = {
#include "assets/actor_146000_animation_01EA4_bank1.inc"
};

static AnimationPackedRotation _gActor146000Animation01EA4Bank4[81] = {
#include "assets/actor_146000_animation_01EA4_bank4.inc"
};

static AnimationRecord _gActor146000Animation01EA4Records[128] = {
#include "assets/actor_146000_animation_01EA4_records.inc"
};

static u16 _gActor146000Animation01EA4Indices[20] = {
#include "assets/actor_146000_animation_01EA4_indices.inc"
};

static AnimationSet _gActor146000Animation01EA4 = {
    _gActor146000Animation01EA4Records,
    _gActor146000Animation01EA4Indices,
    { NULL, _gActor146000Animation01EA4Bank1, NULL, NULL, _gActor146000Animation01EA4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor146000Animation0207CBank1[3] = {
#include "assets/actor_146000_animation_0207C_bank1.inc"
};

static AnimationPackedRotation _gActor146000Animation0207CBank4[29] = {
#include "assets/actor_146000_animation_0207C_bank4.inc"
};

static AnimationRecord _gActor146000Animation0207CRecords[60] = {
#include "assets/actor_146000_animation_0207C_records.inc"
};

static u16 _gActor146000Animation0207CIndices[20] = {
#include "assets/actor_146000_animation_0207C_indices.inc"
};

static AnimationSet _gActor146000Animation0207C = {
    _gActor146000Animation0207CRecords,
    _gActor146000Animation0207CIndices,
    { NULL, _gActor146000Animation0207CBank1, NULL, NULL, _gActor146000Animation0207CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor146000Animation02498Bank1[3] = {
#include "assets/actor_146000_animation_02498_bank1.inc"
};

static AnimationPackedRotation _gActor146000Animation02498Bank4[80] = {
#include "assets/actor_146000_animation_02498_bank4.inc"
};

static AnimationRecord _gActor146000Animation02498Records[154] = {
#include "assets/actor_146000_animation_02498_records.inc"
};

static u16 _gActor146000Animation02498Indices[20] = {
#include "assets/actor_146000_animation_02498_indices.inc"
};

static AnimationSet _gActor146000Animation02498 = {
    _gActor146000Animation02498Records,
    _gActor146000Animation02498Indices,
    { NULL, _gActor146000Animation02498Bank1, NULL, NULL, _gActor146000Animation02498Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor146000Animation02654Bank1[2] = {
#include "assets/actor_146000_animation_02654_bank1.inc"
};

static AnimationPackedRotation _gActor146000Animation02654Bank4[25] = {
#include "assets/actor_146000_animation_02654_bank4.inc"
};

static AnimationRecord _gActor146000Animation02654Records[60] = {
#include "assets/actor_146000_animation_02654_records.inc"
};

static u16 _gActor146000Animation02654Indices[20] = {
#include "assets/actor_146000_animation_02654_indices.inc"
};

static AnimationSet _gActor146000Animation02654 = {
    _gActor146000Animation02654Records,
    _gActor146000Animation02654Indices,
    { NULL, _gActor146000Animation02654Bank1, NULL, NULL, _gActor146000Animation02654Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor146000Animation0294CBank1[4] = {
#include "assets/actor_146000_animation_0294C_bank1.inc"
};

static AnimationPackedRotation _gActor146000Animation0294CBank4[62] = {
#include "assets/actor_146000_animation_0294C_bank4.inc"
};

static AnimationRecord _gActor146000Animation0294CRecords[96] = {
#include "assets/actor_146000_animation_0294C_records.inc"
};

static u16 _gActor146000Animation0294CIndices[20] = {
#include "assets/actor_146000_animation_0294C_indices.inc"
};

static AnimationSet _gActor146000Animation0294C = {
    _gActor146000Animation0294CRecords,
    _gActor146000Animation0294CIndices,
    { NULL, _gActor146000Animation0294CBank1, NULL, NULL, _gActor146000Animation0294CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor146000Animation02C20Bank1[4] = {
#include "assets/actor_146000_animation_02C20_bank1.inc"
};

static AnimationPackedRotation _gActor146000Animation02C20Bank4[58] = {
#include "assets/actor_146000_animation_02C20_bank4.inc"
};

static AnimationRecord _gActor146000Animation02C20Records[91] = {
#include "assets/actor_146000_animation_02C20_records.inc"
};

static u16 _gActor146000Animation02C20Indices[20] = {
#include "assets/actor_146000_animation_02C20_indices.inc"
};

static AnimationSet _gActor146000Animation02C20 = {
    _gActor146000Animation02C20Records,
    _gActor146000Animation02C20Indices,
    { NULL, _gActor146000Animation02C20Bank1, NULL, NULL, _gActor146000Animation02C20Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor146000Animation02E40Bank1[2] = {
#include "assets/actor_146000_animation_02E40_bank1.inc"
};

static AnimationPackedRotation _gActor146000Animation02E40Bank4[25] = {
#include "assets/actor_146000_animation_02E40_bank4.inc"
};

static AnimationRecord _gActor146000Animation02E40Records[85] = {
#include "assets/actor_146000_animation_02E40_records.inc"
};

static u16 _gActor146000Animation02E40Indices[20] = {
#include "assets/actor_146000_animation_02E40_indices.inc"
};

static AnimationSet _gActor146000Animation02E40 = {
    _gActor146000Animation02E40Records,
    _gActor146000Animation02E40Indices,
    { NULL, _gActor146000Animation02E40Bank1, NULL, NULL, _gActor146000Animation02E40Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor146000Animation033B4Bank1[2] = {
#include "assets/actor_146000_animation_033B4_bank1.inc"
};

static AnimationPackedRotation _gActor146000Animation033B4Bank4[135] = {
#include "assets/actor_146000_animation_033B4_bank4.inc"
};

static AnimationRecord _gActor146000Animation033B4Records[188] = {
#include "assets/actor_146000_animation_033B4_records.inc"
};

static u16 _gActor146000Animation033B4Indices[20] = {
#include "assets/actor_146000_animation_033B4_indices.inc"
};

static AnimationSet _gActor146000Animation033B4 = {
    _gActor146000Animation033B4Records,
    _gActor146000Animation033B4Indices,
    { NULL, _gActor146000Animation033B4Bank1, NULL, NULL, _gActor146000Animation033B4Bank4, NULL, NULL, NULL },
};

// The stored handler has the two-argument enemy shape, not a `TaskFunc`'s.
TaskDesc D_actor_146000_801351FC = { { { TASK_BODY_NONE, 192 } }, (TaskFunc)enemyDestroy, { .value = 0 } };

TaskDesc D_actor_146000_80135208 = { { { TASK_BODY_NONE, 32 } }, func_actor_146000_80131E24, { .value = 0 } };

_Actor146000CompanionAnimationBankExtensionStorage D_actor_146000_80135214 = { .data = { { &_gActor146000Animation0207C, &_gActor146000Animation02498, &_gActor146000Animation02654, &_gActor146000Animation0294C, &_gActor146000Animation02C20, &_gActor146000Animation02E40, &_gActor146000Animation033B4 }, { { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 49, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 51, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE } } } };

AnimationPlayRequest D_actor_146000_80135294 = { { .index = 1 }, 52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_146000_801352A8 = { { .index = 1 }, 53, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

_Actor146000PlayerAnimationBankExtensionStorage D_actor_146000_801352BC = { .data = { { &_gActor146000Animation00504, &_gActor146000Animation00950, &_gActor146000Animation00C78, &_gActor146000Animation011F8, &_gActor146000Animation015D8, &_gActor146000Animation01898, &_gActor146000Animation01AEC, &_gActor146000Animation01EA4 }, { { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE }, { { .index = 1 }, 51, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE } } } };

AnimationPlayRequest D_actor_146000_80135340 = { { .index = 1 }, 52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_146000_80135354 = { { .index = 1 }, 53, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

// Retained parameter record; layout follows the adjacent script arguments.
AnimationPlayRequest D_actor_146000_80135368 = { { .index = 1 }, 54, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_146000_8013537C = { { .index = 1 }, 54, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_146000_80135390 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_146000_801353A4 = { { .index = 6 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationBankCopyRequest D_actor_146000_801353B8 = { { .words = D_actor_146000_801352BC.words }, ANIMATION_BANK_EXTENSION_CAPACITY };

AnimationBankCopyRequest D_actor_146000_801353C0 = { { .words = D_actor_146000_80135214.words }, ANIMATION_BANK_EXTENSION_CAPACITY };

ActorTransform D_actor_146000_801353C8 = { { 4668, 0, -1337, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_146000_801353E0 = { { 5885, 0, -1300, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_146000_801353F8 = { { 0x281E, 0, -1000, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_146000_80135410 = { { 9020, 0, -1000, 0 }, { 0, 1024, 0, 0 } };

EvsCommand D_actor_146000_80135428[57] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_146000_801353B8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_146000_801353C0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146000_80135390 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146000_801353A4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_CAP_DIRECT_VIEW_IDS, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1011 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_146000_801353F8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_146000_80135410 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146000_80135390 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146000_801352A8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146000_8013537C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146000_80135214.data.playRequests[0] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146000_80135214.data.playRequests[1] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146000_80135214.data.playRequests[2] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146000_8013537C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146000_80135214.data.playRequests[0] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 9 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146000_80135214.data.playRequests[1] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146000_80135214.data.playRequests[2] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146000_801352BC.data.playRequests[1] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146000_801352BC.data.playRequests[2] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146000_80135214.data.playRequests[3] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146000_80135294 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146000_80135340 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146000_80135354 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_EVENT_STATE, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146000_80135390 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_146000_80135410 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146000_801353A4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_146000_80135980[25] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 3 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_146000_801353B8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146000_80135390 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146000_801353A4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_CAP_DIRECT_VIEW_IDS, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1011 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_146000_801353C8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146000_80135390 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x53200006 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1010 }, { .message = { .pointer = &D_actor_146000_801353E0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146000_801352BC.data.playRequests[0] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_JUMP, { .commands = D_actor_146000_80135428 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

EvsCommand D_actor_146000_80135BD8[16] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_EVENT_STATE, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_146000_801353A4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_146000_80135410 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_146000_801353F8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

void func_actor_146000_80131E24(Task* arg0)
{
    s32 state;
    s8  session;

    state = arg0->state;
    switch (state) {
        case 0:
            if (gameFlagGetNibble(GAME_FLAG_BURNER_DEFEATED) != 0) {
                func_800E8634(D_actor_146000_80135980, 0, D_actor_146000_80135BD8);
                gameFlagSetNibble(GAME_FLAG_COMPANION_2_SCHEDULE, 7);
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = 4;
            } else {
                func_800E8634(D_actor_146000_80135428, 1, D_actor_146000_80135BD8);
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = 2;
            }
            arg0->state++;
            return;
        case 1:
            session = gGameSession->eventState;
            if (session == 2) {
                arg0->state = session;
            }
            return;
        case 2:
            sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
            gameFlagSetNibble(GAME_FLAG_COMPANION_1_SCHEDULE, 0);
            Gp_ApplyAreaRecs(D_dryfield_night_water_hole_80183618);
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = 0x19;
            gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = state;
            gDisplayState.spriteVariant                                = 1;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
    }
}
