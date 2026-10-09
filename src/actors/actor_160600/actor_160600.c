#include "actors/task_tables.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/rand.h>

#include "common.h"

#include "actors/actor.h"

#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/enemy.h"
#include "gameplay/evs.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/tmd_types.h"
#include "../../shared/paced_walk.h"
#include "../../shared/walker.h"

// Message-table callbacks use the argument views required by this TU.

static s32 _pacedWalkSetPairModelDraw(Task* task, s32 messageId, s32 requestFlags, s32 unusedArg);

extern TaskMessageEntry gPacedWalkMsgTable[6];
extern u8               gPacedWalkAnimBank[];
extern u8               gPacedWalkEffectParts[];

static TmdSource _gActor160600SoldierABody;
static void      _pacedWalkTask(Task* task);

static s32 _pacedWalkStartSmoking(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArgument);

extern AnimationPlayRequest D_actor_160600_80134EF0;
extern AnimationPlayRequest D_actor_160600_80134F68;
extern AnimationPlayRequest D_actor_160600_80134F7C;
extern AnimationPlayRequest D_actor_160600_80134F90;
extern AnimationPlayRequest D_actor_160600_80134FA4;
extern AnimationPlayRequest D_actor_160600_80135030;
extern AnimationPlayRequest D_actor_160600_80135044;
extern AnimationPlayRequest D_actor_160600_80135058;
extern AnimationPlayRequest D_actor_160600_801351BC;
extern AnimationPlayRequest D_actor_160600_801351D0;
extern AnimationPlayRequest D_actor_160600_801351E4;
extern GameActorMoveAnim    D_actor_160600_80135208;
extern ActorTransform       D_actor_160600_801350A0;
extern ActorTransform       D_actor_160600_80135130;
extern ActorTransform       D_actor_160600_80135148;
static void                 _actor160600DismissCompanion(void);

static AnimationSet _gActor160600Animation02BE0;
static AnimationSet _gActor160600Animation02DA0;
static AnimationSet _gActor160600Animation02FB8;

static AnimationPackedPose _gActor160600Animation00AF4Bank1[3] = {
#include "assets/actor_160600_animation_00AF4_bank1.inc"
};

static AnimationPackedRotation _gActor160600Animation00AF4Bank4[28] = {
#include "assets/actor_160600_animation_00AF4_bank4.inc"
};

static AnimationRecord _gActor160600Animation00AF4Records[57] = {
#include "assets/actor_160600_animation_00AF4_records.inc"
};

static u16 _gActor160600Animation00AF4Indices[20] = {
#include "assets/actor_160600_animation_00AF4_indices.inc"
};

static AnimationSet _gActor160600Animation00AF4 = {
    _gActor160600Animation00AF4Records,
    _gActor160600Animation00AF4Indices,
    { NULL, _gActor160600Animation00AF4Bank1, NULL, NULL, _gActor160600Animation00AF4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160600Animation00CC0Bank1[3] = {
#include "assets/actor_160600_animation_00CC0_bank1.inc"
};

static AnimationPackedRotation _gActor160600Animation00CC0Bank4[29] = {
#include "assets/actor_160600_animation_00CC0_bank4.inc"
};

static AnimationRecord _gActor160600Animation00CC0Records[57] = {
#include "assets/actor_160600_animation_00CC0_records.inc"
};

static u16 _gActor160600Animation00CC0Indices[20] = {
#include "assets/actor_160600_animation_00CC0_indices.inc"
};

static AnimationSet _gActor160600Animation00CC0 = {
    _gActor160600Animation00CC0Records,
    _gActor160600Animation00CC0Indices,
    { NULL, _gActor160600Animation00CC0Bank1, NULL, NULL, _gActor160600Animation00CC0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160600Animation00E88Bank1[3] = {
#include "assets/actor_160600_animation_00E88_bank1.inc"
};

static AnimationPackedRotation _gActor160600Animation00E88Bank4[28] = {
#include "assets/actor_160600_animation_00E88_bank4.inc"
};

static AnimationRecord _gActor160600Animation00E88Records[57] = {
#include "assets/actor_160600_animation_00E88_records.inc"
};

static u16 _gActor160600Animation00E88Indices[20] = {
#include "assets/actor_160600_animation_00E88_indices.inc"
};

static AnimationSet _gActor160600Animation00E88 = {
    _gActor160600Animation00E88Records,
    _gActor160600Animation00E88Indices,
    { NULL, _gActor160600Animation00E88Bank1, NULL, NULL, _gActor160600Animation00E88Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160600Animation010F0Bank1[4] = {
#include "assets/actor_160600_animation_010F0_bank1.inc"
};

static AnimationPackedRotation _gActor160600Animation010F0Bank4[39] = {
#include "assets/actor_160600_animation_010F0_bank4.inc"
};

static AnimationRecord _gActor160600Animation010F0Records[83] = {
#include "assets/actor_160600_animation_010F0_records.inc"
};

static u16 _gActor160600Animation010F0Indices[20] = {
#include "assets/actor_160600_animation_010F0_indices.inc"
};

static AnimationSet _gActor160600Animation010F0 = {
    _gActor160600Animation010F0Records,
    _gActor160600Animation010F0Indices,
    { NULL, _gActor160600Animation010F0Bank1, NULL, NULL, _gActor160600Animation010F0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160600Animation01368Bank1[4] = {
#include "assets/actor_160600_animation_01368_bank1.inc"
};

static AnimationPackedRotation _gActor160600Animation01368Bank4[48] = {
#include "assets/actor_160600_animation_01368_bank4.inc"
};

static AnimationRecord _gActor160600Animation01368Records[78] = {
#include "assets/actor_160600_animation_01368_records.inc"
};

static u16 _gActor160600Animation01368Indices[20] = {
#include "assets/actor_160600_animation_01368_indices.inc"
};

static AnimationSet _gActor160600Animation01368 = {
    _gActor160600Animation01368Records,
    _gActor160600Animation01368Indices,
    { NULL, _gActor160600Animation01368Bank1, NULL, NULL, _gActor160600Animation01368Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160600Animation01504Bank1[2] = {
#include "assets/actor_160600_animation_01504_bank1.inc"
};

static AnimationPackedRotation _gActor160600Animation01504Bank4[20] = {
#include "assets/actor_160600_animation_01504_bank4.inc"
};

static AnimationRecord _gActor160600Animation01504Records[57] = {
#include "assets/actor_160600_animation_01504_records.inc"
};

static u16 _gActor160600Animation01504Indices[20] = {
#include "assets/actor_160600_animation_01504_indices.inc"
};

static AnimationSet _gActor160600Animation01504 = {
    _gActor160600Animation01504Records,
    _gActor160600Animation01504Indices,
    { NULL, _gActor160600Animation01504Bank1, NULL, NULL, _gActor160600Animation01504Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160600Animation017ACBank1[2] = {
#include "assets/actor_160600_animation_017AC_bank1.inc"
};

static AnimationPackedRotation _gActor160600Animation017ACBank4[41] = {
#include "assets/actor_160600_animation_017AC_bank4.inc"
};

static AnimationRecord _gActor160600Animation017ACRecords[103] = {
#include "assets/actor_160600_animation_017AC_records.inc"
};

static u16 _gActor160600Animation017ACIndices[20] = {
#include "assets/actor_160600_animation_017AC_indices.inc"
};

static AnimationSet _gActor160600Animation017AC = {
    _gActor160600Animation017ACRecords,
    _gActor160600Animation017ACIndices,
    { NULL, _gActor160600Animation017ACBank1, NULL, NULL, _gActor160600Animation017ACBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160600Animation01948Bank1[2] = {
#include "assets/actor_160600_animation_01948_bank1.inc"
};

static AnimationPackedRotation _gActor160600Animation01948Bank4[20] = {
#include "assets/actor_160600_animation_01948_bank4.inc"
};

static AnimationRecord _gActor160600Animation01948Records[57] = {
#include "assets/actor_160600_animation_01948_records.inc"
};

static u16 _gActor160600Animation01948Indices[20] = {
#include "assets/actor_160600_animation_01948_indices.inc"
};

static AnimationSet _gActor160600Animation01948 = {
    _gActor160600Animation01948Records,
    _gActor160600Animation01948Indices,
    { NULL, _gActor160600Animation01948Bank1, NULL, NULL, _gActor160600Animation01948Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160600Animation01F1CBank1[12] = {
#include "assets/actor_160600_animation_01F1C_bank1.inc"
};

static AnimationPackedRotation _gActor160600Animation01F1CBank4[138] = {
#include "assets/actor_160600_animation_01F1C_bank4.inc"
};

static AnimationRecord _gActor160600Animation01F1CRecords[179] = {
#include "assets/actor_160600_animation_01F1C_records.inc"
};

static u16 _gActor160600Animation01F1CIndices[20] = {
#include "assets/actor_160600_animation_01F1C_indices.inc"
};

static AnimationSet _gActor160600Animation01F1C = {
    _gActor160600Animation01F1CRecords,
    _gActor160600Animation01F1CIndices,
    { NULL, _gActor160600Animation01F1CBank1, NULL, NULL, _gActor160600Animation01F1CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160600Animation02734Bank1[19] = {
#include "assets/actor_160600_animation_02734_bank1.inc"
};

static AnimationPackedRotation _gActor160600Animation02734Bank4[189] = {
#include "assets/actor_160600_animation_02734_bank4.inc"
};

static AnimationRecord _gActor160600Animation02734Records[252] = {
#include "assets/actor_160600_animation_02734_records.inc"
};

static u16 _gActor160600Animation02734Indices[20] = {
#include "assets/actor_160600_animation_02734_indices.inc"
};

static AnimationSet _gActor160600Animation02734 = {
    _gActor160600Animation02734Records,
    _gActor160600Animation02734Indices,
    { NULL, _gActor160600Animation02734Bank1, NULL, NULL, _gActor160600Animation02734Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160600Animation02A38Bank1[6] = {
#include "assets/actor_160600_animation_02A38_bank1.inc"
};

static AnimationPackedRotation _gActor160600Animation02A38Bank4[46] = {
#include "assets/actor_160600_animation_02A38_bank4.inc"
};

static AnimationRecord _gActor160600Animation02A38Records[109] = {
#include "assets/actor_160600_animation_02A38_records.inc"
};

static u16 _gActor160600Animation02A38Indices[20] = {
#include "assets/actor_160600_animation_02A38_indices.inc"
};

static AnimationSet _gActor160600Animation02A38 = {
    _gActor160600Animation02A38Records,
    _gActor160600Animation02A38Indices,
    { NULL, _gActor160600Animation02A38Bank1, NULL, NULL, _gActor160600Animation02A38Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160600Animation02BE0Bank1[2] = {
#include "assets/actor_160600_animation_02BE0_bank1.inc"
};

static AnimationPackedRotation _gActor160600Animation02BE0Bank4[23] = {
#include "assets/actor_160600_animation_02BE0_bank4.inc"
};

static AnimationRecord _gActor160600Animation02BE0Records[57] = {
#include "assets/actor_160600_animation_02BE0_records.inc"
};

static u16 _gActor160600Animation02BE0Indices[20] = {
#include "assets/actor_160600_animation_02BE0_indices.inc"
};

static AnimationSet _gActor160600Animation02BE0 = {
    _gActor160600Animation02BE0Records,
    _gActor160600Animation02BE0Indices,
    { NULL, _gActor160600Animation02BE0Bank1, NULL, NULL, _gActor160600Animation02BE0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160600Animation02DA0Bank1[2] = {
#include "assets/actor_160600_animation_02DA0_bank1.inc"
};

static AnimationPackedRotation _gActor160600Animation02DA0Bank4[26] = {
#include "assets/actor_160600_animation_02DA0_bank4.inc"
};

static AnimationRecord _gActor160600Animation02DA0Records[60] = {
#include "assets/actor_160600_animation_02DA0_records.inc"
};

static u16 _gActor160600Animation02DA0Indices[20] = {
#include "assets/actor_160600_animation_02DA0_indices.inc"
};

static AnimationSet _gActor160600Animation02DA0 = {
    _gActor160600Animation02DA0Records,
    _gActor160600Animation02DA0Indices,
    { NULL, _gActor160600Animation02DA0Bank1, NULL, NULL, _gActor160600Animation02DA0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160600Animation02FB8Bank1[2] = {
#include "assets/actor_160600_animation_02FB8_bank1.inc"
};

static AnimationPackedRotation _gActor160600Animation02FB8Bank4[28] = {
#include "assets/actor_160600_animation_02FB8_bank4.inc"
};

static AnimationRecord _gActor160600Animation02FB8Records[80] = {
#include "assets/actor_160600_animation_02FB8_records.inc"
};

static u16 _gActor160600Animation02FB8Indices[20] = {
#include "assets/actor_160600_animation_02FB8_indices.inc"
};

static AnimationSet _gActor160600Animation02FB8 = {
    _gActor160600Animation02FB8Records,
    _gActor160600Animation02FB8Indices,
    { NULL, _gActor160600Animation02FB8Bank1, NULL, NULL, _gActor160600Animation02FB8Bank4, NULL, NULL, NULL },
};

enum {
    ACTOR_160600_COMPANION_EXTENSION_COPY_WORDS = 10,
    ACTOR_160600_PLAYER_UNARMED_WALK_CLIP       = 19,
    ACTOR_160600_PLAYER_OPENING_ARRIVAL_CLIP    = ANIMATION_BANK_BASE_SET_COUNT,
    ACTOR_160600_PLACED_ACTOR_TASK_PRIORITY     = 96,
};

/// Player scene clips for extension indices 47..61, with unused holes at 52..55.
///
/// The opening scene installs a 32-word raw span beginning at this table.
/// After the 15 pointers, that span includes three complete animation requests
/// for the placed actor and the first two words of the fourth. Trailing words are
/// never selected as clips. Preserve the following records' order and keep
/// this package loaded while the player's extended clips are in use.
static AnimationSet* _gActor160600PlayerExtensionSets[15] = {
    &_gActor160600Animation00AF4,
    &_gActor160600Animation00CC0,
    &_gActor160600Animation00E88,
    &_gActor160600Animation010F0,
    &_gActor160600Animation01368,
    NULL,
    NULL,
    NULL,
    NULL,
    &_gActor160600Animation01504,
    &_gActor160600Animation017AC,
    &_gActor160600Animation01948,
    &_gActor160600Animation01F1C,
    &_gActor160600Animation02734,
    &_gActor160600Animation02A38,
};

// Requests for the package's own actor start here, one per clip id: the scripts send them to the actor this package places in the scene, which resolves them in its own bank. This one is not referenced.
AnimationPlayRequest D_actor_160600_80134E3C = { { .index = 1 }, 0, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Resets the placed actor to clip 1.
///
/// The placed walker's own bank supplies the clip; `source.index` and the
/// world-collision word are ignored by its playback handler.
static AnimationPlayRequest _gActor160600PlacedActorAnimation1Request = {
    .source               = { .index = 1 },
    .animationId          = 1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Resets the placed actor to clip 2.
///
/// The placed walker's own bank supplies the clip; `source.index` and the
/// world-collision word are ignored by its playback handler.
static AnimationPlayRequest _gActor160600PlacedActorAnimation2Request = {
    .source               = { .index = 1 },
    .animationId          = 2,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Resets the placed actor to clip 3.
///
/// The placed walker's own bank supplies the clip; `source.index` and the
/// world-collision word are ignored by its playback handler.
static AnimationPlayRequest _gActor160600PlacedActorAnimation3Request = {
    .source               = { .index = 1 },
    .animationId          = 3,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Blends the placed actor into clip 4 over 8 normal-rate frames.
///
/// The placed walker's own bank supplies the clip; `source.index` and the
/// world-collision word are ignored by its playback handler.
static AnimationPlayRequest _gActor160600PlacedActorAnimation4Request = {
    .source               = { .index = 1 },
    .animationId          = 4,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 8,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Resets the placed actor to clip 5.
///
/// The placed walker's own bank supplies the clip; `source.index` and the
/// world-collision word are ignored by its playback handler.
static AnimationPlayRequest _gActor160600PlacedActorAnimation5Request = {
    .source               = { .index = 1 },
    .animationId          = 5,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Resets the placed actor to clip 6.
///
/// The placed walker's own bank supplies the clip; `source.index` and the
/// world-collision word are ignored by its playback handler.
static AnimationPlayRequest _gActor160600PlacedActorAnimation6Request = {
    .source               = { .index = 1 },
    .animationId          = 6,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Resets the placed actor to clip 7.
///
/// The placed walker's own bank supplies the clip; `source.index` and the
/// world-collision word are ignored by its playback handler.
static AnimationPlayRequest _gActor160600PlacedActorAnimation7Request = {
    .source               = { .index = 1 },
    .animationId          = 7,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Resets the placed actor to clip 8.
///
/// The placed walker's own bank supplies the clip; `source.index` and the
/// world-collision word are ignored by its playback handler.
static AnimationPlayRequest _gActor160600PlacedActorAnimation8Request = {
    .source               = { .index = 1 },
    .animationId          = 8,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

AnimationPlayRequest D_actor_160600_80134EF0 = { { .index = 1 }, 9, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Resets the placed actor to clip 10.
///
/// The placed walker's own bank supplies the clip; `source.index` and the
/// world-collision word are ignored by its playback handler.
static AnimationPlayRequest _gActor160600PlacedActorAnimation10Request = {
    .source               = { .index = 1 },
    .animationId          = 10,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Resets the placed actor to clip 11.
///
/// The placed walker's own bank supplies the clip; `source.index` and the
/// world-collision word are ignored by its playback handler.
static AnimationPlayRequest _gActor160600PlacedActorAnimation11Request = {
    .source               = { .index = 1 },
    .animationId          = 11,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Resets the placed actor to clip 12.
///
/// The placed walker's own bank supplies the clip; `source.index` and the
/// world-collision word are ignored by its playback handler.
static AnimationPlayRequest _gActor160600PlacedActorAnimation12Request = {
    .source               = { .index = 1 },
    .animationId          = 12,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

AnimationPlayRequest D_actor_160600_80134F40[2] = {
    { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 47, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
};

AnimationPlayRequest D_actor_160600_80134F68 = { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160600_80134F7C = { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160600_80134F90 = { { .index = 1 }, 50, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160600_80134FA4 = { { .index = 1 }, 51, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Blends the player into clip 56 over 8 normal-rate frames.
///
/// The scene runner supplies the equipped-weapon bank index in a local copy.
/// Playback disables the player's world-grid participation.
static AnimationPlayRequest _gActor160600PlayerAnimation56Request = {
    .source               = { .index = 1 },
    .animationId          = 56,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 8,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Blends the player into clip 57 over 10 normal-rate frames.
///
/// The scene runner supplies the equipped-weapon bank index in a local copy.
/// Playback disables the player's world-grid participation.
static AnimationPlayRequest _gActor160600PlayerAnimation57Request = {
    .source               = { .index = 1 },
    .animationId          = 57,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Blends the player into clip 58 over 10 normal-rate frames.
///
/// The scene runner supplies the equipped-weapon bank index in a local copy.
/// Playback disables the player's world-grid participation.
static AnimationPlayRequest _gActor160600PlayerAnimation58Request = {
    .source               = { .index = 1 },
    .animationId          = 58,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Blends the player into clip 59 over 10 normal-rate frames.
///
/// The scene runner supplies the equipped-weapon bank index in a local copy.
/// Playback disables the player's world-grid participation.
static AnimationPlayRequest _gActor160600PlayerAnimation59Request = {
    .source               = { .index = 1 },
    .animationId          = 59,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Blends the player into clip 60 over 10 normal-rate frames.
///
/// The scene runner supplies the equipped-weapon bank index in a local copy.
/// Playback disables the player's world-grid participation.
static AnimationPlayRequest _gActor160600PlayerAnimation60Request = {
    .source               = { .index = 1 },
    .animationId          = 60,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

/// Blends the player into clip 61 over 10 normal-rate frames.
///
/// The scene runner supplies the equipped-weapon bank index in a local copy.
/// Playback disables the player's world-grid participation.
static AnimationPlayRequest _gActor160600PlayerAnimation61Request = {
    .source               = { .index = 1 },
    .animationId          = 61,
    .blend                = ANIMATION_BLEND_INTERPOLATE,
    .blendFrames          = 10,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

AnimationPlayRequest D_actor_160600_80135030 = { { .index = 1 }, 47, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_160600_80135044 = { { .index = 1 }, 48, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_ENABLE };

AnimationPlayRequest D_actor_160600_80135058 = { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

/// Companion scene clips for extension indices 47..49.
///
/// The opening scene installs ten raw words beginning at this table: three
/// clip pointers, both following copy requests, and XYZ of the next placement.
/// Only the three pointers are selected as clips. Preserve that record order
/// and keep this package loaded while the companion's extended clips are used.
static AnimationSet* _gActor160600CompanionExtensionSets[3] = {
    &_gActor160600Animation02BE0,
    &_gActor160600Animation02DA0,
    &_gActor160600Animation02FB8,
};

/// Installs the companion's three scene clips and seven following non-clip words.
///
/// The ten-word source span includes both copy requests and the next placement's
/// XYZ. The count describes this raw transfer, rather than the clip-table length.
static AnimationBankCopyRequest _gActor160600CompanionExtensionCopyRequest = {
    .source    = { .words = (const s32*)_gActor160600CompanionExtensionSets },
    .wordCount = ACTOR_160600_COMPANION_EXTENSION_COPY_WORDS,
};

/// Installs the player's scene clips and fills the rest of its extension with raw words.
///
/// All 32 words are copied, including the 17 request words following the clip
/// table. Scene playback selects only valid pointers in indices 47..61.
static AnimationBankCopyRequest _gActor160600PlayerExtensionCopyRequest = {
    .source    = { .words = (const s32*)_gActor160600PlayerExtensionSets },
    .wordCount = ANIMATION_BANK_EXTENSION_CAPACITY,
};

// Where one of the two scene scripts places the companion before it walks it; the first of the run of placements that follows.
ActorTransform D_actor_160600_80135088 = { { 2350, 0, 1980, 0 }, { 0, 682, 0, 0 } };

ActorTransform D_actor_160600_801350A0 = { { 4870, 0, 3400, 0 }, { 0, 682, 0, 0 } };

/// Companion placement for the later scene exchange.
///
/// Position is in room-coordinate units; yaw is 682 of 4096 units per turn.
static ActorTransform _gActor160600CompanionConversationPlacement = {
    .pos = { 5250, 0, 3520, 0 },
    .rot = { 0, 682, 0, 0 },
};

/// Placed-actor placement at the start of the scene.
///
/// The walker applies room-coordinate XYZ and yaw -1251 in 4096 units per turn.
static ActorTransform _gActor160600PlacedActorOpeningPlacement = {
    .pos = { 6080, 0, 3850, 0 },
    .rot = { 0, -1251, 0, 0 },
};

/// Placed-actor placement restored after the opening walk or a skipped scene.
///
/// Matches the room's placed pose: room-coordinate XYZ and yaw -967 per 4096-unit turn.
static ActorTransform _gActor160600PlacedActorRestoredPlacement = {
    .pos = { 6080, 0, 4000, 0 },
    .rot = { 0, -967, 0, 0 },
};

/// Player placement before the opening scene walk.
///
/// Position is in room-coordinate units; yaw is -113 of 4096 units per turn.
static ActorTransform _gActor160600PlayerOpeningPlacement = {
    .pos = { 2300, 0, 1170, 0 },
    .rot = { 0, -113, 0, 0 },
};

/// Destination of the player's opening scene walk.
///
/// Only room-coordinate XYZ is read; the move ignores the stored rotation.
/// The opening walk animation record selects unarmed walk and arrival clip 47.
static ActorTransform _gActor160600PlayerOpeningWalkTarget = {
    .pos = { 2300, 0, 2600, 0 },
    .rot = { 0, -113, 0, 0 },
};

ActorTransform D_actor_160600_80135130 = { { 4360, 0, 1890, 0 }, { 0, 455, 0, 0 } };

ActorTransform D_actor_160600_80135148 = { { 5080, 0, 2830, 0 }, { 0, 455, 0, 0 } };

/// Player placement for the later scene exchange.
///
/// Position is in room-coordinate units; yaw is 455 of 4096 units per turn.
static ActorTransform _gActor160600PlayerConversationPlacement = {
    .pos = { 5280, 0, 3010, 0 },
    .rot = { 0, 455, 0, 0 },
};

/// Player departure position and facing target.
///
/// Walking reads room-coordinate XYZ; the earlier turn request reads only yaw,
/// 3470 of 4096 units per turn. Neither operation applies the whole transform.
static ActorTransform _gActor160600PlayerDepartureTarget = {
    .pos = { 2360, 0, 5040, 0 },
    .rot = { 0, 3470, 0, 0 },
};

/// Player placement used by normal and skipped scene completion.
///
/// Position is in room-coordinate units with zero pitch, yaw and roll.
static ActorTransform _gActor160600PlayerRestoredPlacement = {
    .pos = { 2066, 0, 6626, 0 },
    .rot = { 0, 0, 0, 0 },
};

/// Resets the player to standing clip 1.
///
/// The scene runner supplies the equipped-weapon bank index in a local copy.
/// Playback disables the player's world-grid participation.
static AnimationPlayRequest _gActor160600PlayerAnimation1Request = {
    .source               = { .index = 1 },
    .animationId          = 1,
    .blend                = ANIMATION_BLEND_RESET,
    .blendFrames          = 0,
    .enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE,
};

AnimationPlayRequest D_actor_160600_801351BC = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160600_801351D0 = { { .index = 1 }, 9, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_160600_801351E4 = { { .index = 1 }, 38, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE };

/// Unarmed walk followed by the first player scene clip on arrival.
///
/// The move copies these bank-relative IDs during dispatch. Clip 47 requires
/// the scene extension to have been installed before the opening walk begins.
static GameActorMoveAnim _gActor160600PlayerOpeningWalkAnimations = {
    .approachAnimId = ACTOR_160600_PLAYER_UNARMED_WALK_CLIP,
    .arrivalAnimId  = ACTOR_160600_PLAYER_OPENING_ARRIVAL_CLIP,
};

GameActorMoveAnim D_actor_160600_80135200 = { 19, 59 };

GameActorMoveAnim D_actor_160600_80135208 = { 19, 61 };

GameActorMoveAnim D_actor_160600_80135210 = { 19, 7 };

GameActorMoveAnim D_actor_160600_80135218 = { 4, 7 };

EvsCommand D_actor_160600_80135220[20] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 11 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160600PlayerAnimation1Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_HIDE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor160600CompanionExtensionCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .animationBankCopy = &_gActor160600PlayerExtensionCopyRequest } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor160600PlayerOpeningPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor160600PlacedActorOpeningPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160600PlacedActorAnimation1Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160600PlacedActorAnimation2Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_MOVE_TO }, { .message = { .transform = &_gActor160600PlayerOpeningWalkTarget } }, { .message = { .moveAnimation = &_gActor160600PlayerOpeningWalkAnimations } } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor160600PlacedActorRestoredPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160600PlacedActorAnimation1Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

EvsCommand D_actor_160600_80135400[56] = {
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160600PlayerAnimation56Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 21 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160600PlayerAnimation57Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 45 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160600PlayerAnimation58Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160600PlacedActorAnimation4Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160600PlayerAnimation56Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 21 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160600PlayerAnimation57Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 45 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160600PlayerAnimation58Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160600PlacedActorAnimation11Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160600PlayerAnimation56Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 21 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160600PlacedActorAnimation12Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160600PlayerAnimation57Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 45 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160600PlayerAnimation58Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160600PlayerAnimation56Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 21 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160600PlayerAnimation57Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 45 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160600PlayerAnimation58Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160600PlacedActorAnimation5Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160600PlayerAnimation56Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 21 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160600PlayerAnimation57Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 45 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160600PlayerAnimation58Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160600PlacedActorAnimation1Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor160600CompanionConversationPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor160600PlayerConversationPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160600PlayerAnimation60Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 50 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_TURN_TO_YAW }, { .message = { .transform = &_gActor160600PlayerDepartureTarget } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160600PlayerAnimation61Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160600PlacedActorAnimation6Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 85 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160600PlacedActorAnimation7Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

EvsCommand D_actor_160600_80135940[16] = {
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x5410000A }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SECONDARY_FADE, { .value = 0 }, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160600PlayerAnimation1Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor160600PlayerRestoredPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor160600DismissCompanion }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160600PlacedActorAnimation10Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x5410000A }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_SECONDARY_FADE, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
};

EvsCommand D_actor_160600_80135AC0[29] = {
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x54100017 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = D_actor_160600_80135220 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_160600_80135130 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1010 }, { .message = { .pointer = &D_actor_160600_80135148 } }, { .message = { .pointer = &D_actor_160600_80135208 } } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160600PlayerAnimation59Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160600PlayerAnimation56Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 21 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160600PlayerAnimation57Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 45 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160600PlayerAnimation58Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160600PlacedActorAnimation3Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = D_actor_160600_80135400 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 12 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160600_80134F68 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160600_80134FA4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_MOVE_TO }, { .message = { .transform = &_gActor160600PlayerDepartureTarget } }, { .message = { .pointer = &D_actor_160600_80135208 } } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160600PlacedActorAnimation8Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = D_actor_160600_80135940 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_160600_80135D78[52] = {
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x54100017 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160600_801351BC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = D_actor_160600_80135220 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_160600_80135088 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_COMPANION }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_RUN_TO }, { .message = { .pointer = &D_actor_160600_801350A0 } }, { .message = { .pointer = &D_actor_160600_80135218 } } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160600_801351D0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160600_801351E4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x40720009 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160600_801351D0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160600PlacedActorAnimation3Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_160600_80135130 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1010 }, { .message = { .pointer = &D_actor_160600_80135148 } }, { .message = { .pointer = &D_actor_160600_80135208 } } },
    { EVENT_SCRIPT_OPCODE_WAIT_ACTOR_ACTION, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160600PlayerAnimation59Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160600PlayerAnimation56Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 21 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160600PlayerAnimation57Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 45 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160600PlayerAnimation58Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = D_actor_160600_80135400 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 13 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160600_80135030 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160600_80134F7C }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160600_80135058 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_SOUND, { .value = 0x40720009 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_160600_80134EF0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160600_80135044 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 10 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160600_801351D0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_160600_80134F90 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_MOVE_TO }, { .message = { .transform = &_gActor160600PlayerDepartureTarget } }, { .message = { .pointer = &D_actor_160600_80135208 } } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160600PlacedActorAnimation8Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALL_SCRIPT, { .commands = D_actor_160600_80135940 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_160600_80136258[16] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 6 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor160600DismissCompanion }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_WEAPONS, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_PLAY }, { .animation = &_gActor160600PlayerAnimation1Request }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = GAME_ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor160600PlayerRestoredPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLAY_ANIMATION }, { .message = { .animation = &_gActor160600PlacedActorAnimation10Request } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_MESSAGE_PLACE }, { .message = { .transform = &_gActor160600PlacedActorRestoredPlacement } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

static TmdBone _gActor160600SoldierABodySkeleton[20] = {
#include "assets/soldier_a_body_skeleton.inc"
};

static u32 _gActor160600SoldierABodyPartVerts[20] = {
#include "assets/soldier_a_body_partVerts.inc"
};

static SVECTOR _gActor160600SoldierABodyVerts[363] = {
#include "assets/soldier_a_body_verts.inc"
};

static SVECTOR _gActor160600SoldierABodyNormals[360] = {
#include "assets/soldier_a_body_normals.inc"
};

static u32 _gActor160600SoldierABodyStream[3907] = {
#include "assets/soldier_a_body_stream.inc"
};

static TmdSource _gActor160600SoldierABody = {
    0,
    21060,
    6448,
    20,
    _gActor160600SoldierABodyPartVerts,
    _gActor160600SoldierABodyVerts,
    _gActor160600SoldierABodyNormals,
    _gActor160600SoldierABodySkeleton,
    _gActor160600SoldierABodyStream,
};

static AnimationPackedPose _gActor160600Animation09ED0Bank1[2] = {
#include "assets/actor_160600_animation_09ED0_bank1.inc"
};

static AnimationPackedRotation _gActor160600Animation09ED0Bank4[28] = {
#include "assets/actor_160600_animation_09ED0_bank4.inc"
};

static AnimationRecord _gActor160600Animation09ED0Records[96] = {
#include "assets/actor_160600_animation_09ED0_records.inc"
};

static u16 _gActor160600Animation09ED0Indices[20] = {
#include "assets/actor_160600_animation_09ED0_indices.inc"
};

static AnimationSet _gActor160600Animation09ED0 = {
    _gActor160600Animation09ED0Records,
    _gActor160600Animation09ED0Indices,
    { NULL, _gActor160600Animation09ED0Bank1, NULL, NULL, _gActor160600Animation09ED0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160600Animation0A168Bank1[2] = {
#include "assets/actor_160600_animation_0A168_bank1.inc"
};

static AnimationPackedRotation _gActor160600Animation0A168Bank4[46] = {
#include "assets/actor_160600_animation_0A168_bank4.inc"
};

static AnimationRecord _gActor160600Animation0A168Records[94] = {
#include "assets/actor_160600_animation_0A168_records.inc"
};

static u16 _gActor160600Animation0A168Indices[20] = {
#include "assets/actor_160600_animation_0A168_indices.inc"
};

static AnimationSet _gActor160600Animation0A168 = {
    _gActor160600Animation0A168Records,
    _gActor160600Animation0A168Indices,
    { NULL, _gActor160600Animation0A168Bank1, NULL, NULL, _gActor160600Animation0A168Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160600Animation0A4B0Bank1[2] = {
#include "assets/actor_160600_animation_0A4B0_bank1.inc"
};

static AnimationPackedRotation _gActor160600Animation0A4B0Bank4[58] = {
#include "assets/actor_160600_animation_0A4B0_bank4.inc"
};

static AnimationRecord _gActor160600Animation0A4B0Records[126] = {
#include "assets/actor_160600_animation_0A4B0_records.inc"
};

static u16 _gActor160600Animation0A4B0Indices[20] = {
#include "assets/actor_160600_animation_0A4B0_indices.inc"
};

static AnimationSet _gActor160600Animation0A4B0 = {
    _gActor160600Animation0A4B0Records,
    _gActor160600Animation0A4B0Indices,
    { NULL, _gActor160600Animation0A4B0Bank1, NULL, NULL, _gActor160600Animation0A4B0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160600Animation0A7E8Bank1[2] = {
#include "assets/actor_160600_animation_0A7E8_bank1.inc"
};

static AnimationPackedRotation _gActor160600Animation0A7E8Bank4[65] = {
#include "assets/actor_160600_animation_0A7E8_bank4.inc"
};

static AnimationRecord _gActor160600Animation0A7E8Records[115] = {
#include "assets/actor_160600_animation_0A7E8_records.inc"
};

static u16 _gActor160600Animation0A7E8Indices[20] = {
#include "assets/actor_160600_animation_0A7E8_indices.inc"
};

static AnimationSet _gActor160600Animation0A7E8 = {
    _gActor160600Animation0A7E8Records,
    _gActor160600Animation0A7E8Indices,
    { NULL, _gActor160600Animation0A7E8Bank1, NULL, NULL, _gActor160600Animation0A7E8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160600Animation0AA00Bank1[2] = {
#include "assets/actor_160600_animation_0AA00_bank1.inc"
};

static AnimationPackedRotation _gActor160600Animation0AA00Bank4[28] = {
#include "assets/actor_160600_animation_0AA00_bank4.inc"
};

static AnimationRecord _gActor160600Animation0AA00Records[80] = {
#include "assets/actor_160600_animation_0AA00_records.inc"
};

static u16 _gActor160600Animation0AA00Indices[20] = {
#include "assets/actor_160600_animation_0AA00_indices.inc"
};

static AnimationSet _gActor160600Animation0AA00 = {
    _gActor160600Animation0AA00Records,
    _gActor160600Animation0AA00Indices,
    { NULL, _gActor160600Animation0AA00Bank1, NULL, NULL, _gActor160600Animation0AA00Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160600Animation0AE94Bank1[2] = {
#include "assets/actor_160600_animation_0AE94_bank1.inc"
};

static AnimationPackedRotation _gActor160600Animation0AE94Bank4[107] = {
#include "assets/actor_160600_animation_0AE94_bank4.inc"
};

static AnimationRecord _gActor160600Animation0AE94Records[160] = {
#include "assets/actor_160600_animation_0AE94_records.inc"
};

static u16 _gActor160600Animation0AE94Indices[20] = {
#include "assets/actor_160600_animation_0AE94_indices.inc"
};

static AnimationSet _gActor160600Animation0AE94 = {
    _gActor160600Animation0AE94Records,
    _gActor160600Animation0AE94Indices,
    { NULL, _gActor160600Animation0AE94Bank1, NULL, NULL, _gActor160600Animation0AE94Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160600Animation0B10CBank1[2] = {
#include "assets/actor_160600_animation_0B10C_bank1.inc"
};

static AnimationPackedRotation _gActor160600Animation0B10CBank4[28] = {
#include "assets/actor_160600_animation_0B10C_bank4.inc"
};

static AnimationRecord _gActor160600Animation0B10CRecords[104] = {
#include "assets/actor_160600_animation_0B10C_records.inc"
};

static u16 _gActor160600Animation0B10CIndices[20] = {
#include "assets/actor_160600_animation_0B10C_indices.inc"
};

static AnimationSet _gActor160600Animation0B10C = {
    _gActor160600Animation0B10CRecords,
    _gActor160600Animation0B10CIndices,
    { NULL, _gActor160600Animation0B10CBank1, NULL, NULL, _gActor160600Animation0B10CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160600Animation0B360Bank1[2] = {
#include "assets/actor_160600_animation_0B360_bank1.inc"
};

static AnimationPackedRotation _gActor160600Animation0B360Bank4[45] = {
#include "assets/actor_160600_animation_0B360_bank4.inc"
};

static AnimationRecord _gActor160600Animation0B360Records[78] = {
#include "assets/actor_160600_animation_0B360_records.inc"
};

static u16 _gActor160600Animation0B360Indices[20] = {
#include "assets/actor_160600_animation_0B360_indices.inc"
};

static AnimationSet _gActor160600Animation0B360 = {
    _gActor160600Animation0B360Records,
    _gActor160600Animation0B360Indices,
    { NULL, _gActor160600Animation0B360Bank1, NULL, NULL, _gActor160600Animation0B360Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160600Animation0B534Bank1[2] = {
#include "assets/actor_160600_animation_0B534_bank1.inc"
};

static AnimationPackedRotation _gActor160600Animation0B534Bank4[23] = {
#include "assets/actor_160600_animation_0B534_bank4.inc"
};

static AnimationRecord _gActor160600Animation0B534Records[68] = {
#include "assets/actor_160600_animation_0B534_records.inc"
};

static u16 _gActor160600Animation0B534Indices[20] = {
#include "assets/actor_160600_animation_0B534_indices.inc"
};

static AnimationSet _gActor160600Animation0B534 = {
    _gActor160600Animation0B534Records,
    _gActor160600Animation0B534Indices,
    { NULL, _gActor160600Animation0B534Bank1, NULL, NULL, _gActor160600Animation0B534Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160600Animation0B6D4Bank1[2] = {
#include "assets/actor_160600_animation_0B6D4_bank1.inc"
};

static AnimationPackedRotation _gActor160600Animation0B6D4Bank4[18] = {
#include "assets/actor_160600_animation_0B6D4_bank4.inc"
};

static AnimationRecord _gActor160600Animation0B6D4Records[60] = {
#include "assets/actor_160600_animation_0B6D4_records.inc"
};

static u16 _gActor160600Animation0B6D4Indices[20] = {
#include "assets/actor_160600_animation_0B6D4_indices.inc"
};

static AnimationSet _gActor160600Animation0B6D4 = {
    _gActor160600Animation0B6D4Records,
    _gActor160600Animation0B6D4Indices,
    { NULL, _gActor160600Animation0B6D4Bank1, NULL, NULL, _gActor160600Animation0B6D4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160600Animation0B8B0Bank1[2] = {
#include "assets/actor_160600_animation_0B8B0_bank1.inc"
};

static AnimationPackedRotation _gActor160600Animation0B8B0Bank4[29] = {
#include "assets/actor_160600_animation_0B8B0_bank4.inc"
};

static AnimationRecord _gActor160600Animation0B8B0Records[64] = {
#include "assets/actor_160600_animation_0B8B0_records.inc"
};

static u16 _gActor160600Animation0B8B0Indices[20] = {
#include "assets/actor_160600_animation_0B8B0_indices.inc"
};

static AnimationSet _gActor160600Animation0B8B0 = {
    _gActor160600Animation0B8B0Records,
    _gActor160600Animation0B8B0Indices,
    { NULL, _gActor160600Animation0B8B0Bank1, NULL, NULL, _gActor160600Animation0B8B0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160600Animation0BAD8Bank1[2] = {
#include "assets/actor_160600_animation_0BAD8_bank1.inc"
};

static AnimationPackedRotation _gActor160600Animation0BAD8Bank4[24] = {
#include "assets/actor_160600_animation_0BAD8_bank4.inc"
};

static AnimationRecord _gActor160600Animation0BAD8Records[88] = {
#include "assets/actor_160600_animation_0BAD8_records.inc"
};

static u16 _gActor160600Animation0BAD8Indices[20] = {
#include "assets/actor_160600_animation_0BAD8_indices.inc"
};

static AnimationSet _gActor160600Animation0BAD8 = {
    _gActor160600Animation0BAD8Records,
    _gActor160600Animation0BAD8Indices,
    { NULL, _gActor160600Animation0BAD8Bank1, NULL, NULL, _gActor160600Animation0BAD8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160600Animation0BD04Bank1[2] = {
#include "assets/actor_160600_animation_0BD04_bank1.inc"
};

static AnimationPackedRotation _gActor160600Animation0BD04Bank4[25] = {
#include "assets/actor_160600_animation_0BD04_bank4.inc"
};

static AnimationRecord _gActor160600Animation0BD04Records[88] = {
#include "assets/actor_160600_animation_0BD04_records.inc"
};

static u16 _gActor160600Animation0BD04Indices[20] = {
#include "assets/actor_160600_animation_0BD04_indices.inc"
};

static AnimationSet _gActor160600Animation0BD04 = {
    _gActor160600Animation0BD04Records,
    _gActor160600Animation0BD04Indices,
    { NULL, _gActor160600Animation0BD04Bank1, NULL, NULL, _gActor160600Animation0BD04Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160600Animation0BF6CBank1[2] = {
#include "assets/actor_160600_animation_0BF6C_bank1.inc"
};

static AnimationPackedRotation _gActor160600Animation0BF6CBank4[31] = {
#include "assets/actor_160600_animation_0BF6C_bank4.inc"
};

static AnimationRecord _gActor160600Animation0BF6CRecords[97] = {
#include "assets/actor_160600_animation_0BF6C_records.inc"
};

static u16 _gActor160600Animation0BF6CIndices[20] = {
#include "assets/actor_160600_animation_0BF6C_indices.inc"
};

static AnimationSet _gActor160600Animation0BF6C = {
    _gActor160600Animation0BF6CRecords,
    _gActor160600Animation0BF6CIndices,
    { NULL, _gActor160600Animation0BF6CBank1, NULL, NULL, _gActor160600Animation0BF6CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor160600Animation0C128Bank1[2] = {
#include "assets/actor_160600_animation_0C128_bank1.inc"
};

static AnimationPackedRotation _gActor160600Animation0C128Bank4[25] = {
#include "assets/actor_160600_animation_0C128_bank4.inc"
};

static AnimationRecord _gActor160600Animation0C128Records[60] = {
#include "assets/actor_160600_animation_0C128_records.inc"
};

static u16 _gActor160600Animation0C128Indices[20] = {
#include "assets/actor_160600_animation_0C128_indices.inc"
};

static AnimationSet _gActor160600Animation0C128 = {
    _gActor160600Animation0C128Records,
    _gActor160600Animation0C128Indices,
    { NULL, _gActor160600Animation0C128Bank1, NULL, NULL, _gActor160600Animation0C128Bank4, NULL, NULL, NULL },
};

TaskMessageEntry gPacedWalkMsgTable[6] = {
    { ACTOR_MESSAGE_PLAY_ANIMATION, _pacedWalkPlayAnimation },
    { ACTOR_MESSAGE_SET_MODEL_DRAW, _pacedWalkSetPairModelDraw },
    { ACTOR_MESSAGE_PLACE, _pacedWalkPlace },
    { ACTOR_COMMAND_MESSAGE_APPLY, _pacedWalkStartSmoking },
    { ACTOR_MESSAGE_WALK_TO, _pacedWalkSetWalkTarget },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc gActor160600PlacedActorTask = {
    .header   = { .fields = { .flags = TASK_BODY_TMD, .priority = ACTOR_160600_PLACED_ACTOR_TASK_PRIORITY } },
    .callback = _pacedWalkTask,
    .data     = { .model = &_gActor160600SoldierABody },
};

u8 gPacedWalkAnimBank[64] = {
    0,
    0,
    0,
    0,
    240,
    188,
    19,
    128,
    136,
    191,
    19,
    128,
    208,
    194,
    19,
    128,
    8,
    198,
    19,
    128,
    32,
    200,
    19,
    128,
    180,
    204,
    19,
    128,
    44,
    207,
    19,
    128,
    128,
    209,
    19,
    128,
    84,
    211,
    19,
    128,
    244,
    212,
    19,
    128,
    208,
    214,
    19,
    128,
    248,
    216,
    19,
    128,
    36,
    219,
    19,
    128,
    140,
    221,
    19,
    128,
    72,
    223,
    19,
    128,
};

u8 gPacedWalkEffectParts[11] = { 1, 3, 5, 6, 9, 14, 15, 16, 17, 18, 19 };

/// Releases the current companion task and clears its session slot.
///
/// Called synchronously by this package's scene scripts. An empty slot is a
/// no-op; a live companion's exit callback must release its owned resources.
static void _actor160600DismissCompanion(void)
{
    if (gameGetTaskSlot(GAME_TASK_SLOT_COMPANION) != NULL) {
        taskCallExit(gameGetTaskSlot(GAME_TASK_SLOT_COMPANION));
        gameSetTaskSlot(NULL, GAME_TASK_SLOT_COMPANION);
    }
}

#define ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW _actorRenderDrawFixedWalkerGroundShadow
#include "../../shared/paced_walk_frame.inc.c"
#undef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW

#include "../../shared/paced_walk_update.inc.c"

/// Dispatches the paced walker's spawn and per-frame states.
///
/// Requires a live TMD task with state 0 or 1 and its owning Enemy borrowed
/// through spawnArg2.pointer. Spawn allocates work and installs teardown;
/// the frame state requires that initialized work and model to remain live.
static void _pacedWalkTask(Task* task)
{
    EnemyTaskFunc stateHandlers[2] = {
        _pacedWalkSpawn,
        _pacedWalkFrame,
    };

    stateHandlers[task->state](task->spawnArg2.pointer, task);
}

#include "../../shared/paced_walk_spawn.inc.c"

/// Releases the walker's enemy record and tears down its task tree.
///
/// Installed after work allocation. spawnArg2.pointer must retain the live
/// owning enemy through this callback; teardown frees owned work, model and
/// child tasks. Neither argument remains live after the callback.
static void _pacedWalkExit(Task* task)
{
    enemyDestroy(task->spawnArg2.pointer, task);
}

#define ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW _actorRenderDrawFixedWalkerGroundShadow
#include "../../shared/walker_shadow.inc.c"
#undef ACTOR_RENDER_DRAW_WALKER_GROUND_SHADOW

#include "../../shared/paced_walk_tick_anim.inc.c"

#include "../../shared/paced_walk_reset_anim.inc.c"

#include "../../shared/paced_walk_blend_anim.inc.c"

#include "../../shared/paced_walk_play_anim.inc.c"

#include "../../shared/paced_walk_show_pair.inc.c"

#include "../../shared/paced_walk_place.inc.c"

/// Latches scripted smoking on the paced walker for command word 1.
///
/// Requires live PacedWalkWork and a borrowed readable command through this
/// synchronous call. Other command words leave the latch unchanged; no command
/// clears it. The message ID and trailing argument are ignored. Returns zero.
static s32 _pacedWalkStartSmoking(Task* task, s32 messageId, const ActorCommand* command, s32 unusedArgument)
{
    enum { PACED_WALK_COMMAND_START_SMOKING = 1 };
    PacedWalkWork* work;
    u16            commandValue;

    commandValue = command->command;
    work         = task->work;
    if (commandValue == PACED_WALK_COMMAND_START_SMOKING) {
        work->smoking = commandValue;
    }
    return 0;
}

#include "../../shared/paced_walk_to.inc.c"
