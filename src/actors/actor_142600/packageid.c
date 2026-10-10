#include "actors/actor_142600.h"

#include "gameplay/animation.h"
#include "gameplay/captions.h"
#include "gameplay/display.h"
#include "gameplay/enemy.h"
#include "gameplay/message.h"
#include "gameplay/player_actor.h"

#include "main/session.h"

// This package contains no code. Its retained data shares the existing ID TU.

static const s32 packageId = PKG_ID;

enum {
    ACTOR_142600_RETAINED_TASK_PRIORITY = 192,
    ACTOR_142600_ARRIVAL_CAP_COMMAND    = 2,
    ACTOR_142600_EXIT_VIEW              = 5,
    ACTOR_142600_WEAPON_SELECTION_BOTH  = 2,
    ACTOR_142600_RESUME_NORMAL_CONTROL  = 0,
    ACTOR_142600_FADE_DEFAULT_RAMP      = 0,
    ACTOR_142600_FADE_KEEP_RAMP         = 0,
    ACTOR_142600_IDLE_ANIMATION         = 1,
    ACTOR_142600_SCENE_ANIMATION_47     = ANIMATION_BANK_BASE_SET_COUNT,
    ACTOR_142600_SCENE_ANIMATION_48,
    ACTOR_142600_SCENE_ANIMATION_49,
    ACTOR_142600_SCENE_ANIMATION_50,
    ACTOR_142600_SCENE_ANIMATION_51,
    ACTOR_142600_SCENE_ANIMATION_52,
    ACTOR_142600_SCENE_ANIMATION_53,
    ACTOR_142600_SCENE_ANIMATION_54,
    ACTOR_142600_SCENE_ANIMATION_55,
    ACTOR_142600_SCENE_ANIMATION_56,
    ACTOR_142600_SCENE_ANIMATION_57,
    ACTOR_142600_SCENE_ANIMATION_58,
};

static AnimationPackedPose _gActor142600Animation00224Bank1[3] = {
#include "assets/actor_142600_animation_00224_bank1.inc"
};

static AnimationPackedRotation _gActor142600Animation00224Bank4[27] = {
#include "assets/actor_142600_animation_00224_bank4.inc"
};

static AnimationRecord _gActor142600Animation00224Records[90] = {
#include "assets/actor_142600_animation_00224_records.inc"
};

static u16 _gActor142600Animation00224Indices[20] = {
#include "assets/actor_142600_animation_00224_indices.inc"
};

static AnimationSet _gActor142600Animation00224 = {
    _gActor142600Animation00224Records,
    _gActor142600Animation00224Indices,
    { NULL, _gActor142600Animation00224Bank1, NULL, NULL, _gActor142600Animation00224Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142600Animation00464Bank1[2] = {
#include "assets/actor_142600_animation_00464_bank1.inc"
};

static AnimationPackedRotation _gActor142600Animation00464Bank4[41] = {
#include "assets/actor_142600_animation_00464_bank4.inc"
};

static AnimationRecord _gActor142600Animation00464Records[77] = {
#include "assets/actor_142600_animation_00464_records.inc"
};

static u16 _gActor142600Animation00464Indices[20] = {
#include "assets/actor_142600_animation_00464_indices.inc"
};

static AnimationSet _gActor142600Animation00464 = {
    _gActor142600Animation00464Records,
    _gActor142600Animation00464Indices,
    { NULL, _gActor142600Animation00464Bank1, NULL, NULL, _gActor142600Animation00464Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142600Animation006A4Bank1[2] = {
#include "assets/actor_142600_animation_006A4_bank1.inc"
};

static AnimationPackedRotation _gActor142600Animation006A4Bank4[41] = {
#include "assets/actor_142600_animation_006A4_bank4.inc"
};

static AnimationRecord _gActor142600Animation006A4Records[77] = {
#include "assets/actor_142600_animation_006A4_records.inc"
};

static u16 _gActor142600Animation006A4Indices[20] = {
#include "assets/actor_142600_animation_006A4_indices.inc"
};

static AnimationSet _gActor142600Animation006A4 = {
    _gActor142600Animation006A4Records,
    _gActor142600Animation006A4Indices,
    { NULL, _gActor142600Animation006A4Bank1, NULL, NULL, _gActor142600Animation006A4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142600Animation008F4Bank1[2] = {
#include "assets/actor_142600_animation_008F4_bank1.inc"
};

static AnimationPackedRotation _gActor142600Animation008F4Bank4[30] = {
#include "assets/actor_142600_animation_008F4_bank4.inc"
};

static AnimationRecord _gActor142600Animation008F4Records[92] = {
#include "assets/actor_142600_animation_008F4_records.inc"
};

static u16 _gActor142600Animation008F4Indices[20] = {
#include "assets/actor_142600_animation_008F4_indices.inc"
};

static AnimationSet _gActor142600Animation008F4 = {
    _gActor142600Animation008F4Records,
    _gActor142600Animation008F4Indices,
    { NULL, _gActor142600Animation008F4Bank1, NULL, NULL, _gActor142600Animation008F4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142600Animation00D38Bank1[2] = {
#include "assets/actor_142600_animation_00D38_bank1.inc"
};

static AnimationPackedRotation _gActor142600Animation00D38Bank4[102] = {
#include "assets/actor_142600_animation_00D38_bank4.inc"
};

static AnimationRecord _gActor142600Animation00D38Records[145] = {
#include "assets/actor_142600_animation_00D38_records.inc"
};

static u16 _gActor142600Animation00D38Indices[20] = {
#include "assets/actor_142600_animation_00D38_indices.inc"
};

static AnimationSet _gActor142600Animation00D38 = {
    _gActor142600Animation00D38Records,
    _gActor142600Animation00D38Indices,
    { NULL, _gActor142600Animation00D38Bank1, NULL, NULL, _gActor142600Animation00D38Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142600Animation010E4Bank1[5] = {
#include "assets/actor_142600_animation_010E4_bank1.inc"
};

static AnimationPackedRotation _gActor142600Animation010E4Bank4[71] = {
#include "assets/actor_142600_animation_010E4_bank4.inc"
};

static AnimationRecord _gActor142600Animation010E4Records[129] = {
#include "assets/actor_142600_animation_010E4_records.inc"
};

static u16 _gActor142600Animation010E4Indices[20] = {
#include "assets/actor_142600_animation_010E4_indices.inc"
};

static AnimationSet _gActor142600Animation010E4 = {
    _gActor142600Animation010E4Records,
    _gActor142600Animation010E4Indices,
    { NULL, _gActor142600Animation010E4Bank1, NULL, NULL, _gActor142600Animation010E4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142600Animation013D4Bank1[2] = {
#include "assets/actor_142600_animation_013D4_bank1.inc"
};

static AnimationPackedRotation _gActor142600Animation013D4Bank4[59] = {
#include "assets/actor_142600_animation_013D4_bank4.inc"
};

static AnimationRecord _gActor142600Animation013D4Records[103] = {
#include "assets/actor_142600_animation_013D4_records.inc"
};

static u16 _gActor142600Animation013D4Indices[20] = {
#include "assets/actor_142600_animation_013D4_indices.inc"
};

static AnimationSet _gActor142600Animation013D4 = {
    _gActor142600Animation013D4Records,
    _gActor142600Animation013D4Indices,
    { NULL, _gActor142600Animation013D4Bank1, NULL, NULL, _gActor142600Animation013D4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142600Animation016C8Bank1[4] = {
#include "assets/actor_142600_animation_016C8_bank1.inc"
};

static AnimationPackedRotation _gActor142600Animation016C8Bank4[54] = {
#include "assets/actor_142600_animation_016C8_bank4.inc"
};

static AnimationRecord _gActor142600Animation016C8Records[103] = {
#include "assets/actor_142600_animation_016C8_records.inc"
};

static u16 _gActor142600Animation016C8Indices[20] = {
#include "assets/actor_142600_animation_016C8_indices.inc"
};

static AnimationSet _gActor142600Animation016C8 = {
    _gActor142600Animation016C8Records,
    _gActor142600Animation016C8Indices,
    { NULL, _gActor142600Animation016C8Bank1, NULL, NULL, _gActor142600Animation016C8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142600Animation01884Bank1[2] = {
#include "assets/actor_142600_animation_01884_bank1.inc"
};

static AnimationPackedRotation _gActor142600Animation01884Bank4[20] = {
#include "assets/actor_142600_animation_01884_bank4.inc"
};

static AnimationRecord _gActor142600Animation01884Records[65] = {
#include "assets/actor_142600_animation_01884_records.inc"
};

static u16 _gActor142600Animation01884Indices[20] = {
#include "assets/actor_142600_animation_01884_indices.inc"
};

static AnimationSet _gActor142600Animation01884 = {
    _gActor142600Animation01884Records,
    _gActor142600Animation01884Indices,
    { NULL, _gActor142600Animation01884Bank1, NULL, NULL, _gActor142600Animation01884Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142600Animation01D98Bank1[8] = {
#include "assets/actor_142600_animation_01D98_bank1.inc"
};

static AnimationPackedRotation _gActor142600Animation01D98Bank4[116] = {
#include "assets/actor_142600_animation_01D98_bank4.inc"
};

static AnimationRecord _gActor142600Animation01D98Records[165] = {
#include "assets/actor_142600_animation_01D98_records.inc"
};

static u16 _gActor142600Animation01D98Indices[20] = {
#include "assets/actor_142600_animation_01D98_indices.inc"
};

static AnimationSet _gActor142600Animation01D98 = {
    _gActor142600Animation01D98Records,
    _gActor142600Animation01D98Indices,
    { NULL, _gActor142600Animation01D98Bank1, NULL, NULL, _gActor142600Animation01D98Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142600Animation02000Bank1[2] = {
#include "assets/actor_142600_animation_02000_bank1.inc"
};

static AnimationPackedRotation _gActor142600Animation02000Bank4[42] = {
#include "assets/actor_142600_animation_02000_bank4.inc"
};

static AnimationRecord _gActor142600Animation02000Records[86] = {
#include "assets/actor_142600_animation_02000_records.inc"
};

static u16 _gActor142600Animation02000Indices[20] = {
#include "assets/actor_142600_animation_02000_indices.inc"
};

static AnimationSet _gActor142600Animation02000 = {
    _gActor142600Animation02000Records,
    _gActor142600Animation02000Indices,
    { NULL, _gActor142600Animation02000Bank1, NULL, NULL, _gActor142600Animation02000Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142600Animation02468Bank1[10] = {
#include "assets/actor_142600_animation_02468_bank1.inc"
};

static AnimationPackedRotation _gActor142600Animation02468Bank4[96] = {
#include "assets/actor_142600_animation_02468_bank4.inc"
};

static AnimationRecord _gActor142600Animation02468Records[136] = {
#include "assets/actor_142600_animation_02468_records.inc"
};

static u16 _gActor142600Animation02468Indices[20] = {
#include "assets/actor_142600_animation_02468_indices.inc"
};

static AnimationSet _gActor142600Animation02468 = {
    _gActor142600Animation02468Records,
    _gActor142600Animation02468Indices,
    { NULL, _gActor142600Animation02468Bank1, NULL, NULL, _gActor142600Animation02468Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142600Animation0276CBank1[5] = {
#include "assets/actor_142600_animation_0276C_bank1.inc"
};

static AnimationPackedRotation _gActor142600Animation0276CBank4[55] = {
#include "assets/actor_142600_animation_0276C_bank4.inc"
};

static AnimationRecord _gActor142600Animation0276CRecords[103] = {
#include "assets/actor_142600_animation_0276C_records.inc"
};

static u16 _gActor142600Animation0276CIndices[20] = {
#include "assets/actor_142600_animation_0276C_indices.inc"
};

static AnimationSet _gActor142600Animation0276C = {
    _gActor142600Animation0276CRecords,
    _gActor142600Animation0276CIndices,
    { NULL, _gActor142600Animation0276CBank1, NULL, NULL, _gActor142600Animation0276CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142600Animation02B40Bank1[8] = {
#include "assets/actor_142600_animation_02B40_bank1.inc"
};

static AnimationPackedRotation _gActor142600Animation02B40Bank4[84] = {
#include "assets/actor_142600_animation_02B40_bank4.inc"
};

static AnimationRecord _gActor142600Animation02B40Records[117] = {
#include "assets/actor_142600_animation_02B40_records.inc"
};

static u16 _gActor142600Animation02B40Indices[20] = {
#include "assets/actor_142600_animation_02B40_indices.inc"
};

static AnimationSet _gActor142600Animation02B40 = {
    _gActor142600Animation02B40Records,
    _gActor142600Animation02B40Indices,
    { NULL, _gActor142600Animation02B40Bank1, NULL, NULL, _gActor142600Animation02B40Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142600Animation02F30Bank1[7] = {
#include "assets/actor_142600_animation_02F30_bank1.inc"
};

static AnimationPackedRotation _gActor142600Animation02F30Bank4[62] = {
#include "assets/actor_142600_animation_02F30_bank4.inc"
};

static AnimationRecord _gActor142600Animation02F30Records[149] = {
#include "assets/actor_142600_animation_02F30_records.inc"
};

static u16 _gActor142600Animation02F30Indices[20] = {
#include "assets/actor_142600_animation_02F30_indices.inc"
};

static AnimationSet _gActor142600Animation02F30 = {
    _gActor142600Animation02F30Records,
    _gActor142600Animation02F30Indices,
    { NULL, _gActor142600Animation02F30Bank1, NULL, NULL, _gActor142600Animation02F30Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142600Animation0329CBank1[7] = {
#include "assets/actor_142600_animation_0329C_bank1.inc"
};

static AnimationPackedRotation _gActor142600Animation0329CBank4[74] = {
#include "assets/actor_142600_animation_0329C_bank4.inc"
};

static AnimationRecord _gActor142600Animation0329CRecords[104] = {
#include "assets/actor_142600_animation_0329C_records.inc"
};

static u16 _gActor142600Animation0329CIndices[20] = {
#include "assets/actor_142600_animation_0329C_indices.inc"
};

static AnimationSet _gActor142600Animation0329C = {
    _gActor142600Animation0329CRecords,
    _gActor142600Animation0329CIndices,
    { NULL, _gActor142600Animation0329CBank1, NULL, NULL, _gActor142600Animation0329CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142600Animation03784Bank1[6] = {
#include "assets/actor_142600_animation_03784_bank1.inc"
};

static AnimationPackedRotation _gActor142600Animation03784Bank4[110] = {
#include "assets/actor_142600_animation_03784_bank4.inc"
};

static AnimationRecord _gActor142600Animation03784Records[166] = {
#include "assets/actor_142600_animation_03784_records.inc"
};

static u16 _gActor142600Animation03784Indices[20] = {
#include "assets/actor_142600_animation_03784_indices.inc"
};

static AnimationSet _gActor142600Animation03784 = {
    _gActor142600Animation03784Records,
    _gActor142600Animation03784Indices,
    { NULL, _gActor142600Animation03784Bank1, NULL, NULL, _gActor142600Animation03784Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142600Animation03AECBank1[5] = {
#include "assets/actor_142600_animation_03AEC_bank1.inc"
};

static AnimationPackedRotation _gActor142600Animation03AECBank4[74] = {
#include "assets/actor_142600_animation_03AEC_bank4.inc"
};

static AnimationRecord _gActor142600Animation03AECRecords[109] = {
#include "assets/actor_142600_animation_03AEC_records.inc"
};

static u16 _gActor142600Animation03AECIndices[20] = {
#include "assets/actor_142600_animation_03AEC_indices.inc"
};

static AnimationSet _gActor142600Animation03AEC = {
    _gActor142600Animation03AECRecords,
    _gActor142600Animation03AECIndices,
    { NULL, _gActor142600Animation03AECBank1, NULL, NULL, _gActor142600Animation03AECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142600Animation03CC4Bank1[2] = {
#include "assets/actor_142600_animation_03CC4_bank1.inc"
};

static AnimationPackedRotation _gActor142600Animation03CC4Bank4[16] = {
#include "assets/actor_142600_animation_03CC4_bank4.inc"
};

static AnimationRecord _gActor142600Animation03CC4Records[76] = {
#include "assets/actor_142600_animation_03CC4_records.inc"
};

static u16 _gActor142600Animation03CC4Indices[20] = {
#include "assets/actor_142600_animation_03CC4_indices.inc"
};

static AnimationSet _gActor142600Animation03CC4 = {
    _gActor142600Animation03CC4Records,
    _gActor142600Animation03CC4Indices,
    { NULL, _gActor142600Animation03CC4Bank1, NULL, NULL, _gActor142600Animation03CC4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor142600Animation03FDCBank1[5] = {
#include "assets/actor_142600_animation_03FDC_bank1.inc"
};

static AnimationPackedRotation _gActor142600Animation03FDCBank4[66] = {
#include "assets/actor_142600_animation_03FDC_bank4.inc"
};

static AnimationRecord _gActor142600Animation03FDCRecords[97] = {
#include "assets/actor_142600_animation_03FDC_records.inc"
};

static u16 _gActor142600Animation03FDCIndices[20] = {
#include "assets/actor_142600_animation_03FDC_indices.inc"
};

static AnimationSet _gActor142600Animation03FDC = {
    _gActor142600Animation03FDCRecords,
    _gActor142600Animation03FDCIndices,
    { NULL, _gActor142600Animation03FDCBank1, NULL, NULL, _gActor142600Animation03FDCBank4, NULL, NULL, NULL },
};

TaskDesc gActor142600TaskDescriptors[1] = {
    // Retain the unspawned descriptor's original, incompatible callback address.
    { { { TASK_BODY_NONE, ACTOR_142600_RETAINED_TASK_PRIORITY } }, (TaskFunc)enemyDestroy, { .value = 0 } },
};

/// Companion scene clips installed at extended bank indices 47..53.
///
/// The 32-word copy reads 25 words past these seven pointers, through the five
/// requests from `_gActor142600UnusedCompanionAnimation47` to
/// `_gActor142600CompanionAnimation50`. Those words are not playable clips.
/// Preserve the original overread and the consecutive declarations; keep the
/// package loaded while its installed clips remain in playback.
static AnimationSet* _gActor142600CompanionAnimationSets[] = { &_gActor142600Animation00224, &_gActor142600Animation00464, &_gActor142600Animation006A4, &_gActor142600Animation008F4, &_gActor142600Animation00D38, &_gActor142600Animation010E4, &_gActor142600Animation013D4 };

/// Unplayed duplicate companion clip-47 request retained in the bank-copy span.
static AnimationPlayRequest _gActor142600UnusedCompanionAnimation47 = { { .index = 1 }, ACTOR_142600_SCENE_ANIMATION_47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

// Script playback copies requests and resolves the live receiver bank before
// dispatch. Stored selectors stay unchanged, including in the raw bank copies.

/// Resets companion scene clip 47 with world collision disabled.
static AnimationPlayRequest _gActor142600CompanionAnimation47 = { { .index = 1 }, ACTOR_142600_SCENE_ANIMATION_47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Resets companion scene clip 48 with world collision disabled.
static AnimationPlayRequest _gActor142600CompanionAnimation48 = { { .index = 1 }, ACTOR_142600_SCENE_ANIMATION_48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Resets companion scene clip 49 with world collision disabled.
static AnimationPlayRequest _gActor142600CompanionAnimation49 = { { .index = 1 }, ACTOR_142600_SCENE_ANIMATION_49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Resets companion scene clip 50 with world collision disabled.
static AnimationPlayRequest _gActor142600CompanionAnimation50 = { { .index = 1 }, ACTOR_142600_SCENE_ANIMATION_50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Resets companion scene clip 51 with world collision disabled.
static AnimationPlayRequest _gActor142600CompanionAnimation51 = { { .index = 1 }, ACTOR_142600_SCENE_ANIMATION_51, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Resets companion scene clip 52 with world collision disabled.
static AnimationPlayRequest _gActor142600CompanionAnimation52 = { { .index = 1 }, ACTOR_142600_SCENE_ANIMATION_52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Resets companion scene clip 53 with world collision disabled.
static AnimationPlayRequest _gActor142600CompanionAnimation53 = { { .index = 1 }, ACTOR_142600_SCENE_ANIMATION_53, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Player scene clips installed at extended bank indices 47..59.
///
/// The 32-word copy reads 19 words past these thirteen pointers: both unused
/// clip-47 requests, clip 48's request and the first four words of clip 49's.
/// Those words are not playable clips. Preserve the original overread and the
/// consecutive declarations; keep the package loaded during scene playback.
/// This scene plays only extension indices 48..50 and 54..58.
static AnimationSet* _gActor142600PlayerAnimationSets[] = { &_gActor142600Animation016C8, &_gActor142600Animation01884, &_gActor142600Animation01D98, &_gActor142600Animation02000, &_gActor142600Animation02B40, &_gActor142600Animation02F30, &_gActor142600Animation0329C, &_gActor142600Animation0276C, &_gActor142600Animation03784, &_gActor142600Animation02468, &_gActor142600Animation03AEC, &_gActor142600Animation03CC4, &_gActor142600Animation03FDC };

/// Two unplayed clip-47 reset requests retained in the player's bank-copy span.
static AnimationPlayRequest _gActor142600UnusedPlayerAnimation47Requests[] = {
    { { .index = 1 }, ACTOR_142600_SCENE_ANIMATION_47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, ACTOR_142600_SCENE_ANIMATION_47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
};

/// Resets player scene clip 48 with world collision disabled.
static AnimationPlayRequest _gActor142600PlayerAnimation48 = { { .index = 1 }, ACTOR_142600_SCENE_ANIMATION_48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Resets player scene clip 49 with world collision disabled.
static AnimationPlayRequest _gActor142600PlayerAnimation49 = { { .index = 1 }, ACTOR_142600_SCENE_ANIMATION_49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Resets player scene clip 50 with world collision disabled.
static AnimationPlayRequest _gActor142600PlayerAnimation50 = { { .index = 1 }, ACTOR_142600_SCENE_ANIMATION_50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Resets the player's equipped-bank idle clip with world collision enabled.
static AnimationPlayRequest _gActor142600PlayerIdleAnimation = { { .index = 1 }, ACTOR_142600_IDLE_ANIMATION, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

/// Blends to the player's equipped-bank idle over 20 frames, disabling world collision.
static AnimationPlayRequest _gActor142600PlayerIdleBlendAnimation = { { .index = 1 }, ACTOR_142600_IDLE_ANIMATION, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE };

/// Resets companion idle with world collision enabled, including the skip path.
///
/// The stored selector 6 is retained, but script dispatch replaces it with the
/// current companion's bank index in a local copy, as for the other requests.
static AnimationPlayRequest _gActor142600CompanionIdleAnimation = { { .index = 6 }, ACTOR_142600_IDLE_ANIMATION, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

/// Unplayed player clip-51..53 reset requests with world collision disabled.
static AnimationPlayRequest _gActor142600UnusedPlayerAnimations51To53[] = {
    { { .index = 1 }, ACTOR_142600_SCENE_ANIMATION_51, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, ACTOR_142600_SCENE_ANIMATION_52, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, ACTOR_142600_SCENE_ANIMATION_53, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
};

/// Blends over 10 frames to player scene clip 54 with world collision disabled.
static AnimationPlayRequest _gActor142600PlayerAnimation54 = { { .index = 1 }, ACTOR_142600_SCENE_ANIMATION_54, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

/// Blends over 10 frames to player scene clip 55 with world collision disabled.
static AnimationPlayRequest _gActor142600PlayerAnimation55 = { { .index = 1 }, ACTOR_142600_SCENE_ANIMATION_55, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

/// Blends over 10 frames to player scene clip 56 with world collision disabled.
static AnimationPlayRequest _gActor142600PlayerAnimation56 = { { .index = 1 }, ACTOR_142600_SCENE_ANIMATION_56, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

/// Resets player scene clip 57 with world collision disabled.
static AnimationPlayRequest _gActor142600PlayerAnimation57 = { { .index = 1 }, ACTOR_142600_SCENE_ANIMATION_57, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Resets player scene clip 58 with world collision disabled.
static AnimationPlayRequest _gActor142600PlayerAnimation58 = { { .index = 1 }, ACTOR_142600_SCENE_ANIMATION_58, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

// Not referenced.
AnimationPlayRequest D_actor_142600_80136060 = { { .index = 1 }, 59, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Installs 32 words from the player's scene table, including its documented overread.
///
/// Sent before scene playback; the loaded character/weapon bank must be writable.
/// The copied scene-set pointers borrow this package for their playback lifetime.
static AnimationBankCopyRequest _gActor142600PlayerAnimationBankExtensionCopy = { { .sets = _gActor142600PlayerAnimationSets }, ANIMATION_BANK_EXTENSION_CAPACITY };

/// Installs 32 words from the companion's scene table, including its documented overread.
///
/// Sent before scene playback; the loaded companion bank must be writable.
/// The copied scene-set pointers borrow this package for their playback lifetime.
static AnimationBankCopyRequest _gActor142600CompanionAnimationBankExtensionCopy = { { .sets = _gActor142600CompanionAnimationSets }, ANIMATION_BANK_EXTENSION_CAPACITY };

/// Player placement for the captioned scene, in room units with yaw 2275/4096 turn.
static ActorTransform _gActor142600PlayerScenePlacement = { { -5860, 0, 1540, 0 }, { 0, 2275, 0, 0 } };

/// Companion placement for the captioned scene, in room units facing a quarter turn.
static ActorTransform _gActor142600CompanionScenePlacement = { { -6440, 0, 110, 0 }, { 0, ACTOR_TRANSFORM_ANGLE_TURN / 4, 0, 0 } };

/// Player's view-5 exit placement shared by normal completion and skipping.
///
/// Position is in room-coordinate units; yaw is half a turn. Placement messages
/// consume this borrowed record synchronously while the package is loaded.
static ActorTransform _gActor142600PlayerExitPlacement = { { -6000, 0, 948, 0 }, { 0, ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 0 } };

/// Companion's view-5 exit placement shared by normal completion and skipping.
///
/// Position is in room-coordinate units; yaw is half a turn. Placement messages
/// consume this borrowed record synchronously while the package is loaded.
static ActorTransform _gActor142600CompanionExitPlacement = { { -6442, 0, 1540, 0 }, { 0, ACTOR_TRANSFORM_ANGLE_HALF_TURN, 0, 0 } };

EvsCommand gActor142600IncineratorArrivalScript[76] = {
    // Install both scene clip tables before the first CAP cue releases playback.
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = CAP_CONTROL_MESSAGE_START }, { .value = ACTOR_142600_ARRIVAL_CAP_COMMAND }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor142600PlayerAnimationBankExtensionCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor142600CompanionAnimationBankExtensionCopy } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600PlayerIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600CompanionIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = ACTOR_142600_WEAPON_SELECTION_BOTH }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600CompanionAnimation47 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor142600PlayerScenePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor142600CompanionScenePlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600PlayerIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600PlayerAnimation54 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600CompanionAnimation53 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_HIDE_ALLOCATE }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600PlayerAnimation50 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600PlayerAnimation48 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600PlayerIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600CompanionAnimation53 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600CompanionAnimation48 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600CompanionAnimation50 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600CompanionAnimation49 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600PlayerAnimation49 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600PlayerIdleBlendAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600CompanionAnimation51 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600CompanionAnimation52 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600PlayerAnimation57 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 29 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600PlayerAnimation58 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600PlayerIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600CompanionAnimation47 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600PlayerAnimation55 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600CompanionAnimation48 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600CompanionAnimation50 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600PlayerAnimation56 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 120 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_142600_EXIT_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = ACTOR_142600_WEAPON_SELECTION_BOTH }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor142600PlayerExitPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600PlayerIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor142600CompanionExitPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600CompanionIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = ACTOR_142600_FADE_KEEP_RAMP }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = ACTOR_142600_RESUME_NORMAL_CONTROL }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = ACTOR_142600_RESUME_NORMAL_CONTROL }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand gActor142600IncineratorArrivalSkipScript[15] = {
    // Cover scene cleanup and exit placement with a black fade.
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = SCREEN_FADE_SUBTRACT }, { .value = ACTOR_142600_FADE_DEFAULT_RAMP }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_SET_MODEL_DRAW }, { .value = PLAYER_ACTOR_MODEL_DRAW_SHOW_AUTO }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = ACTOR_142600_EXIT_VIEW }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = ACTOR_142600_WEAPON_SELECTION_BOTH }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor142600CompanionIdleAnimation }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor142600CompanionExitPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor142600PlayerExitPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = ACTOR_142600_RESUME_NORMAL_CONTROL }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_END_SCRIPTED }, { .value = ACTOR_142600_RESUME_NORMAL_CONTROL }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};
