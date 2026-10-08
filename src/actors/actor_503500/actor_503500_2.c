#include "actor_503500_private.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "common.h"

#include "actors/actor_503500.h"

#include "gameplay/companion_load.h"
#include "gameplay/display.h"
#include "gameplay/animation.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/collision.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/pad_script.h"
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_combat.h"
#include "gameplay/scene_runtime.h"

#include "main/areas.h"
#include "main/coord.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gfx_types.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "rooms/shelter_r48.h"

/// Work block of a drift-sprite emitter, a coordinate task that sheds Shelter
/// R48's drift sprites from one fixed point of the room.
///
/// Each sprite is spawned on the emitter's own coordinate, drifting narrowly
/// upward, with `spriteSize` and `cellPeriod` packed into its spawn argument.
/// While the room's scene state winds the emitter down the sprites shrink,
/// run through their cells faster and come less often, and the emitter ends
/// once `spawnInterval` passes sixteen frames.
///
/// The emitter's task allocates the block zeroed and keeps it in `Task::work`.
typedef struct {
    s32     spriteSize;    // Size the next sprite is spawned with, a perspective numerator; only bits 0..11 reach the sprite. Shrinks to a floor while winding down
    s32     cellPeriod;    // Frames the next sprite shows each animation cell, in 1/4096 of a frame; only the whole frames reach the sprite. Shrinks to one frame while winding down
    Fixed16 spawnInterval; // Running frames between sprites: one is spawned when the task's frame count passes the integer half. Grows while winding down
} _Actor503500DriftSpriteEmitterWork;
STATIC_ASSERT_SIZEOF(_Actor503500DriftSpriteEmitterWork, 0xC);

/// Spawn positions `_actor503500DriftSpriteEmitterTask` indexes by `Task::spawnArg1`.
extern SVECTOR D_actor_503500_8014B97C[];

extern TaskDesc D_actor_503500_8014B964[];
/// Event scripts in the overlay's `.data`, handed to `evsStartScriptWithSkip` (which
/// forwards the first to `taskSpawn`).
extern EvsCommand D_actor_503500_8014CD98[];
extern EvsCommand D_actor_503500_8014D098[];

static void _actor503500DriftSpriteEmitterTask(Task* task);
static void _actor503500FadeFromBlackTask(Task* task);
static void _actor503500StaffCardSceneTask(Task* task);

static AnimationSet _gActor503500Animation16D8C;
static AnimationSet _gActor503500Animation16F18;
static AnimationSet _gActor503500Animation1789C;
static AnimationSet _gActor503500Animation181EC;
static AnimationSet _gActor503500Animation18374;
static AnimationSet _gActor503500Animation18820;
static AnimationSet _gActor503500Animation18B10;
static AnimationSet _gActor503500Animation18FCC;
static AnimationSet _gActor503500Animation19354;
static AnimationSet _gActor503500Animation195A4;
static AnimationSet _gActor503500Animation19B10;

extern AnimationPlayRequest      D_actor_503500_8014B9E8;
extern AnimationPlayRequest      D_actor_503500_8014B9FC[10];
extern AnimationPlayRequest      D_actor_503500_8014BAC4;
extern ActorCommand              D_actor_503500_8014BC14;
extern ActorCommand              D_actor_503500_8014BC18[2];
extern ActorCommand              D_actor_503500_8014BC20;
extern ActorCommand              D_actor_503500_8014BC24;
extern ActorCommand              D_actor_503500_8014BC28;
extern AnimationBankCopyRequest  D_actor_503500_8014B9CC;
extern PadScriptCmd              D_actor_503500_8014D2F0[2];
extern PadScriptCmd              D_actor_503500_8014D300[3];
extern PadScriptVibrationSegment D_actor_503500_8014D2F8[2];
extern PadScriptVibrationSegment D_actor_503500_8014D30C[3];
extern ActorTransform            D_actor_503500_8014BAD8;
extern ActorTransform            D_actor_503500_8014BAF0;
extern ActorTransform            D_actor_503500_8014BB08;
extern ActorTransform            D_actor_503500_8014BB20;
extern ActorTransform            D_actor_503500_8014BB38;
extern ActorTransform            D_actor_503500_8014BBB4[2];
extern ActorTransform            D_actor_503500_8014BBE4;
extern ActorTransform            D_actor_503500_8014BBFC;
static void                      _actor503500StageSceneAudioStart(void);
static void                      _actor503500EnqueueScenePlayback(void);
static void                      _actor503500FinishScene(void);
static void                      _actor503500CancelScene(void);
static void                      _actor503500ReloadPodBottom(void);
static void                      _actor503500SpawnDriftSpriteEmitter(s32 positionIndex);
static void                      _actor503500SpawnFadeFromBlack(s32 holdFrames);
static void                      _actor503500CancelRoomEffects(void);
static void                      _actor503500ReleaseBossBattle(s8 endDelayFrames);
static void                      _actor503500AddSessionFlowFlags(s32 flowFlags);
static void                      _actor503500LockAttachmentsForEvent(void);
static void                      _actor503500RequestViewRespawn(void);
static void                      _actor503500SetStaffCardUseState(s32 cardUseState);
static void                      _actor503500SetRoomBackgroundSpritesVisible(s32 visible);
static void                      _actor503500ClearSavedPlayerTransform(void);
static void                      _actor503500SavePlayerTransform(void);
static void                      _actor503500RestorePlayerTransform(void);
static void                      _actor503500SetPlayerUpdateHold(u8 holdPlayerUpdate);
static void                      _actor503500ResetPlayerWeaponAttack(void);
static void                      _actor503500HaltPadScript(void);

/// Progress value installed by both normal and skipped Shelter R48 entry scripts.
enum { ACTOR_503500_STAFF_CARD_USE_READY = 1 };

static TmdSource _gActor503500BrahmanTorso;
static TmdSource _gActor503500Model22A30;
static TmdSource _gActor503500Model23550;
static TmdSource _gActor503500Model24C70;
static TmdSource _gActor503500Model261C8;
static TmdSource _gActor503500Model27198;
static TmdSource _gActor503500Model28168;
static TmdSource _gActor503500Model29128;
static TmdSource _gActor503500Model2A450;
static TmdSource _gActor503500Model2C1C0;

static AnimationPackedPose _gActor503500Animation16D8CBank1[6] = {
#include "assets/actor_503500_animation_16D8C_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation16D8CBank4[46] = {
#include "assets/actor_503500_animation_16D8C_bank4.inc"
};

static AnimationRecord _gActor503500Animation16D8CRecords[109] = {
#include "assets/actor_503500_animation_16D8C_records.inc"
};

static u16 _gActor503500Animation16D8CIndices[20] = {
#include "assets/actor_503500_animation_16D8C_indices.inc"
};

static AnimationSet _gActor503500Animation16D8C = {
    _gActor503500Animation16D8CRecords,
    _gActor503500Animation16D8CIndices,
    { NULL, _gActor503500Animation16D8CBank1, NULL, NULL, _gActor503500Animation16D8CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation16F18Bank1[2] = {
#include "assets/actor_503500_animation_16F18_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation16F18Bank4[16] = {
#include "assets/actor_503500_animation_16F18_bank4.inc"
};

static AnimationRecord _gActor503500Animation16F18Records[57] = {
#include "assets/actor_503500_animation_16F18_records.inc"
};

static u16 _gActor503500Animation16F18Indices[20] = {
#include "assets/actor_503500_animation_16F18_indices.inc"
};

static AnimationSet _gActor503500Animation16F18 = {
    _gActor503500Animation16F18Records,
    _gActor503500Animation16F18Indices,
    { NULL, _gActor503500Animation16F18Bank1, NULL, NULL, _gActor503500Animation16F18Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation1789CBank1[32] = {
#include "assets/actor_503500_animation_1789C_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation1789CBank4[172] = {
#include "assets/actor_503500_animation_1789C_bank4.inc"
};

static AnimationRecord _gActor503500Animation1789CRecords[321] = {
#include "assets/actor_503500_animation_1789C_records.inc"
};

static u16 _gActor503500Animation1789CIndices[20] = {
#include "assets/actor_503500_animation_1789C_indices.inc"
};

static AnimationSet _gActor503500Animation1789C = {
    _gActor503500Animation1789CRecords,
    _gActor503500Animation1789CIndices,
    { NULL, _gActor503500Animation1789CBank1, NULL, NULL, _gActor503500Animation1789CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation181ECBank1[65] = {
#include "assets/actor_503500_animation_181EC_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation181ECBank4[111] = {
#include "assets/actor_503500_animation_181EC_bank4.inc"
};

static AnimationRecord _gActor503500Animation181ECRecords[270] = {
#include "assets/actor_503500_animation_181EC_records.inc"
};

static u16 _gActor503500Animation181ECIndices[20] = {
#include "assets/actor_503500_animation_181EC_indices.inc"
};

static AnimationSet _gActor503500Animation181EC = {
    _gActor503500Animation181ECRecords,
    _gActor503500Animation181ECIndices,
    { NULL, _gActor503500Animation181ECBank1, NULL, NULL, _gActor503500Animation181ECBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation18374Bank1[2] = {
#include "assets/actor_503500_animation_18374_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation18374Bank4[15] = {
#include "assets/actor_503500_animation_18374_bank4.inc"
};

static AnimationRecord _gActor503500Animation18374Records[57] = {
#include "assets/actor_503500_animation_18374_records.inc"
};

static u16 _gActor503500Animation18374Indices[20] = {
#include "assets/actor_503500_animation_18374_indices.inc"
};

static AnimationSet _gActor503500Animation18374 = {
    _gActor503500Animation18374Records,
    _gActor503500Animation18374Indices,
    { NULL, _gActor503500Animation18374Bank1, NULL, NULL, _gActor503500Animation18374Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation18820Bank1[13] = {
#include "assets/actor_503500_animation_18820_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation18820Bank4[92] = {
#include "assets/actor_503500_animation_18820_bank4.inc"
};

static AnimationRecord _gActor503500Animation18820Records[148] = {
#include "assets/actor_503500_animation_18820_records.inc"
};

static u16 _gActor503500Animation18820Indices[20] = {
#include "assets/actor_503500_animation_18820_indices.inc"
};

static AnimationSet _gActor503500Animation18820 = {
    _gActor503500Animation18820Records,
    _gActor503500Animation18820Indices,
    { NULL, _gActor503500Animation18820Bank1, NULL, NULL, _gActor503500Animation18820Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation18B10Bank1[3] = {
#include "assets/actor_503500_animation_18B10_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation18B10Bank4[57] = {
#include "assets/actor_503500_animation_18B10_bank4.inc"
};

static AnimationRecord _gActor503500Animation18B10Records[102] = {
#include "assets/actor_503500_animation_18B10_records.inc"
};

static u16 _gActor503500Animation18B10Indices[20] = {
#include "assets/actor_503500_animation_18B10_indices.inc"
};

static AnimationSet _gActor503500Animation18B10 = {
    _gActor503500Animation18B10Records,
    _gActor503500Animation18B10Indices,
    { NULL, _gActor503500Animation18B10Bank1, NULL, NULL, _gActor503500Animation18B10Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation18FCCBank1[4] = {
#include "assets/actor_503500_animation_18FCC_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation18FCCBank4[82] = {
#include "assets/actor_503500_animation_18FCC_bank4.inc"
};

static AnimationRecord _gActor503500Animation18FCCRecords[189] = {
#include "assets/actor_503500_animation_18FCC_records.inc"
};

static u16 _gActor503500Animation18FCCIndices[20] = {
#include "assets/actor_503500_animation_18FCC_indices.inc"
};

static AnimationSet _gActor503500Animation18FCC = {
    _gActor503500Animation18FCCRecords,
    _gActor503500Animation18FCCIndices,
    { NULL, _gActor503500Animation18FCCBank1, NULL, NULL, _gActor503500Animation18FCCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation19354Bank1[3] = {
#include "assets/actor_503500_animation_19354_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation19354Bank4[58] = {
#include "assets/actor_503500_animation_19354_bank4.inc"
};

static AnimationRecord _gActor503500Animation19354Records[139] = {
#include "assets/actor_503500_animation_19354_records.inc"
};

static u16 _gActor503500Animation19354Indices[20] = {
#include "assets/actor_503500_animation_19354_indices.inc"
};

static AnimationSet _gActor503500Animation19354 = {
    _gActor503500Animation19354Records,
    _gActor503500Animation19354Indices,
    { NULL, _gActor503500Animation19354Bank1, NULL, NULL, _gActor503500Animation19354Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation195A4Bank1[3] = {
#include "assets/actor_503500_animation_195A4_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation195A4Bank4[29] = {
#include "assets/actor_503500_animation_195A4_bank4.inc"
};

static AnimationRecord _gActor503500Animation195A4Records[90] = {
#include "assets/actor_503500_animation_195A4_records.inc"
};

static u16 _gActor503500Animation195A4Indices[20] = {
#include "assets/actor_503500_animation_195A4_indices.inc"
};

static AnimationSet _gActor503500Animation195A4 = {
    _gActor503500Animation195A4Records,
    _gActor503500Animation195A4Indices,
    { NULL, _gActor503500Animation195A4Bank1, NULL, NULL, _gActor503500Animation195A4Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation19B10Bank1[10] = {
#include "assets/actor_503500_animation_19B10_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation19B10Bank4[126] = {
#include "assets/actor_503500_animation_19B10_bank4.inc"
};

static AnimationRecord _gActor503500Animation19B10Records[171] = {
#include "assets/actor_503500_animation_19B10_records.inc"
};

static u16 _gActor503500Animation19B10Indices[20] = {
#include "assets/actor_503500_animation_19B10_indices.inc"
};

static AnimationSet _gActor503500Animation19B10 = {
    _gActor503500Animation19B10Records,
    _gActor503500Animation19B10Indices,
    { NULL, _gActor503500Animation19B10Bank1, NULL, NULL, _gActor503500Animation19B10Bank4, NULL, NULL, NULL },
};

TaskDesc D_actor_503500_8014B958 = { { { TASK_BODY_NONE, 192 } }, _actor503500StaffCardSceneTask, { .value = 0 } };

TaskDesc D_actor_503500_8014B964[2] = {
    { { { TASK_BODY_COORD, 192 } }, _actor503500DriftSpriteEmitterTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, _actor503500FadeFromBlackTask, { .value = 0 } },
};

SVECTOR D_actor_503500_8014B97C[4] = {
    { 5000, 1000, 5000, 0 },
    { 8000, 1000, 7000, 0 },
    { 9000, 1000, 2000, 0 },
    { 6000, 1000, 9000, 0 },
};

AnimationSet* D_actor_503500_8014B99C[12] = {
    NULL,
    &_gActor503500Animation16D8C,
    &_gActor503500Animation16F18,
    &_gActor503500Animation1789C,
    &_gActor503500Animation181EC,
    &_gActor503500Animation18374,
    &_gActor503500Animation18B10,
    &_gActor503500Animation18FCC,
    &_gActor503500Animation19354,
    &_gActor503500Animation195A4,
    &_gActor503500Animation19B10,
    &_gActor503500Animation18820,
};

AnimationBankCopyRequest D_actor_503500_8014B9CC = { { .sets = D_actor_503500_8014B99C }, ARRAY_SIZE(D_actor_503500_8014B99C) };

AnimationPlayRequest D_actor_503500_8014B9D4 = { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_503500_8014B9E8 = { { .index = 1 }, 48, ANIMATION_BLEND_INTERPOLATE, 20, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_503500_8014B9FC[10] = {
    { { .index = 1 }, 49, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 50, ANIMATION_BLEND_INTERPOLATE, 10, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 51, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 52, ANIMATION_BLEND_INTERPOLATE, 4, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 53, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 54, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 55, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 56, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 57, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 58, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
};

AnimationPlayRequest D_actor_503500_8014BAC4 = { { .index = 1 }, 9, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_actor_503500_8014BAD8 = { { 2500, -2000, 6900, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_actor_503500_8014BAF0 = { { 2000, -2000, 6900, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_actor_503500_8014BB08 = { { 1990, -2000, 6580, 0 }, { 0, 910, 0, 0 } };

ActorTransform D_actor_503500_8014BB20 = { { 2240, -2000, 6770, 0 }, { 0, 910, 0, 0 } };

ActorTransform D_actor_503500_8014BB38 = { { 2000, 0, 1000, 0 }, { 0, 0, 0, 0 } };

AnimationPlayRequest D_actor_503500_8014BB50[5] = {
    { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 2, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 0 }, 14, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE },
    { { .index = 1 }, 4, ANIMATION_BLEND_INTERPOLATE, 40, ANIMATION_WORLD_COLLISION_DISABLE },
};

ActorTransform D_actor_503500_8014BBB4[2] = {
    { { 5200, 5200, 6500, 0 }, { 0, -1024, 0, 0 } },
    { { 8490, 1000, 6210, 0 }, { 0, -682, 0, 0 } },
};

ActorTransform D_actor_503500_8014BBE4 = { { 7000, 1000, 7000, 0 }, { 0, -1024, 0, 0 } };

ActorTransform D_actor_503500_8014BBFC = { { 7300, 500, 7600, 0 }, { 0, 1536, 0, 0 } };

ActorCommand D_actor_503500_8014BC14 = { { .loc = { 4, 48 } }, 0 };

ActorCommand D_actor_503500_8014BC18[2] = {
    { { .loc = { 4, 48 } }, 1 },
    { { .loc = { 4, 48 } }, 2 },
};

ActorCommand D_actor_503500_8014BC20 = { { .loc = { 4, 48 } }, 3 };

ActorCommand D_actor_503500_8014BC24 = { { .loc = { 4, 48 } }, 4 };

ActorCommand D_actor_503500_8014BC28 = { { .loc = { 4, 48 } }, 5 };

AnimationPlayRequest D_actor_503500_8014BC2C = { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_503500_8014BC40 = { { .index = 0 }, 1, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_actor_503500_8014BC54 = { { .index = 0 }, 2, ANIMATION_BLEND_INTERPOLATE, 4, ANIMATION_WORLD_COLLISION_DISABLE };

ActorTransform D_actor_503500_8014BC68 = { { 5190, 3920, 7820, 0 }, { 0, 1024, 0, 0 } };

ActorTransform D_actor_503500_8014BC80[2] = {
    { { 0x2B5C, 560, 7970, 0 }, { 0, 1024, 0, 0 } },
    { { 0x3232, 1610, 7970, 0 }, { 0, 1024, 0, 0 } },
};

ActorCommand D_actor_503500_8014BCB0 = { { .loc = { 4, 48 } }, 0 };

ActorCommand D_actor_503500_8014BCB4 = { { .loc = { 4, 48 } }, 1 };

ActorCommand D_actor_503500_8014BCB8 = { { .loc = { 4, 48 } }, 2 };

ActorCommand D_actor_503500_8014BCBC = { { .loc = { 4, 48 } }, 3 };

ActorTransform D_actor_503500_8014BCC0 = { { 3350, -3360, 0x2D82, 0 }, { 0, -512, 0, 0 } };

ActorTransform D_actor_503500_8014BCD8 = { { 0x311A, -3360, 2430, 0 }, { 0, 1535, 0, 0 } };

ActorTransform D_actor_503500_8014BCF0 = { { 7960, -3360, 7040, 0 }, { 0, -512, 0, 0 } };

ActorTransform D_actor_503500_8014BD08 = { { 7960, -3360, 7040, 0 }, { 0, 1535, 0, 0 } };

ActorCommand D_actor_503500_8014BD20 = { { .loc = { 4, 48 } }, ACTOR_503500_SLIDER_COMMAND_STOP };

ActorCommand D_actor_503500_8014BD24 = { { .loc = { 4, 48 } }, ACTOR_503500_SLIDER_COMMAND_PATH_FIRST };

ActorCommand D_actor_503500_8014BD28 = { { .loc = { 4, 48 } }, ACTOR_503500_SLIDER_COMMAND_PATH_SECOND };

ActorCommand D_actor_503500_8014BD2C = { { .loc = { 4, 48 } }, ACTOR_503500_SLIDER_COMMAND_SHAKE };

EvsSceneKey D_actor_503500_8014BD30 = { 6, 10, 11 };

EvsSceneKey D_actor_503500_8014BD38 = { 6, 11, 11 };

EvsSceneKey D_actor_503500_8014BD40 = { 6, 80, 11 };

EvsCommand D_actor_503500_8014BD48[56] = {
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_actor_503500_8014BD30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500StageSceneAudioStart }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_503500_8014B9CC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_503500_8014BAD8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = D_actor_503500_8014BBB4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .pointer = D_actor_503500_8014BC18 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor503500SpawnDriftSpriteEmitter }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor503500SpawnDriftSpriteEmitter }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor503500SpawnDriftSpriteEmitter }, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor503500SpawnFadeFromBlack }, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor503500SetRoomBackgroundSpritesVisible }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500ClearSavedPlayerTransform }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 10 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500EnqueueScenePlayback }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = D_actor_503500_8014B9FC }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 80 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_503500_8014B9FC[9] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_503500_8014B9FC[8] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_503500_8014B9FC[1] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 39 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_503500_8014B9FC[3] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 30 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_503500_8014BB50[2] } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_503500_8014B9FC[2] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_503500_8014BB50[4] } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor503500AddSessionFlowFlags }, { .value = GAME_SESSION_FLOW_SKIP_ENDING_MUSIC }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor503500AddSessionFlowFlags }, { .value = GAME_SESSION_FLOW_SKIP_AREA_MUSIC }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500FinishScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500CancelScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_503500_8014BAF0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_503500_8014BAC4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_503500_8014BBE4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_503500_8014BC14 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500CancelRoomEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = sceneEngageBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500CancelRoomEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_503500_8014C288[29] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500CancelRoomEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_503500_8014BAF0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_503500_8014B9CC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_503500_8014BAC4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_503500_8014BBE4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_503500_8014BC14 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500HaltPadScript }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500CancelScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor503500SetRoomBackgroundSpritesVisible }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500ClearSavedPlayerTransform }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_DIRTY_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_AREA_MUSIC, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500CancelRoomEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500CancelRoomEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor503500AddSessionFlowFlags }, { .value = GAME_SESSION_FLOW_SKIP_ENDING_MUSIC }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor503500AddSessionFlowFlags }, { .value = GAME_SESSION_FLOW_SKIP_AREA_MUSIC }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = sceneEngageBattle }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500CancelRoomEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_503500_8014C540[61] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_503500_8014B9CC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_503500_8014B9E8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_actor_503500_8014BD38 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500StageSceneAudioStart }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor503500AddSessionFlowFlags }, { .value = GAME_SESSION_FLOW_REEQUIP_WEAPON }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500LockAttachmentsForEvent }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500CancelRoomEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_AMBIENT_RGB, { .value = 100 }, { .value = 100 }, { .value = 100 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_503500_8014BB08 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_503500_8014BBB4[1] } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2003 }, { .message = { .pointer = &D_actor_503500_8014BB50[3] } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 12 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500EnqueueScenePlayback }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_503500_8014B9FC[7] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .pointer = &D_actor_503500_8014BC18[1] } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 3 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 85 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_FRAMEBUFFER_BLEND, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_FRAMEBUFFER_BLEND, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_503500_8014B9FC[4] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 300 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_503500_8014BB20 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 80 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_503500_8014B9FC[5] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2004 }, { .message = { .pointer = &D_actor_503500_8014BC68 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_503500_8014BC40 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_503500_8014BCB4 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_VIBRATION, { .padCommands = D_actor_503500_8014D2F0 }, { .vibrationSegments = D_actor_503500_8014D2F8 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 23 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_503500_8014BCB0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = 2003 }, { .message = { .pointer = &D_actor_503500_8014BC54 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_503500_8014BCB8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_503500_8014B9FC[6] }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_503500_8014BCB0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500FinishScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS8 = _actor503500ReleaseBossBattle }, { .value = 5 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_503500_8014BC20 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_503500_8014BCBC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor503500SetRoomBackgroundSpritesVisible }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_SKIP_TARGET, { .commands = NULL }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = 32 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = 33 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor503500SetStaffCardUseState }, { .value = ACTOR_503500_STAFF_CARD_USE_READY }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_503500_8014CAF8[28] = {
    { EVENT_SCRIPT_OPCODE_SET_FRAMEBUFFER_BLEND, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500HaltPadScript }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_STOP_AREA_MUSIC, { .value = 60 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor503500AddSessionFlowFlags }, { .value = GAME_SESSION_FLOW_REEQUIP_WEAPON }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_503500_8014BB20 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_503500_8014B9CC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_503500_8014B9E8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackS8 = _actor503500ReleaseBossBattle }, { .value = 11 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_503500_8014BC20 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 1 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_503500_8014BCBC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEAR_AMBIENT_RGB, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500LockAttachmentsForEvent }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500CancelRoomEffects }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500CancelScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor503500SetStaffCardUseState }, { .value = ACTOR_503500_STAFF_CARD_USE_READY }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callback = _actor503500SetRoomBackgroundSpritesVisible }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 3 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SET_DIRTY_VIEW, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 20 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RETURN_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = 32 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = 33 }, { .value = 2005 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_503500_8014CD98[32] = {
    { EVENT_SCRIPT_OPCODE_SELECT_SCENE, { .sceneKey = &D_actor_503500_8014BD40 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500StageSceneAudioStart }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackU8 = _actor503500SetPlayerUpdateHold }, { .value = 1 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 2 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_503500_8014B9CC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_503500_8014B9E8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = 32 }, { .value = 2004 }, { .message = { .pointer = &D_actor_503500_8014BCC0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = 33 }, { .value = 2004 }, { .message = { .pointer = &D_actor_503500_8014BCD8 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500RequestViewRespawn }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500EnqueueScenePlayback }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 4 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = 32 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_503500_8014BD2C } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1011 }, { .value = 2 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = 32 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = 33 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = 32 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_503500_8014BD24 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = 33 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_503500_8014BD28 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = 32 }, { .value = 2004 }, { .message = { .pointer = &D_actor_503500_8014BCF0 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = 33 }, { .value = 2004 }, { .message = { .pointer = &D_actor_503500_8014BD08 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = 32 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_503500_8014BD20 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = EVENT_SCRIPT_MESSAGE_TARGET_OTHER_SCENE_CHILD }, { .value = 33 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_503500_8014BD20 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_START_VIBRATION, { .padCommands = D_actor_503500_8014D300 }, { .vibrationSegments = D_actor_503500_8014D30C }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500FinishScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500ReloadPodBottom }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_503500_8014D098[8] = {
    { EVENT_SCRIPT_OPCODE_START_PRIMARY_FADE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_503500_8014B9CC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_503500_8014B9E8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_FRAMES, { .value = 8 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500CancelScene }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CLEANUP_SCENE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500ReloadPodBottom }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

EvsCommand D_actor_503500_8014D158[17] = {
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = ANIMATION_MESSAGE_COPY_BANK_EXTENSION }, { .message = { .pointer = &D_actor_503500_8014B9CC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_PLAY_WEAPON_ANIMATION, { .value = 3 }, { .value = 0 }, { .value = 1000 }, { .animation = &D_actor_503500_8014B9E8 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500ResetPlayerWeaponAttack }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500LockAttachmentsForEvent }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500SavePlayerTransform }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1001 }, { .message = { .pointer = &D_actor_503500_8014BB38 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_503500_8014BC24 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2005 }, { .value = 1 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = 2004 }, { .message = { .pointer = &D_actor_503500_8014BBFC } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_CAP_CONTROL }, { .value = 0 }, { .value = 4000 }, { .value = 5 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_WAIT_CAP_CUE, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_CALLBACK, { .callbackNoArg = _actor503500RestorePlayerTransform }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_SCENE }, { .value = 0 }, { .value = ACTOR_COMMAND_MESSAGE_APPLY }, { .message = { .command = &D_actor_503500_8014BC28 } }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_RESTORE_VIEW, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 }, { .value = 0 } },
    { EVENT_SCRIPT_OPCODE_SEND_MESSAGE, { .value = GAME_TASK_SLOT_PLAYER }, { .value = 0 }, { .value = 1009 }, { .value = 2 }, { .value = 0 } },
    { .opcode = EVENT_SCRIPT_OPCODE_END },
};

PadScriptCmd D_actor_503500_8014D2F0[2] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

PadScriptVibrationSegment D_actor_503500_8014D2F8[2] = {
    { 255, 107, 20, 1 },
    { 0, 0, 8, 0 },
};

PadScriptCmd D_actor_503500_8014D300[3] = {
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 1) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_PLAY, 2) },
    { PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0), PAD_SCRIPT_COMMAND(PAD_SCRIPT_STOP, 0) }
};

PadScriptVibrationSegment D_actor_503500_8014D30C[3] = {
    { 0, 0, 8, 0 },
    { 254, 252, 8, 1 },
    { 255, 22, 11, 1 },
};

static TmdBone _gActor503500BrahmanTorsoSkeleton[20] = {
#include "assets/brahman_torso_skeleton.inc"
};

static u32 _gActor503500BrahmanTorsoPartVerts[20] = {
#include "assets/brahman_torso_partVerts.inc"
};

static SVECTOR _gActor503500BrahmanTorsoVerts[441] = {
#include "assets/brahman_torso_verts.inc"
};

static SVECTOR _gActor503500BrahmanTorsoNormals[523] = {
#include "assets/brahman_torso_normals.inc"
};

static u32 _gActor503500BrahmanTorsoStream[5235] = {
#include "assets/brahman_torso_stream.inc"
};

static TmdSource _gActor503500BrahmanTorso = {
    0,
    27988,
    8696,
    20,
    _gActor503500BrahmanTorsoPartVerts,
    _gActor503500BrahmanTorsoVerts,
    _gActor503500BrahmanTorsoNormals,
    _gActor503500BrahmanTorsoSkeleton,
    _gActor503500BrahmanTorsoStream,
};

static TmdBone _gActor503500Model22A30Skeleton[1] = {
#include "assets/actor_503500_model_22A30_skeleton.inc"
};

static u32 _gActor503500Model22A30PartVerts[1] = {
#include "assets/actor_503500_model_22A30_partVerts.inc"
};

static SVECTOR _gActor503500Model22A30Verts[30] = {
#include "assets/actor_503500_model_22A30_verts.inc"
};

static SVECTOR _gActor503500Model22A30Normals[30] = {
#include "assets/actor_503500_model_22A30_normals.inc"
};

static u32 _gActor503500Model22A30Stream[250] = {
#include "assets/actor_503500_model_22A30_stream.inc"
};

static TmdSource _gActor503500Model22A30 = {
    0,
    1708,
    0,
    1,
    _gActor503500Model22A30PartVerts,
    _gActor503500Model22A30Verts,
    _gActor503500Model22A30Normals,
    _gActor503500Model22A30Skeleton,
    _gActor503500Model22A30Stream,
};

static TmdBone _gActor503500Model23550Skeleton[9] = {
#include "assets/actor_503500_model_23550_skeleton.inc"
};

static u32 _gActor503500Model23550PartVerts[9] = {
#include "assets/actor_503500_model_23550_partVerts.inc"
};

static SVECTOR _gActor503500Model23550Verts[85] = {
#include "assets/actor_503500_model_23550_verts.inc"
};

static SVECTOR _gActor503500Model23550Normals[96] = {
#include "assets/actor_503500_model_23550_normals.inc"
};

static u32 _gActor503500Model23550Stream[1019] = {
#include "assets/actor_503500_model_23550_stream.inc"
};

static TmdSource _gActor503500Model23550 = {
    0,
    4624,
    2912,
    9,
    _gActor503500Model23550PartVerts,
    _gActor503500Model23550Verts,
    _gActor503500Model23550Normals,
    _gActor503500Model23550Skeleton,
    _gActor503500Model23550Stream,
};

static TmdBone _gActor503500Model24C70Skeleton[9] = {
#include "assets/actor_503500_model_24C70_skeleton.inc"
};

static u32 _gActor503500Model24C70PartVerts[9] = {
#include "assets/actor_503500_model_24C70_partVerts.inc"
};

static SVECTOR _gActor503500Model24C70Verts[85] = {
#include "assets/actor_503500_model_24C70_verts.inc"
};

static SVECTOR _gActor503500Model24C70Normals[96] = {
#include "assets/actor_503500_model_24C70_normals.inc"
};

static u32 _gActor503500Model24C70Stream[1019] = {
#include "assets/actor_503500_model_24C70_stream.inc"
};

static TmdSource _gActor503500Model24C70 = {
    0,
    4624,
    2912,
    9,
    _gActor503500Model24C70PartVerts,
    _gActor503500Model24C70Verts,
    _gActor503500Model24C70Normals,
    _gActor503500Model24C70Skeleton,
    _gActor503500Model24C70Stream,
};

static TmdBone _gActor503500Model261C8Skeleton[9] = {
#include "assets/actor_503500_model_261C8_skeleton.inc"
};

static u32 _gActor503500Model261C8PartVerts[9] = {
#include "assets/actor_503500_model_261C8_partVerts.inc"
};

static SVECTOR _gActor503500Model261C8Verts[54] = {
#include "assets/actor_503500_model_261C8_verts.inc"
};

static SVECTOR _gActor503500Model261C8Normals[70] = {
#include "assets/actor_503500_model_261C8_normals.inc"
};

static u32 _gActor503500Model261C8Stream[661] = {
#include "assets/actor_503500_model_261C8_stream.inc"
};

static TmdSource _gActor503500Model261C8 = {
    0,
    2928,
    1820,
    9,
    _gActor503500Model261C8PartVerts,
    _gActor503500Model261C8Verts,
    _gActor503500Model261C8Normals,
    _gActor503500Model261C8Skeleton,
    _gActor503500Model261C8Stream,
};

static TmdBone _gActor503500Model27198Skeleton[9] = {
#include "assets/actor_503500_model_27198_skeleton.inc"
};

static u32 _gActor503500Model27198PartVerts[9] = {
#include "assets/actor_503500_model_27198_partVerts.inc"
};

static SVECTOR _gActor503500Model27198Verts[54] = {
#include "assets/actor_503500_model_27198_verts.inc"
};

static SVECTOR _gActor503500Model27198Normals[72] = {
#include "assets/actor_503500_model_27198_normals.inc"
};

static u32 _gActor503500Model27198Stream[661] = {
#include "assets/actor_503500_model_27198_stream.inc"
};

static TmdSource _gActor503500Model27198 = {
    0,
    2928,
    1820,
    9,
    _gActor503500Model27198PartVerts,
    _gActor503500Model27198Verts,
    _gActor503500Model27198Normals,
    _gActor503500Model27198Skeleton,
    _gActor503500Model27198Stream,
};

static TmdBone _gActor503500Model28168Skeleton[9] = {
#include "assets/actor_503500_model_28168_skeleton.inc"
};

static u32 _gActor503500Model28168PartVerts[9] = {
#include "assets/actor_503500_model_28168_partVerts.inc"
};

static SVECTOR _gActor503500Model28168Verts[54] = {
#include "assets/actor_503500_model_28168_verts.inc"
};

static SVECTOR _gActor503500Model28168Normals[72] = {
#include "assets/actor_503500_model_28168_normals.inc"
};

static u32 _gActor503500Model28168Stream[661] = {
#include "assets/actor_503500_model_28168_stream.inc"
};

static TmdSource _gActor503500Model28168 = {
    0,
    2928,
    1820,
    9,
    _gActor503500Model28168PartVerts,
    _gActor503500Model28168Verts,
    _gActor503500Model28168Normals,
    _gActor503500Model28168Skeleton,
    _gActor503500Model28168Stream,
};

static TmdBone _gActor503500Model29128Skeleton[9] = {
#include "assets/actor_503500_model_29128_skeleton.inc"
};

static u32 _gActor503500Model29128PartVerts[9] = {
#include "assets/actor_503500_model_29128_partVerts.inc"
};

static SVECTOR _gActor503500Model29128Verts[54] = {
#include "assets/actor_503500_model_29128_verts.inc"
};

static SVECTOR _gActor503500Model29128Normals[70] = {
#include "assets/actor_503500_model_29128_normals.inc"
};

static u32 _gActor503500Model29128Stream[661] = {
#include "assets/actor_503500_model_29128_stream.inc"
};

static TmdSource _gActor503500Model29128 = {
    0,
    2928,
    1820,
    9,
    _gActor503500Model29128PartVerts,
    _gActor503500Model29128Verts,
    _gActor503500Model29128Normals,
    _gActor503500Model29128Skeleton,
    _gActor503500Model29128Stream,
};

static TmdBone _gActor503500Model2A450Skeleton[4] = {
#include "assets/actor_503500_model_2A450_skeleton.inc"
};

static u32 _gActor503500Model2A450PartVerts[4] = {
#include "assets/actor_503500_model_2A450_partVerts.inc"
};

static SVECTOR _gActor503500Model2A450Verts[129] = {
#include "assets/actor_503500_model_2A450_verts.inc"
};

static SVECTOR _gActor503500Model2A450Normals[129] = {
#include "assets/actor_503500_model_2A450_normals.inc"
};

static u32 _gActor503500Model2A450Stream[1339] = {
#include "assets/actor_503500_model_2A450_stream.inc"
};

static TmdSource _gActor503500Model2A450 = {
    0,
    8228,
    992,
    4,
    _gActor503500Model2A450PartVerts,
    _gActor503500Model2A450Verts,
    _gActor503500Model2A450Normals,
    _gActor503500Model2A450Skeleton,
    _gActor503500Model2A450Stream,
};

static TmdBone _gActor503500Model2C1C0Skeleton[4] = {
#include "assets/actor_503500_model_2C1C0_skeleton.inc"
};

static u32 _gActor503500Model2C1C0PartVerts[4] = {
#include "assets/actor_503500_model_2C1C0_partVerts.inc"
};

static SVECTOR _gActor503500Model2C1C0Verts[124] = {
#include "assets/actor_503500_model_2C1C0_verts.inc"
};

static SVECTOR _gActor503500Model2C1C0Normals[124] = {
#include "assets/actor_503500_model_2C1C0_normals.inc"
};

static u32 _gActor503500Model2C1C0Stream[1288] = {
#include "assets/actor_503500_model_2C1C0_stream.inc"
};

static TmdSource _gActor503500Model2C1C0 = {
    0,
    7968,
    860,
    4,
    _gActor503500Model2C1C0PartVerts,
    _gActor503500Model2C1C0Verts,
    _gActor503500Model2C1C0Normals,
    _gActor503500Model2C1C0Skeleton,
    _gActor503500Model2C1C0Stream,
};

static AnimationPackedPose _gActor503500Animation2DB14Bank1[3] = {
#include "assets/actor_503500_animation_2DB14_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation2DB14Bank4[99] = {
#include "assets/actor_503500_animation_2DB14_bank4.inc"
};

static AnimationRecord _gActor503500Animation2DB14Records[207] = {
#include "assets/actor_503500_animation_2DB14_records.inc"
};

static u16 _gActor503500Animation2DB14Indices[20] = {
#include "assets/actor_503500_animation_2DB14_indices.inc"
};

AnimationSet gActor503500Animation2DB14 = {
    _gActor503500Animation2DB14Records,
    _gActor503500Animation2DB14Indices,
    { NULL, _gActor503500Animation2DB14Bank1, NULL, NULL, _gActor503500Animation2DB14Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation2E4DCBank1[15] = {
#include "assets/actor_503500_animation_2E4DC_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation2E4DCBank4[230] = {
#include "assets/actor_503500_animation_2E4DC_bank4.inc"
};

static AnimationRecord _gActor503500Animation2E4DCRecords[331] = {
#include "assets/actor_503500_animation_2E4DC_records.inc"
};

static u16 _gActor503500Animation2E4DCIndices[20] = {
#include "assets/actor_503500_animation_2E4DC_indices.inc"
};

AnimationSet gActor503500Animation2E4DC = {
    _gActor503500Animation2E4DCRecords,
    _gActor503500Animation2E4DCIndices,
    { NULL, _gActor503500Animation2E4DCBank1, NULL, NULL, _gActor503500Animation2E4DCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation2EF88Bank1[15] = {
#include "assets/actor_503500_animation_2EF88_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation2EF88Bank4[247] = {
#include "assets/actor_503500_animation_2EF88_bank4.inc"
};

static AnimationRecord _gActor503500Animation2EF88Records[371] = {
#include "assets/actor_503500_animation_2EF88_records.inc"
};

static u16 _gActor503500Animation2EF88Indices[20] = {
#include "assets/actor_503500_animation_2EF88_indices.inc"
};

AnimationSet gActor503500Animation2EF88 = {
    _gActor503500Animation2EF88Records,
    _gActor503500Animation2EF88Indices,
    { NULL, _gActor503500Animation2EF88Bank1, NULL, NULL, _gActor503500Animation2EF88Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation2FC70Bank1[18] = {
#include "assets/actor_503500_animation_2FC70_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation2FC70Bank4[304] = {
#include "assets/actor_503500_animation_2FC70_bank4.inc"
};

static AnimationRecord _gActor503500Animation2FC70Records[448] = {
#include "assets/actor_503500_animation_2FC70_records.inc"
};

static u16 _gActor503500Animation2FC70Indices[20] = {
#include "assets/actor_503500_animation_2FC70_indices.inc"
};

AnimationSet gActor503500Animation2FC70 = {
    _gActor503500Animation2FC70Records,
    _gActor503500Animation2FC70Indices,
    { NULL, _gActor503500Animation2FC70Bank1, NULL, NULL, _gActor503500Animation2FC70Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation306E0Bank1[20] = {
#include "assets/actor_503500_animation_306E0_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation306E0Bank4[181] = {
#include "assets/actor_503500_animation_306E0_bank4.inc"
};

static AnimationRecord _gActor503500Animation306E0Records[407] = {
#include "assets/actor_503500_animation_306E0_records.inc"
};

static u16 _gActor503500Animation306E0Indices[20] = {
#include "assets/actor_503500_animation_306E0_indices.inc"
};

AnimationSet gActor503500Animation306E0 = {
    _gActor503500Animation306E0Records,
    _gActor503500Animation306E0Indices,
    { NULL, _gActor503500Animation306E0Bank1, NULL, NULL, _gActor503500Animation306E0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation30F1CBank1[16] = {
#include "assets/actor_503500_animation_30F1C_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation30F1CBank4[166] = {
#include "assets/actor_503500_animation_30F1C_bank4.inc"
};

static AnimationRecord _gActor503500Animation30F1CRecords[293] = {
#include "assets/actor_503500_animation_30F1C_records.inc"
};

static u16 _gActor503500Animation30F1CIndices[20] = {
#include "assets/actor_503500_animation_30F1C_indices.inc"
};

AnimationSet gActor503500Animation30F1C = {
    _gActor503500Animation30F1CRecords,
    _gActor503500Animation30F1CIndices,
    { NULL, _gActor503500Animation30F1CBank1, NULL, NULL, _gActor503500Animation30F1CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation31788Bank1[18] = {
#include "assets/actor_503500_animation_31788_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation31788Bank4[183] = {
#include "assets/actor_503500_animation_31788_bank4.inc"
};

static AnimationRecord _gActor503500Animation31788Records[282] = {
#include "assets/actor_503500_animation_31788_records.inc"
};

static u16 _gActor503500Animation31788Indices[20] = {
#include "assets/actor_503500_animation_31788_indices.inc"
};

AnimationSet gActor503500Animation31788 = {
    _gActor503500Animation31788Records,
    _gActor503500Animation31788Indices,
    { NULL, _gActor503500Animation31788Bank1, NULL, NULL, _gActor503500Animation31788Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation31D8CBank1[12] = {
#include "assets/actor_503500_animation_31D8C_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation31D8CBank4[131] = {
#include "assets/actor_503500_animation_31D8C_bank4.inc"
};

static AnimationRecord _gActor503500Animation31D8CRecords[198] = {
#include "assets/actor_503500_animation_31D8C_records.inc"
};

static u16 _gActor503500Animation31D8CIndices[20] = {
#include "assets/actor_503500_animation_31D8C_indices.inc"
};

AnimationSet gActor503500Animation31D8C = {
    _gActor503500Animation31D8CRecords,
    _gActor503500Animation31D8CIndices,
    { NULL, _gActor503500Animation31D8CBank1, NULL, NULL, _gActor503500Animation31D8CBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation32E24Bank1[43] = {
#include "assets/actor_503500_animation_32E24_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation32E24Bank4[310] = {
#include "assets/actor_503500_animation_32E24_bank4.inc"
};

static AnimationRecord _gActor503500Animation32E24Records[603] = {
#include "assets/actor_503500_animation_32E24_records.inc"
};

static u16 _gActor503500Animation32E24Indices[20] = {
#include "assets/actor_503500_animation_32E24_indices.inc"
};

AnimationSet gActor503500Animation32E24 = {
    _gActor503500Animation32E24Records,
    _gActor503500Animation32E24Indices,
    { NULL, _gActor503500Animation32E24Bank1, NULL, NULL, _gActor503500Animation32E24Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation333DCBank1[14] = {
#include "assets/actor_503500_animation_333DC_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation333DCBank4[101] = {
#include "assets/actor_503500_animation_333DC_bank4.inc"
};

static AnimationRecord _gActor503500Animation333DCRecords[203] = {
#include "assets/actor_503500_animation_333DC_records.inc"
};

static u16 _gActor503500Animation333DCIndices[20] = {
#include "assets/actor_503500_animation_333DC_indices.inc"
};

AnimationSet gActor503500Animation333DC = {
    _gActor503500Animation333DCRecords,
    _gActor503500Animation333DCIndices,
    { NULL, _gActor503500Animation333DCBank1, NULL, NULL, _gActor503500Animation333DCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation33C14Bank1[21] = {
#include "assets/actor_503500_animation_33C14_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation33C14Bank4[164] = {
#include "assets/actor_503500_animation_33C14_bank4.inc"
};

static AnimationRecord _gActor503500Animation33C14Records[279] = {
#include "assets/actor_503500_animation_33C14_records.inc"
};

static u16 _gActor503500Animation33C14Indices[20] = {
#include "assets/actor_503500_animation_33C14_indices.inc"
};

AnimationSet gActor503500Animation33C14 = {
    _gActor503500Animation33C14Records,
    _gActor503500Animation33C14Indices,
    { NULL, _gActor503500Animation33C14Bank1, NULL, NULL, _gActor503500Animation33C14Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation33EBCBank1[5] = {
#include "assets/actor_503500_animation_33EBC_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation33EBCBank4[30] = {
#include "assets/actor_503500_animation_33EBC_bank4.inc"
};

static AnimationRecord _gActor503500Animation33EBCRecords[105] = {
#include "assets/actor_503500_animation_33EBC_records.inc"
};

static u16 _gActor503500Animation33EBCIndices[20] = {
#include "assets/actor_503500_animation_33EBC_indices.inc"
};

AnimationSet gActor503500Animation33EBC = {
    _gActor503500Animation33EBCRecords,
    _gActor503500Animation33EBCIndices,
    { NULL, _gActor503500Animation33EBCBank1, NULL, NULL, _gActor503500Animation33EBCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation341D8Bank1[5] = {
#include "assets/actor_503500_animation_341D8_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation341D8Bank4[37] = {
#include "assets/actor_503500_animation_341D8_bank4.inc"
};

static AnimationRecord _gActor503500Animation341D8Records[127] = {
#include "assets/actor_503500_animation_341D8_records.inc"
};

static u16 _gActor503500Animation341D8Indices[20] = {
#include "assets/actor_503500_animation_341D8_indices.inc"
};

AnimationSet gActor503500Animation341D8 = {
    _gActor503500Animation341D8Records,
    _gActor503500Animation341D8Indices,
    { NULL, _gActor503500Animation341D8Bank1, NULL, NULL, _gActor503500Animation341D8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation350C8Bank1[48] = {
#include "assets/actor_503500_animation_350C8_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation350C8Bank4[270] = {
#include "assets/actor_503500_animation_350C8_bank4.inc"
};

static AnimationRecord _gActor503500Animation350C8Records[522] = {
#include "assets/actor_503500_animation_350C8_records.inc"
};

static u16 _gActor503500Animation350C8Indices[20] = {
#include "assets/actor_503500_animation_350C8_indices.inc"
};

AnimationSet gActor503500Animation350C8 = {
    _gActor503500Animation350C8Records,
    _gActor503500Animation350C8Indices,
    { NULL, _gActor503500Animation350C8Bank1, NULL, NULL, _gActor503500Animation350C8Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation35390Bank1[5] = {
#include "assets/actor_503500_animation_35390_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation35390Bank4[34] = {
#include "assets/actor_503500_animation_35390_bank4.inc"
};

static AnimationRecord _gActor503500Animation35390Records[109] = {
#include "assets/actor_503500_animation_35390_records.inc"
};

static u16 _gActor503500Animation35390Indices[20] = {
#include "assets/actor_503500_animation_35390_indices.inc"
};

AnimationSet gActor503500Animation35390 = {
    _gActor503500Animation35390Records,
    _gActor503500Animation35390Indices,
    { NULL, _gActor503500Animation35390Bank1, NULL, NULL, _gActor503500Animation35390Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation356DCBank1[6] = {
#include "assets/actor_503500_animation_356DC_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation356DCBank4[43] = {
#include "assets/actor_503500_animation_356DC_bank4.inc"
};

static AnimationRecord _gActor503500Animation356DCRecords[130] = {
#include "assets/actor_503500_animation_356DC_records.inc"
};

static u16 _gActor503500Animation356DCIndices[20] = {
#include "assets/actor_503500_animation_356DC_indices.inc"
};

AnimationSet gActor503500Animation356DC = {
    _gActor503500Animation356DCRecords,
    _gActor503500Animation356DCIndices,
    { NULL, _gActor503500Animation356DCBank1, NULL, NULL, _gActor503500Animation356DCBank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation38AE0Bank1[110] = {
#include "assets/actor_503500_animation_38AE0_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation38AE0Bank4[1079] = {
#include "assets/actor_503500_animation_38AE0_bank4.inc"
};

static AnimationRecord _gActor503500Animation38AE0Records[1900] = {
#include "assets/actor_503500_animation_38AE0_records.inc"
};

static u16 _gActor503500Animation38AE0Indices[20] = {
#include "assets/actor_503500_animation_38AE0_indices.inc"
};

AnimationSet gActor503500Animation38AE0 = {
    _gActor503500Animation38AE0Records,
    _gActor503500Animation38AE0Indices,
    { NULL, _gActor503500Animation38AE0Bank1, NULL, NULL, _gActor503500Animation38AE0Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation3A190Bank1[101] = {
#include "assets/actor_503500_animation_3A190_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation3A190Bank4[476] = {
#include "assets/actor_503500_animation_3A190_bank4.inc"
};

static AnimationRecord _gActor503500Animation3A190Records[653] = {
#include "assets/actor_503500_animation_3A190_records.inc"
};

static u16 _gActor503500Animation3A190Indices[20] = {
#include "assets/actor_503500_animation_3A190_indices.inc"
};

AnimationSet gActor503500Animation3A190 = {
    _gActor503500Animation3A190Records,
    _gActor503500Animation3A190Indices,
    { NULL, _gActor503500Animation3A190Bank1, NULL, NULL, _gActor503500Animation3A190Bank4, NULL, NULL, NULL },
};

static AnimationPackedPose _gActor503500Animation3C968Bank1[151] = {
#include "assets/actor_503500_animation_3C968_bank1.inc"
};

static AnimationPackedRotation _gActor503500Animation3C968Bank4[928] = {
#include "assets/actor_503500_animation_3C968_bank4.inc"
};

static AnimationRecord _gActor503500Animation3C968Records[1149] = {
#include "assets/actor_503500_animation_3C968_records.inc"
};

static u16 _gActor503500Animation3C968Indices[20] = {
#include "assets/actor_503500_animation_3C968_indices.inc"
};

AnimationSet gActor503500Animation3C968 = {
    _gActor503500Animation3C968Records,
    _gActor503500Animation3C968Indices,
    { NULL, _gActor503500Animation3C968Bank1, NULL, NULL, _gActor503500Animation3C968Bank4, NULL, NULL, NULL },
};

DamageAttack D_actor_503500_8016E7B0[2] = {
    { 20, 2 },
    { 25, 2 },
};

DamageAttack D_actor_503500_8016E7B8[2] = {
    { 15, 3 },
    { 15, 2 },
};

DamageAttack D_actor_503500_8016E7C0[1] = {
    { 40, 6 },
};

DamageAttack D_actor_503500_8016E7C4[1] = {
    { 30, 1 },
};

DamageAttack D_actor_503500_8016E7C8[1] = {
    { 120, 6 },
};

DamageAttack* D_actor_503500_8016E7CC[1] = {
    D_actor_503500_8016E7B0,
};

DamageAttack* D_actor_503500_8016E7D0[1] = {
    D_actor_503500_8016E7B8,
};

DamageAttack* D_actor_503500_8016E7D4[2] = {
    D_actor_503500_8016E7C0,
    D_actor_503500_8016E7C4,
};

DamageAttack* D_actor_503500_8016E7DC[1] = {
    D_actor_503500_8016E7C8,
};

DamageAttack D_actor_503500_8016E7E0[1] = { 0 };

DamageAttack D_actor_503500_8016E7E4[1] = {
    { 25, 7 },
};

DamageAttack D_actor_503500_8016E7E8[1] = {
    { 45, 7 },
};

EnemyParams D_actor_503500_8016E7EC[17] = {
    { D_actor_503500_8016E7E0, 3500, 300, 500, 200, 250, 3, 0, 0 },
    { D_actor_503500_8016E7E0, 1500, 100, 500, 0, 100, 0, 0, 0 },
    { D_actor_503500_8016E7E0, 500, 0, 0, 0, 100, 10, 100, 10 },
    { D_actor_503500_8016E7E0, 500, 0, 0, 0, 100, 10, 100, 10 },
    { D_actor_503500_8016E7E0, 700, 0, 0, 0, 100, 0, 0, 0 },
    { D_actor_503500_8016E7E0, 700, 0, 0, 0, 100, 0, 0, 0 },
    { D_actor_503500_8016E7E0, 1500, 700, 2000, 0, 100, 0, 0, 0 },
    { D_actor_503500_8016E7E0, 700, 800, 3000, 0, 100, 0, 0, 0 },
    { D_actor_503500_8016E7E0, 700, 800, 3000, 0, 100, 0, 0, 0 },
    { D_actor_503500_8016E7E0, 1500, 200, 500, 0, 100, 0, 0, 0 },
    { D_actor_503500_8016E7E8, 1000, 1000, 5000, 0, 100, 0, 0, 0 },
    { D_actor_503500_8016E7E8, 1000, 1000, 5000, 0, 100, 0, 0, 0 },
    { D_actor_503500_8016E7E0, 700, 100, 500, 0, 100, 0, 0, 0 },
    { D_actor_503500_8016E7E4, 100, 0, 0, 0, 100, 10, 100, 10 },
    { D_actor_503500_8016E7E4, 100, 0, 0, 0, 100, 10, 100, 10 },
    { D_actor_503500_8016E7E4, 100, 0, 0, 0, 100, 10, 100, 10 },
    { D_actor_503500_8016E7E4, 100, 0, 0, 0, 100, 10, 100, 10 },
};

s8 D_actor_503500_8016E8FC[20] = {
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    2,
    2,
    2,
    2,
    0,
    1,
    1,
    2,
    2,
    2,
    1,
    2,
    0,
};

u8 D_actor_503500_8016E910[20] = {
    15,
    107,
    55,
    87,
    59,
    91,
    118,
    55,
    87,
    108,
    59,
    91,
    11,
    55,
    55,
    87,
    87,
    0,
    0,
    0,
};

TaskDesc D_actor_503500_8016E924[17] = {
    { { { TASK_BODY_TMD, 96 } }, actor503500BossTask, { .model = &_gActor503500BrahmanTorso } },
    { { { TASK_BODY_TMD, 96 } }, actor503500PinkFlashEmitterTask, { .model = &_gActor503500Model22A30 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, actor503500LargeChainTask, { .model = &_gActor503500Model24C70 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, actor503500LargeChainTask, { .model = &_gActor503500Model23550 } },
    { { { TASK_BODY_COORD, 96 } }, actor503500LargeOrbEmitterTask, { .value = 0 } },
    { { { TASK_BODY_COORD, 96 } }, actor503500LargeOrbEmitterTask, { .value = 0 } },
    { { { TASK_BODY_COORD, 96 } }, actor503500RearPartTask, { .value = 0 } },
    { { { TASK_BODY_COORD, 96 } }, actor503500ChainBaseTask, { .value = 0 } },
    { { { TASK_BODY_COORD, 96 } }, actor503500ChainBaseTask, { .value = 0 } },
    { { { TASK_BODY_COORD, 96 } }, actor503500SmallOrbEmitterTask, { .value = 0 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, actor503500ArmTask, { .model = &_gActor503500Model2A450 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, actor503500ArmTask, { .model = &_gActor503500Model2C1C0 } },
    { { { TASK_BODY_COORD, 96 } }, actor503500YellowFlashEmitterTask, { .value = 0 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, actor503500LungingChainTask, { .model = &_gActor503500Model27198 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, actor503500LungingChainTask, { .model = &_gActor503500Model261C8 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, actor503500LungingChainTask, { .model = &_gActor503500Model29128 } },
    { { { (TASK_BODY_TMD | TASK_DESC_SKIP_AUTO_MODEL_BUFFER), 96 } }, actor503500LungingChainTask, { .model = &_gActor503500Model28168 } },
};

TaskDesc D_actor_503500_8016E9F0[5] = {
    { { { TASK_BODY_COORD, 192 } }, actor503500BallisticShotTask, { .value = 0 } },
    { { { TASK_BODY_COORD, 192 } }, actor503500LingeringShotTask, { .value = 0 } },
    { { { TASK_BODY_COORD, 192 } }, actor503500PinkFlashAttackTask, { .value = 0 } },
    { { { TASK_BODY_COORD, 192 } }, actor503500YellowFlashAttackTask, { .value = 0 } },
    { { { TASK_BODY_COORD, 192 } }, actor503500OrangeFlashAttackTask, { .value = 0 } },
};

/// Emits drift sprites from a fixed Shelter R48 position until its scene winds down.
///
/// Requires a coordinate body and spawn argument 1 in 0..3. Task teardown owns
/// the allocated work; spawned sprites have independent lifetimes. Only actor-running
/// ticks advance the spawn counter; scene-state wind-down runs on every call.
/// Scene values 0/1 require a running, unskipped event; 2/3 wait for the event
/// to end before shrinking sprites and increasing the interval; 4 only increases
/// the interval. Other values end the task. Initialization also runs the emitter.
static void _actor503500DriftSpriteEmitterTask(Task* task)
{
    enum {
        ACTOR_503500_DRIFT_SPRITE_EMITTER_INITIALIZE          = 0,
        ACTOR_503500_DRIFT_SPRITE_EMITTER_EMITTING            = 1,
        ACTOR_503500_DRIFT_SCENE_INACTIVE                     = 0,
        ACTOR_503500_DRIFT_SCENE_ENTRY                        = 1,
        ACTOR_503500_DRIFT_SCENE_PHASE_TRANSITION             = 2,
        ACTOR_503500_DRIFT_SCENE_DEFEAT                       = 3,
        ACTOR_503500_DRIFT_SCENE_STAFF_CARD                   = 4,
        ACTOR_503500_DRIFT_SPRITE_EMITTER_SPRITE_SIZE         = 0xC00,      // Sprite size until the emitter winds down
        ACTOR_503500_DRIFT_SPRITE_EMITTER_SPRITE_SIZE_STEP    = 0x10,       // Size lost per winding-down frame
        ACTOR_503500_DRIFT_SPRITE_EMITTER_SPRITE_SIZE_MIN     = 0x100,      // Floor of the shrinking size
        ACTOR_503500_DRIFT_SPRITE_EMITTER_CELL_PERIOD         = 0x4000,     // Four frames per cell until the emitter winds down
        ACTOR_503500_DRIFT_SPRITE_EMITTER_CELL_PERIOD_STEP    = 0x20,       // 1/128 of a frame lost per winding-down frame
        ACTOR_503500_DRIFT_SPRITE_EMITTER_CELL_PERIOD_MIN     = 0x1000,     // Floor of the shrinking period, one frame per cell
        ACTOR_503500_DRIFT_SPRITE_EMITTER_SPAWN_INTERVAL      = 0x60000,    // Six frames between sprites until the emitter winds down
        ACTOR_503500_DRIFT_SPRITE_EMITTER_SPAWN_INTERVAL_STEP = 0x1000,     // 1/16 of a frame gained per winding-down frame
        ACTOR_503500_DRIFT_SPRITE_EMITTER_SPAWN_INTERVAL_MAX  = 0x100000,   // Sixteen frames; the emitter ends once its interval is longer
        ACTOR_503500_DRIFT_SPRITE_EMITTER_SPAWN_SIZE_MASK     = 0xFFF,      // Size bits of the drift sprite's spawn argument
        ACTOR_503500_DRIFT_SPRITE_EMITTER_SPAWN_PERIOD_MASK   = 0xF000,     // Frames-per-cell nibble of the drift sprite's spawn argument
        ACTOR_503500_DRIFT_SPRITE_EMITTER_SPAWN_NARROW_UPWARD = 0x03800000, // Movement kind 3 (random, narrowly upward) at speed 0x80
    };
    GfxCoord*                           coord;
    _Actor503500DriftSpriteEmitterWork* work;
    SVECTOR*                            spawnPosition;
    u8                                  finished;

    coord = task->extra.coordBody->coord;
    if (task->state == ACTOR_503500_DRIFT_SPRITE_EMITTER_INITIALIZE) {
        spawnPosition     = &D_actor_503500_8014B97C[task->spawnArg1.value];
        coord->coord.t[0] = spawnPosition->vx;
        coord->coord.t[1] = spawnPosition->vy;
        coord->coord.t[2] = spawnPosition->vz;
        gfxSetRotIdentity(&coord->coord);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        work                = memCalloc(sizeof(*work), false);
        if (work == NULL) {
            taskKill(task);
            return;
        }
        task->work               = work;
        work->spriteSize         = ACTOR_503500_DRIFT_SPRITE_EMITTER_SPRITE_SIZE;
        work->cellPeriod         = ACTOR_503500_DRIFT_SPRITE_EMITTER_CELL_PERIOD;
        work->spawnInterval.word = ACTOR_503500_DRIFT_SPRITE_EMITTER_SPAWN_INTERVAL;
        task->state++; // Enters ACTOR_503500_DRIFT_SPRITE_EMITTER_EMITTING; emission runs this tick too.
    }
    work = task->work;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        if (work->spawnInterval.halves.integer < ++task->killCountdown) {
            task->killCountdown = 0;
            effectSpawn(EFFECT_SHELTER_R48_DRIFT_SPRITE, coord,
                        (work->cellPeriod & ACTOR_503500_DRIFT_SPRITE_EMITTER_SPAWN_PERIOD_MASK) | ACTOR_503500_DRIFT_SPRITE_EMITTER_SPAWN_NARROW_UPWARD | (work->spriteSize & ACTOR_503500_DRIFT_SPRITE_EMITTER_SPAWN_SIZE_MASK), NULL);
        }
    }
    switch (gameFlagGetNibble(GAME_FLAG_SHELTER_R48_SCENE_STATE)) {
        case ACTOR_503500_DRIFT_SCENE_INACTIVE:
        case ACTOR_503500_DRIFT_SCENE_ENTRY:
            if (gGameSession->eventState == 0) {
                taskKill(task);
                return;
            }
            finished = gGameSession->evtSkipped;
            break;
        case ACTOR_503500_DRIFT_SCENE_PHASE_TRANSITION:
        case ACTOR_503500_DRIFT_SCENE_DEFEAT:
            if (gGameSession->eventState != 0) {
                return;
            }
            // Winding down: later sprites are smaller and shorter-lived.
            work->cellPeriod -= ACTOR_503500_DRIFT_SPRITE_EMITTER_CELL_PERIOD_STEP;
            if (work->cellPeriod < ACTOR_503500_DRIFT_SPRITE_EMITTER_CELL_PERIOD_MIN) {
                work->cellPeriod = ACTOR_503500_DRIFT_SPRITE_EMITTER_CELL_PERIOD_MIN;
            }
            work->spriteSize -= ACTOR_503500_DRIFT_SPRITE_EMITTER_SPRITE_SIZE_STEP;
            if (work->spriteSize < ACTOR_503500_DRIFT_SPRITE_EMITTER_SPRITE_SIZE_MIN) {
                work->spriteSize = ACTOR_503500_DRIFT_SPRITE_EMITTER_SPRITE_SIZE_MIN;
            }
            // Fall through: both wind-down modes also stretch the spawn interval.
        case ACTOR_503500_DRIFT_SCENE_STAFF_CARD:
            work->spawnInterval.word += ACTOR_503500_DRIFT_SPRITE_EMITTER_SPAWN_INTERVAL_STEP;
            finished                  = work->spawnInterval.word > ACTOR_503500_DRIFT_SPRITE_EMITTER_SPAWN_INTERVAL_MAX;
            break;
        default:
            taskKill(task);
            return;
    }
    if (finished) {
        taskKill(task);
    }
}

/// Queues a centred 320x240 subtractive tile and its blend command at OT slot 3.
///
/// Channels are byte subtraction intensities. Requires centred screen coordinates,
/// a writable ordering-table slot 3 and arena space for a TILE and DR_TPAGE.
/// Both packets borrow the current arena until GPU drawing completes. The draw
/// command enables drawing into the displayed area, disables dithering and
/// leaves subtractive blending active afterward.
static inline void _actor503500DrawBlackOverlay(u8 red, u8 green, u8 blue)
{
    enum { BLACK_OVERLAY_WIDTH          = 320,
           BLACK_OVERLAY_HEIGHT         = 240,
           BLACK_OVERLAY_OT_SLOT        = 3,
           BLACK_OVERLAY_TEXTURE_PAGE_X = 320 };
    TILE*     tile;
    DR_TPAGE* drawMode;

    tile           = gGpuPrimCursor;
    gGpuPrimCursor = tile + 1;
    setTile(tile);
    SetSemiTrans(tile, 1);
    tile->x0 = -BLACK_OVERLAY_WIDTH / 2;
    tile->y0 = -BLACK_OVERLAY_HEIGHT / 2;
    tile->w  = BLACK_OVERLAY_WIDTH;
    tile->h  = BLACK_OVERLAY_HEIGHT;
    setRGB0(tile, red, green, blue);
    addPrim(gGpuCurrentOt + BLACK_OVERLAY_OT_SLOT, tile);
    drawMode       = gGpuPrimCursor;
    gGpuPrimCursor = drawMode + 1;
    setDrawTPage(drawMode, 1, 0, getTPage(0, GPU_BLEND_SUBTRACT, BLACK_OVERLAY_TEXTURE_PAGE_X, 0));
    // OT insertion prepends the blend command so it executes before the tile.
    addPrim(gGpuCurrentOt + BLACK_OVERLAY_OT_SLOT, drawMode);
}

/// Holds the screen black for the spawn delay, then reveals it eight shade levels per tick.
///
/// `spawnArg1.value` is a mutable frame countdown; the hold ends once it is
/// negative or the event is skipped. Initialization, hold and reveal advance
/// only while scene actors run, but the overlay draws on every callback.
/// `killCountdown` holds a signed halfword shade, initially 255. Each tick
/// draws its preceding low byte, including initialization and teardown ticks.
/// Requires the current frame arena and OT slot 3; task teardown owns the task.
static void _actor503500FadeFromBlackTask(Task* task)
{
    enum { FADE_INITIALIZE,
           FADE_HOLD,
           FADE_REVEAL,
           FADE_BLACK_LEVEL = 255,
           FADE_SHADE_STEP  = 8 };
    u8 red, green, blue;

    // Sample before updating so the final decrement still draws the preceding shade.
    red = green = blue = task->killCountdown;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        switch (task->state) {
            case FADE_INITIALIZE:
                task->killCountdown = FADE_BLACK_LEVEL;
                task->state++;
                break;
            case FADE_HOLD:
                if (--task->spawnArg1.value < 0 || gGameSession->evtSkipped != 0) {
                    task->state++;
                }
                break;
            case FADE_REVEAL:
                task->killCountdown -= FADE_SHADE_STEP;
                if (task->killCountdown < 0) {
                    taskKill(task);
                }
                break;
            default:
                taskKill(task);
                break;
        }
    }
    _actor503500DrawBlackOverlay(red, green, blue);
}

/// Stages the selected scene's audio start for later CD queue admission.
///
/// Entry scripts first select the scene. Its descriptor and buffers must stay
/// live through deferred request consumption; a missing slot leaves the request alone.
static void _actor503500StageSceneAudioStart(void)
{
    cdCmdStageSceneAudioStart();
}

/// Starts playback of the entry script's selected scene/audio session.
///
/// Prepared descriptor and buffers must stay live through playback. A selected
/// slot needs room in the CD request ring; without a slot, playback mode starts directly.
static void _actor503500EnqueueScenePlayback(void)
{
    cdCmdEnqueueScenePlayback();
}

/// Finishes the selected scene's streaming state and restores its saved random values.
///
/// Normal entry/card-use scripts call this after their final CAP cue. Requires
/// a successful scene selection; buffer and task teardown remain with their owners.
static void _actor503500FinishScene(void)
{
    streamFinishScene();
}

/// Discards deferred scene playback, requests CD cancellation and finishes the scene.
///
/// Entry and skip scripts use this after selecting a scene. Streaming/RNG state
/// finishes immediately; CD cancellation completes through subsequent dispatches.
static void _actor503500CancelScene(void)
{
    cdCmdCancelScene();
}

/// Returns to Shelter B2's pod bottom after normal or skipped staff-card playback.
///
/// Replaces the live save's area, warp and room selectors, retaining stage and view.
/// The resident reload task captures the frame and applies the saved location later.
static void _actor503500ReloadPodBottom(void)
{
    enum {
        ACTOR_503500_POD_BOTTOM_WARP = 1,
        ACTOR_503500_POD_BOTTOM_ROOM = 1,
    };
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = GAME_AREA_SHELTER_B2_POD_BOTTOM;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = ACTOR_503500_POD_BOTTOM_WARP;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.room = ACTOR_503500_POD_BOTTOM_ROOM;
    taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
}

/// Starts a drift-sprite emitter at room position `positionIndex` (0..3).
///
/// Requires this overlay and Shelter R48 resources loaded. Spawn failure is ignored;
/// successful tasks own their work and follow the room's scene-state lifetime.
static void _actor503500SpawnDriftSpriteEmitter(s32 positionIndex)
{
    enum {
        ACTOR_503500_SCENE_TASK_DRIFT_EMITTER = 0,
    };
    taskSpawnFromTable(D_actor_503500_8014B964, ACTOR_503500_SCENE_TASK_DRIFT_EMITTER, positionIndex, 0);
}

/// Starts the entry scene's fade from black and publishes its borrowed task handle.
///
/// `holdFrames` counts actor-running ticks; the hold ends on the first decrement
/// below zero or on event skip. A negative value reveals on the first hold tick.
/// Failure publishes NULL; task teardown owns the fade, whose handle may become stale.
static void _actor503500SpawnFadeFromBlack(s32 holdFrames)
{
    enum {
        ACTOR_503500_SCENE_TASK_FADE_FROM_BLACK = 1,
    };
    D_actor_503500_80176558 = taskSpawnFromTable(D_actor_503500_8014B964, ACTOR_503500_SCENE_TASK_FADE_FROM_BLACK, holdFrames, 0);
}

/// Requests cancellation of all room effects at an entry-script transition.
///
/// Requires the live room-effect state; cancellation is handled by its later updates.
static void _actor503500CancelRoomEffects(void)
{
    roomEffectRequestCancelAll();
}

/// Releases the placed boss's battle hold with rewards and sets the battle-end delay.
///
/// Placement index 0 must remain live while a battle hold exists. The delay is
/// in actor-running frames, copied as a low byte (scripts pass 5 or 11).
/// The task and its enemy work remain owned by the scene.
static void _actor503500ReleaseBossBattle(s8 endDelayFrames)
{
    enum { BOSS_PLACEMENT_INDEX = 0 };

    sceneReleaseBattleRefWithRewards(sceneFindPlacedActor(BOSS_PLACEMENT_INDEX), 0x23);
    gSceneCombatState.signals.bytes.endDelayFrames = endDelayFrames;
}

/// Adds session music-loading or weapon-restoration options from an entry script.
///
/// `flowFlags` uses `GAME_SESSION_FLOW_*`; only its low byte is stored.
/// Existing options accumulate until the session's flow handling clears them.
static void _actor503500AddSessionFlowFlags(s32 flowFlags)
{
    gGameSession->flowFlags |= flowFlags;
}

/// Queues normal and skip scripts for accepted staff-card use, then releases this launcher.
///
/// Requires the room and actor overlays loaded and event control already held.
/// The event runner retains the static script tables; it outlives this bodyless task.
static void _actor503500StaffCardSceneTask(Task* task)
{
    evsStartScriptWithSkip(D_actor_503500_8014CD98, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_actor_503500_8014D098);
    taskKill(task);
}

/// Locks attachment activation and cancels an active attachment for the event.
///
/// Attachment updates consume the lock; ordinary activation is blocked while set.
static void _actor503500LockAttachmentsForEvent(void)
{
    Gp_StateC08.flags |= ATTACHMENT_FLAG_EVENT_LOCK;
}

/// Requests deferred respawn of the saved view after the card-use scene begins.
static void _actor503500RequestViewRespawn(void)
{
    gGameSession->viewDirty = true;
}

/// Sets Shelter R48's staff-card progress from a normal or skipped entry script.
///
/// Only the low nibble is stored: 0 inactive, 1 ready for a staff card, 2 card-use
/// started. Both callers install 1; the room advances to 2 on card acceptance.
static void _actor503500SetStaffCardUseState(s32 cardUseState)
{
    gameFlagSetNibble(GAME_FLAG_100, cardUseState);
}

/// Adapts an event-script operand to Shelter R48's background-sprite visibility.
///
/// Only the low byte is passed: 0 hides, 1 shows, other byte values leave the
/// sprites unchanged. Requires the current room's mutable sprite resources.
static void _actor503500SetRoomBackgroundSpritesVisible(s32 visible)
{
    enum {
        ACTOR_503500_EVENT_OPERAND_BYTE_MASK = 0xFF,
    };
    shelterR48SetBackgroundSpritesVisible(visible & ACTOR_503500_EVENT_OPERAND_BYTE_MASK);
}

/// Invalidates the saved player placement by clearing its position sentinel.
///
/// All-zero XYZ suppresses restoration; the saved rotation remains available.
static void _actor503500ClearSavedPlayerTransform(void)
{
    D_actor_503500_8017655C.pos.vx = 0;
    D_actor_503500_8017655C.pos.vy = 0;
    D_actor_503500_8017655C.pos.vz = 0;
}

/// Saves the live player's root translation and Euler angles for a temporary scene placement.
///
/// Requires the player task, model root and `GameActor` work live. Positions use
/// the root's parent-coordinate units; angles use 4096 units per turn. The
/// package retains the copy until cleared or replaced; an origin position
/// cannot be restored because it is also the empty sentinel.
static void _actor503500SavePlayerTransform(void)
{
    Task*     playerTask;
    GfxCoord* rootCoord;
    SVECTOR*  savedRotation;

    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    rootCoord  = playerTask->extra.tmd->coords;

    D_actor_503500_8017655C.pos.vx = rootCoord->coord.t[0];
    D_actor_503500_8017655C.pos.vy = rootCoord->coord.t[1];
    D_actor_503500_8017655C.pos.vz = rootCoord->coord.t[2];

    savedRotation     = &D_actor_503500_8017655C.rot;
    savedRotation->vx = ((GameActor*)playerTask->work)->rotation.vx;
    savedRotation->vy = ((GameActor*)playerTask->work)->rotation.vy;
    savedRotation->vz = ((GameActor*)playerTask->work)->rotation.vz;
}

/// Restores the saved player placement after the temporary scene, when its position is nonzero.
///
/// Requires a live player with its placement-message handler. Dispatch borrows
/// the package's saved `ActorTransform` synchronously and copies XYZ/angles;
/// the snapshot remains valid for later restoration until cleared or replaced.
static void _actor503500RestorePlayerTransform(void)
{
    Task* playerTask;

    playerTask = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if ((D_actor_503500_8017655C.pos.vx != 0) || (D_actor_503500_8017655C.pos.vy != 0) ||
        (D_actor_503500_8017655C.pos.vz != 0)) {
        TASK_MESSAGE_DISPATCH_POINTER(playerTask, GAME_ACTOR_MESSAGE_PLACE, &D_actor_503500_8017655C, 0);
    }
}

/// Holds or releases the player's ordinary state tick during the card-use scene.
///
/// Zero permits the tick; any nonzero byte suppresses it without stopping the
/// rest of player movement/collision upkeep. The script passes 1 to hold it.
static void _actor503500SetPlayerUpdateHold(u8 holdPlayerUpdate)
{
    D_80115768 = holdPlayerUpdate;
}

/// Resets the player's weapon attack before the temporary card-use scene.
///
/// Requires the live player task and equipped weapon. Resets weapon effects,
/// eases aiming back and disables weapon pair/grid collision before the script
/// holds and temporarily places the player.
static void _actor503500ResetPlayerWeaponAttack(void)
{
    playerActorResetWeaponAttack(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), gPlayerStatus.weapon, 0);
}

/// Stops scripted controller vibration when an entry scene is skipped.
///
/// Raises the script/hold/ramp halt requests and clears script activity and
/// vibration output. Pending script tasks consume the halt requests on update.
static void _actor503500HaltPadScript(void)
{
    padScriptHalt();
    // The binary repeats this clear after the halt helper has already cleared it.
    gGameSession->padScriptFlags = 0;
}

void actor503500ClearFadeFromBlackHandle(s32 unused)
{
    D_actor_503500_80176558 = NULL;
}
